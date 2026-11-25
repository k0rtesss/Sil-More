#!/usr/bin/env python3
"""Check repeated player noise on unchanged and mutated terrain."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import BUILD, HARNESS, ROOT


OUT = ROOT / "scripts/output/perf-followup/noise/fixture"
CHECKS = r'''
#include "log/perf.h"

static int floods, cache_hits;

void __real_sil_perf_end(const char* label, sil_perf_stamp start);
void __wrap_sil_perf_end(const char* label, sil_perf_stamp start)
{
    if (!strcmp(label, "flow.noise")) ++floods;
    if (!strcmp(label, "flow.noise.reuse")) ++cache_hits;
    __real_sil_perf_end(label, start);
}

static Uint64 noise_digest(void)
{
    Uint64 digest = 14695981039346656037ULL;
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
        for (int x = 0; x < p_ptr->cur_map_wid; ++x) {
            digest ^= flow_dist(FLOW_PLAYER_NOISE, y, x);
            digest *= 1099511628211ULL;
        }
    return digest;
}

static void check_noise(void)
{
    enum { height = 198, width = 198, y = 90, x = 95, border = 100, target = 107 };
    bool baseline = SDL_getenv("NOISE_TEST_BASELINE") != NULL;
    p_ptr->cur_map_hgt = height;
    p_ptr->cur_map_wid = width;
    for (int row = 0; row < height; ++row)
        for (int col = 0; col < width; ++col)
            cave_feat[row][col] = col == border ? FEAT_WALL_EXTRA : FEAT_FLOOR;

    u64b rng = Rand_state_export();
    update_flow(y, x, FLOW_PLAYER_NOISE);
    assert(flow_dist(FLOW_PLAYER_NOISE, y, target) == FLOW_MAX_DIST);
    Uint64 wall = noise_digest();

    /* Waiting on unchanged terrain repeatedly publishes the same field. */
    floods = cache_hits = 0;
    Uint64 start = SDL_GetTicksNS();
    for (int i = 0; i < 400; ++i)
        update_flow(y, x, FLOW_PLAYER_NOISE);
    double elapsed_ms = (double)(SDL_GetTicksNS() - start) / 1e6;
    assert(floods == (baseline ? 400 : 0));
    assert(cache_hits == (baseline ? 0 : 400));
    assert(noise_digest() == wall);
    printf("400 stationary noise requests: %d floods, %d reuses, %.3f ms\n",
        floods, cache_hits, elapsed_ms);

    /* Direct dungeon load/edit storage and the normal feature setter both
     * affect the same field. A concealed door also carries the +5 cost. */
    cave_feat[y][border] = FEAT_DOOR_HEAD;
    update_flow(y, x, FLOW_PLAYER_NOISE);
    assert(flow_dist(FLOW_PLAYER_NOISE, y, target) == target - x + 5);
    Uint64 closed = noise_digest();
    cave_feat[y][border] = FEAT_SECRET;
    update_flow(y, x, FLOW_PLAYER_NOISE);
    assert(noise_digest() == closed);
    cave_set_feat(y, border, FEAT_FLOOR);
    update_flow(y, x, FLOW_PLAYER_NOISE);
    assert(flow_dist(FLOW_PLAYER_NOISE, y, target) == target - x);
    Uint64 opened = noise_digest();

    /* Level setup clears live costs; a tutorial preview restores them.
     * Matching only source/terrain would leave these altered costs live. */
    memset(cave_cost[FLOW_PLAYER_NOISE], FLOW_MAX_DIST, sizeof(cave_cost[0]));
    flow_center_y[FLOW_PLAYER_NOISE] = flow_center_x[FLOW_PLAYER_NOISE] = 0;
    update_center_y[FLOW_PLAYER_NOISE] = update_center_x[FLOW_PLAYER_NOISE] = 0;
    update_flow(y, x, FLOW_PLAYER_NOISE);
    assert(noise_digest() == opened);
    assert(flow_center_y[FLOW_PLAYER_NOISE] == y && update_center_y[FLOW_PLAYER_NOISE] == y);
    assert(flow_center_x[FLOW_PLAYER_NOISE] == x && update_center_x[FLOW_PLAYER_NOISE] == x);

    update_flow(y, x + 1, FLOW_PLAYER_NOISE);
    assert(flow_dist(FLOW_PLAYER_NOISE, y, target) == target - x - 1);
    Uint64 moved = noise_digest();
    p_ptr->cur_map_wid = width / 2;
    update_flow(y, x + 1, FLOW_PLAYER_NOISE);
    Uint64 resized = noise_digest();
    p_ptr->cur_map_wid = width;
    update_flow(y, x + 1, FLOW_PLAYER_NOISE);
    assert(noise_digest() == moved);

    update_flow(y, target, FLOW_MONSTER_NOISE);
    assert(flow_dist(FLOW_MONSTER_NOISE, y, x) == target - x);
    assert(noise_digest() == moved && Rand_state_export() == rng);
    printf("Field fingerprints: %016llx %016llx %016llx %016llx %016llx\n",
        (unsigned long long)wall, (unsigned long long)closed, (unsigned long long)opened,
        (unsigned long long)moved, (unsigned long long)resized);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, help="Compile the supplied pre-cache cave-flow.c")
    args = parser.parse_args()
    output = OUT / ("before" if args.baseline else "after")
    output.mkdir(parents=True, exist_ok=True)
    start = HARNESS.index("static cptr make_path")
    init = HARNESS[start:HARNESS.index("    int last = ")]
    source = output / "check.c"
    source.write_text(HARNESS[:start] + CHECKS + init
                      + "    check_noise(); sdl_quit_hook(NULL); return 0;\n}\n", encoding="utf-8")
    flow_source = args.baseline.resolve() if args.baseline else ROOT / "src/cave/cave-flow.c"
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = output / "objects.rsp"
    response.write_text("\n".join(f'"{obj}"' for obj in objects if not obj.endswith((
        "/src/main.c.obj", "/src/cave/cave-flow.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software", SDL_AUDIO_DRIVER="dummy",
               CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    if args.baseline:
        env["NOISE_TEST_BASELINE"] = "1"
    exe = output / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-DSIL_PERF_DIAGNOSTICS", "-std=c17", "-O3",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", "-I", str(ROOT / "src/cave"), str(source), str(flow_source),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=sil_perf_end",
        "-o", str(exe),
    ], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(dir=output, prefix="profile-") as profile:
        result = subprocess.run([str(exe), "--windowed"], cwd=ROOT, env=dict(env, CONTROL_TEST_PROFILE=profile),
                                capture_output=True, text=True, timeout=30)
        (output / "validation.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        print(result.stdout, end="")
        if result.returncode:
            print(result.stderr, end="")
        result.check_returncode()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check production ordinary-cave M: rules; build-incremental.ps1 first."""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/cave-floor-palette-rules-check"

HARNESS = r'''
#include "angband.h"
#include "externs.h"
#include "init.h"
#include "init/init-parse-internal.h"
#include "log/log.h"
#include "rng.h"
#include <assert.h>

static errr parse(const char* text) {
    char line[4096];
    SDL_strlcpy(line, text, sizeof(line));
    return parse_style_levels(line, NULL);
}

int main(void) {
    log_set_level(LOG_WARN);
    Rand_state_init(314159);
    u64b rng = Rand_state_export();
    cave_floor_palette got, previous;
    assert(!z_info && !style_info);
    assert(!parse("V:0.9.8"));
    assert(!parse("M:0:1:1: 63:1")); /* Parse before style data exists. */
    assert(!styles_cave_floor_palette(0, &got));
    z_info = calloc(1, sizeof(*z_info)); z_info->style_max = 64;
    style_info = calloc(64, sizeof(*style_info));
    for (int i=0; i<64; i++) style_info[i].name = i+1;
    assert(styles_cave_floor_palette(0, &got));
    assert(got.coverage==1 && got.patches==1 && got.count==1);
    assert(got.styles[0]==63 && got.weights[0]==1);
    assert(!styles_cave_floor_palette(1, &got));
    assert(!styles_cave_floor_palette(-1, &got));
    assert(!styles_cave_floor_palette(64, &got));
    assert(!styles_cave_floor_palette(0, NULL));
    assert(!parse("M:0:40:4: 0:1000 1:1 2:2 3:3 4:4 5:5 6:6 63:1000 # max limits"));
    assert(styles_cave_floor_palette(0, &previous));
    assert(previous.coverage==40 && previous.patches==4 && previous.count==8);
    assert(previous.styles[7]==63 && previous.weights[7]==1000);
    const char* bad[] = {
        "M:", "M:0", "M:0:", "M:0:1", "M:0:1:", "M:0:1:1",
        "M:0:1:1:", "M:0:1:1: # empty", "M:0:0:1: 1:1",
        "M:0:41:1: 1:1", "M:0:1:0: 1:1", "M:0:1:5: 1:1",
        "M:-1:1:1: 1:1", "M:64:1:1: 1:1", "M:0:1:1: -1:1",
        "M:0:1:1: 64:1", "M:0:1:1: 1:0", "M:0:1:1: 1:1001",
        "M:0:1:1: 1:-1", "M:+0:1:1: 1:1", "M:0:1:1: +1:1",
        "M:0:1:1: 1:+1", "M:0:1:1: 1", "M:0:1:1: 1:",
        "M:0:1:1: 1:1x", "M:0:1:1: 1:1 junk", "M:0:1:1: 1:1:2",
        "M:0:1:1: 1:1,2:2", "M:0:1:1: 1:1 2:2 trailing",
        "M:0:1:1: 1:1 2:1 3:1 4:1 5:1 6:1 7:1 8:1 9:1",
        "M:999999999999999999999999999:1:1: 1:1",
        "M:0:999999999999999999999999999:1: 1:1",
        "M:0:1:999999999999999999999999999: 1:1",
        "M:0:1:1: 999999999999999999999999999:1",
        "M:0:1:1: 1:999999999999999999999999999"
    };
    for (unsigned i=0; i<N_ELEMENTS(bad); i++) {
        assert(parse(bad[i])!=0);
        assert(styles_cave_floor_palette(0, &got));
        assert(!memcmp(&got, &previous, sizeof(got)));
    }
    cave_floor_palette invalid = previous;
    invalid.weights[7]=1001;
    assert(!styles_set_cave_floor_palette(0, &invalid));
    assert(!styles_set_cave_floor_palette(-1, &previous));
    assert(!styles_set_cave_floor_palette(64, &previous));
    assert(!styles_set_cave_floor_palette(0, NULL));
    assert(styles_cave_floor_palette(0, &got));
    assert(!memcmp(&got, &previous, sizeof(got)));
    /* Invalid runtime references disable the whole rule without deleting it. */
    style_info[63].name=0;
    assert(!styles_cave_floor_palette(0, &got));
    style_info[63].name=64;
    style_info[0].name=0;
    assert(!styles_cave_floor_palette(0, &got));
    style_info[0].name=1;
    z_info->style_max=63;
    assert(!styles_cave_floor_palette(0, &got));
    z_info->style_max=64;
    assert(styles_cave_floor_palette(0, &got));
    assert(!parse("M:0:12:2:\t2:7\t3:8# comment"));
    assert(styles_cave_floor_palette(0, &got));
    assert(got.coverage==12 && got.patches==2 && got.count==2);
    assert(got.styles[0]==2 && got.weights[0]==7);
    assert(got.styles[1]==3 && got.weights[1]==8);
    assert(!parse("M:63:1:1: 0:1"));
    assert(styles_cave_floor_palette(63, &got));
    assert(!parse("V:0.9.8"));
    assert(!styles_cave_floor_palette(0, &got));
    assert(!styles_cave_floor_palette(63, &got));
    assert(!parse("M:0:1:1: 63:1"));
    assert(styles_cave_floor_palette(0, &got));
    styles_cave_floor_palettes_clear();
    assert(!styles_cave_floor_palette(0, &got));
    assert(Rand_state_export()==rng);
    free(style_info); free(z_info);
    puts("Cave floor palette rules: bounds, malformed input, atomic replacement, reparse, runtime validation and RNG preservation: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith("/src/main.c.obj")]
    rsp = OUT / "objects.rsp"
    rsp.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *[str(BUILD / "_deps" / x) for x in ("SDL", "SDL_image", "SDL_ttf", "SDL_mixer")],
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        "@" + str(rsp), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

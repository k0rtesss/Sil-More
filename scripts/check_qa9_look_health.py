#!/usr/bin/env python3
"""Render real SDL Look health bars, including zero-HP wizard creatures.

Reuses the Windows CMake objects and an isolated SDL/template profile.
The zero and stale cases crash the pre-fix renderer with integer division by
zero.  Ordinary and peaceful creatures retain their existing presentation.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/qa9-look-health"
CASES = ("zero", "negative", "stale", "ordinary", "peaceful")

CHECKS = r'''
    const char *scenario = SDL_getenv("LOOK_HEALTH_CASE");
    const char *png = SDL_getenv("LOOK_HEALTH_PNG");
    int race = 0;
    for (int i = 1; i < z_info->r_max; i++)
        if (r_info[i].name && !strcmp(r_name + r_info[i].name, "Wolf")) {
            race = i; break;
        }
    assert(race && scenario && png);
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 40;
    p_ptr->py = p_ptr->px = 12;
    mon_max = 2;
    memset(&mon_list[1], 0, sizeof(mon_list[1]));
    monster_type *monster = &mon_list[1];
    monster->r_idx = race;
    monster->ml = true;
    monster->fy = 12; monster->fx = 13;
    monster->hp = 5; monster->maxhp = 10;
    cave_m_idx[12][13] = 1;
    bool ordinary = !strcmp(scenario, "ordinary");
    bool peaceful = !strcmp(scenario, "peaceful");
    bool stale = !strcmp(scenario, "stale");
    if (peaceful) r_info[race].flags1 |= RF1_PEACEFUL;
    if (!strcmp(scenario, "zero")) monster->maxhp = 0;
    if (!strcmp(scenario, "negative")) monster->maxhp = -3;
    styled_monster_health_bars = true;
    sdl_unified_look_sidebar_begin(false, false, -1);
    sdl_unified_look_sidebar_add_header("MONSTERS:");
    sdl_unified_look_sidebar_add_entry(0, 1, 12, 13,
        TERM_WHITE, TERM_WHITE, "C", "Wolf -------- 5");
    sdl_unified_look_sidebar_finish();
    if (stale) monster->maxhp = 0;
    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    assert(SDL_RenderClear(g_state.renderer));
    sdl_unified_look_sidebar_render();
    SDL_Surface *surface = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(surface && IMG_SavePNG(surface, png));
    SDL_DestroySurface(surface);
    assert(monster->hp == 5);
    assert(monster_health_bar_allowed(monster) == ordinary);
    assert(g_unified_look_sidebar.count == 2);
    if (ordinary || stale)
        assert(g_unified_look_sidebar.items[1].health_m_idx == 1);
    else
        assert(g_unified_look_sidebar.items[1].health_m_idx == 0);
    printf("Look health render: %s PASS\n", scenario);
    sdl_unified_look_sidebar_clear();
    sdl_quit_hook(NULL);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", choices=CASES)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS[:HARNESS.index("    int last = ")] + CHECKS,
                      encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith("/src/main.c.obj")),
                        encoding="utf-8")
    env = os.environ.copy()
    env.pop("SIL_MORE_CONTROL_DIR", None)
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in
          ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for case in ((args.scenario,) if args.scenario else CASES):
        with tempfile.TemporaryDirectory(prefix=case + "-", dir=OUT) as profile:
            run_env = dict(env, CONTROL_TEST_PROFILE=profile, LOOK_HEALTH_CASE=case,
                           LOOK_HEALTH_PNG=str(OUT / (case + ".png")))
            subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
                           env=run_env, check=True, timeout=25)


if __name__ == "__main__":
    main()

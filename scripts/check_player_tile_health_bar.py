#!/usr/bin/env python3
"""Render player tile health bars and check retained-map invalidation.

Uses production SDL code, real assets, dummy video and disposable profiles.
No personal save/config or desktop input is used.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/player-tile-health"

CHECKS = r'''
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 32;
    p_ptr->py = p_ptr->px = 12;
    p_ptr->mhp = p_ptr->chp = 40;
    p_ptr->is_dead = false;
    cave_m_idx[12][12] = -1;
    cave_feat[12][12] = FEAT_FLOOR;
    cave_info[12][12] = CAVE_MARK | CAVE_SEEN;
    op_ptr->monster_tile_health_bar_mode = MONSTER_TILE_HEALTH_BARS_OFF;
    pixel_monster_status_icons = false;
    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_FRect cell = {8, 8, 48, 64};
    const int hp[] = {40, 20, 1};
    for (int tiles = 0; tiles < 2; ++tiles) {
        g_state.use_tiles = tiles != 0;
        arg_graphics = tiles ? GRAPHICS_MICROCHASM : GRAPHICS_NONE;
        byte a, ta; char c, tc;
        map_info(12, 12, &a, &c, &ta, &tc);
        styled_player_tile_health_bar = false;
        sdl_draw_map_tile_layers_at(12, 12, a, c, ta, tc, &cell);
        SDL_Rect sample = {8, 8, 48, 16};
        SDL_Surface* baseline = SDL_RenderReadPixels(g_state.renderer, &sample);
        assert(baseline);
        Uint64 disabled = sdl_minimap_monster_health_hash();
        styled_player_tile_health_bar = true;
        assert(sdl_minimap_monster_health_hash() != disabled);
        Uint64 previous_hash = disabled;
        for (int i = 0; i < (int)N_ELEMENTS(hp); ++i) {
            p_ptr->chp = hp[i];
            assert(sdl_map_player_health_bar_visible(12, 12));
            assert(!sdl_map_player_health_bar_visible(12, 13));
            assert(!sdl_map_player_health_bar_visible(-1, 12));
            Uint64 hash = sdl_minimap_monster_health_hash();
            assert(hash != previous_hash); previous_hash = hash;
            sdl_draw_map_tile_layers_at(12, 12, a, c, ta, tc, &cell);
            SDL_Surface* pixels = SDL_RenderReadPixels(g_state.renderer, &sample);
            assert(pixels && pixels->w == baseline->w && pixels->h == baseline->h);
            int changed = 0;
            for (int y = 0; y < pixels->h; ++y) for (int x = 0; x < pixels->w; ++x) {
                Uint8 r,g,b,alpha,br,bg,bb,ba;
                assert(SDL_ReadSurfacePixel(pixels,x,y,&r,&g,&b,&alpha));
                assert(SDL_ReadSurfacePixel(baseline,x,y,&br,&bg,&bb,&ba));
                changed += r != br || g != bg || b != bb;
            }
            assert(changed > 20);
            char path[1200];
            strnfmt(path,sizeof(path),"%s/%s-hp%d.png",SDL_getenv("HEALTH_TEST_OUTPUT"),
                tiles ? "tiles" : "ascii",hp[i]);
            assert(IMG_SavePNG(pixels,path)); SDL_DestroySurface(pixels);
        }
        SDL_DestroySurface(baseline);
        Uint64 live = sdl_minimap_monster_health_hash();
        p_ptr->is_dead = true;
        assert(!sdl_map_player_health_bar_visible(12, 12));
        assert(sdl_minimap_monster_health_hash() != live);
        p_ptr->is_dead = false; p_ptr->mhp = 0;
        assert(!sdl_map_player_health_bar_visible(12, 12));
        sdl_draw_map_tile_layers_at(12, 12, a, c, ta, tc, &cell);
        p_ptr->mhp = 40; p_ptr->chp = 40;
    }
    puts("Player tile HP: ASCII/tiles at full, half and one HP; opt-out, dead/invalid cells, zero max HP and retained-map invalidation: PASS");
    sdl_quit_hook(NULL); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    prefix = HARNESS[:HARNESS.index("    int last = ")]
    prefix = prefix.replace('#include "sdl/main-sdl-private.h"',
                            '#include "sdl/render/sdl-term-callbacks.c"')
    source.write_text(prefix + CHECKS, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith(("/src/main.c.obj", "/src/sdl/render/sdl-term-callbacks.c.obj"))),
        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software", SDL_AUDIO_DRIVER="dummy",
               CONTROL_TEST_ASSETS=str(ROOT / "lib"), HEALTH_TEST_OUTPUT=str(OUT),
               CONTROL_TEST_LOG_BASE=str(OUT / "check.exe"))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
                       env=dict(env, CONTROL_TEST_PROFILE=profile), check=True, timeout=30)


if __name__ == "__main__":
    main()

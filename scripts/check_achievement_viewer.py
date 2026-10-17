#!/usr/bin/env python3
"""Render the production achievement book with real fonts in a hidden SDL fixture."""
from pathlib import Path
import json
import os
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievement-viewer-check"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "achievements-ui.c"

int main(int argc, char **argv)
{
    assert(argc == 7);
    int width = atoi(argv[3]), height = atoi(argv[4]), bigger = atoi(argv[5]);
    maxima limits = {0};
    player_type player = {0};
    z_info = &limits; p_ptr = &player;
    player.playing = true; player.cur_map_hgt = 32; player.cur_map_wid = 64;
    character_generated = character_dungeon = true;
    log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)); assert(TTF_Init());
    sdl_config_set_defaults(&config); sdl_sync_palette();
    SDL_strlcpy(config.story_font, argv[1], sizeof(config.story_font));
    SDL_strlcpy(config.story_font2, argv[1], sizeof(config.story_font2));
    config.use_unsafe_area = true;
    config.aux_view_font_size = 16;
    set_sdl_menu_bigger_font(SDL_MENU_FONT_HELP, bigger != 0);
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_HELP);
    g_state.system_scale = 1;
    g_state.window = SDL_CreateWindow("Achievement render fixture", width, height, SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, "software"); assert(g_state.renderer);
    g_state.safe_area = (SDL_Rect){0,0,width,height};
    sdl_view *view = &g_views[PANE_MAIN];
    term_init(&view->t, width/8, height/16, 256);
    Term_activate(&view->t); term_screen = &view->t; view->term_ready = true;
    view->rect = (SDL_Rect){0,0,width,height};
    view->cell_w = 8; view->cell_h = 16; view->cols = width/8; view->rows = height/16;
    view->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, width, height); assert(view->canvas);
    ANGBAND_DIR_APEX = argv[2]; sil_achievements_init();
    assert(sil_achievements_saved());
    assert(!strcmp(main_menu_title(MAIN_MENU_ACHIEVEMENTS), "Achievements"));
    assert(main_menu_keyboard_key(MAIN_MENU_ACHIEVEMENTS) == 'v');
    build_achievement_book(0);
    ui_menu_click_begin();
    SDL_SetRenderDrawColor(g_state.renderer, 25, 28, 29, 255); SDL_RenderClear(g_state.renderer);
    sdl_character_sheet_screen_render();
    sdl_character_sheet_screen_state *book = &g_sdl_character_sheet_screen;
    int pages = book->narrative_page_count;
    assert(pages >= 1 && pages < SDL_BOOK_MAX_PAGES);
    assert(book->narrative_page_start[0] == 0);
    assert(book->narrative_page_start[pages] == book->narrative_para_count);
    for (int i = 0; i < SIL_ACH_COUNT; ++i) {
        bool title = false, description = false;
        for (int j = 0; j < book->narrative_para_count; ++j) {
            title |= strstr(book->narrative_paras[j], sil_achievement_get(i)->title) != NULL;
            description |= !strcmp(book->narrative_paras[j], sil_achievement_get(i)->description);
        }
        assert(title && description);
    }
    for (int page = 0; page < pages; ++page) {
        sdl_character_sheet_screen_set_book_page(page);
        SDL_SetRenderDrawColor(g_state.renderer, 25, 28, 29, 255); SDL_RenderClear(g_state.renderer);
        ui_menu_click_begin(); sdl_character_sheet_screen_render();
        assert(book->select_page == page);
        assert(book->narrative_page_start[page+1] > book->narrative_page_start[page]);
        bool close = false;
        for (int hit = 0; hit < book->hit_count; ++hit) {
            SDL_FRect rect = book->hits[hit].rect;
            assert(rect.x >= -1 && rect.y >= -1);
            assert(rect.x + rect.w <= width + 1 && rect.y + rect.h <= height + 1);
            if (book->hits[hit].choice == SDL_SELECT_CLICK_CLOSE) close = true;
        }
        assert(close);
        if (page == 0 || page == pages-1) {
            char path[160];
            snprintf(path, sizeof(path), "%s-%dx%d-big%d-page%d.bmp", argv[6], width,height,bigger,page+1);
            SDL_Surface *pixels = SDL_RenderReadPixels(g_state.renderer, NULL); assert(pixels);
            assert(SDL_SaveBMP(pixels, path)); SDL_DestroySurface(pixels);
        }
    }
    printf("Achievement viewer %s %dx%d big=%d: %d pages, %dpx body, complete catalogue and bounded Back hitboxes PASS\n",
        argv[6], width,height,bigger,pages,book->narrative_body_px);
    sdl_character_sheet_screen_hide();
    sdl_story_font_cache_clear(); SDL_DestroyTexture(view->canvas);
    term_nuke(&view->t); view->term_ready=false; term_screen=NULL; Term=NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window); g_state.window=NULL;
    sdl_pop_terminal_menu_scale();
    TTF_Quit(); SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    # externs.h is intentionally unguarded in this repository; the private
    # renderer header above already includes it. Keep production functions exact.
    implementation = (ROOT / "src/achievements-ui.c").read_text().replace(
        '#include "externs.h"', '')
    source.write_text(HARNESS.replace('#include "achievements-ui.c"', implementation), encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [obj for obj in objects if not obj.endswith(("/src/main.c.obj", "/src/achievements-ui.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")] +
        ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as directory:
        enum = (ROOT / "src/achievements.h").read_text().split("typedef enum sil_achievement_id {", 1)[1].split("} sil_achievement_id;", 1)[0]
        keys = re.findall(r"\bSIL_ACH_([A-Z][A-Z0-9_]*)(?:\s*=\s*\d+)?\s*,", enum)
        for earned in (False, True):
            profile = Path(directory) / str(earned)
            profile.mkdir()
            (profile / "achievements.json").write_text(json.dumps({
                "version": 1, "unlocked": dict.fromkeys(
                    keys, 1800000000) if earned else {}}))
            for width, height, bigger in ((1280,720,0), (1280,800,1), (580,1280,1), (1280,580,1), (360,800,1)):
                subprocess.run([
                    str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"), str(profile),
                    str(width), str(height), str(bigger), "unlocked" if earned else "locked"],
                    cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

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
enum { SDL_CHAR_SHEET_TEXT_LEN = WRAP_TEXT_LEN, SDL_CHAR_SHEET_MAX_LINES = WRAP_MAX_LINES };
/* Fail headlessly: the Windows CRT assertion dialog would stall CI. */
#undef assert
#define assert(check) do { if (!(check)) { fprintf(stderr,"Fixture assertion: %s at line %d\n",#check,__LINE__); exit(3); } } while (0)
extern int sdl_char_sheet_wrap_text(TTF_Font*, cptr, float,
    char (*)[SDL_CHAR_SHEET_TEXT_LEN], int);

static SDL_Rect green_ink(SDL_Surface* surface, SDL_Rect region)
{
    int left=surface->w, top=surface->h, right=-1, bottom=-1;
    for (int y=MAX(0,region.y); y<MIN(surface->h,region.y+region.h); ++y)
        for (int x=MAX(0,region.x); x<MIN(surface->w,region.x+region.w); ++x) {
            Uint8 r,g,b,a;
            assert(SDL_ReadSurfacePixel(surface,x,y,&r,&g,&b,&a));
            if ((int)g*a/255>50 && g>r*2 && g>b*2) {
                left=MIN(left,x); right=MAX(right,x); top=MIN(top,y); bottom=MAX(bottom,y);
            }
        }
    assert(right>=left && bottom>=top);
    return (SDL_Rect){left,top,right-left+1,bottom-top+1};
}

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
    assert(book->narrative_para_count >= SIL_ACH_COUNT + 2);
    assert(book->narrative_para_count <= SDL_BOOK_MAX_PARAS);
    assert(g_state.tileset); /* Icons must render even when the map is ASCII. */
    for (int i = 0; i < SIL_ACH_COUNT; ++i) {
        bool title = false, description = false;
        for (int j = 0; j < book->narrative_para_count; ++j) {
            title |= strstr(book->narrative_paras[j], sil_achievement_get(i)->title) != NULL;
            description |= strstr(book->narrative_paras[j], sil_achievement_get(i)->description) != NULL;
        }
        bool visible = !sil_achievement_get(i)->hidden
            || sil_achievement_unlocked_at(i);
        assert(title == visible && description == visible);
        int para = i + 2;
        assert(book->narrative_icon_row[para] == (visible ? achievement_icons[i].row : 32));
        assert(book->narrative_icon_col[para] == (visible ? achievement_icons[i].col : 0));
        assert(book->narrative_para_lines[para] >= 2);
    }
    TTF_Font *body_font = sdl_story_font_for_height_slot(book->narrative_body_px,
        SDL_STORY_FONT_SLOT_NARRATIVE);
    /* Pagination stores rounded cache keys; inspect the exact painted viewport
     * for line-fit assertions so subpixel panel widths cannot change one wrap. */
    float book_w = book->select_scroll_rect.w;
    assert(SDL_fabsf(book_w - sdl_char_sheet_book_width(book->narrative_body_px,
        book->narrative_paginated_for_w)) < 1.0f);
    float line_h = sdl_char_sheet_line_h(body_font, book->narrative_body_px, 1.28f);
    assert(book->narrative_icon_book);
    if (bigger) assert(book->narrative_body_px >= (height > width ? 48 : 26));
    if (width <= 580 && height > width) {
        assert(book_w >= width - 55); /* Panel insets, not body-size padding. */
        float reading_w = book_w - MIN(40.0f, book_w*0.12f);
        assert(reading_w >= width*0.75f);
        if (width == 360) assert(book->narrative_body_px == 50);
        if (width == 580) assert(book->narrative_body_px == 69);
        const char* words[] = { "Undiscovered", "achievement" };
        for (int i=0; i<2; ++i) {
            char word_lines[SDL_CHAR_SHEET_MAX_LINES][SDL_CHAR_SHEET_TEXT_LEN];
            assert(sdl_char_sheet_text_width(body_font,words[i]) <= (int)reading_w);
            assert(sdl_char_sheet_wrap_text(body_font,words[i],reading_w,
                word_lines,SDL_CHAR_SHEET_MAX_LINES)==1);
            assert(!strcmp(word_lines[0],words[i]));
        }
    }
    for (int para = 0; para < book->narrative_para_count; ++para) {
        char lines[SDL_CHAR_SHEET_MAX_LINES][SDL_CHAR_SHEET_TEXT_LEN];
        float gutter = book->narrative_icon_row[para] >= 0
            ? MIN(line_h * 1.6f, book_w <= 680.0f
                ? MIN(40.0f, book_w * 0.12f) : book_w * 0.18f) : 0;
        float text_w = book_w - gutter;
        int count = sdl_char_sheet_wrap_text(body_font, book->narrative_paras[para],
            text_w, lines, SDL_CHAR_SHEET_MAX_LINES);
        assert(count <= SDL_CHAR_SHEET_MAX_LINES);
        assert(count == book->narrative_para_lines[para]);
        for (int line = 0; line < count; ++line)
            assert(sdl_char_sheet_text_width(body_font, lines[line]) <= (int)text_w);
    }
    /* Compare the drawn Back ink with a full native-font raster. Hitbox bounds
     * alone cannot detect a glyph cropped by a shorter texture source rect. */
    int prompt_px = sdl_menu_font_px(sdl_char_sheet_clampi((int)(height*0.025f),13,30));
    TTF_Font* back_font = sdl_story_font_for_height_slot(prompt_px, SDL_STORY_FONT_SLOT_DEFAULT);
    if (book->narrative_paginated_for_w >= 700) {
        float prompt_h = sdl_char_sheet_line_h(back_font,prompt_px,1.02f);
        back_font = sdl_story_font_for_height_slot((int)(prompt_h*1.12f+0.5f),SDL_STORY_FONT_SLOT_DEFAULT);
    }
    SDL_Surface* expected_back = TTF_RenderText_Blended(back_font,"Back",0,(SDL_Color){0,255,0,255});
    assert(expected_back);
    SDL_Rect expected_ink = green_ink(expected_back,(SDL_Rect){0,0,expected_back->w,expected_back->h});
    SDL_DestroySurface(expected_back);
    for (int page = 0; page < pages; ++page) {
        sdl_character_sheet_screen_set_book_page(page);
        SDL_SetRenderDrawColor(g_state.renderer, 25, 28, 29, 255); SDL_RenderClear(g_state.renderer);
        ui_menu_click_begin(); sdl_character_sheet_screen_render();
        assert(book->select_page == page);
        assert(book->narrative_page_start[page+1] > book->narrative_page_start[page]);
        bool close = false;
        SDL_FRect close_rect = {0};
        for (int hit = 0; hit < book->hit_count; ++hit) {
            SDL_FRect rect = book->hits[hit].rect;
            assert(rect.x >= -1 && rect.y >= -1);
            assert(rect.x + rect.w <= width + 1 && rect.y + rect.h <= height + 1);
            if (book->hits[hit].choice == SDL_SELECT_CLICK_CLOSE) { close = true; close_rect = rect; }
        }
        assert(close);
        SDL_Surface *pixels = SDL_RenderReadPixels(g_state.renderer, NULL); assert(pixels);
        SDL_Rect back_ink = green_ink(pixels,(SDL_Rect){(int)close_rect.x,(int)close_rect.y,
            (int)SDL_ceilf(close_rect.w),(int)SDL_ceilf(close_rect.h)});
        if (abs(back_ink.w-expected_ink.w)>1 || abs(back_ink.h-expected_ink.h)>1)
            fprintf(stderr,"Back ink %dx%d differs from native %dx%d at %dx%d big=%d\n",
                back_ink.w,back_ink.h,expected_ink.w,expected_ink.h,width,height,bigger);
        assert(abs(back_ink.w-expected_ink.w)<=1 && abs(back_ink.h-expected_ink.h)<=1);
        assert(back_ink.y>=0 && back_ink.y+back_ink.h<=height-2);
        if (page == 0 || page == MIN(2, pages-1) || page == pages-1) {
            char path[160];
            snprintf(path, sizeof(path), "%s-%dx%d-big%d-page%d.bmp", argv[6], width,height,bigger,page+1);
            assert(SDL_SaveBMP(pixels, path));
        }
        SDL_DestroySurface(pixels);
    }
    printf("Achievement viewer %s %dx%d big=%d: %d pages, %dpx body, complete catalogue, fitted lines and complete native Back ink PASS\n",
        argv[6], width,height,bigger,pages,book->narrative_body_px);
    sdl_character_sheet_screen_hide();
    sdl_story_font_cache_clear(); SDL_DestroyTexture(view->canvas);
    term_nuke(&view->t); view->term_ready=false; term_screen=NULL; Term=NULL;
    SDL_DestroyTexture(g_state.tileset); g_state.tileset=NULL;
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
    renderer = (ROOT / "src/sdl/ui/sdl-screens.c").read_text(encoding="utf-8")
    fixture = HARNESS.replace('#include "achievements-ui.c"', implementation)
    for placeholder, name in (("WRAP_TEXT_LEN", "SDL_CHAR_SHEET_TEXT_LEN"),
                              ("WRAP_MAX_LINES", "SDL_CHAR_SHEET_MAX_LINES")):
        fixture = fixture.replace(placeholder, re.search(name + r"\s*=\s*(\d+)", renderer)[1])
    source.write_text(fixture, encoding="utf-8")
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
                    str(width), str(height), str(bigger), str(OUT / ("unlocked" if earned else "locked"))],
                    cwd=ROOT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

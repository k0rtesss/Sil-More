#!/usr/bin/env python3
"""Check UI catalogue glyphs and render localized text through SDL offscreen."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

from check_tutorial_translation import LANGUAGES

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/ui-language-fonts"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "sdl/ui/sdl-question-menu.c"
#include "tutorial/tutorial.h"
#include "ui/menu-click.h"
#include "ui/ui-language.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static const SDL_Color ink = {255, 255, 255, 255};
static bool touch_picker_fixture;

bool __real_sdl_touch_only_device_active(void);
bool __wrap_sdl_touch_only_device_active(void)
{
    return touch_picker_fixture || __real_sdl_touch_only_device_active();
}

static void check_glyphs(TTF_Font *font, cptr code, cptr text)
{
    bool missing = false;
    assert(font);
    size_t len = strlen(text);
    while (len) {
        Uint32 glyph = SDL_StepUTF8(&text, &len);
        if (!TTF_FontHasGlyph(font, glyph)) {
            fprintf(stderr, "%s missing glyph U+%04X\n", code, (unsigned)glyph);
            missing = true;
        }
    }
    assert(!missing);
}

static Uint64 cell_pixels(sdl_view *view, cptr glyph, bool expect_ink)
{
    assert(!Term_clear());
    assert(!Term_putstr(2, 1, -1, TERM_WHITE, glyph));
    assert(!Term_fresh());
    assert(SDL_SetRenderTarget(g_state.renderer, view->canvas));
    SDL_Rect area = {2 * view->cell_w, view->cell_h, 2 * view->cell_w, view->cell_h};
    SDL_Surface *pixels = SDL_RenderReadPixels(g_state.renderer, &area);
    assert(pixels);
    Uint64 hash = 1;
    int bright = 0;
    for (int y = 0; y < pixels->h; y++) {
        for (int x = 0; x < pixels->w; x++) {
            Uint8 r, g, b, a;
            assert(SDL_ReadSurfacePixel(pixels, x, y, &r, &g, &b, &a));
            if (r || g || b) bright++;
            hash = hash * 33 + r;
        }
    }
    SDL_DestroySurface(pixels);
    if (expect_ink && bright <= 4) {
        fprintf(stderr, "%s U+%04X yielded %d bright pixels\n", ui_language(),
            (unsigned)SDL_StepUTF8(&glyph, NULL), bright);
        assert(bright > 4);
    }
    return hash;
}

static void check_sample(sdl_view *view, cptr code, cptr label, cptr glyph)
{
    const int size = 28, width = 90, height = 28;
    tutorial_set_language(streq(code, "ja") ? "ko" : "ja");
    Uint64 font_generation = g_story_font_generation;
    ui_set_language(code);
    assert(g_story_font_generation != font_generation);
    assert(streq(ui_language(), code));
    check_glyphs(sdl_story_font_for_height(size), code, label);
    check_glyphs(sdl_story_font_for_height_slot(size, SDL_STORY_FONT_SLOT_MENU), code, label);

    int px = sdl_ui_fitted_font_px(label, SDL_STORY_FONT_SLOT_MENU, size,
        width, height, false);
    TTF_Font *font = sdl_story_font_for_height_slot(px, SDL_STORY_FONT_SLOT_MENU);
    int w = 0, h = 0;
    assert(sdl_ui_text_texture(font, label, ink, &w, &h));
    assert(w > 4 && h > 4 && w <= width && h <= height);

    Uint64 localized = cell_pixels(view, glyph, true);
    ui_set_language("en");
    if (streq(code, "ja") || streq(code, "ko") || streq(code, "zh")) {
        /* Rasterize the same UTF-8 glyph without a matching interface face,
         * then switch back at the same size and compare the real glyph. */
        assert(localized != cell_pixels(view, glyph, false));
        ui_set_language(code);
        assert(localized == cell_pixels(view, glyph, true));
    }
}

static void check_picker(void)
{
    const char *regions[] = {"en", "ja", "ko", "zh", "uk"};
    size_t count;
    const tutorial_locale *locales = tutorial_available_locales(&count);
    for (size_t i = 0; i < N_ELEMENTS(regions); i++) {
        ui_set_language(regions[i]);
        tutorial_set_language(regions[(i + 1) % N_ELEMENTS(regions)]);
        TTF_Font *font = sdl_story_font_for_height_slot(24,
            SDL_STORY_FONT_SLOT_LANGUAGE_PICKER);
        for (size_t j = 0; j < count; j++)
            check_glyphs(font, locales[j].code, locales[j].name);
    }
}

static void check_touch_picker_scroll(size_t count)
{
    sdl_question_menu_layout_info layout;
    assert(sdl_question_menu_layout(&layout));
    assert(layout.scrollable && layout.row_h >= sdl_ui_min_tap_px());
    size_t last = count - 1;
    assert(streq(g_question_menu.entries[last].text, "한국어"));
    assert(!layout.rows[last].h);

    ui_menu_click_begin();
    const SDL_FingerID finger = 1;
    for (int i = 0; i < 4 && !layout.rows[last].h; i++) {
        float x = layout.entries_rect.x + layout.entries_rect.w / 2;
        float y = layout.entries_rect.y + layout.entries_rect.h - layout.row_h;
        float end_y = layout.entries_rect.y + layout.row_h;
        assert(sdl_question_menu_handle_touch_down(x, y, finger));
        assert(sdl_question_menu_handle_touch_motion(x, end_y, finger));
        assert(sdl_question_menu_handle_touch_up(x, end_y, finger));
        assert(sdl_question_menu_layout(&layout));
    }
    SDL_FRect last_row = layout.rows[last];
    assert(last_row.h >= sdl_ui_min_tap_px());
    assert(last_row.y >= layout.entries_rect.y
        && last_row.y + last_row.h <= layout.entries_rect.y + layout.entries_rect.h);
    float x = last_row.x + last_row.w / 2;
    float y = last_row.y + last_row.h / 2;
    assert(sdl_question_menu_handle_touch_down(x, y, finger));
    assert(sdl_question_menu_handle_touch_up(x, y, finger));
    int choice = -1, action = 0;
    assert(ui_menu_click_take_action(&choice, &action));
    assert(choice == (int)last && action == UI_MENU_CLICK_PRIMARY);
    ui_menu_click_clear();
    Term_flush();
}

static void capture_picker(cptr out_dir, int width, int height, cptr code,
    cptr title, cptr description)
{
    char filename[1024];
    size_t count;
    const tutorial_locale *locales = tutorial_available_locales(&count);
    bool portrait = width < height;
    touch_picker_fixture = portrait;
    g_startup_device_class = portrait ? SDL_STARTUP_DEVICE_MOBILE_TOUCH
        : SDL_STARTUP_DEVICE_DESKTOP;
    SDL_Rect screen = {0, 0, width, height};
    g_state.safe_area = screen;
    sdl_view *view = &g_views[PANE_MAIN];
    view->rect = screen;
    view->cols = width / view->cell_w;
    view->rows = height / view->cell_h;
    assert(SDL_SetWindowSize(g_state.window, width, height));
    assert(!Term_resize(view->cols, view->rows));
    SDL_Texture *target = SDL_CreateTexture(g_state.renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    assert(target && SDL_SetRenderTarget(g_state.renderer, target));
    assert(SDL_SetRenderDrawColor(g_state.renderer, 20, 25, 31, 255));
    assert(SDL_RenderClear(g_state.renderer));

    ui_set_language(code);
    tutorial_set_language(streq(code, "ja") ? "zh" : "ja");
    sdl_question_menu_begin(title);
    sdl_question_menu_use_language_picker_fonts();
    sdl_question_menu_set_desc(description);
    int highlight = 0;
    for (size_t i = 0; i < count; i++) {
        const char *shortcut = streq(locales[i].code, "en") ? "e)"
            : streq(locales[i].code, "de") ? "d)" : "";
        sdl_question_menu_add_entry((int)i, shortcut, locales[i].name, TERM_WHITE);
        if (streq(locales[i].code, code)) highlight = (int)i;
    }
    sdl_question_menu_set_highlight(highlight);
    sdl_question_menu_finish();
    assert(g_question_menu.count == (int)count);
    SDL_FRect panel;
    assert(sdl_question_menu_current_rect(&panel));
    assert(panel.x >= 0 && panel.y >= 0 && panel.x + panel.w <= width
        && panel.y + panel.h <= height);
    sdl_question_menu_render();
    SDL_Surface *pixels = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(pixels);
    strnfmt(filename, sizeof(filename), "%s/picker-%s-%s.png", out_dir, code,
        portrait ? "portrait" : "desktop");
    assert(IMG_SavePNG(pixels, filename));
    SDL_DestroySurface(pixels);
    if (portrait && streq(code, "en"))
        check_touch_picker_scroll(count);
    sdl_question_menu_clear();
    assert(SDL_SetRenderTarget(g_state.renderer, NULL));
    SDL_DestroyTexture(target);
}

static void capture_pickers(cptr out_dir)
{
    const struct { cptr code, title, description; } cases[] = {
        {"en", "Interface language", "Choose the language for menus, controls, status text, and messages."},
        {"ru", "Язык интерфейса", "Выберите язык меню, элементов управления, строк состояния и сообщений."},
        {"ja", "インターフェースの言語", "メニュー、操作、ステータス表示、メッセージの言語を選択します。"},
        {"ko", "인터페이스 언어", "메뉴, 조작, 상태 표시 및 메시지의 언어를 선택합니다."},
        {"zh", "界面语言", "选择菜单、操作、状态文字和消息的语言。"},
    };
    for (size_t i = 0; i < N_ELEMENTS(cases); i++) {
        capture_picker(out_dir, 1280, 720, cases[i].code, cases[i].title,
            cases[i].description);
        capture_picker(out_dir, 390, 844, cases[i].code, cases[i].title,
            cases[i].description);
    }
}

static int check_catalogues(cptr path)
{
    char *contents = SDL_LoadFile(path, NULL);
    assert(contents);
    cJSON *catalogues = cJSON_Parse(contents), *item;
    SDL_free(contents);
    assert(cJSON_IsObject(catalogues));
    int count = 0;
    cJSON_ArrayForEach(item, catalogues) {
        cptr code = item->string;
        assert(cJSON_IsString(item));
        ui_set_language(code);
        assert(streq(ui_language(), code));
        tutorial_set_language(streq(code, "ja") ? "zh" : "ja");
        for (int slot = 0; slot < STORY_FONT_SLOTS; slot++)
            check_glyphs(sdl_story_font_for_height_slot(22, slot), code, item->valuestring);
        count++;
    }
    cJSON_Delete(catalogues);
    return count;
}

int main(int argc, char **argv)
{
    assert(argc == 2 || argc == 3);
    log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO) && TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font, "lib/xtra/font/Cinzel-Medium.ttf", sizeof(config.story_font));
    SDL_strlcpy(config.story_font2, "lib/xtra/font/MarcellusSC-Regular.ttf", sizeof(config.story_font2));
    SDL_strlcpy(config.monospace_font, "lib/xtra/font/VictorMono-Medium.ttf", sizeof(config.monospace_font));
    g_state.window = SDL_CreateWindow("Interface font fixture", 640, 192, SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, "software");
    assert(g_state.renderer);
    for (int i = 0; i < 256; i++) {
        const byte *color = angband_color_table[i];
        g_state.palette[i] = (SDL_Color){color[1], color[2], color[3], 255};
    }
    sdl_view *view = &g_views[PANE_MAIN];
    view->cols = 40;
    view->rows = 8;
    view->cell_w = 16;
    view->cell_h = 24;
    view->rect = (SDL_Rect){0, 0, 640, 192};
    view->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, view->rect.w, view->rect.h);
    assert(view->canvas);
    view->font_atlas = sdl_load_ttf_font_cells(config.monospace_font,
        view->cell_w, view->cell_h, NULL);
    assert(view->font_atlas && !sdl_view_link_term(view, PANE_MAIN));
    Term_activate(&view->t);
    term_screen = Term;
    sdl_set_present_suppressed(true);
    check_sample(view, "ro", "Inițializare și opțiuni", "Ș");
    check_sample(view, "uk", "Спорядження й зброя", "Ї");
    check_sample(view, "ja", "持ち物と装備", "持");
    check_sample(view, "ko", "소지품과 장비", "품");
    check_sample(view, "zh", "物品和装备", "装");
    check_picker();
    int count = check_catalogues(argv[1]);
    if (argc == 3)
        capture_pickers(argv[2]);
    printf("SDL UI fonts, native picker labels, terminal pixels and %d catalogue repertoires passed\n",
        count);
    return 0;
}
'''


def repertoire(path):
    catalogue = json.loads(path.read_text(encoding="utf-8"))
    text = {char for domain in ("strings", "messages")
            for entry in catalogue[domain].values() for char in entry
            if not char.isspace()}
    return "".join(sorted(text))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    checks = parser.add_mutually_exclusive_group()
    checks.add_argument("--samples-only", action="store_true")
    checks.add_argument("--language", choices=LANGUAGES, action="append")
    parser.add_argument("--capture", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-question-menu.c.obj")
    response.write_text("\n".join(f'"{obj}"' for obj in objects
                                  if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=sdl_touch_only_device_active",
                    "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    locales = () if args.samples_only else args.language or LANGUAGES
    repertoires = {code: repertoire(ROOT / f"lib/help/ui.{code}.json") for code in locales}
    fixtures = OUT / "repertoires.json"
    fixtures.write_text(json.dumps(repertoires, ensure_ascii=False), encoding="utf-8")
    command = [str(exe), str(fixtures)]
    if args.capture:
        command.append(str(OUT))
    subprocess.run(command, cwd=ROOT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()

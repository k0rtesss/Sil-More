#!/usr/bin/env python3
"""Replay handheld pointer/finger input through real SDL menus, without saves.

Requires a current standard Windows or native Linux Steam build. Override the
build tree with SIL_TOUCH_INPUT_BUILD. Device enumeration and pointer packets
are fixtures; this does not establish physical Steam Deck hardware coverage.
"""
from pathlib import Path
import json
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get("SIL_TOUCH_INPUT_BUILD", ROOT /
    ("build-standard" if os.name == "nt" else "build-linux-steam"))).resolve()
OUT = ROOT / "scripts/output/handheld-touch-check"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "sdl/ui/sdl-question-menu.c"
#include <assert.h>

static errr extra(int action, int value) { return 0; }
static errr text(int x, int y, int n, byte attr, const char* s) { return 0; }
static errr wipe(int x, int y, int n) { return 0; }
static errr cursor(int x, int y) { return 0; }

static void empty_keys(void)
{
    char key;
    while (!Term_inkey(&key, false, true)) {}
}

static SDL_FPoint open_menu(void)
{
    sdl_question_menu_clear(); ui_menu_click_begin(); empty_keys();
    ui_menu_click_add(111, 0, 0, 1); ui_menu_click_add(222, 0, 1, 1);
    sdl_question_menu_begin("Touch input regression");
    sdl_question_menu_add_entry(111, "a", "First answer", TERM_WHITE);
    sdl_question_menu_add_entry(222, "b", "Second answer", TERM_WHITE);
    sdl_question_menu_finish();
    sdl_question_menu_layout_info layout;
    assert(sdl_question_menu_layout(&layout));
    assert(layout.rows[1].w > 0 && layout.rows[1].h > 0);
    return (SDL_FPoint){layout.rows[1].x + layout.rows[1].w * 0.5f,
        layout.rows[1].y + layout.rows[1].h * 0.5f};
}

static void check_choice(bool expected)
{
    int choice = -1, action = -1;
    bool selected = ui_menu_click_take_action(&choice, &action);
    assert(selected == expected);
    if (expected) {
        assert(choice == 222 && action == UI_MENU_CLICK_PRIMARY);
        assert(!ui_menu_click_take_action(&choice, &action));
        char key;
        assert(!Term_inkey(&key, false, true) && key == '\r');
        assert(Term_inkey(&key, false, true));
    } else {
        char key;
        assert(Term_inkey(&key, false, true));
    }
    assert(!g_touch_mouse_fallback_active);
}

static void pointer(SDL_FPoint point, SDL_MouseID id, bool expected)
{
    float x, y;
    assert(SDL_RenderCoordinatesToWindow(g_state.renderer,
        point.x, point.y, &x, &y));
    SDL_Event event = {0};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.which = id; event.button.button = SDL_BUTTON_LEFT;
    event.button.windowID = SDL_GetWindowID(g_state.window);
    event.button.x = x; event.button.y = y; event.button.down = true;
    sdl_handle_event(&g_state, &event);
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.x = x; event.button.y = y; event.button.down = false;
    sdl_handle_event(&g_state, &event);
    check_choice(expected);
}

static void finger(SDL_FPoint point)
{
    float x, y;
    int w, h;
    assert(SDL_GetWindowSize(g_state.window, &w, &h));
    assert(SDL_RenderCoordinatesToWindow(g_state.renderer,
        point.x, point.y, &x, &y));
    SDL_Event event = {0};
    event.type = SDL_EVENT_FINGER_DOWN;
    event.tfinger.touchID = 123; event.tfinger.fingerID = 456;
    event.tfinger.windowID = SDL_GetWindowID(g_state.window);
    event.tfinger.x = x / w; event.tfinger.y = y / h;
    sdl_handle_event(&g_state, &event);
    event.type = SDL_EVENT_FINGER_UP;
    sdl_handle_event(&g_state, &event);
    check_choice(true);
}

static int legacy_offset;

static void open_legacy_menu(void)
{
    sdl_question_menu_clear(); sdl_menu_touch_cancel();
    sdl_menu_scroll_cancel(); ui_scroll_area_clear();
    ui_menu_click_begin(); empty_keys(); legacy_offset = 10;
    ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
    for (int row = 5; row < 11; ++row)
        ui_menu_click_add(row < 8 ? 111 : 222, 0, row, 40);
    ui_scroll_area_begin_cols(0, 39, 5, 10,
        SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
    ui_scroll_area_set_offset_target(&legacy_offset, 20);
}

static void check_legacy_choice(int expected, int expected_action)
{
    int choice = -1, action = -1;
    assert(ui_menu_click_take_action(&choice, &action) == (expected != 0));
    if (expected) {
        assert(choice == expected && action == expected_action);
        assert(!ui_menu_click_take_action(&choice, &action));
        char key;
        assert(!Term_inkey(&key, false, true));
        assert(key == (action == UI_MENU_CLICK_SECONDARY
            ? UI_MENU_CLICK_WAKE_KEY : '\r'));
    }
    char key;
    assert(Term_inkey(&key, false, true));
}

static void legacy_tap(float x, float y, float end_x, float end_y,
    bool motion, bool mouse, int expected)
{
    open_legacy_menu();
    assert(sdl_menu_touch_handle_pointer_down(x, y, 456, mouse));
    if (motion)
        assert(sdl_menu_touch_handle_pointer_motion(end_x, end_y, 456, mouse));
    if (g_menu_touch_press.active)
        assert(sdl_menu_touch_handle_pointer_up(end_x, end_y, 456, mouse));
    if (ui_menu_click_has_pending() != (expected != 0))
        fprintf(stderr, "Legacy tap %.1f,%.1f -> %.1f,%.1f motion=%d mouse=%d expected=%d\n",
            x, y, end_x, end_y, motion, mouse, expected);
    check_legacy_choice(expected, UI_MENU_CLICK_PRIMARY);
    if (expected) assert(legacy_offset == 10 && !g_menu_scroll_drag.active);
}

static void legacy_finger(float y, float end_y)
{
    open_legacy_menu();
    int w, h;
    assert(SDL_GetWindowSize(g_state.window, &w, &h));
    const Uint32 types[] = {SDL_EVENT_FINGER_DOWN,
        SDL_EVENT_FINGER_MOTION, SDL_EVENT_FINGER_UP};
    for (int i = 0; i < 3; ++i) {
        float x, window_y;
        assert(SDL_RenderCoordinatesToWindow(g_state.renderer,
            160, i ? end_y : y, &x, &window_y));
        SDL_Event event = {0}; event.type = types[i];
        event.tfinger.touchID = 123; event.tfinger.fingerID = 456;
        event.tfinger.windowID = SDL_GetWindowID(g_state.window);
        event.tfinger.x = x / w; event.tfinger.y = window_y / h;
        sdl_handle_event(&g_state, &event);
        if (i < 2) assert(g_menu_touch_press.active);
    }
    assert(!g_menu_touch_press.active && legacy_offset == 10);
    check_legacy_choice(111, UI_MENU_CLICK_PRIMARY);
}

static void check_wrapped_menu(void)
{
    sdl_view* view = &g_views[PANE_MAIN];
    view->term_ready = true; view->cols = 40; view->rows = 24;
    view->cell_w = 16; view->cell_h = 32;
    view->rect = (SDL_Rect){0, 0, 640, 768};
    view->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, 640, 768);
    assert(view->canvas);
    config.touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT] = true;

    /* Replay native finger packets at both renderer scales as well. */
    for (int scaled = 0; scaled < 2; ++scaled) {
        assert(SDL_SetRenderScale(g_state.renderer,
            scaled ? 1.5f : 1.0f, scaled ? 1.5f : 1.0f));
        legacy_finger(191.5f, 192.5f);
        legacy_finger(192.5f, 191.5f);
    }

    /* Every column of every continuation line belongs to its item. */
    for (int row = 5; row < 11; ++row)
        for (int col = 0; col < 40; ++col)
            legacy_tap(col * 16 + 8, row * 32 + 16,
                col * 16 + 8, row * 32 + 16, false, false,
                row < 8 ? 111 : 222);

    for (int mouse = 0; mouse < 2; ++mouse)
        for (int motion = 0; motion < 2; ++motion) {
            /* A one-pixel wobble across a column or wrapped line stays a tap. */
            for (int direction = -1; direction <= 1; direction += 2) {
                legacy_tap(160 - direction * .5f, 176,
                    160 + direction * .5f, 176, motion, mouse, 111);
                for (int boundary = 6; boundary <= 7; ++boundary)
                    legacy_tap(160, boundary * 32 - direction * .5f,
                        160, boundary * 32 + direction * .5f,
                        motion, mouse, 111);
                /* Crossing into the next item must never select either one. */
                legacy_tap(160, 256 - direction * .5f,
                    160, 256 + direction * .5f, motion, mouse, 0);
            }
            /* A drag exceeding the threshold cancels even within one item. */
            legacy_tap(160, 160.5f, 160,
                161.5f + sdl_touch_swipe_threshold_px(), motion, mouse, 0);
        }

    /* Real vertical drags still hand off to scrolling without selecting. */
    open_legacy_menu();
    assert(sdl_menu_touch_handle_pointer_down(160, 160.5f, 456, false));
    assert(sdl_menu_touch_handle_pointer_motion(160, 226, 456, false));
    assert(!g_menu_touch_press.active && g_menu_scroll_drag.active);
    assert(legacy_offset != 10);
    sdl_menu_scroll_cancel(); empty_keys(); check_legacy_choice(0, 0);

    /* A long press survives the same small movement and fires exactly once. */
    open_legacy_menu();
    assert(sdl_menu_touch_handle_pointer_down(160, 191.5f, 456, false));
    assert(sdl_menu_touch_handle_pointer_motion(160, 192.5f, 456, false));
    assert(sdl_menu_touch_flush_pending_press(g_menu_touch_press.start_time
        + (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL));
    check_legacy_choice(111, UI_MENU_CLICK_SECONDARY);
    assert(!sdl_menu_touch_handle_pointer_up(160, 192.5f, 456, false));
    check_legacy_choice(0, 0);

    /* Cancellation and another finger cannot finish the original press. */
    open_legacy_menu();
    assert(sdl_menu_touch_handle_pointer_down(160, 176, 456, false));
    assert(!sdl_menu_touch_handle_pointer_motion(161, 176, 789, false));
    assert(!sdl_menu_touch_handle_pointer_up(161, 176, 789, false));
    assert(g_menu_touch_press.active);
    sdl_menu_touch_cancel();
    assert(!sdl_menu_touch_handle_pointer_up(160, 176, 456, false));
    check_legacy_choice(0, 0);

    ui_scroll_area_clear(); ui_menu_click_clear();
    SDL_DestroyTexture(view->canvas); view->canvas = NULL; view->term_ready = false;
    puts("Wrapped menu: full rows, cell-boundary taps, scrolling, long press and cancellation PASS");
}

int main(int argc, char** argv)
{
    assert(argc == 2); log_set_quiet(true);
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)); assert(TTF_Init());
    term t; assert(!term_init(&t, 80, 24, 256)); Term_activate(&t); term_screen = &t;
    t.xtra_hook = extra; t.text_hook = text; t.wipe_hook = wipe; t.curs_hook = cursor;
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font, argv[1], sizeof(config.story_font));
    SDL_strlcpy(config.story_font2, argv[1], sizeof(config.story_font2));
    g_state.system_scale = 1;
    g_state.window = SDL_CreateWindow("Handheld touch fixture", 1280, 800, 0);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, NULL);
    assert(g_state.renderer);

    int cases = 0;
    for (int scaled = 0; scaled < 2; ++scaled) {
        assert(SDL_SetRenderScale(g_state.renderer,
            scaled ? 1.5f : 1.0f, scaled ? 1.5f : 1.0f));
        for (int direct = 0; direct < 2; ++direct) {
            g_direct_touch_present = direct;
            config.mouse_enabled = false;
            g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP_HANDHELD;
            /* Gamescope may list a virtual touchscreen but deliver only
             * ordinary seat-pointer clicks. Enumeration must not drop them. */
            printf("handheld pointer: scale=%d enumerated_touch=%d\n", scaled, direct);
            fflush(stdout);
            pointer(open_menu(), 0, true); ++cases;
            g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER;
            pointer(open_menu(), 0, true); ++cases;
            g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP;
            pointer(open_menu(), 0, false); ++cases;
            config.mouse_enabled = true;
            pointer(open_menu(), 0, true); ++cases;
            config.mouse_enabled = false;
            g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP_HANDHELD;
            /* SDL-tagged duplicate mouse packets remain suppressed when
             * native finger input is present. The finger selects once. */
            pointer(open_menu(), SDL_TOUCH_MOUSEID, !direct); ++cases;
            g_direct_touch_present = true;
            finger(open_menu()); ++cases;
        }
    }
    check_wrapped_menu();
    sdl_question_menu_clear(); ui_menu_click_clear();
    sdl_ui_text_cache_clear(); sdl_story_font_cache_clear();
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer = NULL;
    SDL_DestroyWindow(g_state.window); g_state.window = NULL;
    term_nuke(&t); TTF_Quit(); SDL_Quit();
    printf("Handheld touch input: %d real SDL menu cases PASS\n", cases);
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIO_DRIVER="dummy")
    env["PATH"] = os.pathsep.join([
        str(BUILD), *(str(BUILD / "_deps" / dep)
                      for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    objdir = "CMakeFiles/sil-more.dir"
    if os.name == "nt":
        objects = shlex.split((BUILD / objdir / "objects1.rsp").read_text())
        compiler = ["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    f"@{objdir}/includes_C.rsp"]
        libraries = [f"@{objdir}/linkLibs.rsp"]
    else:
        entries = json.loads((BUILD / "compile_commands.json").read_text())
        flags = shlex.split(next(e["command"] for e in entries
                                if e["file"].endswith("/src/sdl/core/sdl-events.c")))
        compiler = [flags[0], "-DUSE_SDL", "-std=gnu17", "-O0",
                    *(f for f in flags if f.startswith(("-I", "-D")))]
        commands = subprocess.check_output(["ninja", "-t", "commands", "sil-more"],
                                           cwd=BUILD, text=True)
        link = shlex.split(next(line for line in commands.splitlines()
                                if " -o sil-more " in line))
        objects = [p for p in link if p.endswith(".o")]
        start = link.index("-o") + 2
        libraries = link[start:link.index("&&", start)] if "&&" in link[start:] else link[start:]
        libraries.append("-lstdc++")  # Steam adapter objects use the C++ runtime.
    objects = [p for p in objects if not p.endswith((
        "/src/main.c.obj", "/src/main.c.o", "/src/sdl/ui/sdl-question-menu.c.obj",
        "/src/sdl/ui/sdl-question-menu.c.o"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    exe = OUT / ("check.exe" if os.name == "nt" else "check")
    subprocess.run([*compiler, "-UNDEBUG", str(source), "@" + str(response), *libraries,
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf")],
                   cwd=BUILD, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

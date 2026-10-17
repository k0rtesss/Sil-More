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

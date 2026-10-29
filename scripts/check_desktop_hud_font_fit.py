#!/usr/bin/env python3
"""Keep modal overlay capacity from reducing automatic desktop HUD fonts."""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/desktop-hud-font-fit"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>
#include <stdio.h>

/* Fail directly instead of opening the Windows assertion dialog. */
#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); } } while (0)

static float density;
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display)
{
    (void)display;
    return density;
}

static void modal_panes_enabled(bool enabled)
{
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_DESCRIPTION
            || pane_config[i].pane == PANE_OVERLAY_MENU)
            pane_config[i].enabled = enabled;
    }
}

static void check(int width, int height, float scale, bool big)
{
    const int hud[] = {PANE_LEFT_PANEL, PANE_ROLLS, PANE_COMBAT,
        PANE_DEPTH, PANE_STATUS};
    int expected[N_ELEMENTS(hud)];
    SDL_Rect screen = {0, 0, width, height}, panes[PANE_MAX] = {0};

    density = scale;
    g_state.system_scale = scale;
    g_state.window = SDL_CreateWindow("Desktop HUD font fixture", width,
        height, SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    sdl_store_platform_max_main_view_scales(width, height);
    sdl_reset_config_to_resolution_defaults(width, height);
    memcpy(pane_config, default_pane_config,
        sizeof(struct pane_config) * default_pane_config_count);
    pane_config_count = default_pane_config_count;
    sdl_normalize_unified_log_pane_config(pane_config, &pane_config_count, true);
    config.use_unsafe_area = true;
    config.bigger_font = big;
    for (int menu = 0; menu < SDL_MENU_FONT_COUNT; menu++)
        config.menu_bigger_font[menu] = big;

    modal_panes_enabled(false);
    for (int i = 0; i < N_ELEMENTS(hud); i++)
        expected[i] = sdl_effective_pane_cell_height_for_type(hud[i]);
    modal_panes_enabled(true);
    for (int i = 0; i < N_ELEMENTS(hud); i++) {
        int actual = sdl_effective_pane_cell_height_for_type(hud[i]);
        if (actual != expected[i]) {
            fprintf(stderr, "%dx%d @%.2f big=%d pane=%d: %d px, expected %d px\n",
                width, height, scale, big, hud[i], actual, expected[i]);
            exit(EXIT_FAILURE);
        }
    }
    /* Retain the established proportional sizes on the reported display. */
    if (width == 2880 && height == 1800 && scale == 2 && !big) {
        assert(expected[0] == 48);
        assert(expected[1] == 44);
    }
    /* The font preview must leave real modal placement and saved choices intact. */
    sdl_place_active_panes(&screen, panes, false, false, false);
    assert(sdl_rect_has_area(&panes[PANE_DESCRIPTION]));
    for (int i = 0; i < pane_config_count; i++) {
        if (pane_config[i].pane == PANE_DESCRIPTION
            || pane_config[i].pane == PANE_OVERLAY_MENU)
            assert(pane_config[i].enabled);
    }
    config.aux_view_font_size = 18;
    assert(sdl_effective_pane_cell_height_for_type(PANE_LEFT_PANEL)
        == sdl_aux_cell_height_for_font_size(sdl_ui_font_px(18)));
    printf("%dx%d @%.2f big=%d: character=%d log=%d; modal and explicit sizes PASS\n",
        width, height, scale, big, expected[0], expected[1]);
    SDL_DestroyWindow(g_state.window);
    g_state.window = NULL;
}

int main(void)
{
    player_type player = {0};
    maxima limits = {0};
    p_ptr = &player;
    z_info = &limits;
    setbuf(stdout, NULL);
    log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS));
    g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP;
    check(2880, 1800, 2, false);
    check(1280, 720, 1, false);
    check(1920, 1080, 1, false);
    check(1920, 1080, 1.5f, false);
    check(3840, 2160, 2, false);
    check(1280, 720, 1, true);
    check(2880, 1800, 2, true);
    SDL_Quit();
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/core/sdl-layout.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin"]
        + [str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")]
        + [env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/sdl/core/sdl-layout.c"), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=SDL_GetDisplayContentScale",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Render every achievement notification with real fonts and inspect text crops.

Uses an isolated SDL software renderer; never opens player saves or configuration.
Run after build-incremental.ps1. Screenshots go under scripts/output/achievement-notifications.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/achievement-notifications"

HARNESS = r'''
#include "angband.h"
#include "achievements.h"
#include "sdl/main-sdl-private.h"
#include "sdl/ui/sdl-main-menu.c"
#include <assert.h>

static int width, height, draws;
static float density;
static bool inspecting;
static player_type player;
SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){ 0, 0, width, height }; }
bool __wrap_sdl_overlay_pane_anchor_rect(int pane, SDL_Rect* rect)
{ *rect = (SDL_Rect){ 0, 0, width, height }; return true; }
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display)
{ return density; }
bool __real_SDL_RenderTexture(SDL_Renderer*, SDL_Texture*, const SDL_FRect*, const SDL_FRect*);
bool __wrap_SDL_RenderTexture(SDL_Renderer* renderer, SDL_Texture* texture,
    const SDL_FRect* source, const SDL_FRect* destination)
{
    if (inspecting) {
        float w, h;
        assert(source && destination && SDL_GetTextureSize(texture, &w, &h));
        /* Check the actual draw, including its final line, not just layout math. */
        assert(source->x == 0 && source->y == 0 && source->w == w && source->h == h);
        assert(destination->w == w && destination->h == h);
        assert(destination->x >= 0 && destination->y >= 0);
        assert(destination->x + w <= width && destination->y + h <= height);
        ++draws;
    }
    return __real_SDL_RenderTexture(renderer, texture, source, destination);
}
static void check_title(cptr title, int id, int big, int menu)
{
    char message[96];
    strnfmt(message, sizeof(message), "Achievement unlocked: %s", title);
    sdl_popup_notification_show(message);
    SDL_FRect panel, text;
    TTF_Font* font;
    assert(sdl_popup_notification_layout(&panel, &text, &font));
    assert(panel.x >= 0 && panel.y >= 0);
    assert(panel.x + panel.w <= width && panel.y + panel.h <= height);
    assert(text.x >= panel.x && text.y >= panel.y);
    assert(text.x + text.w <= panel.x + panel.w);
    assert(text.y + text.h <= panel.y + panel.h);
    int w, h;
    assert(TTF_GetStringSizeWrapped(font, message, 0, (int)text.w, &w, &h));
    assert(w <= text.w && h <= text.h);
    assert(SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255));
    assert(SDL_RenderClear(g_state.renderer));
    g_popup_notification.fade_in_end_ns = SDL_GetTicksNS();
    int previous = draws;
    inspecting = true;
    sdl_popup_notification_render();
    inspecting = false;
    assert(draws == previous + 1);
    if (width == 768 && menu && strstr(title, "King Who Kept")) {
        char path[128];
        strnfmt(path, sizeof(path), "notification-%dx%d-%s.png", width, height, big ? "big" : "normal");
        SDL_Surface* image = SDL_RenderReadPixels(g_state.renderer, NULL);
        assert(image && IMG_SavePNG(image, path));
        SDL_DestroySurface(image);
    }
}
int main(int argc, char** argv)
{
    assert(argc == 5); setbuf(stdout, NULL); log_set_quiet(true);
    width = atoi(argv[1]); height = atoi(argv[2]); density = (float)atof(argv[3]);
    assert(SDL_Init(SDL_INIT_VIDEO) && TTF_Init());
    sdl_config_set_defaults(&config);
    strnfmt(config.story_font, sizeof(config.story_font), "%s/Cinzel-Medium.ttf", argv[4]);
    strnfmt(config.story_font2, sizeof(config.story_font2), "%s/EBGaramond-Regular.ttf", argv[4]);
    strnfmt(config.monospace_font, sizeof(config.monospace_font), "%s/VictorMono-Medium.ttf", argv[4]);
    g_state.window = SDL_CreateWindow("Achievement notifications", width, height, SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, "software"); assert(g_state.renderer);
    g_state.system_scale = density; g_state.safe_area = (SDL_Rect){0, 0, width, height};
    g_state.palette[TERM_L_WHITE] = (SDL_Color){230, 230, 230, 255};
    g_state.palette[TERM_L_BLUE] = (SDL_Color){120, 160, 255, 255};
    p_ptr = &player; player.playing = true; character_generated = character_dungeon = true;
    sdl_view* view = &g_views[PANE_MAIN];
    assert(term_init(&view->t, 80, 24, 256) == 0);
    Term_activate(&view->t); term_screen = &view->t;
    view->term_ready = true; view->rect = (SDL_Rect){0, 0, width, height};
    config.main_view_scale = 2;
    for (int big = 0; big < 2; ++big) {
        config.bigger_font = big;
        for (int i = 0; i < SDL_MENU_FONT_COUNT; ++i) config.menu_bigger_font[i] = big;
        for (int menu = 0; menu < 2; ++menu) {
            config.show_main_menu_button = menu;
            for (int i = 0; i < SIL_ACH_COUNT; ++i)
                check_title(sil_achievement_get(i)->title, i, big, menu);
        }
    }
    printf("PASS: %d notification draws at %dx%d @%.2f, every title, normal/Big Font, menu shown/hidden, complete native text raster.\n", draws, width, height, density);
    sdl_ui_text_cache_clear(); sdl_story_font_cache_clear(); term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window);
    TTF_Quit(); SDL_Quit(); return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects if not obj.endswith(
        ("/src/main.c.obj", "/src/sdl/ui/sdl-main-menu.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env["SDL_VIDEO_DRIVER"] = "dummy"
    exe = OUT / "check.exe"
    wraps = ("SDL_RenderTexture", "sdl_get_layout_screen_rect", "sdl_overlay_pane_anchor_rect", "SDL_GetDisplayContentScale")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for w, h, density in ((768, 576, 1), (360, 800, 1), (800, 360, 1), (1280, 800, 2)):
        subprocess.run([str(exe), str(w), str(h), str(density), str(ROOT / "lib/xtra/font")],
                       cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

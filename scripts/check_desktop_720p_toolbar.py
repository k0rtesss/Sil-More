#!/usr/bin/env python3
"""Check Quick Access Auto persistence, settings, layout, and rendering.

Compiles the touched sources into an isolated harness using the existing build
dependencies; it does not rewrite the shared build or read personal data.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/desktop-720p-toolbar"

SETTINGS_HARNESS = r'''
#include "cmd/ui/cmd-ui-settings.c"
#include <assert.h>

static int picker_result, picker_default;
int __wrap_ui_question_ask_overlay_buttons(cptr title, cptr desc,
    const ui_question_option* options, int count,
    const ui_question_button* buttons, int button_count, int anchor_y,
    int anchor_x, int default_index)
{
    assert(streq(title, "Quick Access Size") && count == 31);
    assert(streq(options[0].label, "Auto") && streq(options[1].label, "Stretch"));
    picker_default = default_index;
    return picker_result;
}

void check_quick_access_settings(void)
{
    struct settings_value_choice choices[32];
    char labels[32][12], value[24];
    int count = touch_top_widget_size_choices(choices, labels);
    assert(count == 31);
    assert(choices[0].value == -1 && streq(choices[0].label, "Auto"));
    assert(choices[1].value == 0 && streq(choices[1].label, "Stretch"));
    assert(choices[2].value == 4 && streq(choices[2].label, "1x"));
    assert(choices[count - 1].value == 32);
    touch_top_widget_format_size(value, sizeof(value), SDL_TOUCH_TOP_PANEL_SIZE_AUTO);
    assert(streq(value, "Auto"));
    touch_top_widget_format_size(value, sizeof(value), SDL_TOUCH_TOP_PANEL_SIZE_STRETCH);
    assert(streq(value, "Stretch"));
    touch_top_widget_format_size(value, sizeof(value), 2.75f);
    assert(streq(value, "2.75x"));
    assert(touch_top_widget_next_size(SDL_TOUCH_TOP_PANEL_SIZE_AUTO, 1)
        == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH);
    assert(touch_top_widget_next_size(SDL_TOUCH_TOP_PANEL_SIZE_STRETCH, -1)
        == SDL_TOUCH_TOP_PANEL_SIZE_AUTO);
    assert(touch_top_widget_next_size(SDL_TOUCH_TOP_PANEL_SIZE_STRETCH, 1)
        == SDL_TOUCH_TOP_PANEL_SIZE_MIN);
    assert(touch_top_widget_next_size(SDL_TOUCH_TOP_PANEL_SIZE_MIN, -1)
        == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH);
    assert(touch_top_widget_next_size(SDL_TOUCH_TOP_PANEL_SIZE_MAX, 1)
        == SDL_TOUCH_TOP_PANEL_SIZE_AUTO);
    assert(touch_top_widget_next_size(SDL_TOUCH_TOP_PANEL_SIZE_AUTO, -1)
        == SDL_TOUCH_TOP_PANEL_SIZE_MAX);
    assert(touch_top_widget_next_size(2.75f, 1) == 3.0f);
    set_sdl_touch_top_panel_size(2.75f);
    picker_result = 0;
    assert(touch_top_widget_pick_size() && picker_default == 9);
    assert(get_sdl_touch_top_panel_size() == SDL_TOUCH_TOP_PANEL_SIZE_AUTO);
    picker_result = 1;
    assert(touch_top_widget_pick_size() && picker_default == 0);
    assert(get_sdl_touch_top_panel_size() == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH);
    picker_result = 2;
    assert(touch_top_widget_pick_size() && picker_default == 1);
    assert(get_sdl_touch_top_panel_size() == SDL_TOUCH_TOP_PANEL_SIZE_MIN);
    picker_result = -1;
    assert(!touch_top_widget_pick_size());
    assert(get_sdl_touch_top_panel_size() == SDL_TOUCH_TOP_PANEL_SIZE_MIN);
    puts("Settings: Auto, Stretch, fixed picker entries and both cycle directions PASS");
}
'''

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "sdl/input/sdl-touch-controls.c"

void check_quick_access_settings(void);
static float density;
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID display) { return density; }

static void check_persistence(void)
{
    struct sdl_config loaded;
    struct sdl_pane_profile restored[SDL_PANE_PROFILE_COUNT] = {{0}};
    int big = SDL_PANE_FONT_PROFILE_INDEX(true, SDL_PANE_ORIENTATION_LANDSCAPE,
        SDL_MIN_TERMINAL_NORMAL);
    int sibling = SDL_PANE_FONT_PROFILE_INDEX(true, SDL_PANE_ORIENTATION_LANDSCAPE,
        SDL_MIN_TERMINAL_COMPACT);

    sdl_config_set_defaults(&config);
    pane_config_count = 0;
    sdl_seed_all_pane_profiles_from_active();
    assert(g_pane_profiles[big].touch_top_panel_size == SDL_TOUCH_TOP_PANEL_SIZE_AUTO);
    g_pane_profiles[0].touch_top_panel_size = SDL_TOUCH_TOP_PANEL_SIZE_STRETCH;
    g_pane_profiles[1].touch_top_panel_size = 2.75f;
    g_pane_profiles[sibling].touch_top_panel_size = 4.25f;
    config.bigger_font = true;
    config.touch_top_panel_size = SDL_TOUCH_TOP_PANEL_SIZE_AUTO;
    assert(sdl_config_save("auto-profile.json", &config, g_pane_profiles,
        SDL_PANE_PROFILE_COUNT));
    assert(sdl_config_load("auto-profile.json", &loaded, restored,
        SDL_PANE_PROFILE_COUNT, NULL) == SDL_CONFIG_LOAD_OK);
    assert(loaded.touch_top_panel_size == SDL_TOUCH_TOP_PANEL_SIZE_AUTO);
    assert(restored[big].touch_top_panel_size == SDL_TOUCH_TOP_PANEL_SIZE_AUTO);
    assert(restored[0].touch_top_panel_size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH);
    assert(restored[1].touch_top_panel_size == 2.75f);
    assert(restored[sibling].touch_top_panel_size == 4.25f);

    for (int i = 0; i < 3; i++) {
        const float previous[] = {0.0f, 2.75f, 6.0f};
        config.touch_top_panel_size = previous[i];
        assert(sdl_config_save("legacy-size.json", &config, NULL, 0));
        assert(sdl_config_load("legacy-size.json", &loaded, NULL, 0, NULL)
            == SDL_CONFIG_LOAD_OK);
        assert(loaded.touch_top_panel_size == previous[i]);
        set_sdl_touch_top_panel_size(previous[i]);
        assert(get_sdl_touch_top_panel_size() == previous[i]);
    }
    FILE* legacy = fopen("legacy-zero.json", "wb");
    assert(legacy);
    fputs("{\"touchControl\":{\"topPanelSize\":0}}", legacy);
    fclose(legacy);
    assert(sdl_config_load("legacy-zero.json", &loaded, NULL, 0, NULL)
        == SDL_CONFIG_LOAD_OK);
    assert(loaded.touch_top_panel_size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH);
    set_sdl_touch_top_panel_size(SDL_TOUCH_TOP_PANEL_SIZE_AUTO);
    assert(get_sdl_touch_top_panel_size() == SDL_TOUCH_TOP_PANEL_SIZE_AUTO);
    assert(sdl_touch_top_panel_size_normalized(-2.0f)
        == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH);
    config.bigger_font = false;
    float normal_default = get_sdl_touch_top_panel_default_size();
    config.bigger_font = true;
    assert(get_sdl_touch_top_panel_default_size() == SDL_TOUCH_TOP_PANEL_SIZE_AUTO);
    char info[16384];
    get_sdl_config_info(info, sizeof(info));
    assert(strstr(info, "Quick Access Size: Auto"));
    set_sdl_touch_top_panel_size(SDL_TOUCH_TOP_PANEL_SIZE_STRETCH);
    get_sdl_config_info(info, sizeof(info));
    assert(strstr(info, "Quick Access Size: Stretch"));
    set_sdl_touch_top_panel_size(2.75f);
    get_sdl_config_info(info, sizeof(info));
    assert(strstr(info, "Quick Access Size: Fixed"));
    g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP_HANDHELD;
    assert(get_sdl_touch_top_panel_default_size() == normal_default);
    g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP;
    config.bigger_font = false;
    assert(get_sdl_touch_top_panel_default_size() == normal_default);
    puts("Default reset and diagnostics: desktop Auto, handheld and normal defaults PASS");
    puts("Persistence: Auto and prior Stretch/numeric sizes in active and saved profiles PASS");
}

static void check_icon(const SDL_FRect* button)
{
    SDL_Color white = {255, 255, 255, 255};
    float icon = MIN(button->w, button->h) * 0.68f;
    float max_icon = MIN(button->w, button->h)
        - sdl_touch_pane_clampf(button->w * 0.18f, 6.0f, 14.0f);
    icon = MIN(icon, max_icon);
    SDL_FRect rect = {button->x + (button->w - icon) * 0.5f,
        button->y + (button->h - icon) * 0.5f, icon, icon};
    int px = sdl_menu_font_px(MAX(12, (int)(rect.h * 0.74f)));
    px = sdl_ui_fitted_font_px("@", SDL_STORY_FONT_SLOT_MENU, px,
        MAX(1.0f, rect.w - 12.0f), MAX(1.0f, rect.h - 8.0f), false);
    TTF_Font* font = sdl_story_font_for_height_slot(MAX(8, px), SDL_STORY_FONT_SLOT_MENU);
    int w = 0, h = 0;
    assert(font && TTF_GetStringSize(font, "@", 0, &w, &h));
    assert(w <= MAX(1.0f, rect.w - 12.0f));
    assert(h <= MAX(1.0f, rect.h - 8.0f));

    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    assert(SDL_RenderClear(g_state.renderer));
    sdl_touch_top_panel_render_icon(button, TOUCH_BIND_TOGGLE_TILES, white, false);
    SDL_Surface* pixels = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(pixels);
    int count = 0;
    for (int y = 0; y < pixels->h; y++) for (int x = 0; x < pixels->w; x++) {
        Uint8 r,g,b,a;
        assert(SDL_ReadSurfacePixel(pixels, x, y, &r, &g, &b, &a));
        if (r > 32 || g > 32 || b > 32) {
            assert(x >= button->x && x < button->x + button->w);
            assert(y >= button->y && y < button->y + button->h);
            count++;
        }
    }
    assert(count > 10);
    SDL_Surface* reference = TTF_RenderText_Blended_Wrapped(font, "@", 0, white,
        MAX(1, (int)(rect.w - 12.0f)));
    assert(reference);
    int expected = 0;
    for (int y = 0; y < reference->h; y++) for (int x = 0; x < reference->w; x++) {
        Uint8 r,g,b,a;
        assert(SDL_ReadSurfacePixel(reference, x, y, &r, &g, &b, &a));
        if (a > 32) expected++;
    }
    printf("Icon pixels: painted %d, independent glyph %d\n", count, expected);
    assert(expected > 10 && count >= expected * 0.95f && count <= expected * 1.05f);
    SDL_DestroySurface(reference);
    SDL_DestroySurface(pixels);
}

static float check_grid(int width, int height, float scale, int rows,
    enum pane_placement place)
{
    SDL_Rect screen = {0, 0, width, height};
    SDL_FRect buttons[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT] = {{0}}, panel = {0};
    density = scale; g_state.system_scale = scale;
    assert(SDL_SetWindowSize(g_state.window, width, height));
    config.touch_top_panel_size = SDL_TOUCH_TOP_PANEL_SIZE_AUTO;
    config.touch_top_panel_rows = rows;
    assert(sdl_touch_top_panel_compute_layout_for_anchor(&screen, &screen, place,
        buttons, &panel));
    assert(panel.x >= -0.5f && panel.y >= -0.5f);
    assert(panel.x + panel.w <= width + 0.5f);
    assert(panel.y + panel.h <= height + 0.5f);
    memcpy(g_touch_top_panel_cached_buttons, buttons, sizeof(buttons));
    g_touch_top_panel_cached_layout_valid = true;
    g_touch_top_panel_cached_generation = g_sdl_present_generation;
    for (int i = 0; i < 6; i++) {
        SDL_FRect b = buttons[i];
        int slot = -1;
        assert(b.w > 0 && b.h > 0);
        assert(b.x >= panel.x - 0.6f && b.y >= panel.y - 0.6f);
        assert(b.x+b.w <= panel.x+panel.w+0.6f);
        assert(b.y+b.h <= panel.y+panel.h+0.6f);
        assert(sdl_touch_top_panel_point_to_slot(b.x+b.w/2, b.y+b.h/2, &slot));
        assert(slot == i);
        for (int j = 0; j < i; j++)
            assert(!SDL_HasRectIntersectionFloat(&b, &buttons[j]));
    }
    check_icon(&buttons[4]);
    printf("Auto %dx%d @%.2f, %d rows, placement %d: six fitted hitboxes and icon PASS\n",
        width, height, scale, rows, place);
    return buttons[0].h;
}

static void check_handheld_stretch(void)
{
    SDL_Rect screen = {0, 0, 1280, 720};
    SDL_FRect automatic[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT] = {{0}};
    SDL_FRect stretch[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT] = {{0}};
    SDL_FRect a = {0}, b = {0};
    g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP_HANDHELD;
    density = g_state.system_scale = 1;
    config.touch_top_panel_rows = 1;
    config.touch_top_panel_size = SDL_TOUCH_TOP_PANEL_SIZE_AUTO;
    assert(sdl_touch_top_panel_compute_layout_for_anchor(&screen, &screen,
        PLACE_BOTTOM_CENTER, automatic, &a));
    config.touch_top_panel_size = SDL_TOUCH_TOP_PANEL_SIZE_STRETCH;
    assert(sdl_touch_top_panel_compute_layout_for_anchor(&screen, &screen,
        PLACE_BOTTOM_CENTER, stretch, &b));
    assert(SDL_RectsEqualFloat(&a, &b));
    for (int i = 0; i < 6; i++) assert(SDL_RectsEqualFloat(&automatic[i], &stretch[i]));
    g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP;
    puts("Desktop-compiled handheld: transferred Auto uses existing Stretch geometry PASS");
}

static void check_obstacle(void)
{
    SDL_Rect screen = {0, 0, 640, 480};
    SDL_FRect buttons[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT] = {{0}}, panel = {0};
    SDL_FRect obstacle = {0, 380, 320, 100};
    density = g_state.system_scale = 1;
    pane_config_count = 1;
    pane_config[0] = (struct pane_config){.pane = PANE_LOG,
        .where = PLACE_BOTTOM_LEFT, .enabled = true};
    g_pane_rects[PANE_LOG] = (SDL_Rect){0, 380, 320, 100};
    config.touch_top_panel_size = SDL_TOUCH_TOP_PANEL_SIZE_AUTO;
    config.touch_top_panel_rows = 1;
    assert(sdl_touch_top_panel_compute_layout_for_anchor(&screen, &screen,
        PLACE_BOTTOM_CENTER, buttons, &panel));
    assert(!SDL_HasRectIntersectionFloat(&panel, &obstacle));
    assert(panel.x >= obstacle.x + obstacle.w);
    memcpy(g_touch_top_panel_cached_buttons, buttons, sizeof(buttons));
    g_touch_top_panel_cached_layout_valid = true;
    g_touch_top_panel_cached_generation = g_sdl_present_generation;
    for (int i = 0; i < 6; i++) {
        int slot = -1;
        assert(sdl_touch_top_panel_point_to_slot(buttons[i].x + buttons[i].w / 2,
            buttons[i].y + buttons[i].h / 2, &slot) && slot == i);
        assert(!SDL_HasRectIntersectionFloat(&buttons[i], &obstacle));
    }
    pane_config_count = 0;
    g_pane_rects[PANE_LOG] = (SDL_Rect){0};
    puts("Auto beside a live overlay: clear span and six hitboxes PASS");
}

int main(int argc, char** argv)
{
    maxima limits = {0};
    player_type player = {0};
    player_other options = {0};
    assert(argc == 3);
    setbuf(stdout, NULL);
    log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS));
    assert(TTF_Init());
    g_startup_device_class = SDL_STARTUP_DEVICE_DESKTOP;
    check_quick_access_settings();
    check_persistence();
    sdl_config_set_defaults(&config);
    config.bigger_font = true;
    for (int i = 0; i < SDL_MENU_FONT_COUNT; i++) config.menu_bigger_font[i] = true;
    config.touch_top_panel_cell_count = 6;
    config.touch_top_panel_arrows_visible = false;
    const int bindings[] = {'j', 'i', 'y', 'h', TOUCH_BIND_TOGGLE_TILES, 'm'};
    memcpy(config.touch_top_panel_bindings, bindings, sizeof(bindings));
    SDL_strlcpy(config.story_font, argv[1], sizeof(config.story_font));
    SDL_strlcpy(config.story_font2, argv[2], sizeof(config.story_font2));
    z_info = &limits; p_ptr = &player; op_ptr = &options; player.playing = true;
    g_state.window = SDL_CreateWindow("Auto toolbar fixture", 1280, 720, SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, "software");
    assert(g_state.renderer);
    float normal = check_grid(1280, 720, 1, 1, PLACE_BOTTOM_CENTER);
    float same_density = check_grid(1920, 1080, 1, 1, PLACE_BOTTOM_CENTER);
    float high_density = check_grid(1920, 1080, 2, 1, PLACE_BOTTOM_CENTER);
    assert(SDL_fabsf(normal - same_density) < 0.1f);
    assert(SDL_fabsf(2 * normal - high_density) < 0.1f);
    check_grid(320, 200, 1, 1, PLACE_BOTTOM_CENTER);
    check_grid(640, 480, 1, 2, PLACE_BOTTOM_CENTER);
    check_grid(800, 360, 2, 1, PLACE_BOTTOM_CENTER);
    check_grid(360, 800, 2, 2, PLACE_TOP_CENTER);
    check_grid(800, 360, 1, 1, PLACE_RIGHT_CENTER);
    check_obstacle();
    check_handheld_stretch();
    sdl_story_font_cache_clear(); sdl_ui_text_cache_clear();
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window);
    TTF_Quit(); SDL_Quit();
    return 0;
}
'''

MOBILE_HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    struct sdl_pane_profile p = {0};
    struct sdl_config cfg, loaded;
    log_set_quiet(true);
    sdl_pane_profile_apply_bigger_font_defaults(&p, SDL_PANE_ORIENTATION_PORTRAIT);
    assert(p.touch_top_panel_size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH);
    sdl_pane_profile_apply_bigger_font_defaults(&p, SDL_PANE_ORIENTATION_LANDSCAPE);
    assert(p.touch_top_panel_size == SDL_TOUCH_TOP_PANEL_SIZE_STRETCH);
    sdl_config_set_defaults(&cfg);
    const float saved[] = {SDL_TOUCH_TOP_PANEL_SIZE_STRETCH, 2.75f,
        SDL_TOUCH_TOP_PANEL_SIZE_AUTO};
    for (int i = 0; i < 3; i++) {
        cfg.touch_top_panel_size = saved[i];
        assert(sdl_config_save("mobile-size.json", &cfg, NULL, 0));
        assert(sdl_config_load("mobile-size.json", &loaded, NULL, 0, NULL)
            == SDL_CONFIG_LOAD_OK);
        assert(loaded.touch_top_panel_size == saved[i]);
    }
    puts("Mobile-compiled defaults: Stretch in both orientations; stored sizes preserved PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    settings = OUT / "settings.c"
    source.write_text(HARNESS, encoding="utf-8")
    settings.write_text(SETTINGS_HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = (
        "/src/main.c.obj", "/src/sdl-config.c.obj",
        "/src/sdl/input/sdl-touch-controls.c.obj", "/src/cmd/ui/cmd-ui-settings.c.obj",
        "/src/sdl/config/sdl-settings.c.obj", "/src/sdl/core/sdl-layout.c.obj",
    )
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + o + '"' for o in objects
                                  if not o.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin"] +
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]] +
        [env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(settings),
                    str(ROOT / "src/sdl-config.c"),
                    str(ROOT / "src/sdl/config/sdl-settings.c"),
                    str(ROOT / "src/sdl/core/sdl-layout.c"), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl,--wrap=SDL_GetDisplayContentScale,--wrap=ui_question_ask_overlay_buttons",
                    "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe), str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf"),
                    str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf")],
                   cwd=OUT, env=env, check=True, timeout=60)
    mobile_source = OUT / "mobile.c"
    mobile_source.write_text(MOBILE_HARNESS, encoding="utf-8")
    mobile_response = OUT / "mobile-objects.rsp"
    mobile_response.write_text("\n".join('"' + o + '"' for o in objects
        if not o.endswith(("/src/main.c.obj", "/src/sdl-config.c.obj"))), encoding="utf-8")
    mobile_exe = OUT / "mobile.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-DSIL_IOS",
                    "-std=c17", "-O0", "-g", "@CMakeFiles/sil-more.dir/includes_C.rsp",
                    str(mobile_source), str(ROOT / "src/sdl-config.c"),
                    "@" + str(mobile_response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(mobile_exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(mobile_exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "angband.h"

#include "fs/resource.h"
#include "sdl-main-internal.h"

static void sdl_quit_hook(cptr str)
{
    (void)str;

    platform_sound_shutdown();
    sdl_gamepad_shutdown();
    sdl_story_font_cache_clear();
    sdl_scene_stack_shutdown();

    if (g_state.window && config_file_path[0] != '\0') {
        if (!config.fullscreen) {
            SDL_GetWindowPosition(g_state.window, &config.window_x,
                &config.window_y);
            SDL_GetWindowSize(g_state.window, &config.window_width,
                &config.window_height);
            log_debug("Saving window position (%d, %d) and size (%dx%d)",
                config.window_x, config.window_y, config.window_width,
                config.window_height);
        }

        if (!sdl_config_save(config_file_path, &config, pane_config,
                pane_config_count))
        {
            log_error("Failed to save SDL configuration during shutdown: %s",
                config_file_path);
        }
    }
}

errr init_sdl(int argc, char **argv)
{
    log_debug("init_sdl starting");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO
            | SDL_INIT_GAMEPAD))
    {
        log_error("SDL_Init failed: %s", SDL_GetError());
        quit("could not init SDL");
    }
    if (!TTF_Init()) {
        log_error("TTF_Init failed: %s", SDL_GetError());
        quit("could not init TTF");
    }

    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    if (!primary) {
        log_error("SDL_GetPrimaryDisplay failed: %s", SDL_GetError());
        quit("could not get primary display ID");
    }

    SDL_Rect screen;
    if (!SDL_GetDisplayBounds(primary, &screen)) {
        log_error("SDL_GetDisplayBounds failed: %s", SDL_GetError());
        quit("could not get primary display bounds");
    }
    log_info("primary display bounds (logical): %dx%d at (%d,%d)",
        screen.w, screen.h, screen.x, screen.y);

    const SDL_DisplayMode* desktop_mode = SDL_GetDesktopDisplayMode(primary);
    if (!desktop_mode) {
        log_error("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
        quit("could not get desktop display mode");
    }

    float pixel_density = desktop_mode->pixel_density;
    int screen_pixels_w = (int)(desktop_mode->w * pixel_density + 0.5f);
    int screen_pixels_h = (int)(desktop_mode->h * pixel_density + 0.5f);

    log_info("primary display desktop mode: %dx%d @%.2fHz, pixel_density=%.2f",
        desktop_mode->w, desktop_mode->h, desktop_mode->refresh_rate,
        pixel_density);
    log_info("primary display physical resolution for defaults: %dx%d",
        screen_pixels_w, screen_pixels_h);

    char config_file[1024];
    if (!resource_build_ui_config_path(config_file, sizeof(config_file)))
        SDL_strlcpy(config_file, "sil_sdl.json", sizeof(config_file));
    SDL_strlcpy(config_file_path, config_file, sizeof(config_file_path));

    log_register_quit_hook(sdl_quit_hook);

    bool config_exists = SDL_GetPathInfo(config_file_path, NULL);

    if (config_exists) {
        char sound_config_path[1024];

        log_debug("Config file exists, loading from: %s", config_file_path);
        sdl_config_set_defaults(&config);
        sdl_copy_default_pane_config();

        sdl_config_load(config_file_path, &config, pane_config,
            &pane_config_count, MAX_PANE_CONFIGS);

        if (!resource_build_active_sound_config_path(sound_config_path,
                sizeof(sound_config_path)))
        {
            SDL_strlcpy(sound_config_path, "sound.json",
                sizeof(sound_config_path));
        }
        sound_config_load(sound_config_path, &g_sound_config);
        use_sound = g_sound_config.enabled;

        log_debug("After loading JSON: scale=%d, default_aux_font=%d, menu_panel_font=%d, margin=%d, fullscreen=%d, tiles=%d, sound=%d",
            config.main_view_scale, config.aux_view_font_size,
            config.menu_panel_font_size, config.margin, config.fullscreen,
            config.tiles, g_sound_config.enabled);
    } else {
        log_debug("Config file not found, using resolution-based defaults");
        sdl_config_set_defaults_for_resolution(&config, pane_config,
            &pane_config_count, MAX_PANE_CONFIGS, screen_pixels_w,
            screen_pixels_h);

        if (pane_config_count == 0)
            sdl_copy_default_pane_config();

        log_debug("After resolution defaults: scale=%d, default_aux_font=%d, menu_panel_font=%d, margin=%d, fullscreen=%d, tiles=%d",
            config.main_view_scale, config.aux_view_font_size,
            config.menu_panel_font_size, config.margin, config.fullscreen,
            config.tiles);
    }

#if defined(__ANDROID__) || defined(SIL_IOS)
    sdl_ensure_default_pane_configs_present(false);
    sdl_ensure_touch_pane_config_present();

    if (!config_exists) {
        for (int i = 0; i < pane_config_count; i++) {
            if (pane_config[i].pane == PANE_TOUCH) {
                pane_config[i].enabled = true;
                pane_config[i].where = PLACE_DOUBLE_RIGHT;
            } else {
                pane_config[i].enabled = false;
            }
        }

        config.enable_right_panes = true;
        config.enable_bottom_panes = false;
        log_info("Mobile default pane layout: touch only enabled; other panes available in settings");
    }
#endif

    sdl_ensure_touch_pane_config_present();

    g_hide_left_panel = config.hide_left_panel;

    sdl_config_apply_cmdline(&config, argc, argv);
    log_debug("After command-line: scale=%d, default_aux_font=%d, menu_panel_font=%d, margin=%d, fullscreen=%d, tiles=%d",
        config.main_view_scale, config.aux_view_font_size,
        config.menu_panel_font_size, config.margin, config.fullscreen,
        config.tiles);

    if (!sdl_config_has_movement_bindings(&config)) {
        sdl_config_set_default_movement_bindings(&config,
            APP_MOVEMENT_PRESET_CLASSIC_SIL);
        log_info("Initialized default movement bindings: Classic Sil");
    }

#if defined(__ANDROID__) || defined(SIL_IOS)
    {
        int mobile_min_cols = platform_current_min_terminal_cols();
        int mobile_min_rows = platform_current_min_terminal_rows();
        int mobile_max_scale_w = (screen_pixels_w / mobile_min_cols) * 2
            / TILE_SIZE;
        int mobile_max_scale_h = screen_pixels_h / mobile_min_rows
            / TILE_SIZE;
        int mobile_max_scale = mobile_max_scale_w;

        if (mobile_max_scale_h < mobile_max_scale)
            mobile_max_scale = mobile_max_scale_h;
        if (mobile_max_scale < 1)
            mobile_max_scale = 1;

        if (!config_exists) {
            if (config.main_view_scale != mobile_max_scale) {
                log_info("Mobile default main_view_scale set to %d for >=%dx%d (%s) at %dx%d",
                    mobile_max_scale, mobile_min_cols, mobile_min_rows,
                    sdl_min_terminal_mode_name(config.min_terminal_mode),
                    screen_pixels_w, screen_pixels_h);
            }
            config.main_view_scale = mobile_max_scale;
        } else if (config.main_view_scale > mobile_max_scale) {
            log_info("Mobile main_view_scale clamped from %d to %d to keep >=%dx%d (%s)",
                config.main_view_scale, mobile_max_scale, mobile_min_cols,
                mobile_min_rows,
                sdl_min_terminal_mode_name(config.min_terminal_mode));
            config.main_view_scale = mobile_max_scale;
        }
    }
#endif

    if (config.main_view_scale <= 0) {
        log_warn("Invalid main_view_scale %d, using 1",
            config.main_view_scale);
        config.main_view_scale = 1;
    }
    if (config.overlay_density < SDL_OVERLAY_DENSITY_AUTO
        || config.overlay_density > SDL_OVERLAY_DENSITY_LARGE)
    {
        log_warn("Invalid overlay_density %d, using auto",
            config.overlay_density);
        config.overlay_density = SDL_OVERLAY_DENSITY_AUTO;
    }
    if (config.aux_view_font_size < 0) {
        log_warn("Invalid aux_view_font_size %d, using auto",
            config.aux_view_font_size);
        config.aux_view_font_size = 0;
    } else if (config.aux_view_font_size > 48) {
        log_warn("Invalid aux_view_font_size %d, clamping to 48",
            config.aux_view_font_size);
        config.aux_view_font_size = 48;
    }
    if (config.menu_panel_font_size < 0) {
        log_warn("Invalid menu_panel_font_size %d, using auto",
            config.menu_panel_font_size);
        config.menu_panel_font_size = 0;
    } else if (config.menu_panel_font_size > 64) {
        log_warn("Invalid menu_panel_font_size %d, clamping to 64",
            config.menu_panel_font_size);
        config.menu_panel_font_size = 64;
    }
    if (config.plain_menu_font_size < 0) {
        log_warn("Invalid plain_menu_font_size %d, using auto",
            config.plain_menu_font_size);
        config.plain_menu_font_size = 0;
    } else if (config.plain_menu_font_size > 64) {
        log_warn("Invalid plain_menu_font_size %d, clamping to 64",
            config.plain_menu_font_size);
        config.plain_menu_font_size = 64;
    }
    if (config.browser_menu_font_size < 0) {
        log_warn("Invalid browser_menu_font_size %d, using auto",
            config.browser_menu_font_size);
        config.browser_menu_font_size = 0;
    } else if (config.browser_menu_font_size > 64) {
        log_warn("Invalid browser_menu_font_size %d, clamping to 64",
            config.browser_menu_font_size);
        config.browser_menu_font_size = 64;
    }
    if (config.character_sheet_font_size < 0) {
        log_warn("Invalid character_sheet_font_size %d, using auto",
            config.character_sheet_font_size);
        config.character_sheet_font_size = 0;
    } else if (config.character_sheet_font_size > 64) {
        log_warn("Invalid character_sheet_font_size %d, clamping to 64",
            config.character_sheet_font_size);
        config.character_sheet_font_size = 64;
    }
    if (config.margin < 0) {
        log_warn("Invalid margin %d, using 0", config.margin);
        config.margin = 0;
    }
    if (!sdl_min_terminal_mode_is_valid(config.min_terminal_mode)) {
#if defined(__ANDROID__) || defined(SIL_IOS)
        log_warn("Invalid min_terminal_mode %d, using compact",
            config.min_terminal_mode);
        config.min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;
#else
        log_warn("Invalid min_terminal_mode %d, using normal",
            config.min_terminal_mode);
        config.min_terminal_mode = SDL_MIN_TERMINAL_NORMAL;
#endif
    }
    if (config.gamepad_deadzone < 0) {
        log_warn("Invalid gamepad_deadzone %d, using 0",
            config.gamepad_deadzone);
        config.gamepad_deadzone = 0;
    } else if (config.gamepad_deadzone > SDL_JOYSTICK_AXIS_MAX) {
        log_warn("Invalid gamepad_deadzone %d, clamping to %d",
            config.gamepad_deadzone, SDL_JOYSTICK_AXIS_MAX);
        config.gamepad_deadzone = SDL_JOYSTICK_AXIS_MAX;
    }
    if (config.gamepad_trigger_threshold < 0) {
        log_warn("Invalid gamepad_trigger_threshold %d, using 0",
            config.gamepad_trigger_threshold);
        config.gamepad_trigger_threshold = 0;
    } else if (config.gamepad_trigger_threshold > SDL_JOYSTICK_AXIS_MAX) {
        log_warn("Invalid gamepad_trigger_threshold %d, clamping to %d",
            config.gamepad_trigger_threshold, SDL_JOYSTICK_AXIS_MAX);
        config.gamepad_trigger_threshold = SDL_JOYSTICK_AXIS_MAX;
    }

    sdl_gamepad_init();

#if defined(__ANDROID__) || defined(SIL_IOS)
    if (!config_exists) {
        int gamepad_count = sdl_gamepad_count();

        config.steamdeck_mode = (gamepad_count > 0);
        log_info("Mobile first-start Steam Deck UI mode set to %s (%d gamepad%s detected)",
            config.steamdeck_mode ? "on" : "off", gamepad_count,
            (gamepad_count == 1) ? "" : "s");
    }
#endif

    log_info("SDL Configuration:");
    log_info("  Main view scale: %d", config.main_view_scale);
    log_info("  Overlay density: %d", config.overlay_density);
    if (config.aux_view_font_size > 0)
        log_info("  Default aux view font size: %d",
            config.aux_view_font_size);
    else
        log_info("  Default aux view font size: auto (%d)",
            sdl_auto_aux_view_font_size());
    if (config.menu_panel_font_size > 0)
        log_info("  Default menu + left panel font size: %d",
            config.menu_panel_font_size);
    else
        log_info("  Default menu + left panel font size: auto (%d)",
            sdl_resolve_menu_panel_font_size(config.menu_panel_font_size));
    if (config.plain_menu_font_size > 0)
        log_info("  Plain menu font size: %d", config.plain_menu_font_size);
    else
        log_info("  Plain menu font size: auto (%d)",
            sdl_effective_menu_font_size_for_panel_style(
                APP_UI_PANEL_STYLE_PLAIN));
    if (config.browser_menu_font_size > 0)
        log_info("  Browser menu font size: %d",
            config.browser_menu_font_size);
    else
        log_info("  Browser menu font size: auto (%d)",
            sdl_effective_menu_font_size_for_panel_style(
                APP_UI_PANEL_STYLE_BROWSER));
    if (config.character_sheet_font_size > 0)
        log_info("  Character sheet font size: %d",
            config.character_sheet_font_size);
    else
        log_info("  Character sheet font size: auto (%d)",
            sdl_effective_menu_font_size_for_panel_style(
                APP_UI_PANEL_STYLE_CHARACTER_SHEET));
    log_info("  Margin: %d", config.margin);
    log_info("  Fullscreen: %s", config.fullscreen ? "true" : "false");
    log_info("  Tiles: %s", config.tiles ? "true" : "false");
    log_info("  Use unsafe area: %s", config.use_unsafe_area ? "true" : "false");
    log_info("  Minimum terminal size: %s (%dx%d)",
        sdl_min_terminal_mode_name(config.min_terminal_mode),
        platform_current_min_terminal_cols(),
        platform_current_min_terminal_rows());
    log_info("  Pane configurations: %d", pane_config_count);
    log_info("  Palette preset: %s",
        config.palette_preset[0] ? config.palette_preset : "classic");

    ui_colors_load_palette_presets();
    if (!ui_colors_apply_palette_preset(config.palette_preset))
        ui_colors_apply_palette_preset("classic");
    SDL_strlcpy(config.palette_preset, ui_colors_current_palette_preset(),
        sizeof(config.palette_preset));

    sdl_sync_palette();

    platform_sound_reload();
    if (!platform_sound_initialize()) {
        log_info("Sound subsystem not initialized; continuing without audio output");
    }

    int window_width;
    int window_height;
    if (config.fullscreen) {
        window_width = screen.w;
        window_height = screen.h;
    } else {
        if (config.window_width > 0 && config.window_height > 0) {
            window_width = config.window_width;
            window_height = config.window_height;
            log_debug("Using saved window size: %dx%d", window_width,
                window_height);
        } else {
            window_width = screen.w * 3 / 4;
            window_height = screen.h * 3 / 4;
            log_debug("Using default window size: %dx%d", window_width,
                window_height);
        }
    }

    sdl_window_create(window_width, window_height, config.fullscreen,
        config.tiles);

    if (!config.fullscreen && config.window_x >= 0 && config.window_y >= 0)
        sdl_window_set_position(config.window_x, config.window_y);

    sdl_load_story_fonts();

    ANGBAND_SYS = "sdl";
    if (config.tiles) {
        ANGBAND_GRAF = "new";
        runtime_cli_set_graphics_mode(GRAPHICS_MICROCHASM);
        use_graphics = GRAPHICS_MICROCHASM;
        use_bigtile = true;
    } else {
        ANGBAND_GRAF = "old";
        runtime_cli_set_graphics_mode(GRAPHICS_PSEUDO);
        use_graphics = GRAPHICS_PSEUDO;
        use_bigtile = false;
    }

    sdl_refresh_safe_area();
    SDL_Rect window = sdl_get_layout_screen_rect();
    log_debug("layout pixel rect (%d,%d %dx%d)", window.x, window.y,
        window.w, window.h);
    resize(&window);
    sdl_scene_stack_init();

    log_debug("init_sdl: SDL views initialized (tiles_mode=%d)",
        config.tiles);

    return 0;
}

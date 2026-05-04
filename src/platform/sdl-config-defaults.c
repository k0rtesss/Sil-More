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

#include "log/log.h"
#include "sdl-config.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

static void sdl_config_clear_gamepad_combo_bindings(struct sdl_config* cfg)
{
    if (!cfg)
        return;

    for (int modifier = 0; modifier < GAMEPAD_MODIFIER_COUNT; modifier++) {
        for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++)
            cfg->gamepad_button_combo_bindings[modifier][i] = GAMEPAD_BIND_NONE;
        for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++)
            cfg->gamepad_trigger_combo_bindings[modifier][i] = GAMEPAD_BIND_NONE;
        for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
            cfg->gamepad_left_stick_combo_bindings[modifier][i] =
                GAMEPAD_BIND_NONE;
            cfg->gamepad_right_stick_combo_bindings[modifier][i] =
                GAMEPAD_BIND_NONE;
        }
    }
}

void sdl_config_set_default_gamepad_bindings(struct sdl_config* cfg)
{
    if (!cfg)
        return;

    for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++)
        cfg->gamepad_button_bindings[i] = GAMEPAD_BIND_NONE;
    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++)
        cfg->gamepad_trigger_bindings[i] = GAMEPAD_BIND_NONE;
    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        cfg->gamepad_left_stick_bindings[i] = GAMEPAD_BIND_NONE;
        cfg->gamepad_right_stick_bindings[i] = GAMEPAD_BIND_NONE;
    }
    sdl_config_clear_gamepad_combo_bindings(cfg);

    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_SOUTH] = ' ';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_EAST] = 'f';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_WEST] = 'u';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_NORTH] = 's';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_LEFT_SHOULDER] = 'e';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_RIGHT_SHOULDER] = 'i';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_START] = ESCAPE;
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_BACK] = 'h';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_LEFT_PADDLE1] = 'r';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_LEFT_PADDLE2] = 'o';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_RIGHT_PADDLE1] = 'q';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_RIGHT_PADDLE2] = '?';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_LEFT_STICK] = 'z';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_RIGHT_STICK] = 'j';

    cfg->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_RIGHT] = 'x';
    cfg->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_LEFT] = 'a';
    cfg->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_UP] = 'M';
    cfg->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_DOWN] = 'b';

    cfg->gamepad_trigger_bindings[0] = GAMEPAD_BIND_SHIFT;
    cfg->gamepad_trigger_bindings[1] = GAMEPAD_BIND_CTRL;

    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT]
        [GAMEPAD_BUTTON_SOUTH] = 'Z';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT]
        [GAMEPAD_BUTTON_EAST] = 'F';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT]
        [GAMEPAD_BUTTON_WEST] = 'x';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT]
        [GAMEPAD_BUTTON_NORTH] = 'S';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT]
        [GAMEPAD_BUTTON_LEFT_SHOULDER] = 'M';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT]
        [GAMEPAD_BUTTON_RIGHT_SHOULDER] = 'p';

    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL]
        [GAMEPAD_BUTTON_SOUTH] = 'z';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL]
        [GAMEPAD_BUTTON_EAST] = '-';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL]
        [GAMEPAD_BUTTON_WEST] = 'X';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL]
        [GAMEPAD_BUTTON_NORTH] = '0';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL]
        [GAMEPAD_BUTTON_BACK] = '\t';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL]
        [GAMEPAD_BUTTON_LEFT_SHOULDER] = 'a';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL]
        [GAMEPAD_BUTTON_RIGHT_SHOULDER] = 'j';

    cfg->gamepad_shoulder_combo_binding = 'l';
}

void sdl_config_set_default_touch_pane_bindings(struct sdl_config* cfg)
{
    static const int main_defaults[SDL_TOUCH_PANE_BUTTON_COUNT] = {
        ESCAPE, 'S', GAMEPAD_BIND_SHIFT,
        'h', 'i', 'j',
        'u', 's', 'f',
        '7', '8', '9',
        '4', INPUT_BIND_CONFIRM, '6',
        '1', '2', '3',
        'l', 'x', 'a',
        'M', 'h', '\t',
    };
    static const int second_defaults[SDL_TOUCH_PANE_BUTTON_COUNT] = {
        TOUCH_PANE_BIND_INHERIT, 'X', GAMEPAD_BIND_SHIFT,
        '\t', 'e', '-',
        'r', '0', 'F',
        TOUCH_PANE_BIND_INHERIT, TOUCH_PANE_BIND_INHERIT,
        TOUCH_PANE_BIND_INHERIT,
        TOUCH_PANE_BIND_INHERIT, 'z', TOUCH_PANE_BIND_INHERIT,
        TOUCH_PANE_BIND_INHERIT, TOUCH_PANE_BIND_INHERIT,
        TOUCH_PANE_BIND_INHERIT,
        'M', 'q', 'p',
        'w', 'b', 'c',
    };
    static const int swipe_defaults[GAMEPAD_STICK_DIR_COUNT] = {
        '8', '2', '4', '6',
    };

    if (!cfg)
        return;

    memcpy(cfg->touch_pane_bindings, main_defaults, sizeof(main_defaults));
    memcpy(cfg->touch_pane_second_bindings, second_defaults,
        sizeof(second_defaults));
    SDL_strlcpy(cfg->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_MAIN], "Main",
        sizeof(cfg->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_MAIN]));
    SDL_strlcpy(cfg->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_SECOND],
        "2nd Panel",
        sizeof(cfg->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_SECOND]));
    cfg->touch_pane_key_labels_visible = false;
    cfg->touch_pane_inventory_equipment_cycle = true;
    cfg->touch_swipe_enabled = true;
    memcpy(cfg->touch_swipe_bindings, swipe_defaults, sizeof(swipe_defaults));
}

typedef struct touch_pane_binding_migration {
    int panel;
    int index;
    int old_binding;
    int new_binding;
    bool old_defaults_only;
    cptr message;
} touch_pane_binding_migration;

static void sdl_config_migrate_touch_pane_binding(struct sdl_config* cfg,
    const touch_pane_binding_migration* migration)
{
    int* bindings;
    char (*labels)[SDL_TOUCH_PANE_LABEL_LEN];

    if (!cfg || !migration)
        return;
    if (migration->panel < 0 || migration->panel >= SDL_TOUCH_PANE_PANEL_COUNT
        || migration->index < 0
        || migration->index >= SDL_TOUCH_PANE_BUTTON_COUNT)
    {
        return;
    }

    bindings = (migration->panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? cfg->touch_pane_second_bindings : cfg->touch_pane_bindings;
    labels = (migration->panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? cfg->touch_pane_second_labels : cfg->touch_pane_labels;
    if (bindings[migration->index] != migration->old_binding
        || labels[migration->index][0])
    {
        return;
    }

    bindings[migration->index] = migration->new_binding;
    log_info("%s", migration->message);
}

void sdl_config_migrate_touch_pane_defaults(struct sdl_config* cfg,
    bool old_touch_pane_defaults)
{
    static const touch_pane_binding_migration migrations[] = {
        { SDL_TOUCH_PANE_PANEL_MAIN, 1, GAMEPAD_BIND_CTRL, 'S', true,
            "Migrated default touch pane Ctrl button to Stealth" },
        { SDL_TOUCH_PANE_PANEL_MAIN, 3, 'e', 'h', false,
            "Migrated default touch pane Equip button to Char" },
        { SDL_TOUCH_PANE_PANEL_MAIN, 5, '-', 'j', false,
            "Migrated default touch pane Fletch button to Supply" },
        { SDL_TOUCH_PANE_PANEL_SECOND, 1, TOUCH_PANE_BIND_INHERIT, 'X', false,
            "Migrated default touch pane Stealth second-panel button to Exchange" },
        { SDL_TOUCH_PANE_PANEL_SECOND, 3, '0', '\t', false,
            "Migrated default touch pane Char second-panel button to Ability" },
        { SDL_TOUCH_PANE_PANEL_SECOND, 4, '-', 'e', false,
            "Migrated default touch pane Inv second-panel button to Equip" },
        { SDL_TOUCH_PANE_PANEL_SECOND, 5, 'q', '-', false,
            "Migrated default touch pane Supply second-panel button to Fletch" },
        { SDL_TOUCH_PANE_PANEL_SECOND, 7, 'S', '0', false,
            "Migrated default touch pane Sing second-panel button to Smith" },
        { SDL_TOUCH_PANE_PANEL_SECOND, 18, 'L', 'M', false,
            "Migrated default touch pane View second-panel button to Map" },
        { SDL_TOUCH_PANE_PANEL_SECOND, 19, 'X', 'q', false,
            "Migrated default touch pane Desc second-panel button to Quaff" },
    };

    if (!cfg)
        return;
    for (size_t i = 0; i < N_ELEMENTS(migrations); i++) {
        if (!migrations[i].old_defaults_only || old_touch_pane_defaults)
            sdl_config_migrate_touch_pane_binding(cfg, &migrations[i]);
    }

    if (old_touch_pane_defaults
        && strcmp(cfg->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_SECOND],
            "Shift") == 0)
    {
        SDL_strlcpy(cfg->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_SECOND],
            "2nd Panel", sizeof(cfg->touch_pane_panel_names[
                SDL_TOUCH_PANE_PANEL_SECOND]));
        log_info("Migrated default touch pane Shift panel name to 2nd Panel");
    }
}

void sdl_config_clear_touch_pane_labels(struct sdl_config* cfg)
{
    if (!cfg)
        return;

    memset(cfg->touch_pane_labels, 0, sizeof(cfg->touch_pane_labels));
    memset(cfg->touch_pane_second_labels, 0,
        sizeof(cfg->touch_pane_second_labels));
}

void sdl_config_set_defaults(struct sdl_config* cfg)
{
    if (!cfg)
        return;

    cfg->layout_schema_version = SDL_LAYOUT_SCHEMA_VERSION;
    cfg->main_view_scale = 1;
    cfg->overlay_density = SDL_OVERLAY_DENSITY_AUTO;
    cfg->aux_view_font_size = 0;
    cfg->menu_panel_font_size = 0;
    cfg->plain_menu_font_size = 0;
    cfg->browser_menu_font_size = 0;
    cfg->character_sheet_font_size = 0;
    cfg->margin = 4;
    cfg->fullscreen = true;
    cfg->tiles = true;
    cfg->use_unsafe_area = false;
    SDL_strlcpy(cfg->palette_preset, "classic",
        sizeof(cfg->palette_preset));
    cfg->enable_right_panes = true;
    cfg->enable_bottom_panes = true;
    cfg->show_pane_borders = true;
    cfg->hide_left_panel = false;
#if defined(__ANDROID__) || defined(SIL_IOS)
    cfg->min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;
#else
    cfg->min_terminal_mode = SDL_MIN_TERMINAL_NORMAL;
#endif

    cfg->window_x = -1;
    cfg->window_y = -1;
    cfg->window_width = 0;
    cfg->window_height = 0;

    SDL_strlcpy(cfg->story_font, "font/Cinzel-Medium.ttf",
        sizeof(cfg->story_font));
    SDL_strlcpy(cfg->monospace_font, "font/VictorMono-Medium.ttf",
        sizeof(cfg->monospace_font));

    cfg->mono_bold = false;
    cfg->mono_italic = false;
    cfg->mono_underline = false;
    cfg->mono_strikethrough = false;
    cfg->mono_hinting = 0;
    cfg->mono_kerning = true;
    cfg->mono_outline = 0;

    cfg->story_bold = false;
    cfg->story_italic = false;
    cfg->story_underline = false;
    cfg->story_strikethrough = false;
    cfg->story_hinting = 0;
    cfg->story_kerning = true;
    cfg->story_outline = 0;

    cfg->gamepad_enabled = true;
    cfg->gamepad_auto_mode = true;
    cfg->steamdeck_mode = false;
    cfg->gamepad_use_dpad = true;
    cfg->gamepad_use_left_stick = true;
    cfg->gamepad_deadzone = 12000;
    cfg->gamepad_trigger_threshold = 16000;
    sdl_config_set_default_gamepad_bindings(cfg);
    sdl_config_set_default_touch_pane_bindings(cfg);
    sdl_config_clear_touch_pane_labels(cfg);
    sdl_config_clear_movement_bindings(cfg);
    cfg->overlay_panel_count = 0;
    memset(cfg->overlay_panels, 0, sizeof(cfg->overlay_panels));
}

void sdl_config_apply_cmdline(struct sdl_config* cfg, int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--scale") == 0) {
            if (argc > i + 1) {
                const char* scale_str = argv[++i];
                int scale = atoi(scale_str);
                if (scale > 0) {
                    cfg->main_view_scale = scale;
                    log_info("Command line: main view scale set to %d", scale);
                }
            }
        } else if (strcmp(argv[i], "--ascii") == 0) {
            cfg->tiles = false;
            log_info("Command line: ASCII mode enabled");
        } else if (strcmp(argv[i], "--windowed") == 0) {
            cfg->fullscreen = false;
            log_info("Command line: windowed mode enabled");
        } else if (strcmp(argv[i], "--fullscreen") == 0) {
            cfg->fullscreen = true;
            log_info("Command line: fullscreen mode enabled");
        } else if (strcmp(argv[i], "--tiles") == 0) {
            cfg->tiles = true;
            log_info("Command line: tiles mode enabled");
        } else if (strcmp(argv[i], "--font-size") == 0) {
            if (argc > i + 1) {
                const char* size_str = argv[++i];
                int size = atoi(size_str);
                if (size > 0) {
                    cfg->aux_view_font_size = size;
                    log_info("Command line: auxiliary view font size set to %d",
                        size);
                }
            }
        } else if (strcmp(argv[i], "--margin") == 0) {
            if (argc > i + 1) {
                const char* margin_str = argv[++i];
                int margin = atoi(margin_str);
                if (margin >= 0) {
                    cfg->margin = margin;
                    log_info("Command line: margin set to %d", margin);
                }
            }
        }
    }
}

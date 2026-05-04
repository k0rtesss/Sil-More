/* File: cmd-ui-settings-touch.c */
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
#include "platform-input.h"
#include "sdl-main-internal.h"
#include "cmd-ui-settings.h"
#include "ui/ui-browser-shell.h"
#include "ui/ui-help.h"
#include "ui/ui-information-scene.h"

static const char* settings_sdl_touch_slot_name(int idx);
static void settings_sdl_touch_panel_name(int panel, char* buf, size_t buflen);
static void settings_sdl_touch_button_label(int panel, int slot, char* buf,
    size_t buflen);
static int settings_sdl_touch_binding(int panel, int slot);
static void settings_sdl_set_touch_binding(int panel, int slot, int binding);
static const int touch_pane_main_action_choices[] = {
    GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', 'E', 't', 'p', 'q',
    'F', 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const char* const touch_zone_center_slot_names[
    SDL_TOUCH_ZONE_CENTER_BINDING_COUNT] = {
        "Left tap",
        "Left long press",
        "Right tap",
        "Right long press",
};

static const char* const touch_corner_slot_names[
    SDL_TOUCH_CORNER_ACTION_BINDING_COUNT] = {
        "Top tap",
        "Top long press",
        "Bottom tap",
        "Bottom long press",
};

static const char* const touch_top_panel_slot_names[
    SDL_TOUCH_TOP_PANEL_BUTTON_COUNT] = {
        "Button 1",
        "Button 2",
        "Button 3",
        "Button 4",
        "Button 5",
        "Button 6",
};

typedef enum touch_setting_row {
    TOUCH_SETTING_PROFILE = 0,
    TOUCH_SETTING_MOVEMENT_MODE,
    TOUCH_SETTING_ROUND_MOVEMENT,
    TOUCH_SETTING_ZONE_OVERLAY,
    TOUCH_SETTING_CORNER_SIDE,
    TOUCH_SETTING_TOP_PANEL_MODE,
    TOUCH_SETTING_TOP_PANEL_DEFAULT_OPEN,
    TOUCH_SETTING_MENU_COMMANDS_INV_EQUIP,
    TOUCH_SETTING_MENU_COMMANDS_SUPPLY,
    TOUCH_SETTING_MENU_COMMANDS_OTHER,
    TOUCH_SETTING_CENTER_BINDINGS,
    TOUCH_SETTING_CORNER_BINDINGS,
    TOUCH_SETTING_TOP_PANEL_BINDINGS,
    TOUCH_SETTING_ROW_COUNT,
} touch_setting_row;

static int touch_binding_choice_count(void)
{
    return (int)N_ELEMENTS(touch_pane_main_action_choices);
}

static const int* touch_binding_choices(void)
{
    return touch_pane_main_action_choices;
}

static int touch_binding_choice_index(int binding)
{
    int count = touch_binding_choice_count();
    const int* choices = touch_binding_choices();

    for (int i = 0; i < count; i++)
    {
        if (choices[i] == binding)
            return i;
    }

    return 0;
}

static void touch_binding_action_label(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    switch (binding)
    {
    case GAMEPAD_BIND_NONE:
        SDL_strlcpy(buf, "Off", buflen);
        return;
    case TOUCH_BIND_TOP_PANEL_OPEN:
        SDL_strlcpy(buf, "Open top panel", buflen);
        return;
    case TOUCH_BIND_TOP_PANEL_CLOSE:
        SDL_strlcpy(buf, "Close top panel", buflen);
        return;
    default:
        binding_action_label(binding, buf, buflen);
        return;
    }
}

static const char* touch_profile_label(int profile)
{
    switch (profile)
    {
    case SDL_TOUCH_PROFILE_CORNERS:
        return "Corners";
    case SDL_TOUCH_PROFILE_ROUND_WHEEL:
        return "Round wheel";
    case SDL_TOUCH_PROFILE_TOUCH_PANE:
    default:
        return "Touch panel";
    }
}

static const char* touch_movement_mode_label(int mode)
{
    switch (mode)
    {
    case SDL_TOUCH_MOVEMENT_OFF:
        return "Off";
    case SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY:
        return "Long press only";
    case SDL_TOUCH_MOVEMENT_ON:
    default:
        return "On";
    }
}

static const char* touch_zone_overlay_mode_label(int mode)
{
    switch (mode)
    {
    case SDL_TOUCH_ZONE_OVERLAY_OFF:
        return "Off";
    case SDL_TOUCH_ZONE_OVERLAY_BORDERS:
        return "Borders";
    case SDL_TOUCH_ZONE_OVERLAY_BORDERS_LABELS:
        return "Borders + labels";
    case SDL_TOUCH_ZONE_OVERLAY_MARKERS:
    default:
        return "Markers";
    }
}

static const char* touch_corner_up_down_side_label(int side)
{
    return (side == SDL_TOUCH_CORNER_UP_DOWN_LEFT) ? "Left" : "Right";
}

static const char* touch_top_panel_mode_label(int mode)
{
    return (mode == SDL_TOUCH_TOP_PANEL_MODE_LONG) ? "Long" : "Short";
}

static int touch_profile_next(int profile, int step)
{
    return (SDL_TOUCH_PROFILE_COUNT + profile + step)
        % SDL_TOUCH_PROFILE_COUNT;
}

static int touch_movement_mode_next(int mode, int step)
{
    int next = mode;

    do
    {
        next = (SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY + 1 + next + step)
            % (SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY + 1);
    } while (next < SDL_TOUCH_MOVEMENT_ON
        || next > SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY);

    return next;
}

static int touch_zone_overlay_mode_next(int mode, int step)
{
    return (SDL_TOUCH_ZONE_OVERLAY_COUNT + mode + step)
        % SDL_TOUCH_ZONE_OVERLAY_COUNT;
}

static int touch_corner_up_down_side_next(int side)
{
    return (side == SDL_TOUCH_CORNER_UP_DOWN_LEFT)
        ? SDL_TOUCH_CORNER_UP_DOWN_RIGHT
        : SDL_TOUCH_CORNER_UP_DOWN_LEFT;
}

static int touch_top_panel_mode_next(int mode)
{
    return (mode == SDL_TOUCH_TOP_PANEL_MODE_LONG)
        ? SDL_TOUCH_TOP_PANEL_MODE_SHORT
        : SDL_TOUCH_TOP_PANEL_MODE_LONG;
}

static void touch_top_panel_binding_label(int index, bool long_press,
    char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (index < 0 || index >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
    {
        buf[0] = '\0';
        return;
    }

    touch_binding_action_label(
        platform_touch_top_panel_binding(index, long_press), buf, buflen);
}

static void touch_control_set_row_default(touch_setting_row row)
{
    switch (row)
    {
    case TOUCH_SETTING_PROFILE:
        platform_set_touch_profile(platform_touch_default_profile());
        break;
    case TOUCH_SETTING_MOVEMENT_MODE:
        platform_set_touch_movement_mode(platform_touch_movement_default_mode());
        break;
    case TOUCH_SETTING_ROUND_MOVEMENT:
        platform_set_touch_round_movement_enabled(
            platform_touch_round_movement_default_enabled());
        break;
    case TOUCH_SETTING_ZONE_OVERLAY:
        platform_set_touch_zone_overlay_mode(
            platform_touch_zone_overlay_default_mode());
        break;
    case TOUCH_SETTING_CORNER_SIDE:
        platform_set_touch_corner_up_down_side(
            platform_touch_corner_up_down_default_side());
        break;
    case TOUCH_SETTING_TOP_PANEL_MODE:
        platform_set_touch_top_panel_mode(
            platform_touch_top_panel_default_mode());
        break;
    case TOUCH_SETTING_TOP_PANEL_DEFAULT_OPEN:
        platform_set_touch_top_panel_default_open(
            platform_touch_top_panel_default_open_default());
        break;
    case TOUCH_SETTING_MENU_COMMANDS_INV_EQUIP:
        platform_set_touch_menu_commands_enabled(
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT,
            platform_touch_menu_commands_default_enabled(
                SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT));
        break;
    case TOUCH_SETTING_MENU_COMMANDS_SUPPLY:
        platform_set_touch_menu_commands_enabled(
            SDL_TOUCH_MENU_CATEGORY_SUPPLY,
            platform_touch_menu_commands_default_enabled(
                SDL_TOUCH_MENU_CATEGORY_SUPPLY));
        break;
    case TOUCH_SETTING_MENU_COMMANDS_OTHER:
        platform_set_touch_menu_commands_enabled(
            SDL_TOUCH_MENU_CATEGORY_OTHER,
            platform_touch_menu_commands_default_enabled(
                SDL_TOUCH_MENU_CATEGORY_OTHER));
        break;
    case TOUCH_SETTING_CENTER_BINDINGS:
        for (int i = 0; i < SDL_TOUCH_ZONE_CENTER_BINDING_COUNT; i++)
        {
            platform_set_touch_zone_center_binding(i,
                platform_touch_zone_center_default_binding(i));
        }
        break;
    case TOUCH_SETTING_CORNER_BINDINGS:
        for (int i = 0; i < SDL_TOUCH_CORNER_ACTION_BINDING_COUNT; i++)
        {
            platform_set_touch_corner_action_binding(i,
                platform_touch_corner_action_default_binding(i));
        }
        break;
    case TOUCH_SETTING_TOP_PANEL_BINDINGS:
        for (int i = 0; i < SDL_TOUCH_TOP_PANEL_BUTTON_COUNT; i++)
        {
            platform_set_touch_top_panel_binding(i, false,
                platform_touch_top_panel_default_binding(i, false));
            platform_set_touch_top_panel_binding(i, true,
                platform_touch_top_panel_default_binding(i, true));
        }
        break;
    default:
        break;
    }
}

static void touch_control_reset_all(void)
{
    for (int row = 0; row < TOUCH_SETTING_ROW_COUNT; row++)
        touch_control_set_row_default((touch_setting_row)row);
}

static bool touch_editor_apply_row_focus(
    const ui_browser_shell_command_result* result, int* cursor, int count,
    int* top, int visible_rows)
{
    return ui_browser_shell_apply_row_focus(result, cursor, count, top,
        visible_rows);
}

static char touch_editor_read_key(
    const ui_browser_shell_command_map* map,
    ui_browser_shell_command_result* result, int* cursor, int count, int* top,
    int visible_rows)
{
    char ch = (char)ui_browser_shell_wait_key_with_wait_reason(map, 0,
        APP_WAIT_REASON_NONE, true, result);

    if (result && touch_editor_apply_row_focus(result, cursor, count, top,
            visible_rows) && !ch)
    {
        return '\0';
    }

    if (!ch)
        return '\0';

    {
        int dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);
    }

    return ch;
}

static void do_cmd_touch_binding_editor(cptr title, cptr subtitle,
    const char* const* slot_names, int slot_count,
    int (*getter)(int index), void (*setter)(int index, int binding),
    int (*default_getter)(int index), bool* settings_changed)
{
    bool done = false;
    bool changed = false;
    int highlight = 0;
    int top = 0;
    const int list_start_row = 5;

    while (!done)
    {
        settings_ui_layout layout = settings_ui_read_layout();
        int visible_rows = settings_ui_list_visible_rows(&layout,
            list_start_row, 6, 4);
        app_ui_scene scene;
        app_ui_panel* panel = settings_browser_scene_begin_ex(&scene,
            title, subtitle ? subtitle : "", 1100, 2200);
        ui_browser_shell_command_map map;
        ui_browser_shell_command_result result;

        ui_browser_shell_command_map_init(&map);
        map.row_activate_key = '\r';
        map.button_keys = NULL;
        map.button_key_count = 0;

        ui_browser_shell_clamp_cursor(&highlight, &top, slot_count,
            visible_rows);

        if (!panel)
        {
            done = true;
            continue;
        }

        if (top > 0)
            app_ui_panel_set_row_offset(panel, (s16b)top);

        for (int i = 0; i < slot_count; i++)
        {
            char action_buf[80];
            byte attr = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            touch_binding_action_label(getter(i), action_buf,
                sizeof(action_buf));
            if (!settings_browser_add_pair_row(panel, (s16b)i, attr,
                    TERM_SLATE, true, i == highlight, slot_names[i],
                    action_buf))
            {
                done = true;
                break;
            }
        }

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "4/6", "Action");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "r", "Reset");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "R", "Reset all");
        (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
            "Esc", "Back");
        if (!done && !ui_information_scene_present_ui(&scene))
        {
            done = true;
            continue;
        }

        {
            char ch = touch_editor_read_key(&map, &result, &highlight,
                slot_count, &top, visible_rows);

            if (!ch)
                continue;

            switch (ch)
            {
            case ESCAPE:
            case 'q':
            case 'Q':
                done = true;
                break;
            case '\n':
            case '\r':
            case ' ':
            case 't':
            case '5':
            case '6':
            {
                int count = touch_binding_choice_count();
                const int* choices = touch_binding_choices();
                int idx = touch_binding_choice_index(getter(highlight));

                idx = (idx + 1) % count;
                setter(highlight, choices[idx]);
                changed = true;
                break;
            }
            case '4':
            case 'n':
            {
                int count = touch_binding_choice_count();
                const int* choices = touch_binding_choices();
                int idx = touch_binding_choice_index(getter(highlight));

                idx = (count + idx - 1) % count;
                setter(highlight, choices[idx]);
                changed = true;
                break;
            }
            case '-':
            case '8':
                highlight = (slot_count + highlight - 1) % slot_count;
                break;
            case '2':
                highlight = (highlight + 1) % slot_count;
                break;
            case 'r':
                setter(highlight, default_getter(highlight));
                changed = true;
                break;
            case 'R':
                for (int i = 0; i < slot_count; i++)
                    setter(i, default_getter(i));
                changed = true;
                break;
            default:
                bell("Illegal command for touch control bindings!");
                break;
            }
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;
}

static void do_cmd_touch_top_panel_binding_editor(bool* settings_changed)
{
    bool done = false;
    bool changed = false;
    bool long_press = false;
    int highlight = 0;
    int top = 0;
    const int list_start_row = 5;

    while (!done)
    {
        settings_ui_layout layout = settings_ui_read_layout();
        int visible_rows = settings_ui_list_visible_rows(&layout,
            list_start_row, 6, 4);
        app_ui_scene scene;
        app_ui_panel* panel;
        char subtitle[64];
        ui_browser_shell_command_map map;
        ui_browser_shell_command_result result;

        panel = settings_browser_scene_begin_ex(&scene, "Top Panel Bindings",
            "", 1100, 2200);
        ui_browser_shell_command_map_init(&map);
        map.row_activate_key = '\r';

        ui_browser_shell_clamp_cursor(&highlight, &top,
            SDL_TOUCH_TOP_PANEL_BUTTON_COUNT, visible_rows);

        if (!panel)
        {
            done = true;
            continue;
        }

        strnfmt(subtitle, sizeof(subtitle), "Editing %s actions",
            long_press ? "long press" : "tap");
        app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);
        if (top > 0)
            app_ui_panel_set_row_offset(panel, (s16b)top);

        for (int i = 0; i < SDL_TOUCH_TOP_PANEL_BUTTON_COUNT; i++)
        {
            char action_buf[80];
            byte attr = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            touch_top_panel_binding_label(i, long_press, action_buf,
                sizeof(action_buf));
            if (!settings_browser_add_pair_row(panel, (s16b)i, attr,
                    TERM_SLATE, true, i == highlight,
                    touch_top_panel_slot_names[i], action_buf))
            {
                done = true;
                break;
            }
        }

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "4/6", "Action");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "Tab", "Tap/Long");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "r", "Reset");
        (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
            "R", "Reset all");
        (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
            "Esc", "Back");
        if (!done && !ui_information_scene_present_ui(&scene))
        {
            done = true;
            continue;
        }

        {
            char ch = touch_editor_read_key(&map, &result, &highlight,
                SDL_TOUCH_TOP_PANEL_BUTTON_COUNT, &top, visible_rows);

            if (!ch)
                continue;

            switch (ch)
            {
            case ESCAPE:
            case 'q':
            case 'Q':
                done = true;
                break;
            case '\t':
                long_press = !long_press;
                break;
            case '\n':
            case '\r':
            case ' ':
            case 't':
            case '5':
            case '6':
            {
                int count = touch_binding_choice_count();
                const int* choices = touch_binding_choices();
                int idx = touch_binding_choice_index(
                    platform_touch_top_panel_binding(highlight, long_press));

                idx = (idx + 1) % count;
                platform_set_touch_top_panel_binding(highlight, long_press,
                    choices[idx]);
                changed = true;
                break;
            }
            case '4':
            case 'n':
            {
                int count = touch_binding_choice_count();
                const int* choices = touch_binding_choices();
                int idx = touch_binding_choice_index(
                    platform_touch_top_panel_binding(highlight, long_press));

                idx = (count + idx - 1) % count;
                platform_set_touch_top_panel_binding(highlight, long_press,
                    choices[idx]);
                changed = true;
                break;
            }
            case '-':
            case '8':
                highlight = (SDL_TOUCH_TOP_PANEL_BUTTON_COUNT + highlight - 1)
                    % SDL_TOUCH_TOP_PANEL_BUTTON_COUNT;
                break;
            case '2':
                highlight = (highlight + 1)
                    % SDL_TOUCH_TOP_PANEL_BUTTON_COUNT;
                break;
            case 'r':
                platform_set_touch_top_panel_binding(highlight, long_press,
                    platform_touch_top_panel_default_binding(highlight,
                        long_press));
                changed = true;
                break;
            case 'R':
                for (int i = 0; i < SDL_TOUCH_TOP_PANEL_BUTTON_COUNT; i++)
                {
                    platform_set_touch_top_panel_binding(i, false,
                        platform_touch_top_panel_default_binding(i, false));
                    platform_set_touch_top_panel_binding(i, true,
                        platform_touch_top_panel_default_binding(i, true));
                }
                changed = true;
                break;
            default:
                bell("Illegal command for top panel bindings!");
                break;
            }
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;
}

static void do_cmd_touch_control_editor(bool* settings_changed)
{
    bool done = false;
    bool changed = false;
    int highlight = TOUCH_SETTING_PROFILE;
    int top = 0;
    const int list_start_row = 5;

    while (!done)
    {
        settings_ui_layout layout = settings_ui_read_layout();
        int visible_rows = settings_ui_list_visible_rows(&layout,
            list_start_row, 6, 6);
        app_ui_scene scene;
        app_ui_panel* panel = settings_browser_scene_begin_ex(&scene,
            "Touch Control", "Profiles, zones, and top panel", 1160, 2200);
        ui_browser_shell_command_map map;
        ui_browser_shell_command_result result;

        ui_browser_shell_command_map_init(&map);
        map.row_activate_key = '\r';
        ui_browser_shell_clamp_cursor(&highlight, &top, TOUCH_SETTING_ROW_COUNT,
            visible_rows);

        if (!panel)
        {
            done = true;
            continue;
        }

        if (top > 0)
            app_ui_panel_set_row_offset(panel, (s16b)top);

        for (int row = 0; row < TOUCH_SETTING_ROW_COUNT; row++)
        {
            char value_buf[80];
            byte attr = (row == highlight) ? TERM_L_BLUE : TERM_WHITE;

            switch ((touch_setting_row)row)
            {
            case TOUCH_SETTING_PROFILE:
                SDL_strlcpy(value_buf,
                    touch_profile_label(platform_touch_profile()),
                    sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight, "Touch profile",
                    value_buf);
                break;
            case TOUCH_SETTING_MOVEMENT_MODE:
                SDL_strlcpy(value_buf, touch_movement_mode_label(
                        platform_touch_movement_mode()), sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight, "Touch movement",
                    value_buf);
                break;
            case TOUCH_SETTING_ROUND_MOVEMENT:
                SDL_strlcpy(value_buf,
                    platform_touch_round_movement_enabled() ? "On" : "Off",
                    sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight, "Round movement layer",
                    value_buf);
                break;
            case TOUCH_SETTING_ZONE_OVERLAY:
                SDL_strlcpy(value_buf, touch_zone_overlay_mode_label(
                        platform_touch_zone_overlay_mode()),
                    sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight, "Zone overlay",
                    value_buf);
                break;
            case TOUCH_SETTING_CORNER_SIDE:
                SDL_strlcpy(value_buf, touch_corner_up_down_side_label(
                        platform_touch_corner_up_down_side()),
                    sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight, "Up/down corner side",
                    value_buf);
                break;
            case TOUCH_SETTING_TOP_PANEL_MODE:
                SDL_strlcpy(value_buf, touch_top_panel_mode_label(
                        platform_touch_top_panel_mode()), sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight, "Top panel mode",
                    value_buf);
                break;
            case TOUCH_SETTING_TOP_PANEL_DEFAULT_OPEN:
                SDL_strlcpy(value_buf,
                    platform_touch_top_panel_default_open() ? "Yes" : "No",
                    sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight,
                    "Top panel default open", value_buf);
                break;
            case TOUCH_SETTING_MENU_COMMANDS_INV_EQUIP:
                SDL_strlcpy(value_buf, platform_touch_menu_commands_enabled(
                        SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT)
                        ? "On" : "Off",
                    sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight,
                    "Inv/equip menu commands", value_buf);
                break;
            case TOUCH_SETTING_MENU_COMMANDS_SUPPLY:
                SDL_strlcpy(value_buf, platform_touch_menu_commands_enabled(
                        SDL_TOUCH_MENU_CATEGORY_SUPPLY) ? "On" : "Off",
                    sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight,
                    "Supply menu commands", value_buf);
                break;
            case TOUCH_SETTING_MENU_COMMANDS_OTHER:
                SDL_strlcpy(value_buf, platform_touch_menu_commands_enabled(
                        SDL_TOUCH_MENU_CATEGORY_OTHER) ? "On" : "Off",
                    sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight,
                    "Other menu commands", value_buf);
                break;
            case TOUCH_SETTING_CENTER_BINDINGS:
                SDL_strlcpy(value_buf, "4 actions", sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight,
                    "Center zone bindings", value_buf);
                break;
            case TOUCH_SETTING_CORNER_BINDINGS:
                SDL_strlcpy(value_buf, "4 actions", sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight,
                    "Corner bindings", value_buf);
                break;
            case TOUCH_SETTING_TOP_PANEL_BINDINGS:
                SDL_strlcpy(value_buf, "6 tap + long", sizeof(value_buf));
                settings_browser_add_pair_row(panel, (s16b)row, attr,
                    TERM_SLATE, true, row == highlight,
                    "Top panel bindings", value_buf);
                break;
            default:
                break;
            }
        }

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "4/6", "Set");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "Enter", "Open");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "r", "Reset");
        (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
            "R", "Reset all");
        (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
            "Esc", "Back");
        if (!ui_information_scene_present_ui(&scene))
        {
            done = true;
            continue;
        }

        {
            char ch = touch_editor_read_key(&map, &result, &highlight,
                TOUCH_SETTING_ROW_COUNT, &top, visible_rows);

            if (!ch)
                continue;

            switch (ch)
            {
            case ESCAPE:
            case 'q':
            case 'Q':
                done = true;
                break;
            case '-':
            case '8':
                highlight = (TOUCH_SETTING_ROW_COUNT + highlight - 1)
                    % TOUCH_SETTING_ROW_COUNT;
                break;
            case '2':
                highlight = (highlight + 1) % TOUCH_SETTING_ROW_COUNT;
                break;
            case '4':
            case 'n':
                switch ((touch_setting_row)highlight)
                {
                case TOUCH_SETTING_PROFILE:
                    platform_set_touch_profile(touch_profile_next(
                        platform_touch_profile(), -1));
                    changed = true;
                    break;
                case TOUCH_SETTING_MOVEMENT_MODE:
                    platform_set_touch_movement_mode(touch_movement_mode_next(
                        platform_touch_movement_mode(), -1));
                    changed = true;
                    break;
                case TOUCH_SETTING_ROUND_MOVEMENT:
                    platform_set_touch_round_movement_enabled(
                        !platform_touch_round_movement_enabled());
                    changed = true;
                    break;
                case TOUCH_SETTING_ZONE_OVERLAY:
                    platform_set_touch_zone_overlay_mode(
                        touch_zone_overlay_mode_next(
                            platform_touch_zone_overlay_mode(), -1));
                    changed = true;
                    break;
                case TOUCH_SETTING_CORNER_SIDE:
                    platform_set_touch_corner_up_down_side(
                        touch_corner_up_down_side_next(
                            platform_touch_corner_up_down_side()));
                    changed = true;
                    break;
                case TOUCH_SETTING_TOP_PANEL_MODE:
                    platform_set_touch_top_panel_mode(
                        touch_top_panel_mode_next(
                            platform_touch_top_panel_mode()));
                    changed = true;
                    break;
                case TOUCH_SETTING_TOP_PANEL_DEFAULT_OPEN:
                    platform_set_touch_top_panel_default_open(
                        !platform_touch_top_panel_default_open());
                    changed = true;
                    break;
                case TOUCH_SETTING_MENU_COMMANDS_INV_EQUIP:
                    platform_set_touch_menu_commands_enabled(
                        SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT,
                        !platform_touch_menu_commands_enabled(
                            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT));
                    changed = true;
                    break;
                case TOUCH_SETTING_MENU_COMMANDS_SUPPLY:
                    platform_set_touch_menu_commands_enabled(
                        SDL_TOUCH_MENU_CATEGORY_SUPPLY,
                        !platform_touch_menu_commands_enabled(
                            SDL_TOUCH_MENU_CATEGORY_SUPPLY));
                    changed = true;
                    break;
                case TOUCH_SETTING_MENU_COMMANDS_OTHER:
                    platform_set_touch_menu_commands_enabled(
                        SDL_TOUCH_MENU_CATEGORY_OTHER,
                        !platform_touch_menu_commands_enabled(
                            SDL_TOUCH_MENU_CATEGORY_OTHER));
                    changed = true;
                    break;
                default:
                    break;
                }
                break;
            case '\n':
            case '\r':
            case ' ':
            case 't':
            case '5':
            case '6':
                switch ((touch_setting_row)highlight)
                {
                case TOUCH_SETTING_PROFILE:
                    platform_set_touch_profile(touch_profile_next(
                        platform_touch_profile(), 1));
                    changed = true;
                    break;
                case TOUCH_SETTING_MOVEMENT_MODE:
                    platform_set_touch_movement_mode(touch_movement_mode_next(
                        platform_touch_movement_mode(), 1));
                    changed = true;
                    break;
                case TOUCH_SETTING_ROUND_MOVEMENT:
                    platform_set_touch_round_movement_enabled(
                        !platform_touch_round_movement_enabled());
                    changed = true;
                    break;
                case TOUCH_SETTING_ZONE_OVERLAY:
                    platform_set_touch_zone_overlay_mode(
                        touch_zone_overlay_mode_next(
                            platform_touch_zone_overlay_mode(), 1));
                    changed = true;
                    break;
                case TOUCH_SETTING_CORNER_SIDE:
                    platform_set_touch_corner_up_down_side(
                        touch_corner_up_down_side_next(
                            platform_touch_corner_up_down_side()));
                    changed = true;
                    break;
                case TOUCH_SETTING_TOP_PANEL_MODE:
                    platform_set_touch_top_panel_mode(
                        touch_top_panel_mode_next(
                            platform_touch_top_panel_mode()));
                    changed = true;
                    break;
                case TOUCH_SETTING_TOP_PANEL_DEFAULT_OPEN:
                    platform_set_touch_top_panel_default_open(
                        !platform_touch_top_panel_default_open());
                    changed = true;
                    break;
                case TOUCH_SETTING_MENU_COMMANDS_INV_EQUIP:
                    platform_set_touch_menu_commands_enabled(
                        SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT,
                        !platform_touch_menu_commands_enabled(
                            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT));
                    changed = true;
                    break;
                case TOUCH_SETTING_MENU_COMMANDS_SUPPLY:
                    platform_set_touch_menu_commands_enabled(
                        SDL_TOUCH_MENU_CATEGORY_SUPPLY,
                        !platform_touch_menu_commands_enabled(
                            SDL_TOUCH_MENU_CATEGORY_SUPPLY));
                    changed = true;
                    break;
                case TOUCH_SETTING_MENU_COMMANDS_OTHER:
                    platform_set_touch_menu_commands_enabled(
                        SDL_TOUCH_MENU_CATEGORY_OTHER,
                        !platform_touch_menu_commands_enabled(
                            SDL_TOUCH_MENU_CATEGORY_OTHER));
                    changed = true;
                    break;
                case TOUCH_SETTING_CENTER_BINDINGS:
                    do_cmd_touch_binding_editor("Center Zone Bindings",
                        "Left/right taps and long presses",
                        touch_zone_center_slot_names,
                        SDL_TOUCH_ZONE_CENTER_BINDING_COUNT,
                        platform_touch_zone_center_binding,
                        platform_set_touch_zone_center_binding,
                        platform_touch_zone_center_default_binding,
                        &changed);
                    break;
                case TOUCH_SETTING_CORNER_BINDINGS:
                    do_cmd_touch_binding_editor("Corner Bindings",
                        "Top/bottom taps and long presses",
                        touch_corner_slot_names,
                        SDL_TOUCH_CORNER_ACTION_BINDING_COUNT,
                        platform_touch_corner_action_binding,
                        platform_set_touch_corner_action_binding,
                        platform_touch_corner_action_default_binding,
                        &changed);
                    break;
                case TOUCH_SETTING_TOP_PANEL_BINDINGS:
                    do_cmd_touch_top_panel_binding_editor(&changed);
                    break;
                default:
                    break;
                }
                break;
            case 'r':
                touch_control_set_row_default((touch_setting_row)highlight);
                changed = true;
                break;
            case 'R':
                touch_control_reset_all();
                changed = true;
                break;
            default:
                bell("Illegal command for touch control settings!");
                break;
            }
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;
}

static const int touch_pane_second_action_choices[] = {
    TOUCH_PANE_BIND_INHERIT, GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', 'E', 't', 'p', 'q',
    'F', 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const int* touch_pane_action_choices_for_panel(int panel, int* count)
{
    if (count)
        *count = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
            ? (int)N_ELEMENTS(touch_pane_second_action_choices)
            : (int)N_ELEMENTS(touch_pane_main_action_choices);

    return (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? touch_pane_second_action_choices
        : touch_pane_main_action_choices;
}

static int touch_pane_action_choice_index(int panel, int binding)
{
    int count = 0;
    const int* choices = touch_pane_action_choices_for_panel(panel, &count);

    for (int i = 0; i < count; i++)
    {
        if (choices[i] == binding)
            return i;
    }
    return 0;
}

static void touch_pane_action_label_for_panel(int panel, int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (binding == TOUCH_PANE_BIND_INHERIT) {
        SDL_strlcpy(buf, "Main panel button", buflen);
        return;
    }

    if (binding == GAMEPAD_BIND_SHIFT) {
        char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
        settings_sdl_touch_panel_name((panel == SDL_TOUCH_PANE_PANEL_SECOND)
                ? SDL_TOUCH_PANE_PANEL_MAIN
                : SDL_TOUCH_PANE_PANEL_SECOND,
            panel_name, sizeof(panel_name));
        strnfmt(buf, buflen, "Switch to %s panel", panel_name);
        return;
    }

    binding_action_label(binding, buf, buflen);
}

static const char* settings_sdl_touch_slot_name(int idx)
{
    return SETTINGS_SDL_GET(touch_pane_slot_name)(idx);
}

static void settings_sdl_touch_panel_name(int panel, char* buf, size_t buflen)
{
    SETTINGS_SDL_GET(touch_pane_panel_name)(panel, buf, buflen);
}

static void settings_sdl_touch_button_label(int panel, int slot, char* buf,
    size_t buflen)
{
    SETTINGS_SDL_GET(touch_pane_button_label_for_panel)(panel, slot, buf,
        buflen);
}

static int settings_sdl_touch_binding(int panel, int slot)
{
    return SETTINGS_SDL_GET(touch_pane_binding_for_panel)(panel, slot);
}

static void settings_sdl_set_touch_binding(int panel, int slot, int binding)
{
    SETTINGS_SDL_SET(touch_pane_binding_for_panel)(panel, slot, binding);
}

static void settings_sdl_set_touch_button_label(int panel, int slot,
    cptr label)
{
    SETTINGS_SDL_SET(touch_pane_button_label_for_panel)(panel, slot, label);
}

static void settings_sdl_set_touch_panel_name(int panel, cptr name)
{
    SETTINGS_SDL_SET(touch_pane_panel_name)(panel, name);
}

static int settings_sdl_touch_default_binding(int panel, int slot)
{
    return SETTINGS_SDL_GET(touch_pane_default_binding_for_panel)(panel, slot);
}

static void do_cmd_touch_panel_button_editor(bool* settings_changed)
{
    int highlight = 0;
    int top = 0;
    int panel = SDL_TOUCH_PANE_PANEL_MAIN;
    bool done = false;
    bool changed = false;
    const int list_start_row = 5;

    while (!done)
    {
        settings_ui_layout layout = settings_ui_read_layout();
        int visible_rows = settings_ui_list_visible_rows(&layout,
            list_start_row, 6, 5);
        ui_browser_shell_command_map map;
        ui_browser_shell_command_result result;

        ui_browser_shell_command_map_init(&map);
        map.row_activate_key = '\r';
        ui_browser_shell_clamp_cursor(&highlight, &top,
            SDL_TOUCH_PANE_BUTTON_COUNT, visible_rows);

        app_ui_scene scene;
        app_ui_panel* ui_panel = settings_browser_scene_begin_ex(&scene,
            "Touch Panel", "", 1100, 2200);

        if (!ui_panel)
        {
            done = true;
        }
        else
        {
            char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
            char info_buf[96];

            settings_sdl_touch_panel_name(panel, panel_name,
                sizeof(panel_name));
            strnfmt(info_buf, sizeof(info_buf), "Editing %s panel%s",
                panel_name,
                (panel == SDL_TOUCH_PANE_PANEL_SECOND)
                    ? " (empty = main panel)"
                    : "");
            app_ui_panel_set_subtitle(ui_panel, TERM_SLATE, info_buf);
            if (top > 0)
                app_ui_panel_set_row_offset(ui_panel, (s16b)top);

            for (int i = 0; i < SDL_TOUCH_PANE_BUTTON_COUNT; i++)
            {
                char action_buf[80];
                char label_buf[SDL_TOUCH_PANE_LABEL_LEN];
                char left_buf[64];
                byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

                settings_sdl_touch_button_label(panel, i,
                    label_buf, sizeof(label_buf));
                touch_pane_action_label_for_panel(panel,
                    settings_sdl_touch_binding(panel, i),
                    action_buf, sizeof(action_buf));

                if (label_buf[0])
                {
                    strnfmt(left_buf, sizeof(left_buf), "%s %s",
                        settings_sdl_touch_slot_name(i), label_buf);
                }
                else
                {
                    strnfmt(left_buf, sizeof(left_buf), "%s",
                        settings_sdl_touch_slot_name(i));
                }

                if (!settings_browser_add_pair_row(ui_panel, (s16b)i, a,
                        TERM_SLATE, true, i == highlight, left_buf,
                        action_buf))
                {
                    done = true;
                    break;
                }
            }

            (void)app_ui_panel_add_footer_action(ui_panel, 1, TERM_WHITE,
                true, "8/2", "Move");
            (void)app_ui_panel_add_footer_action(ui_panel, 2, TERM_WHITE,
                true, "4/6", "Action");
            (void)app_ui_panel_add_footer_action(ui_panel, 3, TERM_WHITE,
                true, "Tab", "Panel");
            (void)app_ui_panel_add_footer_action(ui_panel, 4, TERM_WHITE,
                true, "l/p", "Rename");
            (void)app_ui_panel_add_footer_action(ui_panel, 5, TERM_WHITE,
                true, "r", "Reset");
            (void)app_ui_panel_add_footer_action(ui_panel, 6, TERM_WHITE,
                true, "R", "Reset all");
            (void)app_ui_panel_add_footer_action(ui_panel, 7, TERM_WHITE,
                true, "Esc", "Back");
            if (!done && !ui_information_scene_present_ui(&scene))
                done = true;
        }

        {
            char ch = touch_editor_read_key(&map, &result, &highlight,
                SDL_TOUCH_PANE_BUTTON_COUNT, &top, visible_rows);

            if (!ch)
                continue;

            switch (ch)
            {
            case ESCAPE:
            case 'q':
            case 'Q':
                done = true;
                break;

            case '-':
            case '8':
                highlight = (SDL_TOUCH_PANE_BUTTON_COUNT + highlight - 1)
                    % SDL_TOUCH_PANE_BUTTON_COUNT;
                break;

            case '2':
                highlight = (highlight + 1) % SDL_TOUCH_PANE_BUTTON_COUNT;
                break;

            case 'n':
            case '4':
            {
                int choice_count = 0;
                const int* choices = touch_pane_action_choices_for_panel(panel,
                    &choice_count);
                int idx = touch_pane_action_choice_index(panel,
                    settings_sdl_touch_binding(panel, highlight));

                idx = (choice_count + idx - 1) % choice_count;
                settings_sdl_set_touch_binding(panel, highlight, choices[idx]);
                changed = true;
                break;
            }

            case 'y':
            case '6':
            case ' ':
            case 't':
            case '5':
            {
                int choice_count = 0;
                const int* choices = touch_pane_action_choices_for_panel(panel,
                    &choice_count);
                int idx = touch_pane_action_choice_index(panel,
                    settings_sdl_touch_binding(panel, highlight));

                idx = (idx + 1) % choice_count;
                settings_sdl_set_touch_binding(panel, highlight, choices[idx]);
                changed = true;
                break;
            }

            case 'l':
            case 'L':
            {
                char prompt[96];
                char prompt_long[96];
                char prompt_medium[96];
                char prompt_short[64];
                char current_label[SDL_TOUCH_PANE_LABEL_LEN];
                char new_label[SDL_TOUCH_PANE_LABEL_LEN];
                char current_buf[96];

                settings_sdl_touch_button_label(panel, highlight,
                    current_label, sizeof(current_label));
                strnfmt(prompt_long, sizeof(prompt_long),
                    "New label for %s (blank = use key label): ",
                    settings_sdl_touch_slot_name(highlight));
                strnfmt(prompt_medium, sizeof(prompt_medium),
                    "New label for %s (blank = default): ",
                    settings_sdl_touch_slot_name(highlight));
                strnfmt(prompt_short, sizeof(prompt_short), "Label for %s: ",
                    settings_sdl_touch_slot_name(highlight));
                strnfmt(prompt, sizeof(prompt), "%s",
                    settings_ui_pick_label(layout.prompt_line_chars,
                        prompt_long, prompt_medium, prompt_short));
                strnfmt(current_buf, sizeof(current_buf), "Current label: %s",
                    current_label);
                new_label[0] = '\0';
                if (settings_ui_prompt_string("Touch Panel", prompt,
                        current_buf, new_label, sizeof(new_label)))
                {
                    settings_sdl_set_touch_button_label(panel, highlight,
                        new_label);
                    changed = true;
                }
                break;
            }

            case '\t':
                panel = (panel == SDL_TOUCH_PANE_PANEL_MAIN)
                    ? SDL_TOUCH_PANE_PANEL_SECOND
                    : SDL_TOUCH_PANE_PANEL_MAIN;
                break;

            case 'p':
            case 'P':
            {
                char prompt[96];
                char current_name[SDL_TOUCH_PANE_LABEL_LEN];
                char new_name[SDL_TOUCH_PANE_LABEL_LEN];
                char current_buf[96];

                settings_sdl_touch_panel_name(panel, current_name,
                    sizeof(current_name));
                strnfmt(prompt, sizeof(prompt), "%s",
                    settings_ui_pick_label(layout.prompt_line_chars,
                        "Name for current panel (blank = default): ",
                        "Panel name (blank = default): ",
                        "Panel name: "));
                strnfmt(current_buf, sizeof(current_buf),
                    "Current panel name: %s", current_name);
                new_name[0] = '\0';
                if (settings_ui_prompt_string("Touch Panel", prompt,
                        current_buf, new_name, sizeof(new_name)))
                {
                    settings_sdl_set_touch_panel_name(panel, new_name);
                    changed = true;
                }
                break;
            }

            case 'r':
                settings_sdl_set_touch_binding(panel, highlight,
                    settings_sdl_touch_default_binding(panel, highlight));
                platform_clear_touch_pane_button_label_for_panel(panel,
                    highlight);
                changed = true;
                break;

            case 'R':
                platform_touch_pane_reset_bindings_to_default();
                changed = true;
                break;

            default:
                bell("Illegal command for touch panel settings!");
                break;
            }
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;
}

static void do_cmd_touch_settings(bool* settings_changed)
{
    int highlight = 1;
    bool done = false;

    while (!done)
    {
        const settings_choice_entry entries[] = {
            { 1, 'c', "c) Touch Control", false },
            { 2, 'p', "p) Touch Panel", false },
            { 3, 'o', "o) Return", false },
        };
        int choice = settings_choice_menu("Touch Settings", entries,
            (int)N_ELEMENTS(entries), &highlight, 3);

        switch (choice)
        {
        case 1:
            do_cmd_touch_control_editor(settings_changed);
            break;
        case 2:
            do_cmd_touch_panel_button_editor(settings_changed);
            break;
        case 3:
            done = true;
            break;
        default:
            break;
        }
    }
}

void do_cmd_touch_pane_button_editor(bool* settings_changed)
{
    do_cmd_touch_settings(settings_changed);
}

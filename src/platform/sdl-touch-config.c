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
#include "sdl-touch-config.h"
#include <string.h>

static const char* touch_profile_to_string(int profile)
{
    switch (profile) {
    case SDL_TOUCH_PROFILE_CORNERS:
        return "CORNERS";
    case SDL_TOUCH_PROFILE_ROUND_WHEEL:
        return "ROUND_WHEEL";
    case SDL_TOUCH_PROFILE_TOUCH_PANE:
    default:
        return "TOUCH_PANE";
    }
}

static int normalize_touch_profile(int profile)
{
    if (profile >= SDL_TOUCH_PROFILE_TOUCH_PANE
        && profile < SDL_TOUCH_PROFILE_COUNT)
    {
        return profile;
    }

    return SDL_TOUCH_PROFILE_TOUCH_PANE;
}

static int parse_touch_profile(const char* value)
{
    if (!value)
        return SDL_TOUCH_PROFILE_TOUCH_PANE;
    if (strcmp(value, "TOUCH_PANE") == 0) return SDL_TOUCH_PROFILE_TOUCH_PANE;
    if (strcmp(value, "TOUCH_PANEL") == 0) return SDL_TOUCH_PROFILE_TOUCH_PANE;
    if (strcmp(value, "CORNERS") == 0) return SDL_TOUCH_PROFILE_CORNERS;
    if (strcmp(value, "ROUND_WHEEL") == 0) return SDL_TOUCH_PROFILE_ROUND_WHEEL;
    if (strcmp(value, "ROUND") == 0) return SDL_TOUCH_PROFILE_ROUND_WHEEL;
    return SDL_TOUCH_PROFILE_TOUCH_PANE;
}

static const char* touch_movement_mode_to_string(int mode)
{
    switch (mode) {
    case SDL_TOUCH_MOVEMENT_OFF:
        return "OFF";
    case SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY:
        return "LONG_PRESS_ONLY";
    case SDL_TOUCH_MOVEMENT_ON:
    default:
        return "ON";
    }
}

static int normalize_touch_movement_mode(int mode)
{
    if (mode == SDL_TOUCH_MOVEMENT_OFF
        || mode == SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY)
    {
        return mode;
    }

    return SDL_TOUCH_MOVEMENT_ON;
}

static int parse_touch_movement_mode(const char* value)
{
    if (!value)
        return SDL_TOUCH_MOVEMENT_ON;
    if (strcmp(value, "ON") == 0) return SDL_TOUCH_MOVEMENT_ON;
    if (strcmp(value, "OFF") == 0) return SDL_TOUCH_MOVEMENT_OFF;
    if (strcmp(value, "LONG_PRESS_ONLY") == 0)
        return SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY;
    if (strcmp(value, "LONG_CLICK_ONLY") == 0)
        return SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY;
    return SDL_TOUCH_MOVEMENT_ON;
}

static const char* touch_zone_overlay_mode_to_string(int mode)
{
    switch (mode) {
    case SDL_TOUCH_ZONE_OVERLAY_OFF:
        return "OFF";
    case SDL_TOUCH_ZONE_OVERLAY_BORDERS:
        return "BORDERS";
    case SDL_TOUCH_ZONE_OVERLAY_BORDERS_LABELS:
        return "BORDERS_LABELS";
    case SDL_TOUCH_ZONE_OVERLAY_MARKERS:
    default:
        return "MARKERS";
    }
}

static int normalize_touch_zone_overlay_mode(int mode)
{
    if (mode >= SDL_TOUCH_ZONE_OVERLAY_OFF
        && mode < SDL_TOUCH_ZONE_OVERLAY_COUNT)
    {
        return mode;
    }

    return SDL_TOUCH_ZONE_OVERLAY_MARKERS;
}

static int parse_touch_zone_overlay_mode(const char* value)
{
    if (!value)
        return SDL_TOUCH_ZONE_OVERLAY_MARKERS;
    if (strcmp(value, "OFF") == 0 || strcmp(value, "NONE") == 0)
        return SDL_TOUCH_ZONE_OVERLAY_OFF;
    if (strcmp(value, "MARKERS") == 0 || strcmp(value, "SMALL_LINES") == 0)
        return SDL_TOUCH_ZONE_OVERLAY_MARKERS;
    if (strcmp(value, "BORDERS") == 0 || strcmp(value, "FULL_BORDERS") == 0)
        return SDL_TOUCH_ZONE_OVERLAY_BORDERS;
    if (strcmp(value, "BORDERS_LABELS") == 0
        || strcmp(value, "FULL_BORDERS_LABELS") == 0
        || strcmp(value, "BORDERS_WITH_NAMES") == 0)
    {
        return SDL_TOUCH_ZONE_OVERLAY_BORDERS_LABELS;
    }
    return SDL_TOUCH_ZONE_OVERLAY_MARKERS;
}

static const char* touch_corner_up_down_side_to_string(int side)
{
    switch (side) {
    case SDL_TOUCH_CORNER_UP_DOWN_LEFT:
        return "LEFT";
    case SDL_TOUCH_CORNER_UP_DOWN_RIGHT:
    default:
        return "RIGHT";
    }
}

static int normalize_touch_corner_up_down_side(int side)
{
    if (side == SDL_TOUCH_CORNER_UP_DOWN_LEFT
        || side == SDL_TOUCH_CORNER_UP_DOWN_RIGHT)
    {
        return side;
    }

    return SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
}

static int parse_touch_corner_up_down_side(const char* value)
{
    if (!value)
        return SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
    if (strcmp(value, "LEFT") == 0) return SDL_TOUCH_CORNER_UP_DOWN_LEFT;
    if (strcmp(value, "RIGHT") == 0) return SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
    return SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
}

static const char* touch_top_panel_mode_to_string(int mode)
{
    switch (mode) {
    case SDL_TOUCH_TOP_PANEL_MODE_LONG:
        return "LONG";
    case SDL_TOUCH_TOP_PANEL_MODE_SHORT:
    default:
        return "SHORT";
    }
}

static int normalize_touch_top_panel_mode(int mode)
{
    if (mode == SDL_TOUCH_TOP_PANEL_MODE_SHORT
        || mode == SDL_TOUCH_TOP_PANEL_MODE_LONG)
    {
        return mode;
    }

    return SDL_TOUCH_TOP_PANEL_MODE_SHORT;
}

static int parse_touch_top_panel_mode(const char* value)
{
    if (!value)
        return SDL_TOUCH_TOP_PANEL_MODE_SHORT;
    if (strcmp(value, "LONG") == 0) return SDL_TOUCH_TOP_PANEL_MODE_LONG;
    if (strcmp(value, "EXTENDED") == 0) return SDL_TOUCH_TOP_PANEL_MODE_LONG;
    if (strcmp(value, "SHORT") == 0) return SDL_TOUCH_TOP_PANEL_MODE_SHORT;
    return SDL_TOUCH_TOP_PANEL_MODE_SHORT;
}

static void sdl_touch_config_load_binding_array(cJSON* array, int* dst,
    int max_count)
{
    int count;

    if (!cJSON_IsArray(array) || !dst || max_count <= 0)
        return;

    count = cJSON_GetArraySize(array);
    for (int i = 0; i < max_count && i < count; i++) {
        cJSON* binding = cJSON_GetArrayItem(array, i);
        if (cJSON_IsNumber(binding))
            dst[i] = binding->valueint;
    }
}

static cJSON* sdl_touch_config_create_int_array(const int* src, int count)
{
    cJSON* array;

    if (!src || count <= 0)
        return NULL;

    array = cJSON_CreateArray();
    if (!array)
        return NULL;

    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(array, cJSON_CreateNumber(src[i]));
    }

    return array;
}

static void sdl_touch_config_set_default_top_panel_bindings(
    struct sdl_config* cfg)
{
    static const int top_panel_defaults[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT] = {
        'z', 'h', 'i', 'a', 'l', 'f',
    };
    static const int top_panel_long_defaults[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT] = {
        'Z', '\t', 'e', 'p', 'j', 'F',
    };

    if (!cfg)
        return;

    memcpy(cfg->touch_top_panel_bindings, top_panel_defaults,
        sizeof(top_panel_defaults));
    memcpy(cfg->touch_top_panel_long_bindings, top_panel_long_defaults,
        sizeof(top_panel_long_defaults));
}

static void sdl_touch_config_migrate_top_panel_layout(
    struct sdl_config* cfg, int tap_count, int long_count)
{
    int old_taps[SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT] = { 0 };
    int old_longs[SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT] = { 0 };
    int saved_taps[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    int saved_longs[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    bool old_default_taps;
    bool old_default_longs;

    if (!cfg)
        return;

    memcpy(saved_taps, cfg->touch_top_panel_bindings, sizeof(saved_taps));
    memcpy(saved_longs, cfg->touch_top_panel_long_bindings,
        sizeof(saved_longs));
    if (tap_count == SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT) {
        memcpy(old_taps, cfg->touch_top_panel_bindings,
            sizeof(old_taps));
    }
    if (long_count == SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT) {
        memcpy(old_longs, cfg->touch_top_panel_long_bindings,
            sizeof(old_longs));
    }

    old_default_taps = tap_count == SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT
        && cfg->touch_top_panel_bindings[0] == 'h'
        && cfg->touch_top_panel_bindings[1] == 'i'
        && cfg->touch_top_panel_bindings[2] == 'j'
        && cfg->touch_top_panel_bindings[3] == 'f';
    old_default_longs = long_count == SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT
        && cfg->touch_top_panel_long_bindings[0] == '\t'
        && cfg->touch_top_panel_long_bindings[1] == 'e'
        && cfg->touch_top_panel_long_bindings[2] == 's'
        && cfg->touch_top_panel_long_bindings[3] == 'F';

    if (old_default_taps && old_default_longs) {
        sdl_touch_config_set_default_top_panel_bindings(cfg);
        log_info("Migrated default top widget buttons to short/long layout");
        return;
    }

    if (tap_count == SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT
        || long_count == SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT)
    {
        sdl_touch_config_set_default_top_panel_bindings(cfg);
        if (tap_count != SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT)
            memcpy(cfg->touch_top_panel_bindings, saved_taps,
                sizeof(saved_taps));
        if (long_count != SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT)
            memcpy(cfg->touch_top_panel_long_bindings, saved_longs,
                sizeof(saved_longs));
    }

    if (tap_count == SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT) {
        for (int i = 0; i < SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT; i++)
            cfg->touch_top_panel_bindings[i + 1] = old_taps[i];
        if (long_count != SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT)
            log_info("Shifted four-button top widget tap bindings into short layout");
    }

    if (long_count == SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT) {
        for (int i = 0; i < SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT; i++)
            cfg->touch_top_panel_long_bindings[i + 1] = old_longs[i];
        log_info((tap_count == SDL_TOUCH_TOP_PANEL_SHORT_BUTTON_COUNT)
            ? "Shifted four-button top widget bindings into short layout"
            : "Shifted four-button top widget long-tap bindings into short layout");
    }
}

void sdl_touch_config_load(cJSON* root, struct sdl_config* cfg,
    bool legacy_swipe_bindings_loaded)
{
    cJSON* touch_control;
    bool saw_top_panel_mode = false;
    bool saw_touch_control_swipe_bindings = false;
    int top_panel_bindings_count = -1;
    int top_panel_long_bindings_count = -1;

    if (!root || !cfg)
        return;

    touch_control = cJSON_GetObjectItemCaseSensitive(root, "touchControl");
    if (cJSON_IsObject(touch_control)) {
        cJSON* profile = cJSON_GetObjectItemCaseSensitive(
            touch_control, "profile");
        cJSON* touch_pane_default_open =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "touchPaneDefaultOpen");
        cJSON* menu_commands = cJSON_GetObjectItemCaseSensitive(
            touch_control, "menuCommandsEnabled");
        cJSON* inventory_menu_commands =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "inventoryEquipmentMenuCommandsEnabled");
        cJSON* supply_menu_commands =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "supplyMenuCommandsEnabled");
        cJSON* other_menu_commands = cJSON_GetObjectItemCaseSensitive(
            touch_control, "otherMenuCommandsEnabled");
        cJSON* movement_mode = cJSON_GetObjectItemCaseSensitive(
            touch_control, "movementMode");
        cJSON* round_movement = cJSON_GetObjectItemCaseSensitive(
            touch_control, "roundMovementLayerEnabled");
        cJSON* corner_button_overlay =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "cornerButtonOverlayMode");
        cJSON* corner_button_markers =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "cornerButtonMarkersEnabled");
        cJSON* corner_button_borders =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "cornerButtonBordersEnabled");
        cJSON* corner_button_center_bindings =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "cornerButtonCenterBindings");
        cJSON* corner_button_up_down_side =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "cornerButtonUpDownSide");
        cJSON* corner_button_action_bindings =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "cornerButtonActionBindings");
        cJSON* top_panel_mode = cJSON_GetObjectItemCaseSensitive(
            touch_control, "topPanelMode");
        cJSON* top_panel_default_open =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "topPanelDefaultOpen");
        cJSON* top_panel_bindings =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "topPanelBindings");
        cJSON* top_panel_long_bindings =
            cJSON_GetObjectItemCaseSensitive(touch_control,
                "topPanelLongBindings");
        cJSON* swipe_enabled = cJSON_GetObjectItemCaseSensitive(
            touch_control, "swipeEnabled");
        cJSON* swipe_bindings = cJSON_GetObjectItemCaseSensitive(
            touch_control, "swipeBindings");

        if (cJSON_IsString(profile) && profile->valuestring) {
            cfg->touch_profile = parse_touch_profile(profile->valuestring);
            log_debug("Loaded touchControl.profile: %s",
                touch_profile_to_string(cfg->touch_profile));
        } else if (cJSON_IsNumber(profile)) {
            cfg->touch_profile = normalize_touch_profile(profile->valueint);
            log_debug("Loaded numeric touchControl.profile: %s",
                touch_profile_to_string(cfg->touch_profile));
        }

        if (cJSON_IsBool(touch_pane_default_open)) {
            cfg->touch_pane_default_open =
                cJSON_IsTrue(touch_pane_default_open);
            log_debug("Loaded touchControl.touchPaneDefaultOpen: %s",
                cfg->touch_pane_default_open ? "true" : "false");
        }

        if (cJSON_IsBool(menu_commands)) {
            bool value = cJSON_IsTrue(menu_commands);

            for (int i = 0; i < SDL_TOUCH_MENU_CATEGORY_COUNT; i++)
                cfg->touch_menu_command_enabled[i] = value;
            log_debug("Loaded legacy touchControl.menuCommandsEnabled: %s",
                value ? "true" : "false");
        }

        if (cJSON_IsBool(inventory_menu_commands)) {
            cfg->touch_menu_command_enabled[
                SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT] =
                cJSON_IsTrue(inventory_menu_commands);
            log_debug("Loaded touchControl.inventoryEquipmentMenuCommandsEnabled: %s",
                cfg->touch_menu_command_enabled[
                    SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT]
                    ? "true" : "false");
        }

        if (cJSON_IsBool(supply_menu_commands)) {
            cfg->touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_SUPPLY] =
                cJSON_IsTrue(supply_menu_commands);
            log_debug("Loaded touchControl.supplyMenuCommandsEnabled: %s",
                cfg->touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_SUPPLY]
                    ? "true" : "false");
        }

        if (cJSON_IsBool(other_menu_commands)) {
            cfg->touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_OTHER] =
                cJSON_IsTrue(other_menu_commands);
            log_debug("Loaded touchControl.otherMenuCommandsEnabled: %s",
                cfg->touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_OTHER]
                    ? "true" : "false");
        }

        if (cJSON_IsString(movement_mode) && movement_mode->valuestring) {
            cfg->touch_movement_mode =
                parse_touch_movement_mode(movement_mode->valuestring);
            log_debug("Loaded touchControl.movementMode: %s",
                touch_movement_mode_to_string(cfg->touch_movement_mode));
        } else if (cJSON_IsNumber(movement_mode)) {
            cfg->touch_movement_mode =
                normalize_touch_movement_mode(movement_mode->valueint);
            log_debug("Loaded numeric touchControl.movementMode: %s",
                touch_movement_mode_to_string(cfg->touch_movement_mode));
        }

        if (cJSON_IsBool(round_movement)) {
            cfg->touch_round_movement_enabled =
                cJSON_IsTrue(round_movement);
            log_debug("Loaded touchControl.roundMovementLayerEnabled: %s",
                cfg->touch_round_movement_enabled ? "true" : "false");
        }

        if (cJSON_IsString(corner_button_overlay)
            && corner_button_overlay->valuestring)
        {
            cfg->touch_zone_overlay_mode =
                parse_touch_zone_overlay_mode(corner_button_overlay->valuestring);
            log_debug("Loaded touchControl.cornerButtonOverlayMode: %s",
                touch_zone_overlay_mode_to_string(
                    cfg->touch_zone_overlay_mode));
        } else if (cJSON_IsNumber(corner_button_overlay)) {
            cfg->touch_zone_overlay_mode =
                normalize_touch_zone_overlay_mode(
                    corner_button_overlay->valueint);
            log_debug("Loaded numeric touchControl.cornerButtonOverlayMode: %s",
                touch_zone_overlay_mode_to_string(
                    cfg->touch_zone_overlay_mode));
        } else if (cJSON_IsBool(corner_button_borders)
            || cJSON_IsBool(corner_button_markers))
        {
            bool borders = cJSON_IsBool(corner_button_borders)
                && cJSON_IsTrue(corner_button_borders);
            bool markers = !cJSON_IsBool(corner_button_markers)
                || cJSON_IsTrue(corner_button_markers);

            cfg->touch_zone_overlay_mode = borders
                ? SDL_TOUCH_ZONE_OVERLAY_BORDERS
                : (markers ? SDL_TOUCH_ZONE_OVERLAY_MARKERS
                           : SDL_TOUCH_ZONE_OVERLAY_OFF);
            log_debug("Migrated touchControl corner button overlay mode: %s",
                touch_zone_overlay_mode_to_string(
                    cfg->touch_zone_overlay_mode));
        }

        if (cJSON_IsArray(corner_button_center_bindings)) {
            int count = cJSON_GetArraySize(corner_button_center_bindings);
            sdl_touch_config_load_binding_array(corner_button_center_bindings,
                cfg->touch_zone_center_bindings,
                SDL_TOUCH_ZONE_CENTER_BINDING_COUNT);
            log_debug("Loaded touchControl.cornerButtonCenterBindings (%d entries)",
                count);
        }

        if (cJSON_IsString(corner_button_up_down_side)
            && corner_button_up_down_side->valuestring)
        {
            cfg->touch_corner_up_down_side =
                parse_touch_corner_up_down_side(
                    corner_button_up_down_side->valuestring);
            log_debug("Loaded touchControl.cornerButtonUpDownSide: %s",
                touch_corner_up_down_side_to_string(
                    cfg->touch_corner_up_down_side));
        } else if (cJSON_IsNumber(corner_button_up_down_side)) {
            cfg->touch_corner_up_down_side =
                normalize_touch_corner_up_down_side(
                    corner_button_up_down_side->valueint);
            log_debug("Loaded numeric touchControl.cornerButtonUpDownSide: %s",
                touch_corner_up_down_side_to_string(
                    cfg->touch_corner_up_down_side));
        }

        if (cJSON_IsArray(corner_button_action_bindings)) {
            int count = cJSON_GetArraySize(corner_button_action_bindings);
            sdl_touch_config_load_binding_array(corner_button_action_bindings,
                cfg->touch_corner_action_bindings,
                SDL_TOUCH_CORNER_ACTION_BINDING_COUNT);
            log_debug("Loaded touchControl.cornerButtonActionBindings (%d entries)",
                count);
        }

        if (cJSON_IsString(top_panel_mode) && top_panel_mode->valuestring) {
            saw_top_panel_mode = true;
            cfg->touch_top_panel_mode =
                parse_touch_top_panel_mode(top_panel_mode->valuestring);
            log_debug("Loaded touchControl.topPanelMode: %s",
                touch_top_panel_mode_to_string(cfg->touch_top_panel_mode));
        } else if (cJSON_IsNumber(top_panel_mode)) {
            saw_top_panel_mode = true;
            cfg->touch_top_panel_mode =
                normalize_touch_top_panel_mode(top_panel_mode->valueint);
            log_debug("Loaded numeric touchControl.topPanelMode: %s",
                touch_top_panel_mode_to_string(cfg->touch_top_panel_mode));
        }

        if (cJSON_IsBool(top_panel_default_open)) {
            cfg->touch_top_panel_default_open =
                cJSON_IsTrue(top_panel_default_open);
            log_debug("Loaded touchControl.topPanelDefaultOpen: %s",
                cfg->touch_top_panel_default_open ? "true" : "false");
        }

        if (cJSON_IsArray(top_panel_bindings)) {
            top_panel_bindings_count = cJSON_GetArraySize(top_panel_bindings);
            sdl_touch_config_load_binding_array(top_panel_bindings,
                cfg->touch_top_panel_bindings,
                SDL_TOUCH_TOP_PANEL_BUTTON_COUNT);
            log_debug("Loaded touchControl.topPanelBindings (%d entries)",
                top_panel_bindings_count);
        }

        if (cJSON_IsArray(top_panel_long_bindings)) {
            top_panel_long_bindings_count =
                cJSON_GetArraySize(top_panel_long_bindings);
            sdl_touch_config_load_binding_array(top_panel_long_bindings,
                cfg->touch_top_panel_long_bindings,
                SDL_TOUCH_TOP_PANEL_BUTTON_COUNT);
            log_debug("Loaded touchControl.topPanelLongBindings (%d entries)",
                top_panel_long_bindings_count);
        }

        if (cJSON_IsBool(swipe_enabled)) {
            cfg->touch_swipe_enabled = cJSON_IsTrue(swipe_enabled);
            log_debug("Loaded touchControl.swipeEnabled: %s",
                cfg->touch_swipe_enabled ? "true" : "false");
        }

        if (cJSON_IsArray(swipe_bindings)) {
            int count = cJSON_GetArraySize(swipe_bindings);
            saw_touch_control_swipe_bindings = true;
            sdl_touch_config_load_binding_array(swipe_bindings,
                cfg->touch_swipe_bindings, TOUCH_SWIPE_DIR_COUNT);
            log_debug("Loaded touchControl.swipeBindings (%d entries)",
                count);
        }
    }

    sdl_touch_config_migrate_top_panel_layout(cfg, top_panel_bindings_count,
        top_panel_long_bindings_count);

    if (!saw_top_panel_mode) {
        cfg->touch_top_panel_mode =
            (cfg->touch_profile == SDL_TOUCH_PROFILE_ROUND_WHEEL
                || cfg->touch_round_movement_enabled)
                ? SDL_TOUCH_TOP_PANEL_MODE_LONG
                : SDL_TOUCH_TOP_PANEL_MODE_SHORT;
    }

    if ((legacy_swipe_bindings_loaded || saw_touch_control_swipe_bindings)
        && cfg->touch_swipe_bindings[TOUCH_SWIPE_DIR_UP] == '8'
        && cfg->touch_swipe_bindings[TOUCH_SWIPE_DIR_DOWN] == '2'
        && cfg->touch_swipe_bindings[TOUCH_SWIPE_DIR_LEFT] == '4'
        && cfg->touch_swipe_bindings[TOUCH_SWIPE_DIR_RIGHT] == '6')
    {
        cfg->touch_swipe_bindings[TOUCH_SWIPE_DIR_UP] =
            TOUCH_BIND_TOP_PANEL_CLOSE;
        cfg->touch_swipe_bindings[TOUCH_SWIPE_DIR_DOWN] =
            TOUCH_BIND_TOP_PANEL_OPEN;
        log_info("Migrated default touch swipe up/down bindings to top panel actions");
    }

    cfg->touch_movement_mode =
        normalize_touch_movement_mode(cfg->touch_movement_mode);
    cfg->touch_zone_overlay_mode =
        normalize_touch_zone_overlay_mode(cfg->touch_zone_overlay_mode);
    cfg->touch_corner_up_down_side =
        normalize_touch_corner_up_down_side(cfg->touch_corner_up_down_side);
    cfg->touch_top_panel_mode =
        normalize_touch_top_panel_mode(cfg->touch_top_panel_mode);
    cfg->touch_profile = normalize_touch_profile(cfg->touch_profile);
}

void sdl_touch_config_save(cJSON* root, const struct sdl_config* cfg)
{
    cJSON* touch_control;

    if (!root || !cfg)
        return;

    touch_control = cJSON_CreateObject();
    if (!touch_control)
        return;

    cJSON_AddStringToObject(touch_control, "profile",
        touch_profile_to_string(cfg->touch_profile));
    cJSON_AddBoolToObject(touch_control, "touchPaneDefaultOpen",
        cfg->touch_pane_default_open);
    cJSON_AddBoolToObject(touch_control,
        "inventoryEquipmentMenuCommandsEnabled",
        cfg->touch_menu_command_enabled[
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT]);
    cJSON_AddBoolToObject(touch_control, "supplyMenuCommandsEnabled",
        cfg->touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_SUPPLY]);
    cJSON_AddBoolToObject(touch_control, "otherMenuCommandsEnabled",
        cfg->touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_OTHER]);
    cJSON_AddStringToObject(touch_control, "movementMode",
        touch_movement_mode_to_string(cfg->touch_movement_mode));
    cJSON_AddBoolToObject(touch_control, "roundMovementLayerEnabled",
        cfg->touch_round_movement_enabled);
    cJSON_AddStringToObject(touch_control, "cornerButtonOverlayMode",
        touch_zone_overlay_mode_to_string(cfg->touch_zone_overlay_mode));
    cJSON_AddStringToObject(touch_control, "cornerButtonUpDownSide",
        touch_corner_up_down_side_to_string(cfg->touch_corner_up_down_side));
    cJSON_AddStringToObject(touch_control, "topPanelMode",
        touch_top_panel_mode_to_string(cfg->touch_top_panel_mode));
    cJSON_AddBoolToObject(touch_control, "topPanelDefaultOpen",
        cfg->touch_top_panel_default_open);
    cJSON_AddBoolToObject(touch_control, "swipeEnabled",
        cfg->touch_swipe_enabled);

    {
        cJSON* center_bindings = sdl_touch_config_create_int_array(
            cfg->touch_zone_center_bindings,
            SDL_TOUCH_ZONE_CENTER_BINDING_COUNT);
        cJSON* corner_action_bindings = sdl_touch_config_create_int_array(
            cfg->touch_corner_action_bindings,
            SDL_TOUCH_CORNER_ACTION_BINDING_COUNT);
        cJSON* top_panel_bindings = sdl_touch_config_create_int_array(
            cfg->touch_top_panel_bindings,
            SDL_TOUCH_TOP_PANEL_BUTTON_COUNT);
        cJSON* top_panel_long_bindings = sdl_touch_config_create_int_array(
            cfg->touch_top_panel_long_bindings,
            SDL_TOUCH_TOP_PANEL_BUTTON_COUNT);
        cJSON* swipe_bindings = sdl_touch_config_create_int_array(
            cfg->touch_swipe_bindings, TOUCH_SWIPE_DIR_COUNT);

        if (center_bindings) {
            cJSON_AddItemToObject(touch_control,
                "cornerButtonCenterBindings", center_bindings);
        }
        if (corner_action_bindings) {
            cJSON_AddItemToObject(touch_control,
                "cornerButtonActionBindings", corner_action_bindings);
        }
        if (top_panel_bindings) {
            cJSON_AddItemToObject(touch_control,
                "topPanelBindings", top_panel_bindings);
        }
        if (top_panel_long_bindings) {
            cJSON_AddItemToObject(touch_control,
                "topPanelLongBindings", top_panel_long_bindings);
        }
        if (swipe_bindings) {
            cJSON_AddItemToObject(touch_control, "swipeBindings",
                swipe_bindings);
        }
    }

    cJSON_AddItemToObject(root, "touchControl", touch_control);
}

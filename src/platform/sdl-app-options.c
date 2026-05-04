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

#include "cJSON.h"
#include "log/log.h"
#include "platform/sdl-app-options.h"
#include "platform-input.h"
#include "sdl-config.h"
#include <SDL3/SDL_filesystem.h>
#include <stdio.h>
#include <stdlib.h>

static bool g_app_intro_seen = false;
static bool g_app_touch_tutorial_seen = false;
static bool g_app_touch_tutorial_requested = false;
static bool g_app_mouse_tutorial_seen = false;
static bool g_app_mouse_tutorial_requested = false;

static const byte app_interface_options[] = {
    OPT_system_beep, OPT_quick_messages, OPT_auto_more, OPT_easy_main_menu,
    OPT_hjkl_movement, OPT_angband_keyset, OPT_space_acts_as_comma,
    OPT_look_objects_sort_by_difficulty, OPT_show_level_generation_debug,
    OPT_NONE
};

static const byte app_text_options[] = {
    OPT_story_lists, OPT_story_lists_inven, OPT_story_lists_inven_pane,
    OPT_story_lists_equip, OPT_story_lists_equip_pane, OPT_story_monster_desc,
    OPT_story_monster_desc_pane, OPT_story_character_sheet,
    OPT_NONE
};

static const byte app_efficiency_options[] = {
    OPT_instant_run, OPT_center_player, OPT_run_avoid_center,
    OPT_NONE
};

static const byte app_gameplay_options[] = {
    OPT_pacifist_attack_warning, OPT_unlock_blitz_mode,
    OPT_NONE
};

static const byte app_visual_options[] = {
    OPT_auto_display_lists, OPT_artifact_unique_color, OPT_hilite_player,
    OPT_hilite_target, OPT_hilite_unwary, OPT_solid_walls, OPT_hybrid_walls,
    OPT_unidentified_items_slate, OPT_stealth_vision, OPT_sleep_icon,
    OPT_show_smithing_difficulty, OPT_show_smithing_difficulty_look,
    OPT_show_elemental_item_rolls,
    OPT_NONE
};

static char* sdl_app_options_read_file_contents(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    long size;
    char* buffer;

    if (!f) {
        log_debug("Could not open JSON file: %s", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)size, f) != (size_t)size) {
        free(buffer);
        fclose(f);
        return NULL;
    }

    buffer[size] = '\0';
    fclose(f);
    return buffer;
}

static bool option_list_contains(const byte* ids, int opt)
{
    if (!ids)
        return false;

    for (int i = 0; ids[i] != OPT_NONE; i++) {
        if ((int)ids[i] == opt)
            return true;
    }

    return false;
}

bool option_is_app_persistent(int opt)
{
    if (opt == OPT_intro_style || opt == OPT_banner_popup_seconds
        || opt == OPT_hide_left_panel || opt == OPT_min_depth_timer_mode)
    {
        return true;
    }
    return option_list_contains(app_interface_options, opt)
        || option_list_contains(app_text_options, opt)
        || option_list_contains(app_efficiency_options, opt)
        || option_list_contains(app_gameplay_options, opt)
        || option_list_contains(app_visual_options, opt);
}

static void sdl_config_apply_app_option_defaults(void)
{
    if (!op_ptr)
        return;

    op_ptr->delay_factor = 5;
    op_ptr->hitpoint_warn = 3;
    op_ptr->main_combat_rolls = platform_steamdeck_mode() ? 2 : 0;
    op_ptr->narrative_banner_seconds = NARRATIVE_BANNER_SECONDS_DEFAULT;
    op_ptr->min_depth_timer_mode = MIN_DEPTH_TIMER_MODE_NORMAL;
#if defined(__ANDROID__) || defined(SIL_IOS)
    op_ptr->ability_desc_mode = 1;
#else
    op_ptr->ability_desc_mode = 0;
#endif
    op_ptr->intro_style = INTRO_STYLE_RANDOM;
}

static void sdl_config_load_app_option_group(cJSON* app_options,
    const char* group_name, const byte* option_ids)
{
    cJSON* group = cJSON_GetObjectItemCaseSensitive(app_options, group_name);
    if (!op_ptr)
        return;
    if (!cJSON_IsObject(group))
        return;

    for (int i = 0; option_ids[i] != OPT_NONE; i++) {
        int opt = option_ids[i];
        cptr key = option_text[opt];
        cJSON* item;

        if (!key)
            continue;

        item = cJSON_GetObjectItemCaseSensitive(group, key);
        if (cJSON_IsBool(item))
            op_ptr->opt[opt] = cJSON_IsTrue(item);
    }
}

static void sdl_config_save_app_option_group(cJSON* app_options,
    const char* group_name, const byte* option_ids)
{
    cJSON* group;

    if (!op_ptr)
        return;

    group = cJSON_CreateObject();
    if (!group)
        return;

    for (int i = 0; option_ids[i] != OPT_NONE; i++) {
        int opt = option_ids[i];
        cptr key = option_text[opt];

        if (!key)
            continue;

        cJSON_AddBoolToObject(group, key, op_ptr->opt[opt]);
    }

    cJSON_AddItemToObject(app_options, group_name, group);
}

static void sdl_config_load_byte_value(cJSON* parent, const char* key,
    byte* out_value, byte max_value)
{
    cJSON* item = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (!cJSON_IsNumber(item))
        return;

    if (item->valueint < 0)
        return;

    if (item->valueint > max_value) {
        *out_value = max_value;
        return;
    }

    *out_value = (byte)item->valueint;
}

void sdl_config_load_app_options(const char* filename)
{
    char* content;
    cJSON* root;
    cJSON* app_options;
    cJSON* item;
    bool config_exists = false;

    sdl_config_apply_app_option_defaults();

    if (filename && filename[0])
        config_exists = SDL_GetPathInfo(filename, NULL);

    g_app_intro_seen = config_exists;
    g_app_touch_tutorial_seen = false;
    g_app_touch_tutorial_requested = false;
    g_app_mouse_tutorial_seen = false;
    g_app_mouse_tutorial_requested = false;

    if (!filename || !filename[0]) {
        log_warn("sdl_config_load_app_options: no config filename provided");
        return;
    }

    content = sdl_app_options_read_file_contents(filename);
    if (!content) {
        log_debug("No app options found in SDL config, using defaults");
        return;
    }

    root = cJSON_Parse(content);
    free(content);

    if (!root) {
        log_warn("sdl_config_load_app_options: failed to parse %s", filename);
        return;
    }

    app_options = cJSON_GetObjectItemCaseSensitive(root, "appOptions");
    if (!cJSON_IsObject(app_options) || !op_ptr) {
        cJSON_Delete(root);
        return;
    }

    item = cJSON_GetObjectItemCaseSensitive(app_options, "introSeen");
    if (cJSON_IsBool(item))
        g_app_intro_seen = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "touchTutorialSeen");
    if (cJSON_IsBool(item))
        g_app_touch_tutorial_seen = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "mouseTutorialSeen");
    if (cJSON_IsBool(item))
        g_app_mouse_tutorial_seen = cJSON_IsTrue(item);

    sdl_config_load_app_option_group(app_options, "interface",
        app_interface_options);
    sdl_config_load_app_option_group(app_options, "text", app_text_options);
    sdl_config_load_app_option_group(app_options, "efficiency",
        app_efficiency_options);
    sdl_config_load_app_option_group(app_options, "gameplay",
        app_gameplay_options);
    sdl_config_load_app_option_group(app_options, "visual",
        app_visual_options);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "interface");
    if (cJSON_IsObject(item))
        sdl_config_load_byte_value(item, "hitpointWarning",
            &op_ptr->hitpoint_warn, 9);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "efficiency");
    if (cJSON_IsObject(item))
        sdl_config_load_byte_value(item, "delayFactor",
            &op_ptr->delay_factor, 9);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "gameplay");
    if (cJSON_IsObject(item)) {
        sdl_config_load_byte_value(item, "minDepthTimerMode",
            &op_ptr->min_depth_timer_mode, MIN_DEPTH_TIMER_MODE_MAX);
    }

    item = cJSON_GetObjectItemCaseSensitive(app_options, "visual");
    if (cJSON_IsObject(item)) {
        sdl_config_load_byte_value(item, "mainCombatRolls",
            &op_ptr->main_combat_rolls, 4);
        sdl_config_load_byte_value(item, "bannerPopupSeconds",
            &op_ptr->narrative_banner_seconds, NARRATIVE_BANNER_SECONDS_MAX);
        sdl_config_load_byte_value(item, "abilityDescMode",
            &op_ptr->ability_desc_mode, 2);
        sdl_config_load_byte_value(item, "introStyle",
            &op_ptr->intro_style, INTRO_STYLE_RANDOM);
    }

    cJSON_Delete(root);
}

bool platform_intro_should_force_flame(void)
{
    return !g_app_intro_seen;
}

void platform_intro_mark_seen(void)
{
    g_app_intro_seen = true;
}

bool platform_touch_tutorial_seen(void)
{
    return g_app_touch_tutorial_seen;
}

void platform_touch_tutorial_mark_seen(void)
{
    g_app_touch_tutorial_seen = true;
}

bool platform_touch_tutorial_requested(void)
{
    return g_app_touch_tutorial_requested;
}

void platform_touch_tutorial_request(void)
{
    g_app_touch_tutorial_requested = true;
}

void platform_touch_tutorial_clear_request(void)
{
    g_app_touch_tutorial_requested = false;
}

bool platform_mouse_tutorial_seen(void)
{
    return g_app_mouse_tutorial_seen;
}

void platform_mouse_tutorial_mark_seen(void)
{
    g_app_mouse_tutorial_seen = true;
}

bool platform_mouse_tutorial_requested(void)
{
    return g_app_mouse_tutorial_requested;
}

void platform_mouse_tutorial_request(void)
{
    g_app_mouse_tutorial_requested = true;
}

void platform_mouse_tutorial_clear_request(void)
{
    g_app_mouse_tutorial_requested = false;
}

void sdl_config_save_app_options(cJSON* root)
{
    cJSON* app_options = NULL;
    cJSON* interface = NULL;
    cJSON* efficiency = NULL;
    cJSON* gameplay = NULL;
    cJSON* visual = NULL;

    if (!root || !op_ptr)
        return;

    app_options = cJSON_CreateObject();
    if (!app_options)
        return;

    cJSON_AddBoolToObject(app_options, "introSeen", g_app_intro_seen);
    cJSON_AddBoolToObject(app_options, "touchTutorialSeen",
        g_app_touch_tutorial_seen);
    cJSON_AddBoolToObject(app_options, "mouseTutorialSeen",
        g_app_mouse_tutorial_seen);

    sdl_config_save_app_option_group(app_options, "interface",
        app_interface_options);
    sdl_config_save_app_option_group(app_options, "text", app_text_options);
    sdl_config_save_app_option_group(app_options, "efficiency",
        app_efficiency_options);
    sdl_config_save_app_option_group(app_options, "gameplay",
        app_gameplay_options);
    sdl_config_save_app_option_group(app_options, "visual", app_visual_options);

    interface = cJSON_GetObjectItemCaseSensitive(app_options, "interface");
    if (cJSON_IsObject(interface)) {
        cJSON_AddNumberToObject(interface, "hitpointWarning",
            op_ptr->hitpoint_warn);
    }

    efficiency = cJSON_GetObjectItemCaseSensitive(app_options, "efficiency");
    if (cJSON_IsObject(efficiency)) {
        cJSON_AddNumberToObject(efficiency, "delayFactor",
            op_ptr->delay_factor);
    }

    gameplay = cJSON_GetObjectItemCaseSensitive(app_options, "gameplay");
    if (cJSON_IsObject(gameplay)) {
        cJSON_AddNumberToObject(gameplay, "minDepthTimerMode",
            op_ptr->min_depth_timer_mode);
    }

    visual = cJSON_GetObjectItemCaseSensitive(app_options, "visual");
    if (cJSON_IsObject(visual)) {
        cJSON_AddNumberToObject(visual, "mainCombatRolls",
            op_ptr->main_combat_rolls);
        cJSON_AddNumberToObject(visual, "bannerPopupSeconds",
            op_ptr->narrative_banner_seconds);
        cJSON_AddNumberToObject(visual, "abilityDescMode",
            op_ptr->ability_desc_mode);
        cJSON_AddNumberToObject(visual, "introStyle", op_ptr->intro_style);
    }

    cJSON_AddItemToObject(root, "appOptions", app_options);
}

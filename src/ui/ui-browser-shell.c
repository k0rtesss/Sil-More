/*
 * Copyright (C) 2026 Sil-More contributors
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

#include "ui/ui-browser-shell.h"

#include "app/app-session.h"
#include "ui/ui-information-scene.h"

void ui_browser_shell_scene_config_init(ui_browser_shell_scene_config* config)
{
    if (!config)
        return;

    memset(config, 0, sizeof(*config));
    config->layer = APP_UI_LAYER_BROWSER;
    config->style = APP_UI_PANEL_STYLE_BROWSER;
    config->panel_flags = APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    config->title_attr = TERM_L_WHITE;
    config->subtitle_attr = TERM_SLATE;
    config->accent_attr = TERM_L_BLUE;
    config->min_width_px = 980;
    config->width_cap_px = 2048;
}

app_ui_panel* ui_browser_shell_begin(app_ui_scene* scene,
    const ui_browser_shell_scene_config* config)
{
    ui_semantic_panel_config semantic_config;

    if (!scene || !config)
        return NULL;

    memset(&semantic_config, 0, sizeof(semantic_config));
    semantic_config.scene_flags = config->scene_flags;
    semantic_config.layer = config->layer;
    semantic_config.style = config->style;
    semantic_config.panel_flags = config->panel_flags;
    semantic_config.title_attr = config->title_attr;
    semantic_config.subtitle_attr = config->subtitle_attr;
    semantic_config.accent_attr = config->accent_attr;
    semantic_config.min_width_px = config->min_width_px;
    semantic_config.width_cap_px = config->width_cap_px;
    semantic_config.title = config->title;
    semantic_config.subtitle = config->subtitle;

    return ui_semantic_scene_begin_panel(scene, &semantic_config);
}

app_ui_panel* ui_browser_shell_begin_browser(app_ui_scene* scene,
    byte title_attr, cptr title, byte subtitle_attr, cptr subtitle,
    byte accent_attr, u16b panel_flags, u16b min_width_px,
    u16b width_cap_px)
{
    ui_browser_shell_scene_config config;

    ui_browser_shell_scene_config_init(&config);
    config.title_attr = title_attr;
    config.title = title;
    config.subtitle_attr = subtitle_attr;
    config.subtitle = subtitle;
    config.accent_attr = accent_attr;
    config.panel_flags = panel_flags;
    config.min_width_px = min_width_px;
    config.width_cap_px = width_cap_px;
    return ui_browser_shell_begin(scene, &config);
}

bool ui_browser_shell_add_footer_actions(app_ui_panel* panel,
    const ui_browser_shell_footer_action* actions, size_t action_count)
{
    size_t i;

    if (!panel)
        return false;
    if (!actions && action_count > 0)
        return false;

    for (i = 0; i < action_count; i++)
    {
        const ui_browser_shell_footer_action* action = &actions[i];

        if (!action->label || !action->label[0])
            continue;
        if (!app_ui_panel_add_footer_action(panel, action->id, action->attr,
                action->enabled, action->key, action->label))
        {
            return false;
        }
    }

    return true;
}

bool ui_browser_shell_add_tab(app_ui_panel* panel, s16b id, byte attr,
    bool active, cptr label, s16b action_key, cptr tooltip)
{
    if (!panel)
        return false;
    if (!app_ui_panel_add_tab(panel, id, attr, active, label))
        return false;

    return app_ui_panel_set_tab_interaction(panel, id, APP_UI_WIDGET_ROLE_TAB,
        APP_UI_WIDGET_ACTION_SELECT,
        APP_UI_INTERACTION_FLAG_POINTER_ENABLED
            | APP_UI_INTERACTION_FLAG_TOUCH_TARGET,
        action_key, tooltip);
}

void ui_browser_shell_prompt_label(int binding, const char* fallback,
    char* buf, size_t buflen)
{
    ui_semantic_prompt_label(binding, fallback, buf, buflen);
}

ui_browser_shell_scroll_keys ui_browser_shell_default_scroll_keys(void)
{
    ui_browser_shell_scroll_keys keys;

    keys.up_key = '8';
    keys.down_key = '2';
    keys.left_key = '4';
    keys.right_key = '6';
    return keys;
}

void ui_browser_shell_command_map_init(ui_browser_shell_command_map* map)
{
    if (!map)
        return;

    memset(map, 0, sizeof(*map));
    map->scroll_keys = ui_browser_shell_default_scroll_keys();
    map->cancel_key = ESCAPE;
    map->row_activate_key = '\r';
    map->row_inspect_key = 'x';
}

char ui_browser_shell_scroll_command_key(const app_ui_command* command,
    const ui_browser_shell_scroll_keys* keys)
{
    ui_browser_shell_scroll_keys default_keys;

    if (!command)
        return '\0';
    if (!keys)
    {
        default_keys = ui_browser_shell_default_scroll_keys();
        keys = &default_keys;
    }

    if (ABS(command->scroll_y) >= ABS(command->scroll_x)
        && command->scroll_y != 0)
    {
        return (command->scroll_y > 0) ? keys->up_key : keys->down_key;
    }
    if (command->scroll_x != 0)
        return (command->scroll_x < 0) ? keys->left_key : keys->right_key;

    return '\0';
}

char ui_browser_shell_direction_command_key(const app_ui_command* command,
    const ui_browser_shell_scroll_keys* keys)
{
    ui_browser_shell_scroll_keys default_keys;

    if (!command)
        return '\0';
    if (!keys)
    {
        default_keys = ui_browser_shell_default_scroll_keys();
        keys = &default_keys;
    }

    if (ABS(command->dy) >= ABS(command->dx) && command->dy != 0)
        return (command->dy < 0) ? keys->up_key : keys->down_key;
    if (command->dx != 0)
        return (command->dx < 0) ? keys->left_key : keys->right_key;

    return '\0';
}

static char ui_browser_shell_lookup_button_key(
    const ui_browser_shell_command_map* map, s16b id)
{
    size_t i;

    if (!map || !map->button_keys)
        return '\0';

    for (i = 0; i < map->button_key_count; i++)
    {
        if (map->button_keys[i].id == id)
            return map->button_keys[i].key;
    }

    return '\0';
}

static void ui_browser_shell_result_clear(
    ui_browser_shell_command_result* result)
{
    if (!result)
        return;

    memset(result, 0, sizeof(*result));
}

static void ui_browser_shell_result_target(
    ui_browser_shell_command_result* result, const app_ui_command* command)
{
    if (!result || !command)
        return;

    result->role = command->target.role;
    result->action = command->target.action;
    result->widget_id = command->target.widget_id;
    result->payload0 = command->target.payload0;
    result->payload1 = command->target.payload1;
}

bool ui_browser_shell_translate_command(const app_ui_command* command,
    const ui_browser_shell_command_map* map,
    ui_browser_shell_command_result* out_result)
{
    ui_browser_shell_command_map default_map;
    ui_browser_shell_command_result result;
    const app_ui_widget_ref* target;

    ui_browser_shell_result_clear(&result);
    if (out_result)
        ui_browser_shell_result_clear(out_result);
    if (!command)
        return false;

    if (!map)
    {
        ui_browser_shell_command_map_init(&default_map);
        map = &default_map;
    }

    target = &command->target;
    result.handled = true;
    ui_browser_shell_result_target(&result, command);

    if (command->kind == APP_UI_COMMAND_KIND_CANCEL
        || target->action == APP_UI_WIDGET_ACTION_CANCEL)
    {
        result.cancel = true;
        result.key = map->cancel_key;
        if (out_result)
            *out_result = result;
        return true;
    }

    if (command->kind == APP_UI_COMMAND_KIND_SCROLL
        || target->role == APP_UI_WIDGET_ROLE_SCROLL_REGION)
    {
        result.key = ui_browser_shell_scroll_command_key(command,
            &map->scroll_keys);
        if (out_result)
            *out_result = result;
        return true;
    }

    if (target->role == APP_UI_WIDGET_ROLE_TAB)
    {
        result.focus_only = (command->kind == APP_UI_COMMAND_KIND_FOCUS);
        if (out_result)
            *out_result = result;
        return true;
    }

    if (target->role == APP_UI_WIDGET_ROLE_LIST_ITEM)
    {
        result.focus_only = (command->kind == APP_UI_COMMAND_KIND_FOCUS);
        result.inspect = command->kind == APP_UI_COMMAND_KIND_INSPECT
            || command->kind == APP_UI_COMMAND_KIND_CONTEXT
            || target->action == APP_UI_WIDGET_ACTION_INSPECT;
        if (!result.focus_only)
            result.key = result.inspect ? map->row_inspect_key
                                        : map->row_activate_key;
        if (out_result)
            *out_result = result;
        return true;
    }

    if (target->role == APP_UI_WIDGET_ROLE_BUTTON)
    {
        result.focus_only = (command->kind == APP_UI_COMMAND_KIND_FOCUS);
        if (!result.focus_only)
        {
            result.key = ui_browser_shell_lookup_button_key(map,
                target->widget_id);
            if (!result.key && target->action_key)
                result.key = (char)(target->action_key & 0xFF);
        }
        if (out_result)
            *out_result = result;
        return true;
    }

    return false;
}

int ui_browser_shell_wait_key(const ui_browser_shell_command_map* map,
    u16b ignored_flags, ui_browser_shell_command_result* out_result)
{
    ui_information_scene_event event;
    ui_browser_shell_command_result result;

    if (out_result)
        ui_browser_shell_result_clear(out_result);

    if (!ui_information_scene_wait_event(&event, ignored_flags))
        return ESCAPE;

    if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
        return event.key;

    if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND
        && ui_browser_shell_translate_command(&event.command, map,
            &result))
    {
        if (out_result)
            *out_result = result;
        return result.key;
    }

    return '\0';
}

int ui_browser_shell_wait_key_with_wait_reason(
    const ui_browser_shell_command_map* map, u16b ignored_flags,
    u16b wait_reason, bool hidden_cursor,
    ui_browser_shell_command_result* out_result)
{
    app_session* session = app_session_current();
    app_wait_scope wait_scope;
    bool pushed_wait = false;
    bool saved_hide_cursor = inkey_cursor_hidden();
    int key;

    if (session && wait_reason != APP_WAIT_REASON_NONE)
    {
        app_session_push_wait_scope(session, &wait_scope, wait_reason, 0, 0);
        pushed_wait = true;
    }

    if (hidden_cursor)
        inkey_set_cursor_hidden(true);

    key = ui_browser_shell_wait_key(map, ignored_flags, out_result);

    if (hidden_cursor)
        inkey_set_cursor_hidden(saved_hide_cursor);
    if (pushed_wait)
        app_session_pop_wait_scope(session, &wait_scope);

    return key;
}

void ui_browser_shell_clamp_cursor(int* cursor, int* top, int count,
    int window_rows)
{
    if (!cursor)
        return;

    if (count <= 0)
    {
        *cursor = 0;
        if (top)
            *top = 0;
        return;
    }

    if (*cursor < 0)
        *cursor = 0;
    if (*cursor >= count)
        *cursor = count - 1;

    if (!top)
        return;

    if (window_rows <= 0)
        window_rows = 1;
    if (*top > *cursor)
        *top = *cursor;
    if (*cursor >= *top + window_rows)
        *top = *cursor - window_rows + 1;
    if (*top < 0)
        *top = 0;
}

bool ui_browser_shell_apply_vertical_key(int ch, int* cursor, int count,
    int page_rows)
{
    int d;
    int next;

    if (!cursor || count <= 0)
        return false;

    d = target_dir((char)ch);
    if (!d || !ddy[d])
        return false;

    next = *cursor;
    if (ddx[d])
        next += ddy[d] * ((page_rows > 0) ? page_rows : 1);
    else
        next += ddy[d];

    if (next < 0)
        next = 0;
    if (next >= count)
        next = count - 1;

    if (next == *cursor)
        return true;

    *cursor = next;
    return true;
}

bool ui_browser_shell_apply_row_focus(
    const ui_browser_shell_command_result* result, int* cursor, int count,
    int* top, int window_rows)
{
    if (!result || !cursor || count <= 0)
        return false;
    if (!result->handled || result->role != APP_UI_WIDGET_ROLE_LIST_ITEM)
        return false;
    if (result->widget_id < 0 || result->widget_id >= count)
        return false;

    *cursor = result->widget_id;
    ui_browser_shell_clamp_cursor(cursor, top, count, window_rows);
    return true;
}

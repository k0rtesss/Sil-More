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

#ifndef INCLUDED_UI_BROWSER_SHELL_H
#define INCLUDED_UI_BROWSER_SHELL_H

#include "app/app-ui-command.h"
#include "ui/ui-semantic-scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_browser_shell_scene_config {
    u16b scene_flags;
    u16b layer;
    u16b style;
    u16b panel_flags;
    byte title_attr;
    byte subtitle_attr;
    byte accent_attr;
    u16b min_width_px;
    u16b width_cap_px;
    cptr title;
    cptr subtitle;
} ui_browser_shell_scene_config;

typedef struct ui_browser_shell_footer_action {
    s16b id;
    byte attr;
    bool enabled;
    cptr key;
    cptr label;
} ui_browser_shell_footer_action;

typedef struct ui_browser_shell_button_key {
    s16b id;
    char key;
} ui_browser_shell_button_key;

typedef struct ui_browser_shell_scroll_keys {
    char up_key;
    char down_key;
    char left_key;
    char right_key;
} ui_browser_shell_scroll_keys;

typedef struct ui_browser_shell_command_map {
    const ui_browser_shell_button_key* button_keys;
    size_t button_key_count;
    ui_browser_shell_scroll_keys scroll_keys;
    char cancel_key;
    char row_activate_key;
    char row_inspect_key;
    bool action_key_fallback;
} ui_browser_shell_command_map;

typedef struct ui_browser_shell_command_result {
    bool handled;
    bool focus_only;
    bool cancel;
    bool inspect;
    u16b role;
    u16b action;
    s16b widget_id;
    char key;
} ui_browser_shell_command_result;

void ui_browser_shell_scene_config_init(ui_browser_shell_scene_config* config);
app_ui_panel* ui_browser_shell_begin(app_ui_scene* scene,
    const ui_browser_shell_scene_config* config);
app_ui_panel* ui_browser_shell_begin_browser(app_ui_scene* scene,
    byte title_attr, cptr title, byte subtitle_attr, cptr subtitle,
    byte accent_attr, u16b panel_flags, u16b min_width_px,
    u16b width_cap_px);
bool ui_browser_shell_add_footer_actions(app_ui_panel* panel,
    const ui_browser_shell_footer_action* actions, size_t action_count);
bool ui_browser_shell_add_tab(app_ui_panel* panel, s16b id, byte attr,
    bool active, cptr label, s16b action_key, cptr tooltip);
void ui_browser_shell_prompt_label(int binding, const char* fallback,
    char* buf, size_t buflen);

ui_browser_shell_scroll_keys ui_browser_shell_default_scroll_keys(void);
void ui_browser_shell_command_map_init(ui_browser_shell_command_map* map);
char ui_browser_shell_scroll_command_key(const app_ui_command* command,
    const ui_browser_shell_scroll_keys* keys);
char ui_browser_shell_direction_command_key(const app_ui_command* command,
    const ui_browser_shell_scroll_keys* keys);
bool ui_browser_shell_translate_command(const app_ui_command* command,
    const ui_browser_shell_command_map* map,
    ui_browser_shell_command_result* out_result);

void ui_browser_shell_clamp_cursor(int* cursor, int* top, int count,
    int window_rows);
bool ui_browser_shell_apply_vertical_key(int ch, int* cursor, int count,
    int page_rows);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_UI_BROWSER_SHELL_H */

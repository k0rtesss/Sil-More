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

#ifndef INCLUDED_PLATFORM_CONFIG_H
#define INCLUDED_PLATFORM_CONFIG_H

#include "h-basic.h"
#include "pane-config.h"

void platform_config_info(char* buf, size_t size);
bool save_pane_config_to_json(void);
cptr platform_config_path(void);
void platform_load_app_options(void);
bool platform_intro_should_force_flame(void);
void platform_intro_mark_seen(void);
bool option_is_app_persistent(int opt);
int platform_main_view_scale(void);
void platform_set_main_view_scale(int value);
int platform_overlay_density(void);
void platform_set_overlay_density(int value);
int platform_min_terminal_mode(void);
void platform_set_min_terminal_mode(int value);
int platform_aux_view_font_size(void);
int platform_effective_aux_view_font_size(void);
void platform_set_aux_view_font_size(int value);
int platform_menu_panel_font_size(void);
int platform_effective_menu_panel_font_size(void);
void platform_set_menu_panel_font_size(int value);
int platform_plain_menu_font_size(void);
int platform_effective_plain_menu_font_size(void);
void platform_set_plain_menu_font_size(int value);
int platform_browser_menu_font_size(void);
int platform_effective_browser_menu_font_size(void);
void platform_set_browser_menu_font_size(int value);
int platform_character_sheet_font_size(void);
int platform_effective_character_sheet_font_size(void);
void platform_set_character_sheet_font_size(int value);
int platform_margin(void);
void platform_set_margin(int value);
int platform_current_min_terminal_cols(void);
int platform_current_min_terminal_rows(void);
bool platform_fullscreen(void);
void platform_set_fullscreen(bool value);
bool platform_tiles(void);
void platform_set_tiles(bool value);
bool platform_use_unsafe_area(void);
void platform_set_use_unsafe_area(bool value);
int get_pane_config_count(void);
bool platform_enable_right_panes(void);
void platform_set_enable_right_panes(bool value);
bool platform_enable_bottom_panes(void);
void platform_set_enable_bottom_panes(bool value);
bool platform_show_pane_borders(void);
void platform_set_show_pane_borders(bool value);
bool platform_hide_left_panel(void);
void platform_set_hide_left_panel(bool value);
int platform_pane_type(int index);
int platform_pane_where(int index);
void platform_set_pane_where(int index, int where);
bool platform_pane_enabled(int index);
int platform_pane_rows(int index);
int platform_pane_cols(int index);
int platform_pane_font_size(int index);
int platform_pane_effective_font_size(int index);
int platform_pane_current_rows(int index);
int platform_pane_current_cols(int index);
void platform_set_pane_rows(int index, int rows);
void platform_set_pane_cols(int index, int cols);
void platform_set_pane_font_size(int index, int font_size);
void platform_set_pane_enabled(int index, bool enabled);
int platform_intro_style(void);
void platform_set_intro_style(int style);
int platform_max_scale(void);
void platform_apply_config(void);
void platform_reset_layout_defaults(void);

#endif /* INCLUDED_PLATFORM_CONFIG_H */

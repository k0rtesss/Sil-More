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

#ifndef INCLUDED_SDL_SCENE_MENU_H
#define INCLUDED_SDL_SCENE_MENU_H

#include "angband.h"

#include "../sdl-main-internal.h"

typedef enum sdl_menu_hit_target_kind {
    SDL_MENU_HIT_TARGET_NONE = 0,
    SDL_MENU_HIT_TARGET_ROW = 1,
    SDL_MENU_HIT_TARGET_FOOTER_ACTION = 2,
    SDL_MENU_HIT_TARGET_TAB = 3,
    SDL_MENU_HIT_TARGET_PANEL = 4
} sdl_menu_hit_target_kind;

typedef struct sdl_menu_hit_target {
    SDL_FRect rect;
    u16b scene_kind;
    u16b panel_index;
    u16b panel_layer;
    u16b panel_style;
    u16b focus_area;
    u16b state_flags;
    s16b focus_order;
    s16b owner_id;
    s32b payload0;
    s32b payload1;
    u16b kind;
    s16b id;
    s16b action_key;
    u16b role;
    u16b action;
    u16b flags;
    char label[APP_UI_LABEL_MAX];
    char tooltip[APP_UI_TOOLTIP_MAX];
} sdl_menu_hit_target;

SDL_Color sdl_menu_color_alpha(byte attr, byte alpha);
SDL_Color sdl_menu_color(byte attr);
void sdl_menu_hit_reset(int origin_x, int origin_y);
void sdl_menu_hit_set_scene(u16b scene_kind);
void sdl_menu_hit_begin_panel(u16b panel_index,
    const app_ui_panel* panel);
void sdl_menu_hit_end_panel(void);
bool sdl_menu_hit_register(u16b kind, s16b id, s16b action_key, u16b role,
    u16b action, u16b flags, const SDL_FRect* canvas_rect, cptr label,
    cptr tooltip);
bool sdl_menu_hit_register_ex(u16b kind, s16b id, s16b action_key,
    u16b role, u16b action, u16b flags, u16b state_flags, s16b owner_id,
    s32b payload0, s32b payload1, const SDL_FRect* canvas_rect, cptr label,
    cptr tooltip);
const sdl_menu_hit_target* sdl_menu_hit_test(float window_x, float window_y);
bool sdl_menu_pointer_handle_event(const SDL_Event* ev);
void sdl_menu_fill_rect(const SDL_FRect* rect, SDL_Color color);
void sdl_menu_draw_rect(const SDL_FRect* rect, SDL_Color color);
void sdl_menu_draw_tile(byte attr, byte ch, const SDL_FRect* dst);
void sdl_menu_draw_view_glyph(const sdl_view* view, const SDL_FRect* dst,
    SDL_Color color, char ch);
int sdl_menu_scale_px(float logical_value);
int sdl_menu_font_size_logical(const app_ui_panel* panel);
int sdl_menu_measure_text(TTF_Font* font, cptr text);
int sdl_menu_icon_slot_px(TTF_Font* font, int line_h);
void sdl_menu_render_icon(TTF_Font* font, float x_px, float y_px,
    int icon_slot_w, int line_h, byte icon_attr, char icon_char);
void sdl_menu_render_text(TTF_Font* font, float x_px, float y_px, int line_h,
    SDL_Color color, cptr text);
bool sdl_menu_document_cell_is_raw(byte attr, char ch, byte terrain_attr,
    char terrain_char);
void sdl_menu_draw_misc_icon(const SDL_FRect* dst, int icon);
int sdl_menu_measure_rich_text_height(TTF_Font* mono_font,
    TTF_Font* story_font, int line_h, int line_gap, int paragraph_gap,
    int width_px, const app_ui_scene* scene, const app_ui_panel* panel);
int sdl_menu_render_rich_text(const app_ui_scene* scene,
    const app_ui_panel* panel, TTF_Font* mono_font, TTF_Font* story_font,
    const SDL_Rect* clip_rect, int line_h, int line_gap, int paragraph_gap,
    int start_y);
bool sdl_menu_render_panel_internal(const sdl_view* main_view, int canvas_w,
    int canvas_h, const app_ui_scene* scene, const app_ui_panel* ui_panel);
bool sdl_menu_render_browser_panel(const sdl_view* main_view, int canvas_w,
    int canvas_h, const app_ui_scene* scene, const app_ui_panel* ui_panel);
bool sdl_menu_render_status_rail_panel(const sdl_view* main_view, int canvas_w,
    int canvas_h, const app_ui_panel* panel);
bool sdl_menu_render_overlay_rail_panel(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_panel* panel);
bool sdl_menu_render_strip_panel(const sdl_view* main_view, int canvas_w,
    int canvas_h, const app_ui_panel* panel);
bool sdl_menu_render_welcome_panel(const sdl_view* main_view, int canvas_w,
    int canvas_h, const app_ui_panel* panel);
bool sdl_menu_render_minimap_panel(const sdl_view* main_view, int canvas_w,
    int canvas_h, const app_ui_scene* scene, const app_ui_panel* panel);
bool sdl_menu_render_character_sheet_panel(const sdl_view* main_view,
    int canvas_w, int canvas_h, const app_ui_scene* scene,
    const app_ui_panel* panel);

#endif /* INCLUDED_SDL_SCENE_MENU_H */

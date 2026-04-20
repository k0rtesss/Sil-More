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

SDL_Color sdl_menu_color_alpha(byte attr, byte alpha);
SDL_Color sdl_menu_color(byte attr);
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

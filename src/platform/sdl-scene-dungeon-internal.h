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

#ifndef INCLUDED_SDL_SCENE_DUNGEON_INTERNAL_H
#define INCLUDED_SDL_SCENE_DUNGEON_INTERNAL_H

#include <SDL3_ttf/SDL_ttf.h>

typedef struct sdl_scene_layout {
    int canvas_w;
    int canvas_h;
    int top_strip_h_px;
    int bottom_strip_h_px;
    int left_panel_w_px;
    int map_origin_x_px;
    int map_origin_y_px;
    int map_width_px;
    int map_height_px;
    int content_bottom_px;
} sdl_scene_layout;

typedef struct sdl_scene_strip_metrics {
    TTF_Font* font;
    int line_h;
    int row_count;
    int strip_h;
    int left_inset_px;
} sdl_scene_strip_metrics;

typedef struct sdl_scene_status_rail_metrics {
    TTF_Font* mono_font;
    TTF_Font* story_font;
    int line_h;
    int icon_slot_w;
    int gap_px;
    int left_inset_px;
    int panel_w_px;
    int row_visible;
} sdl_scene_status_rail_metrics;

enum {
    SDL_SCENE_NARRATIVE_BANNER_POP_IN_MS = 220u,
    SDL_SCENE_NARRATIVE_BANNER_POP_OUT_MS = 260u
};

#endif /* INCLUDED_SDL_SCENE_DUNGEON_INTERNAL_H */

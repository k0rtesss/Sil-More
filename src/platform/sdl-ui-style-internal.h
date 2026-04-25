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

#ifndef INCLUDED_SDL_UI_STYLE_INTERNAL_H
#define INCLUDED_SDL_UI_STYLE_INTERNAL_H

#include <SDL3/SDL.h>

typedef struct sdl_ui_style {
    const char* name;
    const char* material;
    const char* backdrop_slot;
    const char* header_slot;
    SDL_Color canvas_fill;
    SDL_Color panel_fill;
    SDL_Color panel_fill_alt;
    SDL_Color panel_border;
    SDL_Color panel_border_soft;
    SDL_Color divider;
    SDL_Color shadow;
    SDL_Color focus_ring;
    SDL_Color selected_fill;
    SDL_Color pressed_fill;
    SDL_Color disabled_fill;
    SDL_Color text;
    SDL_Color text_muted;
    SDL_Color text_subtle;
    SDL_Color text_disabled;
    SDL_Color accent;
    SDL_Color accent_soft;
    SDL_Color accent_dim;
    SDL_Color success;
    SDL_Color warning;
    SDL_Color danger;
    SDL_Color magic;
    SDL_Color cool;
    float margin_x;
    float margin_y;
    float pad_x;
    float pad_y;
    float line_gap;
    float section_gap;
    float item_gap;
    float column_gap;
    float pill_gap;
    float pill_pad_x;
    float pill_pad_y;
    float row_pad_y;
    float border_px;
    float focus_px;
    float shadow_px;
} sdl_ui_style;

#endif /* INCLUDED_SDL_UI_STYLE_INTERNAL_H */

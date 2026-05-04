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

#ifndef INCLUDED_SDL_TOUCH_CONTROLS_H
#define INCLUDED_SDL_TOUCH_CONTROLS_H

#include "angband.h"

void sdl_touch_controls_reset_input_state(void);
int sdl_touch_controls_pending_timeout_ms(Uint64 now_ns);
bool sdl_touch_controls_flush_pending_press(Uint64 now_ns);
bool sdl_touch_controls_top_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_touch_controls_top_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id);
bool sdl_touch_controls_top_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id);
bool sdl_touch_controls_zone_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_touch_controls_zone_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id);
bool sdl_touch_controls_zone_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id);
void sdl_touch_controls_handle_pointer_canceled(SDL_FingerID finger_id);
void sdl_touch_controls_render(int canvas_w, int canvas_h);
void sdl_touch_controls_set_top_panel_open(bool open);
void sdl_touch_controls_cancel_top_panel_press(void);
void sdl_touch_controls_cancel_zone_press(void);

void sdl_touch_pane_draw_button_text(const SDL_FRect* rect, const char* name,
    const char* symbol, SDL_Color color);
void sdl_touch_pane_send_binding(int binding, bool second_panel,
    bool long_press);
float sdl_touch_swipe_threshold_px(void);
bool sdl_touch_swipe_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_touch_swipe_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id);

#endif /* INCLUDED_SDL_TOUCH_CONTROLS_H */

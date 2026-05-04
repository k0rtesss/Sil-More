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

#ifndef INCLUDED_SDL_ROUND_MOVEMENT_H
#define INCLUDED_SDL_ROUND_MOVEMENT_H

bool sdl_round_movement_handle_event(const SDL_Event* ev);
void sdl_round_movement_render(const sdl_view* main_view,
    const app_dungeon_snapshot* snapshot);
void sdl_round_movement_reset_input_state(void);

#endif /* INCLUDED_SDL_ROUND_MOVEMENT_H */

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

#pragma once

#include "pane-config.h"
#include "SDL3/SDL_rect.h"

/* Instance of a pane itself. */
struct pane {
    SDL_Rect rect;
    int index;
};

void place_panes(const struct pane_config* config, int count, SDL_Rect* panes,
    const SDL_Rect* window, const int* cell_widths, const int* cell_heights,
    int margin);

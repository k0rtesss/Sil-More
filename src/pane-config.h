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

#include "h-basic.h"
#include <stdint.h>

/* Available pane types in the game. */
enum pane_type {
    PANE_MAIN = 0,
    PANE_INVENTORY = 1,
    PANE_WORN = 2,
    PANE_ROLLS = 3,
    PANE_INFO = 4,
    PANE_CHARACTER = 5,
    PANE_LOG = 6,
    PANE_MONSTERS = 7,
    PANE_TOUCH = 8,
    PANE_MAX = 9,
};

/* Where the pane is placed. */
enum pane_placement {
    PLACE_BOTTOM = 1u << 0,
    PLACE_RIGHT = 1u << 1,
    PLACE_LEFT = 1u << 2,
    PLACE_DOUBLE_LEFT = 1u << 3,
    PLACE_DOUBLE_RIGHT = 1u << 4,
    PLACE_DOUBLE_BOTTOM = 1u << 5,
};

struct rect {
    union {
        struct {
            int rows;
            int cols;
        };
        int size[2];
    };
};

/* Specifications of a pane. */
struct pane_specs {
    uint32_t placement;
    struct rect min_rect;
};

/* Configuration for a pane. */
struct pane_config {
    enum pane_type pane;
    enum pane_placement where;
    bool enabled;
    struct rect rect;
    int font_size;
    float ratio;
};

bool pane_placement_is_side(enum pane_placement where);
bool pane_placement_is_bottom(enum pane_placement where);
bool pane_type_allows_placement(enum pane_type type, enum pane_placement where);
int pane_primary_min_cells(enum pane_type type, enum pane_placement where);
int pane_secondary_min_cells(enum pane_type type, enum pane_placement where);
enum pane_placement pane_first_allowed_placement(enum pane_type type);
enum pane_placement pane_next_allowed_placement(enum pane_type type,
    enum pane_placement current, int delta);
const char* pane_placement_name(enum pane_placement where);

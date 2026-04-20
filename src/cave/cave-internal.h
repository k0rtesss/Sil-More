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

#ifndef INCLUDED_CAVE_INTERNAL_H
#define INCLUDED_CAVE_INTERNAL_H

#include "../angband.h"
#include "cave.h"

/*
 * Wave 0 staging internal header for future cave module extractions.
 *
 * Cave split work should share declarations here rather than creating new
 * dependencies on src/externs.h.
 */

/*
 * Lane E cave-local helpers used to keep src/cave/cave.c focused on
 * cave-state orchestration while visuals own style-derived rendering details.
 */
byte cave_visuals_get_depth_color(int depth);
void cave_visuals_set_feat_with_color(int y, int x, int feat, int color);

#endif /* INCLUDED_CAVE_INTERNAL_H */

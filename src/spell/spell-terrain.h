/* File: spell/spell-terrain.h */
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

/*
 * Terrain manipulation, area destruction, and lighting effects.
 */

#ifndef INCLUDED_SPELL_TERRAIN_H
#define INCLUDED_SPELL_TERRAIN_H

#include "../h-basic.h"

void destroy_area(int y1, int x1, int r, bool full);
void earthquake(int cy, int cx, int pit_y, int pit_x, int r, int who);
bool close_chasm(int y, int x, int power);
bool close_chasms(int power);
void light_room(int y1, int x1);
void darken_room(int y1, int x1);
bool light_area(int dd, int ds, int rad);
bool darken_area(int dd, int ds, int rad);

#endif /* INCLUDED_SPELL_TERRAIN_H */

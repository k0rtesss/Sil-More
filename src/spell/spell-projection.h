/* File: spell/spell-projection.h */
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
 * Core spell projection engine: bolts, beams, balls, arcs, and area effects.
 */

#ifndef INCLUDED_SPELL_PROJECTION_H
#define INCLUDED_SPELL_PROJECTION_H

#include "../h-basic.h"

bool project(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg, int degrees, bool uniform);

bool lock_door(int y, int x, int power);
bool lock_doors_radius(int y0, int x0, int radius, int power);

bool project_bolt(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg);
bool project_beam(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg);
bool project_ball(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg, bool uniform);
bool explosion(int who, int rad, int y0, int x0, int dd, int ds, int dif,
    int typ);
bool project_arc(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg, int degrees);

bool project_los_not_player(int y1, int x1, int dd, int ds, int dif, int typ);
bool project_los(int typ, int dd, int ds, int dif, bool silent);
bool project_los_grids(int typ, int dd, int ds, int dif);

void clear_temp_array(void);
void cave_temp_mark(int y, int x, bool room);
void spread_cave_temp(int y1, int x1, int range, bool room);

bool fire_bolt_beam_special(int typ, int dir, int dd, int ds, int dif,
    int rad, u32b flg);
bool fire_ball(int typ, int dir, int dd, int ds, int dif, int rad);
bool fire_arc(int typ, int dir, int dd, int ds, int dif, int rad,
    int degrees);
bool fire_bolt(int typ, int dir, int dd, int ds, int dif);
bool fire_beam(int typ, int dir, int dd, int ds, int dif);
bool fire_bolt_or_beam(int prob, int typ, int dir, int dd, int ds, int dif);

bool light_line(int dir);
bool destroy_door(int dir);
bool disarm_trap(int dir);

#endif /* INCLUDED_SPELL_PROJECTION_H */

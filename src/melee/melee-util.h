/* File: melee/melee-util.h */
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
 * Grid/monster utility functions for movement validation and scent.
 * Split from melee2.c for better code organization.
 */

#ifndef INCLUDED_MELEE_UTIL_H
#define INCLUDED_MELEE_UTIL_H

#include "../h-basic.h"

typedef struct monster_type monster_type;
typedef struct monster_race monster_race;

extern int get_scent(int y, int x);
extern bool cave_exist_mon(
    monster_race* r_ptr, int y, int x, bool occupied_ok, bool can_dig);
extern int cave_passable_mon(monster_type* m_ptr, int y, int x, bool* bash);
extern bool attacker_at(int y, int x);
extern int adj_mon_count(int y, int x);
extern void tell_allies(int y, int x, u32b flag);

#endif /* INCLUDED_MELEE_UTIL_H */

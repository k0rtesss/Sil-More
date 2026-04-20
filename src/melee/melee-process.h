/* File: melee/melee-process.h */
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
 * Monster turn processing, ranged attack selection, morale, and perception.
 * Split from melee2.c for better code organization.
 */

#ifndef INCLUDED_MELEE_PROCESS_H
#define INCLUDED_MELEE_PROCESS_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

#define TURN_RANGE 3

extern int challenge_check(monster_type* m_ptr);
extern void find_range(monster_type* m_ptr);
extern int get_chance_of_ranged_attack(monster_type* m_ptr);
extern void wander(monster_type* m_ptr);
extern void produce_cloud(monster_type* m_ptr);
extern int morale_from_friends(monster_type* m_ptr);
extern void calc_morale(monster_type* m_ptr);
extern void calc_stance(monster_type* m_ptr);
extern void process_monsters(s16b minimum_energy);
extern void monster_perception(
    bool player_centered, bool main_roll, int difficulty);

#endif /* INCLUDED_MELEE_PROCESS_H */

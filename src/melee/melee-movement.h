/* File: melee/melee-movement.h */
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
 * Monster movement AI and movement execution.
 * Split from melee2.c for better code organization.
 */

#ifndef INCLUDED_MELEE_MOVEMENT_H
#define INCLUDED_MELEE_MOVEMENT_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

extern bool monster_can_smell(monster_type* m_ptr);
extern bool get_move_wander(monster_type* m_ptr, int* ty, int* tx);
extern bool get_move(
    monster_type* m_ptr, int* ty, int* tx, bool* fear, bool must_use_target);
extern bool make_move(
    monster_type* m_ptr, int* ty, int* tx, bool fear, bool* bash);
extern void process_move(monster_type* m_ptr, int ty, int tx, bool bash);
extern void warning_message(monster_type* m_ptr);
extern void monster_exchange_places(monster_type* m_ptr);
extern int calc_hesitance(monster_type* m_ptr);

#endif /* INCLUDED_MELEE_MOVEMENT_H */

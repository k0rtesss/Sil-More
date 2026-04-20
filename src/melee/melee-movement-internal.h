/* File: melee/melee-movement-internal.h */
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

#ifndef INCLUDED_MELEE_MOVEMENT_INTERNAL_H
#define INCLUDED_MELEE_MOVEMENT_INTERNAL_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

bool get_move_retreat(monster_type* m_ptr, int* ty, int* tx);
void get_move_advance(monster_type* m_ptr, int* ty, int* tx);
int calc_vulnerability(int fy, int fx);
bool get_route_to_target(monster_type* m_ptr, int* ty, int* tx);
bool melee_movement_push_aside(monster_type* m_ptr, monster_type* n_ptr);

#endif /* INCLUDED_MELEE_MOVEMENT_INTERNAL_H */

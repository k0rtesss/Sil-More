/* File: melee/melee-attack.h */
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
 * Monster attack mechanics, melee attacks, ranged attacks, and clouds.
 * Split from melee1.c for better code organization.
 */

#ifndef INCLUDED_MELEE_ATTACK_H
#define INCLUDED_MELEE_ATTACK_H

#include "../h-basic.h"

typedef struct monster_type monster_type;
typedef struct object_type object_type;

extern bool blocking_bonus_active(void);
extern int elem_bonus(int effect);
extern int protection_roll(int typ, bool melee);
extern int p_min(int typ, bool melee);
extern int p_max(int typ, bool melee);
extern int dodging_bonus(void);
extern bool monster_charge(monster_type* m_ptr);
extern bool is_traitor_item(int item_slot);
extern void do_betrayal_ring_amulet(void);
extern void do_betrayal_helm_crown(void);
extern bool make_attack_normal(monster_type* m_ptr);
extern int get_sides(int attack);
extern void mon_cloud(int m_idx, int typ, int dd, int ds, int dif, int rad);
extern void shriek(monster_type* m_ptr);
extern bool make_attack_ranged(monster_type* m_ptr, int attack);
extern void cloud_surround(int r_idx, int* typ, int* dd, int* ds, int* rad);

#endif /* INCLUDED_MELEE_ATTACK_H */

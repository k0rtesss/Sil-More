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

#ifndef INCLUDED_PLAYER_WEAPON_STATS_H
#define INCLUDED_PLAYER_WEAPON_STATS_H

#include "h-basic.h"

typedef struct object_type object_type;

byte total_mdd(const object_type* o_ptr);
byte strength_modified_ds(const object_type* o_ptr, int str_adjustment);
byte total_mds(const object_type* o_ptr, int str_adjustment);
bool two_handed_melee(void);
int hand_and_a_half_bonus(const object_type* o_ptr);
int bow_bonus(const object_type* o_ptr);
int axe_bonus(const object_type* o_ptr);
int polearm_bonus(const object_type* o_ptr);
byte total_ads(const object_type* j_ptr);

#endif /* INCLUDED_PLAYER_WEAPON_STATS_H */

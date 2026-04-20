/* File: monster/monster-death.h */
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
 * Transitional public header for monster damage/death helpers used outside the
 * monster core implementation.
 */

#ifndef INCLUDED_MONSTER_MONSTER_DEATH_H
#define INCLUDED_MONSTER_MONSTER_DEATH_H

#include "h-basic.h"

void maybe_update_morgoth_state_from_hp(monster_type* m_ptr);
void anger_morgoth(int level);
void break_truce(bool obvious);
void create_chosen_artefact(byte name1, int y, int x, bool identify);
int drop_loot(monster_type* m_ptr);
void monster_death(int m_idx);
bool mon_take_hit(int m_idx, int dam, cptr note, int who);
bool similar_monsters(int m1y, int m1x, int m2y, int m2x);

#endif /* INCLUDED_MONSTER_MONSTER_DEATH_H */

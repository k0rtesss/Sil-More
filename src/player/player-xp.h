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

#ifndef INCLUDED_PLAYER_XP_H
#define INCLUDED_PLAYER_XP_H

#include "h-basic.h"

typedef struct monster_race monster_race;
typedef struct monster_type monster_type;

void check_experience(void);
s32b adjusted_mon_exp(const monster_race* r_ptr, bool kill);
void gain_exp(s32b amount);
void gain_knowledge_points(s32b amount, cptr reason);
void lose_exp(s32b amount);
void falling_damage(bool stun);
void scare_onlooking_friends(const monster_type* m_ptr, int amount);

#endif /* INCLUDED_PLAYER_XP_H */

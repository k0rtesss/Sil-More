/* File: spell/spell-monster.h */
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
 * Monster control spells and monster-facing song effects.
 */

#ifndef INCLUDED_SPELL_MONSTER_H
#define INCLUDED_SPELL_MONSTER_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

bool slow_monsters(int power);
bool sleep_monsters(int power);
bool destroy_traps(int power);
bool open_doors(int power);
bool lock_doors(int power);
void wake_all_monsters(int who);
bool make_aggressive(void);
bool banishment(void);
bool mass_banishment(void);

void song_of_binding(monster_type* m_ptr);
void song_of_piercing(monster_type* m_ptr);
void song_of_oaths(monster_type* m_ptr);
void hatch_spider(monster_type* m_ptr);

#endif /* INCLUDED_SPELL_MONSTER_H */

/* File: spell/spell-teleport.h */
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
 * Teleportation spells and effects.
 */

#ifndef INCLUDED_SPELL_TELEPORT_H
#define INCLUDED_SPELL_TELEPORT_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

void teleport_away(int m_idx, int dis);
void teleport_player(int dis);
void teleport_player_to(int ny, int nx);
void teleport_towards(int oy, int ox, int ny, int nx);
void teleport_player_level(void);
void stun_monster(monster_type* m_ptr, int stun);

#endif /* INCLUDED_SPELL_TELEPORT_H */

/* File: cmd-monster.h */
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
 * Transitional public header for the monster command split.
 */

#ifndef INCLUDED_CMD_MONSTER_H
#define INCLUDED_CMD_MONSTER_H

#include "h-basic.h"

void new_wandering_flow(monster_type* m_ptr, int y, int x);
void new_wandering_destination(
    monster_type* m_ptr, monster_type* leader_ptr);
void drop_iron_crown(monster_type* m_ptr, const char* msg);
int success_chance(int sides, int skill, int difficulty);

#endif /* INCLUDED_CMD_MONSTER_H */

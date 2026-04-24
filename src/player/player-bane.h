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

#ifndef INCLUDED_PLAYER_BANE_H
#define INCLUDED_PLAYER_BANE_H

#include "h-basic.h"

typedef struct monster_type monster_type;

enum
{
    PLAYER_BANE_TYPES = 13
};

extern char* bane_name[];

int bane_type_killed(int bane_type);
int elf_bane_bonus(monster_type* m_ptr);
int dwarf_bane_bonus(monster_type* m_ptr);
int bane_bonus(monster_type* m_ptr);
int bane_bonus_for_type(int bane_type_idx);
int artifact_bane_bonus(monster_type* m_ptr);
int spider_bane_bonus(void);
int artifact_spider_bane_bonus(void);
int unique_bane_bonus(monster_type* m_ptr);
int unique_bane_type_killed(void);

#endif /* INCLUDED_PLAYER_BANE_H */

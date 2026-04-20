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

#ifndef INCLUDED_OBJECT_OBJECT_STATE_H
#define INCLUDED_OBJECT_OBJECT_STATE_H

#include "h-basic.h"

extern s16b o_max;
extern s16b o_cnt;
extern object_type* o_list;
extern object_type* inventory;

extern s16b alloc_kind_size;
extern alloc_entry* alloc_kind_table;
extern s16b alloc_ego_size;
extern alloc_entry* alloc_ego_table;
extern s16b alloc_race_size;
extern alloc_entry* alloc_race_table;

extern s16b object_level;
extern byte object_generation_mode;

#endif /* INCLUDED_OBJECT_OBJECT_STATE_H */

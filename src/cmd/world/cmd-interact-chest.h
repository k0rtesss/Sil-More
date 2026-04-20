/* File: cmd-interact-chest.h */
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

#ifndef INCLUDED_CMD_INTERACT_CHEST_H
#define INCLUDED_CMD_INTERACT_CHEST_H

#include "h-basic.h"

typedef struct object_type object_type;

void cmd_interact_search_skeleton(int y, int x, s16b o_idx);
bool cmd_interact_open_chest(int y, int x, s16b o_idx);
bool cmd_interact_disarm_chest(int y, int x, s16b o_idx);
void chest_release_contents(object_type* o_ptr, int y, int x, int destroy_typ);

#endif /* INCLUDED_CMD_INTERACT_CHEST_H */

/* File: cmd-interact-chest.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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

#include "angband.h"
#include "cmd-interact-chest-internal.h"

void cmd_interact_search_skeleton(int y, int x, s16b o_idx)
{
    cmd_interact_search_skeleton_impl(y, x, o_idx);
}

bool cmd_interact_open_chest(int y, int x, s16b o_idx)
{
    return cmd_interact_open_chest_impl(y, x, o_idx);
}

bool cmd_interact_disarm_chest(int y, int x, s16b o_idx)
{
    return cmd_interact_disarm_chest_impl(y, x, o_idx);
}
/* File: cmd-interact-chest.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
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
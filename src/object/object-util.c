/* File: object-util.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "item_set.h"
#include "object/object-flags.h"
#include "object/object-util.h"

int get_paired_artefact(int art_idx)
{
    return item_sets_get_paired_artefact(art_idx);
}

bool player_can_treat_as_throwing_flags(const object_type* o_ptr, u32b f3)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (f3 & TR3_THROWING)
        return true;

    return false;
}

bool weapon_is_impale_eligible(const object_type* o_ptr)
{
    u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    if (f3 & TR3_POLEARM)
        return true;

    if ((o_ptr->tval == TV_SWORD)
        && (k_info[o_ptr->k_idx].flags3 & TR3_TWO_HANDED))
    {
        return true;
    }

    return false;
}

bool player_can_treat_as_throwing(const object_type* o_ptr)
{
    u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    return player_can_treat_as_throwing_flags(o_ptr, f3);
}

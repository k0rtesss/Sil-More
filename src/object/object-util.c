/* File: object-util.c */
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
#include "item_set.h"
#include "log/log.h"
#include "object/object-flags.h"
#include "object/object-util.h"
#include <limits.h>

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

bool object_is_searched_skeleton(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && (o_ptr->tval == TV_SKELETON)
        && (o_ptr->pval <= 0);
}

bool player_can_treat_as_throwing(const object_type* o_ptr)
{
    u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    return player_can_treat_as_throwing_flags(o_ptr, f3);
}

static bool ego_affix_has_only_flag_effects(const ego_item_type* e_ptr)
{
    if (!e_ptr)
        return false;

    if (e_ptr->max_att || e_ptr->to_dd || e_ptr->to_ds || e_ptr->max_evn
        || e_ptr->to_pd || e_ptr->to_ps || e_ptr->cost <= 0
        || e_ptr->level || e_ptr->rarity)
    {
        return false;
    }

    if (e_ptr->abilities)
        return false;

    for (int i = 0; i < A_MAX; i++)
    {
        if (e_ptr->stat_bonus[i] != 0)
            return false;
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (e_ptr->skill_bonus_set[i] || e_ptr->skill_bonus_min[i] != 0
            || e_ptr->skill_bonus[i] != 0)
        {
            return false;
        }
    }

    return true;
}

static bool object_is_fire_breakable_weapon(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_POLEARM);
}

static s32b pack_fire_broken_weapon_payload(s16b att, byte dd, byte ds)
{
    u32b payload = (u32b)(u16b)att;
    payload |= ((u32b)dd << 16);
    payload |= ((u32b)ds << 24);
    return (s32b)payload;
}

static void unpack_fire_broken_weapon_payload(s32b payload, s16b* att, byte* dd,
    byte* ds)
{
    u32b bits = (u32b)payload;

    if (att)
        *att = (s16b)(bits & 0xFFFFU);
    if (dd)
        *dd = (byte)((bits >> 16) & 0xFFU);
    if (ds)
        *ds = (byte)((bits >> 24) & 0xFFU);
}

bool object_is_fire_broken(const object_type* o_ptr)
{
    return object_runtime_state(o_ptr) == OBJECT_RUNTIME_STATE_FIRE_BROKEN;
}

bool object_break_shafted_weapon_by_fire(object_type* o_ptr)
{
    if (!object_is_fire_breakable_weapon(o_ptr))
        return false;

    if (object_is_fire_broken(o_ptr))
        return true;

    object_set_runtime_payload(
        o_ptr, pack_fire_broken_weapon_payload(o_ptr->att, o_ptr->dd, o_ptr->ds));
    object_set_runtime_state(o_ptr, OBJECT_RUNTIME_STATE_FIRE_BROKEN);

    if (o_ptr->att > SHRT_MIN)
        o_ptr->att--;

    if (o_ptr->ds > 1)
        o_ptr->ds--;
    else if (o_ptr->dd > 1)
        o_ptr->dd--;

    pseudo_id(o_ptr);
    return true;
}

bool object_repair_fire_broken_weapon(object_type* o_ptr)
{
    s16b att = 0;
    byte dd = 0;
    byte ds = 0;

    if (!object_is_fire_broken(o_ptr))
        return false;

    unpack_fire_broken_weapon_payload(
        object_runtime_payload(o_ptr), &att, &dd, &ds);

    o_ptr->att = att;
    o_ptr->dd = dd;
    o_ptr->ds = ds;
    object_set_runtime_state(o_ptr, OBJECT_RUNTIME_STATE_NONE);
    object_set_runtime_payload(o_ptr, 0);

    pseudo_id(o_ptr);
    return true;
}

bool object_break_brass_lantern(object_type* o_ptr)
{
    byte old_prefix;
    bool old_prefix_carried_intrinsic_curse = false;
    bool new_state_is_intrinsically_cursed = false;

    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_LIGHT
        || o_ptr->sval != SV_LIGHT_LANTERN)
    {
        return false;
    }

    old_prefix = object_ego_prefix(o_ptr);
    if (old_prefix == EGO_BROKEN_BRASS_LANTERN)
    {
        o_ptr->ident |= IDENT_BROKEN;
        return true;
    }

    if (old_prefix)
    {
        if (old_prefix >= z_info->e_max)
            return false;

        if (!ego_affix_has_only_flag_effects(&e_info[old_prefix]))
        {
            log_warn(
                "object_break_brass_lantern: unsupported lantern prefix %d",
                old_prefix);
            return false;
        }

        old_prefix_carried_intrinsic_curse
            = (e_info[old_prefix].flags3
                & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
            != 0;
    }

    object_set_ego_prefix(o_ptr, EGO_BROKEN_BRASS_LANTERN);
    o_ptr->ident |= IDENT_BROKEN;

    if (o_ptr->name1
        && (a_info[o_ptr->name1].flags3
            & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE)))
    {
        new_state_is_intrinsically_cursed = true;
    }

    if (k_info[o_ptr->k_idx].flags3
        & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
    {
        new_state_is_intrinsically_cursed = true;
    }

    if (object_ego_suffix(o_ptr)
        && (e_info[object_ego_suffix(o_ptr)].flags3
            & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE)))
    {
        new_state_is_intrinsically_cursed = true;
    }

    if (new_state_is_intrinsically_cursed)
        o_ptr->ident |= IDENT_CURSED;
    else if (old_prefix_carried_intrinsic_curse)
        o_ptr->ident &= ~IDENT_CURSED;

    pseudo_id(o_ptr);
    return true;
}

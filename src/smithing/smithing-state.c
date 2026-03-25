/* File: smithing-state.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "smithing/smithing-internal.h"
#include "externs.h"

/* Smithing flow control (used by the UI). */
bool enchant_then_numbers = false;

/* Object being created (persisted in the savefile). */
static object_type smith_o_body;
object_type* smith_o_ptr = &smith_o_body;

/* Backups used for preview/undo within the smithing UI. */
static object_type smith2_o_body;
object_type* smith2_o_ptr = &smith2_o_body;

static object_type smith3_o_body;
object_type* smith3_o_ptr = &smith3_o_body;

/* Alloy states for the main object and its backups. */
smith_alloy_state smith_alloy;
smith_alloy_state smith2_alloy;
smith_alloy_state smith3_alloy;

/* Costs computed as a side-effect of object_difficulty(). */
smithing_cost_type smithing_cost;

const smithing_tval_desc smithing_tvals[MAX_SMITHING_TVALS] = {
    { CAT_WEAPON, TV_SWORD, "Sword" },
    { CAT_WEAPON, TV_POLEARM, "Axe or Polearm" },
    { CAT_WEAPON, TV_HAFTED, "Blunt Weapon" },
    { CAT_WEAPON, TV_DIGGING, "Digger" },
    { CAT_WEAPON, TV_BOW, "Bow" },
    { CAT_WEAPON, TV_ARROW, "Arrows" },
    { CAT_JEWELRY, TV_RING, "Ring" },
    { CAT_JEWELRY, TV_AMULET, "Amulet" },
    { CAT_JEWELRY, TV_LIGHT, "Light" },
    { CAT_JEWELRY, TV_HORN, "Horn" },
    { CAT_ARMOUR, TV_SOFT_ARMOR, "Soft Armour" },
    { CAT_ARMOUR, TV_MAIL, "Mail" },
    { CAT_ARMOUR, TV_CLOAK, "Cloak" },
    { CAT_ARMOUR, TV_SHIELD, "Shield" },
    { CAT_ARMOUR, TV_HELM, "Helm" },
    { CAT_ARMOUR, TV_GLOVES, "Gloves" },
    { CAT_ARMOUR, TV_BOOTS, "Boots" },
};

void smith_clear_alloy_state(smith_alloy_state* state)
{
    state->type = SMITH_ALLOY_NONE;
    state->bonus_att = 0;
    state->bonus_ds = 0;
    state->bonus_evn = 0;
    state->bonus_ps = 0;
}

void smith_remove_alloy_bonus(object_type* o_ptr, smith_alloy_state* state)
{
    if (!state)
        return;

    if (state->type != SMITH_ALLOY_NONE && o_ptr && o_ptr->k_idx)
    {
        o_ptr->att -= state->bonus_att;
        if (o_ptr->ds >= state->bonus_ds)
            o_ptr->ds -= state->bonus_ds;
        else
            o_ptr->ds = 0;
        o_ptr->evn -= state->bonus_evn;
        if (o_ptr->ps >= state->bonus_ps)
            o_ptr->ps -= state->bonus_ps;
        else
            o_ptr->ps = 0;
    }

    smith_clear_alloy_state(state);
}

int smith_item_category(const object_type* o_ptr)
{
    if (!o_ptr)
        return -1;

    for (int i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        if (smithing_tvals[i].tval == o_ptr->tval)
            return smithing_tvals[i].category;
    }

    return -1;
}

bool smith_alloy_applicable(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (object_has_evil_alignment(o_ptr))
        return false;

    int cat = smith_item_category(o_ptr);
    if ((cat != CAT_WEAPON) && (cat != CAT_ARMOUR))
        return false;

    /* Cannot alloy items that are already made of special metal */
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    if (k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON))
        return false;

    /* Armour: only major metal pieces */
    if (cat == CAT_ARMOUR)
    {
        switch (o_ptr->tval)
        {
        case TV_MAIL:
        case TV_SHIELD:
        case TV_HELM:
            return true;
        default:
            return false;
        }
    }

    /* Weapons: exclude quarterstaves (wooden) */
    if ((o_ptr->tval == TV_HAFTED) && (o_ptr->sval == SV_QUARTERSTAFF))
        return false;

    return true;
}

bool smith_apply_alloy(
    object_type* o_ptr, smith_alloy_state* state, smith_alloy_type new_type)
{
    if (!o_ptr || !state)
        return false;

    smith_remove_alloy_bonus(o_ptr, state);

    if (new_type == SMITH_ALLOY_NONE)
        return true;

    if (!smith_alloy_applicable(o_ptr))
        return false;

    int cat = smith_item_category(o_ptr);
    if (cat == CAT_WEAPON)
    {
        if (new_type == SMITH_ALLOY_MITHRIL)
            state->bonus_att = 1;
        else if (new_type == SMITH_ALLOY_STAR_IRON)
            state->bonus_ds = 1;
    }
    else if (cat == CAT_ARMOUR)
    {
        if (new_type == SMITH_ALLOY_MITHRIL)
            state->bonus_evn = 1;
        else if (new_type == SMITH_ALLOY_STAR_IRON)
            state->bonus_ps = 1;
    }
    else
    {
        return false;
    }

    o_ptr->att += state->bonus_att;
    o_ptr->ds += state->bonus_ds;
    o_ptr->evn += state->bonus_evn;
    o_ptr->ps += state->bonus_ps;
    state->type = new_type;
    return true;
}

int smith_alloy_weight_required(const object_type* o_ptr)
{
    int total_weight = o_ptr->weight * ((o_ptr->number > 0) ? o_ptr->number : 1);
    return (total_weight + 3) / 4;
}

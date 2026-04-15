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

/*
 * Artifice (custom artefact) bonus limits.
 *
 * When smithing a custom artefact, the item's max values from the R: line
 * are extended by these per-category bonuses. All artefact-specific limits
 * live in this single table so they are easy to find and tune.
 */
typedef struct
{
    int att_bonus;
    int att_floor;
    int ds_bonus;
    int evn_bonus;
    int evn_floor;
    int ps_bonus;
    int ps_floor;
    int pval_bonus;
} artifice_limits_t;

enum
{
    ARTIFICE_ARROW,
    ARTIFICE_MELEE,
    ARTIFICE_BOW,
    ARTIFICE_DIGGING,
    ARTIFICE_ARMOR,
    ARTIFICE_GLOVES,
    ARTIFICE_RING,
    ARTIFICE_AMULET,
    ARTIFICE_DEFAULT,
    ARTIFICE_MAX
};

static const artifice_limits_t artifice_table[ARTIFICE_MAX] = {
    { 8, 0, 0, 0, 0, 0, 0, 0 },
    { 4, 0, 2, 1, 0, 0, 0, 4 },
    { 4, 0, 2, 0, 0, 0, 0, 4 },
    { 4, 0, 2, 0, 0, 0, 0, 4 },
    { 1, 0, 0, 1, 0, 2, 0, 4 },
    { 2, 0, 0, 1, 0, 2, 0, 4 },
    { 0, 4, 0, 0, 4, 0, 0, 4 },
    { 0, 0, 0, 0, 0, 0, 3, 4 },
    { 0, 0, 0, 0, 0, 0, 0, 4 },
};

static int artifice_category(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW: return ARTIFICE_ARROW;
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED: return ARTIFICE_MELEE;
    case TV_BOW: return ARTIFICE_BOW;
    case TV_DIGGING: return ARTIFICE_DIGGING;
    case TV_GLOVES: return ARTIFICE_GLOVES;
    case TV_BOOTS:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL: return ARTIFICE_ARMOR;
    case TV_RING: return ARTIFICE_RING;
    case TV_AMULET: return ARTIFICE_AMULET;
    default: return ARTIFICE_DEFAULT;
    }
}

static const artifice_limits_t* artifice_bonus_for(const object_type* o_ptr)
{
    return &artifice_table[artifice_category(o_ptr)];
}

static void smithing_ego_bonus_sums(const object_type* o_ptr,
    int* max_att_sum, int* max_att_min_inc,
    int* to_ds_sum, int* to_ds_min_inc,
    int* max_evn_sum, int* max_evn_min_inc,
    int* to_ps_sum, int* to_ps_min_inc,
    int* max_pval_sum, int* max_pval_min_inc)
{
    if (max_att_sum) *max_att_sum = 0;
    if (max_att_min_inc) *max_att_min_inc = 0;
    if (to_ds_sum) *to_ds_sum = 0;
    if (to_ds_min_inc) *to_ds_min_inc = 0;
    if (max_evn_sum) *max_evn_sum = 0;
    if (max_evn_min_inc) *max_evn_min_inc = 0;
    if (to_ps_sum) *to_ps_sum = 0;
    if (to_ps_min_inc) *to_ps_min_inc = 0;
    if (max_pval_sum) *max_pval_sum = 0;
    if (max_pval_min_inc) *max_pval_min_inc = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return;

    byte egos[2] = { object_ego_prefix(o_ptr), object_ego_suffix(o_ptr) };
    for (int i = 0; i < 2; i++)
    {
        byte e_idx = egos[i];
        ego_item_type* e_ptr;
        int max_att;
        int to_ds;
        int max_evn;
        int to_ps;

        if (!e_idx)
            continue;

        e_ptr = &e_info[e_idx];
        max_att = (int)(int8_t)e_ptr->max_att;
        to_ds = (int)(int8_t)e_ptr->to_ds;
        max_evn = (int)(int8_t)e_ptr->max_evn;
        to_ps = (int)(int8_t)e_ptr->to_ps;

        if (max_att)
        {
            if (max_att_sum) *max_att_sum += max_att;
            if (max_att_min_inc)
                (*max_att_min_inc) += (max_att > 0) ? 1 : -1;
        }
        if (to_ds)
        {
            if (to_ds_sum) *to_ds_sum += to_ds;
            if (to_ds_min_inc)
                (*to_ds_min_inc) += (to_ds > 0) ? 1 : -1;
        }
        if (max_evn)
        {
            if (max_evn_sum) *max_evn_sum += max_evn;
            if (max_evn_min_inc)
                (*max_evn_min_inc) += (max_evn > 0) ? 1 : -1;
        }
        if (to_ps)
        {
            if (to_ps_sum) *to_ps_sum += to_ps;
            if (to_ps_min_inc)
                (*to_ps_min_inc) += (to_ps > 0) ? 1 : -1;
        }

        if (e_ptr->max_pval > 0)
        {
            if (max_pval_sum) *max_pval_sum += e_ptr->max_pval;
            if (max_pval_min_inc)
                (*max_pval_min_inc) += (e_ptr->min_pval > 0) ? e_ptr->min_pval : 1;
        }
    }
}

bool smithing_variable_protection_dice(const object_type* o_ptr)
{
    return o_ptr && o_ptr->tval == TV_AMULET
        && ((o_ptr->sval == SV_AMULET_PROTECTION)
            || (o_ptr->name1 && (o_ptr->pd > 0)));
}

typedef struct
{
    byte pd;
    byte ps;
} smithing_protection_combo;

static const smithing_protection_combo smithing_amulet_protection_combos[] = {
    { 1, 1 },
    { 1, 2 },
    { 1, 3 },
    { 2, 1 },
    { 2, 2 },
    { 2, 3 },
};

static int smithing_protection_combo_index(const object_type* o_ptr)
{
    if (!smithing_variable_protection_dice(o_ptr))
        return -1;

    for (size_t i = 0; i < N_ELEMENTS(smithing_amulet_protection_combos); i++)
    {
        if ((o_ptr->pd == smithing_amulet_protection_combos[i].pd)
            && (o_ptr->ps == smithing_amulet_protection_combos[i].ps))
        {
            return (int)i;
        }
    }

    return -1;
}

static void smithing_set_protection_combo(object_type* o_ptr, int combo_idx)
{
    if (!o_ptr)
        return;
    if (combo_idx < 0
        || combo_idx >= (int)N_ELEMENTS(smithing_amulet_protection_combos))
    {
        return;
    }

    o_ptr->pd = smithing_amulet_protection_combos[combo_idx].pd;
    o_ptr->ps = smithing_amulet_protection_combos[combo_idx].ps;
}

bool object_has_evil_alignment(const object_type* o_ptr)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;
    (void)f3;
    return (f4 & TR4_EVIL_ITEM) != 0;
}

int att_valid(void)
{
    return att_max() > att_min();
}

int att_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_att_sum = 0;
    int att;

    smithing_ego_bonus_sums(
        smith_o_ptr, &max_att_sum, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL);

    att = k_ptr->max_att + max_att_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);

        att += al->att_bonus;
        if (al->att_floor > att)
            att = al->att_floor;
    }

    return att;
}

int att_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_att_min_inc = 0;

    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, &max_att_min_inc, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL);

    return k_ptr->att + max_att_min_inc;
}

int ds_valid(void)
{
    return ds_max() > ds_min();
}

int ds_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ds_sum = 0;
    int ds;

    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, &to_ds_sum, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL);

    ds = k_ptr->max_ds + to_ds_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);

        ds += al->ds_bonus;
    }

    return ds;
}

int ds_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ds_min_inc = 0;
    int ds;

    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, &to_ds_min_inc, NULL, NULL, NULL, NULL,
        NULL, NULL);

    ds = k_ptr->ds + to_ds_min_inc;
    if ((k_ptr->dd > 0) && (ds < 1))
        ds = 1;

    return ds;
}

int evn_valid(void)
{
    return evn_max() > evn_min();
}

int evn_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_evn_sum = 0;
    int evn;

    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, &max_evn_sum, NULL, NULL, NULL,
        NULL, NULL);

    evn = k_ptr->max_evn + max_evn_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);

        evn += al->evn_bonus;
        if (al->evn_floor > evn)
            evn = al->evn_floor;
    }

    return evn;
}

int evn_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_evn_min_inc = 0;

    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, &max_evn_min_inc, NULL,
        NULL, NULL, NULL);

    return k_ptr->evn + max_evn_min_inc;
}

int ps_valid(void)
{
    return ps_max() > ps_min();
}

int ps_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ps_sum = 0;
    int ps;

    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, &to_ps_sum, NULL,
        NULL, NULL);

    ps = k_ptr->max_ps + to_ps_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);

        ps += al->ps_bonus;
        if (al->ps_floor > ps)
            ps = al->ps_floor;
    }

    return ps;
}

int ps_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ps_min_inc = 0;

    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        &to_ps_min_inc, NULL, NULL);

    return k_ptr->ps + to_ps_min_inc;
}

bool smithing_can_increase_protection(const object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return false;

    if (!smithing_variable_protection_dice(o_ptr))
        return o_ptr->ps < ps_max();

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx >= 0)
        return combo_idx < (int)N_ELEMENTS(smithing_amulet_protection_combos) - 1;

    return (o_ptr->pd <= 1) && (o_ptr->ps < 1);
}

bool smithing_can_decrease_protection(const object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return false;

    if (!smithing_variable_protection_dice(o_ptr))
        return o_ptr->ps > ps_min();

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx > 0)
        return true;

    return combo_idx == 0 && ps_min() < 1;
}

void smithing_increase_protection(object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        o_ptr->ps++;
        return;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx >= 0)
    {
        smithing_set_protection_combo(o_ptr, combo_idx + 1);
        return;
    }

    smithing_set_protection_combo(o_ptr, 0);
}

void smithing_decrease_protection(object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        o_ptr->ps--;
        return;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx > 0)
    {
        smithing_set_protection_combo(o_ptr, combo_idx - 1);
        return;
    }

    if (combo_idx == 0 && ps_min() < 1)
    {
        o_ptr->pd = 1;
        o_ptr->ps = 0;
    }
}

int pval_valid(void)
{
    u32b f1, f2, f3;

    object_flags(smith_o_ptr, &f1, &f2, &f3);
    return (f1 & TR1_PVAL_MASK) != 0;
}

int pval_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_pval_sum = 0;
    int max_pval_min_inc = 0;
    int pval = k_ptr->max_pval;
    u32b f1, f2, f3;

    object_flags(smith_o_ptr, &f1, &f2, &f3);
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        &max_pval_sum, &max_pval_min_inc);

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);

        pval += al->pval_bonus;
    }

    if (cursed_p(smith_o_ptr))
        pval -= max_pval_min_inc;
    else
        pval += max_pval_sum;

    return pval;
}

int pval_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int base_min = k_ptr->pval;
    byte egos[2] = { object_ego_prefix(smith_o_ptr), object_ego_suffix(smith_o_ptr) };

    for (int i = 0; i < 2; i++)
    {
        byte e_idx = egos[i];
        ego_item_type* e_ptr;

        if (!e_idx)
            continue;

        e_ptr = &e_info[e_idx];
        if (e_ptr->min_pval > 0)
            base_min += e_ptr->min_pval;
        else if (e_ptr->max_pval > 0)
            base_min += 1;
    }

    return base_min;
}

int wgt_valid(void)
{
    switch (smith_o_ptr->tval)
    {
    case TV_ARROW:
    case TV_RING:
    case TV_AMULET:
    case TV_LIGHT:
    case TV_HORN:
        return false;
    default:
        return true;
    }
}

int wgt_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    return div_round(k_ptr->weight, 2) * 3;
}

int wgt_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    return div_round(k_ptr->weight, 3) * 2;
}

byte smith_default_stack_size(const object_type* o_ptr)
{
    bool is_arrow = (o_ptr->tval == TV_ARROW);
    bool is_spear = (o_ptr->tval == TV_POLEARM) && (o_ptr->sval == SV_SPEAR);
    bool is_dagger = (o_ptr->tval == TV_SWORD) && (o_ptr->sval == SV_DAGGER);
    bool is_artifact;
    bool is_enchanted;

    if (!(is_arrow || is_spear || is_dagger))
        return o_ptr->number ? o_ptr->number : 1;

    is_artifact = (o_ptr->name1 != 0);
    is_enchanted = (!is_artifact) && object_has_ego(o_ptr);

    if (is_arrow)
    {
        if (is_artifact) return 12;
        if (is_enchanted) return 18;
        return 24;
    }

    if (is_artifact) return 1;
    if (is_enchanted) return 2;
    return 3;
}

void create_base_object(int tval, int sval)
{
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);

    object_prep(smith_o_ptr, lookup_kind(tval, sval));
    apply_magic_fake(smith_o_ptr);
    smith_o_ptr->weight = (&k_info[smith_o_ptr->k_idx])->weight;
    smith_o_ptr->ident |= (IDENT_KNOWN | IDENT_SPOIL);
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);
}

bool smith_base_item_kind_allowed(const object_kind* k_ptr)
{
    if (!k_ptr)
        return false;
    if (k_ptr->flags3 & TR3_INSTA_ART)
        return false;
    if (k_ptr->flags4 & TR4_EVIL_ITEM)
        return false;

    if (k_ptr->flags3 & TR3_NO_SMITHING)
    {
        bool allow_override = false;

        if ((c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_EOL)
            && (k_ptr->tval == TV_SOFT_ARMOR)
            && (k_ptr->sval == SV_ARMOUR_OF_GALVORN))
        {
            allow_override = true;
        }

        if (!allow_override)
            return false;
    }

    return true;
}

void modify_numbers(int choice)
{
    switch (choice)
    {
    case SMT_NUM_MENU_I_ATT:
        if ((smith_o_ptr->tval == TV_ARROW) && !smith_o_ptr->name1)
            smith_o_ptr->att += 3;
        else
            smith_o_ptr->att++;
        break;
    case SMT_NUM_MENU_D_ATT:
        if ((smith_o_ptr->tval == TV_ARROW) && !smith_o_ptr->name1)
            smith_o_ptr->att -= 3;
        else
            smith_o_ptr->att--;
        break;
    case SMT_NUM_MENU_I_DS:
        smith_o_ptr->ds++;
        break;
    case SMT_NUM_MENU_D_DS:
        smith_o_ptr->ds--;
        break;
    case SMT_NUM_MENU_I_EVN:
        smith_o_ptr->evn++;
        break;
    case SMT_NUM_MENU_D_EVN:
        smith_o_ptr->evn--;
        break;
    case SMT_NUM_MENU_I_PS:
        smithing_increase_protection(smith_o_ptr);
        break;
    case SMT_NUM_MENU_D_PS:
        smithing_decrease_protection(smith_o_ptr);
        break;
    case SMT_NUM_MENU_I_WGT:
        smith_o_ptr->weight += 5;
        break;
    case SMT_NUM_MENU_D_WGT:
        smith_o_ptr->weight -= 5;
        break;
    case SMT_NUM_MENU_ALLOY_CYCLE:
    {
        smith_alloy_type next = SMITH_ALLOY_NONE;

        if (!p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY])
        {
            bell("You need Alloy mastery to do that.");
            break;
        }
        if (!smith_alloy_applicable(smith_o_ptr))
        {
            bell("Alloying doesn't apply to this item.");
            smith_remove_alloy_bonus(smith_o_ptr, &smith_alloy);
            break;
        }

        if (smith_alloy.type == SMITH_ALLOY_NONE)
            next = SMITH_ALLOY_MITHRIL;
        else if (smith_alloy.type == SMITH_ALLOY_MITHRIL)
            next = SMITH_ALLOY_STAR_IRON;

        (void)smith_apply_alloy(smith_o_ptr, &smith_alloy, next);
        break;
    }
    case SMT_NUM_MENU_ALLOY_CLEAR:
        smith_remove_alloy_bonus(smith_o_ptr, &smith_alloy);
        break;
    default:
        break;
    }
}

static int smith_collect_bonus_entries(smith_bonus_entry* entries, int max_entries)
{
    u32b f1, f2, f3;
    int n = 0;

    struct stat_flag_map
    {
        int stat;
        u32b flag_pos;
        u32b flag_neg;
    };

    static const struct stat_flag_map stat_flags[A_MAX] = {
        { A_STR, TR1_STR, TR1_NEG_STR },
        { A_DEX, TR1_DEX, TR1_NEG_DEX },
        { A_CON, TR1_CON, TR1_NEG_CON },
        { A_GRA, TR1_GRA, TR1_NEG_GRA },
    };

    struct skill_flag_map
    {
        int skill;
        u32b flag;
    };

    static const struct skill_flag_map skill_flags[] = {
        { S_MEL, TR1_MEL },
        { S_ARC, TR1_ARC },
        { S_STL, TR1_STL },
        { S_PER, TR1_PER },
        { S_WIL, TR1_WIL },
        { S_SMT, TR1_SMT },
        { S_SNG, TR1_SNG },
    };

    struct special_flag_map
    {
        int special;
        u32b flag;
    };

    static const struct special_flag_map special_flags[] = {
        { SMT_BONUS_SPECIAL_DAMAGE_SIDES, TR1_DAMAGE_SIDES },
        { SMT_BONUS_SPECIAL_TUNNEL, TR1_TUNNEL },
    };

    if (!entries || max_entries <= 0)
        return 0;

    object_flags(smith_o_ptr, &f1, &f2, &f3);
    (void)f2;
    (void)f3;

    for (int i = 0; i < A_MAX && n < max_entries; i++)
    {
        if ((f1 & (stat_flags[i].flag_pos | stat_flags[i].flag_neg)) == 0)
            continue;

        entries[n].kind = SMT_BONUS_ENTRY_STAT;
        entries[n].index = stat_flags[i].stat;
        entries[n].flag_pos = stat_flags[i].flag_pos;
        entries[n].flag_neg = stat_flags[i].flag_neg;
        entries[n].flag = 0;
        n++;
    }

    for (int i = 0; i < (int)N_ELEMENTS(skill_flags) && n < max_entries; i++)
    {
        if ((f1 & skill_flags[i].flag) == 0)
            continue;

        entries[n].kind = SMT_BONUS_ENTRY_SKILL;
        entries[n].index = skill_flags[i].skill;
        entries[n].flag_pos = 0;
        entries[n].flag_neg = 0;
        entries[n].flag = skill_flags[i].flag;
        n++;
    }

    for (int i = 0; i < (int)N_ELEMENTS(special_flags) && n < max_entries; i++)
    {
        if ((f1 & special_flags[i].flag) == 0)
            continue;

        entries[n].kind = SMT_BONUS_ENTRY_SPECIAL;
        entries[n].index = special_flags[i].special;
        entries[n].flag_pos = 0;
        entries[n].flag_neg = 0;
        entries[n].flag = special_flags[i].flag;
        n++;
    }

    return n;
}

int smith_collect_bonus_actions(smith_bonus_action* actions, int max_actions)
{
    smith_bonus_entry entries[16];
    int entry_count = smith_collect_bonus_entries(entries, (int)N_ELEMENTS(entries));
    int action_count = 0;

    if (!actions || max_actions <= 0)
        return 0;

    for (int i = 0; i < entry_count && action_count < max_actions; i++)
    {
        actions[action_count].entry = entries[i];
        actions[action_count].delta = 1;
        action_count++;

        if (action_count >= max_actions)
            break;

        actions[action_count].entry = entries[i];
        actions[action_count].delta = -1;
        action_count++;
    }

    return action_count;
}

bool smith_adjust_bonus_entry(const smith_bonus_entry* entry, int delta)
{
    int max_bonus = pval_max();
    int floor_bonus = pval_min();
    int min_bonus = 0;
    int value = 0;

    if (!entry || !smith_o_ptr || delta == 0)
        return false;

    if (entry->kind == SMT_BONUS_ENTRY_STAT)
    {
        u32b f1, f2, f3;
        bool has_pos;
        bool has_neg;
        int new_value;

        object_flags(smith_o_ptr, &f1, &f2, &f3);
        (void)f2;
        (void)f3;

        has_pos = (f1 & entry->flag_pos) != 0;
        has_neg = (f1 & entry->flag_neg) != 0;

        if (has_pos && has_neg)
            min_bonus = -max_bonus;
        else if (has_neg)
        {
            min_bonus = -max_bonus;
            max_bonus = 0;
        }
        else
        {
            min_bonus = floor_bonus;
        }

        value = smith_o_ptr->stat_bonus[entry->index];
        new_value = value + delta;
        if ((new_value < min_bonus) || (new_value > max_bonus))
            return false;

        smith_o_ptr->stat_bonus[entry->index] = new_value;
        return true;
    }

    if (entry->kind == SMT_BONUS_ENTRY_SKILL)
    {
        int new_value = smith_o_ptr->skill_bonus[entry->index] + delta;

        if ((new_value < floor_bonus) || (new_value > max_bonus))
            return false;
        smith_o_ptr->skill_bonus[entry->index] = new_value;
        return true;
    }

    value = smith_o_ptr->pval;
    if ((value + delta < floor_bonus) || (value + delta > max_bonus))
        return false;
    smith_o_ptr->pval = (s16b)(value + delta);
    return true;
}

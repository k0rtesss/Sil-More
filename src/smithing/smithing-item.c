/* File: smithing-item.c */

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
#include "log/log.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"

void artefact_copy(artefact_type* a1_ptr, artefact_type* a2_ptr)
{
    /* Copy the structure */
    memcpy(a1_ptr, a2_ptr, sizeof(artefact_type));
}

/*
 * Fills in the details on the artefact type being created.
 */
void add_artefact_details(void)
{
    smith_a_ptr->tval = smith_o_ptr->tval;
    smith_a_ptr->sval = smith_o_ptr->sval;
    smith_a_ptr->pval = smith_o_ptr->pval;
    smith_a_ptr->att = smith_o_ptr->att;
    smith_a_ptr->evn = smith_o_ptr->evn;
    smith_a_ptr->dd = smith_o_ptr->dd;
    smith_a_ptr->ds = smith_o_ptr->ds;
    smith_a_ptr->pd = smith_o_ptr->pd;
    smith_a_ptr->ps = smith_o_ptr->ps;
    smith_a_ptr->weight = smith_o_ptr->weight;
    smith_a_ptr->flags1 |= (&k_info[smith_o_ptr->k_idx])->flags1;
    smith_a_ptr->flags2 |= (&k_info[smith_o_ptr->k_idx])->flags2;
    smith_a_ptr->flags3 |= (&k_info[smith_o_ptr->k_idx])->flags3;

    memcpy(smith_a_ptr->stat_bonus, smith_o_ptr->stat_bonus, sizeof(smith_a_ptr->stat_bonus));
    memcpy(smith_a_ptr->skill_bonus, smith_o_ptr->skill_bonus, sizeof(smith_a_ptr->skill_bonus));
    memset(smith_a_ptr->stat_bonus_set, 0, sizeof(smith_a_ptr->stat_bonus_set));
    memset(smith_a_ptr->skill_bonus_set, 0, sizeof(smith_a_ptr->skill_bonus_set));

    smith_a_ptr->cur_num = 1;
    smith_a_ptr->found_num = 1;
    smith_a_ptr->spawn_num = 1;
    smith_a_ptr->level = object_difficulty(smith_o_ptr);
    smith_a_ptr->rarity = 10;
}

static bool smith_has_category_ability(const object_type* o_ptr)
{
    int cat;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    cat = smith_item_category(o_ptr);
    if ((cat == CAT_WEAPON) && !p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH])
        return false;
    if ((cat == CAT_ARMOUR) && !p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH])
        return false;
    if ((cat == CAT_JEWELRY) && !p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        return false;

    return true;
}

static bool smith_has_alignment_conflict(const object_type* o_ptr,
    int prefix_idx, int suffix_idx)
{
    u32b f1, f2, f3, f4;
    bool has_noble;
    bool has_evil;

    if (!o_ptr)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;
    (void)f3;

    has_noble = (f4 & TR4_NOBLE_ITEM) != 0;
    has_evil = (f4 & TR4_EVIL_ITEM) != 0;

    if (prefix_idx > 0)
    {
        if (e_info[prefix_idx].flags4 & TR4_NOBLE_ITEM)
            has_noble = true;
        if (e_info[prefix_idx].flags4 & TR4_EVIL_ITEM)
            has_evil = true;
    }

    if (suffix_idx > 0)
    {
        if (e_info[suffix_idx].flags4 & TR4_NOBLE_ITEM)
            has_noble = true;
        if (e_info[suffix_idx].flags4 & TR4_EVIL_ITEM)
            has_evil = true;
    }

    return has_noble && has_evil;
}

bool ego_forbids_prefix_combo(int e_idx)
{
    if (e_idx <= 0 || e_idx >= z_info->e_max)
        return false;

    return (e_info[e_idx].flags4 & TR4_NO_PREFIX) != 0;
}

static bool smith_ego_is_forbidden_affix(const ego_item_type* e_ptr)
{
    if (!e_ptr)
        return true;
    if (e_ptr->flags3 & (TR3_DAMAGED | TR3_NO_SMITHING))
        return true;
    if (e_ptr->flags4 & (TR4_JINX | TR4_EVIL_ITEM))
        return true;
    return false;
}

static bool smith_ego_matches_item_type(const object_type* o_ptr,
    const ego_item_type* e_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || !e_ptr)
        return false;

    for (int j = 0; j < EGO_TVALS_MAX; j++)
    {
        if (o_ptr->tval != e_ptr->tval[j])
            continue;
        if (o_ptr->sval < e_ptr->min_sval[j])
            continue;
        if (o_ptr->sval > e_ptr->max_sval[j])
            continue;

        return true;
    }

    return false;
}

bool smith_ego_can_apply_to_object(const object_type* o_ptr, int e_idx,
    int fixed_prefix, int fixed_suffix, bool selecting_prefix)
{
    ego_item_type* e_ptr;
    const char* raw_name;
    bool is_prefix;

    if (!o_ptr || !o_ptr->k_idx || e_idx <= 0 || e_idx >= z_info->e_max)
        return false;

    e_ptr = &e_info[e_idx];
    raw_name = e_name + e_ptr->name;
    is_prefix = ego_name_is_prefix(raw_name);

    if (selecting_prefix != is_prefix)
        return false;
    if (smith_ego_is_forbidden_affix(e_ptr))
        return false;
    if (!smith_ego_matches_item_type(o_ptr, e_ptr))
        return false;

    if (selecting_prefix)
    {
        if (ego_forbids_prefix_combo(fixed_suffix))
            return false;
        if (smith_has_alignment_conflict(o_ptr, e_idx, fixed_suffix))
            return false;
    }
    else
    {
        if ((fixed_prefix != 0) && ego_forbids_prefix_combo(e_idx))
            return false;
        if (smith_has_alignment_conflict(o_ptr, fixed_prefix, e_idx))
            return false;
    }

    return true;
}

bool ego_prefix_can_apply_to_object(const object_type* o_ptr, int e_idx)
{
    return smith_ego_can_apply_to_object(o_ptr, e_idx, 0, 0, true);
}

static void smith_apply_stat_skill_flag_delta(object_type* o_ptr,
    u32b f1_before, u32b f1_after)
{
    int pval = o_ptr->pval;
    int pval_abs = ABS(pval);
    bool before_str;
    bool after_str;
    bool before_dex;
    bool after_dex;
    bool before_con;
    bool after_con;
    bool before_gra;
    bool after_gra;
    bool before_mel;
    bool after_mel;
    bool before_arc;
    bool after_arc;
    bool before_stl;
    bool after_stl;
    bool before_per;
    bool after_per;
    bool before_wil;
    bool after_wil;
    bool before_smt;
    bool after_smt;
    bool before_sng;
    bool after_sng;

    if (!o_ptr)
        return;

    before_str = (f1_before & (TR1_STR | TR1_NEG_STR)) != 0;
    after_str = (f1_after & (TR1_STR | TR1_NEG_STR)) != 0;
    if (!after_str)
        o_ptr->stat_bonus[A_STR] = 0;
    else if (!before_str)
        o_ptr->stat_bonus[A_STR] = (f1_after & TR1_NEG_STR) ? -pval_abs : pval_abs;
    if ((f1_after & TR1_STR) && !(f1_after & TR1_NEG_STR)
        && o_ptr->stat_bonus[A_STR] < 0)
        o_ptr->stat_bonus[A_STR] = -o_ptr->stat_bonus[A_STR];
    if ((f1_after & TR1_NEG_STR) && !(f1_after & TR1_STR)
        && o_ptr->stat_bonus[A_STR] > 0)
        o_ptr->stat_bonus[A_STR] = -o_ptr->stat_bonus[A_STR];

    before_dex = (f1_before & (TR1_DEX | TR1_NEG_DEX)) != 0;
    after_dex = (f1_after & (TR1_DEX | TR1_NEG_DEX)) != 0;
    if (!after_dex)
        o_ptr->stat_bonus[A_DEX] = 0;
    else if (!before_dex)
        o_ptr->stat_bonus[A_DEX] = (f1_after & TR1_NEG_DEX) ? -pval_abs : pval_abs;
    if ((f1_after & TR1_DEX) && !(f1_after & TR1_NEG_DEX)
        && o_ptr->stat_bonus[A_DEX] < 0)
        o_ptr->stat_bonus[A_DEX] = -o_ptr->stat_bonus[A_DEX];
    if ((f1_after & TR1_NEG_DEX) && !(f1_after & TR1_DEX)
        && o_ptr->stat_bonus[A_DEX] > 0)
        o_ptr->stat_bonus[A_DEX] = -o_ptr->stat_bonus[A_DEX];

    before_con = (f1_before & (TR1_CON | TR1_NEG_CON)) != 0;
    after_con = (f1_after & (TR1_CON | TR1_NEG_CON)) != 0;
    if (!after_con)
        o_ptr->stat_bonus[A_CON] = 0;
    else if (!before_con)
        o_ptr->stat_bonus[A_CON] = (f1_after & TR1_NEG_CON) ? -pval_abs : pval_abs;
    if ((f1_after & TR1_CON) && !(f1_after & TR1_NEG_CON)
        && o_ptr->stat_bonus[A_CON] < 0)
        o_ptr->stat_bonus[A_CON] = -o_ptr->stat_bonus[A_CON];
    if ((f1_after & TR1_NEG_CON) && !(f1_after & TR1_CON)
        && o_ptr->stat_bonus[A_CON] > 0)
        o_ptr->stat_bonus[A_CON] = -o_ptr->stat_bonus[A_CON];

    before_gra = (f1_before & (TR1_GRA | TR1_NEG_GRA)) != 0;
    after_gra = (f1_after & (TR1_GRA | TR1_NEG_GRA)) != 0;
    if (!after_gra)
        o_ptr->stat_bonus[A_GRA] = 0;
    else if (!before_gra)
        o_ptr->stat_bonus[A_GRA] = (f1_after & TR1_NEG_GRA) ? -pval_abs : pval_abs;
    if ((f1_after & TR1_GRA) && !(f1_after & TR1_NEG_GRA)
        && o_ptr->stat_bonus[A_GRA] < 0)
        o_ptr->stat_bonus[A_GRA] = -o_ptr->stat_bonus[A_GRA];
    if ((f1_after & TR1_NEG_GRA) && !(f1_after & TR1_GRA)
        && o_ptr->stat_bonus[A_GRA] > 0)
        o_ptr->stat_bonus[A_GRA] = -o_ptr->stat_bonus[A_GRA];

    before_mel = (f1_before & TR1_MEL) != 0;
    after_mel = (f1_after & TR1_MEL) != 0;
    if (!after_mel)
        o_ptr->skill_bonus[S_MEL] = 0;
    else if (!before_mel)
        o_ptr->skill_bonus[S_MEL] = pval;

    before_arc = (f1_before & TR1_ARC) != 0;
    after_arc = (f1_after & TR1_ARC) != 0;
    if (!after_arc)
        o_ptr->skill_bonus[S_ARC] = 0;
    else if (!before_arc)
        o_ptr->skill_bonus[S_ARC] = pval;

    before_stl = (f1_before & TR1_STL) != 0;
    after_stl = (f1_after & TR1_STL) != 0;
    if (!after_stl)
        o_ptr->skill_bonus[S_STL] = 0;
    else if (!before_stl)
        o_ptr->skill_bonus[S_STL] = pval;

    before_per = (f1_before & TR1_PER) != 0;
    after_per = (f1_after & TR1_PER) != 0;
    if (!after_per)
        o_ptr->skill_bonus[S_PER] = 0;
    else if (!before_per)
        o_ptr->skill_bonus[S_PER] = pval;

    before_wil = (f1_before & TR1_WIL) != 0;
    after_wil = (f1_after & TR1_WIL) != 0;
    if (!after_wil)
        o_ptr->skill_bonus[S_WIL] = 0;
    else if (!before_wil)
        o_ptr->skill_bonus[S_WIL] = pval;

    before_smt = (f1_before & TR1_SMT) != 0;
    after_smt = (f1_after & TR1_SMT) != 0;
    if (!after_smt)
        o_ptr->skill_bonus[S_SMT] = 0;
    else if (!before_smt)
        o_ptr->skill_bonus[S_SMT] = pval;

    before_sng = (f1_before & TR1_SNG) != 0;
    after_sng = (f1_after & TR1_SNG) != 0;
    if (!after_sng)
        o_ptr->skill_bonus[S_SNG] = 0;
    else if (!before_sng)
        o_ptr->skill_bonus[S_SNG] = pval;
}

void create_special(int ego_prefix, int ego_suffix)
{
    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;

    if (ego_forbids_prefix_combo(ego_suffix))
        ego_prefix = 0;

    object_set_ego_prefix(smith_o_ptr, ego_prefix);
    object_set_ego_suffix(smith_o_ptr, ego_suffix);

    if (object_has_ego(smith_o_ptr))
        object_into_special(smith_o_ptr, p_ptr->skill_use[S_SMT], true);

    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);
}

bool enchant_menu_has_applicable_affix(const object_type* base_o_ptr,
    int fixed_prefix, int fixed_suffix, bool selecting_prefix)
{
    if (!base_o_ptr || !smith_o_ptr || base_o_ptr->tval == 0)
        return false;
    if (object_has_evil_alignment(smith_o_ptr))
        return false;

    for (int i = 1; i < z_info->e_max; i++)
    {
        if (smith_ego_can_apply_to_object(
                base_o_ptr, i, fixed_prefix, fixed_suffix, selecting_prefix))
        {
            return true;
        }
    }

    return false;
}

void prepare_artefact(void)
{
    log_debug("Preparing artifact for modification");

    artefact_copy(smith_a_ptr, smith2_a_ptr);
    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;

    smith_o_ptr->name1 = smith_a_name;
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);

    for (int i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;

    log_trace("Artifact preparation complete - %d abilities synchronized",
        smith_a_ptr->abilities);
}

bool applicable_flag(u32b f, int flagset, object_type* o_ptr)
{
    bool ok = false;
    u32b f1, f2, f3, f4;

    if ((flagset == 1) && (f == TR1_SHARPNESS2)
        && (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
    {
        switch (smith_o_ptr->tval)
        {
        case TV_SWORD:
        case TV_HAFTED:
        case TV_POLEARM:
        case TV_DIGGING:
            return true;
        default:
            break;
        }
    }

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    if ((flagset == 1) && (f == TR1_SMT))
    {
        if (o_ptr->tval != TV_HAFTED || o_ptr->sval != SV_WAR_HAMMER)
            return false;
        if (!(f1 & TR1_BRAND_FIRE))
            return false;
        return true;
    }

    for (int i = ART_ULTIMATE; i < z_info->art_norm_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        if (a_ptr->tval != o_ptr->tval)
            continue;

        switch (flagset)
        {
        case 1:
            if (a_ptr->flags1 & f)
                ok = true;
            break;
        case 2:
            if (a_ptr->flags2 & f)
                ok = true;
            break;
        case 3:
            if (a_ptr->flags3 & f)
                ok = true;
            break;
        case 4:
            if (a_ptr->flags4 & f)
                ok = true;
            break;
        default:
            break;
        }
    }

    (void)f2;
    (void)f3;
    (void)f4;
    return ok;
}

void add_artefact_flag(u32b f, int flagset)
{
    u32b f1_before, f2, f3;
    u32b f1_after;

    log_trace("Adding artifact flag %u in flagset %d", f, flagset);

    prepare_artefact();
    object_flags(smith_o_ptr, &f1_before, &f2, &f3);

    if (flagset == 1)
        smith_a_ptr->flags1 |= f;
    if (flagset == 2)
        smith_a_ptr->flags2 |= f;
    if (flagset == 3)
        smith_a_ptr->flags3 |= f;
    if (flagset == 4)
        smith_a_ptr->flags4 |= f;

    object_flags(smith_o_ptr, &f1_after, &f2, &f3);
    smith_apply_stat_skill_flag_delta(smith_o_ptr, f1_before, f1_after);
}

void remove_artefact_flag(u32b f, int flagset)
{
    u32b f1_before, f2, f3;
    u32b f1_after;

    log_trace("Removing artifact flag %u from flagset %d", f, flagset);

    prepare_artefact();
    object_flags(smith_o_ptr, &f1_before, &f2, &f3);

    if (flagset == 1)
        smith_a_ptr->flags1 &= ~f;
    if (flagset == 2)
        smith_a_ptr->flags2 &= ~f;
    if (flagset == 3)
        smith_a_ptr->flags3 &= ~f;
    if (flagset == 4)
        smith_a_ptr->flags4 &= ~f;

    if ((flagset == 1) && (f == TR1_BRAND_FIRE))
        smith_a_ptr->flags1 &= ~TR1_SMT;

    object_flags(smith_o_ptr, &f1_after, &f2, &f3);
    smith_apply_stat_skill_flag_delta(smith_o_ptr, f1_before, f1_after);
}

bool ability_can_be_smithed(ability_type* b_ptr)
{
    for (int j = 0; j < ABILITY_TVALS_MAX; j++)
    {
        if (b_ptr->tval[j] != 0)
            return true;
    }

    return false;
}

bool applicable_ability(ability_type* b_ptr, object_type* o_ptr)
{
    bool ok = false;
    u32b f1, f2, f3;

    for (int j = 0; j < ABILITY_TVALS_MAX; j++)
    {
        if (o_ptr->tval != b_ptr->tval[j])
            continue;
        if (o_ptr->sval < b_ptr->min_sval[j])
            continue;
        if (o_ptr->sval > b_ptr->max_sval[j])
            continue;
        ok = true;
    }

    object_flags(o_ptr, &f1, &f2, &f3);
    if ((f3 & TR3_POLEARM)
        && (b_ptr->skilltype == S_MEL)
        && (b_ptr->abilitynum == MEL_POLEARMS))
    {
        ok = true;
    }

    (void)f2;
    return ok;
}

void add_artefact_ability(int skilltype, int abilitynum)
{
    log_trace("Adding artifact ability - skill:%d ability:%d", skilltype,
        abilitynum);

    prepare_artefact();

    if (smith_a_ptr->abilities < 4)
    {
        bool already_present = false;

        for (int i = 0; i < smith_a_ptr->abilities; i++)
        {
            if ((smith_a_ptr->skilltype[i] == skilltype)
                && (smith_a_ptr->abilitynum[i] == abilitynum))
            {
                already_present = true;
            }
        }

        if (!already_present)
        {
            smith_a_ptr->skilltype[smith_a_ptr->abilities] = skilltype;
            smith_a_ptr->abilitynum[smith_a_ptr->abilities] = abilitynum;
            smith_a_ptr->bane_type[smith_a_ptr->abilities] = 0;
            smith_a_ptr->abilities++;
        }
    }

    for (int i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
}

void remove_artefact_ability(int skilltype, int abilitynum)
{
    int location = -1;

    log_trace("Removing artifact ability - skill:%d ability:%d", skilltype,
        abilitynum);

    prepare_artefact();

    for (int i = 0; i < smith_a_ptr->abilities; i++)
    {
        if ((smith_a_ptr->skilltype[i] == skilltype)
            && (smith_a_ptr->abilitynum[i] == abilitynum))
        {
            location = i;
        }
    }

    if (location >= 0)
    {
        for (int i = location; i < smith_a_ptr->abilities - 1; i++)
        {
            smith_a_ptr->skilltype[i] = smith_a_ptr->skilltype[i + 1];
            smith_a_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i + 1];
            smith_a_ptr->bane_type[i] = smith_a_ptr->bane_type[i + 1];
        }

        smith_a_ptr->skilltype[smith_a_ptr->abilities - 1] = 0;
        smith_a_ptr->abilitynum[smith_a_ptr->abilities - 1] = 0;
        smith_a_ptr->bane_type[smith_a_ptr->abilities - 1] = 0;
        smith_a_ptr->abilities--;
    }

    for (int i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
}

bool has_ability(artefact_type* a_ptr, int skilltype, int abilitynum)
{
    for (int i = 0; i < a_ptr->abilities; i++)
    {
        if ((a_ptr->skilltype[i] == skilltype)
            && (a_ptr->abilitynum[i] == abilitynum))
        {
            return true;
        }
    }

    return false;
}

bool object_can_reforge_prefix(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (o_ptr->name1)
        return false;
    if (object_is_damaged_item(o_ptr))
        return false;
    if (object_has_evil_alignment(o_ptr))
        return false;
    if (is_smithed_by_player(o_ptr))
        return false;
    if (object_ego_prefix(o_ptr))
        return false;
    if (ego_forbids_prefix_combo((int)object_ego_suffix(o_ptr)))
        return false;
    if (!smith_has_category_ability(o_ptr))
        return false;

    for (int i = 1; i < z_info->e_max; i++)
    {
        if (ego_prefix_can_apply_to_object(o_ptr, i))
            return true;
    }

    return false;
}

int find_reforge_target_item(void)
{
    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;
        if (object_can_repair_damage(o_ptr) || object_can_reforge_prefix(o_ptr))
            return i;
    }

    return -1;
}

/*
 * Finalize and grant the item currently being crafted.
 */
void create_smithing_item(void)
{
    int slot;
    object_type* o_ptr;
    char o_name[80];

    log_debug("Creating smithing item");

    // pay the ability/experience costs of smithing
    pay_costs();

    // if making an artefact, copy its attributes into the proper place in the
    // a_info array
    if (smith_o_ptr->name1)
    {
        log_info("Creating new artifact");
        smith_o_ptr->name1 = z_info->art_rand_max + p_ptr->self_made_arts;

        artefact_copy(&a_info[smith_o_ptr->name1], smith_a_ptr);
        artefact_type* created = &a_info[smith_o_ptr->name1];
        if (score_guid_is_zero(&created->guid)) {
            created->guid = score_guid_random();
        }
        (void)score_artefact_register(created);
        p_ptr->self_made_arts++;

        // make sure to display it as cursed if it is so
        if (smith_a_ptr->flags3
            & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        {
            smith_o_ptr->ident |= (IDENT_CURSED);
            log_debug("Artifact marked as cursed");
        }

        // Store the depth at which it was created
        smith_o_ptr->xtra1 = p_ptr->depth;
        
        log_debug("Artifact #%d created at depth %d", p_ptr->self_made_arts, p_ptr->depth);
    }

        /* ------------------------------------------------------ */
        /* New escape-curse: smithing can back-fire               */
        /* ------------------------------------------------------ */
        {
            int stacks = curse_flag_count_cur(CUR_SMITHCURSE);          /* 0-3 */
            if (stacks &&            /* must have the curse          */
                !(smith_o_ptr->ident & IDENT_CURSED) &&             /* not already */
                (smith_o_ptr->tval != TV_LIGHT))                    /* skip torches */
            {
                if (rand_int(100) < 10 * stacks)                    /* 10 % / stack */
                {
                    log_debug("Smithing curse triggered - adding random curse");
                    add_random_curse(smith_o_ptr);
                }
            }
        }


    // remove the spoiler ident flag
    smith_o_ptr->ident &= ~(IDENT_SPOIL);

    // identify the object
    ident(smith_o_ptr);

    // create description
    object_desc(o_name, sizeof(o_name), smith_o_ptr, true, 3);

    // Record the depth where the object was created
    do_cmd_note(format("Made %s  %d.%d lb", o_name,
                    (smith_o_ptr->weight * smith_o_ptr->number) / 10,
                    (smith_o_ptr->weight * smith_o_ptr->number) % 10),
        p_ptr->depth);

    // Get the slot of the forged item
    slot = inven_carry(smith_o_ptr, true);

    // Check if the item couldn't fit in inventory (e.g., group limit)
    if (slot < 0)
    {
        // Drop it on the floor instead
        log_debug("Smithed item couldn't fit in inventory, dropping to floor");
        drop_near(smith_o_ptr, 0, p_ptr->py, p_ptr->px);
        
        // Describe the object
        object_desc(o_name, sizeof(o_name), smith_o_ptr, true, 3);
        
        // Message
        msg_format("You have forged %s, but it falls to the floor.", o_name);
        log_info("Created smithing item (dropped): %s", o_name);
    }
    else
    {
        // Get the item itself
        o_ptr = &inventory[slot];
        
        // Mark the item as smithed by the player (using unused1 field)
        o_ptr->unused1 = 1;  /* 1 = smithed by player, 0 = found item */

        // Describe the object
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        // Message
        msg_format("You have %s (%c).", o_name, index_to_label(slot));
        log_info("Created smithing item: %s", o_name);
    }

    // Wipe the smithing object
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);
}


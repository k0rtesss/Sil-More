/* File: ui-smithing-screen.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "app/app-session.h"
#include "object/object-ui-select.h"
#include "ui/smithing/ui-smithing-internal.h"
#include "externs.h"
#include "log/log.h"

static int smith_ui_last_desc_row = -1;

static char smith_ui_inkey_with_wait_reason(void)
{
    app_wait_scope scope;
    app_session* session = app_session_current();
    char ch;

    app_session_push_wait_scope(session, &scope,
        APP_WAIT_REASON_LIST_SELECTION, 0, 0);
    inkey_set_cursor_hidden(true);
    ch = inkey();
    inkey_set_cursor_hidden(false);
    app_session_pop_wait_scope(session, &scope);
    return ch;
}

static int smith_ui_term_wid(void)
{
    return (Term && (Term->wid > 0)) ? Term->wid : 80;
}

static int smith_ui_term_hgt(void)
{
    return (Term && (Term->hgt > 0)) ? Term->hgt : 24;
}

static bool smith_ui_compact_width(void)
{
    return (smith_ui_term_wid() < 72);
}

static bool smith_ui_compact_height(void)
{
    return (smith_ui_term_hgt() <= 18);
}

static int smith_ui_secondary_col(void)
{
    return smith_ui_compact_width() ? COL_SMT2 : 36;
}

static int smith_ui_cost_col(void)
{
    int wid = smith_ui_term_wid();
    int col = wid - (smith_ui_compact_width() ? 15 : 18);
    int min_col = smith_ui_secondary_col() + 14;

    if (col < min_col)
        col = min_col;
    if (col < 32)
        col = 32;
    if (col > wid - 1)
        col = wid - 1;

    return col;
}

static int smith_ui_dense_row0(void)
{
    return smith_ui_compact_height() ? 1 : 2;
}

static int smith_ui_dense_row(int index0)
{
    return smith_ui_dense_row0() + index0;
}

static int smith_ui_dense_highlight_row(int highlight)
{
    return smith_ui_dense_row0() + highlight - 1;
}

static int smith_ui_cost_title_row(void)
{
    return smith_ui_compact_height() ? 6 : 8;
}

static int smith_ui_cost_item_row(int index0)
{
    return smith_ui_cost_title_row() + 2 + index0;
}

static int smith_ui_desc_col(void)
{
    return COL_SMT1;
}

static bool smith_ui_show_lore(void)
{
    return (smith_ui_term_hgt() > 18);
}

static int smith_ui_preferred_desc_lines(void)
{
    int hgt = smith_ui_term_hgt();

    if (hgt <= 18)
        return 2;
    if (hgt <= 20)
        return 3;
    if (hgt <= 22)
        return 4;

    return 5;
}

static void smith_ui_reset_description_state(void)
{
    smith_ui_last_desc_row = -1;
}

static void smith_ui_clear_from_row(int row)
{
    int wid = smith_ui_term_wid();
    int hgt = smith_ui_term_hgt();

    if (row < 0)
        row = 0;
    if (row >= hgt)
        return;

    for (int y = row; y < hgt; y++)
        Term_erase(0, y, wid);
}

static int smith_ui_used_bottom_row(void)
{
    if (!Term || !Term->scr)
        return 0;

    for (int y = smith_ui_term_hgt() - 1; y >= 0; y--)
    {
        for (int x = 0; x < smith_ui_term_wid(); x++)
        {
            if ((Term->scr->c[y][x] != ' ')
                || (Term->scr->a[y][x] != Term->attr_blank)
                || (Term->scr->story[y][x] != 0))
            {
                return y;
            }
        }
    }

    return 0;
}

static int smith_ui_description_row(void)
{
    int hgt = smith_ui_term_hgt();
    int row = MAX(
        smith_ui_used_bottom_row() + 1, hgt - smith_ui_preferred_desc_lines());
    int min_lines = smith_ui_show_lore() ? 2 : 1;

    if ((row >= hgt) || ((hgt - row) < min_lines))
        return -1;

    return row;
}

static int smith_ui_weight_col(void)
{
    int col = smith_ui_cost_col() - 10;

    if (col <= COL_SMT2 + 16)
        return -1;

    return col;
}

static void smith_ui_put_cost_line(int index0, byte attr, cptr text)
{
    Term_putstr(smith_ui_cost_col() + 2, smith_ui_cost_item_row(index0), -1,
        attr, text);
}

#define COL_SMT3 (smith_ui_secondary_col())
#define COL_SMT4 (smith_ui_cost_col())

/*
 * A list of tvals and their textual names
 */
typedef struct smithing_flag_cat
{
    int category;
    cptr desc;
} smithing_flag_cat;

#define CAT_STAT 1
#define CAT_SUST 2
#define CAT_SKILL 3
#define CAT_MEL 4
#define CAT_SLAY 5
#define CAT_RES 6
#define CAT_MISC 7

#define MAX_CATS 7

#define MAX_SMITHING_FLAGS (32 * 4)

static const smithing_flag_cat smithing_flag_cats[]
    = { { CAT_STAT, "Stat bonuses" }, { CAT_SUST, "Sustains" },
          { CAT_SKILL, "Skill bonuses" }, { CAT_MEL, "Melee powers" },
          { CAT_SLAY, "Slays" }, { CAT_RES, "Resistances" },
          { CAT_MISC, "Misc" } };

/*
 * A structure to hold a flag and its smithing category
 */
typedef struct smithing_flag_desc
{
    int category;
    u32b flag;
    int flagset;
    cptr desc;
} smithing_flag_desc;

/*
 * A list of tvals and their textual names
 */
static const smithing_flag_desc smithing_flag_types[] = { { CAT_STAT, TR1_STR,
                                                              1, "Str bonus" },
    { CAT_STAT, TR1_DEX, 1, "Dex bonus" },
    { CAT_STAT, TR1_CON, 1, "Con bonus" },
    { CAT_STAT, TR1_GRA, 1, "Gra bonus" },
    { CAT_STAT, TR1_NEG_STR, 1, "Str penalty" },
    { CAT_STAT, TR1_NEG_DEX, 1, "Dex penalty" },
    { CAT_STAT, TR1_NEG_CON, 1, "Con penalty" },
    { CAT_STAT, TR1_NEG_GRA, 1, "Gra penalty" },
    { CAT_SKILL, TR1_ARC, 1, "Archery" }, { CAT_SKILL, TR1_STL, 1, "Stealth" },
    { CAT_SKILL, TR1_PER, 1, "Perception" }, { CAT_SKILL, TR1_WIL, 1, "Will" },
    { CAT_SKILL, TR1_SMT, 1, "Smithing" }, { CAT_SKILL, TR1_SNG, 1, "Song" },
    { CAT_MISC, TR1_DAMAGE_SIDES, 1, "Damage bonus" },
    { CAT_MISC, TR2_LIGHT, 2, "Light" },
    { CAT_MISC, TR2_SLOW_DIGEST, 2, "Sustenance" },
    { CAT_MISC, TR2_REGEN, 2, "Regeneration" },
    { CAT_MISC, TR2_SEE_INVIS, 2, "See Invisible" },
    { CAT_MISC, TR2_FREE_ACT, 2, "Free Action" },
    { CAT_MISC, TR2_SPEED, 2, "Speed" },
    { CAT_MISC, TR2_RADIANCE, 2, "Radiance" },
    { CAT_MISC, TR3_CHEAT_DEATH, 3, "Cheat Death" },
    { CAT_MISC, TR3_STAND_FAST, 3, "Stand Fast" },
    { CAT_MISC, TR3_AVOID_TRAPS, 3, "Avoid Traps" },
    { CAT_MISC, TR3_MEDIC, 3, "Medicine Bonus" },
    { CAT_MISC, TR4_PROT_FIRE, 4, "Protection vs Fire" },
    { CAT_MISC, TR4_PROT_COLD, 4, "Protection vs Cold" },
    { CAT_MISC, TR4_PROT_POIS, 4, "Protection vs Poison" },
    { CAT_MISC, TR4_PROT_DARK, 4, "Protection vs Darkness" },
    { CAT_MEL, TR1_TUNNEL, 1, "Tunneling Bonus" },
    { CAT_MEL, TR1_SHARPNESS, 1, "Sharpness" },
    { CAT_MEL, TR1_SHARPNESS2, 1, "Sharpness2" },
    { CAT_MEL, TR1_VAMPIRIC, 1, "Vampiric" },
    { CAT_MEL, TR3_ACCURATE, 3, "Accurate" },
    { CAT_SLAY, TR1_SLAY_ORC, 1, "Slay Orc" },
    { CAT_SLAY, TR1_SLAY_TROLL, 1, "Slay Troll" },
    { CAT_SLAY, TR1_SLAY_WOLF, 1, "Slay Wolf" },
    { CAT_SLAY, TR1_SLAY_SPIDER, 1, "Slay Spider" },
    { CAT_SLAY, TR1_SLAY_UNDEAD, 1, "Slay Undead" },
    { CAT_SLAY, TR1_SLAY_RAUKO, 1, "Slay Rauko" },
    { CAT_SLAY, TR1_SLAY_DRAGON, 1, "Slay Dragon" },
    { CAT_SLAY, TR4_SLAY_SERPENT, 4, "Slay Serpent" },
    { CAT_SLAY, TR4_SLAY_VAMPIRE, 4, "Slay Vampire" },
    { CAT_SLAY, TR4_SLAY_HORROR, 4, "Slay Horror" },
    { CAT_SLAY, TR4_SLAY_CAT, 4, "Slay Cat" },
    { CAT_SLAY, TR4_SLAY_GIANT, 4, "Slay Giant" },
    { CAT_SLAY, TR1_BRAND_COLD, 1, "Brand with Cold" },
    { CAT_SLAY, TR1_BRAND_FIRE, 1, "Brand with Fire" },
    { CAT_SLAY, TR1_BRAND_POIS, 1, "Brand with Poison" },
    { CAT_SUST, TR2_SUST_STR, 2, "Sustain Str" },
    { CAT_SUST, TR2_SUST_DEX, 2, "Sustain Dex" },
    { CAT_SUST, TR2_SUST_CON, 2, "Sustain Con" },
    { CAT_SUST, TR2_SUST_GRA, 2, "Sustain Gra" },
    { CAT_RES, TR2_RES_COLD, 2, "Resist Cold" },
    { CAT_RES, TR2_RES_FIRE, 2, "Resist Fire" },
    { CAT_RES, TR2_RES_POIS, 2, "Resist Poison" },
    { CAT_RES, TR2_RES_BLEED, 2, "Resist Bleeding" },
    { CAT_RES, TR2_RES_FEAR, 2, "Resist Fear" },
    { CAT_RES, TR2_RES_BLIND, 2, "Resist Blindness" },
    { CAT_RES, TR2_RES_CONFU, 2, "Resist Confusion" },
    { CAT_RES, TR2_RES_STUN, 2, "Resist Stunning" },
    { CAT_RES, TR2_RES_HALLU, 2, "Resist Hallucination" }, { 0, 0, 0, "" } };

/*
 * Artifice (custom artefact) bonus limits.
 *
 * When smithing a custom artefact, the item's max values from the R: line
 * are extended by these per-category bonuses.  All artefact-specific limits
 * live in this single table so they are easy to find and tune.
 *
 * 'bonus' fields are ADDED to the normal max (e.g. weapon att = max_att + 4).
 * 'floor' fields set a MINIMUM artefact max (e.g. rings always reach att 4).
 * The result is: artefact_max = max(normal_max + ego + bonus, floor).
 */

/* Forward declarations for data-driven smithing limit functions */
static void smithing_ego_bonus_sums(const object_type* o_ptr,
    int* max_att_sum, int* max_att_min_inc,
    int* to_ds_sum, int* to_ds_min_inc,
    int* max_evn_sum, int* max_evn_min_inc,
    int* to_ps_sum, int* to_ps_min_inc,
    int* max_pval_sum, int* max_pval_min_inc);
int att_max(void);
int att_min(void);
int ds_max(void);
int ds_min(void);
int evn_max(void);
int evn_min(void);
int ps_max(void);
int ps_min(void);

typedef struct
{
    int att_bonus;
    int att_floor;   /* 0 = unused */
    int ds_bonus;
    int evn_bonus;
    int evn_floor;   /* 0 = unused */
    int ps_bonus;
    int ps_floor;    /* 0 = unused */
    int pval_bonus;
} artifice_limits_t;

/* Indexed by a small enum — looked up via artifice_bonus_for(). */
enum {
    ARTIFICE_ARROW,
    ARTIFICE_MELEE,     /* sword, polearm, hafted */
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
    /*               att_b att_f ds_b evn_b evn_f ps_b ps_f pval_b */
    /* ARROW   */  {  8,    0,    0,   0,    0,    0,   0,   0  },
    /* MELEE   */  {  4,    0,    2,   1,    0,    0,   0,   4  },
    /* BOW     */  {  4,    0,    2,   0,    0,    0,   0,   4  },
    /* DIGGING */  {  4,    0,    2,   0,    0,    0,   0,   4  },
    /* ARMOR   */  {  1,    0,    0,   1,    0,    2,   0,   4  },
    /* GLOVES  */  {  2,    0,    0,   1,    0,    2,   0,   4  },
    /* RING    */  {  0,    4,    0,   0,    4,    0,   0,   4  },
    /* AMULET  */  {  0,    0,    0,   0,    0,    0,   3,   4  },
    /* DEFAULT */  {  0,    0,    0,   0,    0,    0,   0,   4  },
};

static int artifice_category(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:      return ARTIFICE_ARROW;
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED:     return ARTIFICE_MELEE;
    case TV_BOW:        return ARTIFICE_BOW;
    case TV_DIGGING:    return ARTIFICE_DIGGING;
    case TV_GLOVES:     return ARTIFICE_GLOVES;
    case TV_BOOTS:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:       return ARTIFICE_ARMOR;
    case TV_RING:       return ARTIFICE_RING;
    case TV_AMULET:     return ARTIFICE_AMULET;
    default:            return ARTIFICE_DEFAULT;
    }
}

static const artifice_limits_t* artifice_bonus_for(const object_type* o_ptr)
{
    return &artifice_table[artifice_category(o_ptr)];
}

/*
 * Determines whether the attack bonus of an item is eligible for modification.
 */
int att_valid(void)
{
    return att_max() > att_min();
}

/*
 * Determines the maximum legal attack bonus for an item.
 * Uses data-driven max_att from object.txt R: lines, ego sums,
 * and the artifice table for custom artefacts.
 */
int att_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_att_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, &max_att_sum, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int att = k_ptr->max_att;
    att += max_att_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        att += al->att_bonus;
        if (al->att_floor > att)
            att = al->att_floor;
    }

    return (att);
}

/*
 * Determines the minimum legal attack bonus for an item.
 */
int att_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_att_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, &max_att_min_inc, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int att = k_ptr->att;
    att += max_att_min_inc;
    return (att);
}

/*
 * Determines whether the damage sides of an item is eligible for modification.
 */
int ds_valid(void)
{
    return ds_max() > ds_min();
}

/*
 * Determines the maximum legal damage sides for an item.
 */
int ds_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ds_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, &to_ds_sum, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int ds = k_ptr->max_ds;
    ds += to_ds_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        ds += al->ds_bonus;
    }

    return (ds);
}

/*
 * Determines the minimum legal damage sides for an item.
 */
int ds_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ds_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, &to_ds_min_inc, NULL, NULL, NULL, NULL, NULL, NULL);

    int ds = k_ptr->ds;
    ds += to_ds_min_inc;

    /* Never allow weapons to reach 0-sided damage. */
    if (k_ptr->dd > 0 && ds < 1)
        ds = 1;

    return (ds);
}

/*
 * Determines whether the evasion bonus of an item is eligible for modification.
 */
int evn_valid(void)
{
    return evn_max() > evn_min();
}

/*
 * Determines the maximum legal evasion bonus for an item.
 */
int evn_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_evn_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, &max_evn_sum, NULL, NULL, NULL, NULL, NULL);

    int evn = k_ptr->max_evn;
    evn += max_evn_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        evn += al->evn_bonus;
        if (al->evn_floor > evn)
            evn = al->evn_floor;
    }

    return (evn);
}

/*
 * Determines the minimum legal evasion bonus for an item.
 */
int evn_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_evn_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, &max_evn_min_inc, NULL, NULL, NULL, NULL);

    int evn = k_ptr->evn;
    evn += max_evn_min_inc;
    return (evn);
}

/*
 * Determines whether the protection sides of an item is eligible for
 * modification.
 */
int ps_valid(void)
{
    return ps_max() > ps_min();
}

/*
 * Determines the maximum legal protection sides for an item.
 */
int ps_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ps_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, &to_ps_sum, NULL, NULL, NULL);

    int ps = k_ptr->max_ps;
    ps += to_ps_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        ps += al->ps_bonus;
        if (al->ps_floor > ps)
            ps = al->ps_floor;
    }

    return (ps);
}

/*
 * Determines the minimum legal protection sides for an item.
 */
int ps_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ps_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &to_ps_min_inc, NULL, NULL);

    int ps = k_ptr->ps;
    ps += to_ps_min_inc;
    return (ps);
}

static bool smithing_variable_protection_dice(const object_type* o_ptr)
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
    size_t i;

    if (!smithing_variable_protection_dice(o_ptr))
        return -1;

    for (i = 0; i < N_ELEMENTS(smithing_amulet_protection_combos); i++)
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

    if (combo_idx < 0 || combo_idx >= (int)N_ELEMENTS(smithing_amulet_protection_combos))
        return;

    o_ptr->pd = smithing_amulet_protection_combos[combo_idx].pd;
    o_ptr->ps = smithing_amulet_protection_combos[combo_idx].ps;
}

static bool smithing_can_increase_protection(const object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return false;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        if (o_ptr->ps < ps_max())
            return true;

        return false;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx >= 0)
        return combo_idx < (int)N_ELEMENTS(smithing_amulet_protection_combos) - 1;

    return (o_ptr->pd <= 1) && (o_ptr->ps < 1);
}

static bool smithing_can_decrease_protection(const object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return false;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        if (o_ptr->ps > ps_min())
            return true;

        return false;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx > 0)
        return true;

    return combo_idx == 0 && ps_min() < 1;
}

static void smithing_increase_protection(object_type* o_ptr)
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

static void smithing_decrease_protection(object_type* o_ptr)
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

/*
 * Determines whether the pval of an item is eligible for modification.
 */
int pval_valid(void)
{
    u32b f1, f2, f3;

    object_flags(smith_o_ptr, &f1, &f2, &f3);

    return (f1 & (TR1_PVAL_MASK));
}

/*
 * Determines the maximum legal pval for an item.
 * Uses data-driven max_pval from object.txt R: lines, ego sums,
 * and the artifice table for custom artefacts.
 */
int pval_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    u32b f1, f2, f3;
    int max_pval_sum = 0;
    int max_pval_min_inc = 0;

    object_flags(smith_o_ptr, &f1, &f2, &f3);
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &max_pval_sum, &max_pval_min_inc);

    /* Start with the data-driven max from R: line */
    int pval = k_ptr->max_pval;

    /* Artefact bonus from the centralized artifice table */
    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        pval += al->pval_bonus;
    }

    /* Ego items have pvals limited by their 'special.txt' C: entries. */
    if (cursed_p(smith_o_ptr))
    {
        pval -= max_pval_min_inc;
    }
    else
    {
        pval += max_pval_sum;
    }

    return (pval);
}

/*
 * Determines the minimum legal pval for an item.
 * Accounts for ego min_pval requirements from special.txt C: line.
 */
int pval_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int base_min = k_ptr->pval;

    /* Check both prefix and suffix egos for min_pval requirements */
    byte egos[2] = { object_ego_prefix(smith_o_ptr), object_ego_suffix(smith_o_ptr) };
    for (int i = 0; i < 2; i++)
    {
        byte e_idx = egos[i];
        if (!e_idx)
            continue;

        ego_item_type* e_ptr = &e_info[e_idx];
        if (e_ptr->min_pval > 0)
        {
            /* Ego requires a minimum pval contribution */
            base_min += e_ptr->min_pval;
        }
        else if (e_ptr->max_pval > 0)
        {
            /* Default: at least +1 pval when ego grants pval */
            base_min += 1;
        }
    }

    return base_min;
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
        if (!e_idx)
            continue;

        ego_item_type* e_ptr = &e_info[e_idx];
        int max_att = (int)(int8_t)e_ptr->max_att;
        int to_ds = (int)(int8_t)e_ptr->to_ds;
        int max_evn = (int)(int8_t)e_ptr->max_evn;
        int to_ps = (int)(int8_t)e_ptr->to_ps;

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

/*
 * Determines whether the weight of an item is eligible for modification.
 */
int wgt_valid(void)
{
    switch (smith_o_ptr->tval)
    {
    case TV_ARROW:
    case TV_RING:
    case TV_AMULET:
    case TV_LIGHT:
    case TV_HORN:
    {
        return (false);
    }
    }

    return (true);
}

/*
 * Determines the maximum legal weight for an item.
 */
int wgt_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int weight = div_round(k_ptr->weight, 2) * 3;
    return (weight);
}

/*
 * Determines the minimum legal weight for an item.
 */
int wgt_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int weight = div_round(k_ptr->weight, 3) * 2;
    return (weight);
}

/*
 * Moves the light blue highlighted letter.
 */
void move_displayed_highlight(
    int old_highlight, byte old_attr, int new_highlight, int col)
{
    char buf[80];

    // remove highlight from the old label
    strnfmt(buf, 80, "%c)", (char)'a' + old_highlight - 1);
    Term_putstr(col, old_highlight + 1, -1, old_attr, buf);

    // highlight the new label
    strnfmt(buf, 80, "%c)", (char)'a' + new_highlight - 1);
    Term_putstr(col, new_highlight + 1, -1, TERM_L_BLUE, buf);
}

void wipe_object_description(void)
{
    if (smith_ui_last_desc_row >= 0)
        smith_ui_clear_from_row(smith_ui_last_desc_row);

    smith_ui_reset_description_state();
}

/*
 * Displays the object's name and description at the bottom of the screen.
 */
void prt_object_description(void)
{
    char o_desc[80];
    char buf[80];
    int display_flag;
    int desc_row;
    int desc_col;
    int desc_width;

    wipe_object_description();

    // abort if there is no object to display
    if (smith_o_ptr->tval == 0)
        return;

    desc_row = smith_ui_description_row();
    if (desc_row < 0)
        return;

    smith_ui_last_desc_row = desc_row;
    smith_ui_clear_from_row(desc_row);

    desc_col = smith_ui_desc_col();
    desc_width = smith_ui_term_wid() - desc_col;

    if (smith_o_ptr->number > 1)
        display_flag = true;
    else
        display_flag = false;

    object_desc(o_desc, sizeof(o_desc), smith_o_ptr, display_flag, 2);

    SDL_strlcat(o_desc,
        format("   %d.%d lb", smith_o_ptr->weight * smith_o_ptr->number / 10,
            (smith_o_ptr->weight * smith_o_ptr->number) % 10),
        sizeof(o_desc));

    if (p_ptr->smithing_leftover)
    {
        strnfmt(buf, sizeof(buf), "In progress: %d turns left",
            p_ptr->smithing_leftover);
        Term_putstr(desc_col, desc_row, desc_width, TERM_L_BLUE, buf);
        desc_row++;
        if (desc_row >= smith_ui_term_hgt())
            return;
    }

    Term_putstr(desc_col, desc_row, desc_width, TERM_L_WHITE, o_desc);
    desc_row++;
    if (desc_row >= smith_ui_term_hgt())
        return;

    Term_gotoxy(desc_col, desc_row);

    /* Set hooks for character dump */
    object_info_out_flags = object_flags;

    /* Set the indent/wrap */
    text_out_indent = desc_col;
    text_out_wrap = smith_ui_term_wid() - 1;

    text_out_hook = text_out_to_screen;

    if (smith_ui_show_lore())
    {
        text_out_c(TERM_WHITE, k_text + k_info[smith_o_ptr->k_idx].text);

        if ((k_text + k_info[smith_o_ptr->k_idx].text)[0] != '\0')
            text_out(" ");
    }

    /* Dump only the mechanical info on short screens. */
    if (object_info_out(smith_o_ptr) && smith_ui_show_lore())
        text_out("\n");

    /* Reset indent/wrap */
    text_out_indent = 0;
    text_out_wrap = 0;
}

/*
 * Determines whether an item is too difficult to make.
 */
void prt_object_difficulty(void)
{
    int dif;
    char buf[80];
    int turn_multiplier = 10;
    int costs = 0;
    byte attr;
    bool affordable = true;
    bool compact = smith_ui_compact_width();
    int cost_title_row = smith_ui_cost_title_row();

    Term_putstr(COL_SMT4, 3, -1, TERM_WHITE, "                 ");

    // abort if there is no object to display
    if (smith_o_ptr->tval == 0)
        return;

    // display difficulty information
    if (too_difficult(smith_o_ptr))
        attr = TERM_L_DARK;
    else
        attr = TERM_SLATE;

    Term_putstr(COL_SMT4, 2, -1, attr, "Difficulty:");

    // change colour if smithing drain is required
    if ((smithing_cost.drain > 0)
        && (smithing_cost.drain <= p_ptr->skill_base[S_SMT]))
    {
        attr = TERM_BLUE;
    }

    // calculate difficulty (and costs)
    dif = object_difficulty(smith_o_ptr);

    sprintf(buf, "%d", dif);
    Term_putstr(COL_SMT4 + 2, 4, -1, attr, buf);

    if (compact)
        strnfmt(buf, sizeof(buf), "/%d",
            p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
    else
        strnfmt(buf, sizeof(buf), "(max %d)",
            p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
    Term_putstr(COL_SMT4 + (compact ? 4 : 5), 4, -1, TERM_L_DARK, buf);

    // display cost information
    if (smithing_cost.weaponsmith)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Weaponsmith");
        costs++;
    }
    if (smithing_cost.armoursmith)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Armoursmith");
        costs++;
    }
    if (smithing_cost.jeweller)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Jeweller");
        costs++;
    }
    if (smithing_cost.enchantment)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Enchantment");
        costs++;
    }
    if (smithing_cost.artifice)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Artifice");
        costs++;
    }
    if (smithing_cost.alloy_mastery)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Alloy Mastery");
        costs++;
    }
    if (smithing_cost.uses > 0)
    {
        if (forge_uses(p_ptr->py, p_ptr->px) >= smithing_cost.uses)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        if (smithing_cost.uses == 1)
        {
            sprintf(buf, "%d Use", smithing_cost.uses);
        }
        else
        {
            sprintf(buf, "%d Uses", smithing_cost.uses);
        }
        if (compact)
        {
            strnfmt(buf, sizeof(buf), "%d/%d uses", smithing_cost.uses,
                forge_uses(p_ptr->py, p_ptr->px));
            smith_ui_put_cost_line(costs, attr, buf);
        }
        else
        {
            smith_ui_put_cost_line(costs, attr, buf);
            strnfmt(buf, sizeof(buf), "(of %d)", forge_uses(p_ptr->py, p_ptr->px));
            Term_putstr(COL_SMT4 + 9, smith_ui_cost_item_row(costs), -1,
                TERM_L_DARK, buf);
        }
        costs++;
    }
    if (smithing_cost.drain > 0)
    {
        if (smithing_cost.drain <= p_ptr->skill_base[S_SMT])
        {
            attr = TERM_BLUE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Smithing", smithing_cost.drain);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.mithril > 0)
    {
        if (smithing_cost.mithril <= mithril_carried())
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, compact ? "%d.%d lb Mith" : "%d.%d lb Mithril",
            smithing_cost.mithril / 10,
            smithing_cost.mithril % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.star_iron > 0)
    {
        if (smithing_cost.star_iron <= star_iron_carried())
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, compact ? "%d.%d lb StIron" : "%d.%d lb Star Iron",
            smithing_cost.star_iron / 10,
            smithing_cost.star_iron % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.str > 0)
    {
        if (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - smithing_cost.str
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Str", smithing_cost.str);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.dex > 0)
    {
        if (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - smithing_cost.dex
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Dex", smithing_cost.dex);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.con > 0)
    {
        if (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - smithing_cost.con
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Con", smithing_cost.con);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.gra > 0)
    {
        if (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - smithing_cost.gra
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Gra", smithing_cost.gra);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.exp > 0)
    {
        if (p_ptr->new_exp >= smithing_cost.exp)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Exp", smithing_cost.exp);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        turn_multiplier /= 2;
    }

    attr = TERM_SLATE;
    sprintf(buf, "%d Turns", MAX(10, dif * turn_multiplier));
    smith_ui_put_cost_line(costs, attr, buf);
    costs++;

    // if (costs == 0)
    //{
    //	Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_SLATE, "-");
    //}

    // display cost title
    if (affordable)
        attr = TERM_SLATE;
    else
        attr = TERM_L_DARK;
    Term_putstr(COL_SMT4, cost_title_row, -1, attr, "Cost:");
}

/*
 * Checks whether you can pay the costs in terms of ability points and
 * experience needed to make the object.
 */
typedef struct reforge_preview_type
{
    int scaled_difficulty;
    int raw_delta_difficulty;
    int turns;
    smithing_cost_type cost;
    bool affordable;
} reforge_preview_type;

static void smithing_cost_reset_local(smithing_cost_type* cost)
{
    if (!cost)
        return;

    memset(cost, 0, sizeof(*cost));
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

    has_noble = ((f4 & TR4_NOBLE_ITEM) != 0);
    has_evil = ((f4 & TR4_EVIL_ITEM) != 0);

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

static bool ego_forbids_prefix_combo(int e_idx)
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
    int j;

    if (!o_ptr || !o_ptr->k_idx || !e_ptr)
        return false;

    for (j = 0; j < EGO_TVALS_MAX; j++)
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

static bool smith_ego_can_apply_to_object(const object_type* o_ptr, int e_idx,
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

static bool ego_prefix_can_apply_to_object(const object_type* o_ptr, int e_idx)
{
    return smith_ego_can_apply_to_object(o_ptr, e_idx, 0, 0, true);
}

static bool object_can_reforge_prefix(const object_type* o_ptr)
{
    int i;

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

    for (i = 1; i < z_info->e_max; i++)
    {
        if (ego_prefix_can_apply_to_object(o_ptr, i))
            return true;
    }

    return false;
}

static int find_reforge_target_item(void)
{
    int i;

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;
        if (object_is_damaged_item(o_ptr) || object_can_reforge_prefix(o_ptr))
            return i;
    }

    return -1;
}

static void smith_eval_object(const object_type* src, int* difficulty,
    smithing_cost_type* cost_out)
{
    object_type smith_backup;
    smith_alloy_state alloy_backup = smith_alloy;
    smithing_cost_type smithing_cost_backup = smithing_cost;

    if (!src || !src->k_idx)
        return;

    object_copy(&smith_backup, smith_o_ptr);
    object_copy(smith_o_ptr, src);
    smith_clear_alloy_state(&smith_alloy);

    if (difficulty)
        *difficulty = object_difficulty(smith_o_ptr);
    else
        (void)object_difficulty(smith_o_ptr);

    if (cost_out)
        *cost_out = smithing_cost;

    object_copy(smith_o_ptr, &smith_backup);
    smith_alloy = alloy_backup;
    smithing_cost = smithing_cost_backup;
}

static bool smith_reforge_difficulty_affordable(int difficulty, int* drain_out)
{
    int effective_skill = p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px);

    if (drain_out)
        *drain_out = 0;

    if (p_ptr->have_ability[S_SPC][SPC_AULE])
    {
        int max_aule_difficulty = effective_skill + (p_ptr->skill_base[S_SMT] * 2);

        if (difficulty <= effective_skill)
            return true;
        if (difficulty <= max_aule_difficulty)
        {
            if (drain_out)
                *drain_out = (difficulty - effective_skill + 1) / 2;
            return true;
        }
        if (drain_out)
            *drain_out = p_ptr->skill_base[S_SMT] + (difficulty - max_aule_difficulty);
        return false;
    }

    if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE])
    {
        int max_masterpiece_difficulty = effective_skill + p_ptr->skill_base[S_SMT];

        if (difficulty <= effective_skill)
            return true;
        if (difficulty <= max_masterpiece_difficulty)
        {
            if (drain_out)
                *drain_out = difficulty - effective_skill;
            return true;
        }
        if (drain_out)
            *drain_out = p_ptr->skill_base[S_SMT] + (difficulty - max_masterpiece_difficulty);
        return false;
    }

    return (difficulty <= effective_skill);
}

static void smithing_cost_delta_positive(const smithing_cost_type* before,
    const smithing_cost_type* after, smithing_cost_type* delta)
{
    smithing_cost_reset_local(delta);

    if (!before || !after || !delta)
        return;

    delta->str = MAX(0, after->str - before->str);
    delta->dex = MAX(0, after->dex - before->dex);
    delta->con = MAX(0, after->con - before->con);
    delta->gra = MAX(0, after->gra - before->gra);
    delta->exp = MAX(0, after->exp - before->exp);
    delta->mithril = MAX(0, after->mithril - before->mithril);
    delta->star_iron = MAX(0, after->star_iron - before->star_iron);
}

static bool reforge_preview_build(const object_type* source, int prefix_idx,
    reforge_preview_type* preview)
{
    int before_diff = 0;
    int after_diff = 0;
    int turn_multiplier = 10;
    smithing_cost_type before_cost;
    smithing_cost_type after_cost;

    if (!source || !source->k_idx || !preview || prefix_idx <= 0)
        return false;

    memset(preview, 0, sizeof(*preview));
    smithing_cost_reset_local(&before_cost);
    smithing_cost_reset_local(&after_cost);

    smith_eval_object(source, &before_diff, &before_cost);

    object_copy(smith_o_ptr, source);
    object_set_ego_prefix(smith_o_ptr, prefix_idx);
    if (!object_apply_ego_affix(smith_o_ptr, prefix_idx, true))
        return false;

    smith_eval_object(smith_o_ptr, &after_diff, &after_cost);

    preview->raw_delta_difficulty = MAX(0, after_diff - before_diff);
    preview->scaled_difficulty = (preview->raw_delta_difficulty * 3 + 1) / 2;
    smithing_cost_delta_positive(&before_cost, &after_cost, &preview->cost);
    preview->cost.uses = 1;

    preview->affordable
        = smith_reforge_difficulty_affordable(
            preview->scaled_difficulty, &preview->cost.drain);

    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        preview->cost.str = 0;
        preview->cost.dex = 0;
        preview->cost.con = 0;
        preview->cost.gra = 0;
        preview->cost.exp = 0;
        turn_multiplier /= 2;
    }

    if ((preview->cost.str > 0)
        && (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - preview->cost.str
            < -5))
        preview->affordable = false;
    if ((preview->cost.dex > 0)
        && (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - preview->cost.dex
            < -5))
        preview->affordable = false;
    if ((preview->cost.con > 0)
        && (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - preview->cost.con
            < -5))
        preview->affordable = false;
    if ((preview->cost.gra > 0)
        && (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - preview->cost.gra
            < -5))
        preview->affordable = false;
    if (preview->cost.exp > p_ptr->new_exp)
        preview->affordable = false;
    if ((preview->cost.mithril > 0)
        && (preview->cost.mithril > mithril_carried()))
        preview->affordable = false;
    if ((preview->cost.star_iron > 0)
        && (preview->cost.star_iron > star_iron_carried()))
        preview->affordable = false;
    if (forge_uses(p_ptr->py, p_ptr->px) < preview->cost.uses)
        preview->affordable = false;
    if ((preview->cost.drain > 0)
        && (preview->cost.drain > p_ptr->skill_base[S_SMT]))
        preview->affordable = false;

    preview->turns = MAX(10, preview->scaled_difficulty * turn_multiplier);
    return true;
}

static void pay_smithing_cost_struct(const smithing_cost_type* cost)
{
    if (!cost)
        return;

    if (cost->str > 0)
        p_ptr->stat_drain[A_STR] -= cost->str;
    if (cost->dex > 0)
        p_ptr->stat_drain[A_DEX] -= cost->dex;
    if (cost->con > 0)
        p_ptr->stat_drain[A_CON] -= cost->con;
    if (cost->gra > 0)
        p_ptr->stat_drain[A_GRA] -= cost->gra;
    if (cost->exp > 0)
        p_ptr->new_exp -= cost->exp;
    if (cost->mithril > 0)
        use_mithril(cost->mithril);
    if (cost->star_iron > 0)
        use_star_iron(cost->star_iron);
    if (cost->uses > 0)
    {
        cave_feat[p_ptr->py][p_ptr->px] -= cost->uses;
        lite_spot(p_ptr->py, p_ptr->px);
    }
    if (cost->drain > 0)
        p_ptr->skill_base[S_SMT] -= cost->drain;

    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
}

// Determine default stack sizes for smithing-created items.
// Normal: arrows 24/18/12, daggers & spears 3/2/1 (normal/enchanted/artefact).
// This keeps arrows and throwable weapons in sensible stack counts.
static byte smith_default_stack_size(const object_type* o_ptr)
{
    bool is_arrow = (o_ptr->tval == TV_ARROW);
    bool is_spear = (o_ptr->tval == TV_POLEARM) && (o_ptr->sval == SV_SPEAR);
    bool is_dagger = (o_ptr->tval == TV_SWORD) && (o_ptr->sval == SV_DAGGER);

    if (!(is_arrow || is_spear || is_dagger))
    {
        return (o_ptr->number ? o_ptr->number : 1);
    }

    bool is_artifact = (o_ptr->name1 != 0);
    bool is_enchanted = (!is_artifact) && object_has_ego(o_ptr);

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

/*
 * Creates the base object (not in the dungeon, but just as a work in progress).
 */
void create_base_object(int tval, int sval)
{
    /* Wipe the object */
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);

    /* Prepare the item */
    object_prep(smith_o_ptr, lookup_kind(tval, sval));

    // set the pval to 1 if needed (and evasion/accuracy for rings)
    apply_magic_fake(smith_o_ptr);

    // use a default weight
    smith_o_ptr->weight = (&k_info[smith_o_ptr->k_idx])->weight;

    // display all attributes
    smith_o_ptr->ident |= (IDENT_KNOWN | IDENT_SPOIL);

    // Apply default stack sizes for smithing output
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);
}

/*
 * Performs the interface and selection work for the sval part of the base item
 * menu.
 */
int create_sval_menu_aux(int tval, int* highlight)
{
    char ch;
    int i, num;
    char buf[80];
    bool valid[20];
    int sval[20];
    int list_col = COL_SMT3;

    // clear the right of the screen
    wipe_screen_from(smith_ui_compact_width() ? list_col : COL_SMT4);

    /* We have to search the whole itemlist. */
    for (num = 0, i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];
        char name[80];

        /* Analyze matching items */
        if (k_ptr->tval == tval)
        {
            /* Skip instant artefact item types */
            if (k_ptr->flags3 & (TR3_INSTA_ART))
                continue;
            if (k_ptr->flags4 & TR4_EVIL_ITEM)
                continue;

            /* Skip certain item types that cannot be made */
            if (k_ptr->flags3 & (TR3_NO_SMITHING))
            {
                bool allow_override = false;
                
                /* Check for specific character unique flag and sval overrides */
                if ((c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_EOL) && 
                    (k_ptr->tval == TV_SOFT_ARMOR) && (k_ptr->sval == SV_ARMOUR_OF_GALVORN))
                {
                    allow_override = true;
                }
                
                if (!allow_override)
                    continue;
            }

            /* Get the "name" of object "i" */
            strip_name(name, i);

            // make a simple version of the object
            create_base_object(tval, k_ptr->sval);

            // Check whether it is a valid choice for creating
            if (affordable(smith_o_ptr))
                valid[num] = true;
            else
                valid[num] = false;

            /* Print it */
            strnfmt(buf, 80, "%c) %s", (char)'a' + num, name);
            Term_putstr(list_col, smith_ui_dense_row(num), -1,
                valid[num] ? TERM_WHITE : TERM_SLATE, buf);

            /* Remember the object sval */
            sval[num] = k_ptr->sval;

            // count the applicable items
            num++;
        }
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(list_col, smith_ui_dense_highlight_row(*highlight), -1,
        TERM_L_BLUE, buf);

    // make a simple version of the object
    create_base_object(tval, sval[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, smith_ui_dense_highlight_row(*highlight));

    /* Get key (while allowing menu commands) */
    ch = smith_ui_inkey_with_wait_reason();

    if ((ch >= 'a') && (ch <= (char)'a' + MAX_SMITHING_TVALS - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        // make a simple version of the object
        create_base_object(tval, sval[*highlight - 1]);

        return (*highlight);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    return (0);
}

/*
 * Displays a menu for choosing a base item's sval.
 */
bool create_sval_menu(int tval)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;
    bool completed = false;

    /* Save screen */
    screen_save();

    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        choice = create_sval_menu_aux(tval, &highlight);

        if (choice >= 1)
        {
            leave_menu = true;
            completed = true;
        }
        else if (choice == -1)
        {
            /* Wipe the object */
            object_wipe(smith_o_ptr);
            smith_clear_alloy_state(&smith_alloy);

            leave_menu = true;
        }
    }

    /* Load screen */
    screen_load();

    return (completed);
}

/*
 * Performs the interface and selection work for the tval part of the base item
 * menu.
 */
int create_tval_menu_aux(int* highlight)
{
    char ch;
    int i;
    char buf[80];
    bool valid[MAX_SMITHING_TVALS];
    byte valid_attr = TERM_WHITE; // default to soothe compilation warnings

    // clear the right of the screen
    wipe_screen_from(COL_SMT2);

    // clear bottom of the screen
    wipe_object_description();

    /* Wipe the smithing object */
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);

    for (i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        strnfmt(buf, 80, "%c) %s", (char)'a' + i, smithing_tvals[i].desc);

        if (smithing_tvals[i].category == CAT_WEAPON)
        {
            valid[i] = true;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
                ? TERM_WHITE
                : TERM_RED;
        }
        if (smithing_tvals[i].category == CAT_ARMOUR)
        {
            valid[i] = true;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
                ? TERM_WHITE
                : TERM_RED;
        }
        if (smithing_tvals[i].category == CAT_JEWELRY)
        {
            valid[i] = true;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_JEWELLER] ? TERM_WHITE
                                                                    : TERM_RED;
        }

        Term_putstr(COL_SMT2, smith_ui_dense_row(i), -1,
            valid[i] ? valid_attr : TERM_L_DARK, buf);
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT2, smith_ui_dense_highlight_row(*highlight), -1,
        TERM_L_BLUE, buf);

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, smith_ui_dense_highlight_row(*highlight));

    /* Get key (while allowing menu commands) */
    ch = smith_ui_inkey_with_wait_reason();

    // choose an option by letter
    if ((ch >= 'a') && (ch <= (char)'a' + MAX_SMITHING_TVALS - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        strnfmt(buf, 80, "%c)", (char)'a' + old_highlight - 1);
        Term_putstr(COL_SMT2, smith_ui_dense_highlight_row(old_highlight), -1,
            valid[old_highlight - 1] ? TERM_WHITE : TERM_L_DARK, buf);
        strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
        Term_putstr(COL_SMT2, smith_ui_dense_highlight_row(*highlight), -1,
            TERM_L_BLUE, buf);

        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = MAX_SMITHING_TVALS;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < MAX_SMITHING_TVALS)
            (*highlight)++;
        else if (*highlight == MAX_SMITHING_TVALS)
            *highlight = 1;
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        return (-1);
    }

    return (0);
}

/*
 * Displays a menu for choosing a base item's tval.
 */
void create_tval_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = create_tval_menu_aux(&highlight);

        if (choice >= 1)
        {
            if (create_sval_menu(smithing_tvals[choice - 1].tval))
            {
                leave_menu = true;
            }
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    enchant_then_numbers = false;

    /* Load screen */
    screen_load();
}

/*
 * Actually modifies the numbers on an item.
 */
static void smith_apply_stat_skill_flag_delta(object_type* o_ptr, u32b f1_before, u32b f1_after)
{
    if (!o_ptr)
        return;

    int pval = o_ptr->pval;
    int pval_abs = ABS(pval);

    bool before_str = (f1_before & (TR1_STR | TR1_NEG_STR)) != 0;
    bool after_str = (f1_after & (TR1_STR | TR1_NEG_STR)) != 0;
    if (!after_str)
    {
        o_ptr->stat_bonus[A_STR] = 0;
    }
    else if (!before_str)
    {
        o_ptr->stat_bonus[A_STR] = (f1_after & TR1_NEG_STR) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_STR) && !(f1_after & TR1_NEG_STR) && o_ptr->stat_bonus[A_STR] < 0)
        o_ptr->stat_bonus[A_STR] = -o_ptr->stat_bonus[A_STR];
    if ((f1_after & TR1_NEG_STR) && !(f1_after & TR1_STR) && o_ptr->stat_bonus[A_STR] > 0)
        o_ptr->stat_bonus[A_STR] = -o_ptr->stat_bonus[A_STR];

    bool before_dex = (f1_before & (TR1_DEX | TR1_NEG_DEX)) != 0;
    bool after_dex = (f1_after & (TR1_DEX | TR1_NEG_DEX)) != 0;
    if (!after_dex)
    {
        o_ptr->stat_bonus[A_DEX] = 0;
    }
    else if (!before_dex)
    {
        o_ptr->stat_bonus[A_DEX] = (f1_after & TR1_NEG_DEX) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_DEX) && !(f1_after & TR1_NEG_DEX) && o_ptr->stat_bonus[A_DEX] < 0)
        o_ptr->stat_bonus[A_DEX] = -o_ptr->stat_bonus[A_DEX];
    if ((f1_after & TR1_NEG_DEX) && !(f1_after & TR1_DEX) && o_ptr->stat_bonus[A_DEX] > 0)
        o_ptr->stat_bonus[A_DEX] = -o_ptr->stat_bonus[A_DEX];

    bool before_con = (f1_before & (TR1_CON | TR1_NEG_CON)) != 0;
    bool after_con = (f1_after & (TR1_CON | TR1_NEG_CON)) != 0;
    if (!after_con)
    {
        o_ptr->stat_bonus[A_CON] = 0;
    }
    else if (!before_con)
    {
        o_ptr->stat_bonus[A_CON] = (f1_after & TR1_NEG_CON) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_CON) && !(f1_after & TR1_NEG_CON) && o_ptr->stat_bonus[A_CON] < 0)
        o_ptr->stat_bonus[A_CON] = -o_ptr->stat_bonus[A_CON];
    if ((f1_after & TR1_NEG_CON) && !(f1_after & TR1_CON) && o_ptr->stat_bonus[A_CON] > 0)
        o_ptr->stat_bonus[A_CON] = -o_ptr->stat_bonus[A_CON];

    bool before_gra = (f1_before & (TR1_GRA | TR1_NEG_GRA)) != 0;
    bool after_gra = (f1_after & (TR1_GRA | TR1_NEG_GRA)) != 0;
    if (!after_gra)
    {
        o_ptr->stat_bonus[A_GRA] = 0;
    }
    else if (!before_gra)
    {
        o_ptr->stat_bonus[A_GRA] = (f1_after & TR1_NEG_GRA) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_GRA) && !(f1_after & TR1_NEG_GRA) && o_ptr->stat_bonus[A_GRA] < 0)
        o_ptr->stat_bonus[A_GRA] = -o_ptr->stat_bonus[A_GRA];
    if ((f1_after & TR1_NEG_GRA) && !(f1_after & TR1_GRA) && o_ptr->stat_bonus[A_GRA] > 0)
        o_ptr->stat_bonus[A_GRA] = -o_ptr->stat_bonus[A_GRA];

    bool before_mel = (f1_before & TR1_MEL) != 0;
    bool after_mel = (f1_after & TR1_MEL) != 0;
    if (!after_mel)
        o_ptr->skill_bonus[S_MEL] = 0;
    else if (!before_mel)
        o_ptr->skill_bonus[S_MEL] = pval;

    bool before_arc = (f1_before & TR1_ARC) != 0;
    bool after_arc = (f1_after & TR1_ARC) != 0;
    if (!after_arc)
        o_ptr->skill_bonus[S_ARC] = 0;
    else if (!before_arc)
        o_ptr->skill_bonus[S_ARC] = pval;

    bool before_stl = (f1_before & TR1_STL) != 0;
    bool after_stl = (f1_after & TR1_STL) != 0;
    if (!after_stl)
        o_ptr->skill_bonus[S_STL] = 0;
    else if (!before_stl)
        o_ptr->skill_bonus[S_STL] = pval;

    bool before_per = (f1_before & TR1_PER) != 0;
    bool after_per = (f1_after & TR1_PER) != 0;
    if (!after_per)
        o_ptr->skill_bonus[S_PER] = 0;
    else if (!before_per)
        o_ptr->skill_bonus[S_PER] = pval;

    bool before_wil = (f1_before & TR1_WIL) != 0;
    bool after_wil = (f1_after & TR1_WIL) != 0;
    if (!after_wil)
        o_ptr->skill_bonus[S_WIL] = 0;
    else if (!before_wil)
        o_ptr->skill_bonus[S_WIL] = pval;

    bool before_smt = (f1_before & TR1_SMT) != 0;
    bool after_smt = (f1_after & TR1_SMT) != 0;
    if (!after_smt)
        o_ptr->skill_bonus[S_SMT] = 0;
    else if (!before_smt)
        o_ptr->skill_bonus[S_SMT] = pval;

    bool before_sng = (f1_before & TR1_SNG) != 0;
    bool after_sng = (f1_after & TR1_SNG) != 0;
    if (!after_sng)
        o_ptr->skill_bonus[S_SNG] = 0;
    else if (!before_sng)
        o_ptr->skill_bonus[S_SNG] = pval;
}

void modify_numbers(int choice)
{
    switch (choice)
    {
    case SMT_NUM_MENU_I_ATT:
    {
        if ((smith_o_ptr->tval == TV_ARROW) && !smith_o_ptr->name1)
            smith_o_ptr->att += 3;
        else
            smith_o_ptr->att++;
        break;
    }
    case SMT_NUM_MENU_D_ATT:
    {
        if ((smith_o_ptr->tval == TV_ARROW) && !smith_o_ptr->name1)
            smith_o_ptr->att -= 3;
        else
            smith_o_ptr->att--;
        break;
    }
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

        smith_alloy_type next = SMITH_ALLOY_NONE;
        if (smith_alloy.type == SMITH_ALLOY_NONE)
            next = SMITH_ALLOY_MITHRIL;
        else if (smith_alloy.type == SMITH_ALLOY_MITHRIL)
            next = SMITH_ALLOY_STAR_IRON;
        else
            next = SMITH_ALLOY_NONE;

        smith_apply_alloy(smith_o_ptr, &smith_alloy, next);
        break;
    }
    case SMT_NUM_MENU_ALLOY_CLEAR:
        smith_remove_alloy_bonus(smith_o_ptr, &smith_alloy);
        break;
    }

    return;
}

/*
 * Performs the interface and selection work for the numbers menu.
 */
int numbers_menu_aux(int* highlight)
{
    int i;
    char ch;
    char buf[80];
    byte attr[SMT_NUM_MENU_MAX];
    bool valid[SMT_NUM_MENU_MAX];
    bool can_afford[SMT_NUM_MENU_MAX] = { false };

    // clear the right of the screen
    wipe_screen_from(COL_SMT2);

    memset(valid, 0, sizeof(valid));

    valid[SMT_NUM_MENU_I_ATT - 1]
        = att_valid() && (smith_o_ptr->att < att_max());
    valid[SMT_NUM_MENU_D_ATT - 1]
        = att_valid() && (smith_o_ptr->att > att_min());
    valid[SMT_NUM_MENU_I_DS - 1] = ds_valid() && (smith_o_ptr->ds < ds_max());
    valid[SMT_NUM_MENU_D_DS - 1] = ds_valid() && (smith_o_ptr->ds > ds_min());
    valid[SMT_NUM_MENU_I_EVN - 1]
        = evn_valid() && (smith_o_ptr->evn < evn_max());
    valid[SMT_NUM_MENU_D_EVN - 1]
        = evn_valid() && (smith_o_ptr->evn > evn_min());
    valid[SMT_NUM_MENU_I_PS - 1] = ps_valid() && smithing_can_increase_protection(smith_o_ptr);
    valid[SMT_NUM_MENU_D_PS - 1] = ps_valid() && smithing_can_decrease_protection(smith_o_ptr);
    valid[SMT_NUM_MENU_I_WGT - 1]
        = wgt_valid() && ((smith_o_ptr->weight + 5) <= wgt_max());
    valid[SMT_NUM_MENU_D_WGT - 1]
        = wgt_valid() && ((smith_o_ptr->weight - 5) >= wgt_min());
    {
        u32b f1, f2, f3;
        object_flags(smith_o_ptr, &f1, &f2, &f3);
        valid[SMT_NUM_MENU_EDIT_BONUSES - 1] = (f1 & (TR1_STR | TR1_NEG_STR | TR1_DEX
                                                     | TR1_NEG_DEX | TR1_CON
                                                     | TR1_NEG_CON | TR1_GRA
                                                     | TR1_NEG_GRA | TR1_MEL
                                                     | TR1_ARC | TR1_STL
                                                     | TR1_PER | TR1_WIL
                                                     | TR1_SMT | TR1_SNG
                                                     | TR1_DAMAGE_SIDES
                                                     | TR1_TUNNEL))
            != 0;
    }
    bool alloy_applicable = smith_alloy_applicable(smith_o_ptr);
    bool has_alloy_mastery = p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY];
    int alloy_weight = alloy_applicable ? smith_alloy_weight_required(smith_o_ptr) : 0;
    int mithril_have = mithril_carried();
    int star_iron_have = star_iron_carried();
    valid[SMT_NUM_MENU_ALLOY_CYCLE - 1] = alloy_applicable && has_alloy_mastery;
    valid[SMT_NUM_MENU_ALLOY_CLEAR - 1] = (smith_alloy.type != SMITH_ALLOY_NONE);

    // retrieve a super backup of the object
    object_copy(smith3_o_ptr, smith_o_ptr);
    smith3_alloy = smith_alloy;
    for (i = 0; i < SMT_NUM_MENU_MAX; i++)
    {
        if ((i == SMT_NUM_MENU_ALLOY_CYCLE - 1)
            || (i == SMT_NUM_MENU_ALLOY_CLEAR - 1)
            || (i == SMT_NUM_MENU_EDIT_BONUSES - 1))
        {
            can_afford[i] = valid[i];
            if (i == SMT_NUM_MENU_ALLOY_CYCLE - 1 && valid[i])
            {
                bool has_any_metal = (mithril_have >= alloy_weight)
                    || (star_iron_have >= alloy_weight);
                attr[i] = has_any_metal ? TERM_WHITE : TERM_SLATE;
            }
            else
            {
                attr[i] = valid[i] ? TERM_WHITE : TERM_L_DARK;
            }
            continue;
        }
        if (valid[i])
        {
            modify_numbers(i + 1);
            can_afford[i] = affordable(smith_o_ptr);

            // retrieve a super backup of the object
            object_copy(smith_o_ptr, smith3_o_ptr);
            smith_alloy = smith3_alloy;
        }

        attr[i] = valid[i] ? (can_afford[i] ? TERM_WHITE : TERM_SLATE)
                           : TERM_L_DARK;
    }

    Term_putstr(COL_SMT2, 2, -1, attr[SMT_NUM_MENU_I_ATT - 1],
        "a) increase attack bonus");
    Term_putstr(COL_SMT2, 3, -1, attr[SMT_NUM_MENU_D_ATT - 1],
        "b) decrease attack bonus");
    Term_putstr(COL_SMT2, 4, -1, attr[SMT_NUM_MENU_I_DS - 1],
        "c) increase damage sides");
    Term_putstr(COL_SMT2, 5, -1, attr[SMT_NUM_MENU_D_DS - 1],
        "d) decrease damage sides");
    Term_putstr(COL_SMT2, 6, -1, attr[SMT_NUM_MENU_I_EVN - 1],
        "e) increase evasion bonus");
    Term_putstr(COL_SMT2, 7, -1, attr[SMT_NUM_MENU_D_EVN - 1],
        "f) decrease evasion bonus");
    Term_putstr(COL_SMT2, 8, -1, attr[SMT_NUM_MENU_I_PS - 1],
        "g) increase protection");
    Term_putstr(COL_SMT2, 9, -1, attr[SMT_NUM_MENU_D_PS - 1],
        "h) decrease protection");
    Term_putstr(
        COL_SMT2, 10, -1, attr[SMT_NUM_MENU_I_WGT - 1], "i) increase weight");
    Term_putstr(
        COL_SMT2, 11, -1, attr[SMT_NUM_MENU_D_WGT - 1], "j) decrease weight");
    Term_putstr(COL_SMT2, 12, -1, attr[SMT_NUM_MENU_ALLOY_CYCLE - 1],
        "k) cycle alloy (none/mithril/star iron)");
    Term_putstr(COL_SMT2, 13, -1, attr[SMT_NUM_MENU_ALLOY_CLEAR - 1],
        "l) remove alloy bonus");
    Term_putstr(COL_SMT2, 14, -1, attr[SMT_NUM_MENU_EDIT_BONUSES - 1],
        "m) adjust special bonuses");
    if (alloy_applicable)
    {
        byte info_attr = has_alloy_mastery ? TERM_SLATE : TERM_L_DARK;
        if (!has_alloy_mastery)
        {
            strnfmt(buf, 80, "Alloy needs %d.%d lb metal (requires Alloy mastery)",
                alloy_weight / 10, alloy_weight % 10);
        }
        else
        {
            if (smith_alloy.type == SMITH_ALLOY_MITHRIL)
                info_attr = (mithril_have >= alloy_weight) ? TERM_SLATE : TERM_RED;
            else if (smith_alloy.type == SMITH_ALLOY_STAR_IRON)
                info_attr = (star_iron_have >= alloy_weight) ? TERM_SLATE : TERM_RED;
            strnfmt(buf, 80,
                "Alloy needs %d.%d lb (mithril %d.%d, star iron %d.%d)",
                alloy_weight / 10, alloy_weight % 10, mithril_have / 10,
                mithril_have % 10, star_iron_have / 10, star_iron_have % 10);
        }
        Term_putstr(COL_SMT2, 15, -1, info_attr, buf);
    }
    else if (!has_alloy_mastery)
    {
        Term_putstr(COL_SMT2, 15, -1, TERM_L_DARK,
            "Alloy requires Alloy mastery.");
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(2, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    ch = smith_ui_inkey_with_wait_reason();

    // choose an option by letter
    if ((ch >= 'a') && (ch <= (char)'a' + SMT_NUM_MENU_MAX - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        // move the light blue highlight
        move_displayed_highlight(
            old_highlight, attr[old_highlight - 1], *highlight, COL_SMT2);

        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = SMT_NUM_MENU_MAX;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < SMT_NUM_MENU_MAX)
            (*highlight)++;
        else if (*highlight == SMT_NUM_MENU_MAX)
            *highlight = 1;
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;
        return (*highlight);
    }

    return (0);
}

typedef enum
{
    SMT_BONUS_ENTRY_STAT = 0,
    SMT_BONUS_ENTRY_SKILL = 1,
    SMT_BONUS_ENTRY_SPECIAL = 2,
} smith_bonus_entry_kind;

typedef enum
{
    SMT_BONUS_SPECIAL_DAMAGE_SIDES = 0,
    SMT_BONUS_SPECIAL_TUNNEL = 1,
} smith_bonus_special_kind;

typedef struct
{
    smith_bonus_entry_kind kind;
    int index;
    u32b flag_pos;
    u32b flag_neg;
    u32b flag;
} smith_bonus_entry;

typedef struct
{
    smith_bonus_entry entry;
    int delta;
} smith_bonus_action;

static const char* smith_bonus_stat_name(int stat)
{
    switch (stat)
    {
    case A_STR:
        return "Strength";
    case A_DEX:
        return "Dexterity";
    case A_CON:
        return "Constitution";
    case A_GRA:
        return "Grace";
    default:
        return "Unknown";
    }
}

static const char* smith_bonus_special_name(int special)
{
    switch (special)
    {
    case SMT_BONUS_SPECIAL_DAMAGE_SIDES:
        return "Damage bonus";
    case SMT_BONUS_SPECIAL_TUNNEL:
        return "Tunneling";
    default:
        return "Unknown";
    }
}

static int smith_collect_bonus_entries(smith_bonus_entry* entries, int max_entries)
{
    u32b f1, f2, f3;
    int n = 0;

    if (!entries || max_entries <= 0)
        return 0;

    object_flags(smith_o_ptr, &f1, &f2, &f3);

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

    struct special_flag_map
    {
        int special;
        u32b flag;
    };

    static const struct special_flag_map special_flags[] = {
        { SMT_BONUS_SPECIAL_DAMAGE_SIDES, TR1_DAMAGE_SIDES },
        { SMT_BONUS_SPECIAL_TUNNEL, TR1_TUNNEL },
    };

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

static int smith_collect_bonus_actions(smith_bonus_action* actions, int max_actions)
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

static bool smith_adjust_bonus_entry(const smith_bonus_entry* entry, int delta)
{
    int max_bonus = pval_max();
    int floor_bonus = pval_min(); /* respect ego min_pval */
    int min_bonus = 0;
    int value = 0;

    if (!entry || !smith_o_ptr || delta == 0)
        return false;

    if (entry->kind == SMT_BONUS_ENTRY_STAT)
    {
        u32b f1, f2, f3;
        object_flags(smith_o_ptr, &f1, &f2, &f3);

        bool has_pos = (f1 & entry->flag_pos) != 0;
        bool has_neg = (f1 & entry->flag_neg) != 0;

        if (has_pos && has_neg)
        {
            min_bonus = -max_bonus;
        }
        else if (has_neg)
        {
            min_bonus = -max_bonus;
            max_bonus = 0;
        }
        else
        {
            /* Positive stat: honour ego min_pval as the lower bound */
            min_bonus = floor_bonus;
        }

        value = smith_o_ptr->stat_bonus[entry->index];
        int new_value = value + delta;
        if (new_value < min_bonus || new_value > max_bonus)
            return false;

        smith_o_ptr->stat_bonus[entry->index] = new_value;
        return true;
    }

    if (entry->kind == SMT_BONUS_ENTRY_SKILL)
    {
        /* Skill bonus: honour ego min_pval as the lower bound */
        value = smith_o_ptr->skill_bonus[entry->index];
        int new_value = value + delta;
        if (new_value < floor_bonus || new_value > max_bonus)
            return false;
        smith_o_ptr->skill_bonus[entry->index] = new_value;
        return true;
    }

    value = smith_o_ptr->pval;
    int new_value = value + delta;
    if (new_value < floor_bonus || new_value > max_bonus)
        return false;
    smith_o_ptr->pval = (s16b)new_value;
    return true;
}

static int smith_bonus_menu_aux(int* highlight)
{
    char ch;
    char buf[80];
    smith_bonus_action actions[26];
    bool valid[26] = { false };
    bool can_afford[26] = { false };
    byte attr[26];
    int num = smith_collect_bonus_actions(actions, (int)N_ELEMENTS(actions));
    const int first_row = 2;
    const int max_row = MAX_SMITHING_TVALS + 2;
    const int max_visible = max_row - first_row + 1;

    wipe_screen_from(COL_SMT2);

    Term_putstr(COL_SMT2, 1, -1, TERM_WHITE,
        "Adjust special bonuses (ESC to return)");

    if (num <= 0)
    {
        Term_putstr(COL_SMT2, 3, -1, TERM_L_DARK,
            "(No editable special bonuses on this item.)");
        Term_fresh();
        (void)smith_ui_inkey_with_wait_reason();
        return -1;
    }

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > num)
        *highlight = num;

    int top = 1;
    if (num > max_visible)
    {
        top = *highlight - max_visible / 2;
        if (top < 1)
            top = 1;
        int max_top = num - max_visible + 1;
        if (top > max_top)
            top = max_top;

        int end = top + max_visible - 1;
        if (end > num)
            end = num;
        strnfmt(buf, sizeof(buf),
            "Adjust special bonuses (ESC to return) [%d-%d/%d]", top, end,
            num);
        Term_putstr(COL_SMT2, 1, -1, TERM_WHITE, buf);
    }

    object_type snapshot;
    smith_alloy_state alloy_snapshot = smith_alloy;

    for (int i = 0; i < num; i++)
    {
        object_copy(&snapshot, smith_o_ptr);

        if (smith_adjust_bonus_entry(&actions[i].entry, actions[i].delta))
        {
            valid[i] = true;
            can_afford[i] = affordable(smith_o_ptr);
        }

        object_copy(smith_o_ptr, &snapshot);
        smith_alloy = alloy_snapshot;

        attr[i] = valid[i] ? (can_afford[i] ? TERM_WHITE : TERM_SLATE)
                           : TERM_L_DARK;

        const char* name = NULL;
        int value = 0;
        if (actions[i].entry.kind == SMT_BONUS_ENTRY_STAT)
        {
            name = smith_bonus_stat_name(actions[i].entry.index);
            value = smith_o_ptr->stat_bonus[actions[i].entry.index];
        }
        else if (actions[i].entry.kind == SMT_BONUS_ENTRY_SKILL)
        {
            name = skill_names_full[actions[i].entry.index];
            value = smith_o_ptr->skill_bonus[actions[i].entry.index];
        }
        else
        {
            name = smith_bonus_special_name(actions[i].entry.index);
            value = smith_o_ptr->pval;
        }
        const char* verb = (actions[i].delta > 0) ? "increase" : "decrease";

        int entry_idx = i + 1;
        int row = first_row + (entry_idx - top);
        if (row >= first_row && row <= max_row)
        {
            strnfmt(buf, sizeof(buf), "%c) %s %-12s (%+d)", (char)'a' + i, verb,
                name, value);
            Term_putstr(COL_SMT2, row, -1, attr[i], buf);
        }
    }

    int hl_row = first_row + (*highlight - top);
    strnfmt(buf, sizeof(buf), "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT2, hl_row, -1, TERM_L_BLUE, buf);

    prt_object_difficulty();
    prt_object_description();

    Term_fresh();
    Term_gotoxy(2, hl_row);

    ch = smith_ui_inkey_with_wait_reason();

    if ((ch == '4') || (ch == ESCAPE))
        return -1;

    if ((ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        if (valid[*highlight - 1])
            return (*highlight);

        *highlight = old_highlight;
        bell("Invalid choice.");
    }

    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else
            *highlight = num;
        return 0;
    }

    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else
            *highlight = 1;
        return 0;
    }

    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
            return *highlight;
        bell("Invalid choice.");
        return 0;
    }

    return 0;
}

static void smith_bonus_menu(void)
{
    int highlight = 1;
    bool leave_menu = false;

    screen_save();

    while (!leave_menu)
    {
        int choice = smith_bonus_menu_aux(&highlight);
        if (choice == -1)
            leave_menu = true;
        else if (choice >= 1)
        {
            smith_bonus_action actions[26];
            int num = smith_collect_bonus_actions(actions, (int)N_ELEMENTS(actions));
            if (choice <= num)
                (void)smith_adjust_bonus_entry(&actions[choice - 1].entry, actions[choice - 1].delta);
        }
    }

    screen_load();
}

/*
 * Displays a menu for modifying numerical bonuses and weight of an item.
 */
void numbers_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    if (object_has_ego(smith_o_ptr))
        enchant_then_numbers = true;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = numbers_menu_aux(&highlight);

        switch (choice)
        {
        case -1:
        {
            leave_menu = true;
            break;
        }

        default:
        {
            if (choice == SMT_NUM_MENU_EDIT_BONUSES)
                smith_bonus_menu();
            else
                modify_numbers(choice);
            break;
        }
        }
    }

    /* Load screen */
    screen_load();

    return;
}

static void ego_name_for_enchant_menu(int e_idx, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;
    buf[0] = '\0';
    if (e_idx <= 0 || e_idx >= z_info->e_max)
        return;

    ego_item_type* e_ptr = &e_info[e_idx];
    const char* raw = e_name + e_ptr->name;
    if (!raw || !raw[0])
        return;

    if (ego_name_is_prefix(raw))
    {
        size_t len = strlen(raw);
        size_t copy_len = (len >= 2) ? (len - 2) : 0;
        if (copy_len >= buflen)
            copy_len = buflen - 1;
        if (copy_len > 0)
        {
            memcpy(buf, raw + 1, copy_len);
            buf[copy_len] = '\0';
        }
        return;
    }

    SDL_strlcpy(buf, raw, buflen);
}

static void prt_reforge_preview(const reforge_preview_type* preview)
{
    char buf[80];
    int costs = 0;
    byte attr = TERM_SLATE;
    bool compact = smith_ui_compact_width();

    wipe_screen_from(COL_SMT4);

    if (!preview)
        return;

    if (!preview->affordable)
        attr = TERM_L_DARK;

    Term_putstr(COL_SMT4, 2, -1, attr, "Reforge Diff:");
    strnfmt(buf, sizeof(buf), "%d", preview->scaled_difficulty);
    Term_putstr(COL_SMT4 + 2, 4, -1, attr, buf);

    if (compact)
        strnfmt(buf, sizeof(buf), "+%d raw", preview->raw_delta_difficulty);
    else
        strnfmt(buf, sizeof(buf), "(+%d raw)", preview->raw_delta_difficulty);
    Term_putstr(COL_SMT4 + (compact ? 4 : 5), 4, -1, TERM_L_DARK, buf);

    Term_putstr(COL_SMT4, smith_ui_cost_title_row(), -1,
        preview->affordable ? TERM_SLATE : TERM_L_DARK, "Cost:");

    if (preview->cost.uses > 0)
    {
        attr = (forge_uses(p_ptr->py, p_ptr->px) >= preview->cost.uses)
            ? TERM_SLATE : TERM_L_DARK;
        if (compact)
            strnfmt(buf, sizeof(buf), "%d/%d uses", preview->cost.uses,
                forge_uses(p_ptr->py, p_ptr->px));
        else
            strnfmt(buf, sizeof(buf), "%d Use%s", preview->cost.uses,
                (preview->cost.uses == 1) ? "" : "s");
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.drain > 0)
    {
        attr = (preview->cost.drain <= p_ptr->skill_base[S_SMT])
            ? TERM_BLUE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Smithing", preview->cost.drain);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.mithril > 0)
    {
        attr = (preview->cost.mithril <= mithril_carried()) ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), compact ? "%d.%d lb Mith" : "%d.%d lb Mithril",
            preview->cost.mithril / 10, preview->cost.mithril % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.star_iron > 0)
    {
        attr = (preview->cost.star_iron <= star_iron_carried()) ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), compact ? "%d.%d lb StIron" : "%d.%d lb Star Iron",
            preview->cost.star_iron / 10, preview->cost.star_iron % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.str > 0)
    {
        attr = (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR] - preview->cost.str >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Str", preview->cost.str);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.dex > 0)
    {
        attr = (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX] - preview->cost.dex >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Dex", preview->cost.dex);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.con > 0)
    {
        attr = (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON] - preview->cost.con >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Con", preview->cost.con);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.gra > 0)
    {
        attr = (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA] - preview->cost.gra >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Gra", preview->cost.gra);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.exp > 0)
    {
        attr = (p_ptr->new_exp >= preview->cost.exp) ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Exp", preview->cost.exp);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }

    strnfmt(buf, sizeof(buf), "%d Turns", preview->turns);
    smith_ui_put_cost_line(costs, TERM_SLATE, buf);
}

static int reforge_prefix_menu(const object_type* source)
{
    char ch;
    char buf[80];
    int i;
    int highlight = 1;
    int entry_count = 0;
    int choice[26];
    bool valid[26];
    reforge_preview_type previews[26];

    if (!source || !source->k_idx)
        return 0;

    screen_save();

    while (true)
    {
        wipe_screen_from(COL_SMT2);
        Term_putstr(COL_SMT2, 1, -1, TERM_WHITE, "Select prefix:");

        entry_count = 0;
        memset(choice, 0, sizeof(choice));
        memset(valid, 0, sizeof(valid));
        memset(previews, 0, sizeof(previews));

        for (i = 1; i < z_info->e_max && entry_count < (int)N_ELEMENTS(choice); i++)
        {
            char ego_label[64];

            if (!ego_prefix_can_apply_to_object(source, i))
                continue;
            if (!reforge_preview_build(source, i, &previews[entry_count]))
                continue;

            valid[entry_count] = previews[entry_count].affordable;
            choice[entry_count] = i;

            ego_name_for_enchant_menu(i, ego_label, sizeof(ego_label));
            strnfmt(buf, sizeof(buf), "%c) %s", (char)'a' + entry_count, ego_label);
            Term_putstr(COL_SMT2, entry_count + 2, -1,
                valid[entry_count] ? TERM_WHITE : TERM_L_DARK, buf);
            entry_count++;
        }

        if (entry_count == 0)
        {
            Term_putstr(COL_SMT2, 3, -1, TERM_L_DARK,
                "(No legal prefixes available.)");
            Term_fresh();
            (void)smith_ui_inkey_with_wait_reason();
            screen_load();
            return 0;
        }

        if (highlight < 1) highlight = 1;
        if (highlight > entry_count) highlight = entry_count;

        strnfmt(buf, sizeof(buf), "%c)", (char)'a' + highlight - 1);
        Term_putstr(COL_SMT2, highlight + 1, -1, TERM_L_BLUE, buf);

        (void)reforge_preview_build(source, choice[highlight - 1],
            &previews[highlight - 1]);
        prt_reforge_preview(&previews[highlight - 1]);
        prt_object_description();

        Term_fresh();
        Term_gotoxy(14, 1 + highlight);

        ch = smith_ui_inkey_with_wait_reason();

        if ((ch >= 'a') && (ch <= (char)'a' + entry_count - 1))
        {
            highlight = (int)ch - 'a' + 1;
            if (!valid[highlight - 1])
                bell("You cannot afford that reforge.");
            else
            {
                screen_load();
                return choice[highlight - 1];
            }
        }
        else if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            if (!valid[highlight - 1])
                bell("You cannot afford that reforge.");
            else
            {
                screen_load();
                return choice[highlight - 1];
            }
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            screen_load();
            return 0;
        }
        else if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (highlight > 1)
                highlight--;
            else
                highlight = entry_count;
        }
        else if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (highlight < entry_count)
                highlight++;
            else
                highlight = 1;
        }
    }
}

static void create_special(int ego_prefix, int ego_suffix)
{
    /* Retrieve a backup of the object */
    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;

    /* Suffix egos marked NO_PREFIX cannot be combined with any prefix. */
    if (ego_forbids_prefix_combo(ego_suffix))
        ego_prefix = 0;

    /* Apply requested ego affixes */
    object_set_ego_prefix(smith_o_ptr, ego_prefix);
    object_set_ego_suffix(smith_o_ptr, ego_suffix);

    /* Apply ego bonuses */
    if (object_has_ego(smith_o_ptr))
        object_into_special(smith_o_ptr, p_ptr->skill_use[S_SMT], true);

    /* Re-evaluate stack size now that an enchantment is applied */
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);
}

static bool enchant_menu_has_applicable_affix(const object_type* base_o_ptr,
    int fixed_prefix, int fixed_suffix, bool selecting_prefix)
{
    int i;

    if (!base_o_ptr || !smith_o_ptr || base_o_ptr->tval == 0)
        return false;
    if (object_has_evil_alignment(smith_o_ptr))
        return false;

    for (i = 1; i < z_info->e_max; i++)
    {
        if (smith_ego_can_apply_to_object(
                base_o_ptr, i, fixed_prefix, fixed_suffix, selecting_prefix))
            return true;
    }

    return false;
}

/*
 * Performs the interface and selection work for the enchantment menu.
 */
static int enchant_menu_aux(int* highlight, int fixed_prefix, int fixed_suffix,
    bool selecting_prefix, const object_type* base_o_ptr)
{
    char ch;
    int i;
    int entry_count = 0;
    char buf[80];
    bool valid[26];
    int choice[26];

    // clear the right of the screen
    wipe_screen_from(COL_SMT2);

    /* Header */
    Term_putstr(COL_SMT2, 1, -1, TERM_WHITE,
        selecting_prefix ? "Select prefix:" : "Select suffix:");

    /* Always allow selecting no affix */
    valid[entry_count] = true;
    choice[entry_count] = 0;
    strnfmt(buf, sizeof(buf), "%c) %s", (char)'a' + entry_count, "(none)");
    Term_putstr(COL_SMT2, entry_count + 2, -1, TERM_WHITE, buf);
    entry_count++;

    /* Suffix egos marked NO_PREFIX only allow "(none)" as the prefix choice. */
    if (selecting_prefix && ego_forbids_prefix_combo(fixed_suffix))
    {
        Term_putstr(COL_SMT2, entry_count + 2, -1, TERM_SLATE,
            "(no prefix allowed with this suffix)");
    }

    /* We have to search the whole special item list. */
    for (i = 1; i < z_info->e_max && entry_count < (int)N_ELEMENTS(choice); i++)
    {
        if (smith_ego_can_apply_to_object(
                base_o_ptr, i, fixed_prefix, fixed_suffix, selecting_prefix))
        {
            /* Make a preview 'special' version of the object */
            if (selecting_prefix)
                create_special(i, fixed_suffix);
            else
                create_special(fixed_prefix, i);

            // Check whether it is a valid choice for creating
            if (affordable(smith_o_ptr))
            {
                valid[entry_count] = true;
            }
            else
            {
                valid[entry_count] = false;
            }

            /* Print it */
            char ego_label[64];
            ego_name_for_enchant_menu(i, ego_label, sizeof(ego_label));
            strnfmt(buf, sizeof(buf), "%c) %s", (char)'a' + entry_count, ego_label);
            Term_putstr(COL_SMT2, entry_count + 2, -1,
                valid[entry_count] ? TERM_WHITE : TERM_SLATE, buf);

            /* Remember the object index */
            choice[entry_count] = i;

            // count the applicable items
            entry_count++;
        }
    }

    if (*highlight < 1) *highlight = 1;
    if (*highlight > entry_count) *highlight = entry_count;

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);

    /* Make a preview 'special' version of the object */
    if (selecting_prefix)
        create_special(choice[*highlight - 1], fixed_suffix);
    else
        create_special(fixed_prefix, choice[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    ch = smith_ui_inkey_with_wait_reason();

    /* Choose by letter */
    if ((ch >= 'a') && (ch <= (char)'a' + entry_count - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        /* Make a preview 'special' version of the object */
        if (selecting_prefix)
            create_special(choice[*highlight - 1], fixed_suffix);
        else
            create_special(fixed_prefix, choice[*highlight - 1]);

        return (*highlight);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
        || (ch == ARROW_RIGHT)
#endif
        )
    {
        return (*highlight);
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE)
#ifdef ARROW_LEFT
        || (ch == ARROW_LEFT)
#endif
        )
    {
        *highlight = -1;
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8'
#ifdef ARROW_UP
        || (ch == ARROW_UP)
#endif
        )
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = entry_count;
    }

    /* Next item */
    if (ch == '2'
#ifdef ARROW_DOWN
        || (ch == ARROW_DOWN)
#endif
        )
    {
        if (*highlight < entry_count)
            (*highlight)++;
        else if (*highlight == entry_count)
            *highlight = 1;
    }

    return (0);
}

/*
 * Brings up a menu for making an item into a {special} item.
 */
bool enchant_menu(void)
{
    int prefix_highlight = 1;
    int suffix_highlight = 1;

    bool completed = false;
    bool leave_menu = false;

    /* Save screen */
    screen_save();

    // stop the item being an artefact, if it was
    smith_o_ptr->name1 = 0;
    smith2_o_ptr->name1 = 0;

    int selected_prefix = (int)object_ego_prefix(smith_o_ptr);
    int selected_suffix = (int)object_ego_suffix(smith_o_ptr);

    bool show_prefix_step =
        enchant_menu_has_applicable_affix(
            smith2_o_ptr, 0, selected_suffix, true) || (selected_prefix != 0);
    bool show_suffix_step =
        enchant_menu_has_applicable_affix(
            smith2_o_ptr, selected_prefix, 0, false) || (selected_suffix != 0);

    if (!show_prefix_step && !show_suffix_step)
    {
        /* Nothing to select; bail out without changing the item. */
        screen_load();
        return false;
    }

    bool selecting_prefix = show_prefix_step;

    /* Process events until menu is abandoned */
    while (!leave_menu)
    {
        if (selecting_prefix)
        {
            int choice_idx = enchant_menu_aux(
                &prefix_highlight, 0, selected_suffix, true, smith2_o_ptr);

            if (choice_idx == -1)
            {
                completed = false;
                leave_menu = true;
                continue;
            }

            if (choice_idx >= 1)
            {
                selected_prefix = (int)object_ego_prefix(smith_o_ptr);
                create_special(selected_prefix, selected_suffix);

                if (show_suffix_step)
                {
                    selecting_prefix = false;
                    continue;
                }

                completed = true;
                leave_menu = true;
                continue;
            }
        }
        else
        {
            int choice_idx = enchant_menu_aux(
                &suffix_highlight, selected_prefix, 0, false, smith2_o_ptr);

            if (choice_idx == -1)
            {
                if (show_prefix_step)
                {
                    /* Back to prefix selection */
                    create_special(selected_prefix, selected_suffix);
                    selecting_prefix = true;
                    continue;
                }

                completed = false;
                leave_menu = true;
                continue;
            }

            if (choice_idx >= 1)
            {
                selected_suffix = (int)object_ego_suffix(smith_o_ptr);
                create_special(selected_prefix, selected_suffix);
                completed = true;
                leave_menu = true;
                continue;
            }
        }
    }

    /* Load screen */
    screen_load();

    return (completed);
}

/*
 * Copies an artefact structure over the top of another one.
 */
void prepare_artefact(void)
{
    int i;

    log_debug("Preparing artifact for modification");

    // retrieve a backup of the artefact
    artefact_copy(smith_a_ptr, smith2_a_ptr);

    // retrieve a backup of the object
    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;

    // set its 'artefact' name to reflect the chosen type
    smith_o_ptr->name1 = smith_a_name;

    // Restore default stack sizes for arrows and other throwable gear
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;

    log_trace("Artifact preparation complete - %d abilities synchronized", smith_a_ptr->abilities);
}

/*
 * Does the given object type support the given flag type?
 */
bool applicable_flag(u32b f, int flagset, object_type* o_ptr)
{
    bool ok = false;
    int i;
    u32b f1, f2, f3, f4;

    /* Telchar may always put SHARPNESS II on a melee weapon               */
    if ((flagset == 1) && (f == TR1_SHARPNESS2) &&
        (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
    {
        switch (smith_o_ptr->tval)                   /* any melee weapon   */
        {
            case TV_SWORD: case TV_HAFTED:
            case TV_POLEARM: case TV_DIGGING:
                return true;
        }
    }

    /* Extract the object flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    /* Warhammers-only: Smithing bonus requires Brand Fire on the same item. */
    if ((flagset == 1) && (f == TR1_SMT))
    {
        if (o_ptr->tval != TV_HAFTED || o_ptr->sval != SV_WAR_HAMMER)
            return false;
        if (!(f1 & TR1_BRAND_FIRE))
            return false;
        return true;
    }

    /* Go through the list of artefacts and see if the flag is applicable for
     * this type  */
    for (i = ART_ULTIMATE; i < z_info->art_norm_max; i++)
    {
        /* Access the artefact */
        artefact_type* a_ptr = &a_info[i];

        /* Skip other types of artefacts */
        if (a_ptr->tval != o_ptr->tval)
            continue;

        switch (flagset)
        {
        case 1:
        {
            if (a_ptr->flags1 & f)
                ok = true;
            break;
        }
        case 2:
        {
            if (a_ptr->flags2 & f)
                ok = true;
            break;
        }
        case 3:
        {
            if (a_ptr->flags3 & f)
                ok = true;
            break;
        }
        case 4:
        {
            if (a_ptr->flags4 & f)
                ok = true;
            break;
        }
        }
    }

    return (ok);
}

/*
 * Adds a given flag to the dummy artefact.
 */
void add_artefact_flag(u32b f, int flagset)
{
    u32b f1_before, f2, f3;
    u32b f1_after;

    log_trace("Adding artifact flag %u in flagset %d", f, flagset);
    
    // prepare the artefact and object for modification
    prepare_artefact();

    object_flags(smith_o_ptr, &f1_before, &f2, &f3);

    // set new flag on the artefact
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

/*
 * Removes a given flag from the dummy artefact.
 */
void remove_artefact_flag(u32b f, int flagset)
{
    u32b f1_before, f2, f3;
    u32b f1_after;

    log_trace("Removing artifact flag %u from flagset %d", f, flagset);
    
    // prepare the artefact and object for modification
    prepare_artefact();

    object_flags(smith_o_ptr, &f1_before, &f2, &f3);

    // unset new flag on the artefact
    if (flagset == 1)
        smith_a_ptr->flags1 &= ~(f);
    if (flagset == 2)
        smith_a_ptr->flags2 &= ~(f);
    if (flagset == 3)
        smith_a_ptr->flags3 &= ~(f);
    if (flagset == 4)
        smith_a_ptr->flags4 &= ~(f);

    /* Keep Smithing dependent on Brand Fire. */
    if ((flagset == 1) && (f == TR1_BRAND_FIRE))
        smith_a_ptr->flags1 &= ~(TR1_SMT);

    object_flags(smith_o_ptr, &f1_after, &f2, &f3);
    smith_apply_stat_skill_flag_delta(smith_o_ptr, f1_before, f1_after);
}

/*
 * Performs the interface and selection work for the artefact flag selection.
 */
int artefact_flag_menu_aux(int category, int* highlight)
{
    char ch;
    int i, num = 0;
    char buf[80];
    bool flag_present[MAX_SMITHING_FLAGS] = { false };
    bool flag_valid[MAX_SMITHING_FLAGS] = { false };
    bool flag_affordable[MAX_SMITHING_FLAGS] = { false };
    u32b flag[MAX_SMITHING_FLAGS];
    int flagset[MAX_SMITHING_FLAGS];
    byte attr;

    // clear the right of the screen
    wipe_screen_from(COL_SMT3);

    // display the categories
    for (i = 0; smithing_flag_types[i].flag != 0; i++)
    {
        if (category == smithing_flag_types[i].category)
        {
            /* Telchar-only: skip Sharpness2 if not in character Telchar */
            if ((smithing_flag_types[i].flagset == 1) &&
                (smithing_flag_types[i].flag == TR1_SHARPNESS2) &&
                !(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
            {
                /* don't even consider it */
                continue;
            }
            flag[num] = smithing_flag_types[i].flag;
            flagset[num] = smithing_flag_types[i].flagset;

            if (((flagset[num] == 1) && (smith2_a_ptr->flags1 & flag[num]))
                || ((flagset[num] == 2) && (smith2_a_ptr->flags2 & flag[num]))
                || ((flagset[num] == 3) && (smith2_a_ptr->flags3 & flag[num]))
                || ((flagset[num] == 4) && (smith2_a_ptr->flags4 & flag[num])))
            {
                flag_present[num] = true;
                flag_valid[num] = true;
            }

            else
            {
                // require that the flag can be present on the object
                if (applicable_flag(flag[num], flagset[num], smith_o_ptr))
                {
                    flag_valid[num] = true;

                    // add this flag to the dummy artefact under construction
                    add_artefact_flag(flag[num], flagset[num]);

                    // Check whether it is a valid choice for creating (needs to
                    // be affordable and successful)
                    if (affordable(smith_o_ptr))
                    {
                        flag_affordable[num] = true;
                    }
                }
            }

        // /* Lock Sharpness II behind Telchar forge */
        // if (flag[num] == TR1_SHARPNESS2 &&
        //     !(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
        //     flag_valid[num] = false;

            attr = flag_present[num]
                ? TERM_BLUE
                : (flag_valid[num]
                        ? (flag_affordable[num] ? TERM_WHITE : TERM_SLATE)
                        : TERM_L_DARK);

            /* Display the line */
            strnfmt(buf, 80, "%c) %s", (char)'a' + num,
                smithing_flag_types[i].desc);
            Term_putstr(COL_SMT3, num + 2, -1, attr, buf);

            num++;
        }
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT3, *highlight + 1, -1, TERM_L_BLUE, buf);

    // add this flag to the dummy artefact under construction
    add_artefact_flag(flag[*highlight - 1], flagset[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    ch = smith_ui_inkey_with_wait_reason();

    /* Abort if there are no choices */
    if (num == 0)
    {
        return (-1);
    }

    /* Choose by letter */
    if ((ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int new_highlight = (int)ch - 'a' + 1;

        if (flag_valid[new_highlight - 1])
        {
            if (new_highlight == *highlight)
            {
                // remove a flag if it already existed
                if (flag_present[*highlight - 1])
                    remove_artefact_flag(
                        flag[*highlight - 1], flagset[*highlight - 1]);
            }
            else
            {
                // restore the artefact from backup
                artefact_copy(smith_a_ptr, smith2_a_ptr);

                *highlight = new_highlight;

                // remove a flag if it already existed
                if (flag_present[*highlight - 1])
                    remove_artefact_flag(
                        flag[*highlight - 1], flagset[*highlight - 1]);

                // otherwise add it
                else
                    add_artefact_flag(
                        flag[*highlight - 1], flagset[*highlight - 1]);
            }

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;

            return (*highlight);
        }
        else
        {
            bell("Invalid choice.");
        }
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (flag_valid[*highlight - 1])
        {
            // remove a flag if it already existed
            if (flag_present[*highlight - 1])
                remove_artefact_flag(
                    flag[*highlight - 1], flagset[*highlight - 1]);

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;

            return (*highlight);
        }

        else
        {
            bell("Invalid choice.");
        }
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;

        // restore the backup artefact and object
        prepare_artefact();

        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    return (0);
}

/*
 * Brings up a menu to select individual flags of a given type to
 * add to (or subtract from) an artefact.
 */
void artefact_flag_menu(int category)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = artefact_flag_menu_aux(category, &highlight);

        if (choice >= 1)
        {
            // don't leave the menu
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    screen_load();
}

/*
 * Can this ability be applied to any item at all?
 * Returns false for stat-only abilities like Grace/Strength/etc that have no valid item types.
 */
static bool ability_can_be_smithed(ability_type* b_ptr)
{
    int j;

    /* Check if this ability has any valid item types defined */
    for (j = 0; j < ABILITY_TVALS_MAX; j++)
    {
        if (b_ptr->tval[j] != 0)
            return true;
    }

    return false;
}

/*
 * Does the given object type support the given ability type?
 */
bool applicable_ability(ability_type* b_ptr, object_type* o_ptr)
{
    bool ok = false;
    int j;

    u32b f1, f2, f3;

    /* Test if this is a legal item type for this ability */
    for (j = 0; j < ABILITY_TVALS_MAX; j++)
    {
        /* Require identical base type */
        if (o_ptr->tval == b_ptr->tval[j])
        {
            /* Require sval in bounds, lower */
            if (o_ptr->sval >= b_ptr->min_sval[j])
            {
                /* Require sval in bounds, upper */
                if (o_ptr->sval <= b_ptr->max_sval[j])
                {
                    /* Accept */
                    ok = true;
                }
            }
        }
    }

    // Polearm Mastery is OK for Polearms
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & TR3_POLEARM)
    {
        if ((b_ptr->skilltype == S_MEL) && (b_ptr->abilitynum == MEL_POLEARMS))
            ok = true;
    }

    return (ok);
}

/*
 * Adds a given ability to the dummy artefact.
 */
void add_artefact_ability(int skilltype, int abilitynum)
{
    int i;

    log_trace("Adding artifact ability - skill:%d ability:%d", skilltype, abilitynum);

    // prepare the artefact and object for modification
    prepare_artefact();

    // set new ability on the artefact
    if (smith_a_ptr->abilities < 4)
    {
        bool already_present = false;

        for (i = 0; i < smith_a_ptr->abilities; i++)
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
            smith_a_ptr->bane_type[smith_a_ptr->abilities] = 0; // Player-smithed banes use player choice
            smith_a_ptr->abilities++;
        }
    }

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
}

/*
 * Removes a given ability from the dummy artefact.
 */
void remove_artefact_ability(int skilltype, int abilitynum)
{
    int i;
    int location = -1;

    log_trace("Removing artifact ability - skill:%d ability:%d", skilltype, abilitynum);

    // prepare the artefact and object for modification
    prepare_artefact();

    // remove new ability on the artefact
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        if ((smith_a_ptr->skilltype[i] == skilltype)
            && (smith_a_ptr->abilitynum[i] == abilitynum))
        {
            location = i;
        }
    }

    if (location >= 0)
    {
        for (i = location; i < smith_a_ptr->abilities - 1; i++)
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

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
}

/*
 * Determines if an artefact type has a given ability.
 */
bool has_ability(artefact_type* a_ptr, int skilltype, int abilitynum)
{
    int i;

    for (i = 0; i < a_ptr->abilities; i++)
    {
        if ((a_ptr->skilltype[i] == skilltype)
            && (a_ptr->abilitynum[i] == abilitynum))
            return (true);
    }

    return (false);
}

/*
 * Performs the interface and selection work for the artefact flag selection.
 */
int artefact_ability_menu_aux(int skill, int* highlight)
{
    char ch;
    int i, num = 0;
    char buf[80];
    ability_type* b_ptr;
    byte attr;
    
    /* Allocate arrays dynamically based on actual max abilities */
    bool* ability_present = mem_alloc_array(z_info->b_max, bool);
    bool* ability_valid = mem_alloc_array(z_info->b_max, bool);
    bool* ability_affordable = mem_alloc_array(z_info->b_max, bool);
    int* ability_nums = mem_alloc_array(z_info->b_max, int);
    
    /* Initialize arrays to zero/false */
    memset(ability_present, 0, z_info->b_max * sizeof(bool));
    memset(ability_valid, 0, z_info->b_max * sizeof(bool));
    memset(ability_affordable, 0, z_info->b_max * sizeof(bool));

    // clear the right of the screen
    wipe_screen_from(COL_SMT3);

    // list the abilities
    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skill)
            continue;

        /* Skip abilities that can't be smithed onto any item (like Grace, stat improvements) */
        if (!ability_can_be_smithed(b_ptr))
            continue;

        // Store the mapping from display index to actual ability number
        ability_nums[num] = b_ptr->abilitynum;

        // Determine the appropriate colour
        if (has_ability(smith2_a_ptr, skill, b_ptr->abilitynum))
        {
            ability_present[num] = true;
            ability_valid[num] = true;
        }
        else
        {
            // require that the ability can be present on the object
            if (applicable_ability(b_ptr, smith_o_ptr))
            {
                ability_valid[num] = true;

                // add this flag to the dummy artefact under construction
                add_artefact_ability(skill, b_ptr->abilitynum);

                // require that the ability was successfully added
                if (has_ability(smith_a_ptr, skill, b_ptr->abilitynum))
                {
                    // Check whether it is a valid choice for creating (needs to
                    // be affordable and successful)
                    if (affordable(smith_o_ptr))
                    {
                        ability_affordable[num] = true;
                    }
                }

                // if the ability wasn't added properly (the item had too many),
                // then it is not valid after all
                else
                {
                    ability_valid[num] = false;
                }
            }
        }

        attr = ability_present[num]
            ? TERM_BLUE
            : (ability_valid[num]
                    ? (ability_affordable[num] ? TERM_WHITE : TERM_SLATE)
                    : TERM_L_DARK);

        /* Display the line */
        strnfmt(buf, 80, "%c) %s", (char)'a' + num, b_name + b_ptr->name);
        Term_putstr(COL_SMT3, num + 2, -1, attr, buf);

        num++;
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT3, *highlight + 1, -1, TERM_L_BLUE, buf);

    // add this ability to the dummy artefact under construction (use actual ability number)
    add_artefact_ability(skill, ability_nums[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    ch = smith_ui_inkey_with_wait_reason();

    /* Abort if there are no choices */
    if (num == 0)
    {
        return (-1);
    }

    /* Choose by letter */
    if ((ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int new_highlight = (int)ch - 'a' + 1;

        if (ability_valid[new_highlight - 1])
        {
            if (new_highlight == *highlight)
            {
                // remove an ability if it already existed
                if (ability_present[*highlight - 1])
                    remove_artefact_ability(skill, ability_nums[*highlight - 1]);
            }
            else
            {
                // restore the artefact from backup
                artefact_copy(smith_a_ptr, smith2_a_ptr);

                *highlight = new_highlight;

                // remove an ability if it already existed
                if (ability_present[*highlight - 1])
                    remove_artefact_ability(skill, ability_nums[*highlight - 1]);

                // otherwise add it
                else
                    add_artefact_ability(skill, ability_nums[*highlight - 1]);
            }

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);

            mem_free(ability_present);
            mem_free(ability_valid);
            mem_free(ability_affordable);
            mem_free(ability_nums);
            return (*highlight);
        }
        else
        {
            bell("Invalid choice.");
        }
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (ability_valid[*highlight - 1])
        {
            // remove an ability if it already existed
            if (ability_present[*highlight - 1])
                remove_artefact_ability(skill, ability_nums[*highlight - 1]);

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);

            mem_free(ability_present);
            mem_free(ability_valid);
            mem_free(ability_affordable);
            mem_free(ability_nums);
            return (*highlight);
        }
        else
        {
            bell("Invalid choice.");
        }
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        // remove any tentatively-added ability from the object
        if (!ability_present[*highlight - 1])
            remove_artefact_ability(skill, ability_nums[*highlight - 1]);

        // restore the backup artefact
        artefact_copy(smith_a_ptr, smith2_a_ptr);

        *highlight = -1;

        mem_free(ability_present);
        mem_free(ability_valid);
        mem_free(ability_affordable);
        mem_free(ability_nums);
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    mem_free(ability_present);
    mem_free(ability_valid);
    mem_free(ability_affordable);
    mem_free(ability_nums);
    return (0);
}

/*
 * Brings up a menu to select individual abilities of a given skill to
 * add to (or subtract from) an artefact.
 */
void artefact_ability_menu(int skill)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = artefact_ability_menu_aux(skill, &highlight);

        if (choice >= 1)
        {
            // don't leave the menu
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    screen_load();
}

/*
 * Allows the player to choose a new name for an artefact.
 */
void rename_artefact(void)
{
    char tmp[20];
    char old_name[20];
    char o_desc[30];
    bool name_selected = false;
    int row = (smith_ui_last_desc_row >= 0) ? smith_ui_last_desc_row
                                            : (smith_ui_term_hgt() - 1);
    int col = smith_ui_desc_col();

    // Clear the names
    tmp[0] = '\0';
    old_name[0] = '\0';

    // Clear object name
    Term_erase(0, row, smith_ui_term_wid());

    // Determine object name
    object_desc(o_desc, sizeof(o_desc), smith_o_ptr, false, -1);

    // Display shortened object name
    Term_putstr(col, row, smith_ui_term_wid() - col, TERM_L_WHITE, o_desc);

    // use old name as a default
    SDL_strlcpy(tmp, smith2_a_ptr->name, sizeof(tmp));

    // save a copy too
    SDL_strlcpy(old_name, op_ptr->full_name, sizeof(old_name));

    /* Prompt for a new name */
    Term_gotoxy(col + strlen(o_desc) + 1, row);

    while (!name_selected)
    {
        if (askfor_name(tmp, sizeof(tmp)))
        {
            SDL_strlcpy(smith2_a_ptr->name, tmp, MAX_LEN_ART_NAME);
            p_ptr->redraw |= (PR_MISC);
        }
        else
        {
            SDL_strlcpy(smith2_a_ptr->name, old_name, MAX_LEN_ART_NAME);
            return;
        }

        if (tmp[0] != '\0')
            name_selected = true;
        else
            SDL_strlcpy(smith2_a_ptr->name, old_name, MAX_LEN_ART_NAME);
    }

    // retrieve a backup of the artefact (all the modifications were done to
    // this backup copy)
    artefact_copy(smith_a_ptr, smith2_a_ptr);
}

/*
 * Performs the interface and selection work for the 1st level artefact menu.
 */
int artefact_menu_aux(int* highlight)
{
    char ch;
    int i, num;
    char buf[80];

    // clear the right of the screen
    wipe_screen_from(COL_SMT2);

    // display the categories for flags
    for (i = 0; i < MAX_CATS; i++)
    {
        strnfmt(buf, 80, "%c) %s", (char)'a' + i, smithing_flag_cats[i].desc);
        Term_putstr(COL_SMT2, smith_ui_dense_row(i), -1, TERM_WHITE, buf);
    }

    // display the categories for abilities (skip Special abilities - S_SPC)
    int display_idx = 0;
    for (i = 0; i < S_MAX; i++)
    {
        /* Skip Special abilities - they cannot be smithed onto items */
        if (i == S_SPC) continue;
        
        strnfmt(
            buf, 80, "%c) %s", (char)'a' + MAX_CATS + display_idx, skill_names_full[i]);
        Term_putstr(COL_SMT2, smith_ui_dense_row(MAX_CATS + display_idx), -1,
            TERM_WHITE, buf);
        display_idx++;
    }

    num = MAX_CATS + display_idx + 1;

    // Menu item for naming artefacts
    strnfmt(buf, 80, "%c) %s", (char)'a' + num - 1, "Name Artefact");
    Term_putstr(COL_SMT2, smith_ui_dense_row(num - 1), -1, TERM_WHITE, buf);

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT2, smith_ui_dense_highlight_row(*highlight), -1,
        TERM_L_BLUE, buf);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, smith_ui_dense_highlight_row(*highlight));

    /* Get key (while allowing menu commands) */
    ch = smith_ui_inkey_with_wait_reason();

    /* Choose by letter */
    if ((ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        strnfmt(buf, 80, "%c)", (char)'a' + old_highlight - 1);
        Term_putstr(COL_SMT2, smith_ui_dense_highlight_row(old_highlight), -1,
            TERM_WHITE, buf);
        strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
        Term_putstr(COL_SMT2, smith_ui_dense_highlight_row(*highlight), -1,
            TERM_L_BLUE, buf);

        return (*highlight);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    return (0);
}

/*
 * Brings up a menu for making a base item into an artefact,
 * by adding flags of various types.
 */
void artefact_menu(void)
{
    int choice = -1;
    int highlight = 1;

    char buf[36];
    bool leave_menu = false;

    log_info("Player opened artifact creation menu");

    /* Save screen */
    screen_save();

    if (!smith_o_ptr->name1)
    {
        log_debug("Initializing new artifact creation");
        // wipe the existing artefact (and its backup)
        artefact_wipe(smith_a_name);
        artefact_wipe(smith2_a_name);

        // add 'ignore all'
        smith2_a_ptr->flags3 |= (TR3_IGNORE_MASK);

        // change the SV for rings and amulets when they start to get made into
        // artefacts
        if (smith_o_ptr->tval == TV_RING)
        {
            create_base_object(TV_RING, SV_RING_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
        }
        if (smith_o_ptr->tval == TV_AMULET)
        {
            create_base_object(TV_AMULET, SV_AMULET_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
            smith2_o_ptr->pd = 1;
        }
    }

    // set the backup artefact name to the player character's name
    if (strlen(smith2_a_ptr->name) == 0)
    {
        sprintf(buf, "of %s", op_ptr->full_name);
        SDL_strlcpy(smith2_a_ptr->name, buf, MAX_LEN_ART_NAME);
    }

    // prepare the artefact and object for modification
    prepare_artefact();

    /* Number of skill categories displayed (S_MAX minus Special abilities) */
    int num_skills = S_MAX - 1;

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = artefact_menu_aux(&highlight);

        if (choice == MAX_CATS + num_skills + 1)
        {
            rename_artefact();
        }
        else if (choice >= MAX_CATS + 1)
        {
            artefact_ability_menu(choice - MAX_CATS - 1);
        }
        else if (choice >= 1)
        {
            artefact_flag_menu(choice);
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    screen_load();

    return;
}

/*
 * Performs the interface and selection work for the melting menu.
 */
int melt_menu_aux(int* highlight)
{
    char ch;
    int i;
    int num = 0;
    object_type* o_ptr;
    u32b f1, f2, f3;
    char desc[80];
    char buf[80];

    // clear the right of the screen
    wipe_screen_from(COL_SMT2);

    // clear bottom of the screen
    wipe_object_description();

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        object_flags(o_ptr, &f1, &f2, &f3);
        
        /* ignore metal items that carry the "can't melt" tag */
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON)) && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            object_desc(desc, 80, o_ptr, false, 2);
            strnfmt(buf, 80, "%c) %s", (char)'a' + num, desc);

            Term_putstr(COL_SMT2, smith_ui_dense_row(num), -1, TERM_WHITE, buf);

            if (smith_ui_weight_col() > 0)
            {
                strnfmt(buf, 80, "%2d.%d lb", o_ptr->weight / 10,
                    o_ptr->weight % 10);
                Term_putstr(smith_ui_weight_col(), smith_ui_dense_row(num), -1,
                    TERM_WHITE, buf);
            }

            num++;
        }
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT2, smith_ui_dense_highlight_row(*highlight), -1,
        TERM_L_BLUE, buf);

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14, smith_ui_dense_highlight_row(*highlight));

    /* Get key (while allowing menu commands) */
    ch = smith_ui_inkey_with_wait_reason();

    // choose an option by letter
    if ((ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        // move the light blue highlight
        move_displayed_highlight(
            old_highlight, TERM_WHITE, *highlight, COL_SMT2);

        return (*highlight);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        return (-1);
    }

    return (0);
}

/*
 * Produces the menu for melting down mithril and star-iron items into their metal pieces.
 */
void melt_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = melt_menu_aux(&highlight);

        if (choice >= 1)
        {
            if (melt_metal_item(choice))
            {
                leave_menu = true;
            }
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    screen_load();
}

static bool smith_item_tester_hook_reforge_target(const object_type* o_ptr)
{
    return object_is_damaged_item(o_ptr) || object_can_reforge_prefix(o_ptr);
}

static bool smith_reforge_item(void)
{
    int slot = -1;
    int prefix_idx = 0;
    char old_name[80];
    char new_name[80];
    object_type smith_backup;
    object_type smith2_backup;
    smith_alloy_state alloy_backup = smith_alloy;
    smith_alloy_state alloy2_backup = smith2_alloy;

    if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    {
        msg_print("You can only reforge items at a forge.");
        return false;
    }

    if (forge_uses(p_ptr->py, p_ptr->px) <= 0)
    {
        msg_print("This forge has no resources left.");
        return false;
    }

    if (!p_ptr->active_ability[S_SMT][SMT_REPAIR])
    {
        bell("You do not know how to reforge gear.");
        return false;
    }

    item_tester_hook = smith_item_tester_hook_reforge_target;
    if (!get_item(&slot, "Reforge which item? ",
            "You have nothing to repair or reforge.", (USE_EQUIP | USE_INVEN)))
    {
        item_tester_hook = NULL;
        return false;
    }
    item_tester_hook = NULL;

    if (slot < 0)
        return false;

    object_copy(&smith_backup, smith_o_ptr);
    object_copy(&smith2_backup, smith2_o_ptr);

    if (object_is_damaged_item(&inventory[slot]))
    {
        if (!repair_damaged_item(slot))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot repair that item.");
            return false;
        }

        cave_feat[p_ptr->py][p_ptr->px] -= 1;
        lite_spot(p_ptr->py, p_ptr->px);

        object_desc(new_name, sizeof(new_name), &inventory[slot], true, 0);
        msg_format("You repair %s.", new_name);
    }
    else
    {
        reforge_preview_type preview;

        if (!object_can_reforge_prefix(&inventory[slot]))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot reforge that item.");
            return false;
        }

        prefix_idx = reforge_prefix_menu(&inventory[slot]);
        if (!prefix_idx)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            return false;
        }

        if (!reforge_preview_build(&inventory[slot], prefix_idx, &preview)
            || !preview.affordable)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot afford that reforge.");
            return false;
        }

        object_desc(old_name, sizeof(old_name), &inventory[slot], true, 0);
        object_set_ego_prefix(&inventory[slot], prefix_idx);
        if (!object_apply_ego_affix(&inventory[slot], prefix_idx, true))
        {
            object_set_ego_prefix(&inventory[slot], 0);
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot reforge that item.");
            return false;
        }

        pay_smithing_cost_struct(&preview.cost);
        inventory[slot].unused1 = 2;
        object_aware(&inventory[slot]);
        object_known(&inventory[slot]);
        object_desc(new_name, sizeof(new_name), &inventory[slot], true, 0);
        msg_format("You reforge %s into %s.", old_name, new_name);
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }

    object_copy(smith_o_ptr, &smith_backup);
    object_copy(smith2_o_ptr, &smith2_backup);
    smith_alloy = alloy_backup;
    smith2_alloy = alloy2_backup;

    p_ptr->redraw |= PR_BASIC;
    return true;
}

typedef struct smith_ui_snapshot_scope
{
    bool active;
} smith_ui_snapshot_scope;

typedef struct smith_ui_main_menu_state
{
    bool valid[SMT_MENU_MAX];
    byte row_attr[SMT_MENU_MAX];
} smith_ui_main_menu_state;

static bool smith_ui_snapshot_active(void)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (!app_session_interactions_enabled(session) || !session)
        return false;

    snapshot = app_session_snapshot(session);
    return snapshot && snapshot->scene == APP_SCENE_KIND_DUNGEON;
}

static void smith_ui_snapshot_refresh(void)
{
    (void)Term_xtra(TERM_XTRA_FRESH, 0);
}

static void smith_ui_begin_legacy_screen(void)
{
    screen_save();
    Term_clear();
    smith_ui_reset_description_state();
}

static bool smith_ui_snapshot_scene_enter(smith_ui_snapshot_scope* scope)
{
    app_session* session = app_session_current();

    if (!scope || !session || !smith_ui_snapshot_active())
        return false;

    memset(scope, 0, sizeof(*scope));
    app_session_clear_interaction(session);
    app_session_clear_dungeon_overlay_scene(session);
    scope->active = true;
    return true;
}

static void smith_ui_snapshot_scene_close(smith_ui_snapshot_scope* scope)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !session)
        return;

    app_session_clear_interaction(session);
    app_session_clear_dungeon_overlay_scene(session);
    scope->active = false;
    smith_ui_snapshot_refresh();
}

static bool smith_ui_snapshot_scene_present(smith_ui_snapshot_scope* scope,
    const app_ui_scene* scene)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !scene || !session)
        return false;
    if (!app_session_publish_dungeon_overlay_scene(session, scene))
        return false;

    smith_ui_snapshot_refresh();
    return true;
}

static bool smith_ui_panel_try_add_detail_line(app_ui_panel* panel, byte attr,
    cptr text)
{
    if (!panel || !text || !text[0])
        return false;
    if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
        return true;

    return app_ui_panel_add_detail_line(panel, attr, text);
}

static cptr smith_ui_main_menu_label(int choice)
{
    switch (choice)
    {
    case SMT_MENU_CREATE:
        return "Base Item";
    case SMT_MENU_ENCHANT:
        return "Enchant";
    case SMT_MENU_ARTEFACT:
        return "Artifice";
    case SMT_MENU_NUMBERS:
        return "Numbers";
    case SMT_MENU_MELT:
        return "Melt";
    case SMT_MENU_REPAIR:
        return "Reforge";
    case SMT_MENU_ACCEPT:
        return (p_ptr->smithing_leftover > 0) ? "Resume" : "Accept";
    default:
        return "Smithing";
    }
}

static void smith_ui_main_menu_build_state(smith_ui_main_menu_state* state)
{
    byte attr;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));

    state->valid[SMT_MENU_CREATE - 1] = true;
    state->valid[SMT_MENU_ENCHANT - 1] = (!smith_o_ptr->name1)
        && (!enchant_then_numbers) && (smith_o_ptr->tval != 0)
        && (smith_o_ptr->tval != TV_HORN)
        && !((smith_o_ptr->tval == TV_DIGGING)
            && (smith_o_ptr->sval == SV_SHOVEL));
    state->valid[SMT_MENU_ARTEFACT - 1] = (!object_has_ego(smith_o_ptr))
        && (smith_o_ptr->tval != 0) && (smith_o_ptr->tval != TV_HORN)
        && (p_ptr->self_made_arts
            < z_info->art_self_made_max - z_info->art_rand_max - 2);
    state->valid[SMT_MENU_NUMBERS - 1] = (smith_o_ptr->tval != 0);
    state->valid[SMT_MENU_MELT - 1]
        = meltable_metal_items_carried() && cave_forge_bold(p_ptr->py, p_ptr->px);
    state->valid[SMT_MENU_REPAIR - 1]
        = cave_forge_bold(p_ptr->py, p_ptr->px)
        && (forge_uses(p_ptr->py, p_ptr->px) > 0)
        && p_ptr->active_ability[S_SMT][SMT_REPAIR]
        && (find_reforge_target_item() >= 0);
    state->valid[SMT_MENU_ACCEPT - 1] = affordable(smith_o_ptr)
        && cave_forge_bold(p_ptr->py, p_ptr->px)
        && (forge_uses(p_ptr->py, p_ptr->px) > 0);

    attr = (p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
               || p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
               || p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        ? TERM_WHITE
        : TERM_RED;
    state->row_attr[SMT_MENU_CREATE - 1]
        = state->valid[SMT_MENU_CREATE - 1] ? attr : TERM_L_DARK;

    attr = p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT] ? TERM_WHITE : TERM_RED;
    state->row_attr[SMT_MENU_ENCHANT - 1]
        = state->valid[SMT_MENU_ENCHANT - 1] ? attr : TERM_L_DARK;

    attr = p_ptr->active_ability[S_SMT][SMT_ARTEFACT] ? TERM_WHITE : TERM_RED;
    state->row_attr[SMT_MENU_ARTEFACT - 1]
        = state->valid[SMT_MENU_ARTEFACT - 1] ? attr : TERM_L_DARK;

    state->row_attr[SMT_MENU_NUMBERS - 1]
        = state->valid[SMT_MENU_NUMBERS - 1] ? TERM_WHITE : TERM_L_DARK;
    state->row_attr[SMT_MENU_MELT - 1]
        = state->valid[SMT_MENU_MELT - 1] ? TERM_WHITE : TERM_L_DARK;

    attr = p_ptr->active_ability[S_SMT][SMT_REPAIR] ? TERM_WHITE : TERM_RED;
    state->row_attr[SMT_MENU_REPAIR - 1]
        = state->valid[SMT_MENU_REPAIR - 1] ? attr : TERM_L_DARK;

    state->row_attr[SMT_MENU_ACCEPT - 1]
        = state->valid[SMT_MENU_ACCEPT - 1] ? TERM_WHITE : TERM_L_DARK;
}

static bool smith_ui_main_menu_add_selected_detail(app_ui_panel* panel,
    int highlight)
{
    if (!panel)
        return false;

    switch (highlight)
    {
    case SMT_MENU_CREATE:
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "Start with a new base item.");

    case SMT_MENU_ENCHANT:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Choose a special enchantment to add to the base item."))
        {
            return false;
        }
        if (smith_o_ptr->name1
            && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "(not compatible with Artifice)"))
        {
            return false;
        }
        if (enchant_then_numbers
            && (!smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                    "(Enchantment cannot be changed")
                || !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                    "after using the Numbers menu)")))
        {
            return false;
        }
        return true;

    case SMT_MENU_ARTEFACT:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Design your own artefact."))
        {
            return false;
        }
        if (object_has_ego(smith_o_ptr)
            && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "(not compatible with Enchant)"))
        {
            return false;
        }
        return true;

    case SMT_MENU_NUMBERS:
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "Change the item's key numbers.");

    case SMT_MENU_MELT:
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "Choose a mithril or star-iron item to melt down.");

    case SMT_MENU_REPAIR:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Repair damaged gear or add a prefix to a found item at the forge.")
            || !smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Reforging uses 1.5x the difficulty delta."))
        {
            return false;
        }
        if (!p_ptr->active_ability[S_SMT][SMT_REPAIR]
            && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "(requires the Reforging ability)"))
        {
            return false;
        }
        if (p_ptr->active_ability[S_SMT][SMT_REPAIR]
            && (find_reforge_target_item() < 0)
            && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "(you carry nothing to reforge)"))
        {
            return false;
        }
        return true;

    case SMT_MENU_ACCEPT:
        if (forge_uses(p_ptr->py, p_ptr->px) > 0)
        {
            return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Create the item you have designed. Press Escape to cancel instead.");
        }
        if (cave_forge_bold(p_ptr->py, p_ptr->px))
        {
            return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "This forge has no resources left, so you cannot create items. Press Escape to exit.");
        }
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "You are not at a forge and thus cannot create items. Press Escape to exit.");
    }

    return true;
}

static bool smith_ui_main_menu_add_current_item_detail(app_ui_panel* panel)
{
    char buf[APP_UI_TEXT_MAX];
    char o_desc[80];
    int dif;
    int turn_multiplier = 10;
    byte attr;
    bool can_afford = true;

    if (!panel)
        return false;

    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, " "))
        return false;
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_BLUE,
            "Current design"))
    {
        return false;
    }

    if (p_ptr->smithing_leftover > 0)
    {
        strnfmt(buf, sizeof(buf), "In progress: %d turns left",
            p_ptr->smithing_leftover);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_BLUE, buf))
            return false;
    }

    if (smith_o_ptr->tval == 0)
    {
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "No base item selected yet.");
    }

    object_desc(o_desc, sizeof(o_desc), smith_o_ptr, smith_o_ptr->number > 1, 2);
    strnfmt(buf, sizeof(buf), "%s   %d.%d lb", o_desc,
        smith_o_ptr->weight * smith_o_ptr->number / 10,
        (smith_o_ptr->weight * smith_o_ptr->number) % 10);
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_WHITE, buf))
        return false;

    if (too_difficult(smith_o_ptr))
        attr = TERM_L_DARK;
    else
        attr = TERM_SLATE;

    dif = object_difficulty(smith_o_ptr);
    if ((smithing_cost.drain > 0)
        && (smithing_cost.drain <= p_ptr->skill_base[S_SMT]))
    {
        attr = TERM_BLUE;
    }

    strnfmt(buf, sizeof(buf), "Difficulty: %d (max %d)", dif,
        p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
    if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
        return false;

    if (smithing_cost.uses > 0 && forge_uses(p_ptr->py, p_ptr->px) < smithing_cost.uses)
        can_afford = false;
    if (smithing_cost.drain > 0
        && smithing_cost.drain > p_ptr->skill_base[S_SMT])
    {
        can_afford = false;
    }
    if (smithing_cost.mithril > 0 && smithing_cost.mithril > mithril_carried())
        can_afford = false;
    if (smithing_cost.star_iron > 0
        && smithing_cost.star_iron > star_iron_carried())
    {
        can_afford = false;
    }
    if (smithing_cost.str > 0
        && p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
            - smithing_cost.str
            < -5)
    {
        can_afford = false;
    }
    if (smithing_cost.dex > 0
        && p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
            - smithing_cost.dex
            < -5)
    {
        can_afford = false;
    }
    if (smithing_cost.con > 0
        && p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
            - smithing_cost.con
            < -5)
    {
        can_afford = false;
    }
    if (smithing_cost.gra > 0
        && p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
            - smithing_cost.gra
            < -5)
    {
        can_afford = false;
    }
    if (smithing_cost.exp > 0 && p_ptr->new_exp < smithing_cost.exp)
        can_afford = false;

    if (!smith_ui_panel_try_add_detail_line(panel,
            can_afford ? TERM_SLATE : TERM_L_DARK, "Cost:"))
    {
        return false;
    }

    if (smithing_cost.weaponsmith
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED, "Weaponsmith"))
    {
        return false;
    }
    if (smithing_cost.armoursmith
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED, "Armoursmith"))
    {
        return false;
    }
    if (smithing_cost.jeweller
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED, "Jeweller"))
    {
        return false;
    }
    if (smithing_cost.enchantment
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED, "Enchantment"))
    {
        return false;
    }
    if (smithing_cost.artifice
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED, "Artifice"))
    {
        return false;
    }
    if (smithing_cost.alloy_mastery
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED,
            "Alloy Mastery"))
    {
        return false;
    }
    if (smithing_cost.uses > 0)
    {
        attr = (forge_uses(p_ptr->py, p_ptr->px) >= smithing_cost.uses)
            ? TERM_SLATE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d/%d uses", smithing_cost.uses,
            forge_uses(p_ptr->py, p_ptr->px));
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.drain > 0)
    {
        attr = (smithing_cost.drain <= p_ptr->skill_base[S_SMT])
            ? TERM_BLUE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Smithing", smithing_cost.drain);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.mithril > 0)
    {
        attr = (smithing_cost.mithril <= mithril_carried()) ? TERM_SLATE
                                                             : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d.%d lb Mithril", smithing_cost.mithril / 10,
            smithing_cost.mithril % 10);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.star_iron > 0)
    {
        attr = (smithing_cost.star_iron <= star_iron_carried()) ? TERM_SLATE
                                                                 : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d.%d lb Star Iron",
            smithing_cost.star_iron / 10, smithing_cost.star_iron % 10);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.str > 0)
    {
        attr = (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - smithing_cost.str
                >= -5)
            ? TERM_SLATE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Str", smithing_cost.str);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.dex > 0)
    {
        attr = (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - smithing_cost.dex
                >= -5)
            ? TERM_SLATE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Dex", smithing_cost.dex);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.con > 0)
    {
        attr = (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - smithing_cost.con
                >= -5)
            ? TERM_SLATE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Con", smithing_cost.con);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.gra > 0)
    {
        attr = (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - smithing_cost.gra
                >= -5)
            ? TERM_SLATE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Gra", smithing_cost.gra);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.exp > 0)
    {
        attr = (p_ptr->new_exp >= smithing_cost.exp) ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Exp", smithing_cost.exp);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }

    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
        turn_multiplier /= 2;
    strnfmt(buf, sizeof(buf), "%d Turns", MAX(10, dif * turn_multiplier));
    return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf);
}

static bool smith_ui_main_menu_build_scene(app_ui_scene* scene,
    const smith_ui_main_menu_state* state, int highlight)
{
    app_ui_panel* panel;
    char subtitle[APP_UI_TEXT_MAX];
    int choice;

    if (!scene || !state)
        return false;

    if (highlight < 1 || highlight > SMT_MENU_MAX)
        highlight = SMT_MENU_CREATE;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Smithing");

    if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    {
        SDL_strlcpy(subtitle, "Exploration mode: smithing requires a forge.",
            sizeof(subtitle));
    }
    else if (forge_uses(p_ptr->py, p_ptr->px) == 0)
    {
        SDL_strlcpy(subtitle,
            "Exploration mode: this forge has no resources left.",
            sizeof(subtitle));
    }
    else if (p_ptr->smithing_leftover > 0)
    {
        strnfmt(subtitle, sizeof(subtitle),
            "Current work can be resumed with %d turns left.",
            p_ptr->smithing_leftover);
    }
    else
    {
        SDL_strlcpy(subtitle, "Choose an action for the current design.",
            sizeof(subtitle));
    }
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 selects, 8/2 moves, a-g jumps, Esc/4 backs out.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Select");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Select");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-g", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");

    for (choice = 1; choice <= SMT_MENU_MAX; choice++)
    {
        char key[APP_UI_KEY_MAX];
        byte row_attr;

        strnfmt(key, sizeof(key), "%c", (char)('a' + choice - 1));
        row_attr = (choice == highlight) ? TERM_L_BLUE : state->row_attr[choice - 1];
        if (!app_ui_panel_add_row(panel, (s16b)choice, row_attr,
                state->valid[choice - 1], choice == highlight, key,
                smith_ui_main_menu_label(choice), ""))
        {
            return false;
        }
    }

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
        smith_ui_main_menu_label(highlight));
    return smith_ui_main_menu_add_selected_detail(panel, highlight)
        && smith_ui_main_menu_add_current_item_detail(panel);
}

static int smith_ui_main_menu_hotkey_choice(char ch)
{
    if (ch >= 'A' && ch <= 'G')
        ch = (char)(ch - 'A' + 'a');
    if (ch >= 'a' && ch <= (char)('a' + SMT_MENU_MAX - 1))
        return (int)(ch - 'a') + 1;

    return 0;
}

static int smithing_menu_snapshot(int* highlight)
{
    smith_ui_snapshot_scope scope;

    if (!highlight)
        return 0;
    if (!smith_ui_snapshot_scene_enter(&scope))
        return 0;

    while (true)
    {
        app_ui_scene scene;
        smith_ui_main_menu_state state;
        int choice;
        char ch;

        if (*highlight < 1 || *highlight > SMT_MENU_MAX)
            *highlight = SMT_MENU_CREATE;

        smith_ui_main_menu_build_state(&state);
        if (!smith_ui_main_menu_build_scene(&scene, &state, *highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot hub: failed to build or publish semantic scene");
            smith_ui_snapshot_scene_close(&scope);
            return 0;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_main_menu_hotkey_choice(ch);
        if (choice > 0)
        {
            *highlight = choice;
            if (state.valid[*highlight - 1])
            {
                smith_ui_snapshot_scene_close(&scope);
                return *highlight;
            }
            bell("Invalid choice.");
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            if (state.valid[*highlight - 1])
            {
                smith_ui_snapshot_scene_close(&scope);
                return *highlight;
            }
            bell("Invalid choice.");
            continue;
        }

        if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            smith_ui_snapshot_scene_close(&scope);
            return -1;
        }

        if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (*highlight > 1)
                (*highlight)--;
            else
                *highlight = SMT_MENU_MAX;
            continue;
        }

        if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (*highlight < SMT_MENU_MAX)
                (*highlight)++;
            else
                *highlight = 1;
            continue;
        }
    }
}

/*
 * Performs the interface and selection work for the smithing screen.
 */

int smithing_menu_aux(int* highlight)
{
    char ch;
    byte valid_attr;
    bool valid[SMT_MENU_MAX];
    char buf[80];

    // clear the right of the screen
    wipe_screen_from(COL_SMT2);

    // determine whether or not we can actually make objects here
    if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    {
        Term_putstr(COL_SMT1, 0, -1, TERM_L_BLUE,
            "Exploration mode:  Smithing requires a forge.");
    }
    else if (forge_uses(p_ptr->py, p_ptr->px) == 0)
    {
        Term_putstr(COL_SMT1, 0, -1, TERM_L_BLUE,
            "Exploration mode:  Smithing requires a forge with resources "
            "left.");
    }

    valid[SMT_MENU_CREATE - 1] = true;
    valid[SMT_MENU_ENCHANT - 1] = (!smith_o_ptr->name1)
        && (!enchant_then_numbers) && (smith_o_ptr->tval != 0)
        && (smith_o_ptr->tval != TV_HORN)
        && !((smith_o_ptr->tval == TV_DIGGING)
            && (smith_o_ptr->sval == SV_SHOVEL));
    valid[SMT_MENU_ARTEFACT - 1] = (!object_has_ego(smith_o_ptr))
        && (smith_o_ptr->tval != 0) && (smith_o_ptr->tval != TV_HORN)
        && (p_ptr->self_made_arts
            < z_info->art_self_made_max - z_info->art_rand_max - 2);
    valid[SMT_MENU_NUMBERS - 1] = (smith_o_ptr->tval != 0);
    valid[SMT_MENU_MELT - 1]
        = meltable_metal_items_carried() && cave_forge_bold(p_ptr->py, p_ptr->px);
    valid[SMT_MENU_REPAIR - 1]
        = cave_forge_bold(p_ptr->py, p_ptr->px)
        && (forge_uses(p_ptr->py, p_ptr->px) > 0)
        && p_ptr->active_ability[S_SMT][SMT_REPAIR]
        && (find_reforge_target_item() >= 0);
    valid[SMT_MENU_ACCEPT - 1] = affordable(smith_o_ptr)
        && cave_forge_bold(p_ptr->py, p_ptr->px)
        && (forge_uses(p_ptr->py, p_ptr->px) > 0);

    // display labels
    valid_attr = (p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
                     || p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
                     || p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        ? TERM_WHITE
        : TERM_RED;
    Term_putstr(COL_SMT1, 2, -1,
        valid[SMT_MENU_CREATE - 1] ? valid_attr : TERM_L_DARK, "a) Base Item");
    valid_attr = (p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT]) ? TERM_WHITE
                                                                 : TERM_RED;
    Term_putstr(COL_SMT1, 3, -1,
        valid[SMT_MENU_ENCHANT - 1] ? valid_attr : TERM_L_DARK, "b) Enchant");
    valid_attr
        = (p_ptr->active_ability[S_SMT][SMT_ARTEFACT]) ? TERM_WHITE : TERM_RED;
    Term_putstr(COL_SMT1, 4, -1,
        valid[SMT_MENU_ARTEFACT - 1] ? valid_attr : TERM_L_DARK, "c) Artifice");
    Term_putstr(COL_SMT1, 5, -1,
        valid[SMT_MENU_NUMBERS - 1] ? TERM_WHITE : TERM_L_DARK, "d) Numbers");
    Term_putstr(COL_SMT1, 6, -1,
        valid[SMT_MENU_MELT - 1] ? TERM_WHITE : TERM_L_DARK, "e) Melt");
    valid_attr = p_ptr->active_ability[S_SMT][SMT_REPAIR] ? TERM_WHITE : TERM_RED;
    Term_putstr(COL_SMT1, 7, -1,
        valid[SMT_MENU_REPAIR - 1] ? valid_attr : TERM_L_DARK, "f) Reforge");

    if (p_ptr->smithing_leftover == 0)
    {
        Term_putstr(COL_SMT1, 8, -1,
            valid[SMT_MENU_ACCEPT - 1] ? TERM_WHITE : TERM_L_DARK, "g) Accept");
    }
    else
    {
        Term_putstr(COL_SMT1, 8, -1,
            valid[SMT_MENU_ACCEPT - 1] ? TERM_WHITE : TERM_L_DARK, "g) Resume");
    }

    // display information about the selected item
    switch (*highlight)
    {
    case SMT_MENU_CREATE:
    {
        Term_putstr(
            COL_SMT2 + 2, 2, -1, TERM_SLATE, "Start with a new base item.");
        break;
    }
    case SMT_MENU_ENCHANT:
    {
        Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
            "Choose a special enchantment to add");
        Term_putstr(COL_SMT2 + 2, 3, -1, TERM_SLATE, "to the base item.");
        if (smith_o_ptr->name1)
            Term_putstr(COL_SMT2 + 2, 5, -1, TERM_L_DARK,
                "(not compatible with Artifice)");
        if (enchant_then_numbers)
        {
            Term_putstr(COL_SMT2 + 2, 5, -1, TERM_L_DARK,
                "(Enchantment cannot be changed");
            Term_putstr(COL_SMT2 + 2, 6, -1, TERM_L_DARK,
                "after using the Numbers menu)");
        }
        break;
    }
    case SMT_MENU_ARTEFACT:
    {
        Term_putstr(
            COL_SMT2 + 2, 2, -1, TERM_SLATE, "Design your own artefact.");
        if (object_has_ego(smith_o_ptr))
            Term_putstr(COL_SMT2 + 2, 4, -1, TERM_L_DARK,
                "(not compatible with Enchant)");
        break;
    }
    case SMT_MENU_NUMBERS:
    {
        Term_putstr(
            COL_SMT2 + 2, 2, -1, TERM_SLATE, "Change the item's key numbers.");
        break;
    }
    case SMT_MENU_MELT:
    {
        Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
            "Choose a mithril or star-iron item");
        Term_putstr(
            COL_SMT2 + 2, 3, -1, TERM_SLATE, "to melt down.");
        break;
    }
    case SMT_MENU_REPAIR:
    {
        Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
            "Repair damaged gear or add a prefix");
        Term_putstr(COL_SMT2 + 2, 3, -1, TERM_SLATE,
            "to a found item at the forge.");
        Term_putstr(COL_SMT2 + 2, 4, -1, TERM_SLATE,
            "Reforging uses 1.5x the difficulty delta.");
        if (!p_ptr->active_ability[S_SMT][SMT_REPAIR])
            Term_putstr(COL_SMT2 + 2, 5, -1, TERM_L_DARK,
                "(requires the Reforging ability)");
        else if (find_reforge_target_item() < 0)
            Term_putstr(COL_SMT2 + 2, 5, -1, TERM_L_DARK,
                "(you carry nothing to reforge)");
        break;
    }
    case SMT_MENU_ACCEPT:
    {
        if (forge_uses(p_ptr->py, p_ptr->px) > 0)
        {
            Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
                "Create the item you have designed.");
            Term_putstr(COL_SMT2 + 2, 4, -1, TERM_SLATE,
                "(to cancel it instead, just press Escape)");
        }
        else if (cave_forge_bold(p_ptr->py, p_ptr->px))
        {
            Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
                "This forge has no resources left, so you");
            Term_putstr(COL_SMT2 + 2, 3, -1, TERM_SLATE,
                "cannot create items. To exit, press Escape.");
        }
        else
        {
            Term_putstr(COL_SMT2 + 2, 2, -1, TERM_SLATE,
                "You are not at a forge and thus cannot");
            Term_putstr(COL_SMT2 + 2, 3, -1, TERM_SLATE,
                "create items. To exit, press Escape.");
        }
        break;
    }
    }

    // highlight the label
    strnfmt(buf, 80, "%c)", (char)'a' + *highlight - 1);
    Term_putstr(COL_SMT1, *highlight + 1, -1, TERM_L_BLUE, buf);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(2, 1 + *highlight);

    /* Get key (while allowing menu commands) */
    ch = smith_ui_inkey_with_wait_reason();

    // choose an option by letter
    if ((ch >= 'a') && (ch <= (char)'a' + SMT_MENU_MAX - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        // move the light blue highlight
        move_displayed_highlight(old_highlight,
            valid[old_highlight - 1] ? TERM_WHITE : TERM_L_DARK, *highlight,
            COL_SMT1);

        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = SMT_MENU_MAX;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < SMT_MENU_MAX)
            (*highlight)++;
        else if (*highlight == SMT_MENU_MAX)
            *highlight = 1;
    }

    /* Leave menu */
    if ((ch == ESCAPE) || (ch == '4'))
    {
        return (-1);
    }

    return (0);
}

/*
 * Brings up a screen for making new items (only works at a forge).
 * Leads to many submenus which help to determine the item's attributes.
 */
void do_cmd_smithing_screen(void)
{
    app_wait_scope wait_scope;
    int actiontype = -1;
    int highlight = 1;
    bool leave_menu = false;
    bool create = false;
    bool use_snapshot_menu = smith_ui_snapshot_active();
    bool legacy_screen_active = false;

    app_session_push_wait_scope(app_session_current(), &wait_scope,
        APP_WAIT_REASON_LIST_SELECTION, 0, 0);

    // if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    //{
    //	msg_print("You can only create items at a forge.");
    //	return;
    //}

    if (cave_forge_bold(p_ptr->py, p_ptr->px)
        && forge_uses(p_ptr->py, p_ptr->px) == 0)
    {
        msg_print("The resources of this forge are exhausted.");
        msg_print(
            "You will be able to browse the options but not make new things.");
    }

    if (!use_snapshot_menu)
    {
        legacy_screen_active = true;
        smith_ui_begin_legacy_screen();
    }

    // Hack: flag that we are in the middle of smithing
    p_ptr->smithing = 1;

    // deal with previous interruptions
    if (p_ptr->smithing_leftover > 0)
    {
        // default to 'resume' if an item is already in progress
        highlight = SMT_MENU_ACCEPT;

        // and backup the smithing item
        object_copy(smith2_o_ptr, smith_o_ptr);
        smith2_alloy = smith_alloy;
    }

    // otherwise wipe the smithing item
    else
    {
        object_wipe(smith_o_ptr);
        smith_clear_alloy_state(&smith_alloy);
    }

    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        actiontype = use_snapshot_menu ? smithing_menu_snapshot(&highlight)
                                       : smithing_menu_aux(&highlight);
        if (use_snapshot_menu && actiontype == 0)
        {
            use_snapshot_menu = false;
            legacy_screen_active = true;
            smith_ui_begin_legacy_screen();
            continue;
        }

        // if an action has been selected...
        switch (actiontype)
        {
        case SMT_MENU_CREATE:
        {
            // this is not a resumption of smithing an item
            p_ptr->smithing_leftover = 0;

            create_tval_menu();

            // backup the smithing object
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;

            break;
        }
        case SMT_MENU_ENCHANT:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                if (!enchant_menu())
                {
                    // restore the smithing object
                    object_copy(smith_o_ptr, smith2_o_ptr);
                    smith_alloy = smith2_alloy;
                }
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_ARTEFACT:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                artefact_menu();
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_NUMBERS:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                numbers_menu();

                // backup the smithing object
                object_copy(smith2_o_ptr, smith_o_ptr);
                smith2_alloy = smith_alloy;
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_MELT:
        {
            if (meltable_metal_items_carried())
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                melt_menu();
            }
            else
            {
                bell("You don't have any mithril or star-iron items.");
            }

            break;
        }
        case SMT_MENU_REPAIR:
        {
            smith_reforge_item();
            break;
        }
        case SMT_MENU_ACCEPT:
        {
            if (smithing_cost.drain > 0)
            {
                char buf[80];

                sprintf(buf,
                    "This will drain your smithing skill by %d points. "
                    "Proceed? ",
                    smithing_cost.drain);
                if (!get_check(buf))
                    break;
            }

            create = true;
            leave_menu = true;
            break;
        }
        case -1:
        {
            leave_menu = true;
            break;
        }
        }
    }

    if (create)
    {
        int turn_multiplier = 10;

        if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
        {
            turn_multiplier /= 2;
        }

        // Display a message
        msg_print("You begin your work.");

        // add the details to the artefact type if applicable
        if (smith_o_ptr->name1)
            add_artefact_details();

        /* Cancel stealth mode */
        p_ptr->stealth_mode = false;

        // Allow the resumption of interrupted smithing
        if (p_ptr->smithing_leftover > 0)
        {
            p_ptr->smithing = p_ptr->smithing_leftover;
        }
        else
        {
            // Set smithing counter
            p_ptr->smithing
                = MAX(10, object_difficulty(smith_o_ptr) * turn_multiplier);

            // Also set the smithing leftover counter (to allow you to resume if
            // interrupted)
            p_ptr->smithing_leftover = p_ptr->smithing;
        }

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Handle stuff */
        handle_stuff();

        /* Refresh */
        Term_fresh();
    }

    else
    {
        if (p_ptr->smithing_leftover == 0)
        {
            /* Wipe the smithing object */
            object_wipe(smith_o_ptr);
            smith_clear_alloy_state(&smith_alloy);
        }

        // Hack: flag that we are done with smithing
        p_ptr->smithing = 0;
    }

    if (legacy_screen_active)
    {
        /* Load screen */
        smith_ui_reset_description_state();
        screen_load();
    }
    else
    {
        app_session_clear_dungeon_overlay_scene(app_session_current());
    }
    app_session_clear_interaction(app_session_current());
    app_session_pop_wait_scope(app_session_current(), &wait_scope);
}

/*
 * Actually creates the item.
 */

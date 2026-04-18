/* File: object-randart-generation.c */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "object/object-randart-internal.h"
#include "object/object-randart.h"

/* Inhibiting factors for large bonus values */
#define INHIBIT_STRONG 6
#define INHIBIT_WEAK 4

/* Numerical index values for the different types */
#define CAT_STATS 0
#define CAT_SPEED 1
#define CAT_SLAYS 2
#define CAT_BRANDS 3
#define CAT_RESISTS 4
#define CAT_ABILITIES 5
#define CAT_TUNNEL 6
#define CAT_IMPACT 7
#define CAT_WEAP_XTRA 8
#define CAT_BOW_XTRA 9
#define CAT_STEALTH 10
#define CAT_VISION 11
#define CAT_COMBAT 12
#define CAT_TO_AC 13
#define CAT_TO_BASE 14
#define CAT_WEIGH_LESS 15
#define CAT_LITE 16
#define CAT_MAX 17

/* The different types of artefacts */
#define ART_TYPE_WEAPON 0
#define ART_TYPE_SHOVEL 1
#define ART_TYPE_BOW 2
#define ART_TYPE_SPECIAL 3
#define ART_TYPE_ARMOR 4
#define ART_TYPE_DRAG_ARMOR 5
#define ART_TYPE_CLOAK 6
#define ART_TYPE_SHIELD 7
#define ART_TYPE_HELM 8
#define ART_TYPE_CROWN 9
#define ART_TYPE_GLOVES 10
#define ART_TYPE_BOOTS 11
#define ART_TYPE_MAX 12

#define ART_THEME_MAX 12
#define COL_THEME_FREQ 0
#define COL_THEME_MIN 1
#define COL_THEME_DROP_TYPE 2
#define COL_MAX 3

#define MIN_ENFORCEMENT 50
#define NORMAL_FREQUENCY 10

#define TR2_LOW_RESIST (TR2_RES_FEAR)
#define TR2_HIGH_RESIST (TR2_RES_STUN | TR2_RES_BLIND | TR2_RES_CONFU)

#define NUM_FAVORED_SLAY_PAIRS 4
#define NUM_FAVORED_RESIST_PAIRS 2

#define MEAN_DAM_INCREMENT 3

extern const byte table_type_freq[ART_TYPE_MAX][CAT_MAX];
extern u16b art_freq[CAT_MAX];
extern const byte theme_type[ART_THEME_MAX][COL_MAX];
extern int art_theme_freq[ART_THEME_MAX];
extern const byte table_stat_freq[ART_TYPE_MAX][A_MAX];
extern byte art_stat_freq[A_MAX];
extern const int table_ability_freq[ART_TYPE_MAX][OBJECT_XTRA_SIZE_POWER];
extern byte art_abil_freq[OBJECT_XTRA_SIZE_POWER];
extern const u32b favored_slay_pairs[NUM_FAVORED_SLAY_PAIRS][2];
extern const u32b favored_resist_pairs[NUM_FAVORED_RESIST_PAIRS][2];
extern s16b* kinds;
extern s32b* base_power;
extern byte* base_item_level;
extern byte* base_item_rarity;
extern byte* base_art_rarity;
extern s16b cur_art_k_idx;

/*
 * Store the original artefact power ratings as a baseline
 */
void store_base_power(void)
{
    int i;
    artefact_type* a_ptr;
    s16b k_idx;

    for (i = 0; i < z_info->art_norm_max; i++)
    {
        /* First store the base power of each item */
        base_power[i] = artefact_power(i);
    }

    for (i = 0; i < z_info->art_norm_max; i++)
    {
        int y;
        bool found_rarity = false;
        alloc_entry* table = alloc_kind_table;

        /* Kinds array was populated in the above step in artefact_power */
        k_idx = kinds[i];
        a_ptr = &a_info[i];

        /* Process probabilities */
        for (y = 0; y < alloc_kind_size; y++)
        {
            if (k_idx != table[y].index)
                continue;

            /* The table is sorted by depth, just use the lowest one */
            base_item_level[i] = table[y].level;

            /* The rarity tables are divided by 100 in the prob_table */
            base_item_rarity[i] = 100 / table[y].prob2;

            /* Paranoia */
            if (base_item_rarity[i] < 1)
                base_item_rarity[i] = 1;

            found_rarity = true;

            break;
        }

        /* Whoops!  Just make something up */
        if (!found_rarity)
        {
            base_item_level[i] = 1;
            base_item_rarity[i] = 1;
        }

        base_art_rarity[i] = a_ptr->rarity;
    }
}

/*
 * We've just added an ability which uses the pval bonus.  Make sure it's
 * not zero.  If it's currently negative, leave it negative (heh heh).
 */
static void do_pval(artefact_type* a_ptr)
{
    if (a_ptr->pval == 0)
    {
        a_ptr->pval = (s16b)(1 + rand_int(2));
    }
    else if (a_ptr->pval < 0)
    {
        if (one_in_(2))
        {
            a_ptr->pval--;
        }
    }
    /* put reasonable limits on stat increases */
    else if (a_ptr->flags1 & TR1_ALL_STATS)
    {
        if (a_ptr->pval > 6)
            a_ptr->pval = 6;
        else if (a_ptr->pval < 6)
            if (one_in_(a_ptr->pval))
                a_ptr->pval++;

        /* = 6 or 7 */
        if (one_in_(INHIBIT_STRONG))
            a_ptr->pval++;
    }
    else if (one_in_(a_ptr->pval))
    {
        /*
         * CR: made this a bit rarer and diminishing with higher pval -
         * also rarer if item has blows/might/shots already
         */
        a_ptr->pval++;
    }
}

void remove_contradictory(artefact_type* a_ptr)
{
    if (a_ptr->flags2 & TR2_AGGRAVATE)
        a_ptr->flags1 &= ~(TR1_STL);

    if (a_ptr->pval < 0)
    {
        if (a_ptr->flags1 & TR1_STR)
            a_ptr->flags2 &= ~(TR2_SUST_STR);
        if (a_ptr->flags1 & TR1_DEX)
            a_ptr->flags2 &= ~(TR2_SUST_DEX);
        if (a_ptr->flags1 & TR1_CON)
            a_ptr->flags2 &= ~(TR2_SUST_CON);
        if (a_ptr->flags1 & TR1_GRA)
            a_ptr->flags2 &= ~(TR2_SUST_GRA);
    }
    if (a_ptr->pval > 0)
    {
        if (a_ptr->flags1 & TR1_NEG_STR)
            a_ptr->flags2 &= ~(TR2_SUST_STR);
        if (a_ptr->flags1 & TR1_NEG_DEX)
            a_ptr->flags2 &= ~(TR2_SUST_DEX);
        if (a_ptr->flags1 & TR1_NEG_CON)
            a_ptr->flags2 &= ~(TR2_SUST_CON);
        if (a_ptr->flags1 & TR1_NEG_GRA)
            a_ptr->flags2 &= ~(TR2_SUST_GRA);
    }
}

/*
 * Choose a random ability using weights based on the given ability frequency
 * table.  The function returns false if no ability can be added.
 */
static bool add_ability(artefact_type* a_ptr)
{
    int abil_selector, abil_counter, counter, abil_freq_total;
    u32b flag;

    /* find out the current frequency total */
    abil_freq_total = 0;

    flag = OBJECT_XTRA_BASE_POWER;

    for (abil_counter = 0; abil_counter < OBJECT_XTRA_SIZE_POWER;
         abil_counter++)
    {
        /* we already have this one added */
        if (a_ptr->flags3 & flag)
        {
            /* Don't try to add it again */
            art_abil_freq[abil_counter] = 0;
        }
        else
        {
            abil_freq_total += art_abil_freq[abil_counter];
        }

        /* shift the bit to check for the next ability */
        flag = flag << 1;
    }

    /* We don't have anything else to add */
    if (abil_freq_total == 0)
        return false;

    /* Generate a random number between 1 and current ability total */
    abil_selector = dieroll(abil_freq_total);

    flag = OBJECT_XTRA_BASE_POWER;

    /* Find the entry in the table that this number represents. */
    counter = 0;
    for (abil_counter = 0; abil_counter < OBJECT_XTRA_SIZE_POWER;
         abil_counter++)
    {
        counter += art_abil_freq[abil_counter];

        /* we found the choice, stop and return the category */
        if (counter >= abil_selector)
            break;

        /* shift the bit to check for the next ability */
        flag = flag << 1;
    }

    /* Wee have the flag to add */
    a_ptr->flags3 |= flag;

    return true;
}

/*
 * Sustain a sustain.  Try hard to add one that is positive.
 */
static bool add_sustain(artefact_type* a_ptr)
{
    int stat_selector, stat_counter, counter, stat_freq_total;
    u32b sust_flag, stat_flag;

    static byte art_sust_freq[A_MAX];

    /* find out the current frequency total */
    stat_freq_total = 0;

    sust_flag = OBJECT_XTRA_BASE_SUSTAIN;
    stat_flag = OBJECT_XTRA_BASE_STAT_ADD;

    for (stat_counter = 0; stat_counter < A_MAX; stat_counter++)
    {
        /* we already have this one added */
        if (a_ptr->flags2 & sust_flag)
        {
            /* Don't try to add it again */
            art_sust_freq[stat_counter] = 0;
        }
        else
        {
            stat_freq_total += art_stat_freq[stat_counter];
            art_sust_freq[stat_counter] = art_stat_freq[stat_counter];

            /*
             * Hack - add in a heavy bias for positive stats that aren't
             * sustained yet
             */
            if ((a_ptr->flags1 & stat_flag) && (a_ptr->pval > 0))
            {
                stat_freq_total += 100;
                art_sust_freq[stat_counter] += 100;
            }
        }

        /* shift the bit to check for the next stat */
        sust_flag = sust_flag << 1;
        stat_flag = stat_flag << 1;
    }

    /* We don't have any stat to sustain */
    if (stat_freq_total == 0)
        return false;

    /* Generate a random number between 1 and current stat total */
    stat_selector = dieroll(stat_freq_total);

    sust_flag = OBJECT_XTRA_BASE_STAT_ADD;

    /* Find the entry in the table that this number represents. */
    counter = 0;
    for (stat_counter = 0; stat_counter < A_MAX; stat_counter++)
    {
        counter += art_sust_freq[stat_counter];

        /* we found the choice, stop and return the category */
        if (counter >= stat_selector)
            break;

        /* shift the bit to check for the next stat */
        sust_flag = sust_flag << 1;
    }

    /* Wee have the flag to add */
    a_ptr->flags2 |= sust_flag;

    return true;
}

static bool add_stat(artefact_type* a_ptr)
{
    int stat_selector, stat_counter, counter, stat_freq_total;
    u32b flag_stat_add, flag_sustain;

    /* find out the current frequency total */
    stat_freq_total = 0;

    flag_stat_add = OBJECT_XTRA_BASE_STAT_ADD;

    for (stat_counter = 0; stat_counter < A_MAX; stat_counter++)
    {
        /* we already have this one added */
        if (a_ptr->flags1 & flag_stat_add)
        {
            art_stat_freq[stat_counter] = 0;
        }
        else
        {
            stat_freq_total += art_stat_freq[stat_counter];
        }

        /* shift the bit to check for the next stat */
        flag_stat_add = flag_stat_add << 1;
    }

    /* We don't have any stat to add */
    if (stat_freq_total == 0)
        return false;

    /* Generate a random number between 1 and current stat total */
    stat_selector = dieroll(stat_freq_total);

    flag_stat_add = OBJECT_XTRA_BASE_STAT_ADD;
    flag_sustain = OBJECT_XTRA_BASE_SUSTAIN;

    /* Find the entry in the table that this number represents. */
    counter = 0;
    for (stat_counter = 0; stat_counter < A_MAX; stat_counter++)
    {
        counter += art_stat_freq[stat_counter];

        /* we found the choice, stop and return the category */
        if (counter >= stat_selector)
            break;

        /* shift the bit to check for the next stat */
        flag_stat_add = flag_stat_add << 1;
        flag_sustain = flag_sustain << 1;
    }

    /* Wee have the flag to add */
    a_ptr->flags1 |= flag_stat_add;

    /* 50% of the time, add the sustain as well */
    if (one_in_(2))
    {
        /* We don't have this one.  Add it */
        a_ptr->flags2 |= flag_sustain;
    }

    /* re-do the pval */
    do_pval(a_ptr);

    return true;
}

/*
 * Add a resist, with all applicable resists having an equal chance.
 * This function could really be used to add anything to flags2
 * so long as each one has an equal chance
 */
static bool add_one_resist(artefact_type* a_ptr, u32b avail_flags)
{
    u32b has_flag_mask = 0L;
    byte i, counter;
    u32b flag_holder = 0x00000001;
    byte number_of_flags = 0;

    has_flag_mask |= a_ptr->flags2;

    /* Limit this to only the relevant flags */
    has_flag_mask &= avail_flags;

    /* first count all the flags */
    for (i = 0; i < 32; i++)
    {
        /*
         * the flag is part of the mask, and the artefact doesn't already have
         * it
         */
        if ((avail_flags & flag_holder) && (!(has_flag_mask & flag_holder)))
            number_of_flags++;

        /* shift to the next bit */
        flag_holder = flag_holder << 1;
    }

    /* no available flags */
    if (number_of_flags == 0)
        return false;

    /* select a flag */
    counter = dieroll(number_of_flags);

    /* re-set some things */
    number_of_flags = 0;
    flag_holder = 0x00000001;

    /* first count all the flags */
    for (i = 0; i < 32; i++)
    {
        if ((avail_flags & flag_holder) && (!(has_flag_mask & flag_holder)))
            number_of_flags++;

        /* We found the flag - stop */
        if (number_of_flags == counter)
            break;

        /* shift to the next bit */
        flag_holder = flag_holder << 1;
    }

    /* add the flag and return */
    a_ptr->flags2 |= flag_holder;

    /* try to add some of the complimentary pairs of resists */
    for (counter = 0; counter < NUM_FAVORED_RESIST_PAIRS; counter++)
    {
        if ((flag_holder == favored_resist_pairs[counter][0]) && (one_in_(2)))
        {
            a_ptr->flags2 |= (favored_resist_pairs[counter][1]);
            break;
        }
    }

    return true;
}

static bool add_brand(artefact_type* a_ptr)
{
    /* Hack - if all brands are added already, exit to avoid infinite loop */
    if ((a_ptr->flags1 & TR1_BRAND_ELEC) && (a_ptr->flags1 & TR1_BRAND_COLD)
        && (a_ptr->flags1 & TR1_BRAND_FIRE) && (a_ptr->flags1 & TR1_BRAND_POIS))
    {
        return false;
    }

    /* Make sure we add one that hasn't been added yet */
    while (true)
    {
        u32b brand_flag = OBJECT_XTRA_BASE_BRAND;

        int r = rand_int(OBJECT_XTRA_SIZE_BRAND);

        /* use bit operations to get to the right stat flag */
        brand_flag = brand_flag << r;

        /* We already have this one */
        if (a_ptr->flags1 & brand_flag)
            continue;

        /* We don't have this one.  Add it */
        a_ptr->flags1 |= brand_flag;

        /* 50% of the time, add the corresponding resist. */
        if (one_in_(2))
        {
            u32b res_flag = OBJECT_XTRA_BASE_LOW_RESIST;
            res_flag = res_flag << r;
            a_ptr->flags2 |= res_flag;
        }

        /* Get out of the loop */
        break;
    }

    return true;
}

/*
 * Add a slay or kill, return false if the artefact has all of them are all
 * full.
 */
static bool add_slay(artefact_type* a_ptr)
{
    byte art_slay_freq[OBJECT_XTRA_SIZE_SLAY];

    int slay_selector, slay_counter, counter, slay_freq_total;
    u32b flag_slay_add;

    /* find out the current frequency total */
    slay_freq_total = 0;

    flag_slay_add = OBJECT_XTRA_BASE_SLAY;

    /* first check the slays */
    for (slay_counter = 0; slay_counter < OBJECT_XTRA_SIZE_SLAY;
         slay_counter++)
    {
        /* hack - don't add a slay when we already have it */
        if (a_ptr->flags1 & flag_slay_add)
            art_slay_freq[slay_counter] = 0;

        /* We don't have this one */
        else
        {
            if (slay_counter < OBJECT_XTRA_SIZE_SLAY)
                art_slay_freq[slay_counter] = NORMAL_FREQUENCY * 2;
            else
                art_slay_freq[slay_counter] = NORMAL_FREQUENCY / 2;
        }

        slay_freq_total += art_slay_freq[slay_counter];

        /* shift the bit to check for the next stat */
        flag_slay_add = flag_slay_add << 1;
    }

    /* We don't have any stat to add */
    if (slay_freq_total == 0)
        return false;

    /* Generate a random number between 1 and current stat total */
    slay_selector = dieroll(slay_freq_total);

    flag_slay_add = OBJECT_XTRA_BASE_SLAY;

    /* Find the entry in the table that this number represents. */
    counter = 0;
    for (slay_counter = 0; slay_counter < OBJECT_XTRA_SIZE_SLAY;
         slay_counter++)
    {
        counter += art_stat_freq[slay_counter];

        /* we found the choice, stop and return the category */
        if (counter >= slay_selector)
            break;

        /* shift the bit to check for the next stat */
        flag_slay_add = flag_slay_add << 1;
    }

    /* Wee have the flag to add */
    a_ptr->flags1 |= flag_slay_add;

    /* try to add some of the complimentary pairs of slays */
    for (counter = 0; counter < NUM_FAVORED_SLAY_PAIRS; counter++)
    {
        if ((flag_slay_add == favored_slay_pairs[counter][0]) && (one_in_(2)))
        {
            a_ptr->flags1 |= (favored_slay_pairs[counter][1]);
            break;
        }
    }

    return true;
}

static void add_att(artefact_type* a_ptr, int fixed, int random)
{
    switch (a_ptr->tval)
    {
    /* Is it a weapon? */
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    {
        break;
    }
    default:
    {
        if ((a_ptr->att > 7) && (!one_in_(INHIBIT_WEAK)))
            return;

        if (a_ptr->att < 3)
            a_ptr->att += damroll(2, 2);
        else
            a_ptr->att++;
        return;
    }
    }

    /* if cursed, make it worse */
    if (a_ptr->att < 0)
    {
        a_ptr->att--;
        return;
    }

    /* Inhibit above certain threshholds */
    if (a_ptr->att > 25)
    {
        if (one_in_(INHIBIT_STRONG))
            a_ptr->att++;
        return;
    }
    else if (a_ptr->att > 15)
    {
        if (one_in_(INHIBIT_WEAK))
            a_ptr->att += dieroll(2);
        return;
    }
    else if (a_ptr->att > 5)
    {
        random /= 2;
    }

    a_ptr->att += (fixed + rand_int(random));
}

/* Sil-y: all this does presently is adds to att, since to_d was removed */
static void add_to_dam(artefact_type* a_ptr)
{
    switch (a_ptr->tval)
    {
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    {
        break;
    }
    default:
    {
        if ((a_ptr->att > 7) && (!one_in_(INHIBIT_WEAK)))
            return;

        if (a_ptr->att < 3)
            a_ptr->att += damroll(2, 2);
        else
            a_ptr->att++;
        return;
    }
    }
}

static void add_to_ac(artefact_type* a_ptr, int fixed, int random)
{
    a_ptr->evn += (s16b)(fixed + rand_int(random));
}

void artefact_apply_pval_stat_skill_bonuses(artefact_type* a_ptr)
{
    if (!a_ptr)
        return;

    for (int i = 0; i < A_MAX; i++)
        a_ptr->stat_bonus[i] = 0;
    for (int i = 0; i < S_MAX; i++)
        a_ptr->skill_bonus[i] = 0;

    const s16b pval = a_ptr->pval;

    if (a_ptr->flags1 & TR1_STR)
        a_ptr->stat_bonus[A_STR] += pval;
    if (a_ptr->flags1 & TR1_DEX)
        a_ptr->stat_bonus[A_DEX] += pval;
    if (a_ptr->flags1 & TR1_CON)
        a_ptr->stat_bonus[A_CON] += pval;
    if (a_ptr->flags1 & TR1_GRA)
        a_ptr->stat_bonus[A_GRA] += pval;

    if (a_ptr->flags1 & TR1_NEG_STR)
        a_ptr->stat_bonus[A_STR] -= pval;
    if (a_ptr->flags1 & TR1_NEG_DEX)
        a_ptr->stat_bonus[A_DEX] -= pval;
    if (a_ptr->flags1 & TR1_NEG_CON)
        a_ptr->stat_bonus[A_CON] -= pval;
    if (a_ptr->flags1 & TR1_NEG_GRA)
        a_ptr->stat_bonus[A_GRA] -= pval;

    if (a_ptr->flags1 & TR1_MEL)
        a_ptr->skill_bonus[S_MEL] += pval;
    if (a_ptr->flags1 & TR1_ARC)
        a_ptr->skill_bonus[S_ARC] += pval;
    if (a_ptr->flags1 & TR1_STL)
        a_ptr->skill_bonus[S_STL] += pval;
    if (a_ptr->flags1 & TR1_PER)
        a_ptr->skill_bonus[S_PER] += pval;
    if (a_ptr->flags1 & TR1_WIL)
        a_ptr->skill_bonus[S_WIL] += pval;
    if (a_ptr->flags1 & TR1_SMT)
        a_ptr->skill_bonus[S_SMT] += pval;
    if (a_ptr->flags1 & TR1_SNG)
        a_ptr->skill_bonus[S_SNG] += pval;
}

/* prepare a basic-non-magic artefact template based on the base object */
void artefact_prep(s16b k_idx, int a_idx)
{
    object_kind* k_ptr = &k_info[k_idx];
    artefact_type* a_ptr = &a_info[a_idx];

    a_ptr->tval = k_ptr->tval;
    a_ptr->sval = k_ptr->sval;
    a_ptr->pval = k_ptr->pval;
    a_ptr->att = k_ptr->att;
    a_ptr->dd = k_ptr->dd;
    a_ptr->ds = k_ptr->ds;
    a_ptr->evn = k_ptr->evn;
    a_ptr->pd = k_ptr->pd;
    a_ptr->ps = k_ptr->ps;
    a_ptr->weight = k_ptr->weight;
    for (int si = 0; si < A_MAX; si++)
        a_ptr->stat_bonus[si] = k_ptr->stat_bonus[si];
    for (int sk = 0; sk < S_MAX; sk++)
        a_ptr->skill_bonus[sk] = k_ptr->skill_bonus[sk];
    a_ptr->flags1 = k_ptr->flags1;
    a_ptr->flags2 = k_ptr->flags2;
    a_ptr->flags3 = k_ptr->flags3;

    /* Artefacts ignore everything */
    a_ptr->flags3 |= TR3_IGNORE_MASK;

    /* Assign basic stats to the artefact based on its artefact level. */
    switch (a_ptr->tval)
    {
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_SWORD:
    case TV_POLEARM:
    {
        a_ptr->att += (s16b)(a_ptr->level / 10 + rand_int(4) + rand_int(4));
        break;
    }
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        break;
    }
    default:
        break;
    }
}

/*
 * Build a suitable frequency table for this item, based on the object type.
 * This must be called before any randart can be made.
 */
void build_freq_table(artefact_type* a_ptr)
{
    int i;

    byte art_type;

    switch (a_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        art_type = ART_TYPE_WEAPON;
        break;
    }
    case TV_DIGGING:
    {
        art_type = ART_TYPE_SHOVEL;
        break;
    }
    case TV_BOW:
    {
        art_type = ART_TYPE_BOW;
        break;
    }
    case TV_RING:
    case TV_AMULET:
    case TV_LIGHT:
    {
        art_type = ART_TYPE_SPECIAL;
        break;
    }
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    {
        art_type = ART_TYPE_ARMOR;
        break;
    }
    case TV_CLOAK:
    {
        art_type = ART_TYPE_CLOAK;
        break;
    }
    case TV_SHIELD:
    {
        art_type = ART_TYPE_SHIELD;
        break;
    }
    case TV_HELM:
    {
        art_type = ART_TYPE_HELM;
        break;
    }
    case TV_CROWN:
    {
        art_type = ART_TYPE_CROWN;
        break;
    }
    case TV_BOOTS:
    {
        art_type = ART_TYPE_BOOTS;
        break;
    }
    case TV_GLOVES:
    {
        art_type = ART_TYPE_GLOVES;
        break;
    }
    default:
        return;
    }

    /* Load the frequencies */
    for (i = 0; i < CAT_MAX; i++)
    {
        art_freq[i] = table_type_freq[art_type][i];
    }

    /* load the stat frequency table */
    for (i = 0; i < A_MAX; i++)
    {
        art_stat_freq[i] = NORMAL_FREQUENCY + table_stat_freq[art_type][i];
    }

    /* Load the abilities frequency table */
    for (i = 0; i < OBJECT_XTRA_SIZE_POWER; i++)
    {
        art_abil_freq[i] = table_ability_freq[art_type][i];
    }

    /* Get the current k_idx */
    cur_art_k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
}

/*
 * Try very hard to increase weighting to succeed in creating minimum values.
 */
void adjust_art_freq_table(void)
{
    byte i;

    int art_min_total = 0;

    for (i = 0; i < ART_THEME_MAX; i++)
    {
        art_theme_freq[i] += (theme_type[i][COL_THEME_MIN]) * MIN_ENFORCEMENT;

        /*
         * keep track of the total to make sure we aren't attempting the
         * impossible
         */
        art_min_total += theme_type[i][COL_THEME_MIN];
    }

    /*
     * If necessary, reduce minimums to make sure the total minimum artefact is
     * less than 80% of the regular artefact set.
     */
    while (art_min_total
        > ((z_info->art_norm_max - z_info->art_spec_max) * 8 / 10))
    {
        for (i = 0; i < ART_THEME_MAX; i++)
        {
            if (art_theme_freq[i] > MIN_ENFORCEMENT)
            {
                art_theme_freq[i] -= MIN_ENFORCEMENT;
            }

            art_min_total--;
        }
    }
}

/*
 * Build the frequency tables
 */
void build_art_freq_table(void)
{
    byte i;

    for (i = 0; i < ART_THEME_MAX; i++)
    {
        art_theme_freq[i] = theme_type[i][COL_THEME_FREQ];
    }
}

/*
 * Pick a category of weapon randomly.
 */
byte get_theme(void)
{
    byte theme;

    int counter, theme_selector, theme_freq_total;

    /* find out the current frequency total */
    theme_freq_total = 0;

    for (theme = 0; theme < ART_THEME_MAX; theme++)
    {
        theme_freq_total += art_theme_freq[theme];
    }

    /* Generate a random number between 1 and current frequency total */
    theme_selector = dieroll(theme_freq_total);

    /* Find the entry in the table that this number represents. */
    counter = 0;
    theme = 0;
    for (theme = 0; theme < ART_THEME_MAX; theme++)
    {
        counter += art_theme_freq[theme];

        if (counter >= theme_selector)
            break;
    }

    if (art_theme_freq[theme] >= MIN_ENFORCEMENT)
        art_theme_freq[theme] -= MIN_ENFORCEMENT;

    return (theme_type[theme][COL_THEME_DROP_TYPE]);
}

/*
 * Randomly select a base item type (tval,sval).  Assign the various fields
 * corresponding to that choice.
 */
void choose_item(int a_idx)
{
    artefact_type* a_ptr = &a_info[a_idx];
    object_kind* k_ptr;
    s16b k_idx;
    byte theme;

    byte target_level;

    if (a_idx < z_info->art_norm_max)
    {
        target_level = (base_item_level[a_idx]
            + (rand_int(MAX_DEPTH - base_item_level[a_idx])));
    }
    else
    {
        target_level = object_level + 5;
    }

    /*
     * Look up the original artefact's base object kind to get level and
     * rarity information to supplement the artefact level/rarity.
     */
    if (a_idx < z_info->art_norm_max)
    {
        int y;

        alloc_entry* table = alloc_kind_table;

        k_idx = kinds[a_idx];

        /* Process probabilities */
        for (y = 0; y < alloc_kind_size; y++)
        {
            if (k_idx != table[y].index)
                continue;

            /* The rarity tables are divided by 100 in the prob_table */
            a_ptr->rarity += (100 / table[y].prob2);

            break;
        }
    }

    theme = get_theme();

    /* prepare the object generation level for a specific theme */
    if (!prep_object_theme(theme))
        return;

    k_idx = 0;

    /* get the object number */
    while (!k_idx)
        k_idx = get_obj_num(target_level);

    /* Clear restriction */
    get_obj_num_hook = NULL;

    /* Un-do the object theme */
    get_obj_num_prep();

    k_ptr = &k_info[k_idx];

    /* prepare a basic-non-magic artefact template based on the base object */
    artefact_prep(k_idx, a_idx);

    a_ptr->flags1 |= k_ptr->flags1;
    a_ptr->flags2 |= k_ptr->flags2;
    a_ptr->flags3 |= k_ptr->flags3;
}

/*
 * Choose a random feature using weights based on the given cumulative frequency
 * table.
 */
static int choose_power_type(void)
{
    int cat_selector, cat_counter, counter, art_freq_total;

    /* find out the current frequency total */
    art_freq_total = 0;

    for (cat_counter = 0; cat_counter < CAT_MAX; cat_counter++)
    {
        art_freq_total += art_freq[cat_counter];
    }

    /* Generate a random number between 1 and current frequency total */
    cat_selector = dieroll(art_freq_total);

    /* Find the entry in the table that this number represents. */
    counter = 0;
    for (cat_counter = 0; cat_counter < CAT_MAX; cat_counter++)
    {
        counter += art_freq[cat_counter];

        if (counter >= cat_selector)
            break;
    }

    return cat_counter;
}

/*
 * Add an ability given by the index choice.
 */
static void add_feature_aux(artefact_type* a_ptr, int choice)
{
    switch (choice)
    {
    case CAT_STATS:
    {
        byte stat_roll = dieroll(((a_ptr->level > 30) ? 30 : a_ptr->level));

        if (stat_roll <= 10)
        {
            if (!add_sustain(a_ptr))
            {
                if (!add_stat(a_ptr))
                    art_freq[CAT_STATS] = 0;
            }
        }
        else
        {
            if (!add_stat(a_ptr))
            {
                if (!add_sustain(a_ptr))
                    art_freq[CAT_STATS] = 0;
            }
        }
        break;
    }
    case CAT_SPEED:
    {
        if ((one_in_(2)) || (a_ptr->flags2 & TR2_SPEED))
        {
            a_ptr->flags2 |= TR2_SPEED;
            do_pval(a_ptr);
        }
        break;
    }
    case CAT_SLAYS:
    {
        if (!add_slay(a_ptr))
            art_freq[CAT_SLAYS] = 0;
        break;
    }
    case CAT_BRANDS:
    {
        if (!add_brand(a_ptr))
            art_freq[CAT_BRANDS] = 0;
        break;
    }
    case CAT_RESISTS:
    {
        byte resist_roll;
        byte highest = ((a_ptr->level > 52) ? 52 : a_ptr->level);

        while (one_in_(10))
            highest += 3;

        resist_roll = dieroll(highest);

        if (resist_roll <= 18)
        {
            if (add_one_resist(a_ptr, TR2_RESISTANCE))
                break;
        }
        if (resist_roll <= 30)
        {
            if (add_one_resist(a_ptr, TR2_LOW_RESIST))
                break;
        }
        if (resist_roll <= 50)
        {
            if (add_one_resist(a_ptr, TR2_HIGH_RESIST))
                break;
        }

        break;
    }
    case CAT_ABILITIES:
    {
        if (!add_ability(a_ptr))
            art_freq[CAT_ABILITIES] = 0;
        break;
    }
    case CAT_TUNNEL:
    {
        object_kind* k_ptr = &k_info[cur_art_k_idx];

        if ((a_ptr->flags1 & TR1_TUNNEL) || (a_ptr->pval < 0)
            || (k_ptr->weight < 50))
        {
            art_freq[CAT_TUNNEL] = 0;
            break;
        }

        switch (a_ptr->tval)
        {
        case TV_HAFTED:
        case TV_DIGGING:
        case TV_POLEARM:
        case TV_SWORD:
        {
            a_ptr->flags1 |= TR1_TUNNEL;
            do_pval(a_ptr);
            break;
        }
        default:
            art_freq[CAT_TUNNEL] = 0;
        }
        break;
    }
    case CAT_IMPACT:
    {
        break;
    }
    case CAT_WEAP_XTRA:
    {
        break;
    }
    case CAT_BOW_XTRA:
    {
        break;
    }
    case CAT_STEALTH:
    {
        if (a_ptr->flags1 & TR1_STL)
        {
            art_freq[CAT_STEALTH] = 0;
            break;
        }
        a_ptr->flags1 |= TR1_STL;
        do_pval(a_ptr);
        break;
    }
    case CAT_VISION:
    {
        if (((a_ptr->flags1 & TR1_PER)) || (a_ptr->pval < 0))
        {
            art_freq[CAT_VISION] = 0;
            break;
        }

        do_pval(a_ptr);
        break;
    }
    case CAT_COMBAT:
    {
        if (one_in_(2))
            add_att(a_ptr, 1, 2 * MEAN_DAM_INCREMENT);
        else
            add_to_dam(a_ptr);
        break;
    }
    case CAT_TO_AC:
    {
        add_to_ac(a_ptr, 1, 2 * MEAN_DAM_INCREMENT);
        break;
    }
    case CAT_TO_BASE:
    {
        switch (a_ptr->tval)
        {
        case TV_HAFTED:
        case TV_POLEARM:
        case TV_SWORD:
        case TV_DIGGING:
        {
            while (one_in_(15))
                a_ptr->dd++;

            if (a_ptr->dd > 9)
                a_ptr->dd = 9;

            while (one_in_(15))
                a_ptr->ds++;

            if (a_ptr->ds > 9)
                a_ptr->ds = 9;
            break;
        }
        case TV_MAIL:
        case TV_SOFT_ARMOR:
        case TV_CLOAK:
        case TV_SHIELD:
        case TV_HELM:
        case TV_CROWN:
        case TV_BOOTS:
        case TV_GLOVES:
        {
            break;
        }
        default:
            art_freq[CAT_TO_BASE] = 0;
        }
        break;
    }
    case CAT_WEIGH_LESS:
    {
        if (a_ptr->weight < 20)
            art_freq[CAT_WEIGH_LESS] = 0;
        else
            a_ptr->weight = (a_ptr->weight * 9) / 10;

        if ((a_ptr->weight < 50) || (one_in_(2)))
            art_freq[CAT_TO_BASE] = 0;
        break;
    }
    case CAT_LITE:
    {
        if (((a_ptr->tval == TV_LIGHT) || (a_ptr->flags2 & TR2_LIGHT))
            || (a_ptr->pval < 0))
        {
            art_freq[CAT_LITE] = 0;
            break;
        }

        a_ptr->flags2 |= TR2_LIGHT;
        break;
    }
    }
}

/*
 * Randomly select an extra ability to be added to the artefact in question.
 */
void add_feature(artefact_type* a_ptr)
{
    int r;

    r = choose_power_type();

    add_feature_aux(a_ptr, r);

    /* Now remove contradictory or redundant powers. */
    remove_contradictory(a_ptr);
}

/*
 * Try to supercharge this item by running through the list of the supercharge
 * abilities and attempting to add each in turn.
 */
void try_supercharge(artefact_type* a_ptr, int final_power)
{
    bool did_supercharge = false;

    if (a_ptr->tval == TV_DIGGING || a_ptr->tval == TV_HAFTED
        || a_ptr->tval == TV_POLEARM || a_ptr->tval == TV_SWORD)
    {
        if (rand_int(a_ptr->level) < (final_power / 10))
        {
            if (one_in_(2))
            {
                a_ptr->dd += 3 + rand_int(4);
                if (a_ptr->dd > 9)
                    a_ptr->dd = 9;
            }
            else
            {
                a_ptr->ds += 3 + rand_int(4);
                if (a_ptr->ds > 9)
                    a_ptr->ds = 9;
            }

            did_supercharge = true;
        }
    }

    /* Aggravation */
    if (did_supercharge)
    {
        switch (a_ptr->tval)
        {
        case TV_BOW:
        case TV_DIGGING:
        case TV_HAFTED:
        case TV_POLEARM:
        case TV_SWORD:
        {
            if (rand_int(100) < (final_power / 8))
            {
                a_ptr->flags2 |= TR2_AGGRAVATE;
            }
            break;
        }

        default:
        {
            if (rand_int(100) < (final_power / 8))
            {
                a_ptr->flags2 |= TR2_AGGRAVATE;
            }
            break;
        }
        }
    }
}

/*
 * Make it bad, or if it's already bad, make it worse!
 */
void do_curse(artefact_type* a_ptr)
{
    if (one_in_(3))
        a_ptr->flags2 |= TR2_AGGRAVATE;

    if ((a_ptr->pval > 0) && (one_in_(2)))
        a_ptr->pval = -a_ptr->pval;
    if ((a_ptr->att > 0) && (one_in_(2)))
        a_ptr->att = -a_ptr->att;

    if (a_ptr->flags3 & TR3_LIGHT_CURSE)
    {
        if (one_in_(2))
            a_ptr->flags3 |= TR3_HEAVY_CURSE;
        return;
    }

    a_ptr->flags3 |= TR3_LIGHT_CURSE;

    if (one_in_(4))
        a_ptr->flags3 |= TR3_HEAVY_CURSE;
}

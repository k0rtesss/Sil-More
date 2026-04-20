/* File: object-randart.c */
/*
 * Copyright (c) 1997 Ben Harrison
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
#include "init.h"
#include "object/object-randart-internal.h"
#include "object/object-randart.h"

#define MAX_TRIES 200
#define BUFLEN 1024

#define MIN_NAME_LEN 5
#define MAX_NAME_LEN 9

/* Total number of different slay types used */

#define SLAY_MAX 0x00010000L

/*
 * Average damage for good ego ammo of various types, used for balance
 * The current values assume normal (non-seeker) ammo enchanted to +9
 */
#define AVG_SLING_AMMO_DAMAGE 11
#define AVG_BOW_AMMO_DAMAGE 12
#define AVG_XBOW_AMMO_DAMAGE 12

/* Inhibiting factors for large bonus values */
#define INHIBIT_STRONG 6
#define INHIBIT_WEAK 4

// Sil-y: note that the bad flags are all thought to be in TR3
// I'd have to change this to allow randarts
#define ART_FLAGS_BAD (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE)

/*
 * Numerical index values for the different types
 * These are to make the code more readable.
 * Note aggravate is handled separately.
 */
#define CAT_STATS 0
#define CAT_SPEED 1
#define CAT_SLAYS 2
#define CAT_BRANDS 3
#define CAT_RESISTS 4 /*includes immunities*/
#define CAT_ABILITIES 5 /*non-pval*/
#define CAT_TUNNEL 6
#define CAT_IMPACT 7
#define CAT_WEAP_XTRA 8
#define CAT_BOW_XTRA 9
#define CAT_STEALTH 10
#define CAT_VISION 11 /*infravision, searching*/
#define CAT_COMBAT 12
#define CAT_TO_AC 13
#define CAT_TO_BASE 14 /*add to base damage dice, sides, or armor base*/
#define CAT_WEIGH_LESS 15
#define CAT_LITE 16
#define CAT_MAX 17

/*The different types of artefacts*/
#define ART_TYPE_WEAPON 0
#define ART_TYPE_SHOVEL 1
#define ART_TYPE_BOW 2
#define ART_TYPE_SPECIAL 3 /*Rings, amulets, lite sources*/
#define ART_TYPE_ARMOR 4
#define ART_TYPE_DRAG_ARMOR 5
#define ART_TYPE_CLOAK 6
#define ART_TYPE_SHIELD 7
#define ART_TYPE_HELM 8
#define ART_TYPE_CROWN 9
#define ART_TYPE_GLOVES 10
#define ART_TYPE_BOOTS 11
#define ART_TYPE_MAX 12

// Useful 'sign' function
#define sign(x) ((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))

bool make_fake_artefact(object_type* o_ptr, byte name1)
{
    int i;

    artefact_type* a_ptr = &a_info[name1];

    if (a_ptr->tval + a_ptr->sval == 0)
        return false;

    i = lookup_kind(a_ptr->tval, a_ptr->sval);
    if (!i)
        return false;

    object_prep(o_ptr, i);

    o_ptr->name1 = name1;
    o_ptr->pval = a_ptr->pval;
    o_ptr->att = a_ptr->att;
    o_ptr->dd = a_ptr->dd;
    o_ptr->ds = a_ptr->ds;
    o_ptr->evn = a_ptr->evn;
    o_ptr->pd = a_ptr->pd;
    o_ptr->ps = a_ptr->ps;
    o_ptr->weight = a_ptr->weight;

    for (i = 0; i < a_ptr->abilities; i++)
    {
        o_ptr->skilltype[i + o_ptr->abilities] = a_ptr->skilltype[i];
        o_ptr->abilitynum[i + o_ptr->abilities] = a_ptr->abilitynum[i];
        o_ptr->bane_type[i + o_ptr->abilities] = a_ptr->bane_type[i];
    }
    o_ptr->abilities += a_ptr->abilities;

    if (a_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);

    return true;
}

/*
 * Table of frequency of each ability for each type.
 * Notice the total values in each row are not consitstent.
 * The total is added up in art_fequ_total, and
 * randart characteristics are selected based on a random number of that total.
 * The numbers are best changed in a spreadsheet, since changing one number
 * affects the percentages for each type of attribute in that item.
 */

/*
        #0  CAT_STATS
        #1  CAT_SPEED
        #2  CAT_SLAYS
        #3  CAT_BRANDS
        #4  CAT_RESISTS
        #5  CAT_ABILITIES
        #6  CAT_TUNNEL
        #7  CAT_IMPACT
        #8  CAT_WEAP_XTRA
        #9  CAT_BOW_XTRA
        #10  CAT_STEALTH
        #11 CAT_VISION
        #12 CAT_COMBAT
        #13 CAT_TO_AC
        #14 CAT_TO_BASE
        #15 CAT_WEIGH_LESS
        #16 CAT_LITE
*/

/*
 * Table of frequency of each ability for each type
 * The appropriate slot from this table is "downloaded" into art_freq table
 * for use when creating each randart.
 */
const byte table_type_freq[ART_TYPE_MAX][CAT_MAX] = {
    /*#0, #1,#2, #3, #4, #5, #6, #7, #8, #9,#10,#11,#12 #13#14 #15,#16*/
    { 20, 2, 60, 7, 30, 12, 5, 2, 2, 0, 8, 1, 120, 10, 18, 10,
        20 }, /*  ART_TYPE_WEAPON   */
    { 16, 1, 20, 3, 20, 10, 80, 10, 1, 0, 8, 1, 40, 10, 10, 10,
        10 }, /*  ART_TYPE_SHOVEL   */
    { 20, 1, 0, 0, 10, 10, 0, 0, 0, 40, 8, 1, 80, 0, 0, 6,
        0 }, /*  ART_TYPE_BOW   	*/
    { 30, 3, 0, 0, 30, 30, 0, 0, 0, 0, 10, 2, 10, 10, 0, 0,
        14 }, /*  ART_TYPE_SPECIAL  */
    { 20, 1, 0, 0, 40, 14, 0, 0, 0, 0, 10, 1, 0, 60, 10, 20,
        4 }, /*  ART_TYPE_ARMOR   	*/
    { 20, 2, 0, 0, 30, 20, 0, 0, 0, 0, 10, 1, 0, 60, 20, 20,
        20 }, /*  ART_TYPE_DRAG_ARMOR   */
    { 14, 2, 0, 0, 30, 14, 0, 0, 0, 0, 20, 1, 0, 40, 10, 10,
        4 }, /*  ART_TYPE_CLOAK 	*/
    { 10, 1, 0, 0, 30, 10, 0, 0, 0, 0, 6, 1, 0, 60, 10, 10,
        8 }, /*  ART_TYPE_SHIELD   */
    { 30, 1, 0, 0, 20, 14, 0, 0, 0, 0, 10, 10, 0, 40, 10, 10,
        14 }, /*  ART_TYPE_HELM   	*/
    { 20, 1, 0, 0, 10, 24, 0, 0, 0, 0, 10, 10, 0, 30, 10, 10,
        20 }, /*  ART_TYPE_CROWN  	*/
    { 10, 1, 0, 0, 16, 10, 0, 0, 0, 0, 6, 1, 20, 50, 10, 10,
        4 }, /*  ART_TYPE_GLOVES   */
    { 16, 5, 0, 0, 16, 10, 0, 0, 0, 0, 30, 1, 0, 40, 10, 10,
        4 } /*  ART_TYPE_BOOTS   	*/
};

/*used to hold the frequencies for the above table for the current randart*/
u16b art_freq[CAT_MAX];

/*
 *This list is sliightly different than the artefact type list above.
 *
 *The different types of artefact themes (Rings, amulets, lite sources)
 */
#define ART_THEME_EDGED 0
#define ART_THEME_POLEARM 1
#define ART_THEME_HAFTED 2
#define ART_THEME_SHOVEL 3
#define ART_THEME_BOW 4
#define ART_THEME_ARMOR 5
#define ART_THEME_DRAG_ARMOR 6
#define ART_THEME_CLOAK 7
#define ART_THEME_SHIELD 8
#define ART_THEME_HELM 9
#define ART_THEME_GLOVES 10
#define ART_THEME_BOOTS 11
#define ART_THEME_MAX 12

#define COL_THEME_FREQ                                                         \
    0 /*frequency of generating an artefact outside of randart set*/
#define COL_THEME_MIN                                                          \
    1 /*Minimum number of artefacts to use have in an artefact set*/
#define COL_THEME_DROP_TYPE                                                    \
    2 /*Drop type to use when initializing the                                 \
         tables*/
#define COL_MAX 3

/*
 * Note the COL_THEME_FREQ must be less than
 * this number for the frequency table to work right
 */
#define MIN_ENFORCEMENT 50

const byte theme_type[ART_THEME_MAX][COL_MAX] = {
    /*#0, #1, #2, */
    { 15, 10, DROP_TYPE_EDGED }, /*  ART_THEME_EDGED   	*/
    { 12, 8, DROP_TYPE_POLEARM }, /*  ART_THEME_POLEARM	*/
    { 12, 8, DROP_TYPE_POLEARM }, /* Was hafted Currently handled by polearm,
                                     but this is needed elsewhere*/
    { 1, 1, DROP_TYPE_DIGGING }, /*  ART_THEME_SHOVEL  	*/
    { 6, 5, DROP_TYPE_BOW }, /*  ART_THEME_BOW   	*/
    { 13, 9, DROP_TYPE_ARMOR }, /*  ART_THEME_ARMOR 	*/
    { 0, 0, DROP_TYPE_ARMOR }, /* Was dragon armor Currently handled by Armor,
                                  but this is needed elsewhere*/
    { 7, 5, DROP_TYPE_CLOAK }, /*  ART_THEME_CLOAK		*/
    { 5, 4, DROP_TYPE_SHIELD }, /*  ART_THEME_SHIELD   	*/
    { 10, 7, DROP_TYPE_HEADGEAR }, /*  ART_THEME_HELM   	*/
    { 8, 6, DROP_TYPE_GLOVES }, /*  ART_THEME_GLOVES   	*/
    { 4, 3, DROP_TYPE_BOOTS } /*  ART_THEME_BOOTS   	*/
};

int art_theme_freq[ART_THEME_MAX];

#define NORMAL_FREQUENCY 10

/*
 * Make some stats more frequent for certain types.  A "0' just means normal
 * frequency The appropriate slot from this table is "downloaded" into art_freq
 * table for use when creating each randart.
 */
const byte table_stat_freq[ART_TYPE_MAX][A_MAX] = {
    /*STR DEX CON GRA */
    { 5, 0, 0, 0 }, /*  ART_TYPE_WEAPON  */
    { 5, 0, 0, 0 }, /*  ART_TYPE_SHOVEL  */
    { 0, 0, 0, 0 }, /*  ART_TYPE_BOW   	*/
    { 0, 0, 0, 0 }, /*  ART_TYPE_SPECIAL */
    { 0, 0, 5, 0 }, /*  ART_TYPE_ARMOR   	*/
    { 0, 0, 0, 0 }, /*  ART_TYPE_DRAG_ARMOR   */
    { 0, 0, 0, 0 }, /*  ART_TYPE_CLOAK 	*/
    { 5, 0, 5, 0 }, /*  ART_TYPE_SHIELD  */
    { 0, 0, 0, 5 }, /*  ART_TYPE_HELM   	*/
    { 0, 0, 0, 10 }, /*  ART_TYPE_CROWN  	*/
    { 0, 10, 5, 0 }, /*  ART_TYPE_GLOVES  */
    { 0, 5, 0, 0 } /*  ART_TYPE_BOOTS   */
};

/*Current randart only - Used to keep weightings for each stat*/
byte art_stat_freq[A_MAX];

/*
 * Frequencies of "higher" resists and immunities are determined by artefact
 * depth rather than by artefact type
 */

/*TR2_RESISTANCE is the basic 4 elements and is already defined in defines.h*/
#define TR2_LOW_RESIST (TR2_RES_FEAR)
#define TR2_HIGH_RESIST (TR2_RES_STUN | TR2_RES_BLIND | TR2_RES_CONFU)

/*
        #0  TR3_SLOW_DIGEST
        #1  TR3_FEATHER
        #2  TR3_LIGHT
        #3  TR3_REGEN
        #4  TR3_TELEPATHY
        #5  TR3_SEE_INVIS
        #6  TR3_FREE_ACT
        #7  TR3_HOLD_LIFE (this table is out of date!)

 * Table of frequency adjustments of each ability for each type
 * 10 is normal frequency, -10 means no chance
 */
const int table_ability_freq[ART_TYPE_MAX][OBJECT_XTRA_SIZE_POWER] = {
    /*#0, #1, #2, #3, #4, #5 */
    { 8, 7, 10, 10, 3, 10 }, /*  ART_TYPE_WEAPON   */
    { 8, 7, 10, 10, 3, 10 }, /*  ART_TYPE_SHOVEL   */
    { 8, 7, 0, 10, 3, 10 }, /*  ART_TYPE_BOW   	*/
    { 8, 5, 0, 10, 3, 10 }, /*  ART_TYPE_SPECIAL  */
    { 8, 7, 0, 10, 3, 10 }, /*  ART_TYPE_ARMOR   	*/
    { 8, 7, 0, 10, 5, 10 }, /*  ART_TYPE_DRAG_ARMOR   */
    { 8, 7, 0, 10, 3, 10 }, /*  ART_TYPE_CLOAK 	*/
    { 8, 7, 0, 10, 3, 10 }, /*  ART_TYPE_SHIELD   */
    { 8, 7, 0, 10, 5, 13 }, /*  ART_TYPE_HELM   	*/
    { 8, 7, 0, 10, 5, 13 }, /*  ART_TYPE_CROWN  	*/
    { 8, 7, 0, 10, 3, 10 }, /*  ART_TYPE_GLOVES   */
    { 8, 20, 0, 10, 3, 10 }, /*  ART_TYPE_BOOTS   	*/
};

/*used to keep frequencies for each ability*/
byte art_abil_freq[OBJECT_XTRA_SIZE_POWER];

#define NUM_FAVORED_SLAY_PAIRS 4

const u32b favored_slay_pairs[NUM_FAVORED_SLAY_PAIRS][2] = {
    { TR1_SLAY_UNDEAD, TR1_SLAY_RAUKO },
    { TR1_SLAY_RAUKO, TR1_SLAY_UNDEAD },
    { TR1_SLAY_ORC, TR1_SLAY_TROLL },
    { TR1_SLAY_TROLL, TR1_SLAY_ORC },
};

#define NUM_FAVORED_RESIST_PAIRS 2

const u32b favored_resist_pairs[NUM_FAVORED_RESIST_PAIRS][2] = {
    { TR2_RES_FIRE, TR2_RES_COLD },
    { TR2_RES_COLD, TR2_RES_FIRE },
};

/*
 * Boost ratings for combinations of ability bonuses
 * We go up to +24 here - anything higher is inhibited
 */
static s16b ability_power[25] = { 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 8, 10, 12, 15,
    18, 21, 24, 28, 32, 37, 42, 48, 55, 65, 75 };

/*
 * Mean start and increment values for att, to_dam and AC.  Update these
 * if the algorithm changes.  They are used in frequency generation.
 */

#define MEAN_HIT_INCREMENT 3
#define MEAN_DAM_INCREMENT 3
#define MEAN_HIT_STARTVAL 8
#define MEAN_DAM_STARTVAL 8
#define MEAN_AC_STARTVAL 15
#define MEAN_AC_INCREMENT 5

/*
 * Cache the results of lookup_kind(), which is expensive and would
 * otherwise be called much too often.
 */
s16b* kinds;

/*
 * Store the original artefact power ratings
 */
s32b* base_power;

/*
 * Store the original base item levels
 */
byte* base_item_level;

/*
 * Store the original base item rarities
 */
byte* base_item_rarity;

/*
 * Store the original artefact rarities
 */
byte* base_art_rarity;

/* Store the current artefact k_idx */

s16b cur_art_k_idx;

/*
 * Use W. Sheldon Simms' random name generator.  Generate a random word using
 * the probability tables we built earlier.  Relies on the ASCII character
 * set.  Relies on European vowels (a, e, i, o, u).  The generated name should
 * be copied/used before calling this function again.
 */
static char* make_word(void)
{
    static char word_buf[90];
    int r, totalfreq;
    int tries, lnum, vow;
    int c_prev, c_cur, c_next;
    char* cp;

startover:
    vow = 0;
    lnum = 0;
    tries = 0;
    cp = word_buf;
    c_prev = c_cur = S_WORD;

    while (1)
    {
    getletter:
        c_next = 0;
        r = rand_int(n_info->ltotal[c_prev][c_cur]);
        totalfreq = n_info->lprobs[c_prev][c_cur][c_next];

        /*find the letter*/
        while (totalfreq <= r)
        {
            c_next++;
            totalfreq += n_info->lprobs[c_prev][c_cur][c_next];
        }

        if (c_next == E_WORD)
        {
            if ((lnum < MIN_NAME_LEN) || vow == 0)
            {
                tries++;
                if (tries < 10)
                    goto getletter;
                goto startover;
            }
            *cp = '\0';
            break;
        }

        if (lnum >= MAX_NAME_LEN)
            goto startover;

        *cp = I2A(c_next);

        if (is_a_vowel(*cp))
            vow++;

        cp++;
        lnum++;
        c_prev = c_cur;
        c_cur = c_next;
    }

    word_buf[0] = toupper((unsigned char)word_buf[0]);

    return (word_buf);
}

void make_random_name(char* random_name, size_t max)
{
    /*get the randomly generated word*/
    SDL_strlcpy(random_name, make_word(), max);

    return;
}

/*
 * Go through the attack types for this monster.
 * We look for the maximum possible maximum damage that this
 * monster can inflict in 10 game turns.  For melee
 * attacks we use the average damage assuming all attacks hit.
 * Spells are handled on a case by case basis.
 * For breaths we assume the monster has maximum HP.  In general
 * we assume all low  Special spells like
 * summoning that don't cause damage are  assigned a
 * 'fake' damage rating depending on player level.
 */

static long eval_max_dam(int r_idx)
{
    int i, x;
    u32b dam = 1;
    u32b melee_dam, atk_dam, spell_dam;
    monster_race* r_ptr;
    u32b flag = 0L; // default to soothe compiler warnings
    u32b breath_mask = 0L; // default to soothe compiler warnings
    u32b attack_mask = 0L; // default to soothe compiler warnings
    u32b flag_counter;

    r_ptr = &r_info[r_idx];

    /*clear the counters*/
    melee_dam = atk_dam = spell_dam = 0;

    for (x = 0; x < 4; x++)
    {
        /*Get the flags 4 monster flags and masks*/
        switch (x)
        {
        case 0:
        {
            flag = r_ptr->flags4;
            attack_mask = RF4_ATTACK_MASK;
            breath_mask = RF4_BREATH_MASK;
            break;
        }
        }

        /*no spells here, continue*/
        if (!flag)
            continue;

        flag_counter = 0x00000001;

        /* using 32 assumes a u32b flag size*/
        for (i = 0; i < 32; i++)
        {
            u16b this_dam = 0;

            /* First make sure monster has the flag*/
            if (flag & flag_counter)
            {
                /*Is it a breath? Should only be flag 4*/
                if (breath_mask & flag_counter)
                {
                    int which_gf = 0;
                    int mult = 1;
                    int div_by = 1;

                    /*hack - all breaths are in flag 4*/

                    if (flag_counter == RF4_BRTH_FIRE)
                        which_gf = GF_FIRE;
                    else if (flag_counter == RF4_BRTH_COLD)
                        which_gf = GF_COLD;
                    else if (flag_counter == RF4_BRTH_POIS)
                        which_gf = GF_POIS;
                    else if (flag_counter == RF4_BRTH_DARK)
                        which_gf = GF_DARK;

                    if (which_gf)
                    {
                        this_dam = 100; // Sil-y: hack, should be breath damage

                        /* handle elemental breaths*/
                        switch (which_gf)
                        {
                        case GF_ACID:
                        case GF_FIRE:
                        case GF_COLD:
                        case GF_ELEC:
                        case GF_POIS:
                        {
                            /* Lets just pretend the player has the right base
                             * resist*/
                            this_dam /= 3;

                            break;
                        }

                        default:
                            break;
                        }

                        this_dam = (this_dam * mult) / div_by;

                        /*slight bonus for cloud_surround*/
                        if (r_ptr->flags2 & RF2_CLOUD_SURROUND)
                            this_dam = this_dam * 11 / 10;
                    }
                }

                /*Is it an arrow, bolt, beam, or ball?*/
                else if (attack_mask & flag_counter)
                {
                    switch (x)
                    {
                    case 0:
                    {
                        this_dam = r_ptr->spell_power
                            * spell_info_RF4[i][COL_SPELL_SIDES];
                        break;
                    }
                    }
                }

                // Sil-y: some other spells snipped here
            }

            if (this_dam > spell_dam)
                spell_dam = this_dam;

            /*shift one bit*/
            flag_counter = flag_counter << 1;
        }
    }

    /* Only do if it has attacks */
    if (!(r_ptr->flags1 & (RF1_NEVER_BLOW)))
    {
        for (i = 0; i < 4; i++)
        {
            /* Extract the attack infomation */
            int effect = r_ptr->blow[i].effect;
            int method = r_ptr->blow[i].method;
            int dd = r_ptr->blow[i].dd;
            int ds = r_ptr->blow[i].ds;

            /* Hack -- no more attacks */
            if (!method)
                continue;

            /* Assume average damage*/
            atk_dam = dd * (ds + 1) / 2;

            switch (method)
            {
            /*possible stun*/
            case RBM_HIT:
            {
                if ((effect == RBE_WOUND) || (effect == RBE_BATTER))
                {
                    atk_dam *= 5;
                    atk_dam /= 4;
                }
                break;
            }
            /*stun definitely most dangerous*/
            case RBM_CRUSH:
            {
                atk_dam *= 4;
                atk_dam /= 3;
                break;
            }
            /*cut*/
            case RBM_CLAW:
            case RBM_BITE:
            case RBM_PECK:
            {
                atk_dam *= 7;
                atk_dam /= 5;
                break;
            }
            default:
                break;
            }

            switch (effect)
            {
            /*other bad effects - minor*/
            case RBE_EAT_ITEM:
            case RBE_EAT_FOOD:
            case RBE_HUNGER:
            case RBE_TERRIFY:
            {
                atk_dam *= 11;
                atk_dam /= 10;
                break;
            }
            /*other bad effects - major*/
            case RBE_UN_BONUS:
            case RBE_UN_POWER:
            case RBE_LOSE_MANA:
            case RBE_SLOW:
            case RBE_POISON:
            case RBE_ACID:
            case RBE_ELEC:
            case RBE_FIRE:
            case RBE_COLD:
            case RBE_DARK:
            case RBE_BLIND:
            case RBE_CONFUSE:
            case RBE_ENTRANCE:
            case RBE_DISEASE:
            case RBE_LOSE_STR:
            case RBE_LOSE_DEX:
            case RBE_LOSE_CON:
            case RBE_LOSE_GRA:
            case RBE_LOSE_STR_CON:
            case RBE_LOSE_ALL:
            case RBE_DISARM:
            case RBE_HALLU:
            {
                atk_dam *= 5;
                atk_dam /= 4;
                break;
            }
            /*Earthquakes*/
            case RBE_SHATTER:
            {
                atk_dam *= 7;
                atk_dam /= 6;
                break;
            }
            /*nothing special*/
            default:
                break;
            }

            /*keep a running total*/
            melee_dam += atk_dam;
        }

        /*Reduce damamge potential for monsters that move randomly*/
        if ((r_ptr->flags1 & (RF1_RAND_25)) || (r_ptr->flags1 & (RF1_RAND_50)))
        {
            int reduce = 100;

            if (r_ptr->flags1 & (RF1_RAND_25))
                reduce -= 25;
            if (r_ptr->flags1 & (RF1_RAND_50))
                reduce -= 50;

            /*even moving randomly one in 8 times will hit the player*/
            reduce += (100 - reduce) / 8;

            /* adjust the melee damage*/
            melee_dam = (melee_dam * reduce) / 100;
        }

        /*monsters who can't move aren't nearly as much of a combat threat*/
        if (r_ptr->flags1 & (RF1_NEVER_MOVE))
        {
            melee_dam /= 4;
        }

        /*but keep a minimum*/
        if (melee_dam < 1)
            melee_dam = 1;
    }

    /*
     * Get the max damage attack
     */

    if (dam < spell_dam)
        dam = spell_dam;
    if (dam < melee_dam)
        dam = melee_dam;

    /*
     * Adjust for speed.  Monster at speed 120 will do double damage,
     * monster at speed 100 will do half, etc.  Bonus for monsters who can haste
     * self.
     */
    dam = (dam * extract_energy[r_ptr->speed]) / 2;

    /*but deep in a minimum*/
    if (dam < 1)
        dam = 1;

    /* We're done */
    return (dam);
}

/*adjust a monsters hit points for how easily the monster is damaged*/
static u32b mon_hp_adjust(int r_idx)
{
    u32b hp;
    int slay_reduce = 0;

    monster_race* r_ptr = &r_info[r_idx];

    /*Get the monster base hitpoints*/
    hp = r_ptr->hdice * (r_ptr->hside + 1) / 2;

    /*
     * Base slays
     */
    if (r_ptr->flags3 & RF3_WOLF)
        slay_reduce++;
    if (r_ptr->flags3 & RF3_SPIDER)
        slay_reduce++;
    if (r_ptr->flags3 & RF3_RAUKO)
        slay_reduce++;
    if (r_ptr->flags3 & RF3_ORC)
        slay_reduce++;
    if (r_ptr->flags3 & RF3_TROLL)
        slay_reduce++;
    if (r_ptr->flags3 & RF3_DRAGON)
        slay_reduce++;
    if (r_ptr->flags3 & RF3_UNDEAD)
        slay_reduce++;

    /* Do the resists to those who have all resists*/
    if (r_ptr->flags3 & RF3_RES_ELEM)
    {
        /*count resists*/
        byte counter = 0;

        if (r_ptr->flags3 & RF3_RES_FIRE)
            counter++;
        if (r_ptr->flags3 & RF3_RES_COLD)
            counter++;
        if (r_ptr->flags3 & RF3_RES_ELEC)
            counter++;
        if (r_ptr->flags3 & RF3_RES_POIS)
            counter++;

        if (counter == 1)
            slay_reduce++;
        else if (counter == 2)
            slay_reduce += 2;
        else if (counter == 3)
            slay_reduce += 3;
        else if (counter == 4)
            slay_reduce += 5;
        else if (counter == 5)
            slay_reduce += 8;
    }

    /*Not very powerful, but so common and easy to use*/
    if (r_ptr->flags3 & RF3_HURT_LITE)
        slay_reduce += 2;
    if (r_ptr->flags3 & RF3_STONE)
        slay_reduce += 2;

    /*cut hitpoint value up to half, depending on suceptability to slays*/
    if (slay_reduce > 15)
        slay_reduce = 50;
    else
        slay_reduce = (30 - slay_reduce);

    hp = (hp * slay_reduce) / 30;

    /*boundry control*/
    if (hp < 1)
        hp = 1;

    /*regeneration means slightly more hp*/
    if (r_ptr->flags2 & RF2_REGENERATE)
    {
        hp *= 10;
        hp /= 9;
    }

    if (r_ptr->flags2 & RF2_INVISIBLE)
    {
        hp = (hp * (r_ptr->level + 2)) / r_ptr->level;
    }

    /*slight increase for no_charm*/
    if (r_ptr->flags3 & RF3_NO_CHARM)
    {
        hp *= 10;
        hp /= 9;
    }

    /*more boundry control*/
    if (hp < 1)
        hp = 1;

    return (hp);
}

/*
 * Initialize the data structures for the monster power ratings
 * ToDo: Add handling and return codes for error conditions if any.
 */

static bool init_mon_power(void)
{
    int i, j;

    s16b mon_count[MAX_DEPTH][CREATURE_TYPE_MAX];
    u32b mon_power_total[MAX_DEPTH][CREATURE_TYPE_MAX];
    monster_race* r_ptr;

    /*first clear the tables*/
    for (i = 0; i < MAX_DEPTH; i++)
    {
        for (j = 0; j < CREATURE_TYPE_MAX; j++)
        {
            mon_count[i][j] = 0;
            mon_power_total[i][j] = 0;
            mon_power_ave[i][j] = 0;
        }
    }

    /*
     * Go through r_info and evaluate power ratings.
     * ASSUMPTION: The monsters in r_info are in ascending order by level.
     */
    for (i = 1; i < z_info->r_max; i++)
    {
        u32b hp;
        u32b dam;

        byte creature_type;

        r_ptr = &r_info[i];

        if (r_ptr->flags1 & (RF1_UNIQUE))
            creature_type = CREATURE_UNIQUE;
        else
            creature_type = CREATURE_NON_UNIQUE;

        hp = mon_hp_adjust(i);

        /* Maximum damage this monster can do in 10 game turns*/
        dam = eval_max_dam(i);

        /* Define the power rating */
        r_ptr->mon_power = hp * dam;

        /*
         * Slight adjustment for group monsters.
         * Escorts are not evaluated because they tend to
         * be much weaker than friends.
         */
        if (r_ptr->flags1 & RF1_FRIEND)
            r_ptr->mon_power = (r_ptr->mon_power * 11) / 10;
        else if (r_ptr->flags1 & RF1_FRIENDS)
            r_ptr->mon_power = (r_ptr->mon_power * 8) / 7;

        mon_count[r_ptr->level][creature_type]++;
        mon_power_total[r_ptr->level][creature_type] += r_ptr->mon_power;
    }

    /*populate the mon_power-ave table*/
    for (i = 0; i < MAX_DEPTH; i++)
    {
        for (j = 0; j < CREATURE_TYPE_MAX; j++)
        {
            /*leave the level as 0*/
            if (mon_count[i][j] == 0)
                continue;

            /*get the average power rater*/
            mon_power_ave[i][j] = mon_power_total[i][j] / mon_count[i][j];
        }
    }

    /*
     * Now smooth it out because some levels aren't consistent, mainly due to
     * there not being enough monsters at the deeper levels
     */
    for (i = 0; i < MAX_DEPTH; i++)
    {
        byte min_level = 1;

        for (j = 0; j < CREATURE_TYPE_MAX; j++)
        {
            /*empty levels*/
            if (mon_power_ave[i][j] == 0)
            {
                /*paranoia, so we don't crash on the next line*/
                if (i <= min_level)
                    continue;

                /*use the previous level*/
                mon_power_ave[i][j] = mon_power_ave[i - 1][j];

                continue;
            }
        }
    }

    /* Now we have all the ratings */
    return (true);
}

#ifdef USE_ART_THEME

/* Return ART_THEME slot.
 * IMPORTANT: Assumes the function can_be_artefact would return true.
 * Not intended for special artefacts!!!
 * Not currently used, but too useful to delete
 */
static byte get_art_theme(const artefact_type* a_ptr)
{
    switch (a_ptr->tval)
    {
    case TV_MAIL: /*fall through*/
    case TV_SOFT_ARMOR:
        return (ART_THEME_ARMOR);
    case TV_SHIELD:
        return (ART_THEME_SHIELD);
    case TV_CLOAK:
        return (ART_THEME_CLOAK);
    case TV_BOOTS:
        return (ART_THEME_BOOTS);
    case TV_GLOVES:
        return (ART_THEME_GLOVES);
    case TV_HELM: /*fall through*/
    case TV_CROWN:
        return (ART_THEME_HELM);
    case TV_BOW:
        return (ART_THEME_BOW);
    case TV_SWORD:
        return (ART_THEME_EDGED);
    case TV_HAFTED:
        return (ART_THEME_HAFTED);
    case TV_POLEARM:
        return (ART_THEME_POLEARM);
    case TV_DIGGING:
        return (ART_THEME_SHOVEL);

    /*notice returning this would crash the game if it was looked
     * up in a table, but I have to return something. -JG
     */
    default:
        return (ART_THEME_MAX);
    }
}

#endif /*USE_ART_THEME*/

/*
 * Calculate the rating for calculating a weapon base damage potential
 */
static int weapon_damage_calc(const artefact_type* a_ptr)
{
    int slay_adjust = 0;
    int damage_calc = (a_ptr->dd * (a_ptr->ds + 1) / 2);

    /*count up the number of slays and brands*/

    if (a_ptr->flags1 & TR1_SLAY_WOLF)
        slay_adjust += 2;
    if (a_ptr->flags1 & TR1_SLAY_SPIDER)
        slay_adjust += 3;
    if (a_ptr->flags1 & TR1_SLAY_UNDEAD)
        slay_adjust += 2;
    if (a_ptr->flags1 & TR1_SLAY_RAUKO)
        slay_adjust += 2;
    if (a_ptr->flags1 & TR1_SLAY_ORC)
        slay_adjust += 2;
    if (a_ptr->flags1 & TR1_SLAY_TROLL)
        slay_adjust += 2;
    if (a_ptr->flags1 & TR1_SLAY_DRAGON)
        slay_adjust += 2;

    if (a_ptr->flags1 & TR1_BRAND_COLD)
        slay_adjust += 4;
    if (a_ptr->flags1 & TR1_BRAND_FIRE)
        slay_adjust += 4;
    if (a_ptr->flags1 & TR1_BRAND_ELEC)
        slay_adjust += 4;
    if (a_ptr->flags1 & TR1_BRAND_POIS)
        slay_adjust += 4;

    /*increse the weapon damage rater based on the number and power of slays*/
    damage_calc += damage_calc * (slay_adjust) / 10;

    return (damage_calc);
}

/*
 * Calculate the multiplier we'll get with a given bow type.
 */
static int bow_multiplier(int sval)
{
    switch (sval)
    {
    case SV_SHORT_BOW:
        return (2);
    case SV_LONG_BOW:
        return (3);
    case SV_DH_LONG_BOW:
        return (4);
    default:
        msg_format("Illegal bow sval %s", sval);
    }

    return (0);
}

/*
 * Evaluate the artefact's overall power level.
 * Must be sure there is an artefact created before calling this function
 */
s32b artefact_power(int a_idx)
{
    const artefact_type* a_ptr = &a_info[a_idx];
    s32b p = 0;

    object_kind* k_ptr = &k_info[cur_art_k_idx];

    int extra_stat_bonus = 0;

    /* Evaluate certain abilities based on type of object. */
    switch (a_ptr->tval)
    {
    case TV_BOW:
    {
        int mult;

        /*
         * Damage multiplier for bows should be weighted less than that
         * for melee weapons, since players typically get fewer shots
         * than hits (note, however, that the multipliers are applied
         * afterwards in the bow calculation, not before as for melee
         * weapons, which tends to bring these numbers back into line).
         */

        p += sign(a_ptr->att) * (ABS(a_ptr->att) / 3);

        if (a_ptr->att > 15)
            p += (a_ptr->att - 15) * 2;
        if (a_ptr->att > 25)
            p += (a_ptr->att - 25) * 2;
        /*
         * Add the average damage of fully enchanted (good) ammo for this
         * weapon.  Could make this dynamic based on k_info if desired.
         */

        if (a_ptr->sval == SV_SHORT_BOW || a_ptr->sval == SV_LONG_BOW)
        {
            p += AVG_BOW_AMMO_DAMAGE;
        }

        mult = bow_multiplier(a_ptr->sval);

        p *= mult;

        p += sign(a_ptr->att) * (ABS(a_ptr->att) / 3);

        /*
         * Correction to match ratings to melee damage ratings.
         * We multiply all missile weapons by 1.5 in order to compare damage.
         * (CR 11/20/01 - changed this to 1.25).
         * Melee weapons assume 5 attacks per turn, so we must also divide
         * by 5 to get equal ratings.
         */

        if (a_ptr->sval == SV_SHORT_BOW || a_ptr->sval == SV_LONG_BOW)
        {
            p = sign(p) * (ABS(p) / 4);
        }
        else
        {
            p = sign(p) * (ABS(p) / 4);
        }

        if (a_ptr->weight < k_ptr->weight)
            p++;

        break;
    }
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    {
        /*calculate the weapon damage (factoring in slays and brands)*/
        p += weapon_damage_calc(a_ptr);

        p += sign(a_ptr->att) * (ABS(a_ptr->att) / 3);

        if (a_ptr->att > 15)
            p += (a_ptr->att - 15) * 2;
        if (a_ptr->att > 25)
            p += (a_ptr->att - 25) * 2;

        /*bonus to damage for well balanced and throwing items*/
        if (a_ptr->flags3 & (TR3_THROWING))
        {
            p += a_ptr->dd * (a_ptr->ds + 1) / 4;
        }

        if (a_ptr->weight < k_ptr->weight)
            p++;

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
        // Sil-y: add modifiers based on attack and defence

        if (a_ptr->weight < k_ptr->weight)
            p++;

        break;
    }
    case TV_LIGHT:
    {
        p += 5;

        p += sign(a_ptr->att) * ((ABS(a_ptr->att) * 2) / 3);

        break;
    }
    case TV_RING:
    case TV_AMULET:
    {
        p += sign(a_ptr->att) * ((ABS(a_ptr->att) * 2) / 3);

        break;
    }
    }

    if (a_ptr->pval > 0)
    {
        if (a_ptr->flags1 & TR1_TUNNEL)
            p += 1;

        p += 3 * a_ptr->stat_bonus[A_STR];
        p += 3 * a_ptr->stat_bonus[A_DEX];
        p += 4 * a_ptr->stat_bonus[A_CON];
        p += 4 * a_ptr->stat_bonus[A_GRA];

        p += a_ptr->skill_bonus[S_MEL];
        p += a_ptr->skill_bonus[S_ARC];
        p += a_ptr->skill_bonus[S_STL];
        p += a_ptr->skill_bonus[S_PER];
        p += a_ptr->skill_bonus[S_WIL];
        p += a_ptr->skill_bonus[S_SMT];
        p += a_ptr->skill_bonus[S_SNG];

        /* Add extra power term if there are a lot of ability bonuses */
        {
            extra_stat_bonus += (a_ptr->stat_bonus[A_STR] > 0) ? a_ptr->stat_bonus[A_STR] : 0;
            extra_stat_bonus += (a_ptr->stat_bonus[A_DEX] > 0) ? a_ptr->stat_bonus[A_DEX] : 0;
            extra_stat_bonus += (a_ptr->stat_bonus[A_CON] > 0) ? a_ptr->stat_bonus[A_CON] : 0;
            extra_stat_bonus += (a_ptr->stat_bonus[A_GRA] > 0) ? a_ptr->stat_bonus[A_GRA] : 0;

            if (extra_stat_bonus > 24)
            {
                /* Inhibit */
                p += 20000;
                return (p);
            }
            else
            {
                p += ability_power[extra_stat_bonus];
            }
        }
    }
    else if (a_ptr->pval < 0) /* hack: don't give large negatives */
    {
        p += 4 * a_ptr->stat_bonus[A_STR];
        p += 4 * a_ptr->stat_bonus[A_DEX];
        p += 4 * a_ptr->stat_bonus[A_CON];
        p += 4 * a_ptr->stat_bonus[A_GRA];

        p += a_ptr->skill_bonus[S_MEL];
        p += a_ptr->skill_bonus[S_ARC];
        p += a_ptr->skill_bonus[S_STL];
        p += a_ptr->skill_bonus[S_PER];
        p += a_ptr->skill_bonus[S_WIL];
        p += a_ptr->skill_bonus[S_SMT];
        p += a_ptr->skill_bonus[S_SNG];
    }

    return (p);
}

/*
 * Note the three special cases (One Ring, Grond, Morgoth).
 */
static void scramble_artefact(int a_idx)
{
    artefact_type* a_ptr = &a_info[a_idx];
    artefact_type artefact_type_body;
    artefact_type* a_old = &artefact_type_body;
    object_kind* k_ptr;
    u32b activates = a_ptr->flags3 & TR3_ACTIVATE;
    s32b power;
    int tries;
    s16b k_idx;
    byte rarity_old, base_rarity_old;
    s16b rarity_new;
    s32b ap;
    bool curse_me = false;
    u32b flags_bad = 0L;
    char buf[MAX_LEN_ART_NAME];

    /* Special cases -- don't randomize these! */
    if (strstr(a_ptr->name, "One Ring"))
        return;
    if (strstr(a_ptr->name, "Grond"))
        return;
    if (strstr(a_ptr->name, "of Morgoth"))
        return;

    /* Skip unused artefacts, too! */
    if (a_ptr->tval == 0)
        return;

    /*if there are bad flags, add them in later*/
    if (a_ptr->flags3 & ART_FLAGS_BAD)
    {
        flags_bad = a_ptr->flags3;
        flags_bad &= ART_FLAGS_BAD;
    }

    /*randomize the name*/
    make_random_name(buf, MAX_LEN_ART_NAME);

    if (!one_in_(3))
    {
        SDL_strlcpy(a_ptr->name, format("'%^s'", buf), MAX_LEN_ART_NAME);
    }
    else
    {
        SDL_strlcpy(a_ptr->name, format("of %^s", buf), MAX_LEN_ART_NAME);
    }

    /* Evaluate the original artefact to determine the power level. */
    power = base_power[a_idx];

    k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);

    /*paranoia - don't attempt to scramble invalid artefacts*/
    if (!k_idx)
        return;

    /* If it has a restricted ability then don't randomize it. */
    if (power > 10000)
    {
        return;
    }

    if (power < 0)
        curse_me = true;

    if (a_idx >= z_info->art_spec_max)
    {
        /*
         * Normal artefact - choose a random base item type.
         */
        int y;
        int new_object_rarity = 0;
        alloc_entry* table = alloc_kind_table;

        /* Capture the rarity of the original base item and artefact */
        base_rarity_old = base_item_rarity[a_idx];
        rarity_old = base_art_rarity[a_idx];

        /*actually get the item*/
        choose_item(a_idx);

        k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
        k_ptr = &k_info[k_idx];

        /*
         * Calculate the proper rarity based on the new type.  We attempt
         * to preserve the 'effective rarity' which is equal to the
         * artefact rarity multiplied by the base item rarity.
         */

        /* Process probabilities */
        for (y = 0; y < alloc_kind_size; y++)
        {
            if (k_idx != table[y].index)
                continue;

            /*The rarity tables are divided by 100 in the prob_table*/
            new_object_rarity = MAX((100 / table[y].prob2), 1);

            break;
        }

        /* Whoops!  Just make something up*/
        if (!new_object_rarity)
            new_object_rarity = 1;

        /*The table is sorted by depth, just use the lowest one*/
        rarity_new
            = ((s16b)rarity_old * (s16b)base_rarity_old) / new_object_rarity;

        if (rarity_new > 255)
            rarity_new = 255;

        /* Got an item - set the new rarity */
        a_ptr->rarity = (byte)rarity_new;
    }
    else
    {
        /* Special artefact (light source, ring, or
                   amulet).  Clear the following fields; leave
                   the rest alone. */
        a_ptr->pval = 0;
        a_ptr->att = 0;
        a_ptr->flags1 = a_ptr->flags2 = 0;

        /* Artefacts ignore everything */
        a_ptr->flags3 = (TR3_IGNORE_MASK);

        k_ptr = &k_info[k_idx];

        ap = artefact_power(a_idx);

        /*make sure there is some room to add something*/
        if ((ap * 10 / 9) > power)
            power = (ap * 10 / 9);
    }

    /* Got a base item. */
    /* Generate the cumulative frequency table for this item type */
    build_freq_table(a_ptr);

    /*add in the throwing flag, and possibly perfect_balance*/
    if (k_ptr->flags3 & (TR3_THROWING))
    {
        a_ptr->flags3 |= TR3_THROWING;
    }

    /* Copy artefact info temporarily. */
    *a_old = *a_ptr;

    /* Give this artefact a shot at being supercharged */
    try_supercharge(a_ptr, power);
    ap = artefact_power(a_idx);
    if (ap > (power * 21) / 20 + 1)
    {
        /* too powerful -- put it back */
        *a_ptr = *a_old;
    }

    /* First draft: add two abilities, then curse it three times. */
    if (curse_me)
    {
        int count;
        /* Copy artefact info temporarily. */
        *a_old = *a_ptr;

        for (count = 0; count < MAX_TRIES; count++)
        {
            add_feature(a_ptr);
            add_feature(a_ptr);
            do_curse(a_ptr);
            do_curse(a_ptr);
            do_curse(a_ptr);
            remove_contradictory(a_ptr);
            ap = artefact_power(a_idx);

            /* Accept if it doesn't have any inhibited abilities */
            if (ap < 10000)
                break;
            /* Otherwise go back and try again */
            else
            {
                *a_ptr = *a_old;
            }
        }

        /* Cursed items never have any resale value */
        a_ptr->cost = 0;
    }

    else
    {
        /*
         * Select a random set of abilities which roughly matches the
         * original's in terms of overall power/usefulness.
         */
        for (tries = 0; tries < MAX_TRIES; tries++)
        {
            /* Copy artefact info temporarily. */
            *a_old = *a_ptr;
            add_feature(a_ptr);
            ap = artefact_power(a_idx);

            /* CR 11/14/01 - pushed both limits up by about 5% */
            if (ap >= (power * 22 / 20))
            {
                /* too powerful -- put it back */
                *a_ptr = *a_old;

                continue;
            }

            else if (ap >= (power * 19) / 20) /* just right */
            {
                break;
            }

        } /* end of power selection */

        /* Set the cost proportional to the power level */
        a_ptr->cost = ap * 1000L;
    }

    if (a_ptr->cost < 0)
        a_ptr->cost = 0;

    /* Restore some flags */
    if (activates)
        a_ptr->flags3 |= TR3_ACTIVATE;
    if (a_idx < z_info->art_norm_max)
        a_ptr->flags3 |= TR3_INSTA_ART;

    /*add back in any bad flags*/
    a_ptr->flags3 |= flags_bad;
}

/*
 * Return nonzero if there is an acceptable minimum set of random artefacts.
 * With the overloaded weighting of randart themes until the minimum is
 * set, it is highly unlikely this routine will fail. -JG
 */
static bool artefacts_acceptable(void)
{
    byte i;

    for (i = 0; i < ART_THEME_MAX; i++)
    {
        if (art_theme_freq[i] >= MIN_ENFORCEMENT)
            return (false);
    }

    /*we have a good set*/
    return (true);
}

static errr scramble(void)
{
    /*Prevent making come unacceptable things for artefacts such as arrows*/
    object_generation_mode = OB_GEN_MODE_RANDART;

    while (true)
    {
        int a_idx;

        /* Generate all the artefacts. */
        for (a_idx = 1; a_idx < z_info->art_norm_max; a_idx++)
        {
            scramble_artefact(a_idx);
        }

        if (artefacts_acceptable())
            break;
    }

    /*Re-Set things*/
    object_generation_mode = OB_GEN_MODE_NORMAL;

    /* Success */
    return (0);
}

static errr do_randart_aux(bool full)
{
    errr result;

    if (full)
    {
        /*note: we are failing here*/

        /* Randomize the artefacts */
        if ((result = scramble()) != 0)
            return (result);
    }

    /* Success */
    return (0);
}

/*build the names table at the beginning on the game*/
void build_randart_tables(void)
{
    /* Allocate the "kinds" array */
    kinds = mem_alloc_array(z_info->art_norm_max, s16b);

    /* Initialize the monster power ratings */
    (void)init_mon_power();

    /*build the frequency tables*/
    build_art_freq_table();
}

/*free the randart tables at the end of the game*/
void free_randart_tables(void) { mem_free_null(kinds); }

/*
 * Randomize the artefacts
 *
 * The full flag toggles between just randomizing the names and
 * complete randomization of the artefacts.
 */
errr do_randart(u32b randart_seed, bool full)
{
    errr err;

    /* Prepare to use the Angband "simple" RNG. */
    u64b saved_state = Rand_state_export();
    Rand_state_import(randart_seed);

    /* Only do all the following if full randomization requested */
    if (full)
    {
        /* Allocate the various "original powers" arrays */
        base_power = mem_alloc_array(z_info->art_norm_max, s32b);
        base_item_level = mem_alloc_array(z_info->art_norm_max, byte);
        base_item_rarity = mem_alloc_array(z_info->art_norm_max, byte);
        base_art_rarity = mem_alloc_array(z_info->art_norm_max, byte);

        /* Store the original power ratings */
        store_base_power();

        /*adjust the randart frequencies to enforce minimum values*/
        adjust_art_freq_table();
    }

    /* Generate the random artefact (names) */
    err = do_randart_aux(full);

    /* Only do all the following if full randomization requested */
    if (full)
    {
        /* Free the "original powers" arrays */
        mem_free_null(base_power);
        mem_free_null(base_item_level);
        mem_free_null(base_item_rarity);
        mem_free_null(base_art_rarity);
    }

    /* When done, resume use of the Angband "complex" RNG. */
    Rand_state_import(saved_state);

    return (err);
}

/*
 * Attempt to create an artefact, returns false if there is no available slot.
 * This function assumes that an object suitable for an artefact is there, or if
 * it is blank it will make one.
 */
bool make_one_randart(object_type* o_ptr, int art_power, bool tailored)
{
    char tmp[MAX_LEN_ART_NAME];
    int i, tries;
    int a_idx = 0;
    s32b ap;
    object_kind* k_ptr;
    artefact_type* a_ptr;
    u32b f1, f2, f3;
    artefact_type artefact_type_body;
    artefact_type* a_old = &artefact_type_body;
    ;

    /*find an artefact slot, return false if none are available*/
    for (i = z_info->art_norm_max; i < z_info->art_max; i++)
    {
        a_ptr = &a_info[i];

        /* This slot is already being used */
        if ((a_ptr->tval + a_ptr->sval) > 0)
            continue;

        /*found a space to create an artefact*/
        a_idx = i;

        break;
    }

    if (!a_idx)
        return (false);

    /*point to the artefact*/
    a_ptr = &a_info[a_idx];

    /* Clear the artefact record */
    memset(a_ptr, 0, sizeof(artefact_type));

    /*point to the object type*/
    k_ptr = &k_info[o_ptr->k_idx];

    /*paranoia - make sure we have a suitable object_type*/
    if (can_be_randart(o_ptr))
    {
        /*prepare a basic, non-magic artefact template based on the object
         * kind*/
        artefact_prep(o_ptr->k_idx, a_idx);

        /*
         * bad hack - because we didn't determine object type based on artefact
         * power, make sure we have room to add power.
         */
        ap = artefact_power(a_idx);
        if (ap > art_power)
            art_power = ap * 6 / 5;
    }

    /*blank template - we need an object*/
    else if (!o_ptr->k_idx)
    {
        s16b k_idx = 0;
        byte old_mode = object_generation_mode;

        byte theme;

        /*Prevent making some unacceptable things for artefacts such as arrows*/
        object_generation_mode = OB_GEN_MODE_RANDART;

        /*prepare the object template*/
        object_wipe(o_ptr);

        /*get the obejct number test it for appropriate power*/
        while (true)
        {
            theme = get_theme();

            /*hack - we don't need any more artefact digging tools*/
            if (theme == DROP_TYPE_DIGGING)
                continue;
        }

        /*prepare the object generation level for a specific theme*/
        prep_object_theme(theme);

        k_idx = 0;

        while (!k_idx)
            k_idx = get_obj_num(MIN(object_level + 20, MAX_DEPTH - 1));

        /* Clear restriction */
        get_obj_num_hook = NULL;

        /* Un-do the object theme */
        get_obj_num_prep();

        /* Clear the artefact record */
        memset(a_ptr, 0, sizeof(artefact_type));

        /*prepare a basic, non-magic artefact template based on the object
         * kind*/
        artefact_prep(k_idx, a_idx);

        ap = artefact_power(a_idx);

        /*make sure there is room to add stuff*/
        if (ap > art_power)
            art_power = ap * 6 / 5;

        /*Re-Set the mode*/
        object_generation_mode = old_mode;

        /*point to the object type (in case it is needed again*/
        k_ptr = &k_info[o_ptr->k_idx];
    }

    /*All others fail, but this shouldn't happen*/
    else
        return (false);

    /*mark the artefact*/
    o_ptr->name1 = a_idx;

    /*add in the throwing flag, and possibly perfect_balance*/
    if (k_ptr->flags3 & (TR3_THROWING))
    {
        a_ptr->flags3 |= TR3_THROWING;
    }

    /*Start artefact naming with a blank name*/
    tmp[0] = '\0';

    /*possibly allow character to name the artefact*/
    if (tailored)
    {
        char ask_first[40];
        strnfmt(ask_first, sizeof(ask_first), "Name your artefact? ");
        if (get_check(ask_first))
        {
            char buf[MAX_LEN_ART_NAME];

            /*start with a blank name*/
            buf[0] = '\0';

            if (prompt_text_input("Enter a name for your artefact:",
                    "Enter accepts, Esc cancels, Backspace erases.", buf,
                    sizeof(buf), false))
            {
                /*The additional check is because players sometimes hit return
                 * accidentally*/
                if (strlen(buf) > 0)
                    SDL_strlcpy(tmp, format("'%^s'", buf), MAX_LEN_ART_NAME);
            }
        }
    }
    /*If none selected, make one at random*/
    if ((!tailored) || (tmp[0] == '\0'))
    {
        char buf[MAX_LEN_ART_NAME];

        /*randomize the name*/
        make_random_name(buf, MAX_LEN_ART_NAME);

        /*Capitalize the name*/
        buf[0] = toupper((unsigned char)buf[0]);

        if (!one_in_(3))
        {
            SDL_strlcpy(tmp, format("'%^s'", buf), MAX_LEN_ART_NAME);
        }
        else
        {
            SDL_strlcpy(tmp, format("of %^s", buf), MAX_LEN_ART_NAME);
        }
    }

    /*copy the name*/
    SDL_strlcpy(a_ptr->name, format("%s", tmp), MAX_LEN_ART_NAME);

    /* Generate the cumulative frequency table for this item type */
    build_freq_table(a_ptr);

    /*save any base object template flags before artefact creation*/
    f1 = k_ptr->flags1;
    f2 = k_ptr->flags2;
    f3 = k_ptr->flags3;

    /*the artefacts are always going to be created on demand, no need for
     * rarity*/
    a_ptr->rarity = MIN(art_power, 255);

    /*The level affects the power of the object*/
    a_ptr->level = object_level;

    /* Copy artefact info temporarily. */
    *a_old = *a_ptr;

    /* Give this artefact a shot at being supercharged */
    try_supercharge(a_ptr, art_power);
    ap = artefact_power(a_idx);
    if (ap >= (art_power * 21 / 20) + 1)
    {
        /* too powerful -- put it back */
        *a_ptr = *a_old;
    }

    /* Restore some flags */
    a_ptr->flags1 |= f1;
    a_ptr->flags2 |= f2;
    a_ptr->flags3 |= f3;

    /*
     * Select a random set of abilities which roughly matches the
     * original's in terms of overall power/usefulness.
     */
    for (tries = 0; tries < MAX_TRIES; tries++)
    {
        /* Copy artefact info temporarily. */
        *a_old = *a_ptr;

        add_feature(a_ptr);
        ap = artefact_power(a_idx);

        if (ap >= (art_power * 21 / 20) + 1)
        {
            /* too powerful -- put it back */
            *a_ptr = *a_old;

            continue;
        }

        else if (ap >= (art_power * 19) / 20 + 1) /* just right */
        {
            break;
        }
    }

    ap = artefact_power(a_idx);

    a_ptr->cost = ap * 1000L;

    if (a_ptr->cost < 0)
        a_ptr->cost = 0;

    /* Hack -- Mark the artefact as "created" */
    a_ptr->cur_num = 1;
    a_ptr->spawn_num = 1;

    /* Keep stat/skill bonuses in sync with final pval/flags. */
    artefact_apply_pval_stat_skill_bonuses(a_ptr);

    /*turn the object into an artefact*/
    object_into_artefact(o_ptr, a_ptr);

    /* Set the good item flag */
    good_item_flag = true;

    return (true);
}

/*
 * Wipe an artefact clean.  Rebuild the names string to delete the name.
 * Check first to make sure it is a randart
 */
void artefact_wipe(int a_idx)
{
    artefact_type* a_ptr = &a_info[a_idx];

    /*Hack - don't wipe any standard artefacts*/
    if (a_idx < z_info->art_norm_max)
        return;

    /* Wipe the structure */
    memset(a_ptr, 0, sizeof(artefact_type));

    /*terminate the string*/
    a_ptr->name[0] = '\0';
}

/*
 * Return true if an artefact can turn into a randart.
 * Note rings, amulets, and lite sources are currently not supported.
 * This is because there is no clear base object type for additional "special"
 * artefacts, plus object flavor would have to be handled for rings and amulets.
 * Someday I would like to resolve this. -JG
 */
bool can_be_randart(const object_type* o_ptr)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    switch (k_ptr->tval)
    {
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_BOW:
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    {
        return (true);
    }
    default:
        return (false);
    }
}





/* File: smithing-difficulty.c */
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
#include "smithing/smithing-internal.h"
#include "log/log.h"

static void dif_mod(int value, int positive_base, int* dif_inc)
{
    int mod = 1 + ((positive_base - 1) / 5);

    // deal with positive values in a triangular number influenced way
    if (value > 0)
    {
        *dif_inc += positive_base * value + mod * (value * (value - 1) / 2);
    }
}

/*
 * Signed difficulty modifier.
 *
 * Positive values use the normal triangular progression.
 * Negative values reduce difficulty, but only by half as much as the matching
 * positive bonus would increase it.
 */
static int dif_mod_signed(int value, int positive_base)
{
    int mod = 1 + ((positive_base - 1) / 5);

    if (value > 0)
    {
        return positive_base * value + mod * (value * (value - 1) / 2);
    }
    else if (value < 0)
    {
        int abs_value = -value;
        int negative_base = (positive_base + 1) / 2;
        int negative_mod = 1 + ((negative_base - 1) / 5);
        return -(negative_base * abs_value
            + negative_mod * (abs_value * (abs_value - 1) / 2));
    }

    return 0;
}

/*
 * Determines the difficulty of a given object.
 */
int object_difficulty(object_type* o_ptr)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    int x, new, base;
    int i;
    int dif = 0;
    int dif_inc = 0;
    int dif_dec = 0;
    int weight_factor;
    u32b f1, f2, f3, f4;
    int brands = 0;
    int dif_mult = 100;
    int cat = 0; // default to soothe compilation warnings

    bool telchar_bonus = (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR);
    bool feanor_bonus  = (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_FEANOR);

    // reset smithing costs
    smithing_cost.str = 0;
    smithing_cost.dex = 0;
    smithing_cost.con = 0;
    smithing_cost.gra = 0;
    smithing_cost.exp = 0;
    smithing_cost.mithril = 0;
    smithing_cost.star_iron = 0;
    smithing_cost.alloy_weight = 0;
    smithing_cost.alloy_metal = SMITH_ALLOY_NONE;
    smithing_cost.alloy_mastery = 0;
    smithing_cost.uses = 1;
    smithing_cost.drain = 0;
    smithing_cost.weaponsmith = 0;
    smithing_cost.armoursmith = 0;
    smithing_cost.jeweller = 0;
    smithing_cost.enchantment = 0;
    smithing_cost.artifice = 0;

    // extract object flags
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    int att_base = o_ptr->att;
    int evn_base = o_ptr->evn;
    int ds_base = o_ptr->ds;
    int ps_base = o_ptr->ps;

    /* When smithing, ignore the optional alloy bonus for difficulty/costs. */
    if (o_ptr == smith_o_ptr)
    {
        att_base -= smith_alloy.bonus_att;
        evn_base -= smith_alloy.bonus_evn;
        ds_base -= smith_alloy.bonus_ds;
        ps_base -= smith_alloy.bonus_ps;
    }

    /* ------------------------------------------------------------------
     *  GAMIL character bonus
     *  � Craft mithril items without mithril material
     *  � Costs 3 forge uses instead of 1
     *  � Mark item with TR3_CANT_MELT so the melt-menu ignores it
     * ------------------------------------------------------------------ */


    /* Telchar: 25 % discount on Sharpness tiers */
    if (telchar_bonus && (f1 & (TR1_SHARPNESS | TR1_SHARPNESS2) || (f3 & TR3_ACCURATE)))
        dif_mult -= 25;

    /*  FEANOR character bonus
     *  � 40% off on all lamps
     *  � 25% off on any fire- or light-branded object */
    if (feanor_bonus)
    {
        /* 40% off on all lamps */
        if (o_ptr->tval == TV_LIGHT)
            dif_mult -= 40;
        /* 25% off on any fire- or light-branded object */
        else if ((f1 & TR1_BRAND_FIRE) || (f2 & (TR2_LIGHT | TR2_RADIANCE)))
            dif_mult -= 25;
    }

    // special rules for horns
    if (o_ptr->tval == TV_HORN)
    {
        dif_inc += k_ptr->level - 1;
        switch (o_ptr->sval)
        {
        case SV_HORN_TERROR:
            smithing_cost.gra += 1;
            break;
        case SV_HORN_THUNDER:
            smithing_cost.dex += 1;
            break;
        case SV_HORN_FORCE:
            smithing_cost.str += 1;
            break;
        case SV_HORN_BLASTING:
            smithing_cost.con += 1;
            break;
            // SV_HORN_WARNING
        }
    }

    // different rules for most other items
    else if (!((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET)))
    {
        // We need to ignore the flags that are basic
        // to the object type and focus on the special/artefact ones. We can do
        // this by subtracting out the basic flags
        f1 &= ~(k_ptr->flags1);
        f2 &= ~(k_ptr->flags2);
        f3 &= ~(k_ptr->flags3);
        f4 &= ~(k_ptr->flags4);

        // need to add tunneling back in...
        if (k_ptr->flags1 & TR1_TUNNEL)
            f1 |= TR1_TUNNEL;

        // need to add stealth back in...
        if (k_ptr->flags1 & TR1_STL)
            f1 |= TR1_STL;

        // need to add accuracy back in...
        if (k_ptr->flags3 & TR3_ACCURATE)
            f3 |= TR3_ACCURATE;

        // need to add sharpness back in...
        if (k_ptr->flags1 & (TR1_SHARPNESS | TR1_SHARPNESS2))
            f1 |= (k_ptr->flags1 & (TR1_SHARPNESS | TR1_SHARPNESS2));

        // need to add mithril-specific flags back in...
        // These are flags that appear on base mithril items but should
        // count toward difficulty as they are "special" properties
        if (k_ptr->flags1 & TR1_DAMAGE_SIDES)
            f1 |= TR1_DAMAGE_SIDES;
        if (k_ptr->flags2 & TR2_REGEN)
            f2 |= TR2_REGEN;
        if (k_ptr->flags2 & TR2_RES_COLD)
            f2 |= TR2_RES_COLD;
        if (k_ptr->flags2 & TR2_RES_FIRE)
            f2 |= TR2_RES_FIRE;
        if (k_ptr->flags3 & TR3_CHEAT_DEATH)
            f3 |= TR3_CHEAT_DEATH;
        if (k_ptr->flags3 & TR3_STAND_FAST)
            f3 |= TR3_STAND_FAST;
        if (k_ptr->flags3 & TR3_ENCHANTABLE)
            f3 |= TR3_ENCHANTABLE;

        // base item
        dif_inc += k_ptr->level / 2;
    }

    // unusual weight
    if (o_ptr->weight == 0)
        weight_factor = 1100;
    else if (o_ptr->weight > k_ptr->weight)
        weight_factor = 100 * o_ptr->weight / k_ptr->weight;
    else
        weight_factor = 100 * k_ptr->weight / o_ptr->weight;

    dif_inc += (weight_factor - 100) / 20;
    if (f4 & (TR4_WEIGHT | TR4_NEG_WEIGHT))
        dif_inc += 5;

    // Jewelry combat bonuses are paid from zero, regardless of base item mins.
    int smith_base_att = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : k_ptr->att;
    int smith_base_evn = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : k_ptr->evn;
    int smith_base_ds = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : k_ptr->ds;
    int smith_base_prot = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : ((k_ptr->ps > 0) ? ((k_ptr->ps + 1) * k_ptr->pd) : 0);

    // attack bonus
    x = att_base - smith_base_att;

    // special costs for attack bonus for weapons
    if (o_ptr->tval == TV_ARROW || o_ptr->tval == TV_BOW
        || o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM
        || o_ptr->tval == TV_HAFTED)
    {
        dif_inc += dif_mod_signed(x, 3);
    }
    // normal costs for other items
    else
    {
        dif_inc += dif_mod_signed(x, 6);
        if (x > 0)
            dif_inc -= 1;
    }

    // evasion bonus
    x = evn_base - smith_base_evn;
    if (o_ptr->tval == TV_SOFT_ARMOR || o_ptr->tval == TV_MAIL
        || o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_HELM
        || o_ptr->tval == TV_CROWN || o_ptr->tval == TV_CLOAK
        || o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
    {
        dif_inc += dif_mod_signed(x, 6);
        if (x > 0)
            dif_inc -= 1;
    }
    else
    {
        dif_inc += dif_mod_signed(x, 9);
        if (x > 0)
            dif_inc -= 2;
    }

    // damage bonus
    x = (ds_base - smith_base_ds);
    // dd used to be a factor here, but a shortsword is far more breakable than
    // a great axe adjusted to make >1 damage sides expensive to smith
    dif_inc += dif_mod_signed(x, 3 * ABS(x) + 2);

    // protection bonus
    base = smith_base_prot;
    int ps_calc = (ps_base > 0) ? ps_base : 0;
    new = (ps_calc > 0) ? ((ps_calc + 1) * o_ptr->pd) : 0;
    x = new - base;

    // special costs for protection sides on hauberks and amulets
    if ((o_ptr->tval == TV_MAIL) && (o_ptr->sval == SV_LONG_CORSLET))
    {
        dif_inc += dif_mod_signed(x, 1);
        if (x > 0)
            dif_inc += 2;
    }
    else if (o_ptr->tval == TV_AMULET)
    {
        dif_inc += dif_mod_signed(x, 1);
        if (x > 0)
            dif_inc += 4;
    }
    else
    {
        dif_inc += dif_mod_signed(x, 3);
    }

    // weapon modifiers
    if (f1 & TR1_SLAY_ORC)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_TROLL)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_WOLF)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_SPIDER)
    {
        dif_inc += 4;
    }
    if (f1 & TR1_SLAY_UNDEAD)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_RAUKO)
    {
        dif_inc += 4;
    }
    if (f1 & TR1_SLAY_DRAGON)
    {
        dif_inc += 4;
    }
    if (f1 & TR1_SLAY_MAN_OR_ELF)
    {
        dif_inc += 5;
    }

    if (f4 & TR4_SLAY_SERPENT)
    {
        dif_inc += 4;
    }
    if (f4 & TR4_SLAY_VAMPIRE)
    {
        dif_inc += 4;
    }
    if (f4 & TR4_SLAY_HORROR)
    {
        dif_inc += 4;
    }
    if (f4 & TR4_SLAY_CAT)
    {
        dif_inc += 3;
    }
    if (f4 & TR4_SLAY_GIANT)
    {
        dif_inc += 3;
    }

    if (f1 & TR1_BRAND_COLD)
    {
        dif_inc += 18;
        smithing_cost.str += 2;
        brands++;
    }
    if (f1 & TR1_BRAND_FIRE)
    {
        dif_inc += 14;
        smithing_cost.str += 2;
        brands++;
    }
    if (f1 & TR1_BRAND_POIS)
    {
        if (o_ptr->tval == TV_ARROW)
        {
            dif_inc += 12;
            smithing_cost.str += 1;
        }
        else
        {
            dif_inc += 16;
            smithing_cost.str += 2;
            brands++;
        }
    }
    if (f1 & TR1_BRAND_ELEC)
    {
        dif_inc += 16;  // No monsters have HURT_ELEC, same as poison
        smithing_cost.str += 2;
        brands++;
    }
    if (brands > 1)
    {
        dif_inc += (brands - 1) * 20;
    }

    if (f1 & TR1_SHARPNESS)
    {
        int sharpness_base = (o_ptr->tval == TV_ARROW) ? 14 : 24;
        dif_inc += sharpness_base;
        smithing_cost.str += (o_ptr->tval == TV_ARROW) ? 1 : 2;
    }
    if (f1 & TR1_SHARPNESS2)
    {
        int sharpness2_base = 40;
        dif_inc += sharpness2_base;
        smithing_cost.str += 4;
    }
    if (f1 & TR1_VAMPIRIC)
    {
        dif_inc += 6;
        smithing_cost.str += 1;
    }
    if (f3 & TR3_WILL_DRAIN)
    {
        dif_inc += 8;  // Like VAMPIRIC+2
    }
    if (f3 & TR3_ACCURATE)
    {
        dif_inc += 15;
        smithing_cost.dex += 1;
    }
    if (f4 & TR4_ARMOR_SHATTER)
    {
        dif_inc += 15;  // Like ACCURATE
    }
    if (f4 & TR4_DEPTH_SCALE_PS)
    {
        dif_inc += 5;  // Situational
    }
    if (f4 & TR4_PAIRED)
    {
        dif_inc += 3;  // Paired weapon bonus
    }
    if (f4 & TR4_SUBTLETY_THROW)
    {
        dif_inc += 15;
    }

    // pval dependent bonuses
    if (f1 & TR1_TUNNEL)
    {
        x = o_ptr->pval - k_ptr->pval;
        dif_mod(x, 8, &dif_inc);
        smithing_cost.str += (x > 0) ? x : 0;
    }

    /* Per-stat/skill bonuses (no longer necessarily tied to a single pval). */
    if (o_ptr->pval > 0 && (f1 & TR1_DAMAGE_SIDES))
    {
        x = o_ptr->pval;
        dif_mod(x, 18, &dif_inc);
        smithing_cost.str += x;
    }

    if (o_ptr->stat_bonus[A_STR] > 0)
    {
        x = o_ptr->stat_bonus[A_STR];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.str += x;
    }
    if (o_ptr->stat_bonus[A_DEX] > 0)
    {
        x = o_ptr->stat_bonus[A_DEX];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.dex += x;
    }
    if (o_ptr->stat_bonus[A_CON] > 0)
    {
        x = o_ptr->stat_bonus[A_CON];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.con += x;
    }
    if (o_ptr->stat_bonus[A_GRA] > 0)
    {
        x = o_ptr->stat_bonus[A_GRA];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.gra += x;
    }

    if (o_ptr->skill_bonus[S_ARC] > 0)
    {
        x = o_ptr->skill_bonus[S_ARC];
        dif_mod(x, 4, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_STL] > 0)
    {
        x = o_ptr->skill_bonus[S_STL];
        dif_mod(x, 4, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_PER] > 0)
    {
        x = o_ptr->skill_bonus[S_PER];
        dif_mod(x, 3, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_WIL] > 0)
    {
        x = o_ptr->skill_bonus[S_WIL];
        dif_mod(x, 3, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_SMT] > 0)
    {
        x = o_ptr->skill_bonus[S_SMT];
        dif_mod(x, 4, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_SNG] > 0)
    {
        x = o_ptr->skill_bonus[S_SNG];
        dif_mod(x, 4, &dif_inc);
    }

    /*
     * Extra difficulty for multiple distinct stat/skill bonuses.
     * First bonus is "free" (already covered by the per-bonus scaling above).
     */
    {
        int stat_count = 0;
        int skill_count = 0;

        if (o_ptr->stat_bonus[A_STR] > 0)
            stat_count++;
        if (o_ptr->stat_bonus[A_DEX] > 0)
            stat_count++;
        if (o_ptr->stat_bonus[A_CON] > 0)
            stat_count++;
        if (o_ptr->stat_bonus[A_GRA] > 0)
            stat_count++;

        if (o_ptr->skill_bonus[S_ARC] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_STL] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_PER] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_WIL] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_SMT] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_SNG] > 0)
            skill_count++;

        if (stat_count > 1)
            dif_inc += (stat_count - 1) * 7;
        if (skill_count > 1)
            dif_inc += (skill_count - 1) * 3;
    }

    // Sustains
    if (f2 & TR2_SUST_STR)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_SUST_DEX)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_SUST_CON)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_SUST_GRA)
    {
        dif_inc += 2;
    }

    // Abilities
    if (f2 & TR2_SLOW_DIGEST)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RADIANCE)
    {
        dif_inc += 6;
        smithing_cost.gra += 1;
    }
    if (f2 & TR2_LIGHT)
    {
        dif_inc += 8;
        smithing_cost.gra += 1;
    }
    if (f2 & TR2_REGEN)
    {
        dif_inc += 4;
    }
    if (f2 & TR2_SEE_INVIS)
    {
        dif_inc += 4;
    }
    if (f2 & TR2_FREE_ACT)
    {
        dif_inc += 7;
    }
    if (f2 & TR2_SPEED)
    {
        dif_inc += 40;
        smithing_cost.con += 5;
    }
    if (f3 & TR3_CHEAT_DEATH)
    {
        dif_inc += 13;
        smithing_cost.con += 1;
    }
    if (f3 & TR3_STAND_FAST)
    {
        dif_inc += 2;
    }
    if (f3 & TR3_AVOID_TRAPS)
    {
        dif_inc += 6;
    }
    if (f3 & TR3_MEDIC)
    {
        dif_inc += 4;
    }
    if (f3 & TR3_OATH_BOOST)
    {
        dif_inc += 5;
    }
    if (f3 & TR3_OATH_NEGATE)
    {
        dif_dec += 5;
    }

    // Elemental Resistances
    if (f2 & TR2_RES_COLD)
    {
        dif_inc += 5;
    }
    if (f2 & TR2_RES_FIRE)
    {
        dif_inc += 5;
    }
    if (f2 & TR2_RES_POIS)
    {
        dif_inc += 5;
    }
    if (f2 & TR2_RES_ELEC)
    {
        dif_inc += 5;
    }

    // Other Resistances
    if (f2 & TR2_RES_BLEED)
    {
        dif_inc += 1;
    }
    if (f2 & TR2_RES_BLIND)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_CONFU)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_STUN)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_FEAR)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_HALLU)
    {
        dif_inc += 1;
    }

    // Penalty Flags
    if (!o_ptr->name1)
    {
        if (f2 & TR2_DANGER)
        {
            dif_dec += 5;
        } // only Danger counts
        if (f2 & TR2_DARKNESS)
        {
            dif_dec += 2;  // Changed from 3
        }
        if (f2 & TR2_AGGRAVATE)
        {
            dif_dec += 3;
        }
        if (f2 & TR2_HAUNTED)
        {
            dif_dec += 5;
        }
        if (f2 & TR2_VUL_COLD)
        {
            dif_dec += 4;
        }
        if (f2 & TR2_VUL_FIRE)
        {
            dif_dec += 4;
        }
        if (f2 & TR2_VUL_POIS)
        {
            dif_dec += 4;
        }
        if (f2 & TR2_TRAITOR)
        {
            dif_dec += 2;
        }
        if (f3 & TR3_LIGHT_CURSE)
        {
            dif_dec += 2;
        }
        if (f3 & TR3_CUMBERSOME)
        {
            dif_dec += 3;
        }
        if (f4 & TR4_UNLIGHT)
        {
            dif_dec += 5;  // Worse than DARKNESS - pure negative, no light bonus
        }
        if (f2 & TR2_SLOWNESS)
        {
            dif_dec += 15;
        }
        if (f2 & TR2_HUNGER)
        {
            dif_dec += 3;
        }
        if (f2 & TR2_FEAR)  // Not RES_FEAR!
        {
            dif_dec += 5;
        }
        if (f3 & TR3_HEAVY_CURSE)
        {
            dif_dec += 4;
        }
        if (f3 & TR3_PERMA_CURSE)
        {
            dif_dec += 6;
        }
    }

    // Abilities
    for (i = 0; i < o_ptr->abilities; i++)
    {
        int level = (&b_info[ability_index(
                         o_ptr->skilltype[i], o_ptr->abilitynum[i])])
                        ->level;

        dif_inc += 5 + (level / 3);
        smithing_cost.exp += 50 * level;
    }

    // Penalty for being an artefact
    if (o_ptr->name1)
    {
        if (!(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_FEANOR)) smithing_cost.uses +=2;
        // else smithing_cost.uses += 2;
    }

    // Set the overall difficulty
    dif = dif_inc - dif_dec;

    // Increased difficulties for minor slots
    switch (wield_slot(o_ptr))
    {
    // case INVEN_WIELD:
    case INVEN_LEFT:
    case INVEN_RIGHT:
    {
        // Celebrimbor: rings are not minor slots (no penalty)
        if (!(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_CELEBRIMBOR))
        {
            dif_mult += 20;
        }
        break;
    }
    // case INVEN_NECK:
    case INVEN_LITE:
    // case INVEN_BODY:
    case INVEN_OUTER:
    // case INVEN_ARM:
    // case INVEN_HEAD:
    case INVEN_HANDS:
    case INVEN_FEET:
    case INVEN_QUIVER1:
    case INVEN_QUIVER2:
    case INVEN_HORN:
    {
        dif_mult += 20;
        break;
    }
    }

    // Decreased difficulties for easily enchatable items
    if (k_ptr->flags3 & (TR3_ENCHANTABLE))
    {
        dif_mult -= 30;
    }

    // Celebrimbor: treat rings as enchantable
    if ((c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_CELEBRIMBOR)
        && (o_ptr->tval == TV_RING))
    {
        dif_mult -= 30;
    }

    // Mithril
    if (k_ptr->flags3 & TR3_MITHRIL)
    {
        smithing_cost.mithril += o_ptr->weight;
    }
    // Star iron
    if (k_ptr->flags3 & TR3_STAR_IRON)
    {
        smithing_cost.star_iron += o_ptr->weight;
    }

    /* Optional alloy bonus */
    if (smith_alloy.type != SMITH_ALLOY_NONE)
    {
        int alloy_weight = smith_alloy_weight_required(o_ptr);
        smithing_cost.alloy_weight = alloy_weight;
        smithing_cost.alloy_metal = smith_alloy.type;

        if (smith_alloy.type == SMITH_ALLOY_MITHRIL)
            smithing_cost.mithril += alloy_weight;
        else if (smith_alloy.type == SMITH_ALLOY_STAR_IRON)
            smithing_cost.star_iron += alloy_weight;
    }

   /* Gamil character bonus � override normal mithril cost */
  if ((c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_GAMIL)      /* you�re Gamil */
      && (k_ptr->flags3 & TR3_MITHRIL)                     /* item is mithril */
      && (mithril_carried() < smithing_cost.mithril))      /* no mithril on hand */
  {
      smithing_cost.uses    = MAX(smithing_cost.uses, 3);  /* cost 3 forge uses */
      smithing_cost.mithril = 0;                           /* waive material */
      o_ptr->ident         |= IDENT_CANT_MELT;             /* can�t melt later */
  }

    // Apply the difficulty multiplier
    dif = dif * dif_mult / 100;

    // Artefact arrows are much easier
    if ((o_ptr->tval == TV_ARROW) && (o_ptr->name1))
        dif /= 2;

    // Deal with masterpiece and Aule's Forge
    int effective_skill = p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px);
    
    if (p_ptr->have_ability[S_SPC][SPC_AULE]) {
        // Aule's Forge: supersedes Masterpiece, allows burning base skill for 2x difficulty allowance
        int max_aule_difficulty = effective_skill + (p_ptr->skill_base[S_SMT] * 2);
        if (dif > effective_skill) {
            if (dif <= max_aule_difficulty) {
                // Can craft this with Aule's Forge - drain base skill efficiently
                int excess = dif - effective_skill;
                smithing_cost.drain += (excess + 1) / 2; // drain 1 skill for every 2 excess points
                log_trace("ABILITY DEBUG: Aule's Forge drain - base_skill: %d, skill_use: %d, effective: %d, max_aule: %d, difficulty: %d, excess: %d, drain: %d", 
                         p_ptr->skill_base[S_SMT], p_ptr->skill_use[S_SMT], effective_skill, max_aule_difficulty, dif, excess, (excess + 1) / 2);
            } else {
                // Too difficult even with Aule's Forge
                smithing_cost.drain += p_ptr->skill_base[S_SMT] + (dif - max_aule_difficulty);
                log_trace("ABILITY DEBUG: Aule's Forge insufficient - max possible: %d, difficulty: %d", max_aule_difficulty, dif);
            }
        } else {
            log_trace("ABILITY DEBUG: Aule's Forge active - no drain needed (difficulty %d <= effective skill %d)", dif, effective_skill);
        }
    } else if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE]) {
        // Regular Masterpiece ability - allows burning base skill for 1x difficulty allowance
        int max_masterpiece_difficulty = effective_skill + p_ptr->skill_base[S_SMT];
        if (dif > effective_skill) {
            if (dif <= max_masterpiece_difficulty) {
                // Can craft this with Masterpiece - drain base skill normally
                smithing_cost.drain += dif - effective_skill;
            } else {
                // Too difficult even with Masterpiece
                smithing_cost.drain += p_ptr->skill_base[S_SMT] + (dif - max_masterpiece_difficulty);
            }
        }
    }

    bool needs_alloy_mastery = ((k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON)) != 0)
        || (smith_alloy.type != SMITH_ALLOY_NONE);

    // determine which additional smithing abilities would be required
    cat = smith_item_category(smith_o_ptr);
    if ((cat == CAT_WEAPON) && !p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH])
    {
        smithing_cost.weaponsmith = 1;
    }
    if ((cat == CAT_ARMOUR) && !p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH])
    {
        smithing_cost.armoursmith = 1;
    }
    if ((cat == CAT_JEWELRY) && !p_ptr->active_ability[S_SMT][SMT_JEWELLER])
    {
        smithing_cost.jeweller = 1;
    }
    if (smith_o_ptr->name1 && !p_ptr->active_ability[S_SMT][SMT_ARTEFACT])
    {
        smithing_cost.artifice = 1;
    }
    if (object_has_ego(smith_o_ptr) && !p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT])
    {
        smithing_cost.enchantment = 1;
    }
    if (needs_alloy_mastery && !p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY])
    {
        smithing_cost.alloy_mastery = 1;
    }
    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        smithing_cost.str = 0;
        smithing_cost.dex = 0;
        smithing_cost.con = 0;
        smithing_cost.gra = 0;
        smithing_cost.exp = 0;
    }

    return (dif);
}

/*
 * Clears the object's name and description at the bottom of the screen.
 */

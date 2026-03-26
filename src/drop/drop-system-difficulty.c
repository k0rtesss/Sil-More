/* File: drop-system-difficulty.c */

#include "angband.h"
#include "drop/drop-system-internal.h"
#include "externs.h"
#include "reliability-checks.h"

/* Baseline smithing difficulty (player-neutral). */
static void drop_dif_mod(int value, int positive_base, int* dif_inc)
{
    if (value > 0)
    {
        int mod = 1 + ((positive_base - 1) / 5);
        *dif_inc += positive_base * value + mod * (value * (value - 1) / 2);
    }
    else if (value < 0)
    {
        int abs_value = -value;
        int negative_base = (positive_base + 1) / 2;
        int negative_mod = 1 + ((negative_base - 1) / 5);
        *dif_inc -= negative_base * abs_value
            + negative_mod * (abs_value * (abs_value - 1) / 2);
    }
}

/* Slot determination without using inventory state (only for difficulty multiplier) */
static s16b neutral_wield_slot(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
        return INVEN_WIELD;
    case TV_BOW:
        return INVEN_BOW;
    case TV_STAFF:
        return INVEN_STAFF;
    case TV_RING:
        return INVEN_LEFT;
    case TV_AMULET:
        return INVEN_NECK;
    case TV_LIGHT:
        return INVEN_LITE;
    case TV_MAIL:
    case TV_SOFT_ARMOR:
        return INVEN_BODY;
    case TV_CLOAK:
        return INVEN_OUTER;
    case TV_SHIELD:
        return INVEN_ARM;
    case TV_CROWN:
    case TV_HELM:
        return INVEN_HEAD;
    case TV_GLOVES:
        return INVEN_HANDS;
    case TV_BOOTS:
        return INVEN_FEET;
    case TV_ARROW:
        return INVEN_QUIVER1;
    default:
        break;
    }
    return -1;
}

bool object_uses_smithing_difficulty(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    switch (o_ptr->tval)
    {
    case TV_ARROW:
        /* Simple arrows are treated as supply; ego/artifact arrows use difficulty. */
        return (o_ptr->name1 != 0) || object_has_ego(o_ptr);

    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_BOW:
    case TV_DIGGING:
        return true;

    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
        return true;

    case TV_RING:
    case TV_AMULET:
    case TV_HORN:
        return true;

    case TV_LIGHT:
        /* Non-Feanorian lights are treated as supply, except Grace lesser jewels. */
        if (o_ptr->sval == SV_LIGHT_FEANORIAN || o_ptr->sval == SV_LIGHT_SILMARIL)
            return true;
        if (o_ptr->sval == SV_LIGHT_LESSER_JEWEL && object_has_ego_idx(o_ptr, EGO_GRACE))
            return true;
        return false;

    default:
        return false;
    }
}

int smithing_difficulty_baseline(const object_type* o_ptr)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    int x, newv, base;
    int dif_inc = 0;
    int dif_dec = 0;
    int weight_factor;
    u32b f1, f2, f3, f4;
    int brands = 0;
    int dif_mult = 100;
    int smith_base_att;
    int smith_base_evn;
    int smith_base_ds;
    int smith_base_prot;

    /* Extract flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    /* Base handling for non-jewelry: add base level */
    if (o_ptr->tval != TV_RING && o_ptr->tval != TV_AMULET)
    {
        /* Note: We do NOT strip base flags anymore.
           We want the total difficulty/value of the item, including its intrinsic properties.
           This ensures high-tier base items (like Mithril) have appropriate difficulty. */
        dif_inc += k_ptr->level / 2;
    }

    /* Weight variance */
    if (o_ptr->weight == 0)
        weight_factor = 1100;
    else if (o_ptr->weight > k_ptr->weight)
        weight_factor = 100 * o_ptr->weight / k_ptr->weight;
    else
        weight_factor = 100 * k_ptr->weight / o_ptr->weight;
    dif_inc += (weight_factor - 100) / 20;
    if (f4 & (TR4_WEIGHT | TR4_NEG_WEIGHT))
        dif_inc += 5;

    smith_base_att = (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        ? 0
        : k_ptr->att;
    smith_base_evn = (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        ? 0
        : k_ptr->evn;
    smith_base_ds = (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        ? 0
        : k_ptr->ds;
    smith_base_prot = (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        ? 0
        : ((k_ptr->ps > 0) ? ((k_ptr->ps + 1) * k_ptr->pd) : 0);

    /* Attack bonus */
    x = o_ptr->att - smith_base_att;
    if ((o_ptr->tval == TV_ARROW || o_ptr->tval == TV_BOW
            || o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM
            || o_ptr->tval == TV_HAFTED)
        && (x > 0))
    {
        drop_dif_mod(x, 3, &dif_inc);
    }
    else
    {
        drop_dif_mod(x, 6, &dif_inc);
        if (x > 0)
            dif_inc -= 1;
    }

    /* Evasion bonus */
    x = o_ptr->evn - smith_base_evn;
    if (o_ptr->tval == TV_MAIL || o_ptr->tval == TV_SOFT_ARMOR
        || o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_CLOAK
        || o_ptr->tval == TV_BOOTS || o_ptr->tval == TV_GLOVES
        || o_ptr->tval == TV_HELM || o_ptr->tval == TV_CROWN)
    {
        drop_dif_mod(x, 6, &dif_inc);
        if (x > 0)
            dif_inc -= 1;
    }
    else
    {
        drop_dif_mod(x, 9, &dif_inc);
        if (x > 0)
            dif_inc -= 2;
    }

    /* Damage bonus */
    x = (o_ptr->ds - smith_base_ds);
    drop_dif_mod(x, 3 * ABS(x) + 2, &dif_inc);

    /* Protection bonus */
    base = smith_base_prot;
    newv = (o_ptr->ps > 0) ? ((o_ptr->ps + 1) * o_ptr->pd) : 0;
    x = newv - base;

    if ((o_ptr->tval == TV_MAIL) && (o_ptr->sval == SV_LONG_CORSLET) && (x > 0))
    {
        drop_dif_mod(x, 1, &dif_inc);
        dif_inc += 2;
    }
    else if ((o_ptr->tval == TV_AMULET) && (x > 0))
    {
        drop_dif_mod(x, 1, &dif_inc);
        dif_inc += 4;
    }
    else
    {
        drop_dif_mod(x, 3, &dif_inc);
    }

    /* Slays and brands */
    if (f1 & TR1_SLAY_ORC)
        dif_inc += 3;
    if (f1 & TR1_SLAY_TROLL)
        dif_inc += 3;
    if (f1 & TR1_SLAY_WOLF)
        dif_inc += 3;
    if (f1 & TR1_SLAY_SPIDER)
        dif_inc += 4;
    if (f1 & TR1_SLAY_UNDEAD)
        dif_inc += 3;
    if (f1 & TR1_SLAY_RAUKO)
        dif_inc += 4;
    if (f1 & TR1_SLAY_DRAGON)
        dif_inc += 4;
    if (f1 & TR1_SLAY_MAN_OR_ELF)
        dif_inc += 5;

    if (f4 & TR4_SLAY_SERPENT)
        dif_inc += 4;
    if (f4 & TR4_SLAY_VAMPIRE)
        dif_inc += 4;
    if (f4 & TR4_SLAY_HORROR)
        dif_inc += 4;
    if (f4 & TR4_SLAY_CAT)
        dif_inc += 3;
    if (f4 & TR4_SLAY_GIANT)
        dif_inc += 3;

    if (f1 & TR1_BRAND_COLD)
    {
        dif_inc += 18;
        brands++;
    }
    if (f1 & TR1_BRAND_FIRE)
    {
        dif_inc += 14;
        brands++;
    }
    if (f1 & TR1_BRAND_POIS)
    {
        if (o_ptr->tval == TV_ARROW)
            dif_inc += 12;
        else
        {
            dif_inc += 16;
            brands++;
        }
    }
    if (f1 & TR1_BRAND_ELEC)
    {
        dif_inc += 16;  /* No monsters have HURT_ELEC, same as poison */
        brands++;
    }
    if (brands > 1)
        dif_inc += (brands - 1) * 20;

    if (f1 & TR1_SHARPNESS)
        dif_inc += (o_ptr->tval == TV_ARROW) ? 14 : 24;
    if (f1 & TR1_SHARPNESS2)
        dif_inc += 40;
    if (f1 & TR1_VAMPIRIC)
        dif_inc += 6;
    if (f3 & TR3_WILL_DRAIN)
        dif_inc += 8;  /* Like VAMPIRIC+2 */
    if (f3 & TR3_ACCURATE)
        dif_inc += 15;
    if (f4 & TR4_ARMOR_SHATTER)
        dif_inc += 15;  /* Like ACCURATE */
    if (f4 & TR4_DEPTH_SCALE_PS)
        dif_inc += 5;  /* Situational */
    if (f4 & TR4_PAIRED)
        dif_inc += 3;  /* Paired weapon bonus */

    /* pval-based bonuses */
    if (f1 & TR1_TUNNEL)
    {
        x = o_ptr->pval - k_ptr->pval;
        drop_dif_mod(x, 8, &dif_inc);
    }
    {
        if (f1 & TR1_DAMAGE_SIDES)
        {
            int v = o_ptr->pval;
            if (v > 0)
                drop_dif_mod(v, 18, &dif_inc);
        }

        if (f1 & (TR1_STR | TR1_NEG_STR))
        {
            int v = o_ptr->stat_bonus[A_STR];
            if (v > 0)
                drop_dif_mod(v, 14, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 10, &dif_dec);
        }
        if (f1 & (TR1_DEX | TR1_NEG_DEX))
        {
            int v = o_ptr->stat_bonus[A_DEX];
            if (v > 0)
                drop_dif_mod(v, 14, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 10, &dif_dec);
        }
        if (f1 & (TR1_CON | TR1_NEG_CON))
        {
            int v = o_ptr->stat_bonus[A_CON];
            if (v > 0)
                drop_dif_mod(v, 14, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 10, &dif_dec);
        }
        if (f1 & (TR1_GRA | TR1_NEG_GRA))
        {
            int v = o_ptr->stat_bonus[A_GRA];
            if (v > 0)
                drop_dif_mod(v, 14, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 10, &dif_dec);
        }

        if (f1 & TR1_ARC)
        {
            int v = o_ptr->skill_bonus[S_ARC];
            if (v > 0)
                drop_dif_mod(v, 4, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 3, &dif_dec);
        }
        if (f1 & TR1_STL)
        {
            int v = o_ptr->skill_bonus[S_STL];
            if (v > 0)
                drop_dif_mod(v, 4, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 3, &dif_dec);
        }
        if (f1 & TR1_PER)
        {
            int v = o_ptr->skill_bonus[S_PER];
            if (v > 0)
                drop_dif_mod(v, 3, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 2, &dif_dec);
        }
        if (f1 & TR1_WIL)
        {
            int v = o_ptr->skill_bonus[S_WIL];
            if (v > 0)
                drop_dif_mod(v, 3, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 2, &dif_dec);
        }
        if (f1 & TR1_SMT)
        {
            int v = o_ptr->skill_bonus[S_SMT];
            if (v > 0)
                drop_dif_mod(v, 4, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 3, &dif_dec);
        }
        if (f1 & TR1_SNG)
        {
            int v = o_ptr->skill_bonus[S_SNG];
            if (v > 0)
                drop_dif_mod(v, 4, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 3, &dif_dec);
        }
    }

    /*
     * Extra difficulty for multiple distinct stat/skill bonuses.
     * First bonus is "free" (already covered by the per-bonus scaling above).
     */
    {
        int stat_count = 0;
        int skill_count = 0;

        if ((f1 & TR1_STR) && o_ptr->stat_bonus[A_STR] > 0)
            stat_count++;
        if ((f1 & TR1_DEX) && o_ptr->stat_bonus[A_DEX] > 0)
            stat_count++;
        if ((f1 & TR1_CON) && o_ptr->stat_bonus[A_CON] > 0)
            stat_count++;
        if ((f1 & TR1_GRA) && o_ptr->stat_bonus[A_GRA] > 0)
            stat_count++;

        if ((f1 & TR1_ARC) && o_ptr->skill_bonus[S_ARC] > 0)
            skill_count++;
        if ((f1 & TR1_STL) && o_ptr->skill_bonus[S_STL] > 0)
            skill_count++;
        if ((f1 & TR1_PER) && o_ptr->skill_bonus[S_PER] > 0)
            skill_count++;
        if ((f1 & TR1_WIL) && o_ptr->skill_bonus[S_WIL] > 0)
            skill_count++;
        if ((f1 & TR1_SMT) && o_ptr->skill_bonus[S_SMT] > 0)
            skill_count++;
        if ((f1 & TR1_SNG) && o_ptr->skill_bonus[S_SNG] > 0)
            skill_count++;

        if (stat_count > 1)
            dif_inc += (stat_count - 1) * 7;
        if (skill_count > 1)
            dif_inc += (skill_count - 1) * 3;
    }

    /* Sustains */
    if (f2 & TR2_SUST_STR)
        dif_inc += 2;
    if (f2 & TR2_SUST_DEX)
        dif_inc += 2;
    if (f2 & TR2_SUST_CON)
        dif_inc += 2;
    if (f2 & TR2_SUST_GRA)
        dif_inc += 2;

    /* Abilities / misc flags */
    if (f2 & TR2_SLOW_DIGEST)
        dif_inc += 2;
    if (f2 & TR2_RADIANCE)
        dif_inc += 6;
    if (f2 & TR2_LIGHT)
        dif_inc += 8;
    if (f2 & TR2_REGEN)
        dif_inc += 4;
    if (f2 & TR2_SEE_INVIS)
        dif_inc += 4;
    if (f2 & TR2_FREE_ACT)
        dif_inc += 7;
    if (f2 & TR2_SPEED)
        dif_inc += 40;
    if (f3 & TR3_CHEAT_DEATH)
        dif_inc += 13;
    if (f3 & TR3_STAND_FAST)
        dif_inc += 2;
    if (f3 & TR3_AVOID_TRAPS)
        dif_inc += 6;
    if (f3 & TR3_MEDIC)
        dif_inc += 4;
    {
        int parity_delta = reliability_smithing_phase01_flag_delta(f2, f3, f4);
        if (parity_delta > 0)
            dif_inc += parity_delta;
        else if (parity_delta < 0)
            dif_dec += -parity_delta;
    }

    if (f2 & TR2_RES_COLD)
        dif_inc += 5;
    if (f2 & TR2_RES_FIRE)
        dif_inc += 5;
    if (f2 & TR2_RES_POIS)
        dif_inc += 5;
    if (f2 & TR2_RES_ELEC)
        dif_inc += 5;

    if (f2 & TR2_RES_BLEED)
        dif_inc += 1;
    if (f2 & TR2_RES_BLIND)
        dif_inc += 2;
    if (f2 & TR2_RES_CONFU)
        dif_inc += 2;
    if (f2 & TR2_RES_STUN)
        dif_inc += 2;
    if (f2 & TR2_RES_FEAR)
        dif_inc += 2;
    if (f2 & TR2_RES_HALLU)
        dif_inc += 1;

    /* Penalty flags - now apply to all items including artefacts */
    if (f2 & TR2_DANGER)
        dif_dec += 5;
    if (f2 & TR2_DARKNESS)
        dif_dec += 2;  /* Changed from 3 to match Python */
    if (f2 & TR2_AGGRAVATE)
        dif_dec += 3;
    if (f2 & TR2_HAUNTED)
        dif_dec += 5;
    if (f2 & TR2_VUL_COLD)
        dif_dec += 4;
    if (f2 & TR2_VUL_FIRE)
        dif_dec += 4;
    if (f2 & TR2_VUL_POIS)
        dif_dec += 4;
    if (f3 & TR3_CUMBERSOME)
        dif_dec += 3;
    if (f4 & TR4_UNLIGHT)
        dif_dec += 5;  /* Worse than DARKNESS - pure negative, no light bonus */
    if (f2 & TR2_SLOWNESS)
        dif_dec += 15;
    if (f2 & TR2_HUNGER)
        dif_dec += 3;
    if (f2 & TR2_FEAR)
        dif_dec += 5;

    /* Curse penalties */
    if (f3 & TR3_LIGHT_CURSE)
        dif_dec += 3;
    if (f3 & TR3_HEAVY_CURSE)
        dif_dec += 4;
    if (f3 & TR3_PERMA_CURSE)
        dif_dec += 8;

    /* Abilities */
    for (int i = 0; i < o_ptr->abilities; i++)
    {
        int level = (&b_info[ability_index(
                         o_ptr->skilltype[i], o_ptr->abilitynum[i])])
                        ->level;
        dif_inc += 5 + (level / 3);
    }

    {
        int dif = dif_inc - dif_dec;

        /* Minor slot multiplier */
        switch (neutral_wield_slot(o_ptr))
        {
        case INVEN_LEFT:
        case INVEN_RIGHT:
        case INVEN_LITE:
        case INVEN_OUTER:
        case INVEN_HANDS:
        case INVEN_FEET:
        case INVEN_QUIVER1:
        case INVEN_QUIVER2:
            dif_mult += 20;
            break;
        default:
            break;
        }

        if ((k_ptr->flags3 & TR3_ENCHANTABLE) || (f3 & TR3_ENCHANTABLE))
            dif_mult -= 30;

        dif = dif * dif_mult / 100;

        if ((o_ptr->tval == TV_ARROW) && (o_ptr->name1))
            dif /= 2;

        return dif;
    }
}

int object_smithing_difficulty(const object_type* o_ptr)
{
    if (!object_uses_smithing_difficulty(o_ptr))
        return 0;

    return smithing_difficulty_baseline(o_ptr);
}

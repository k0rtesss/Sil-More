/* File: cmd-identify.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "object/object-ui-select.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"

static void slay_desc(char* description, u32b flag, const monster_type* m_ptr)
{
    char m_name[80];
    char m_poss[80];

    /* Monster description */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
    monster_desc(m_poss, sizeof(m_poss), m_ptr, 0x22);

    if (flag == TR3_WILL_DRAIN)
    {
        sprintf(description, "drains %s will", m_poss);
        return;
    }

    switch (flag)
    {
    case TR1_SHARPNESS:
        sprintf(description, "cuts deeply");
        break;
    case TR1_SHARPNESS2:
        sprintf(description, "cuts effortlessly");
        break;
    case TR1_VAMPIRIC:
        sprintf(description, "drains life from %s", m_name);
        break;
    case TR1_SLAY_ORC:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_WOLF:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_SPIDER:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_UNDEAD:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_RAUKO:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_DRAGON:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_TROLL:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_MAN_OR_ELF:
        sprintf(description, "strikes truly");
        break;
    case TR4_SLAY_SERPENT:
    case TR4_SLAY_VAMPIRE:
    case TR4_SLAY_HORROR:
    case TR4_SLAY_CAT:
    case TR4_SLAY_GIANT:
        sprintf(description, "strikes truly");
        break;
    case TR1_BRAND_ELEC:
        sprintf(description, "shocks %s with the force of lightning", m_name);
        break;
    case TR1_BRAND_FIRE:
        sprintf(description, "burns %s with an inner fire", m_name);
        break;
    case TR1_BRAND_COLD:
        sprintf(description, "freezes %s", m_name);
        break;
    case TR1_BRAND_POIS:
        sprintf(description, "poisons %s", m_name);
        break;
    case TR4_ARMOR_SHATTER:
        sprintf(description, "shatters %s armor", m_poss);
        break;
    }

    return;
}

extern void ident(object_type* o_ptr)
{
    /* Identify it */
    object_aware(o_ptr);
    object_known(o_ptr);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    return;
}

extern void ident_on_wield(object_type* o_ptr)
{
    u32b f1, f2, f3, f4;
    u32b orig_f1;

    bool notice = false;

    char o_full_name[80];

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Get the flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    orig_f1 = f1;

    // Ignore previously identified items
    if (object_known_p(o_ptr))
    {
        return;
    }

    // identify the special item types that do nothing much
    // (since they have no hidden abilities, they must already be obvious)
    if (object_has_ego(o_ptr))
    {
        bool all_trivial = true;
        byte ego_pfx = object_ego_prefix(o_ptr);
        byte ego_sfx = object_ego_suffix(o_ptr);

        if (ego_pfx)
        {
            ego_item_type* e_ptr = &e_info[ego_pfx];
            if ((e_ptr->flags1 != 0L) || (e_ptr->flags2 != 0L)
                || ((e_ptr->flags3 | (TR3_IGNORE_ALL)) != (TR3_IGNORE_ALL))
                || (e_ptr->flags4 != 0L)
                || (e_ptr->abilities != 0))
            {
                all_trivial = false;
            }
        }

        if (ego_sfx)
        {
            ego_item_type* e_ptr = &e_info[ego_sfx];
            if ((e_ptr->flags1 != 0L) || (e_ptr->flags2 != 0L)
                || ((e_ptr->flags3 | (TR3_IGNORE_ALL)) != (TR3_IGNORE_ALL))
                || (e_ptr->flags4 != 0L)
                || (e_ptr->abilities != 0))
            {
                all_trivial = false;
            }
        }

        if (all_trivial)
            notice = true;
    }

    // identify true sight if it cures blindness
    if (p_ptr->blind && (f2 & (TR2_SEE_INVIS)))
    {
        notice = true;
    }

    // Currently tunneling is an unambiguous ego on mattocks, so auto-ID
    if (f1 & TR1_TUNNEL)
    {
        notice = true;
    }

    if (f3 & TR3_ACCURATE)
    {
        notice = true;
    }

    if (f3 & TR3_CUMBERSOME)
    {
        notice = true;
    }

    if (o_ptr->name1 || object_has_ego(o_ptr))
    {
        // For special items and artefacts, we need to ignore the flags that are
        // basic to the object type and focus on the special/artefact ones. We
        // can do this by subtracting out the basic flags

        f1 &= ~(k_ptr->flags1);
        f2 &= ~(k_ptr->flags2);
        f3 &= ~(k_ptr->flags3);

        /*
         * If a special/artefact modifies pval on a base that already has a pval
         * flag (e.g. Shadow Cloak has STEALTH), stripping base flags would hide
         * the effect and prevent auto-identification on wear.
         */
        {
            u32b base_pval_flags = (orig_f1 & k_ptr->flags1);

            if ((base_pval_flags & (TR1_TUNNEL | TR1_DAMAGE_SIDES))
                && (o_ptr->pval != k_ptr->pval))
            {
                f1 |= (base_pval_flags & (TR1_TUNNEL | TR1_DAMAGE_SIDES));
            }

            if ((base_pval_flags & (TR1_STR | TR1_NEG_STR))
                && (o_ptr->stat_bonus[A_STR] != k_ptr->stat_bonus[A_STR]))
                f1 |= (base_pval_flags & (TR1_STR | TR1_NEG_STR));
            if ((base_pval_flags & (TR1_DEX | TR1_NEG_DEX))
                && (o_ptr->stat_bonus[A_DEX] != k_ptr->stat_bonus[A_DEX]))
                f1 |= (base_pval_flags & (TR1_DEX | TR1_NEG_DEX));
            if ((base_pval_flags & (TR1_CON | TR1_NEG_CON))
                && (o_ptr->stat_bonus[A_CON] != k_ptr->stat_bonus[A_CON]))
                f1 |= (base_pval_flags & (TR1_CON | TR1_NEG_CON));
            if ((base_pval_flags & (TR1_GRA | TR1_NEG_GRA))
                && (o_ptr->stat_bonus[A_GRA] != k_ptr->stat_bonus[A_GRA]))
                f1 |= (base_pval_flags & (TR1_GRA | TR1_NEG_GRA));

            if ((base_pval_flags & TR1_MEL)
                && (o_ptr->skill_bonus[S_MEL] != k_ptr->skill_bonus[S_MEL]))
                f1 |= (base_pval_flags & TR1_MEL);
            if ((base_pval_flags & TR1_ARC)
                && (o_ptr->skill_bonus[S_ARC] != k_ptr->skill_bonus[S_ARC]))
                f1 |= (base_pval_flags & TR1_ARC);
            if ((base_pval_flags & TR1_STL)
                && (o_ptr->skill_bonus[S_STL] != k_ptr->skill_bonus[S_STL]))
                f1 |= (base_pval_flags & TR1_STL);
            if ((base_pval_flags & TR1_PER)
                && (o_ptr->skill_bonus[S_PER] != k_ptr->skill_bonus[S_PER]))
                f1 |= (base_pval_flags & TR1_PER);
            if ((base_pval_flags & TR1_WIL)
                && (o_ptr->skill_bonus[S_WIL] != k_ptr->skill_bonus[S_WIL]))
                f1 |= (base_pval_flags & TR1_WIL);
            if ((base_pval_flags & TR1_SMT)
                && (o_ptr->skill_bonus[S_SMT] != k_ptr->skill_bonus[S_SMT]))
                f1 |= (base_pval_flags & TR1_SMT);
            if ((base_pval_flags & TR1_SNG)
                && (o_ptr->skill_bonus[S_SNG] != k_ptr->skill_bonus[S_SNG]))
                f1 |= (base_pval_flags & TR1_SNG);
        }
    }

    if (f2 & (TR2_DARKNESS))
    {
        notice = true;
        msg_print("It shrouds you in darkness.");
    }
    else if (f4 & (TR4_UNLIGHT))
    {
        notice = true;
        msg_print("It dims your light.");
    }
    else if (f2 & (TR2_LIGHT))
    {
        if (o_ptr->tval != TV_LIGHT)
        {
            notice = true;
            msg_print("It glows with a wondrous light.");
        }
        else if ((o_ptr->sval == SV_LIGHT_FEANORIAN)
            || (o_ptr->sval == SV_LIGHT_LESSER_JEWEL)
            || player_light_has_fuel(o_ptr))
        {
            notice = true;
            msg_print("It glows very brightly.");
        }
    }
    else if (f2 & (TR2_SLOWNESS))
    {
        notice = true;
        msg_print("It slows your movement.");
    }
    else if (f2 & (TR2_SPEED))
    {
        notice = true;
        msg_print("It speeds your movement.");
    }

    else if (f1 & (TR1_DAMAGE_SIDES))
    {
        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (o_ptr->pval > 0)
        {
            notice = true;
            msg_print("You feel more forceful in melee.");
        }
        else if (o_ptr->pval < 0)
        {
            notice = true;
            msg_print("You feel less forceful in melee.");
        }
    }
    else if ((f1 & (TR1_STR)) || (f1 & (TR1_NEG_STR)))
    {
        int bonus = o_ptr->stat_bonus[A_STR];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel stronger.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less strong.");
        }
    }
    else if ((f1 & (TR1_DEX)) || (f1 & (TR1_NEG_DEX)))
    {
        int bonus = o_ptr->stat_bonus[A_DEX];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more agile.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less agile.");
        }
    }
    else if ((f1 & (TR1_CON)) || (f1 & (TR1_NEG_CON)))
    {
        int bonus = o_ptr->stat_bonus[A_CON];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more resilient.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less resilient.");
        }
    }
    else if ((f1 & (TR1_GRA)) || (f1 & (TR1_NEG_GRA)))
    {
        int bonus = o_ptr->stat_bonus[A_GRA];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more attuned to the world.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less attuned to the world.");
        }
    }
    else if (f1 & (TR1_MEL))
    {
        int bonus = o_ptr->skill_bonus[S_MEL];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more in control of your weapon.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less in control of your weapon.");
        }
    }
    else if (f1 & (TR1_ARC))
    {
        int bonus = o_ptr->skill_bonus[S_ARC];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more accurate at archery.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less accurate at archery.");
        }
    }
    else if (f1 & (TR1_STL))
    {
        int bonus = o_ptr->skill_bonus[S_STL];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("Your movements become quieter.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("Your movements less quiet.");
        }
    }
    else if (f1 & (TR1_PER))
    {
        int bonus = o_ptr->skill_bonus[S_PER];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more perceptive.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less perceptive.");
        }
    }
    else if (f1 & (TR1_WIL))
    {
        int bonus = o_ptr->skill_bonus[S_WIL];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more firm of will.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less firm of will.");
        }
    }
    else if (f1 & (TR1_SMT))
    {
        int bonus = o_ptr->skill_bonus[S_SMT];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel a desire to craft things with your hands.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less able to craft things.");
        }
    }
    else if (f1 & (TR1_SNG))
    {
        int bonus = o_ptr->skill_bonus[S_SNG];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You are filled with inspiration.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel a loss of inspiration.");
        }
    }

    // identify the item types that grant abilities
    else if (k_ptr->abilities > 0)
    {
        notice = true;
        msg_format("You have gained the ability '%s'.",
            b_name
                + (&b_info[ability_index(
                       k_ptr->skilltype[0], k_ptr->abilitynum[0])])
                      ->name);
    }

    // identify the special item types that grant abilities
    else if (object_has_ego(o_ptr))
    {
        byte ego_pfx = object_ego_prefix(o_ptr);
        byte ego_sfx = object_ego_suffix(o_ptr);

        ego_item_type* e_ptr = NULL;
        if (ego_pfx && e_info[ego_pfx].abilities > 0)
            e_ptr = &e_info[ego_pfx];
        else if (ego_sfx && e_info[ego_sfx].abilities > 0)
            e_ptr = &e_info[ego_sfx];

        if (e_ptr && e_ptr->abilities > 0)
        {
            notice = true;
            msg_format("You have gained the ability '%s'.",
                b_name
                    + (&b_info[ability_index(
                           e_ptr->skilltype[0], e_ptr->abilitynum[0])])
                          ->name);
        }
    }

    // identify the artefacts that grant abilities
    else if (o_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];

        if (a_ptr->abilities > 0)
        {
            notice = true;
            msg_format("You have gained the ability '%s'.",
                b_name
                    + (&b_info[ability_index(
                           a_ptr->skilltype[0], a_ptr->abilitynum[0])])
                          ->name);
        }
    }

    // can identify <+0> items if you already know the flavour
    else if (k_info[o_ptr->k_idx].flavor)
    {
        if (object_aware_p(o_ptr))
        {
            if (o_ptr->tval != TV_STAFF)
                notice = true;
        }
        else if (o_ptr->att > 0)
        {
            notice = true;
            msg_print("You somehow feel more accurate in combat.");
        }
        else if (o_ptr->att < 0)
        {
            notice = true;
            msg_print("You somehow feel less accurate in combat.");
        }
        else if (o_ptr->evn > 0)
        {
            notice = true;
            msg_print("You somehow feel harder to hit.");
        }
        else if (o_ptr->evn < 0)
        {
            notice = true;
            msg_print("You somehow feel more vulnerable.");
        }
        else if (o_ptr->pd > 0)
        {
            notice = true;
            msg_print("You somehow feel more protected.");
        }
    }

    if (notice)
    {
        if (object_uses_smithing_difficulty(o_ptr))
        {
            player_mark_object_experienced(o_ptr);
        }
        else
        {
            /* identify the object */
            ident(o_ptr);

            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

            /* Print the messages */
            msg_format("You recognize it as %s.", o_full_name);
        }
    }

    return;
}

extern void ident_resist(u32b flag)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char effect_string[80];
    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;
    object_kind* k_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];
        k_ptr = &k_info[o_ptr->k_idx];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        {
            bool is_quiver1 = (i == INVEN_QUIVER1);
            bool is_quiver2 = (i == INVEN_QUIVER2);
            bool is_throwing_item = player_can_treat_as_throwing_flags(o_ptr, f3);

            if (is_quiver1)
                continue;
            if (is_quiver2 && !is_throwing_item)
                continue;
        }

        if (o_ptr->name1 || object_has_ego(o_ptr))
        {
            // For special items and artefacts, we need to ignore the flags that
            // are basic to the object type and focus on the special/artefact
            // ones. We can do this by subtracting out the basic flags

            f1 &= ~(k_ptr->flags1);
            f2 &= ~(k_ptr->flags2);
            f3 &= ~(k_ptr->flags3);
        }

        if (!object_known_p(o_ptr))
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            if ((flag == TR2_RES_COLD) && (f2 & (TR2_RES_COLD)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s partly protects you from the chill.",
                    o_short_name);
            }
            else if ((flag == TR2_RES_FIRE) && (f2 & (TR2_RES_FIRE)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s partly protects you from the flame.",
                    o_short_name);
            }
            else if ((flag == TR2_RES_POIS) && (f2 & (TR2_RES_POIS)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s partly protects you from the poison.",
                    o_short_name);
            }
            else if ((flag == TR2_RES_BLEED) && (f2 & (TR2_RES_BLEED)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your bleeding is slowed by your %s.", o_short_name);
            }
            else if ((flag == TR2_RES_COLD) && (f2 & (TR2_VUL_COLD)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s intensifies the chill.", o_short_name);
            }
            else if ((flag == TR2_RES_FIRE) && (f2 & (TR2_VUL_FIRE)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s intensifies the flame.", o_short_name);
            }
            else if ((flag == TR2_RES_POIS) && (f2 & (TR2_VUL_POIS)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s intensifies the poison.", o_short_name);
            }
            else if ((flag == TR2_RES_FEAR) && (f2 & (TR2_RES_FEAR)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s fills you with courage.", o_short_name);
            }
            else if ((flag == TR2_RES_BLIND) && (f2 & (TR2_RES_BLIND)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s protects your sight.", o_short_name);
            }
            else if ((flag == TR2_RES_HALLU) && (f2 & (TR2_RES_HALLU)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s protects your sight.", o_short_name);
            }
            else if ((flag == TR2_RES_CONFU) && (f2 & (TR2_RES_CONFU)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s fills you with calm.", o_short_name);
            }
            else if ((flag == TR2_RES_STUN) && (f2 & (TR2_RES_STUN)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s fills you with calm.", o_short_name);
            }
            else if ((flag == TR2_FREE_ACT) && (f2 & (TR2_FREE_ACT)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s glows softly.", o_short_name);
            }
            else if ((flag == TR2_SUST_STR) && (f2 & (TR2_SUST_STR)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your strength.", o_short_name);
            }
            else if ((flag == TR2_SUST_DEX) && (f2 & (TR2_SUST_DEX)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your dexterity.", o_short_name);
            }
            else if ((flag == TR2_SUST_CON) && (f2 & (TR2_SUST_CON)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your constitution.", o_short_name);
            }
            else if ((flag == TR2_SUST_GRA) && (f2 & (TR2_SUST_GRA)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your grace.", o_short_name);
            }
        }

        if (notice)
        {
            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
                msg_format("%s", effect_string);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                /* Print the messages */
                msg_format("%s", effect_string);
                msg_format("You realize that it is %s.", o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_passive(void)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char effect_string[80];
    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_REGEN)) && (p_ptr->chp < p_ptr->mhp))
            {
                notice = true;
                SDL_strlcpy(effect_string,
                    "You notice that you are recovering much faster than "
                    "usual.",
                    sizeof(effect_string));
            }
            else if ((f2 & (TR2_AGGRAVATE)))
            {
                notice = true;
                SDL_strlcpy(effect_string,
                    "You notice that you are enraging your enemies.",
                    sizeof(effect_string));
            }
            else if ((f2 & (TR2_DANGER)))
            {
                notice = true;
                SDL_strlcpy(effect_string,
                    "You notice that you are attracting more powerful enemies.",
                    sizeof(effect_string));
            }
        }

        if (notice)
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            msg_format("%s", effect_string);

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_see_invisible(const monster_type* m_ptr)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char m_name[80];
    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_SEE_INVIS)))
            {
                notice = true;
            }
        }

        if (notice)
        {
            /* Get the monster name */
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            msg_format("You notice that you can see %s very clearly.", m_name);

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_haunted(void)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_HAUNTED)))
            {
                notice = true;
            }
        }

        if (notice)
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            msg_print("You notice that wraiths are being drawn to you.");

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

/*
 * Identifies a hunger or sustenance item and prints a message
 */
void ident_hunger(void)
{
    u32b f1, f2, f3;
    int i;
    bool notice = false;
    char o_full_name[80];
    char o_short_name[80];
    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_HUNGER)) && (p_ptr->hunger > 0))
            {
                notice = true;
            }

            if ((f2 & (TR2_SLOW_DIGEST)) && (p_ptr->hunger < 0))
            {
                notice = true;
            }
        }

        if (notice)
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            if (f2 & (TR2_HUNGER))
                msg_print("You notice that you are growing hungry much faster "
                          "than before.");
            else if (f2 & (TR2_SLOW_DIGEST))
                msg_print("You notice that you are growing hungry slower than "
                          "before.");

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_f2(u32b flag, object_type* supplied_object)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr = supplied_object;

    if (!o_ptr)
    {
        /* Scan the equipment */
        for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            o_ptr = &inventory[i];

            /* Skip non-objects */
            if (!o_ptr->k_idx)
                continue;

            /* Extract the item flags */
            object_flags(o_ptr, &f1, &f2, &f3);

            if (!object_known_p(o_ptr) && (f2 & (flag)))
            {
                notice = true;
                break;
            }
        }
    }
    else if (!object_known_p(o_ptr))
    {
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f2 & flag)
        {
            notice = true;
        }
    }

    if (notice && o_ptr)
    {
        /* Short, pre-identification object description */
        object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

        if (object_uses_smithing_difficulty(o_ptr))
        {
            player_mark_object_experienced(o_ptr);
            msg_format("You learn more about your %s.", o_short_name);
        }
        else
        {
            /* identify the object */
            ident(o_ptr);

            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

            msg_format(
                "You realize that your %s is %s.", o_short_name, o_full_name);
        }
    }
}

extern void ident_f3(u32b flag, object_type* supplied_object)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr = supplied_object;

    if (!o_ptr)
    {
        /* Scan the equipment */
        for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            o_ptr = &inventory[i];

            /* Skip non-objects */
            if (!o_ptr->k_idx)
                continue;

            /* Extract the item flags */
            object_flags(o_ptr, &f1, &f2, &f3);

            if (!object_known_p(o_ptr) && (f3 & (flag)))
            {
                notice = true;
                break;
            }
        }
    }
    else if (!object_known_p(o_ptr))
    {
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f3 & flag)
        {
            notice = true;
        }
    }

    if (notice && o_ptr)
    {
        /* Short, pre-identification object description */
        object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

        if (object_uses_smithing_difficulty(o_ptr))
        {
            player_mark_object_experienced(o_ptr);
            msg_format("You learn more about your %s.", o_short_name);
        }
        else
        {
            /* identify the object */
            ident(o_ptr);

            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

            msg_format(
                "You realize that your %s is %s.", o_short_name, o_full_name);
        }
    }
}

/*
 * Identifies a weapon from one of its slays being active and prints a message
 */
void ident_weapon_by_use(
    object_type* o_ptr, const monster_type* m_ptr, u32b flag)
{
    char o_short_name[80];
    char o_full_name[80];
    char slay_description[160];

    /* Short, pre-identification object description */
    object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

    /* Description of the 'slay' */
    slay_desc(slay_description, flag, m_ptr);

    /* Print the messages */
    msg_format("Your %s %s.", o_short_name, slay_description);
    if (object_uses_smithing_difficulty(o_ptr))
    {
        player_mark_object_experienced(o_ptr);
    }
    else
    {
        /* identify the object */
        ident(o_ptr);

        /* Full object description */
        object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

        msg_format("You recognize it as %s.", o_full_name);
    }

    return;
}

void ident_bow_arrow_by_use(object_type* j_ptr, object_type* i_ptr,
    object_type* o_ptr, const monster_type* m_ptr, u32b bow_flag,
    u32b arrow_flag)
{
    char i_short_name[80];
    char i_full_name[80];
    char j_short_name[80];
    char j_full_name[80];
    char slay_description[160];

    /* Short, pre-identification bow and arrow description */
    object_desc(j_short_name, sizeof(j_short_name), j_ptr, false, 0);
    object_desc(i_short_name, sizeof(i_short_name), i_ptr, false, 0);

    if (arrow_flag)
    {
        slay_desc(slay_description, arrow_flag, m_ptr);

        msg_format("Your %s %s.", i_short_name, slay_description);
        if (object_uses_smithing_difficulty(i_ptr))
        {
            player_mark_object_experienced(i_ptr);
            player_mark_object_experienced(o_ptr);
        }
        else
        {
            /* Identify the arrow and remaining arrows */
            object_aware(i_ptr);
            object_known(i_ptr);
            object_aware(o_ptr);
            object_known(o_ptr);

            /* Recalculate bonuses */
            p_ptr->update |= (PU_BONUS);

            /* Combine / Reorder the pack (later) */
            p_ptr->notice |= (PN_COMBINE | PN_REORDER);

            /* Window stuff */
            p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

            /* Full arrow description */
            object_desc(i_full_name, sizeof(i_full_name), i_ptr, true, 3);

            msg_format("You recognize it as %s.", i_full_name);
        }

        // don't carry on to identify the bow on the same shot
        return;
    }

    if (bow_flag)
    {
        slay_desc(slay_description, bow_flag, m_ptr);

        msg_format("Your shot %s.", slay_description);
        if (object_uses_smithing_difficulty(j_ptr))
        {
            player_mark_object_experienced(j_ptr);
        }
        else
        {
            /* Identify the bow */
            object_aware(j_ptr);
            object_known(j_ptr);

            /* Recalculate bonuses */
            p_ptr->update |= (PU_BONUS);

            /* Combine / Reorder the pack (later) */
            p_ptr->notice |= (PN_COMBINE | PN_REORDER);

            /* Window stuff */
            p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

            /* Full bow description */
            object_desc(j_full_name, sizeof(j_full_name), j_ptr, true, 3);

            msg_format("You recognize your %s to be %s.", j_short_name,
                j_full_name);
        }
    }

    return;
}

void apply_weapon_combat_effects(object_type* o_ptr, monster_type* m_ptr,
    int skill_type, int net_dam, bool fatal_blow, cptr armor_shatter_noun)
{
    monster_race* r_ptr;
    u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0;

    if (!o_ptr || !o_ptr->k_idx || !m_ptr)
        return;

    r_ptr = &r_info[m_ptr->r_idx];
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    if (!fatal_blow && (net_dam > 0))
    {
        if ((f4 & TR4_ARMOR_SHATTER) && (r_ptr->flags3 & RF3_HAS_ARMOUR))
        {
            int shatter_skill = p_ptr->skill_use[skill_type];
            int resist_skill = monster_skill(m_ptr, S_WIL);

            if (skill_check(NULL, shatter_skill, resist_skill, m_ptr) > 0)
            {
                if (m_ptr->armor_ps_reduction < r_ptr->ps)
                {
                    m_ptr->armor_ps_reduction++;

                    if (m_ptr->ml)
                    {
                        char m_poss[80];
                        monster_desc(m_poss, sizeof(m_poss), m_ptr, 0x22);
                        msg_format("Your %s shatters %s armor!",
                            armor_shatter_noun ? armor_shatter_noun : "attack",
                            m_poss);
                    }

                    if (!object_known_p(o_ptr))
                    {
                        ident_weapon_by_use(o_ptr, m_ptr, TR4_ARMOR_SHATTER);
                    }
                }
            }
        }

        if ((f3 & TR3_WILL_DRAIN) && !(r_ptr->flags2 & RF2_MINDLESS))
        {
            int drain_skill = p_ptr->skill_use[skill_type];
            int resist_skill = monster_skill(m_ptr, S_WIL);

            if (skill_check(NULL, drain_skill, resist_skill, m_ptr) > 0)
            {
                m_ptr->song_will_penalty++;

                if (m_ptr->ml)
                {
                    char m_poss[80];
                    monster_desc(m_poss, sizeof(m_poss), m_ptr, 0x22);
                    msg_format("You drain %s will!", m_poss);
                }

                if (!object_known_p(o_ptr))
                {
                    ident_weapon_by_use(o_ptr, m_ptr, TR3_WILL_DRAIN);
                }
            }
        }
    }

    if (fatal_blow && (f1 & TR1_VAMPIRIC) && !monster_nonliving(r_ptr))
    {
        if (hp_player(7, false, false) && !object_known_p(o_ptr))
        {
            ident_weapon_by_use(o_ptr, m_ptr, TR1_VAMPIRIC);
        }
    }
}

/*
 * Makes checks against perception to see if the weapon becomes identified
 *
 * Returns the flag that was noticed, the calling function can send this to
 * ident_weapon_by_use
 */

static u32b maybe_notice_slay(const object_type* o_ptr, u32b flag)
{
    u32b noticed_flag = 0L;

    if (!object_known_p(o_ptr))
    {
        noticed_flag = flag;
    }

    return noticed_flag;
}

/*
 * Determines the number of bonus dice from slays/brands
 *
 * Note that "flasks of oil" do NOT do fire damage, although they
 * certainly could be made to do so.  XXX XXX
 *
 * All 'slays' and 'brands' do one additional die (these are cumulative)
 * 'kills' do an additional two dice.
 */
int slay_bonus(
    const object_type* o_ptr, const monster_type* m_ptr, u32b* noticed_flag)
{
    int slay_bonus_dice = 0;
    int brand_bonus_dice = 0;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    u32b f1, f2, f3, f4;

    /* Extract the flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    /* Some "weapons" and "arrows" do extra damage */
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    case TV_BOW:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_DIGGING:
    {
        /* Slay Wolf */
        if ((f1 & (TR1_SLAY_WOLF)) && (r_ptr->flags3 & (RF3_WOLF)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_WOLF);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_WOLF);
        }

        /* Slay Spider */
        if ((f1 & (TR1_SLAY_SPIDER)) && (r_ptr->flags3 & (RF3_SPIDER)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_SPIDER);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_SPIDER);
        }

        /* Slay Undead */
        if ((f1 & (TR1_SLAY_UNDEAD)) && (r_ptr->flags3 & (RF3_UNDEAD)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_UNDEAD);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_UNDEAD);
        }

        /* Slay Rauko */
        if ((f1 & (TR1_SLAY_RAUKO)) && (r_ptr->flags3 & (RF3_RAUKO)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_RAUKO);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_RAUKO);
        }

        /* Slay Orc */
        if ((f1 & (TR1_SLAY_ORC)) && (r_ptr->flags3 & (RF3_ORC)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_ORC);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_ORC);
        }

        /* Slay Troll */
        if ((f1 & (TR1_SLAY_TROLL)) && (r_ptr->flags3 & (RF3_TROLL)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_TROLL);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_TROLL);
        }

        /* Slay Dragon */
        if ((f1 & (TR1_SLAY_DRAGON)) && (r_ptr->flags3 & (RF3_DRAGON)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_DRAGON);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_DRAGON);
        }

        /* Slay Serpent */
        if ((f4 & (TR4_SLAY_SERPENT)) && (r_ptr->flags3 & (RF3_SERPENT)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_SERPENT);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR4_SLAY_SERPENT);
        }

        /* Slay Vampire */
        if ((f4 & (TR4_SLAY_VAMPIRE)) && (r_ptr->flags3 & (RF3_VAMPIRE)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_VAMPIRE);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR4_SLAY_VAMPIRE);
        }

        /* Slay Horror */
        if ((f4 & (TR4_SLAY_HORROR)) && (r_ptr->flags3 & (RF3_HORROR)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_HORROR);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR4_SLAY_HORROR);
        }

        /* Slay Cat */
        if ((f4 & (TR4_SLAY_CAT)) && (r_ptr->flags3 & (RF3_CAT)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_CAT);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR4_SLAY_CAT);
        }

        /* Slay Giant */
        if ((f4 & (TR4_SLAY_GIANT)) && (r_ptr->flags3 & (RF3_GIANT)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_GIANT);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR4_SLAY_GIANT);
        }

        /* Slay Men and Elves */
        if ((f1 & (TR1_SLAY_MAN_OR_ELF))
            && ((r_ptr->flags3 & (RF3_MAN)) || (r_ptr->flags3 & (RF3_ELF))))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_MAN);
                l_ptr->flags3 |= (RF3_ELF);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_MAN_OR_ELF);
        }

        /* Brand (Elec) */
        if (f1 & (TR1_BRAND_ELEC))
        {
            /* Notice immunity */
            if (r_ptr->flags3 & (RF3_RES_ELEC))
            {
                if (m_ptr->ml)
                {
                    l_ptr->flags3 |= (RF3_RES_ELEC);
                }
            }

            /* Otherwise, take the damage */
            else
            {
                brand_bonus_dice += 1;

                *noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_ELEC);
            }
        }

        /* Brand (Fire) */
        if (f1 & (TR1_BRAND_FIRE))
        {
            /* Notice immunity */
            if (r_ptr->flags3 & (RF3_RES_FIRE))
            {
                if (m_ptr->ml)
                {
                    l_ptr->flags3 |= (RF3_RES_FIRE);
                }
            }

            /* Otherwise, take the damage */
            else
            {
                brand_bonus_dice += 1;

                *noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_FIRE);

                // extra bonus against vulnerable creatures
                if (r_ptr->flags3 & (RF3_HURT_FIRE))
                {
                    brand_bonus_dice += 1;

                    /* Memorize the effects */
                    l_ptr->flags3 |= (RF3_HURT_FIRE);
                }
            }
        }

        /* Brand (Cold) */
        if (f1 & (TR1_BRAND_COLD))
        {
            /* Notice immunity */
            if (r_ptr->flags3 & (RF3_RES_COLD))
            {
                if (m_ptr->ml)
                {
                    l_ptr->flags3 |= (RF3_RES_COLD);
                }
            }

            /* Otherwise, take the damage */
            else
            {
                brand_bonus_dice += 1;

                *noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_COLD);

                // extra bonus against vulnerable creatures
                if (r_ptr->flags3 & (RF3_HURT_COLD))
                {
                    brand_bonus_dice += 1;

                    /* Memorize the effects */
                    l_ptr->flags3 |= (RF3_HURT_COLD);
                }
            }
        }

        /* Brand (Poison) */
        if (f1 & (TR1_BRAND_POIS))
        {
            /* Notice immunity */
            if (r_ptr->flags3 & (RF3_RES_POIS))
            {
                if (m_ptr->ml)
                {
                    l_ptr->flags3 |= (RF3_RES_POIS);
                }
            }

            /* Otherwise, take the damage */
            else
            {
                brand_bonus_dice += 1;

                *noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_POIS);
            }
        }

        break;
    }
    }

    if ((slay_bonus_dice > 0) || (brand_bonus_dice > 1))
    {
        // cause a temporary morale penalty
        scare_onlooking_friends(m_ptr, -20);
    }

    return (slay_bonus_dice + brand_bonus_dice);
}

/*
 * Determines the protection percentage
 */
extern int prt_after_sharpness(const object_type* o_ptr, u32b* noticed_flag)
{
    int protection = 100;

    u32b f1, f2, f3;

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Sharpness */
    if (f1 & (TR1_SHARPNESS))
    {
        *noticed_flag = maybe_notice_slay(o_ptr, TR1_SHARPNESS);
        protection = 50;
    }

    /* Sharpness 2 */
    if (f1 & (TR1_SHARPNESS2))
    {
        *noticed_flag = maybe_notice_slay(o_ptr, TR1_SHARPNESS2);
        protection = 0;
    }

    if (protection < 0)
        protection = 0;

    return protection;
}

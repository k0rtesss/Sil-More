/* File: cmd-item-drop.c */

/*
 * Copyright (c) 2001 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "log/log.h"
#include "metarun.h"
#include "object/object-ui-enhanced.h"
#include "object/object-ui-select.h"

void do_cmd_drop(void)
{
    int item, amt;

    object_type* o_ptr;

    cptr q, s;

    /* Get an item */
    q = "Drop which item? ";
    s = "You have nothing to drop.";
    if (!get_item(&item, q, s, (USE_EQUIP | USE_INVEN)))
        return;

    if (item == SUPPLIES_INDEX)
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_DROP, -1, false, true);
        return;
    }

    /* Get the item (in the pack) */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Get the item (on the floor) */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    /* Get a quantity */
    amt = get_quantity(NULL, o_ptr->number);

    /* Allow user abort */
    if (amt <= 0)
        return;

    if (((item == INVEN_QUIVER1) || (item == INVEN_QUIVER2)) && cursed_p(o_ptr))
    {
        msg_print("You cannot bear to part with it.");
        return;
    }

    /* Hack -- Cannot remove cursed items */
    if ((item >= INVEN_WIELD) && cursed_p(o_ptr))
    {
        if (p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
        {
            /* Message */
            msg_print("With a great strength of will, you break the curse!");

            /* Uncurse the object */
            uncurse_object(o_ptr);
        }
        else
        {
            /* Oops */
            msg_print("You cannot bear to part with it.");

            /* Nope */
            return;
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Drop (some of) the item */
    inven_drop(item, amt);

    /* Update quiver display if needed */
    p_ptr->redraw |= (PR_QUIVER);
}

/*
 * An "item_tester_hook" for destroying objects
 */
static bool item_tester_hook_destroy(const object_type* o_ptr)
{
    if (o_ptr) { } // suppresses warnings about this function

    return (true);
}

/*
 *  Shatter the player's wielded weapon.
 */
void shatter_weapon(int silnum)
{
    int i;
    object_type* w_ptr = &inventory[INVEN_WIELD];
    char w_name[80];
    int anger_level;

    log_debug("shatter_weapon: called for silmaril #%d", silnum);
    
    /* Set the appropriate shatter flag for this silmaril */
    if (silnum == 2)
    {
        p_ptr->crown_shatter_sil2 = true;
        log_debug("shatter_weapon: set crown_shatter_sil2 = true");
    }
    else if (silnum == 3)
    {
        p_ptr->crown_shatter_sil3 = true;
        log_debug("shatter_weapon: set crown_shatter_sil3 = true");
    }

    /* Get the basic name of the object */
    object_desc(w_name, sizeof(w_name), w_ptr, false, 0);

    if (silnum == 2)
        msg_print(
            "You strive to free a second Silmaril, but it is not fated to be.");
    else
        msg_print(
            "You strive to free a third Silmaril, but it is not fated to be.");

    msg_format(
        "As you strike the crown, your %s shatters into innumerable pieces.",
        w_name);

    // make more noise
    stealth_score -= 5;

    inven_item_increase(INVEN_WIELD, -1);
    inven_item_optimize(INVEN_WIELD);

    /* Determine anger level based on which Silmaril (2nd = state 3, 3rd = state 4) */
    anger_level = (silnum == 2) ? 3 : 4;

    log_debug("shatter_weapon: anger_level=%d for silmaril #%d", anger_level, silnum);

    /* Process monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* If Morgoth, then anger him */
        if (m_ptr->r_idx == R_IDX_MORGOTH)
        {
            log_debug("shatter_weapon: found Morgoth at (%d,%d), cdis=%d, alertness=%d",
                     m_ptr->fy, m_ptr->fx, m_ptr->cdis, m_ptr->alertness);
            
            if ((m_ptr->cdis <= 5)
                && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
            {
                log_debug("shatter_weapon: Morgoth sees shard strike, calling anger_morgoth(%d)", 
                         anger_level);
                msg_print("A shard strikes Morgoth upon his cheek.");
                set_alertness(m_ptr, ALERTNESS_VERY_ALERT);
                anger_morgoth(anger_level);
            }
            else
            {
                log_debug("shatter_weapon: Morgoth doesn't see/is too far");
            }
        }
    }
}

void prise_silmaril(void)
{
    object_type* o_ptr;
    object_type* w_ptr;
    artefact_type* a_ptr;

    object_type object_type_body;

    cptr freed_msg = NULL; // default to soothe compiler warnings

    bool freed = false;

    int slot = 0;

    int dam = 0;
    int prt = 0;
    int net_dam = 0;
    int prt_percent = 0;
    int hit_result = 0;
    int crit_bonus_dice = 0;
    int pd = 10;
    int noise = 0;
    u32b dummy_noticed_flag;

    int mds = p_ptr->mds;
    int attack_mod = p_ptr->skill_use[S_MEL];

    char o_name[80];

    // the Crown is on the ground
    o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];

    log_debug("prise_silmaril: attempting to prise silmaril from crown artifact %d", 
             o_ptr->name1);
    log_debug("prise_silmaril: current morgoth_state=%d, silmarils_possessed=%d",
             p_ptr->morgoth_state, silmarils_possessed());

    switch (o_ptr->name1)
    {
    case ART_MORGOTH_3:
    {
        noise = 5;
        freed_msg = "You have freed a Silmaril!";
        break;
    }
    case ART_MORGOTH_2:
    {
        noise = 10;

        if (p_ptr->crown_shatter)
            freed_msg = "The fates be damned! You free a second Silmaril.";
        else
            freed_msg = "You free a second Silmaril.";

        msg_print(
            "As you reach for the second jewel, you feel the weight of "
            "Morgoth's wrath pressing upon you.");
        msg_print(
            "To take another Silmaril will kindle a fury beyond measure.");
        if (!get_check("Will you dare to claim it? "))
            return;

        break;
    }
    case ART_MORGOTH_1:
    {
        noise = 15;

        freed_msg
            = "You free the final Silmaril. You have a very bad feeling about "
              "this.";

        msg_print(
            "Looking into the hallowed light of the final Silmaril, you are "
            "filled with a strange dread.");
        if (!get_check("Are you sure you wish to proceed? "))
            return;

        break;
    }
    }

    /* Get the weapon */
    w_ptr = &inventory[INVEN_WIELD];

    // undo rapid attack penalties
    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        // undo strength adjustment to the attack
        mds = total_mds(w_ptr, 0);

        // undo the dexterity adjustment to the attack
        attack_mod += 3;
    }

    /* Test for hit */
    hit_result = hit_roll(attack_mod, 0, PLAYER, NULL, true);

    /* Make some noise */
    stealth_score -= noise;

    // Determine damage
    if (hit_result > 0)
    {
        crit_bonus_dice = crit_bonus(hit_result, w_ptr->weight,
            &r_info[R_IDX_MORGOTH], S_MEL, false, NULL, w_ptr);

        dam = damroll(p_ptr->mdd + crit_bonus_dice, mds);
        prt = damroll(pd, 4);

        prt_percent = prt_after_sharpness(w_ptr, &dummy_noticed_flag);

        if (prt_percent < 0)
        {
            prt_percent = 0;
        }

        prt = (prt * prt_percent) / 100;
        net_dam = dam - prt;

        /* No negative damage */
        if (net_dam < 0)
            net_dam = 0;

        // update_combat_rolls1b(PLAYER, true);
        update_combat_rolls2(p_ptr->mdd + crit_bonus_dice, mds, dam, pd, 4, prt,
            prt_percent, GF_HURT, true);
    }

    // if you succeed in prising out a Silmaril...
    if (net_dam > 0)
    {
        freed = true;

        switch (o_ptr->name1)
        {
        case ART_MORGOTH_3:
        {
            /* Process monsters - anger Morgoth when 1st Silmaril is taken */
            for (int i = 1; i < mon_max; i++)
            {
                monster_type* m_ptr = &mon_list[i];

                /* If Morgoth, then anger him to state 2 for 1st Silmaril */
                if (m_ptr->r_idx == R_IDX_MORGOTH
                    && m_ptr->alertness >= ALERTNESS_ALERT)
                {
                    log_debug("prise_silmaril: found Morgoth at (%d,%d), cdis=%d",
                             m_ptr->fy, m_ptr->fx, m_ptr->cdis);
                    
                    if ((m_ptr->cdis <= 5)
                        && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
                    {
                        log_debug("prise_silmaril: Morgoth sees 1st silmaril taken, calling anger_morgoth(2)");
                        msg_print("Morgoth roars in fury!");
                        anger_morgoth(2);
                    }
                    else
                    {
                        log_debug("prise_silmaril: Morgoth alert but doesn't see/too far");
                    }
                }
            }
            break;
        }
        case ART_MORGOTH_2:
        {
            /* 50% chance to shatter if not already shattered on 2nd silmaril */
            if (!p_ptr->crown_shatter_sil2 && one_in_(2))
            {
                log_debug("prise_silmaril: 2nd silmaril shatter check failed (50%%), calling shatter_weapon(2)");
                shatter_weapon(2);
                freed = false;
            }
            else
            {
                log_debug("prise_silmaril: 2nd silmaril - no shatter (already_shattered=%d)", 
                         p_ptr->crown_shatter_sil2);
                
                /* Process monsters */
                for (int i = 1; i < mon_max; i++)
                {
                    monster_type* m_ptr = &mon_list[i];

                    /* If Morgoth, then anger him to state 3 for 2nd Silmaril */
                    if (m_ptr->r_idx == R_IDX_MORGOTH
                        && m_ptr->alertness >= ALERTNESS_ALERT)
                    {
                        log_debug("prise_silmaril: found Morgoth at (%d,%d), cdis=%d",
                                 m_ptr->fy, m_ptr->fx, m_ptr->cdis);
                        
                        if ((m_ptr->cdis <= 5)
                            && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
                        {
                            log_debug("prise_silmaril: Morgoth sees 2nd silmaril taken, calling anger_morgoth(3)");
                            msg_print("Morgoth howls with rage!");
                            anger_morgoth(3);
                        }
                        else
                        {
                            log_debug("prise_silmaril: Morgoth alert but doesn't see/too far");
                        }
                    }
                }
            }
            break;
        }
        case ART_MORGOTH_1:
        {
            /* 100% shatter on 3rd silmaril if not already shattered on 3rd */
            if (!p_ptr->crown_shatter_sil3)
            {
                log_debug("prise_silmaril: 3rd silmaril shatter check (100%%), calling shatter_weapon(3)");
                shatter_weapon(3);
                freed = false;
            }
            else
            {
                log_debug("prise_silmaril: 3rd silmaril - no shatter (already_shattered=%d), but cursed!",
                         p_ptr->crown_shatter_sil3);
                p_ptr->cursed = true;
            }
            break;
        }
        }

        if (freed)
        {
            // change its type to that of the crown with one less silmaril
            o_ptr->name1--;

            // get the details of this new crown
            a_ptr = &a_info[o_ptr->name1];

            // modify the existing crown
            object_into_artefact(o_ptr, a_ptr);

            // report success
            msg_print(freed_msg);

            // Get new local object
            o_ptr = &object_type_body;

            // Make Silmaril
            object_prep(o_ptr, lookup_kind(TV_LIGHT, SV_LIGHT_SILMARIL));

            // Get it
            slot = inven_carry(o_ptr, false);

            if (slot == SUPPLIES_INDEX)
            {
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                char label = supplies_label_char();
                if (!label)
                    label = 'a';
                msg_format("You add %s to your supplies (%c).", o_name, label);
            }
            else if (slot >= 0)
            {
                /* Get the object again */
                o_ptr = &inventory[slot];

                /* Describe the object */
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                /* Message */
                msg_format("You have %s (%c).", o_name, index_to_label(slot));
            }
            else
            {
                /* Inventory full - find best adjacent square for Silmaril */
                int dy, dx;
                int best_y = p_ptr->py;
                int best_x = p_ptr->px;
                int backup_y = -1;
                int backup_x = -1;
                bool found_ideal = false;
                bool found_backup = false;
                
                /* First pass: try to find square with no items AND no monsters */
                for (dy = -1; dy <= 1; dy++)
                {
                    for (dx = -1; dx <= 1; dx++)
                    {
                        int ty = p_ptr->py + dy;
                        int tx = p_ptr->px + dx;
                        
                        /* Skip center */
                        if (dy == 0 && dx == 0) continue;
                        
                        /* Check if square is valid, empty floor, no objects, no monsters */
                        if (in_bounds_fully(ty, tx) && 
                            cave_clean_bold(ty, tx) && 
                            cave_m_idx[ty][tx] == 0)
                        {
                            best_y = ty;
                            best_x = tx;
                            found_ideal = true;
                            break;
                        }
                        /* Backup: empty floor with no objects (but monster might be there) */
                        else if (!found_backup && in_bounds_fully(ty, tx) && cave_clean_bold(ty, tx))
                        {
                            backup_y = ty;
                            backup_x = tx;
                            found_backup = true;
                        }
                    }
                    if (found_ideal) break;
                }
                
                /* Use backup square if no ideal square found */
                if (!found_ideal && found_backup)
                {
                    best_y = backup_y;
                    best_x = backup_x;
                    log_debug("prise_silmaril: no monster-free square, using backup at (%d,%d)", best_y, best_x);
                }
                
                /* Drop the Silmaril */
                if (found_ideal)
                {
                    log_debug("prise_silmaril: inventory full, dropping Silmaril at (%d,%d) (no items, no monsters)", 
                             best_y, best_x);
                }
                else if (found_backup)
                {
                    log_debug("prise_silmaril: inventory full, dropping Silmaril at (%d,%d) (WARNING: monster may be present)", 
                             best_y, best_x);
                }
                else
                {
                    log_debug("prise_silmaril: inventory full, no adjacent empty square, using drop_near fallback");
                }
                
                drop_near(o_ptr, 0, best_y, best_x);
                
                /* Describe what we dropped */
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                msg_format("You have no room, so %s drops to the floor.", o_name);
            }

            // Break the truce (always)
            break_truce(true);

            // add a note to the notes file
            do_cmd_note("Cut a Silmaril from Morgoth's crown", p_ptr->depth);
        }
    }

    // if you fail to prise out a Silmaril...
    else
    {
        msg_print("Try though you might, you were unable to free a Silmaril.");

        // Break the truce if creatures see
        break_truce(false);
    }

    // check for taking of final Silmaril
    if (o_ptr->name1 == ART_MORGOTH_0)
    {
        log_debug("prise_silmaril: final silmaril taken! Calling anger_morgoth(4)");
        msg_print("You hear a cry of vengeance echo through the iron hells.");
        msg_print("You feel your doom awaiting you.");
        wake_all_monsters(0);
        anger_morgoth(4);  // Final Silmaril pushes Morgoth to desperate state
    }
    
    log_debug("prise_silmaril: complete, freed=%s, final morgoth_state=%d", 
             freed ? "true" : "false", p_ptr->morgoth_state);
}

/*
 * Destroy an item
 */
void do_cmd_destroy(void)
{
    int item, amt;
    int old_number;
    int old_charges = 0;

    object_type* o_ptr;

    char o_name[80];

    char out_val[160];

    cptr q, s;

    item_tester_hook = item_tester_hook_destroy;

    // Special case for prising Silmarils from the Iron Crown of Morgoth
    o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
    if ((o_ptr->name1 >= ART_MORGOTH_1) && (o_ptr->name1 <= ART_MORGOTH_3))
    {
        // Select the melee weapon
        o_ptr = &inventory[INVEN_WIELD];

        // No weapon
        if (!o_ptr->k_idx)
        {
            msg_print(
                "To prise a Silmaril from the crown, you would need to wield a "
                "weapon.");
        }

        // Wielding a weapon
        else
        {
            if (get_check(
                    "Will you try to prise a Silmaril from the Iron Crown? "))
            {
                prise_silmaril();

                /* Take a turn */
                p_ptr->energy_use = 100;

                // store the action type
                p_ptr->previous_action[0] = ACTION_MISC;

                return;
            }
        }
    }

    /* Get an item */
    q = "Destroy which item? ";
    s = "You have nothing to destroy.";
    if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR)))
        return;

    /* Get the item (in the pack) */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Get the item (on the floor) */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    // Special case for Iron Crown of Morgoth, if it has Silmarils left
    if ((o_ptr->name1 >= ART_MORGOTH_1) && (o_ptr->name1 <= ART_MORGOTH_3))
    {
        if (item >= 0)
        {
            msg_print("You would have to put it down first.");
        }
        else
        {
            /* No weapon */
            if (!o_ptr->k_idx)
            {
                msg_print("To prise a Silmaril from the crown, you would need "
                          "to wield a "
                          "weapon.");
            }
            else
            {
                msg_print(
                    "You decide to try to prise out a Silmaril after all.");

                prise_silmaril();

                /* Take a turn */
                p_ptr->energy_use = 100;

                // store the action type
                p_ptr->previous_action[0] = ACTION_MISC;

                return;
            }
        }
    }

    /* Get a quantity */
    amt = get_quantity(NULL, o_ptr->number);

    /* Allow user abort */
    if (amt <= 0)
        return;

    /* Describe the object */
    old_number = o_ptr->number;

    /* Hack, state the correct number of charges to be destroyed if staff*/
    if ((o_ptr->tval == TV_STAFF) && (amt < o_ptr->number))
    {
        /*save the number of charges*/
        old_charges = o_ptr->pval;

        /*distribute the charges*/
        o_ptr->pval -= o_ptr->pval * amt / o_ptr->number;

        o_ptr->pval = old_charges - o_ptr->pval;
    }

    /*hack -  make sure we get the right amount displayed*/
    o_ptr->number = amt;

    /*now describe with correct amount*/
    if (item < 0)
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
    else
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /*reverse the hack*/
    o_ptr->number = old_number;

    /* Check for known special items */
    strnfmt(out_val, sizeof(out_val), "Really destroy %s? ", o_name);

    if (!get_check(out_val))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Message */
    msg_format("You destroy %s.", o_name);

    /*hack, restore the proper number of charges after the messages have printed
     * so the proper number of charges are destroyed*/
    if (old_charges)
        o_ptr->pval = old_charges;

    /* Eliminate the item (from the pack) */
    if (item >= 0)
    {
        inven_item_increase(item, -amt);
        inven_item_describe(item);
        inven_item_optimize(item);
    }

    /* Eliminate the item (from the floor) */
    else
    {
        floor_item_increase(0 - item, -amt);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }
}

/*
 * Observe an item, displaying what is known about it
 */

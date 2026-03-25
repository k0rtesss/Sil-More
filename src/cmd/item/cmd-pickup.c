/* File: cmd-pickup.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "main-sdl.h"
#include "object/object-ui-select.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include <math.h>

void give_player_item(object_type * o_ptr)
{
    char o_name[80];
    object_type copy = *o_ptr;

    int slot = inven_carry(o_ptr, true);

    if (slot == SUPPLIES_INDEX)
    {
        object_desc(o_name, sizeof(o_name), &copy, true, 3);
        char label = supplies_label_char();
        if (!label)
            label = 'a';
        msg_format("You add %s to your supplies (%c).", o_name, label);
        sound(MSG_PICK);
        return;
    }

    if (slot < 0)
        return;
    
    /* Play pickup sound */
    sound(MSG_PICK);

    /* reset the pointer to the new location to pick up the count of the item
       in the inventory */
    o_ptr = &inventory[slot];

    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    msg_format("You have %s (%c).", o_name, index_to_label(slot));

    /* Update quiver display if this was a throwing weapon or arrow */
    if ((slot == INVEN_QUIVER1 || slot == INVEN_QUIVER2) ||
        (copy.tval == TV_ARROW))
    {
        p_ptr->redraw |= (PR_QUIVER);
    }
}

bool is_weapon_or_armor(const object_type* o_ptr)
{
    /* Check if it's a weapon */
    if (o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM || 
        o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_BOW)
        return true;
        
    /* Check if it's armor */
    if (o_ptr->tval == TV_SOFT_ARMOR || o_ptr->tval == TV_MAIL || 
        o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_HELM || 
        o_ptr->tval == TV_CROWN || o_ptr->tval == TV_CLOAK || 
        o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
        return true;
        
    return false;
}

bool smith_oath_forbids_object(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return chosen_oath(OATH_SMITH) && !oath_invalid(OATH_SMITH)
        && is_weapon_or_armor(o_ptr) && !is_smithed_by_player(o_ptr);
}

bool smith_oath_confirm_break(void)
{
    char* prompt;

    if (!chosen_oath(OATH_SMITH) || oath_invalid(OATH_SMITH))
        return true;

    prompt = oath_confirmation_prompt(OATH_SMITH);
    if (!prompt || !prompt[0])
        prompt = "Are you certain you wish to break your Oath of the Smith?";

    if (!get_check_oath_multiline(prompt))
        return false;

    p_ptr->oaths_broken |= OATH_SMITH_FLAG;
    apply_oath_breaking_curse(OATH_SMITH);
    return true;
}

/*
 * Check if an object was smithed by the player
 */
static const object_type* replacement_filter_incoming = NULL;

static bool pack_item_matches_replacement_type(const object_type* incoming,
                                               const object_type* candidate)
{
    if (!incoming || !candidate || !candidate->k_idx)
        return false;

    if (incoming->tval == candidate->tval)
        return true;

    int incoming_slot = wield_slot(incoming);
    if (incoming_slot >= INVEN_WIELD && incoming_slot < INVEN_TOTAL)
    {
        int candidate_slot = wield_slot(candidate);
        if (candidate_slot == incoming_slot)
            return true;
    }

    return false;
}

static void format_staff_prompt_name(char* buf, size_t max,
                                     const object_type* o_ptr, bool pref)
{
    char full[80];
    const char* staff_of;

    if (!buf || max == 0)
        return;

    buf[0] = '\0';

    if (!o_ptr || !o_ptr->k_idx)
        return;

    object_desc(full, sizeof(full), o_ptr, pref, 0);

    if (o_ptr->tval != TV_STAFF)
    {
        SDL_strlcpy(buf, full, max);
        return;
    }

    staff_of = strstr(full, "Staff of ");
    if (!staff_of)
    {
        SDL_strlcpy(buf, full, max);
        return;
    }

    if (!pref)
    {
        SDL_strlcpy(buf, staff_of, max);
        return;
    }

    if (!strncmp(full, "The ", 4))
        strnfmt(buf, max, "The %s", staff_of);
    else if (!strncmp(full, "no more ", 8))
        strnfmt(buf, max, "no more %s", staff_of);
    else
        strnfmt(buf, max, "a %s", staff_of);
}

bool is_smithed_by_player(const object_type* o_ptr)
{
    return (o_ptr->unused1 != 0);
}

/*
 * Prompt the player to drop an inventory item so a new object can be picked up.
 * Returns true if an item was dropped, false if the player declined or nothing was dropped.
 */
static bool prompt_replace_pack_item(const object_type* incoming)
{
    char incoming_name[80];
    char prompt[160];

    /* Ensure story font is disabled before showing messages */
    if (sdl_is_story_font_enabled())
        sdl_story_font_disable();

    object_desc(incoming_name, sizeof(incoming_name), incoming, true, 3);
    msg_format("No room for %s.", incoming_name);
    msg_print("Choose an item to replace.");

    strnfmt(prompt, sizeof(prompt), "Replace which item to pick up %s? ", incoming_name);

    while (true)
    {
        int item;

        if (!get_item(&item, prompt,
                "You have nothing to replace.", (USE_INVEN)))
        {
            return false;
        }

        if ((item < 0) || (item >= INVEN_PACK))
        {
            bell("Illegal object choice!");
            continue;
        }

        object_type* drop_ptr = &inventory[item];

        if (!drop_ptr->k_idx)
        {
            bell("That slot is empty.");
            continue;
        }

        inven_drop(item, drop_ptr->number);

        /* Let inventory housekeeping run before we attempt the pickup again */
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        notice_stuff();

        return true;
    }
}

/*
 * Helper routine for py_pickup() and py_pickup_floor().
 *
 * Add the given dungeon object to the character's inventory.
 *
 * Delete the object afterwards.
 */

void py_pickup_aux(int o_idx)
{
    object_type* o_ptr;
    char o_name[120];
    
    o_ptr = &o_list[o_idx];
    // Remember the floor position even if give_player_item wipes the object
    int pickup_y = o_ptr->iy;
    int pickup_x = o_ptr->ix;

    /*hack - don't pickup &nothings*/
    if (o_ptr->k_idx)
    {
        /* Check for Oath of the Smith violation */
        if (smith_oath_forbids_object(o_ptr))
        {
            if (!smith_oath_confirm_break())
                return;
        }

        /* Check for supply items with partial pickup option */
        if (supplies_is_supply_object(o_ptr) && o_ptr->number > 1)
        {
            int max_qty = supplies_max_absorbable_quantity(o_ptr);
            
            /* If we can't absorb all of it but can absorb some, offer partial pickup */
            if (max_qty > 0 && max_qty < o_ptr->number)
            {
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                
                char prompt[160];
                strnfmt(prompt, sizeof(prompt), 
                        "Your supply cache can only hold %d of %d. Pick up how many? (0-%d): ",
                        max_qty, o_ptr->number, max_qty);
                
                int qty = get_quantity(prompt, max_qty);
                
                if (qty <= 0)
                {
                    msg_print("You leave it on the ground.");
                    return;
                }
                
                /* Create a partial object to pick up */
                object_type partial;
                object_copy(&partial, o_ptr);
                partial.number = qty;
                
                give_player_item(&partial);
                
                /* Reduce the floor object */
                o_ptr->number -= qty;
                
                /* Break the truce if creatures see */
                break_truce(false);
                
                return;
            }
        }
        
        give_player_item(o_ptr);

        // Break the truce if creatures see
        break_truce(false);

        if (!o_ptr->k_idx || o_ptr->number <= 0)
        {
            if (!o_ptr->k_idx)
            {
                o_ptr->iy = pickup_y;
                o_ptr->ix = pickup_x;
            }
            delete_object_idx(o_idx);
        }

        return;
    }

    /* Delete the object */
    o_ptr->iy = pickup_y;
    o_ptr->ix = pickup_x;
    delete_object_idx(o_idx);
}

/*
 * Allow the player to sort through items in a pile and
 * pickup what they want.  This command does not use
 * any energy because it costs a player no extra energy
 * to walk into a grid and automatically pick up items
 */
void do_cmd_pickup_from_pile(void)
{
    bool picked_up_item = false;

    /*
     * Loop through and pick up objects until escape is hit or the backpack
     * can't hold anything else.
     */
    while (true)
    {
        int item;

        char prompt[80];

        int floor_list[MAX_FLOOR_STACK];

        int floor_num;

        /*start with everything updated*/
        handle_stuff();

        /* Scan for floor objects */
        floor_num = scan_floor(
            floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x01);

        /* No pile */
        if (floor_num < 1)
        {
            if (picked_up_item)
                msg_format("There are no more objects where you are standing.");
            else
                msg_format("There are no objects where you are standing.");
            break;
        }

        /* Restrict the choices */
        item_tester_hook = inven_carry_okay;

        /* re-test to see if we can pick any of them up */
        floor_num = scan_floor(
            floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x01);

        /* Nothing can be picked up */
        if (floor_num < 1)
        {
            msg_format("Your backpack is full.");
            break;
        }

        /* Save screen */
        screen_save();

        /* Display */
        show_floor(floor_list, floor_num);

        SDL_strlcpy(
            prompt, "Pick up which object? (ESC to cancel):", sizeof(prompt));

        /*clear the restriction*/
        item_tester_hook = NULL;

        /* Get the object number to be bought */
        item = get_menu_choice(floor_num, prompt);

        /*player chose escape*/
        if (item == -1)
        {
            screen_load();
            break;
        }

        /* Pick up the object */
        py_pickup_aux(floor_list[item]);

        /*Mark that we picked something up*/
        picked_up_item = true;

        /* Load screen */
        screen_load();
    }

    /*clear the restriction*/
    item_tester_hook = NULL;

    /* Combine / Reorder the pack */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Update quiver display if needed */
    p_ptr->redraw |= (PR_QUIVER);

    /* Just be sure all inventory management is done. */
    notice_stuff();
}

static void report_pack_limit_failure(const char* o_name, bool still)
{
    if (inven_carry_limit_failed())
    {
        cptr label = inven_carry_limit_label();
        int limit = inven_carry_limit_value();

        if (label)
        {
            /* Special message for supply weight limit */
            if (strcmp(label, "supply weight") == 0)
            {
                msg_format("Your supply cache cannot carry any more weight (limit %d lbs).",
                           limit);
                return;
            }

            if (still)
                msg_format("Your pack still cannot hold more %s (limit %d).", label,
                           limit);
            else
                msg_format("Your pack cannot hold more %s (limit %d).", label, limit);
            return;
        }
    }

    if (still)
        msg_format("You still have no room for %s.", o_name);
    else
        msg_format("You have no room for %s.", o_name);
}

typedef enum
{
    PICKUP_FAILURE_ABORT = 0,
    PICKUP_FAILURE_RETRY,
    PICKUP_FAILURE_EQUIPPED
} pickup_failure_result;

static bool item_tester_limit_group(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (replacement_filter_incoming
        && !pack_item_matches_replacement_type(replacement_filter_incoming, o_ptr))
        return false;

    return inven_carry_limit_can_replace(o_ptr);
}

static bool pack_has_limit_candidates(const object_type* incoming)
{
    for (int i = 0; i < INVEN_PACK; i++)
    {
        object_type* j_ptr = &inventory[i];

        if (!j_ptr->k_idx)
            continue;

        if (!inven_carry_limit_can_replace(j_ptr))
            continue;

        if (!pack_item_matches_replacement_type(incoming, j_ptr))
            continue;

        return true;
    }

    return false;
}

static bool prompt_replace_pack_item_limit(const object_type* incoming,
                                           const char* incoming_name)
{
    char prompt[160];
    cptr label = inven_carry_limit_label();
    int limit = inven_carry_limit_value();
    bool replaced = false;

    bool old_item_tester_full = item_tester_full;
    byte old_item_tester_tval = item_tester_tval;
    bool (*old_item_tester_hook)(const object_type*) = item_tester_hook;
    const object_type* old_filter = replacement_filter_incoming;

    /* Ensure story font is disabled before showing messages */
    if (sdl_is_story_font_enabled())
        sdl_story_font_disable();

    if (label)
        msg_format("You already carry %s (limit %d).", label, limit);
    else
        msg_print("You cannot carry any more of those.");

    msg_print("Choose an item to replace.");

    strnfmt(prompt, sizeof(prompt),
            "Replace which item to pick up %s? ", incoming_name);

    replacement_filter_incoming = incoming;
    item_tester_tval = 0;
    item_tester_hook = item_tester_limit_group;
    item_tester_full = false;

    while (true)
    {
        int item;

        if (!get_item(&item, prompt, "You have nothing to replace.", USE_INVEN))
            break;

        if ((item < 0) || (item >= INVEN_PACK))
        {
            bell("Illegal object choice!");
            continue;
        }

        object_type* drop_ptr = &inventory[item];

        if (!drop_ptr->k_idx)
        {
            bell("That slot is empty.");
            continue;
        }

        if (!inven_carry_limit_can_replace(drop_ptr))
        {
            msg_print("That will not make enough room.");
            continue;
        }

        inven_drop(item, drop_ptr->number);

        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        notice_stuff();

        replaced = true;
        break;
    }

    replacement_filter_incoming = old_filter;
    item_tester_hook = old_item_tester_hook;
    item_tester_tval = old_item_tester_tval;
    item_tester_full = old_item_tester_full;

    return replaced;
}

static pickup_failure_result handle_zero_limit_pickup(object_type* incoming,
                                                      int floor_o_idx,
                                                      const char* incoming_name)
{
    int slot = wield_slot(incoming);

    msg_format("You cannot carry %s in your pack.", incoming_name);

    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
    {
        msg_print("It does not fit anywhere on your body.");
        return PICKUP_FAILURE_ABORT;
    }

    object_type* equip_ptr = &inventory[slot];

    if (!equip_ptr->k_idx)
    {
        if (get_check("Wear it now? "))
        {
            do_cmd_wield(incoming, 0 - floor_o_idx);
            return PICKUP_FAILURE_EQUIPPED;
        }

        msg_print("You leave it on the ground.");
        return PICKUP_FAILURE_ABORT;
    }

    if (cursed_p(equip_ptr))
    {
        char equipped_name[80];
        object_desc(equipped_name, sizeof(equipped_name), equip_ptr, true, 3);
        msg_format("You cannot remove %s.", equipped_name);
        return PICKUP_FAILURE_ABORT;
    }

    screen_save();
    show_equip();
    msg_print(NULL);
    screen_load();

    char equipped_name[80];
    object_desc(equipped_name, sizeof(equipped_name), equip_ptr, true, 3);

    char prompt[160];
    strnfmt(prompt, sizeof(prompt), "Replace %s with %s? ", equipped_name,
            incoming_name);

    if (get_check(prompt))
    {
        do_cmd_wield(incoming, 0 - floor_o_idx);
        return PICKUP_FAILURE_EQUIPPED;
    }

    msg_print("You decide to keep your current equipment.");
    return PICKUP_FAILURE_ABORT;
}

static pickup_failure_result handle_group_limit_pickup(object_type* incoming,
                                                       const char* incoming_name)
{
    if (!pack_has_limit_candidates(incoming))
        return PICKUP_FAILURE_ABORT;

    if (!prompt_replace_pack_item_limit(incoming, incoming_name))
        return PICKUP_FAILURE_ABORT;

    return PICKUP_FAILURE_RETRY;
}

static pickup_failure_result resolve_pickup_failure(object_type* incoming,
                                                    int floor_o_idx,
                                                    const char* incoming_name,
                                                    bool attempted_replacement)
{
    if (inven_carry_limit_failed())
    {
        if (inven_carry_limit_value() <= 0)
            return handle_zero_limit_pickup(incoming, floor_o_idx,
                                            incoming_name);

        pickup_failure_result limit_result =
            handle_group_limit_pickup(incoming, incoming_name);

        if (limit_result == PICKUP_FAILURE_ABORT)
            report_pack_limit_failure(incoming_name, attempted_replacement);

        return limit_result;
    }

    if (prompt_replace_pack_item(incoming))
        return PICKUP_FAILURE_RETRY;

    report_pack_limit_failure(incoming_name, attempted_replacement);
    return PICKUP_FAILURE_ABORT;
}

void py_pickup(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    bool done_pickup = false;

    s16b this_o_idx, next_o_idx = 0;

    object_type* o_ptr;

    char o_name[80];

    /* Automatically destroy squelched items in pile if necessary */
    do_squelch_pile(py, px);

    /* Scan the pile of objects */
    for (this_o_idx = cave_o_idx[py][px]; this_o_idx; this_o_idx = next_o_idx)
    {
        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Describe the object */
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Hack -- disturb */
        disturb(0, 0);

        /* End loop if squelched stuff reached */
        if ((k_info[o_ptr->k_idx].squelch == SQUELCH_ALWAYS)
            && (k_info[o_ptr->k_idx].aware))
        {
            next_o_idx = 0;
            continue;
        }

        bool attempted_replacement = false;
        bool skip_current_item = false;

        if (p_ptr->active_ability[S_WIL][WIL_CHANNELING] && o_ptr->tval == TV_STAFF && o_ptr->pval > 0)
        {
            int target_slot = -1;
            object_type* target = NULL;

            object_type* wielded = &inventory[INVEN_STAFF];
            if (wielded->k_idx && wielded->k_idx == o_ptr->k_idx)
            {
                target = wielded;
                target_slot = INVEN_STAFF;
            }

            if (!target)
            {
                for (int i = 0; i < INVEN_PACK; i++)
                {
                    object_type* pack_obj = &inventory[i];
                    if (!pack_obj->k_idx)
                        continue;
                    if (pack_obj->tval != TV_STAFF)
                        continue;
                    if (pack_obj->k_idx != o_ptr->k_idx)
                        continue;
                    target = pack_obj;
                    target_slot = i;
                    break;
                }
            }

            if (target)
            {
                int mult = CHANNELING_CHARGE_MULTIPLIER;
                int existing_raw = MAX(target->pval, 0);
                int donor_raw = MAX(o_ptr->pval, 0);
                int existing_uses = existing_raw / mult;
                int donor_uses = donor_raw / mult;
                if (donor_uses > 0)
                {
                    double existing_term = pow((double)existing_uses, 1.5);
                    double donor_term = pow((double)donor_uses, 1.5);
                    double combined_uses_raw = 0.0;
                    double sum_terms = existing_term + donor_term;
                    if (sum_terms > 0.0)
                        combined_uses_raw = pow(sum_terms, 2.0 / 3.0);
                    int combined_uses = (int)(combined_uses_raw + 0.5);
                    long combined_pval = (long)combined_uses * mult;
                    long max_pval = (long)(32767 / mult) * mult;
                    if (combined_pval > max_pval)
                        combined_pval = max_pval;
                    combined_uses = (int)(combined_pval / mult);
                    int gain_uses = combined_uses - existing_uses;
                    if (gain_uses > 0)
                    {
                        char target_name[80];
                        char donor_name[80];
                        char prompt[120];
                        format_staff_prompt_name(
                            target_name, sizeof(target_name), target, false);
                        format_staff_prompt_name(
                            donor_name, sizeof(donor_name), o_ptr, true);
                        
                        log_debug("Channeling: donor floor staff k_idx=%d pval=%d number=%d, target inv slot %d k_idx=%d pval=%d number=%d",
                                  o_ptr->k_idx, o_ptr->pval, o_ptr->number,
                                  target_slot, target->k_idx, target->pval, target->number);
                        
                        strnfmt(prompt, sizeof(prompt),
                            "Channel %s into your %s (%d charges)?",
                            donor_name, target_name, combined_uses);
                        if (get_check(prompt))
                        {
                            target->pval = (s16b)combined_pval;
                            target->ident &= ~(IDENT_EMPTY);
                            o_ptr->pval = 0;
                            o_ptr->ident |= IDENT_EMPTY;
                            
                            log_debug("Channeling complete: target now has pval=%d number=%d, donor has pval=%d number=%d",
                                      target->pval, target->number, o_ptr->pval, o_ptr->number);
                            
                            if (target_slot >= 0 && target_slot < INVEN_TOTAL)
                                inven_item_charges(target_slot);
                            p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);
                            p_ptr->window |= (PW_EQUIP | PW_PLAYER_0 | PW_INVEN);
                            msg_format("You channel %d charge%s into your %s (now %d).",
                                gain_uses, (gain_uses == 1) ? "" : "s",
                                target_name, combined_uses);
                            delete_object_idx(this_o_idx);
                            
                            log_debug("Channeling: deleted floor object idx %d", this_o_idx);
                            
                            done_pickup = true;
                            p_ptr->previous_action[0] = ACTION_MISC;
                            p_ptr->energy_use = 100;
                            skip_current_item = true;
                        }
                    }
                }
            }
        }

        if (skip_current_item)
            continue;

        while (!inven_carry_okay(o_ptr))
        {
            pickup_failure_result failure = resolve_pickup_failure(
                o_ptr, this_o_idx, o_name, attempted_replacement);

            if (failure == PICKUP_FAILURE_RETRY)
            {
                attempted_replacement = true;
                continue;
            }

            if (failure == PICKUP_FAILURE_EQUIPPED)
            {
                done_pickup = true;
                skip_current_item = true;
            }
            else
            {
                skip_current_item = true;
            }

            break;
        }

        if (skip_current_item)
            continue;

        // Check whether it would be too heavy
        if (p_ptr->total_weight + o_ptr->weight > weight_limit() * 3 / 2)
        {
            if (o_ptr->k_idx)
                msg_format("You cannot lift %s.", o_name);

            /* Check the next object */
            continue;
        }

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;

        /* Take a turn */
        p_ptr->energy_use = 100;

        /* Pick up the object */
        py_pickup_aux(this_o_idx);

        done_pickup = true;
    }

    if (!done_pickup)
    {
        p_ptr->previous_action[0] = ACTION_NOTHING;
        p_ptr->energy_use = 0;
    }
}

/*
 * Determine if a trap affects the player.
 * Based on player's evasion.
 */

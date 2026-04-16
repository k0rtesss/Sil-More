/* File: cmd-item-utility.c */

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

static void do_cmd_observe_enhanced(void);

void do_cmd_observe(void)
{
    /* Set up for enhanced menu cycling */
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Mark that 'x' command opened this menu */
    current_menu_command = 'x';
    current_menu_state = 0;  /* Start with inventory */
    
    /* Start the enhanced menu system */
    do_cmd_observe_enhanced();
}

/*
 * Enhanced observe command that supports cycling between inventory/equipment
 */
static void do_cmd_observe_enhanced(void)
{
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Clear any previous menu actions at start of new session */
    extern int enhanced_menu_action;
    extern int enhanced_equip_action;
    enhanced_menu_action = 0;
    enhanced_equip_action = 0;
    
    log_trace("do_cmd_observe_enhanced: Starting enhanced observe cycle");
    
    /* Continue cycling until user escapes or performs an action */
    while (true)
    {
        log_trace("do_cmd_observe_enhanced: Loop iteration, current_menu_state=%d", current_menu_state);
        
        if (current_menu_state == 0) {
            log_trace("do_cmd_observe_enhanced: Displaying inventory");
            /* Display inventory */
            do_cmd_inven();
            
            /* Check if user wants to switch to equipment */
            extern int enhanced_menu_action;
            log_trace("do_cmd_observe_enhanced: After inventory, enhanced_menu_action=%d", enhanced_menu_action);
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                log_trace("do_cmd_observe_enhanced: Switching to equipment");
                current_menu_state = 1;
                enhanced_menu_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_menu_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_inven */
                log_trace("do_cmd_observe_enhanced: Examining item, exiting cycle");
                enhanced_menu_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                log_trace("do_cmd_observe_enhanced: Exiting cycle (inven action=%d)", enhanced_menu_action);
                break;
            }
        }
        else {
            /* Display equipment */
            log_trace("do_cmd_observe_enhanced: Displaying equipment, current_menu_state=%d", current_menu_state);
            do_cmd_equip();
            log_trace("do_cmd_observe_enhanced: Returned from equipment, current_menu_state=%d", current_menu_state);
            
            /* Check if user wants to switch to inventory */
            extern int enhanced_equip_action;
            log_trace("do_cmd_observe_enhanced: After equipment, enhanced_equip_action=%d", enhanced_equip_action);
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                log_trace("do_cmd_observe_enhanced: Switching to inventory");
                current_menu_state = 0;
                enhanced_equip_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_equip_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_equip */
                log_trace("do_cmd_observe_enhanced: Examining item, exiting cycle");
                enhanced_equip_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                log_trace("do_cmd_observe_enhanced: Exiting cycle (equip action=%d)", enhanced_equip_action);
                break;
            }
        }
    }
    
    /* Clear the command state */
    current_menu_command = 0;
    current_menu_state = 0;
}

/*
 * Helper function which actually removes the inscription
 */
void uninscribe(object_type* o_ptr)
{
    /* Remove the inscription */
    o_ptr->obj_note = 0;

    /* Message */
    msg_print("Inscription removed.");

    /* Combine the pack */
    p_ptr->notice |= (PN_COMBINE);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
}

/*
 * Remove the inscription from an object
 * XXX Mention item (when done)?
 */
void do_cmd_uninscribe(void)
{
    int item;

    object_type* o_ptr;

    cptr q, s;

    /* Get an item */
    q = "Un-inscribe which item? ";
    s = "You have nothing to un-inscribe.";
    if (!get_item(&item, q, s, (USE_EQUIP | USE_INVEN | USE_FLOOR)))
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

    /* Nothing to remove */
    if (!o_ptr->obj_note)
    {
        msg_print("That item had no inscription to remove.");
        return;
    }

    // Do the work
    uninscribe(o_ptr);
}

/*
 * Inscribe an object with a comment
 */
void do_cmd_inscribe(void)
{
    int item;

    object_type* o_ptr;

    char o_name[80];

    char tmp[80];

    cptr q, s;

    /* Get an item */
    q = "Inscribe which item? ";
    s = "You have nothing to inscribe.";
    if (!get_item(&item, q, s, (USE_EQUIP | USE_INVEN | USE_FLOOR)))
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

    /* Describe the activity */
    if (item < 0)
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
    else
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Message */
    msg_format("Inscribing %s.", o_name);
    message_flush();

    /* Start with nothing */
    SDL_strlcpy(tmp, "", sizeof(tmp));

    /* Use old inscription */
    if (o_ptr->obj_note)
    {
        /* Start with the old inscription */
        strnfmt(tmp, sizeof(tmp), "%s", quark_str(o_ptr->obj_note));
    }

    /* Get a new inscription (possibly empty) */
    if (prompt_text_input("Inscription:",
            "Enter accepts, Esc cancels, Backspace erases.", tmp,
            sizeof(tmp), false))
    {
        // if given an empty inscription, then uninscribe instead
        if (strlen(tmp) == 0)
        {
            uninscribe(o_ptr);
            return;
        }

        /* Save the inscription */
        o_ptr->obj_note = quark_add(tmp);

        /* Combine the pack */
        p_ptr->notice |= (PN_COMBINE);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }
}

/*
 * An "item_tester_hook" for refueling lanterns
 */
static bool item_tester_refuel_lantern(const object_type* o_ptr)
{
    /* Flasks of oil are okay */
    if (o_ptr->tval == TV_FLASK)
        return (true);

    /* Non-empty lanterns are okay */
    if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_LANTERN)
        && player_light_has_fuel(o_ptr))
    {
        return (true);
    }

    /* Assume not okay */
    return (false);
}

/*
 * Refill the player's lamp (from the pack or floor)
 */
void do_cmd_refuel_lamp(object_type* default_o_ptr, int default_item)
{
    int item;

    object_type* o_ptr;
    object_type* j_ptr;

    cptr q, s;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = default_item;
    }
    /* Get an item */
    else
    {
        /* Restrict the choices */
        item_tester_hook = item_tester_refuel_lantern;

        /* Get an item */
        q = "Refill with which source of oil? ";
        s = "You have no sources of oil.";
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
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Get the lantern */
    j_ptr = &inventory[INVEN_LITE];

    /* Refuel from a latern */
    if (o_ptr->sval == SV_LIGHT_LANTERN)
    {
        player_light_add_fuel(j_ptr, o_ptr->timeout);
    }
    /* Refuel from a flask */
    else
    {
        player_light_add_fuel(j_ptr, o_ptr->pval);
    }

    /* Message */
    msg_print("You fuel your lamp.");

    /* Comment */
    if (player_light_fuel(j_ptr) >= player_light_max_fuel(j_ptr))
    {
        player_light_set_fuel(j_ptr, player_light_max_fuel(j_ptr));
        msg_print("Your lamp is full.");
    }

    /* Refilled from a latern */
    if (o_ptr->sval == SV_LIGHT_LANTERN)
    {
        /* Unstack if necessary */
        if (o_ptr->number > 1)
        {
            object_type* i_ptr;
            object_type object_type_body;

            /* Get local object */
            i_ptr = &object_type_body;

            /* Obtain a local object */
            object_copy(i_ptr, o_ptr);

            /* Modify quantity */
            i_ptr->number = 1;

            /* Remove fuel */
            i_ptr->timeout = 0;

            /* Unstack the used item */
            o_ptr->number--;

            /* Carry or drop */
            if (item >= 0)
            {
                item = inven_carry(i_ptr, false);
                if (item < 0)
                    drop_near(i_ptr, 0, p_ptr->py, p_ptr->px);
            }
            else
                drop_near(i_ptr, 0, p_ptr->py, p_ptr->px);
        }

        /* Empty a single latern */
        else
        {
            /* No more fuel */
            o_ptr->timeout = 0;
        }

        /* Combine / Reorder the pack (later) */
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN);
    }

    /* Refilled from a flask */
    else
    {
        /* Decrease the item (from the pack) */
        if (item >= 0)
        {
            inven_item_increase(item, -1);
            inven_item_describe(item);
            inven_item_optimize(item);
        }

        /* Decrease the item (from the floor) */
        else
        {
            floor_item_increase(0 - item, -1);
            floor_item_describe(0 - item);
            floor_item_optimize(0 - item);
        }
    }

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP);

    // get another chance to identify the lamp
    ident_on_wield(j_ptr);

    p_ptr->redraw |= (PR_LIGHT);

    /* Force immediate sidebar update */
    handle_stuff();
}

/*
 * An "item_tester_hook" for refueling torches
 */
static bool item_tester_refuel_torch(const object_type* o_ptr)
{
    /* Torches are okay */
    if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_TORCH))
        return (true);

    /* Assume not okay */
    return (false);
}

/*
 * An "item_tester_hook" for refueling torches
 */
static bool item_tester_refuel_mallorn(const object_type* o_ptr)
{
    /* Torches are okay */
    if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_MALLORN))
        return (true);

    /* Assume not okay */
    return (false);
}

/*
 * Refuel the player's torch (from the pack or floor)
 */
void do_cmd_refuel_torch(
    object_type* default_o_ptr, int default_item, bool is_mallorn)
{
    int item;

    object_type* o_ptr;
    object_type* j_ptr;

    cptr q, s;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = default_item;
    }
    /* Get an item */
    else
    {
        /* Restrict the choices */
        item_tester_hook = is_mallorn ? item_tester_refuel_mallorn
                                      : item_tester_refuel_torch;

        /* Get an item */
        q = "Refuel with which torch? ";
        s = "You have no extra torches.";
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
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Get the primary torch */
    j_ptr = &inventory[INVEN_LITE];
    
    log_debug("do_cmd_refuel_torch: BEFORE refuel - j_ptr (INVEN_LITE) k_idx=%d timeout=%d",
              j_ptr->k_idx, j_ptr->timeout);
    log_debug("do_cmd_refuel_torch: BEFORE refuel - o_ptr (item=%d) k_idx=%d timeout=%d",
              item, o_ptr->k_idx, o_ptr->timeout);

    /* Refuel */
    j_ptr->timeout += o_ptr->timeout + 5;
    
    log_debug("do_cmd_refuel_torch: AFTER refuel - j_ptr timeout=%d", j_ptr->timeout);

    /* Message */
    msg_print("You combine the torches.");

    /* Over-fuel message */
    int max_fuel = is_mallorn ? FUEL_MALLORN : FUEL_TORCH;
    if (j_ptr->timeout >= max_fuel)
    {
        j_ptr->timeout = max_fuel;
        msg_print("Your torch is fully fueled.");
    }

    /* Refuel message */
    else
    {
        msg_print("Your torch glows more brightly.");
    }

    /* Decrease the item (from the pack) */
    if (item >= 0)
    {
        inven_item_increase(item, -1);
        inven_item_describe(item);
        inven_item_optimize(item);
    }

    /* Decrease the item (from the floor) */
    else
    {
        floor_item_increase(0 - item, -1);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP);

    // get another chance to identify the torch
    ident_on_wield(j_ptr);

    p_ptr->redraw |= (PR_LIGHT);

    /* Force immediate sidebar update */
    handle_stuff();
}

/*
 * Refuel the player's lamp or torch
 */
void do_cmd_refuel(void)
{
    object_type* o_ptr;

    /* Get the light */
    o_ptr = &inventory[INVEN_LITE];

    /* It is nothing */
    if (o_ptr->tval != TV_LIGHT)
    {
        msg_print("You are not wielding a light.");
    }

    /* It's a lamp */
    else if (o_ptr->sval == SV_LIGHT_LANTERN)
    {
        do_cmd_refuel_lamp(NULL, 0);
    }

    /* It's a torch */
    else if (o_ptr->sval == SV_LIGHT_TORCH)
    {
        do_cmd_refuel_torch(NULL, 0, false);
    }

    /* It's a torch */
    else if (o_ptr->sval == SV_LIGHT_MALLORN)
    {
        do_cmd_refuel_torch(NULL, 0, true);
    }

    /* No torch to refuel */
    else
    {
        msg_print("Your light cannot be refueled.");
    }
}

/*
 * Target command
 */

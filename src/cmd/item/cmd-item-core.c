/* File: cmd-item-core.c */

/*
 * Copyright (c) 2001 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "object/object-ui-enhanced.h"
#include "object/object-ui-select.h"

static int get_equip_sound(const object_type* o_ptr)
{
    /* Swords */
    if (o_ptr->tval == TV_SWORD)
        return MSG_EQUIP_SWORD;
    
    /* Bows and arrows */
    if (o_ptr->tval == TV_BOW || o_ptr->tval == TV_ARROW)
        return MSG_EQUIP_BOW;
    
    /* Other weapons */
    if (o_ptr->tval == TV_POLEARM || o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_DIGGING)
        return MSG_EQUIP_WEAPON;
    
    /* Chain armor (mail) */
    if (o_ptr->tval == TV_MAIL)
        return MSG_EQUIP_MAIL;
    
    /* Leather armor (soft armor) */
    if (o_ptr->tval == TV_SOFT_ARMOR)
        return MSG_EQUIP_LEATHER;
    
    /* All other types of armor */
    if (o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_CLOAK || o_ptr->tval == TV_HELM || 
        o_ptr->tval == TV_CROWN || o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
        return MSG_EQUIP_ARMOR;
    
    /* Rings and amulets */
    if (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        return MSG_EQUIP_JEWELRY;
    
    /* Default - no sound */
    return -1;
}

/*
 * Helper function to determine the unequip sound based on item type
 */
static int get_unequip_sound(const object_type* o_ptr)
{
    /* Swords */
    if (o_ptr->tval == TV_SWORD)
        return MSG_UNEQUIP_SWORD;
    
    /* Bows and arrows */
    if (o_ptr->tval == TV_BOW || o_ptr->tval == TV_ARROW)
        return MSG_UNEQUIP_BOW;
    
    /* Other weapons */
    if (o_ptr->tval == TV_POLEARM || o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_DIGGING)
        return MSG_UNEQUIP_WEAPON;
    
    /* Chain armor (mail) */
    if (o_ptr->tval == TV_MAIL)
        return MSG_UNEQUIP_MAIL;
    
    /* Leather armor (soft armor) */
    if (o_ptr->tval == TV_SOFT_ARMOR)
        return MSG_UNEQUIP_LEATHER;
    
    /* All other types of armor */
    if (o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_CLOAK || o_ptr->tval == TV_HELM || 
        o_ptr->tval == TV_CROWN || o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
        return MSG_UNEQUIP_ARMOR;
    
    /* Rings and amulets */
    if (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        return MSG_UNEQUIP_JEWELRY;
    
    /* Default - no sound */
    return -1;
}

/*
 * The "wearable" tester
 */
static bool item_tester_hook_wear(const object_type* o_ptr)
{
    // Despite being a crown, the Iron Crown cannot be worn
    if ((o_ptr->name1 >= ART_MORGOTH_0) && (o_ptr->name1 <= ART_MORGOTH_3))
        return (false);

    /* Check for a usable slot */
    if (wield_slot(o_ptr) >= INVEN_WIELD)
        return (true);

    /* Assume not wearable */
    return (false);
}

static bool item_tester_hook_ring_slots(const object_type* o_ptr)
{
    return (o_ptr == &inventory[INVEN_LEFT]) || (o_ptr == &inventory[INVEN_RIGHT]);
}

bool throw_slot_menu_active = false;
bool throw_slot_enabled[INVEN_TOTAL];

static bool item_tester_hook_throw_slots(const object_type* o_ptr)
{
    if (!throw_slot_menu_active)
        return false;

    if (!o_ptr)
        return false;

    if ((o_ptr < inventory) || (o_ptr >= inventory + INVEN_TOTAL))
        return false;

    int slot = (int)(o_ptr - inventory);

    return throw_slot_enabled[slot];
}

static bool smith_oath_takeoff_hits_pack(const object_type* o_ptr, int source_item)
{
    if (!smith_oath_forbids_object(o_ptr))
        return false;

    if (source_item >= 0 && source_item < INVEN_PACK)
        return inven_carry_okay_after_removing(o_ptr, source_item, 1);

    return inven_carry_okay(o_ptr);
}

bool open_supplies_menu_with_context(supply_menu_action default_action, int default_group, bool default_focus, bool default_hotkey)
{
    supply_menu_request request = {0};
    supply_menu_action action = default_action;
    bool hotkey = default_hotkey;
    bool focus = default_focus;
    int group = default_group;

    if (supplies_has_pending_action())
    {
        supply_menu_action pending = supplies_pending_action();
        if (pending != SUPPLY_MENU_ACTION_NONE)
            action = pending;
        hotkey = supplies_pending_hotkey();
        int pending_group = supplies_pending_group();
        if (pending_group >= 0 && pending_group < SUPPLY_GROUP_MAX)
        {
            focus = true;
            group = pending_group;
        }
        supplies_clear_pending_action();
    }

    request.action = action;
    request.hotkey_mode = hotkey;
    if (focus && group >= 0 && group < SUPPLY_GROUP_MAX)
    {
        request.focus_group = true;
        request.group = group;
    }

    return do_cmd_knowledge_supplies(&request);
}

/* Flag indicating enhanced menus need to refresh the main display after closing */
static bool enhanced_drop_refresh_pending = false;

/*
 * Use an item by index, helper for enhanced menus
 */
void do_cmd_use_item_by_index(int item)
{
    if (item == SUPPLIES_INDEX)
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE, -1, false, true);
        return;
    }

    object_type* o_ptr;

    /* Get the item (in the pack) */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
        log_debug("do_cmd_use_item_by_index: Using item from inventory, index=%d", item);
    }

    /* Get the item (on the floor) */
    else
    {
        o_ptr = &o_list[0 - item];
        log_debug("do_cmd_use_item_by_index: Using item from floor, index=%d, o_list index=%d", item, 0 - item);
    }

    // determine the action based on the item type
    switch (o_ptr->tval)
    {
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    case TV_LIGHT:
    case TV_AMULET:
    case TV_RING:
    case TV_ARROW:
    case TV_FLASK:
    {
        if (item < INVEN_WIELD)
        {
            object_type* l_ptr = &inventory[INVEN_LITE];
            bool try_to_wield = true;

            // possibly refuel a light
            if ((o_ptr->tval == TV_FLASK)
                || ((l_ptr->tval == o_ptr->tval) && (l_ptr->sval == o_ptr->sval)
                    && ((o_ptr->sval == SV_LIGHT_TORCH)
                        || (o_ptr->sval == SV_LIGHT_LANTERN)
                        || (o_ptr->sval == SV_LIGHT_MALLORN))))
            {
                if ((l_ptr->sval == SV_LIGHT_TORCH)
                    && (o_ptr->tval != TV_FLASK))
                {
                    if ((o_ptr->timeout + l_ptr->timeout <= FUEL_TORCH)
                        || get_check(
                            "Refueling from this torch will waste some fuel. "
                            "Proceed? "))
                    {
                        do_cmd_refuel_torch(o_ptr, item, false);
                        try_to_wield = false;
                    }
                }
                else if ((l_ptr->sval == SV_LIGHT_MALLORN)
                    && (o_ptr->tval != TV_FLASK))
                {
                    if ((o_ptr->timeout + l_ptr->timeout <= FUEL_TORCH)
                        || get_check(
                            "Refueling from this mallorn torch will waste "
                            "some fuel. Proceed? "))
                    {
                        do_cmd_refuel_torch(o_ptr, item, true);
                        try_to_wield = false;
                    }
                }
                else if (l_ptr->sval == SV_LIGHT_LANTERN)
                {
                    if ((o_ptr->timeout + l_ptr->timeout <= FUEL_LAMP)
                        || get_check(
                            "Refueling from this flask will waste some oil. "
                            "Proceed? "))
                    {
                        do_cmd_refuel_lamp(o_ptr, item);
                        try_to_wield = false;
                    }
                }
            }

            if (o_ptr->tval == TV_FLASK && try_to_wield)
            {
                if ((l_ptr->tval != TV_LIGHT)
                    || (l_ptr->sval != SV_LIGHT_LANTERN))
                {
                    msg_print("You are not wielding a lantern.");
                }
                try_to_wield = false;
            }

            if (try_to_wield)
            {
                log_debug("do_cmd_use_item_by_index: Calling do_cmd_wield with item=%d (o_ptr tval=%d)", item, o_ptr->tval);
                /* Handle arrows and throwing weapons */
                if (o_ptr->tval == TV_ARROW)
                {
                    do_cmd_wield(o_ptr, item);
                }
                else
                {
                    do_cmd_wield(o_ptr, item);
                }
            }
        }
        else
        {
            /* Handle equipped arrows specially */
            if (o_ptr->tval == TV_ARROW)
            {
                do_cmd_takeoff(o_ptr, item);
            }
            else
            {
                do_cmd_takeoff(o_ptr, item);
            }
        }
        break;
    }
    case TV_NOTE:
    {
        note_info_screen(o_ptr);
        break;
    }
    case TV_METAL:
    {
        msg_print("To smith with mithril or star-iron, take them to a forge and "
                  "type (,).");
        break;
    }
    case TV_CHEST:
    {
        msg_print("You would need to put it down to open it.");
        break;
    }
    case TV_STAFF:
    {
        extern char current_menu_command;
        /* If wielding ('w' command), equip the staff directly */
        if (current_menu_command == 'w')
        {
            do_cmd_wield(o_ptr, item);
        }
        else
        {
            /* Otherwise, activate it (for 'u' command) */
            do_cmd_activate_staff(o_ptr, item);
        }
        break;
    }
    case TV_GEM:
    {
        do_cmd_use_gem(o_ptr, item);
        break;
    }
    case TV_HORN:
    {
        do_cmd_play_instrument(o_ptr, item);
        break;
    }
    case TV_POTION:
    {
        do_cmd_quaff_potion(o_ptr, item);
        break;
    }
    case TV_FOOD:
    {
        do_cmd_eat_food(o_ptr, item);
        break;
    }
    default:
    {
        msg_print("It has no use.");
        break;
    }
    }
}

/*
 * Use an item, a unified 'use' command.
 */
void do_cmd_use_item(void)
{
    /* Set up for enhanced menu cycling */
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Mark that 'u' command opened this menu */
    current_menu_command = 'u';
    current_menu_state = 0;  /* Start with inventory */
    
    /* Start the enhanced menu system */
    do_cmd_use_item_enhanced();
}

/*
 * Wrapper for wear/wield command with enhanced menu support
 */
void do_cmd_wield_wrapper(void)
{
    /* Set up for enhanced menu cycling */
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Mark that 'w' command opened this menu */
    current_menu_command = 'w';
    current_menu_state = 0;  /* Start with inventory */
    
    /* Start the enhanced menu system */
    do_cmd_wield_enhanced();
}

/*
 * Enhanced wear/wield command that supports cycling between inventory/equipment
 */
void do_cmd_wield_enhanced(void)
{
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Clear any previous menu actions at start of new session */
    extern int enhanced_menu_action;
    extern int enhanced_equip_action;
    enhanced_menu_action = 0;
    enhanced_equip_action = 0;
    
    log_trace("do_cmd_wield_enhanced: Starting enhanced wear/wield cycle");
    
    /* Set the filter to only show wearable items */
    item_tester_hook = item_tester_hook_wear;
    log_debug("do_cmd_wield_enhanced: Set item_tester_hook to item_tester_hook_wear (%p)", (void*)item_tester_hook);
    
    /* Continue cycling until user escapes or performs an action */
    while (true)
    {
        if (current_menu_state == 0) {
            /* Display inventory */
            do_cmd_inven();
            
            /* Check if user wants to switch to equipment */
            extern int enhanced_menu_action;
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                current_menu_state = 1;
                enhanced_menu_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_menu_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_inven */
                enhanced_menu_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
        else {
            /* Display equipment */
            do_cmd_equip();
            
            /* Check if user wants to switch to inventory */
            extern int enhanced_equip_action;
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                current_menu_state = 0;
                enhanced_equip_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_equip_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_equip */
                enhanced_equip_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
    }
    
    /* Clear the filter */
    item_tester_hook = NULL;
    
    /* Clear the command state */
    current_menu_command = 0;
    current_menu_state = 0;
}

/*
 * Enhanced use item command that supports cycling between inventory/equipment
 */
void do_cmd_use_item_enhanced(void)
{
    extern char current_menu_command;
    extern int current_menu_state;
    
    /* Clear any previous menu actions at start of new session */
    extern int enhanced_menu_action;
    extern int enhanced_equip_action;
    enhanced_menu_action = 0;
    enhanced_equip_action = 0;
    
    log_trace("do_cmd_use_item_enhanced: Starting enhanced use item cycle");
    
    /* Continue cycling until user escapes or performs an action */
    while (true)
    {
        if (current_menu_state == 0) {
            /* Display inventory */
            do_cmd_inven();
            
            /* Check if user wants to switch to equipment */
            extern int enhanced_menu_action;
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                current_menu_state = 1;
                enhanced_menu_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_menu_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_inven */
                enhanced_menu_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
        else {
            /* Display equipment */
            do_cmd_equip();
            
            /* Check if user wants to switch to inventory */
            extern int enhanced_equip_action;
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                current_menu_state = 0;
                enhanced_equip_action = 0;  /* Reset after using */
                continue;
            }
            else if (enhanced_equip_action == ENHANCED_ACTION_EXAMINE) {
                /* Examine item - handled by do_cmd_equip */
                enhanced_equip_action = 0;  /* Reset after using */
                break;
            }
            else {
                /* Exit or item was used */
                break;
            }
        }
    }
    
    /* Clear the command state */
    current_menu_command = 0;
    current_menu_state = 0;
}

/*
 * Direct access inventory with cycling support
 */
void do_cmd_inven_direct(void)
{
    log_debug("do_cmd_inven_direct: Starting direct access inventory with cycling");
    
    int menu_state = 0;  /* 0=inventory, 1=equipment */
    
    while (true) {
        if (menu_state == 0) {
            /* Display inventory */
            log_trace("do_cmd_inven_direct: Showing inventory");
            do_cmd_inven();
            
            /* Check action */
            extern int enhanced_menu_action;
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                log_trace("do_cmd_inven_direct: Switching to equipment");
                menu_state = 1;
                enhanced_menu_action = 0;
                continue;
            }
            else {
                /* Exit or item examined */
                enhanced_menu_action = 0;
                break;
            }
        }
        else {
            /* Display equipment */
            log_trace("do_cmd_inven_direct: Showing equipment");
            do_cmd_equip();
            
            /* Check action */
            extern int enhanced_equip_action;
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                log_trace("do_cmd_inven_direct: Switching to inventory");
                menu_state = 0;
                enhanced_equip_action = 0;
                continue;
            }
            else {
                /* Exit or item examined */
                enhanced_equip_action = 0;
                break;
            }
        }
    }
    
    log_debug("do_cmd_inven_direct: Direct access cycling finished");
}

/*
 * Direct access equipment with cycling support
 */
void do_cmd_equip_direct(void)
{
    log_debug("do_cmd_equip_direct: Starting direct access equipment with cycling");
    
    int menu_state = 1;  /* 0=inventory, 1=equipment */
    
    while (true) {
        if (menu_state == 0) {
            /* Display inventory */
            log_trace("do_cmd_equip_direct: Showing inventory");
            do_cmd_inven();
            
            /* Check action */
            extern int enhanced_menu_action;
            if (enhanced_menu_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to equipment */
                log_trace("do_cmd_equip_direct: Switching to equipment");
                menu_state = 1;
                enhanced_menu_action = 0;
                continue;
            }
            else {
                /* Exit or item examined */
                enhanced_menu_action = 0;
                break;
            }
        }
        else {
            /* Display equipment */
            log_trace("do_cmd_equip_direct: Showing equipment");
            do_cmd_equip();
            
            /* Check action */
            extern int enhanced_equip_action;
            if (enhanced_equip_action == ENHANCED_ACTION_SWITCH) {
                /* Switch to inventory */
                log_trace("do_cmd_equip_direct: Switching to inventory");
                menu_state = 0;
                enhanced_equip_action = 0;
                continue;
            }
            else {
                /* Exit or item examined */
                enhanced_equip_action = 0;
                break;
            }
        }
    }
    
    log_debug("do_cmd_equip_direct: Direct access cycling finished");
}

/*
 * Display inventory
 */
void do_cmd_inven(void)
{
    log_debug("do_cmd_inven: Starting inventory command");
    
    /* Hack -- Start in "inventory" mode */
    p_ptr->command_wrk = (USE_INVEN);

    enhanced_inventory_selected_item = -1;

    /* Hack -- show empty slots */
    item_tester_full = true;

    /* Force viewing mode */
    p_ptr->command_see = true;

    /* Display the inventory with snapshot-aware overlay rendering. */
    run_inven_enhanced_menu();

    /* Hack -- hide empty slots */
    item_tester_full = false;

    extern int enhanced_menu_action;
    extern int enhanced_inventory_selected_item;

    int action = enhanced_menu_action;
    int selected_index = enhanced_inventory_selected_item;
    bool death_view = death_spectator_active();

    switch (action)
    {
    case ENHANCED_ACTION_EXAMINE:
    {
        log_trace("do_cmd_inven: Examining item %d", selected_index);
        extern char current_menu_command;
        /* Show comparisons when accessed via 'x' menu OR when examining via arrow-right in direct access */
        bool include_comparisons = (current_menu_command == 'u' || current_menu_command == 'x' || current_menu_command == 0);
        describe_item_with_comparisons(selected_index, include_comparisons);
        break;
    }

    case ENHANCED_ACTION_USE:
        if (death_view)
        {
            msg_print("You can no longer take that action.");
        }
        else
        {
            log_trace("do_cmd_inven: Using item %d", selected_index);
            if (selected_index != -1)
                do_cmd_use_item_by_index(selected_index);
        }
        break;

    case ENHANCED_ACTION_DROP:
        if (death_view)
        {
            msg_print("You can no longer take that action.");
        }
        else
        {
            log_trace("do_cmd_inven: Dropping item %d", selected_index);
            if (selected_index >= 0)
                do_cmd_drop_item_by_index(selected_index);
            else
                bell("Cannot drop floor items from this menu!");
        }
        break;

    case ENHANCED_ACTION_SUPPLIES:
    {
        log_trace("do_cmd_inven: Opening supplies menu (command=%c)", current_menu_command ? current_menu_command : '0');
        supply_menu_action default_action = SUPPLY_MENU_ACTION_NONE;
        bool default_hotkey = false;
        if (current_menu_command == 'u')
        {
            default_action = SUPPLY_MENU_ACTION_USE;
            default_hotkey = true;
        }
        else if (current_menu_command == 'd')
        {
            default_action = SUPPLY_MENU_ACTION_DROP;
            default_hotkey = true;
        }
        open_supplies_menu_with_context(default_action, -1, false, default_hotkey);
        break;
    }

    default:
        break;
    }

    if (enhanced_drop_refresh_pending)
    {
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_MESSAGE);
        enhanced_drop_refresh_pending = false;
    }

    /* Ensure the main display reflects any changes (drops, etc.) */
    handle_stuff();

    if (action != ENHANCED_ACTION_SWITCH)
        enhanced_menu_action = ENHANCED_ACTION_NONE;
    enhanced_inventory_selected_item = -1;
    
    log_debug("do_cmd_inven: Exiting");
}

/*
 * Display equipment
 */
void do_cmd_equip(void)
{
    log_debug("do_cmd_equip: Starting equipment command");
    
    /* Hack -- Start in "equipment" mode */
    p_ptr->command_wrk = (USE_EQUIP);

    enhanced_equipment_selected_item = -1;

    /* Hack -- show empty slots */
    item_tester_full = true;

    /* Force viewing mode */
    p_ptr->command_see = true;

    /* Display the equipment with snapshot-aware overlay rendering. */
    run_equip_enhanced_menu();

    /* Hack -- undo the hack above */
    item_tester_full = false;

    extern int enhanced_equip_action;
    extern int enhanced_equipment_selected_item;

    int action = enhanced_equip_action;
    int selected_index = enhanced_equipment_selected_item;
    bool death_view = death_spectator_active();

    switch (action)
    {
    case ENHANCED_ACTION_EXAMINE:
        log_trace("do_cmd_equip: Examining item %d", selected_index);
        if (selected_index >= INVEN_WIELD && selected_index < INVEN_TOTAL)
        {
            (void)player_try_identify_smithing_object_on_examine(
                &inventory[selected_index], true);
            object_info_screen(&inventory[selected_index]);
        }
        break;

    case ENHANCED_ACTION_USE:
        if (death_view)
        {
            msg_print("You can no longer take that action.");
        }
        else
        {
            log_trace("do_cmd_equip: Using item %d", selected_index);
            if (selected_index != -1)
                do_cmd_use_item_by_index(selected_index);
        }
        break;

    case ENHANCED_ACTION_DROP:
        if (death_view)
        {
            msg_print("You can no longer take that action.");
        }
        else
        {
            log_trace("do_cmd_equip: Dropping item %d", selected_index);
            if (selected_index >= INVEN_WIELD)
                do_cmd_drop_item_by_index(selected_index);
        }
        break;

    default:
        break;
    }

    if (enhanced_drop_refresh_pending)
    {
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_MESSAGE);
        enhanced_drop_refresh_pending = false;
    }

    /* Ensure the main display reflects any changes (drops, etc.) */
    handle_stuff();

    if (action != ENHANCED_ACTION_SWITCH)
        enhanced_equip_action = ENHANCED_ACTION_NONE;
    enhanced_equipment_selected_item = -1;
    
    log_debug("do_cmd_equip: Exiting");
}

/*
 * Wield or wear a single item from the pack or floor
 */
void do_cmd_wield(object_type* default_o_ptr, int default_item)
{
    int item, slot;

    object_type* o_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    cptr act;

    cptr q, s;

    int i, quantity, original_quantity;

    bool weapon_less_effective = false;

    bool grants_two_weapon = false;

    char o_name[80];

    bool combine = false;
    bool is_throwing = false;

    u32b f1, f2, f3, f4;

    log_debug("do_cmd_wield: Called with default_o_ptr=%p, default_item=%d", (void*)default_o_ptr, default_item);

    /* Ensure throw_slot_menu_active is false at start */
    throw_slot_menu_active = false;
    for (i = 0; i < INVEN_TOTAL; i++)
        throw_slot_enabled[i] = false;

    // use specified item if possible
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = default_item;
        log_debug("do_cmd_wield: Using default item, tval=%d, sval=%d, k_idx=%d", 
            o_ptr->tval, o_ptr->sval, o_ptr->k_idx);
    }
    /* Get an item */
    else
    {
        /* Restrict the choices */
        item_tester_hook = item_tester_hook_wear;

        /* Get an item */
        q = "Wear/Wield which item? ";
        s = "You have nothing you can wear or wield.";
        if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR)))
            return;

        /* Get the item (in the pack) */
        if (item >= 0)
        {
            o_ptr = &inventory[item];
        }
        else
        {
            o_ptr = &o_list[0 - item];
        }
    }

    // remember how many there were
    original_quantity = o_ptr->number;

    // Check whether it would be too heavy
    if ((item < 0)
        && (p_ptr->total_weight + o_ptr->weight > weight_limit() * 3 / 2))
    {
        /* Describe it */
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);

        log_debug("do_cmd_wield: Floor item too heavy - total=%d + item=%d > limit=%d", 
            p_ptr->total_weight, o_ptr->weight, weight_limit() * 3 / 2);

        if (o_ptr->k_idx)
            msg_format("You cannot lift %s.", o_name);
        else
            log_debug("do_cmd_wield: WARNING - o_ptr->k_idx is 0, no message shown to user!");

        /* Abort */
        return;
    }
    
    log_debug("do_cmd_wield: Weight check passed or inventory item (item=%d)", item);

    /* Check the slot */
    slot = wield_slot(o_ptr);
    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
    {
        if (item < 0)
            object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
        else
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        msg_format("You cannot wear or wield %s.", o_name);
        return;
    }

    if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_LANTERN)
        && player_lamp_oil_would_overflow_with_bonus(o_ptr->timeout,
            (item < 0) ? 1 : 0)
        && !get_check("Taking this lamp will waste some oil. Proceed? "))
    {
        return;
    }

    /* Ask for ring to replace */
    if ((o_ptr->tval == TV_RING) && inventory[INVEN_LEFT].k_idx
        && inventory[INVEN_RIGHT].k_idx)
    {
        item_tester_tval = TV_RING;
        item_tester_hook = item_tester_hook_ring_slots;
        item_tester_full = false;

        q = "Replace which ring? ";
        s = "Oops.";
        if (!get_item(&slot, q, s, USE_EQUIP))
        {
            item_tester_tval = 0;
            item_tester_hook = NULL;
            return;
        }

        item_tester_tval = 0;
        item_tester_hook = NULL;
    }

    object_flags(o_ptr, &f1, &f2, &f3);
    is_throwing = player_can_treat_as_throwing_flags(o_ptr, f3);

    log_debug("do_cmd_wield: item=%d, is_throwing=%d, slot=%d", item, is_throwing, slot);

    if (is_throwing)
    {
        bool any_throw_dest = false;
        int slot_choice;

        log_debug("do_cmd_wield: Throwing weapon detected, showing slot menu");
        throw_slot_menu_active = true;

        for (i = 0; i < INVEN_TOTAL; i++)
            throw_slot_enabled[i] = false;

        {
            object_type* wield_ptr = &inventory[INVEN_WIELD];
            bool allow_wield = true;

            if (wield_ptr->k_idx && cursed_p(wield_ptr)
                && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
            {
                allow_wield = false;
            }

            if (allow_wield)
            {
                throw_slot_enabled[INVEN_WIELD] = true;
                any_throw_dest = true;
            }
        }

        {
            object_type* q1_ptr = &inventory[INVEN_QUIVER1];
            bool allow_quiver = true;

            if (q1_ptr->k_idx && cursed_p(q1_ptr)
                && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
            {
                allow_quiver = false;
            }

            if (allow_quiver)
            {
                throw_slot_enabled[INVEN_QUIVER1] = true;
                any_throw_dest = true;
            }
        }

        {
            object_type* q2_ptr = &inventory[INVEN_QUIVER2];
            bool allow_quiver = true;

            if (q2_ptr->k_idx && cursed_p(q2_ptr)
                && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
            {
                allow_quiver = false;
            }

            if (allow_quiver)
            {
                throw_slot_enabled[INVEN_QUIVER2] = true;
                any_throw_dest = true;
            }
        }

        if (!any_throw_dest)
        {
            log_debug("do_cmd_wield: No available slot for throwing weapon, returning");
            msg_print("You have no available slot for that throwing weapon.");
            throw_slot_menu_active = false;
            return;
        }

        slot_choice = slot;

        if (!throw_slot_enabled[slot_choice])
        {
            if (throw_slot_enabled[INVEN_QUIVER1])
                slot_choice = INVEN_QUIVER1;
            else if (throw_slot_enabled[INVEN_QUIVER2])
                slot_choice = INVEN_QUIVER2;
            else
                slot_choice = INVEN_WIELD;
        }

        item_tester_hook = item_tester_hook_throw_slots;
        item_tester_full = false;

        q = "Place throwing weapon where? ";
        s = "Oops.";

        bool saved_command_see = p_ptr->command_see;
        byte saved_command_wrk = p_ptr->command_wrk;
        p_ptr->command_see = true;
        p_ptr->command_wrk = (USE_EQUIP);

        bool slot_selected = get_item(&slot_choice, q, s, USE_EQUIP);

        p_ptr->command_see = saved_command_see;
        p_ptr->command_wrk = saved_command_wrk;

        if (!slot_selected)
        {
            log_debug("do_cmd_wield: User cancelled slot selection, cleaning up and returning");
            item_tester_hook = NULL;
            item_tester_full = false;

            for (i = 0; i < INVEN_TOTAL; i++)
                throw_slot_enabled[i] = false;

            throw_slot_menu_active = false;
            return;
        }

        log_debug("do_cmd_wield: User selected slot %d for throwing weapon", slot_choice);

        item_tester_hook = NULL;
        item_tester_full = false;
        throw_slot_menu_active = false;

        slot = slot_choice;

        if ((slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2))
        {
            if (inventory[slot].k_idx
                && object_similar(&inventory[slot], o_ptr))
                combine = true;
        }

        for (i = 0; i < INVEN_TOTAL; i++)
            throw_slot_enabled[i] = false;
    }
    else
    {
        // Special cases for merging arrows
        if (object_similar(&inventory[INVEN_QUIVER1], o_ptr))
        {
            slot = INVEN_QUIVER1;
            combine = true;
        }
        else if (object_similar(&inventory[INVEN_QUIVER2], o_ptr))
        {
            slot = INVEN_QUIVER2;
            combine = true;
        }
        /* Ask for arrow set to replace */
        else if (o_ptr->tval == TV_ARROW)
        {
            bool any_quiver_dest = false;
            int slot_choice = slot;

            throw_slot_menu_active = true;

            for (i = 0; i < INVEN_TOTAL; i++)
                throw_slot_enabled[i] = false;

            {
                object_type* q1_ptr = &inventory[INVEN_QUIVER1];
                bool allow_quiver = true;

                if (q1_ptr->k_idx && cursed_p(q1_ptr)
                    && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
                {
                    allow_quiver = false;
                }

                if (allow_quiver)
                {
                    throw_slot_enabled[INVEN_QUIVER1] = true;
                    any_quiver_dest = true;
                }
            }

            {
                object_type* q2_ptr = &inventory[INVEN_QUIVER2];
                bool allow_quiver = true;

                if (q2_ptr->k_idx && cursed_p(q2_ptr)
                    && !p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
                {
                    allow_quiver = false;
                }

                if (allow_quiver)
                {
                    throw_slot_enabled[INVEN_QUIVER2] = true;
                    any_quiver_dest = true;
                }
            }

            if (!any_quiver_dest)
            {
                msg_print("You have no available quiver slot for those arrows.");
                throw_slot_menu_active = false;
                for (i = 0; i < INVEN_TOTAL; i++)
                    throw_slot_enabled[i] = false;
                return;
            }

            if (!throw_slot_enabled[slot_choice])
            {
                if (throw_slot_enabled[INVEN_QUIVER1])
                    slot_choice = INVEN_QUIVER1;
                else if (throw_slot_enabled[INVEN_QUIVER2])
                    slot_choice = INVEN_QUIVER2;
            }

            item_tester_hook = item_tester_hook_throw_slots;
            item_tester_full = false;

            q = "Place arrows in which quiver? ";
            s = "Oops.";

            bool saved_command_see = p_ptr->command_see;
            byte saved_command_wrk = p_ptr->command_wrk;
            p_ptr->command_see = true;
            p_ptr->command_wrk = (USE_EQUIP);

            bool slot_selected = get_item(&slot_choice, q, s, USE_EQUIP);

            p_ptr->command_see = saved_command_see;
            p_ptr->command_wrk = saved_command_wrk;

            if (!slot_selected)
            {
                item_tester_hook = NULL;
                item_tester_full = false;
                throw_slot_menu_active = false;
                for (i = 0; i < INVEN_TOTAL; i++)
                    throw_slot_enabled[i] = false;
                return;
            }

            item_tester_hook = NULL;
            item_tester_full = false;
            throw_slot_menu_active = false;

            slot = slot_choice;

            if ((slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2))
            {
                if (inventory[slot].k_idx && object_similar(&inventory[slot], o_ptr))
                    combine = true;
            }

            for (i = 0; i < INVEN_TOTAL; i++)
                throw_slot_enabled[i] = false;
        }
    }

    // Check for paired weapons (e.g., Glamdring + Orcrist)
    // Paired weapons can be wielded together without Two Weapon Fighting
    bool paired_weapon_prompt = false;
    if (o_ptr->name1 && inventory[INVEN_WIELD].k_idx)
    {
        int paired_idx = get_paired_artefact(o_ptr->name1);
        if (paired_idx && inventory[INVEN_WIELD].name1 == paired_idx)
        {
            // The weapon we're trying to wield is paired with our main hand weapon
            if (!(k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
                && !(k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF)))
            {
                if (get_check("Wield alongside its mate in your off-hand? "))
                {
                    slot = INVEN_ARM;
                    paired_weapon_prompt = true;
                }
            }
        }
    }

    // Ask about two weapon fighting if necessary
    for (i = 0; i < o_ptr->abilities; i++)
    {
        if ((o_ptr->skilltype[i] == S_MEL)
            && (o_ptr->abilitynum[i] == MEL_TWO_WEAPON)
            && object_known_p(o_ptr))
        {
            grants_two_weapon = true;
        }
    }
    if (!paired_weapon_prompt
        && (p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON] || grants_two_weapon)
        && ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM)
            || (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING)))
    {
        if (!(k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
            && !(k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF)))
        {
            if (get_check("Do you wish to wield it in your off-hand? "))
            {
                slot = INVEN_ARM;
            }
        }
    }

    /* Prevent wielding into a cursed slot */
    if (cursed_p(&inventory[slot]))
    {
        /* Describe it */
        object_desc(o_name, sizeof(o_name), &inventory[slot], false, 0);

        /* Message */
        msg_format("You cannot bear to give up the %s you are %s.", o_name,
            describe_use(slot));

        /* Cancel the command */
        return;
    }

    /* Check if Maedhros character is trying to wield a two-handed weapon */
    if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS))
    {
        msg_print("Your injury prevents you from wielding two-handed weapons.");
        return;
    }

    /* Check if Maedhros character is trying to wield a shield */
    if ((o_ptr->tval == TV_SHIELD)
        && (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS))
    {
        msg_print("Your injury prevents you from using shields.");
        return;
    }

    /* Deal with wielding of two-handed weapons when already using a shield */
    if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && (inventory[INVEN_ARM].k_idx))
    {
        if (cursed_p(&inventory[INVEN_ARM]))
        {
            if (inventory[INVEN_ARM].tval == TV_SHIELD)
            {
                msg_print("You would need to remove your shield, but cannot "
                          "bear to part "
                          "with it.");
            }
            else
            {
                msg_print("You would need to remove your off-hand weapon, but "
                          "cannot bear to "
                          "part with it.");
            }

            /* Cancel the command */
            return;
        }

        // warn about dropping item in left hand
        if ((item < 0) && (&inventory[INVEN_PACK - 1])->tval)
        {
            /* Flush input */
            input_clear_pending();

            if (inventory[INVEN_ARM].tval == TV_SHIELD)
            {
                if (!get_check(
                        "This would require removing (and dropping) your "
                        "shield. Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
            else
            {
                msg_print("This would require removing (and dropping) your "
                          "off-hand weapon.");
                if (!get_check("Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
        }
    }

    /* Deal with wielding of shield or second weapon when already wielding a two
     * handed weapon */
    if ((slot == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_TWO_HANDED)))
    {
        if (cursed_p(&inventory[INVEN_WIELD]))
        {
            msg_print("You would need to put down your weapon, but cannot bear "
                      "to part "
                      "with it.");

            /* Cancel the command */
            return;
        }

        // warn about dropping item in left hand
        if ((item < 0) && (&inventory[INVEN_PACK - 1])->tval)
        {
            /* Flush input */
            input_clear_pending();

            if (inventory[INVEN_ARM].tval == TV_SHIELD)
            {
                if (!get_check(
                        "This would require removing (and dropping) your "
                        "weapon. Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
            else
            {
                msg_print(
                    "This would require removing (and dropping) your weapon.");
                if (!get_check("Proceed? "))
                {
                    /* Cancel the command */
                    return;
                }
            }
        }
    }

    /* Deal with wielding of shield or second weapon when already wielding a
     * hand and a half weapon */
    if ((slot == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_HAND_AND_A_HALF))
        && (!inventory[INVEN_ARM].k_idx))
    {
        weapon_less_effective = true;
    }

    if (smith_oath_forbids_object(o_ptr) && !smith_oath_confirm_break())
        return;

    if (inventory[slot].k_idx && !combine
        && smith_oath_takeoff_hits_pack(&inventory[slot], item)
        && !smith_oath_confirm_break())
    {
        return;
    }

    if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && inventory[INVEN_ARM].k_idx
        && smith_oath_takeoff_hits_pack(&inventory[INVEN_ARM], item)
        && !smith_oath_confirm_break())
    {
        return;
    }

    if ((slot == INVEN_ARM)
        && inventory[INVEN_WIELD].k_idx
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_TWO_HANDED))
        && smith_oath_takeoff_hits_pack(&inventory[INVEN_WIELD], item)
        && !smith_oath_confirm_break())
    {
        return;
    }
    
    /* Oath of Light: warn before equipping shadowed items */
    if (chosen_oath(OATH_LIGHT) && !oath_invalid(OATH_LIGHT))
    {
        object_flags4(o_ptr, &f1, &f2, &f3, &f4);
        if ((f2 & TR2_DARKNESS) || (f4 & TR4_UNLIGHT) || (f3 & TR3_LIGHT_CURSE))
        {
            char* prompt = oath_confirmation_prompt(OATH_LIGHT);
            if (!prompt || !prompt[0]) {
                prompt = "This item will dim your light. Break the Oath of Light?";
            }
            
            if (!get_check_oath_multiline(prompt))
            {
                log_trace("do_cmd_wield: Player declined to break Oath of Light for item (tval=%d, sval=%d)", o_ptr->tval, o_ptr->sval);
                return;
            }
            
            p_ptr->oaths_broken |= OATH_LIGHT_FLAG;
            p_ptr->active_ability[S_SPC][SPC_OATH_LIGHT] = false;
            apply_oath_breaking_curse(OATH_LIGHT);
            metarun_ban_oath(OATH_LIGHT);
            log_trace("do_cmd_wield: Oath of Light broken by equipping shadowed item");
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Obtain local object */
    object_copy(i_ptr, o_ptr);

    bool target_is_quiver = (slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2);

    // Handle quantity differently for arrows or throwing weapons heading to a quiver
    if ((i_ptr->tval == TV_ARROW) || (is_throwing && target_is_quiver))
    {
        if (combine)
        {
            int stack_limit = object_stack_limit(&inventory[slot]);
            quantity = MIN(o_ptr->number,
                stack_limit - (&inventory[slot])->number);
        }
        else
        {
            int stack_limit = object_stack_limit(i_ptr);
            quantity = MIN(o_ptr->number, stack_limit);
        }
    }
    else
    {
        quantity = 1;
    }

    /* Modify quantity */
    i_ptr->number = quantity;

    /* Decrease the item (from the pack) */
    if (item >= 0)
    {
        log_debug(
            "do_cmd_wield: Before decrease - item=%d, k_idx=%d, ego_pfx=%d, ego_sfx=%d, number=%d",
            item, inventory[item].k_idx, object_ego_prefix(&inventory[item]),
            object_ego_suffix(&inventory[item]), inventory[item].number);
        inven_item_increase(item, -quantity);
        inven_item_optimize(item);
        log_debug("do_cmd_wield: After optimize - item=%d, k_idx=%d", 
                  item, inventory[item].k_idx);
    }

    /* Decrease the item (from the floor) */
    else
    {
        floor_item_increase(0 - item, -quantity);
        floor_item_optimize(0 - item);
    }

    /* Get the wield slot */
    o_ptr = &inventory[slot];
    
    log_debug("do_cmd_wield: Wield slot %d - has k_idx=%d, ego_pfx=%d, ego_sfx=%d",
        slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr));

    /* Take off existing item */
    if (o_ptr->k_idx && !combine)
    {
        if (slot == INVEN_LITE && player_light_carry_cap(i_ptr) > 0)
            player_light_reserve_incoming(i_ptr, i_ptr->number);

        log_debug(
            "do_cmd_wield: Taking off existing item from slot %d - k_idx=%d, ego_pfx=%d, ego_sfx=%d",
            slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr));
        /* Take off existing item */
        (void)inven_takeoff(slot, 255);
        player_light_clear_incoming_reservation();
        
        /* Refresh pointer after takeoff */
        o_ptr = &inventory[slot];
        log_debug("do_cmd_wield: After takeoff, slot %d now has k_idx=%d", 
                  slot, o_ptr->k_idx);
    }

    /* Deal with wielding of two-handed weapons when already using a shield */
    if ((k_info[i_ptr->k_idx].flags3 & (TR3_TWO_HANDED))
        && (inventory[INVEN_ARM].k_idx))
    {
        /* Take off shield */
        check_pack_overflow();
        (void)inven_takeoff(INVEN_ARM, 255);
    }

    /* Deal with wielding of shield or second weapon when already wielding a two
     * handed weapon */
    if ((slot == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_TWO_HANDED)))
    {
        /* Stop wielding two handed weapon */
        (void)inven_takeoff(INVEN_WIELD, 255);
    }

    /* Combine the new stuff into the equipment */
    if (combine)
    {
        log_debug(
            "do_cmd_wield: Combining - slot %d has k_idx=%d ego_pfx=%d ego_sfx=%d, adding k_idx=%d ego_pfx=%d ego_sfx=%d",
            slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr),
            i_ptr->k_idx, object_ego_prefix(i_ptr), object_ego_suffix(i_ptr));
        msg_print(
            "You combine them with some that are already in your quiver.");
        object_absorb(o_ptr, i_ptr);
    }
    /* Wear the new stuff */
    else
    {
        log_debug(
            "do_cmd_wield: Copying to slot %d - source k_idx=%d ego_pfx=%d ego_sfx=%d",
            slot, i_ptr->k_idx, object_ego_prefix(i_ptr), object_ego_suffix(i_ptr));
        object_copy(o_ptr, i_ptr);
        log_debug(
            "do_cmd_wield: After copy, slot %d now has k_idx=%d ego_pfx=%d ego_sfx=%d",
            slot, o_ptr->k_idx, object_ego_prefix(o_ptr), object_ego_suffix(o_ptr));
    }

    /* Once the player has equipped an item, remember its combat stats forever. */
    o_ptr->ident |= (IDENT_HANDLED);

    /* Increment the equip counter by hand */
    if (!combine)
        p_ptr->equip_cnt++;

    /* Attempt identification immediately upon equipping (before printing message) */
    {
        bool slot_is_quiver1 = (slot == INVEN_QUIVER1);
        bool slot_is_quiver2 = (slot == INVEN_QUIVER2);
        bool quiver2_grants_bonuses = slot_is_quiver2 && is_throwing;
        bool apply_wield_effects
            = !slot_is_quiver1 && (!slot_is_quiver2 || quiver2_grants_bonuses);

        if (apply_wield_effects)
        {
            ident_on_wield(o_ptr);

            // activate all of its new abilities
            for (i = 0; i < o_ptr->abilities; i++)
            {
                if (!p_ptr->have_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]])
                {
                    p_ptr->have_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]]
                        = true;
                    p_ptr->active_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]]
                        = true;
                }
            }
        }
    }

    /* Where is the item now */
    if ((slot == INVEN_WIELD)
        || ((slot == INVEN_ARM) && (o_ptr->tval != TV_SHIELD)))
    {
        act = "You are wielding";
    }
    else if (slot == INVEN_BOW)
    {
        act = "You are shooting with";
    }
    else if (slot == INVEN_LITE)
    {
        act = "Your light source is";
    }
    else if (slot == INVEN_HORN)
    {
        act = "You are carrying";
    }
    else if ((slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2))
    {
        act = "In your quiver you have";
    }
    else
    {
        act = "You are wearing";
    }

    /* Describe the result */
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Message */
    msg_format("%s %s (%c).", act, o_name, index_to_label(slot));

    /* Play equip sound */
    {
        int equip_sound = get_equip_sound(o_ptr);
        if (equip_sound >= 0)
            sound(equip_sound);
    }

    // Deal with wielding from the floor
    if (item < 0)
    {
        if (target_is_quiver && (quantity < original_quantity)
            && ((i_ptr->tval == TV_ARROW) || is_throwing))
        {
            int floor_idx = 0 - item;
            object_type* floor_ptr = &o_list[floor_idx];

            if (floor_ptr->k_idx && floor_ptr->number > 0)
                py_pickup_aux(floor_idx);
        }

        /* Forget monster */
        o_ptr->held_m_idx = 0;

        /* Forget location */
        o_ptr->iy = o_ptr->ix = 0;

        // Break the truce if picking up an item from the floor
        break_truce(false);

        // Special effects when picking up all the items from the floor
        if (i_ptr->number == original_quantity)
        {
            /* No longer marked */
            o_ptr->marked = false;
        }
    }

    /* Cursed! */
    if (cursed_p(o_ptr))
    {
        /* Warn the player */
        msg_print("You have a bad feeling about this...");

        /* Remove special inscription, if any */
        if (o_ptr->discount >= INSCRIP_NULL)
            o_ptr->discount = 0;

        /* Sense the object if allowed */
        if (o_ptr->discount == 0)
            o_ptr->discount = INSCRIP_CURSED;

        /* The object has been "sensed" */
        o_ptr->ident |= (IDENT_SENSE);
    }

    /* Items with BREAKS_PERMA_CURSE can break the Oath of Feanor on all equipped items */
    {
        u32b o_f1, o_f2, o_f3, o_f4;
        object_flags4(o_ptr, &o_f1, &o_f2, &o_f3, &o_f4);

        if (o_f4 & TR4_BREAKS_PERMA_CURSE)
        {
            int j;
            bool oath_broken = false;

            /* Check all equipped items for the Oath of Feanor (perma-curse) */
            for (j = INVEN_WIELD; j < INVEN_TOTAL; j++)
            {
                object_type *eq_ptr = &inventory[j];
                u32b eq_f1, eq_f2, eq_f3;

                if (!eq_ptr->k_idx) continue;

                object_flags(eq_ptr, &eq_f1, &eq_f2, &eq_f3);

                if ((eq_f3 & TR3_PERMA_CURSE) && cursed_p(eq_ptr))
                {
                    /* Break the curse - the holy light overcomes the oath */
                    eq_ptr->ident &= ~IDENT_CURSED;
                    oath_broken = true;
                }
            }

            if (oath_broken)
            {
                msg_print("The holy light breaks the Oath of Feanor!");
            }
        }
    }

    if (weapon_less_effective)
    {
        /* Describe it */
        object_desc(o_name, sizeof(o_name), &inventory[INVEN_WIELD], false, 0);

        /* Message */
        msg_format(
            "You are no longer able to wield your %s as effectively.", o_name);
    }

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Recalculate mana */
    p_ptr->update |= (PU_MANA);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST | PR_MAP | PR_QUIVER);

    /* Update light display when wielding a light source */
    if (slot == INVEN_LITE)
    {
        p_ptr->redraw |= (PR_LIGHT);
    }

    /* Force immediate sidebar update */
    handle_stuff();
    inven_enforce_current_pack_limits();

    /*
     * Smithing identification checks depend on the player's current effective
     * skills, so retry now that equipped bonuses have been applied.
     */
    if (player_try_identify_smithing_object(o_ptr, true, 0))
    {
        /* Ensure the newly-identified item (and any resulting bonuses) display immediately. */
        handle_stuff();
    }
}

/*
 * Take off an item
 */
void do_cmd_takeoff(object_type* default_o_ptr, int default_item)
{
    int item;
    bool can_break_curse;

    object_type* o_ptr;

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
        q = "Remove which item? ";
        s = "You are not wearing anything to remove.";
        if (!get_item(&item, q, s, (USE_EQUIP)))
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

    can_break_curse = p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING];

    if (((item == INVEN_QUIVER1) || (item == INVEN_QUIVER2)) && cursed_p(o_ptr))
    {
        msg_print("You cannot bear to part with it.");
        return;
    }
    else if (cursed_p(o_ptr) && can_break_curse)
    {
        {
            object_type carry_preview;
            object_copy(&carry_preview, o_ptr);
            carry_preview.ident &= ~(IDENT_CURSED);
            carry_preview.ident |= IDENT_UNCURSED;

            if (carry_preview.discount >= INSCRIP_NULL)
                carry_preview.discount = 0;

            if (smith_oath_forbids_object(o_ptr) && inven_carry_okay(&carry_preview)
                && !smith_oath_confirm_break())
            {
                return;
            }

            /* Message */
            msg_print("With a great strength of will, you break the curse!");

            /* Uncurse the object */
            uncurse_object(o_ptr);
        }
    }
    else if (cursed_p(o_ptr))
    {
        /* Oops */
        msg_print("You cannot bear to part with it.");

        /* Nope */
        return;
    }
    else if (smith_oath_forbids_object(o_ptr) && inven_carry_okay(o_ptr)
        && !smith_oath_confirm_break())
    {
        return;
    }


    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Get unequip sound before taking off (since o_ptr may be modified) */
    int unequip_sound = get_unequip_sound(o_ptr);

    /* Take off the item */
    (void)inven_takeoff(item, 255);

    /* Play unequip sound */
    if (unequip_sound >= 0)
        sound(unequip_sound);

    /* Deal with wielding of shield when already wielding a hand and a half
     * weapon
     */
    if ((item == INVEN_ARM)
        && (k_info[inventory[INVEN_WIELD].k_idx].flags3
            & (TR3_HAND_AND_A_HALF)))
    {
        char o_name[80];

        /* Describe it */
        object_desc(o_name, sizeof(o_name), &inventory[INVEN_WIELD], false, 0);

        /* Message */
        msg_format("You can now wield your %s more effectively.", o_name);
    }

    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST | PR_MAP | PR_QUIVER);

    /* Update light display when removing a light source */
    if (item == INVEN_LITE)
    {
        p_ptr->redraw |= (PR_LIGHT);
    }

    /* Force immediate sidebar update */
    handle_stuff();
    inven_enforce_current_pack_limits();
}

/*
 * Drop an item by index (for enhanced menus)
 */
void do_cmd_drop_item_by_index(int item)
{
    if (item == SUPPLIES_INDEX)
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_DROP, -1, false, true);
        return;
    }

    int amt;
    object_type* o_ptr;

    /* Paranoia */
    if (item < 0 || item >= INVEN_TOTAL)
        return;

    /* Get the item */
    o_ptr = &inventory[item];

    /* Nothing there */
    if (!o_ptr->k_idx)
        return;

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
    p_ptr->energy_use = 50;

    /* Drop (some of) the item */
    inven_drop(item, amt);

    /* Update quiver display if needed */
    p_ptr->redraw |= (PR_QUIVER);

    enhanced_drop_refresh_pending = true;
}

/*
 * Drop an item
 */

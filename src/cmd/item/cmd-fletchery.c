/* File: cmd-fletchery.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "item_set.h"
#include "log/log.h"
#include "object/object-ui-enhanced.h"
#include "object/object-ui-select.h"
#include "player/killer.h"
#include "metarun.h"

#define MIN_DEPTH_COUNTER_STEP 180000
#define MIN_DEPTH_BASE_INCREMENT_START 85
#define MIN_DEPTH_BASE_INCREMENT_DIVISOR 850
#define MIN_DEPTH_INCREMENT_PER_BONUS 3

static bool item_tester_hook_fletchery_source(const object_type* o_ptr)
{
    if (!o_ptr)
        return false;

    if (o_ptr->tval == TV_ARROW)
    {
        if (o_ptr->name1 || object_has_ego(o_ptr) || o_ptr->att > 0)
            return false;
        return true;
    }

    if (o_ptr->tval == TV_LIGHT
        && (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN))
    {
        if (o_ptr->name1 || object_has_ego(o_ptr))
            return false;
        return true;
    }

    if (o_ptr->tval == TV_STAFF)
    {
        if (o_ptr->name1 || object_has_ego(o_ptr))
            return false;
        return true;
    }

    return false;
}
enum fletch_source_type
{
    FLETCH_SOURCE_INVEN = 0,
    FLETCH_SOURCE_EQUIP = 1,
    FLETCH_SOURCE_FLOOR = 2
};

typedef struct fletch_choice_s
{
    enum fletch_source_type type;
    int index;
} fletch_choice_t;

static void distribute_fletchered_arrows(const object_type* arrows)
{
    if (!arrows || arrows->number <= 0 || arrows->k_idx == 0)
        return;

    object_type leftover = *arrows;
    bool combined_existing = false;

    /* Try to top up quiver slots first */
    for (int slot = INVEN_QUIVER1; slot <= INVEN_QUIVER2 && leftover.number > 0; slot++)
    {
        object_type* slot_obj = &inventory[slot];
        if (!slot_obj->k_idx)
            continue;
        if (!object_similar(slot_obj, &leftover))
            continue;
        int before = leftover.number;
        object_absorb(slot_obj, &leftover);
        if (leftover.number != before)
            combined_existing = true;
    }

    /* Then fill stacks in the main pack */
    for (int slot = 0; slot < INVEN_PACK && leftover.number > 0; slot++)
    {
        object_type* slot_obj = &inventory[slot];
        if (!slot_obj->k_idx)
            continue;
        if (!object_similar(slot_obj, &leftover))
            continue;
        int before = leftover.number;
        object_absorb(slot_obj, &leftover);
        if (leftover.number != before)
            combined_existing = true;
    }

    /* Finally, attempt to add to any other equipped stacks */
    for (int slot = INVEN_WIELD; slot < INVEN_TOTAL && leftover.number > 0; slot++)
    {
        if (slot >= INVEN_QUIVER1 && slot <= INVEN_QUIVER2)
            continue;
        object_type* slot_obj = &inventory[slot];
        if (!slot_obj->k_idx)
            continue;
        if (!object_similar(slot_obj, &leftover))
            continue;
        int before = leftover.number;
        object_absorb(slot_obj, &leftover);
        if (leftover.number != before)
            combined_existing = true;
    }

    if (combined_existing)
    {
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }

    if (leftover.number <= 0)
        return;

    object_type carry_obj = leftover;
    int carry_slot = inven_carry(&carry_obj, true);

    if (carry_slot == SUPPLIES_INDEX)
    {
        char arrow_name[80];
        object_desc(arrow_name, sizeof(arrow_name), &carry_obj, true, 3);
        char label = supplies_label_char();
        if (!label)
            label = 'a';
        msg_format("You add %s to your supplies (%c).", arrow_name, label);
    }
    else if (carry_slot >= 0)
    {
        object_type* carried = &inventory[carry_slot];
        char arrow_name[80];
        object_desc(arrow_name, sizeof(arrow_name), carried, true, 3);
        msg_format("You have %s (%c).", arrow_name, index_to_label(carry_slot));

        if (carry_obj.number > 0)
        {
            drop_near(&carry_obj, 0, p_ptr->py, p_ptr->px);
            msg_print("Some arrows spill to the ground.");
        }
    }
    else
    {
        drop_near(&carry_obj, 0, p_ptr->py, p_ptr->px);
        msg_print("Your pack is too full; you leave the arrows on the ground.");
    }

    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
}

static bool fletchery_choose_source(fletch_choice_t* out_choice)
{
    extern int enhanced_menu_action;
    extern int enhanced_inventory_selected_item;
    extern char current_menu_command;

    /* Prepare inventory menu to include equipment */
    inventory_menu_set_include_equip(true);

    bool old_full = item_tester_full;
    bool old_command_see = p_ptr->command_see;
    int old_command_wrk = p_ptr->command_wrk;
    char old_menu_command = current_menu_command;

    /* Only show fletchery candidates */
    item_tester_full = false;
    item_tester_hook = item_tester_hook_fletchery_source;
    p_ptr->command_wrk = (USE_INVEN);
    p_ptr->command_see = true;
    current_menu_command = 0;

    enhanced_menu_action = ENHANCED_ACTION_NONE;
    enhanced_inventory_selected_item = -1;

    screen_save();
    show_inven_enhanced();
    screen_load();

    inventory_menu_set_include_equip(false);

    item_tester_hook = NULL;
    item_tester_full = old_full;
    p_ptr->command_see = old_command_see;
    p_ptr->command_wrk = old_command_wrk;
    current_menu_command = old_menu_command;

    int action = enhanced_menu_action;
    int selection = enhanced_inventory_selected_item;

    enhanced_menu_action = ENHANCED_ACTION_NONE;
    enhanced_inventory_selected_item = -1;

    if (action != ENHANCED_ACTION_USE || selection == -1)
        return false;

    if (selection == SUPPLIES_INDEX)
    {
        msg_print("Supplies cannot be used for fletchery.");
        return false;
    }

    if (selection < 0)
    {
        out_choice->type = FLETCH_SOURCE_FLOOR;
        out_choice->index = 0 - selection;
    }
    else if (selection >= INVEN_WIELD)
    {
        out_choice->type = FLETCH_SOURCE_EQUIP;
        out_choice->index = selection;
    }
    else
    {
        out_choice->type = FLETCH_SOURCE_INVEN;
        out_choice->index = selection;
    }

    return true;
}

void do_cmd_fletchery(void)
{
    object_type* o_ptr;
    fletch_choice_t choice;

    if (!p_ptr->active_ability[S_ARC][ARC_FLETCHERY])
    {
        msg_print("You need the ability 'fletchery' to use this command.");
        return;
    }

    if (!fletchery_choose_source(&choice))
        return;

    bool from_floor = (choice.type == FLETCH_SOURCE_FLOOR);

    int source_index = choice.index;
    int floor_idx = from_floor ? source_index : 0;

    if (from_floor)
        o_ptr = &o_list[floor_idx];
    else
        o_ptr = &inventory[source_index];

    bool is_arrow = (o_ptr->tval == TV_ARROW);
    bool is_torch = (o_ptr->tval == TV_LIGHT)
        && (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN);
    bool is_staff = (o_ptr->tval == TV_STAFF);

    if (is_arrow)
    {
        if (from_floor)
        {
            msg_print("You need to pick up those arrows before you can work on them.");
            return;
        }

        /* Take a turn */
        p_ptr->energy_use = 100;

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;

        msg_print(
            "You begin straightening and adjusting the feathering of the arrows.");

        p_ptr->fletch_item = source_index;
        p_ptr->fletching = o_ptr->number;
        return;
    }

    if (is_torch || is_staff)
    {
        int max_convert = o_ptr->number;
        if (max_convert <= 0)
        {
            msg_print("You have nothing to work with.");
            return;
        }

        int amount = get_quantity("Convert how many?", max_convert);
        if (amount <= 0)
            return;

        /* Take a turn */
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0] = ACTION_MISC;

        object_type source = *o_ptr;
        source.number = amount;

        char source_name[80];
        object_desc(source_name, sizeof(source_name), &source, true, 3);

        int arrows_per = is_staff ? 6 : 3;
        int produced_total = amount * arrows_per;

        msg_format("You carve %d +3 arrow%s from %s.", produced_total,
            (produced_total == 1) ? "" : "s", source_name);

        /* Remove the raw materials */
        if (from_floor)
        {
            floor_item_increase(floor_idx, -amount);
            floor_item_optimize(floor_idx);
        }
        else
        {
            inven_item_increase(source_index, -amount);
            inven_item_optimize(source_index);
        }

        object_type arrow_proto;
        object_prep(&arrow_proto, lookup_kind(TV_ARROW, SV_NORMAL_ARROW));
        arrow_proto.number = produced_total;
        arrow_proto.att = 3;

        distribute_fletchered_arrows(&arrow_proto);
        return;
    }

    msg_print("That item cannot be used for fletchery.");

}
void finish_fletching(int turns_left)
{
    object_type* o_ptr = &inventory[p_ptr->fletch_item];
    int count = o_ptr->number - turns_left;

    /* Unstack if necessary */
    if (count > 0)
    {
        /* Message */
        msg_format("You improve %d arrows.", count);

        object_type* i_ptr;
        object_type object_type_body;

        /* Get local object */
        i_ptr = &object_type_body;

        /* Obtain a local object */
        object_copy(i_ptr, o_ptr);

        /* Modify quantity */
        i_ptr->number = count;
        i_ptr->att = 3;

        /* Reduce original pile */
        inven_item_increase(p_ptr->fletch_item, -count);
        inven_item_optimize(p_ptr->fletch_item);

        /* Add new arrows */
        distribute_fletchered_arrows(i_ptr);
    }
    else
    {
        msg_print("You did not manage to improve any arrows.");
    }

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN);

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP);
}

/*
 * Determine if a given grid may be "tunneled"
 */

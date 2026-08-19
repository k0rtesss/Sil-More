#include "angband.h"
#include "externs.h"
#include "object/object-internal.h"
#include "object/object-ui-select.h"

static bool reject_broken_item_use(const object_type* o_ptr)
{
    if (!object_has_broken_prefix(o_ptr))
        return false;

    msg_print("Broken items must be repaired before they can be used.");
    return true;
}

typedef enum understanding_gem_source_type
{
    UNDERSTANDING_GEM_SOURCE_NONE,
    UNDERSTANDING_GEM_SOURCE_PACK,
    UNDERSTANDING_GEM_SOURCE_SUPPLIES
} understanding_gem_source_type;

typedef struct understanding_gem_source
{
    understanding_gem_source_type type;
    object_type* o_ptr;
    int index;
} understanding_gem_source;

static bool understanding_gem_matches(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && o_ptr->number > 0
        && o_ptr->tval == TV_GEM && o_ptr->sval == SV_GEM_UNDERSTANDING
        && object_aware_p(o_ptr) && !object_has_broken_prefix(o_ptr);
}

static int carried_understanding_gem_count(
    understanding_gem_source* first_source)
{
    int count = 0;

    if (first_source)
        *first_source = (understanding_gem_source){
            UNDERSTANDING_GEM_SOURCE_NONE, NULL, -1
        };

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);

        if (!understanding_gem_matches(o_ptr))
            continue;

        if (first_source
            && first_source->type == UNDERSTANDING_GEM_SOURCE_NONE)
        {
            first_source->type = UNDERSTANDING_GEM_SOURCE_SUPPLIES;
            first_source->o_ptr = o_ptr;
            first_source->index = i;
        }
        count += o_ptr->number;
    }

    for (int ordinal = 0; ordinal < player_pack_entry_count(); ordinal++)
    {
        int item = player_pack_entry_handle_at(ordinal);
        object_type* o_ptr = player_inventory_object(item);

        if (!understanding_gem_matches(o_ptr))
            continue;

        if (first_source
            && first_source->type == UNDERSTANDING_GEM_SOURCE_NONE)
        {
            first_source->type = UNDERSTANDING_GEM_SOURCE_PACK;
            first_source->o_ptr = o_ptr;
            first_source->index = item;
        }
        count += o_ptr->number;
    }

    return count;
}

static bool understanding_gem_target(const object_type* viewed_o_ptr,
    object_type** target_o_ptr, int* target_item)
{
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    if (target_o_ptr)
        *target_o_ptr = NULL;
    if (target_item)
        *target_item = 0;

    if (!viewed_o_ptr || !viewed_o_ptr->k_idx
        || !object_is_unidentified_for_display(viewed_o_ptr))
    {
        return false;
    }

    {
        int item = player_inventory_handle_for_object(viewed_o_ptr);

        if (item >= 0)
        {
            if (target_o_ptr)
                *target_o_ptr = player_inventory_object(item);
            if (target_item)
                *target_item = item;
            return true;
        }
    }

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);

        if (viewed_o_ptr != o_ptr)
            continue;

        if (target_o_ptr)
            *target_o_ptr = o_ptr;
        if (target_item)
            *target_item = SUPPLIES_INDEX + i;
        return true;
    }

    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px,
        0x00);
    for (int i = 0; i < floor_num; i++)
    {
        int o_idx = floor_list[i];

        if (o_idx <= 0 || o_idx >= o_max || viewed_o_ptr != &o_list[o_idx])
            continue;

        if (target_o_ptr)
            *target_o_ptr = &o_list[o_idx];
        if (target_item)
            *target_item = 0 - o_idx;
        return true;
    }

    return false;
}

int understanding_gem_count_for_item_description(
    const object_type* viewed_o_ptr)
{
    if (!character_dungeon || !p_ptr || !p_ptr->playing || p_ptr->is_dead)
        return 0;

    if (!understanding_gem_target(viewed_o_ptr, NULL, NULL))
        return 0;

    return carried_understanding_gem_count(NULL);
}

bool do_cmd_use_understanding_gem_on_item(const object_type* viewed_o_ptr)
{
    understanding_gem_source source;
    object_type* target_o_ptr;
    int target_item;

    if (!character_dungeon || !p_ptr || !p_ptr->playing || p_ptr->is_dead)
        return false;
    if (!understanding_gem_target(viewed_o_ptr, &target_o_ptr, &target_item))
        return false;
    if (carried_understanding_gem_count(&source) <= 0 || !source.o_ptr)
        return false;

    sound(MSG_USE_GEM);
    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = ACTION_MISC;

    do_ident_item(target_item, target_o_ptr);
    break_truce(false);

    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    object_tried(source.o_ptr);
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
    source.o_ptr->xtra1++;

    if (source.type == UNDERSTANDING_GEM_SOURCE_SUPPLIES)
    {
        supplies_consume_quantity(source.index, 1);
        supplies_refresh_entry(source.index);
    }
    else if (source.type == UNDERSTANDING_GEM_SOURCE_PACK)
    {
        inven_item_increase(source.index, -1);
        inven_item_describe(source.index);
        inven_item_optimize(source.index);
    }

    return true;
}

static byte harness_activatable_tval = 0;

static int carried_inventory_index(const object_type* o_ptr)
{
    return player_inventory_handle_for_object(o_ptr);
}

static bool item_tester_hook_harness_activatable(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != harness_activatable_tval)
        return false;

    if (carried_inventory_index(o_ptr) < 0)
        return false;

    return inventory_limit_group_for_object(o_ptr) == INV_LIMIT_HARNESS;
}

static bool choose_harness_activatable(byte tval, cptr prompt, cptr none_msg,
    object_type** chosen_o_ptr, int* chosen_item)
{
    byte old_item_tester_tval = item_tester_tval;
    bool (*old_item_tester_hook)(const object_type*) = item_tester_hook;
    bool old_item_tester_full = item_tester_full;
    int count = 0;
    int item = -1;
    bool picked;

    if (chosen_o_ptr)
        *chosen_o_ptr = NULL;
    if (chosen_item)
        *chosen_item = -1;

    for (int ordinal = 0; ordinal < player_pack_entry_count(); ordinal++)
    {
        int handle = player_pack_entry_handle_at(ordinal);
        object_type* o_ptr = player_inventory_object(handle);

        if (!o_ptr->k_idx || o_ptr->tval != tval
            || inventory_limit_group_for_object(o_ptr) != INV_LIMIT_HARNESS)
        {
            continue;
        }

        count++;
        item = handle;
    }

    /* The retired slots can only be populated transiently by an old save. */
    for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx || o_ptr->tval != tval
            || inventory_limit_group_for_object(o_ptr) != INV_LIMIT_HARNESS)
        {
            continue;
        }

        count++;
        item = i;
    }

    if (count == 0)
    {
        msg_print(none_msg);
        return false;
    }

    if (count > 1)
    {
        harness_activatable_tval = tval;
        item_tester_tval = 0;
        item_tester_hook = item_tester_hook_harness_activatable;
        item_tester_full = false;

        picked = open_inventory_item_select_menu(USE_INVEN | USE_EQUIP,
            prompt, none_msg, &item);

        harness_activatable_tval = 0;
        item_tester_tval = old_item_tester_tval;
        item_tester_hook = old_item_tester_hook;
        item_tester_full = old_item_tester_full;

        if (!picked)
            return false;
    }

    if (!player_inventory_handle_valid(item))
        return false;

    if (chosen_o_ptr)
        *chosen_o_ptr = player_inventory_object(item);
    if (chosen_item)
        *chosen_item = item;

    return true;
}

static void msg_print_object_identified(const object_type* o_ptr)
{
    char o_name[80];
    object_desc(o_name, sizeof(o_name), o_ptr, true, 0);
    msg_format("You identify %s.", o_name);
}

static const object_type* sanctity_target_excluded = NULL;

typedef struct sanctity_target_entry
{
    int item;
    object_type* o_ptr;
} sanctity_target_entry;

static bool item_tester_hook_sanctity_target(const object_type* o_ptr)
{
    bool can_remove_jinx;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr == sanctity_target_excluded)
        return false;

    can_remove_jinx = p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING]
        && object_has_ego_flag4(o_ptr, TR4_JINX);

    if (!cursed_p(o_ptr)
        && ((o_ptr->ident & IDENT_UNCURSED)
            || (o_ptr->discount == INSCRIP_UNCURSED))
        && !can_remove_jinx)
        return false;

    if (cursed_p(o_ptr))
        return true;

    if (can_remove_jinx)
        return true;

    if (!object_known_p(o_ptr))
        return true;

    return false;
}

static int sanctity_collect_targets(sanctity_target_entry entries[],
    int max_entries, const object_type* gem_o_ptr)
{
    int count = 0;
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    if (!entries || max_entries <= 0)
        return 0;

    sanctity_target_excluded = gem_o_ptr;

    for (int ordinal = 0;
         ordinal < player_pack_entry_count() && count < max_entries;
         ordinal++)
    {
        int item = player_pack_entry_handle_at(ordinal);
        object_type* o_ptr = player_inventory_object(item);

        if (!item_tester_hook_sanctity_target(o_ptr))
            continue;

        entries[count].item = item;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    for (int i = INVEN_WIELD; i < INVEN_TOTAL && count < max_entries; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!item_tester_hook_sanctity_target(o_ptr))
            continue;

        entries[count].item = i;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);
    for (int i = 0; i < floor_num && count < max_entries; i++)
    {
        int o_idx = floor_list[i];
        object_type* o_ptr = &o_list[o_idx];

        if (!item_tester_hook_sanctity_target(o_ptr))
            continue;

        entries[count].item = 0 - o_idx;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    sanctity_target_excluded = NULL;
    return count;
}

static bool sanctity_choose_target_from_entries(
    const sanctity_target_entry entries[], int count, int* out_item)
{
    object_choice_entry* choices;
    int selected = -1;
    char desc[80];

    if (!entries || count <= 0 || !out_item)
        return false;

    choices = mem_alloc_array(count, object_choice_entry);

    for (int i = 0; i < count; i++)
    {
        char label[6];

        strnfmt(label, sizeof(label), "%c)", index_to_label(i));
        object_choice_entry_make(&choices[i], entries[i].item,
            entries[i].o_ptr, label,
            (entries[i].item < 0) ? "On floor"
                                  : mention_use(entries[i].item));
    }

    strnfmt(desc, sizeof(desc), "%d eligible sanctity target%s", count,
        (count == 1) ? "" : "s");
    if (!object_choice_overlay("Cleanse which item?", desc, choices, count, 0,
            &selected))
    {
        choices = mem_free(choices);
        return false;
    }

    if (selected < 0 || selected >= count)
    {
        choices = mem_free(choices);
        return false;
    }

    *out_item = entries[selected].item;
    choices = mem_free(choices);
    return true;
}

static bool sanctity_choose_target(const object_type* gem_o_ptr,
    object_type** target_o_ptr)
{
    int chosen_item;
    int count;
    int capacity = player_pack_entry_count()
        + (INVEN_TOTAL - INVEN_WIELD) + MAX_FLOOR_STACK;
    sanctity_target_entry* entries;

    if (!target_o_ptr)
        return false;

    entries = mem_alloc_array(MAX(capacity, 1), sanctity_target_entry);
    count = sanctity_collect_targets(entries, capacity, gem_o_ptr);
    if (count <= 0)
    {
        msg_print("You have no target to cleanse.");
        entries = mem_free(entries);
        return false;
    }

    if (!sanctity_choose_target_from_entries(entries, count, &chosen_item))
    {
        entries = mem_free(entries);
        return false;
    }

    *target_o_ptr = inventory_item_to_object_ptr(chosen_item);
    entries = mem_free(entries);
    return ((*target_o_ptr != NULL) && (*target_o_ptr)->k_idx);
}

/*
 * This file includes code for eating food, drinking potions,
 * using staffs, playing instruments, and activating artefacts.
 *
 * In all cases, if the player becomes "aware" of the item's use
 * by testing it, mark it as "aware" and reward some experience
 * based on the object's level, always rounding up.  If the player
 * remains "unaware", mark that object "kind" as "tried".
 *
 * Note the overly paranoid warning about potential pack
 * overflow, which allows the player to use and drop a stacked item.
 *
 * In all "unstacking" scenarios, the "used" object is "carried" as if
 * the player had just picked it up.  In particular, this means that if
 * the use of an item induces pack overflow, that item will be dropped.
 *
 * For simplicity, these routines induce a full "pack reorganization"
 * which not only combines similar items, but also reorganizes various
 * items to obey the current "sorting" method.  This may require about
 * 400 item comparisons, but only occasionally.
 *
 * There may be a BIG problem with any "effect" that can cause "changes"
 * to the inventory.  For example, a "scroll of recharging" used to be
 * able to cause a staff to "disappear", moving the inventory up.  Luckily, the
 * scrolls all appear BEFORE the staffs/wands, so this is not a problem.
 * But, for example, a "staff of recharging" could cause MAJOR problems.
 * In such a case, it will be best to either (1) "postpone" the effect
 * until the end of the function, or (2) "change" the effect, say, into
 * giving a staff "negative" charges, or "turning a staff into a stick".
 * It seems as though a "rod of recharging" might in fact cause problems.
 * The basic problem is that the act of recharging (and destroying) an
 * item causes the inducer of that action to "move", causing "o_ptr" to
 * no longer point at the correct item, with horrifying results.
 *
 * Note that food/potions/scrolls no longer use bit-flags for effects,
 * but instead use the "sval" (which is also used to sort the objects).
 */

/*
 * Eat some food (from the pack or floor)
 */
void do_cmd_eat_food(object_type* default_o_ptr, int default_item)
{
    int item;
    bool ident;
    bool aware;
    int kind_index;

    object_type* o_ptr = NULL;
    int supply_index = supplies_current_action();
    bool from_supplies = (supply_index >= 0);
    cptr q, s;

    /* Use specified item if possible */
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = from_supplies ? SUPPLIES_INDEX : default_item;
    }
    /* Get an item */
    else
    {
        /* Restrict choices to food */
        item_tester_tval = TV_FOOD;

        /* Get an item */
        q = "Eat which item? ";
        s = "You have nothing to eat.";
        supplies_set_pending_action(SUPPLY_MENU_ACTION_USE,
            supplies_has_group(SUPPLY_GROUP_HERBS) ? SUPPLY_GROUP_HERBS
                                                   : SUPPLY_GROUP_FOOD,
            true);
        if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR, q, s,
                &item))
        {
            supplies_clear_pending_action();
            return;
        }

        if (item == SUPPLIES_INDEX)
        {
            supplies_clear_pending_action();
            open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE,
                supplies_has_group(SUPPLY_GROUP_HERBS) ? SUPPLY_GROUP_HERBS
                                                       : SUPPLY_GROUP_FOOD,
                true, true);
            return;
        }

        supplies_clear_pending_action();

        /* Get the item (in the pack) */
        if (item >= SUPPLIES_INDEX)
        {
            supply_index = item - SUPPLIES_INDEX;
            o_ptr = supplies_entry_at(supply_index);
            from_supplies = true;
        }
        else if (player_inventory_handle_valid(item))
        {
            o_ptr = player_inventory_object(item);
            from_supplies = false;
            supply_index = -1;
        }

        /* Get the item (on the floor) */
        else
        {
            o_ptr = &o_list[0 - item];
        }

    }

    if (!o_ptr)
        return;

    if (player_pack_action_start(PLAYER_PACK_ACTION_EAT, item, 0, false,
            o_ptr))
        return;

    if (reject_broken_item_use(o_ptr))
        return;

    /* Sound */
    sound(MSG_EAT);

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Identity not known yet */
    ident = false;

    // Save the k_idx and awareness info
    kind_index = o_ptr->k_idx;
    aware = object_aware_p(o_ptr);

    /* Eat the food */
    use_object(o_ptr, &ident);

    /* We have tried it */
    object_tried(o_ptr);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* The player is now aware of the object */
    if (ident && !object_aware_p(o_ptr))
    {
        object_aware(o_ptr);
        msg_print_object_identified(o_ptr);
    }

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Destroy a food in the pack or supplies */
    if (from_supplies && supply_index >= 0)
    {
        supplies_consume_quantity(supply_index, 1);
        supplies_refresh_entry(supply_index);
    }
    else if (item >= 0)
    {
        inven_item_increase(item, -1);
        inven_item_describe(item);
        inven_item_optimize(item);
    }

    /* Destroy a food on the floor */
    else
    {
        floor_item_increase(0 - item, -1);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }

    // allow autoinscribing of the herb
    if (!ident && !aware)
    {
        if (easter_time())
        {
            if (get_check("Autoinscribe this easter egg type? "))
            {
                do_cmd_autoinscribe_item(kind_index);
            }
        }
        else
        {
            if (get_check((o_ptr->sval <= SV_FOOD_SICKNESS)
                    ? "Autoinscribe this herb type? "
                    : "Autoinscribe this food type? "))
            {
                do_cmd_autoinscribe_item(kind_index);
            }
        }
    }
}

/*
 * Quaff a potion (from the pack or the floor)
 */
void do_cmd_quaff_potion(object_type* default_o_ptr, int default_item)
{
    int item;
    bool ident;
    bool aware;
    int kind_index;
    object_type* o_ptr = NULL;
    int supply_index = supplies_current_action();
    bool from_supplies = (supply_index >= 0);
    cptr q, s;

    /* Use specified item if possible */
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = from_supplies ? SUPPLIES_INDEX : default_item;
    }
    /* Get an item */
    else
    {
        /* Restrict choices to potions */
        item_tester_tval = TV_POTION;

        /* Get an item */
        q = "Quaff which potion? ";
        s = "You have no potions to quaff.";
        supplies_set_pending_action(SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_POTIONS, true);
        if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR, q, s,
                &item))
        {
            supplies_clear_pending_action();
            return;
        }

        if (item == SUPPLIES_INDEX)
        {
            supplies_clear_pending_action();
            open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_POTIONS, true, true);
            return;
        }

        supplies_clear_pending_action();

        /* Get the item (in the pack) */
        if (item >= SUPPLIES_INDEX)
        {
            supply_index = item - SUPPLIES_INDEX;
            o_ptr = supplies_entry_at(supply_index);
            from_supplies = true;
        }
        else if (player_inventory_handle_valid(item))
        {
            o_ptr = player_inventory_object(item);
            from_supplies = false;
            supply_index = -1;
        }

        /* Get the item (on the floor) */
        else
        {
            o_ptr = &o_list[0 - item];
        }

    }

    if (!o_ptr)
        return;

    if (player_pack_action_start(PLAYER_PACK_ACTION_QUAFF, item, 0, false,
            o_ptr))
        return;

    if (reject_broken_item_use(o_ptr))
        return;

    /* Sound */
    sound(MSG_QUAFF);

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Not identified yet */
    ident = false;

    // Save the k_idx and awareness info
    kind_index = o_ptr->k_idx;
    aware = object_aware_p(o_ptr);

    /* Quaff the potion */
    use_object(o_ptr, &ident);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* The item has been tried */
    object_tried(o_ptr);

    /* An identification was made */
    if (ident && !object_aware_p(o_ptr))
    {
        object_aware(o_ptr);
        msg_print_object_identified(o_ptr);
    }

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Destroy a potion in the pack or supplies */
    if (from_supplies && supply_index >= 0)
    {
        supplies_consume_quantity(supply_index, 1);
        supplies_refresh_entry(supply_index);
    }
    else if (item >= 0)
    {
        inven_item_increase(item, -1);
        inven_item_describe(item);
        inven_item_optimize(item);
    }

    /* Destroy a potion on the floor */
    else
    {
        floor_item_increase(0 - item, -1);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }

    // allow autoinscribing of the potion
    if (!ident && !aware)
    {
        if (get_check("Autoinscribe this potion type? "))
        {
            do_cmd_autoinscribe_item(kind_index);
        }
    }
}

/*
 * Play an instrument
 */
void do_cmd_play_instrument(object_type* default_o_ptr, int default_item)
{
    bool ident;

    object_type* o_ptr = NULL;

    /* Use specified item if possible */
    if (default_o_ptr != NULL)
    {
        int carried_item = carried_inventory_index(default_o_ptr);

        o_ptr = default_o_ptr;

        if (default_item < 0 || carried_item < 0)
        {
            msg_print("Pick up the horn before sounding it.");
            return;
        }
    }
    /* Choose any carried horn from the Harness. */
    else
    {
        if (!choose_harness_activatable(TV_HORN,
                "Sound which horn?", "You have no horn in your Harness.",
                &o_ptr, NULL))
        {
            return;
        }
    }

    if (!o_ptr)
        return;

    if (o_ptr->tval != TV_HORN)
    {
        msg_print("You can only sound a horn.");
        return;
    }

    if (player_pack_action_start(PLAYER_PACK_ACTION_PLAY,
            carried_inventory_index(o_ptr), 0, false, o_ptr))
        return;

    if (reject_broken_item_use(o_ptr))
        return;

    /* Not identified yet */
    ident = false;

    /* Play the instrument */
    if (!use_object(o_ptr, &ident))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    // end the current song
    change_song(SNG_NOTHING);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Tried the object */
    object_tried(o_ptr);

    /* Experiencing effects helps identify smithing-difficulty items, but does not auto-ID them. */
    if (ident)
    {
        if (object_uses_smithing_difficulty(o_ptr))
        {
            player_mark_object_experienced(o_ptr);
        }
        else if (!object_aware_p(o_ptr))
        {
            object_aware(o_ptr);
            msg_print_object_identified(o_ptr);
        }
    }

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
}

/*
 * Use a staff
 *
 * One charge of one staff disappears.
 *
 * Hack -- staffs of identify can be "cancelled".
 */
void do_cmd_activate_staff(object_type* default_o_ptr, int default_item)
{
    int item;

    bool ident;

    object_type* o_ptr = NULL;

    bool use_charge;

    /* Use specified item if possible */
    if (default_o_ptr != NULL)
    {
        int carried_item = carried_inventory_index(default_o_ptr);

        o_ptr = default_o_ptr;

        if (default_item < 0 || carried_item < 0)
        {
            msg_print("Pick up the staff before activating it.");
            return;
        }

        item = carried_item;
    }
    /* Choose any carried staff from the Harness. */
    else
    {
        if (!choose_harness_activatable(TV_STAFF,
                "Activate which staff?", "You have no staff in your Harness.",
                &o_ptr, &item))
        {
            return;
        }
    }

    if (!o_ptr)
        return;

    if (o_ptr->tval != TV_STAFF)
    {
        msg_print("You can only activate a staff.");
        return;
    }

    if (player_pack_action_start(PLAYER_PACK_ACTION_ACTIVATE_STAFF, item, 0,
            false, o_ptr))
        return;

    if (reject_broken_item_use(o_ptr))
        return;

    if (o_ptr->ident & (IDENT_EMPTY))
    {
        msg_print("The staff has no charges left.");
        return;
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Not identified yet */
    ident = false;

    /* Notice empty staffs */
    if (o_ptr->pval < CHANNELING_CHARGE_MULTIPLIER)
    {
        flush();
        msg_print("The staff has no charges left.");
        o_ptr->ident |= (IDENT_EMPTY);
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        p_ptr->window |= (PW_INVEN);
        return;
    }

    /* Sound */
    sound(MSG_ZAP);

    /* Use the staff */
    use_charge = use_object(o_ptr, &ident);

    // Break the truce
    break_truce(false);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Tried the item */
    object_tried(o_ptr);

    /* An identification was made */
    if (ident && !object_aware_p(o_ptr))
    {
        object_aware(o_ptr);
        msg_print_object_identified(o_ptr);
    }

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Hack -- some uses are "free" */
    if (!use_charge)
        return;

    /* Consume the item */
    /* Staffs always expend their bundled charges */
    o_ptr->pval -= CHANNELING_CHARGE_MULTIPLIER;
    if (o_ptr->pval < 0)
        o_ptr->pval = 0;
    // mark times used
    o_ptr->xtra1++;

    if (item >= 0)
    {
        inven_item_charges(item);
    }
}

/*
 * Use a gem
 *
 * One gem is consumed on use.
 */
void do_cmd_use_gem(object_type* default_o_ptr, int default_item)
{
    int item;
    bool ident;
    object_type* o_ptr = NULL;
    object_type* sanctity_target_o_ptr = NULL;
    bool use_charge;

    int supply_index = supplies_current_action();
    bool from_supplies = (supply_index >= 0);
    cptr q, s;

    /* Use specified item if possible */
    if (default_o_ptr != NULL)
    {
        o_ptr = default_o_ptr;
        item = from_supplies ? SUPPLIES_INDEX : default_item;
    }
    /* Get an item */
    else
    {
        /* Restrict choices to gems */
        item_tester_tval = TV_GEM;

        /* Get an item */
        q = "Use which gem? ";
        s = "You have no gems to use.";
        supplies_set_pending_action(SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_GEMS, true);
        if (!open_inventory_item_select_menu(USE_INVEN | USE_FLOOR, q, s,
                &item))
        {
            supplies_clear_pending_action();
            return;
        }

        if (item == SUPPLIES_INDEX)
        {
            supplies_clear_pending_action();
            open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_GEMS, true, true);
            return;
        }

        supplies_clear_pending_action();

        /* Get the item (in the pack) */
        if (item >= SUPPLIES_INDEX)
        {
            supply_index = item - SUPPLIES_INDEX;
            o_ptr = supplies_entry_at(supply_index);
            from_supplies = true;
        }
        else if (player_inventory_handle_valid(item))
        {
            o_ptr = player_inventory_object(item);
            from_supplies = false;
            supply_index = -1;
        }

        /* Get the item (on the floor) */
        else
        {
            o_ptr = &o_list[0 - item];
        }

    }

    if (!o_ptr)
        return;

    if (player_pack_action_start(PLAYER_PACK_ACTION_USE_GEM, item, 0, false,
            o_ptr))
        return;

    if (reject_broken_item_use(o_ptr))
        return;

    if (o_ptr->number <= 0)
    {
        msg_print("You have no gems left.");
        return;
    }

    if (o_ptr->sval == SV_GEM_SANCTITY)
    {
        if (!sanctity_choose_target(o_ptr, &sanctity_target_o_ptr))
        {
            return;
        }
    }

    /* Sound */
    sound(MSG_USE_GEM);

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Not identified yet */
    ident = false;

    /* Use the gem */
    if (o_ptr->sval == SV_GEM_SANCTITY)
        use_charge = use_sanctity_gem_on(sanctity_target_o_ptr, &ident);
    else
        use_charge = use_object(o_ptr, &ident);

    // Break the truce
    break_truce(false);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Tried the item */
    object_tried(o_ptr);

    /* An identification was made */
    if (ident && !object_aware_p(o_ptr))
    {
        object_aware(o_ptr);
        msg_print_object_identified(o_ptr);
    }

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Hack -- some uses are "free" */
    if (!use_charge)
        return;

    /* Consume the item */
    o_ptr->xtra1++;

    if (from_supplies && supply_index >= 0)
    {
        supplies_consume_quantity(supply_index, 1);
        supplies_refresh_entry(supply_index);
    }
    else if (item >= 0)
    {
        inven_item_increase(item, -1);
        inven_item_describe(item);
        inven_item_optimize(item);
    }
    else
    {
        floor_item_increase(0 - item, -1);
        floor_item_describe(0 - item);
        floor_item_optimize(0 - item);
    }
}

/*
 * Hook to determine if an object is activatable
 */
static bool item_tester_hook_activate(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    if (object_has_broken_prefix(o_ptr))
        return (false);

    /* Not known */
    if (!object_known_p(o_ptr))
        return (false);

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Check activation flag */
    if (f3 & (TR3_ACTIVATE))
        return (true);

    /* Assume not */
    return (false);
}

/*
 * Activate a wielded object.  Wielded objects never stack.
 * And even if they did, activatable objects never stack.
 *
 * Note that it always takes a turn to activate an artefact, even if
 * the user hits "escape" at the "direction" prompt.
 */
void do_cmd_activate_by_index(int item)
{
    int lev, score, difficulty;
    bool ident;
    object_type* o_ptr;

    if (player_inventory_handle_valid(item))
    {
        o_ptr = player_inventory_object(item);
    }
    else if (item < 0 && 0 - item > 0 && 0 - item < o_max)
    {
        o_ptr = &o_list[0 - item];
    }
    else
    {
        return;
    }

    if (!o_ptr->k_idx)
        return;

    if (player_pack_action_start(PLAYER_PACK_ACTION_ACTIVATE, item, 0, false,
            o_ptr))
        return;

    if (reject_broken_item_use(o_ptr))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Extract the item level */
    lev = k_info[o_ptr->k_idx].level;

    /* Hack -- use artefact level instead */
    if (artefact_p(o_ptr))
        lev = a_info[o_ptr->name1].level;

    /* Base chance of success */
    score = p_ptr->skill_use[S_WIL];

    // Base difficulty
    difficulty = lev / 2;

    /* Confusion hurts skill */
    if (p_ptr->confused)
        difficulty += 5;

    /* Roll for usage */
    if (skill_check(PLAYER, score, difficulty, NULL) <= 0)
    {
        flush();
        msg_print("You could not draw upon its powers.");
        return;
    }

    /* Sound */
    sound(MSG_ACTIVATE);

    /* Activate the object */
    (void)use_object(o_ptr, &ident);
}

void do_cmd_activate(void)
{
    int item;

    /* Prepare the hook */
    item_tester_hook = item_tester_hook_activate;

    if (!open_inventory_item_select_menu(USE_EQUIP, "Activate which item? ",
            "You have nothing to activate.", &item))
    {
        return;
    }

    do_cmd_activate_by_index(item);
}

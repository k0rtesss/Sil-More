/* File: spell/spell-identify.c */

#include "angband.h"
#include "externs.h"
#include "object/object-ui-identify.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "supplies.h"
#include <math.h>

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_digger(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    object_flags(o_ptr, &f1, &f2, &f3);

    if ((f1 & (TR1_TUNNEL)) && (o_ptr->pval > 0))
    {
        return (true);
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_wieldable_ided_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_wieldable_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    case TV_BOW:
    case TV_ARROW:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_ided_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    case TV_BOW:
    case TV_ARROW:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "armour"
 */
bool item_tester_hook_ided_armour(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_BOOTS:
    case TV_GLOVES:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "armour"
 */
bool item_tester_hook_armour(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_BOOTS:
    case TV_GLOVES:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify non-herb food
 */
bool item_tester_hook_non_herb_food(const object_type* o_ptr)
{
    if ((o_ptr->tval == TV_FOOD) && (o_ptr->pval > 300))
        return (true);

    return (false);
}

/*
 * Hook to specify light with fuel or that does not need fuel
 */
bool item_tester_hook_light_with_fuel(const object_type* o_ptr)
{
    if (o_ptr->tval != TV_LIGHT)
        return (false);

    if (o_ptr->timeout < 1 && fuelable_light_p(o_ptr))
        return (false);

    return (true);
}

/*
 * Hook to specify "enchantable amulet"
 */
bool item_tester_hook_enchantable_amulet(const object_type* o_ptr)
{
    if ((o_ptr->tval == TV_AMULET) && (o_ptr->pval > 0))
        return (true);

    return (false);
}

/*
 * Identify an object chosen from the unified unidentified list.
 * Returns true if an item was identified.
 */
bool ident_spell(bool include_floor)
{
    int item;
    object_type* o_ptr;

    if (!display_unified_identify_menu(include_floor, &item, &o_ptr))
        return false;

    do_ident_item(item, o_ptr);

    return true;
}

/*
 * Hook for "get_item()".  Determine if something is rechargable.
 */
bool item_tester_hook_recharge(const object_type* o_ptr)
{
    /* Recharge staffs */
    if (o_ptr->tval == TV_STAFF)
        return (true);

    /* Nope */
    return (false);
}

/*
 * Recharge a staff from the Harness or on the floor.
 *
 * Mage -- Recharge I --> recharge(5)
 * Mage -- Recharge II --> recharge(40)
 * Mage -- Recharge III --> recharge(100)
 *
 * Priest -- Recharge --> recharge(15)
 *
 * Scroll of recharging --> recharge(60)
 *
 * recharge(20) = 1/6 failure for empty 10th level wand
 * recharge(60) = 1/10 failure for empty 10th level wand
 *
 * It is harder to recharge high level, and highly charged wands.
 *
 * XXX XXX XXX Beware of "sliding index errors".
 *
 * Should probably not "destroy" over-charged items, unless we
 * "replace" them by, say, a broken stick or some such.  The only
 * reason this is okay is because "scrolls of recharging" appear
 * BEFORE all staves/wands/rods in the inventory.  Note that the
 * new "auto_sort_pack" option would correctly handle replacing
 * the "broken" wand with any other item (i.e. a broken stick).
 *
 */
bool recharge(int num)
{
    int item;
    object_type* o_ptr;
    byte old_item_tester_tval = item_tester_tval;
    bool (*old_item_tester_hook)(const object_type*) = item_tester_hook;
    bool old_item_tester_full = item_tester_full;
    bool picked;

    item_tester_tval = 0;
    item_tester_hook = item_tester_hook_recharge;
    item_tester_full = false;

    picked = open_inventory_item_select_menu(USE_INVEN | USE_EQUIP | USE_FLOOR,
        "Recharge which staff?", "You have nothing to recharge.", &item);

    item_tester_tval = old_item_tester_tval;
    item_tester_hook = old_item_tester_hook;
    item_tester_full = old_item_tester_full;

    if (!picked)
        return (false);

    /* Get the item (in the pack) */
    if (player_inventory_handle_valid(item))
        o_ptr = player_inventory_object(item);

    /* Get the item (on the floor) */
    else
        o_ptr = &o_list[0 - item];

    /* Attempt to Recharge a staff, or handle failure to recharge . */
    if (o_ptr->tval == TV_STAFF)
    {
        if (o_ptr->sval == SV_STAFF_RECHARGING
            && p_ptr->active_ability[S_WIL][WIL_CHANNELING])
        {
            num /= 2;
        }

        /* Recharge the staff. */
        o_ptr->pval += num;

        if (object_aware_p(o_ptr) && (o_ptr->ident & (IDENT_EMPTY)))
        {
            object_aware(o_ptr);
            object_known(o_ptr);
        }

        /* Hack -- we no longer think the item is empty */
        o_ptr->ident &= ~(IDENT_EMPTY);
    }

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Something was done */
    return (true);
}

/*
 * Hook to specify "arrows"
 */
bool item_tester_hook_ided_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return false;
    }
    }

    return (false);
}

/*
 * Hook to specify "arrows"
 */
bool item_tester_hook_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify ordinary arrows
 */
bool item_tester_hook_ordinary_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        if (o_ptr->name1 || object_has_ego(o_ptr) || o_ptr->att > 0)
            return false;
        return true;
    }
    }

    return false;
}

/*
 * Identifies all objects in the equipment, inventory and supplies,
 * announcing each one.
 */
void identify_and_describe_pack(void)
{
    int item;
    object_type* o_ptr;

    /* Identify equipment */
    for (item = INVEN_WIELD; item < INVEN_TOTAL; item++)
    {
        /* Get the object */
        o_ptr = &inventory[item];

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Identify it */
        do_ident_item(item, o_ptr);
    }

    /* Identify inventory */
    for (int ordinal = 0; ordinal < player_pack_entry_count(); ordinal++)
    {
        item = player_pack_entry_handle_at(ordinal);
        /* Get the object */
        o_ptr = player_inventory_object(item);

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Identify it */
        do_ident_item(item, o_ptr);
    }

    /* Identify arrows in the dedicated Quiver store. */
    for (int quiver_idx = 0;
        quiver_idx < player_quiver_store_entry_count(); quiver_idx++)
    {
        o_ptr = player_quiver_store_entry_at(quiver_idx);
        if (!o_ptr || !o_ptr->k_idx || object_known_p(o_ptr))
            continue;
        do_ident_item(QUIVER_INDEX + quiver_idx, o_ptr);
    }

    /* Identify supplies */
    int supply_count = supplies_entry_count();
    for (int supply_idx = 0; supply_idx < supply_count; supply_idx++)
    {
        o_ptr = supplies_entry_at(supply_idx);
        if (!o_ptr || !o_ptr->k_idx)
            continue;
        if (object_known_p(o_ptr))
            continue;

        do_ident_item(SUPPLIES_INDEX + supply_idx, o_ptr);
    }
}

/* Mass-identify handler */
bool mass_identify(int rad)
{
    /* Direct the ball to the player */
    target_set_location(p_ptr->py, p_ptr->px);

    /* Cast the ball spell */
    fire_ball(GF_IDENTIFY, 5, 0, 0, -1, rad);

    /* Identify equipment, inventory and supplies */
    identify_and_describe_pack();

    /* This spell always works */
    return (true);
}

/*
 * Execute some common code of the identify spells.
 * "item" is used to print the slot occupied by an object in equip/inven.
 * ANY negative value assigned to "item" can be used for specifying an object
 * on the floor (they don't have a slot, example: the code used to handle
 * GF_IDENTIFY in project_o).
 */
void do_ident_item(int item, object_type* o_ptr)
{
    char o_name[80];

    /* Identify it */
    object_aware(o_ptr);
    object_known(o_ptr);

    /* Apply an autoinscription, if necessary */
    apply_autoinscription(o_ptr);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    /* Description */
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Describe */
    if (item >= QUIVER_INDEX && item < QUIVER_INDEX_END)
    {
        msg_format("In your quiver: %s.", o_name);
        p_ptr->redraw |= PR_QUIVER;
    }
    else if (item >= SUPPLIES_INDEX)
    {
        int supply_index = item - SUPPLIES_INDEX;
        msg_format("In your supplies: %s.", o_name);
        supplies_refresh_entry(supply_index);
    }
    else if (player_inventory_handle_is_equipped(item))
    {
        msg_format(
            "%^s: %s (%c).", describe_use(item), o_name, index_to_label(item));
    }
    else if (item >= 0)
    {
        msg_format("In your pack: %s (%c).", o_name, index_to_label(item));
    }
    else
    {
        msg_format("On the ground: %s.", o_name);
    }
}

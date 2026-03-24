/* File: object-inventory.c */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "supplies.h"

enum inventory_limit_group
{
    INV_LIMIT_NONE = 0,
    INV_LIMIT_ARROW,
    INV_LIMIT_BOW,
    INV_LIMIT_STAFF,
    INV_LIMIT_DIGGING,
    INV_LIMIT_BOOTS,
    INV_LIMIT_GLOVES,
    INV_LIMIT_HELM_CROWN,
    INV_LIMIT_ROUND_SHIELD,
    INV_LIMIT_OTHER_SHIELD,
    INV_LIMIT_CLOAK,
    INV_LIMIT_SOFT_ARMOUR,
    INV_LIMIT_MAIL,
    INV_LIMIT_MELEE_WEAPON,
    INV_LIMIT_SUPPLY_WEIGHT
};

static bool carry_limit_last_failed = false;
static enum inventory_limit_group carry_limit_last_group = INV_LIMIT_NONE;
static int carry_limit_last_limit = 0;
static char carry_limit_last_label[64];

static void clear_inventory_limit_failure(void)
{
    carry_limit_last_failed = false;
    carry_limit_last_group = INV_LIMIT_NONE;
    carry_limit_last_limit = 0;
    carry_limit_last_label[0] = '\0';
}

static bool object_is_truly_two_handed(const object_type* o_ptr)
{
    if (!o_ptr)
        return false;

    switch (o_ptr->tval)
    {
        case TV_HAFTED:
            return (o_ptr->sval == SV_QUARTERSTAFF);
        case TV_POLEARM:
            return (o_ptr->sval == SV_GREAT_SPEAR) || (o_ptr->sval == SV_GLAIVE)
                || (o_ptr->sval == SV_GREAT_AXE);
        case TV_SWORD:
            return (o_ptr->sval == SV_GREAT_SWORD)
                || (o_ptr->sval == SV_STAR_IRON_GREAT_SWORD);
        default:
            break;
    }

    return false;
}

static bool get_inventory_limit_info(const object_type* o_ptr,
                                     enum inventory_limit_group* group,
                                     int* limit,
                                     int* cost)
{
    enum inventory_limit_group local_group = INV_LIMIT_NONE;
    int local_limit = 0;
    int local_cost = 1;
    bool found = true;

    if (!o_ptr || !o_ptr->k_idx)
    {
        found = false;
    }
    else
    {
        switch (o_ptr->tval)
        {
            case TV_ARROW:
                local_group = INV_LIMIT_ARROW;
                local_limit = 2;
                break;
            case TV_BOW:
                local_group = INV_LIMIT_BOW;
                local_limit = 1;
                break;
            case TV_STAFF:
                local_group = INV_LIMIT_STAFF;
                local_limit = 1;
                break;
            case TV_DIGGING:
                local_group = INV_LIMIT_DIGGING;
                local_limit = 1;
                break;
            case TV_BOOTS:
                local_group = INV_LIMIT_BOOTS;
                local_limit = 2;
                break;
            case TV_GLOVES:
                local_group = INV_LIMIT_GLOVES;
                local_limit = 2;
                break;
            case TV_HELM:
            case TV_CROWN:
                local_group = INV_LIMIT_HELM_CROWN;
                local_limit = 1;
                break;
            case TV_SHIELD:
                if (o_ptr->sval == SV_ROUND_SHIELD || o_ptr->sval == SV_BROKEN_SHIELD)
                {
                    local_group = INV_LIMIT_ROUND_SHIELD;
                    local_limit = 1;
                }
                else
                {
                    local_group = INV_LIMIT_OTHER_SHIELD;
                    local_limit = 0;
                }
                break;
            case TV_CLOAK:
                local_group = INV_LIMIT_CLOAK;
                local_limit = 3;
                break;
            case TV_SOFT_ARMOR:
                if (o_ptr->sval == SV_ROBE)
                {
                    local_group = INV_LIMIT_CLOAK;
                    local_limit = 3;
                }
                else
                {
                    local_group = INV_LIMIT_SOFT_ARMOUR;
                    local_limit = 1;
                }
                break;
            case TV_MAIL:
                local_group = INV_LIMIT_MAIL;
                local_limit = 0;
                break;
            case TV_HAFTED:
            case TV_POLEARM:
            case TV_SWORD:
                local_group = INV_LIMIT_MELEE_WEAPON;
                local_limit = 2;
                local_cost = object_is_truly_two_handed(o_ptr) ? 2 : 1;
                break;
            default:
                found = false;
                break;
        }
    }

    if (found)
    {
        if (p_ptr->active_ability[S_EVN][EVN_HEAVY_ARMOUR])
        {
            if (local_group == INV_LIMIT_MAIL
                || local_group == INV_LIMIT_HELM_CROWN
                || local_group == INV_LIMIT_ROUND_SHIELD
                || local_group == INV_LIMIT_OTHER_SHIELD)
            {
                local_limit += 1;
            }
        }
    }

    if (group)
        *group = local_group;
    if (limit)
        *limit = local_limit;
    if (cost)
        *cost = local_cost;

    return found;
}

static int inventory_limit_usage(enum inventory_limit_group group)
{
    int usage = 0;

    if (group == INV_LIMIT_NONE)
        return 0;

    for (int idx = 0; idx <= INVEN_PACK; idx++)
    {
        object_type* slot_ptr = &inventory[idx];

        if (!slot_ptr->k_idx)
            continue;

        enum inventory_limit_group slot_group;
        int slot_limit;
        int slot_cost;

        if (!get_inventory_limit_info(slot_ptr, &slot_group, &slot_limit,
                                       &slot_cost))
            continue;

        if (slot_group != group)
            continue;

        usage += slot_cost;
    }

    return usage;
}

static void fill_inventory_limit_label(enum inventory_limit_group group,
                                       const object_type* o_ptr)
{
    switch (group)
    {
        case INV_LIMIT_ARROW:
            SDL_strlcpy(carry_limit_last_label, "arrow stacks",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_BOW:
            SDL_strlcpy(carry_limit_last_label, "bows",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_STAFF:
            SDL_strlcpy(carry_limit_last_label, "walking staves",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_DIGGING:
            SDL_strlcpy(carry_limit_last_label, "digging tools",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_BOOTS:
            SDL_strlcpy(carry_limit_last_label, "pairs of boots",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_GLOVES:
            SDL_strlcpy(carry_limit_last_label, "pairs of gloves",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_HELM_CROWN:
            SDL_strlcpy(carry_limit_last_label, "helms or crowns",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_ROUND_SHIELD:
            SDL_strlcpy(carry_limit_last_label, "round shields",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_OTHER_SHIELD:
            SDL_strlcpy(carry_limit_last_label, "non-round shields",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_CLOAK:
            SDL_strlcpy(carry_limit_last_label, "cloaks",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_SOFT_ARMOUR:
            SDL_strlcpy(carry_limit_last_label, "soft armour",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_MAIL:
            SDL_strlcpy(carry_limit_last_label, "mail armour",
                      sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_MELEE_WEAPON:
            if (object_is_truly_two_handed(o_ptr))
                SDL_strlcpy(carry_limit_last_label,
                          "two-handed melee weapons",
                          sizeof(carry_limit_last_label));
            else
                SDL_strlcpy(carry_limit_last_label, "melee weapons",
                          sizeof(carry_limit_last_label));
            break;
        case INV_LIMIT_SUPPLY_WEIGHT:
            SDL_strlcpy(carry_limit_last_label, "supply weight",
                      sizeof(carry_limit_last_label));
            break;
        default:
            SDL_strlcpy(carry_limit_last_label, "items of this type",
                      sizeof(carry_limit_last_label));
            break;
    }
}

static void set_inventory_limit_failure(enum inventory_limit_group group,
                                        int limit,
                                        const object_type* o_ptr)
{
    carry_limit_last_failed = true;
    carry_limit_last_group = group;
    carry_limit_last_limit = limit;
    fill_inventory_limit_label(group, o_ptr);
}

bool inven_carry_limit_can_replace(const object_type* o_ptr)
{
    enum inventory_limit_group group;
    int limit;
    int cost;

    if (!carry_limit_last_failed)
        return false;

    if (carry_limit_last_limit <= 0)
        return false;

    if (!o_ptr)
        return false;

    if (!get_inventory_limit_info(o_ptr, &group, &limit, &cost))
        return false;

    if (group != carry_limit_last_group)
        return false;

    return (cost > 0);
}

static bool inventory_type_slot_available(const object_type* o_ptr,
                                          bool record_failure)
{
    enum inventory_limit_group group;
    int limit;
    int cost;

    if (!get_inventory_limit_info(o_ptr, &group, &limit, &cost))
        return true;

    if (limit <= 0)
    {
        if (record_failure)
            set_inventory_limit_failure(group, limit, o_ptr);
        return false;
    }

    int used = inventory_limit_usage(group);

    if (used + cost <= limit)
        return true;

    if (record_failure)
        set_inventory_limit_failure(group, limit, o_ptr);

    return false;
}

int object_stack_limit(const object_type* o_ptr)
{
    if (!o_ptr)
        return MAX_STACK_SIZE - 1;

    if (o_ptr->tval == TV_SWORD && o_ptr->sval == SV_DAGGER)
        return 7;

    if (o_ptr->tval == TV_POLEARM && o_ptr->sval == SV_SPEAR)
        return 5;

    if (o_ptr->tval == TV_POLEARM && o_ptr->sval == SV_HAND_AXE)
        return 3;

    if (o_ptr->tval == TV_ARROW)
        return 48;

    return MAX_STACK_SIZE - 1;
}





/*
 * Describe the charges on an item in the inventory.
 */
void inven_item_charges(int item)
{
    int visible_charges = 0;
    object_type* o_ptr = &inventory[item];

    /* Require staff */
    if (o_ptr->tval != TV_STAFF)
        return;

    /* Require known item */
    if (!object_known_p(o_ptr))
        return;

    visible_charges = (o_ptr->pval + CHANNELING_CHARGE_MULTIPLIER - 1)
        / CHANNELING_CHARGE_MULTIPLIER;
    if (visible_charges < 0)
        visible_charges = 0;

    /* Print a message */
    msg_format("You have %d charge%s remaining.", visible_charges,
        (visible_charges != 1) ? "s" : "");
}

/*
 * Describe an item in the inventory.
 */
void inven_item_describe(int item)
{
    object_type* o_ptr = &inventory[item];

    char o_name[80];

    if (artefact_p(o_ptr) && object_known_p(o_ptr))
    {
        /* Get a description */
        object_desc(o_name, sizeof(o_name), o_ptr, false, 3);

        /* Print a message */
        msg_format(
            "You no longer have the %s (%c).", o_name, index_to_label(item));
    }
    else
    {
        /* Get a description */
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        /* Print a message */
        msg_format("You have %s (%c).", o_name, index_to_label(item));
    }
}

/*
 * Increase the "number" of an item in the inventory
 */
void inven_item_increase(int item, int num)
{
    object_type* o_ptr = &inventory[item];

    /* Log staff number changes for debugging */
    if (o_ptr->tval == TV_STAFF)
    {
        log_debug("inven_item_increase called on staff at slot %d: num=%d, current number=%d pval=%d k_idx=%d sval=%d",
                  item, num, o_ptr->number, o_ptr->pval, o_ptr->k_idx, o_ptr->sval);
    }

    /* Apply */
    num += o_ptr->number;

    /* Bounds check */
    if (num > 255)
        num = 255;
    else if (num < 0)
        num = 0;

    /* Un-apply */
    num -= o_ptr->number;

    /* Change the number and weight */
    if (num)
    {
        /* Add the number */
        o_ptr->number += num;

        /* Log staff number after change */
        if (o_ptr->tval == TV_STAFF)
        {
            log_debug("inven_item_increase: staff at slot %d now has number=%d (changed by %d)",
                      item, o_ptr->number, num);
            if (o_ptr->number == 0)
            {
                log_error("WARNING: Staff number changed to 0! This will cause deletion. k_idx=%d sval=%d pval=%d",
                          o_ptr->k_idx, o_ptr->sval, o_ptr->pval);
            }
        }

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Recalculate mana XXX */
        p_ptr->update |= (PU_MANA);

        /* Combine the pack */
        p_ptr->notice |= (PN_COMBINE);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }
}

/*
 * Erase an inventory slot if it has no more items
 */
void inven_item_optimize(int item)
{
    object_type* o_ptr = &inventory[item];

    /* Only optimize real items */
    if (!o_ptr->k_idx)
        return;

    /* Log staff optimization attempts for debugging */
    if (o_ptr->tval == TV_STAFF)
    {
        log_debug("inven_item_optimize called on staff at slot %d: k_idx=%d sval=%d pval=%d number=%d",
                  item, o_ptr->k_idx, o_ptr->sval, o_ptr->pval, o_ptr->number);
    }

    /* Only optimize empty items */
    if (o_ptr->number)
        return;

    /* Log staff deletion */
    if (o_ptr->tval == TV_STAFF)
    {
        log_error("STAFF DELETION BUG: Deleting staff at slot %d with number=0! k_idx=%d sval=%d pval=%d",
                  item, o_ptr->k_idx, o_ptr->sval, o_ptr->pval);
    }

    /* The item is in the pack */
    if (item < INVEN_WIELD)
    {
        int i;

        /* One less item */
        p_ptr->inven_cnt--;

        /* Slide everything down */
        for (i = item; i < INVEN_PACK; i++)
        {
            /* Hack -- slide object */
            memcpy(&inventory[i], &inventory[i + 1], sizeof(object_type));
        }

        /* Hack -- wipe hole */
        memset(&inventory[i], 0, sizeof(object_type));

        /* Window stuff */
        p_ptr->window |= (PW_INVEN);
    }

    /* The item is being wielded */
    else
    {
        /* One less item */
        p_ptr->equip_cnt--;

        /* Erase the empty slot */
        object_wipe(&inventory[item]);

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Recalculate mana XXX */
        p_ptr->update |= (PU_MANA);

        /* Window stuff */
        p_ptr->window |= (PW_EQUIP | PW_PLAYER_0);

        p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);
    }
}

/*
 * Describe the charges on an item on the floor.
 */
void floor_item_charges(int item)
{
    int visible_charges = 0;
    object_type* o_ptr = &o_list[item];

    /* Require staff */
    if (o_ptr->tval != TV_STAFF)
        return;

    /* Require known item */
    if (!object_known_p(o_ptr))
        return;

    visible_charges = (o_ptr->pval + CHANNELING_CHARGE_MULTIPLIER - 1)
        / CHANNELING_CHARGE_MULTIPLIER;
    if (visible_charges < 0)
        visible_charges = 0;

    /* Print a message */
    msg_format("There are %d charge%s remaining.", visible_charges,
        (visible_charges != 1) ? "s" : "");
}

/*
 * Describe an item on the floor.
 */
void floor_item_describe(int item)
{
    object_type* o_ptr = &o_list[item];

    char o_name[80];

    /* Get a description */
    object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Print a message */
    if (!p_ptr->blind)
        msg_format("You see %s.", o_name);
}

/*
 * Increase the "number" of an item on the floor
 */
void floor_item_increase(int item, int num)
{
    object_type* o_ptr = &o_list[item];

    /* Apply */
    num += o_ptr->number;

    /* Bounds check */
    if (num > 255)
        num = 255;
    else if (num < 0)
        num = 0;

    /* Un-apply */
    num -= o_ptr->number;

    /* Change the number */
    o_ptr->number += num;
}

/*
 * Optimize an item on the floor (destroy "empty" items)
 */
void floor_item_optimize(int item)
{
    object_type* o_ptr = &o_list[item];

    /* Paranoia -- be sure it exists */
    if (!o_ptr->k_idx)
        return;

    /* Only optimize empty items */
    if (o_ptr->number)
        return;

    /* Delete the object */
    delete_object_idx(item);
}

/*
 *  overflow the player's backpack if needed
 */
void check_pack_overflow(void)
{
    if (inventory[INVEN_PACK].k_idx)
    {
        int item = INVEN_PACK;

        char o_name[80];

        object_type* o_ptr;

        /* Get the slot to be dropped */
        o_ptr = &inventory[item];

        /* Disturbing */
        disturb(0, 0);

        /* Warning */
        msg_print("Your pack overflows!");

        /* Describe */
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        /* Message */
        msg_format("You drop %s (%c).", o_name, index_to_label(item));

        /* Drop it (carefully) near the player */
        drop_near(o_ptr, 0, p_ptr->py, p_ptr->px);

        /* Modify, Describe, Optimize */
        inven_item_increase(item, -255);
        inven_item_describe(item);
        inven_item_optimize(item);

        /* Notice stuff (if needed) */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff (if needed) */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff (if needed) */
        if (p_ptr->redraw)
            redraw_stuff();

        /* Window stuff (if needed) */
        if (p_ptr->window)
            window_stuff();
    }
}

/*
 * Check if we have space for an item in the pack without overflow
 */
bool inven_carry_okay(const object_type* o_ptr)
{
    int j;

    clear_inventory_limit_failure();

    /* Similar slot? */
    for (j = 0; j < INVEN_PACK; j++)
    {
        object_type* j_ptr = &inventory[j];

        if (!j_ptr->k_idx)
            continue;

        if (object_similar(j_ptr, o_ptr))
            return (true);
    }

    // Check for combining in quiver first
    if (o_ptr->tval == TV_ARROW)
    {
        int empty_quiver = 0;

        for (j = INVEN_QUIVER1; j <= INVEN_QUIVER2; j++)
        {
            object_type* j_ptr = &inventory[j];

            if (!j_ptr->k_idx)
            {
                if (empty_quiver == 0)
                    empty_quiver = j;
                continue;
            }

            if (object_similar(j_ptr, o_ptr))
                return (true);
        }

        if ((empty_quiver > 0) && o_ptr->pickup)
            return (true);
    }

    /* Throwing weapons can combine with similar items in quiver, 
       or go back to their original empty quiver slot */
    if (player_can_treat_as_throwing(o_ptr))
    {
        int empty_quiver = 0;
        bool has_desired_slot = (o_ptr->pickup_slot == INVEN_QUIVER1) || 
                                (o_ptr->pickup_slot == INVEN_QUIVER2);
        
        for (j = INVEN_QUIVER1; j <= INVEN_QUIVER2; j++)
        {
            object_type* j_ptr = &inventory[j];

            if (!j_ptr->k_idx)
            {
                if (empty_quiver == 0)
                    empty_quiver = j;
                continue;
            }

            if (object_similar(j_ptr, o_ptr))
                return (true);
        }
        
        /* Thrown items can go back to an empty quiver slot */
        if ((empty_quiver > 0) && o_ptr->pickup)
            return (true);
            
        /* Or specifically to their original slot if it's empty */
        if (has_desired_slot && (inventory[o_ptr->pickup_slot].k_idx == 0))
            return (true);
    }

    if (!inventory_type_slot_available(o_ptr, true))
        return (false);

    bool supply_item = supplies_is_supply_object(o_ptr);
    bool supplies_present = (supplies_entry_count() > 0);
    int logical_items = p_ptr->inven_cnt + (supplies_present ? 1 : 0);

    if (supply_item)
    {
        if (!supplies_present)
        {
            /* Need to allocate one slot for the supplies bundle. */
            if (logical_items >= INVEN_PACK)
                return (false);
        }

        /* Check if the item would exceed the supply weight limit */
        if (!supplies_can_absorb_object(o_ptr))
        {
            /* Check if we can do partial pickup */
            int max_qty = supplies_max_absorbable_quantity(o_ptr);
            if (max_qty > 0 && o_ptr->number > 1)
            {
                /* Partial pickup is possible, allow it through */
                return (true);
            }
            
            /* Can't pick up any, show error */
            set_inventory_limit_failure(INV_LIMIT_SUPPLY_WEIGHT, 25, o_ptr);
            return (false);
        }

        return (true);
    }

    /* Non-supply item */
    if (logical_items >= INVEN_PACK)
        return (false);

    return (true);
}

bool inven_carry_okay_after_removing(
    const object_type* o_ptr, int remove_item, int remove_amt)
{
    object_type saved_item;
    bool had_removed_item = false;
    s16b saved_inven_cnt = p_ptr->inven_cnt;
    bool result;

    if (!o_ptr)
        return false;

    clear_inventory_limit_failure();

    /* Simulate removing the source pack item so swap prompts reflect the real outcome. */
    if (remove_item >= 0 && remove_item < INVEN_PACK && remove_amt > 0
        && inventory[remove_item].k_idx)
    {
        object_copy(&saved_item, &inventory[remove_item]);
        had_removed_item = true;

        if (remove_amt >= inventory[remove_item].number)
        {
            object_wipe(&inventory[remove_item]);
            p_ptr->inven_cnt--;
        }
        else
        {
            inventory[remove_item].number -= remove_amt;
        }
    }

    result = inven_carry_okay(o_ptr);

    if (had_removed_item)
    {
        object_copy(&inventory[remove_item], &saved_item);
        p_ptr->inven_cnt = saved_inven_cnt;
    }

    clear_inventory_limit_failure();
    return result;
}

/*
 * Add an item to the players inventory, and return the slot used.
 *
 * If the new item can combine with an existing item in the inventory,
 * it will do so, using "object_similar()" and "object_absorb()", else,
 * the item will be placed into the "proper" location in the inventory.
 *
 * This function can be used to "over-fill" the player's pack, but only
 * once, and such an action must trigger the "overflow" code immediately.
 * Note that when the pack is being "over-filled", the new item must be
 * placed into the "overflow" slot, and the "overflow" must take place
 * before the pack is reordered, but (optionally) after the pack is
 * combined.  This may be tricky.  See "dungeon.c" for info.
 *
 * Note that this code must remove any location/stack information
 * from the object once it is placed into the inventory.
 */
s16b inven_carry(object_type* o_ptr, bool combine_ammo)
{
    int i = 1; // default value to soothe compilation warnings
    int j, k;
    int n = -1;

    object_type* j_ptr;

    clear_inventory_limit_failure();

    /*paranoia, don't pick up "&nothings"*/
    if (!o_ptr->k_idx)
        return (-1);

    if (supplies_is_supply_object(o_ptr))
    {
        object_type copy;
        object_copy(&copy, o_ptr);
        if (supplies_absorb_object(&copy))
        {
            object_wipe(o_ptr);
            return SUPPLIES_INDEX;
        }
        /* If absorption failed, treat as normal item. */
    }

    int desired_slot = o_ptr->pickup_slot;
    bool wanted_auto_recover = o_ptr->pickup ? true : false;
    bool wants_throw_slot = (desired_slot == INVEN_QUIVER1) || (desired_slot == INVEN_QUIVER2);

    if (wants_throw_slot)
    {
        object_type* d_ptr = &inventory[desired_slot];
        bool is_throwing = player_can_treat_as_throwing(o_ptr);
        bool is_arrow = (o_ptr->tval == TV_ARROW);

        if (is_throwing || is_arrow)
        {
            if (d_ptr->k_idx == 0)
            {
                int limit = object_stack_limit(o_ptr);
                int placed = MIN(o_ptr->number, limit);
                object_copy(d_ptr, o_ptr);
                d_ptr->number = placed;
                d_ptr->pickup = false;
                d_ptr->pickup_slot = -1;
                d_ptr->ident |= IDENT_HANDLED;
                o_ptr->number -= placed;

                p_ptr->equip_cnt++;
                p_ptr->notice |= (PN_COMBINE | PN_REORDER);
                p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

                if (o_ptr->number <= 0)
                {
                    o_ptr->pickup = false;
                    o_ptr->pickup_slot = -1;
                    return (desired_slot);
                }

                o_ptr->pickup = wanted_auto_recover;
                o_ptr->pickup_slot = -1;
            }
            else if (object_similar(d_ptr, o_ptr))
            {
                object_absorb(d_ptr, o_ptr);
                d_ptr->pickup = false;
                d_ptr->pickup_slot = -1;
                d_ptr->ident |= IDENT_HANDLED;
                p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

                if (o_ptr->number == 0)
                {
                    o_ptr->pickup = false;
                    o_ptr->pickup_slot = -1;
                    return (desired_slot);
                }

                o_ptr->pickup = wanted_auto_recover;
                o_ptr->pickup_slot = -1;
            }
        }
        o_ptr->pickup_slot = -1;
    }

    // Check for combining in quiver first
    if (o_ptr->tval == TV_ARROW && combine_ammo)
    {
        int empty_quiver = 0;

        // arrows combine with similar arrows
        for (j = INVEN_QUIVER1; j <= INVEN_QUIVER2; j++)
        {
            j_ptr = &inventory[j];

            /* Skip non-objects */
            if (!j_ptr->k_idx)
            {
                // keep track of the first empty quiver
                if (empty_quiver == 0)
                    empty_quiver = j;
                continue;
            }

            /* Check if the two items can be combined */
            if (object_similar(j_ptr, o_ptr))
            {
                /* Combine the items */
                object_absorb(j_ptr, o_ptr);
                j_ptr->ident |= IDENT_HANDLED;

                /* Window stuff */
                p_ptr->window |= (PW_INVEN);

                if (o_ptr->number == 0)
                {
                    /* Success */
                    return (j);
                }
                else
                {
                    char j_name[80];

                    // combination message
                    msg_print(
                        "You combine them with the arrows in your quiver.");

                    /* Describe the object */
                    object_desc(j_name, sizeof(j_name), j_ptr, true, 3);

                    /* Message */
                    msg_format("You have %s (%c).", j_name, index_to_label(j));
                }
            }
        }

        // arrows that have been fired can also fit back into an empty quiver
        // slot
        if ((empty_quiver > 0) && o_ptr->pickup)
        {
            o_ptr->pickup = false;
            o_ptr->pickup_slot = -1;

            if ((o_ptr >= o_list) && (o_ptr < o_list + o_max))
            {
                int floor_idx = (int)(o_ptr - o_list);
                do_cmd_wield(o_ptr, 0 - floor_idx);
            }

            return (-1);
        }
    }

    /* Handle throwing weapons - try to combine with existing in quiver first */
    if (player_can_treat_as_throwing(o_ptr))
    {
        int empty_quiver = 0;

        /* Check for combining with existing throwing weapons in quiver */
        for (j = INVEN_QUIVER1; j <= INVEN_QUIVER2; j++)
        {
            j_ptr = &inventory[j];

            if (!j_ptr->k_idx)
            {
                if (empty_quiver == 0)
                    empty_quiver = j;
                continue;
            }

            if (object_similar(j_ptr, o_ptr))
            {
                object_absorb(j_ptr, o_ptr);
                j_ptr->ident |= IDENT_HANDLED;
                p_ptr->window |= (PW_INVEN | PW_EQUIP);

                if (o_ptr->number == 0)
                    return (j);
                
                /* Partial absorption - show message and continue to pack */
                char j_name[80];
                object_desc(j_name, sizeof(j_name), j_ptr, true, 3);
                msg_format("You combine some with %s (%c).", j_name, index_to_label(j));
                break;
            }
        }

        if ((empty_quiver > 0) && o_ptr->pickup)
        {
            int limit = object_stack_limit(o_ptr);
            int placed = MIN(o_ptr->number, limit);
            object_type* d_ptr = &inventory[empty_quiver];

            object_copy(d_ptr, o_ptr);
            d_ptr->number = placed;
            d_ptr->pickup = false;
            d_ptr->pickup_slot = -1;
            d_ptr->ident |= IDENT_HANDLED;
            o_ptr->number -= placed;
            o_ptr->pickup = false;
            o_ptr->pickup_slot = -1;

            p_ptr->equip_cnt++;
            p_ptr->notice |= (PN_COMBINE | PN_REORDER);
            p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

            if (o_ptr->number <= 0)
                return (empty_quiver);
        }

        /* Any overflow will fall through to pack handling below */
    }

    /* Check for combining */
    for (j = 0; j < INVEN_PACK; j++)
    {
        j_ptr = &inventory[j];

        /* Skip non-objects */
        if (!j_ptr->k_idx)
            continue;

        /* Hack -- track last item */
        n = j;

        /* Check if the two items can be combined */
        if (object_similar(j_ptr, o_ptr))
        {
            /* Combine the items */
            object_absorb(j_ptr, o_ptr);
            j_ptr->ident |= IDENT_HANDLED;

            /* Recalculate bonuses */
            p_ptr->update |= (PU_BONUS);

            /* Window stuff */
            p_ptr->window |= (PW_INVEN);

            if (o_ptr->number == 0)
            {
                /* Success */
                return (j);
            }
            else
            {
                char j_name[80];

                // combination message
                msg_print("You combine them with some items in your pack.");

                /* Describe the object */
                object_desc(j_name, sizeof(j_name), j_ptr, true, 3);

                /* Message */
                msg_format("You have %s (%c).", j_name, index_to_label(j));
            }
        }
    }

    /* Paranoia */
    if (!inventory_type_slot_available(o_ptr, true))
        return (-1);

    /* Check if we have room, accounting for supplies */
    bool supplies_present = (supplies_entry_count() > 0);
    int logical_items = p_ptr->inven_cnt + (supplies_present ? 1 : 0);
    if (logical_items >= INVEN_PACK)
        return (-1);

    /* Find an empty slot */
    for (j = 0; j <= INVEN_PACK; j++)
    {
        j_ptr = &inventory[j];

        /* Use it if found */
        if (!j_ptr->k_idx)
            break;
    }

    /* Use that slot */
    i = j;

    /* Apply an autoinscription */
    apply_autoinscription(o_ptr);

    /* Reset the pickup flag */
    o_ptr->pickup = false;
    o_ptr->pickup_slot = -1;

    /* Reorder the pack */
    if (i < INVEN_PACK)
    {
        s32b o_value, j_value;

        /* Get the "value" of the item */
        o_value = object_value(o_ptr);

        /* Scan every occupied slot */
        for (j = 0; j < INVEN_PACK; j++)
        {
            j_ptr = &inventory[j];

            /* Use empty slots */
            if (!j_ptr->k_idx)
                break;

            /* Objects sort by decreasing type */
            if (o_ptr->tval > j_ptr->tval)
                break;
            if (o_ptr->tval < j_ptr->tval)
                continue;

            /* Non-aware (flavored) items always come last */
            if (!object_aware_p(o_ptr))
                continue;
            if (!object_aware_p(j_ptr))
                break;

            /* Objects sort by increasing sval */
            if (o_ptr->sval < j_ptr->sval)
                break;
            if (o_ptr->sval > j_ptr->sval)
                continue;

            /* Lites sort by decreasing fuel */
            if (o_ptr->tval == TV_LIGHT)
            {
                if (o_ptr->timeout > j_ptr->timeout)
                    break;
                if (o_ptr->timeout < j_ptr->timeout)
                    continue;
            }

            // This next bit is complicated: identified art > pseudo art >
            // identified special > pseudo special > other

            /* Identified artefacts beat the rest */
            if (!(object_known_p(o_ptr) && artefact_p(o_ptr))
                && (object_known_p(j_ptr) && artefact_p(j_ptr)))
                continue;
            if ((object_known_p(o_ptr) && artefact_p(o_ptr))
                && !(object_known_p(j_ptr) && artefact_p(j_ptr)))
                break;

            /* Then pseudo-identified {artefact} */
            if (!(!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr))
                && (!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr)))
                continue;
            if ((!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr))
                && !(!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr)))
                break;

            /* Then identified specials */
            if (!(object_known_p(o_ptr) && ego_item_p(o_ptr))
                && (object_known_p(j_ptr) && ego_item_p(j_ptr)))
                continue;
            if ((object_known_p(o_ptr) && ego_item_p(o_ptr))
                && !(object_known_p(j_ptr) && ego_item_p(j_ptr)))
                break;

            /* Then pseudo-identified {special} */
            if (!(!object_known_p(o_ptr) && special_pseudo_p(o_ptr))
                && (!object_known_p(j_ptr) && special_pseudo_p(j_ptr)))
                continue;
            if ((!object_known_p(o_ptr) && special_pseudo_p(o_ptr))
                && !(!object_known_p(j_ptr) && special_pseudo_p(j_ptr)))
                break;

            /* Determine the "value" of the pack item */
            j_value = object_value(j_ptr);

            /* Objects sort by decreasing value */
            if (o_value > j_value)
                break;
            if (o_value < j_value)
                continue;

            /* Objects sort by increasing weight */
            if (o_ptr->weight < j_ptr->weight)
                break;
            if (o_ptr->weight > j_ptr->weight)
                continue;
        }

        /* Use that slot */
        i = j;

        /* Slide objects */
        for (k = n; k >= i; k--)
        {
            /* Hack -- Slide the item */
            object_copy(&inventory[k + 1], &inventory[k]);
        }

        /* Wipe the empty slot */
        object_wipe(&inventory[i]);
    }

    /* Copy the item */
    object_copy(&inventory[i], o_ptr);

    /* Get the new object */
    j_ptr = &inventory[i];
    j_ptr->ident |= IDENT_HANDLED;

    int limit = object_stack_limit(j_ptr);
    if (j_ptr->number > limit)
    {
        int excess = j_ptr->number - limit;
        j_ptr->number = limit;
        if (o_ptr != j_ptr)
            o_ptr->number = excess;
    }
    else if (o_ptr != j_ptr)
    {
        o_ptr->number -= j_ptr->number;
    }

    /* Forget stack */
    j_ptr->next_o_idx = 0;

    /* Forget monster */
    j_ptr->held_m_idx = 0;

    /* Forget location */
    j_ptr->iy = j_ptr->ix = 0;

    /* No longer marked */
    j_ptr->marked = false;

    /* Count the items */
    p_ptr->inven_cnt++;

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine and Reorder pack */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN);

    /* Return the slot */
    return (i);
}

bool inven_carry_limit_failed(void)
{
    return carry_limit_last_failed;
}

enum inventory_limit_group inven_carry_limit_group(void)
{
    return carry_limit_last_group;
}

cptr inven_carry_limit_label(void)
{
    if (!carry_limit_last_failed)
        return NULL;

    if (!carry_limit_last_label[0])
        return NULL;

    return carry_limit_last_label;
}

int inven_carry_limit_value(void)
{
    return carry_limit_last_limit;
}

/*
 * Take off (some of) a non-cursed equipment item
 *
 * Note that only one item at a time can be wielded per slot.
 *
 * Note that taking off an item when "full" may cause that item
 * to fall to the ground.
 *
 * Return the inventory slot into which the item is placed.
 */
s16b inven_takeoff(int item, int amt)
{
    int slot;

    object_type* o_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    cptr act;

    char o_name[80];

    /* Get the item to take off */
    o_ptr = &inventory[item];

    /* Paranoia */
    if (amt <= 0)
        return (-1);

    /* Verify */
    if (amt > o_ptr->number)
        amt = o_ptr->number;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Obtain a local object */
    object_copy(i_ptr, o_ptr);

    /* Modify quantity */
    i_ptr->number = amt;

    object_type drop_obj;
    object_copy(&drop_obj, i_ptr);
    drop_obj.pickup = false;
    drop_obj.pickup_slot = -1;

    object_type drop_template;
    object_copy(&drop_template, &drop_obj);

    /* Describe the object */
    object_desc(o_name, sizeof(o_name), i_ptr, true, 3);

    /* Took off weapon */
    if ((item == INVEN_WIELD)
        || ((item == INVEN_ARM) && (i_ptr->tval != TV_SHIELD)))
    {
        act = "You were wielding";
    }

    /* Took off bow */
    else if (item == INVEN_BOW)
    {
        act = "You were holding";
    }

    /* Took off light */
    else if (item == INVEN_LITE)
    {
        act = "You were holding";
    }

    /* Took off arrows */
    else if ((item == INVEN_QUIVER1) || (item == INVEN_QUIVER2))
    {
        act = "You have removed from your quiver";
    }

    /* Took off something */
    else
    {
        act = "You were wearing";
    }

    /* Modify, Optimize */
    log_debug("inven_takeoff: Before decrease - item=%d (k_idx=%d, prefix=%d, suffix=%d, number=%d)",
              item, o_ptr->k_idx, (int)object_ego_prefix(o_ptr), (int)object_ego_suffix(o_ptr), o_ptr->number);
    log_debug("inven_takeoff: Taking off copy - k_idx=%d, prefix=%d, suffix=%d, number=%d",
              i_ptr->k_idx, (int)object_ego_prefix(i_ptr), (int)object_ego_suffix(i_ptr), i_ptr->number);
    inven_item_increase(item, -amt);
    inven_item_optimize(item);

    /* Carry the object */
    log_debug("inven_takeoff: Calling inven_carry with k_idx=%d, prefix=%d, suffix=%d", 
              i_ptr->k_idx, (int)object_ego_prefix(i_ptr), (int)object_ego_suffix(i_ptr));
    slot = inven_carry(i_ptr, false);
    log_debug("inven_takeoff: inven_carry returned slot=%d", slot);

    if (slot == SUPPLIES_INDEX)
    {
        char label = supplies_label_char();
        if (!label)
            label = 'a';
        msg_format("%s %s (%c).", act, o_name, label);
        return slot;
    }

    if (slot >= 0)
    {
        /* Message */
        msg_format("%s %s (%c).", act, o_name, index_to_label(slot));
        return slot;
    }

    /* Could not carry the item; place it on the floor instead. */
    msg_format("%s %s.", act, o_name);

    if (inven_carry_limit_failed())
    {
        cptr label = inven_carry_limit_label();
        int limit = inven_carry_limit_value();

        if (label)
            msg_format("Your pack cannot hold more %s (limit %d).", label, limit);
        else
            msg_print("You have no room in your pack.");
    }
    else
    {
        msg_print("You have no room in your pack.");
    }

    bool can_drop_here = (cave_feat[p_ptr->py][p_ptr->px] == FEAT_FLOOR
        || cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT);
    s16b o_idx = 0;

    if (can_drop_here)
    {
        o_idx = floor_carry(p_ptr->py, p_ptr->px, &drop_obj);

        if (o_idx > 0)
        {
            msg_print("It lands at your feet.");
            return (0 - o_idx);
        }
    }

    for (int d = 0; d < 8; d++)
    {
        int yy = p_ptr->py + ddy_ddd[d];
        int xx = p_ptr->px + ddx_ddd[d];

        if (!in_bounds_fully(yy, xx))
            continue;

        if (cave_feat[yy][xx] != FEAT_FLOOR
            && cave_feat[yy][xx] != FEAT_SUNLIGHT)
            continue;

        if (cave_o_idx[yy][xx] != 0)
            continue;

        object_copy(&drop_obj, &drop_template);
        o_idx = floor_carry(yy, xx, &drop_obj);
        if (o_idx > 0)
        {
            msg_print("It lands nearby.");
            return (0 - o_idx);
        }
    }

    object_copy(&drop_obj, &drop_template);
    drop_near(&drop_obj, 0, p_ptr->py, p_ptr->px);
    msg_print("It falls nearby.");
    return (-1);
}

/*
 * Drop (some of) a non-cursed inventory/equipment item
 *
 * The object will be dropped "near" the current location
 */
void inven_drop(int item, int amt)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    object_type* o_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    char o_name[120];

    /* Get the original object */
    o_ptr = &inventory[item];

    /* Error check */
    if (amt <= 0)
        return;

    /* Not too many */
    if (amt > o_ptr->number)
        amt = o_ptr->number;

    /* Take off equipment */
    if (item >= INVEN_WIELD)
    {
        /* Take off first */
        item = inven_takeoff(item, amt);

        if (item < 0)
            return;

        /* Get the original object */
        o_ptr = &inventory[item];
    }

    /* Get local object */
    i_ptr = &object_type_body;

    /* Obtain local object */
    object_copy(i_ptr, o_ptr);

    /* Modify quantity */
    i_ptr->number = amt;

    /* Describe local object */
    object_desc(o_name, sizeof(o_name), i_ptr, true, 3);

    /* Message */
    msg_format("You drop %s (%c).", o_name, index_to_label(item));

    /* Drop it near the player */
    drop_near(i_ptr, 0, py, px);

    /* Modify, Describe, Optimize */
    inven_item_increase(item, -amt);
    inven_item_describe(item);
    inven_item_optimize(item);
}

/*
 * Combine items in the pack
 *
 * Note special handling of the "overflow" slot
 */
void combine_pack(void)
{
    int i, j, k;

    object_type* o_ptr;
    object_type* j_ptr;

    bool flag = false;

    /* Combine the pack (backwards) */
    for (i = INVEN_PACK; i > 0; i--)
    {
        /* Get the item */
        o_ptr = &inventory[i];

        /* Skip empty items */
        if (!o_ptr->k_idx)
            continue;

        /* Scan the items above that item */
        for (j = 0; j < i; j++)
        {
            /* Get the item */
            j_ptr = &inventory[j];

            /* Skip empty items */
            if (!j_ptr->k_idx)
                continue;

            /* Can we drop "o_ptr" onto "j_ptr"? */
            if (object_similar(j_ptr, o_ptr))
            {
                /* Take note */
                flag = true;

                /* Add together the item counts */
                object_absorb(j_ptr, o_ptr);

                /* Window stuff */
                p_ptr->window |= (PW_INVEN);

                if (o_ptr->number == 0)
                {
                    /* One object is gone */
                    p_ptr->inven_cnt--;

                    /* Slide everything down */
                    for (k = i; k < INVEN_PACK; k++)
                    {
                        /* Hack -- slide object */
                        memcpy(&inventory[k], &inventory[k + 1], sizeof(object_type));
                    }

                    /* Hack -- wipe hole */
                    object_wipe(&inventory[k]);

                    /* Done */
                    break;
                }
            }
        }
    }

    /* Message */
    if (flag)
        msg_print("You combine some items in your pack.");
}

/*
 * Reorder items in the pack
 *
 * Note special handling of the "overflow" slot
 */
void reorder_pack(bool display_message)
{
    int i, j, k;

    s32b o_value;
    s32b j_value;

    object_type* o_ptr;
    object_type* j_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    bool flag = false;

    /* Re-order the pack (forwards) */
    for (i = 0; i < INVEN_PACK; i++)
    {
        /* Mega-Hack -- allow "proper" over-flow */
        if ((i == INVEN_PACK) && (p_ptr->inven_cnt == INVEN_PACK))
            break;

        /* Get the item */
        o_ptr = &inventory[i];

        /* Skip empty slots */
        if (!o_ptr->k_idx)
            continue;

        /* Get the "value" of the item */
        o_value = object_value(o_ptr);

        /* Scan every occupied slot */
        for (j = 0; j < INVEN_PACK; j++)
        {
            /* Get the item already there */
            j_ptr = &inventory[j];

            /* Use empty slots */
            if (!j_ptr->k_idx)
                break;

            /* Objects sort by decreasing type */
            if (o_ptr->tval > j_ptr->tval)
                break;
            if (o_ptr->tval < j_ptr->tval)
                continue;

            /* Non-aware (flavored) items always come last */
            if (!object_aware_p(o_ptr))
                continue;
            if (!object_aware_p(j_ptr))
                break;

            /* Objects sort by increasing sval */
            if (o_ptr->sval < j_ptr->sval)
                break;
            if (o_ptr->sval > j_ptr->sval)
                continue;

            // This next bit is complicated: identified art > pseudo art >
            // identified special > pseudo special > other

            /* Identified artefacts beat the rest */
            if (!(object_known_p(o_ptr) && artefact_p(o_ptr))
                && (object_known_p(j_ptr) && artefact_p(j_ptr)))
                continue;
            if ((object_known_p(o_ptr) && artefact_p(o_ptr))
                && !(object_known_p(j_ptr) && artefact_p(j_ptr)))
                break;

            /* Then pseudo-identified {artefact} */
            if (!(!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr))
                && (!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr)))
                continue;
            if ((!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr))
                && !(!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr)))
                break;

            /* Then identified specials */
            if (!(object_known_p(o_ptr) && ego_item_p(o_ptr))
                && (object_known_p(j_ptr) && ego_item_p(j_ptr)))
                continue;
            if ((object_known_p(o_ptr) && ego_item_p(o_ptr))
                && !(object_known_p(j_ptr) && ego_item_p(j_ptr)))
                break;

            /* Then pseudo-identified {special} */
            if (!(!object_known_p(o_ptr) && special_pseudo_p(o_ptr))
                && (!object_known_p(j_ptr) && special_pseudo_p(j_ptr)))
                continue;
            if ((!object_known_p(o_ptr) && special_pseudo_p(o_ptr))
                && !(!object_known_p(j_ptr) && special_pseudo_p(j_ptr)))
                break;

            /* Lites sort by decreasing fuel */
            if (o_ptr->tval == TV_LIGHT)
            {
                if (o_ptr->timeout > j_ptr->timeout)
                    break;
                if (o_ptr->timeout < j_ptr->timeout)
                    continue;
            }

            /* Determine the "value" of the pack item */
            j_value = object_value(j_ptr);

            /* Objects sort by decreasing value */
            if (o_value > j_value)
                break;
            if (o_value < j_value)
                continue;

            /* Objects sort by increasing weight */
            if (o_ptr->weight < j_ptr->weight)
                break;
            if (o_ptr->weight > j_ptr->weight)
                continue;
        }

        /* Never move down */
        if (j >= i)
            continue;

        /* Take note */
        flag = true;

        /* Get local object */
        i_ptr = &object_type_body;

        /* Save a copy of the moving item */
        object_copy(i_ptr, &inventory[i]);

        /* Slide the objects */
        for (k = i; k > j; k--)
        {
            /* Slide the item */
            object_copy(&inventory[k], &inventory[k - 1]);
        }

        /* Insert the moving item */
        object_copy(&inventory[j], i_ptr);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN);

        handle_stuff();
    }

    /* Message */
    if (flag && display_message)
        msg_print("You reorder some items in your pack.");
}

/* File: object-slot.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "angband.h"
#include "object/object-slot.h"
#include "supplies.h"

/*
 * Convert an inventory index into a one character label.
 *
 * Note that the label does NOT distinguish inven/equip.
 */
char index_to_label(int i)
{
    if (i < INVEN_WIELD)
    {
        int offset = (supplies_entry_count() > 0) ? 1 : 0;
        return (I2A(i + offset));
    }

    return (I2A(i - INVEN_WIELD));
}

s16b label_to_inven(int c)
{
    int i;

    i = (islower((unsigned char)c) ? A2I(c) : -1);

    if (supplies_entry_count() > 0)
    {
        if (c == supplies_label_char())
            return SUPPLIES_INDEX;
        i -= 1;
    }

    if ((i < 0) || (i >= INVEN_PACK))
        return (-1);

    if (!inventory[i].k_idx)
        return (-1);

    return (i);
}

s16b label_to_equip(int c)
{
    int i;

    i = (islower((unsigned char)c) ? A2I(c) : -1) + INVEN_WIELD;

    if ((i < INVEN_WIELD) || (i >= INVEN_TOTAL))
        return (-1);

    if (!inventory[i].k_idx)
        return (-1);

    return (i);
}

s16b wield_slot(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
        return (INVEN_WIELD);

    case TV_BOW:
        return (INVEN_BOW);

    case TV_STAFF:
        return (INVEN_STAFF);

    case TV_HORN:
        return (INVEN_HORN);

    case TV_RING:
        if (!inventory[INVEN_RIGHT].k_idx)
            return (INVEN_RIGHT);
        return (INVEN_LEFT);

    case TV_AMULET:
        return (INVEN_NECK);

    case TV_LIGHT:
        return (INVEN_LITE);

    case TV_MAIL:
    case TV_SOFT_ARMOR:
        return (INVEN_BODY);

    case TV_CLOAK:
        return (INVEN_OUTER);

    case TV_SHIELD:
        return (INVEN_ARM);

    case TV_CROWN:
    case TV_HELM:
        return (INVEN_HEAD);

    case TV_GLOVES:
        return (INVEN_HANDS);

    case TV_BOOTS:
        return (INVEN_FEET);

    case TV_ARROW:
        if (object_similar(&inventory[INVEN_QUIVER1], o_ptr))
            return (INVEN_QUIVER1);
        if (object_similar(&inventory[INVEN_QUIVER2], o_ptr))
            return (INVEN_QUIVER2);
        if (!inventory[INVEN_QUIVER2].k_idx && inventory[INVEN_QUIVER1].k_idx)
            return (INVEN_QUIVER2);
        return (INVEN_QUIVER1);
    }

    return (-1);
}

cptr describe_empty_slot(int i)
{
    cptr p;

    switch (i)
    {
    case INVEN_WIELD: p = "(no weapon)"; break;
    case INVEN_BOW: p = "(no bow)"; break;
    case INVEN_STAFF: p = "(no walking staff)"; break;
    case INVEN_LEFT: p = "(no left ring)"; break;
    case INVEN_RIGHT: p = "(no right ring)"; break;
    case INVEN_NECK: p = "(no amulet)"; break;
    case INVEN_LITE: p = "(no light source)"; break;
    case INVEN_BODY: p = "(no body armour)"; break;
    case INVEN_OUTER: p = "(no cloak)"; break;
    case INVEN_ARM: p = "(no shield)"; break;
    case INVEN_HEAD: p = "(no helmet)"; break;
    case INVEN_HANDS: p = "(no gloves)"; break;
    case INVEN_FEET: p = "(no boots)"; break;
    case INVEN_QUIVER1: p = "(empty 1st quiver)"; break;
    case INVEN_QUIVER2: p = "(empty 2nd quiver)"; break;
    case INVEN_HORN: p = "(no horn)"; break;
    default: p = "(empty slot)"; break;
    }

    return (p);
}

cptr mention_use(int i)
{
    cptr p;

    switch (i)
    {
    case INVEN_WIELD: p = "Wielding"; break;
    case INVEN_BOW: p = "Shooting"; break;
    case INVEN_STAFF: p = "Walking staff"; break;
    case INVEN_LEFT: p = "Left ring"; break;
    case INVEN_RIGHT: p = "Right ring"; break;
    case INVEN_NECK: p = "Around neck"; break;
    case INVEN_LITE: p = "Light"; break;
    case INVEN_BODY: p = "On body"; break;
    case INVEN_OUTER: p = "About body"; break;
    case INVEN_ARM: p = "Off-hand"; break;
    case INVEN_HEAD: p = "On head"; break;
    case INVEN_HANDS: p = "On hands"; break;
    case INVEN_FEET: p = "On feet"; break;
    case INVEN_QUIVER1: p = "1st quiver"; break;
    case INVEN_QUIVER2: p = "2nd quiver"; break;
    case INVEN_HORN: p = "Horn"; break;
    default: p = "In pack"; break;
    }

    return (p);
}

cptr describe_use(int i)
{
    cptr p;

    switch (i)
    {
    case INVEN_WIELD: p = "wielding"; break;
    case INVEN_BOW: p = "wielding"; break;
    case INVEN_STAFF: p = "using as a walking staff"; break;
    case INVEN_LEFT: p = "wearing on your left hand"; break;
    case INVEN_RIGHT: p = "wearing on your right hand"; break;
    case INVEN_NECK: p = "wearing around your neck"; break;
    case INVEN_LITE: p = "using to light the way"; break;
    case INVEN_BODY: p = "wearing on your body"; break;
    case INVEN_OUTER: p = "wearing on your back"; break;
    case INVEN_ARM: p = "wearing on your arm"; break;
    case INVEN_HEAD: p = "wearing on your head"; break;
    case INVEN_HANDS: p = "wearing on your hands"; break;
    case INVEN_FEET: p = "wearing on your feet"; break;
    case INVEN_QUIVER1: p = "carrying in your quiver"; break;
    case INVEN_QUIVER2: p = "carrying in your quiver"; break;
    case INVEN_HORN: p = "carrying at your side"; break;
    default: p = "carrying in your pack"; break;
    }

    return p;
}

bool item_tester_okay(const object_type* o_ptr)
{
    bool in_inventory = (o_ptr >= inventory) && (o_ptr < inventory + INVEN_TOTAL);

    if (throw_slot_menu_active && in_inventory)
    {
        int idx = (int)(o_ptr - inventory);

        if (!throw_slot_enabled[idx])
            return (false);

        if (!o_ptr->k_idx)
            return (true);
    }

    if (!o_ptr->k_idx)
    {
        if (item_tester_full && in_inventory && (o_ptr >= inventory + INVEN_WIELD))
            return (true);
        return (false);
    }

    if (!in_inventory && object_is_searched_skeleton(o_ptr))
        return false;

    if (item_tester_tval)
    {
        if (!(item_tester_tval == o_ptr->tval))
            return (false);
    }

    if (item_tester_hook)
    {
        if (!(*item_tester_hook)(o_ptr))
            return (false);
    }

    return (true);
}

int scan_floor(int* items, int size, int y, int x, int mode)
{
    int this_o_idx, next_o_idx;
    int num = 0;

    if (!in_bounds(y, x))
        return (0);

    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr = &o_list[this_o_idx];

        next_o_idx = o_ptr->next_o_idx;

        if ((mode & 0x01) && !item_tester_okay(o_ptr))
            continue;

        if ((mode & 0x02) && !o_ptr->marked)
            continue;

        items[num++] = this_o_idx;

        if (num >= size)
            break;
    }

    return (num);
}

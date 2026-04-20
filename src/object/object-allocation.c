/* File: object-allocation.c */
/*
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

s16b o_max = 1;
s16b o_cnt = 0;
object_type* o_list;
object_type* inventory;

s16b alloc_kind_size;
alloc_entry* alloc_kind_table;
s16b alloc_ego_size;
alloc_entry* alloc_ego_table;
s16b alloc_race_size;
alloc_entry* alloc_race_table;

s16b object_level;
byte object_generation_mode;

bool prep_object_theme(int themetype);

/*
 * Hack -- determine if a template is a damaged item
 *
 */
static bool kind_is_damaged_item(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];
    return k_ptr->flags3 & TR3_DAMAGED;
}

/*
 * Apply a "object restriction function" to the "object allocation table"
 */
void get_obj_num_prep(void)
{
    int i;

    /* Get the entry */
    alloc_entry* table = alloc_kind_table;

    /* Scan the allocation table */
    for (i = 0; i < alloc_kind_size; i++)
    {
        /* Accept objects which pass the restriction, if any */
        if (!get_obj_num_hook)
        {
            // damaged items only on skeletons
            if (kind_is_damaged_item(table[i].index))
                table[i].prob2 = 0;
            else
                table[i].prob2 = table[i].prob1;
        }
        else if ((*get_obj_num_hook)(table[i].index))
        {
            /* Accept this object */
            table[i].prob2 = table[i].prob1;
        }
        /* Do not use this object */
        else
        {
            /* Decline this object */
            table[i].prob2 = 0;
        }
    }
}

/*
 * Choose an object kind that seems "appropriate" to the given level
 *
 * This function uses the "prob2" field of the "object allocation table",
 * and various local information, to calculate the "prob3" field of the
 * same table, which is then used to choose an "appropriate" object, in
 * a relatively efficient manner.
 *
 * It is (slightly) more likely to acquire an object of the given level
 * than one of a lower level.  This is done by choosing several objects
 * appropriate to the given level and keeping the "hardest" one.
 *
 * Note that if no objects are "appropriate", then this function will
 * fail, and return zero, but this should *almost* never happen.
 * (but it does happen with certain themed items occasionally). -JG
 */
s16b get_obj_num(int level)
{
    int i, j, p;

    int k_idx;

    long value, total;

    object_kind* k_ptr;

    alloc_entry* table = alloc_kind_table;

    /* Boost level */
    if (level > 0)
    {
        /* Occasional "boost" */
        if (one_in_(GREAT_OBJ))
        {
            // most of the time, choose a new deeper depth, weighted towards the
            // current depth
            if (level < MORGOTH_DEPTH)
            {
                int x = rand_range(level + 1, MORGOTH_DEPTH);
                int y = rand_range(level + 1, MORGOTH_DEPTH);

                level = MIN(x, y);
            }

            // but if it was already very deep, just increment it
            else
            {
                level++;
            }
        }
    }

    /* Reset total */
    total = 0L;

    /* Process probabilities */
    for (i = 0; i < alloc_kind_size; i++)
    {
        /* Objects are sorted by depth */
        if (table[i].level > level)
            break;

        /* Default */
        table[i].prob3 = 0;

        /* Get the index */
        k_idx = table[i].index;

        /* Get the actual kind */
        k_ptr = &k_info[k_idx];

        /* Hack -- prevent embedded chests*/
        if ((object_generation_mode == OB_GEN_MODE_CHEST)
            && (k_ptr->tval == TV_CHEST))
            continue;

        /* Accept */
        table[i].prob3 = table[i].prob2;

        /* Total */
        total += table[i].prob3;
    }

    /* No legal objects */
    if (total <= 0)
        return (0);

    /* Pick an object */
    value = rand_int(total);

    /* Find the object */
    for (i = 0; i < alloc_kind_size; i++)
    {
        /* Found the entry */
        if (value < table[i].prob3)
            break;

        /* Decrement */
        value = value - table[i].prob3;
    }

    /* Power boost */
    p = rand_int(100);

    /* Try for a "better" object once (50%) or twice (10%) */
    if (p < 60)
    {
        /* Save old */
        j = i;

        /* Pick a object */
        value = rand_int(total);

        /* Find the monster */
        for (i = 0; i < alloc_kind_size; i++)
        {
            /* Found the entry */
            if (value < table[i].prob3)
                break;

            /* Decrement */
            value = value - table[i].prob3;
        }

        /* Keep the "best" one */
        if (table[i].level < table[j].level)
            i = j;
    }

    /* Try for a "better" object twice (10%) */
    if (p < 10)
    {
        /* Save old */
        j = i;

        /* Pick a object */
        value = rand_int(total);

        /* Find the object */
        for (i = 0; i < alloc_kind_size; i++)
        {
            /* Found the entry */
            if (value < table[i].prob3)
                break;

            /* Decrement */
            value = value - table[i].prob3;
        }

        /* Keep the "best" one */
        if (table[i].level < table[j].level)
            i = j;
    }

    /* Result */
    return (table[i].index);
}

/*
 * Hack -- determine if a template is footwear.
 *
 */
static bool kind_is_boots(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* footwear -- */
    case TV_BOOTS:
    {
        return (true);
    }
    }

    /* Assume not footwear */
    return (false);
}

/*
 * Hack -- determine if a template is headgear.
 *
 */
static bool kind_is_headgear(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Headgear -- Suitable */
    case TV_HELM:
    case TV_CROWN:
    {
        return (true);
    }
    }

    /* Assume not headgear */
    return (false);
}

/*
 * Hack -- determine if a template is armor.
 *
 */
static bool kind_is_armor(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Armor -- suitable */
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    {
        return (true);
    }
    }

    /* Assume not armor */
    return (false);
}

/*
 * Hack -- determine if a template is gloves.
 *
 */
static bool kind_is_gloves(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Gloves -- suitable */
    case TV_GLOVES:
    {
        return (true);
    }
    }

    /* Assume not suitable  */
    return (false);
}

/*
 * Hack -- determine if a template is a cloak.
 *
 */
static bool kind_is_cloak(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
        /* Cloaks -- suitable */

    case TV_CLOAK:
    {
        return (true);
    }
    }

    /* Assume not a suitable  */
    return (false);
}

/*
 * Hack -- determine if a template is a shield.
 *
 */
static bool kind_is_shield(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* shield -- suitable */
    case TV_SHIELD:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

/*
 * Hack -- determine if a template is a bow/arrow.
 */

static bool kind_is_bow(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* All bows and arrows are suitable  */
    case TV_BOW:
    {
        return (true);
    }
    }

    /* Assume not suitable  */
    return (false);
}

/*
 * Hack -- determine if a template is a "good" digging tool
 *
 */
static bool kind_is_digging_tool(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Diggers -- Good */
    case TV_DIGGING:
    {
        return (true);
    }
    }

    /* Assume not good */
    return (false);
}

/*
 * Hack -- determine if a template is a edged weapon.
 */
static bool kind_is_edged(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Edged Weapons -- suitable */
    case TV_SWORD:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

/*
 * Hack -- determine if a template is a polearm.
 */
static bool kind_is_polearm(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Weapons -- suitable */
    case TV_POLEARM:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

/*
 * Hack -- determine if a template is a weapon.
 */
static bool kind_is_weapon(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Weapons -- suitable */
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

/*
 * Set the object theme
 */

/*
 * This is an imcomplete list of themes.  Returns false if theme not found.
 * Used primarily for Randarts
 */
bool prep_object_theme(int themetype)
{
    /*get the store creation mode*/
    switch (themetype)
    {
    case DROP_TYPE_SHIELD:
    {
        get_obj_num_hook = kind_is_shield;
        break;
    }
    case DROP_TYPE_WEAPON:
    {
        get_obj_num_hook = kind_is_weapon;
        break;
    }
    case DROP_TYPE_EDGED:
    {
        get_obj_num_hook = kind_is_edged;
        break;
    }
    case DROP_TYPE_POLEARM:
    {
        get_obj_num_hook = kind_is_polearm;
        break;
    }
    case DROP_TYPE_ARMOR:
    {
        get_obj_num_hook = kind_is_armor;
        break;
    }
    case DROP_TYPE_BOOTS:
    {
        get_obj_num_hook = kind_is_boots;
        break;
    }
    case DROP_TYPE_BOW:
    {
        get_obj_num_hook = kind_is_bow;
        break;
    }
    case DROP_TYPE_CLOAK:
    {
        get_obj_num_hook = kind_is_cloak;
        break;
    }
    case DROP_TYPE_GLOVES:
    {
        get_obj_num_hook = kind_is_gloves;
        break;
    }
    case DROP_TYPE_HEADGEAR:
    {
        get_obj_num_hook = kind_is_headgear;
        break;
    }
    case DROP_TYPE_DIGGING:
    {
        get_obj_num_hook = kind_is_digging_tool;

        break;
    }
    case DROP_TYPE_DAMAGED:
    {
        get_obj_num_hook = kind_is_damaged_item;

        break;
    }

    default:
        return (false);
    }

    /*prepare the allocation table*/
    get_obj_num_prep();

    return (true);
}

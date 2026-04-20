/* File: smithing-materials.c */
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
#include "smithing/smithing-internal.h"

bool melt_metal_item(int item_num)
{
    int number = 0;
    int item, i;
    u32b f1, f2, f3;

    for (item = 0; item < INVEN_TOTAL; item++)
    {
        object_type* o_ptr = &inventory[item];

        object_flags(o_ptr, &f1, &f2, &f3);

        /* Skip metal items that can't be melted (Gamil-forged) */
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON)) && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            number += 1;
        }

        if (number == item_num)
        {
            int slots_needed = o_ptr->weight / 99;
            int empty_slots = 0;

            // Equipments needs an extra slot
            if (item >= INVEN_WIELD)
                slots_needed++;

            // Count empty slots
            for (i = INVEN_PACK - 1; i > 0; i--)
            {
                if (!(&inventory[i])->k_idx)
                    empty_slots++;
            }

            if (empty_slots < slots_needed)
            {
                msg_print("You do not have enough room in your pack.");
                if (slots_needed - empty_slots == 1)
                {
                    msg_print("You must free up another slot.");
                }
                else
                {
                    msg_format("You must free up %d more slots.",
                        slots_needed - empty_slots);
                }
                return (false);
            }

            if (get_check("Are you sure you wish to melt this item down? "))
            {
                int slot;
                object_type* i_ptr;
                object_type object_type_body;
                int metal_sval;

                // Determine which metal type to create
                if (f3 & TR3_STAR_IRON)
                    metal_sval = SV_METAL_STAR_IRON;
                else
                    metal_sval = SV_METAL_MITHRIL;

                // Get local object
                i_ptr = &object_type_body;

                // Prepare the base object for the metal
                object_prep(i_ptr, lookup_kind(TV_METAL, metal_sval));

                // set the appropriate quantity
                i_ptr->number = o_ptr->weight;

                // remove the item
                inven_item_increase(item, -1);
                inven_item_describe(item);
                inven_item_optimize(item);
                window_stuff();

                // give the mithril to the player...

                // if there is too much, then break it up
                while (i_ptr->number > 99)
                {
                    object_type* i_ptr2;
                    object_type object_type_body2;

                    // Get local object
                    i_ptr2 = &object_type_body2;

                    // decrease the main stack
                    i_ptr->number -= 99;

                    // Prepare the base object for the metal
                    object_prep(
                        i_ptr2, lookup_kind(TV_METAL, metal_sval));

                    // increase the new stack
                    i_ptr2->number = 99;

                    // give it to the player
                    slot = inven_carry(i_ptr2, true);
                    if ((slot >= 0) && (slot < INVEN_TOTAL))
                    {
                        inven_item_optimize(slot);
                        inven_item_describe(slot);
                    }
                    else
                    {
                        drop_near(i_ptr2, 0, p_ptr->py, p_ptr->px);
                        msg_print("Some metal falls to the floor.");
                    }
                    window_stuff();
                }

                // now give the last stack of mithril to the player
                slot = inven_carry(i_ptr, true);
                if ((slot >= 0) && (slot < INVEN_TOTAL))
                {
                    inven_item_optimize(slot);
                    inven_item_describe(slot);
                }
                else
                {
                    drop_near(i_ptr, 0, p_ptr->py, p_ptr->px);
                    msg_print("Some metal falls to the floor.");
                }
                window_stuff();

                return (true);
            }

            else
                return (false);
        }
    }

    return (false);
}

int meltable_metal_items_carried(void)
{
    int number = 0;
    int item;
    u32b f1, f2, f3;

    for (item = 0; item < INVEN_TOTAL; item++)
    {
        object_type* o_ptr = &inventory[item];

        object_flags(o_ptr, &f1, &f2, &f3);

        /* Only count metal items that can be melted (exclude Gamil-forged) */
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON))
            && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            number += 1;
        }
    }

    return (number);
}

static int metal_carried(byte sval)
{
    int w = 0;
    int item;

    for (item = 0; item < INVEN_WIELD; item++)
    {
        object_type* o_ptr = &inventory[item];

        if ((o_ptr->tval == TV_METAL) && (o_ptr->sval == sval))
        {
            w += o_ptr->number;
        }
    }

    return (w);
}

int mithril_carried(void)
{
    return metal_carried(SV_METAL_MITHRIL);
}

int star_iron_carried(void)
{
    return metal_carried(SV_METAL_STAR_IRON);
}

static void use_metal(byte sval, int cost)
{
    int item;

    for (item = INVEN_WIELD - 1; item >= 0 && cost > 0; item--)
    {
        object_type* o_ptr = &inventory[item];

        if ((o_ptr->tval == TV_METAL) && (o_ptr->sval == sval))
        {
            int use = MIN(o_ptr->number, cost);
            inven_item_increase(item, -use);
            inven_item_describe(item);
            inven_item_optimize(item);
            cost -= use;
        }
    }
}

void use_mithril(int cost)
{
    use_metal(SV_METAL_MITHRIL, cost);
}

void use_star_iron(int cost)
{
    use_metal(SV_METAL_STAR_IRON, cost);
}

/*
 * Determines how many uses are left for a given forge.
 */
int forge_uses(int y, int x)
{
    byte feat = cave_feat[y][x];

    if (!cave_forge_bold(y, x))
        return (0);

    if (feat <= FEAT_FORGE_NORMAL_TAIL)
        return (feat - FEAT_FORGE_NORMAL_HEAD);
    if (feat <= FEAT_FORGE_GOOD_TAIL)
        return (feat - FEAT_FORGE_GOOD_HEAD);
    else
        return (feat - FEAT_FORGE_UNIQUE_HEAD);
}

/*
 * Determines how high a bonus is provided by a given forge.
 */
int forge_bonus(int y, int x)
{
    byte feat = cave_feat[y][x];

    if (!cave_forge_bold(y, x))
        return (0);

    if (feat <= FEAT_FORGE_NORMAL_TAIL)
        return (0);
    if (feat <= FEAT_FORGE_GOOD_TAIL)
        return (3);
    else
        return (7);
}

/*
 * Determines the difficulty modifier for pvals.
 *
 * The marginal difficulty of increasing a pval increases by 1 each time, if the
 * base is up to 5, by 2 each time if the base is 6--10, and so on.
 */

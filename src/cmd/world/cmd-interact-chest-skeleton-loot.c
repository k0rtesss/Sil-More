/* File: cmd-interact-chest-skeleton-loot.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cmd-interact-chest-internal.h"

static void prep_skeleton_food(object_type* o_ptr, byte skeleton_sval)
{
    switch (skeleton_sval)
    {
    case SV_SKELETON_ELF:
        object_prep(o_ptr, lookup_kind(TV_FOOD,
            one_in_(2) ? SV_FOOD_LEMBAS : SV_FOOD_BREAD));
        break;
    case SV_SKELETON_ORC:
        object_prep(o_ptr, lookup_kind(TV_FOOD, SV_FOOD_MEAT));
        break;
    case SV_SKELETON_HUMAN:
    default:
        object_prep(o_ptr, lookup_kind(TV_FOOD,
            one_in_(2) ? SV_FOOD_BREAD : SV_FOOD_MEAT));
        break;
    }

    object_known(o_ptr);
}

static bool prep_skeleton_light(object_type* o_ptr)
{
    int depth = 1;

    if (p_ptr && p_ptr->depth > 0)
        depth = p_ptr->depth;

    object_wipe(o_ptr);
    if (!drop_generate_object(
            depth, DROP_QUALITY_NORMAL, DROP_TYPE_SIMPLE_LIGHTS, false, o_ptr))
        return false;

    object_known(o_ptr);
    return true;
}

static bool skeleton_damaged_item_allowed(byte skeleton_sval, const object_type* o_ptr)
{
    chest_alignment_type alignment = chest_item_alignment(o_ptr);

    if (alignment == CHEST_ALIGNMENT_INVALID)
        return false;
    if (skeleton_sval == SV_SKELETON_ELF && alignment == CHEST_ALIGNMENT_EVIL)
        return false;
    if (skeleton_sval == SV_SKELETON_ORC && alignment == CHEST_ALIGNMENT_NOBLE)
        return false;

    return true;
}

static bool generate_skeleton_damaged_item(object_type* o_ptr, byte skeleton_sval,
    bool* no_item_generated)
{
    bool old_allow_noble = drop_allow_noble;
    bool old_allow_evil = drop_allow_evil;
    bool generated_any = false;

    if (no_item_generated)
        *no_item_generated = false;

    drop_allow_noble = (skeleton_sval == SV_SKELETON_HUMAN
        || skeleton_sval == SV_SKELETON_ELF);
    drop_allow_evil = (skeleton_sval == SV_SKELETON_HUMAN
        || skeleton_sval == SV_SKELETON_ORC);

    for (int attempt = 0; attempt < 50; attempt++)
    {
        object_wipe(o_ptr);
        if (!make_object(o_ptr, DROP_QUALITY_NORMAL, DROP_TYPE_DAMAGED))
            continue;

        generated_any = true;

        if (skeleton_damaged_item_allowed(skeleton_sval, o_ptr))
        {
            drop_allow_noble = old_allow_noble;
            drop_allow_evil = old_allow_evil;
            return false;
        }
    }

    object_wipe(o_ptr);
    drop_allow_noble = old_allow_noble;
    drop_allow_evil = old_allow_evil;
    if (no_item_generated)
        *no_item_generated = !generated_any;
    return true;
}

/*
 * Attempt to search the given skeleton at the given location
 *
 * Assumes there is no monster blocking the destination
 */
void cmd_interact_search_skeleton_impl(int y, int x, s16b o_idx)
{
    bool search_failed = true;
    bool auto_carry_food = false;
    bool no_item_generated = false;
    object_type* o_ptr = &o_list[o_idx];

    // Searched already
    if (o_ptr->pval == 0)
    {
        return;
    }

    object_generation_mode = OB_GEN_MODE_SKELETON;

    cmd_interact_chest_maybe_show_skeleton_note(o_ptr->sval, y, x);

    object_type* i_ptr;
    object_type object_type_body;
    i_ptr = &object_type_body;

    int roll = rand_int(100);

    if (roll < 20)
    {
        prep_skeleton_food(i_ptr, o_ptr->sval);
        auto_carry_food = true;
        search_failed = false;
    }
    else if (roll < 40)
    {
        search_failed = !prep_skeleton_light(i_ptr);
    }
    else if (roll < 50)
    {
        search_failed = generate_skeleton_damaged_item(
            i_ptr, o_ptr->sval, &no_item_generated);
    }
    else
    {
        search_failed = true;
    }

    o_ptr->pval = 0;

    object_generation_mode = OB_GEN_MODE_NORMAL;

    if (search_failed)
    {
        if (no_item_generated)
            msg_print("You sift the bones, but they yield only dust.");
        else
            msg_print("You failed to find anything among the bones.");
    }
    else
    {
        if (i_ptr->k_idx)
        {
            int slot = -1;
            char o_name[80];

            if (i_ptr->tval != TV_ARROW)
            {
                i_ptr->number = 1;
            }
            else
            {
                i_ptr->number = dieroll(4) + 2;
                msg_format("You gather up %d arrows.", i_ptr->number);
            }

            object_desc(o_name, sizeof(o_name), i_ptr, true, 0);

            if (auto_carry_food)
            {
                slot = inven_carry(i_ptr, true);

                if (slot == SUPPLIES_INDEX)
                {
                    char label = supplies_label_char();
                    if (!label)
                        label = 'a';
                    msg_format("You recover %s and add it to your supplies (%c).", o_name, label);
                }
                else if (slot >= 0)
                {
                    msg_format("You recover %s (%c).", o_name, index_to_label(slot));
                }
                else
                {
                    msg_format("You recover %s from the bones.", o_name);
                    drop_near(i_ptr, -1, y, x);
                }
            }
            else
            {
                msg_format("You find %s among the bones.", o_name);
                drop_near(i_ptr, -1, y, x);
            }

            /* Break the truce if creatures see */
            break_truce(false);
        }
    }
}


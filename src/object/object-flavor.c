/* File: object-flavor.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "object/object-flavor.h"

static void flavor_assign_fixed(void)
{
    int i, j;

    for (i = 0; i < z_info->flavor_max; i++)
    {
        flavor_type* flavor_ptr = &flavor_info[i];

        /* Skip random flavors */
        if (flavor_ptr->sval == SV_UNKNOWN)
            continue;

        for (j = 0; j < z_info->k_max; j++)
        {
            /* Skip other objects */
            if ((k_info[j].tval == flavor_ptr->tval)
                && (k_info[j].sval == flavor_ptr->sval))
            {
                /* Store the flavor index */
                k_info[j].flavor = i;
            }
        }
    }
}

static void flavor_assign_random(byte tval)
{
    int i, j;
    int flavor_count = 0;
    int choice;

    /* Count the random flavors for the given tval */
    for (i = 0; i < z_info->flavor_max; i++)
    {
        if ((flavor_info[i].tval == tval)
            && (flavor_info[i].sval == SV_UNKNOWN))
        {
            flavor_count++;
        }
    }

    for (i = 0; i < z_info->k_max; i++)
    {
        /* Skip other object types */
        if (k_info[i].tval != tval)
            continue;

        /* Skip objects that already are flavored */
        if (k_info[i].flavor != 0)
            continue;

        /* HACK - Ordinary food is "boring" */
        if ((tval == TV_FOOD) && (k_info[i].sval >= SV_FOOD_MIN_FOOD))
            continue;

        if (!flavor_count)
            quit(format("Not enough flavors for tval %d.", tval));

        /* Select a flavor */
        choice = rand_int(flavor_count);

        /* Find and store the flavor */
        for (j = 0; j < z_info->flavor_max; j++)
        {
            /* Skip other tvals */
            if (flavor_info[j].tval != tval)
                continue;

            /* Skip assigned svals */
            if (flavor_info[j].sval != SV_UNKNOWN)
                continue;

            if (choice == 0)
            {
                /* Store the flavor index */
                k_info[i].flavor = j;

                /* Mark the flavor as used */
                flavor_info[j].sval = k_info[i].sval;

                /* One less flavor to choose from */
                flavor_count--;

                break;
            }

            choice--;
        }
    }
}

bool easter_time(void)
{
    /* Stubbed out (original implementation used time functions). */
    return false;
}

/*
 * Prepare the "variable" part of the "k_info" array.
 */
void flavor_init(void)
{
    int i;

    u64b saved_state = Rand_state_export();
    Rand_state_import(seed_flavor);

    flavor_assign_fixed();

    flavor_assign_random(TV_RING);
    flavor_assign_random(TV_AMULET);
    flavor_assign_random(TV_STAFF);
    flavor_assign_random(TV_GEM);
    flavor_assign_random(TV_HORN);
    flavor_assign_random(TV_FOOD);
    flavor_assign_random(TV_POTION);

    Rand_state_import(saved_state);

    /* Analyze every object */
    for (i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /*Skip "empty" objects*/
        if (!k_ptr->name)
            continue;

        /*No flavor yields aware*/
        if (!k_ptr->flavor || (k_ptr->tval == TV_ARROW))
            k_ptr->aware = true;

        // Easter Eggs
        if (easter_time() && (k_ptr->tval == TV_FOOD) && k_ptr->flavor)
        {
            k_ptr->flavor += 20;
        }
    }
}

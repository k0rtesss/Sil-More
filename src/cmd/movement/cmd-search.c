/* File: cmd-search.c */
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

void do_cmd_search(void)
{
    /* Allow repeated command */
    if (p_ptr->command_arg)
    {
        /* Set repeat count */
        p_ptr->command_rep = p_ptr->command_arg - 1;

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Cancel the arg */
        p_ptr->command_arg = 0;
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Search */
    search();
}

/*
 * Hack -- toggle stealth mode
 */
void do_cmd_toggle_stealth(void)
{
    /* Stop stealth mode */
    if (p_ptr->stealth_mode)
    {
        /* Clear the stealth mode flag */
        p_ptr->stealth_mode = false;

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);
    }

    /* Start stealth mode */
    else
    {
        if (p_ptr->rage)
        {
            msg_print("You are far too enraged to move stealthily.");
            return;
        }

        /* Set the stealth mode flag */
        p_ptr->stealth_mode = true;

        /* Update stuff */
        p_ptr->update |= (PU_BONUS);

        /* Redraw stuff */
        p_ptr->redraw |= (PR_STATE | PR_SPEED);
    }
}

static void search_square(int y, int x, int dist, int searching)
{
    int score = 0;
    int difficulty = 0;
    int chest_level = 0;

    object_type* o_ptr;
    int chest_trap_present = false;

    // determine if a trap is present
    for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
    {
        if ((o_ptr->tval == TV_CHEST) && chest_traps[o_ptr->pval]
            && !object_known_p(o_ptr))
        {
            chest_trap_present = true;
            chest_level = o_ptr->pval;
            break;
        }
    }

    // if searching, discover unknown adjacent squares of interest
    if (searching)
    {
        if ((dist == 1) && !(cave_info[y][x] & (CAVE_MARK)))
        {
            // mark all non-floor non-trap squares
            if (!cave_floorlike_bold(y, x))
            {
                cave_info[y][x] |= (CAVE_MARK);
            }

            // mark an object, but not the square it is in
            if (cave_o_idx[y][x] != 0)
            {
                (&o_list[cave_o_idx[y][x]])->marked = true;
            }

            /* Redraw */
            dungeon_mark_map_for_redraw();
        }
    }

    // if there is anything to notice...
    if ((cave_trap_bold(y, x) && (cave_info[y][x] & (CAVE_HIDDEN)))
        || (cave_feat[y][x] == FEAT_SECRET) || chest_trap_present)
    {
        // give up if the square is unseen and not adjacent
        if ((dist > 1) && !(cave_info[y][x] & (CAVE_SEEN)))
            return;

        // no bonus for searching on your own square
        if (dist < 1)
        {
            dist = 1;
        }

        // Determine the base score
        score = p_ptr->skill_use[S_PER] + cave_light[y][x];

        // If using the search command give a score bonus
        if (searching)
            score += 5;

        // Determine the base difficulty
        if (chest_trap_present)
        {
            difficulty = chest_level / 2;
        }
        else
        {
            if (p_ptr->depth > 0)
            {
                difficulty = p_ptr->depth / 2;
            }
            else
            {
                difficulty = 10;
            }
        }

        // Give various penalties
        if (p_ptr->blind || no_light() || p_ptr->image)
            difficulty += 5; // can't see properly
        if (p_ptr->confused)
            difficulty += 5; // confused
        if (dist == 2)
            difficulty += 2; // distance 2
        if (dist == 3)
            difficulty += 4; // distance 3
        if (dist == 4)
            difficulty += 6; // distance 4
        if cave_trap_bold (y, x)
            difficulty += 5; // dungeon trap
        if (cave_feat[y][x] == FEAT_SECRET)
            difficulty += 10; // secret door
        if (chest_trap_present)
            difficulty += 15; // chest trap
        // if (cave_info[y][x] & (CAVE_ICKY)) difficulty
        // += 2;   // inside least/lesser/greater vaults

        // Spider bane bonus helps to find webs
        if (cave_feat[y][x] == FEAT_TRAP_WEB)
        {
            difficulty -= spider_bane_bonus();
            difficulty -= artifact_spider_bane_bonus();
        }

        /* Sometimes, notice things */
        if (skill_check(PLAYER, score, difficulty, NULL) > 0)
        {
            /* Dungeon trap */
            if (cave_trap_bold(y, x))
            {
                /* Reveal the trap */
                reveal_trap(y, x);

                /* Message */
                msg_print("You have found a trap.");

                /* Disturb */
                disturb(0, 0);
            }

            /* Secret door */
            if (cave_feat[y][x] == FEAT_SECRET)
            {
                /* Message */
                msg_print("You have found a secret door.");

                /* Pick a door */
                place_closed_door(y, x);

                /* Disturb */
                disturb(0, 0);
            }

            if (chest_trap_present)
            {
                /* Message */
                msg_print("You have discovered a trap on the chest!");

                /* Know the trap */
                object_known(o_ptr);

                /* Notice it */
                disturb(0, 0);
            }
        }
    }
}

/*
 * Search for adjacent hidden things
 */
void search(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x;

    /* Search the adjacent grids */
    for (y = (py - 1); y <= (py + 1); y++)
    {
        for (x = (px - 1); x <= (px + 1); x++)
        {
            if ((x != px) || (y != py))
                search_square(y, x, 1, true);
        }
    }

    // also make the normal perception check
    perceive();
}

/*
 * Maybe notice hidden things nearby
 */
void perceive(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x, dist;

    /* Search nearby grids */
    for (y = (py - 4); y <= (py + 4); y++)
    {
        for (x = (px - 4); x <= (px + 4); x++)
        {
            if (in_bounds(y, x))
            {
                dist = distance(py, px, y, x);

                /* Search only if adjacent, player lit or permanently lit */
                if ((dist <= 1) || (p_ptr->cur_light >= dist)
                    || (cave_info[y][x] & (CAVE_GLOW)))
                {
                    /* Search only if also within four grids and in line of
                     * sight*/
                    if ((dist <= 4) && los(py, px, y, x))
                    {
                        search_square(y, x, dist, false);
                    }
                }
            }
        }
    }
}

/*
 * Check if an object is a weapon or armor that would violate the Oath of the Smith
 */

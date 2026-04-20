/* File: cave-awareness.c */
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

#include "cave/cave-internal.h"

/*
 * Track a new monster
 */
void health_track(int m_idx)
{
    /* Track a new guy */
    p_ptr->health_who = m_idx;

    /* Redraw (later) */
    p_ptr->redraw |= (PR_HEALTHBAR);
}

/*
 * Hack -- track the given monster race
 */
void monster_race_track(int r_idx)
{
    // don't track when hallucinating
    if (p_ptr->image)
        return;

    // don't track when raging
    if (p_ptr->rage)
        return;

    /* Save this monster ID */
    p_ptr->monster_race_idx = r_idx;

    /* Window stuff */
    p_ptr->window |= (PW_MONSTER);
}

/*
 * Hack -- track the given object kind
 */
void object_kind_track(int k_idx)
{
    /* Save this object ID */
    p_ptr->object_kind_idx = k_idx;

    /* Window stuff */
    p_ptr->window |= (PW_OBJECT);
}

/*
 * Something has happened to disturb the player.
 *
 * The first arg indicates a major disturbance, which affects stealth mode.
 *
 * The second arg is currently unused, but could induce output flush.
 *
 * All disturbance cancels repeated commands, resting, and running.
 */
void disturb(int stop_stealth, int unused_flag)
{
    /* Unused parameter */
    (void)unused_flag;

    /* Cancel auto-commands */
    /* p_ptr->command_new = 0; */

    /* Cancel repeated commands */
    if (p_ptr->command_rep)
    {
        /* Cancel */
        p_ptr->command_rep = 0;

        /* Redraw the state (later) */
        p_ptr->redraw |= (PR_STATE);
    }

    /* Cancel Resting */
    if (p_ptr->resting)
    {
        /* Cancel */
        p_ptr->resting = 0;

        /* Redraw the state (later) */
        p_ptr->redraw |= (PR_STATE);
    }

    /* Cancel Smithing */
    if (p_ptr->smithing)
    {
        /* Cancel */
        p_ptr->smithing = 0;

        // Display a message
        msg_print("Your work is interrupted!");

        /* Redraw the state (later) */
        p_ptr->redraw |= (PR_STATE);
    }

    /* Cancel Smithing */
    if (p_ptr->fletching)
    {
        // Display a message
        msg_print("Your work is interrupted!");

        finish_fletching(p_ptr->fletching);

        /* Cancel */
        p_ptr->fletching = 0;

        /* Redraw the state (later) */
        p_ptr->redraw |= (PR_STATE);
    }

    /* Cancel running */
    if (p_ptr->running)
    {
        /* Cancel */
        p_ptr->running = 0;

        /* Check for new panel if appropriate */
        if (center_player && run_avoid_center)
            verify_panel();
    }

    /* Cancel stealth if requested */
    if (stop_stealth && p_ptr->stealth_mode)
    {
        // signal that it will be stopped at the end of the turn
        stop_stealth_mode = true;
    }

    /* Flush the input */
    input_clear_pending();
}

/* File: cmd-interact-door.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "app/app-command.h"
#include "externs.h"
#include "item_set.h"
#include "log/log.h"
#include "platform-story-font.h"
#include "object/object-ui-enhanced.h"
#include "object/object-ui-select.h"
#include "player/killer.h"
#include "metarun.h"
#include "cmd-world.h"

/*
 * Determine if a given grid may be "opened"
 */
static bool do_cmd_open_test(int y, int x)
{
    /* Must have knowledge */
    if (!(cave_info[y][x] & (CAVE_MARK)))
    {
        /* Message */
        msg_print("You see nothing there.");

        /* Nope */
        return (false);
    }

    /* Must be a closed door */
    if (!cave_known_closed_door_bold(y, x))
    {
        /* Message */
        message(MSG_NOTHING_TO_OPEN, 0, "You see nothing there to open.");

        /* Nope */
        return (false);
    }

    /* Okay */
    return (true);
}

/*
 * Perform the basic "open" command on doors
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
bool do_cmd_open_aux(int y, int x)
{
    int score, power, difficulty;

    bool more = false;

    /* Verify legality */
    if (!do_cmd_open_test(y, x))
        return (false);

    /* Jammed door */
    if (cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x08)
    {
        /* Stuck */
        msg_print("The door appears to be stuck.");
    }

    /* Locked door */
    else if (cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x01)
    {
        /* Get the score in favour (=perception) */
        score = p_ptr->skill_use[S_PER];

        /* Determine door power based on the door power (1 to 7)*/
        power = cave_feat[y][x] - FEAT_DOOR_HEAD;

        // Base difficulty is the door power + 5
        difficulty = power + 5;

        /* Penalize some conditions */
        if (p_ptr->blind || no_light() || p_ptr->image)
            difficulty += 5;
        if (p_ptr->confused)
            difficulty += 5;

        /* Success */
        if (skill_check(PLAYER, score, difficulty, NULL) > 0)
        {
            /* Message */
            message(MSG_OPENDOOR, 0, "You have picked the lock.");

            /* Open the door */
            cave_set_feat(y, x, FEAT_OPEN);

            /* Update the visuals */
            p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
        }

        /* Failure */
        else
        {
            /* Failure */
            app_command_clear_pending();
            platform_frame_flush_events();

            /* Message */
            message(MSG_LOCKPICK_FAIL, 0, "You failed to pick the lock.");

            /* We may keep trying */
            more = true;
        }
    }

    /* Closed door */
    else
    {
        /* Open the door */
        cave_set_feat(y, x, FEAT_OPEN);

        /* Update the visuals */
        p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

        /* Sound */
        sound(MSG_OPENDOOR);
    }

    /* Result */
    return (more);
}

/*
 * Open a closed/locked/jammed door or a closed/locked chest.
 */
void do_cmd_open(void)
{
    int y, x, dir;

    s16b o_idx;

    bool more = false;

    int num_doors, num_chests;

    /* Count closed doors */
    num_doors = cmd_interact_count_feats(&y, &x, cmd_interact_is_closed, false);

    /* Count chests (locked) */
    num_chests = cmd_interact_count_chests(&y, &x, false);

    /* See if only one target */
    if ((num_doors + num_chests) == 1)
    {
        p_ptr->command_dir = cmd_interact_coords_to_dir(y, x);
    }

    else if ((num_doors + num_chests) == 0)
    {
        msg_print("There is nothing in your square (or adjacent) to open.");
        return;
    }

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
        return;

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    /* Check for chests */
    o_idx = cmd_interact_chest_check(y, x);

    /* Verify legality */
    if (!o_idx && !do_cmd_open_test(y, x))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Apply confusion */
    if (confuse_dir(&dir))
    {
        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];

        /* Check for chest */
        o_idx = cmd_interact_chest_check(y, x);
    }

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

    /* Monster */
    if (cave_m_idx[y][x] > 0)
    {
        /* Message */
        msg_print("There is a monster in the way!");

        /* Attack */
        py_attack(y, x, ATT_MAIN);
    }

    /* Chest */
    else if (o_idx)
    {
        /* Open the chest */
        more = cmd_interact_open_chest(y, x, o_idx);
    }

    /* Door */
    else
    {
        /* Open the door */
        more = do_cmd_open_aux(y, x);
    }

    /* Cancel repeat unless we may continue */
    if (!more)
        disturb(0, 0);
}

/*
 * Determine if a given grid may be "closed"
 */
static bool do_cmd_close_test(int y, int x)
{
    /* Must have knowledge */
    if (!(cave_info[y][x] & (CAVE_MARK)))
    {
        /* Message */
        msg_print("You see nothing there.");

        /* Nope */
        return (false);
    }

    /* Require open/broken door */
    if ((cave_feat[y][x] != FEAT_OPEN) && (cave_feat[y][x] != FEAT_BROKEN))
    {
        /* Message */
        msg_print("You see nothing there to close.");

        /* Nope */
        return (false);
    }

    /* Okay */
    return (true);
}

/*
 * Perform the basic "close" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
bool cmd_interact_close_aux(int y, int x)
{
    /* Verify legality */
    if (!do_cmd_close_test(y, x))
        return (false);

    /* Broken door */
    if (cave_feat[y][x] == FEAT_BROKEN)
    {
        /* Message */
        msg_print("The door appears to be broken.");
        return (false);
    }
    /* Ward the open door */
    else if (singing(SNG_THRESHOLDS))
    {
        int difficulty = (c_info[p_ptr->pcharacter].flags & UNQ_SNG_MEL) ? 15 : 0;
        int result = skill_check(
            PLAYER, ability_bonus(S_SNG, SNG_THRESHOLDS), difficulty, NULL);
        if (result > 9)
        {
            msg_print("You close the door, singing a song of trust unbroken.");
            cave_set_feat(y, x, FEAT_WARDED3);
        }
        else if (result > 0)
        {
            msg_print("You close the door, singing charms of binding.");
            cave_set_feat(y, x, FEAT_WARDED2);
        }
        else
        {
            msg_print("You close the door, singing words of warding.");
            cave_set_feat(y, x, FEAT_WARDED);
        }
    }
    else
    {
        /* Close the open door */
        cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
    }

    /* Update the visuals */
    p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

    /* Sound */
    sound(MSG_SHUTDOOR);

    /* Result */
    return (false);
}

/*
 * Close an open door.
 */
void do_cmd_close(void)
{
    int y, x, dir;

    bool more = false;

    /* Count open doors */
    if (cmd_interact_count_feats(&y, &x, cmd_interact_is_open, false) == 1)
    {
        p_ptr->command_dir = cmd_interact_coords_to_dir(y, x);
    }

    else if (cmd_interact_count_feats(&y, &x, cmd_interact_is_open, false) == 0)
    {
        msg_print("There is no adjacent door to close.");
        return;
    }

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
        return;

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    /* Verify legality */
    if (!do_cmd_close_test(y, x))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Apply confusion */
    if (confuse_dir(&dir))
    {
        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];
    }

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

    /* Monster */
    if (cave_m_idx[y][x] > 0)
    {
        /* Message */
        msg_print("There is a monster in the way!");

        /* Attack */
        py_attack(y, x, ATT_MAIN);
    }

    /* Door */
    else
    {
        /* Close door */
        more = cmd_interact_close_aux(y, x);
    }

    /* Cancel repeat unless told not to */
    if (!more)
        disturb(0, 0);
}


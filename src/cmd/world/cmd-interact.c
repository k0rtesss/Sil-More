/* File: cmd-interact.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "app/app-command.h"
#include "item_set.h"
#include "log/log.h"
#include "platform-story-font.h"
#include "object/object-ui-enhanced.h"
#include "object/object-ui-select.h"
#include "player/killer.h"
#include "metarun.h"
#include "cmd-world.h"
#include "ui/smithing/ui-smithing-screen.h"

/*
 * Determine if a grid contains a chest
 */
s16b cmd_interact_chest_check(int y, int x)
{
    s16b this_o_idx, next_o_idx = 0;

    /* Scan all objects in the grid */
    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Skip unknown chests XXX XXX */
        /* if (!o_ptr->marked) continue; */

        /* Check for chest */
        if (o_ptr->tval == TV_CHEST)
            return (this_o_idx);
    }

    /* No chest */
    return (0);
}


/*
 * Return true if the given feature is an open door
 */
bool cmd_interact_is_open(int feat) { return (feat == FEAT_OPEN); }

/*
 * Return true if the given feature is a closed door
 */
bool cmd_interact_is_closed(int feat)
{
    return (((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_DOOR_TAIL))
        || feat == FEAT_WARDED || feat == FEAT_WARDED2 || feat == FEAT_WARDED3);
}

/*
 * Return true if the given feature is a trap
 */
bool cmd_interact_is_trap(int feat)
{
    bool test_trap = false;

    if ((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))
        test_trap = true;

    return (test_trap);
}

/*
 * Return the number of doors/traps around (or under) the character.
 */
int cmd_interact_count_feats(int* y, int* x, bool (*test)(int feat), bool under)
{
    int d;
    int xx, yy;
    int count = 0; /* Count how many matches */

    /* Check around (and under) the character */
    for (d = 0; d < 9; d++)
    {
        /* if not searching under player continue */
        if ((d == 8) && !under)
            continue;

        /* Extract adjacent (legal) location */
        yy = p_ptr->py + ddy_ddd[d];
        xx = p_ptr->px + ddx_ddd[d];

        /* Paranoia */
        if (!in_bounds_fully(yy, xx))
            continue;

        /* Must have knowledge */
        if (!(cave_info[yy][xx] & (CAVE_MARK)))
            continue;

        /* Not looking for this feature */
        if (!((*test)(cave_feat[yy][xx])))
            continue;

        /* Count it */
        ++count;

        /* Remember the location of the last door found */
        *y = yy;
        *x = xx;
    }

    /* All done */
    return count;
}

/*
 * Return the number of chests around (or under) the character.
 * If requested, count only trapped chests.
 */
int cmd_interact_count_chests(int* y, int* x, bool trapped)
{
    int d, count, o_idx;

    object_type* o_ptr;

    /* Count how many matches */
    count = 0;

    /* Check around (and under) the character */
    for (d = 0; d < 9; d++)
    {
        /* Extract adjacent (legal) location */
        int yy = p_ptr->py + ddy_ddd[d];
        int xx = p_ptr->px + ddx_ddd[d];

        /* No (visible) chest is there */
        if ((o_idx = cmd_interact_chest_check(yy, xx)) == 0)
            continue;

        /* Grab the object */
        o_ptr = &o_list[o_idx];

        /* Already open */
        if (o_ptr->pval == 0)
            continue;

        /* No (known) traps here */
        if (trapped
            && (!object_known_p(o_ptr) || (o_ptr->pval < 0)
                || !chest_traps[o_ptr->pval]))
        {
            continue;
        }

        /* Count it */
        ++count;

        /* Remember the location of the last chest found */
        *y = yy;
        *x = xx;
    }

    /* All done */
    return count;
}

/*
 * Extract a "direction" which will move one step from the player location
 * towards the given "target" location (or "5" if no motion necessary).
 */
int cmd_interact_coords_to_dir(int y, int x)
{
    return (motion_dir(p_ptr->py, p_ptr->px, y, x));
}


/*
 * Exchange places with a monster.
 */
void do_cmd_exchange(void)
{
    int y, x, dir;

    monster_type* m_ptr;
    monster_race* r_ptr;
    char m_name[80];

    if (!p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES])
    {
        msg_print(
            "You need the ability 'exchange places' to use this command.");
        return;
    }

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
        return;

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    // deal with overburdened characters
    if (p_ptr->total_weight > weight_limit() * 3 / 2)
    {
        /* Abort */
        msg_print("You are too burdened to move.");

        return;
    }

    // Can't exchange from within pits
    if (cave_pit_bold(p_ptr->py, p_ptr->px))
    {
        /* Message */
        msg_print(
            "You would have to escape the pit before being able to exchange "
            "places.");

        return;
    }
    // Can't exchange from within webs
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
    {
        /* Message */
        msg_print(
            "You would have to escape the web before being able to exchange "
            "places.");

        return;
    }
    else if ((cave_m_idx[y][x] <= 0) || !(&mon_list[cave_m_idx[y][x]])->ml)
    {
        /* Message */
        msg_print("You cannot see a monster there to exchange places with.");

        return;
    }
    else if (cave_wall_bold(y, x))
    {
        /* Message */
        msg_print("You cannot enter the wall.");

        return;
    }
    else if (cave_any_closed_door_bold(y, x))
    {
        /* Message */
        msg_print("You cannot enter the closed door.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_RUBBLE)
    {
        /* Message */
        msg_print("You cannot enter the rubble.");

        return;
    }
    else
    {
        m_ptr = &mon_list[cave_m_idx[y][x]];
        r_ptr = &r_info[m_ptr->r_idx];

        if ((r_ptr->flags1 & (RF1_NEVER_MOVE))
            || (r_ptr->flags1 & (RF1_HIDDEN_MOVE)))
        {
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            /* Message */
            msg_format("You cannot get past %s.", m_name);

            return;
        }
    }

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

    // re-check for a visible monster (in case confusion changed the move)
    if ((cave_m_idx[y][x] <= 0) || !(&mon_list[cave_m_idx[y][x]])->ml)
    {
        /* Message */
        msg_print("You cannot see a monster there to exchange places with.");

        return;
    }

    else if (cave_wall_bold(y, x))
    {
        /* Message */
        msg_print("There is a wall in the way.");

        return;
    }
    else if (cave_any_closed_door_bold(y, x))
    {
        /* Message */
        msg_print("There is a door in the way.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_RUBBLE)
    {
        /* Message */
        msg_print("There is a pile of rubble in the way.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_CHASM)
    {
        /* Message */
        msg_print("You cannot exchange places over the chasm.");

        return;
    }

    // recalculate the monster info (in case confusion changed the move)
    m_ptr = &mon_list[cave_m_idx[y][x]];
    r_ptr = &r_info[m_ptr->r_idx];
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Message */
    msg_format("You exchange places with %s.", m_name);

    // attack of opportunity
    if ((m_ptr->alertness >= ALERTNESS_ALERT) && !m_ptr->confused
        && !(r_ptr->flags2 & (RF2_MINDLESS)))
    {
        msg_print("It attacks you as you slip past.");
        make_attack_normal(m_ptr);
    }

    // Alert the monster
    make_alert(m_ptr);

    // Swap positions with the monster
    monster_swap(p_ptr->py, p_ptr->px, y, x);

    /* Set off traps */
    if (cave_trap_bold(y, x) || (cave_feat[y][x] == FEAT_CHASM))
    {
        // If it is hidden
        if (cave_info[y][x] & (CAVE_HIDDEN))
        {
            /* Reveal the trap */
            reveal_trap(y, x);
        }

        /* Hit the trap */
        hit_trap(y, x);
    }
}



/*
 * Manipulate an adjacent grid in some way
 *
 * Attack monsters, tunnel through walls, disarm traps, open doors.
 *
 * This command must always take energy, to prevent free detection
 * of invisible monsters.
 *
 * The "semantics" of this command must be chosen before the player
 * is confused, and it must be verified against the new grid.
 */
void do_cmd_alter(void)
{
    int y, x, dir;

    int feat;

    bool chest_trap = false;
    bool chest_present = false;
    bool skeleton_present = false;

    bool more = false;

    /* Get a direction */
    if (!get_rep_dir(&dir))
        return;

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    /* Original feature */
    feat = cave_feat[y][x];

    /* Must have knowledge to know feature XXX XXX */
    if (!(cave_info[y][x] & (CAVE_MARK)))
        feat = FEAT_NONE;

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

    // check for chests and chest traps
    if (cave_o_idx[y][x])
    {
        object_type* o_ptr = &o_list[cave_o_idx[y][x]];

        if (o_ptr->tval == TV_CHEST)
        {
            chest_present = true;

            if ((o_ptr->pval > 0) && chest_traps[o_ptr->pval]
                && object_known_p(o_ptr))
                chest_trap = true;
        }
        else if ((o_ptr->tval == TV_SKELETON)
            && !object_is_searched_skeleton(o_ptr))
        {
            skeleton_present = true;
        }
    }

    bool is_marked = (cave_info[y][x] & CAVE_MARK) > 0;
    bool is_visible = (cave_info[y][x] & CAVE_SEEN) > 0;

    /*Is there a monster on the space?*/
    if (cave_m_idx[y][x] > 0)
    {
        py_attack(y, x, ATT_MAIN);
    }
    // deal with players who can't see the square
    else if ((dir != 5) && !(is_marked || is_visible))
    {
        if (cave_floor_bold(y, x))
        {
            /* Oops */
            msg_print("You strike, but there is nothing there.");
        }
        else
        {
            msg_print("You hit something hard.");
            cave_info[y][x] |= (CAVE_MARK);
            dungeon_mark_map_for_redraw();
        }
    }

    /* Tunnel through walls */
    else if (cave_wall_bold(y, x))
    {
        /* Tunnel */
        cmd_interact_tunnel_aux(y, x);
    }

    /* Bash doors */
    else if (cave_known_closed_door_bold(y, x))
    {
        /* Bash */
        cmd_interact_bash_aux(y, x);
    }

    /* Disarm known dungeon traps */
    else if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
    {
        /* Disarm */
        more = cmd_interact_disarm_aux(y, x);
    }

    /* Disarm known chest traps */
    else if (chest_trap)
    {
        /* Disarm */
        more = cmd_interact_disarm_chest(y, x, cave_o_idx[y][x]);
    }

    /* Open chest with no known traps */
    else if (chest_present)
    {
        /* Disarm */
        more = cmd_interact_open_chest(y, x, cave_o_idx[y][x]);
    }

    /* Search a skeleton */
    else if (skeleton_present)
    {
        /* Disarm */
        cmd_interact_search_skeleton(y, x, cave_o_idx[y][x]);
    }

    /* Close open doors */
    else if (feat == FEAT_OPEN)
    {
        if (dir == 5)
        {
            msg_print("To close the door you would need to move out from the "
                      "doorway.");
        }
        else
        {
            /* Close */
            cmd_interact_close_aux(y, x);
        }
    }

    /* Ascend upwards stairs */
    else if ((dir == 5) && ((feat == FEAT_LESS) || (feat == FEAT_LESS_SHAFT)))
    {
        /* Ascend */
        if (get_check("Are you sure you wish to ascend? "))
            do_cmd_go_up();
    }

    /* Descend downwards stairs */
    else if ((dir == 5) && ((feat == FEAT_MORE) || (feat == FEAT_MORE_SHAFT)))
    {
        /* Descend */
        if (get_check("Are you sure you wish to descend? "))
            do_cmd_go_down();
    }

    /* Use forges */
    else if ((dir == 5) && cave_forge_bold(y, x))
    {
        /* Use forge */
        do_cmd_smithing_screen();
        more = true;

        // don't take a turn...
        p_ptr->energy_use = 0;
    }

    /* Pick up items */
    else if ((dir == 5) && (cave_o_idx[y][x]))
    {
        /* Get item */
        do_cmd_pickup();
    }

    /* Oops */
    else if (dir == 5)
    {
        /* Oops */
        msg_print("There is nothing here to use.");

        // don't take a turn...
        p_ptr->energy_use = 0;
    }

    /* Oops */
    else
    {
        /* Oops */
        msg_print("You strike, but there is nothing there.");
    }

/* Cancel repetition unless we can continue */
    if (!more)
        disturb(0, 0);
}

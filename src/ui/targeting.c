/* File: targeting.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

/*
 * Panel management and targeting helpers.
 * Split mechanically from xtra2.c during the WP13 refactor port.
 */

#include "angband.h"
#include "app/app-command.h"
#include "app/app-session.h"
#include "targeting.h"

static void targeting_direction_prompt(cptr prompt, cptr detail)
{
    app_session* session = app_session_current();

    if (!app_session_interactions_enabled(session))
        return;

    app_session_begin_interaction(session, APP_INTERACTION_KIND_PROMPT,
        APP_WAIT_REASON_TARGETING,
        APP_INTERACTION_FLAG_CAN_CONFIRM
            | APP_INTERACTION_FLAG_CAN_CANCEL);
    app_session_set_interaction_prompt(session, TERM_WHITE,
        prompt ? prompt : "");
    app_session_set_interaction_detail(session, TERM_SLATE,
        detail ? detail : "");
}

/*
 * Modify the current panel to the given coordinates, adjusting only to
 * ensure the coordinates are legal, and return true if anything done.
 *
 * Hack -- The surface should never be scrolled around.
 *
 * Note that monsters are no longer affected in any way by panel changes.
 *
 * As a total hack, whenever the current panel changes, we assume that
 * the "overhead view" window should be updated.
 */
bool modify_panel(int wy, int wx)
{
    /* Verify wy, adjust if needed */
    if (p_ptr->cur_map_hgt < SCREEN_HGT)
        wy = 0;
    else if (wy > p_ptr->cur_map_hgt - SCREEN_HGT)
        wy = p_ptr->cur_map_hgt - SCREEN_HGT;
    if (wy < 0)
        wy = 0;

    /* Verify wx, adjust if needed */
    if (p_ptr->cur_map_wid < SCREEN_WID)
        wx = 0;
    else if (wx > p_ptr->cur_map_wid - SCREEN_WID)
        wx = p_ptr->cur_map_wid - SCREEN_WID;
    if (wx < 0)
        wx = 0;

    /* React to changes */
    if ((p_ptr->wy != wy) || (p_ptr->wx != wx))
    {
        /* Save wy, wx */
        p_ptr->wy = wy;
        p_ptr->wx = wx;

        /* Redraw map */
        p_ptr->redraw |= (PR_MAP);

        /* Hack -- Window stuff */
        p_ptr->window |= (PW_OVERHEAD);

        /* Changed */
        return (true);
    }

    /* No change */
    return (false);
}

/*
 * Perform the minimum "whole panel" adjustment to ensure that the given
 * location is contained inside the current panel, and return true if any
 * such adjustment was performed.
 */
bool adjust_panel(int y, int x)
{
    int wy = p_ptr->wy;
    int wx = p_ptr->wx;

    /* Adjust as needed */
    while (y >= wy + SCREEN_HGT)
        wy += SCREEN_HGT;
    while (y < wy)
        wy -= SCREEN_HGT;

    /* Adjust as needed */
    while (x >= wx + SCREEN_WID)
        wx += SCREEN_WID;
    while (x < wx)
        wx -= SCREEN_WID;

    /* Use "modify_panel" */
    return (modify_panel(wy, wx));
}

/*
 * Change the current panel to the panel lying in the given direction.
 *
 * Return true if the panel was changed.
 */
bool change_panel(int dir)
{
    int wy = p_ptr->wy + ddy[dir] * PANEL_HGT;
    int wx = p_ptr->wx + ddx[dir] * PANEL_WID;

    /* Use "modify_panel" */
    return (modify_panel(wy, wx));
}

/*
 * Verify the current panel (relative to the player location).
 *
 * By default, when the player gets "too close" to the edge of the current
 * panel, the map scrolls one panel in that direction so that the player
 * is no longer so close to the edge.
 *
 * The "center_player" option allows the current panel to always be centered
 * around the player, which is very expensive, and also has some interesting
 * gameplay ramifications.
 */
void verify_panel(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int wy = p_ptr->wy;
    int wx = p_ptr->wx;

    bool compact_panel_y = (SCREEN_HGT < PANEL_HGT * 2);
    bool compact_panel_x = (SCREEN_WID < PANEL_WID * 2);

    int v_margin = compact_panel_y ? (SCREEN_HGT / 4) : 13;
    int h_margin = compact_panel_x ? (SCREEN_WID / 4) : 17;

    if (v_margin < 2)
        v_margin = 2;
    if (h_margin < 4)
        h_margin = 4;

    if (v_margin > (SCREEN_HGT - 1) / 2)
        v_margin = (SCREEN_HGT - 1) / 2;
    if (h_margin > (SCREEN_WID - 1) / 2)
        h_margin = (SCREEN_WID - 1) / 2;

    int v_step = compact_panel_y ? (SCREEN_HGT - 2 * v_margin) : PANEL_HGT;
    int h_step = compact_panel_x ? (SCREEN_WID - 2 * h_margin) : PANEL_WID;

    if (v_step < 1)
        v_step = 1;
    if (h_step < 1)
        h_step = 1;

    /* Scroll screen vertically when off-center */
    if (center_player && (!p_ptr->running || !run_avoid_center))
    {
        wy = py - SCREEN_HGT / 2;
    }

    // Sil-y: make this an option
    // by default it is 2 for vertical and 4 for hor
    // needs to be programmed better
    // this doesn't do quite what it says on bigscreen
    // it can end up assymmetric up/down or l/r due to panels

    /* Scroll screen vertically when near top/bottom edge */
    else if (py < wy + v_margin)
    {
        if (compact_panel_y)
            wy -= v_step;
        else
            wy = ((py - PANEL_HGT / 2) / PANEL_HGT) * PANEL_HGT;
    }
    else if (py >= wy + SCREEN_HGT - v_margin)
    {
        if (compact_panel_y)
            wy += v_step;
        else
            wy = ((py - PANEL_HGT / 2) / PANEL_HGT) * PANEL_HGT;
    }

    /* Scroll screen horizontally when off-center */
    if (center_player && (!p_ptr->running || !run_avoid_center)
        && (px != wx + SCREEN_WID / 2))
    {
        wx = px - SCREEN_WID / 2;
    }

    /* Scroll screen horizontally when near left/right edge */
    else if (px < wx + h_margin)
    {
        if (compact_panel_x)
            wx -= h_step;
        else
            wx = ((px - PANEL_WID / 2) / PANEL_WID) * PANEL_WID;
    }
    else if (px >= wx + SCREEN_WID - h_margin)
    {
        if (compact_panel_x)
            wx += h_step;
        else
            wx = ((px - PANEL_WID / 2) / PANEL_WID) * PANEL_WID;
    }

    /* Scroll if needed */
    bool panel_changed = modify_panel(wy, wx);

    /* Safety net: never allow the player to remain outside the visible panel. */
    if (!panel_contains(py, px))
    {
        if (adjust_panel(py, px))
            panel_changed = true;
    }

    if (panel_changed)
    {
        /* Optional disturb on "panel change" */
        if (!center_player)
            disturb(0, 0);
    }
}

/*
 * Angband sorting algorithm -- quick sort in place
 *
 * Note that the details of the data we are sorting is hidden,
 * and we rely on the "ang_sort_comp()" and "ang_sort_swap()"
 * function hooks to interact with the data, which is given as
 * two pointers, and which may have any user-defined form.
 */
void ang_sort_aux(void* u, void* v, int p, int q)
{
    int z, a, b;

    /* Done sort */
    if (p >= q)
        return;

    /* Pivot */
    z = p;

    /* Begin */
    a = p;
    b = q;

    /* Partition */
    while (true)
    {
        /* Slide i2 */
        while (!(*ang_sort_comp)(u, v, b, z))
            b--;

        /* Slide i1 */
        while (!(*ang_sort_comp)(u, v, z, a))
            a++;

        /* Done partition */
        if (a >= b)
            break;

        /* Swap */
        (*ang_sort_swap)(u, v, a, b);

        /* Advance */
        a++, b--;
    }

    /* Recurse left side */
    ang_sort_aux(u, v, p, b);

    /* Recurse right side */
    ang_sort_aux(u, v, b + 1, q);
}

/*
 * Angband sorting algorithm -- quick sort in place
 *
 * Note that the details of the data we are sorting is hidden,
 * and we rely on the "ang_sort_comp()" and "ang_sort_swap()"
 * function hooks to interact with the data, which is given as
 * two pointers, and which may have any user-defined form.
 */
void ang_sort(void* u, void* v, int n)
{
    /* Sort the array */
    ang_sort_aux(u, v, 0, n - 1);
}

/*** Targetting Code ***/

/*
 * Given a "source" and "target" location, extract a "direction",
 * which will move one step from the "source" towards the "target".
 *
 * Note that we use "diagonal" motion whenever possible.
 *
 * We return "5" if no motion is needed.
 */
int motion_dir(int y1, int x1, int y2, int x2)
{
    /* No movement required */
    if ((y1 == y2) && (x1 == x2))
        return (5);

    /* South or North */
    if (x1 == x2)
        return ((y1 < y2) ? 2 : 8);

    /* East or West */
    if (y1 == y2)
        return ((x1 < x2) ? 6 : 4);

    /* South-east or South-west */
    if (y1 < y2)
        return ((x1 < x2) ? 3 : 1);

    /* North-east or North-west */
    if (y1 > y2)
        return ((x1 < x2) ? 9 : 7);

    /* Paranoia */
    return (5);
}

/*
 * Extract a direction (or zero) from a character
 */
int target_dir(char ch)
{
    if (isdigit((unsigned char)ch))
    {
        return D2I(ch);
    }

    /* Preserve the prompt-local center key. */
    if (ch == 'z')
        return 5;

    return 0;
}

/*
 * Determine is a monster makes a reasonable target
 *
 * The concept of "targetting" was stolen from "Morgul" (?)
 *
 * The player can target any location, or any "target-able" monster.
 *
 * Currently, a monster is "target_able" if it is visible, and if
 * the player can hit it with a projection, and the player is not
 * hallucinating.  This allows use of "use closest target" macros.
 */
bool target_able(int m_idx)
{
    monster_type* m_ptr;

    /* No monster */
    if (m_idx <= 0)
        return (false);

    /* Get monster */
    m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* Monster must be alive */
    if (!m_ptr->r_idx)
        return (false);

    /* Monster must be visible */
    if (!m_ptr->ml)
        return (false);

    /* Monster must not be peaceful */
    if (r_ptr->flags1 & (RF1_PEACEFUL))
        return (false);

    /* Monster must be projectable */
    if (!player_can_fire_bold(m_ptr->fy, m_ptr->fx))
        return (false);

    /* Hack -- no targeting hallucinations */
    if (p_ptr->image)
        return (false);

    /* Rage and labyrinth partitions both suppress remembered-grid targeting. */
    if (!grid_info_is_available(m_ptr->fy, m_ptr->fx))
        return (false);

    /* Hack -- Never target trappers XXX XXX XXX */
    /* if (CLEAR_ATTR && (CLEAR_CHAR)) return (false); */

    /* Assume okay */
    return (true);
}

/*
 * Update (if necessary) and verify (if possible) the target.
 *
 * We return true if the target is "okay" and false otherwise.
 */
bool target_okay(int range)
{
    /* No target */
    if (!p_ptr->target_set)
        return (false);

    /* Accept some "location" targets */
    if (p_ptr->target_who == 0)
    {
        /* Never "target" the player's own grid */
        if ((p_ptr->target_row == p_ptr->py) && (p_ptr->target_col == p_ptr->px))
            return (false);

        // reject things beyond range
        if ((range > 0)
            && (distance(
                    p_ptr->py, p_ptr->px, p_ptr->target_row, p_ptr->target_col)
                > range))
            return (false);

        // accept things in LOF
        if (cave_info[p_ptr->target_row][p_ptr->target_col] & (CAVE_FIRE))
            return (true);

        // accept walls (for horn of blasting stuff)
        else if (cave_info[p_ptr->target_row][p_ptr->target_col] & (CAVE_WALL))
            return (true);

        // reject others
        else
            return (false);
    }

    /* Check "monster" targets */
    if (p_ptr->target_who > 0)
    {
        int m_idx = p_ptr->target_who;

        /* Accept reasonable targets */
        if (target_able(m_idx))
        {
            monster_type* m_ptr = &mon_list[m_idx];

            /* Get the monster location */
            p_ptr->target_row = m_ptr->fy;
            p_ptr->target_col = m_ptr->fx;

            // reject things beyond range
            if ((range > 0)
                && (distance(p_ptr->py, p_ptr->px, p_ptr->target_row,
                        p_ptr->target_col)
                    > range))
                return (false);

            /* Good target */
            return (true);
        }
    }

    /* Assume no target */
    return (false);
}

/*
 * Update (if necessary) and verify (if possible) the target.
 *
 * Very similar to target_okay, but does not require projectibility, just line
 * of sight
 *
 * We return true if the target is "okay" and false otherwise.
 */
bool target_sighted(void)
{
    /* No target */
    if (!p_ptr->target_set)
        return (false);

    /* Accept "location" targets */
    if (p_ptr->target_who == 0)
        return (true);

    /* Check "monster" targets */
    if (p_ptr->target_who > 0)
    {
        int m_idx = p_ptr->target_who;
        monster_type* m_ptr = &mon_list[m_idx];

        /* Accept reasonable targets */
        if (player_can_see_bold(m_ptr->fy, m_ptr->fx) && m_ptr->ml)
        {
            /* Get the monster location */
            p_ptr->target_row = m_ptr->fy;
            p_ptr->target_col = m_ptr->fx;

            /* Good target */
            return (true);
        }
    }

    /* Assume no target */
    return (false);
}

/*
 * Set the target to a monster (or nobody)
 */
void target_set_monster(int m_idx)
{
    /* Acceptable target */
    if ((m_idx > 0) && target_able(m_idx))
    {
        monster_type* m_ptr = &mon_list[m_idx];

        /* Save target info */
        p_ptr->target_set = true;
        p_ptr->target_who = m_idx;
        p_ptr->target_row = m_ptr->fy;
        p_ptr->target_col = m_ptr->fx;
    }

    /* Clear target */
    else
    {
        /* Reset target info */
        p_ptr->target_set = false;
        p_ptr->target_who = 0;
        p_ptr->target_row = 0;
        p_ptr->target_col = 0;
    }

    app_session_mark_snapshot_dirty(app_session_current(),
        APP_SNAPSHOT_INVALIDATE_TARGET | APP_SNAPSHOT_INVALIDATE_CURSOR
            | APP_SNAPSHOT_INVALIDATE_MAP);
}

/*
 * Set the target to a location
 */
void target_set_location(int y, int x)
{
    /* Legal target */
    if (in_bounds_fully(y, x))
    {
        /* Save target info */
        p_ptr->target_set = true;
        p_ptr->target_who = 0;
        p_ptr->target_row = y;
        p_ptr->target_col = x;
    }

    /* Clear target */
    else
    {
        /* Reset target info */
        p_ptr->target_set = false;
        p_ptr->target_who = 0;
        p_ptr->target_row = 0;
        p_ptr->target_col = 0;
    }

    app_session_mark_snapshot_dirty(app_session_current(),
        APP_SNAPSHOT_INVALIDATE_TARGET | APP_SNAPSHOT_INVALIDATE_CURSOR
            | APP_SNAPSHOT_INVALIDATE_MAP);
}

/*
 * Sorting hook -- comp function -- by "monster priority"
 *
 * Sorts monsters by: 1) Uniques first, 2) Then by depth (higher depth first), 3) Then by distance
 * We use "u" and "v" to point to arrays of "x" and "y" positions.
 */
static bool ang_sort_comp_monster_priority(const void* u, const void* v, int a, int b)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    byte* x = (byte*)(u);
    byte* y = (byte*)(v);

    int m_idx_a, m_idx_b;
    monster_type* m_ptr_a;
    monster_type* m_ptr_b;
    monster_race* r_ptr_a;
    monster_race* r_ptr_b;
    
    /* Get monster indices */
    m_idx_a = cave_m_idx[y[a]][x[a]];
    m_idx_b = cave_m_idx[y[b]][x[b]];
    
    /* Safety check */
    if (!m_idx_a && !m_idx_b) return false;
    if (!m_idx_a) return false; /* b comes first */
    if (!m_idx_b) return true;  /* a comes first */
    
    /* Get monster pointers */
    m_ptr_a = &mon_list[m_idx_a];
    m_ptr_b = &mon_list[m_idx_b];
    r_ptr_a = &r_info[m_ptr_a->r_idx];
    r_ptr_b = &r_info[m_ptr_b->r_idx];
    
    /* Check if either is unique */
    bool unique_a = (r_ptr_a->flags1 & RF1_UNIQUE) != 0;
    bool unique_b = (r_ptr_b->flags1 & RF1_UNIQUE) != 0;
    
    /* Uniques always come first */
    if (unique_a && !unique_b) return true;  /* a comes first */
    if (!unique_a && unique_b) return false; /* b comes first */
    
    /* Both unique or both non-unique, sort by depth (higher depth first) */
    if (r_ptr_a->level != r_ptr_b->level)
    {
        return (r_ptr_a->level >= r_ptr_b->level);
    }
    
    /* Same depth, sort by distance (closer first) */
    int da, db, kx, ky;

    /* Absolute distance components for a */
    kx = x[a] - px;
    kx = ABS(kx);
    ky = y[a] - py;
    ky = ABS(ky);
    da = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Absolute distance components for b */
    kx = x[b] - px;
    kx = ABS(kx);
    ky = y[b] - py;
    ky = ABS(ky);
    db = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Compare the distances */
    return (da <= db);
}

/*
 * Sorting hook -- comp function -- by "distance to player"
 *
 * We use "u" and "v" to point to arrays of "x" and "y" positions,
 * and sort the arrays by double-distance to the player.
 */
static bool ang_sort_comp_distance(const void* u, const void* v, int a, int b)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    byte* x = (byte*)(u);
    byte* y = (byte*)(v);

    int da, db, kx, ky;

    /* Absolute distance components */
    kx = x[a];
    kx -= px;
    kx = ABS(kx);
    ky = y[a];
    ky -= py;
    ky = ABS(ky);

    /* Approximate Double Distance to the first point */
    da = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Absolute distance components */
    kx = x[b];
    kx -= px;
    kx = ABS(kx);
    ky = y[b];
    ky -= py;
    ky = ABS(ky);

    /* Approximate Double Distance to the first point */
    db = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Compare the distances */
    return (da <= db);
}

/*
 * Sorting hook -- swap function -- by "distance to player"
 *
 * We use "u" and "v" to point to arrays of "x" and "y" positions,
 * and sort the arrays by distance to the player.
 */
static void ang_sort_swap_distance(void* u, void* v, int a, int b)
{
    byte* x = (byte*)(u);
    byte* y = (byte*)(v);

    byte temp;

    /* Swap "x" */
    temp = x[a];
    x[a] = x[b];
    x[b] = temp;

    /* Swap "y" */
    temp = y[a];
    y[a] = y[b];
    y[b] = temp;
}

/*
 * Hack -- determine if a given location is "interesting"
 */
static bool determine_location_is_interesting(int y, int x)
{
    object_type* o_ptr;

    /* Player grids are always interesting */
    if (cave_m_idx[y][x] < 0)
        return (true);

    /* Handle hallucination */
    if (p_ptr->image)
        return (false);

    /* Rage and labyrinth partitions both suppress remembered-grid look data. */
    if (!grid_info_is_available(y, x))
        return (false);

    /* Check for objects first (only shown when on floors, not when in rubble) */
    /* This is checked BEFORE monsters to prevent showing unmarked objects under detected monsters */
    if (cave_floorlike_bold(y, x) || (cave_feat[y][x] == FEAT_SUNLIGHT))
    {
        /* Scan all objects in the grid */
        for (o_ptr = get_first_object(y, x); o_ptr;
             o_ptr = get_next_object(o_ptr))
        {
            /* Memorized object - this makes the location interesting */
            if (o_ptr->marked && !object_is_searched_skeleton(o_ptr))
                return (true);
        }
    }

    /* Visible monsters (checked AFTER objects) */
    /* This ensures that a location with a monster but no marked objects */
    /* is interesting for monster targeting but NOT for object listing */
    if (cave_m_idx[y][x] > 0)
    {
        monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];

        /* Visible monsters */
        if (m_ptr->ml)
            return (true);
    }

    /* Interesting memorized features */
    if (cave_info[y][x] & (CAVE_MARK))
    {
        /* Notice chasms */
        if (cave_feat[y][x] == FEAT_CHASM)
            return (true);

        /* Notice glyphs */
        if (cave_glyph(y, x))
            return (true);

        /* Notice forges */
        if (cave_forge_bold(y, x))
            return (true);

        /* Notice doors */
        if (cave_feat[y][x] == FEAT_OPEN)
            return (true);
        if (cave_feat[y][x] == FEAT_BROKEN)
            return (true);

        /* Notice stairs */
        if (cave_stair_bold(y, x))
            return (true);

        /* Notice traps */
        if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
            return (true);

        /* Notice doors */
        if (cave_known_closed_door_bold(y, x))
            return (true);

        /* Notice rubble */
        if (cave_feat[y][x] == FEAT_RUBBLE)
            return (true);
    }

    /* Nope */
    return (false);
}

/*
 * Prepare the "temp" array for "target_interactive_set"
 *
 * Return the number of target_able monsters in the set.
 */
void get_sorted_target_list(int mode, int range)
{
    int y, x;

    /* Reset "temp" array */
    temp_n = 0;

    /* Scan the current panel */
    for (y = p_ptr->wy; y < p_ptr->wy + SCREEN_HGT; y++)
    {
        for (x = p_ptr->wx; x < p_ptr->wx + SCREEN_WID; x++)
        {
            /* Check bounds */
            if (!in_bounds_fully(y, x))
                continue;

            // Previously required LOS, but this is now ignored...

            /* Require "interesting" contents */
            if (!determine_location_is_interesting(y, x))
                continue;

            /* Special mode */
            if (mode & (TARGET_KILL))
            {
                /* Must contain a monster */
                if (!(cave_m_idx[y][x] > 0))
                    continue;

                /* Must be a targetable monster */
                if (!target_able(cave_m_idx[y][x]))
                    continue;

                // possibly restrict the distance from the player
                if ((range > 0)
                    && (distance(p_ptr->py, p_ptr->px, y, x) > range))
                    continue;
            }
            else if (mode & (TARGET_LIST_MONSTER))
            {
                /* Must contain a monster */
                if (!(cave_m_idx[y][x] > 0))
                    continue;
            }
            else if (mode & (TARGET_LIST_OBJECT))
            {
                if (!(cave_o_idx[y][x] > 0))
                    continue;
            }

            /* Save the location */
            temp_x[temp_n] = x;
            temp_y[temp_n] = y;
            temp_n++;
        }
    }

    /* Set the sort hooks */
    if (mode & (TARGET_LIST_MONSTER))
    {
        /* Use monster priority sorting (uniques first, then by depth, then by distance) */
        ang_sort_comp = ang_sort_comp_monster_priority;
    }
    else
    {
        /* Use distance sorting for objects and other targets */
        ang_sort_comp = ang_sort_comp_distance;
    }
    ang_sort_swap = ang_sort_swap_distance;

    /* Sort the positions */
    ang_sort(temp_x, temp_y, temp_n);
}

/*
 * Examine a grid, return a keypress.
 *
 * The "mode" argument contains the "TARGET_LOOK" bit flag, which
 * indicates that the "space" key should scan through the contents
 * of the grid, instead of simply returning immediately.  This lets
 * the "look" command get complete information, without making the
 * "target" command annoying.
 *
 * The "info" argument contains the "commands" which should be shown
 * inside the "[xxx]" text.  This string must never be empty, or grids
 * containing monsters will be displayed with an extra comma.
 *
 * Note that if a monster is in the grid, we update both the monster
 * recall info and the health bar info to track that monster.
 *
 * This function correctly handles multiple objects per grid, and objects
 * and terrain features in the same grid, though the latter never happens.
 *
 * This function must handle blindness/hallucination.
 */
/*
 * Takes a delta coordinates and returns a direction.
 * e.g. (1,0) is south, which is direction 2.
 */
int dir_from_delta(int deltay, int deltax)
{
    s16b dird[3][3] = { { 7, 8, 9 }, { 4, 5, 6 }, { 1, 2, 3 } };

    return (dird[deltay + 1][deltax + 1]);
}

/*
 * Gives the overall direction from point 1 to point 2.
 * Uses orthogonals when breaking ties.
 */
int rough_direction(int y1, int x1, int y2, int x2)
{
    int deltay = y2 - y1; // these represent the displacement
    int deltax = x2 - x1;

    int dy, dx; // these represent the direction

    // determine the main direction from the source to the target
    if (deltay == 0)
        dy = 0;
    else
        dy = (deltay > 0) ? 1 : -1;

    if (deltax == 0)
        dx = 0;
    else
        dx = (deltax > 0) ? 1 : -1;

    if ((deltax != 0) && (ABS(deltay) / ABS(deltax) >= 2))
        dx = 0;
    if ((deltay != 0) && (ABS(deltax) / ABS(deltay) >= 2))
        dy = 0;

    return (dir_from_delta(dy, dx));
}

/*
 * Get an "aiming direction" (1,2,3,4,6,7,8,9 or 5) from the user.
 *
 * Return true if a direction was chosen, otherwise return false.
 *
 * The direction "5" is special, and means "use current target".
 *
 * This function tracks and uses the "global direction", and uses
 * that as the "desired direction", if it is set.
 *
 * Note that "Force Target", if set, will pre-empt user interaction,
 * if there is a usable target already set.
 *
 * If the range variable is 0, there is no range limit.
 *
 * Currently this function applies confusion directly.
 */
bool get_aim_dir(int* dp, int range)
{
    app_wait_scope wait_scope;
    app_movement_command movement_command;
    int dir;

    char ch;

    cptr p;

    app_session_push_wait_scope(app_session_current(), &wait_scope,
        APP_WAIT_REASON_TARGETING, 0, range);

#ifdef ALLOW_REPEAT

    if (repeat_pull(dp))
    {
        /* Verify */
        if (!(*dp == 5 && !target_okay(range)))
        {
            app_session_pop_wait_scope(app_session_current(), &wait_scope);
            return (true);
        }
        else
        {
            /* Invalid repeat - reset it */
            repeat_clear();
        }
    }

#endif /* ALLOW_REPEAT */

    /* Initialize */
    (*dp) = 0;

    /* Global direction */
    dir = p_ptr->command_dir;
    if ((dir == 5) && !target_okay(range))
        dir = 0;
    if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
        dir = 0;

    /* Hack -- auto-target if requested */
    //	if (use_old_target && target_okay(range)) dir = 5;

    /* Ask until satisfied */
    while (!dir)
    {
        /* Choose a prompt */
        if (!target_okay(range))
        {
            p = "Direction ('f' for closest, '*' to choose a target, ESC to "
                "cancel)? ";
        }
        else
        {
            p = "Direction ('f' for target, '*' to re-target, ESC to cancel)? ";
        }

        message_flush();
        targeting_direction_prompt(p,
            "Use movement keys or digits. '*' chooses a target, Esc cancels.");
        app_movement_command_clear(&movement_command);
        ch = '\0';
        (void)app_command_wait_input(APP_MOVEMENT_CONTEXT_TARGETING,
            APP_WAIT_REASON_NONE, &movement_command, &ch);

        if (app_movement_command_is_valid(&movement_command))
        {
            dir = app_movement_direction_to_legacy_keypad(
                movement_command.direction.direction);

            if (dir == 5)
            {
                if (target_okay(range))
                {
                    dir = 5;
                }
                else
                {
                    /* Prepare the "temp" array */
                    get_sorted_target_list(TARGET_KILL, range);

                    /* Monster */
                    if (temp_n)
                    {
                        target_set_monster(cave_m_idx[temp_y[0]][temp_x[0]]);
                        health_track(cave_m_idx[temp_y[0]][temp_x[0]]);
                        dir = 5;
                    }
                    else
                    {
                        dir = 0;
                    }
                }
            }
            else if ((dir == 5) && !target_okay(range))
            {
                dir = 0;
            }

            if (!dir)
                bell("Illegal aim direction!");
            continue;
        }

        if (ch == ESCAPE)
            break;

        /* Analyze */
        switch (ch)
        {
        /* Set new target, use target if legal */
        case '*':
        {
            if (target_set_interactive(TARGET_KILL, range))
                dir = 5;
            break;
        }

        /* Use current target, if set and legal, otherwise pick next target */
        case 'f':
        case 'F':
        case 't':
        case '5':
        case 'z':
        {
            if (target_okay(range))
                dir = 5;
            else
            {
                /* Prepare the "temp" array */
                get_sorted_target_list(TARGET_KILL, range);

                /* Monster */
                if (temp_n)
                {
                    target_set_monster(cave_m_idx[temp_y[0]][temp_x[0]]);
                    health_track(cave_m_idx[temp_y[0]][temp_x[0]]);
                    dir = 5;
                }
            }
            break;
        }

            // Sil-y: there is some chance that these UP and DOWN things
            //        will cause trouble elsewhere

        case '>':
        {
            dir = DIRECTION_DOWN;
            break;
        }
        case '<':
        {
            dir = DIRECTION_UP;
            break;
        }

        /* Possible direction */
        default:
        {
            dir = 0;
            break;
        }
        }

        /* Error */
        if (!dir)
            bell("Illegal aim direction!");
    }

    /* No direction */
    if (!dir)
    {
        app_session_clear_interaction(app_session_current());
        app_session_pop_wait_scope(app_session_current(), &wait_scope);
        return (false);
    }

    /* Save the direction */
    p_ptr->command_dir = dir;

    /* Check for confusion */
    // Sil-y: Doesn't use the new confusion method, but might be difficult to
    // use it
    if ((dir != DIRECTION_UP) && (dir != DIRECTION_DOWN) && p_ptr->confused)
    {
        /* Randomish direction */
        dir = ddd[rand_int(8)];
    }

    /* Notice confusion */
    if (p_ptr->command_dir != dir)
    {
        /* Warn the user */
        msg_print("You are confused.");
    }

    /* Save direction */
    (*dp) = dir;

#ifdef ALLOW_REPEAT

    repeat_push(dir);

#endif /* ALLOW_REPEAT */

    /* A "valid" direction was entered */
    app_session_clear_interaction(app_session_current());
    app_session_pop_wait_scope(app_session_current(), &wait_scope);
    return (true);
}

/*
 * Request a "movement" direction (1,2,3,4,5,6,7,8,9) from the user.
 *
 * Return true if a direction was chosen, otherwise return false.
 *
 * Direction "0" is illegal and will not be accepted.
 *
 * This function tracks and uses the "global direction", and uses
 * that as the "desired direction", if it is set.
 */
bool get_rep_dir(int* dp)
{
    app_wait_scope wait_scope;
    app_movement_command movement_command;
    int dir;

    char ch;

    app_session_push_wait_scope(app_session_current(), &wait_scope,
        APP_WAIT_REASON_TARGETING, 0, 0);

#ifdef ALLOW_REPEAT

    if (repeat_pull(dp))
    {
        app_session_pop_wait_scope(app_session_current(), &wait_scope);
        return (true);
    }

#endif /* ALLOW_REPEAT */

    /* Initialize */
    (*dp) = 0;

    /* Global direction */
    dir = p_ptr->command_dir;

    message_flush();

    /* Get a direction */
    while (!dir)
    {
        targeting_direction_prompt("Direction (ESC to cancel)?",
            "Use movement keys or digits. Esc cancels.");
        app_movement_command_clear(&movement_command);
        ch = '\0';
        (void)app_command_wait_input(
            APP_MOVEMENT_CONTEXT_DIRECTION_PROMPT, APP_WAIT_REASON_NONE,
            &movement_command, &ch);

        if (app_movement_command_is_valid(&movement_command))
        {
            dir = app_movement_direction_to_legacy_keypad(
                movement_command.direction.direction);
            if (!dir)
                bell("Illegal repeatable direction!");
            continue;
        }

        if (ch == ESCAPE)
            break;
        if (ch)
            bell("Illegal repeatable direction!");
    }

    /* Aborted */
    if (!dir)
    {
        app_session_clear_interaction(app_session_current());
        app_session_pop_wait_scope(app_session_current(), &wait_scope);
        return (false);
    }

    /* Save desired direction */
    p_ptr->command_dir = dir;

    /* Save direction */
    (*dp) = dir;

#ifdef ALLOW_REPEAT

    repeat_push(dir);

#endif /* ALLOW_REPEAT */

    /* Success */
    app_session_clear_interaction(app_session_current());
    app_session_pop_wait_scope(app_session_current(), &wait_scope);
    return (true);
}

/*
 * Apply confusion, if needed, to a direction
 *
 * Display a message and return true if direction changes.
 */
bool confuse_dir(int* dp)
{
    int dir;
    int i;

    /* Default */
    dir = (*dp);

    /* Apply "confusion" */
    if (p_ptr->confused)
    {
        /* If no direction given, then completely randomise it */
        if (dir == 5)
        {
            /* Random direction */
            dir = ddd[rand_int(8)];
        }
        else
        {
            // gives 3 chances to be turned left and 3 chances to be turned
            // right leads to a binomial distribution of direction around the
            // intended one:
            //
            // 15 20 15
            //  6     6   (chances are all out of 64)
            //  1  0  1

            i = damroll(3, 2) - damroll(3, 2);

            dir = cycle[chome[*dp] + i];
        }
    }

    /* Notice confusion */
    if ((*dp) != dir)
    {
        /* Warn the user */
        msg_print("You are confused.");

        /* Save direction */
        (*dp) = dir;

        /* Confused */
        return (true);
    }

    /* Not confused */
    return (false);
}


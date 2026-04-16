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
#include "log/log.h"
#include "platform-story-font.h"
#include "ui/ui-information-scene.h"

static void targeting_snapshot_prompt(cptr text)
{
    app_session* session = app_session_current();

    if (!app_session_interactions_enabled(session))
        return;

    app_session_begin_interaction(session, APP_INTERACTION_KIND_TARGETING,
        APP_WAIT_REASON_TARGETING,
        APP_INTERACTION_FLAG_CAN_CONFIRM
            | APP_INTERACTION_FLAG_CAN_CANCEL);
    app_session_set_interaction_prompt(session, TERM_WHITE, text ? text : "");
    app_session_set_interaction_detail(session, TERM_SLATE,
        "Use direction keys to move, Enter targets, Esc cancels.");
}

static void targeting_direction_prompt(cptr prompt, cptr detail)
{
    app_session* session = app_session_current();

    if (!app_session_interactions_enabled(session))
        return;

    app_session_begin_interaction(session, APP_INTERACTION_KIND_PROMPT,
        APP_WAIT_REASON_TARGETING,
        APP_INTERACTION_FLAG_CAN_CONFIRM
            | APP_INTERACTION_FLAG_CAN_CANCEL);
    app_session_set_interaction_prompt(session, TERM_WHITE, prompt ? prompt : "");
    app_session_set_interaction_detail(session, TERM_SLATE,
        detail ? detail : "");
}

static char targeting_inkey_with_wait_reason(void)
{
    return (char)ui_information_scene_wait_key_with_wait_reason(
        APP_WAIT_REASON_TARGETING);
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
 * Monster health description.
 */
static void look_mon_desc(char* buf, size_t max, int m_idx)
{
    monster_type* m_ptr = &mon_list[m_idx];

    // start the string empty
    SDL_strlcpy(buf, "(", max);

    if (p_ptr->wizard)
    {
        if (m_ptr->alertness < ALERTNESS_UNWARY)
            SDL_strlcat(buf, format("asleep (%d), ", m_ptr->alertness), max);
        else if (m_ptr->alertness < ALERTNESS_ALERT)
            SDL_strlcat(buf, format("unwary (%d), ", m_ptr->alertness), max);
        else
            SDL_strlcat(buf, format("alert (%d), ", m_ptr->alertness), max);
    }

    if (m_ptr->confused)
        SDL_strlcat(buf, "confused, ", max);
    if (m_ptr->stunned)
        SDL_strlcat(buf, "stunned, ", max);
    if ((m_ptr->slowed) && (!m_ptr->hasted))
        SDL_strlcat(buf, "slowed, ", max);
    if ((!m_ptr->slowed) && (m_ptr->hasted))
        SDL_strlcat(buf, "hasted, ", max);

    // If nothing is going to be written, wipe the string
    if (strlen(buf) == 1)
    {
        buf[0] = '\0';
    }
    // Otherwise finish it
    else
    {
        // trim the final ", " first
        buf[strlen(buf) - 2] = '\0';
        SDL_strlcat(buf, ") ", max);
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
 * Hack -- help "select" a location (see below)
 */
static s16b target_pick(int y1, int x1, int dy, int dx)
{
    int i, v;

    int x2, y2, x3, y3, x4, y4;

    int b_i = -1, b_v = 9999;

    /* Scan the locations */
    for (i = 0; i < temp_n; i++)
    {
        /* Point 2 */
        x2 = temp_x[i];
        y2 = temp_y[i];

        /* Directed distance */
        x3 = (x2 - x1);
        y3 = (y2 - y1);

        /* Verify quadrant */
        if (dx && (x3 * dx <= 0))
            continue;
        if (dy && (y3 * dy <= 0))
            continue;

        /* Absolute distance */
        x4 = ABS(x3);
        y4 = ABS(y3);

        /* Verify quadrant */
        if (dy && !dx && (x4 > y4))
            continue;
        if (dx && !dy && (y4 > x4))
            continue;

        /* Approximate Double Distance */
        v = ((x4 > y4) ? (x4 + x4 + y4) : (y4 + y4 + x4));

        /* Penalize location XXX XXX XXX */

        /* Track best */
        if ((b_i >= 0) && (v >= b_v))
            continue;

        /* Track best */
        b_i = i;
        b_v = v;
    }

    /* Result */
    return (b_i);
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
static int target_set_interactive_aux(int y, int x, int mode, cptr info, bool use_story_font)
{
    s16b this_o_idx, next_o_idx = 0;

    cptr s1, s2, s3;

    bool boring;

    bool floored;

    int feat;

    int query;

    char out_val[256];

    (void)use_story_font;

    /* Repeat forever */
    while (1)
    {
        char more[8];
        // reset the 'more' buffer
        strnfmt(more, 1, "");

        /* Paranoia */
        query = ' ';

        /* Assume boring */
        boring = true;

        /* Default */
        s1 = "You see ";
        s2 = "";
        s3 = "";

        /* The player */
        if (cave_m_idx[y][x] < 0)
        {
            /* Description */
            s1 = "You are ";

            /* Preposition */
            s2 = "on ";
        }

        /* Hack -- hallucination */
        if (p_ptr->image)
        {
            /* Display a message */
            strnfmt(out_val, sizeof(out_val),
                "What you see is not to be believed.  [%s]", info);

            targeting_snapshot_prompt(out_val);
            dungeon_note_cursor_relative(y, x);
            query = targeting_inkey_with_wait_reason();

            /* Stop on everything but "return" */
            if ((query != '\n') && (query != '\r'))
                break;

            /* Repeat forever */
            continue;
        }

        /* Actual monsters */
        if ((cave_m_idx[y][x] > 0) && grid_info_is_available(y, x))
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];

            /* Visible */
            if (m_ptr->ml)
            {
                bool recall = false;

                char m_name[80];

                bool show_more = false;

                /* Not boring */
                boring = false;

                if (p_ptr->rage)
                {
                    SDL_strlcpy(m_name, "an enemy", sizeof(m_name));
                }
                else
                {
                    /* Get the monster name ("a kobold") */
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0x08);
                }

                /* Hack -- track this monster race */
                monster_race_track(m_ptr->r_idx);

                /* Hack -- health bar for this monster */
                health_track(cave_m_idx[y][x]);

                /* Hack -- handle stuff */
                handle_stuff();

                /* Interact */
                while (1)
                {
                    /* Recall, but not when raging */
                    if ((recall) && !p_ptr->rage)
                    {
                        int recall_key = ESCAPE;
                        char recall_prompt[160];

                        app_session_clear_interaction(
                            app_session_current());
                        strnfmt(recall_prompt, sizeof(recall_prompt),
                            "  [(r)ecall, %s]", info);
                        if (!ui_information_scene_show_monster_recall(
                                m_ptr->r_idx, m_ptr, recall_prompt, true,
                                &recall_key))
                        {
                            log_error("targeting: semantic recall "
                                "scene unavailable");
                            bell("Monster recall screen unavailable.");
                            query = '\r';
                        }
                        else
                        {
                            query = (char)recall_key;
                        }
                    }

                    /* Normal */
                    else
                    {
                        /* Describe the monster, unless a mimic */
                        char buf[80];

                        look_mon_desc(buf, sizeof(buf), cave_m_idx[y][x]);

                        // determine if there is more info to display...

                        // visible squares with monsters holding things
                        if ((cave_info[y][x] & (CAVE_SEEN))
                            && m_ptr->hold_o_idx)
                        {
                            show_more = true;
                        }

                        // known objects on the floor
                        else if (grid_info_is_available(y, x)
                            && (cave_floorlike_bold(y, x)
                                || (cave_feat[y][x] == FEAT_SUNLIGHT))
                            && cave_o_idx[y][x]
                            && (&o_list[cave_o_idx[y][x]])->marked)
                        {
                            show_more = true;
                        }

                        // standing in a known unusual terrain such as wall or
                        // door
                        else if (!cave_floorlike_bold(y, x)
                            && (cave_info[y][x] & (CAVE_MARK)))
                        {
                            show_more = true;
                        }

                        if (show_more)
                        {
                            strnfmt(more, 8, "-more- ");
                        }

                        /* Describe, and prompt for recall */
                        if (p_ptr->wizard)
                        {
                            strnfmt(out_val, sizeof(out_val),
                                "%s%s%s%s %s%s [(r)ecall, %s] (%d:%d)", s1, s2,
                                s3, m_name, buf, more, info, y, x);
                        }

                        else
                        {
                            strnfmt(out_val, sizeof(out_val),
                                "%s%s%s%s %s%s [(r)ecall, %s]", s1, s2, s3,
                                m_name, buf, more, info);
                        }

                        targeting_snapshot_prompt(out_val);

                        /* Place cursor */
                        dungeon_note_cursor_relative(y, x);

                        /* Command */
                        query = targeting_inkey_with_wait_reason();
                    }

                    /* Normal commands */
                    if (query != 'r')
                        break;

                    /* Toggle recall */
                    recall = !recall;
                }

                /* Stop on everything but "return"/"space" */
                if ((query != '\n') && (query != '\r') && (query != ' '))
                    break;

                /* Sometimes stop at "space" key */
                if ((query == ' ') && !(mode & (TARGET_LOOK)))
                    break;

                /* Stop if not asked to continue */
                if (!show_more)
                    break;

                /* Change the intro */
                s1 = "It is ";

                /* Hack -- take account of gender */
                if (r_ptr->flags1 & (RF1_FEMALE))
                    s1 = "She is ";
                else if (r_ptr->flags1 & (RF1_MALE))
                    s1 = "He is ";

                /* Use a preposition */
                s2 = "carrying ";

                /* Scan all objects being carried */
                for (this_o_idx = m_ptr->hold_o_idx; this_o_idx;
                     this_o_idx = next_o_idx)
                {
                    char o_name[80];

                    object_type* o_ptr;

                    /* Get the object */
                    o_ptr = &o_list[this_o_idx];

                    /* Get the next object */
                    next_o_idx = o_ptr->next_o_idx;

                    /*Don't let the player see certain objects (used for vault
                     * treasure)*/
                    if ((o_ptr->ident & (IDENT_HIDE_CARRY)) && (!p_ptr->wizard)
                        && (!cheat_peek))
                        continue;

                    /* Obtain an object description */
                    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                    /* Describe the object */
                    if (p_ptr->wizard)
                    {
                        strnfmt(out_val, sizeof(out_val),
                            "%s%s%s%s %s [%s] (%d:%d)", s1, s2, s3, o_name,
                            more, info, y, x);
                    }
                    else
                    {
                        strnfmt(out_val, sizeof(out_val), "%s%s%s%s %s [%s]",
                            s1, s2, s3, o_name, more, info);
                    }

                    targeting_snapshot_prompt(out_val);
                    dungeon_note_cursor_relative(y, x);
                    query = targeting_inkey_with_wait_reason();

                    /* Stop on everything but "return"/"space" */
                    if ((query != '\n') && (query != '\r') && (query != ' '))
                        break;

                    /* Sometimes stop at "space" key */
                    if ((query == ' ') && !(mode & (TARGET_LOOK)))
                        break;

                    /* Change the intro */
                    s2 = "also carrying ";
                }

                /* Double break */
                if (this_o_idx)
                    break;

                /* Use a preposition */
                s2 = "on ";
            }
        }
        // if the square doesn't include a monster...
        else
        {
            // cancel health tracking
            health_track(0);

            /* Hack -- handle stuff */
            handle_stuff();
        }

        /* Assume not floored */
        floored = false;

        /* Scan all objects in the grid */
        for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
        {
            object_type* o_ptr;

            /* Get the object */
            o_ptr = &o_list[this_o_idx];

            /* Get the next object */
            next_o_idx = o_ptr->next_o_idx;

            /* Skip objects if floored */
            if (floored)
                continue;

            /* Objects (only shown when on floors, not when in rubble) */
            if (cave_floorlike_bold(y, x) || (cave_feat[y][x] == FEAT_SUNLIGHT))
            {
                /* Describe it */
                if (o_ptr->marked && grid_info_is_available(y, x))
                {
                    char o_name[80];

                    /* Not boring */
                    boring = false;

                    /* Obtain an object description */
                    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                    /* Describe the object */
                    if (p_ptr->wizard)
                    {
                        strnfmt(out_val, sizeof(out_val),
                            "%s%s%s%s %s [%s] (%d:%d)", s1, s2, s3, o_name,
                            more, info, y, x);
                    }
                    else
                    {
                        strnfmt(out_val, sizeof(out_val), "%s%s%s%s %s [%s]",
                            s1, s2, s3, o_name, more, info);
                    }

                    targeting_snapshot_prompt(out_val);
                    dungeon_note_cursor_relative(y, x);
                    query = targeting_inkey_with_wait_reason();

                    /* Stop on everything but "return"/"space" */
                    if ((query != '\n') && (query != '\r') && (query != ' '))
                        break;

                    /* Sometimes stop at "space" key */
                    if ((query == ' ') && !(mode & (TARGET_LOOK)))
                        break;

                    /* Change the intro */
                    s1 = "It is ";

                    /* Plurals */
                    if (o_ptr->number != 1)
                        s1 = "They are ";

                    /* Preposition */
                    s2 = "on ";
                }
            }
        }

        /* Double break */
        if (this_o_idx)
            break;

        /* Feature (apply "mimic") */
        feat = f_info[cave_feat[y][x]].mimic;

        /* Require knowledge about grid, or ability to see grid */
        if ((!grid_info_is_available(y, x)
                || (!(cave_info[y][x] & (CAVE_MARK))
                    && !player_can_see_bold(y, x)))
            && (distance(p_ptr->py, p_ptr->px, y, x) > 0))
        {
            /* Forget feature */
            feat = FEAT_NONE;
        }

        /* Terrain feature if needed */
        if (boring || !cave_floorlike_bold(y, x))
        {
            cptr name = f_name + f_info[feat].name;

            /* Hack -- handle unknown grids */
            if (feat == FEAT_NONE)
                name = "unknown square";

            /* Pick a prefix */
            if (*s2 && (feat >= FEAT_DOOR_HEAD))
                s2 = "in ";

            /* Use the definite article for the unique forge */
            if ((feat >= FEAT_FORGE_UNIQUE_HEAD)
                && (feat <= FEAT_FORGE_UNIQUE_TAIL))
            {
                s3 = "the ";
            }

            /* Pick proper indefinite article */
            else
            {
                s3 = (is_a_vowel(name[0])) ? "an " : "a ";
            }

            /* Display a message */
            if (p_ptr->wizard)
            {
                strnfmt(out_val, sizeof(out_val),
                    "%s%s%s%s (%d) %s [%s] (%d:%d)", s1, s2, s3, name,
                    cave_feat[y][x], more, info, y, x);
            }
            else
            {
                strnfmt(out_val, sizeof(out_val), "%s%s%s%s %s [%s]", s1, s2,
                    s3, name, more, info);
            }

            targeting_snapshot_prompt(out_val);
            dungeon_note_cursor_relative(y, x);
            query = targeting_inkey_with_wait_reason();

            /* Stop on everything but "return"/"space" */
            if ((query != '\n') && (query != '\r') && (query != ' '))
                break;
        }

        /* Stop on everything but "return" */
        if ((query != '\n') && (query != '\r'))
            break;
    }

    // make sure the health tracking is sorted out
    if (p_ptr->target_who)
    {
        health_track(p_ptr->target_who);
    }
    else
    {
        health_track(0);
    }

    /* Keep going */
    return (query);
}

/*
 * Handle "target" and "look".
 *
 * Note that this code can be called from "get_aim_dir()".
 *
 * Currently, when "flag" is true, that is, when
 * "interesting" grids are being used, and a directional key is used, we
 * only scroll by a single panel, in the direction requested, and check
 * for any interesting grids on that panel.  The "correct" solution would
 * actually involve scanning a larger set of grids, including ones in
 * panels which are adjacent to the one currently scanned, but this is
 * overkill for this function.  XXX XXX
 *
 * Hack -- targetting/observing an "outer border grid" may induce
 * problems, so this is not currently allowed.
 *
 * The player can use the direction keys to move among "interesting"
 * grids in a heuristic manner, or the "space", "+", and "-" keys to
 * move through the "interesting" grids in a sequential manner, or
 * can enter "location" mode, and use the direction keys to move one
 * grid at a time in any direction.  The "t" (set target) command will
 * only target a monster (as opposed to a location) if the monster is
 * target_able and the "interesting" mode is being used.
 *
 * The current grid is described using the "look" method above, and
 * a new command may be entered at any time, but note that if the
 * "TARGET_LOOK" bit flag is set (or if we are in "location" mode,
 * where "space" has no obvious meaning) then "space" will scan
 * through the description of the current grid until done, instead
 * of immediately jumping to the next "interesting" grid.  This
 * allows the "target" command to retain its old semantics.
 *
 * The "*", "+", and "-" keys may always be used to jump immediately
 * to the next (or previous) interesting grid, in the proper mode.
 *
 * The "return" key may always be used to scan through a complete
 * grid description (forever).
 *
 * if the range variable is 0, there is no range limit
 *
 * This command will cancel any old target, even if used from
 * inside the "look" command.
 */
bool target_set_interactive(int mode, int range)
{
    app_wait_scope wait_scope;
    int py = p_ptr->py;
    int px = p_ptr->px;

    int i, d, m, t, bd;

    int y = py;
    int x = px;

    int y2; // these dummy variables are needed in path determination stuff
    int x2;

    int adjusted_range;

    bool done = false;

    bool flag = true;

    bool valid_target;

    bool new_target = false;

    char query;

    char info[80];

    bool use_story_look = story_look_enabled() && (mode & TARGET_LOOK);

    u16b path[MAX_RANGE];
    int max;

    bool wiz = mode & (TARGET_WIZ);

    app_session_push_wait_scope(app_session_current(), &wait_scope,
        APP_WAIT_REASON_TARGETING, mode, range);

    // turn off auto if doing wizard mode dungeon modification
    if (wiz)
        flag = false;

    if (range == 0)
        adjusted_range = MAX_RANGE;
    else
        adjusted_range = range;

    /* Prepare the "temp" array */
    get_sorted_target_list(mode, range);

    /* Start near the player */
    m = 0;

    /* Interact */
    while (!done)
    {
        max = 0;

        /* Interesting grids */
        if (flag && temp_n)
        {
            y = temp_y[m];
            x = temp_x[m];

            y2 = y;
            x2 = x;

            // need to compute 'max' whether or not we are in 'target mode'
            // in order to correctly determine if a square is targetable
            // taking player's limited knowledge into account

            max = project_path(path, adjusted_range, py, px, &y2, &x2,
                (PROJECT_THRU | PROJECT_INVISIPASS));

            // Check whether the target location is valid (ie within the path)
            if ((max == 0)
                || ((((GRID_Y(path[max - 1]) <= y) && (y <= py))
                        || ((GRID_Y(path[max - 1]) >= y) && (y >= py)))
                    && (((GRID_X(path[max - 1]) <= x) && (x <= px))
                        || ((GRID_X(path[max - 1]) >= x) && (x >= px)))))
            {
                valid_target = true;
            }
            else
            {
                valid_target = false;
            }

            // prepare the relevant prompt
            if (valid_target)
            {
                SDL_strlcpy(info, "(t)arget, (m)anual, <dir>", sizeof(info));
            }
            else
            {
                SDL_strlcpy(info, "(m)anual, <dir>", sizeof(info));
            }

            /* Describe and Prompt */
            if (use_story_look)
                sdl_story_font_enable();
            query = target_set_interactive_aux(y, x, mode, info, use_story_look);
            if (use_story_look)
                sdl_story_font_disable();

            /* Assume no "direction" */
            d = 0;

            /* Analyze */
            switch (query)
            {
            case ESCAPE:
            case 'q':
            {
                done = true;
                break;
            }

            case ' ':
            case '*':
            case '+':
            {
                if (++m == temp_n)
                {
                    m = 0;
                }
                break;
            }

            case '-':
            {
                if (m-- == 0)
                {
                    m = temp_n - 1;
                }
                break;
            }

            case 'p':
            {
                /* Recenter around player */
                verify_panel();

                /* Handle stuff */
                handle_stuff();

                y = py;
                x = px;
                __attribute__((fallthrough));
            }

            case 'm':
            {
                flag = false;
                break;
            }

            case 't':
            case '5':
            case 'z':
            case '\n':
            case '\r':
            {
                int m_idx = cave_m_idx[y][x];

                if ((p_ptr->py == y) && (p_ptr->px == x))
                {
                    done = true;
                }
                else if ((m_idx > 0) && target_able(m_idx))
                {
                    health_track(m_idx);
                    target_set_monster(m_idx);
                    new_target = true;
                    done = true;
                }
                else if (valid_target)
                {
                    target_set_location(y, x);
                    health_track(0);
                    new_target = true;
                    done = true;
                }
                else
                {
                    bell("Illegal target.");
                }
                break;
            }

            default:
            {
                /* Extract direction */
                d = target_dir(query);

                /* Oops */
                if (!d)
                    bell("Illegal command for target mode!");

                break;
            }
            }

            /* Hack -- move around */
            if (d)
            {
                int old_y = temp_y[m];
                int old_x = temp_x[m];

                /* Find a new monster */
                i = target_pick(old_y, old_x, ddy[d], ddx[d]);

                /* Scroll to find interesting grid */
                if (i < 0)
                {
                    int old_wy = p_ptr->wy;
                    int old_wx = p_ptr->wx;

                    /* Change if legal */
                    if (change_panel(d))
                    {
                        /* Recalculate interesting grids */
                        get_sorted_target_list(mode, range);

                        /* Find a new monster */
                        i = target_pick(old_y, old_x, ddy[d], ddx[d]);

                        /* Restore panel if needed */
                        if ((i < 0) && modify_panel(old_wy, old_wx))
                        {
                            /* Recalculate interesting grids */
                            get_sorted_target_list(mode, range);
                        }

                        /* Handle stuff */
                        handle_stuff();
                    }
                }

                /* Use interesting grid if found */
                if (i >= 0)
                    m = i;
            }
        }

        /* Arbitrary grids */
        else if (!wiz)
        {
            y2 = y;
            x2 = x;

            // need to compute 'max' whether or not we are in 'target mode'
            // in order to correctly determine if a square is targetable
            // taking player's limited knowledge into account
            max = project_path(path, adjusted_range, py, px, &y2, &x2,
                (PROJECT_THRU | PROJECT_INVISIPASS));

            // Check whether the target location is valid (ie within the path)
            if ((max == 0)
                || ((((GRID_Y(path[max - 1]) <= y) && (y <= py))
                        || ((GRID_Y(path[max - 1]) >= y) && (y >= py)))
                    && (((GRID_X(path[max - 1]) <= x) && (x <= px))
                        || ((GRID_X(path[max - 1]) >= x) && (x >= px)))))
            {
                valid_target = true;
            }
            else
            {
                valid_target = false;
            }

            // prepare the relevant prompt
            if (valid_target || p_ptr->wizard)
            {
                SDL_strlcpy(info, "(t)arget, (a)uto, <dir>", sizeof(info));
            }
            else
            {
                SDL_strlcpy(info, "(a)uto, <dir>", sizeof(info));
            }

            /* Describe and Prompt (enable "TARGET_LOOK") */
            if (use_story_look)
                sdl_story_font_enable();
            query = target_set_interactive_aux(y, x, mode | TARGET_LOOK, info, use_story_look);
            if (use_story_look)
                sdl_story_font_disable();

            /* Assume no direction */
            d = 0;

            /* Analyze the keypress */
            switch (query)
            {
            case ESCAPE:
            case 'q':
            {
                done = true;
                break;
            }

            case 'p':
            {
                /* Recenter around player */
                verify_panel();

                /* Handle stuff */
                handle_stuff();

                y = py;
                x = px;
                __attribute__((fallthrough));
            }

            case 'a':
            {
                flag = true;

                m = 0;
                bd = 999;

                /* Pick a nearby monster */
                for (i = 0; i < temp_n; i++)
                {
                    t = distance(y, x, temp_y[i], temp_x[i]);

                    /* Pick closest */
                    if (t < bd)
                    {
                        m = i;
                        bd = t;
                    }
                }

                /* Nothing interesting */
                if (bd == 999)
                    flag = false;

                break;
            }

            case 't':
            case '5':
            case 'z':
            case '\n':
            case '\r':
            {
                if ((p_ptr->py == y) && (p_ptr->px == x))
                {
                    done = true;
                }
                else if (valid_target || p_ptr->wizard)
                {
                    target_set_location(y, x);
                    health_track(0);
                    new_target = true;
                    done = true;
                }
                else
                {
                    bell("Illegal target.");
                }
                break;
            }

            default:
            {
                /* Extract a direction */
                d = target_dir(query);

                /* Oops */
                if (!d)
                    bell("Illegal command for target mode!");

                break;
            }
            }

            /* Handle "direction" */
            if (d)
            {
                /* Move */
                x += ddx[d];
                y += ddy[d];

                /* Slide into legality */
                if (x >= p_ptr->cur_map_wid - 1)
                    x--;
                else if (x <= 0)
                    x++;

                /* Slide into legality */
                if (y >= p_ptr->cur_map_hgt - 1)
                    y--;
                else if (y <= 0)
                    y++;

                /* Adjust panel if needed */
                if (adjust_panel(y, x))
                {
                    /* Handle stuff */
                    handle_stuff();

                    /* Recalculate interesting grids */
                    get_sorted_target_list(mode, range);
                }
            }
        }

        /* Wizard dungeon modification */
        else
        {
            bool inc_monster = false;
            bool inc_object = false;
            bool inc_terrain = false;
            bool reroll_monster = false;
            bool reroll_object = false;
            bool found = false;

            y2 = y;
            x2 = x;

            // prepare the relevant prompt
            SDL_strlcpy(info, "<space>, <tab>, <dir>", sizeof(info));

            /* Describe and Prompt (enable "TARGET_LOOK") */
            query = target_set_interactive_aux(y, x, mode | TARGET_LOOK, info, use_story_look);

            /* Assume no direction */
            d = 0;

            // space increments (and is handled specially)
            if (query == ' ')
            {
                // increment a monster race
                if (cave_m_idx[y][x])
                    inc_monster = true;
                // increment an object kind
                else if (cave_o_idx[y][x])
                    inc_object = true;
                // increment a terrain type
                else
                    inc_terrain = true;
            }

            // tab rerolls (and is handled specially)
            if (query == '\t')
            {
                // reroll a monster race
                if (cave_m_idx[y][x])
                    reroll_monster = true;
                // reroll an object kind
                else if (cave_o_idx[y][x])
                    reroll_object = true;
            }

            // escape exits
            if (query == ESCAPE)
            {
                done = true;
            }

            // backspace changes the light level (and is handled specially)
            else if (query == '\b')
            {
                // toggle the cave_glow value
                if (cave_info[y][x] & (CAVE_GLOW))
                {
                    cave_info[y][x] &= ~(CAVE_GLOW);
                    if (cave_floorlike_bold(y, x))
                    {
                        cave_info[y][x] &= ~(CAVE_MARK);
                    }
                }
                else
                {
                    cave_info[y][x] |= (CAVE_GLOW);
                }

                update_view();
            }

            // numbers move
            else if (strchr("12346789", query))
            {
                /* Extract a direction */
                d = target_dir(query);
            }

            // summon a creature
            else if (strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWX"
                            "YZ&@",
                         query)
                || inc_monster || reroll_monster)
            {
                monster_race* r_ptr;
                monster_race* old_r_ptr;
                monster_type* m_ptr;

                // recreate a monster of the same type.
                if (reroll_monster)
                {
                    m_ptr = &mon_list[cave_m_idx[y][x]];
                    i = m_ptr->r_idx;
                    found = true;
                }

                // go through monster race list and find next monster with that
                // symbol.
                else if (inc_monster)
                {
                    m_ptr = &mon_list[cave_m_idx[y][x]];
                    old_r_ptr = &r_info[m_ptr->r_idx];

                    for (i = 1; i < z_info->r_max; i++)
                    {
                        r_ptr = &r_info[(i + m_ptr->r_idx) % z_info->r_max];

                        // stop when you find one
                        if ((r_ptr->d_char == old_r_ptr->d_char)
                            && (r_ptr->cur_num < r_ptr->max_num)
                            && (r_ptr->level <= 25))
                        {
                            found = true;
                            i = (i + m_ptr->r_idx) % z_info->r_max;
                            break;
                        }
                    }
                }

                // go through monster race list and find first monster with that
                // symbol.
                else
                {
                    for (i = 1; i < z_info->r_max; i++)
                    {
                        r_ptr = &r_info[i];

                        // stop when you find one
                        if (r_ptr->d_char == query)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // if one was found, then place it
                if (found)
                {
                    // delete any existing monster
                    if (cave_m_idx[y][x])
                    {
                        delete_monster_idx(cave_m_idx[y][x]);
                    }
                    // place the new one
                    place_monster_one(y, x, i, true, true, NULL);
                }
            }

            // create an object
            else if (strchr("([)|/\\]}-~*\"=_?!~,", query) || inc_object
                || reroll_object)
            {
                object_kind* old_k_ptr;
                object_type* o_ptr;
                object_kind* k_ptr;
                object_type* i_ptr;
                object_type object_type_body;

                // recreate an object of the same type.
                if (reroll_object)
                {
                    o_ptr = &o_list[cave_o_idx[y][x]];
                    i = o_ptr->k_idx;
                    found = true;
                }

                // go through object kind list and find next object kind with
                // that symbol.
                else if (inc_object)
                {
                    o_ptr = &o_list[cave_o_idx[y][x]];
                    old_k_ptr = &k_info[o_ptr->k_idx];

                    for (i = 1; i < z_info->k_max; i++)
                    {
                        k_ptr = &k_info[(i + o_ptr->k_idx) % z_info->k_max];

                        // stop when you find one
                        if (k_ptr->d_char == old_k_ptr->d_char)
                        {
                            found = true;
                            i = (i + o_ptr->k_idx) % z_info->k_max;
                            break;
                        }
                    }
                }

                // go through object kind list and find first object kind with
                // that symbol.
                else
                {
                    for (i = 1; i < z_info->k_max; i++)
                    {
                        k_ptr = &k_info[i];

                        // stop when you find one
                        if (k_ptr->d_char == query)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // if one was found, then place it
                if (found)
                {
                    // delete any existing item
                    if (cave_o_idx[y][x])
                    {
                        delete_object_idx(cave_o_idx[y][x]);
                    }

                    /* Get local object */
                    i_ptr = &object_type_body;

                    /* Create the item */
                    object_prep(i_ptr, i);

                    /* Apply magic (no messages, no artefacts) */
                    apply_magic(
                        i_ptr, p_ptr->depth, false, false, false, false);

                    if (i_ptr->tval == TV_ARROW)
                        i_ptr->number = 24;

                    /* Drop the object from heaven */
                    drop_near(i_ptr, -1, y, x);
                }
            }

            // change the terrain
            else if (strchr(".;'^+#:%0<>", query) || inc_terrain)
            {
                feature_type* f_ptr;
                feature_type* old_f_ptr;

                // go through terrain list and find next terrain type with that
                // symbol.
                if (inc_terrain)
                {
                    old_f_ptr = &f_info[cave_feat[y][x]];

                    for (i = 1; i < z_info->f_max; i++)
                    {
                        f_ptr = &f_info[(i + cave_feat[y][x]) % z_info->f_max];

                        // stop when you find one
                        if (f_ptr->d_char == old_f_ptr->d_char)
                        {
                            found = true;
                            i = (i + cave_feat[y][x]) % z_info->f_max;
                            break;
                        }
                    }
                }

                // go through terrain list and find first terrain type with that
                // symbol.
                else
                {
                    for (i = 1; i < z_info->f_max; i++)
                    {
                        f_ptr = &f_info[i];

                        // stop when you find one
                        if (f_ptr->d_char == query)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // if one was found, then place it
                if (found)
                {
                    // delete any existing monster
                    if (cave_m_idx[y][x])
                    {
                        delete_monster_idx(cave_m_idx[y][x]);
                    }
                    // delete any existing item
                    if (cave_o_idx[y][x])
                    {
                        delete_object_idx(cave_o_idx[y][x]);
                    }
                    // place the new terrain
                    cave_info[y][x] &= ~(CAVE_MARK);
                    cave_set_feat(y, x, i);
                    update_view();
                }
            }

            // unexpected symbol
            else if ((query != ' ') && (query != '\t'))
            {
                bell("Illegal command for target mode!");
            }

            /* Handle "direction" */
            if (d)
            {
                /* Move */
                x += ddx[d];
                y += ddy[d];

                /* Slide into legality */
                if (x >= p_ptr->cur_map_wid - 1)
                    x--;
                else if (x <= 0)
                    x++;

                /* Slide into legality */
                if (y >= p_ptr->cur_map_hgt - 1)
                    y--;
                else if (y <= 0)
                    y++;

                /* Adjust panel if needed */
                if (adjust_panel(y, x))
                {
                    /* Handle stuff */
                    handle_stuff();

                    /* Recalculate interesting grids */
                    get_sorted_target_list(mode, range);
                }
            }
        }
    }

    /* Forget */
    temp_n = 0;

    /* Recenter around player */
    verify_panel();

    /* Handle stuff */
    handle_stuff();

    /* Failure to set target */
    if (!new_target)
    {
        // if we did not select a new target and were in targetting mode, then
        // abort target
        if (mode & (TARGET_KILL))
        {
            target_set_monster(0);
            health_track(0);
        }
        app_session_clear_interaction(app_session_current());
        app_session_pop_wait_scope(app_session_current(), &wait_scope);
        return (false);
    }

    /* Success */
    app_session_clear_interaction(app_session_current());
    app_session_pop_wait_scope(app_session_current(), &wait_scope);
    return (true);
}

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


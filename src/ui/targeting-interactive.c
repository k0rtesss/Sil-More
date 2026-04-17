/* File: targeting-interactive.c */

/*
 * Lane-local interactive targeting helpers split from targeting.c.
 */

#include "angband.h"
#include "app/app-session.h"
#include "log/log.h"
#include "platform-story-font.h"
#include "targeting.h"
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

static char targeting_inkey_with_wait_reason(void)
{
    return (char)ui_information_scene_wait_key_with_wait_reason(
        APP_WAIT_REASON_TARGETING);
}

/*
 * Monster health description.
 */
static void look_mon_desc(char* buf, size_t max, int m_idx)
{
    monster_type* m_ptr = &mon_list[m_idx];

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

    if (strlen(buf) == 1)
    {
        buf[0] = '\0';
    }
    else
    {
        buf[strlen(buf) - 2] = '\0';
        SDL_strlcat(buf, ") ", max);
    }
}

/*
 * Hack -- help "select" a location (see below)
 */
static s16b target_pick(int y1, int x1, int dy, int dx)
{
    int i, v;
    int x2, y2, x3, y3, x4, y4;
    int b_i = -1, b_v = 9999;

    for (i = 0; i < temp_n; i++)
    {
        x2 = temp_x[i];
        y2 = temp_y[i];
        x3 = (x2 - x1);
        y3 = (y2 - y1);

        if (dx && (x3 * dx <= 0))
            continue;
        if (dy && (y3 * dy <= 0))
            continue;

        x4 = ABS(x3);
        y4 = ABS(y3);

        if (dy && !dx && (x4 > y4))
            continue;
        if (dx && !dy && (y4 > x4))
            continue;

        v = ((x4 > y4) ? (x4 + x4 + y4) : (y4 + y4 + x4));
        if ((b_i >= 0) && (v >= b_v))
            continue;

        b_i = i;
        b_v = v;
    }

    return (b_i);
}

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
                platform_story_font_enable();
            query = target_set_interactive_aux(y, x, mode, info, use_story_look);
            if (use_story_look)
                platform_story_font_disable();

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
                platform_story_font_enable();
            query = target_set_interactive_aux(y, x, mode | TARGET_LOOK, info, use_story_look);
            if (use_story_look)
                platform_story_font_disable();

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


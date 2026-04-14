/* File: melee/melee-combat-display.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

/*
 * Combat roll tracking, display, and history.
 * Split from melee1.c for better code organization.
 */

#include "angband.h"
#include "app/app-ui.h"
#include "app/app-session.h"
#include "externs.h"
#include "log/log.h"
#include "melee/melee-attack.h"
#include "melee/melee-combat-display.h"
#include "ui/ui-information-scene.h"

combat_roll combat_rolls[2][MAX_COMBAT_ROLLS];
int combat_number = 0;
int combat_number_old = 0;
int turns_since_combat = 0;
char combat_roll_special_char = 0;
byte combat_roll_special_attr = 0;
combat_history_round combat_history[MAX_COMBAT_HISTORY];
int combat_history_head = 0;
int combat_history_count = 0;

#define COMBAT_HISTORY_UI_WINDOW 48

/*
 * Refresh the semantic dungeon snapshot after combat overlay state changes.
 * The map is included because main_combat_rolls still affects SCREEN_HGT.
 */
void refresh_main_combat_overlay(void)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (!session)
        return;

    snapshot = app_session_snapshot(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_DUNGEON)
        return;

    app_session_mark_snapshot_dirty(session,
        APP_SNAPSHOT_INVALIDATE_MAP | APP_SNAPSHOT_INVALIDATE_STATUS
            | APP_SNAPSHOT_INVALIDATE_PANES);
    (void)app_session_build_dungeon_snapshot(session, 0, 0, 0);
}

static bool combat_roll_is_player_attack(const combat_roll* roll)
{
    if (!roll)
        return false;
    if (roll->is_attacker_player)
        return true;

    return (roll->attacker_char == r_info[0].d_char)
        && (roll->attacker_attr == r_info[0].d_attr);
}

void new_combat_round(void)
{
    int i;

    log_trace("[ROUND] new_combat_round: ENTER turns_since_combat=%d, combat_number=%d, combat_number_old=%d", turns_since_combat, combat_number, combat_number_old);
    if (combat_number != 0)
        combat_number_old = combat_number;
    combat_number = 0;
    turns_since_combat++;

    /* Add the previous round to combat history before we lose it */
    add_combat_round_to_history();

    log_trace("[ROUND] new_combat_round: after inc, turns_since_combat=%d", turns_since_combat);
    if (turns_since_combat == 1)
    {
        // copy previous round's rolls into old round's rolls
        log_trace("[ROUND] copy current->old: combat_number_old(before)=%d", combat_number_old);
        for (i = 0; i < MAX_COMBAT_ROLLS; i++)
        {
            memcpy(&combat_rolls[1][i], &combat_rolls[0][i], sizeof(combat_roll));
            log_trace("[ROUND]   copied i=%d att_type=%d att=%d evn=%d dam=%d prot=%d atk=%c def=%c", i,
                      combat_rolls[1][i].att_type,
                      combat_rolls[1][i].att,
                      combat_rolls[1][i].evn,
                      combat_rolls[1][i].dam,
                      combat_rolls[1][i].prot,
                      combat_rolls[1][i].attacker_char,
                      combat_rolls[1][i].defender_char);
        }
    }
    else if (turns_since_combat == 11)
    {
        // reset old round's rolls
        combat_number_old = 0;
        for (i = 0; i < MAX_COMBAT_ROLLS; i++)
        {
            combat_rolls[1][i].att_type = COMBAT_ROLL_NONE;
        }
    }

    // reset new round's rolls
    for (i = 0; i < MAX_COMBAT_ROLLS; i++)
    {
        combat_rolls[0][i].att_type = COMBAT_ROLL_NONE;
    }
    log_trace("[ROUND] new_combat_round: EXIT turns_since_combat=%d, combat_number=%d, combat_number_old=%d", turns_since_combat, combat_number, combat_number_old);
}

/*
 * Update combat roll table part 1 (the attack rolls)
 */
void update_combat_rolls1(const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool vis, int att, int att_roll, int evn,
    int evn_roll)
{
    monster_race* r_ptr1;
    monster_race* r_ptr2;

    if (m_ptr1 == PLAYER)
    {
        r_ptr1 = &r_info[0];
    }
    else if (m_ptr1 == NULL)
    {
        // hack for traps hitting you
        r_ptr1 = NULL;
    }
    else if (p_ptr->image)
    {
        r_ptr1 = &r_info[m_ptr1->image_r_idx];
    }
    else
    {
        r_ptr1 = &r_info[m_ptr1->r_idx];
    }

    if (m_ptr2 == PLAYER)
    {
        r_ptr2 = &r_info[0];
    }
    else if (m_ptr2 == NULL)
    {
        // hack for attacking Morgoth's crown
        r_ptr2 = NULL;
    }
    else if (p_ptr->image)
    {
        r_ptr2 = &r_info[m_ptr2->image_r_idx];
    }
    else
    {
        r_ptr2 = &r_info[m_ptr2->r_idx];
    }

    log_trace("[ROLL1] enter: combat_number=%d old=%d turns_since_combat=%d", combat_number, combat_number_old, turns_since_combat);
    if (combat_number < MAX_COMBAT_ROLLS)
    {
        combat_rolls[0][combat_number].att_type = COMBAT_ROLL_ROLL;

        if (m_ptr1 == NULL)
        {
            combat_rolls[0][combat_number].attacker_char
                = combat_roll_special_char;
            combat_rolls[0][combat_number].attacker_attr
                = combat_roll_special_attr;
            combat_rolls[0][combat_number].is_attacker_player = false;
        }
        else if (vis || (m_ptr1 == PLAYER))
        {
            if (m_ptr1 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_attacker_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].attacker_char = r_ptr1->d_char;
                    combat_rolls[0][combat_number].attacker_attr = r_ptr1->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].attacker_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].attacker_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_attacker_player = false;
                combat_rolls[0][combat_number].attacker_char = graphics_are_ascii() ? r_ptr1->d_char : r_ptr1->x_char;

                if (p_ptr->rage)
                {
                    combat_rolls[0][combat_number].attacker_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].attacker_attr = graphics_are_ascii() ? r_ptr1->d_attr : r_ptr1->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_attacker_player = false;
            combat_rolls[0][combat_number].attacker_char = '?';
            combat_rolls[0][combat_number].attacker_attr = TERM_SLATE;
        }

        // hack for Iron Crown
        if (m_ptr2 == NULL)
        {
            combat_rolls[0][combat_number].defender_char = ']';
            combat_rolls[0][combat_number].defender_attr = TERM_L_DARK;
            combat_rolls[0][combat_number].is_defender_player = false;
        }
        else if (vis || (m_ptr2 == PLAYER))
        {
            if (m_ptr2 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_defender_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].defender_char = r_ptr2->d_char;
                    combat_rolls[0][combat_number].defender_attr = r_ptr2->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].defender_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].defender_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_defender_player = false;
                combat_rolls[0][combat_number].defender_char = graphics_are_ascii() ? r_ptr2->d_char : r_ptr2->x_char;

                if (p_ptr->rage)
                {
                    combat_rolls[0][combat_number].defender_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].defender_attr = graphics_are_ascii() ? r_ptr2->d_attr : r_ptr2->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_defender_player = false;
            combat_rolls[0][combat_number].defender_char = '?';
            combat_rolls[0][combat_number].defender_attr = TERM_SLATE;
        }

        combat_rolls[0][combat_number].att = att;
        combat_rolls[0][combat_number].att_roll = att_roll;
        combat_rolls[0][combat_number].evn = evn;
        combat_rolls[0][combat_number].evn_roll = evn_roll;

    log_trace("[ROLL1] added at index=%d atk=%c def=%c att=%d ar=%d evn=%d er=%d", combat_number,
          combat_rolls[0][combat_number].attacker_char,
          combat_rolls[0][combat_number].defender_char,
          combat_rolls[0][combat_number].att,
          combat_rolls[0][combat_number].att_roll,
          combat_rolls[0][combat_number].evn,
          combat_rolls[0][combat_number].evn_roll);
    combat_number++;
    turns_since_combat = 0;
    log_trace("[ROLL1] exit: combat_number=%d old=%d", combat_number, combat_number_old);
    }

}

/*
 * Update combat roll table part 1b (the attack when there is no roll made -- eg
 * breath attack)
 */
void update_combat_rolls1b(
    const monster_type* m_ptr1, const monster_type* m_ptr2, bool vis)
{
    monster_race* r_ptr1;
    monster_race* r_ptr2;

    if (m_ptr1 == PLAYER)
    {
        r_ptr1 = &r_info[0];
    }
    else if (m_ptr1 == NULL)
    {
        // hack for traps hitting you
        r_ptr1 = NULL;
    }
    else if (p_ptr->image)
    {
        r_ptr1 = &r_info[m_ptr1->image_r_idx];
    }
    else
    {
        r_ptr1 = &r_info[m_ptr1->r_idx];
    }

    if (m_ptr2 == PLAYER)
    {
        r_ptr2 = &r_info[0];
    }
    else if (p_ptr->image)
    {
        r_ptr2 = &r_info[m_ptr2->image_r_idx];
    }
    else
    {
        r_ptr2 = &r_info[m_ptr2->r_idx];
    }

    log_trace("[ROLL1B] enter: combat_number=%d old=%d turns_since_combat=%d", combat_number, combat_number_old, turns_since_combat);
    if (combat_number < MAX_COMBAT_ROLLS)
    {
        combat_rolls[0][combat_number].att_type = COMBAT_ROLL_AUTO;

        if (m_ptr1 == NULL)
        {
            combat_rolls[0][combat_number].attacker_char
                = combat_roll_special_char;
            combat_rolls[0][combat_number].attacker_attr
                = combat_roll_special_attr;
            combat_rolls[0][combat_number].is_attacker_player = false;
        }
        else if (vis || (m_ptr1 == PLAYER))
        {
            if (m_ptr1 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_attacker_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].attacker_char = r_ptr1->d_char;
                    combat_rolls[0][combat_number].attacker_attr = r_ptr1->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].attacker_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].attacker_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_attacker_player = false;
                combat_rolls[0][combat_number].attacker_char = graphics_are_ascii() ? r_ptr1->d_char : r_ptr1->x_char;

                if (p_ptr->rage)
                {
                    combat_rolls[0][combat_number].attacker_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].attacker_attr = graphics_are_ascii() ? r_ptr1->d_attr : r_ptr1->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_attacker_player = false;
            combat_rolls[0][combat_number].attacker_char = '?';
            combat_rolls[0][combat_number].attacker_attr = TERM_SLATE;
        }

        if (vis || (m_ptr2 == PLAYER))
        {
            if (m_ptr2 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_defender_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].defender_char = r_ptr2->d_char;
                    combat_rolls[0][combat_number].defender_attr = r_ptr2->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].defender_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].defender_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_defender_player = false;
                combat_rolls[0][combat_number].defender_char = graphics_are_ascii() ? r_ptr2->d_char : r_ptr2->x_char;

                if (p_ptr->rage)
                {
                    combat_rolls[0][combat_number].defender_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].defender_attr = graphics_are_ascii() ? r_ptr2->d_attr : r_ptr2->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_defender_player = false;
            combat_rolls[0][combat_number].defender_char = '?';
            combat_rolls[0][combat_number].defender_attr = TERM_SLATE;
        }

    log_trace("[ROLL1B] added index=%d atk=%c def=%c (AUTO)", combat_number,
          combat_rolls[0][combat_number].attacker_char,
          combat_rolls[0][combat_number].defender_char);
    combat_number++;
    turns_since_combat = 0;
    log_trace("[ROLL1B] exit: combat_number=%d old=%d", combat_number, combat_number_old);
    }

}

/*
 * Update combat roll table part 2 (the damage rolls)
 */
void update_combat_rolls2(int dd, int ds, int dam, int pd, int ps, int prot,
    int prt_percent, int dam_type, bool melee)
{
    log_trace("[ROLL2] enter: combat_number=%d old=%d last_index=%d", combat_number, combat_number_old, combat_number - 1);
    if (combat_number - 1 < MAX_COMBAT_ROLLS)
    {
        combat_roll* roll = &combat_rolls[0][combat_number - 1];

        roll->dam_type = dam_type;
        roll->dd = dd;
        roll->ds = ds;
        roll->dam = dam;
        roll->pd = pd;
        roll->ps = ps;
        roll->prot = prot;
        roll->prt_percent = prt_percent;
        roll->melee = melee;
        log_trace("[ROLL2] filled index=%d dd=%d ds=%d dam=%d pd=%d ps=%d prot=%d prt%%=%d melee=%d", 
                  combat_number - 1, dd, ds, dam, pd, ps, prot, prt_percent, melee);

        // deal with protection for the player
        // this hackishly uses the pd and ps to store the min and max prot for
        // the player
        if (pd == -1)
        {
            // use the protection values for pure elemental types if there was
            // no attack roll
            if (roll->att_type == COMBAT_ROLL_AUTO)
            {
                roll->pd = p_min(dam_type, melee);
                roll->ps = p_max(dam_type, melee);
            }
            // otherwise use the normal protection values
            else
            {
                roll->pd = p_min(GF_HURT, melee);
                roll->ps = p_max(GF_HURT, melee);
            }
    }
    log_trace("[ROLL2] exit: index=%d done", combat_number - 1);

        app_session_note_animation(app_session_current(),
            APP_ANIMATION_HINT_DAMAGE,
            roll->is_defender_player ? APP_DUNGEON_PLAYER_SUBJECT : 0,
            roll->is_attacker_player ? APP_DUNGEON_PLAYER_SUBJECT : 0,
            dam, dam_type,
            APP_SNAPSHOT_INVALIDATE_STATUS | APP_SNAPSHOT_INVALIDATE_PANES
                | APP_SNAPSHOT_INVALIDATE_MAP);
    }
    
}

/*
 * Clear the live combat-roll presentation after settings changes.
 * SDL snapshot mode handles this by rebuilding the main dungeon snapshot.
 */
void clear_main_combat_rolls_area(void)
{
    refresh_main_combat_overlay();
}

/*
 * Add the current combat round to the history buffer
 */
void add_combat_round_to_history(void)
{
    int i;
    
    /* Only add if there were actual combat rolls this round */
    if (combat_number_old == 0) {
        return;
    }

    /* Get next position in circular buffer */
    combat_history_head = (combat_history_head + 1) % MAX_COMBAT_HISTORY;
    
    /* Update count if we haven't filled the buffer yet */
    if (combat_history_count < MAX_COMBAT_HISTORY) {
        combat_history_count++;
    }
    
    /* Store the combat round data */
    combat_history[combat_history_head].turn_count = turn;
    combat_history[combat_history_head].num_rolls = combat_number_old;
    
    /* Copy the combat rolls from the previous round */
    for (i = 0; i < combat_number_old && i < MAX_COMBAT_ROLLS; i++) {
        memcpy(&combat_history[combat_history_head].rolls[i], &combat_rolls[1][i], sizeof(combat_roll));
    }
}

static int combat_history_total_rolls(void)
{
    int total = 0;

    for (int i = 0; i < combat_history_count; i++)
        total += combat_history[i].num_rolls;

    return total;
}

static bool combat_history_locate_roll(int absolute_index, int* history_idx,
    int* roll_idx)
{
    int total_rolls = 0;

    if (absolute_index < 0)
        return false;

    for (int h = 0; h < combat_history_count; h++)
    {
        int hist_idx = (combat_history_head - h + MAX_COMBAT_HISTORY)
            % MAX_COMBAT_HISTORY;

        if (total_rolls + combat_history[hist_idx].num_rolls > absolute_index)
        {
            if (history_idx)
                *history_idx = hist_idx;
            if (roll_idx)
                *roll_idx = absolute_index - total_rolls;
            return true;
        }

        total_rolls += combat_history[hist_idx].num_rolls;
    }

    return false;
}

static bool combat_history_try_add_detail_line(app_ui_panel* panel, byte attr,
    cptr text)
{
    if (!panel || !text || !text[0])
        return true;
    if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
        return true;

    return app_ui_panel_add_detail_line(panel, attr, text);
}

static void combat_history_format_row_label(char* buf, size_t buf_size,
    const combat_roll* roll)
{
    int att_total;
    int evn_total;
    int hit_margin;
    int net_dam;

    if (!buf || buf_size == 0)
        return;

    SDL_strlcpy(buf, "", buf_size);
    if (!roll)
        return;

    net_dam = roll->dam - roll->prot;
    if (net_dam < 0)
        net_dam = 0;

    if (roll->att_type == COMBAT_ROLL_ROLL)
    {
        att_total = roll->att + roll->att_roll;
        evn_total = roll->evn + roll->evn_roll;
        hit_margin = att_total - evn_total;

        if (hit_margin > 0)
        {
            strnfmt(buf, buf_size, "Atk %d vs %d, hit %d, net %d",
                att_total, evn_total, hit_margin, net_dam);
        }
        else
        {
            strnfmt(buf, buf_size, "Atk %d vs %d, miss",
                att_total, evn_total);
        }
    }
    else if (roll->att_type == COMBAT_ROLL_AUTO)
    {
        strnfmt(buf, buf_size, "Automatic effect, net %d", net_dam);
    }
}

static void combat_history_format_row_meta(char* buf, size_t buf_size,
    const combat_history_round* round, const combat_roll* roll)
{
    if (!buf || buf_size == 0)
        return;

    SDL_strlcpy(buf, "", buf_size);
    if (!round || !roll)
        return;

    if (roll->dd > 0 && roll->ds > 0)
    {
        strnfmt(buf, buf_size, "Turn %d  %dd%d raw %d prot %d",
            round->turn_count, roll->dd, roll->ds, roll->dam, roll->prot);
    }
    else
    {
        strnfmt(buf, buf_size, "Turn %d", round->turn_count);
    }
}

static void combat_history_build_search_text(char* buf, size_t buf_size,
    const combat_history_round* round, const combat_roll* roll)
{
    if (!buf || buf_size == 0)
        return;

    SDL_strlcpy(buf, "", buf_size);
    if (!round || !roll)
        return;

    strnfmt(buf, buf_size, "Turn %d %c %c (%+d) (%dd%d)",
        round->turn_count,
        roll->attacker_char ? roll->attacker_char : '?',
        roll->defender_char ? roll->defender_char : '?', roll->att, roll->dd,
        roll->ds);
}

static bool combat_history_roll_matches(const combat_history_round* round,
    const combat_roll* roll, cptr shower)
{
    char search_buf[120];

    if (!shower || !shower[0] || !round || !roll)
        return false;

    combat_history_build_search_text(search_buf, sizeof(search_buf), round,
        roll);
    return strstr(search_buf, shower) != NULL;
}

static bool combat_history_add_selected_roll_detail(app_ui_panel* panel,
    const combat_history_round* round, const combat_roll* roll)
{
    char buf[APP_UI_TEXT_MAX];
    int net_dam;

    if (!panel || !round || !roll)
        return false;

    strnfmt(buf, sizeof(buf), "Turn %d", round->turn_count);
    app_ui_panel_set_detail_title(panel,
        combat_roll_is_player_attack(roll) ? TERM_L_BLUE : TERM_WHITE, buf);

    if (!combat_history_try_add_detail_line(panel, TERM_SLATE,
            "Attacker and defender are shown as the row icons."))
    {
        return false;
    }

    if (roll->att_type == COMBAT_ROLL_ROLL)
    {
        int att_total = roll->att + roll->att_roll;
        int evn_total = roll->evn + roll->evn_roll;
        int hit_margin = att_total - evn_total;

        strnfmt(buf, sizeof(buf), "Attack total: %d (base %+d, roll %d)",
            att_total, roll->att, roll->att_roll);
        if (!combat_history_try_add_detail_line(panel,
                combat_roll_is_player_attack(roll) ? TERM_L_BLUE : TERM_WHITE,
                buf))
        {
            return false;
        }

        strnfmt(buf, sizeof(buf), "Evasion total: %d (base %+d, roll %d)",
            evn_total, roll->evn, roll->evn_roll);
        if (!combat_history_try_add_detail_line(panel, TERM_WHITE, buf))
            return false;

        if (hit_margin > 0)
            strnfmt(buf, sizeof(buf), "Hit margin: %d", hit_margin);
        else
            SDL_strlcpy(buf, "Hit margin: none", sizeof(buf));
        if (!combat_history_try_add_detail_line(panel,
                (hit_margin > 0) ? TERM_L_RED : TERM_SLATE, buf))
        {
            return false;
        }
    }
    else if (roll->att_type == COMBAT_ROLL_AUTO)
    {
        if (!combat_history_try_add_detail_line(panel, TERM_SLATE,
                "Automatic effect; no attack roll was made."))
        {
            return false;
        }
    }

    net_dam = roll->dam - roll->prot;
    if (net_dam < 0)
        net_dam = 0;

    if (roll->dd > 0 && roll->ds > 0)
    {
        strnfmt(buf, sizeof(buf), "Damage: %dd%d -> %d raw",
            roll->dd, roll->ds, roll->dam);
        if (!combat_history_try_add_detail_line(panel,
                combat_roll_is_player_attack(roll) ? TERM_L_BLUE : TERM_WHITE,
                buf))
        {
            return false;
        }
    }

    strnfmt(buf, sizeof(buf), "Protection: %d (%d%%)",
        roll->prot, roll->prt_percent);
    if (!combat_history_try_add_detail_line(panel, TERM_SLATE, buf))
        return false;

    strnfmt(buf, sizeof(buf), "Net damage: %d", net_dam);
    if (!combat_history_try_add_detail_line(panel,
            (net_dam > 0) ? TERM_L_RED : TERM_SLATE, buf))
    {
        return false;
    }

    if (!combat_history_try_add_detail_line(panel, TERM_SLATE,
            roll->melee ? "Context: melee." : "Context: ranged/effect."))
    {
        return false;
    }

    return true;
}

static bool combat_history_build_browser_scene(app_ui_scene* scene,
    int selected_index, int total_rolls, cptr shower)
{
    app_ui_panel* panel;
    char subtitle[APP_UI_TEXT_MAX];
    int window_start = 0;
    int window_end = 0;

    if (!scene)
        return false;

    if (selected_index < 0)
        selected_index = 0;
    if (total_rolls > 0 && selected_index >= total_rolls)
        selected_index = total_rolls - 1;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 1100, 2400);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Combat History");

    if (total_rolls > 0)
    {
        strnfmt(subtitle, sizeof(subtitle), "Selected %d of %d rolls",
            selected_index + 1, total_rolls);
    }
    else
    {
        SDL_strlcpy(subtitle, "No recorded combat rolls", sizeof(subtitle));
    }
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

    if (shower && shower[0])
    {
        char filter_buf[APP_UI_TEXT_MAX];

        strnfmt(filter_buf, sizeof(filter_buf), "Highlight: %s", shower);
        if (!app_ui_panel_add_body_line(panel, TERM_L_BLUE, filter_buf))
            return false;
    }

    if (total_rolls <= 0)
    {
        if (!app_ui_panel_add_body_line(panel, TERM_SLATE,
                "No combat rolls recorded."))
        {
            return false;
        }

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "Esc", "Back");
        return true;
    }

    window_start = selected_index - (COMBAT_HISTORY_UI_WINDOW / 2);
    if (window_start < 0)
        window_start = 0;
    if (window_start > total_rolls - COMBAT_HISTORY_UI_WINDOW)
        window_start = MAX(0, total_rolls - COMBAT_HISTORY_UI_WINDOW);
    window_end = MIN(total_rolls, window_start + COMBAT_HISTORY_UI_WINDOW);

    for (int absolute_index = window_start; absolute_index < window_end;
         absolute_index++)
    {
        int history_idx = -1;
        int roll_idx = -1;
        combat_history_round* round;
        combat_roll* roll;
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        bool selected;
        bool matches;
        byte row_attr;
        byte meta_attr;
        u16b row_index;

        if (!combat_history_locate_roll(absolute_index, &history_idx, &roll_idx))
            continue;

        round = &combat_history[history_idx];
        roll = &round->rolls[roll_idx];
        if (roll->att_type == COMBAT_ROLL_NONE)
            continue;

        combat_history_format_row_label(label, sizeof(label), roll);
        combat_history_format_row_meta(meta, sizeof(meta), round, roll);
        selected = absolute_index == selected_index;
        matches = combat_history_roll_matches(round, roll, shower);
        row_attr = combat_roll_is_player_attack(roll) ? TERM_L_BLUE : TERM_WHITE;
        meta_attr = matches ? TERM_YELLOW : TERM_SLATE;
        row_index = panel->row_count;

        if (!app_ui_panel_add_row_ex(panel, (s16b)absolute_index, row_attr,
                meta_attr, roll->attacker_attr, roll->attacker_char, true,
                selected, "", label, meta))
        {
            return false;
        }

        panel->rows[row_index].extra_icon_attr = roll->defender_attr;
        panel->rows[row_index].extra_icon_char = roll->defender_char;
    }

    {
        int history_idx = -1;
        int roll_idx = -1;

        if (combat_history_locate_roll(selected_index, &history_idx, &roll_idx))
        {
            combat_history_round* round = &combat_history[history_idx];
            combat_roll* roll = &round->rolls[roll_idx];

            if (!combat_history_add_selected_roll_detail(panel, round, roll))
                return false;
        }
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "p/n", "Page");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "=", "Show");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "/", "Find");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    return true;
}

static bool do_cmd_combat_history_information_scene(void)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    char shower[80];
    char finder[80];
    int selected_index = 0;
    int n;

    if (!ui_information_scene_enter(&scope))
        return false;

    SDL_strlcpy(finder, "", sizeof(finder));
    SDL_strlcpy(shower, "", sizeof(shower));
    n = combat_history_total_rolls();

    while (true)
    {
        int old_selected_index;
        char ch;

        if (!combat_history_build_browser_scene(&scene, selected_index, n,
                shower)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = (char)ui_information_scene_wait_key();
        if (ch == ESCAPE)
            break;

        old_selected_index = selected_index;

        if (n <= 0)
        {
            bell(NULL);
            continue;
        }

        if (ch == '=')
        {
            if (!term_get_string("Show: ", shower, sizeof(shower)))
                continue;
            continue;
        }
        if (ch == '/')
        {
            s16b z;

            if (term_get_string("Find: ", finder, sizeof(finder)))
            {
                SDL_strlcpy(shower, finder, sizeof(shower));

                for (z = selected_index + 1; z < n; z++)
                {
                    int history_idx = -1;
                    int roll_idx = -1;
                    combat_roll* search_roll;
                    char search_buf[120];

                    if (!combat_history_locate_roll(z, &history_idx, &roll_idx))
                        continue;

                    search_roll = &combat_history[history_idx].rolls[roll_idx];
                    strnfmt(search_buf, sizeof(search_buf),
                        "Turn %d %c (%+d) (%dd%d)",
                        combat_history[history_idx].turn_count,
                        search_roll->attacker_char, search_roll->att,
                        search_roll->dd, search_roll->ds);

                    if (strstr(search_buf, finder))
                    {
                        selected_index = z;
                        break;
                    }
                }
            }
            continue;
        }

        if ((ch == 'p') || (ch == KTRL('P')) || (ch == ' '))
        {
            if (selected_index + 20 < n)
                selected_index += 20;
        }
        if (ch == '+')
        {
            if (selected_index + 10 < n)
                selected_index += 10;
        }
        if (ch == '8')
        {
            if (selected_index >= 1)
                selected_index -= 1;
        }
        if ((ch == 'n') || (ch == KTRL('N')))
            selected_index = (selected_index >= 20)
                ? (selected_index - 20)
                : 0;
        if (ch == '-')
            selected_index = (selected_index >= 10)
                ? (selected_index - 10)
                : 0;
        if ((ch == '2') || (ch == '\n') || (ch == '\r'))
        {
            if (selected_index + 1 < n)
                selected_index += 1;
        }

        if (selected_index == old_selected_index)
            bell(NULL);
    }

    ui_information_scene_leave(&scope);
    return true;
}

/*
 * Display combat history menu similar to message log
 */
void do_cmd_combat_history(void)
{
    if (!ui_information_scene_supported())
    {
        log_warn("combat history: snapshot renderer required; legacy renderer removed");
        msg_print("Combat history viewer requires the snapshot UI renderer.");
        return;
    }

    if (!do_cmd_combat_history_information_scene())
    {
        log_warn("combat history: information-scene presentation failed on the snapshot renderer path");
        msg_print("Combat history viewer unavailable.");
    }
}

/*
 * Refresh the live combat-roll presentation after state changes.
 * SDL snapshot mode renders this semantically instead of drawing term rows.
 */
void display_main_combat_rolls(void)
{
    log_trace("display_main_combat_rolls: Starting - combat_number=%d, combat_number_old=%d, num_lines=%d",
        combat_number, combat_number_old, op_ptr->main_combat_rolls);
    refresh_main_combat_overlay();
}





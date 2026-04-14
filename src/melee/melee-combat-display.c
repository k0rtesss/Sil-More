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

static int  original_main_combat_rolls = -1;
static bool main_combat_rolls_deferral_active = false;
static bool main_combat_rolls_restored = false;

static void maybe_restore_main_combat_rolls(void)
{
    if (main_combat_rolls_deferral_active && !main_combat_rolls_restored) {
        if (op_ptr->main_combat_rolls == 0 && original_main_combat_rolls > 0) {
            op_ptr->main_combat_rolls = original_main_combat_rolls;
            main_combat_rolls_restored = true;
            main_combat_rolls_deferral_active = false;
            p_ptr->redraw |= (PR_MAP); /* recompute SCREEN_HGT */
            log_trace("maybe_restore_main_combat_rolls: restored to %d", op_ptr->main_combat_rolls);
        }
    }
}

static bool combat_main_view_uses_legacy_term_rendering(void)
{
    return !ui_information_scene_supported();
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
    /* Ensure we restore deferred main_combat_rolls before recording first roll */
    maybe_restore_main_combat_rolls();
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

    /* Window stuff - DO NOT set flag here; wait for update_combat_rolls2() to complete the data */
    /* p_ptr->window |= (PW_COMBAT_ROLLS); */
}

/*
 * Update combat roll table part 1b (the attack when there is no roll made -- eg
 * breath attack)
 */
void update_combat_rolls1b(
    const monster_type* m_ptr1, const monster_type* m_ptr2, bool vis)
{
    /* Ensure we restore deferred main_combat_rolls before recording first roll */
    maybe_restore_main_combat_rolls();
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

    /* Window stuff - DO NOT set flag here; defer to main loop to avoid mid-combat updates */
    /* p_ptr->window |= (PW_COMBAT_ROLLS); */
}

/*
 * Update combat roll table part 2 (the damage rolls)
 */
void update_combat_rolls2(int dd, int ds, int dam, int pd, int ps, int prot,
    int prt_percent, int dam_type, bool melee)
{
    /* Ensure we restore deferred main_combat_rolls before completing roll */
    maybe_restore_main_combat_rolls();
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
    
    /* Window stuff - DO NOT set flag here; defer to main loop to avoid mid-combat updates */
    /* p_ptr->window |= (PW_COMBAT_ROLLS); */
}

/*
 * Display combat rolls in a window
 */

typedef struct combat_display_entry
{
    int round;
    int index;
} combat_display_entry;

static int collect_combat_display_entries(combat_display_entry* ordered, int max_entries)
{
    int count = 0;

    for (int round = 0; round < 2; round++)
    {
        int combat_num_for_round = (round == 0) ? combat_number : combat_number_old;
        if (combat_num_for_round <= 0)
            continue;

        int player_indices[MAX_COMBAT_ROLLS];
        int monster_indices[MAX_COMBAT_ROLLS];
        int player_count = 0;
        int monster_count = 0;

        for (int idx = combat_num_for_round - 1; idx >= 0; idx--)
        {
            if (combat_rolls[round][idx].att_type == COMBAT_ROLL_NONE)
                continue;

            if (combat_rolls[round][idx].is_attacker_player)
            {
                if (player_count < MAX_COMBAT_ROLLS)
                    player_indices[player_count++] = idx;
            }
            else
            {
                if (monster_count < MAX_COMBAT_ROLLS)
                    monster_indices[monster_count++] = idx;
            }
        }

        for (int i = 0; (i < player_count) && (count < max_entries); i++)
        {
            ordered[count].round = round;
            ordered[count].index = player_indices[i];
            count++;
        }

        for (int i = 0; (i < monster_count) && (count < max_entries); i++)
        {
            ordered[count].round = round;
            ordered[count].index = monster_indices[i];
            count++;
        }
    }

    return count;
}

static void draw_combat_roll_line(int row, int base_col_offset,
    const combat_roll* roll)
{
    char buf[80];
    int net_att = 0;
    int net_dam;
    int a_att;
    int a_evn;
    int a_hit;
    int a_dam_roll;
    int a_prot_roll;
    int a_net_dam = TERM_L_RED;
    int res = 1;

    log_trace("draw_combat_roll_line: row=%d att_type=%d attacker=%c defender=%c",
        row, roll->att_type, roll->attacker_char, roll->defender_char);

    if (roll->is_defender_player)
    {
        switch (roll->dam_type)
        {
        case GF_FIRE:
            res = resist_fire();
            break;
        case GF_COLD:
            res = resist_cold();
            break;
        case GF_POIS:
            res = resist_pois();
            a_net_dam = TERM_GREEN;
            break;
        case GF_DARK:
            res = resist_dark();
            break;
        default:
            res = 1;
            a_net_dam = TERM_L_RED;
            break;
        }
    }

    if (roll->is_attacker_player)
    {
        a_att = TERM_L_BLUE;
        a_evn = TERM_WHITE;
        a_hit = TERM_L_RED;
        a_dam_roll = TERM_L_BLUE;
        if (roll->prt_percent >= 100)
            a_prot_roll = TERM_WHITE;
        else if (roll->prt_percent >= 1)
            a_prot_roll = TERM_SLATE;
        else
            a_prot_roll = TERM_DARK;
    }
    else
    {
        a_att = TERM_WHITE;
        a_evn = TERM_L_BLUE;
        a_hit = TERM_L_RED;
        a_dam_roll = TERM_WHITE;
        if (roll->prt_percent >= 100)
            a_prot_roll = TERM_L_BLUE;
        else if (roll->prt_percent >= 1)
            a_prot_roll = TERM_BLUE;
        else
            a_prot_roll = TERM_DARK;
    }

    Term_putstr(base_col_offset, row, 1, TERM_WHITE, " ");
    Term_queue_char(base_col_offset + 1, row,
        roll->attacker_attr, roll->attacker_char, 0, 0);
    if (use_bigtile && !graphics_are_ascii())
    {
        if ((roll->attacker_attr & 0x80) && ((byte)roll->attacker_char & 0x80))
            Term_queue_char(base_col_offset + 2, row, 255, -1, 0, 0);
        else
            Term_queue_char(base_col_offset + 2, row, TERM_WHITE, ' ', 0, 0);
    }

    int tile_offset = (use_bigtile && !graphics_are_ascii()) ? 1 : 0;
    int base_col = base_col_offset + 2 + tile_offset;

    if (roll->att_type == COMBAT_ROLL_ROLL)
    {
        int col = base_col;

        if (roll->att < 10)
        {
            strnfmt(buf, sizeof(buf), "  (%+d)", roll->att);
        }
        else
        {
            strnfmt(buf, sizeof(buf), " (%+d)", roll->att);
        }
        Term_putstr(col, row, -1, a_att, buf);
        col += 6;

        strnfmt(buf, sizeof(buf), "%4d", roll->att + roll->att_roll);
        Term_putstr(col, row, -1, a_att, buf);
        col += 4;

        net_att = roll->att_roll + roll->att - roll->evn_roll - roll->evn;
        if (net_att > 0)
        {
            strnfmt(buf, sizeof(buf), "%4d", net_att);
            Term_putstr(col, row, -1, a_hit, buf);
        }
        else
        {
            Term_putstr(col, row, -1, TERM_SLATE, "   -");
        }
        col += 4;

        strnfmt(buf, sizeof(buf), "%4d", roll->evn + roll->evn_roll);
        Term_putstr(col, row, -1, a_evn, buf);
        col += 4;

        if (roll->evn < 10)
        {
            strnfmt(buf, sizeof(buf), "   [%+d]", roll->evn);
        }
        else
        {
            strnfmt(buf, sizeof(buf), "  [%+d]", roll->evn);
        }
        Term_putstr(col, row, -1, a_evn, buf);
        col += 7;

        Term_putstr(col, row, 1, TERM_WHITE, " ");
        col += 1;

        Term_queue_char(col, row,
            roll->defender_attr, roll->defender_char, 0, 0);
        if (use_bigtile && !graphics_are_ascii())
        {
            if ((roll->defender_attr & 0x80) && ((byte)roll->defender_char & 0x80))
                Term_queue_char(col + 1, row, 255, -1, 0, 0);
            else
                Term_queue_char(col + 1, row, TERM_WHITE, ' ', 0, 0);
            col += 2;
        }
        else
        {
            col += 1;
        }

        int damage_col = base_col + 25 + 1;
        if (use_bigtile && !graphics_are_ascii())
            damage_col += 2;
        else
            damage_col += 1;

        if ((net_att > 0) || (roll->att_type == COMBAT_ROLL_AUTO))
        {
            Term_putstr(damage_col, row, -1, TERM_L_DARK, "  ->");
            damage_col += 4;

            if (roll->ds < 10)
            {
                strnfmt(buf, sizeof(buf), "   (%dd%d) ", roll->dd, roll->ds);
            }
            else
            {
                strnfmt(buf, sizeof(buf), "  (%dd%d)", roll->dd, roll->ds);
            }
            Term_putstr(damage_col, row, -1, a_dam_roll, buf);
            damage_col += 9;

            strnfmt(buf, sizeof(buf), "%4d", roll->dam);
            Term_putstr(damage_col, row, -1, a_dam_roll, buf);
            damage_col += 4;

            net_dam = roll->dam - roll->prot;
            if (net_dam < 0)
                net_dam = 0;

            if (net_dam > 0)
            {
                strnfmt(buf, sizeof(buf), "%4d", net_dam);
                Term_addstr(-1, a_net_dam, buf);
            }
            else
            {
                Term_addstr(-1, TERM_SLATE, "   -");
            }

            strnfmt(buf, sizeof(buf), "%4d", roll->prot);
            Term_addstr(-1, a_prot_roll, buf);

            log_debug("COMBAT_ROLL_ROLL protection: is_defender_player=%d",
                roll->is_defender_player);

            if (roll->is_defender_player)
            {
                strnfmt(buf, sizeof(buf), "  [%d-%d]", (roll->pd * roll->prt_percent) / 100,
                    (roll->ps * roll->prt_percent) / 100);
                Term_addstr(-1, a_prot_roll, buf);
            }
            else
            {
                if ((roll->ps < 1) || (roll->pd < 1))
                {
                    SDL_strlcpy(buf, "        ", sizeof(buf));
                    Term_addstr(-1, a_prot_roll, buf);
                }
                else if (roll->ps < 10)
                {
                    strnfmt(buf, sizeof(buf), "   [%dd%d]", roll->pd, roll->ps);
                    Term_addstr(-1, a_prot_roll, buf);
                }
                else
                {
                    strnfmt(buf, sizeof(buf), "  [%dd%d]", roll->pd, roll->ps);
                    Term_addstr(-1, a_prot_roll, buf);
                }
                if ((roll->prt_percent > 0) && (roll->prt_percent < 100))
                {
                    strnfmt(buf, sizeof(buf), " (%d%%)", roll->prt_percent);
                    Term_addstr(-1, a_prot_roll, buf);
                }
            }
        }
    }
    else if (roll->att_type == COMBAT_ROLL_AUTO)
    {
        int col = base_col;
        Term_putstr(col, row, -1, TERM_L_DARK,
            "                         ");
        col += 25;

        Term_putstr(col, row, 1, TERM_WHITE, " ");
        col += 1;

        Term_queue_char(col, row,
            roll->defender_attr, roll->defender_char, 0, 0);
        if (use_bigtile && !graphics_are_ascii())
        {
            if ((roll->defender_attr & 0x80) && ((byte)roll->defender_char & 0x80))
                Term_queue_char(col + 1, row, 255, -1, 0, 0);
            else
                Term_queue_char(col + 1, row, TERM_WHITE, ' ', 0, 0);
            col += 2;
        }
        else
        {
            col += 1;
        }

        int damage_col = base_col + 25 + 1;
        if (use_bigtile && !graphics_are_ascii())
            damage_col += 2;
        else
            damage_col += 1;

        int net_auto;
        if (roll->melee)
            net_auto = roll->dam - roll->prot;
        else if (res > 0)
            net_auto = (roll->dam / res) - roll->prot;
        else
            net_auto = (roll->dam * (-res)) - roll->prot;

        Term_putstr(damage_col, row, -1, TERM_L_DARK, "  ->");
        damage_col += 4;

        if (roll->ds < 10)
        {
            strnfmt(buf, sizeof(buf), "   (%dd%d) ", roll->dd, roll->ds);
        }
        else
        {
            strnfmt(buf, sizeof(buf), "  (%dd%d)", roll->dd, roll->ds);
        }
        Term_putstr(damage_col, row, -1, a_dam_roll, buf);
        damage_col += 9;

        strnfmt(buf, sizeof(buf), "%4d", roll->dam);
        Term_putstr(damage_col, row, -1, a_dam_roll, buf);
        damage_col += 4;

        if (net_auto > 0)
        {
            strnfmt(buf, sizeof(buf), "%4d", net_auto);
            Term_addstr(-1, a_net_dam, buf);
        }
        else
        {
            Term_addstr(-1, TERM_SLATE, "   -");
        }

        strnfmt(buf, sizeof(buf), "%4d", roll->prot);
        Term_addstr(-1, a_prot_roll, buf);

        log_debug("COMBAT_ROLL_AUTO protection: is_defender_player=%d",
            roll->is_defender_player);

        if (roll->is_defender_player)
        {
            if (!(roll->melee))
            {
                if (res > 1)
                {
                    strnfmt(buf, sizeof(buf), "  1/%d then", res);
                    Term_addstr(-1, TERM_L_BLUE, buf);
                }
                else if (res < 0)
                {
                    strnfmt(buf, sizeof(buf), "  x%d then", -res);
                    Term_addstr(-1, TERM_L_BLUE, buf);
                }
            }

            strnfmt(buf, sizeof(buf), "  [%d-%d]", roll->pd, roll->ps);
            Term_addstr(-1, a_prot_roll, buf);
        }
        else
        {
            if ((roll->ps < 1) || (roll->pd < 1))
            {
                SDL_strlcpy(buf, "        ", sizeof(buf));
                Term_addstr(-1, a_prot_roll, buf);
            }
            else if (roll->ps < 10)
            {
                strnfmt(buf, sizeof(buf), "   [%dd%d]", roll->pd, roll->ps);
                Term_addstr(-1, a_prot_roll, buf);
            }
            else
            {
                strnfmt(buf, sizeof(buf), "  [%dd%d]", roll->pd, roll->ps);
                Term_addstr(-1, a_prot_roll, buf);
            }
            if ((roll->prt_percent > 0) && (roll->prt_percent < 100))
            {
                strnfmt(buf, sizeof(buf), " (%d%%)", roll->prt_percent);
                Term_addstr(-1, a_prot_roll, buf);
            }
        }
    }
}

void display_combat_rolls(void)
{

    int i;

    log_trace("display_combat_rolls: Starting - combat_number=%d, combat_number_old=%d",
        combat_number, combat_number_old);

    for (i = 0; i < Term->hgt; i++)
    {
        Term_erase(0, i, 255);
    }

    combat_display_entry ordered[MAX_COMBAT_ROLLS * 2];
    int total_entries = collect_combat_display_entries(ordered, MAX_COMBAT_ROLLS * 2);
    int entries_to_show = MIN(total_entries, Term->hgt);

    for (int entry_idx = 0; entry_idx < entries_to_show; entry_idx++)
    {
        int round = ordered[entry_idx].round;
        int idx = ordered[entry_idx].index;

        draw_combat_roll_line(entry_idx, 0, &combat_rolls[round][idx]);
    }
}


/*
 * Clear all 4 combat rolls lines in main terminal (used when settings change)
 */
void clear_main_combat_rolls_area(void)
{
    int i;
    const int col_offset = COL_MAP; /* align with display offset */

    if (!combat_main_view_uses_legacy_term_rendering())
        return;

    /* Clear all 4 possible lines (one row up from bottom to avoid status line) */
    for (i = 0; i < 4; i++)
    {
        Term_putstr(col_offset, Term->hgt - 4 - 1 + i, 65, TERM_WHITE, "                                                                 ");
    }
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

static bool combat_history_information_scene_pause(
    ui_information_scene_scope* scope)
{
    if (!scope)
        return false;

    ui_information_scene_leave(scope);
    return true;
}

static bool combat_history_information_scene_resume(
    ui_information_scene_scope* scope)
{
    if (!scope)
        return false;

    return ui_information_scene_enter(scope);
}

static app_ui_panel* combat_history_begin_document_scene(app_ui_scene* scene)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_DOCUMENT;
    panel->min_width_px = 0;
    panel->width_cap_px = 0;
    return panel;
}

static bool combat_history_scene_add_clipped_text(app_ui_scene* scene,
    app_ui_panel* panel, int row, int view_col, int view_width, int col,
    byte attr, cptr text)
{
    char clipped[APP_UI_TEXT_MAX];
    size_t len;
    int left;
    int right;
    int copy_len;

    if (!scene || !panel)
        return false;
    if (!text || !text[0] || view_width <= 0)
        return true;

    len = strlen(text);
    if (len == 0)
        return true;

    left = MAX(col, view_col);
    right = MIN(col + (int)len, view_col + view_width);
    if (right <= left)
        return true;

    copy_len = right - left;
    if (copy_len >= (int)sizeof(clipped))
        copy_len = (int)sizeof(clipped) - 1;
    memcpy(clipped, text + (left - col), (size_t)copy_len);
    clipped[copy_len] = '\0';

    return app_ui_panel_add_document_text_ex(scene, panel, (s16b)row,
        (s16b)(left - view_col), attr, STORY_FLAG_CELL_ALIGN, clipped);
}

static int combat_history_scene_glyph_display_width(void)
{
    return (use_bigtile && !graphics_are_ascii()) ? 2 : 1;
}

static byte combat_history_scene_glyph_render_width(byte attr, char ch)
{
    if (use_bigtile && !graphics_are_ascii() && (attr & TILE_FLAG)
        && (((byte)ch) & TILE_FLAG))
    {
        return 2;
    }

    return 1;
}

static bool combat_history_scene_add_glyph(app_ui_scene* scene,
    app_ui_panel* panel, int row, int view_col, int view_width, int col,
    byte attr, char ch)
{
    int dst_col;
    byte render_width;

    if (!scene || !panel)
        return false;
    if (view_width <= 0 || col < view_col || col >= view_col + view_width)
        return true;

    dst_col = col - view_col;
    render_width = combat_history_scene_glyph_render_width(attr, ch);
    if (render_width > (byte)(view_width - dst_col))
    {
        if (render_width > 1)
            return true;
        render_width = 1;
    }

    return app_ui_panel_add_document_cell_ex(scene, panel, (s16b)row,
        (s16b)dst_col, attr, ch, 0, 0, 0, render_width);
}

static bool combat_history_build_ui_scene(app_ui_scene* scene, int start_index,
    int offset, int wid, int hgt, int total_rolls, cptr shower, int* out_shown)
{
    app_ui_panel* panel;
    int j;
    char buf[120];

    (void)shower;

    if (!scene)
        return false;

    panel = combat_history_begin_document_scene(scene);
    if (!panel)
        return false;

    for (j = 0; (j < hgt - 4) && (start_index + j < total_rolls); j++)
    {
        int history_idx = -1;
        int roll_idx = -1;
        combat_history_round* round;
        combat_roll* roll;
        int a_att;
        int a_evn;
        int a_hit;
        int a_dam_roll;
        int a_prot_roll;
        int a_net_dam;
        bool is_player_attack;
        int line_y = hgt - 3 - j;
        int col = 0;

        if (!combat_history_locate_roll(start_index + j, &history_idx,
                &roll_idx))
        {
            continue;
        }

        round = &combat_history[history_idx];
        roll = &round->rolls[roll_idx];
        if (roll->att_type == COMBAT_ROLL_NONE)
            continue;

        is_player_attack = combat_roll_is_player_attack(roll);

        if (is_player_attack)
        {
            a_att = TERM_L_BLUE;
            a_evn = TERM_WHITE;
            a_hit = TERM_L_RED;
            a_dam_roll = TERM_L_BLUE;
            a_net_dam = TERM_L_RED;
            if (roll->prt_percent >= 100)
                a_prot_roll = TERM_WHITE;
            else if (roll->prt_percent >= 1)
                a_prot_roll = TERM_SLATE;
            else
                a_prot_roll = TERM_DARK;
        }
        else
        {
            a_att = TERM_WHITE;
            a_evn = TERM_L_BLUE;
            a_hit = TERM_L_RED;
            a_dam_roll = TERM_WHITE;
            a_net_dam = TERM_L_RED;
            if (roll->prt_percent >= 100)
                a_prot_roll = TERM_L_BLUE;
            else if (roll->prt_percent >= 1)
                a_prot_roll = TERM_BLUE;
            else
                a_prot_roll = TERM_DARK;
        }

        if (roll_idx == 0)
        {
            strnfmt(buf, sizeof(buf), "Turn %d:", round->turn_count);
            if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                    offset, wid, col, TERM_L_DARK, buf))
            {
                return false;
            }
            col += 9;
        }
        else
        {
            col += 9;
        }

        col += 1;
        if (!combat_history_scene_add_glyph(scene, panel, line_y, offset, wid,
                col, roll->attacker_attr, roll->attacker_char))
        {
            return false;
        }
        col += combat_history_scene_glyph_display_width();

        if (roll->att_type == COMBAT_ROLL_ROLL)
        {
            int net_att;

            if (roll->att < 10)
                strnfmt(buf, sizeof(buf), "  (%+d)", roll->att);
            else
                strnfmt(buf, sizeof(buf), " (%+d)", roll->att);
            if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                    offset, wid, col, a_att, buf))
            {
                return false;
            }
            col += strlen(buf);

            strnfmt(buf, sizeof(buf), "%4d", roll->att + roll->att_roll);
            if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                    offset, wid, col, a_att, buf))
            {
                return false;
            }
            col += 4;

            net_att = roll->att_roll + roll->att - roll->evn_roll - roll->evn;
            if (net_att > 0)
                strnfmt(buf, sizeof(buf), "%4d", net_att);
            else
                SDL_strlcpy(buf, "   -", sizeof(buf));
            if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                    offset, wid, col,
                    (net_att > 0) ? a_hit : TERM_SLATE, buf))
            {
                return false;
            }
            col += 4;

            strnfmt(buf, sizeof(buf), "%4d", roll->evn + roll->evn_roll);
            if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                    offset, wid, col, a_evn, buf))
            {
                return false;
            }
            col += 4;

            if (roll->evn < 10)
                strnfmt(buf, sizeof(buf), "   [%+d]", roll->evn);
            else
                strnfmt(buf, sizeof(buf), "  [%+d]", roll->evn);
            if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                    offset, wid, col, a_evn, buf))
            {
                return false;
            }
            col += strlen(buf);

            col += 1;
            if (!combat_history_scene_add_glyph(scene, panel, line_y, offset,
                    wid, col, roll->defender_attr, roll->defender_char))
            {
                return false;
            }
            col += combat_history_scene_glyph_display_width();

            if (net_att > 0)
            {
                int net_dam;

                if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                        offset, wid, col, TERM_L_DARK, "  ->"))
                {
                    return false;
                }
                col += 4;

                if (roll->ds < 10)
                    strnfmt(buf, sizeof(buf), "   (%dd%d)", roll->dd,
                        roll->ds);
                else
                    strnfmt(buf, sizeof(buf), "  (%dd%d)", roll->dd,
                        roll->ds);
                if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                        offset, wid, col, a_dam_roll, buf))
                {
                    return false;
                }
                col += strlen(buf);

                strnfmt(buf, sizeof(buf), "%4d", roll->dam);
                if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                        offset, wid, col, a_dam_roll, buf))
                {
                    return false;
                }
                col += 4;

                net_dam = roll->dam - roll->prot;
                if (net_dam < 0)
                    net_dam = 0;

                if (net_dam > 0)
                    strnfmt(buf, sizeof(buf), "%4d", net_dam);
                else
                    SDL_strlcpy(buf, "   -", sizeof(buf));
                if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                        offset, wid, col,
                        (net_dam > 0) ? a_net_dam : TERM_SLATE, buf))
                {
                    return false;
                }
                col += 4;

                strnfmt(buf, sizeof(buf), "%4d", roll->prot);
                if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                        offset, wid, col, a_prot_roll, buf))
                {
                    return false;
                }
                col += 4;
            }
        }
        else if (roll->att_type == COMBAT_ROLL_AUTO)
        {
            int net_dam;

            col += 25;
            col += 1;
            if (!combat_history_scene_add_glyph(scene, panel, line_y, offset,
                    wid, col, roll->defender_attr, roll->defender_char))
            {
                return false;
            }
            col += combat_history_scene_glyph_display_width();

            if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                    offset, wid, col, TERM_L_DARK, "  ->"))
            {
                return false;
            }
            col += 4;

            if (roll->ds < 10)
                strnfmt(buf, sizeof(buf), "   (%dd%d)", roll->dd, roll->ds);
            else
                strnfmt(buf, sizeof(buf), "  (%dd%d)", roll->dd, roll->ds);
            if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                    offset, wid, col, a_dam_roll, buf))
            {
                return false;
            }
            col += strlen(buf);

            strnfmt(buf, sizeof(buf), "%4d", roll->dam);
            if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                    offset, wid, col, a_dam_roll, buf))
            {
                return false;
            }
            col += 4;

            net_dam = roll->dam - roll->prot;
            if (net_dam < 0)
                net_dam = 0;

            if (net_dam > 0)
                strnfmt(buf, sizeof(buf), "%4d", net_dam);
            else
                SDL_strlcpy(buf, "   -", sizeof(buf));
            if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                    offset, wid, col,
                    (net_dam > 0) ? a_net_dam : TERM_SLATE, buf))
            {
                return false;
            }
            col += 4;

            strnfmt(buf, sizeof(buf), "%4d", roll->prot);
            if (!combat_history_scene_add_clipped_text(scene, panel, line_y,
                    offset, wid, col, a_prot_roll, buf))
            {
                return false;
            }
            col += 4;
        }
    }

    if (out_shown)
        *out_shown = j;

    strnfmt(buf, sizeof(buf), "Combat History (%d-%d of %d rolls), Offset %d",
        start_index, start_index + j - 1, total_rolls, offset);
    if (!combat_history_scene_add_clipped_text(scene, panel, 0, 0, wid, 0,
            TERM_WHITE, buf))
    {
        return false;
    }

    if (!combat_history_scene_add_clipped_text(scene, panel, hgt - 1, 0, wid,
            0,
            TERM_WHITE,
            "[Press 'p' for older, 'n' for newer, '=' to highlight, '/' to search, or ESCAPE]"))
    {
        return false;
    }

    return true;
}

static bool combat_history_present_ui_scene(int start_index, int offset, int wid,
    int hgt, int total_rolls, cptr shower)
{
    app_ui_scene scene;

    if (!combat_history_build_ui_scene(&scene, start_index, offset, wid, hgt,
            total_rolls, shower, NULL))
    {
        return false;
    }

    return ui_information_scene_present_ui(&scene);
}

static void combat_round_details_format_line(char* buf, size_t buf_size,
    const combat_roll* roll)
{
    int net_att;

    if (!buf || buf_size == 0)
        return;

    buf[0] = '\0';
    if (!roll)
        return;

    strnfmt(buf, buf_size, " %c", roll->attacker_char ? roll->attacker_char : '?');

    if (roll->att_type == COMBAT_ROLL_ROLL)
    {
        net_att = roll->att_roll + roll->att - roll->evn_roll - roll->evn;
        strnfmt(buf + strlen(buf), buf_size - strlen(buf),
            " (%+d)%4d %4d %4d [%+d] %c", roll->att,
            roll->att + roll->att_roll, (net_att > 0) ? net_att : 0,
            roll->evn + roll->evn_roll, roll->evn,
            roll->defender_char ? roll->defender_char : '?');

        if (net_att > 0)
        {
            int net_dam = roll->dam - roll->prot;

            if (net_dam < 0)
                net_dam = 0;
            strnfmt(buf + strlen(buf), buf_size - strlen(buf),
                " -> (%dd%d) %4d %4d %4d", roll->dd, roll->ds, roll->dam,
                net_dam, roll->prot);
        }
    }
    else if (roll->att_type == COMBAT_ROLL_AUTO)
    {
        int net_dam = roll->dam - roll->prot;

        if (net_dam < 0)
            net_dam = 0;

        strnfmt(buf + strlen(buf), buf_size - strlen(buf),
            "                         %c -> (%dd%d) %4d %4d %4d",
            roll->defender_char ? roll->defender_char : '?', roll->dd,
            roll->ds, roll->dam, net_dam, roll->prot);
    }
}

static bool combat_round_details_build_ui_scene(app_ui_scene* scene,
    const combat_history_round* round, int wid, int hgt)
{
    app_ui_panel* panel;
    char buf[120];
    int row = 2;
    int footer_row = (hgt > 0) ? (hgt - 1) : 0;
    bool wrote_any = false;

    if (!scene || !round)
        return false;

    panel = combat_history_begin_document_scene(scene);
    if (!panel)
        return false;

    strnfmt(buf, sizeof(buf), "Combat Details - Turn %d (%d roll%s)",
        round->turn_count, round->num_rolls, (round->num_rolls == 1) ? "" : "s");
    if (!combat_history_scene_add_clipped_text(scene, panel, 0, 0, wid, 0,
            TERM_WHITE, buf))
    {
        return false;
    }

    for (int pass = 0; pass < 2; pass++)
    {
        for (int i = 0; i < round->num_rolls; i++)
        {
            const combat_roll* roll = &round->rolls[i];
            bool player_attack;

            if (roll->att_type == COMBAT_ROLL_NONE)
                continue;

            player_attack = combat_roll_is_player_attack(roll);
            if (player_attack != (pass == 0))
                continue;

            if (row >= footer_row)
            {
                if (!combat_history_scene_add_clipped_text(scene, panel,
                        footer_row - 1, 0, wid, 0, TERM_SLATE,
                        "... more rolls omitted ..."))
                {
                    return false;
                }
                goto footer;
            }

            combat_round_details_format_line(buf, sizeof(buf), roll);
            if (!combat_history_scene_add_clipped_text(scene, panel, row, 0,
                    wid, 0, player_attack ? TERM_L_BLUE : TERM_WHITE, buf))
            {
                return false;
            }

            row++;
            wrote_any = true;
        }
    }

    if (!wrote_any)
    {
        if (!combat_history_scene_add_clipped_text(scene, panel, row, 0, wid,
                0, TERM_SLATE, "No combat rolls recorded for this round."))
        {
            return false;
        }
    }

footer:
    return combat_history_scene_add_clipped_text(scene, panel, footer_row, 0,
        wid, 0, TERM_WHITE, "[Press any key to return]");
}

static bool do_cmd_combat_history_information_scene(void)
{
    ui_information_scene_scope scope;
    char shower[80];
    char finder[80];
    int i = 0;
    int q = 0;
    int n;

    if (!ui_information_scene_enter(&scope))
        return false;

    SDL_strlcpy(finder, "", sizeof(finder));
    SDL_strlcpy(shower, "", sizeof(shower));
    n = combat_history_total_rolls();

    while (true)
    {
        int wid;
        int hgt;
        int old_i;
        char ch;

        Term_get_size(&wid, &hgt);
        if (!combat_history_present_ui_scene(i, q, wid, hgt, n, shower))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = (char)ui_information_scene_wait_key();
        if (ch == ESCAPE)
            break;

        old_i = i;

        if (ch == '4')
        {
            q = (q >= wid / 2) ? (q - wid / 2) : 0;
            continue;
        }
        if (ch == '6')
        {
            q += wid / 2;
            continue;
        }
        if (ch == '=')
        {
            if (!combat_history_information_scene_pause(&scope))
                break;

            if (!term_get_string("Show: ", shower, sizeof(shower)))
            {
                if (!combat_history_information_scene_resume(&scope))
                    return false;
                continue;
            }

            if (!combat_history_information_scene_resume(&scope))
                return false;
            continue;
        }
        if (ch == '/')
        {
            s16b z;

            if (!combat_history_information_scene_pause(&scope))
                break;

            if (term_get_string("Find: ", finder, sizeof(finder)))
            {
                SDL_strlcpy(shower, finder, sizeof(shower));

                for (z = i + 1; z < n; z++)
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
                        i = z;
                        break;
                    }
                }
            }

            if (!combat_history_information_scene_resume(&scope))
                return false;
            continue;
        }

        if ((ch == 'p') || (ch == KTRL('P')) || (ch == ' '))
        {
            if (i + 20 < n)
                i += 20;
        }
        if (ch == '+')
        {
            if (i + 10 < n)
                i += 10;
        }
        if ((ch == '8') || (ch == '\n') || (ch == '\r'))
        {
            if (i + 1 < n)
                i += 1;
        }
        if ((ch == 'n') || (ch == KTRL('N')))
            i = (i >= 20) ? (i - 20) : 0;
        if (ch == '-')
            i = (i >= 10) ? (i - 10) : 0;
        if (ch == '2')
            i = (i >= 1) ? (i - 1) : 0;

        if (i == old_i)
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
 * Display detailed combat rolls for a specific round
 */
void display_combat_round_details(combat_history_round* round)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    int wid;
    int hgt;

    if (!round)
        return;

    if (!ui_information_scene_supported())
    {
        log_warn("combat detail: snapshot renderer required; legacy renderer removed");
        msg_print("Combat detail viewer requires the snapshot UI renderer.");
        return;
    }

    if (!ui_information_scene_enter(&scope))
    {
        log_warn("combat detail: unable to enter information scene");
        msg_print("Combat detail viewer unavailable.");
        return;
    }

    Term_get_size(&wid, &hgt);
    if (!combat_round_details_build_ui_scene(&scene, round, wid, hgt)
        || !ui_information_scene_present_ui(&scene))
    {
        ui_information_scene_leave(&scope);
        log_warn("combat detail: information-scene presentation failed on the snapshot renderer path");
        msg_print("Combat detail viewer unavailable.");
        return;
    }

    (void)ui_information_scene_wait_key_nonrepeat();
    ui_information_scene_leave(&scope);
}

/*
 * Display recent combat rolls in the main terminal's bottom rows
 */
void display_main_combat_rolls(void)
{

    int i;
    int num_lines = op_ptr->main_combat_rolls;

    if (original_main_combat_rolls == -1) {
        original_main_combat_rolls = num_lines;
        if (original_main_combat_rolls > 0) {
            op_ptr->main_combat_rolls = 0;
            num_lines = 0;
            main_combat_rolls_deferral_active = true;
            log_trace("display_main_combat_rolls: deferring initial lines (saved %d)", original_main_combat_rolls);
        }
    }

    if (!combat_main_view_uses_legacy_term_rendering())
        return;

    log_trace("display_main_combat_rolls: Starting - combat_number=%d, combat_number_old=%d, num_lines=%d",
        combat_number, combat_number_old, num_lines);

    const int col_offset = COL_MAP;

    for (i = 0; i < num_lines; i++)
    {
        Term_putstr(col_offset, Term->hgt - num_lines - 1 + i, 65, TERM_WHITE,
            "                                                                 ");
    }

    if (num_lines == 0)
        return;

    if (combat_number == 0 && combat_number_old == 0)
        return;

    int start_row = Term->hgt - num_lines - 1;

    combat_display_entry ordered[MAX_COMBAT_ROLLS * 2];
    int total_entries = collect_combat_display_entries(ordered, MAX_COMBAT_ROLLS * 2);
    int entries_to_show = MIN(num_lines, total_entries);

    for (int entry_idx = 0; entry_idx < entries_to_show; entry_idx++)
    {
        int round = ordered[entry_idx].round;
        int idx = ordered[entry_idx].index;
        int row = start_row + entry_idx;

        draw_combat_roll_line(row, col_offset, &combat_rolls[round][idx]);
    }
}





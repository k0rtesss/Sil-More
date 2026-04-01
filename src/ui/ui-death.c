/* File: ui/ui-death.c */

#include "angband.h"
#include "externs.h"

#include "blitz.h"
#include "log/log.h"
#include "player/killer.h"
#include "player/player-oaths.h"
#include "score/score_ui.h"
#include "ui/ui-character-screen.h"
#include "ui/ui-death.h"
#include "ui/ui-information-scene.h"

void do_cmd_morgoth_victory(void)
{
    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[160];

    p_ptr->morgoth_slain = true;
    flush();
    p_ptr->is_dead = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;
    p_ptr->escaped = false;

    (void)strftime(long_day, sizeof(long_day), "%d %B %Y", localtime(&ct));

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    strnfmt(buf, sizeof(buf),
        "On %s you broke the illusion binding Morgoth to his throne.",
        long_day);
    do_cmd_note(buf, p_ptr->depth);

    do_cmd_note(
        "The Valar hail your impossible triumph and pour out their blessing.",
        p_ptr->depth);

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    SDL_strlcpy(p_ptr->died_from, "Morgoth's illusory defeat",
        sizeof(p_ptr->died_from));

    killer_mark_other(SCORE_KILLER_OTHER);
    killer_commit(p_ptr->died_from);

    if (run_mode_is_blitz())
        blitz_show_end_summary(3);
}

void ui_death_print_tomb(struct high_score* the_score)
{
    if (p_ptr->escaped)
    {
        if (p_ptr->oath_type > 0 && !oath_invalid(p_ptr->oath_type))
            Term_putstr(
                15, 2, -1, TERM_L_BLUE, "You have escaped and kept your oath");
        else
            Term_putstr(15, 2, -1, TERM_L_BLUE, "You have escaped");
    }
    else if (p_ptr->morgoth_slain)
    {
        Term_putstr(15, 2, -1, TERM_YELLOW,
            "You are acclaimed as the Slayer of Morgoth");
    }
    else
    {
        Term_putstr(15, 2, -1, TERM_L_BLUE, "You have been slain");
    }

    display_single_score(TERM_WHITE, 1, 0, 0, false, the_score);
}

void ui_death_show_character_info(void)
{
    int term_wid = 80;
    int term_hgt = 24;
    ui_information_scene_scope scope;
    bool scene_active = ui_information_scene_enter(&scope);

    Term_get_size(&term_wid, &term_hgt);
    display_player(0);

    Term_putstr(MAX(0, term_wid - 18), term_hgt - 2, -1, TERM_L_WHITE,
        "(press any key)");

    if (scene_active)
    {
        if (!ui_information_scene_present_term())
        {
            ui_information_scene_leave(&scope);
            scene_active = false;
        }
    }
    else
    {
        Term_fresh();
    }

    if ((scene_active ? ui_information_scene_wait_key_nonrepeat() : inkey()) == ESCAPE)
    {
        if (scene_active)
            ui_information_scene_leave(&scope);
        return;
    }

    if (p_ptr->equip_cnt)
    {
        Term_clear();
        item_tester_full = true;
        show_equip();
        prt("You are using:", 0, 0);
        Term_putstr(MAX(0, term_wid - 18), term_hgt - 2, -1, TERM_L_WHITE,
            "(press any key)");
        if (scene_active)
        {
            if (!ui_information_scene_present_term())
            {
                ui_information_scene_leave(&scope);
                scene_active = false;
            }
        }
        else
        {
            Term_fresh();
        }
        if ((scene_active ? ui_information_scene_wait_key_nonrepeat() : inkey())
            == ESCAPE)
        {
            if (scene_active)
                ui_information_scene_leave(&scope);
            return;
        }
        item_tester_full = false;
    }

    if (p_ptr->inven_cnt)
    {
        Term_clear();
        item_tester_full = true;
        show_inven();
        prt("You are carrying:", 0, 0);
        Term_putstr(MAX(0, term_wid - 18),
            MIN(p_ptr->inven_cnt + 2, term_hgt - 2), -1, TERM_L_WHITE,
            "(press any key)");
        if (scene_active)
        {
            if (!ui_information_scene_present_term())
            {
                ui_information_scene_leave(&scope);
                scene_active = false;
            }
        }
        else
        {
            Term_fresh();
        }
        if ((scene_active ? ui_information_scene_wait_key_nonrepeat() : inkey())
            == ESCAPE)
        {
            if (scene_active)
                ui_information_scene_leave(&scope);
            return;
        }
        item_tester_full = false;
    }

    if (scene_active)
        ui_information_scene_leave(&scope);
    do_cmd_knowledge_notes();
}

int ui_death_final_menu(int* highlight)
{
    char ch;
    bool morgoth_victory = (p_ptr->morgoth_slain && !p_ptr->escaped);
    int term_wid = 80;
    int term_hgt = 24;
    int separator_row;
    int option_row;
    char separator[96];
    ui_information_scene_scope scope;
    bool scene_active = ui_information_scene_enter(&scope);
    const char* option_a = morgoth_victory ? "a) Review the Valar's record"
                                           : "a) View scores";
    const char* option_b = morgoth_victory ? "b) Survey Angband one last time"
                                           : "b) Final look";
    const char* option_c = morgoth_victory ? "c) Rehear the proclamations"
                                           : "c) View final messages";
    const char* option_d = morgoth_victory ? "d) Review your legend"
                                           : "d) View character sheet";
    const char* option_e = morgoth_victory ? "e) Append to the annals"
                                           : "e) Add comment to notes";
    const char* option_f = morgoth_victory ? "f) Archive your legend"
                                           : "f) Save character sheet";
    const char* option_exit = "g) Exit";

    Term_get_size(&term_wid, &term_hgt);
    separator_row = (term_hgt < 20) ? 9 : 10;
    option_row = separator_row + 2;
    memset(separator, '_', sizeof(separator) - 1);
    separator[MIN((int)sizeof(separator) - 1, MAX(1, term_wid - 6))] = '\0';

    Term_putstr(3, separator_row, term_wid - 6, TERM_L_DARK, separator);
    Term_putstr(15, option_row++, term_wid - 15,
        (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE, option_a);
    Term_putstr(15, option_row++, term_wid - 15,
        (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE, option_b);
    Term_putstr(15, option_row++, term_wid - 15,
        (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE, option_c);
    Term_putstr(15, option_row++, term_wid - 15,
        (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE, option_d);
    Term_putstr(15, option_row++, term_wid - 15,
        (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE, option_e);
    Term_putstr(15, option_row++, term_wid - 15,
        (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE, option_f);
    Term_putstr(15, option_row, term_wid - 15,
        (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE, option_exit);

    if (scene_active)
    {
        if (!ui_information_scene_present_term())
        {
            ui_information_scene_leave(&scope);
            scene_active = false;
            Term_fresh();
        }
    }
    else
    {
        Term_fresh();
    }
    Term_gotoxy(10, separator_row + 1 + *highlight);

    inkey_set_cursor_hidden(true);
    if (scene_active)
        ch = (char)ui_information_scene_wait_key();
    else
        ch = inkey();
    inkey_set_cursor_hidden(false);

    if (scene_active)
        ui_information_scene_leave(&scope);

    if (ch == 'a')
    {
        *highlight = 1;
        return 1;
    }
    if (ch == 'b')
    {
        *highlight = 2;
        return 2;
    }
    if (ch == 'c')
    {
        *highlight = 3;
        return 3;
    }
    if (ch == 'd')
    {
        *highlight = 4;
        return 4;
    }
    if (ch == 'e')
    {
        *highlight = 5;
        return 5;
    }
    if (ch == 'f')
    {
        *highlight = 6;
        return 6;
    }
    if ((ch == 'g') || (ch == 'q') || (ch == 'Q'))
    {
        *highlight = 7;
        return 7;
    }
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
        return *highlight;
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = 7;
    }
    if (ch == '2')
    {
        if (*highlight < 7)
            (*highlight)++;
        else if (*highlight == 7)
            *highlight = 1;
    }

    return 0;
}

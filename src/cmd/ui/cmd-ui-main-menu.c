/* File: cmd-ui-main-menu.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "platform-ui.h"
#include "object/object-ui-select.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-oaths.h"
#include "sound-config.h"
#include "platform-audio.h"

extern struct sound_config g_sound_config;
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "runtime/runtime-game.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "cmd-ui.h"

#define MAIN_MENU_RETURN 1
#define MAIN_MENU_CHARACTER 2
#define MAIN_MENU_KNOWLEDGE 3
#define MAIN_MENU_QUEST_STATUS 4
#define MAIN_MENU_SCORES 5
#define MAIN_MENU_NOTE 6
#define MAIN_MENU_MAP 7
#define MAIN_MENU_MESSAGES 8
#define MAIN_MENU_SCREENSHOT 9
#define MAIN_MENU_STORY 10
#define MAIN_MENU_OPTIONS 11
#define MAIN_MENU_HELP 12
#define MAIN_MENU_ABORT 13
#define MAIN_MENU_SAVE 14
#define MAIN_MENU_SAVE_QUIT 15

#define MAIN_MENU_MAX 16

static int main_menu_calc_width(void)
{
    /* Keep in sync with the strings printed in main_menu_aux(). */
    static const char* lines[] = {
        "Character sheet      (c)",
        "Known lore           (a)",
        "Quest status         (t)",
        "Halls of Mandos      (d)",
        "Run history          (v)",
        "Map                  (m)",
        "Log                  (l)",
        "Combat history       (x)",
        "Hint messages        (i)",
        "The story so far     (y)",
        "Options and misc     (o)",
        "Help                 (h)",
        "Suicide              (k)",
        "Save                 (s)",
        "Quit with save       (q)",
        "Return to game       (r)",
    };

    int max_w = 0;
    for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++)
    {
        int w = (int)strlen(lines[i]);
        if (w > max_w)
            max_w = w;
    }
    return max_w;
}

static void do_cmd_hint_messages(bool* out_pending_look, int* out_look_y,
    int* out_look_x);

/*
 * Performs the interface and selection work for the main menu.
 */
int main_menu_aux(int* highlight)
{
    char ch;
    int i;
    bool death_view = death_spectator_active();

    int menu_w = main_menu_calc_width();
    const int top_pad = 1;
    const int bottom_pad = (Term && (Term->hgt <= 18)) ? 0 : 1;
    const int row_first = top_pad;
    int menu_h = MAIN_MENU_MAX + top_pad + bottom_pad;
    int col_main = 0;
    int row_top = 0;
    if (Term)
    {
        col_main = (Term->wid - menu_w) / 2;
        if (col_main < 0)
            col_main = 0;

        /* Keep the menu fixed vertically.
         * At height 20, start at row 0 so all menu rows fit.
         * Otherwise keep row 0 for message bar and start menu at row 1. */
        if (Term->hgt <= 18)
            row_top = 0;
        else
            row_top = (Term->hgt > 1) ? 1 : 0;
    }

    if (death_view && (*highlight >= 13) && (*highlight <= 15))
        *highlight = 16;

    for (i = 0; i < menu_h; i++)
    {
        int y = row_top + i;
        if (!Term || y < 0 || y >= Term->hgt)
            continue;

        int clear_x = col_main - 2;
        if (clear_x < 0)
            clear_x = 0;
        int clear_w = menu_w + 4;
        if (clear_x + clear_w > Term->wid)
            clear_w = Term->wid - clear_x;
        if (clear_w > 0)
            Term_erase(clear_x, y, clear_w);
    }

    Term_putstr(col_main, row_top + row_first + 0, -1,
        (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE,
        "Character sheet      (c)");
    Term_putstr(col_main, row_top + row_first + 1, -1,
        (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE,
        "Known lore           (a)");
    Term_putstr(col_main, row_top + row_first + 2, -1,
        (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE,
        "Quest status         (t)");
    Term_putstr(col_main, row_top + row_first + 3, -1,
        (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE,
        "Halls of Mandos      (d)");
    Term_putstr(col_main, row_top + row_first + 4, -1,
        (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE,
        "Run history          (v)");
    Term_putstr(col_main, row_top + row_first + 5, -1,
        (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE,
        "Map                  (m)");
    Term_putstr(col_main, row_top + row_first + 6, -1,
        (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE,
        "Log                  (l)");
    Term_putstr(col_main, row_top + row_first + 7, -1,
        (*highlight == 8) ? TERM_L_BLUE : TERM_WHITE,
        "Combat history       (x)");
    Term_putstr(col_main, row_top + row_first + 8, -1,
        (*highlight == 9) ? TERM_L_BLUE : TERM_WHITE,
        "Hint messages        (i)");
    Term_putstr(col_main, row_top + row_first + 9, -1,
        (*highlight == 10) ? TERM_L_BLUE : TERM_WHITE,
        "The story so far     (y)");
    Term_putstr(col_main, row_top + row_first + 10, -1,
        (*highlight == 11) ? TERM_L_BLUE : TERM_WHITE,
        "Options and misc     (o)");
    Term_putstr(col_main, row_top + row_first + 11, -1,
        (*highlight == 12) ? TERM_L_BLUE : TERM_WHITE,
        "Help                 (h)");
    byte suicide_color = death_view ? TERM_L_DARK
        : ((*highlight == 13) ? TERM_L_BLUE : TERM_WHITE);
    Term_putstr(col_main, row_top + row_first + 12, -1, suicide_color,
        "Suicide              (k)");
    byte save_color = death_view ? TERM_L_DARK
        : ((*highlight == 14) ? TERM_L_BLUE : TERM_WHITE);
    Term_putstr(col_main, row_top + row_first + 13, -1, save_color,
        "Save                 (s)");
    byte quit_color = death_view ? TERM_L_DARK
        : ((*highlight == 15) ? TERM_L_BLUE : TERM_WHITE);
    Term_putstr(col_main, row_top + row_first + 14, -1, quit_color,
        "Quit with save       (q)");
    Term_putstr(col_main, row_top + row_first + 15, -1,
        (*highlight == 16) ? TERM_L_BLUE : TERM_WHITE,
        "Return to game       (r)");

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    {
        int cursor_y = row_top + row_first + (*highlight - 1);
        if (Term)
        {
            if (cursor_y < 0)
                cursor_y = 0;
            if (cursor_y >= Term->hgt)
                cursor_y = Term->hgt - 1;
        }
        Term_gotoxy(col_main, cursor_y);
    }

    /* Get key (while allowing menu commands) */
    inkey_set_cursor_hidden(true);
    ch = inkey();
    inkey_set_cursor_hidden(false);

    // choose an option by letter - alphabetical mapping (updated for new order)
    switch (ch)
    {
    case 'c':
        *highlight = 1;
        return (*highlight);  // Character sheet
    case 'a':
        *highlight = 2;
        return (*highlight);  // Known lore
    case 't':
        *highlight = 3;
        return (*highlight);  // Quest status
    case 'd':
        *highlight = 4;
        return (*highlight);  // Halls of Mandos
    case 'v':
        *highlight = 5;
        return (*highlight);  // Run history
    case 'm':
        *highlight = 6;
        return (*highlight);  // Map
    case 'l':
        *highlight = 7;
        return (*highlight);  // Log
    case 'x':
        *highlight = 8;
        return (*highlight); // Combat history
    case 'i':
        *highlight = 9;
        return (*highlight); // Hint messages
    case 'y':
        *highlight = 10;
        return (*highlight); // The story so far
    case 'o':
        *highlight = 11;
        return (*highlight); // Options and misc
    case 'h':
        *highlight = 12;
        return (*highlight); // Help
    case 'k':
        if (death_view) {
            msg_print("You can no longer take that action.");
            break;
        }
        *highlight = 13;
        return (*highlight); // Suicide
    case 's':
        if (death_view) {
            msg_print("You can no longer take that action.");
            break;
        }
        *highlight = 14;
        return (*highlight); // Save
    case 'q':
        if (death_view) {
            msg_print("You can no longer take that action.");
            break;
        }
        *highlight = 15;
        return (*highlight); // Quit with save
    case 'r':
        *highlight = 16;
        return (*highlight); // Return to game
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = MAIN_MENU_MAX;
        while (death_view && (*highlight >= 13) && (*highlight <= 15))
        {
            if (*highlight > 1)
                (*highlight)--;
            else
                *highlight = MAIN_MENU_MAX;
        }
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < MAIN_MENU_MAX)
            (*highlight)++;
        else if (*highlight == MAIN_MENU_MAX)
            *highlight = 1;
        while (death_view && (*highlight >= 13) && (*highlight <= 15))
        {
            if (*highlight < MAIN_MENU_MAX)
                (*highlight)++;
            else
                *highlight = 1;
        }
    }

    /* Leave menu */
    if ((ch == ESCAPE) || (ch == '4'))
    {
        return (-1);
    }

    return (0);
}

/*
 * Brings up a menu for choosing some of the game's more abstruse options.
 */
void do_cmd_main_menu(void)
{
    int actiontype = -1;
    int highlight = 1;
    bool leave_menu = false;
    bool pending_hint_look = false;
    int pending_hint_look_y = -1;
    int pending_hint_look_x = -1;

    /* Clear any active banner before opening main menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        actiontype = main_menu_aux(&highlight);

        if (death_spectator_active() && (actiontype >= 13) && (actiontype <= 15))
        {
            msg_print("You can no longer take that action.");
            continue;
        }

        // if an action has been selected...
        switch (actiontype)
        {
        case 1: // Character sheet (c)
        {
            do_cmd_character_sheet();
            leave_menu = true;
            break;
        }
        case 2: // Known lore (a)
        {
            do_cmd_knowledge_browser_page(cmd_ui_knowledge_last_page());
            leave_menu = true;
            break;
        }
        case 3: // Quest status (t)
        {
            do_cmd_quest_status();
            leave_menu = true;
            break;
        }
        case 4: // Halls of Mandos (d)
        {
            log_info("main menu: opening Halls of Mandos view");
            show_scores_interactive(true);
            leave_menu = true;
            break;
        }
        case 5: // Run history (v)
        {
            do_cmd_run_history();
            leave_menu = true;
            break;
        }
        case 6: // Map (m)
        {
            do_cmd_view_map();
            leave_menu = true;
            break;
        }
        case 7: // Log (l)
        {
            do_cmd_messages();
            leave_menu = true;
            break;
        }
        case 8: // Combat history (x)
        {
            do_cmd_combat_history();
            leave_menu = true;
            break;
        }
        case 9: // Hint messages (i)
        {
            do_cmd_hint_messages(&pending_hint_look, &pending_hint_look_y,
                &pending_hint_look_x);
            leave_menu = true;
            break;
        }
        case 10: // The story so far (y)
        {
            /* Save screen before showing story */
            screen_save();
            print_story(15, 1);
            /* Load screen after story */
            screen_load();
            leave_menu = true;
            break;
        }
        case 11: // Options and misc (o)
        {
            do_cmd_options();
            leave_menu = true;
            break;
        }
        case 12: // Help (h)
        {
            do_cmd_help();
            leave_menu = true;
            break;
        }
        case 13: // Suicide (k)
        {
            do_cmd_suicide();
            leave_menu = true;
            break;
        }
        case 14: // Save (s)
        {
            do_cmd_save_game();
            leave_menu = true;
            break;
        }
        case 15: // Quit with save (q)
        {
            do_cmd_save_game();

            /* Stop playing */
            p_ptr->playing = false;

            /* Mark that we want to quit to menu, not exit application */
            p_ptr->quit_to_menu = true;

            /* Leaving */
            p_ptr->leaving = true;
            leave_menu = true;
            break;
        }
        case 16: // Return to game (r)
        {
            leave_menu = true;
            break;
        }
        case -1:
        {
            leave_menu = true;
            break;
        }
        default:
        {
            /* Invalid selection - stay in menu */
            break;
        }
        }
    }

    /* Load screen */
    screen_load();

    if (pending_hint_look)
    {
        do_cmd_redraw();
        do_cmd_look_at(pending_hint_look_y, pending_hint_look_x);
    }

}

/*
 * Recall the most recent message
 */
void do_cmd_message_one(void)
{
    /* Recall one message XXX XXX XXX */
    c_prt(message_color(0), format("> %s", message_str(0)), 0, 0);
}

static bool hint_message_has_source(const hint_message_meta* meta)
{
    return meta && meta->source_y >= 0 && meta->source_x >= 0
        && meta->source_y < p_ptr->cur_map_hgt && meta->source_x < p_ptr->cur_map_wid;
}

static bool hint_message_is_word_boundary(char ch)
{
    return (ch == '\0') || !isalnum((unsigned char)ch);
}

static bool hint_message_phrase_matches(const char* line, int offset, const char* phrase)
{
    size_t len;

    if (!line || !phrase || !phrase[0])
        return false;

    len = strlen(phrase);
    if (strncmp(line + offset, phrase, len) != 0)
        return false;

    if (offset > 0 && !hint_message_is_word_boundary(line[offset - 1]))
        return false;

    return hint_message_is_word_boundary(line[offset + len]);
}

static int hint_message_match_length(const char* line, int offset,
    const hint_message_meta* meta, byte* out_attr)
{
    int best_len = 0;
    byte best_attr = TERM_WHITE;

    if (!meta)
        return 0;

    for (int cue = 0; cue < meta->cue_count; ++cue)
    {
        const char* dist = meta->cue_dists[cue];
        const char* dir = meta->cue_dirs[cue];

        if (hint_message_phrase_matches(line, offset, dist))
        {
            int len = (int)strlen(dist);
            if (len > best_len)
            {
                best_len = len;
                best_attr = TERM_YELLOW;
            }
        }

        if (hint_message_phrase_matches(line, offset, dir))
        {
            int len = (int)strlen(dir);
            if (len > best_len)
            {
                best_len = len;
                best_attr = TERM_L_BLUE;
            }
        }
    }

    if (out_attr)
        *out_attr = best_attr;

    return best_len;
}

static void hint_message_put_segment(int row, int col, byte attr, const char* text)
{
    if (!text || !text[0])
        return;

    if (sdl_is_story_font_enabled())
        story_print_text(row, col, 0, attr, text);
    else
        Term_putstr(col, row, -1, attr, text);
}

static void hint_message_draw_colored_line(int row, int col, byte base_attr,
    const char* line, const hint_message_meta* meta)
{
    int start = 0;
    int cursor = col;
    int len;

    if (!line)
        line = "";

    len = (int)strlen(line);
    for (int i = 0; i < len; )
    {
        byte match_attr = base_attr;
        int match_len = hint_message_match_length(line, i, meta, &match_attr);
        if (match_len > 0)
        {
            if (i > start)
            {
                char plain[100];
                int plain_len = i - start;
                memcpy(plain, line + start, plain_len);
                plain[plain_len] = '\0';
                hint_message_put_segment(row, cursor, base_attr, plain);
                cursor += plain_len;
            }

            {
                char special[HINT_MESSAGE_CUE_TEXT_MAX + 1];
                memcpy(special, line + i, match_len);
                special[match_len] = '\0';
                hint_message_put_segment(row, cursor, match_attr, special);
            }

            cursor += match_len;
            i += match_len;
            start = i;
        }
        else
        {
            ++i;
        }
    }

    if (start < len)
    {
        char tail[100];
        int tail_len = len - start;
        memcpy(tail, line + start, tail_len);
        tail[tail_len] = '\0';
        hint_message_put_segment(row, cursor, base_attr, tail);
    }
}

static const char* hint_message_title(int index)
{
    byte line_count = hint_messages_message_line_count(index);
    for (int li = 0; li < line_count; ++li)
    {
        const char* line = hint_messages_message_line(index, li);
        if (line && line[0])
            return line;
    }

    return "";
}

static void hint_message_build_title(char* buf, size_t buf_sz, const char* title,
    int max_len)
{
    if (!buf || buf_sz == 0)
        return;

    if (!title)
        title = "";

    if (max_len < 4 || (int)strlen(title) <= max_len)
    {
        strnfmt(buf, buf_sz, "%s", title);
        return;
    }

    strnfmt(buf, buf_sz, "%.*s...", max_len - 3, title);
}

static void hint_message_draw_list_row(int row, int idx, bool selected, int wid)
{
    hint_message_meta meta;
    char prefix[8];
    char title_buf[96];
    const char* title = hint_message_title(idx);
    byte prefix_attr = selected ? TERM_L_BLUE : TERM_WHITE;
    byte title_attr = selected ? TERM_L_WHITE : TERM_WHITE;
    byte chrome_attr = TERM_SLATE;
    int col = 0;
    int title_room;

    hint_messages_message_meta(idx, &meta);

    Term_erase(0, row, 255);

    strnfmt(prefix, sizeof(prefix), "%2d) ", idx + 1);
    Term_putstr(col, row, -1, prefix_attr, prefix);
    col += (int)strlen(prefix);

    title_room = MAX(8, wid - col - 1);
    if (meta.cue_count > 0)
        title_room = MIN(title_room, MAX(wid / 2, 24));
    hint_message_build_title(title_buf, sizeof(title_buf), title, title_room);
    Term_putstr(col, row, -1, title_attr, title_buf);
    col += (int)strlen(title_buf);

    if (meta.cue_count <= 0 || col >= wid - 4)
        return;

    Term_putstr(col, row, -1, chrome_attr, " [");
    col += 2;

    for (int cue = 0; cue < meta.cue_count && col < wid - 1; ++cue)
    {
        if (cue > 0)
        {
            Term_putstr(col, row, -1, chrome_attr, "; ");
            col += 2;
        }

        if (meta.cue_dists[cue][0])
        {
            Term_putstr(col, row, -1, TERM_YELLOW, meta.cue_dists[cue]);
            col += (int)strlen(meta.cue_dists[cue]);
        }

        if (meta.cue_dists[cue][0] && meta.cue_dirs[cue][0] && col < wid - 1)
        {
            Term_putstr(col, row, -1, chrome_attr, " ");
            col += 1;
        }

        if (meta.cue_dirs[cue][0] && col < wid - 1)
        {
            Term_putstr(col, row, -1, TERM_L_BLUE, meta.cue_dirs[cue]);
            col += (int)strlen(meta.cue_dirs[cue]);
        }
    }

    if (col < wid - 1)
        Term_putstr(col, row, -1, chrome_attr, "]");
}

static bool hint_message_show_internal(int index, int* look_y, int* look_x,
    bool manage_screen)
{
    int wid = 80;
    int hgt = 24;
    int row = 4;
    int col = 8;
    char ch;
    hint_message_meta meta;
    byte line_count;
    bool request_look = false;

    hint_messages_ensure_level_state();
    line_count = hint_messages_message_line_count(index);
    if (!line_count)
        return false;

    hint_messages_message_meta(index, &meta);

    if (manage_screen)
        screen_save();

    sdl_story_font_enable();

    while (1)
    {
        Term_clear();
        Term_get_size(&wid, &hgt);

        for (int li = 0; li < line_count && row + li < hgt - 1; ++li)
        {
            const char* line = hint_messages_message_line(index, li);
            byte base_attr = (li == 0) ? TERM_L_WHITE : TERM_WHITE;
            hint_message_draw_colored_line(row + li, col, base_attr, line,
                (li == 0) ? NULL : &meta);
        }

        if (hint_message_has_source(&meta))
        {
            prt("[Press any key to continue, or 'l' to look at the skeleton]",
                hgt - 1, 0);
        }
        else
        {
            prt("[Press any key to continue]", hgt - 1, 0);
        }

        Term_fresh();

        inkey_set_cursor_hidden(true);
        ch = inkey();
        inkey_set_cursor_hidden(false);

        if ((ch == 'l' || ch == 'L') && hint_message_has_source(&meta))
        {
            if (look_y)
                *look_y = meta.source_y;
            if (look_x)
                *look_x = meta.source_x;
            request_look = true;
            break;
        }

        break;
    }

    sdl_story_font_disable();
    if (manage_screen)
        screen_load();

    return request_look;
}

void show_hint_message_screen(int index)
{
    int look_y = -1;
    int look_x = -1;

    if (hint_message_show_internal(index, &look_y, &look_x, true))
    {
        do_cmd_redraw();
        do_cmd_look_at(look_y, look_x);
    }
}

static void do_cmd_hint_messages(bool* out_pending_look, int* out_look_y,
    int* out_look_x)
{
    char ch;

    int wid, hgt;
    bool pending_look = false;
    int look_y = -1;
    int look_x = -1;

    /* Clear any active banner before opening hint messages */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    hint_messages_ensure_level_state();

    int n = (int)hint_messages_count_for_save();
    if (n <= 0)
    {
        msg_print("You recall no hint messages on this level.");
        return;
    }

    int sel = 0;
    int top = 0;

    Term_get_size(&wid, &hgt);

    /* Save screen */
    screen_save();

    while (1)
    {
        Term_clear();

        int rows = hgt - 4;
        if (rows < 1)
            rows = 1;

        if (sel < 0)
            sel = 0;
        if (sel >= n)
            sel = n - 1;

        if (sel < top)
            top = sel;
        if (sel >= top + rows)
            top = sel - rows + 1;
        if (top < 0)
            top = 0;
        if (top > n - rows)
            top = n - rows;
        if (top < 0)
            top = 0;

        prt(format("Hint Messages (%d)", n), 0, 0);
        prt("[Press '8'/'2' to move, Enter to read, 'l' to look, or ESCAPE]",
            hgt - 1, 0);

        for (int row = 0; row < rows && top + row < n; ++row)
        {
            int idx = top + row;
            hint_message_draw_list_row(2 + row, idx, idx == sel, wid);
        }

        Term_fresh();
        ch = inkey();

        if (ch == ESCAPE)
            break;

        if (ch == '8')
        {
            sel = (sel > 0) ? (sel - 1) : (n - 1);
            continue;
        }

        if (ch == '2')
        {
            sel = (sel + 1 < n) ? (sel + 1) : 0;
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
        {
            int selected_look_y = -1;
            int selected_look_x = -1;

            if (hint_message_show_internal(sel, &selected_look_y, &selected_look_x, false))
            {
                pending_look = true;
                look_y = selected_look_y;
                look_x = selected_look_x;
                break;
            }
            continue;
        }

        if (ch == 'l' || ch == 'L')
        {
            hint_message_meta meta;
            hint_messages_message_meta(sel, &meta);
            if (hint_message_has_source(&meta))
            {
                pending_look = true;
                look_y = meta.source_y;
                look_x = meta.source_x;
                break;
            }

            bell(NULL);
            continue;
        }

        bell(NULL);
    }

    /* Load screen */
    screen_load();

    if (out_pending_look)
        *out_pending_look = pending_look;
    if (out_look_y)
        *out_look_y = look_y;
    if (out_look_x)
        *out_look_x = look_x;
}

/*
 * Show previous messages to the user
 *
 * The screen format uses line 0 and 23 for headers and prompts,
 * skips line 1 and 22, and uses line 2 thru 21 for old messages.
 *
 * This command shows you which commands you are viewing, and allows
 * you to "search" for strings in the recall.
 *
 * Note that messages may be longer than 80 characters, but they are
 * displayed using "infinite" length, with a special sub-command to
 * "slide" the virtual display to the left or right.
 *
 * Attempt to only hilite the matching portions of the string.
 */
void do_cmd_messages(void)
{
    char ch;

    int i, j, n, q;
    int wid, hgt;

    char shower[80];
    char finder[80];

    /* Clear any active banner before opening message history */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Wipe finder */
    SDL_strlcpy(finder, "", sizeof(finder));

    /* Wipe shower */
    SDL_strlcpy(shower, "", sizeof(shower));

    /* Total messages */
    n = message_num();

    /* Start on first message */
    i = 0;

    /* Start at leftmost edge */
    q = 0;

    /* Get size */
    Term_get_size(&wid, &hgt);

    /* Save screen */
    screen_save();

    /* Process requests until done */
    while (1)
    {
        /* Clear screen */
        Term_clear();

        /* Dump messages */
        for (j = 0; (j < hgt - 4) && (i + j < n); j++)
        {
            cptr msg = message_str((s16b)(i + j));
            byte attr = message_color((s16b)(i + j));

            /* Apply horizontal scroll */
            msg = ((int)strlen(msg) >= q) ? (msg + q) : "";

            /* Dump the messages, bottom to top */
            Term_putstr(0, hgt - 3 - j, -1, attr, msg);

            /* Hilite "shower" */
            if (shower[0])
            {
                cptr str = msg;

                /* Display matches */
                while ((str = strstr(str, shower)) != NULL)
                {
                    int len = strlen(shower);

                    /* Display the match */
                    Term_putstr(
                        str - msg, hgt - 3 - j, len, TERM_YELLOW, shower);

                    /* Advance */
                    str += len;
                }
            }
        }

        /* Display header XXX XXX XXX */
        prt(format(
                "Message Recall (%d-%d of %d), Offset %d", i, i + j - 1, n, q),
            0, 0);

        /* Display prompt (not very informative) */
        prt("[Press 'p' for older, 'n' for newer, ..., or ESCAPE]", hgt - 1, 0);

        /* Get a command */
        ch = inkey();

        /* Exit on Escape */
        if (ch == ESCAPE)
            break;

        /* Hack -- Save the old index */
        j = i;

        /* Horizontal scroll */
        if (ch == '4')
        {
            /* Scroll left */
            q = (q >= wid / 2) ? (q - wid / 2) : 0;

            /* Success */
            continue;
        }

        /* Horizontal scroll */
        if (ch == '6')
        {
            /* Scroll right */
            q = q + wid / 2;

            /* Success */
            continue;
        }

        /* Hack -- handle show */
        if (ch == '=')
        {
            /* Prompt */
            prt("Show: ", hgt - 1, 0);

            /* Get a "shower" string, or continue */
            if (!askfor_aux(shower, sizeof(shower)))
                continue;

            /* Okay */
            continue;
        }

        /* Hack -- handle find */
        if (ch == '/')
        {
            s16b z;

            /* Prompt */
            prt("Find: ", hgt - 1, 0);

            /* Get a "finder" string, or continue */
            if (!askfor_aux(finder, sizeof(finder)))
                continue;

            /* Show it */
            SDL_strlcpy(shower, finder, sizeof(shower));

            /* Scan messages */
            for (z = i + 1; z < n; z++)
            {
                cptr msg = message_str(z);

                /* Search for it */
                if (strstr(msg, finder))
                {
                    /* New location */
                    i = z;

                    /* Done */
                    break;
                }
            }
        }

        /* Recall 20 older messages */
        if ((ch == 'p') || (ch == KTRL('P')) || (ch == ' '))
        {
            /* Go older if legal */
            if (i + 20 < n)
                i += 20;
        }

        /* Recall 10 older messages */
        if (ch == '+')
        {
            /* Go older if legal */
            if (i + 10 < n)
                i += 10;
        }

        /* Recall 1 older message */
        if ((ch == '8') || (ch == '\n') || (ch == '\r'))
        {
            /* Go newer if legal */
            if (i + 1 < n)
                i += 1;
        }

        /* Recall 20 newer messages */
        if ((ch == 'n') || (ch == KTRL('N')))
        {
            /* Go newer (if able) */
            i = (i >= 20) ? (i - 20) : 0;
        }

        /* Recall 10 newer messages */
        if (ch == '-')
        {
            /* Go newer (if able) */
            i = (i >= 10) ? (i - 10) : 0;
        }

        /* Recall 1 newer messages */
        if (ch == '2')
        {
            /* Go newer (if able) */
            i = (i >= 1) ? (i - 1) : 0;
        }

        /* Hack -- Error of some kind */
        if (i == j)
            bell(NULL);
    }

    /* Load screen */
    screen_load();
}


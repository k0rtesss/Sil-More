/* File: cmd-ui-character.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "main-sdl.h"
#include "object/object-ui-select.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-oaths.h"
#include "sound-config.h"
#include "sdl-sound.h"
#include "ui/ui-character-screen.h"

extern struct sound_config g_sound_config;
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "cmd-ui.h"
/*
 * Hack -- character sheet
 */
static void character_sheet_put_prompt_fit(int col, int row, int wid, byte attr, cptr text)
{
    char buf[256];
    int max_len;

    if (!text)
        return;

    if (wid < 1)
        wid = 80;

    max_len = wid - col - 1;
    if (max_len < 1)
        return;

    SDL_strlcpy(buf, text, sizeof(buf));
    if ((int)strlen(buf) > max_len)
        buf[max_len] = '\0';

    Term_putstr(col, row, -1, attr, buf);
}

static bool character_sheet_prompt_append(char* buf, size_t buflen, cptr token, int max_width)
{
    size_t cur_len;
    size_t tok_len;
    int sep = 0;

    if (!buf || !buflen || !token || !token[0])
        return true;

    cur_len = strlen(buf);
    tok_len = strlen(token);
    if (cur_len > 0)
        sep = 2;

    if ((int)(cur_len + sep + tok_len) > max_width)
        return false;

    if (sep)
        SDL_strlcat(buf, "  ", buflen);
    SDL_strlcat(buf, token, buflen);
    return true;
}

static void character_sheet_build_prompt(bool steamdeck, bool include_curses,
    int wid, char* out, size_t outsz)
{
    int max_width;

    if (!out || !outsz)
        return;

    out[0] = '\0';

    if (wid < 1)
        wid = 80;

    max_width = wid - 2;
    if (max_width < 1)
        return;

    if (steamdeck)
    {
        char notes_label[16], story_label[16], file_label[16];
        char abilities_label[16], increase_label[16], help_label[16], back_label[16];
        char token[7][64];

        controller_prompt_label('n', "n", notes_label, sizeof(notes_label));
        controller_prompt_label(steamdeck_secondary_key(), "Y", story_label, sizeof(story_label));
        controller_prompt_label('e', "L1", file_label, sizeof(file_label));
        controller_prompt_label(steamdeck_alt_action_key(), "X", abilities_label, sizeof(abilities_label));
        controller_prompt_label(steamdeck_confirm_key(), "A", increase_label, sizeof(increase_label));
        controller_prompt_label(steamdeck_info_key(), "RS", help_label, sizeof(help_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));

        strnfmt(token[0], sizeof(token[0]), "%s abilities", abilities_label);
        strnfmt(token[1], sizeof(token[1]), "%s increase", increase_label);
        strnfmt(token[2], sizeof(token[2]), "%s help", help_label);
        strnfmt(token[3], sizeof(token[3]), "%s back", back_label);
        strnfmt(token[4], sizeof(token[4]), "%s notes", notes_label);
        strnfmt(token[5], sizeof(token[5]), "%s story", story_label);
        strnfmt(token[6], sizeof(token[6]), "%s file", file_label);

        for (int i = 0; i < 4; i++)
            (void)character_sheet_prompt_append(out, outsz, token[i], max_width);
        for (int i = 4; i < 7; i++)
            (void)character_sheet_prompt_append(out, outsz, token[i], max_width);
    }
    else
    {
        const char* essential[] = {
            "a abilities", "Space/i increase", "? help", "ESC back"
        };
        const char* optional[] = {
            "n notes", "s story", "f file"
        };

        for (int i = 0; i < (int)(sizeof(essential) / sizeof(essential[0])); i++)
            (void)character_sheet_prompt_append(out, outsz, essential[i], max_width);

        if (include_curses)
            (void)character_sheet_prompt_append(out, outsz, "c curses", max_width);

        for (int i = 0; i < (int)(sizeof(optional) / sizeof(optional[0])); i++)
            (void)character_sheet_prompt_append(out, outsz, optional[i], max_width);
    }

    if (!out[0])
    {
        if (steamdeck)
            SDL_strlcpy(out, "B back", outsz);
        else
            SDL_strlcpy(out, "ESC back", outsz);
    }
}

static void character_sheet_draw_page_indicator(int sheet_page, int compact_pages,
    int wid, int row, bool use_story_font)
{
    char page_buf[32];
    int col;

    if (compact_pages <= 1)
        return;

    if (wid < 1)
        wid = 80;

    strnfmt(page_buf, sizeof(page_buf), "%d/%d", sheet_page + 1, compact_pages);
    col = wid - (int)strlen(page_buf) - 1;
    if (col < 0)
        col = 0;

    if (use_story_font)
        sdl_story_font_enable();

    Term_putstr(col, row, -1, TERM_SLATE, page_buf);

    if (use_story_font)
        sdl_story_font_disable();
}

void do_cmd_character_sheet(void)
{
    char ch;

    int mode = 0;
    int sheet_page = 1;
    int body_scroll = 0;
    int last_sheet_page = -1;

    /* Clear any active banner before opening character sheet */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Forever */
    while (1)
    {
        int wid = 80;
        int hgt = 24;
        int prompt_row;
        int indicator_row = 1;
        bool compact_sheet;
        int compact_pages;
        int max_body_scroll = 0;
        bool steamdeck = steamdeck_controls_active();

        Term_get_size(&wid, &hgt);
        if (wid < 1)
            wid = 80;
        if (hgt < 1)
            hgt = 24;

        compact_sheet = (wid < 80);
        compact_pages = compact_sheet ? 2 : 1;
        if (sheet_page >= compact_pages)
            sheet_page = compact_pages - 1;
        if (sheet_page < 0)
            sheet_page = 0;
        if (sheet_page != last_sheet_page)
        {
            body_scroll = 0;
            last_sheet_page = sheet_page;
        }

        if (compact_sheet)
        {
            switch (sheet_page)
            {
            case 0: mode = DISPLAY_PLAYER_MODE_COMPACT_DESC_FLAGS; break;
            case 1: mode = DISPLAY_PLAYER_MODE_COMPACT_STATS_SKILLS; break;
            default: mode = DISPLAY_PLAYER_MODE_COMPACT_DESC_FLAGS; break;
            }
        }
        else
        {
            mode = DISPLAY_PLAYER_MODE_STANDARD;
        }

        if (compact_sheet && sheet_page == 0)
            display_player_compact_set_scroll(body_scroll);
        else
            display_player_compact_set_scroll(0);

        /* Display the player */
        display_player(mode);
        if (compact_sheet && sheet_page == 0)
        {
            max_body_scroll = display_player_compact_get_max_scroll();
            if (body_scroll > max_body_scroll)
            {
                body_scroll = max_body_scroll;
                display_player_compact_set_scroll(body_scroll);
                display_player(mode);
            }
        }

        if (compact_sheet && hgt <= 18)
            indicator_row = 0;

        prompt_row = hgt - 1;
        if (prompt_row < 0)
            prompt_row = 0;
        Term_erase(0, prompt_row, 255);

        /* Prompt - dynamic, width-aware, and user-friendly for new players */
        {
            char prompt_buf[256];
#ifdef DEBUG_CURSES
            const bool include_curses = true;
#else
            const bool include_curses = false;
#endif

            character_sheet_build_prompt(steamdeck, include_curses, wid, prompt_buf, sizeof(prompt_buf));

            if (story_character_enabled())
                sdl_story_font_enable();

            character_sheet_put_prompt_fit(1, prompt_row, wid, TERM_L_WHITE, prompt_buf);

            character_sheet_draw_page_indicator(sheet_page, compact_pages, wid,
                indicator_row,
                story_character_enabled());

            if (story_character_enabled())
                sdl_story_font_disable();
        }

        Term_fresh();  /* Render commands */

        if (story_character_enabled()) {
            sdl_story_font_disable();
        }

        /* Query */
        ch = inkey();

        /* Exit - B button (back) or ESC */
        if (ch == ESCAPE || (steamdeck && ch == steamdeck_back_key()))
            break;
        if ((ch == '\r') || (ch == '\n') || (ch == 'q') || (ch == 'Q'))
            break;

        if (compact_pages > 1)
        {
            if (sheet_page == 0 && max_body_scroll > 0)
            {
                if (ch == '8')
                {
                    if (body_scroll > 0)
                        body_scroll--;
                    continue;
                }
                if (ch == '2')
                {
                    if (body_scroll < max_body_scroll)
                        body_scroll++;
                    continue;
                }
            }

            if ((ch == '4') || ((ch == '8') && !(sheet_page == 0 && max_body_scroll > 0)))
            {
                sheet_page = (sheet_page + compact_pages - 1) % compact_pages;
                continue;
            }
            if ((ch == '6') || ((ch == '2') && !(sheet_page == 0 && max_body_scroll > 0)))
            {
                sheet_page = (sheet_page + 1) % compact_pages;
                continue;
            }
        }

        /* Increase skills - 'i', Space, or confirm button */
        if (ch == 'i' || ch == ' ' || ch == INPUT_BIND_CONFIRM
            || (steamdeck && ch == steamdeck_confirm_key()))
        {
            gain_skills();
            /* Force redraw after skill changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* Show notes - 'n' */
        else if (ch == 'n')
        {
            do_cmd_knowledge_notes();
        }

        /* Story stats - 's' or Y button */
        else if (ch == 's' || (steamdeck && ch == steamdeck_secondary_key()))
        {
            print_metarun_stats();
        }

#ifdef DEBUG_CURSES
        /* Curses Menu */
        else if (ch == 'c')
        {
            dbg_show_active_flags();
        }
#endif

        /* Abilities - 'a', Tab, or X button */
        else if ((ch == 'a') || (ch == '\t') || (steamdeck && ch == steamdeck_alt_action_key()))
        {
            (void)do_cmd_ability_screen();
            /* Force redraw after ability changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* File dump - 'f' or L1 ('e') */
        else if (ch == 'f' || (steamdeck && ch == 'e'))
        {
            char ftmp[80];

            strnfmt(ftmp, sizeof(ftmp), "%s.txt", op_ptr->base_name);

            if (term_get_string("File name: ", ftmp, sizeof(ftmp)))
            {
                if (ftmp[0] && (ftmp[0] != ' '))
                {
                    if (file_character(ftmp, false))
                    {
                        msg_print("Character dump failed!");
                    }
                    else
                    {
                        msg_print("Character dump successful.");
                    }
                }
            }
        }

        /* Tutorial / Help - '?' or RS Right */
        else if (ch == '?' || (steamdeck && ch == steamdeck_info_key()))
        {
            display_character_tutorial();
        }

        /* Oops */
        else
        {
            bell("Illegal command for character sheet!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_load();

    /* Force redraw after screen restore if skills/abilities were changed */
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
    handle_stuff();
}

/* File: cmd-ui-character.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "platform-input.h"
#include "object/object-ui-select.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-oaths.h"
#include "sound-config.h"
#include "platform-audio.h"
#include "ui/ui-character-screen.h"
#include "ui/ui-information-scene.h"

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

static bool character_sheet_pause_information_scene(
    ui_information_scene_scope* scope)
{
    if (!scope || !scope->active)
        return false;

    ui_information_scene_leave(scope);
    return true;
}

static bool character_sheet_resume_information_scene(
    ui_information_scene_scope* scope)
{
    if (!scope)
        return false;

    return ui_information_scene_enter(scope);
}

void do_cmd_character_sheet(void)
{
    char ch;
    ui_information_scene_scope info_scope;
    bool steamdeck;

    /* Clear any active banner before opening character sheet */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    if (!ui_information_scene_supported())
    {
        log_warn("character sheet: snapshot renderer required; legacy renderer removed");
        msg_print("Character sheet requires the snapshot UI renderer.");
        return;
    }

    if (!ui_information_scene_enter(&info_scope))
        return;

    /* Forever */
    while (1)
    {
        app_ui_scene scene;
        char prompt_buf[256];

        steamdeck = steamdeck_controls_active();
        {
#ifdef DEBUG_CURSES
            const bool include_curses = true;
#else
            const bool include_curses = false;
#endif

            character_sheet_build_prompt(steamdeck, include_curses,
                (int)sizeof(prompt_buf) - 1,
                prompt_buf, sizeof(prompt_buf));
        }

        if (!build_character_sheet_ui_scene(&scene, prompt_buf)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&info_scope);
            log_warn("character sheet: semantic scene presentation failed");
            msg_print("Character sheet unavailable.");
            return;
        }

        /* Query */
        ch = (char)ui_information_scene_wait_key();

        /* Exit - B button (back) or ESC */
        if (ch == ESCAPE || (steamdeck && ch == steamdeck_back_key()))
            break;
        if ((ch == '\r') || (ch == '\n') || (ch == 'q') || (ch == 'Q'))
            break;

        /* Increase skills - 'i', Space, or confirm button */
        if (ch == 'i' || ch == ' ' || ch == INPUT_BIND_CONFIRM
            || (steamdeck && ch == steamdeck_confirm_key()))
        {
            if (!character_sheet_pause_information_scene(&info_scope))
                break;
            gain_skills();
            if (!character_sheet_resume_information_scene(&info_scope))
                return;
            /* Force redraw after skill changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* Show notes - 'n' */
        else if (ch == 'n')
        {
            if (!character_sheet_pause_information_scene(&info_scope))
                break;
            do_cmd_knowledge_notes();
            if (!character_sheet_resume_information_scene(&info_scope))
                return;
        }

        /* Story stats - 's' or Y button */
        else if (ch == 's' || (steamdeck && ch == steamdeck_secondary_key()))
        {
            if (!character_sheet_pause_information_scene(&info_scope))
                break;
            print_metarun_stats();
            if (!character_sheet_resume_information_scene(&info_scope))
                return;
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
            if (!character_sheet_pause_information_scene(&info_scope))
                break;
            (void)do_cmd_ability_screen();
            if (!character_sheet_resume_information_scene(&info_scope))
                return;
            /* Force redraw after ability changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* File dump - 'f' or L1 ('e') */
        else if (ch == 'f' || (steamdeck && ch == 'e'))
        {
            char ftmp[80];

            if (!character_sheet_pause_information_scene(&info_scope))
                break;
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
            if (!character_sheet_resume_information_scene(&info_scope))
                return;
        }

        /* Tutorial / Help - '?' or RS Right */
        else if (ch == '?' || (steamdeck && ch == steamdeck_info_key()))
        {
            if (!character_sheet_pause_information_scene(&info_scope))
                break;
            display_character_tutorial();
            if (!character_sheet_resume_information_scene(&info_scope))
                return;
        }

        /* Oops */
        else
        {
            bell("Illegal command for character sheet!");
        }

        /* Flush messages */
        message_flush();
    }

    ui_information_scene_leave(&info_scope);

    /* Force redraw after screen restore if skills/abilities were changed */
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
    handle_stuff();
}

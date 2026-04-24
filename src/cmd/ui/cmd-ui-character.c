/* File: cmd-ui-character.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
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
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "cmd-ui.h"

typedef enum character_sheet_footer_action {
    CHARACTER_SHEET_ACTION_ABILITIES = 1,
    CHARACTER_SHEET_ACTION_INCREASE = 2,
    CHARACTER_SHEET_ACTION_HELP = 3,
    CHARACTER_SHEET_ACTION_BACK = 4,
    CHARACTER_SHEET_ACTION_NOTES = 5,
    CHARACTER_SHEET_ACTION_STORY = 6,
    CHARACTER_SHEET_ACTION_FILE = 7
} character_sheet_footer_action;

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

static bool character_sheet_add_footer_actions(app_ui_scene* scene,
    bool steamdeck)
{
    app_ui_panel* panel;

    if (!scene || scene->panel_count == 0)
        return false;

    panel = &scene->panels[0];
    if (steamdeck)
    {
        char abilities_label[APP_UI_KEY_MAX];
        char increase_label[APP_UI_KEY_MAX];
        char help_label[APP_UI_KEY_MAX];
        char back_label[APP_UI_KEY_MAX];

        controller_prompt_label(steamdeck_alt_action_key(), "X",
            abilities_label, sizeof(abilities_label));
        controller_prompt_label(steamdeck_confirm_key(), "A",
            increase_label, sizeof(increase_label));
        controller_prompt_label(steamdeck_info_key(), "RS", help_label,
            sizeof(help_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        if (!app_ui_panel_add_footer_action(panel,
                CHARACTER_SHEET_ACTION_ABILITIES, TERM_WHITE, true,
                abilities_label, "Abilities")
            || !app_ui_panel_add_footer_action(panel,
                CHARACTER_SHEET_ACTION_INCREASE, TERM_L_BLUE, true,
                increase_label, "Increase")
            || !app_ui_panel_add_footer_action(panel,
                CHARACTER_SHEET_ACTION_HELP, TERM_WHITE, true, help_label,
                "Help")
            || !app_ui_panel_add_footer_action(panel,
                CHARACTER_SHEET_ACTION_BACK, TERM_WHITE, true, back_label,
                "Back"))
        {
            return false;
        }
    }
    else if (!app_ui_panel_add_footer_action(panel,
            CHARACTER_SHEET_ACTION_ABILITIES, TERM_WHITE, true, "a",
            "Abilities")
        || !app_ui_panel_add_footer_action(panel,
            CHARACTER_SHEET_ACTION_INCREASE, TERM_L_BLUE, true, "Space",
            "Increase")
        || !app_ui_panel_add_footer_action(panel, CHARACTER_SHEET_ACTION_HELP,
            TERM_WHITE, true, "?", "Help")
        || !app_ui_panel_add_footer_action(panel, CHARACTER_SHEET_ACTION_BACK,
            TERM_WHITE, true, "Esc", "Back"))
    {
        return false;
    }

    return app_ui_panel_add_footer_action(panel, CHARACTER_SHEET_ACTION_NOTES,
        TERM_WHITE, true, "n", "Notes")
        && app_ui_panel_add_footer_action(panel, CHARACTER_SHEET_ACTION_STORY,
            TERM_WHITE, true, "s", "Story")
        && app_ui_panel_add_footer_action(panel, CHARACTER_SHEET_ACTION_FILE,
            TERM_WHITE, true, "f", "File");
}

static bool character_sheet_command_to_key(const app_ui_command* command,
    char* out_key)
{
    const app_ui_widget_ref* target;

    if (out_key)
        *out_key = '\0';
    if (!command || !out_key)
        return false;

    target = &command->target;
    if (command->kind == APP_UI_COMMAND_KIND_CANCEL
        || target->action == APP_UI_WIDGET_ACTION_CANCEL)
    {
        *out_key = ESCAPE;
        return true;
    }

    if (target->role == APP_UI_WIDGET_ROLE_BUTTON)
    {
        if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
            return true;

        switch (target->widget_id)
        {
        case 1:
            *out_key = 'a';
            return true;
        case 2:
            *out_key = ' ';
            return true;
        case 3:
            *out_key = '?';
            return true;
        case 4:
            *out_key = ESCAPE;
            return true;
        case 5:
            *out_key = 'n';
            return true;
        case 6:
            *out_key = 's';
            return true;
        case 7:
            *out_key = 'f';
            return true;
        default:
            return true;
        }
    }

    return false;
}

void do_cmd_character_sheet(void)
{
    char ch;
    ui_information_scene_scope info_scope;
    bool steamdeck;

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
            || !character_sheet_add_footer_actions(&scene, steamdeck)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&info_scope);
            log_warn("character sheet: semantic scene presentation failed");
            msg_print("Character sheet unavailable.");
            return;
        }

        /* Query */
        {
            ui_information_scene_event event;
            char command_key = '\0';

            ch = '\0';
            if (!ui_information_scene_wait_event(&event, 0))
            {
                ch = ESCAPE;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            {
                ch = (char)event.key;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND
                && character_sheet_command_to_key(&event.command,
                    &command_key))
            {
                ch = command_key;
                if (!ch)
                    continue;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND)
            {
                continue;
            }
        }

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

            if (prompt_text_input("File name:",
                    "Enter accepts, Esc cancels, Backspace erases.", ftmp,
                    sizeof(ftmp), false))
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

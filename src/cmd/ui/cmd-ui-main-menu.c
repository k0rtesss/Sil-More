/* File: cmd-ui-main-menu.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "app/app-session.h"
#include "platform-story-font.h"
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
#include "runtime-cli.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "cmd-ui.h"
#include "ui/ui-information-scene.h"

#define MAIN_MENU_CHARACTER 1
#define MAIN_MENU_KNOWLEDGE 2
#define MAIN_MENU_QUEST_STATUS 3
#define MAIN_MENU_SCORES 4
#define MAIN_MENU_NOTE 9
#define MAIN_MENU_MAP 6
#define MAIN_MENU_MESSAGES 7
#define MAIN_MENU_SCREENSHOT 8
#define MAIN_MENU_STORY 10
#define MAIN_MENU_OPTIONS 11
#define MAIN_MENU_HELP 12
#define MAIN_MENU_ABORT 13
#define MAIN_MENU_SAVE 14
#define MAIN_MENU_SAVE_QUIT 15
#define MAIN_MENU_RETURN 16

#define MAIN_MENU_MAX 16

typedef struct main_menu_scene_scope {
    bool active;
    app_snapshot previous_snapshot;
} main_menu_scene_scope;

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

static bool main_menu_scene_enter(main_menu_scene_scope* scope)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (!scope)
        return false;

    memset(scope, 0, sizeof(*scope));
    if (!runtime_cli_snapshot_renderer() || !session)
        return false;

    snapshot = app_session_snapshot(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_DUNGEON)
        return false;

    scope->previous_snapshot = *snapshot;
    scope->active = true;
    return true;
}

static bool main_menu_scene_add_row(app_menu_scene* scene, int id,
    int highlight, bool death_view, cptr label, cptr meta)
{
    byte attr = TERM_WHITE;
    bool enabled = true;

    if (!scene || !label)
        return false;

    if (death_view && id >= MAIN_MENU_ABORT && id <= MAIN_MENU_SAVE_QUIT)
    {
        attr = TERM_L_DARK;
        enabled = false;
    }
    else if (highlight == id)
    {
        attr = TERM_L_BLUE;
    }

    return app_menu_scene_add_row(scene, (s16b)id, attr, enabled,
        highlight == id, "", label, meta ? meta : "");
}

static bool main_menu_scene_present(main_menu_scene_scope* scope, int highlight,
    bool death_view)
{
    app_session* session = app_session_current();
    app_menu_scene scene;

    if (!scope || !scope->active || !session)
        return false;

    if (highlight < 1 || highlight > MAIN_MENU_MAX)
        highlight = MAIN_MENU_CHARACTER;
    if (death_view && highlight >= MAIN_MENU_ABORT
        && highlight <= MAIN_MENU_SAVE_QUIT)
    {
        highlight = MAIN_MENU_RETURN;
    }

    app_menu_scene_init(&scene);
    scene.flags = APP_MENU_SCENE_FLAG_DIM_BACKDROP
        | APP_MENU_SCENE_FLAG_PLAIN;
    app_menu_scene_set_widths(&scene, 300, 460);

    if (!main_menu_scene_add_row(&scene, MAIN_MENU_CHARACTER, highlight,
            death_view, "Character sheet      (c)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_KNOWLEDGE, highlight,
            death_view, "Known lore           (a)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_QUEST_STATUS, highlight,
            death_view, "Quest status         (t)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_SCORES, highlight,
            death_view, "Halls of Mandos      (d)", "")
        || !main_menu_scene_add_row(&scene, 5, highlight, death_view,
            "Run history          (v)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_MAP, highlight,
            death_view, "Map                  (m)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_MESSAGES, highlight,
            death_view, "Log                  (l)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_SCREENSHOT, highlight,
            death_view, "Combat history       (x)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_NOTE, highlight,
            death_view, "Hint messages        (i)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_STORY, highlight,
            death_view, "The story so far     (y)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_OPTIONS, highlight,
            death_view, "Options and misc     (o)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_HELP, highlight,
            death_view, "Help                 (h)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_ABORT, highlight,
            death_view, "Suicide              (k)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_SAVE, highlight,
            death_view, "Save                 (s)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_SAVE_QUIT, highlight,
            death_view, "Quit with save       (q)", "")
        || !main_menu_scene_add_row(&scene, MAIN_MENU_RETURN, highlight,
            death_view, "Return to game       (r)", ""))
    {
        return false;
    }

    app_session_clear_interaction(session);
    if (!app_session_publish_dungeon_overlay_menu(session, &scene))
        return false;

    (void)Term_xtra(TERM_XTRA_FRESH, 0);
    return true;
}

static void main_menu_scene_leave(main_menu_scene_scope* scope)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !session)
        return;

    app_session_clear_dungeon_overlay_menu(session);
    app_session_set_snapshot(session, &scope->previous_snapshot);
    scope->active = false;
    (void)Term_xtra(TERM_XTRA_FRESH, 0);
}

static void main_menu_publish_interaction(int highlight, bool death_view)
{
    app_session* session;

    session = app_session_current();
    if (!app_session_interactions_enabled(session))
        return;

    if (highlight < 1 || highlight > MAIN_MENU_MAX)
        highlight = 1;
    if (death_view && highlight >= 13 && highlight <= 15)
        highlight = MAIN_MENU_RETURN;

    app_session_begin_interaction(session, APP_INTERACTION_KIND_LIST,
        APP_WAIT_REASON_COMMAND_INPUT,
        APP_INTERACTION_FLAG_CAN_CONFIRM
            | APP_INTERACTION_FLAG_CAN_CANCEL
            | APP_INTERACTION_FLAG_SHOW_OPTIONS
            | APP_INTERACTION_FLAG_PLAIN_LIST);

    (void)app_session_add_interaction_option(session,
        (highlight == MAIN_MENU_CHARACTER) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == MAIN_MENU_CHARACTER,
        "Character sheet      (c)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == MAIN_MENU_KNOWLEDGE) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == MAIN_MENU_KNOWLEDGE,
        "Known lore           (a)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == MAIN_MENU_QUEST_STATUS) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == MAIN_MENU_QUEST_STATUS,
        "Quest status         (t)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == 4) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == 4,
        "Halls of Mandos      (d)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == 5) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == 5,
        "Run history          (v)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == MAIN_MENU_MAP) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == MAIN_MENU_MAP,
        "Map                  (m)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == MAIN_MENU_MESSAGES) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == MAIN_MENU_MESSAGES,
        "Log                  (l)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == MAIN_MENU_SCREENSHOT) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == MAIN_MENU_SCREENSHOT,
        "Combat history       (x)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == MAIN_MENU_NOTE) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == MAIN_MENU_NOTE,
        "Hint messages        (i)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == MAIN_MENU_STORY) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == MAIN_MENU_STORY,
        "The story so far     (y)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == MAIN_MENU_OPTIONS) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == MAIN_MENU_OPTIONS,
        "Options and misc     (o)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == MAIN_MENU_HELP) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == MAIN_MENU_HELP,
        "Help                 (h)", "");
    (void)app_session_add_interaction_option(session,
        death_view ? TERM_L_DARK
                   : ((highlight == MAIN_MENU_ABORT) ? TERM_L_BLUE : TERM_WHITE),
        0, !death_view, highlight == MAIN_MENU_ABORT,
        "Suicide              (k)", "");
    (void)app_session_add_interaction_option(session,
        death_view ? TERM_L_DARK
                   : ((highlight == MAIN_MENU_SAVE) ? TERM_L_BLUE : TERM_WHITE),
        0, !death_view, highlight == MAIN_MENU_SAVE,
        "Save                 (s)", "");
    (void)app_session_add_interaction_option(session,
        death_view ? TERM_L_DARK
                   : ((highlight == MAIN_MENU_SAVE_QUIT) ? TERM_L_BLUE : TERM_WHITE),
        0, !death_view, highlight == MAIN_MENU_SAVE_QUIT,
        "Quit with save       (q)", "");
    (void)app_session_add_interaction_option(session,
        (highlight == MAIN_MENU_RETURN) ? TERM_L_BLUE : TERM_WHITE,
        0, true, highlight == MAIN_MENU_RETURN,
        "Return to game       (r)", "");

    app_session_set_interaction_selected(session,
        (s16b)(highlight - 1));
}

/*
 * Performs the interface and selection work for the main menu.
 */
static int main_menu_aux(int* highlight, bool scene_active,
    bool clear_fullscreen, main_menu_scene_scope* menu_scene_scope)
{
    char ch;
    int i;
    bool death_view = death_spectator_active();
    bool use_menu_scene = menu_scene_scope && menu_scene_scope->active
        && main_menu_scene_present(menu_scene_scope, *highlight, death_view);

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
        if (clear_fullscreen || Term->hgt <= 18)
            row_top = 0;
        else
            row_top = (Term->hgt > 1) ? 1 : 0;
    }

    if (clear_fullscreen && Term)
        menu_h = Term->hgt;

    if (death_view && (*highlight >= 13) && (*highlight <= 15))
        *highlight = 16;

    if (!use_menu_scene)
    {
        for (i = 0; i < menu_h; i++)
        {
            int y = row_top + i;
            if (!Term || y < 0 || y >= Term->hgt)
                continue;

            int clear_x = clear_fullscreen ? 0 : col_main - 2;
            int clear_w = clear_fullscreen ? Term->wid : menu_w + 4;

            if (clear_x < 0)
                clear_x = 0;
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

        main_menu_publish_interaction(*highlight, death_view);

        /* Flush the prompt */
        if (!scene_active || !ui_information_scene_present_term())
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
    }

    /* Get key (while allowing menu commands) */
    inkey_set_cursor_hidden(true);
    if (scene_active)
        ch = (char)ui_information_scene_wait_key();
    else
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
    app_session* session = app_session_current();
    const app_snapshot* snapshot;
    int actiontype = -1;
    int highlight = 1;
    bool leave_menu = false;
    bool pending_hint_look = false;
    int pending_hint_look_y = -1;
    int pending_hint_look_x = -1;
    ui_information_scene_scope scene_scope;
    main_menu_scene_scope menu_scene_scope;
    bool menu_scene_active;
    bool allow_information_scene;
    bool scene_active;
    bool clear_fullscreen;
    bool restore_saved_screen;

    /* Clear any active banner before opening main menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    snapshot = session ? app_session_snapshot(session) : NULL;
    menu_scene_active = main_menu_scene_enter(&menu_scene_scope);
    if (menu_scene_active
        && !main_menu_scene_present(&menu_scene_scope, highlight,
            death_spectator_active()))
    {
        main_menu_scene_leave(&menu_scene_scope);
        menu_scene_active = false;
    }

    allow_information_scene = snapshot
        && snapshot->scene == APP_SCENE_KIND_INFORMATION
        && !menu_scene_active;
    scene_active = allow_information_scene
        && ui_information_scene_enter(&scene_scope);
    clear_fullscreen = scene_active;
    restore_saved_screen = !scene_active && !menu_scene_active;

    /* Save screen */
    if (restore_saved_screen)
        screen_save();
    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        actiontype = main_menu_aux(&highlight, scene_active,
            clear_fullscreen, &menu_scene_scope);

        if (death_spectator_active() && (actiontype >= 13) && (actiontype <= 15))
        {
            msg_print("You can no longer take that action.");
            continue;
        }

        switch (actiontype)
        {
        case 1: // Character sheet (c)
        case 2: // Known lore (a)
        case 3: // Quest status (t)
        case 4: // Halls of Mandos (d)
        case 5: // Run history (v)
        case 6: // Map (m)
        case 7: // Log (l)
        case 8: // Combat history (x)
        case 9: // Hint messages (i)
        case 10: // The story so far (y)
        case 11: // Options and misc (o)
        case 12: // Help (h)
        case 13: // Suicide (k)
        case 14: // Save (s)
        case 15: // Quit with save (q)
        case 16: // Return to game (r)
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
    if (scene_active)
        ui_information_scene_leave(&scene_scope);
    else if (menu_scene_active)
        main_menu_scene_leave(&menu_scene_scope);
    else if (restore_saved_screen)
        screen_load();

    if (app_session_interactions_enabled(session))
        app_session_clear_interaction(session);

    switch (actiontype)
    {
    case 1: // Character sheet (c)
    {
        do_cmd_character_sheet();
        break;
    }
    case 2: // Known lore (a)
    {
        do_cmd_knowledge_browser_page(cmd_ui_knowledge_last_page());
        break;
    }
    case 3: // Quest status (t)
    {
        do_cmd_quest_status();
        break;
    }
    case 4: // Halls of Mandos (d)
    {
        log_info("main menu: opening Halls of Mandos view");
        show_scores_interactive(true);
        break;
    }
    case 5: // Run history (v)
    {
        do_cmd_run_history();
        break;
    }
    case 6: // Map (m)
    {
        do_cmd_view_map();
        break;
    }
    case 7: // Log (l)
    {
        do_cmd_messages();
        break;
    }
    case 8: // Combat history (x)
    {
        do_cmd_combat_history();
        break;
    }
    case 9: // Hint messages (i)
    {
        do_cmd_hint_messages(&pending_hint_look, &pending_hint_look_y,
            &pending_hint_look_x);
        break;
    }
    case 10: // The story so far (y)
    {
        /* Save screen before showing story */
        screen_save();
        print_story(15, 1);
        /* Load screen after story */
        screen_load();
        break;
    }
    case 11: // Options and misc (o)
    {
        do_cmd_options();
        break;
    }
    case 12: // Help (h)
    {
        do_cmd_help();
        break;
    }
    case 13: // Suicide (k)
    {
        do_cmd_suicide();
        break;
    }
    case 14: // Save (s)
    {
        do_cmd_save_game();
        break;
    }
    case 15: // Quit with save (q)
    {
        /* Stop playing */
        p_ptr->playing = false;

        /* Mark that we want to quit to menu, not exit application */
        p_ptr->quit_to_menu = true;

        /* Leaving */
        p_ptr->leaving = true;
        break;
    }
    case 16: // Return to game (r)
    case -1:
    {
        break;
    }
    default:
    {
        break;
    }
    }

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

static bool hint_message_add_information_segment(app_information_scene* scene,
    s16b row, s16b col, byte attr, byte story, const char* text)
{
    if (!scene || !text || !text[0])
        return true;

    return app_information_scene_add_text_ex(scene, row, col, attr, story,
        text);
}

static bool hint_message_build_information_colored_line(
    app_information_scene* scene, int row, int col, byte base_attr,
    byte story, const char* line, const hint_message_meta* meta)
{
    int start = 0;
    int cursor = col;
    int len;

    if (!scene)
        return false;
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
                if (!hint_message_add_information_segment(scene, (s16b)row,
                        (s16b)cursor, base_attr, story, plain))
                {
                    return false;
                }
                cursor += plain_len;
            }

            {
                char special[HINT_MESSAGE_CUE_TEXT_MAX + 1];

                memcpy(special, line + i, match_len);
                special[match_len] = '\0';
                if (!hint_message_add_information_segment(scene, (s16b)row,
                        (s16b)cursor, match_attr, story, special))
                {
                    return false;
                }
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
        if (!hint_message_add_information_segment(scene, (s16b)row,
                (s16b)cursor, base_attr, story, tail))
        {
            return false;
        }
    }

    return true;
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

static bool hint_message_build_information_list_row(app_information_scene* scene,
    int row, int idx, bool selected, int wid)
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

    if (!scene)
        return false;

    hint_messages_message_meta(idx, &meta);

    strnfmt(prefix, sizeof(prefix), "%2d) ", idx + 1);
    if (!app_information_scene_add_text(scene, (s16b)row, (s16b)col,
            prefix_attr, prefix))
    {
        return false;
    }
    col += (int)strlen(prefix);

    title_room = MAX(8, wid - col - 1);
    if (meta.cue_count > 0)
        title_room = MIN(title_room, MAX(wid / 2, 24));
    hint_message_build_title(title_buf, sizeof(title_buf), title, title_room);
    if (!app_information_scene_add_text(scene, (s16b)row, (s16b)col,
            title_attr, title_buf))
    {
        return false;
    }
    col += (int)strlen(title_buf);

    if (meta.cue_count <= 0 || col >= wid - 4)
        return true;

    if (!app_information_scene_add_text(scene, (s16b)row, (s16b)col,
            chrome_attr, " ["))
    {
        return false;
    }
    col += 2;

    for (int cue = 0; cue < meta.cue_count && col < wid - 1; ++cue)
    {
        if (cue > 0)
        {
            if (!app_information_scene_add_text(scene, (s16b)row, (s16b)col,
                    chrome_attr, "; "))
            {
                return false;
            }
            col += 2;
        }

        if (meta.cue_dists[cue][0])
        {
            if (!app_information_scene_add_text(scene, (s16b)row, (s16b)col,
                    TERM_YELLOW, meta.cue_dists[cue]))
            {
                return false;
            }
            col += (int)strlen(meta.cue_dists[cue]);
        }

        if (meta.cue_dists[cue][0] && meta.cue_dirs[cue][0] && col < wid - 1)
        {
            if (!app_information_scene_add_text(scene, (s16b)row, (s16b)col,
                    chrome_attr, " "))
            {
                return false;
            }
            col += 1;
        }

        if (meta.cue_dirs[cue][0] && col < wid - 1)
        {
            if (!app_information_scene_add_text(scene, (s16b)row, (s16b)col,
                    TERM_L_BLUE, meta.cue_dirs[cue]))
            {
                return false;
            }
            col += (int)strlen(meta.cue_dirs[cue]);
        }
    }

    if (col < wid - 1)
    {
        if (!app_information_scene_add_text(scene, (s16b)row, (s16b)col,
                chrome_attr, "]"))
        {
            return false;
        }
    }

    return true;
}

static bool hint_message_build_information_detail_scene(
    app_information_scene* scene, int index, int hgt)
{
    hint_message_meta meta;
    byte line_count;
    byte story = STORY_FLAG_USE;
    int row = 4;
    int col = 8;

    if (!scene)
        return false;

    hint_messages_ensure_level_state();
    line_count = hint_messages_message_line_count(index);
    if (!line_count)
        return false;

    hint_messages_message_meta(index, &meta);
    app_information_scene_init(scene);

    for (int li = 0; li < line_count && row + li < hgt - 1; ++li)
    {
        const char* line = hint_messages_message_line(index, li);
        byte base_attr = (li == 0) ? TERM_L_WHITE : TERM_WHITE;
        const hint_message_meta* line_meta = (li == 0) ? NULL : &meta;

        if (!hint_message_build_information_colored_line(scene, row + li, col,
                base_attr, story, line, line_meta))
        {
            return false;
        }
    }

    if (hint_message_has_source(&meta))
    {
        if (!app_information_scene_add_text(scene, (s16b)(hgt - 1), 0,
                TERM_WHITE,
                "[Press any key to continue, or 'l' to look at the skeleton]"))
        {
            return false;
        }
    }
    else
    {
        if (!app_information_scene_add_text(scene, (s16b)(hgt - 1), 0,
                TERM_WHITE, "[Press any key to continue]"))
        {
            return false;
        }
    }

    return true;
}

static bool hint_message_show_information_scene(int index, int* look_y,
    int* look_x, bool* out_request_look)
{
    ui_information_scene_scope scope;
    app_information_scene scene;
    hint_message_meta meta;
    byte line_count;

    if (out_request_look)
        *out_request_look = false;

    hint_messages_ensure_level_state();
    line_count = hint_messages_message_line_count(index);
    if (!line_count)
        return false;

    if (!ui_information_scene_enter(&scope))
        return false;

    hint_messages_message_meta(index, &meta);

    while (1)
    {
        int wid;
        int hgt;
        char ch;

        Term_get_size(&wid, &hgt);
        if (!hint_message_build_information_detail_scene(&scene, index, hgt))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        if (!ui_information_scene_present(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = (char)ui_information_scene_wait_key_nonrepeat();
        if ((ch == 'l' || ch == 'L') && hint_message_has_source(&meta))
        {
            if (look_y)
                *look_y = meta.source_y;
            if (look_x)
                *look_x = meta.source_x;
            if (out_request_look)
                *out_request_look = true;
            ui_information_scene_leave(&scope);
            return true;
        }

        break;
    }

    ui_information_scene_leave(&scope);
    return true;
}

void show_hint_message_screen(int index)
{
    int look_y = -1;
    int look_x = -1;
    bool request_look = false;

    if (!ui_information_scene_supported())
    {
        log_warn("hint message detail: snapshot renderer required; legacy detail renderer removed");
        msg_print("Hint message viewer requires the snapshot UI renderer.");
        return;
    }

    if (!hint_message_show_information_scene(index, &look_y, &look_x,
            &request_look))
    {
        log_warn("hint message detail: information-scene presentation failed on the snapshot renderer path");
        msg_print("Hint message viewer unavailable.");
        return;
    }

    if (request_look)
    {
        do_cmd_redraw();
        do_cmd_look_at(look_y, look_x);
    }
}

static bool do_cmd_hint_messages_information_scene(bool* out_pending_look,
    int* out_look_y, int* out_look_x)
{
    ui_information_scene_scope scope;
    bool pending_look = false;
    int look_y = -1;
    int look_x = -1;
    int n;
    int sel = 0;
    int top = 0;

    hint_messages_ensure_level_state();
    n = (int)hint_messages_count_for_save();
    if (n <= 0)
        return false;

    if (!ui_information_scene_enter(&scope))
        return false;

    while (1)
    {
        app_information_scene scene;
        int wid;
        int hgt;
        int rows;
        char ch;

        app_information_scene_init(&scene);
        Term_get_size(&wid, &hgt);

        rows = hgt - 4;
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

        {
            char title[64];

            strnfmt(title, sizeof(title), "Hint Messages (%d)", n);
            if (!app_information_scene_add_text(&scene, 0, 0, TERM_WHITE, title)
                || !app_information_scene_add_text(&scene, (s16b)(hgt - 1), 0,
                    TERM_WHITE,
                    "[Press '8'/'2' to move, Enter to read, 'l' to look, or ESCAPE]"))
            {
                ui_information_scene_leave(&scope);
                return false;
            }
        }

        for (int row = 0; row < rows && top + row < n; ++row)
        {
            int idx = top + row;

            if (!hint_message_build_information_list_row(&scene, 2 + row, idx,
                    idx == sel, wid))
            {
                ui_information_scene_leave(&scope);
                return false;
            }
        }

        if (!ui_information_scene_present(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = (char)ui_information_scene_wait_key();

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

            bool request_look = false;

            if (!hint_message_show_information_scene(sel, &selected_look_y,
                    &selected_look_x, &request_look))
            {
                ui_information_scene_leave(&scope);
                return false;
            }

            if (request_look)
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

    ui_information_scene_leave(&scope);

    if (out_pending_look)
        *out_pending_look = pending_look;
    if (out_look_y)
        *out_look_y = look_y;
    if (out_look_x)
        *out_look_x = look_x;

    return true;
}

static void do_cmd_hint_messages(bool* out_pending_look, int* out_look_y,
    int* out_look_x)
{
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

    if (!ui_information_scene_supported())
    {
        log_warn("hint messages: snapshot renderer required; legacy hint-message renderer removed");
        msg_print("Hint message browser requires the snapshot UI renderer.");
        return;
    }

    if (!do_cmd_hint_messages_information_scene(out_pending_look,
            out_look_y, out_look_x))
    {
        log_warn("hint messages: information-scene presentation failed on the snapshot renderer path");
        msg_print("Hint message browser unavailable.");
    }
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
static bool message_recall_information_scene_pause(
    ui_information_scene_scope* scope)
{
    if (!scope)
        return false;

    ui_information_scene_leave(scope);
    return true;
}

static bool message_recall_information_scene_resume(
    ui_information_scene_scope* scope)
{
    if (!scope)
        return false;

    return ui_information_scene_enter(scope);
}

static bool do_cmd_messages_information_scene(void)
{
    ui_information_scene_scope scope;
    char shower[80];
    char finder[80];
    int i = 0;
    int q = 0;

    if (!ui_information_scene_enter(&scope))
        return false;

    SDL_strlcpy(finder, "", sizeof(finder));
    SDL_strlcpy(shower, "", sizeof(shower));

    while (true)
    {
        app_information_scene scene;
        int n = message_num();
        int wid;
        int hgt;
        int shown;
        int old_i;
        char ch;

        app_information_scene_init(&scene);
        Term_get_size(&wid, &hgt);

        for (shown = 0; (shown < hgt - 4) && (i + shown < n); shown++)
        {
            cptr msg = message_str((s16b)(i + shown));
            byte attr = message_color((s16b)(i + shown));
            char visible[APP_INFORMATION_TEXT_MAX];

            if ((int)strlen(msg) >= q)
                SDL_strlcpy(visible, msg + q, sizeof(visible));
            else
                visible[0] = '\0';

            (void)app_information_scene_add_text(&scene,
                (s16b)(hgt - 3 - shown), 0, attr, visible);

            if (shower[0])
            {
                cptr str = visible;

                while ((str = strstr(str, shower)) != NULL)
                {
                    size_t len = strlen(shower);
                    char highlight[APP_INFORMATION_TEXT_MAX];

                    if (len >= sizeof(highlight))
                        len = sizeof(highlight) - 1;
                    memcpy(highlight, str, len);
                    highlight[len] = '\0';

                    (void)app_information_scene_add_text(&scene,
                        (s16b)(hgt - 3 - shown), (s16b)(str - visible),
                        TERM_YELLOW, highlight);

                    if (len == 0)
                        break;
                    str += len;
                }
            }
        }

        {
            char header[APP_INFORMATION_TEXT_MAX];
            char prompt[APP_INFORMATION_TEXT_MAX];
            int last_shown = (shown > 0) ? (i + shown - 1) : i;

            strnfmt(header, sizeof(header),
                "Message Recall (%d-%d of %d), Offset %d", i, last_shown, n, q);
            (void)app_information_scene_add_text(&scene, 0, 0, TERM_WHITE,
                header);

            SDL_strlcpy(prompt,
                "[Press 'p' for older, 'n' for newer, ..., or ESCAPE]",
                sizeof(prompt));
            (void)app_information_scene_add_text(&scene, (s16b)(hgt - 1), 0,
                TERM_WHITE, prompt);
        }

        if (!ui_information_scene_present(&scene))
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
            if (!message_recall_information_scene_pause(&scope))
                break;

            if (!term_get_string("Show: ", shower, sizeof(shower)))
            {
                if (!message_recall_information_scene_resume(&scope))
                    return false;
                continue;
            }

            if (!message_recall_information_scene_resume(&scope))
                return false;

            continue;
        }

        if (ch == '/')
        {
            s16b z;

            if (!message_recall_information_scene_pause(&scope))
                break;

            if (term_get_string("Find: ", finder, sizeof(finder)))
            {
                SDL_strlcpy(shower, finder, sizeof(shower));

                for (z = i + 1; z < n; z++)
                {
                    cptr msg = message_str(z);

                    if (strstr(msg, finder))
                    {
                        i = z;
                        break;
                    }
                }
            }

            if (!message_recall_information_scene_resume(&scope))
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

void do_cmd_messages(void)
{
    /* Clear any active banner before opening message history */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    if (!ui_information_scene_supported())
    {
        log_warn("message recall: snapshot renderer required; legacy message-recall renderer removed");
        msg_print("Message recall requires the snapshot UI renderer.");
        return;
    }

    if (!do_cmd_messages_information_scene())
    {
        log_warn("message recall: information-scene presentation failed on the snapshot renderer path");
        msg_print("Message recall unavailable.");
    }
    return;
}


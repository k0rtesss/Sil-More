/* File: cmd-ui-knowledge.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "platform-input.h"
#include "player/player-abilities.h"
#include "fs/file.h"
#include "fs/path.h"
#include "log/log.h"
#include "metarun.h"
#include "cmd-ui-knowledge.h"

static int g_knowledge_last_page = KNOWLEDGE_PAGE_ARTEFACTS;

int cmd_ui_knowledge_last_page(void)
{
    return g_knowledge_last_page;
}

void knowledge_set_last_page(int page)
{
    g_knowledge_last_page = page;
}

bool knowledge_pause_information_scene(ui_information_scene_scope* scope)
{
    if (!scope || !scope->active)
        return false;

    ui_information_scene_leave(scope);
    return true;
}

bool knowledge_resume_information_scene(ui_information_scene_scope* scope)
{
    if (!scope)
        return false;

    return ui_information_scene_enter(scope);
}

bool knowledge_enter_information_scene_or_report(
    ui_information_scene_scope* scope, cptr log_name, cptr unavailable_message)
{
    if (ui_information_scene_enter(scope))
        return true;

    log_warn("%s: semantic scene unavailable on the snapshot renderer path",
        log_name ? log_name : "knowledge");
    if (unavailable_message && unavailable_message[0])
        msg_print(unavailable_message);
    return false;
}

bool knowledge_present_ui_scene_or_abort(
    ui_information_scene_scope* scope, bool build_ok, app_ui_scene* scene,
    cptr scene_name, cptr user_message)
{
    if (build_ok && scene && ui_information_scene_present_ui(scene))
        return true;

    log_error("knowledge: failed to present SDL semantic scene for %s",
        scene_name ? scene_name : "unknown knowledge screen");
    if (scope && scope->active)
        ui_information_scene_leave(scope);
    bell(user_message ? user_message : "Knowledge screen unavailable.");
    if (user_message && user_message[0])
        msg_print(user_message);
    return false;
}

/* display the notes file */
void do_cmd_knowledge_notes(void)
{
    show_buffer(notes_buffer, 0);
}

/*
 * Display oath status information
 */
void do_cmd_knowledge_oaths(void)
{
    ang_file* fff;
    char file_name[1024];

    /* Temporary file */
    if (!path_temp(file_name, sizeof(file_name)))
        return;

    /* Open a new file */
    fff = ang_file_open(file_name, "w");

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Scan the oaths */
    ang_file_printf(fff, "Oath Status\n\n");

    /* Check current character oath */
    if (p_ptr->have_ability[S_SPC][SPC_OATH_MERCY])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_MERCY])
            ang_file_printf(fff, "Current Oath: Oath of Mercy (Active)\n\n");
        else
            ang_file_printf(fff, "Current Oath: Oath of Mercy (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_SILENCE])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_SILENCE])
            ang_file_printf(fff, "Current Oath: Oath of Silence (Active)\n\n");
        else
            ang_file_printf(fff, "Current Oath: Oath of Silence (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_IRON])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_IRON])
            ang_file_printf(fff, "Current Oath: Oath of Iron (Active)\n\n");
        else
            ang_file_printf(fff, "Current Oath: Oath of Iron (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_SMITH])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_SMITH])
            ang_file_printf(fff, "Current Oath: Oath of the Smith (Active)\n\n");
        else
            ang_file_printf(fff, "Current Oath: Oath of the Smith (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_VALOROUS])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_VALOROUS])
        {
            ang_file_printf(fff,
                "Current Oath: Oath of Valorous Heart (Active)\n\n");
        }
        else
        {
            ang_file_printf(fff,
                "Current Oath: Oath of Valorous Heart (Broken)\n\n");
        }
    }
    else
    {
        ang_file_printf(fff, "Current Oath: None\n\n");
    }

    /* Display metarun oath status */
    ang_file_printf(fff, "Metarun Oath Status:\n");

    /* Check unlocked oaths */
    {
        bool has_unlocked = false;

        if (oath_unlocked(OATH_MERCY))
        {
            ang_file_printf(fff, "  Oath of Mercy: Unlocked");
            if (oath_banned(OATH_MERCY))
                ang_file_printf(fff, " (Banned this run)");
            ang_file_printf(fff, "\n");
            has_unlocked = true;
        }

        if (oath_unlocked(OATH_SILENCE))
        {
            ang_file_printf(fff, "  Oath of Silence: Unlocked");
            if (oath_banned(OATH_SILENCE))
                ang_file_printf(fff, " (Banned this run)");
            ang_file_printf(fff, "\n");
            has_unlocked = true;
        }

        if (oath_unlocked(OATH_IRON))
        {
            ang_file_printf(fff, "  Oath of Iron: Unlocked");
            if (oath_banned(OATH_IRON))
                ang_file_printf(fff, " (Banned this run)");
            ang_file_printf(fff, "\n");
            has_unlocked = true;
        }

        if (oath_unlocked(OATH_SMITH))
        {
            ang_file_printf(fff, "  Oath of the Smith: Unlocked");
            if (oath_banned(OATH_SMITH))
                ang_file_printf(fff, " (Banned this run)");
            ang_file_printf(fff, "\n");
            has_unlocked = true;
        }

        if (oath_unlocked(OATH_VALOROUS))
        {
            ang_file_printf(fff, "  Oath of Valorous Heart: Unlocked");
            if (oath_banned(OATH_VALOROUS))
                ang_file_printf(fff, " (Banned this run)");
            ang_file_printf(fff, "\n");
            has_unlocked = true;
        }

        if (!has_unlocked)
        {
            ang_file_printf(fff, "  No oaths unlocked yet.\n");
            ang_file_printf(fff,
                "  Complete Valar quests to unlock new oaths.\n");
        }
    }

    /* Close the file */
    ang_file_close(fff);

    /* Display the file contents */
    show_file(file_name, "Oath Status", 0);

    /* Remove the file */
    fd_kill(file_name);
}

static cptr knowledge_tab_label(int page)
{
    static const cptr labels[] = { "Arts", "Objs", "Mons", "Curses" };

    if (page < KNOWLEDGE_PAGE_ARTEFACTS || page > KNOWLEDGE_PAGE_CURSES)
        return "";

    return labels[page];
}

void knowledge_browser_cursor_with_rows(char ch, int* column, int* grp_cur,
    int grp_cnt, int* list_cur, int list_cnt, int page_rows)
{
    int d;
    int col = *column;
    int grp = *grp_cur;
    int list = *list_cur;
    int page_jump = (page_rows > 0) ? page_rows : KNOWLEDGE_BROWSER_ROWS;

    /* Extract direction */
    d = target_dir(ch);

    if (!d)
        return;

    /* Diagonals */
    if ((ddx[d] > 0) && ddy[d])
    {
        /* Browse group list */
        if (!col)
        {
            int old_grp = grp;

            grp += ddy[d] * page_jump;

            if (grp >= grp_cnt)
                grp = grp_cnt - 1;
            if (grp < 0)
                grp = 0;
            if (grp != old_grp)
                list = 0;
        }

        /* Browse entry list */
        else
        {
            list += ddy[d] * page_jump;

            if (list >= list_cnt)
                list = list_cnt - 1;
            if (list < 0)
                list = 0;
        }

        *grp_cur = grp;
        *list_cur = list;
        return;
    }

    if (ddx[d])
    {
        col += ddx[d];
        if (col < 0)
            col = 0;
        if (col > 1)
            col = 1;

        *column = col;
        return;
    }

    /* Browse group list */
    if (!col)
    {
        int old_grp = grp;

        grp += ddy[d];

        if (grp >= grp_cnt)
            grp = grp_cnt - 1;
        if (grp < 0)
            grp = 0;
        if (grp != old_grp)
            list = 0;
    }

    /* Browse entry list */
    else
    {
        list += ddy[d];

        if (list >= list_cnt)
            list = list_cnt - 1;
        if (list < 0)
            list = 0;
    }

    *grp_cur = grp;
    *list_cur = list;
}

int knowledge_normalize_page(int page)
{
    if (page < KNOWLEDGE_PAGE_ARTEFACTS || page > KNOWLEDGE_PAGE_CURSES)
        return cmd_ui_knowledge_last_page();

    return page;
}

bool knowledge_handle_page_input(char ch, int* page)
{
    int next_page = *page;

    switch (ch)
    {
    case 'A':
    case 'a':
        next_page = KNOWLEDGE_PAGE_ARTEFACTS;
        break;
    case 'B':
    case 'b':
        next_page = KNOWLEDGE_PAGE_OBJECTS;
        break;
    case 'N':
    case 'n':
        next_page = KNOWLEDGE_PAGE_MONSTERS;
        break;
    case 'U':
    case 'u':
        next_page = KNOWLEDGE_PAGE_CURSES;
        break;
    case '\t':
    case ']':
    case 'I':
    case 'i':
        next_page = (*page + 1) % 4;
        break;
    case '[':
    case 'E':
    case 'e':
        next_page = (*page + 3) % 4;
        break;
    default:
        return false;
    }

    *page = next_page;
    knowledge_set_last_page(next_page);
    return true;
}

bool knowledge_handle_tab_navigation(char ch, int* page, bool* tabs_focus,
    bool can_focus_tabs)
{
    int d = target_dir(ch);

    if (!*tabs_focus)
    {
        if (can_focus_tabs && d && !ddx[d] && (ddy[d] < 0))
        {
            *tabs_focus = true;
            return true;
        }

        return false;
    }

    if (d)
    {
        if (ddx[d] > 0)
        {
            *page = (*page + 1) % 4;
            knowledge_set_last_page(*page);
            return true;
        }
        if (ddx[d] < 0)
        {
            *page = (*page + 3) % 4;
            knowledge_set_last_page(*page);
            return true;
        }
        if (ddy[d] > 0)
        {
            *tabs_focus = false;
            return true;
        }
        if (ddy[d] < 0)
        {
            return true;
        }
    }

    return false;
}

bool knowledge_is_recall_input(int ch)
{
    int confirm_key = steamdeck_confirm_key();

    if (ch == ' ' || ch == 'R' || ch == 'r' || ch == 'X' || ch == 'x'
        || ch == INPUT_BIND_CONFIRM)
    {
        return true;
    }

    if (confirm_key != GAMEPAD_BIND_NONE && ch == confirm_key)
        return true;

    return false;
}

static void knowledge_scene_add_tabs(app_ui_panel* panel, int page,
    bool tabs_focus)
{
    int i;

    if (!panel)
        return;

    for (i = KNOWLEDGE_PAGE_ARTEFACTS; i <= KNOWLEDGE_PAGE_CURSES; i++)
    {
        byte attr = TERM_SLATE;

        if (i == page)
            attr = tabs_focus ? TERM_YELLOW : TERM_L_BLUE;
        (void)app_ui_panel_add_tab(panel, (s16b)i, attr, i == page,
            knowledge_tab_label(i));
    }
}

app_ui_panel* knowledge_scene_begin(app_ui_scene* scene, int page,
    bool tabs_focus, cptr status)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 980, 2048);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Known lore");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "");
    knowledge_scene_add_tabs(panel, page, tabs_focus);
    if (status && status[0])
        (void)app_ui_panel_add_body_line(panel, TERM_L_BLUE, status);

    return panel;
}

void knowledge_scene_set_focus(app_ui_panel* panel, bool tabs_focus)
{
    if (!panel)
        return;

    if (tabs_focus && panel->tab_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_TABS;
        return;
    }
    if (panel->row_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->focus_id = (panel->selected_row >= 0)
            ? panel->rows[panel->selected_row].id
            : panel->rows[0].id;
    }
}

static bool knowledge_build_root_menu_scene(app_ui_scene* scene, int selected)
{
    static const struct
    {
        int id;
        cptr key;
        cptr label;
    } entries[] = {
        { 1, "1", "Display known lore browser" },
        { 2, "2", "Display supplies overview" },
        { 3, "3", "Display names of the fallen" },
        { 4, "4", "Display kill counts" },
        { 5, "5", "Display character notes file" },
        { 6, "6", "Display oath status" }
    };
    app_ui_panel* panel;
    size_t i;

    panel = knowledge_scene_begin(scene, KNOWLEDGE_PAGE_ARTEFACTS, false,
        "Browser, history, and oath records.");
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_L_WHITE, "Display current knowledge");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Choose a screen");
    panel->tab_count = 0;
    panel->body_line_count = 0;

    for (i = 0; i < N_ELEMENTS(entries); i++)
    {
        if (!app_ui_panel_add_row(panel, (s16b)entries[i].id, TERM_WHITE, true,
                (int)i == selected, entries[i].key, entries[i].label, ""))
        {
            return false;
        }
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Select");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Esc", "Back");
    knowledge_scene_set_focus(panel, false);
    return true;
}

/*
 * Display kill counts
 */
void do_cmd_knowledge_kills(void)
{
    ang_file* fff;
    char file_name[1024];
    u16b* who;
    int n;
    int i;

    /* Temporary file */
    fff = ang_file_open_temp(file_name, sizeof(file_name));

    if (!fff)
        return;

    /* Allocate the array */
    who = mem_alloc_array(z_info->r_max, u16b);

    /* Collect matching monsters */
    for (n = 0, i = 1; i < z_info->r_max - 1; i++)
    {
        monster_lore* l_ptr = &l_list[i];

        if (l_ptr->pkills > 0)
            who[n++] = i;
    }

    /* Print the monsters (highest kill counts first) */
    for (i = n - 1; i >= 0; i--)
    {
        monster_race* r_ptr = &r_info[who[i]];
        monster_lore* l_ptr = &l_list[who[i]];

        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            ang_file_printf(fff, "         %-40s\n", r_name + r_ptr->name);
        }
        else
        {
            ang_file_printf(fff, "  %5d  %-40s\n", l_ptr->pkills,
                r_name + r_ptr->name);
        }
    }

    mem_free_null(who);

    ang_file_close(fff);

    show_file(file_name, "Kill counts", 0);

    fd_kill(file_name);
}

/*
 * Interact with "knowledge"
 */
void do_cmd_knowledge(void)
{
    ui_information_scene_scope info_scope;
    char ch;
    int selected = 0;

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    if (!knowledge_enter_information_scene_or_report(&info_scope,
            "knowledge menu",
            "Knowledge menu unavailable."))
    {
        return;
    }

    /* Interact until done */
    while (1)
    {
        app_ui_scene scene;

        if (!knowledge_present_ui_scene_or_abort(&info_scope,
                knowledge_build_root_menu_scene(&scene, selected), &scene,
                "knowledge root menu", "Knowledge menu unavailable."))
        {
            goto cleanup;
        }

        ch = (char)ui_information_scene_wait_key();
        if (steamdeck_controls_active() && ch == steamdeck_back_key())
            ch = ESCAPE;

        {
            int d = target_dir(ch);

            if (d && !ddx[d] && ddy[d] < 0)
            {
                if (selected > 0)
                    selected--;
                continue;
            }
            if (d && !ddx[d] && ddy[d] > 0)
            {
                if (selected < 5)
                    selected++;
                continue;
            }
            if (ch == '\r' || ch == '\n' || ch == ' '
                || ch == INPUT_BIND_CONFIRM
                || (steamdeck_confirm_key() != GAMEPAD_BIND_NONE
                    && ch == steamdeck_confirm_key()))
            {
                ch = (char)('1' + selected);
            }
        }

        if (ch == ESCAPE)
            break;

        if (ch == '1')
        {
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            do_cmd_knowledge_browser_page(cmd_ui_knowledge_last_page());
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }
        else if (ch == '2')
        {
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            do_cmd_knowledge_supplies(NULL);
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }
        else if (ch == '3')
        {
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            show_scores_interactive(true);
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }
        else if (ch == '4')
        {
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            do_cmd_knowledge_kills();
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }
        else if (ch == '5')
        {
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            do_cmd_knowledge_notes();
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }
        else if (ch == '6')
        {
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            do_cmd_knowledge_oaths();
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }
        else
        {
            bell("Illegal command for knowledge!");
        }

        message_flush();
    }

cleanup:
    if (info_scope.active)
        ui_information_scene_leave(&info_scope);
}

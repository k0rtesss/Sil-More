/* File: cmd-ui-main-menu-help.c */
/*
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
#include "app/app-session.h"
#include "cmd-ui.h"
#include "cmd-ui-main-menu-help.h"
#include "log/log.h"
#include "sdl-config.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-semantic-scene.h"
#include "ui/ui-touch-mouse-tutorial.h"
typedef enum main_menu_help_choice {
    MAIN_MENU_HELP_CHOICE_HELP = 1,
    MAIN_MENU_HELP_CHOICE_TOUCH = 2,
    MAIN_MENU_HELP_CHOICE_MOUSE = 3,
    MAIN_MENU_HELP_CHOICE_BACK = 4
} main_menu_help_choice;

static bool main_menu_help_build_ui_scene(app_ui_scene* scene, int highlight)
{
    app_ui_panel* panel;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_HUB;
    panel->flags |= APP_UI_PANEL_FLAG_SHOW_DETAIL;
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1360);
    app_ui_panel_set_icon(panel, TERM_YELLOW, '?');
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Help and Tutorials");
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        "Choose a reference screen or replay the input tutorial.");

    if (!app_ui_panel_add_body_line(panel, TERM_SLATE,
            "Tutorials use the same semantic scene path as the rest of the UI."))
    {
        return false;
    }

    if (!app_ui_panel_add_row(panel, MAIN_MENU_HELP_CHOICE_HELP, TERM_WHITE,
            true, highlight == MAIN_MENU_HELP_CHOICE_HELP, "1", "Help",
            "Rules and commands"))
    {
        return false;
    }
    if (!app_ui_panel_add_row(panel, MAIN_MENU_HELP_CHOICE_TOUCH, TERM_WHITE,
            true, highlight == MAIN_MENU_HELP_CHOICE_TOUCH, "2",
            "Touch Tutorial", "Touch zones, pane, and profile choice"))
    {
        return false;
    }
    if (!app_ui_panel_add_row(panel, MAIN_MENU_HELP_CHOICE_MOUSE, TERM_WHITE,
            true, highlight == MAIN_MENU_HELP_CHOICE_MOUSE, "3",
            "Mouse Tutorial", "Hover, click, and context actions"))
    {
        return false;
    }
    if (!app_ui_panel_add_row(panel, MAIN_MENU_HELP_CHOICE_BACK, TERM_WHITE,
            true, highlight == MAIN_MENU_HELP_CHOICE_BACK, "4", "Back",
            "Return to the main menu"))
    {
        return false;
    }

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
        highlight == MAIN_MENU_HELP_CHOICE_HELP ? "Help"
            : highlight == MAIN_MENU_HELP_CHOICE_TOUCH ? "Touch Tutorial"
            : highlight == MAIN_MENU_HELP_CHOICE_MOUSE ? "Mouse Tutorial"
            : "Back");
    if (highlight == MAIN_MENU_HELP_CHOICE_HELP)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
            "Open the standard help browser.");
    }
    else if (highlight == MAIN_MENU_HELP_CHOICE_TOUCH)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
            "Walk through the touch layout and choose a preset.");
    }
    else if (highlight == MAIN_MENU_HELP_CHOICE_MOUSE)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
            "Walk through mouse clicks, hover, and contextual actions.");
    }
    else
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
            "Return to the main menu.");
    }

    if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Enter", "Select")
        || !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Esc", "Back"))
    {
        return false;
    }

    return true;
}

void main_menu_help_or_tutorials(void)
{
    ui_information_scene_scope scope;
    int highlight = MAIN_MENU_HELP_CHOICE_HELP;

    if (!ui_information_scene_enter(&scope))
    {
        log_warn("help chooser: semantic scene entry required");
        do_cmd_help();
        return;
    }

    while (1)
    {
        app_ui_scene scene;
        int key;

        if (!main_menu_help_build_ui_scene(&scene, highlight)
            || !ui_semantic_scene_present_and_wait_key(&scene, false, false,
                APP_WAIT_REASON_INFORMATIONAL_PAUSE, &key))
        {
            ui_information_scene_leave(&scope);
            log_warn("help chooser: semantic scene presentation failed");
            return;
        }

        if (key == ESCAPE)
        {
            highlight = MAIN_MENU_HELP_CHOICE_BACK;
            break;
        }
        if (key == '1')
        {
            highlight = MAIN_MENU_HELP_CHOICE_HELP;
            break;
        }
        if (key == '2')
        {
            highlight = MAIN_MENU_HELP_CHOICE_TOUCH;
            break;
        }
        if (key == '3')
        {
            highlight = MAIN_MENU_HELP_CHOICE_MOUSE;
            break;
        }
        if (key == '4')
        {
            highlight = MAIN_MENU_HELP_CHOICE_BACK;
            break;
        }
    }

    ui_information_scene_leave(&scope);

    switch (highlight)
    {
    case MAIN_MENU_HELP_CHOICE_HELP:
        do_cmd_help();
        break;
    case MAIN_MENU_HELP_CHOICE_TOUCH:
        if (!display_touch_tutorial())
            log_warn("touch tutorial: semantic scene presentation failed");
        break;
    case MAIN_MENU_HELP_CHOICE_MOUSE:
        if (!display_mouse_tutorial())
            log_warn("mouse tutorial: semantic scene presentation failed");
        break;
    default:
        break;
    }
}

void main_menu_show_requested_input_tutorials(void)
{
    if (platform_touch_tutorial_requested())
    {
        if (!display_touch_tutorial())
            log_warn("touch tutorial: semantic scene presentation failed");
    }

    if (platform_mouse_tutorial_requested())
    {
        if (!display_mouse_tutorial())
            log_warn("mouse tutorial: semantic scene presentation failed");
    }
}

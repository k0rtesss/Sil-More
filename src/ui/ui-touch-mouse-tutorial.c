/*
 * Copyright (C) 2026 Sil-More contributors
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

#include "platform-config.h"
#include "platform-input.h"
#include "sdl-config.h"
#include "sdl-main-internal.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-semantic-scene.h"
#include "ui/ui-touch-mouse-tutorial.h"

#define TUTORIAL_PAGE_NEXT 1
#define TUTORIAL_PAGE_BACK 2
#define TUTORIAL_PAGE_REPLAY 3

typedef struct tutorial_choice_entry {
    int profile;
    cptr title;
    cptr body;
} tutorial_choice_entry;

static void tutorial_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    ui_semantic_prompt_label(binding, fallback, buf, buflen);
}

static bool tutorial_scene_begin(app_ui_scene* scene, app_ui_panel** out_panel,
    cptr title, cptr subtitle)
{
    app_ui_panel* panel;

    if (!scene || !out_panel)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = ui_semantic_scene_begin_panel(scene,
        &(const ui_semantic_panel_config) {
            APP_UI_SCENE_FLAG_DIM_BACKDROP,
            APP_UI_LAYER_BROWSER,
            APP_UI_PANEL_STYLE_DOCUMENT,
            APP_UI_PANEL_FLAG_SHOW_DETAIL,
            TERM_YELLOW,
            TERM_SLATE,
            TERM_L_BLUE,
            920,
            1500,
            title,
            subtitle
        });
    if (!panel)
        return false;

    panel->accent_attr = TERM_L_BLUE;
    *out_panel = panel;
    return true;
}

static bool tutorial_add_lines(app_ui_panel* panel, byte attr,
    const char* const* lines, int line_count)
{
    if (!panel || !lines || line_count < 0)
        return false;

    for (int i = 0; i < line_count; i++) {
        if (!lines[i] || !lines[i][0])
            continue;
        if (!app_ui_panel_add_body_line(panel, attr, lines[i]))
            return false;
    }

    return true;
}

static bool tutorial_add_nav_footer(app_ui_panel* panel, bool steamdeck,
    cptr next_label, cptr back_label)
{
    char next_key[APP_UI_KEY_MAX];
    char back_key[APP_UI_KEY_MAX];

    if (!panel)
        return false;

    if (steamdeck)
    {
        tutorial_prompt_label(steamdeck_confirm_key(), "A", next_key,
            sizeof(next_key));
        tutorial_prompt_label(steamdeck_back_key(), "B", back_key,
            sizeof(back_key));
    }
    else
    {
        SDL_strlcpy(next_key, "Enter", sizeof(next_key));
        SDL_strlcpy(back_key, "Esc", sizeof(back_key));
    }

    return app_ui_panel_add_footer_action(panel, TUTORIAL_PAGE_NEXT,
               TERM_L_BLUE, true, next_key, next_label)
        && app_ui_panel_add_footer_action(panel, TUTORIAL_PAGE_BACK,
            TERM_WHITE, true, back_key, back_label);
}

static bool tutorial_wait_scene_key(const app_ui_scene* scene, int* out_key)
{
    if (!scene)
        return false;

    return ui_semantic_scene_present_and_wait_key(scene, false, false,
        APP_WAIT_REASON_INFORMATIONAL_PAUSE, out_key);
}

static bool tutorial_touch_choice_scene(int current_profile, int* out_choice)
{
    static const tutorial_choice_entry choices[] = {
        { SDL_TOUCH_PROFILE_TOUCH_PANE,
            "Touch pane + touch screen",
            "Visible command pad with movement and common actions on screen." },
        { SDL_TOUCH_PROFILE_CORNERS,
            "Corners + top widget",
            "Side corner movement zones and a short top command widget." },
        { SDL_TOUCH_PROFILE_ROUND_WHEEL,
            "Round wheel + top widget",
            "Radial movement with a longer top command widget." },
        { -1,
            "Start tutorial again",
            "Replay the touch tutorial before choosing a preset." },
    };
    app_ui_scene scene;
    app_ui_panel* panel;
    char subtitle[APP_UI_TEXT_MAX];
    int key;

    if (out_choice)
        *out_choice = -1;

    strnfmt(subtitle, sizeof(subtitle), "Current profile: %d",
        current_profile);
    if (!tutorial_scene_begin(&scene, &panel, "Choose Touch Preset",
            subtitle))
    {
        return false;
    }

    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->selected_row = -1;

    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Pick the layout to keep enabled, or replay the tutorial first.");

    for (int i = 0; i < (int)N_ELEMENTS(choices); i++)
    {
        char key_buf[8];
        byte attr = (choices[i].profile == current_profile)
            ? TERM_L_BLUE : TERM_WHITE;
        bool selected = (choices[i].profile == current_profile);

        if (i < 3)
            strnfmt(key_buf, sizeof(key_buf), "%d", i + 1);
        else
            SDL_strlcpy(key_buf, "4", sizeof(key_buf));

        if (!app_ui_panel_add_row(panel, (s16b)choices[i].profile, attr, true, selected,
                key_buf, choices[i].title, choices[i].body))
        {
            return false;
        }

        if (selected)
            panel->selected_row = (s16b)i;
    }

    (void)app_ui_panel_add_footer_action(panel, TUTORIAL_PAGE_NEXT,
        TERM_L_BLUE, true, "Enter", "Select");
    (void)app_ui_panel_add_footer_action(panel, TUTORIAL_PAGE_BACK,
        TERM_WHITE, true, "Esc", "Keep current");
    (void)app_ui_panel_add_footer_action(panel, TUTORIAL_PAGE_REPLAY,
        TERM_WHITE, true, "r", "Replay");

    if (!tutorial_wait_scene_key(&scene, &key))
        return false;

    if (key == ESCAPE)
    {
        if (out_choice)
            *out_choice = -1;
        return true;
    }

    if (key == '1' || key == '2' || key == '3')
    {
        if (out_choice)
        {
            switch (key)
            {
            case '1':
                *out_choice = SDL_TOUCH_PROFILE_TOUCH_PANE;
                break;
            case '2':
                *out_choice = SDL_TOUCH_PROFILE_CORNERS;
                break;
            case '3':
                *out_choice = SDL_TOUCH_PROFILE_ROUND_WHEEL;
                break;
            }
        }
        return true;
    }

    if (key == '4' || key == 'r' || key == 'R')
    {
        if (out_choice)
            *out_choice = -2;
        return true;
    }

    if (key == '\r' || key == '\n' || key == ' ')
    {
        if (out_choice)
            *out_choice = current_profile;
        return true;
    }

    return false;
}

static bool tutorial_apply_touch_profile(int profile)
{
    platform_set_touch_profile(profile);

    switch (profile)
    {
    case SDL_TOUCH_PROFILE_CORNERS:
        platform_set_touch_pane_default_open(false);
        platform_set_touch_movement_mode(SDL_TOUCH_MOVEMENT_ON);
        platform_set_touch_round_movement_enabled(false);
        platform_set_touch_zone_overlay_mode(SDL_TOUCH_ZONE_OVERLAY_MARKERS);
        platform_set_touch_corner_up_down_side(SDL_TOUCH_CORNER_UP_DOWN_RIGHT);
        platform_set_touch_top_panel_mode(SDL_TOUCH_TOP_PANEL_MODE_SHORT);
        platform_set_touch_top_panel_default_open(true);
        break;

    case SDL_TOUCH_PROFILE_ROUND_WHEEL:
        platform_set_touch_pane_default_open(false);
        platform_set_touch_movement_mode(SDL_TOUCH_MOVEMENT_OFF);
        platform_set_touch_round_movement_enabled(true);
        platform_set_touch_zone_overlay_mode(SDL_TOUCH_ZONE_OVERLAY_OFF);
        platform_set_touch_corner_up_down_side(SDL_TOUCH_CORNER_UP_DOWN_RIGHT);
        platform_set_touch_top_panel_mode(SDL_TOUCH_TOP_PANEL_MODE_LONG);
        platform_set_touch_top_panel_default_open(true);
        break;

    case SDL_TOUCH_PROFILE_TOUCH_PANE:
    default:
        platform_set_touch_pane_default_open(true);
        platform_set_touch_movement_mode(SDL_TOUCH_MOVEMENT_ON);
        platform_set_touch_round_movement_enabled(false);
        platform_set_touch_zone_overlay_mode(SDL_TOUCH_ZONE_OVERLAY_MARKERS);
        platform_set_touch_top_panel_mode(SDL_TOUCH_TOP_PANEL_MODE_SHORT);
        platform_set_touch_top_panel_default_open(false);
        break;
    }

    platform_apply_config();
    return true;
}

static void tutorial_mark_touch_seen_and_save(void)
{
    if (!platform_touch_tutorial_seen())
        platform_touch_tutorial_mark_seen();
    platform_touch_tutorial_clear_request();

    if (config_file_path[0] != '\0')
    {
        sdl_config_save(config_file_path, &config, pane_config,
            pane_config_count);
    }
}

static void tutorial_mark_mouse_seen_and_save(void)
{
    if (!platform_mouse_tutorial_seen())
        platform_mouse_tutorial_mark_seen();
    platform_mouse_tutorial_clear_request();

    if (config_file_path[0] != '\0')
    {
        sdl_config_save(config_file_path, &config, pane_config,
            pane_config_count);
    }
}

bool display_touch_tutorial(void)
{
    ui_information_scene_scope scope;
    bool steamdeck = steamdeck_controls_active();
    int page = 0;
    bool choosing_profile = false;
    const char* const touch_page_0[] = {
        "Tap highlighted regions to open game views and menus.",
        "Tap a zone to act. Hold it for an alternate binding.",
        "Swipe the edge to open or hide the touch pane.",
    };
    const char* const touch_page_1[] = {
        "Touch pane: on-screen buttons for the actions you use most.",
        "Corners: movement in the corners and commands in the top row.",
        "Round wheel: drag to a direction and pull outward for Ctrl+direction.",
    };

    if (!ui_information_scene_enter(&scope))
        return false;

    while (page >= 0 && page < 2)
    {
        app_ui_scene scene;
        app_ui_panel* panel;
        const char* const* lines = (page == 0) ? touch_page_0 : touch_page_1;
        int line_count = (page == 0) ? (int)N_ELEMENTS(touch_page_0)
                                     : (int)N_ELEMENTS(touch_page_1);
        char title[64];
        char subtitle[96];
        int key;

        strnfmt(title, sizeof(title), "Touch Tutorial");
        strnfmt(subtitle, sizeof(subtitle), "Page %d of 2", page + 1);
        if (!tutorial_scene_begin(&scene, &panel, title, subtitle))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        panel->style = APP_UI_PANEL_STYLE_DOCUMENT;
        panel->accent_attr = TERM_L_BLUE;
        if (!tutorial_add_lines(panel, TERM_WHITE, lines, line_count))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        if (page == 0)
        {
            (void)app_ui_panel_add_body_line(panel, TERM_L_BLUE,
                "If you use a touchscreen, the next page lets you choose a preset.");
        }

        if (!tutorial_add_nav_footer(panel, steamdeck,
                (page == 1) ? "Choose profile" : "Next",
                (page == 0) ? "Close" : "Back"))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        if (!tutorial_wait_scene_key(&scene, &key))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        if (key == ESCAPE)
        {
            if (page == 0)
                break;
            page--;
            continue;
        }

        if (key == '\r' || key == '\n' || key == ' ')
        {
            if (page == 0)
            {
                page++;
                continue;
            }
            choosing_profile = true;
            break;
        }

        page++;
    }

    if (choosing_profile)
    {
        int profile = platform_touch_profile();
        int choice = -1;

        if (tutorial_touch_choice_scene(profile, &choice))
        {
            if (choice == -2)
            {
                ui_information_scene_leave(&scope);
                return display_touch_tutorial();
            }
            if (choice >= 0)
            {
                (void)tutorial_apply_touch_profile(choice);
                tutorial_mark_touch_seen_and_save();
                ui_information_scene_leave(&scope);
                return true;
            }
        }

        tutorial_mark_touch_seen_and_save();
        ui_information_scene_leave(&scope);
        return true;
    }

    tutorial_mark_touch_seen_and_save();
    ui_information_scene_leave(&scope);
    return true;
}

bool display_mouse_tutorial(void)
{
    ui_information_scene_scope scope;
    bool steamdeck = steamdeck_controls_active();
    int page = 0;
    int key;
    const char* const mouse_page_0[] = {
        "Click highlighted regions to open views and menus.",
        "Left-click the map to move to an explored square or select a target.",
        "Right-click opens contextual actions, look, or special movement choices.",
    };
    const char* const mouse_page_1[] = {
        "Hover can reveal extra detail, and long-press behaves like a hold.",
        "Menu rows and footer actions are clickable.",
        "Mouse movement can be changed later in Input or mouse settings.",
    };

    if (!ui_information_scene_enter(&scope))
        return false;

    while (page >= 0 && page < 2)
    {
        app_ui_scene scene;
        app_ui_panel* panel;
        const char* const* lines = (page == 0) ? mouse_page_0 : mouse_page_1;
        int line_count = (page == 0) ? (int)N_ELEMENTS(mouse_page_0)
                                     : (int)N_ELEMENTS(mouse_page_1);
        char subtitle[96];

        strnfmt(subtitle, sizeof(subtitle), "Page %d of 2", page + 1);
        if (!tutorial_scene_begin(&scene, &panel, "Mouse Tutorial", subtitle))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        panel->style = APP_UI_PANEL_STYLE_DOCUMENT;
        panel->accent_attr = TERM_L_BLUE;
        if (!tutorial_add_lines(panel, TERM_WHITE, lines, line_count))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        if (!tutorial_add_nav_footer(panel, steamdeck,
                (page == 1) ? "Done" : "Next",
                (page == 0) ? "Close" : "Back"))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        if (!tutorial_wait_scene_key(&scene, &key))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        if (key == ESCAPE)
        {
            if (page == 0)
                break;
            page--;
            continue;
        }

        if (key == '\r' || key == '\n' || key == ' ')
        {
            if (page == 0)
            {
                page++;
                continue;
            }

            break;
        }

        page++;
    }

    tutorial_mark_mouse_seen_and_save();
    ui_information_scene_leave(&scope);
    return true;
}

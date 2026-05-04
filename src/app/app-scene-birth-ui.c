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

#include "app-scene-birth.h"
#include "app-scene-birth-ui-internal.h"

#include "app/app-session.h"
#include "app/app-ui.h"
#include "blitz.h"
#include "log/log.h"
#include "platform-input.h"
#include "sdl-config.h"
#include "platform-story-font.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-semantic-scene.h"
#include "ui/ui-touch-mouse-tutorial.h"

typedef struct birth_menu {
    bool ghost;
    cptr name;
    cptr text;
} birth_menu;

typedef struct birth_menu_scene_scope {
    bool active;
    app_wait_scope wait_scope;
} birth_menu_scene_scope;

#define INVALID_CHOICE 255

static bool oath_option_selectable(int oath_id, int available_mask);

static const char *character_ability_names[S_MAX][ABILITIES_MAX] =
{
    [S_MEL] = {
        [MEL_POWER] = "Power",
        [MEL_FINESSE] = "Finesse",
        [MEL_KNOCK_BACK] = "Knock Back",
        [MEL_THROWING] = "Throwing",
        [MEL_POLEARMS] = "Polearm Mastery",
        [MEL_CHARGE] = "Charge",
        [MEL_FOLLOW_THROUGH] = "Follow-Through",
        [MEL_IMPALE] = "Impale",
        [MEL_CONTROL] = "Subtlety",
        [MEL_WHIRLWIND_ATTACK] = "Whirlwind Attack",
        [MEL_ZONE_OF_CONTROL] = "Zone of Control",
        [MEL_SMITE] = "Smite",
        [MEL_TWO_WEAPON] = "Two Weapon Fighting",
        [MEL_RAPID_ATTACK] = "Rapid Attack",
        [MEL_STR] = NULL,
    },
    [S_ARC] = {
        [ARC_ROUT] = "Rout",
        [ARC_FLETCHERY] = "Fletchery",
        [ARC_POINT_BLANK] = "Point Blank Archery",
        [ARC_PUNCTURE] = "Puncture",
        [ARC_AMBUSH] = "Ambush",
        [ARC_VERSATILITY] = "Versatility",
        [ARC_CRIPPLING] = "Crippling Shot",
        [ARC_DEADLY_HAIL] = "Deadly Hail",
        [ARC_DEX] = NULL,
    },
    [S_EVN] = {
        [EVN_DODGING] = "Dodging",
        [EVN_BLOCKING] = "Blocking",
        [EVN_PARRY] = "Parry",
        [EVN_CROWD_FIGHTING] = "Crowd Fighting",
        [EVN_LEAPING] = "Leaping",
        [EVN_SPRINTING] = "Sprinting",
        [EVN_FLANKING] = "Flanking",
        [EVN_HEAVY_ARMOUR] = "Heavy Armour Use",
        [EVN_RIPOSTE] = "Riposte",
        [EVN_CONTROLLED_RETREAT] = "Controlled Retreat",
        [EVN_DEX] = NULL,
    },
    [S_STL] = {
        [STL_DISGUISE] = "Disguise",
        [STL_ASSASSINATION] = "Assassination",
        [STL_CRUEL_BLOW] = "Cruel Blow",
        [STL_EXCHANGE_PLACES] = "Exchange Places",
        [STL_OPPORTUNIST] = "Opportunist",
        [STL_VANISH] = "Vanish",
        [STL_DEX] = NULL,
    },
    [S_PER] = {
        [PER_QUICK_STUDY] = "Quick Study",
        [PER_FOCUSED_ATTACK] = "Focused Attack",
        [PER_KEEN_SENSES] = "Keen Senses",
        [PER_CONCENTRATION] = "Concentration",
        [PER_ALCHEMY] = "Alchemy",
        [PER_BANE] = "Bane",
        [PER_OUTWIT] = "Outwit",
        [PER_LISTEN] = "Resonance",
        [PER_MASTER_HUNTER] = "Master Hunter",
        [PER_GRA] = NULL,
    },
    [S_WIL] = {
        [WIL_CURSE_BREAKING] = "Curse Breaking",
        [WIL_CHANNELING] = "Channeling",
        [WIL_STRENGTH_IN_ADVERSITY] = "Strength in Adversity",
        [WIL_FORMIDABLE] = "Formidable",
        [WIL_INNER_LIGHT] = "Inner Light",
        [WIL_INDOMITABLE] = "Indomitable",
        [WIL_OATH] = "Oath",
        [WIL_POISON_RESISTANCE] = "Poison Resistance",
        [WIL_VENGEANCE] = "Vengeance",
        [WIL_MAJESTY] = "Majesty",
        [WIL_CON] = NULL,
    },
    [S_SMT] = {
        [SMT_WEAPONSMITH] = "Weaponsmith",
        [SMT_ARMOURSMITH] = "Armoursmith",
        [SMT_JEWELLER] = "Jeweller",
        [SMT_ENCHANTMENT] = "Enchantment",
        [SMT_EXPERTISE] = "Expertise",
        [SMT_ARTEFACT] = "Artifice",
        [SMT_MASTERPIECE] = "Masterpiece",
        [SMT_ALLOY_MASTERY] = "Alloy mastery",
        [SMT_GRA] = NULL,
    },
    [S_SNG] = {
        [SNG_ELBERETH] = "Song of Elbereth",
        [SNG_CHALLENGE] = "Song of Challenge",
        [SNG_DELVINGS] = "Song of Delvings",
        [SNG_FREEDOM] = "Song of Freedom",
        [SNG_SILENCE] = "Song of Silence",
        [SNG_STAUNCHING] = "Song of Staunching",
        [SNG_THRESHOLDS] = "Song of Thresholds",
        [SNG_TREES] = "Song of the Trees",
        [SNG_REVEALING] = "Song of Revealing",
        [SNG_WOVEN_THEMES] = "Woven Themes",
        [SNG_SLAYING] = "Song of Slaying",
        [SNG_ELVENESS] = "Song of Elveness",
        [SNG_STAYING] = "Song of Staying",
        [SNG_DISGUISE] = "Song of Disguise",
        [SNG_LORIEN] = "Song of Lórien",
        [SNG_SHATTERING] = "Song of Shattering",
        [SNG_MASTERY] = "Song of Mastery",
        [SNG_CONTEST] = "Song of Contest",
        [SNG_LAMENT] = "Song of Lament",
        [SNG_GRA] = NULL,
    },
    [S_SPC] = {
        [SPC_MANDOS] = "Mandos' Doom",
        [SPC_AULE] = "Aulë's Forge",
        [SPC_OATH_MERCY] = "Oath of Mercy",
        [SPC_OATH_SILENCE] = "Oath of Silence",
        [SPC_OATH_IRON] = "Oath of Iron",
        [SPC_NIENA_MERCY] = "Niena's Gift of Mercy",
        [SPC_OATH_SMITH] = "Oath of the Smith",
        [SPC_OATH_VALOROUS] = "Oath of the Valorous Heart",
        [SPC_UNIQUE_BANE] = "Unique Bane",
        [SPC_OATH_LIGHT] = "Oath of Light",
    },
};

static bool birth_menu_scene_enter(birth_menu_scene_scope* scope, u16b reason)
{
    app_session *session;

    if (!scope)
        return false;

    memset(scope, 0, sizeof(*scope));
    session = app_session_current();
    if (!session
        || !app_session_has_flag(session, APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT))
    {
        return false;
    }

    app_session_push_wait_scope(session, &scope->wait_scope, reason, 0, 0);
    app_session_clear_inputs(session);
    scope->active = true;
    return true;
}

static void birth_menu_scene_leave(birth_menu_scene_scope* scope)
{
    app_session *session = app_session_current();

    if (!scope || !scope->active || !session)
        return;

    app_session_clear_inputs(session);
    app_session_clear_menu_snapshot(session);
    app_session_set_snapshot(session, NULL);
    app_session_pop_wait_scope(session, &scope->wait_scope);
    scope->active = false;
}

static void birth_show_input_tutorials(void)
{
    bool touch_requested = platform_touch_tutorial_requested();
    bool mouse_requested = platform_mouse_tutorial_requested();
    bool portable = portable_controls_active();

    if (touch_requested || (portable && !platform_touch_tutorial_seen()))
    {
        if (!display_touch_tutorial())
            log_warn("touch tutorial: semantic scene presentation failed");
    }

    if (mouse_requested || (!portable && !platform_mouse_tutorial_seen()))
    {
        if (!display_mouse_tutorial())
            log_warn("mouse tutorial: semantic scene presentation failed");
    }
}

static void birth_menu_clear_pending_input(void)
{
    app_session *session = app_session_current();

    if (session)
        app_session_clear_inputs(session);
}

static void birth_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    ui_semantic_prompt_label(binding, fallback, buf, buflen);
}

static bool birth_confirm_input(int ch, bool steamdeck)
{
    if (ch == '\r' || ch == '\n' || ch == ' ' || ch == INPUT_BIND_CONFIRM)
        return true;

    if (steamdeck && ch == steamdeck_confirm_key())
        return true;

    return false;
}

static char birth_ui_direction_command_key(const app_ui_command* command)
{
    if (!command)
        return '\0';

    if (command->kind == APP_UI_COMMAND_KIND_SCROLL)
    {
        if (ABS(command->scroll_y) >= ABS(command->scroll_x)
            && command->scroll_y != 0)
        {
            return (command->scroll_y > 0) ? '8' : '2';
        }
        if (command->scroll_x != 0)
            return (command->scroll_x < 0) ? '4' : '6';
    }
    if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
    {
        if (ABS(command->dy) >= ABS(command->dx) && command->dy != 0)
            return (command->dy < 0) ? '8' : '2';
        if (command->dx != 0)
            return (command->dx < 0) ? '4' : '6';
    }

    return '\0';
}

static bool birth_ui_command_cancelled(const app_ui_command* command)
{
    return command
        && (command->kind == APP_UI_COMMAND_KIND_CANCEL
            || command->target.action == APP_UI_WIDGET_ACTION_CANCEL);
}

static bool birth_ui_command_activated(const app_ui_command* command)
{
    return command
        && (command->kind == APP_UI_COMMAND_KIND_ACTIVATE
            || command->kind == APP_UI_COMMAND_KIND_SELECT
            || command->target.action == APP_UI_WIDGET_ACTION_ACTIVATE
            || command->target.action == APP_UI_WIDGET_ACTION_SELECT);
}

static bool birth_ui_wait_event(ui_information_scene_event* event, u16b reason)
{
    return ui_information_scene_wait_event_with_wait_reason(event, 0, reason,
        true);
}

static char birth_wait_assignment_review_key(void)
{
    ui_information_scene_event event;

    while (birth_ui_wait_event(&event, APP_WAIT_REASON_CONFIRM))
    {
        const app_ui_command* command;

        if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            return (char)event.key;
        if (event.kind != UI_INFORMATION_SCENE_EVENT_COMMAND)
            continue;

        command = &event.command;
        if (birth_ui_command_cancelled(command))
            return ESCAPE;
        if (command->target.role == APP_UI_WIDGET_ROLE_BUTTON)
        {
            if (command->target.widget_id == 1)
                return '\r';
            if (command->target.widget_id == 2)
                return ESCAPE;
        }
        if (birth_ui_command_activated(command))
            return '\r';
    }

    return ESCAPE;
}

static char birth_wait_selection_key(int* cur, int num,
    bool allow_full_description_screen)
{
    ui_information_scene_event event;

    while (birth_ui_wait_event(&event, APP_WAIT_REASON_LIST_SELECTION))
    {
        const app_ui_command* command;

        if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            return (char)event.key;
        if (event.kind != UI_INFORMATION_SCENE_EVENT_COMMAND)
            continue;

        command = &event.command;
        if (birth_ui_command_cancelled(command))
            return ESCAPE;
        if (command->kind == APP_UI_COMMAND_KIND_SCROLL
            || command->kind == APP_UI_COMMAND_KIND_FOCUS)
        {
            char dir_key = birth_ui_direction_command_key(command);

            if (dir_key)
                return dir_key;
        }
        if (command->target.role == APP_UI_WIDGET_ROLE_LIST_ITEM)
        {
            int choice = command->target.widget_id;

            if (choice >= 0 && choice < num && cur)
                *cur = choice;
            if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
                return '\0';
            if (allow_full_description_screen
                && (command->kind == APP_UI_COMMAND_KIND_INSPECT
                    || command->kind == APP_UI_COMMAND_KIND_CONTEXT
                    || command->target.action == APP_UI_WIDGET_ACTION_INSPECT))
            {
                return 'f';
            }
            if (birth_ui_command_activated(command))
                return '\r';
            return '\0';
        }
        if (command->target.role == APP_UI_WIDGET_ROLE_BUTTON)
        {
            switch (command->target.widget_id)
            {
            case 1:
                return '\r';
            case 2:
                return ESCAPE;
            case 3:
                return 'r';
            case 4:
                return allow_full_description_screen ? 'f' : '\0';
            case 5:
                return 'o';
            case 6:
                return 's';
            case 7:
                return 'h';
            case 8:
                return 'q';
            default:
                break;
            }
        }
    }

    return ESCAPE;
}

static char birth_wait_oath_key(int* highlight, int available_mask)
{
    ui_information_scene_event event;

    while (birth_ui_wait_event(&event, APP_WAIT_REASON_LIST_SELECTION))
    {
        const app_ui_command* command;

        if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            return (char)event.key;
        if (event.kind != UI_INFORMATION_SCENE_EVENT_COMMAND)
            continue;

        command = &event.command;
        if (birth_ui_command_cancelled(command))
            return ESCAPE;
        if (command->kind == APP_UI_COMMAND_KIND_SCROLL
            || command->kind == APP_UI_COMMAND_KIND_FOCUS)
        {
            char dir_key = birth_ui_direction_command_key(command);

            if (dir_key)
                return dir_key;
        }
        if (command->target.role == APP_UI_WIDGET_ROLE_LIST_ITEM)
        {
            int oath_id = command->target.widget_id;

            if (oath_option_selectable(oath_id, available_mask) && highlight)
                *highlight = oath_id;
            if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
                return '\0';
            if (birth_ui_command_activated(command))
                return '\r';
            return '\0';
        }
        if (command->target.role == APP_UI_WIDGET_ROLE_BUTTON)
        {
            if (command->target.widget_id == 1)
                return '\r';
            if (command->target.widget_id == 2)
                return ESCAPE;
        }
    }

    return ESCAPE;
}

static char birth_wait_allocation_key(int* selected, int count,
    bool skip_special, bool include_quit)
{
    ui_information_scene_event event;

    while (birth_ui_wait_event(&event, APP_WAIT_REASON_LIST_SELECTION))
    {
        const app_ui_command* command;

        if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            return (char)event.key;
        if (event.kind != UI_INFORMATION_SCENE_EVENT_COMMAND)
            continue;

        command = &event.command;
        if (birth_ui_command_cancelled(command))
            return ESCAPE;
        if (command->kind == APP_UI_COMMAND_KIND_SCROLL
            || command->kind == APP_UI_COMMAND_KIND_FOCUS)
        {
            char dir_key = birth_ui_direction_command_key(command);

            if (dir_key)
                return dir_key;
        }
        if (command->target.role == APP_UI_WIDGET_ROLE_LIST_ITEM)
        {
            int row_id = command->target.widget_id;

            if (selected && row_id >= 0 && row_id < count
                && (!skip_special || row_id != S_SPC))
            {
                *selected = row_id;
            }
            return '\0';
        }
        if (command->target.role == APP_UI_WIDGET_ROLE_BUTTON)
        {
            switch (command->target.widget_id)
            {
            case 1:
                return '\r';
            case 2:
                return ESCAPE;
            case 3:
                return '4';
            case 4:
                return '6';
            case 5:
                return include_quit ? 'q' : '\0';
            default:
                break;
            }
        }
    }

    return ESCAPE;
}

static void birth_trimmed_stat_label(int stat, char* buf, size_t buflen)
{
    const char *label;
    size_t len;

    if (!buf || !buflen)
        return;

    label = (stat >= 0 && stat < A_MAX) ? stat_names[stat] : "";
    SDL_strlcpy(buf, label ? label : "", buflen);
    len = strlen(buf);
    while (len > 0 && buf[len - 1] == ' ')
        buf[--len] = '\0';
}

static void birth_build_stats_prompt(bool steamdeck, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];
        char quit_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        birth_prompt_label('q', "Start", quit_label, sizeof(quit_label));
        strnfmt(buf, buflen, "D-pad allocate  %s back  %s confirm  %s quit",
            back_label, confirm_label, quit_label);
        return;
    }

    strnfmt(buf, buflen,
        "Arrows allocate  ESC back  SPACE/ENTER confirm  q quit");
}

static void birth_build_skills_prompt(bool steamdeck, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];
        char quit_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        birth_prompt_label('q', "q", quit_label, sizeof(quit_label));
        strnfmt(buf, buflen, "D-pad allocate  %s back  %s confirm  %s quit",
            back_label, confirm_label, quit_label);
        return;
    }

    strnfmt(buf, buflen,
        "Arrows allocate  ESC back  SPACE/ENTER confirm  q quit");
}

static void birth_build_review_prompt(bool steamdeck, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(buf, buflen, "%s back to assignment  %s continue", back_label,
            confirm_label);
        return;
    }

    strnfmt(buf, buflen, "ESC back to assignment  SPACE/ENTER continue");
}

static bool birth_build_stats_allocation_ui_scene(app_ui_scene* scene,
    const int stats[A_MAX], int selected_stat, int points_left, bool steamdeck)
{
    app_ui_panel *panel;
    char prompt[160];
    char subtitle[64];
    int i;

    if (!scene || !stats)
        return false;

    birth_build_stats_prompt(steamdeck, prompt, sizeof(prompt));
    if (!build_character_sheet_ui_scene(scene, prompt))
        return false;

    strnfmt(subtitle, sizeof(subtitle), "Points Left: %d", points_left);
    panel = ui_semantic_scene_append_panel(scene,
        &(const ui_semantic_panel_config) {
            0,
            APP_UI_LAYER_MODAL,
            APP_UI_PANEL_STYLE_BROWSER,
            0,
            TERM_L_BLUE,
            TERM_L_GREEN,
            TERM_L_BLUE,
            420,
            560,
            "Allocate Stats",
            subtitle
        });
    if (!panel)
        return false;

    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->selected_row = selected_stat;

    for (i = 0; i < A_MAX; i++)
    {
        char label[32];
        char meta[16];
        bool selected = (i == selected_stat);

        birth_trimmed_stat_label(i, label, sizeof(label));
        strnfmt(meta, sizeof(meta), "%d", birth_stat_cost(stats[i]));
        if (!app_ui_panel_add_row(panel, i, selected ? TERM_L_BLUE : TERM_WHITE,
                true, selected, "", label, meta))
        {
            return false;
        }
    }

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                confirm_label, "Confirm")
            && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Back")
            && app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "4", "Lower")
            && app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                "6", "Raise")
            && app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
                "q", "Quit");
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Enter", "Confirm")
        && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Esc", "Back")
        && app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "4", "Lower")
        && app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "6", "Raise")
        && app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
            "q", "Quit");
}

static bool birth_present_stats_allocation_ui_scene(const int stats[A_MAX],
    int selected_stat, int points_left, bool steamdeck)
{
    app_ui_scene scene;

    if (!birth_build_stats_allocation_ui_scene(&scene, stats, selected_stat,
            points_left, steamdeck)
        || !ui_information_scene_present_ui(&scene))
    {
        log_warn("birth stats allocation: semantic scene presentation failed");
        return false;
    }

    return true;
}

static bool birth_build_skills_allocation_ui_scene(app_ui_scene* scene,
    int selected_skill, const int old_base[S_MAX], const int skill_gain[S_MAX],
    int exp_left, bool steamdeck)
{
    app_ui_panel *panel;
    char prompt[160];
    char subtitle[64];
    int i;
    int selected_row = 0;
    int row_index = 0;

    if (!scene || !old_base || !skill_gain)
        return false;

    birth_build_skills_prompt(steamdeck, prompt, sizeof(prompt));
    if (!build_character_sheet_ui_scene(scene, prompt))
        return false;

    strnfmt(subtitle, sizeof(subtitle), "Experience Left: %d", exp_left);
    panel = ui_semantic_scene_append_panel(scene,
        &(const ui_semantic_panel_config) {
            0,
            APP_UI_LAYER_MODAL,
            APP_UI_PANEL_STYLE_BROWSER,
            0,
            TERM_L_BLUE,
            TERM_L_GREEN,
            TERM_L_BLUE,
            420,
            640,
            "Allocate Skills",
            subtitle
        });
    if (!panel)
        return false;

    panel->focus_area = APP_UI_FOCUS_ROWS;

    for (i = 0; i < S_MAX; i++)
    {
        char meta[16];
        bool selected;

        if (i == S_SPC)
            continue;

        selected = (i == selected_skill);
        if (selected)
            selected_row = row_index;
        strnfmt(meta, sizeof(meta), "%d",
            birth_skill_cost(old_base[i], skill_gain[i]));
        if (!app_ui_panel_add_row(panel, i, selected ? TERM_L_BLUE : TERM_WHITE,
                true, selected, "", skill_names_full[i], meta))
        {
            return false;
        }
        row_index++;
    }

    panel->selected_row = selected_row;
    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                confirm_label, "Confirm")
            && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Back")
            && app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "4", "Lower")
            && app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                "6", "Raise")
            && app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
                "q", "Quit");
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Enter", "Confirm")
        && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Esc", "Back")
        && app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "4", "Lower")
        && app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "6", "Raise")
        && app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
            "q", "Quit");
}

static bool birth_present_skills_allocation_ui_scene(int selected_skill,
    const int old_base[S_MAX], const int skill_gain[S_MAX], int exp_left,
    bool steamdeck)
{
    app_ui_scene scene;

    if (!birth_build_skills_allocation_ui_scene(&scene, selected_skill,
            old_base, skill_gain, exp_left, steamdeck)
        || !ui_information_scene_present_ui(&scene))
    {
        log_warn("birth skills allocation: semantic scene presentation failed");
        return false;
    }

    return true;
}

static bool birth_build_assignment_review_ui_scene(app_ui_scene* scene,
    bool steamdeck)
{
    app_ui_panel *panel;
    char prompt[160];

    if (!scene)
        return false;

    birth_build_review_prompt(steamdeck, prompt, sizeof(prompt));
    if (!build_character_sheet_ui_scene(scene, prompt))
        return false;

    panel = ui_semantic_scene_append_panel(scene,
        &(const ui_semantic_panel_config) {
            0,
            APP_UI_LAYER_MODAL,
            APP_UI_PANEL_STYLE_PLAIN,
            0,
            TERM_L_BLUE,
            0,
            TERM_L_BLUE,
            420,
            560,
            "Character Review",
            NULL
        });
    if (!panel)
        return false;

    if (!app_ui_panel_add_body_line(panel, TERM_WHITE,
            "Review the character sheet before you start."))
    {
        return false;
    }
    if (!app_ui_panel_add_body_line(panel, TERM_SLATE,
            "Continue to start, or go back to adjust your assignments."))
    {
        return false;
    }

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                confirm_label, "Continue")
            && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Back");
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Enter", "Continue")
        && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Esc", "Back");
}

static bool birth_show_semantic_assignment_review(bool steamdeck)
{
    char ch;

    while (1)
    {
        app_ui_scene scene;

        if (!birth_build_assignment_review_ui_scene(&scene, steamdeck)
            || !ui_information_scene_present_ui(&scene))
        {
            log_warn("birth assignment review: semantic scene presentation failed");
            return false;
        }

        ch = birth_wait_assignment_review_key();

        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;

        if ((ch == ESCAPE) || (ch == '4') || (ch == 'q') || (ch == 'Q'))
            return false;

        if (birth_confirm_input(ch, steamdeck) || (ch == '6'))
            return true;
    }
}

static bool birth_ui_panel_add_wrapped_lines(app_ui_panel* panel, byte attr,
    cptr text, bool detail_lines)
{
    char line_buffer[APP_UI_TEXT_MAX];
    int line_pos = 0;
    const char *text_ptr = text;
    int max_width = APP_UI_TEXT_MAX - 1;

    if (!panel || !text || !text[0])
        return true;

    while (*text_ptr)
    {
        while (*text_ptr == ' ' && line_pos == 0)
            text_ptr++;

        if (*text_ptr == '\n')
        {
            line_buffer[line_pos] = '\0';
            if (line_pos > 0)
            {
                if (detail_lines)
                {
                    if (!app_ui_panel_add_detail_line(panel, attr, line_buffer))
                        return false;
                }
                else if (!app_ui_panel_add_body_line(panel, attr, line_buffer))
                {
                    return false;
                }
            }
            else if (detail_lines)
            {
                if (!app_ui_panel_add_detail_line(panel, attr, " "))
                    return false;
            }
            else if (!app_ui_panel_add_body_line(panel, attr, " "))
            {
                return false;
            }

            line_pos = 0;
            text_ptr++;
            continue;
        }

        if (line_pos >= max_width)
        {
            int wrap_pos = line_pos - 1;

            while (wrap_pos > 0 && line_buffer[wrap_pos] != ' ')
                wrap_pos--;

            if (wrap_pos > 0)
            {
                int remaining = line_pos - wrap_pos - 1;

                line_buffer[wrap_pos] = '\0';
                if (detail_lines)
                {
                    if (!app_ui_panel_add_detail_line(panel, attr, line_buffer))
                        return false;
                }
                else if (!app_ui_panel_add_body_line(panel, attr, line_buffer))
                {
                    return false;
                }

                for (int i = 0; i < remaining; i++)
                    line_buffer[i] = line_buffer[wrap_pos + 1 + i];
                line_pos = remaining;
            }
            else
            {
                line_buffer[line_pos] = '\0';
                if (detail_lines)
                {
                    if (!app_ui_panel_add_detail_line(panel, attr, line_buffer))
                        return false;
                }
                else if (!app_ui_panel_add_body_line(panel, attr, line_buffer))
                {
                    return false;
                }
                line_pos = 0;
            }

            continue;
        }

        line_buffer[line_pos++] = *text_ptr++;
    }

    if (line_pos > 0)
    {
        line_buffer[line_pos] = '\0';
        if (detail_lines)
            return app_ui_panel_add_detail_line(panel, attr, line_buffer);
        return app_ui_panel_add_body_line(panel, attr, line_buffer);
    }

    return true;
}

static int character_choice_index_by_name(cptr choice_name)
{
    int character_idx;

    if (!choice_name)
        return -1;

    for (character_idx = 0; character_idx < z_info->c_max; character_idx++)
    {
        if (!strcmp(choice_name, c_name + c_info[character_idx].name))
            return character_idx;
    }

    return -1;
}

static cptr character_selection_header_text(bool character_phase)
{
    (void)character_phase;
    return "Character Selection:";
}

static void birth_choice_full_name(birth_menu choice, char* full_name,
    size_t full_name_len)
{
    int character_idx;

    if (!full_name || full_name_len == 0)
        return;

    character_idx = character_choice_index_by_name(choice.name);
    if (character_idx >= 0)
    {
        strnfmt(full_name, full_name_len, "%s%s",
            c_name + c_info[character_idx].name,
            c_name + c_info[character_idx].alt_name);
    }
    else
    {
        strnfmt(full_name, full_name_len, "%s",
            choice.name ? choice.name : "");
    }
}

static bool birth_description_scene_add_footer(app_ui_panel* panel)
{
    if (!panel)
        return false;

    if (steamdeck_controls_active())
    {
        char back_label[16];

        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            back_label, "Return");
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Any key", "Return");
}

static bool birth_build_description_ui_scene(app_ui_scene* scene,
    cptr title, cptr text)
{
    app_ui_panel *panel;
    byte story = platform_story_font_enabled() ? STORY_FLAG_USE : 0;

    if (!scene)
        return false;

    panel = ui_semantic_scene_begin_plain(scene,
        APP_UI_SCENE_FLAG_USE_BACKDROP | APP_UI_SCENE_FLAG_DIM_BACKDROP,
        APP_UI_LAYER_MODAL, TERM_L_BLUE, title, 0, NULL, TERM_L_BLUE, 900,
        1600);
    if (!panel)
        return false;
    if (text && text[0])
    {
        if (!app_ui_panel_begin_rich_paragraph(scene, panel))
            return false;
        if (!app_ui_panel_add_rich_text_ex(scene, panel, TERM_WHITE, story,
                text))
        {
            return false;
        }
    }

    return birth_description_scene_add_footer(panel);
}

static bool birth_show_description_ui_scene(birth_menu choice)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    char full_name[64];

    if (!ui_information_scene_enter(&scope))
    {
        log_warn("birth description: semantic scene unavailable");
        return false;
    }

    birth_choice_full_name(choice, full_name, sizeof(full_name));
    if (!birth_build_description_ui_scene(&scene, full_name, choice.text)
        || !ui_semantic_scene_present_and_wait_key(&scene, true, false,
            APP_WAIT_REASON_NONE, NULL))
    {
        ui_information_scene_leave(&scope);
        log_warn("birth description: semantic scene presentation failed");
        return false;
    }
    ui_information_scene_leave(&scope);
    return true;
}

static int collect_character_starting_abilities(int character, cptr out[],
    int out_max)
{
    int count = 0;

    if (character <= 0)
        return 0;

    if (c_info[character].flags_u & UNQ_MIM)
        return 0;

    for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
    {
        int stat = c_info[character].a_adj[slot][0];
        int ability = c_info[character].a_adj[slot][1];
        cptr name;

        if (stat < 0)
            break;

        if (stat >= S_MAX || ability < 0 || ability >= ABILITIES_MAX)
            continue;

        name = character_ability_names[stat][ability];
        if (!name)
            continue;

        if (out && count < out_max)
            out[count] = name;

        count++;
    }

    return count;
}

static void birth_selection_row_key(int index, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (index >= 0 && index < 26)
    {
        strnfmt(buf, buflen, "%c", I2A(index));
        return;
    }
    if (index >= 26 && index < 52)
    {
        strnfmt(buf, buflen, "%c", (char)('A' + (index - 26)));
        return;
    }

    buf[0] = '\0';
}

static void birth_character_power_stars(int character_idx, char* buf,
    size_t buflen, byte* attr)
{
    byte power = 1;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    if (attr)
        *attr = TERM_WHITE;

    if (character_idx >= 0 && character_idx < z_info->c_max)
        power = c_info[character_idx].power;

    switch (power)
    {
    case 0:
        if (attr)
            *attr = TERM_RED;
        strnfmt(buf, buflen, "*");
        break;
    case 1:
        if (attr)
            *attr = TERM_WHITE;
        strnfmt(buf, buflen, "**");
        break;
    case 2:
        if (attr)
            *attr = TERM_GREEN;
        strnfmt(buf, buflen, "***");
        break;
    case 3:
    case 4:
        if (attr)
            *attr = TERM_L_GREEN;
        strnfmt(buf, buflen, "***");
        break;
    default:
        if (attr)
            *attr = TERM_WHITE;
        strnfmt(buf, buflen, "**");
        break;
    }
}

static bool birth_selection_add_race_detail_lines(app_ui_panel* panel,
    birth_menu choice)
{
    int race;

    if (!panel)
        return false;

    for (race = 0; race < z_info->p_max; race++)
    {
        if (!strcmp(choice.name, p_name + p_info[race].name))
            break;
    }
    if (race >= z_info->p_max)
        return false;

    for (int i = 0; i < A_MAX; i++)
    {
        char line[64];
        int adj = p_info[race].r_adj[i];
        byte attr = TERM_L_DARK;

        if (adj < 0)
            attr = TERM_RED;
        else if (adj == 1)
            attr = TERM_GREEN;
        else if (adj == 2)
            attr = TERM_L_GREEN;
        else if (adj > 2)
            attr = TERM_L_BLUE;

        strnfmt(line, sizeof(line), "%s %+d", stat_names[i], adj);
        if (!app_ui_panel_add_detail_line(panel, attr, line))
            return false;
    }

    if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
        return true;
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
        return false;
    return birth_ui_panel_add_wrapped_lines(panel, TERM_WHITE, choice.text,
        true);
}

static bool birth_selection_add_character_detail_lines(app_ui_panel* panel,
    birth_menu choice)
{
    int character_idx = character_choice_index_by_name(choice.name);
    birth_compact_flag_line trait_lines[64];
    cptr ability_lines[CHARACTER_ABILITY_MAX];
    int ability_count;
    int trait_max_len = 0;
    int trait_count;

    if (!panel || character_idx < 0 || character_idx >= z_info->c_max)
        return false;

    for (int i = 0; i < A_MAX; i++)
    {
        char line[64];
        int adj = c_info[character_idx].h_adj[i] + rp_ptr->r_adj[i]
            + birth_curses_stat_adj(i);
        byte attr = TERM_L_DARK;

        if (adj < 0)
            attr = TERM_RED;
        else if (adj == 1)
            attr = TERM_GREEN;
        else if (adj == 2)
            attr = TERM_L_GREEN;
        else if (adj > 2)
            attr = TERM_L_BLUE;

        strnfmt(line, sizeof(line), "%s %+d", stat_names[i], adj);
        if (!app_ui_panel_add_detail_line(panel, attr, line))
            return false;
    }

    if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
        return true;
    ability_count = collect_character_starting_abilities(character_idx,
        ability_lines, (int)N_ELEMENTS(ability_lines));
    if (ability_count > 0)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " ")
            || !app_ui_panel_add_detail_line(panel, TERM_L_BLUE,
                "Starting abilities"))
        {
            return false;
        }
        for (int i = 0; i < ability_count; i++)
        {
            if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
                return true;
            if (!app_ui_panel_add_detail_line(panel, TERM_YELLOW,
                    ability_lines[i]))
            {
                return false;
            }
        }
    }

    trait_count = birth_collect_character_trait_lines(p_ptr->prace,
        character_idx, false, trait_lines, (int)N_ELEMENTS(trait_lines),
        &trait_max_len);
    if (trait_count > 0)
    {
        (void)trait_max_len;
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " ")
            || !app_ui_panel_add_detail_line(panel, TERM_L_BLUE,
                "Traits and modifiers"))
        {
            return false;
        }
        for (int i = 0; i < trait_count; i++)
        {
            if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
                return true;
            if (!app_ui_panel_add_detail_line(panel, trait_lines[i].attr,
                    trait_lines[i].txt))
            {
                return false;
            }
        }
    }

    if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
        return true;
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
        return false;
    return birth_ui_panel_add_wrapped_lines(panel, TERM_WHITE, choice.text,
        true);
}

static bool birth_selection_build_ui_scene(app_ui_scene* scene,
    birth_menu* choices, int num, int cur, bool allow_full_description_screen)
{
    app_ui_panel *panel;
    bool steamdeck = steamdeck_controls_active();
    bool character_phase = allow_full_description_screen;
    char subtitle[80];

    if (!scene || !choices || num <= 0 || cur < 0 || cur >= num)
        return false;

    panel = ui_semantic_scene_begin_browser(scene, TERM_L_BLUE,
        character_selection_header_text(character_phase), 0, NULL,
        TERM_L_BLUE, APP_UI_PANEL_FLAG_SHOW_DETAIL
            | APP_UI_PANEL_FLAG_SCROLL_ROWS, 980, 2048);
    if (!panel)
        return false;

    panel->focus_area = APP_UI_FOCUS_ROWS;
    if (character_phase)
    {
        strnfmt(subtitle, sizeof(subtitle), "Race: %s",
            p_name + p_info[p_ptr->prace].name);
        app_ui_panel_set_subtitle(panel, TERM_WHITE, subtitle);
    }

    for (int i = 0; i < num; i++)
    {
        char keybuf[8];
        char meta[16];
        byte meta_attr = TERM_WHITE;

        birth_selection_row_key(i, keybuf, sizeof(keybuf));
        meta[0] = '\0';
        if (character_phase)
        {
            birth_character_power_stars(character_choice_index_by_name(
                choices[i].name), meta, sizeof(meta), &meta_attr);
        }

        if (!app_ui_panel_add_row_ex(panel, i,
                (i == cur) ? TERM_L_BLUE
                           : (choices[i].ghost ? TERM_SLATE : TERM_WHITE),
                meta_attr, 0, 0, !choices[i].ghost, i == cur, keybuf,
                choices[i].name, meta))
        {
            return false;
        }
    }

    panel->selected_row = (s16b)cur;
    app_ui_panel_set_detail_title(panel, TERM_WHITE, choices[cur].name);

    if (character_phase)
    {
        if (!birth_selection_add_character_detail_lines(panel, choices[cur]))
            return false;
    }
    else
    {
        if (!birth_selection_add_race_detail_lines(panel, choices[cur]))
            return false;
    }

    if (steamdeck)
    {
        char confirm_label[16];
        char detail_label[16];
        char back_label[16];
        char random_label[16];
        char help_label[16];
        char quit_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A",
            confirm_label, sizeof(confirm_label));
        birth_prompt_label(steamdeck_alt_action_key(), "X",
            detail_label, sizeof(detail_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        birth_prompt_label('r', "r", random_label, sizeof(random_label));
        birth_prompt_label('?', "?", help_label, sizeof(help_label));
        if (streq(help_label, "?"))
            birth_prompt_label('h', "h", help_label, sizeof(help_label));
        birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

        if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                confirm_label, "Select")
            || !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Back")
            || !app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                random_label, "Random"))
        {
            return false;
        }
        if (allow_full_description_screen
            && !app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                detail_label, "Description"))
        {
            return false;
        }
        if (!app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
                "o", "Options")
            || !app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
                "s", "Scores")
            || !app_ui_panel_add_footer_action(panel, 7, TERM_WHITE, true,
                help_label, "Help")
            || !app_ui_panel_add_footer_action(panel, 8, TERM_WHITE, true,
                quit_label, "Quit"))
        {
            return false;
        }
    }
    else
    {
        if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                "Enter", "Select")
            || !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "Esc", "Back")
            || !app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "r", "Random"))
        {
            return false;
        }
        if (allow_full_description_screen
            && !app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                "f", "Description"))
        {
            return false;
        }
        if (!app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
                "o", "Options")
            || !app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
                "s", "Scores")
            || !app_ui_panel_add_footer_action(panel, 7, TERM_WHITE, true,
                "h/?", "Help")
            || !app_ui_panel_add_footer_action(panel, 8, TERM_WHITE, true,
                "q", "Quit"))
        {
            return false;
        }
    }
    return true;
}

static void display_character_description_screen(birth_menu choice)
{
    (void)birth_show_description_ui_scene(choice);
}

static int get_player_choice(birth_menu* choices, int num, int def,
    bool allow_full_description_screen)
{
    int next;
    int i, dir;
    char c;
    bool done = false;
    int cur = def ? def : 0;
    bool steamdeck = steamdeck_controls_active();

    while (true)
    {
        app_ui_scene scene;

        if (!birth_selection_build_ui_scene(&scene, choices, num, cur,
                allow_full_description_screen)
            || !ui_information_scene_present_ui(&scene))
        {
            log_warn("birth selection: semantic scene presentation failed");
            return INVALID_CHOICE;
        }

        if (done)
            return cur;

        c = birth_wait_selection_key(&cur, num,
            allow_full_description_screen);
        if (c == '\0')
            continue;

        if ((c == 'Q') || (c == 'q'))
            quit(NULL);

        if ((c == ESCAPE) || (c == '4')
            || (steamdeck && c == steamdeck_back_key()))
        {
            return INVALID_CHOICE;
        }

        if (birth_confirm_input(c, steamdeck) || (c == '6'))
        {
            if (choices[cur].ghost)
                bell("Your race cannot choose that character.");
            else
                return cur;
        }

        if (c == 's' || c == 'S')
        {
            show_scores_interactive(false);
            birth_menu_clear_pending_input();
            continue;
        }

        if (c == 'h' || c == 'H' || c == '?')
        {
            do_cmd_help();
            birth_menu_clear_pending_input();
            continue;
        }

        if (allow_full_description_screen
            && (c == 'f' || c == 'F'
                || (steamdeck && c == steamdeck_alt_action_key())))
        {
            display_character_description_screen(choices[cur]);
            birth_menu_clear_pending_input();
            continue;
        }

        if (c == 'r')
        {
            do
            {
                cur = rand_int(num);
            } while (choices[cur].ghost);

            done = true;
        }
        else if (isalpha(c))
        {
            if ((c == 'O') || (c == 'o'))
            {
                do_cmd_options();
                birth_menu_clear_pending_input();
            }
            else
            {
                int choice;

                if (islower(c))
                    choice = A2I(c);
                else
                    choice = c - 'A' + 26;

                if ((choice > -1) && (choice < num) && !(choices[choice].ghost))
                {
                    cur = choice;
                    done = true;
                }
                else if ((choice > -1) && (choice < num)
                    && choices[choice].ghost)
                {
                    bell("Your race cannot choose that character.");
                }
                else
                {
                    bell("Illegal response to question!");
                }
            }
        }
        else if (isdigit(c))
        {
            dir = target_dir(c);

            if (dir == 8)
            {
                next = -1;
                for (i = 0; i < cur; i++)
                    next = i;

                if (next != -1)
                    cur = next;
            }

            if (dir == 2)
            {
                next = -1;
                for (i = num - 1; i > cur; i--)
                    next = i;

                if (next != -1)
                    cur = next;
            }
        }
        else
        {
            bell("Illegal response to question!");
        }
    }
}

static bool get_player_race(void)
{
    int i;
    birth_menu *races;
    int race;

    races = mem_alloc_array(z_info->p_max, birth_menu);

    for (i = 0; i < z_info->p_max; i++)
    {
        races[i].name = p_name + p_info[i].name;
        races[i].ghost = false;
        races[i].text = p_text + p_info[i].text;
    }

    race = get_player_choice(races, z_info->p_max, p_ptr->prace, false);

    if (race == INVALID_CHOICE)
    {
        races = mem_free(races);
        return false;
    }

    if (race != p_ptr->prace)
    {
        p_ptr->history[0] = '\0';
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        for (i = 0; i < A_MAX; i++)
            p_ptr->stat_base[i] = 0;
    }
    p_ptr->prace = race;

    rp_ptr = &p_info[p_ptr->prace];

    races = mem_free(races);

    return true;
}

static int character_choice_is_set(int bit)
{
    int word;
    int shift;

    if (bit < 0 || bit >= FLAG_COUNT)
        return 0;

    word = bit / 32;
    shift = bit % 32;
    return (rp_ptr->choice[word] & (1U << shift)) != 0;
}

static bool get_character_profile(void)
{
    int i;
    int character = 0;
    int character_choice;
    int previous_choice = 0;
    birth_menu *character_menu;
    int no_character_flags = 1;

    for (int idx = 0; idx < FLAG_WORDS; ++idx)
    {
        if (rp_ptr->choice[idx] != 0)
        {
            no_character_flags = 0;
            break;
        }
    }
    if (no_character_flags)
    {
        p_ptr->pcharacter = 0;
        current_character_profile = &c_info[p_ptr->pcharacter];
        return true;
    }

    character_menu = mem_alloc_array(z_info->c_max, birth_menu);

    for (i = 0; i < z_info->c_max; i++)
    {
        if (character_choice_is_set(i))
        {
            character_menu[character].ghost
                = highscore_dead(c_name + c_info[i].name);
            character_menu[character].name = c_name + c_info[i].name;
            character_menu[character].text = c_text + c_info[i].text;
            if (p_ptr->pcharacter == i)
                previous_choice = character;
            character++;
        }
    }

    character_choice = get_player_choice(character_menu, character,
        previous_choice, true);

    if (character_choice == INVALID_CHOICE)
    {
        character_menu = mem_free(character_menu);
        return false;
    }

    character = 0;
    for (i = 0; i < z_info->c_max; i++)
    {
        if (character_choice_is_set(i))
        {
            if (character_choice == character)
            {
                if (i != p_ptr->pcharacter)
                {
                    int j;

                    p_ptr->history[0] = '\0';
                    p_ptr->age = 0;
                    p_ptr->ht = 0;
                    p_ptr->wt = 0;
                    for (j = 0; j < A_MAX; j++)
                        p_ptr->stat_base[j] = 0;
                }
                p_ptr->pcharacter = i;
            }
            character++;
        }
    }

    current_character_profile = &c_info[p_ptr->pcharacter];

    character_menu = mem_free(character_menu);
    return true;
}

NavResult birth_run_character_creation_menu(void)
{
    birth_menu_scene_scope scene_scope;
    int phase = 1;

    if (!birth_menu_scene_enter(&scene_scope, APP_WAIT_REASON_LIST_SELECTION))
    {
        log_warn("character creation: semantic menu scene unavailable");
        return NAV_TO_MAIN;
    }

    while (phase <= 2)
    {
        if (phase == 1)
        {
            if (!get_player_race())
            {
                birth_menu_scene_leave(&scene_scope);
                return NAV_TO_MAIN;
            }

            phase++;
        }

        if (phase == 2)
        {
            if (!get_character_profile())
            {
                phase = 1;
                continue;
            }

            phase++;
        }
    }

    birth_menu_scene_leave(&scene_scope);
    birth_finalize_character_creation_selection();
    return NAV_OK;
}

static int oath_selectable_max_id(void)
{
    int max_oath_id = OATH_LIGHT;

    if (!z_info)
        return max_oath_id;
    if (z_info->oath_max <= 1)
        return 0;
    if (max_oath_id >= z_info->oath_max)
        max_oath_id = z_info->oath_max - 1;
    if (max_oath_id < 0)
        max_oath_id = 0;

    return max_oath_id;
}

static int oath_collect_visible(int available_mask, int* visible_oaths,
    int max_visible)
{
    int visible_count = 0;
    int max_oath_id = oath_selectable_max_id();

    if (visible_oaths && visible_count < max_visible)
        visible_oaths[visible_count] = 0;
    visible_count++;

    for (int i = 1; i <= max_oath_id; i++)
    {
        if (!(available_mask & (1 << (i - 1))) && !oath_banned(i))
            continue;

        if (visible_oaths && visible_count < max_visible)
            visible_oaths[visible_count] = i;

        visible_count++;
    }

    return visible_count;
}

static bool oath_option_selectable(int oath_id, int available_mask)
{
    if (oath_id == 0)
        return true;

    return ((available_mask & (1 << (oath_id - 1))) != 0)
        && !oath_banned(oath_id);
}

static void oath_move_highlight(int* highlight, int direction, int available_mask)
{
    int oath_max = oath_selectable_max_id() + 1;
    int original = *highlight;
    int next = *highlight;

    if (oath_max <= 0)
    {
        *highlight = 0;
        return;
    }

    do
    {
        next += direction;
        if (next < 0)
            next = oath_max - 1;
        if (next >= oath_max)
            next = 0;

        if ((next == 0)
            || (available_mask & (1 << (next - 1)))
            || oath_banned(next))
        {
            *highlight = next;
            return;
        }
    } while (next != original);
}

static bool oath_build_ui_scene(app_ui_scene* scene, int available_mask,
    int highlight)
{
    app_ui_panel *panel;
    int visible_oaths[16];
    int visible_count;
    bool steamdeck = steamdeck_controls_active();

    if (!scene)
        return false;

    visible_count = oath_collect_visible(available_mask, visible_oaths,
        (int)N_ELEMENTS(visible_oaths));
    if (visible_count > (int)N_ELEMENTS(visible_oaths))
        visible_count = (int)N_ELEMENTS(visible_oaths);

    panel = ui_semantic_scene_begin_browser(scene, TERM_L_BLUE,
        "Choose your Oath", 0, NULL, TERM_L_BLUE,
        APP_UI_PANEL_FLAG_SHOW_DETAIL | APP_UI_PANEL_FLAG_SCROLL_ROWS,
        980, 2048);
    if (!panel)
        return false;

    panel->focus_area = APP_UI_FOCUS_ROWS;
    for (int i = 0; i < visible_count; i++)
    {
        int oath_id = visible_oaths[i];
        char keybuf[8];
        byte attr;
        bool enabled = oath_option_selectable(oath_id, available_mask);

        birth_selection_row_key(i, keybuf, sizeof(keybuf));
        if (oath_banned(oath_id) && oath_id > 0)
            attr = (highlight == oath_id) ? TERM_L_RED : TERM_RED;
        else if (highlight == oath_id)
            attr = TERM_L_BLUE;
        else
            attr = TERM_WHITE;

        if (!app_ui_panel_add_row(panel, oath_id, attr, enabled,
                highlight == oath_id, keybuf, oath_name_str(oath_id), ""))
        {
            return false;
        }
        if (highlight == oath_id)
            panel->selected_row = (s16b)i;
    }

    app_ui_panel_set_detail_title(panel,
        oath_banned(highlight) ? TERM_L_RED
            : (highlight == 0 ? TERM_WHITE : TERM_L_BLUE),
        oath_name_str(highlight));

    if (oath_banned(highlight) && highlight > 0)
    {
        char *banned_text = oath_banned_text(highlight);

        if (!app_ui_panel_add_detail_line(panel, TERM_L_RED, "OATH BROKEN"))
            return false;
        if (!birth_ui_panel_add_wrapped_lines(panel, TERM_RED,
                (banned_text && banned_text[0]) ? banned_text
                    : "Thy oath lies shattered, and thy name is marked in shame for this age.",
                true))
        {
            return false;
        }
    }
    else if (highlight == 0)
    {
        if (!birth_ui_panel_add_wrapped_lines(panel, TERM_SLATE,
                "Walk free of binding words.", true)
            || !birth_ui_panel_add_wrapped_lines(panel, TERM_SLATE,
                "Take no oath and remain unbound by sacred vows.", true))
        {
            return false;
        }
    }
    else
    {
        char line_buf[768];

        if (oath_description(highlight) && oath_description(highlight)[0])
        {
            strnfmt(line_buf, sizeof(line_buf), "Description: %s",
                oath_description(highlight));
            if (!birth_ui_panel_add_wrapped_lines(panel, TERM_SLATE, line_buf,
                    true))
            {
                return false;
            }
        }
        if (oath_pledge(highlight) && oath_pledge(highlight)[0])
        {
            strnfmt(line_buf, sizeof(line_buf), "Pledge: %s",
                oath_pledge(highlight));
            if (!birth_ui_panel_add_wrapped_lines(panel, TERM_L_BLUE, line_buf,
                    true))
            {
                return false;
            }
        }
        if (oath_reward_text(highlight) && oath_reward_text(highlight)[0])
        {
            strnfmt(line_buf, sizeof(line_buf), "Reward: %s",
                oath_reward_text(highlight));
            if (!birth_ui_panel_add_wrapped_lines(panel, TERM_L_GREEN, line_buf,
                    true))
            {
                return false;
            }
        }
        if (oath_forbidden(highlight) && oath_forbidden(highlight)[0])
        {
            strnfmt(line_buf, sizeof(line_buf), "Forbidden: %s",
                oath_forbidden(highlight));
            if (!birth_ui_panel_add_wrapped_lines(panel, TERM_L_RED, line_buf,
                    true))
            {
                return false;
            }
        }
    }

    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Oaths grant power, but they bind your actions.");
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Breaking an oath brings curse and shame.");

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A",
            confirm_label, sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                confirm_label, "Select")
            || !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Back")
            || !app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, false,
                "D-pad", "Navigate"))
        {
            return false;
        }
    }
    else if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Enter", "Select")
        || !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Esc", "Back")
        || !app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "8/2", "Navigate"))
    {
        return false;
    }

    return true;
}

NavResult birth_select_oath(void)
{
    int available_mask = get_available_oaths_mask();

    if (available_mask == 0)
    {
        p_ptr->oath_type = 0;
        log_debug("No oaths available, skipping oath selection");
        return NAV_OK;
    }

    int highlight = 1;
    int choice = 0;
    bool steamdeck = steamdeck_controls_active();

    for (int i = 1; i <= oath_selectable_max_id(); i++)
    {
        if (available_mask & (1 << (i - 1)))
        {
            highlight = i;
            break;
        }
    }

    while (true)
    {
        int visible_oaths[16];
        int visible_count;
        char key;
        app_ui_scene scene;

        visible_count = oath_collect_visible(available_mask, visible_oaths,
            (int)N_ELEMENTS(visible_oaths));
        if (visible_count > (int)N_ELEMENTS(visible_oaths))
            visible_count = (int)N_ELEMENTS(visible_oaths);

        if (!oath_build_ui_scene(&scene, available_mask, highlight)
            || !ui_information_scene_present_ui(&scene))
        {
            log_warn("oath selection: semantic scene presentation failed");
            return NAV_BACK;
        }
        key = birth_wait_oath_key(&highlight, available_mask);
        if (key == '\0')
            continue;

        if (steamdeck && key == steamdeck_back_key())
            return NAV_BACK;
        if (key == ESCAPE || key == 'q')
            return NAV_BACK;

        if (birth_confirm_input(key, steamdeck) || key == '6')
        {
            if (oath_option_selectable(highlight, available_mask))
            {
                choice = highlight;
                break;
            }
        }

        if (key >= 'a' && key < 'a' + visible_count)
        {
            int display_pos = key - 'a';

            if (display_pos >= 0 && display_pos < visible_count
                && oath_option_selectable(visible_oaths[display_pos],
                    available_mask))
            {
                choice = visible_oaths[display_pos];
                break;
            }

            continue;
        }

        if (key == '8')
            oath_move_highlight(&highlight, -1, available_mask);

        if (key == '2')
            oath_move_highlight(&highlight, 1, available_mask);
    }

    p_ptr->oath_type = choice;

    if (choice > 0 && choice < z_info->oath_max)
    {
        oath_type *oath_ptr = &oath_info[choice];

        if (oath_ptr->reward_type > 0 && oath_ptr->reward_value > 0)
        {
            int skill_category = oath_ptr->reward_type;
            int ability_id = oath_ptr->reward_value;

            if (skill_category >= 0 && skill_category < S_MAX
                && ability_id >= 0 && ability_id < ABILITIES_MAX)
            {
                p_ptr->have_ability[skill_category][ability_id] = true;
                p_ptr->innate_ability[skill_category][ability_id] = true;
                p_ptr->active_ability[skill_category][ability_id] = true;

                log_debug("Granted oath %d abilities from data: skill=%d, ability=%d",
                    choice, skill_category, ability_id);
            }
            else
            {
                log_warn("Oath %d ability out of bounds: skill=%d (max %d), ability=%d (max %d)",
                    choice, skill_category, S_MAX - 1, ability_id,
                    ABILITIES_MAX - 1);
            }
        }
        else
        {
            log_debug("No ability reward found for oath %d", choice);
        }
    }

    if (choice == 0)
        log_debug("No oath selected");
    else
        log_debug("Oath selected: %s (%d)", oath_name_str(choice), choice);

    return NAV_OK;
}

NavResult birth_run_stats_allocation(void)
{
    int i;
    int stat = 0;
    int stats[A_MAX];
    int cost;
    char ch;

    for (i = 0; i < A_MAX; i++)
        stats[i] = p_ptr->stat_base[i];

    birth_prepare_character_extra();

    log_debug("Checking if tutorial should be shown...");
    if (!run_mode_is_blitz() && highscore_is_empty())
    {
        log_info("First-time player detected - showing character screen tutorial");

        for (i = 0; i < A_MAX; i++)
        {
            int bonus = rp_ptr->r_adj[i] + current_character_profile->h_adj[i]
                + birth_curses_stat_adj(i);

            p_ptr->stat_base[i] = stats[i] + bonus;
            p_ptr->stat_drain[i] = 0;
        }

        p_ptr->update |= (PU_BONUS | PU_HP);
        update_stuff();
        p_ptr->chp = p_ptr->mhp;
        calc_voice();
        p_ptr->csp = p_ptr->msp;

        display_character_tutorial();
        log_info("Character screen tutorial completed");
        birth_show_input_tutorials();
    }
    else
    {
        log_info("Not showing tutorial - scores file has entries");
    }

    log_trace("Starting stats allocation interface");

    while (1)
    {
        bool steamdeck = steamdeck_controls_active();

        cost = 0;

        for (i = 0; i < A_MAX; i++)
        {
            int bonus = rp_ptr->r_adj[i] + current_character_profile->h_adj[i]
                + birth_curses_stat_adj(i);

            p_ptr->stat_base[i] = stats[i] + bonus;
            p_ptr->stat_drain[i] = 0;
            cost += birth_stat_cost(stats[i]);
        }

        if (cost > BIRTH_MAX_COST)
        {
            bell("Excessive stats!");
            stats[stat]--;
            continue;
        }

        p_ptr->new_exp = p_ptr->exp = birth_get_start_xp();
        p_ptr->update |= (PU_BONUS | PU_HP);
        update_stuff();
        p_ptr->chp = p_ptr->mhp;
        calc_voice();
        p_ptr->csp = p_ptr->msp;

        if (!birth_present_stats_allocation_ui_scene(stats, stat,
                BIRTH_MAX_COST - cost, steamdeck))
        {
            log_warn("birth stats allocation: semantic scene unavailable");
            return NAV_TO_MAIN;
        }

        ch = birth_wait_allocation_key(&stat, A_MAX, false, true);
        if (ch == '\0')
            continue;

        if ((ch == 'Q') || (ch == 'q'))
        {
            if (turn == 0)
                return NAV_TO_MAIN;
            return NAV_QUIT;
        }

        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (ch == ESCAPE)
            return NAV_BACK;

        if (birth_confirm_input(ch, steamdeck))
            return NAV_OK;

        if (ch == '8')
            stat = (stat + A_MAX - 1) % A_MAX;

        if (ch == '2')
            stat = (stat + 1) % A_MAX;

        if ((ch == '4') && (stats[stat] > 0))
            stats[stat]--;

        if (ch == '6')
            stats[stat]++;
    }
}

NavResult gain_skills(void)
{
    int i;
    int skill = 0;
    int old_base[S_MAX];
    int skill_gain[S_MAX];
    int old_new_exp = p_ptr->new_exp;
    int total_cost = 0;
    char ch;
    NavResult result = NAV_OK;

    log_debug("Starting skills allocation with %d experience points",
        p_ptr->new_exp);

    for (i = 0; i < S_MAX; i++)
        old_base[i] = p_ptr->skill_base[i];

    for (i = 0; i < S_MAX; i++)
        skill_gain[i] = 0;

    while (1)
    {
        bool steamdeck = steamdeck_controls_active();

        total_cost = 0;

        for (i = 0; i < S_MAX; i++)
        {
            if (i == S_SPC)
                continue;
            total_cost += birth_skill_cost(old_base[i], skill_gain[i]);
        }

        p_ptr->new_exp = old_new_exp - total_cost;

        if (p_ptr->new_exp < 0)
        {
            bell("Excessive skills!");
            skill_gain[skill]--;
            continue;
        }

        p_ptr->update |= PU_BONUS;
        p_ptr->redraw |= (PR_EXP | PR_BASIC);

        for (i = 0; i < S_MAX; i++)
        {
            if (i == S_SPC)
                continue;
            p_ptr->skill_base[i] = old_base[i] + skill_gain[i];
        }

        update_stuff();

        if (!birth_present_skills_allocation_ui_scene(skill, old_base,
                skill_gain, p_ptr->new_exp, steamdeck))
        {
            log_warn("birth skills allocation: semantic scene unavailable");
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++)
            {
                if (i != S_SPC)
                    p_ptr->skill_base[i] = old_base[i];
            }
            return NAV_TO_MAIN;
        }

        ch = birth_wait_allocation_key(&skill, S_MAX, true, true);
        if (ch == '\0')
            continue;

        if (((ch == 'Q') || (ch == 'q')) && (turn == 0))
        {
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++)
            {
                if (i != S_SPC)
                    p_ptr->skill_base[i] = old_base[i];
            }
            return NAV_TO_MAIN;
        }

        if (birth_confirm_input(ch, steamdeck))
        {
            if (birth_assignment_review_pending())
            {
                if (!birth_show_semantic_assignment_review(steamdeck))
                    continue;
                birth_set_assignment_review_pending(false);
            }
            result = NAV_OK;
            break;
        }

        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (ch == ESCAPE)
        {
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++)
            {
                if (i != S_SPC)
                    p_ptr->skill_base[i] = old_base[i];
            }
            result = NAV_BACK;
            break;
        }

        if (ch == '8')
        {
            do {
                skill = (skill + S_MAX - 1) % S_MAX;
            } while (skill == S_SPC);
        }

        if (ch == '2')
        {
            do {
                skill = (skill + 1) % S_MAX;
            } while (skill == S_SPC);
        }

        if ((ch == '4') && (skill_gain[skill] > 0))
            skill_gain[skill]--;

        if (ch == '6' && skill != S_SPC)
            skill_gain[skill]++;
    }

    p_ptr->update |= PU_BONUS;
    update_stuff();

    log_debug("Skills allocation completed, spent %d experience",
        old_new_exp - p_ptr->new_exp);

    return result;
}

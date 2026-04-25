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

#include "app/app-session.h"
#include "app/app-ui.h"
#include "blitz.h"
#include "log/log.h"
#include "platform-input.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-semantic-scene.h"

#define BLITZ_MAX_EFFECT_COUNT 9

typedef struct birth_menu_scene_scope {
    bool active;
    app_wait_scope wait_scope;
} birth_menu_scene_scope;

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

static void birth_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    ui_semantic_prompt_label(binding, fallback, buf, buflen);
}

static char blitz_ui_direction_command_key(const app_ui_command* command)
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

static bool blitz_ui_command_cancelled(const app_ui_command* command)
{
    return command
        && (command->kind == APP_UI_COMMAND_KIND_CANCEL
            || command->target.action == APP_UI_WIDGET_ACTION_CANCEL);
}

static bool blitz_ui_command_activated(const app_ui_command* command)
{
    return command
        && (command->kind == APP_UI_COMMAND_KIND_ACTIVATE
            || command->kind == APP_UI_COMMAND_KIND_SELECT
            || command->target.action == APP_UI_WIDGET_ACTION_ACTIVATE
            || command->target.action == APP_UI_WIDGET_ACTION_SELECT);
}

static bool blitz_ui_wait_event(ui_information_scene_event* event)
{
    return ui_information_scene_wait_event_with_wait_reason(event, 0,
        APP_WAIT_REASON_LIST_SELECTION, true);
}

static char blitz_wait_setup_key(int* selected)
{
    ui_information_scene_event event;

    while (blitz_ui_wait_event(&event))
    {
        const app_ui_command* command;

        if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            return (char)event.key;
        if (event.kind != UI_INFORMATION_SCENE_EVENT_COMMAND)
            continue;

        command = &event.command;
        if (blitz_ui_command_cancelled(command))
            return ESCAPE;
        if (command->kind == APP_UI_COMMAND_KIND_SCROLL
            || command->kind == APP_UI_COMMAND_KIND_FOCUS)
        {
            char dir_key = blitz_ui_direction_command_key(command);

            if (dir_key)
                return dir_key;
        }
        if (command->target.role == APP_UI_WIDGET_ROLE_LIST_ITEM)
        {
            int row_id = command->target.widget_id;

            if (selected && row_id >= 0 && row_id < 5)
                *selected = row_id;
            return '\0';
        }
        if (command->target.role == APP_UI_WIDGET_ROLE_BUTTON)
        {
            if (command->target.widget_id == 1)
                return '\r';
            if (command->target.widget_id == 2)
                return ESCAPE;
            if (command->target.widget_id == 4)
                return '4';
            if (command->target.widget_id == 5)
                return '6';
        }
    }

    return ESCAPE;
}

static char blitz_wait_effect_picker_key(const int ids[], int count,
    int* selected)
{
    ui_information_scene_event event;

    while (blitz_ui_wait_event(&event))
    {
        const app_ui_command* command;

        if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            return (char)event.key;
        if (event.kind != UI_INFORMATION_SCENE_EVENT_COMMAND)
            continue;

        command = &event.command;
        if (blitz_ui_command_cancelled(command))
            return ESCAPE;
        if (command->kind == APP_UI_COMMAND_KIND_SCROLL
            || command->kind == APP_UI_COMMAND_KIND_FOCUS)
        {
            char dir_key = blitz_ui_direction_command_key(command);

            if (dir_key)
                return dir_key;
        }
        if (command->target.role == APP_UI_WIDGET_ROLE_LIST_ITEM)
        {
            for (int i = 0; ids && i < count; i++)
            {
                if (ids[i] != command->target.widget_id)
                    continue;
                if (selected)
                    *selected = i;
                if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
                    return '\0';
                return blitz_ui_command_activated(command) ? '\r' : '\0';
            }
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

static int blitz_character_choice_is_set(int bit)
{
    int word;
    int shift;

    if (bit < 0 || bit >= FLAG_COUNT)
        return 0;

    word = bit / 32;
    shift = bit % 32;
    return (rp_ptr->choice[word] & (1U << shift)) != 0;
}

static cptr blitz_character_mode_name(byte mode)
{
    switch (mode)
    {
    case BLITZ_CHARACTER_RANDOM_STATS:
        return "Random with stats";
    case BLITZ_CHARACTER_SELECTED:
        return "Selected";
    default:
        return "Random";
    }
}

static cptr blitz_effect_mode_name(byte mode)
{
    switch (mode)
    {
    case BLITZ_EFFECT_SELECTED:
        return "Selected";
    case BLITZ_EFFECT_SELECTED_DESCR:
        return "Selected + descriptions";
    default:
        return "Random";
    }
}

static void blitz_setup_clamp(blitz_setup* setup)
{
    if (!setup)
        return;

    if (setup->character_mode > BLITZ_CHARACTER_SELECTED)
        setup->character_mode = BLITZ_CHARACTER_RANDOM;
    if (setup->effect_mode > BLITZ_EFFECT_SELECTED_DESCR)
        setup->effect_mode = BLITZ_EFFECT_RANDOM;
    if (setup->blessing_count > BLITZ_MAX_EFFECT_COUNT)
        setup->blessing_count = BLITZ_MAX_EFFECT_COUNT;
    if (setup->curse_count > BLITZ_MAX_EFFECT_COUNT)
        setup->curse_count = BLITZ_MAX_EFFECT_COUNT;
    if (setup->curse_count < setup->blessing_count)
        setup->curse_count = setup->blessing_count;
}

void birth_blitz_pick_random_race_and_character(void)
{
    int race = 0;
    int available[64];
    int available_count = 0;

    if (!z_info)
        return;

    race = rand_int(z_info->p_max);
    p_ptr->prace = race;
    rp_ptr = &p_info[p_ptr->prace];

    for (int i = 0; i < z_info->c_max
        && available_count < (int)N_ELEMENTS(available); i++)
    {
        if (blitz_character_choice_is_set(i))
            available[available_count++] = i;
    }

    if (available_count <= 0)
        p_ptr->pcharacter = 0;
    else
        p_ptr->pcharacter = available[rand_int(available_count)];

    current_character_profile = &c_info[p_ptr->pcharacter];
}

static bool blitz_setup_build_ui_scene(app_ui_scene* scene,
    const blitz_setup* setup, int selected)
{
    static const char* const titles[] = {
        "Character", "Oaths", "Blessings", "Curses", "Effect Picks"
    };
    app_ui_panel *panel;
    cptr detail = "";
    char meta[64];
    char keybuf[16];
    bool steamdeck = steamdeck_controls_active();

    if (!scene || !setup)
        return false;

    panel = ui_semantic_scene_begin_browser(scene, TERM_YELLOW, "Blitz Setup",
        TERM_SLATE,
        "Configure a self-contained Blitz run. Story progress stays untouched.",
        TERM_L_BLUE, APP_UI_PANEL_FLAG_SHOW_DETAIL
            | APP_UI_PANEL_FLAG_SCROLL_ROWS, 980, 1800);
    if (!panel)
        return false;

    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->selected_row = selected;

    strnfmt(meta, sizeof(meta), "%s",
        blitz_character_mode_name(setup->character_mode));
    if (!app_ui_panel_add_row(panel, 0, selected == 0 ? TERM_L_BLUE : TERM_WHITE,
            true, selected == 0, "", titles[0], meta))
    {
        return false;
    }

    strnfmt(meta, sizeof(meta), "%s", setup->oaths_enabled ? "Yes" : "No");
    if (!app_ui_panel_add_row(panel, 1, selected == 1 ? TERM_L_BLUE : TERM_WHITE,
            true, selected == 1, "", titles[1], meta))
    {
        return false;
    }

    strnfmt(meta, sizeof(meta), "%d", setup->blessing_count);
    if (!app_ui_panel_add_row(panel, 2, selected == 2 ? TERM_L_BLUE : TERM_WHITE,
            true, selected == 2, "", titles[2], meta))
    {
        return false;
    }

    strnfmt(meta, sizeof(meta), "%d", setup->curse_count);
    if (!app_ui_panel_add_row(panel, 3, selected == 3 ? TERM_L_BLUE : TERM_WHITE,
            true, selected == 3, "", titles[3], meta))
    {
        return false;
    }

    strnfmt(meta, sizeof(meta), "%s", blitz_effect_mode_name(setup->effect_mode));
    if (!app_ui_panel_add_row(panel, 4, selected == 4 ? TERM_L_BLUE : TERM_WHITE,
            true, selected == 4, "", titles[4], meta))
    {
        return false;
    }

    switch (selected)
    {
    case 0:
        detail = "Choose a selected character, a fully random character, or a random character with manual stat assignment.";
        break;
    case 1:
        detail = "Enable oath selection during birth, or skip oaths entirely for this Blitz run.";
        break;
    case 2:
        detail = "Pick how many blessings will be applied before the run starts.";
        break;
    case 3:
        detail = "Pick how many curses will be applied before the run starts. Curses can never be fewer than blessings.";
        break;
    case 4:
        detail = "Choose whether effects are random, selected from a list, or selected with full descriptions and effect text.";
        break;
    default:
        break;
    }

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
        (selected >= 0 && selected < 5) ? titles[selected] : "Blitz Setup");
    if (!birth_ui_panel_add_wrapped_lines(panel, TERM_WHITE, detail, true))
        return false;

    if (steamdeck)
    {
        birth_prompt_label(steamdeck_confirm_key(), "A", keybuf,
            sizeof(keybuf));
        if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                keybuf, "Begin"))
        {
            return false;
        }
        birth_prompt_label(steamdeck_back_key(), "B", keybuf,
            sizeof(keybuf));
        if (!app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                keybuf, "Back"))
        {
            return false;
        }
        if (!app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, false,
                "D-pad", "Move/Change"))
        {
            return false;
        }
    }
    else
    {
        if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                "Enter", "Begin")
            || !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "Esc", "Back")
            || !app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "8/2", "Move")
            || !app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                "4", "Dec")
            || !app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
                "6", "Inc"))
        {
            return false;
        }
    }

    return true;
}

NavResult birth_blitz_setup_menu(void)
{
    blitz_setup *setup = blitz_current_setup_mutable();
    int selected = 0;
    bool steamdeck = steamdeck_controls_active();
    birth_menu_scene_scope scene_scope;

    blitz_setup_clamp(setup);
    if (!birth_menu_scene_enter(&scene_scope, APP_WAIT_REASON_LIST_SELECTION))
    {
        log_warn("blitz setup: semantic menu scene unavailable");
        return NAV_TO_MAIN;
    }

    while (1)
    {
        app_ui_scene scene;
        char key;

        if (!blitz_setup_build_ui_scene(&scene, setup, selected)
            || !ui_information_scene_present_ui(&scene))
        {
            log_warn("blitz setup: semantic scene presentation failed");
            birth_menu_scene_leave(&scene_scope);
            return NAV_TO_MAIN;
        }
        key = blitz_wait_setup_key(&selected);
        if (key == '\0')
            continue;

        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()))
        {
            birth_menu_scene_leave(&scene_scope);
            return NAV_TO_MAIN;
        }

        if (key == '\n' || key == '\r' || key == ' '
            || (steamdeck && key == steamdeck_confirm_key()))
        {
            birth_menu_scene_leave(&scene_scope);
            return NAV_OK;
        }

        if (key == '8')
        {
            selected = (selected + 4) % 5;
            continue;
        }

        if (key == '2')
        {
            selected = (selected + 1) % 5;
            continue;
        }

        if (key != '4' && key != '6')
            continue;

        switch (selected)
        {
        case 0:
            if (key == '4')
            {
                setup->character_mode = (setup->character_mode
                    == BLITZ_CHARACTER_RANDOM)
                    ? BLITZ_CHARACTER_SELECTED
                    : setup->character_mode - 1;
            }
            else
            {
                setup->character_mode = (setup->character_mode
                    == BLITZ_CHARACTER_SELECTED)
                    ? BLITZ_CHARACTER_RANDOM
                    : setup->character_mode + 1;
            }
            break;
        case 1:
            setup->oaths_enabled = !setup->oaths_enabled;
            break;
        case 2:
            if (key == '4' && setup->blessing_count > 0)
                setup->blessing_count--;
            else if (key == '6'
                && setup->blessing_count < BLITZ_MAX_EFFECT_COUNT)
            {
                setup->blessing_count++;
            }
            break;
        case 3:
            if (key == '4' && setup->curse_count > 0)
                setup->curse_count--;
            else if (key == '6' && setup->curse_count < BLITZ_MAX_EFFECT_COUNT)
                setup->curse_count++;
            break;
        case 4:
            if (key == '4')
            {
                setup->effect_mode = (setup->effect_mode
                    == BLITZ_EFFECT_RANDOM)
                    ? BLITZ_EFFECT_SELECTED_DESCR
                    : setup->effect_mode - 1;
            }
            else
            {
                setup->effect_mode = (setup->effect_mode
                    == BLITZ_EFFECT_SELECTED_DESCR)
                    ? BLITZ_EFFECT_RANDOM
                    : setup->effect_mode + 1;
            }
            break;
        default:
            break;
        }

        blitz_setup_clamp(setup);
    }
}

static cptr blitz_curse_name_str(int id)
{
    cptr raw = cu_name + cu_info[id].name;

    if (strncmp(raw, "Curse of ", 9) == 0)
        raw += 9;
    return raw;
}

static cptr blitz_blessing_name_str(int id)
{
    if (cu_info[id].blessing_name)
    {
        cptr raw = cu_name + cu_info[id].blessing_name;

        if (strncmp(raw, "Blessing of ", 12) == 0)
            raw += 12;
        return raw;
    }

    return blitz_curse_name_str(id);
}

static int blitz_collect_eligible_effect_ids(bool blessing, int ids[],
    int max_ids)
{
    int count = 0;

    for (int id = 0; z_info && id < z_info->cu_max && count < max_ids; id++)
    {
        int stacks = CURSE_GET(id);
        int blessing_stacks = (stacks < 0) ? -stacks : 0;
        int curse_stacks = (stacks > 0) ? stacks : 0;
        byte curse_cap = (byte)CURSE_CURSE_CAP(id);
        byte blessing_cap = (byte)CURSE_BLESSING_CAP(id);

        if (blessing)
        {
            if (!cu_info[id].blessing_name)
                continue;
            if (stacks > 0)
                continue;
            if (blessing_cap > 0 && blessing_stacks >= blessing_cap)
                continue;
        }
        else
        {
            if (!cu_info[id].name)
                continue;
            if (curse_cap > 0 && curse_stacks >= curse_cap)
                continue;
        }

        ids[count++] = id;
    }

    return count;
}

static int blitz_weighted_random_curse_pick(void)
{
    long total = 0;
    int w_max = 1;
    bool tilt = (p_info[p_ptr->prace].flags & RHF_CURSE)
        || (c_info[p_ptr->pcharacter].flags & RHF_CURSE);

    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name)
            continue;
        if (cu_info[i].weight > w_max)
            w_max = cu_info[i].weight;
    }

    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        byte w = cu_info[i].weight ? cu_info[i].weight : 1;
        int cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        long base;

        if (!cu_info[i].name)
            continue;
        if (cap && cnt >= cap)
            continue;
        if (tilt && w == w_max)
            continue;

        base = tilt ? w + ((w_max + 1 - w) >> 1) : w;
        total += base / (cnt + 1);
    }

    if (!total)
        return -1;

    long pick = rand_int(total);
    long run = 0;

    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        byte w = cu_info[i].weight ? cu_info[i].weight : 1;
        int cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        long base;
        long eff;

        if (!cu_info[i].name)
            continue;
        if (cap && cnt >= cap)
            continue;
        if (tilt && w == w_max)
            continue;

        base = tilt ? w + ((w_max + 1 - w) >> 1) : w;
        eff = base / (cnt + 1);
        run += eff;
        if (pick < run)
            return i;
    }

    return -1;
}

static int blitz_weighted_random_blessing_pick(void)
{
    int eligible[METAR_CURSE_SLOTS];
    int weights[METAR_CURSE_SLOTS];
    int count = 0;
    int total_weight = 0;

    for (int id = 0; z_info && id < z_info->cu_max
        && count < METAR_CURSE_SLOTS; id++)
    {
        int stacks = CURSE_GET(id);
        int blessing_stacks = (stacks < 0) ? -stacks : 0;
        int base_weight;
        int effective_weight;

        if (!cu_info[id].blessing_name)
            continue;
        if (stacks > 0)
            continue;
        if (CURSE_BLESSING_CAP(id) > 0
            && blessing_stacks >= CURSE_BLESSING_CAP(id))
        {
            continue;
        }

        eligible[count] = id;
        base_weight = cu_info[id].weight > 0 ? cu_info[id].weight : 1;
        effective_weight = base_weight / (blessing_stacks + 1);
        weights[count] = (effective_weight > 0) ? effective_weight : 1;
        total_weight += weights[count];
        count++;
    }

    if (count <= 0 || total_weight <= 0)
        return -1;

    int roll = rand_int(total_weight);
    int sum = 0;

    for (int i = 0; i < count; i++)
    {
        sum += weights[i];
        if (roll < sum)
            return eligible[i];
    }

    return eligible[0];
}

static bool blitz_effect_picker_build_ui_scene(app_ui_scene* scene,
    bool blessing, bool show_effects, int ordinal, int total,
    const int ids[], int count, int selected)
{
    app_ui_panel *panel;
    int selected_id;
    curse_type *cu;
    cptr desc;
    cptr power;
    bool steamdeck = steamdeck_controls_active();
    char title[80];

    if (!scene || !ids || count <= 0 || selected < 0 || selected >= count)
        return false;

    selected_id = ids[selected];
    cu = &cu_info[selected_id];
    desc = blessing
        ? (cu->blessing_text ? cu_text + cu->blessing_text : "")
        : (cu->text ? cu_text + cu->text : "");
    power = blessing
        ? (cu->blessing_power ? cu_text + cu->blessing_power : "")
        : (cu->power ? cu_text + cu->power : "");

    strnfmt(title, sizeof(title), "Choose %s %d of %d",
        blessing ? "Blessing" : "Curse", ordinal, total);
    panel = ui_semantic_scene_begin_browser(scene, TERM_YELLOW, title,
        blessing ? TERM_L_GREEN : TERM_L_RED,
        blessing ? "Blessings" : "Curses", TERM_L_BLUE,
        APP_UI_PANEL_FLAG_SHOW_DETAIL | APP_UI_PANEL_FLAG_SCROLL_ROWS,
        980, 1800);
    if (!panel)
        return false;

    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->selected_row = selected;

    for (int row = 0; row < count; row++)
    {
        int id = ids[row];
        cptr name = blessing ? blitz_blessing_name_str(id)
                             : blitz_curse_name_str(id);
        byte attr = (row == selected)
            ? TERM_L_BLUE
            : (blessing ? TERM_L_GREEN : TERM_L_RED);

        if (!app_ui_panel_add_row(panel, id, attr, true,
                row == selected, "", name, ""))
        {
            return false;
        }
    }

    app_ui_panel_set_detail_title(panel, TERM_WHITE,
        blessing ? blitz_blessing_name_str(selected_id)
                 : blitz_curse_name_str(selected_id));
    if (desc && desc[0]
        && !birth_ui_panel_add_wrapped_lines(panel, TERM_SLATE, desc, true))
    {
        return false;
    }

    if (show_effects && power && power[0])
    {
        char power_line[512];

        if (panel->detail_line_count > 0
            && !app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
        {
            return false;
        }

        strnfmt(power_line, sizeof(power_line), "Effect: %s", power);
        if (!birth_ui_panel_add_wrapped_lines(panel,
                blessing ? TERM_L_GREEN : TERM_L_RED, power_line, true))
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
                confirm_label, "Select")
            && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Back")
            && app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, false,
                "D-pad", "Navigate");
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Enter", "Select")
        && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Esc", "Back")
        && app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "8/2", "Navigate");
}

static int blitz_select_effect_from_list(bool blessing, bool show_effects,
    int ordinal, int total)
{
    int ids[METAR_CURSE_SLOTS];
    int count = blitz_collect_eligible_effect_ids(blessing, ids,
        METAR_CURSE_SLOTS);
    int selected = 0;
    bool steamdeck = steamdeck_controls_active();

    if (count <= 0)
        return -1;

    while (1)
    {
        app_ui_scene scene;
        int selected_id = ids[selected];
        char key;

        if (!blitz_effect_picker_build_ui_scene(&scene, blessing,
                show_effects, ordinal, total, ids, count, selected)
            || !ui_information_scene_present_ui(&scene))
        {
            log_warn("blitz effect picker: semantic scene presentation failed");
            return -1;
        }

        key = blitz_wait_effect_picker_key(ids, count, &selected);
        if (key == '\0')
            continue;

        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()))
            return -1;
        if (key == '\n' || key == '\r' || key == ' '
            || (steamdeck && key == steamdeck_confirm_key()))
        {
            return selected_id;
        }
        if (key == '8')
        {
            selected = (selected + count - 1) % count;
            continue;
        }
        if (key == '2')
        {
            selected = (selected + 1) % count;
            continue;
        }
    }
}

static void blitz_apply_effect_pick(int id, bool blessing)
{
    CURSE_ADD(id, blessing ? -1 : 1);
    CURSE_SEEN_SET(id);
}

static bool blitz_effect_summary_build_ui_scene(app_ui_scene* scene)
{
    app_ui_panel *panel;
    bool steamdeck = steamdeck_controls_active();

    if (!scene)
        return false;

    panel = ui_semantic_scene_begin_browser(scene, TERM_YELLOW,
        "Blitz Effects", TERM_SLATE,
        "Starting blessings and curses for this Blitz run.", TERM_L_BLUE,
        0, 900, 1600);
    if (!panel)
        return false;

    for (int id = 0; z_info && id < z_info->cu_max; id++)
    {
        int stacks = CURSE_GET(id);
        char line[128];

        if (stacks == 0)
            continue;

        strnfmt(line, sizeof(line), "%s x%d",
            (stacks < 0) ? blitz_blessing_name_str(id)
                         : blitz_curse_name_str(id),
            (stacks < 0) ? -stacks : stacks);
        if (!app_ui_panel_add_body_line(panel,
                stacks < 0 ? TERM_L_GREEN : TERM_L_RED, line))
        {
            return false;
        }
    }

    if (panel->body_line_count == 0
        && !app_ui_panel_add_body_line(panel, TERM_SLATE,
            "No blessings or curses selected."))
    {
        return false;
    }

    if (steamdeck)
    {
        char confirm_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            confirm_label, "Continue");
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Any key", "Continue");
}

static void blitz_show_effect_summary(void)
{
    app_ui_scene scene;

    if (!blitz_effect_summary_build_ui_scene(&scene)
        || !ui_information_scene_present_ui(&scene))
    {
        log_warn("blitz effect summary: semantic scene presentation failed");
        return;
    }

    (void)ui_information_scene_wait_dismissal_with_wait_reason(0,
        APP_WAIT_REASON_INFORMATIONAL_PAUSE, true);
}

NavResult birth_blitz_configure_effects(void)
{
    const blitz_setup *setup = blitz_current_setup();

    blitz_runtime_reset();

    for (int i = 0; i < setup->curse_count; i++)
    {
        int id = (setup->effect_mode == BLITZ_EFFECT_RANDOM)
            ? blitz_weighted_random_curse_pick()
            : blitz_select_effect_from_list(false,
                setup->effect_mode == BLITZ_EFFECT_SELECTED_DESCR, i + 1,
                setup->curse_count);
        if (id < 0)
            return NAV_BACK;
        blitz_apply_effect_pick(id, false);
    }

    for (int i = 0; i < setup->blessing_count; i++)
    {
        int id = (setup->effect_mode == BLITZ_EFFECT_RANDOM)
            ? blitz_weighted_random_blessing_pick()
            : blitz_select_effect_from_list(true,
                setup->effect_mode == BLITZ_EFFECT_SELECTED_DESCR, i + 1,
                setup->blessing_count);
        if (id < 0)
            return NAV_BACK;
        blitz_apply_effect_pick(id, true);
    }

    if (setup->curse_count > 0 || setup->blessing_count > 0)
        blitz_show_effect_summary();

    return NAV_OK;
}

static void blitz_auto_assign_stats(int stats[A_MAX])
{
    int cost = 0;

    for (int i = 0; i < A_MAX; i++)
        stats[i] = 0;

    while (cost < BIRTH_MAX_COST)
    {
        int choices[A_MAX];
        int choice_count = 0;

        for (int i = 0; i < A_MAX; i++)
        {
            int next = stats[i] + 1;
            int next_cost;

            if (next > 6)
                continue;
            next_cost = cost - birth_stat_cost(stats[i])
                + birth_stat_cost(next);
            if (next_cost <= BIRTH_MAX_COST)
                choices[choice_count++] = i;
        }

        if (choice_count <= 0)
            break;

        int pick = choices[rand_int(choice_count)];
        cost -= birth_stat_cost(stats[pick]);
        stats[pick]++;
        cost += birth_stat_cost(stats[pick]);
    }
}

static void blitz_auto_assign_skills(void)
{
    int old_base[S_MAX];
    int gains[S_MAX];
    int budget;

    for (int i = 0; i < S_MAX; i++)
    {
        old_base[i] = p_ptr->skill_base[i];
        gains[i] = 0;
    }

    budget = p_ptr->new_exp;

    while (budget > 0)
    {
        int choices[S_MAX];
        int weights[S_MAX];
        int choice_count = 0;
        int total_weight = 0;

        for (int i = 0; i < S_MAX; i++)
        {
            int delta;
            int weight = 2;

            if (i == S_SPC)
                continue;

            delta = birth_skill_cost(old_base[i], gains[i] + 1)
                - birth_skill_cost(old_base[i], gains[i]);
            if (delta <= 0 || delta > budget)
                continue;

            for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
            {
                int skill_idx = c_info[p_ptr->pcharacter].a_adj[slot][0];
                if (skill_idx < 0)
                    break;
                if (skill_idx == i)
                    weight += 3;
            }

            choices[choice_count] = i;
            weights[choice_count] = weight;
            total_weight += weight;
            choice_count++;
        }

        if (choice_count <= 0)
            break;

        int roll = rand_int(total_weight);
        int sum = 0;
        int chosen = choices[0];

        for (int i = 0; i < choice_count; i++)
        {
            sum += weights[i];
            if (roll < sum)
            {
                chosen = choices[i];
                break;
            }
        }

        budget -= birth_skill_cost(old_base[chosen], gains[chosen] + 1)
            - birth_skill_cost(old_base[chosen], gains[chosen]);
        gains[chosen]++;
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (i == S_SPC)
            continue;
        p_ptr->skill_base[i] = old_base[i] + gains[i];
    }

    p_ptr->new_exp = budget;
}

NavResult birth_blitz_auto_build_character(void)
{
    int stats[A_MAX];

    birth_prepare_character_extra();
    blitz_auto_assign_stats(stats);

    for (int i = 0; i < A_MAX; i++)
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

    blitz_auto_assign_skills();
    p_ptr->update |= PU_BONUS;
    update_stuff();
    p_ptr->chp = p_ptr->mhp;
    calc_voice();
    p_ptr->csp = p_ptr->msp;

    return NAV_OK;
}

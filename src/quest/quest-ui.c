/* File: quest-ui.c */
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
#include "metarun.h"
#include "platform-time.h"
#include "quest/quest.h"
#include "ui/ui-browser-shell.h"
#include "ui/ui-information-scene.h"
#include "log/log.h"

static cptr get_quest_title(int quest_idx)
{
    log_trace("QUEST TITLE: quest_idx=%d, z_info->quest_max=%d", quest_idx, z_info ? z_info->quest_max : -1);
    
    if (!z_info || quest_idx <= 0 || quest_idx >= z_info->quest_max || !quest_info) {
        log_trace("QUEST TITLE: Invalid bounds check, returning Unknown Quest");
        return "Unknown Quest";
    }
    
    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr) {
        log_trace("QUEST TITLE: q_ptr is NULL, returning Unknown Quest");
        return "Unknown Quest";
    }
    
    if (q_ptr->title_text && q_text) {
        log_trace("QUEST TITLE: Using title_text");
        return q_text + q_ptr->title_text;
    }
    
    /* Fallback to quest name */
    if (q_ptr->name && quest_name_text) {
        log_trace("QUEST TITLE: Using quest name fallback");
        return quest_name_text + q_ptr->name;
    }
    
    log_trace("QUEST TITLE: No valid text found, returning Unknown Quest");
    return "Unknown Quest";
}

/*
 * Get quest challenge description from quest data
 */
static cptr get_quest_challenge(int quest_idx)
{
    log_trace("QUEST CHALLENGE: quest_idx=%d, z_info->quest_max=%d", quest_idx, z_info ? z_info->quest_max : -1);
    
    if (!z_info || quest_idx <= 0 || quest_idx >= z_info->quest_max || !quest_info) {
        log_trace("QUEST CHALLENGE: Invalid bounds check, returning Unknown challenge");
        return "Unknown challenge";
    }
    
    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr) {
        log_trace("QUEST CHALLENGE: q_ptr is NULL, returning Unknown challenge");
        return "Unknown challenge";
    }
    
    if (q_ptr->challenge_text && q_text) {
        log_trace("QUEST CHALLENGE: Using challenge_text");
        return q_text + q_ptr->challenge_text;
    }
    
    log_trace("QUEST CHALLENGE: No valid text found, returning default");
    return "Face the unknown challenge";
}

/*
 * Get oath name from oath ID using oath_info data
 */
static cptr get_oath_name_from_id(byte oath_id)
{
    if (oath_id <= 0 || oath_id >= z_info->oath_max) return "No oath";
    
    oath_type* o_ptr = &oath_info[oath_id];
    if (o_ptr->name) {
        return oath_name_text + o_ptr->name;
    }
    
    /* Fallback to hardcoded names if oath_info not loaded */
    switch(oath_id) {
        case 0: return "No oath";
        case 1: return "Mercy oath";
        case 2: return "Silence oath";
        case 3: return "Iron oath";  
        case 4: return "Smith oath";
        default: return "Unknown oath";
    }
}

static cptr process_quest_placeholders(cptr text, int quest_idx);
static cptr get_quest_reward_text(int quest_idx);

enum {
    QUEST_STATUS_WRAP_WIDTH = 68,
    QUEST_TYPEWRITER_WRAP_WIDTH = 70,
    QUEST_TYPEWRITER_PAGE_LINE_MAX = 15
};

typedef struct quest_status_entry {
    int quest_id;
    int current_state;
    int rewarded_state;
    int metarun_quest_id;
    byte title_attr;
    byte meta_attr;
    char title[APP_UI_LABEL_MAX];
    char meta[APP_UI_META_MAX];
} quest_status_entry;

typedef struct quest_typewriter_scene_line {
    byte attr;
    char text[APP_UI_TEXT_MAX];
} quest_typewriter_scene_line;

typedef struct quest_typewriter_scene_state {
    byte title_attr;
    byte subtitle_attr;
    int line_count;
    char title[APP_UI_TITLE_MAX];
    char subtitle[APP_UI_TEXT_MAX];
    quest_typewriter_scene_line lines[QUEST_TYPEWRITER_PAGE_LINE_MAX];
} quest_typewriter_scene_state;

static bool quest_ui_panel_add_line(app_ui_panel* panel, byte attr, cptr text,
    bool detail_lines)
{
    cptr line = text;

    if (!panel)
        return false;

    if (!line || !line[0])
        line = " ";

    if (detail_lines)
        return app_ui_panel_add_detail_line(panel, attr, line);

    return app_ui_panel_add_body_line(panel, attr, line);
}

static bool quest_ui_panel_add_wrapped_lines(app_ui_panel* panel, byte attr,
    cptr text, bool detail_lines, size_t wrap_chars)
{
    const char* cursor = text;

    if (!panel || !text || !text[0])
        return true;
    if (wrap_chars < 8)
        wrap_chars = 8;

    while (*cursor)
    {
        const char* start;
        const char* split = NULL;
        size_t len = 0;
        char line[APP_UI_TEXT_MAX];

        while (*cursor == ' ' || *cursor == '\t')
            cursor++;

        if (*cursor == '\n')
        {
            cursor++;
            if (!quest_ui_panel_add_line(panel, attr, " ", detail_lines))
                return false;
            continue;
        }

        start = cursor;
        while (cursor[len] && cursor[len] != '\n')
        {
            if (len < wrap_chars)
            {
                if (cursor[len] == ' ' || cursor[len] == '\t')
                    split = cursor + len;
            }
            else if (split)
            {
                break;
            }
            else if (len >= sizeof(line) - 1u)
            {
                break;
            }
            len++;
        }

        if (cursor[len] && cursor[len] != '\n' && split && split > start)
            len = (size_t)(split - start);
        else if (len == 0 && cursor[len] && cursor[len] != '\n')
            len = 1;

        if (len >= sizeof(line))
            len = sizeof(line) - 1u;

        memcpy(line, start, len);
        line[len] = '\0';
        while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t'))
            line[--len] = '\0';

        if (line[0] && !quest_ui_panel_add_line(panel, attr, line,
                detail_lines))
        {
            return false;
        }

        cursor = start + len;
        while (*cursor == ' ' || *cursor == '\t')
            cursor++;
        if (*cursor == '\n')
            cursor++;
    }

    return true;
}

static void quest_typewriter_scene_set_text(quest_typewriter_scene_state* state,
    int row, byte attr, cptr text)
{
    if (!state || row < 0 || row >= QUEST_TYPEWRITER_PAGE_LINE_MAX)
    {
        return;
    }

    state->lines[row].attr = attr;
    SDL_strlcpy(state->lines[row].text, text ? text : "",
        sizeof(state->lines[row].text));
    if (row + 1 > state->line_count)
        state->line_count = row + 1;
}

static void quest_typewriter_scene_put_char(quest_typewriter_scene_state* state,
    int row, byte attr, char ch)
{
    size_t len;

    if (!state || row < 0 || row >= QUEST_TYPEWRITER_PAGE_LINE_MAX || ch == '\0')
    {
        return;
    }

    if (state->lines[row].attr == 0)
        state->lines[row].attr = attr;
    len = strlen(state->lines[row].text);
    if (len + 1 >= sizeof(state->lines[row].text))
        return;

    state->lines[row].text[len] = ch;
    state->lines[row].text[len + 1] = '\0';
    if (row + 1 > state->line_count)
        state->line_count = row + 1;
}

static bool quest_typewriter_scene_publish(
    const quest_typewriter_scene_state* state)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    int i;

    if (!state)
        return false;

    app_ui_scene_init(&scene);
    scene.flags = APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(&scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->accent_attr = state->title_attr;
    app_ui_panel_set_widths(panel, 760, 1180);
    app_ui_panel_set_title(panel, state->title_attr, state->title);
    if (state->subtitle[0])
        app_ui_panel_set_subtitle(panel, state->subtitle_attr,
            state->subtitle);

    for (i = 0; i < state->line_count; i++)
    {
        if (!quest_ui_panel_add_line(panel, state->lines[i].attr,
                state->lines[i].text[0] ? state->lines[i].text : " ", false))
        {
            return false;
        }
    }

    return ui_information_scene_present_ui(&scene);
}

static bool quest_typewriter_present(quest_typewriter_scene_state* state)
{
    if (!state)
        return false;

    if (quest_typewriter_scene_publish(state))
        return true;

    log_warn("quest typewriter: semantic scene publish failed");
    return false;
}

static void quest_typewriter_scene_init(quest_typewriter_scene_state* state,
    cptr title, byte title_attr)
{
    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    state->title_attr = title_attr;
    state->subtitle_attr = TERM_SLATE;
    SDL_strlcpy(state->title, title ? title : "", sizeof(state->title));
}

static void quest_typewriter_scene_clear(
    quest_typewriter_scene_state* state)
{
    if (!state)
        return;

    memset(state->lines, 0, sizeof(state->lines));
    state->line_count = 0;
}

static void quest_typewriter_scene_set_subtitle(
    quest_typewriter_scene_state* state, byte attr, cptr text)
{
    if (!state)
        return;

    state->subtitle_attr = attr;
    SDL_strlcpy(state->subtitle, text ? text : "", sizeof(state->subtitle));
}

static bool quest_typewriter_poll_skip_key(char* out_key)
{
    app_session* session = app_session_current();
    app_input input;

    if (!out_key || !session || !ui_information_scene_is_active())
        return false;

    while (app_session_pop_input(session, &input))
    {
        if (input.layer != APP_INPUT_LAYER_LEGACY
            || input.type != APP_INPUT_TYPE_KEY)
        {
            continue;
        }

        *out_key = (char)(input.payload.key.logical_key & 0xFFu);
        return true;
    }

    return false;
}

static int quest_typewriter_estimate_lines(cptr text, int wrap_width)
{
    int lines = 1;
    int col = 0;
    int i;

    if (!text || !text[0])
        return 1;
    if (wrap_width < 1)
        wrap_width = 1;

    for (i = 0; text[i]; i++)
    {
        if (text[i] == '\n')
        {
            lines++;
            col = 0;
            continue;
        }

        if (col >= wrap_width)
        {
            lines++;
            col = 0;
        }
        col++;
    }

    return lines;
}

static bool quest_typewriter_wait_for_continue(
    quest_typewriter_scene_state* state, cptr prompt)
{
    int key = ESCAPE;

    if (!state)
        return false;

    quest_typewriter_scene_set_subtitle(state, TERM_L_WHITE,
        prompt ? prompt : "Press any key to continue.");
    if (!quest_typewriter_present(state))
        return false;

    while (true)
    {
        ui_information_scene_event event;

        if (!ui_information_scene_wait_event(&event, APP_INPUT_FLAG_REPEAT))
            break;
        if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
        {
            key = event.key;
            break;
        }
        if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND)
        {
            const app_ui_command* command = &event.command;

            if (command->kind == APP_UI_COMMAND_KIND_CANCEL
                || command->target.action == APP_UI_WIDGET_ACTION_CANCEL)
            {
                key = ESCAPE;
                break;
            }
            if (command->kind == APP_UI_COMMAND_KIND_ACTIVATE
                || command->kind == APP_UI_COMMAND_KIND_SELECT
                || command->target.action == APP_UI_WIDGET_ACTION_ACTIVATE
                || command->target.action == APP_UI_WIDGET_ACTION_SELECT)
            {
                key = '\r';
                break;
            }
        }
    }
    if (key == ESCAPE || key == 'q' || key == 'Q')
        return false;

    return true;
}

static bool quest_typewriter_render_char(quest_typewriter_scene_state* state,
    int row, byte attr, char ch, bool skipped)
{
    quest_typewriter_scene_put_char(state, row, attr, ch);

    if (skipped)
        return true;
    if (!quest_typewriter_present(state))
        return false;

    platform_delay_ms(25u);
    return true;
}

static void quest_typewriter_begin_page(quest_typewriter_scene_state* state,
    bool skipped, cptr skip_prompt)
{
    quest_typewriter_scene_clear(state);
    quest_typewriter_scene_set_subtitle(state, TERM_SLATE,
        skipped ? "" : (skip_prompt ? skip_prompt : ""));
}

static cptr quest_status_oath_name(int quest_id)
{
    if (!z_info || !quest_info || quest_id <= 0 || quest_id >= z_info->quest_max)
        return "Unknown oath";

    return get_oath_name_from_id(quest_info[quest_id].oath_id);
}

static bool quest_status_append_entry(quest_status_entry* entries, int* count,
    int max_count, int quest_id, int current_state, int rewarded_state,
    int metarun_quest_id, cptr meta, byte meta_attr)
{
    quest_status_entry* entry;

    if (!entries || !count || *count < 0 || *count >= max_count)
        return false;

    entry = &entries[*count];
    memset(entry, 0, sizeof(*entry));
    entry->quest_id = quest_id;
    entry->current_state = current_state;
    entry->rewarded_state = rewarded_state;
    entry->metarun_quest_id = metarun_quest_id;
    entry->title_attr = TERM_YELLOW;
    entry->meta_attr = meta_attr;
    SDL_strlcpy(entry->title, get_quest_title(quest_id), sizeof(entry->title));
    SDL_strlcpy(entry->meta, meta ? meta : "", sizeof(entry->meta));
    (*count)++;
    return true;
}

static int quest_status_collect_entries(quest_status_entry* entries,
    int max_count)
{
    int count = 0;

    if (!entries || max_count <= 0)
        return 0;

    if (p_ptr->tulkas_quest > TULKAS_QUEST_NOT_STARTED)
    {
        cptr meta = "Unknown";
        byte meta_attr = TERM_SLATE;

        switch (p_ptr->tulkas_quest)
        {
        case TULKAS_QUEST_GIVER_PRESENT:
            meta = "Available";
            meta_attr = TERM_L_BLUE;
            break;
        case TULKAS_QUEST_ACTIVE:
            meta = "Active";
            meta_attr = TERM_WHITE;
            break;
        case TULKAS_QUEST_COMPLETE:
            meta = "Complete";
            meta_attr = TERM_L_GREEN;
            break;
        case TULKAS_QUEST_REWARDED:
            meta = "Rewarded";
            meta_attr = TERM_L_GREEN;
            break;
        }

        (void)quest_status_append_entry(entries, &count, max_count,
            QUEST_ID_TULKAS, p_ptr->tulkas_quest, TULKAS_QUEST_REWARDED,
            METARUN_QUEST_TULKAS, meta, meta_attr);
    }

    if (p_ptr->aule_quest > AULE_QUEST_NOT_STARTED)
    {
        cptr meta = "Unknown";
        byte meta_attr = TERM_SLATE;

        switch (p_ptr->aule_quest)
        {
        case AULE_QUEST_FORGE_PRESENT:
            meta = "Available";
            meta_attr = TERM_L_BLUE;
            break;
        case AULE_QUEST_ACTIVE:
            meta = "Active";
            meta_attr = TERM_WHITE;
            break;
        case AULE_QUEST_SUCCESS:
            meta = "Complete";
            meta_attr = TERM_L_GREEN;
            break;
        case AULE_QUEST_REWARDED:
            meta = "Rewarded";
            meta_attr = TERM_L_GREEN;
            break;
        }

        (void)quest_status_append_entry(entries, &count, max_count,
            QUEST_ID_AULE, p_ptr->aule_quest, AULE_QUEST_REWARDED,
            METARUN_QUEST_AULE, meta, meta_attr);
    }

    if (p_ptr->mandos_quest > MANDOS_QUEST_NOT_STARTED)
    {
        cptr meta = "Unknown";
        byte meta_attr = TERM_SLATE;

        switch (p_ptr->mandos_quest)
        {
        case MANDOS_QUEST_GIVER_PRESENT:
            meta = "Available";
            meta_attr = TERM_L_BLUE;
            break;
        case MANDOS_QUEST_ACTIVE:
            meta = "Active";
            meta_attr = TERM_WHITE;
            break;
        case MANDOS_QUEST_SUCCESS:
            meta = "Complete";
            meta_attr = TERM_L_GREEN;
            break;
        case MANDOS_QUEST_REWARDED:
            meta = "Rewarded";
            meta_attr = TERM_L_GREEN;
            break;
        }

        (void)quest_status_append_entry(entries, &count, max_count,
            QUEST_ID_MANDOS, p_ptr->mandos_quest, MANDOS_QUEST_REWARDED,
            METARUN_QUEST_MANDOS, meta, meta_attr);
    }

    if (p_ptr->niena_quest > NIENA_QUEST_NOT_STARTED)
    {
        cptr meta = "Unknown";
        byte meta_attr = TERM_SLATE;

        switch (p_ptr->niena_quest)
        {
        case NIENA_QUEST_GIVER_PRESENT:
            meta = "Available";
            meta_attr = TERM_L_BLUE;
            break;
        case NIENA_QUEST_ACTIVE:
            meta = "Active";
            meta_attr = TERM_WHITE;
            break;
        case NIENA_QUEST_SUCCESS:
            meta = "Complete";
            meta_attr = TERM_L_GREEN;
            break;
        case NIENA_QUEST_REWARDED:
            meta = "Rewarded";
            meta_attr = TERM_L_GREEN;
            break;
        case NIENA_QUEST_FAILED:
            meta = "Failed";
            meta_attr = TERM_RED;
            break;
        }

        (void)quest_status_append_entry(entries, &count, max_count,
            QUEST_ID_NIENA, p_ptr->niena_quest, NIENA_QUEST_REWARDED,
            METARUN_QUEST_NIENA, meta, meta_attr);
    }

    if (p_ptr->orome_quest > OROME_QUEST_NOT_STARTED)
    {
        cptr meta = "Unknown";
        byte meta_attr = TERM_SLATE;

        switch (p_ptr->orome_quest)
        {
        case OROME_QUEST_GIVER_PRESENT:
            meta = "Available";
            meta_attr = TERM_L_BLUE;
            break;
        case OROME_QUEST_ACTIVE:
            meta = "Active";
            meta_attr = TERM_WHITE;
            break;
        case OROME_QUEST_SUCCESS:
            meta = "Complete";
            meta_attr = TERM_L_GREEN;
            break;
        case OROME_QUEST_REWARDED:
            meta = "Rewarded";
            meta_attr = TERM_L_GREEN;
            break;
        }

        (void)quest_status_append_entry(entries, &count, max_count,
            QUEST_ID_OROME, p_ptr->orome_quest, OROME_QUEST_REWARDED,
            METARUN_QUEST_OROME, meta, meta_attr);
    }

    if (p_ptr->varda_quest > VARDA_QUEST_NOT_STARTED)
    {
        cptr meta = "Unknown";
        byte meta_attr = TERM_SLATE;

        switch (p_ptr->varda_quest)
        {
        case VARDA_QUEST_GIVER_PRESENT:
            meta = "Available";
            meta_attr = TERM_L_BLUE;
            break;
        case VARDA_QUEST_ACTIVE:
            meta = "Active";
            meta_attr = TERM_WHITE;
            break;
        case VARDA_QUEST_SUCCESS:
            meta = "Complete";
            meta_attr = TERM_L_GREEN;
            break;
        case VARDA_QUEST_REWARDED:
            meta = "Rewarded";
            meta_attr = TERM_L_GREEN;
            break;
        case VARDA_QUEST_FAILED:
            meta = "Failed";
            meta_attr = TERM_RED;
            break;
        }

        (void)quest_status_append_entry(entries, &count, max_count,
            QUEST_ID_VARDA, p_ptr->varda_quest, VARDA_QUEST_REWARDED,
            METARUN_QUEST_VARDA, meta, meta_attr);
    }

    return count;
}

static bool quest_status_add_reward_detail(app_ui_panel* panel, int quest_id,
    bool received)
{
    char buf[APP_UI_TEXT_MAX];

    strnfmt(buf, sizeof(buf), received ? "Reward: %s received" : "Reward: %s",
        get_quest_reward_text(quest_id));
    return quest_ui_panel_add_wrapped_lines(panel, TERM_SLATE, buf, true,
        QUEST_STATUS_WRAP_WIDTH);
}

static bool quest_status_add_challenge_detail(app_ui_panel* panel, int quest_id,
    byte attr)
{
    cptr challenge = get_quest_challenge(quest_id);

    if (quest_id == QUEST_ID_TULKAS)
        challenge = process_quest_placeholders(challenge, quest_id);

    return quest_ui_panel_add_wrapped_lines(panel, attr, challenge, true,
        QUEST_STATUS_WRAP_WIDTH);
}

static bool quest_status_add_previous_completion_detail(app_ui_panel* panel,
    const quest_status_entry* entry)
{
    int completed;
    char buf[APP_UI_TEXT_MAX];

    if (!panel || !entry)
        return false;

    completed = metarun_quest_completion_count(entry->metarun_quest_id);
    if (completed <= 0 || entry->current_state == entry->rewarded_state)
        return true;

    if (!quest_ui_panel_add_line(panel, TERM_L_DARK,
            "Previously completed in metarun:", true))
    {
        return false;
    }

    strnfmt(buf, sizeof(buf), "%s (metarun x%d)",
        quest_status_oath_name(entry->quest_id), completed);
    return quest_ui_panel_add_wrapped_lines(panel, TERM_SLATE, buf, true,
        QUEST_STATUS_WRAP_WIDTH);
}

static bool quest_status_build_entry_detail(app_ui_panel* panel,
    const quest_status_entry* entry)
{
    char buf[APP_UI_TEXT_MAX];

    if (!panel || !entry)
        return false;

    switch (entry->quest_id)
    {
    case QUEST_ID_TULKAS:
        switch (entry->current_state)
        {
        case TULKAS_QUEST_GIVER_PRESENT:
            return quest_ui_panel_add_line(panel, TERM_L_BLUE,
                       "Available - Tulkas awaits", true)
                && quest_status_add_challenge_detail(panel, entry->quest_id,
                    TERM_SLATE)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case TULKAS_QUEST_ACTIVE:
            return quest_status_add_challenge_detail(panel, entry->quest_id,
                       TERM_WHITE)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case TULKAS_QUEST_COMPLETE:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Complete - Return for reward", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case TULKAS_QUEST_REWARDED:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Completed by this character", true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    true);
        }
        break;

    case QUEST_ID_AULE:
        switch (entry->current_state)
        {
        case AULE_QUEST_FORGE_PRESENT:
            return quest_ui_panel_add_line(panel, TERM_L_BLUE,
                       "Available - Aulë awaits", true)
                && quest_status_add_challenge_detail(panel, entry->quest_id,
                    TERM_SLATE)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case AULE_QUEST_ACTIVE:
            return quest_ui_panel_add_line(panel, TERM_WHITE,
                       "Active - Seek the forge-halls", true)
                && quest_status_add_challenge_detail(panel, entry->quest_id,
                    TERM_SLATE)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case AULE_QUEST_SUCCESS:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Complete - Return for reward", true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case AULE_QUEST_REWARDED:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Completed by this character", true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    true);
        }
        break;

    case QUEST_ID_MANDOS:
        switch (entry->current_state)
        {
        case MANDOS_QUEST_GIVER_PRESENT:
            return quest_ui_panel_add_line(panel, TERM_L_BLUE,
                       "Available - Mandos waits beyond death", true)
                && quest_status_add_challenge_detail(panel, entry->quest_id,
                    TERM_SLATE)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case MANDOS_QUEST_ACTIVE:
            return quest_ui_panel_add_line(panel, TERM_WHITE,
                       "Active - Escape the houses of waiting", true)
                && quest_status_add_challenge_detail(panel, entry->quest_id,
                    TERM_SLATE)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case MANDOS_QUEST_SUCCESS:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Complete - Claim Mandos's favour", true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case MANDOS_QUEST_REWARDED:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Completed by this character", true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    true);
        }
        break;

    case QUEST_ID_NIENA:
        switch (entry->current_state)
        {
        case NIENA_QUEST_GIVER_PRESENT:
            return quest_ui_panel_add_line(panel, TERM_L_BLUE,
                       "Available - Niena offers mercy", true)
                && quest_status_add_challenge_detail(panel, entry->quest_id,
                    TERM_SLATE)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case NIENA_QUEST_ACTIVE:
            strnfmt(buf, sizeof(buf), "Monsters seen: %d  killed: %d",
                p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
            return quest_ui_panel_add_line(panel, TERM_WHITE,
                       "Active - Walk the path of mercy", true)
                && quest_ui_panel_add_line(panel, TERM_SLATE, buf, true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case NIENA_QUEST_SUCCESS:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Complete - Claim Niena's grace", true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case NIENA_QUEST_REWARDED:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Completed by this character", true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    true);
        case NIENA_QUEST_FAILED:
            strnfmt(buf, sizeof(buf), "Failed: %d seen, %d killed",
                p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
            return quest_ui_panel_add_line(panel, TERM_RED, buf, true)
                && quest_ui_panel_add_wrapped_lines(panel, TERM_SLATE,
                    "You took a life and lost Niena's mercy.", true,
                    QUEST_STATUS_WRAP_WIDTH)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        }
        break;

    case QUEST_ID_OROME:
        switch (entry->current_state)
        {
        case OROME_QUEST_GIVER_PRESENT:
            return quest_ui_panel_add_line(panel, TERM_L_BLUE,
                       "Available - Oromë awaits", true)
                && quest_status_add_challenge_detail(panel, entry->quest_id,
                    TERM_SLATE)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case OROME_QUEST_ACTIVE:
            strnfmt(buf, sizeof(buf), "Wolves killed: %d/100",
                p_ptr->orome_wolves_killed);
            if (!quest_ui_panel_add_line(panel, TERM_WHITE,
                    "Active - Hunt the fell kindreds", true)
                || !quest_ui_panel_add_line(panel,
                    p_ptr->orome_wolves_killed >= 100 ? TERM_L_GREEN
                                                      : TERM_SLATE,
                    buf, true))
            {
                return false;
            }
            strnfmt(buf, sizeof(buf), "Spiders killed: %d/80",
                p_ptr->orome_spiders_killed);
            if (!quest_ui_panel_add_line(panel,
                    p_ptr->orome_spiders_killed >= 80 ? TERM_L_GREEN
                                                      : TERM_SLATE,
                    buf, true))
            {
                return false;
            }
            strnfmt(buf, sizeof(buf), "Serpents killed: %d/60",
                p_ptr->orome_serpents_killed);
            if (!quest_ui_panel_add_line(panel,
                    p_ptr->orome_serpents_killed >= 60 ? TERM_L_GREEN
                                                       : TERM_SLATE,
                    buf, true))
            {
                return false;
            }
            strnfmt(buf, sizeof(buf), "Vampires killed: %d/30",
                p_ptr->orome_vampires_killed);
            return quest_ui_panel_add_line(panel,
                       p_ptr->orome_vampires_killed >= 30 ? TERM_L_GREEN
                                                          : TERM_SLATE,
                       buf, true)
                && quest_status_add_challenge_detail(panel, entry->quest_id,
                    TERM_SLATE)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case OROME_QUEST_SUCCESS:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Complete - Return for reward", true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case OROME_QUEST_REWARDED:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Completed by this character", true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    true);
        }
        break;

    case QUEST_ID_VARDA:
        switch (entry->current_state)
        {
        case VARDA_QUEST_GIVER_PRESENT:
            return quest_ui_panel_add_line(panel, TERM_L_BLUE,
                       "Available - Varda waits in sunlight", true)
                && quest_status_add_challenge_detail(panel, entry->quest_id,
                    TERM_SLATE)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case VARDA_QUEST_ACTIVE:
            return quest_ui_panel_add_line(panel, TERM_WHITE,
                       "Active - Seek Duruin's bastion", true)
                && quest_status_add_challenge_detail(panel, entry->quest_id,
                    TERM_SLATE)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case VARDA_QUEST_SUCCESS:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Complete - Claim Varda's blessing", true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    false)
                && quest_ui_panel_add_line(panel, TERM_WHITE, " ", true)
                && quest_status_add_previous_completion_detail(panel, entry);
        case VARDA_QUEST_REWARDED:
            return quest_ui_panel_add_line(panel, TERM_L_GREEN,
                       "Completed by this character", true)
                && quest_status_add_reward_detail(panel, entry->quest_id,
                    true);
        case VARDA_QUEST_FAILED:
            return quest_ui_panel_add_line(panel, TERM_RED,
                       "Failed - Duruin's Bastion was left behind", true)
                && quest_ui_panel_add_line(panel, TERM_SLATE,
                    "Leaving the first level reached after 500 ft without slaying Duruin ended Varda's quest.",
                    true);
        }
        break;
    }

    return quest_ui_panel_add_line(panel, TERM_SLATE, "No quest detail.",
        true);
}

static bool quest_status_build_browser_scene(app_ui_scene* scene,
    const quest_status_entry* entries, int entry_count, int selected)
{
    app_ui_panel* panel;
    ui_browser_shell_scene_config config;
    int i;

    if (!scene || !entries || entry_count <= 0)
        return false;

    if (selected < 0)
        selected = 0;
    if (selected >= entry_count)
        selected = entry_count - 1;

    ui_browser_shell_scene_config_init(&config);
    config.min_width_px = 980;
    config.width_cap_px = 1700;
    config.title_attr = TERM_YELLOW;
    config.title = "Quest Status";
    config.subtitle_attr = TERM_SLATE;
    config.subtitle = "Current run progress and metarun echoes";
    panel = ui_browser_shell_begin(scene, &config);
    if (!panel)
        return false;

    for (i = 0; i < entry_count; i++)
    {
        if (!app_ui_panel_add_row_ex(panel, (s16b)i, entries[i].title_attr,
                entries[i].meta_attr, 0, '\0', true, i == selected, "",
                entries[i].title, entries[i].meta))
        {
            return false;
        }
    }

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, entries[selected].title);
    if (!quest_status_build_entry_detail(panel, &entries[selected]))
        return false;

    {
        ui_browser_shell_footer_action actions[3];
        size_t count = 0;

        if (entry_count > 1)
        {
            actions[count++] = (ui_browser_shell_footer_action){
                1, TERM_WHITE, true, "8/2", "Move"
            };
        }
        actions[count++] = (ui_browser_shell_footer_action){
            2, TERM_WHITE, true, "Enter", "Back"
        };
        actions[count++] = (ui_browser_shell_footer_action){
            3, TERM_WHITE, true, "Esc", "Back"
        };
        (void)ui_browser_shell_add_footer_actions(panel, actions, count);
    }
    return true;
}

static char quest_status_command_key(const app_ui_command* command,
    int entry_count, int* selected, bool* handled)
{
    static const ui_browser_shell_button_key button_keys[] = {
        { 2, ESCAPE },
        { 3, ESCAPE }
    };
    ui_browser_shell_command_map map;
    ui_browser_shell_command_result result;

    if (handled)
        *handled = false;
    if (!command || !selected)
        return '\0';

    ui_browser_shell_command_map_init(&map);
    map.button_keys = button_keys;
    map.button_key_count = N_ELEMENTS(button_keys);
    map.row_activate_key = '\0';

    if (!ui_browser_shell_translate_command(command, &map, &result))
    {
        if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
        {
            if (handled)
                *handled = true;
            return ui_browser_shell_direction_command_key(command, NULL);
        }
        return '\0';
    }

    if (handled)
        *handled = true;

    if (result.role == APP_UI_WIDGET_ROLE_LIST_ITEM)
    {
        if (result.widget_id >= 0 && result.widget_id < entry_count)
            *selected = result.widget_id;
        return '\0';
    }

    return result.key;
}

static int quest_status_wait_key(int entry_count, int* selected)
{
    while (true)
    {
        ui_information_scene_event event;

        if (!ui_information_scene_wait_event(&event, APP_INPUT_FLAG_REPEAT))
            return ESCAPE;
        if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            return event.key;
        if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND)
        {
            bool handled = false;
            char key = quest_status_command_key(&event.command, entry_count,
                selected, &handled);

            if (key || handled)
                return key;
        }
    }
}

static bool quest_status_add_previous_completion_body_line(app_ui_panel* panel,
    int quest_id, int metarun_quest_id, int current_state, int rewarded_state)
{
    int completed;
    char buf[APP_UI_TEXT_MAX];

    completed = metarun_quest_completion_count(metarun_quest_id);
    if (completed <= 0 || current_state == rewarded_state)
        return true;

    strnfmt(buf, sizeof(buf), "%s - %s (metarun x%d)", get_quest_title(quest_id),
        quest_status_oath_name(quest_id), completed);
    return quest_ui_panel_add_wrapped_lines(panel, TERM_SLATE, buf, false,
        QUEST_STATUS_WRAP_WIDTH);
}

static bool quest_status_build_empty_scene(app_ui_scene* scene)
{
    app_ui_panel* panel;
    bool added_previous = false;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 640, 980);
    app_ui_panel_set_title(panel, TERM_YELLOW, "Quest Status");
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        "No active or completed quests this run");

    if (!quest_ui_panel_add_wrapped_lines(panel, TERM_SLATE,
            "Quest vaults may appear as you delve deeper...", false,
            QUEST_STATUS_WRAP_WIDTH))
    {
        return false;
    }

    if (metarun_quest_completion_count(METARUN_QUEST_TULKAS) > 0
        && p_ptr->tulkas_quest != TULKAS_QUEST_REWARDED)
    {
        added_previous = true;
    }
    if (metarun_quest_completion_count(METARUN_QUEST_AULE) > 0
        && p_ptr->aule_quest != AULE_QUEST_REWARDED)
    {
        added_previous = true;
    }
    if (metarun_quest_completion_count(METARUN_QUEST_MANDOS) > 0
        && p_ptr->mandos_quest != MANDOS_QUEST_REWARDED)
    {
        added_previous = true;
    }
    if (metarun_quest_completion_count(METARUN_QUEST_NIENA) > 0
        && p_ptr->niena_quest != NIENA_QUEST_REWARDED)
    {
        added_previous = true;
    }
    if (metarun_quest_completion_count(METARUN_QUEST_OROME) > 0
        && p_ptr->orome_quest != OROME_QUEST_REWARDED)
    {
        added_previous = true;
    }
    if (metarun_quest_completion_count(METARUN_QUEST_VARDA) > 0
        && p_ptr->varda_quest != VARDA_QUEST_REWARDED)
    {
        added_previous = true;
    }

    if (added_previous)
    {
        if (!quest_ui_panel_add_line(panel, TERM_WHITE, " ", false)
            || !quest_ui_panel_add_line(panel, TERM_L_DARK,
                "Previously completed in metarun:", false)
            || !quest_status_add_previous_completion_body_line(panel,
                QUEST_ID_TULKAS, METARUN_QUEST_TULKAS, p_ptr->tulkas_quest,
                TULKAS_QUEST_REWARDED)
            || !quest_status_add_previous_completion_body_line(panel,
                QUEST_ID_AULE, METARUN_QUEST_AULE, p_ptr->aule_quest,
                AULE_QUEST_REWARDED)
            || !quest_status_add_previous_completion_body_line(panel,
                QUEST_ID_MANDOS, METARUN_QUEST_MANDOS, p_ptr->mandos_quest,
                MANDOS_QUEST_REWARDED)
            || !quest_status_add_previous_completion_body_line(panel,
                QUEST_ID_NIENA, METARUN_QUEST_NIENA, p_ptr->niena_quest,
                NIENA_QUEST_REWARDED)
            || !quest_status_add_previous_completion_body_line(panel,
                QUEST_ID_OROME, METARUN_QUEST_OROME, p_ptr->orome_quest,
                OROME_QUEST_REWARDED)
            || !quest_status_add_previous_completion_body_line(panel,
                QUEST_ID_VARDA, METARUN_QUEST_VARDA, p_ptr->varda_quest,
                VARDA_QUEST_REWARDED))
        {
            return false;
        }
    }

    {
        const ui_browser_shell_footer_action actions[] = {
            { 1, TERM_WHITE, true, "Enter", "Back" },
            { 2, TERM_WHITE, true, "Esc", "Back" }
        };

        (void)ui_browser_shell_add_footer_actions(panel, actions,
            N_ELEMENTS(actions));
    }
    return true;
}

static bool do_cmd_quest_status_information_scene(void)
{
    ui_information_scene_scope scope;
    quest_status_entry entries[8];
    int entry_count;
    int selected = 0;

    if (!ui_information_scene_enter(&scope))
        return false;

    entry_count = quest_status_collect_entries(entries, (int)N_ELEMENTS(entries));
    if (entry_count <= 0)
    {
        app_ui_scene scene;

        if (!quest_status_build_empty_scene(&scene)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        (void)ui_information_scene_wait_dismissal(APP_INPUT_FLAG_REPEAT);
        ui_information_scene_leave(&scope);
        return true;
    }

    while (true)
    {
        app_ui_scene scene;
        int ch;
        int dir;

        if (!quest_status_build_browser_scene(&scene, entries, entry_count,
                selected)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = quest_status_wait_key(entry_count, &selected);
        dir = target_dir((char)ch);
        if (dir == 8)
        {
            selected = (selected + entry_count - 1) % entry_count;
            continue;
        }
        if (dir == 2)
        {
            selected = (selected + 1) % entry_count;
            continue;
        }
        if (ch == ESCAPE || ch == '\r' || ch == '\n' || ch == ' '
            || ch == 'q' || ch == 'Q')
        {
            break;
        }
    }

    ui_information_scene_leave(&scope);
    return true;
}

/*
 * Simple string search function - finds needle in haystack
 * Returns pointer to first occurrence, or NULL if not found
 */
static char* my_strstr(const char* haystack, const char* needle)
{
    if (!haystack || !needle) return NULL;
    
    int needle_len = strlen(needle);
    if (needle_len == 0) return (char*)haystack;
    
    for (const char* p = haystack; *p; p++) {
        int i;
        for (i = 0; i < needle_len && p[i] && p[i] == needle[i]; i++);
        if (i == needle_len) {
            return (char*)p;
        }
    }
    return NULL;
}

/*
 * Process placeholders in quest text (challenge, etc.) with actual values
 */
static cptr process_quest_placeholders(cptr text, int quest_idx)
{
    static char processed_buf[256];

    if (!text) {
        return "";
    }

    SDL_strlcpy(processed_buf, text, sizeof(processed_buf));
    
    if (quest_idx == QUEST_ID_TULKAS) {
        /* Replace [monster name] with actual monster name */
        char* monster_pos = my_strstr(processed_buf, "[monster name]");
        if (monster_pos && p_ptr->tulkas_target_r_idx > 0 && p_ptr->tulkas_target_r_idx < z_info->r_max) {
            monster_race* r_ptr = &r_info[p_ptr->tulkas_target_r_idx];
            char before[128], after[128];
            int before_len = monster_pos - processed_buf;
            SDL_strlcpy(before, processed_buf, before_len + 1);
            before[before_len] = '\0';
            SDL_strlcpy(after, monster_pos + 14, sizeof(after)); /* 14 = strlen("[monster name]") */
            strnfmt(processed_buf, sizeof(processed_buf), "%s%s%s", before, r_name + r_ptr->name, after);
        }
        
        /* Replace [artifact name] with actual artifact name */
        char* artifact_pos = my_strstr(processed_buf, "[artifact name]");
        if (artifact_pos && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
            artefact_type* a_ptr = &a_info[p_ptr->tulkas_prize_a_idx];
            char before[128], after[128];
            int before_len = artifact_pos - processed_buf;
            SDL_strlcpy(before, processed_buf, before_len + 1);
            before[before_len] = '\0';
            SDL_strlcpy(after, artifact_pos + 15, sizeof(after)); /* 15 = strlen("[artifact name]") */
            
            /* Get proper artifact name using object_desc */
            char artifact_name[120];
            if (a_ptr->name[0] != '\0') {
                /* Create a temporary object to get proper description */
                object_type temp_obj;
                object_wipe(&temp_obj);
                
                /* Set up the object as the artifact */
                s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
                if (k_idx > 0) {
                    object_prep(&temp_obj, k_idx);
                    temp_obj.name1 = p_ptr->tulkas_prize_a_idx;
                    temp_obj.ident |= IDENT_KNOWN;
                    
                    /* Get the full artifact description */
                    object_desc(artifact_name, sizeof(artifact_name), &temp_obj, true, 0);
                } else {
                    SDL_strlcpy(artifact_name, a_ptr->name, sizeof(artifact_name));
                }
            } else {
                SDL_strlcpy(artifact_name, "a legendary weapon", sizeof(artifact_name));
            }
            
            strnfmt(processed_buf, sizeof(processed_buf), "%s%s%s", before, artifact_name, after);
        }
    }
    
    return processed_buf;
}

/*
 * Get quest reward description for status display using actual quest data
 */
static cptr get_quest_reward_text(int quest_idx)
{
    static char reward_buf[200];
    char temp_buf[100];
    
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return "Unknown reward";
    
    quest_type* q_ptr = &quest_info[quest_idx];
    reward_buf[0] = '\0';
    
    /* Handle special Tulkas artifact reward */
    if (quest_idx == QUEST_ID_TULKAS && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
        artefact_type* a_ptr = &a_info[p_ptr->tulkas_prize_a_idx];
        if (a_ptr->name[0] != '\0') {
            /* Create a temporary object to get proper description */
            object_type temp_obj;
            object_wipe(&temp_obj);
            
            /* Set up the object as the artifact */
            s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
            if (k_idx > 0) {
                object_prep(&temp_obj, k_idx);
                temp_obj.name1 = p_ptr->tulkas_prize_a_idx;
                temp_obj.ident |= IDENT_KNOWN;
                
                /* Get the full artifact description */
                object_desc(reward_buf, sizeof(reward_buf), &temp_obj, true, 0);
                return reward_buf;
            } else {
                SDL_strlcpy(reward_buf, a_ptr->name, sizeof(reward_buf));
                return reward_buf;
            }
        }
    }
    
    /* Varda reward description */
    if (quest_idx == QUEST_ID_VARDA) {
        SDL_strlcpy(reward_buf, "Choose one radiant artefact and unlock the Oath of Light (+1 light radius)", sizeof(reward_buf));
        return reward_buf;
    }
    
    /* Build reward description from quest data */
    bool has_rewards = false;
    
    /* Check stat bonuses */
    if (q_ptr->stat_bonuses[0] || q_ptr->stat_bonuses[1] || q_ptr->stat_bonuses[2] || q_ptr->stat_bonuses[3]) {
        has_rewards = true;
        SDL_strlcat(reward_buf, "Stats: ", sizeof(reward_buf));
        
        if (q_ptr->stat_bonuses[0]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Str ", q_ptr->stat_bonuses[0]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[1]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Dex ", q_ptr->stat_bonuses[1]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[2]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Con ", q_ptr->stat_bonuses[2]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[3]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Gra ", q_ptr->stat_bonuses[3]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
    }
    
    /* Check skill bonuses */
    if (q_ptr->skill_type && q_ptr->skill_bonus) {
        if (has_rewards) SDL_strlcat(reward_buf, "| ", sizeof(reward_buf));
        has_rewards = true;
        
        /* Convert skill type to name */
        cptr skill_name = "Unknown";
        switch (q_ptr->skill_type) {
            case 0: skill_name = "Melee"; break;
            case 1: skill_name = "Archery"; break;
            case 2: skill_name = "Evasion"; break;
            case 3: skill_name = "Stealth"; break;
            case 4: skill_name = "Perception"; break;
            case 5: skill_name = "Will"; break;
            case 6: skill_name = "Smithing"; break;
            case 7: skill_name = "Song"; break;
        }
        strnfmt(temp_buf, sizeof(temp_buf), "+%d %s ", q_ptr->skill_bonus, skill_name);
        SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
    }
    
    /* Check special abilities */
    if (q_ptr->ability_type && q_ptr->ability_id < ABILITIES_MAX) {
        if (has_rewards) SDL_strlcat(reward_buf, "| ", sizeof(reward_buf));
        has_rewards = true;
        
        /* Get ability name from ability database */
        cptr ability_name = "Special ability";
        if (q_ptr->ability_type == S_SPC) { /* Special abilities type */
            /* Use ability_index to find the ability and get its name */
            int idx = ability_index(S_SPC, q_ptr->ability_id);
            if (idx >= 0 && idx < z_info->b_max) {
                ability_type* b_ptr = &b_info[idx];
                if (b_ptr->name) {
                    ability_name = b_name + b_ptr->name;
                }
            }
        }
        
        SDL_strlcat(reward_buf, ability_name, sizeof(reward_buf));
    }
    
    /* Check oath association */
    if (q_ptr->oath_id) {
        if (has_rewards) SDL_strlcat(reward_buf, " | ", sizeof(reward_buf));
        has_rewards = true;
        SDL_strlcat(reward_buf, get_oath_name_from_id(q_ptr->oath_id), sizeof(reward_buf));
    }
    
    if (!has_rewards) {
        SDL_strlcpy(reward_buf, "Unknown reward", sizeof(reward_buf));
    }
    
    return reward_buf;
}

/*
 * Show quest status for current metarun - only active and completed quests
 * Now uses quest.txt data instead of hardcoded values
 */
void do_cmd_quest_status(void)
{
    log_trace("QUEST STATUS: do_cmd_quest_status() called");

    /* Safety check: ensure we have a valid player and metarun */
    if (!p_ptr) {
        log_trace("QUEST STATUS: No player data available");
        msg_print("No character data available.");
        return;
    }

    log_trace("QUEST STATUS: Player exists, quest states - Tulkas: %d, Aule: %d, Mandos: %d",
              p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest);

    if (!do_cmd_quest_status_information_scene())
    {
        log_warn("quest status: information-scene presentation failed on the snapshot renderer path");
        msg_print("Quest status viewer unavailable.");
    }
}

/*
 * Quest typewriter menu function - displays quest dialog with typewriter effect
 * Based on print_story_intro() style
 */
void quest_typewriter_menu(cptr title, cptr texts[], int total_texts, byte title_color, byte text_color)
{
    const char* skip_prompt = "Enter/Esc skip typing";
    const char* continue_prompt = "Press any key to continue. Q/Esc closes.";
    const char* final_prompt = "Press any key to return.";
    bool skipped = false;
    bool scene_failed = false;
    bool aborted = false;
    ui_information_scene_scope info_scope;
    quest_typewriter_scene_state scene_state;
    int wrap_width = QUEST_TYPEWRITER_WRAP_WIDTH;
    int row = 0;
    int col = 0;

    if (!ui_information_scene_enter(&info_scope))
    {
        log_warn("quest typewriter: semantic scene required");
        msg_print("Quest dialog unavailable.");
        return;
    }

    quest_typewriter_scene_init(&scene_state, title, title_color);
    quest_typewriter_begin_page(&scene_state, skipped, skip_prompt);
    if (!quest_typewriter_present(&scene_state))
    {
        scene_failed = true;
        goto cleanup;
    }

    for (int idx = 0; idx < total_texts; idx++)
    {
        const char *s = texts[idx];

        if (!s || !s[0])
        {
            if (row >= QUEST_TYPEWRITER_PAGE_LINE_MAX)
            {
                if (!quest_typewriter_wait_for_continue(&scene_state,
                        continue_prompt))
                {
                    aborted = true;
                    goto cleanup;
                }

                quest_typewriter_begin_page(&scene_state, skipped, skip_prompt);
                row = 0;
            }

            quest_typewriter_scene_set_text(&scene_state, row, text_color, " ");
            row++;
            col = 0;

            if (!skipped)
                platform_delay_ms(200u);
            continue;
        }

        if (row > 0
            && row + quest_typewriter_estimate_lines(s, wrap_width)
                > QUEST_TYPEWRITER_PAGE_LINE_MAX)
        {
            if (!quest_typewriter_wait_for_continue(&scene_state,
                    continue_prompt))
            {
                aborted = true;
                goto cleanup;
            }

            quest_typewriter_begin_page(&scene_state, skipped, skip_prompt);
            row = 0;
            col = 0;
        }

        col = 0;

        for (int i = 0; s[i]; )
        {
            if (s[i] == '\n')
            {
                row++;
                if (row >= QUEST_TYPEWRITER_PAGE_LINE_MAX)
                {
                    if (!quest_typewriter_wait_for_continue(&scene_state,
                            continue_prompt))
                    {
                        aborted = true;
                        goto cleanup;
                    }

                    quest_typewriter_begin_page(&scene_state, skipped,
                        skip_prompt);
                    row = 0;
                }
                col = 0;
                i++;
                continue;
            }

            while (s[i] == ' ' || s[i] == '\t')
            {
                int spaces = (s[i] == '\t') ? (4 - (col % 4)) : 1;

                if (col == 0)
                {
                    i++;
                    continue;
                }

                while (spaces-- > 0 && col < wrap_width)
                {
                    if (!quest_typewriter_render_char(&scene_state, row,
                            text_color, ' ', skipped))
                    {
                        scene_failed = true;
                        goto cleanup;
                    }
                    col++;
                }
                i++;
            }

            if (!s[i] || s[i] == '\n')
                continue;

            {
                int word_start = i;
                int word_len = 0;

                while (s[i + word_len] && s[i + word_len] != ' '
                    && s[i + word_len] != '\t' && s[i + word_len] != '\n')
                {
                    word_len++;
                }

                if (col > 0 && col + word_len > wrap_width)
                {
                    row++;
                    if (row >= QUEST_TYPEWRITER_PAGE_LINE_MAX)
                    {
                        if (!quest_typewriter_wait_for_continue(&scene_state,
                                continue_prompt))
                        {
                            aborted = true;
                            goto cleanup;
                        }

                        quest_typewriter_begin_page(&scene_state, skipped,
                            skip_prompt);
                        row = 0;
                    }
                    col = 0;
                }

                for (int j = 0; j < word_len; j++)
                {
                    char check_key;

                    if (!skipped && quest_typewriter_poll_skip_key(&check_key)
                        && (check_key == ESCAPE || check_key == '\n'
                            || check_key == '\r'))
                    {
                        skipped = true;
                        quest_typewriter_scene_set_subtitle(&scene_state,
                            TERM_SLATE, "");
                    }

                    if (!quest_typewriter_render_char(&scene_state, row,
                            text_color, s[word_start + j], skipped))
                    {
                        scene_failed = true;
                        goto cleanup;
                    }
                    col++;
                    if (col >= wrap_width && j + 1 < word_len)
                    {
                        row++;
                        if (row >= QUEST_TYPEWRITER_PAGE_LINE_MAX)
                        {
                            if (!quest_typewriter_wait_for_continue(
                                    &scene_state, continue_prompt))
                            {
                                aborted = true;
                                goto cleanup;
                            }

                            quest_typewriter_begin_page(&scene_state, skipped,
                                skip_prompt);
                            row = 0;
                        }
                        col = 0;
                    }
                }

                i += word_len;
            }
        }

        row++;
        if (row >= QUEST_TYPEWRITER_PAGE_LINE_MAX && idx + 1 < total_texts)
        {
            if (!quest_typewriter_wait_for_continue(&scene_state,
                    continue_prompt))
            {
                aborted = true;
                goto cleanup;
            }

            quest_typewriter_begin_page(&scene_state, skipped, skip_prompt);
            row = 0;
        }
        col = 0;

        if (!skipped)
            platform_delay_ms(400u);
    }

    if (skipped && !quest_typewriter_present(&scene_state))
    {
        scene_failed = true;
        goto cleanup;
    }

    if (!quest_typewriter_wait_for_continue(&scene_state, final_prompt))
    {
        aborted = true;
        goto cleanup;
    }

cleanup:
    quest_typewriter_scene_clear(&scene_state);
    ui_information_scene_leave(&info_scope);

    if (scene_failed && !aborted)
        msg_print("Quest dialog unavailable.");
}

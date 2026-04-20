/* File: score_ui-run-history.c */
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

/*
 * Lane-local run history detail helpers split from score_ui.c.
 */

#include "angband.h"
#include "log/log.h"
#include "platform-input.h"
#include "score/score_runs.h"
#include "score/score_ui-browser.h"
#include "score/score_ui-run-history.h"
#include "ui/ui-information-scene.h"
#include <time.h>

bool score_ui_run_history_is_current(const run_history_entry* entry)
{
    if (!entry)
        return false;
    if (!character_generated || !p_ptr || p_ptr->is_dead)
        return false;
    if (entry->record.status != SCORE_RECORD_ALIVE)
        return false;
    return (entry->record.metarun_id == metar.id);
}

const char* score_ui_run_history_race_name(byte idx)
{
    if (!p_info || !p_name || !z_info || idx >= z_info->p_max)
        return "<unknown>";
    return p_name + p_info[idx].name;
}

static const char* run_history_monster_name(u16b r_idx)
{
    if (!r_info || !r_name || !z_info || r_idx == 0 || r_idx >= z_info->r_max)
        return "<unknown>";
    return r_name + r_info[r_idx].name;
}

const char* score_ui_run_status_label(score_record_status status)
{
    switch (status) {
    case SCORE_RECORD_ALIVE: return "Alive";
    case SCORE_RECORD_DEAD: return "Dead";
    case SCORE_RECORD_ESCAPED: return "Escaped";
    default: return "Unknown";
    }
}

void score_ui_run_history_format_timestamp(u32b utc, bool include_time,
    char* out, size_t out_len)
{
    if (!out || out_len == 0)
        return;

    out[0] = '\0';
    if (!utc)
    {
        SDL_strlcpy(out, "----", out_len);
        return;
    }

    time_t ts = (time_t)utc;
    struct tm* tm_info = localtime(&ts);
    if (!tm_info)
    {
        SDL_strlcpy(out, "----", out_len);
        return;
    }

    if (include_time)
    {
        if (strftime(out, out_len, "%Y-%m-%d %H:%M", tm_info) == 0)
            SDL_strlcpy(out, "----", out_len);
    }
    else if (strftime(out, out_len, "%Y-%m-%d", tm_info) == 0)
    {
        SDL_strlcpy(out, "----", out_len);
    }
}

static bool run_history_prepare_artefact_object(
    const score_run_artefact_v1* entry, object_type* out)
{
    if (!entry || !out || !z_info)
        return false;
    if (entry->a_idx <= 0 || entry->a_idx >= z_info->art_max)
        return false;

    object_wipe(out);

#ifdef ALLOW_SPOILERS
    if (make_fake_artefact(out, (byte)entry->a_idx))
        goto prepared;
#endif

    artefact_type* art = &a_info[entry->a_idx];
    if (!art || (art->tval == 0 && art->sval == 0))
        return false;

    s16b k_idx = lookup_kind(art->tval, art->sval);
    if (k_idx <= 0)
        return false;

    object_prep(out, k_idx);
    out->name1 = (byte)entry->a_idx;
    out->pval = art->pval;
    out->att = art->att;
    out->dd = art->dd;
    out->ds = art->ds;
    out->evn = art->evn;
    out->pd = art->pd;
    out->ps = art->ps;
    out->weight = art->weight;

    for (int i = 0; i < art->abilities; i++)
    {
        out->skilltype[i + out->abilities] = art->skilltype[i];
        out->abilitynum[i + out->abilities] = art->abilitynum[i];
    }
    out->abilities += art->abilities;

    if (art->flags3 & (TR3_LIGHT_CURSE))
        out->ident |= IDENT_CURSED;

prepared:
    out->ident |= IDENT_KNOWN | IDENT_SENSE;
    object_known(out);
    return true;
}


static const char* run_detail_panel_names[RUN_PANEL_COUNT] = {
    "General", "Stats", "Abilities", "Milestones", "Artefacts", "Monsters"
};

#define RUN_HISTORY_UI_PAGE_STEP 8
#define RUN_HISTORY_UI_LIST_WINDOW 64

static int* run_history_build_monster_order(
    const score_run_detail_block* details, run_monster_sort_mode mode,
    int total);
static const char* run_history_format_depth_label(
    const score_run_milestone_v1* entry, char* buffer, size_t len);
static const char* run_history_monster_sort_labels[RUN_MON_SORT_COUNT];

static void run_history_ui_add_tabs(app_ui_panel* panel,
    run_detail_panel active, const bool available[RUN_PANEL_COUNT])
{
    int i;

    if (!panel)
        return;

    for (i = 0; i < RUN_PANEL_COUNT; i++)
    {
        byte attr = available && available[i] ? TERM_L_WHITE : TERM_SLATE;

        if (i == (int)active)
            attr = available && available[i] ? TERM_L_BLUE : TERM_SLATE;
        (void)app_ui_panel_add_tab(panel, (s16b)i, attr,
            i == (int)active, run_detail_panel_names[i]);
    }
}

static bool run_history_ui_add_section_row(app_ui_panel* panel, cptr label)
{
    app_ui_row* row;

    if (!panel || !label)
        return false;
    if (!app_ui_panel_add_row_ex(panel, -1, TERM_L_BLUE, TERM_L_BLUE,
            0, '\0', true, false, "", label, ""))
    {
        return false;
    }

    row = &panel->rows[panel->row_count - 1];
    row->flags |= APP_UI_ITEM_FLAG_SECTION;
    return true;
}

static bool run_history_ui_add_value_row(app_ui_panel* panel, byte label_attr,
    cptr label, byte value_attr, cptr value)
{
    return app_ui_panel_add_row_ex(panel, -1, label_attr, value_attr, 0, '\0',
        true, false, "", label ? label : "", value ? value : "");
}

static bool run_history_ui_handle_scroll_key(int* offset, int ch, int total)
{
    int delta = 0;

    if (!offset || total <= 0)
        return false;

    switch (ch)
    {
    case '2':
    case 'j':
    case 'J':
#ifdef ARROW_DOWN
    case ARROW_DOWN:
#endif
        delta = 1;
        break;
    case '8':
    case 'k':
    case 'K':
#ifdef ARROW_UP
    case ARROW_UP:
#endif
        delta = -1;
        break;
    case '3':
    case 'n':
    case 'N':
        delta = RUN_HISTORY_UI_PAGE_STEP;
        break;
    case '-':
    case '7':
    case 'p':
    case 'P':
        delta = -RUN_HISTORY_UI_PAGE_STEP;
        break;
    default:
        return false;
    }

    *offset += delta;
    if (*offset < 0)
        *offset = 0;
    if (*offset >= total)
        *offset = total - 1;
    return true;
}

static void run_history_ui_window(int total, int highlight, int* start,
    int* end)
{
    int window_start;

    if (start)
        *start = 0;
    if (end)
        *end = 0;
    if (total <= 0)
        return;

    window_start = highlight - (RUN_HISTORY_UI_LIST_WINDOW / 2);
    if (window_start < 0)
        window_start = 0;
    if (window_start > total - RUN_HISTORY_UI_LIST_WINDOW)
        window_start = MAX(0, total - RUN_HISTORY_UI_LIST_WINDOW);

    if (start)
        *start = window_start;
    if (end)
        *end = MIN(total, window_start + RUN_HISTORY_UI_LIST_WINDOW);
}

static void run_history_ui_build_header(char* title, size_t title_size,
    char* subtitle, size_t subtitle_size, const score_record_v1* rec,
    cptr player, cptr race_name, cptr status_label)
{
    if (title && title_size > 0)
        strnfmt(title, title_size, "Run #%u Details",
            rec ? rec->record_id : 0u);
    if (subtitle && subtitle_size > 0)
    {
        strnfmt(subtitle, subtitle_size, "%s  |  %s  |  %s",
            player ? player : "<unknown>",
            race_name ? race_name : "<unknown>",
            status_label ? status_label : "<unknown>");
    }
}

static void run_history_ui_add_general_rows(app_ui_panel* panel,
    const score_record_v1* rec, const run_history_entry* entry, cptr created,
    cptr completed, bool current_run)
{
    char buf[APP_UI_META_MAX];
    byte status_color;

    if (!panel || !rec || !entry)
        return;

    status_color = (rec->status == SCORE_RECORD_ALIVE) ? TERM_L_GREEN
        : (rec->status == SCORE_RECORD_DEAD) ? TERM_L_RED
        : TERM_ORANGE;

    (void)run_history_ui_add_section_row(panel, "Run");
    strnfmt(buf, sizeof(buf), "%d points", entry->rating);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Rating", TERM_WHITE,
        buf);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Status",
        status_color, score_ui_run_status_label(rec->status));
    if (current_run)
    {
        strnfmt(buf, sizeof(buf), "%s (run in progress)", created);
        (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Started",
            TERM_L_GREEN, buf);
    }
    else
    {
        (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Started",
            TERM_SLATE, created);
        (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Completed",
            TERM_SLATE, completed);
    }

    (void)run_history_ui_add_section_row(panel, "Progress");
    strnfmt(buf, sizeof(buf), "%d ft", rec->max_depth * 50);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Max depth",
        TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "%d ft", rec->exit_depth * 50);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Exit depth",
        TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "%u", (unsigned)rec->silmarils);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Silmarils",
        rec->silmarils > 0 ? TERM_VIOLET : TERM_L_DARK, buf);

    (void)run_history_ui_add_section_row(panel, "Totals");
    strnfmt(buf, sizeof(buf), "%u", (unsigned)rec->quests_completed);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Quests completed",
        TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "%u", (unsigned)rec->uniques_killed);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Uniques defeated",
        rec->uniques_killed > 0 ? TERM_YELLOW : TERM_L_DARK, buf);
    strnfmt(buf, sizeof(buf), "%u", (unsigned)rec->artefacts_found);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Artefacts found",
        rec->artefacts_found > 0 ? TERM_YELLOW : TERM_L_DARK, buf);
    strnfmt(buf, sizeof(buf), "%u", (unsigned)rec->skills_learned);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Skills learned",
        TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "%u", (unsigned)rec->abilities_learned);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Abilities learned",
        TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "%lu", (unsigned long)rec->kills_seen);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Monsters seen",
        TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "%lu", (unsigned long)rec->kills_total);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Monsters killed",
        TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "%lu", (unsigned long)rec->xp_earned);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Experience gained",
        TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "%lu", (unsigned long)rec->turns_spent);
    (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, "Turns spent",
        TERM_WHITE, buf);

    if (rec->status == SCORE_RECORD_DEAD && rec->cause_of_death[0])
    {
        (void)run_history_ui_add_section_row(panel, "Death");
        (void)run_history_ui_add_value_row(panel, TERM_L_RED, "Cause",
            TERM_L_RED, rec->cause_of_death);
    }
}

static void run_history_ui_add_stats_rows(app_ui_panel* panel,
    const score_run_detail_block* details)
{
    int i;

    if (!panel || !details)
        return;

    (void)run_history_ui_add_section_row(panel, "Stats");
    if (!details->stats || details->stats_count == 0)
    {
        (void)run_history_ui_add_value_row(panel, TERM_L_DARK, "No stat data",
            TERM_L_DARK, "Recorded stat data is unavailable for this run.");
    }
    else
    {
        for (i = 0; i < details->stats_count; i++)
        {
            const score_run_stat_v1* entry = &details->stats[i];
            const char* label = (entry->stat_index < A_MAX)
                ? stat_names_full[entry->stat_index]
                : "<unknown>";
            char meta[APP_UI_META_MAX];

            strnfmt(meta, sizeof(meta), "Base %d  Drain %d  Current %d",
                entry->base, entry->drain, entry->current);
            (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, label,
                TERM_WHITE, meta);
        }
    }

    (void)run_history_ui_add_section_row(panel, "Skills");
    if (!details->skills || details->skills_count == 0)
    {
        (void)run_history_ui_add_value_row(panel, TERM_L_DARK, "No skill data",
            TERM_L_DARK, "Recorded skill data is unavailable for this run.");
    }
    else
    {
        for (i = 0; i < details->skills_count; i++)
        {
            const score_run_skill_v1* entry = &details->skills[i];
            const char* label = (entry->skill_index < S_MAX)
                ? skill_names_full[entry->skill_index]
                : "<unknown>";
            char meta[APP_UI_META_MAX];

            strnfmt(meta, sizeof(meta),
                "Base %d  Current %d  Stat %+d  Other %+d",
                entry->base, entry->current, entry->stat_bonus,
                entry->item_bonus);
            (void)run_history_ui_add_value_row(panel, TERM_L_WHITE, label,
                TERM_WHITE, meta);
        }
    }
}

static void run_history_ui_add_list_footer(app_ui_panel* panel,
    run_detail_panel active_panel, bool can_inspect, bool can_sort)
{
    if (!panel)
        return;

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "4/6", "View");
    if (active_panel == RUN_PANEL_GENERAL || active_panel == RUN_PANEL_STATS)
    {
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "8/2", "Scroll");
    }
    else
    {
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "8/2", "Move");
    }
    if (can_inspect)
    {
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "Enter", "Inspect");
    }
    if (can_sort)
    {
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "s", "Sort");
    }
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
}

static void run_history_ui_add_ability_rows(app_ui_panel* panel,
    const score_run_detail_block* details, run_detail_list_state* state)
{
    int total = details ? details->ability_count : 0;
    int start;
    int end;

    if (!panel || !details || total <= 0)
        return;
    if (state->highlight < 0)
        state->highlight = 0;
    if (state->highlight >= total)
        state->highlight = total - 1;

    run_history_ui_window(total, state->highlight, &start, &end);
    for (int idx = start; idx < end; idx++)
    {
        const score_run_ability_v1* entry = &details->abilities[idx];
        const char* skill = (entry->skill_index < S_MAX)
            ? skill_names_full[entry->skill_index]
            : "<unknown skill>";
        const char* ability_name = "<unknown ability>";
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];

        if (entry->skill_index < S_MAX && entry->ability_index < ABILITIES_MAX)
        {
            ability_type* b_ptr = &b_info[ability_index(entry->skill_index,
                entry->ability_index)];

            if (b_ptr && b_ptr->name && b_name)
                ability_name = b_name + b_ptr->name;
        }

        strnfmt(label, sizeof(label), "%s - %s", skill, ability_name);
        strnfmt(meta, sizeof(meta), "Turn %lu  %d ft",
            (unsigned long)entry->player_turn, entry->depth * 50);
        (void)app_ui_panel_add_row_ex(panel, (s16b)idx,
            TERM_WHITE, TERM_SLATE, 0, '\0', true,
            idx == state->highlight, "", label, meta);
    }

    if (state->highlight >= 0 && state->highlight < total)
    {
        const score_run_ability_v1* entry = &details->abilities[state->highlight];
        const char* skill = (entry->skill_index < S_MAX)
            ? skill_names_full[entry->skill_index]
            : "<unknown skill>";
        const char* ability_name = "<unknown ability>";
        char buf[APP_UI_TEXT_MAX];

        if (entry->skill_index < S_MAX && entry->ability_index < ABILITIES_MAX)
        {
            ability_type* b_ptr = &b_info[ability_index(entry->skill_index,
                entry->ability_index)];

            if (b_ptr && b_ptr->name && b_name)
                ability_name = b_name + b_ptr->name;
        }

        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Ability");
        (void)app_ui_panel_add_detail_line(panel, TERM_L_WHITE, ability_name);
        strnfmt(buf, sizeof(buf), "Skill: %s", skill);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
        strnfmt(buf, sizeof(buf), "Order: %u", entry->order);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
        strnfmt(buf, sizeof(buf), "Turn: %lu", (unsigned long)entry->player_turn);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
        strnfmt(buf, sizeof(buf), "Depth: %d ft", entry->depth * 50);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
    }
}

static void run_history_ui_add_milestone_rows(app_ui_panel* panel,
    const score_run_detail_block* details, run_detail_list_state* state)
{
    int total = details ? details->milestone_count : 0;
    int start;
    int end;

    if (!panel || !details || total <= 0)
        return;
    if (state->highlight < 0)
        state->highlight = 0;
    if (state->highlight >= total)
        state->highlight = total - 1;

    run_history_ui_window(total, state->highlight, &start, &end);
    for (int idx = start; idx < end; idx++)
    {
        const score_run_milestone_v1* entry = &details->milestones[idx];
        char depth_buf[16];
        char meta[APP_UI_META_MAX];

        strnfmt(meta, sizeof(meta), "Turn %lu  %s",
            (unsigned long)entry->player_turn,
            run_history_format_depth_label(entry, depth_buf, sizeof(depth_buf)));
        (void)app_ui_panel_add_row_ex(panel, (s16b)idx, TERM_WHITE,
            TERM_SLATE, 0, '\0', true, idx == state->highlight, "",
            entry->note[0] ? entry->note : "(no note)", meta);
    }

    if (state->highlight >= 0 && state->highlight < total)
    {
        const score_run_milestone_v1* entry
            = &details->milestones[state->highlight];
        char depth_buf[16];
        char buf[APP_UI_TEXT_MAX];

        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Milestone");
        (void)app_ui_panel_add_detail_line(panel, TERM_L_WHITE,
            entry->note[0] ? entry->note : "(no note)");
        strnfmt(buf, sizeof(buf), "Turn: %lu",
            (unsigned long)entry->player_turn);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
        strnfmt(buf, sizeof(buf), "Depth: %s",
            run_history_format_depth_label(entry, depth_buf, sizeof(depth_buf)));
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
    }
}

static void run_history_ui_add_artefact_rows(app_ui_panel* panel,
    const score_run_detail_block* details, run_detail_list_state* state)
{
    int total;
    int start;
    int end;

    if (!panel || !details)
        return;

    total = details->header.artefact_count;
    if (total > details->header.artefact_capacity)
        total = details->header.artefact_capacity;
    if (total <= 0)
        return;
    if (state->highlight < 0)
        state->highlight = 0;
    if (state->highlight >= total)
        state->highlight = total - 1;

    run_history_ui_window(total, state->highlight, &start, &end);
    for (int idx = start; idx < end; idx++)
    {
        const score_run_artefact_v1* entry = &details->artefacts[idx];
        object_type temp_obj;
        char full_desc[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        byte icon_attr = TERM_WHITE;
        char icon_char = '?';

        full_desc[0] = '\0';
        meta[0] = '\0';
        if (run_history_prepare_artefact_object(entry, &temp_obj))
        {
            object_desc(full_desc, sizeof(full_desc), &temp_obj, true, 0);
            icon_attr = object_attr(&temp_obj);
            icon_char = object_char(&temp_obj);
        }
        else if (z_info && entry->a_idx > 0 && entry->a_idx < z_info->art_max)
        {
            artefact_type* art = &a_info[entry->a_idx];

            if (art && art->name[0])
                SDL_strlcpy(full_desc, art->name, sizeof(full_desc));
        }
        if (!full_desc[0])
            SDL_strlcpy(full_desc, "<unknown artefact>", sizeof(full_desc));
        SDL_strlcpy(meta, entry->forged ? "Forged" : "Artefact", sizeof(meta));

        (void)app_ui_panel_add_row_ex(panel, (s16b)idx, TERM_YELLOW,
            TERM_SLATE, icon_attr, icon_char, true, idx == state->highlight,
            "", full_desc, meta);
    }

    if (state->highlight >= 0 && state->highlight < total)
    {
        const score_run_artefact_v1* entry = &details->artefacts[state->highlight];
        object_type temp_obj;
        char desc[APP_UI_TEXT_MAX];

        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Artefact");
        if (run_history_prepare_artefact_object(entry, &temp_obj))
            object_desc(desc, sizeof(desc), &temp_obj, true, 0);
        else
            SDL_strlcpy(desc, "<unknown artefact>", sizeof(desc));
        (void)app_ui_panel_add_detail_line(panel, TERM_YELLOW, desc);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            entry->forged ? "Forged during this run." : "Recovered artefact.");
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
            "Press Enter to inspect.");
    }
}

static void run_history_ui_add_monster_rows(app_ui_panel* panel,
    const score_run_detail_block* details, run_detail_list_state* state,
    run_monster_sort_mode sort_mode)
{
    int total;
    int start;
    int end;
    int* order;

    if (!panel || !details)
        return;

    total = details->header.monster_count;
    if (total > details->header.monster_capacity)
        total = details->header.monster_capacity;
    if (total <= 0)
        return;
    if (state->highlight < 0)
        state->highlight = 0;
    if (state->highlight >= total)
        state->highlight = total - 1;

    order = run_history_build_monster_order(details, sort_mode, total);
    if (!order)
        return;

    run_history_ui_window(total, state->highlight, &start, &end);
    for (int idx = start; idx < end; idx++)
    {
        const score_run_monster_v1* entry = &details->monsters[order[idx]];
        const char* name = run_history_monster_name(entry->r_idx);
        monster_race* r_ptr = NULL;
        char meta[APP_UI_META_MAX];
        byte icon_attr = TERM_WHITE;
        char icon_char = '?';

        if (z_info && entry->r_idx > 0 && entry->r_idx < z_info->r_max)
            r_ptr = &r_info[entry->r_idx];
        if (r_ptr)
        {
            icon_attr = monster_attr(r_ptr);
            icon_char = monster_char(r_ptr);
        }

        strnfmt(meta, sizeof(meta), "Seen %u  Slain %u  Deaths %u",
            (unsigned)entry->seen, (unsigned)entry->killed,
            (unsigned)entry->deaths);
        (void)app_ui_panel_add_row_ex(panel, (s16b)idx,
            entry->killed > 0 ? TERM_L_GREEN : TERM_WHITE, TERM_SLATE,
            icon_attr, icon_char, true, idx == state->highlight,
            "", name, meta);
    }

    if (state->highlight >= 0 && state->highlight < total)
    {
        const score_run_monster_v1* entry = &details->monsters[order[state->highlight]];
        char buf[APP_UI_TEXT_MAX];

        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Monster");
        (void)app_ui_panel_add_detail_line(panel, TERM_L_WHITE,
            run_history_monster_name(entry->r_idx));
        strnfmt(buf, sizeof(buf), "Seen: %u", (unsigned)entry->seen);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
        strnfmt(buf, sizeof(buf), "Slain: %u", (unsigned)entry->killed);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
        strnfmt(buf, sizeof(buf), "Deaths: %u", (unsigned)entry->deaths);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
        strnfmt(buf, sizeof(buf), "Sort: %s",
            run_history_monster_sort_labels[sort_mode]);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
            "Press Enter to inspect. Press s to sort.");
    }

    mem_free(order);
}

static int run_history_ui_build_scene(app_ui_scene* scene,
    const run_history_entry* entry, const score_run_detail_block* details,
    bool have_details, bool current_run, cptr player, cptr race_name,
    cptr created, cptr completed, const bool available[RUN_PANEL_COUNT],
    run_detail_panel active_panel, run_detail_view_state* view)
{
    app_ui_panel* panel;
    char title[APP_UI_TITLE_MAX];
    char subtitle[APP_UI_TEXT_MAX];
    const score_record_v1* rec = entry ? &entry->record : NULL;
    int total_rows = 0;

    if (!scene || !entry || !view || !rec)
        return -1;

    panel = score_ui_begin_browser_scene(scene,
        APP_UI_PANEL_FLAG_TOP_ANCHORED
            | APP_UI_PANEL_FLAG_LEFT_ANCHORED
            | APP_UI_PANEL_FLAG_SCROLL_ROWS);
    if (!panel)
        return -1;
    run_history_ui_build_header(title, sizeof(title), subtitle,
        sizeof(subtitle), rec, player, race_name,
        score_ui_run_status_label(rec->status));
    app_ui_panel_set_title(panel, TERM_L_WHITE, title);
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);
    run_history_ui_add_tabs(panel, active_panel, available);

    switch (active_panel)
    {
    case RUN_PANEL_GENERAL:
        run_history_ui_add_general_rows(panel, rec, entry, created, completed,
            current_run);
        app_ui_panel_set_row_offset(panel, (s16b)view->general_top);
        total_rows = panel->row_count;
        break;
    case RUN_PANEL_STATS:
        if (have_details)
            run_history_ui_add_stats_rows(panel, details);
        else
            (void)run_history_ui_add_value_row(panel, TERM_L_DARK,
                "No detail data", TERM_L_DARK,
                "This run does not have a recorded detail payload.");
        app_ui_panel_set_row_offset(panel, (s16b)view->stats_top);
        total_rows = panel->row_count;
        break;
    case RUN_PANEL_ABILITIES:
        if (have_details && details->ability_count > 0)
            run_history_ui_add_ability_rows(panel, details, &view->abilities);
        else
            (void)run_history_ui_add_value_row(panel, TERM_L_DARK,
                "No ability timeline", TERM_L_DARK,
                "No ability timeline was recorded for this run.");
        total_rows = have_details ? details->ability_count : 0;
        break;
    case RUN_PANEL_MILESTONES:
        if (have_details && details->milestone_count > 0)
            run_history_ui_add_milestone_rows(panel, details, &view->milestones);
        else
            (void)run_history_ui_add_value_row(panel, TERM_L_DARK,
                "No milestones", TERM_L_DARK,
                "No milestone log was recorded for this run.");
        total_rows = have_details ? details->milestone_count : 0;
        break;
    case RUN_PANEL_ARTEFACTS:
        if (have_details && details->header.artefact_count > 0)
            run_history_ui_add_artefact_rows(panel, details, &view->artefacts);
        else
            (void)run_history_ui_add_value_row(panel, TERM_L_DARK,
                "No artefacts", TERM_L_DARK,
                "No artefact data was recorded for this run.");
        total_rows = have_details ? MIN(details->header.artefact_count,
            details->header.artefact_capacity) : 0;
        break;
    case RUN_PANEL_MONSTERS:
        if (have_details && details->header.monster_count > 0)
            run_history_ui_add_monster_rows(panel, details, &view->monsters,
                view->monster_sort_mode);
        else
            (void)run_history_ui_add_value_row(panel, TERM_L_DARK,
                "No monster encounters", TERM_L_DARK,
                "No monster encounter data was recorded for this run.");
        total_rows = have_details ? MIN(details->header.monster_count,
            details->header.monster_capacity) : 0;
        break;
    default:
        break;
    }

    run_history_ui_add_list_footer(panel, active_panel,
        total_rows > 0
            && (active_panel == RUN_PANEL_ARTEFACTS
                || active_panel == RUN_PANEL_MONSTERS),
        total_rows > 0 && active_panel == RUN_PANEL_MONSTERS);
    return total_rows;
}

static const char* run_history_monster_sort_labels[RUN_MON_SORT_COUNT] = {
    "First met",
    "Depth (uniques first)"
};

static const score_run_detail_block* g_monster_sort_details = NULL;
static run_monster_sort_mode g_monster_sort_mode = RUN_MON_SORT_APPEARANCE;

static bool run_history_monster_is_unique(const score_run_monster_v1* entry)
{
    if (!entry || !r_info || !z_info)
        return false;
    if (entry->r_idx <= 0 || entry->r_idx >= z_info->r_max)
        return false;
    const monster_race* r_ptr = &r_info[entry->r_idx];
    return (r_ptr->flags1 & RF1_UNIQUE) != 0;
}

static int run_history_monster_level(const score_run_monster_v1* entry)
{
    if (!entry || !r_info || !z_info)
        return -1;
    if (entry->r_idx <= 0 || entry->r_idx >= z_info->r_max)
        return -1;
    return r_info[entry->r_idx].level;
}

static int run_history_compare_monsters(const void* va, const void* vb)
{
    int ia = *(const int*)va;
    int ib = *(const int*)vb;
    const score_run_monster_v1* ma = &g_monster_sort_details->monsters[ia];
    const score_run_monster_v1* mb = &g_monster_sort_details->monsters[ib];

    if (g_monster_sort_mode == RUN_MON_SORT_DEPTH) {
        int a_unique = run_history_monster_is_unique(ma) ? 1 : 0;
        int b_unique = run_history_monster_is_unique(mb) ? 1 : 0;
        if (a_unique != b_unique)
            return (b_unique - a_unique);

        int a_level = run_history_monster_level(ma);
        int b_level = run_history_monster_level(mb);
        if (a_level != b_level)
            return (b_level - a_level);
    }

    if (ia != ib)
        return (ia < ib) ? -1 : 1;
    return 0;
}

static int* run_history_build_monster_order(const score_run_detail_block* details,
                                            run_monster_sort_mode mode,
                                            int total)
{
    if (total <= 0)
        return NULL;
    int* order = mem_alloc_array(total, int);
    if (!order)
        return NULL;
    for (int i = 0; i < total; i++)
        order[i] = i;
    if (mode == RUN_MON_SORT_APPEARANCE)
        return order;

    g_monster_sort_details = details;
    g_monster_sort_mode = mode;
    qsort(order, total, sizeof(int), run_history_compare_monsters);
    g_monster_sort_details = NULL;
    return order;
}

static const char* run_history_format_depth_label(const score_run_milestone_v1* entry,
                                                  char* buffer, size_t len)
{
    if (entry->depth_label[0]) {
        SDL_strlcpy(buffer, entry->depth_label, len);
        return buffer;
    }
    int feet = entry->depth * 50;
    if (feet <= 0) {
        SDL_strlcpy(buffer, "-", len);
        return buffer;
    }
    strnfmt(buffer, len, "%5d ft", feet);
    return buffer;
}

static void run_history_examine_artefact(const score_run_detail_block* details,
                                         const run_detail_list_state* state)
{
    int total = details->header.artefact_count;
    if (total > details->header.artefact_capacity)
        total = details->header.artefact_capacity;
    int idx = state->highlight;
    if (idx < 0 || idx >= total)
        return;
    const score_run_artefact_v1* entry = &details->artefacts[idx];
    object_type fake_obj;
    if (run_history_prepare_artefact_object(entry, &fake_obj)) {
        object_info_screen(&fake_obj);
    } else {
        bell("Artefact information not available.");
    }
}

static void run_history_examine_monster(const score_run_detail_block* details,
                                        const run_detail_list_state* state,
                                        run_monster_sort_mode mode)
{
    int total = details->header.monster_count;
    if (total > details->header.monster_capacity)
        total = details->header.monster_capacity;
    int idx = state->highlight;
    if (idx < 0 || idx >= total)
        return;

    int* order = run_history_build_monster_order(details, mode, total);
    if (!order)
        return;
    const score_run_monster_v1* entry = &details->monsters[order[idx]];
    mem_free(order);

    if (z_info && entry->r_idx > 0 && entry->r_idx < z_info->r_max) {
        if (!ui_information_scene_show_monster_recall(entry->r_idx, NULL,
                NULL, false, NULL))
        {
            log_warn("run history: failed to present monster recall scene");
            bell("Monster information not available.");
        }
    } else {
        bell("Monster information not available.");
    }
}

void run_history_show_detail(const run_history_entry* entry)
{
    if (!entry)
        return;

    const score_record_v1* rec = &entry->record;
    score_run_detail_block details;
    memset(&details, 0, sizeof(details));
    bool have_details = (entry->detail_offset >= 0)
        && score_runs_load_details(entry->detail_offset, &details);

    bool current_run = score_ui_run_history_is_current(entry);
    if ((!have_details || details.header.monster_count == 0
            || details.header.artefact_count == 0)
        && current_run) {
        score_runs_free_details(&details);
        memset(&details, 0, sizeof(details));
        have_details = score_runs_snapshot_details(&details);
        if (!have_details)
            log_warn("run_history: unable to hydrate live detail payload");
    }

    char player[33];
    if (rec->player_name[0]) {
        SDL_strlcpy(player, rec->player_name, sizeof(player));
    } else if (rec->savefile_hint[0]) {
        SDL_strlcpy(player, rec->savefile_hint, sizeof(player));
    } else {
        SDL_strlcpy(player, "<unknown>", sizeof(player));
    }

    char created[32], completed[32];
    score_ui_run_history_format_timestamp(rec->created_utc, true, created,
        sizeof(created));
    score_ui_run_history_format_timestamp(rec->completed_utc, true, completed,
        sizeof(completed));

    const char* race_name = score_ui_run_history_race_name(rec->race_id);

    bool panel_has_data[RUN_PANEL_COUNT];
    panel_has_data[RUN_PANEL_GENERAL] = true;
    panel_has_data[RUN_PANEL_STATS] = true;
    panel_has_data[RUN_PANEL_ABILITIES] = have_details && details.ability_count > 0;
    panel_has_data[RUN_PANEL_MILESTONES] = have_details && details.milestone_count > 0;
    panel_has_data[RUN_PANEL_ARTEFACTS] = have_details && details.header.artefact_count > 0;
    panel_has_data[RUN_PANEL_MONSTERS] = have_details && details.header.monster_count > 0;

    run_detail_panel panel = RUN_PANEL_GENERAL;
    run_detail_view_state view = {0};
    bool done = false;
    ui_information_scene_scope detail_scope;
    if (!ui_information_scene_enter(&detail_scope))
    {
        log_warn("run history detail: information-scene scope unavailable");
        msg_print("Run history detail viewer unavailable.");
        if (have_details)
            score_runs_free_details(&details);
        return;
    }

    while (!done) {
        bool steamdeck = steamdeck_controls_active();
        app_ui_scene scene;
        int current_total_rows;

        current_total_rows = run_history_ui_build_scene(&scene, entry, &details,
            have_details, current_run, player, race_name, created, completed,
            panel_has_data, panel, &view);
        if (current_total_rows < 0 || !ui_information_scene_present_ui(&scene))
        {
            log_warn("run history detail: failed to present semantic scene");
            msg_print("Run history detail viewer unavailable.");
            break;
        }

        int ch = ui_information_scene_wait_key();

        if (steamdeck) {
            if (ch == steamdeck_back_key())
                ch = ESCAPE;
            else if (ch == steamdeck_confirm_key())
                ch = '\r';
            else if (ch == steamdeck_secondary_key())
                ch = 's';
        }

        switch (ch) {
        case ESCAPE:
        case 'q':
        case 'Q':
            done = true;
            break;
        case '4':
#ifdef ARROW_LEFT
        case ARROW_LEFT:
#endif
        case 'h':
        case 'H':
            panel = (run_detail_panel)((panel + RUN_PANEL_COUNT - 1) % RUN_PANEL_COUNT);
            break;
        case '6':
#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
        case 'l':
        case 'L':
            panel = (run_detail_panel)((panel + 1) % RUN_PANEL_COUNT);
            break;
        default: {
            bool handled = false;
            switch (panel) {
            case RUN_PANEL_GENERAL:
                handled = run_history_ui_handle_scroll_key(&view.general_top,
                    ch, current_total_rows);
                break;
            case RUN_PANEL_STATS:
                handled = run_history_ui_handle_scroll_key(&view.stats_top, ch,
                    current_total_rows);
                break;
            case RUN_PANEL_ABILITIES:
                handled = run_history_ui_handle_scroll_key(
                    &view.abilities.highlight, ch, current_total_rows);
                break;
            case RUN_PANEL_MILESTONES:
                handled = run_history_ui_handle_scroll_key(
                    &view.milestones.highlight, ch, current_total_rows);
                break;
            case RUN_PANEL_ARTEFACTS:
                if (ch == ' ' || ch == '\r' || ch == '\n' ||
                    ch == 'x' || ch == 'X' || ch == 'r' || ch == 'R') {
                    run_history_examine_artefact(&details, &view.artefacts);
                    handled = true;
                } else {
                    handled = run_history_ui_handle_scroll_key(
                        &view.artefacts.highlight, ch, current_total_rows);
                }
                break;
            case RUN_PANEL_MONSTERS:
                if (ch == ' ' || ch == '\r' || ch == '\n' ||
                    ch == 'x' || ch == 'X' || ch == 'r' || ch == 'R') {
                    run_history_examine_monster(&details, &view.monsters,
                        view.monster_sort_mode);
                    handled = true;
                } else if (ch == 's' || ch == 'S') {
                    view.monster_sort_mode =
                        (run_monster_sort_mode)((view.monster_sort_mode + 1) % RUN_MON_SORT_COUNT);
                    handled = true;
                } else {
                    handled = run_history_ui_handle_scroll_key(
                        &view.monsters.highlight, ch, current_total_rows);
                }
                break;
            default:
                handled = false;
                break;
            }
            if (!handled)
                bell("Unknown command.");
            break;
        }
        }
    }

    ui_information_scene_leave(&detail_scope);

    if (have_details)
        score_runs_free_details(&details);
}

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

#ifndef INCLUDED_SCORE_UI_RUN_HISTORY_H
#define INCLUDED_SCORE_UI_RUN_HISTORY_H

#include "h-basic.h"
#include "score/score_runs.h"

typedef struct run_history_entry {
    score_record_v1 record;
    s64b detail_offset;
    int rating;
} run_history_entry;

typedef enum run_detail_panel {
    RUN_PANEL_GENERAL = 0,
    RUN_PANEL_STATS,
    RUN_PANEL_ABILITIES,
    RUN_PANEL_MILESTONES,
    RUN_PANEL_ARTEFACTS,
    RUN_PANEL_MONSTERS,
    RUN_PANEL_COUNT
} run_detail_panel;

typedef enum run_monster_sort_mode {
    RUN_MON_SORT_APPEARANCE = 0,
    RUN_MON_SORT_DEPTH,
    RUN_MON_SORT_COUNT
} run_monster_sort_mode;

typedef struct run_detail_list_state {
    int highlight;
} run_detail_list_state;

typedef struct run_detail_view_state {
    int general_top;
    int stats_top;
    run_detail_list_state abilities;
    run_detail_list_state milestones;
    run_detail_list_state artefacts;
    run_detail_list_state monsters;
    run_monster_sort_mode monster_sort_mode;
} run_detail_view_state;

bool score_ui_run_history_is_current(const run_history_entry* entry);
const char* score_ui_run_status_label(score_record_status status);
const char* score_ui_run_history_race_name(byte idx);
void score_ui_run_history_format_timestamp(u32b utc, bool include_time,
    char* out, size_t out_len);
void run_history_show_detail(const run_history_entry* entry);

#endif /* INCLUDED_SCORE_UI_RUN_HISTORY_H */

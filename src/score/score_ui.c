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

#include "score/score_ui.h"

#include "angband.h"
#include "fs/file.h"
#include "fs/path.h"
#include "log/log.h"
#include "platform-input.h"
#include "score/score_entry.h"
#include "score/score_io.h"
#include "score/score_logic.h"
#include "score/score_runs.h"
#include "score/score_ui-browser.h"
#include "score/score_ui-run-history.h"
#include "ui/ui-browser-shell.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-semantic-scene.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Helper to build score/meta file path correctly for both portable and normal builds */
static bool build_meta_path(char* buf, size_t len, const char* filename)
{
#ifdef SIL_USE_LOCAL_DATA
    /* Portable build: in apex directory */
    return path_build(buf, len, ANGBAND_DIR_APEX, filename);
#else
    /* Normal build: in meta directory (parent of metaruns) */
    if (ANGBAND_DIR_METARUN && *ANGBAND_DIR_METARUN) {
        char meta_dir[1024];
        SDL_strlcpy(meta_dir, ANGBAND_DIR_METARUN, sizeof(meta_dir));
        char* last_sep = strrchr(meta_dir, PATH_SEP[0]);
        if (last_sep) *last_sep = '\0';
        return path_build(buf, len, meta_dir, filename);
    } else {
        return path_build(buf, len, ANGBAND_DIR_APEX, filename);
    }
#endif
}

#define RUN_HISTORY_MAX       256
#define RUN_HISTORY_ROWS       15
#define SCORE_BROWSER_SHORT_ROWS 10

static void run_history_refresh_active_run(void)
{
    if (!character_generated || !p_ptr || p_ptr->is_dead)
        return;

    time_t now = time(NULL);
    if (now == (time_t)-1)
        now = 0;

    if (!score_refresh_live_snapshot(now, "run_history")) {
        log_warn("run_history: unable to refresh live snapshot before viewing");
    }
}

static high_score forced_highlight_entry;
static bool forced_highlight_active = false;
static bool force_interactive_scores = false;
static bool score_last_layout_short = true;

void score_ui_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    ui_browser_shell_prompt_label(binding, fallback, buf, buflen);
}

app_ui_panel* score_ui_begin_browser_scene(app_ui_scene* scene,
    u16b panel_flags)
{
    ui_browser_shell_scene_config config;
    app_ui_panel* panel;

    ui_browser_shell_scene_config_init(&config);
    config.scene_flags = APP_UI_SCENE_FLAG_USE_BACKDROP;
    config.style = APP_UI_PANEL_STYLE_BROWSER;
    config.accent_attr = TERM_L_BLUE;
    config.panel_flags = panel_flags;
    config.min_width_px = 1180;
    config.width_cap_px = 2200;
    panel = ui_browser_shell_begin(scene, &config);
    if (panel)
        app_ui_panel_set_icon(panel, TERM_YELLOW, '*');
    return panel;
}

typedef enum
{
    SCORE_VIEW_ORDER_SCORE = 0,
    SCORE_VIEW_ORDER_CHRONOLOGY = 1
} score_view_order;

typedef enum
{
    SCORE_BROWSER_FILTER_ALL = 0,
    SCORE_BROWSER_FILTER_ACTIVE,
    SCORE_BROWSER_FILTER_TROPHY,
    SCORE_BROWSER_FILTER_FALLEN,
    SCORE_BROWSER_FILTER_COUNT
} score_browser_filter;

#define SCORE_BROWSER_FILTER_TAB_BASE 200

static score_browser_filter score_last_filter = SCORE_BROWSER_FILTER_ALL;

typedef enum
{
    RUN_HISTORY_SORT_DATE = 0,
    RUN_HISTORY_SORT_RATING = 1
} run_history_sort_order;

typedef enum
{
    RUN_HISTORY_FILTER_ALL = 0,
    RUN_HISTORY_FILTER_ACTIVE,
    RUN_HISTORY_FILTER_TROPHY,
    RUN_HISTORY_FILTER_FALLEN,
    RUN_HISTORY_FILTER_COUNT
} run_history_filter;

#define RUN_HISTORY_FILTER_TAB_BASE 100

static const char* run_history_sort_label(run_history_sort_order order)
{
    return (order == RUN_HISTORY_SORT_RATING) ? "Rating" : "Date";
}

static const char* run_history_filter_label(run_history_filter filter)
{
    switch (filter)
    {
    case RUN_HISTORY_FILTER_ACTIVE:
        return "Active";
    case RUN_HISTORY_FILTER_TROPHY:
        return "Trophy";
    case RUN_HISTORY_FILTER_FALLEN:
        return "Fallen";
    case RUN_HISTORY_FILTER_ALL:
    default:
        return "All";
    }
}

static bool run_history_entry_has_trophy(const run_history_entry* entry)
{
    const score_record_v1* rec;

    if (!entry)
        return false;

    rec = &entry->record;
    return rec->status == SCORE_RECORD_ESCAPED
        || (rec->run_flags & SCORE_RUN_FLAG_MORGOTH_SLAIN)
        || (rec->run_flags & SCORE_RUN_FLAG_ANGBAND_ESCAPED)
        || rec->silmarils > 0;
}

static bool run_history_filter_matches(const run_history_entry* entry,
    run_history_filter filter)
{
    const score_record_v1* rec;

    if (!entry)
        return false;

    rec = &entry->record;
    switch (filter)
    {
    case RUN_HISTORY_FILTER_ACTIVE:
        return rec->status == SCORE_RECORD_ALIVE;
    case RUN_HISTORY_FILTER_TROPHY:
        return run_history_entry_has_trophy(entry);
    case RUN_HISTORY_FILTER_FALLEN:
        return rec->status == SCORE_RECORD_DEAD
            && !run_history_entry_has_trophy(entry);
    case RUN_HISTORY_FILTER_ALL:
    default:
        return true;
    }
}

static int run_history_filter_count(const run_history_entry* entries,
    int count, run_history_filter filter)
{
    int matches = 0;

    if (!entries || count <= 0)
        return 0;

    for (int i = 0; i < count; i++)
    {
        if (run_history_filter_matches(&entries[i], filter))
            matches++;
    }

    return matches;
}

static int run_history_apply_filter(const run_history_entry* source,
    int source_count, run_history_entry* out, run_history_filter filter)
{
    int out_count = 0;

    if (!source || !out || source_count <= 0)
        return 0;

    for (int i = 0; i < source_count; i++)
    {
        if (!run_history_filter_matches(&source[i], filter))
            continue;
        out[out_count++] = source[i];
    }

    return out_count;
}

static run_history_filter run_history_next_filter(
    const int counts[RUN_HISTORY_FILTER_COUNT], run_history_filter filter)
{
    int cursor = (int)filter;

    for (int i = 0; i < RUN_HISTORY_FILTER_COUNT; i++)
    {
        cursor++;
        if (cursor >= RUN_HISTORY_FILTER_COUNT)
            cursor = 0;
        if (!counts || counts[cursor] > 0)
            return (run_history_filter)cursor;
    }

    return filter;
}

static void run_history_add_filter_tabs(app_ui_panel* panel,
    const int counts[RUN_HISTORY_FILTER_COUNT], run_history_filter active)
{
    if (!panel)
        return;

    for (int i = 0; i < RUN_HISTORY_FILTER_COUNT; i++)
    {
        char tooltip[APP_UI_TEXT_MAX];

        if (counts && counts[i] <= 0)
            continue;

        strnfmt(tooltip, sizeof(tooltip), "Show %s runs",
            run_history_filter_label((run_history_filter)i));
        (void)ui_browser_shell_add_tab(panel,
            (s16b)(RUN_HISTORY_FILTER_TAB_BASE + i),
            i == (int)active ? TERM_L_BLUE : TERM_WHITE, i == (int)active,
            run_history_filter_label((run_history_filter)i), 0, tooltip);
    }
}

static char run_history_status_short(score_record_status status)
{
    switch (status) {
    case SCORE_RECORD_ALIVE:
        return 'A';
    case SCORE_RECORD_DEAD:
        return 'D';
    case SCORE_RECORD_ESCAPED:
        return 'E';
    default:
        return '?';
    }
}

static void run_history_build_high_score(const score_record_v1* rec, high_score* out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!rec)
        return;

    strnfmt(out->what, sizeof(out->what), "%s", VERSION_STRING);
    strnfmt(out->pts, sizeof(out->pts), "%4d", rec->net_curses);
    strnfmt(out->turns, sizeof(out->turns), "%9lu", (unsigned long)rec->turns_spent);

    if (rec->completed_utc) {
        time_t ts = (time_t)rec->completed_utc;
        struct tm* tm_info = localtime(&ts);
        if (tm_info)
            strftime(out->day, sizeof(out->day), "@%Y%m%d", tm_info);
    }

    const char* who = rec->player_name[0] ? rec->player_name :
        (rec->savefile_hint[0] ? rec->savefile_hint : "<unknown>");
    strnfmt(out->who, sizeof(out->who), "%-.15s", who);

    strnfmt(out->p_r, sizeof(out->p_r), "%02u", (unsigned)rec->race_id);
    strnfmt(out->p_h, sizeof(out->p_h), "%02u", (unsigned)rec->character_id);
    strnfmt(out->cur_dun, sizeof(out->cur_dun), "%3u", (unsigned)rec->exit_depth);
    strnfmt(out->max_dun, sizeof(out->max_dun), "%3u", (unsigned)rec->max_depth);
    strnfmt(out->cur_lev, sizeof(out->cur_lev), "%3u", (unsigned)rec->uniques_killed);

    const char* how = (rec->status == SCORE_RECORD_ALIVE)
        ? "(alive and well)"
        : (rec->cause_of_death[0] ? rec->cause_of_death : "(unknown)");
    strnfmt(out->how, sizeof(out->how), "%-.49s", how);

    int sils = (rec->silmarils > 9) ? 9 : (int)rec->silmarils;
    strnfmt(out->silmarils, sizeof(out->silmarils), "%1d", sils);
    out->morgoth_slain[0] = (rec->run_flags & SCORE_RUN_FLAG_MORGOTH_SLAIN) ? 't' : 'f';
    out->escaped[0] = (rec->run_flags & SCORE_RUN_FLAG_ANGBAND_ESCAPED) ? 't' : 'f';
}

static int run_history_compute_rating(const score_record_v1* rec)
{
    if (!rec)
        return 0;
    high_score temp;
    run_history_build_high_score(rec, &temp);
    return score_points(&temp);
}

static int compare_scores_qsort(const void* va, const void* vb)
{
    const high_score* a = (const high_score*)va;
    const high_score* b = (const high_score*)vb;
    return score_compare(a, b);
}

static long score_day_key(const high_score* entry)
{
    if (!entry)
        return LONG_MIN;

    if (streq(entry->how, "(alive and well)"))
        return LONG_MAX;

    if (entry->day[0] != '@')
        return LONG_MIN + 1;

    char buf[32];
    SDL_strlcpy(buf, entry->day + 1, sizeof(buf));
    char* end = NULL;
    long value = strtol(buf, &end, 10);
    if (value <= 0 || !end || *end != '\0')
        return LONG_MIN + 1;

    return value;
}

static int compare_scores_chronological(const void* va, const void* vb)
{
    const high_score* a = (const high_score*)va;
    const high_score* b = (const high_score*)vb;

    long day_a = score_day_key(a);
    long day_b = score_day_key(b);
    if (day_a != day_b)
        return (day_a > day_b) ? -1 : 1;

    int cmp = strcmp(a->who, b->who);
    if (cmp != 0)
        return cmp;

    cmp = strcmp(a->how, b->how);
    if (cmp != 0)
        return cmp;

    return score_compare(a, b);
}

static int deduplicate_scores_by_name(high_score* entries, int count)
{
    if (count <= 1)
        return count;

    high_score unique[MAX_HISCORES + 1];
    int unique_scores[MAX_HISCORES + 1];
    int unique_count = 0;

    for (int i = 0; i < count; i++)
    {
        int pts = score_points(&entries[i]);
        bool merged = false;

        for (int j = 0; j < unique_count; j++)
        {
            if (streq(entries[i].who, unique[j].who))
            {
                if (pts > unique_scores[j]
                    || (pts == unique_scores[j] && strcmp(entries[i].day, unique[j].day) > 0)
                    || (pts == unique_scores[j] && streq(entries[i].day, unique[j].day)
                        && strcmp(entries[i].how, unique[j].how) > 0))
                {
                    unique[j] = entries[i];
                    unique_scores[j] = pts;
                }
                merged = true;
                break;
            }
        }

        if (!merged)
        {
            unique[unique_count] = entries[i];
            unique_scores[unique_count] = pts;
            unique_count++;
        }
    }

    for (int i = 0; i < unique_count; i++)
    {
        entries[i] = unique[i];
    }

    return unique_count;
}

static const char* score_view_order_label(score_view_order order)
{
    return (order == SCORE_VIEW_ORDER_CHRONOLOGY)
        ? "Date (newest first)"
        : "Score (highest first)";
}
static bool score_identity_matches(const high_score* a, const high_score* b)
{
    if (!a || !b)
        return false;
    return streq(a->who, b->who)
        && streq(a->day, b->day)
        && streq(a->how, b->how);
}
static int find_score_index(const high_score* entries, int count, const high_score* target)
{
    if (!target)
        return -1;
    for (int i = 0; i < count; i++)
    {
        if (score_identity_matches(&entries[i], target))
            return i;
    }
    return -1;
}
static void set_forced_highlight_entry(const high_score* entry)
{
    if (entry) {
        forced_highlight_entry = *entry;
        forced_highlight_active = true;
    } else {
        forced_highlight_active = false;
    }
}
static byte score_entry_color(const high_score* entry, bool highlight)
{
    if (highlight) return TERM_YELLOW;

    if (!entry) return TERM_SLATE;

    if (streq(entry->how, "(alive and well)"))
        return TERM_L_GREEN;

    if (entry->escaped[0] == 't')
        return TERM_GREEN;

    if (entry->morgoth_slain[0] == 't')
        return TERM_L_RED;

    int sil = atoi(entry->silmarils);
    if (sil > 0)
        return TERM_ORANGE;

    int depth = atoi(entry->max_dun);
    if (depth >= 10)
        return TERM_WHITE;
    if (depth >= 5)
        return TERM_L_WHITE;

    return TERM_SLATE;
}
static const char* score_entry_character_suffix(const high_score* entry)
{
    int ph;

    if (!entry || !c_info || !c_name || !z_info)
        return "";

    ph = atoi(entry->p_h);
    if (ph < 0 || ph >= z_info->c_max)
        return "";

    return c_name + c_info[ph].alt_name;
}

static const char* score_entry_race_name(const high_score* entry)
{
    int pr;

    if (!entry || !p_info || !p_name || !z_info)
        return "<unknown>";

    pr = atoi(entry->p_r);
    if (pr < 0 || pr >= z_info->p_max)
        return "<unknown>";

    return p_name + p_info[pr].name;
}

static void score_format_entry_day(const high_score* entry, char* out,
    size_t out_len)
{
    const char* when;

    if (!out || out_len == 0)
        return;

    if (!entry)
    {
        out[0] = '\0';
        return;
    }

    for (when = entry->day; isspace((unsigned char)*when); when++)
        ;

    if ((*when == '@') && strlen(when) == 9)
    {
        char month[4];

        sprintf(month, "%.2s", when + 5);
        atomonth(atoi(month), month);

        if (*(when + 7) == '0')
            strnfmt(out, out_len, "%.1s %.3s %.4s", when + 8, month,
                when + 1);
        else
            strnfmt(out, out_len, "%.2s %.3s %.4s", when + 7, month,
                when + 1);
        return;
    }

    SDL_strlcpy(out, when, out_len);
}

static void score_format_entry_points(const high_score* entry, char* out,
    size_t out_len)
{
    char commas[16];

    if (!out || out_len == 0)
        return;

    if (!entry)
    {
        out[0] = '\0';
        return;
    }

    comma_number(commas, score_points(entry));
    SDL_strlcpy(out, commas, out_len);
}

static void score_format_entry_turns(const high_score* entry, char* out,
    size_t out_len)
{
    char commas[16];

    if (!out || out_len == 0)
        return;

    if (!entry)
    {
        out[0] = '\0';
        return;
    }

    comma_number(commas, atoi(entry->turns));
    SDL_strlcpy(out, commas, out_len);
}

static int score_entry_depth_feet(const high_score* entry)
{
    if (!entry)
        return 0;

    return atoi(entry->cur_dun) * 50;
}

static int score_entry_silmarils(const high_score* entry)
{
    if (!entry)
        return 0;

    return parse_score_int(entry->silmarils, sizeof(entry->silmarils), 0);
}

static const char* score_browser_filter_label(score_browser_filter filter)
{
    switch (filter)
    {
    case SCORE_BROWSER_FILTER_ACTIVE:
        return "Active";
    case SCORE_BROWSER_FILTER_TROPHY:
        return "Trophy";
    case SCORE_BROWSER_FILTER_FALLEN:
        return "Fallen";
    case SCORE_BROWSER_FILTER_ALL:
    default:
        return "All";
    }
}

static bool score_browser_entry_has_trophy(const high_score* entry)
{
    if (!entry)
        return false;

    return entry->escaped[0] == 't'
        || entry->morgoth_slain[0] == 't'
        || score_entry_silmarils(entry) > 0;
}

static bool score_browser_filter_matches(const high_score* entry,
    score_browser_filter filter)
{
    if (!entry)
        return false;

    switch (filter)
    {
    case SCORE_BROWSER_FILTER_ACTIVE:
        return streq(entry->how, "(alive and well)");
    case SCORE_BROWSER_FILTER_TROPHY:
        return score_browser_entry_has_trophy(entry);
    case SCORE_BROWSER_FILTER_FALLEN:
        return !streq(entry->how, "(alive and well)")
            && !score_browser_entry_has_trophy(entry);
    case SCORE_BROWSER_FILTER_ALL:
    default:
        return true;
    }
}

static int score_browser_filter_count(const high_score* entries, int count,
    score_browser_filter filter)
{
    int matches = 0;

    if (!entries || count <= 0)
        return 0;

    for (int i = 0; i < count; i++)
    {
        if (score_browser_filter_matches(&entries[i], filter))
            matches++;
    }

    return matches;
}

static int score_browser_apply_filter(const high_score* source,
    int source_count, high_score* out, score_browser_filter filter)
{
    int out_count = 0;

    if (!source || !out || source_count <= 0)
        return 0;

    for (int i = 0; i < source_count; i++)
    {
        if (!score_browser_filter_matches(&source[i], filter))
            continue;
        out[out_count++] = source[i];
    }

    return out_count;
}

static score_browser_filter score_browser_next_filter(
    const int counts[SCORE_BROWSER_FILTER_COUNT], score_browser_filter filter)
{
    int cursor = (int)filter;

    for (int i = 0; i < SCORE_BROWSER_FILTER_COUNT; i++)
    {
        cursor++;
        if (cursor >= SCORE_BROWSER_FILTER_COUNT)
            cursor = 0;
        if (!counts || counts[cursor] > 0)
            return (score_browser_filter)cursor;
    }

    return filter;
}

static void score_browser_add_filter_tabs(app_ui_panel* panel,
    const int counts[SCORE_BROWSER_FILTER_COUNT], score_browser_filter active)
{
    if (!panel)
        return;

    for (int i = 0; i < SCORE_BROWSER_FILTER_COUNT; i++)
    {
        char tooltip[APP_UI_TEXT_MAX];

        if (counts && counts[i] <= 0)
            continue;

        strnfmt(tooltip, sizeof(tooltip), "Show %s heroes",
            score_browser_filter_label((score_browser_filter)i));
        (void)ui_browser_shell_add_tab(panel,
            (s16b)(SCORE_BROWSER_FILTER_TAB_BASE + i),
            i == (int)active ? TERM_L_BLUE : TERM_WHITE, i == (int)active,
            score_browser_filter_label((score_browser_filter)i), 0, tooltip);
    }
}

static int score_entry_net_curses(const high_score* entry)
{
    if (!entry || !scores_version_has_curses(score_file_global_ctx()))
        return 0;

    return parse_score_int(entry->pts, sizeof(entry->pts), 0);
}

static void score_build_outcome_summary(const high_score* entry, bool compact,
    char* out, size_t out_len)
{
    if (!out || out_len == 0)
        return;

    if (!entry)
    {
        out[0] = '\0';
        return;
    }

    if (entry->escaped[0] == 't')
    {
        if (score_entry_silmarils(entry) > 0 || entry->morgoth_slain[0] == 't')
            strnfmt(out, out_len, "Escaped with the light of Valinor");
        else
            strnfmt(out, out_len, "Escaped Angband");
        return;
    }

    if (streq(entry->how, "(alive and well)"))
    {
        strnfmt(out, out_len, compact ? "Alive" :
            "Lives still, deep within Angband's vaults");
        return;
    }

    if (entry->morgoth_slain[0] == 't')
    {
        if (compact)
            strnfmt(out, out_len, "Slayer of Morgoth's shadow");
        else
            strnfmt(out, out_len, "Victorious over Morgoth's illusion (%s)",
                entry->how);
        return;
    }

    strnfmt(out, out_len, "Slain by %s", entry->how);
}

static void score_build_trophy_summary(const high_score* entry, char* out,
    size_t out_len)
{
    int silmarils;

    if (!out || out_len == 0)
        return;

    if (!entry)
    {
        out[0] = '\0';
        return;
    }

    silmarils = score_entry_silmarils(entry);
    if (entry->morgoth_slain[0] == 't' && silmarils > 0)
    {
        strnfmt(out, out_len, "Slew Morgoth's shadow and brought back %d Silmaril%s.",
            silmarils, (silmarils == 1) ? "" : "s");
        return;
    }
    if (entry->morgoth_slain[0] == 't')
    {
        strnfmt(out, out_len, "Slew Morgoth's shadow.");
        return;
    }
    if (silmarils == 1)
    {
        strnfmt(out, out_len, "Freed a Silmaril.");
        return;
    }
    if (silmarils == 2)
    {
        strnfmt(out, out_len, "Freed two Silmarils.");
        return;
    }
    if (silmarils == 3)
    {
        strnfmt(out, out_len, "Freed all three Silmarils.");
        return;
    }
    if (silmarils > 3)
    {
        strnfmt(out, out_len, "Freed suspiciously many Silmarils.");
        return;
    }
    if (entry->escaped[0] == 't')
    {
        strnfmt(out, out_len, "Escaped empty-handed.");
        return;
    }

    out[0] = '\0';
}

static void score_build_browser_row_label(const high_score* entry, int place,
    char* out, size_t out_len)
{
    const char* who = (entry && entry->who[0]) ? entry->who : "<unknown>";

    if (!out || out_len == 0)
        return;

    strnfmt(out, out_len, "%2d. %s%s", place, who,
        score_entry_character_suffix(entry));
}

static void score_build_browser_row_meta(const high_score* entry, bool detailed,
    char* out, size_t out_len)
{
    char when[32];
    char points[16];
    char outcome[APP_UI_META_MAX];

    if (!out || out_len == 0)
        return;

    if (!entry)
    {
        out[0] = '\0';
        return;
    }

    score_format_entry_day(entry, when, sizeof(when));
    score_format_entry_points(entry, points, sizeof(points));
    score_build_outcome_summary(entry, true, outcome, sizeof(outcome));

    if (detailed)
        strnfmt(out, out_len, "%s  |  %s pts", when, points);
    else
        strnfmt(out, out_len, "%s pts  |  %s", points, outcome);
}

static void score_add_browser_detail(app_ui_panel* panel,
    const high_score* entry, int place, bool detailed)
{
    char title[APP_UI_TITLE_MAX];
    char buf[APP_UI_TEXT_MAX];
    char when[32];
    char points[16];
    char turns[16];
    char outcome[APP_UI_TEXT_MAX];
    char trophy[APP_UI_TEXT_MAX];
    int curses;
    byte accent_attr;

    if (!panel || !entry)
        return;

    accent_attr = score_entry_color(entry, true);
    if (accent_attr == TERM_YELLOW)
        accent_attr = TERM_L_BLUE;

    score_build_browser_row_label(entry, place, title, sizeof(title));
    app_ui_panel_set_detail_title(panel, accent_attr, title);

    score_format_entry_day(entry, when, sizeof(when));
    score_format_entry_points(entry, points, sizeof(points));
    score_format_entry_turns(entry, turns, sizeof(turns));
    score_build_outcome_summary(entry, false, outcome, sizeof(outcome));
    score_build_trophy_summary(entry, trophy, sizeof(trophy));
    curses = score_entry_net_curses(entry);

    strnfmt(buf, sizeof(buf), "Score: %s pts  |  Date: %s", points, when);
    (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, buf);
    (void)app_ui_panel_add_detail_line(panel, score_entry_color(entry, false),
        outcome);
    strnfmt(buf, sizeof(buf), "Race: %s  |  Depth: %d ft  |  Turns: %s",
        score_entry_race_name(entry), score_entry_depth_feet(entry), turns);
    (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);

    if (curses > 0)
    {
        strnfmt(buf, sizeof(buf), "Curse ledger: %d curse%s", curses,
            (curses == 1) ? "" : "s");
        (void)app_ui_panel_add_detail_line(panel, TERM_L_RED, buf);
    }
    else if (curses < 0)
    {
        strnfmt(buf, sizeof(buf), "Curse ledger: %d blessing%s", -curses,
            (curses == -1) ? "" : "s");
        (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN, buf);
    }

    if (trophy[0])
        (void)app_ui_panel_add_detail_line(panel, TERM_YELLOW, trophy);

    if (detailed && entry->how[0] && !streq(entry->how, "(alive and well)"))
    {
        strnfmt(buf, sizeof(buf), "Fate: %s", entry->how);
        (void)app_ui_panel_add_detail_line(panel,
            entry->escaped[0] == 't' ? TERM_GREEN : TERM_SLATE, buf);
    }
}

static bool score_pages_command_to_key(const app_ui_command* command,
    int count, int start_index, int entries_per_page, bool has_more,
    int* highlight_index, score_browser_filter* filter, char* out_key)
{
    ui_browser_shell_button_key button_keys[] = {
        { 1, 's' },
        { 2, 'l' },
        { 3, ESCAPE },
        { 4, has_more ? '\r' : ESCAPE },
        { 5, 'f' }
    };
    ui_browser_shell_command_map map;
    ui_browser_shell_command_result result;

    if (out_key)
        *out_key = '\0';
    if (!command || !out_key)
        return false;

    ui_browser_shell_command_map_init(&map);
    map.button_keys = button_keys;
    map.button_key_count = N_ELEMENTS(button_keys);
    map.row_activate_key = '\0';

    if (!ui_browser_shell_translate_command(command, &map, &result))
        return false;

    if (result.role == APP_UI_WIDGET_ROLE_LIST_ITEM)
    {
        if (highlight_index && result.widget_id >= 0
            && result.widget_id < count
            && result.widget_id >= start_index
            && result.widget_id < start_index + entries_per_page)
        {
            *highlight_index = result.widget_id;
        }
        return true;
    }

    if (result.role == APP_UI_WIDGET_ROLE_TAB)
    {
        int requested = result.widget_id - SCORE_BROWSER_FILTER_TAB_BASE;

        if (filter && requested >= 0 && requested < SCORE_BROWSER_FILTER_COUNT)
            *filter = (score_browser_filter)requested;
        return true;
    }

    *out_key = result.key;
    return true;
}

static char display_scores_pages_information(const high_score* entries,
    int count, int highlight_index, score_view_order order, bool detailed,
    int page_size)
{
    ui_information_scene_scope scope;
    bool steamdeck = steamdeck_controls_active();
    char order_label[16] = "";
    char layout_label[16] = "";
    char exit_label[16] = "";
    char next_label[16] = "";
    score_browser_filter filter = score_last_filter;
    high_score visible_entries[MAX_HISCORES + 1];
    int filter_counts[SCORE_BROWSER_FILTER_COUNT];
    int visible_count;
    int visible_highlight = -1;

    if (!ui_information_scene_enter(&scope))
    {
        log_warn("scores: unable to enter information-scene scope");
        return 0;
    }

    if (steamdeck)
    {
        score_ui_prompt_label(steamdeck_secondary_key(), "Y", order_label,
            sizeof(order_label));
        score_ui_prompt_label(steamdeck_alt_action_key(), "X", layout_label,
            sizeof(layout_label));
        score_ui_prompt_label(steamdeck_back_key(), "B", exit_label,
            sizeof(exit_label));
        score_ui_prompt_label(steamdeck_confirm_key(), "A", next_label,
            sizeof(next_label));
    }

    if (!entries || count <= 0)
    {
        app_ui_scene scene;
        app_ui_panel* panel;

        panel = score_ui_begin_browser_scene(&scene,
            APP_UI_PANEL_FLAG_SHOW_DETAIL);
        if (!panel)
        {
            ui_information_scene_leave(&scope);
            return 0;
        }
        app_ui_panel_set_title(panel, TERM_L_WHITE, "Halls of Mandos");
        app_ui_panel_set_subtitle(panel, TERM_SLATE,
            score_view_order_label(order));
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
            "No recorded heroes yet.");
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            "No recorded heroes yet.");
        if (steamdeck)
        {
            const ui_browser_shell_footer_action actions[] = {
                { 1, TERM_WHITE, true, next_label, "Close" }
            };

            (void)ui_browser_shell_add_footer_actions(panel, actions,
                N_ELEMENTS(actions));
        }
        else
        {
            const ui_browser_shell_footer_action actions[] = {
                { 1, TERM_WHITE, true, "Any", "Close" }
            };

            (void)ui_browser_shell_add_footer_actions(panel, actions,
                N_ELEMENTS(actions));
        }
        (void)ui_semantic_scene_present_and_wait_key(&scene, true, false,
            APP_WAIT_REASON_NONE, NULL);
        ui_information_scene_leave(&scope);
        return 0;
    }

    for (int i = 0; i < SCORE_BROWSER_FILTER_COUNT; i++)
    {
        filter_counts[i] = score_browser_filter_count(entries, count,
            (score_browser_filter)i);
    }
    if (filter_counts[filter] <= 0)
        filter = SCORE_BROWSER_FILTER_ALL;
    visible_count = score_browser_apply_filter(entries, count, visible_entries,
        filter);
    if (visible_count <= 0)
    {
        filter = SCORE_BROWSER_FILTER_ALL;
        visible_count = score_browser_apply_filter(entries, count,
            visible_entries, filter);
    }
    if (highlight_index >= 0 && highlight_index < count)
        visible_highlight = find_score_index(visible_entries, visible_count,
            &entries[highlight_index]);
    if (visible_highlight < 0 && visible_count > 0)
        visible_highlight = 0;

    {
        int start_index = 0;
        bool highlight_pending = true;

        while (start_index < visible_count)
        {
            int entries_per_page;
            int selected_index;
            bool has_more;
            app_ui_scene scene;
            app_ui_panel* panel;
            char title[APP_UI_TITLE_MAX];
            char subtitle[APP_UI_TEXT_MAX];
            int ch;

            entries_per_page = detailed ? page_size : SCORE_BROWSER_SHORT_ROWS;
            if (detailed && entries_per_page > page_size)
                entries_per_page = page_size;
            if (entries_per_page < 1)
                entries_per_page = 1;

            if (highlight_pending && highlight_index >= 0)
            {
                int max_start = visible_count - entries_per_page;

                if (max_start < 0)
                    max_start = 0;
                start_index = (visible_highlight / entries_per_page)
                    * entries_per_page;
                if (start_index > max_start)
                    start_index = max_start;
                highlight_pending = false;
            }

            selected_index = (visible_highlight >= start_index
                && visible_highlight < start_index + entries_per_page)
                ? visible_highlight
                : start_index;
            has_more = (start_index + entries_per_page < visible_count);

            panel = score_ui_begin_browser_scene(&scene,
                APP_UI_PANEL_FLAG_SHOW_DETAIL);
            if (!panel)
            {
                log_warn("scores: failed to build browser scene");
                ui_information_scene_leave(&scope);
                return 0;
            }
            panel->focus_area = APP_UI_FOCUS_ROWS;
            score_browser_add_filter_tabs(panel, filter_counts, filter);

            strnfmt(title, sizeof(title), "Halls of Mandos");
            strnfmt(subtitle, sizeof(subtitle),
                "%s  |  Layout: %s  |  Filter: %s  |  Page %d",
                score_view_order_label(order), detailed ? "Full" : "Short",
                score_browser_filter_label(filter),
                (start_index / entries_per_page) + 1);
            app_ui_panel_set_title(panel, TERM_L_WHITE, title);
            app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

            for (int idx = start_index; idx < visible_count
                && idx < start_index + entries_per_page; idx++)
            {
                char label[APP_UI_LABEL_MAX];
                char meta[APP_UI_META_MAX];
                bool selected = (idx == selected_index);
                byte attr = score_entry_color(&visible_entries[idx], selected);
                char icon_char = ' ';

                if (visible_entries[idx].morgoth_slain[0] == 't')
                    icon_char = 'V';
                else if (score_entry_silmarils(&visible_entries[idx]) > 0)
                    icon_char = '*';

                score_build_browser_row_label(&visible_entries[idx],
                    idx + 1, label, sizeof(label));
                score_build_browser_row_meta(&visible_entries[idx], detailed,
                    meta, sizeof(meta));
                if (!app_ui_panel_add_row_ex(panel, (s16b)idx, attr,
                        TERM_SLATE, attr, icon_char, true, selected, "",
                        label, meta))
                {
                    ui_information_scene_leave(&scope);
                    return 0;
                }
            }

            score_add_browser_detail(panel, &visible_entries[selected_index],
                selected_index + 1, detailed);

            if (steamdeck)
            {
                const ui_browser_shell_footer_action actions[] = {
                    { 1, TERM_WHITE, true, order_label, "Order" },
                    { 2, TERM_WHITE, true, layout_label, "Layout" },
                    { 3, TERM_WHITE, true, exit_label, "Exit" },
                    { 4, TERM_L_BLUE, true, next_label,
                        has_more ? "Next" : "Close" },
                    { 5, TERM_WHITE, true, "F", "Filter" }
                };

                (void)ui_browser_shell_add_footer_actions(panel, actions,
                    N_ELEMENTS(actions));
            }
            else
            {
                const ui_browser_shell_footer_action actions[] = {
                    { 1, TERM_WHITE, true, "S", "Order" },
                    { 2, TERM_WHITE, true, "L", "Layout" },
                    { 3, TERM_WHITE, true, "Esc", "Exit" },
                    { 4, TERM_L_BLUE, true, "Any",
                        has_more ? "Next" : "Close" },
                    { 5, TERM_WHITE, true, "F", "Filter" }
                };

                (void)ui_browser_shell_add_footer_actions(panel, actions,
                    N_ELEMENTS(actions));
            }

            if (!ui_information_scene_present_ui(&scene))
            {
                log_warn("scores: failed to present browser page");
                ui_information_scene_leave(&scope);
                return 0;
            }

            {
                ui_information_scene_event event;
                char command_key = '\0';

                ch = 0;
                if (!ui_information_scene_wait_event(&event, 0))
                {
                    ch = ESCAPE;
                }
                else if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
                {
                    ch = event.key;
                }
                else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND)
                {
                    score_browser_filter previous_filter = filter;

                    if (!score_pages_command_to_key(&event.command,
                            visible_count, start_index, entries_per_page,
                            has_more, &visible_highlight, &filter,
                            &command_key))
                    {
                        continue;
                    }

                    ch = command_key;
                    if (!ch)
                    {
                        if (filter == previous_filter)
                            continue;

                        score_last_filter = filter;
                        visible_count = score_browser_apply_filter(entries,
                            count, visible_entries, filter);
                        if (visible_count <= 0)
                        {
                            filter = SCORE_BROWSER_FILTER_ALL;
                            visible_count = score_browser_apply_filter(
                                entries, count, visible_entries, filter);
                        }
                        visible_highlight = 0;
                        start_index = 0;
                        highlight_pending = false;
                        continue;
                    }
                }
            }

            if (steamdeck)
            {
                int back_key = steamdeck_back_key();
                int confirm_key = steamdeck_confirm_key();
                int alt_key = steamdeck_alt_action_key();
                int secondary_key = steamdeck_secondary_key();

                if (ch == back_key)
                {
                    ui_information_scene_leave(&scope);
                    return ESCAPE;
                }
                if (ch == confirm_key || ch == '\r' || ch == '\n')
                {
                    if (!has_more)
                        break;
                }
                if (ch == alt_key)
                    ch = 'l';
                if (ch == secondary_key)
                    ch = 's';
            }

            if (ch == ESCAPE)
            {
                ui_information_scene_leave(&scope);
                return ESCAPE;
            }
            if (ch == 's' || ch == 'S' || ch == 'o' || ch == 'O')
            {
                ui_information_scene_leave(&scope);
                return (char)ch;
            }
            if (ch == 'l' || ch == 'L')
            {
                ui_information_scene_leave(&scope);
                return (char)ch;
            }
            if (ch == 'f' || ch == 'F')
            {
                filter = score_browser_next_filter(filter_counts, filter);
                score_last_filter = filter;
                visible_count = score_browser_apply_filter(entries, count,
                    visible_entries, filter);
                if (visible_count <= 0)
                {
                    filter = SCORE_BROWSER_FILTER_ALL;
                    visible_count = score_browser_apply_filter(entries, count,
                        visible_entries, filter);
                }
                visible_highlight = 0;
                start_index = 0;
                highlight_pending = false;
                continue;
            }

            if (!has_more)
                break;

            start_index += entries_per_page;
        }
    }

    ui_information_scene_leave(&scope);
    return 0;
}
void display_scores(int from, int to)
{
    (void)from;
    (void)to;

    log_info("Displaying high scores with interactive controls");
    show_scores_interactive(true);
    quit(NULL);
}
void display_scores_short(int from, int to)
{
    (void)from;
    (void)to;

    bool previous_layout = score_last_layout_short;
    score_last_layout_short = true;
    show_scores_interactive(true);
    score_last_layout_short = previous_layout;
}
static bool ensure_entry_visible(high_score* entries, int* count, int capacity,
                                 const high_score* target, bool sort_by_score, int* highlight_index)
{
    if (!entries || !count || !target || capacity <= 0)
        return false;

    int idx = find_score_index(entries, *count, target);
    if (idx >= 0)
    {
        if (highlight_index) *highlight_index = idx;
        return true;
    }

    if (*count < capacity)
    {
        entries[*count] = *target;
        (*count)++;
    }
    else
    {
        entries[capacity - 1] = *target;
        *count = capacity;
    }

    if (sort_by_score)
        qsort(entries, *count, sizeof(high_score), compare_scores_qsort);
    else
        qsort(entries, *count, sizeof(high_score), compare_scores_chronological);

    *count = deduplicate_scores_by_name(entries, *count);
    if (*count > MAX_HISCORES)
        *count = MAX_HISCORES;

    idx = find_score_index(entries, *count, target);

    if (idx < 0)
    {
        for (int i = 0; i < *count; i++)
        {
            if (streq(entries[i].who, target->who))
            {
                entries[i] = *target;
                if (sort_by_score)
                    qsort(entries, *count, sizeof(high_score), compare_scores_qsort);
                else
                    qsort(entries, *count, sizeof(high_score), compare_scores_chronological);
                idx = find_score_index(entries, *count, target);
                break;
            }
        }
    }

    if (highlight_index && idx >= 0)
        *highlight_index = idx;

    return idx >= 0;
}
void show_scores(bool longscore)
{
    bool preview_allowed = (!force_interactive_scores && !forced_highlight_active && character_generated && !p_ptr->is_dead);
    log_info("show_scores: longscore=%d force_interactive=%d generated=%d dead=%d preview=%d",
             longscore ? 1 : 0,
             force_interactive_scores ? 1 : 0,
             character_generated ? 1 : 0,
             p_ptr->is_dead ? 1 : 0,
             preview_allowed ? 1 : 0);

    high_score ordered_by_score[MAX_HISCORES + 1];
    high_score ordered_by_time[MAX_HISCORES + 1];

    int count_score = collect_high_scores(ordered_by_score, MAX_HISCORES, true);
    int count_time = collect_high_scores(ordered_by_time, MAX_HISCORES, false);

    const int capacity = MAX_HISCORES + 1;
    int page_size = 5;
    bool detailed = !score_last_layout_short;
    score_view_order order = SCORE_VIEW_ORDER_SCORE;

    high_score highlight_buffer;
    const high_score* highlight_entry = NULL;
    if (forced_highlight_active)
    {
        highlight_entry = &forced_highlight_entry;
    }
    else if (character_generated)
    {
        if (p_ptr->is_dead)
        {
            if (create_score(&highlight_buffer) == 0)
                highlight_entry = &highlight_buffer;
        }
        else if (build_live_preview_score(&highlight_buffer))
        {
            highlight_entry = &highlight_buffer;
        }
    }

    int highlight_score = -1;
    int highlight_time = -1;
    if (highlight_entry)
    {
        highlight_score = find_score_index(ordered_by_score, count_score, highlight_entry);
        highlight_time = find_score_index(ordered_by_time, count_time, highlight_entry);
        log_debug("show_scores: highlight indices score=%d time=%d for %s",
                  highlight_score, highlight_time, highlight_entry->who);

        if (highlight_score < 0)
            ensure_entry_visible(ordered_by_score, &count_score, capacity, highlight_entry, true, &highlight_score);
        if (highlight_time < 0)
            ensure_entry_visible(ordered_by_time, &count_time, capacity, highlight_entry, false, &highlight_time);
    }

    while (true)
    {
        const high_score* list = (order == SCORE_VIEW_ORDER_SCORE)
            ? ordered_by_score : ordered_by_time;
        int count = (order == SCORE_VIEW_ORDER_SCORE) ? count_score
            : count_time;
        int highlight = (order == SCORE_VIEW_ORDER_SCORE)
            ? highlight_score : highlight_time;
        char response;

        log_debug("show_scores: rendering page (order=%s count=%d highlight=%d)",
            (order == SCORE_VIEW_ORDER_SCORE) ? "score" : "time",
            count, highlight);

        response = display_scores_pages_information(list, count, highlight,
            order, detailed, page_size);
        if (response == 's' || response == 'S' || response == 'o'
            || response == 'O')
        {
            order = (order == SCORE_VIEW_ORDER_SCORE)
                ? SCORE_VIEW_ORDER_CHRONOLOGY : SCORE_VIEW_ORDER_SCORE;
            continue;
        }
        if (response == 'l' || response == 'L')
        {
            detailed = !detailed;
            score_last_layout_short = !detailed;
            continue;
        }
        break;
    }

    forced_highlight_active = false;
    score_last_layout_short = !detailed;
}
void show_scores_interactive(bool longscore)
{
    bool previous = force_interactive_scores;
    force_interactive_scores = true;
    log_debug("show_scores_interactive: forcing interactive display (longscore=%d)", longscore ? 1 : 0);
    show_scores(longscore);
    force_interactive_scores = previous;
}
void show_scores_interactive_highlight(bool longscore, const high_score* entry)
{
    high_score saved_entry;
    bool had_forced = forced_highlight_active;
    if (had_forced) saved_entry = forced_highlight_entry;

    if (entry) {
        set_forced_highlight_entry(entry);
    } else {
        forced_highlight_active = false;
    }

    show_scores_interactive(longscore);

    if (had_forced) {
        forced_highlight_entry = saved_entry;
        forced_highlight_active = true;
    } else {
        forced_highlight_active = false;
    }
}

void show_scores_interactive_highlight_from_file(bool longscore,
                                                 const char* filepath,
                                                 const high_score* entry)
{
    if (!filepath || !filepath[0]) {
        show_scores_interactive_highlight(longscore, entry);
        return;
    }

    score_file_ctx temp_ctx;
    score_file_reset_ctx(&temp_ctx);

    safe_setuid_grab();
    temp_ctx.fd = score_file_open(filepath, O_RDONLY);
    safe_setuid_drop();
    if (!temp_ctx.fd) {
        log_warn("show_scores_interactive_highlight_from_file: unable to open %s",
                 filepath);
        show_scores_interactive_highlight(longscore, entry);
        return;
    }

    log_debug("show_scores_interactive_highlight_from_file: rendering %s",
              filepath);
    score_file_ctx* previous_ctx = score_file_set_active_ctx(&temp_ctx);
    show_scores_interactive_highlight(longscore, entry);
    score_file_set_active_ctx(previous_ctx);

    (void)ang_file_close_compat(temp_ctx.fd);
    score_file_reset_ctx(&temp_ctx);
}

static bool run_history_skip_details(ang_file* file, s64b* detail_offset)
{
    if (!file)
        return false;

    ang_file_off_t header_pos = ang_file_tell_compat(file);
    score_run_detail_header_v1 detail_header;
    if (ang_file_read_compat(file, &detail_header, sizeof(detail_header))
            != sizeof(detail_header))
        return false;

    if (detail_offset)
        *detail_offset = (s64b)header_pos;

    return score_runs_skip_detail_payload(file, &detail_header);
}

static int run_history_compare_date_desc(const void* a, const void* b)
{
    const run_history_entry* ea = (const run_history_entry*)a;
    const run_history_entry* eb = (const run_history_entry*)b;
    const score_record_v1* ra = &ea->record;
    const score_record_v1* rb = &eb->record;

    if (ra->completed_utc > rb->completed_utc)
        return -1;
    if (ra->completed_utc < rb->completed_utc)
        return 1;

    if (ra->record_id > rb->record_id)
        return -1;
    if (ra->record_id < rb->record_id)
        return 1;

    if (ra->created_utc > rb->created_utc)
        return -1;
    if (ra->created_utc < rb->created_utc)
        return 1;

    return 0;
}

static int run_history_compare_rating_desc(const void* a, const void* b)
{
    const run_history_entry* ea = (const run_history_entry*)a;
    const run_history_entry* eb = (const run_history_entry*)b;
    if (ea->rating > eb->rating)
        return -1;
    if (ea->rating < eb->rating)
        return 1;
    return run_history_compare_date_desc(a, b);
}

static void run_history_sort_entries(run_history_entry* entries,
                                     int count,
                                     run_history_sort_order order)
{
    if (!entries || count <= 1)
        return;
    if (order == RUN_HISTORY_SORT_RATING)
        qsort(entries, count, sizeof(run_history_entry),
              run_history_compare_rating_desc);
    else
        qsort(entries, count, sizeof(run_history_entry),
              run_history_compare_date_desc);
}

static int collect_run_history(run_history_entry* out, int capacity)
{
    if (capacity <= 0 || !out)
        return 0;

    char path[1024];
    if (!build_meta_path(path, sizeof(path), SCORE_RUNS_DB_FILENAME))
        return 0;

    safe_setuid_grab();
    ang_file* file = ang_file_open_compat(path, "rb");
    safe_setuid_drop();

    if (!file)
        return 0;

    score_db_header db_header;
    if (ang_file_read_compat(file, &db_header, sizeof(db_header))
            != sizeof(db_header)
        || memcmp(db_header.magic, SCORE_DB_MAGIC, sizeof(db_header.magic))
            != 0) {
        (void)ang_file_close_compat(file);
        return 0;
    }

    run_history_entry* ring = mem_alloc_array(capacity, run_history_entry);
    if (!ring) {
        (void)ang_file_close_compat(file);
        return 0;
    }

    int stored = 0;
    score_record_v1 temp;
    while (ang_file_read_compat(file, &temp, sizeof(temp)) == sizeof(temp)) {
        s64b detail_offset = (s64b)ang_file_tell_compat(file);
        if (!run_history_skip_details(file, &detail_offset))
            break;
        run_history_entry* slot = &ring[stored % capacity];
        slot->record = temp;
        slot->detail_offset = detail_offset;
        slot->rating = run_history_compute_rating(&temp);
        stored++;
    }

    (void)ang_file_close_compat(file);

    int count = (stored < capacity) ? stored : capacity;
    if (count <= 0) {
        mem_free(ring);
        return 0;
    }

    int start = (stored > capacity) ? (stored % capacity) : 0;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % capacity;
        out[i] = ring[idx];
    }

    mem_free(ring);
    return count;
}

static bool run_history_list_command_to_key(const app_ui_command* command,
    int count, int page_offset, int rows, int* highlight,
    run_history_filter* filter, char* out_key)
{
    static const ui_browser_shell_button_key button_keys[] = {
        { 1, '\r' },
        { 4, 'r' },
        { 5, 'f' },
        { 6, ESCAPE }
    };
    ui_browser_shell_command_map map;
    ui_browser_shell_command_result result;

    if (out_key)
        *out_key = '\0';
    if (!command || !highlight || !out_key)
        return false;

    ui_browser_shell_command_map_init(&map);
    map.button_keys = button_keys;
    map.button_key_count = N_ELEMENTS(button_keys);

    if (!ui_browser_shell_translate_command(command, &map, &result))
        return false;

    if (result.role == APP_UI_WIDGET_ROLE_LIST_ITEM)
    {
        if (result.widget_id >= 0 && result.widget_id < count
            && result.widget_id >= page_offset
            && result.widget_id < page_offset + rows)
        {
            *highlight = result.widget_id;
        }
        if (result.focus_only)
            return true;
        *out_key = result.key;
        return true;
    }

    if (result.role == APP_UI_WIDGET_ROLE_TAB)
    {
        int requested = result.widget_id - RUN_HISTORY_FILTER_TAB_BASE;

        if (filter && requested >= 0 && requested < RUN_HISTORY_FILTER_COUNT)
            *filter = (run_history_filter)requested;
        return true;
    }

    *out_key = result.key;
    return true;
}

static bool do_cmd_run_history_information(run_history_entry* entries, int count)
{
    ui_information_scene_scope scope;
    run_history_sort_order sort_order = RUN_HISTORY_SORT_DATE;
    run_history_filter filter = RUN_HISTORY_FILTER_ALL;
    run_history_entry visible_entries[RUN_HISTORY_MAX];
    int filter_counts[RUN_HISTORY_FILTER_COUNT];
    int visible_count;
    int page_offset = 0;
    int highlight = 0;

    if (!ui_information_scene_enter(&scope))
        return false;

    for (int i = 0; i < RUN_HISTORY_FILTER_COUNT; i++)
    {
        filter_counts[i] = run_history_filter_count(entries, count,
            (run_history_filter)i);
    }
    if (filter_counts[filter] <= 0)
        filter = RUN_HISTORY_FILTER_ALL;
    visible_count = run_history_apply_filter(entries, count, visible_entries,
        filter);
    if (visible_count <= 0)
    {
        ui_information_scene_leave(&scope);
        return true;
    }

    run_history_sort_entries(entries, count, sort_order);
    run_history_sort_entries(visible_entries, visible_count, sort_order);

    while (true)
    {
        int rows;
        int total_pages;
        int last_page_offset;
        int page;
        int ch;
        bool steamdeck = steamdeck_controls_active();
        char confirm_label[16] = "";
        char back_label[16] = "";
        char sort_label[16] = "";
        app_ui_scene scene;
        app_ui_panel* panel;
        const run_history_entry* selected_entry;
        const score_record_v1* rec;
        char title[APP_UI_TITLE_MAX];
        char subtitle[APP_UI_TEXT_MAX];
        char player[33];
        char date[16];
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        char created[32];
        char completed[32];
        char detail[APP_UI_TEXT_MAX];
        byte row_color;

        if (steamdeck) {
            score_ui_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            score_ui_prompt_label(steamdeck_back_key(), "B",
                back_label, sizeof(back_label));
            score_ui_prompt_label(steamdeck_secondary_key(), "Y",
                sort_label, sizeof(sort_label));
        }

        rows = RUN_HISTORY_ROWS;
        if (rows < 1)
            rows = 1;
        total_pages = (visible_count + rows - 1) / rows;
        last_page_offset = ((visible_count - 1) / rows) * rows;
        if (last_page_offset < 0)
            last_page_offset = 0;

        if (page_offset < 0)
            page_offset = 0;
        if (page_offset > last_page_offset)
            page_offset = last_page_offset;
        if (highlight < 0)
            highlight = 0;
        if (highlight >= visible_count)
            highlight = visible_count - 1;
        if (highlight < page_offset)
            page_offset = (highlight / rows) * rows;
        if (highlight >= page_offset + rows)
            page_offset = (highlight / rows) * rows;

        page = (rows > 0) ? (page_offset / rows) : 0;
        selected_entry = &visible_entries[highlight];
        rec = &selected_entry->record;

        panel = score_ui_begin_browser_scene(&scene,
            APP_UI_PANEL_FLAG_SHOW_DETAIL);
        if (!panel)
        {
            ui_information_scene_leave(&scope);
            return false;
        }
        panel->focus_area = APP_UI_FOCUS_ROWS;
        run_history_add_filter_tabs(panel, filter_counts, filter);

        strnfmt(title, sizeof(title), "Run History");
        strnfmt(subtitle, sizeof(subtitle),
            "%d/%d entries  |  Page %d/%d  |  Sort: %s  |  Filter: %s",
            visible_count, count, page + 1, total_pages,
            run_history_sort_label(sort_order),
            run_history_filter_label(filter));
        app_ui_panel_set_title(panel, TERM_L_WHITE, title);
        app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

        for (int i = page_offset; i < visible_count
            && i < page_offset + rows; i++)
        {
            const score_record_v1* row_rec = &visible_entries[i].record;
            const char* row_player = row_rec->player_name[0]
                ? row_rec->player_name
                : (row_rec->savefile_hint[0] ? row_rec->savefile_hint
                                             : "<unknown>");
            int depth_ft = row_rec->exit_depth * 50;
            char icon_char = run_history_status_short(row_rec->status);

            score_ui_run_history_format_timestamp(row_rec->completed_utc,
                false, date,
                sizeof(date));
            strnfmt(label, sizeof(label), "%s  %s", date, row_player);
            strnfmt(meta, sizeof(meta), "%s  %d pts  %d ft  %u Sil",
                score_ui_run_status_label(row_rec->status),
                visible_entries[i].rating, depth_ft,
                (unsigned)row_rec->silmarils);

            row_color = (i == highlight) ? TERM_YELLOW
                : (row_rec->status == SCORE_RECORD_ALIVE) ? TERM_L_GREEN
                : (row_rec->run_flags & SCORE_RUN_FLAG_MORGOTH_SLAIN)
                    ? TERM_L_RED
                : (row_rec->run_flags & SCORE_RUN_FLAG_ANGBAND_ESCAPED)
                    ? TERM_GREEN
                : (row_rec->silmarils > 0) ? TERM_VIOLET
                : TERM_WHITE;

            if (!app_ui_panel_add_row_ex(panel, (s16b)i, row_color, TERM_SLATE,
                    row_color, icon_char, true, i == highlight, "",
                    label, meta))
            {
                ui_information_scene_leave(&scope);
                return false;
            }
        }

        SDL_strlcpy(player, rec->player_name[0] ? rec->player_name
            : (rec->savefile_hint[0] ? rec->savefile_hint : "<unknown>"),
            sizeof(player));
        score_ui_run_history_format_timestamp(rec->created_utc, true, created,
            sizeof(created));
        score_ui_run_history_format_timestamp(rec->completed_utc, true,
            completed,
            sizeof(completed));
        strnfmt(title, sizeof(title), "%s  |  Run #%u", player,
            (unsigned)rec->record_id);
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, title);
        strnfmt(detail, sizeof(detail), "Status: %s  |  Rating: %d points",
            score_ui_run_status_label(rec->status), selected_entry->rating);
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, detail);
        strnfmt(detail, sizeof(detail), "Race: %s",
            score_ui_run_history_race_name(rec->race_id));
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, detail);
        strnfmt(detail, sizeof(detail), "Started: %s", created);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, detail);
        if (score_ui_run_history_is_current(selected_entry))
        {
            strnfmt(detail, sizeof(detail), "Current run in progress.");
            (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN, detail);
        }
        else
        {
            strnfmt(detail, sizeof(detail), "Completed: %s", completed);
            (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, detail);
        }
        strnfmt(detail, sizeof(detail), "Depth: exit %d ft  |  max %d ft",
            rec->exit_depth * 50, rec->max_depth * 50);
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, detail);
        strnfmt(detail, sizeof(detail), "Silmarils: %u  |  Quests: %u  |  Uniques: %u",
            (unsigned)rec->silmarils, (unsigned)rec->quests_completed,
            (unsigned)rec->uniques_killed);
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, detail);
        if (rec->cause_of_death[0])
        {
            (void)app_ui_panel_add_detail_line(panel,
                rec->status == SCORE_RECORD_DEAD ? TERM_L_RED : TERM_SLATE,
                rec->cause_of_death);
        }
        else if (rec->status == SCORE_RECORD_ALIVE)
        {
            (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN,
                "Alive and still delving.");
        }
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
            "Press Enter to open the full run detail view.");

        if (steamdeck)
        {
            const ui_browser_shell_footer_action actions[] = {
                { 1, TERM_L_BLUE, true, confirm_label, "Details" },
                { 2, TERM_WHITE, true, "Up/Down", "Move" },
                { 3, TERM_WHITE, true, "3/7", "Page" },
                { 4, TERM_WHITE, true, sort_label, "Sort" },
                { 5, TERM_WHITE, true, "F", "Filter" },
                { 6, TERM_WHITE, true, back_label, "Back" }
            };

            (void)ui_browser_shell_add_footer_actions(panel, actions,
                N_ELEMENTS(actions));
        }
        else
        {
            const ui_browser_shell_footer_action actions[] = {
                { 1, TERM_L_BLUE, true, "Enter", "Details" },
                { 2, TERM_WHITE, true, "8/2", "Move" },
                { 3, TERM_WHITE, true, "3/7", "Page" },
                { 4, TERM_WHITE, true, "R", "Sort" },
                { 5, TERM_WHITE, true, "F", "Filter" },
                { 6, TERM_WHITE, true, "Esc", "Back" }
            };

            (void)ui_browser_shell_add_footer_actions(panel, actions,
                N_ELEMENTS(actions));
        }

        if (!ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        {
            ui_information_scene_event event;
            char command_key = '\0';

            ch = 0;
            if (!ui_information_scene_wait_event(&event, 0))
            {
                ch = ESCAPE;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            {
                ch = event.key;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND)
            {
                run_history_filter previous_filter = filter;

                if (!run_history_list_command_to_key(&event.command,
                        visible_count, page_offset, rows, &highlight,
                        &filter, &command_key))
                {
                    continue;
                }

                ch = command_key;
                if (!ch)
                {
                    if (filter == previous_filter)
                        continue;

                    visible_count = run_history_apply_filter(entries, count,
                        visible_entries, filter);
                    run_history_sort_entries(visible_entries, visible_count,
                        sort_order);
                    page_offset = 0;
                    highlight = 0;
                    continue;
                }
            }
        }
        if (steamdeck) {
            if (ch == steamdeck_back_key())
                ch = ESCAPE;
            else if (ch == steamdeck_confirm_key())
                ch = '\r';
            else if (ch == steamdeck_secondary_key())
                ch = 'r';
        }

        switch (ch)
        {
        case ESCAPE:
        case 'q':
            ui_information_scene_leave(&scope);
            return true;

        case 'r':
        case 'R':
            sort_order = (sort_order == RUN_HISTORY_SORT_DATE)
                ? RUN_HISTORY_SORT_RATING : RUN_HISTORY_SORT_DATE;
            run_history_sort_entries(entries, count, sort_order);
            run_history_sort_entries(visible_entries, visible_count,
                sort_order);
            page_offset = 0;
            highlight = 0;
            break;

        case 'f':
        case 'F':
            filter = run_history_next_filter(filter_counts, filter);
            visible_count = run_history_apply_filter(entries, count,
                visible_entries, filter);
            run_history_sort_entries(visible_entries, visible_count,
                sort_order);
            page_offset = 0;
            highlight = 0;
            break;

        case 'y':
        case 'Y':
        case ' ':
        case '6':
        case '\r':
        case '\n':
#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
            ui_information_scene_leave(&scope);
            run_history_show_detail(&visible_entries[highlight]);
            if (!ui_information_scene_enter(&scope))
            {
                /* Detail view ran fine, but we can no longer resume the
                 * information scene; treat as completed successfully. */
                return true;
            }
            break;

        case '3':
        case 'n':
        case 'N':
            if (page_offset + rows < visible_count)
            {
                page_offset += rows;
                if (page_offset > last_page_offset)
                    page_offset = last_page_offset;
                highlight += rows;
                if (highlight >= visible_count)
                    highlight = visible_count - 1;
            }
            else
            {
                bell("Already at last page.");
            }
            break;

        case '4':
#ifdef ARROW_LEFT
        case ARROW_LEFT:
#endif
        case '7':
        case '-':
        case 'p':
        case 'P':
            if (page_offset > 0)
            {
                page_offset -= rows;
                if (page_offset < 0)
                    page_offset = 0;
                if (highlight < page_offset)
                    highlight = page_offset;
            }
            else
            {
                bell("Already at first page.");
            }
            break;

        case '8':
        case 'k':
        case 'K':
            if (highlight > 0)
            {
                highlight--;
                if (highlight < page_offset)
                    page_offset = (highlight / rows) * rows;
            }
            else
            {
                bell("Already at top entry.");
            }
            break;

        case '2':
        case 'j':
        case 'J':
            if (highlight + 1 < visible_count)
            {
                highlight++;
                if (highlight >= page_offset + rows)
                    page_offset = (highlight / rows) * rows;
            }
            else
            {
                bell("Already at last entry.");
            }
            break;

        default:
            break;
        }
    }
}

void do_cmd_run_history(void)
{
    run_history_refresh_active_run();

    {
        run_history_entry entries[RUN_HISTORY_MAX];
        int count = collect_run_history(entries, RUN_HISTORY_MAX);

        if (count <= 0)
        {
            msg_print("No run history is available.");
            return;
        }

        if (!do_cmd_run_history_information(entries, count))
        {
            log_warn("run history: information-scene presentation failed on the snapshot renderer path");
            msg_print("Run history viewer unavailable.");
        }
        return;
    }
}

#include "score/score_ui.h"

#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "platform-input.h"
#include "score/score_entry.h"
#include "score/score_io.h"
#include "score/score_logic.h"
#include "score/score_runs.h"
#include "ui/ui-information-scene.h"
#include "metarun.h"

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

static void run_history_show_detail(const run_history_entry* entry);
static bool run_history_prepare_artefact_object(
    const score_run_artefact_v1* entry, object_type* out);

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

static bool run_history_is_current(const run_history_entry* entry)
{
    if (!entry)
        return false;
    if (!character_generated || !p_ptr || p_ptr->is_dead)
        return false;
    if (entry->record.status != SCORE_RECORD_ALIVE)
        return false;
    return (entry->record.metarun_id == metar.id);
}

static high_score forced_highlight_entry;
static bool forced_highlight_active = false;
static bool force_interactive_scores = false;
static bool score_last_layout_short = true;

static void score_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

typedef enum
{
    SCORE_VIEW_ORDER_SCORE = 0,
    SCORE_VIEW_ORDER_CHRONOLOGY = 1
} score_view_order;

typedef enum
{
    RUN_HISTORY_SORT_DATE = 0,
    RUN_HISTORY_SORT_RATING = 1
} run_history_sort_order;

static const char* run_history_sort_label(run_history_sort_order order)
{
    return (order == RUN_HISTORY_SORT_RATING) ? "Rating" : "Date";
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

    if (!ui_information_scene_enter(&scope))
    {
        log_warn("scores: unable to enter information-scene scope");
        return 0;
    }

    if (steamdeck)
    {
        score_prompt_label(steamdeck_secondary_key(), "Y", order_label,
            sizeof(order_label));
        score_prompt_label(steamdeck_alt_action_key(), "X", layout_label,
            sizeof(layout_label));
        score_prompt_label(steamdeck_back_key(), "B", exit_label,
            sizeof(exit_label));
        score_prompt_label(steamdeck_confirm_key(), "A", next_label,
            sizeof(next_label));
    }

    if (!entries || count <= 0)
    {
        app_ui_scene scene;
        app_ui_panel* panel;

        app_ui_scene_init(&scene);
        panel = app_ui_scene_append_panel(&scene, APP_UI_LAYER_BROWSER);
        if (!panel)
        {
            ui_information_scene_leave(&scope);
            return 0;
        }
        panel->style = APP_UI_PANEL_STYLE_BROWSER;
        panel->flags |= APP_UI_PANEL_FLAG_SHOW_DETAIL;
        panel->accent_attr = TERM_L_BLUE;
        app_ui_panel_set_widths(panel, 1180, 2200);
        app_ui_panel_set_title(panel, TERM_L_WHITE, "Halls of Mandos");
        app_ui_panel_set_subtitle(panel, TERM_SLATE,
            score_view_order_label(order));
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
            "No recorded heroes yet.");
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            "No recorded heroes yet.");
        if (steamdeck)
        {
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
                next_label, "Close");
        }
        else
        {
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
                "Any", "Close");
        }
        (void)ui_information_scene_present_ui(&scene);
        (void)ui_information_scene_wait_key_nonrepeat();
        ui_information_scene_leave(&scope);
        return 0;
    }

    {
        int start_index = 0;
        bool highlight_pending = true;

        while (start_index < count)
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
                int max_start = count - entries_per_page;

                if (max_start < 0)
                    max_start = 0;
                start_index = (highlight_index / entries_per_page)
                    * entries_per_page;
                if (start_index > max_start)
                    start_index = max_start;
                highlight_pending = false;
            }

            selected_index = (highlight_index >= start_index
                && highlight_index < start_index + entries_per_page)
                ? highlight_index
                : start_index;
            has_more = (start_index + entries_per_page < count);

            app_ui_scene_init(&scene);
            panel = app_ui_scene_append_panel(&scene, APP_UI_LAYER_BROWSER);
            if (!panel)
            {
                log_warn("scores: failed to build browser scene");
                ui_information_scene_leave(&scope);
                return 0;
            }
            panel->style = APP_UI_PANEL_STYLE_BROWSER;
            panel->flags |= APP_UI_PANEL_FLAG_SHOW_DETAIL;
            panel->focus_area = APP_UI_FOCUS_ROWS;
            panel->accent_attr = TERM_L_BLUE;
            app_ui_panel_set_widths(panel, 1180, 2200);

            strnfmt(title, sizeof(title), "Halls of Mandos");
            strnfmt(subtitle, sizeof(subtitle),
                "%s  |  Layout: %s  |  Page %d",
                score_view_order_label(order), detailed ? "Full" : "Short",
                (start_index / entries_per_page) + 1);
            app_ui_panel_set_title(panel, TERM_L_WHITE, title);
            app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

            for (int idx = start_index; idx < count
                && idx < start_index + entries_per_page; idx++)
            {
                char label[APP_UI_LABEL_MAX];
                char meta[APP_UI_META_MAX];
                bool selected = (idx == selected_index);
                byte attr = score_entry_color(&entries[idx], selected);
                char icon_char = ' ';

                if (entries[idx].morgoth_slain[0] == 't')
                    icon_char = 'V';
                else if (score_entry_silmarils(&entries[idx]) > 0)
                    icon_char = '*';

                score_build_browser_row_label(&entries[idx], idx + 1, label,
                    sizeof(label));
                score_build_browser_row_meta(&entries[idx], detailed, meta,
                    sizeof(meta));
                if (!app_ui_panel_add_row_ex(panel, (s16b)idx, attr,
                        TERM_SLATE, attr, icon_char, true, selected, "",
                        label, meta))
                {
                    ui_information_scene_leave(&scope);
                    return 0;
                }
            }

            score_add_browser_detail(panel, &entries[selected_index],
                selected_index + 1, detailed);

            if (steamdeck)
            {
                (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
                    order_label, "Order");
                (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                    layout_label, "Layout");
                (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                    exit_label, "Exit");
                (void)app_ui_panel_add_footer_action(panel, 4, TERM_L_BLUE, true,
                    next_label, has_more ? "Next" : "Close");
            }
            else
            {
                (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
                    "S", "Order");
                (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                    "L", "Layout");
                (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                    "Esc", "Exit");
                (void)app_ui_panel_add_footer_action(panel, 4, TERM_L_BLUE, true,
                    "Any", has_more ? "Next" : "Close");
            }

            if (!ui_information_scene_present_ui(&scene))
            {
                log_warn("scores: failed to present browser page");
                ui_information_scene_leave(&scope);
                return 0;
            }

            ch = ui_information_scene_wait_key();

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

    SDL_CloseIO(temp_ctx.fd);
    score_file_reset_ctx(&temp_ctx);
}

#if 0
static const char* score_run_killer_kind_label(score_killer_kind kind)
{
    switch (kind) {
    case SCORE_KILLER_MONSTER: return "Monster";
    case SCORE_KILLER_TRAP: return "Trap";
    case SCORE_KILLER_FALL: return "Fall";
    case SCORE_KILLER_SELF: return "Self";
    case SCORE_KILLER_OTHER: return "Other";
    default: return "Unknown";
    }
}
#endif

static const char* score_run_status_label(score_record_status status)
{
    switch (status) {
    case SCORE_RECORD_ALIVE: return "Alive";
    case SCORE_RECORD_DEAD: return "Dead";
    case SCORE_RECORD_ESCAPED: return "Escaped";
    default: return "Unknown";
    }
}

static bool run_history_skip_details(SDL_IOStream* file, s64b* detail_offset)
{
    if (!file)
        return false;

    Sint64 header_pos = SDL_TellIO(file);
    score_run_detail_header_v1 header;
    if (SDL_ReadIO(file, &header, sizeof(header)) != sizeof(header))
        return false;

    if (detail_offset)
        *detail_offset = (s64b)header_pos;

    return score_runs_skip_detail_payload(file, &header);
}
static const char* run_history_race_name(byte idx)
{
    if (!p_info || !p_name || !z_info || idx >= z_info->p_max)
        return "<unknown>";
    return p_name + p_info[idx].name;
}
#if 0
static const char* run_history_character_name(byte idx)
{
    if (!c_info || !c_name || !z_info || idx >= z_info->c_max)
        return "<unknown>";
    return c_name + c_info[idx].name;
}
#endif
static const char* run_history_monster_name(u16b r_idx)
{
    if (!r_info || !r_name || !z_info || r_idx == 0 || r_idx >= z_info->r_max)
        return "<unknown>";
    return r_name + r_info[r_idx].name;
}
static void run_history_format_timestamp(u32b utc, bool include_time,
                                         char* out, size_t out_len)
{
    if (!out || out_len == 0)
        return;

    if (!utc) {
        SDL_strlcpy(out, "----", out_len);
        return;
    }

    time_t ts = (time_t)utc;
    struct tm* tm_info = localtime(&ts);
    if (!tm_info) {
        SDL_strlcpy(out, "----", out_len);
        return;
    }

    const char* fmt = include_time ? "%Y-%m-%d %H:%M" : "%Y-%m-%d";
    if (strftime(out, out_len, fmt, tm_info) == 0) {
        SDL_strlcpy(out, "----", out_len);
    }
}

#if 0
static void run_history_format_flags(byte run_flags, char* out, size_t out_len)
{
    if (!out || out_len == 0)
        return;

    out[0] = '\0';
    bool first = true;

    #define APPEND_FLAG(label) \
        do { \
            if (!first) SDL_strlcat(out, ", ", out_len); \
            SDL_strlcat(out, (label), out_len); \
            first = false; \
        } while (0)

    if (run_flags & SCORE_RUN_FLAG_MORGOTH_SLAIN)
        APPEND_FLAG("Morgoth slain");
    if (run_flags & SCORE_RUN_FLAG_ANGBAND_ESCAPED)
        APPEND_FLAG("Escaped");
    if (run_flags & SCORE_RUN_FLAG_NOSCORE)
        APPEND_FLAG("No score");
    if (run_flags & SCORE_RUN_FLAG_CHEAT)
        APPEND_FLAG("Cheat");
    if (run_flags & SCORE_RUN_FLAG_BLITZ)
        APPEND_FLAG("Blitz");

    #undef APPEND_FLAG

    if (first)
        SDL_strlcpy(out, "(none)", out_len);
}
#endif

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
    SDL_IOStream* file = SDL_IOFromFile(path, "rb");
    safe_setuid_drop();

    if (!file)
        return 0;

    score_db_header header;
    if (SDL_ReadIO(file, &header, sizeof(header)) != sizeof(header) ||
        memcmp(header.magic, SCORE_DB_MAGIC, sizeof(header.magic)) != 0) {
        SDL_CloseIO(file);
        return 0;
    }

    run_history_entry* ring = mem_alloc_array(capacity, run_history_entry);
    if (!ring) {
        SDL_CloseIO(file);
        return 0;
    }

    int stored = 0;
    score_record_v1 temp;
    while (SDL_ReadIO(file, &temp, sizeof(temp)) == sizeof(temp)) {
        s64b detail_offset = (s64b)SDL_TellIO(file);
        if (!run_history_skip_details(file, &detail_offset))
            break;
        run_history_entry* slot = &ring[stored % capacity];
        slot->record = temp;
        slot->detail_offset = detail_offset;
        slot->rating = run_history_compute_rating(&temp);
        stored++;
    }

    SDL_CloseIO(file);

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

static bool do_cmd_run_history_information(run_history_entry* entries, int count)
{
    ui_information_scene_scope scope;
    run_history_sort_order sort_order = RUN_HISTORY_SORT_DATE;
    int page_offset = 0;
    int highlight = 0;

    if (!ui_information_scene_enter(&scope))
        return false;

    run_history_sort_entries(entries, count, sort_order);

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
            score_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            score_prompt_label(steamdeck_back_key(), "B",
                back_label, sizeof(back_label));
            score_prompt_label(steamdeck_secondary_key(), "Y",
                sort_label, sizeof(sort_label));
        }

        rows = RUN_HISTORY_ROWS;
        if (rows < 1)
            rows = 1;
        total_pages = (count + rows - 1) / rows;
        last_page_offset = ((count - 1) / rows) * rows;
        if (last_page_offset < 0)
            last_page_offset = 0;

        if (page_offset < 0)
            page_offset = 0;
        if (page_offset > last_page_offset)
            page_offset = last_page_offset;
        if (highlight < 0)
            highlight = 0;
        if (highlight >= count)
            highlight = count - 1;
        if (highlight < page_offset)
            page_offset = (highlight / rows) * rows;
        if (highlight >= page_offset + rows)
            page_offset = (highlight / rows) * rows;

        page = (rows > 0) ? (page_offset / rows) : 0;
        selected_entry = &entries[highlight];
        rec = &selected_entry->record;

        app_ui_scene_init(&scene);
        panel = app_ui_scene_append_panel(&scene, APP_UI_LAYER_BROWSER);
        if (!panel)
        {
            ui_information_scene_leave(&scope);
            return false;
        }
        panel->style = APP_UI_PANEL_STYLE_BROWSER;
        panel->flags |= APP_UI_PANEL_FLAG_SHOW_DETAIL;
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->accent_attr = TERM_L_BLUE;
        app_ui_panel_set_widths(panel, 1180, 2200);

        strnfmt(title, sizeof(title), "Run History");
        strnfmt(subtitle, sizeof(subtitle), "%d entries  |  Page %d/%d  |  Sort: %s",
            count, page + 1, total_pages, run_history_sort_label(sort_order));
        app_ui_panel_set_title(panel, TERM_L_WHITE, title);
        app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

        for (int i = page_offset; i < count && i < page_offset + rows; i++)
        {
            const score_record_v1* row_rec = &entries[i].record;
            const char* row_player = row_rec->player_name[0]
                ? row_rec->player_name
                : (row_rec->savefile_hint[0] ? row_rec->savefile_hint
                                             : "<unknown>");
            int depth_ft = row_rec->exit_depth * 50;
            char icon_char = run_history_status_short(row_rec->status);

            run_history_format_timestamp(row_rec->completed_utc, false, date,
                sizeof(date));
            strnfmt(label, sizeof(label), "%s  %s", date, row_player);
            strnfmt(meta, sizeof(meta), "%s  %d pts  %d ft  %u Sil",
                score_run_status_label(row_rec->status), entries[i].rating,
                depth_ft, (unsigned)row_rec->silmarils);

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
        run_history_format_timestamp(rec->created_utc, true, created,
            sizeof(created));
        run_history_format_timestamp(rec->completed_utc, true, completed,
            sizeof(completed));
        strnfmt(title, sizeof(title), "%s  |  Run #%u", player,
            (unsigned)rec->record_id);
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, title);
        strnfmt(detail, sizeof(detail), "Status: %s  |  Rating: %d points",
            score_run_status_label(rec->status), selected_entry->rating);
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, detail);
        strnfmt(detail, sizeof(detail), "Race: %s", run_history_race_name(
            rec->race_id));
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, detail);
        strnfmt(detail, sizeof(detail), "Started: %s", created);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, detail);
        if (run_history_is_current(selected_entry))
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
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                confirm_label, "Details");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "Up/Down", "Move");
            (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "3/7", "Page");
            (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                sort_label, "Sort");
            (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
                back_label, "Back");
        }
        else
        {
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                "Enter", "Details");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "8/2", "Move");
            (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "3/7", "Page");
            (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                "R", "Sort");
            (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
                "Esc", "Back");
        }

        if (!ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = ui_information_scene_wait_key();
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
            run_history_show_detail(&entries[highlight]);
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
            if (page_offset + rows < count)
            {
                page_offset += rows;
                if (page_offset > last_page_offset)
                    page_offset = last_page_offset;
                highlight += rows;
                if (highlight >= count)
                    highlight = count - 1;
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
            if (highlight + 1 < count)
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
        status_color, score_run_status_label(rec->status));
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

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return -1;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 1180, 2200);
    run_history_ui_build_header(title, sizeof(title), subtitle,
        sizeof(subtitle), rec, player, race_name,
        score_run_status_label(rec->status));
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

static void run_history_show_detail(const run_history_entry* entry)
{
    if (!entry)
        return;

    const score_record_v1* rec = &entry->record;
    score_run_detail_block details;
    memset(&details, 0, sizeof(details));
    bool have_details = (entry->detail_offset >= 0)
        && score_runs_load_details(entry->detail_offset, &details);

    bool current_run = run_history_is_current(entry);
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
    run_history_format_timestamp(rec->created_utc, true, created, sizeof(created));
    run_history_format_timestamp(rec->completed_utc, true, completed, sizeof(completed));

    const char* race_name = run_history_race_name(rec->race_id);

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

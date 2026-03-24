/* File: level-generation.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "level-generation/level-generation-internal.h"
#include "log/log.h"
#include "gen-log.h"
#include "metarun.h"
#include <SDL3/SDL.h>
/* Ensure C library prototypes are visible for tools */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>


typedef enum {
    LEVEL_GEN_STAGE_PLANNING = 0,
    LEVEL_GEN_STAGE_FOUNDATIONS,
    LEVEL_GEN_STAGE_SHAPING,
    LEVEL_GEN_STAGE_LINKING,
    LEVEL_GEN_STAGE_ENTRY,
    LEVEL_GEN_STAGE_TREASURE,
    LEVEL_GEN_STAGE_MONSTERS,
    LEVEL_GEN_STAGE_FINALIZING,
    LEVEL_GEN_STAGE_COUNT
} level_gen_screen_stage_t;

#define LEVEL_GEN_STAGE_DONE LEVEL_GEN_STAGE_COUNT
#define LEVEL_GEN_SCREEN_DEBUG_LINES 32
#define LEVEL_GEN_SCREEN_ISSUES 12

typedef struct level_gen_issue_count {
    char key[64];
    int count;
} level_gen_issue_count;

typedef struct level_gen_screen_state {
    bool active;
    bool debug;
    bool screen_saved;
    int attempt;
    int total_failures;
    int stage;
    int spinner;
    Uint64 last_draw_ticks;
    char depth_label[64];
    char status_text[160];
    char detail_text[160];
    char final_text[160];
    char last_failure[160];
    char last_quest_vault_failure[160];
    char debug_lines[LEVEL_GEN_SCREEN_DEBUG_LINES][160];
    int debug_count;
    level_gen_issue_count issues[LEVEL_GEN_SCREEN_ISSUES];
    int issue_count;
} level_gen_screen_state;

static level_gen_screen_state level_gen_screen = {0};
static char level_gen_debug_last_greater_vault_name[80] = "";
static char level_gen_debug_active_quest_vault_name[80] = "";
static char level_gen_debug_last_quest_vault_name[80] = "";
static char level_gen_debug_questgiver_name[80] = "";
static char level_gen_debug_last_room_name[80] = "";

static const char* level_gen_stage_user_labels[LEVEL_GEN_STAGE_COUNT] = {
    "Planning the level",
    "Laying the bedrock",
    "Shaping the halls",
    "Linking passages",
    "Setting doors and stairs",
    "Stocking the treasure",
    "Mustering the foes",
    "Final checks"
};

static const char* level_gen_stage_debug_labels[LEVEL_GEN_STAGE_COUNT] = {
    "Planning",
    "Foundations",
    "Rooms and partitions",
    "Connections",
    "Entry placement",
    "Objects and traps",
    "Monster pass",
    "Final validation"
};

static const char* level_gen_stage_status[LEVEL_GEN_STAGE_COUNT] = {
    "Surveying the next depth.",
    "Setting the foundation.",
    "The halls are taking shape.",
    "Passages are being linked.",
    "Doors and stairs are being set.",
    "Treasure is being placed.",
    "Creatures are moving in.",
    "Checking the final details."
};


static void level_gen_debug_reset_context(void)
{
    g_vault_name[0] = '\0';
    level_gen_debug_last_greater_vault_name[0] = '\0';
    level_gen_debug_active_quest_vault_name[0] = '\0';
    level_gen_debug_last_quest_vault_name[0] = '\0';
    level_gen_debug_questgiver_name[0] = '\0';
    level_gen_debug_last_room_name[0] = '\0';
}

static void level_gen_screen_build_partition_summary(char* total_buf,
    size_t total_buflen, char* types_buf, size_t types_buflen);

static void level_gen_screen_build_generated_summary(char* quest_buf,
    size_t quest_buflen, char* roulette_buf, size_t roulette_buflen,
    char* giver_buf, size_t giver_buflen, char* gv_buf, size_t gv_buflen);

static void level_gen_screen_fit_text(char* buf, size_t buflen, cptr text,
    int max_chars)
{
    if (!buflen)
        return;

    if (!text)
        text = "";

    if (max_chars <= 0)
    {
        buf[0] = '\0';
    }
    else if ((int)strlen(text) <= max_chars)
    {
        SDL_strlcpy(buf, text, buflen);
    }
    else if (max_chars <= 3)
    {
        strnfmt(buf, buflen, "%.*s", max_chars, text);
    }
    else
    {
        strnfmt(buf, buflen, "%.*s...", max_chars - 3, text);
    }
}

static void level_gen_screen_put_centered(int row, byte attr, cptr text)
{
    int wid = 80;
    int hgt = 24;
    char buf[256];
    int col;

    Term_get_size(&wid, &hgt);
    if (row < 0 || row >= hgt)
        return;

    level_gen_screen_fit_text(buf, sizeof(buf), text, MAX(1, wid - 2));
    col = (wid - (int)strlen(buf)) / 2;
    if (col < 0)
        col = 0;

    Term_putstr(col, row, -1, attr, buf);
}

static void level_gen_screen_put_fitted(int col, int row, int width, byte attr,
    cptr text)
{
    char buf[256];

    if (width <= 0)
        return;

    level_gen_screen_fit_text(buf, sizeof(buf), text, width);
    Term_putstr(col, row, -1, attr, buf);
}

static int level_gen_screen_print_wrapped(int row, int col, int width,
    int max_lines, byte attr, cptr text)
{
    const char* p = text ? text : "";
    int lines = 0;

    if (width <= 0 || max_lines <= 0)
        return 0;

    while (*p && lines < max_lines)
    {
        char buf[256];
        const char* start;
        int len = 0;
        int last_space = -1;
        bool forced_newline = false;

        while (*p == ' ')
            p++;

        if (!*p)
            break;

        start = p;

        while (*p && *p != '\n')
        {
            char ch = isprint((unsigned char)*p) ? *p : ' ';

            if (len < MIN(width, (int)sizeof(buf) - 1))
            {
                if (ch == ' ')
                    last_space = len;
                buf[len++] = ch;
                p++;
                continue;
            }

            break;
        }

        if (*p == '\n')
        {
            forced_newline = true;
            p++;
        }
        else if (*p && len >= width && last_space >= 0)
        {
            p = start + last_space + 1;
            len = last_space;
        }

        while (len > 0 && buf[len - 1] == ' ')
            len--;

        if ((lines == max_lines - 1) && *p)
        {
            if (width > 3)
            {
                if (len > width - 3)
                    len = width - 3;
                memcpy(buf + len, "...", 3);
                len += 3;
            }
        }

        buf[len] = '\0';
        Term_putstr(col, row + lines, -1, attr, buf);
        lines++;

        if (forced_newline)
            continue;
    }

    return lines;
}

static int level_gen_screen_count_wrapped_lines(int width, int max_lines,
    cptr text)
{
    const char* p = text ? text : "";
    int lines = 0;

    if (width <= 0 || max_lines <= 0)
        return 0;

    while (*p && lines < max_lines)
    {
        const char* start;
        int len = 0;
        int last_space = -1;

        while (*p == ' ')
            p++;

        if (!*p)
            break;

        start = p;

        while (*p && *p != '\n')
        {
            char ch = isprint((unsigned char)*p) ? *p : ' ';

            if (len < width)
            {
                if (ch == ' ')
                    last_space = len;
                len++;
                p++;
                continue;
            }

            break;
        }

        if (*p == '\n')
        {
            p++;
        }
        else if (*p && len >= width && last_space >= 0)
        {
            p = start + last_space + 1;
        }

        lines++;
    }

    return lines;
}

static void level_gen_screen_format_depth_label(char* buf, size_t buflen)
{
    if (!p_ptr || p_ptr->depth <= 0)
        SDL_strlcpy(buf, "The Gates of Angband", buflen);
    else
        strnfmt(buf, buflen, "%d ft.", p_ptr->depth * 50);
}

static bool level_gen_screen_capture_category(cptr category)
{
    if (!category)
        return false;

    return !strcmp(category, "SUMMARY")
        || !strcmp(category, "FAIL")
        || !strcmp(category, "QUEST")
        || !strcmp(category, "PARTITION")
        || !strcmp(category, "CONNECT")
        || !strcmp(category, "STAIRS");
}

static void level_gen_screen_append_debug_line(cptr text)
{
    if (!text || !text[0])
        return;

    if (level_gen_screen.debug_count >= LEVEL_GEN_SCREEN_DEBUG_LINES)
    {
        memmove(level_gen_screen.debug_lines, level_gen_screen.debug_lines + 1,
            (LEVEL_GEN_SCREEN_DEBUG_LINES - 1)
            * sizeof(level_gen_screen.debug_lines[0]));
        level_gen_screen.debug_count = LEVEL_GEN_SCREEN_DEBUG_LINES - 1;
    }

    SDL_strlcpy(level_gen_screen.debug_lines[level_gen_screen.debug_count], text,
        sizeof(level_gen_screen.debug_lines[0]));
    level_gen_screen.debug_count++;
}

static bool level_gen_screen_extract_context_value(cptr reason, cptr key,
    char* buf, size_t buflen)
{
    char pattern[16];
    const char* start;
    const char* end;

    if (!reason || !key || !buf || buflen == 0)
        return false;

    strnfmt(pattern, sizeof(pattern), "%s=", key);
    start = strstr(reason, pattern);
    if (!start)
        return false;

    start += strlen(pattern);
    end = start;
    while (*end && *end != ',' && *end != ']')
        end++;

    while (end > start && isspace((unsigned char)end[-1]))
        end--;

    if (end <= start)
        return false;

    strnfmt(buf, buflen, "%.*s", (int)(end - start), start);
    return true;
}

static void level_gen_screen_extract_issue_key(cptr reason, char* buf,
    size_t buflen)
{
    char named[80];
    const char* end;
    size_t len;

    if (!reason || !reason[0])
    {
        SDL_strlcpy(buf, "Generation failed", buflen);
        return;
    }

    if (strstr(reason, "QUEST VAULT FAILED"))
    {
        if (level_gen_screen_extract_context_value(reason, "qv", named, sizeof(named)))
        {
            strnfmt(buf, buflen, "QUEST VAULT FAILED (%s)", named);
            return;
        }
        if (level_gen_debug_last_quest_vault_name[0])
        {
            strnfmt(buf, buflen, "QUEST VAULT FAILED (%s)",
                level_gen_debug_last_quest_vault_name);
            return;
        }
    }

    if (level_gen_screen_extract_context_value(reason, "gv", named, sizeof(named)))
    {
        end = strchr(reason, ':');
        if (!end)
            end = reason + strlen(reason);
        while (end > reason && isspace((unsigned char)end[-1]))
            end--;
        strnfmt(buf, buflen, "%.*s (%s)", (int)(end - reason), reason, named);
        return;
    }

    if (level_gen_screen_extract_context_value(reason, "qv", named, sizeof(named)))
    {
        end = strchr(reason, ':');
        if (!end)
            end = reason + strlen(reason);
        while (end > reason && isspace((unsigned char)end[-1]))
            end--;
        strnfmt(buf, buflen, "%.*s (%s)", (int)(end - reason), reason, named);
        return;
    }

    end = strchr(reason, ':');
    if (!end)
        end = reason + strlen(reason);

    while (end > reason && isspace((unsigned char)end[-1]))
        end--;

    len = (size_t)(end - reason);
    if (len >= buflen)
        len = buflen - 1;

    memcpy(buf, reason, len);
    buf[len] = '\0';
}

static void level_gen_screen_record_issue(cptr reason)
{
    char key[64];

    level_gen_screen_extract_issue_key(reason, key, sizeof(key));

    for (int i = 0; i < level_gen_screen.issue_count; i++)
    {
        if (!strcmp(level_gen_screen.issues[i].key, key))
        {
            level_gen_screen.issues[i].count++;
            return;
        }
    }

    if (level_gen_screen.issue_count >= LEVEL_GEN_SCREEN_ISSUES)
        return;

    SDL_strlcpy(level_gen_screen.issues[level_gen_screen.issue_count].key, key,
        sizeof(level_gen_screen.issues[level_gen_screen.issue_count].key));
    level_gen_screen.issues[level_gen_screen.issue_count].count = 1;
    level_gen_screen.issue_count++;
}

void level_gen_debug_note_room_name(cptr name)
{
    if (name && name[0])
        SDL_strlcpy(level_gen_debug_last_room_name, name,
            sizeof(level_gen_debug_last_room_name));
}

void level_gen_debug_note_greater_vault_name(cptr name)
{
    if (name && name[0])
        SDL_strlcpy(level_gen_debug_last_greater_vault_name, name,
            sizeof(level_gen_debug_last_greater_vault_name));
}

void level_gen_debug_note_quest_vault_name(cptr name)
{
    if (name && name[0])
        SDL_strlcpy(level_gen_debug_last_quest_vault_name, name,
            sizeof(level_gen_debug_last_quest_vault_name));
}

static const char* level_gen_debug_quest_name(int quest_id)
{
    quest_type* q_ptr;

    if (quest_id > 0 && quest_id < z_info->quest_max)
    {
        q_ptr = &quest_info[quest_id];
        if (q_ptr->name && quest_name_text)
            return quest_name_text + q_ptr->name;
    }

    switch (quest_id)
    {
    case QUEST_ID_TULKAS:
        return "Tulkas the Strong";
    case QUEST_ID_AULE:
        return "Aule the Smith";
    case QUEST_ID_MANDOS:
        return "Mandos the Doomsman";
    case QUEST_ID_NIENA:
        return "Niena, Lady of Pity";
    case QUEST_ID_OROME:
        return "Orome the Hunter";
    case QUEST_ID_VARDA:
        return "Varda, Lady of the Stars";
    default:
        return NULL;
    }
}

void level_gen_debug_note_questgiver(int quest_id)
{
    const char* name = level_gen_debug_quest_name(quest_id);

    if (name && name[0])
        SDL_strlcpy(level_gen_debug_questgiver_name, name,
            sizeof(level_gen_debug_questgiver_name));
}

void level_gen_debug_activate_quest_vault_name(cptr name)
{
    if (name && name[0])
        SDL_strlcpy(level_gen_debug_active_quest_vault_name, name,
            sizeof(level_gen_debug_active_quest_vault_name));
}

static void level_gen_debug_append_context(char* buf, size_t buflen, cptr key,
    cptr value)
{
    char tmp[256];

    if (!value || !value[0])
        return;

    if (buf[0])
        strnfmt(tmp, sizeof(tmp), "%s, %s=%s", buf, key, value);
    else
        strnfmt(tmp, sizeof(tmp), "%s=%s", key, value);

    SDL_strlcpy(buf, tmp, buflen);
}

static void level_gen_debug_build_failure_reason(char* buf, size_t buflen,
    cptr reason)
{
    char context[256] = "";

    level_gen_debug_append_context(
        context, sizeof(context), "gv",
        g_vault_name[0] ? g_vault_name : level_gen_debug_last_greater_vault_name);
    level_gen_debug_append_context(
        context, sizeof(context), "qv",
        level_gen_debug_active_quest_vault_name[0]
            ? level_gen_debug_active_quest_vault_name
            : level_gen_debug_last_quest_vault_name);
    level_gen_debug_append_context(
        context, sizeof(context), "room", level_gen_debug_last_room_name);

    if (context[0])
        strnfmt(buf, buflen, "%s [%s]", reason ? reason : "Generation failed.",
            context);
    else
        SDL_strlcpy(buf, reason ? reason : "Generation failed.", buflen);
}

static void level_gen_screen_build_generated_summary(char* quest_buf,
    size_t quest_buflen, char* roulette_buf, size_t roulette_buflen,
    char* giver_buf, size_t giver_buflen, char* gv_buf, size_t gv_buflen)
{
    int roulette_winner = debug_get_quest_lottery_winner();
    const char* roulette_name = level_gen_debug_quest_name(roulette_winner);

    if (level_gen_debug_active_quest_vault_name[0])
        strnfmt(quest_buf, quest_buflen, "Quest vaults: 1 (%s)",
            level_gen_debug_active_quest_vault_name);
    else
        SDL_strlcpy(quest_buf, "Quest vaults: 0", quest_buflen);

    if (roulette_name && roulette_name[0])
        strnfmt(roulette_buf, roulette_buflen, "Roulette winner: %s",
            roulette_name);
    else
        SDL_strlcpy(roulette_buf, "Roulette winner: none", roulette_buflen);

    if (level_gen_debug_questgiver_name[0])
        strnfmt(giver_buf, giver_buflen, "Quest giver spawned: 1 (%s)",
            level_gen_debug_questgiver_name);
    else
        SDL_strlcpy(giver_buf, "Quest giver spawned: 0", giver_buflen);

    if (g_vault_name[0])
        strnfmt(gv_buf, gv_buflen, "Greater vaults: 1 (%s)", g_vault_name);
    else
        SDL_strlcpy(gv_buf, "Greater vaults: 0", gv_buflen);
}

static void level_gen_screen_draw_recent_events(int row, int col, int width,
    int max_rows)
{
    int start = level_gen_screen.debug_count;
    int used_rows = 0;

    if (width <= 0 || max_rows <= 0)
        return;

    while (start > 0)
    {
        int remaining = max_rows - used_rows;
        int needed;

        if (remaining <= 0)
            break;

        needed = level_gen_screen_count_wrapped_lines(width, remaining,
            level_gen_screen.debug_lines[start - 1]);
        if (needed <= 0)
        {
            start--;
            continue;
        }

        used_rows += needed;
        start--;
    }

    for (int i = start; i < level_gen_screen.debug_count && max_rows > 0; ++i)
    {
        int drawn = level_gen_screen_print_wrapped(row, col, width, max_rows,
            TERM_SLATE, level_gen_screen.debug_lines[i]);

        row += drawn;
        max_rows -= drawn;
    }
}

static void level_gen_screen_draw_user(int wid, int hgt)
{
    static const char spinner_frames[] = "|/-\\";
    int top;
    int row;
    int stage_row;
    int bar_width;
    int filled = 0;
    int progress_units = 0;
    char buf[256];
    char bar[80];

    level_gen_screen.spinner =
        (level_gen_screen.spinner + 1) % ((int)sizeof(spinner_frames) - 1);

    top = MAX(0, (hgt - (LEVEL_GEN_STAGE_COUNT + 8)) / 2);
    row = top;

    level_gen_screen_put_centered(row++, TERM_L_BLUE, "Preparing the Level");
    level_gen_screen_put_centered(row++, TERM_L_WHITE, level_gen_screen.depth_label);
    row++;

    if (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE)
    {
        level_gen_screen_put_centered(
            row++, TERM_L_GREEN,
            level_gen_screen.final_text[0] ? level_gen_screen.final_text
                                           : "The level is ready.");
    }
    else
    {
        strnfmt(buf, sizeof(buf), "%c %s",
            spinner_frames[level_gen_screen.spinner],
            level_gen_screen.status_text[0] ? level_gen_screen.status_text
                                            : "Preparing the level.");
        level_gen_screen_put_centered(row++, TERM_YELLOW, buf);
    }

    bar_width = MIN(34, MAX(18, wid - 10));
    if (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE)
    {
        filled = bar_width;
    }
    else
    {
        progress_units = (2 * MAX(level_gen_screen.stage, 0)) + 1;
        filled = (progress_units * bar_width) / (2 * LEVEL_GEN_STAGE_COUNT);
        if (filled < 1)
            filled = 1;
        if (filled > bar_width)
            filled = bar_width;
    }

    if ((size_t)(bar_width + 3) > sizeof(bar))
        bar_width = (int)sizeof(bar) - 3;

    bar[0] = '[';
    for (int i = 0; i < bar_width; i++)
        bar[i + 1] = (i < filled) ? '#' : '.';
    bar[bar_width + 1] = ']';
    bar[bar_width + 2] = '\0';
    level_gen_screen_put_centered(row++, TERM_L_GREEN, bar);

    row++;
    stage_row = row;

    for (int i = 0; i < LEVEL_GEN_STAGE_COUNT && stage_row < hgt - 1; i++)
    {
        byte attr = TERM_SLATE;
        char marker = ' ';

        if (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE
            || i < level_gen_screen.stage)
        {
            attr = TERM_L_GREEN;
            marker = 'x';
        }
        else if (i == level_gen_screen.stage)
        {
            attr = TERM_YELLOW;
            marker = '>';
        }

        strnfmt(buf, sizeof(buf), "[%c] %s", marker,
            level_gen_stage_user_labels[i]);
        level_gen_screen_put_centered(stage_row++, attr, buf);
    }

    level_gen_screen_put_centered(
        hgt - 1, TERM_SLATE, "Large levels can take a moment.");
}

static void level_gen_screen_draw_debug(int wid, int hgt)
{
    char buf[256];
    char qv_status_buf[256];
    char partition_buf[256];
    char type_buf[256];
    char quest_buf[256];
    char roulette_buf[256];
    char giver_buf[256];
    char gv_buf[256];
    int width = MAX(1, wid - 2);
    int footer_row = MAX(0, hgt - 1);
    bool split = (wid >= 90);

    level_gen_screen_put_centered(0, TERM_L_BLUE, "Level Generation Debug");

    strnfmt(buf, sizeof(buf), "%s | attempts %d | retries %d | %s",
        level_gen_screen.depth_label,
        MAX(level_gen_screen.attempt, 1),
        level_gen_screen.total_failures,
        (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE) ? "ready" : "running");
    level_gen_screen_put_fitted(1, 1, width, TERM_L_WHITE, buf);

    if (level_gen_debug_active_quest_vault_name[0])
    {
        strnfmt(qv_status_buf, sizeof(qv_status_buf), "Quest vault status: %s",
            level_gen_debug_active_quest_vault_name);
    }
    else if (level_gen_screen.stage != LEVEL_GEN_STAGE_DONE
        && level_gen_screen.last_quest_vault_failure[0])
    {
        strnfmt(qv_status_buf, sizeof(qv_status_buf), "Quest vault status: %s",
            level_gen_screen.last_quest_vault_failure);
    }
    else
    {
        qv_status_buf[0] = '\0';
    }

    level_gen_screen_build_partition_summary(partition_buf,
        sizeof(partition_buf), type_buf, sizeof(type_buf));
    level_gen_screen_build_generated_summary(quest_buf, sizeof(quest_buf),
        roulette_buf, sizeof(roulette_buf), giver_buf, sizeof(giver_buf),
        gv_buf, sizeof(gv_buf));

    if (split)
    {
        int left_col = 1;
        int gap = 2;
        int left_w = MAX(32, MIN(42, wid / 3));
        int right_col;
        int right_w;
        int left_row = 3;
        int right_row = 3;
        int issue_lines;
        int recent_lines;

        if (left_w > wid - 26)
            left_w = wid - 26;
        right_col = left_col + left_w + gap;
        right_w = MAX(10, wid - right_col - 1);

        level_gen_screen_put_fitted(left_col, 2, left_w, TERM_L_BLUE, "Summary:");
        level_gen_screen_put_fitted(right_col, 2, right_w, TERM_L_BLUE,
            "Recent generation events:");

        strnfmt(buf, sizeof(buf), "Stage: %s",
            (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE)
                ? "Complete"
                : level_gen_stage_debug_labels[level_gen_screen.stage]);
        level_gen_screen_put_fitted(left_col, left_row++, left_w, TERM_YELLOW, buf);

        strnfmt(buf, sizeof(buf), "Current: %s",
            level_gen_screen.detail_text[0] ? level_gen_screen.detail_text
                                            : "(waiting)");
        left_row += level_gen_screen_print_wrapped(
            left_row, left_col, left_w, 2, TERM_SLATE, buf);

        strnfmt(buf, sizeof(buf), "Last retry: %s",
            level_gen_screen.last_failure[0] ? level_gen_screen.last_failure
                                             : "(none)");
        left_row += level_gen_screen_print_wrapped(
            left_row, left_col, left_w, 3, TERM_ORANGE, buf);

        if (left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_SLATE, partition_buf);
        }
        if (type_buf[0] && left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_SLATE, type_buf);
        }
        if (left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_YELLOW, quest_buf);
        }
        if (left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_YELLOW, roulette_buf);
        }
        if (left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_YELLOW, giver_buf);
        }
        if (left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_L_GREEN, gv_buf);
        }
        if (qv_status_buf[0] && left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_ORANGE, qv_status_buf);
        }

        if (left_row < footer_row)
            level_gen_screen_put_fitted(left_col, left_row++, left_w, TERM_L_BLUE,
                "Main issues:");

        issue_lines = MAX(1, MIN(6,
            level_gen_screen.issue_count ? level_gen_screen.issue_count : 1));
        for (int shown = 0; shown < issue_lines && left_row < footer_row; shown++)
        {
            int best = -1;

            for (int i = 0; i < level_gen_screen.issue_count; i++)
            {
                if (level_gen_screen.issues[i].count <= 0)
                    continue;
                if (best < 0
                    || level_gen_screen.issues[i].count
                        > level_gen_screen.issues[best].count)
                {
                    best = i;
                }
            }

            if (best < 0)
            {
                level_gen_screen_put_fitted(left_col, left_row++, left_w, TERM_SLATE,
                    "(no retries yet)");
                break;
            }

            strnfmt(buf, sizeof(buf), "%dx %s",
                level_gen_screen.issues[best].count,
                level_gen_screen.issues[best].key);
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, 2, TERM_SLATE, buf);
            level_gen_screen.issues[best].count *= -1;
        }

        recent_lines = MAX(1, footer_row - right_row);
        level_gen_screen_draw_recent_events(right_row, right_col, right_w,
            recent_lines);
    }
    else
    {
        int row = 3;
        int issue_lines;
        int recent_lines;
        int remaining;

        strnfmt(buf, sizeof(buf), "Stage: %s",
            (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE)
                ? "Complete"
                : level_gen_stage_debug_labels[level_gen_screen.stage]);
        level_gen_screen_put_fitted(1, row++, width, TERM_YELLOW, buf);

        strnfmt(buf, sizeof(buf), "Current: %s",
            level_gen_screen.detail_text[0] ? level_gen_screen.detail_text
                                            : "(waiting)");
        row += level_gen_screen_print_wrapped(row, 1, width, 2, TERM_SLATE, buf);

        strnfmt(buf, sizeof(buf), "Last retry: %s",
            level_gen_screen.last_failure[0] ? level_gen_screen.last_failure
                                             : "(none)");
        row += level_gen_screen_print_wrapped(row, 1, width, 2, TERM_ORANGE, buf);

        if (row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_SLATE, partition_buf);
        }
        if (type_buf[0] && row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_SLATE, type_buf);
        }
        if (row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_YELLOW, quest_buf);
        }
        if (row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_YELLOW, roulette_buf);
        }
        if (row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_YELLOW, giver_buf);
        }
        if (row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_L_GREEN, gv_buf);
        }
        if (qv_status_buf[0] && row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_ORANGE, qv_status_buf);
        }

        remaining = footer_row - row;
        issue_lines = MIN(4, MAX(2, remaining / 3));
        if (issue_lines > remaining - 2)
            issue_lines = MAX(1, remaining - 2);
        recent_lines = MAX(1, remaining - issue_lines - 1);

        if (row < footer_row)
            level_gen_screen_put_fitted(1, row++, width, TERM_L_BLUE, "Main issues:");

        for (int shown = 0; shown < issue_lines && row < footer_row; shown++)
        {
            int best = -1;

            for (int i = 0; i < level_gen_screen.issue_count; i++)
            {
                if (level_gen_screen.issues[i].count <= 0)
                    continue;
                if (best < 0
                    || level_gen_screen.issues[i].count
                        > level_gen_screen.issues[best].count)
                {
                    best = i;
                }
            }

            if (best < 0)
            {
                level_gen_screen_put_fitted(1, row++, width, TERM_SLATE,
                    "(no retries yet)");
                break;
            }

            strnfmt(buf, sizeof(buf), "%dx %s",
                level_gen_screen.issues[best].count,
                level_gen_screen.issues[best].key);
            level_gen_screen_put_fitted(1, row++, width, TERM_SLATE, buf);
            level_gen_screen.issues[best].count *= -1;
        }

        if (row < footer_row)
            level_gen_screen_put_fitted(1, row++, width, TERM_L_BLUE,
                "Recent generation events:");

        level_gen_screen_draw_recent_events(row, 1, width, recent_lines);
    }

    for (int i = 0; i < level_gen_screen.issue_count; i++)
        level_gen_screen.issues[i].count = ABS(level_gen_screen.issues[i].count);

    if (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE)
    {
        level_gen_screen_put_centered(
            footer_row, TERM_L_WHITE, "Press any key to continue.");
    }
    else
    {
        level_gen_screen_put_fitted(1, footer_row, width, TERM_SLATE,
            "Watching generation live...");
    }
}

static void level_gen_screen_draw_now(void)
{
    int wid = 80;
    int hgt = 24;

    if (!level_gen_screen.active || !Term)
        return;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    Term_clear();

    if (level_gen_screen.debug)
        level_gen_screen_draw_debug(wid, hgt);
    else
        level_gen_screen_draw_user(wid, hgt);

    Term_fresh();
    Term_xtra(TERM_XTRA_BORED, 0);
}

static void level_gen_screen_maybe_draw(bool force)
{
    Uint64 now;
    Uint64 min_interval;

    if (!level_gen_screen.active || !Term)
        return;

    now = SDL_GetTicks();
    min_interval = level_gen_screen.debug ? 75 : 125;

    if (!force && (now - level_gen_screen.last_draw_ticks < min_interval))
        return;

    level_gen_screen.last_draw_ticks = now;
    level_gen_screen_draw_now();
}

static void level_gen_screen_observer(cptr category, cptr message)
{
    char line[192];

    if (!level_gen_screen.active)
        return;

    if (message
        && (!strcmp(category ? category : "", "FAIL")
            || strstr(message, "FAILED")
            || strstr(message, "forcing regeneration")))
    {
        SDL_strlcpy(level_gen_screen.last_failure, message,
            sizeof(level_gen_screen.last_failure));
    }

    if (message && strstr(message, "QUEST VAULT FAILED"))
    {
        SDL_strlcpy(level_gen_screen.last_quest_vault_failure, message,
            sizeof(level_gen_screen.last_quest_vault_failure));
    }

    if (level_gen_screen.debug && level_gen_screen_capture_category(category))
    {
        strnfmt(line, sizeof(line), "%s: %s", category ? category : "GEN",
            message ? message : "");
        level_gen_screen_append_debug_line(line);
    }

    level_gen_screen_maybe_draw(false);
}

static void level_gen_screen_begin(void)
{
    memset(&level_gen_screen, 0, sizeof(level_gen_screen));

    if (!Term)
        return;

    level_gen_screen.active = true;
    level_gen_screen.debug = (op_ptr && show_level_generation_debug);
    level_gen_screen.stage = LEVEL_GEN_STAGE_PLANNING;
    level_gen_screen_format_depth_label(level_gen_screen.depth_label,
        sizeof(level_gen_screen.depth_label));
    SDL_strlcpy(level_gen_screen.status_text, "Preparing the level.",
        sizeof(level_gen_screen.status_text));

    screen_save();
    level_gen_screen.screen_saved = true;
    gen_log_set_observer(level_gen_screen_observer);
    level_gen_screen_maybe_draw(true);
}

static void level_gen_screen_start_attempt(void)
{
    if (!level_gen_screen.active)
        return;

    level_gen_screen.attempt++;
    level_gen_debug_reset_context();
    level_gen_screen.stage = LEVEL_GEN_STAGE_PLANNING;
    level_gen_screen.detail_text[0] = '\0';
    level_gen_screen.final_text[0] = '\0';
    SDL_strlcpy(level_gen_screen.status_text, level_gen_stage_status[LEVEL_GEN_STAGE_PLANNING],
        sizeof(level_gen_screen.status_text));
    level_gen_screen_maybe_draw(true);
}

static void level_gen_screen_set_stage(level_gen_screen_stage_t stage,
    cptr detail)
{
    if (!level_gen_screen.active)
        return;

    level_gen_screen.stage = stage;
    SDL_strlcpy(level_gen_screen.status_text, level_gen_stage_status[stage],
        sizeof(level_gen_screen.status_text));
    if (detail)
        SDL_strlcpy(level_gen_screen.detail_text, detail,
            sizeof(level_gen_screen.detail_text));
    else
        level_gen_screen.detail_text[0] = '\0';

    level_gen_screen_maybe_draw(true);
}

static void level_gen_screen_note_failure(cptr reason)
{
    cptr active_reason = reason;
    char decorated_reason[256];

    if (!level_gen_screen.active)
        return;

    if (!active_reason || !active_reason[0])
        active_reason = level_gen_screen.last_failure;
    if (!active_reason || !active_reason[0])
        active_reason = "Generation failed.";

    level_gen_debug_build_failure_reason(
        decorated_reason, sizeof(decorated_reason), active_reason);
    SDL_strlcpy(level_gen_screen.last_failure, decorated_reason,
        sizeof(level_gen_screen.last_failure));
    level_gen_screen.total_failures++;
    level_gen_screen_record_issue(decorated_reason);

    SDL_strlcpy(level_gen_screen.status_text, "Trying another arrangement.",
        sizeof(level_gen_screen.status_text));
    if (level_gen_screen.debug)
    {
        SDL_strlcpy(level_gen_screen.detail_text, decorated_reason,
            sizeof(level_gen_screen.detail_text));
    }

    level_gen_screen_maybe_draw(true);
}

static void level_gen_screen_finish(bool success)
{
    if (!level_gen_screen.active)
        return;

    if (success)
    {
        level_gen_screen.stage = LEVEL_GEN_STAGE_DONE;
        SDL_strlcpy(level_gen_screen.final_text, "The level is ready.",
            sizeof(level_gen_screen.final_text));
        level_gen_screen_maybe_draw(true);
    }

    gen_log_set_observer(NULL);

    if (success && level_gen_screen.debug)
    {
        flush();
        hide_cursor = true;
        (void)inkey();
        hide_cursor = false;
    }

    if (level_gen_screen.screen_saved)
        screen_load();

    memset(&level_gen_screen, 0, sizeof(level_gen_screen));
}

static int cached_quest_vault_roll = -1;
static bool cached_gv_level_roll_resolved = false;
static bool cached_gv_level_roll_allowed = false;
static int cached_gv_level_roll_candidates = 0;

static void reset_generation_retry_locks(void)
{
    reset_quest_lottery_state();
    cached_quest_vault_roll = -1;
    cached_gv_level_roll_resolved = false;
    cached_gv_level_roll_allowed = false;
    cached_gv_level_roll_candidates = 0;
}

/*
 * Note that Level generation is *not* an important bottleneck,
 * though it can be annoyingly slow on older machines...  Thus
 * we emphasize "simplicity" and "correctness" over "speed".
 *
 * This entire file is only needed for generating levels.
 * This may allow smart compilers to only load it when needed.
 *
 * Consider the "vault.txt" file for vault generation.
 *
 * In this file, we use the "special" granite and perma-wall sub-types,
 * where "basic" is normal, "inner" is inside a room, "outer" is the
 * outer wall of a room, and "solid" is the outer wall of the dungeon
 * or any walls that may not be pierced by corridors.
 *
 * Note that the cave grid flags changed in a rather drastic manner
 * for Angband 2.8.0 (and 2.7.9+), in particular, dungeon terrain
 * features, such as doors and stairs and traps and rubble and walls,
 * are all handled as a set of 64 possible "terrain features", and
 * not as "fake" objects (440-479) as in pre-2.8.0 versions.
 *
 * The 64 new "dungeon features" will also be used for "visual display"
 * but we must be careful not to allow, for example, the user to display
 * hidden traps in a different way from floors, or secret doors in a way
 * different from granite walls, or even permanent granite in a different
 * way from granite.  XXX XXX XXX
 *
 * Sil notes:
 *
 * I do not make any use of "solid" walls, but have left the type in.
 * The code previously used a lot of 11x11 blocks in room generation.
 * I have mostly removed references to this now.
 * The rooms are now placed at random in the dungeon.
 * The corridor generation has been simplified a lot for aesthetic purposes.
 * Note that level generation can fail (if the level is unconnected, or for
 * other reasons) and that each room and corridor generation can fail too. This
 * is not a problem as they are generated until success and often succeed.
 */

/*
 * Dungeon generation values
 */

#define DUN_DEST 1 /* 1/chance of having a destroyed level */

/*
 * Dungeon streamer generation values
 */
#define DUN_STR_DEN 5 /* Density of streamers */
#define DUN_STR_RNG 2 /* Width of streamers */
#define DUN_STR_QUA 4 /* Number of quartz streamers */

/*
 * Dungeon treausre allocation values
 */
#define DUN_OBJ_CHANCE_ROOM 30 /* determines number of items found in rooms */
#define DUN_OBJ_CHANCE_BOTH                                                    \
    5 /* determines number of items found in rooms/corridors */

/*
 * Hack -- Dungeon allocation "places"
 */
#define ALLOC_SET_CORR 1 /* Hallway */
#define ALLOC_SET_ROOM 2 /* Room */
#define ALLOC_SET_BOTH 3 /* Anywhere */

/*
 * Hack -- Dungeon allocation "types"
 */
#define ALLOC_TYP_RUBBLE 1 /* Rubble */
#define ALLOC_TYP_OBJECT 5 /* Object */

/*
 * Maximum numbers of rooms along each axis (currently 6x18)
 */

#define MAX_ROOMS_ROW (MAX_DUNGEON_HGT / BLOCK_HGT)
#define MAX_ROOMS_COL (MAX_DUNGEON_WID / BLOCK_WID)

/*
 * Bounds on some arrays used in the "dun_data" structure.
 * These bounds are checked, though usually this is a formality.
 */
#define DOOR_MAX 200
#define WALL_MAX 500
#define TUNN_MAX 900

bool allow_uniques;

/*
 * Maximal number of room types
 */
#define ROOM_MAX 12

#define LABYRINTH_START_DEPTH 7
#define BIG_CAVE_START_DEPTH 10
#define CHASM_START_DEPTH 14
#define SPECIAL_CAP_STEP 5
#define SPECIAL_CAP_MAX 3
/* Special-mode depth gates and caps (tweak to rebalance rarity) */

void reset_partition_population_metadata(void);
partition_population_meta current_partition_population_meta[25];

void init_partition_chest_recipe(partition_chest_recipe* recipe)
{
    if (!recipe)
        return;

    recipe->chest_mode = 0;
    recipe->material_wood_pct = -1;
    recipe->material_steel_pct = -1;
    recipe->material_jewel_pct = -1;
    recipe->anchor_pref = PARTITION_CHEST_ANCHOR_ANY;
}

static void set_partition_chest_recipe(partition_population_meta* meta, int slot,
    int chest_mode, int wooden_pct, int steel_pct, int jewel_pct,
    partition_chest_anchor_pref anchor_pref)
{
    if (!meta || slot < 0 || slot >= PARTITION_CHEST_RECIPE_MAX)
        return;

    meta->chest_recipes[slot].chest_mode = (byte)chest_mode;
    meta->chest_recipes[slot].material_wood_pct = (s16b)wooden_pct;
    meta->chest_recipes[slot].material_steel_pct = (s16b)steel_pct;
    meta->chest_recipes[slot].material_jewel_pct = (s16b)jewel_pct;
    meta->chest_recipes[slot].anchor_pref = (byte)anchor_pref;
    if (meta->chest_count < slot + 1)
        meta->chest_count = slot + 1;
}

static const char* level_gen_partition_mode_name(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        return "Roomy";
    case QUAD_MODE_CAVEY:
        return "Cavey";
    case QUAD_MODE_RUINED:
        return "Ruined";
    case QUAD_MODE_LABYRINTH:
        return "Labyrinth";
    case QUAD_MODE_CHASM:
        return "Chasm";
    case QUAD_MODE_BIG_CAVE:
        return "Big cave";
    }

    return "Unknown";
}

static void level_gen_screen_append_list_item(char* buf, size_t buflen,
    cptr text)
{
    char tmp[256];

    if (!buf || buflen == 0 || !text || !text[0])
        return;

    if (buf[0])
        strnfmt(tmp, sizeof(tmp), "%s, %s", buf, text);
    else
        strnfmt(tmp, sizeof(tmp), "%s", text);

    SDL_strlcpy(buf, tmp, buflen);
}

static void level_gen_screen_build_partition_summary(char* total_buf,
    size_t total_buflen, char* types_buf, size_t types_buflen)
{
    int mode_counts[QUAD_MODE_BIG_CAVE + 1] = {0};
    int total = MIN(current_partition_count, 25);
    int distinct = 0;

    if (total_buflen > 0)
        total_buf[0] = '\0';
    if (types_buflen > 0)
        types_buf[0] = '\0';

    if (total <= 0)
    {
        SDL_strlcpy(total_buf, "Partitions: (pending)", total_buflen);
        return;
    }

    for (int i = 0; i < total; ++i)
    {
        int mode = current_partition_modes[i];

        if (mode < QUAD_MODE_ROOMY || mode > QUAD_MODE_BIG_CAVE)
            mode = QUAD_MODE_ROOMY;

        if (mode_counts[mode]++ == 0)
            distinct++;
    }

    strnfmt(total_buf, total_buflen, "Partitions: %d total, %d type%s",
        total, distinct, (distinct == 1) ? "" : "s");

    for (int mode = QUAD_MODE_ROOMY; mode <= QUAD_MODE_BIG_CAVE; ++mode)
    {
        char item[64];

        if (mode_counts[mode] <= 0)
            continue;

        strnfmt(item, sizeof(item), "%d %s", mode_counts[mode],
            level_gen_partition_mode_name((quadrant_mode_t)mode));
        level_gen_screen_append_list_item(types_buf, types_buflen, item);
    }

    if (types_buf[0])
    {
        char mix[256];

        strnfmt(mix, sizeof(mix), "Type mix: %s", types_buf);
        SDL_strlcpy(types_buf, mix, types_buflen);
    }
}

static bool area_is_basic_granite(int y1, int x1, int y2, int x2)
{
    if (x2 >= MAX_DUNGEON_WID || y2 >= MAX_DUNGEON_HGT || y1 < 0 || x1 < 0)
        return false;

    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (cave_feat[y][x] != FEAT_WALL_EXTRA)
                return false;
        }
    }
    return true;
}

void remember_partition_grid(int rows, int cols, int count)
{
    current_partition_rows = rows;
    current_partition_cols = cols;
    current_partition_count = count;
    reset_partition_population_metadata();
    for (int i = 0; i < 25; ++i)
    {
        current_partition_modes[i] = QUAD_MODE_ROOMY;
        current_partition_densities[i] = DENSITY_NORMAL;
        current_partition_big_cave_types[i] = BIG_CAVE_NONE;
        current_partition_bridge_styles[i] = -1;
    }
}

/* Partition helper: compute bounds for a given partition index */
static void apply_quadrant_generation_modes(void);
static void repair_all_outer_walls(void);
static bool carve_chasm_with_bridges(int y_min, int y_max, int x_min, int x_max,
    int floor_style, int bridge_style);
static int room_connection_degree(int room_idx);
static bool connect_rooms_with_logging(int r1, int r2, const char *tag, bool allow_desperate);
static bool is_big_partition_mode(quadrant_mode_t mode);
bool generation_escape_tunnel_bold(int y, int x);

/* Disabled helpers kept for reference (see #if 0 blocks near usage sites). */
#if 0
static void seed_ca_blob_anchors(void);
static void seed_bsp_slice_anchors(void);
static void ensure_partition_connectivity(void);
#endif

/* Carve a small cellular-automata style blob and register it as an anchor */
#if 0
static bool carve_ca_blob_anchor(void)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;

    /* Pick blob dimensions (moderate footprint to avoid over-densifying) */
    int h = rand_range(8, 12);
    int w = rand_range(10, 16);
    int y1 = rand_range(3, p_ptr->cur_map_hgt - h - 3);
    int x1 = rand_range(3, p_ptr->cur_map_wid - w - 3);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    /* Ensure we are carving into untouched granite */
    /* Allow slight overlap with walls but not existing floors */
    if (y1 < 1 || x1 < 1 || y2 >= p_ptr->cur_map_hgt - 1 || x2 >= p_ptr->cur_map_wid - 1)
        return false;
    for (int y = y1 - 1; y <= y2 + 1; ++y)
    {
        for (int x = x1 - 1; x <= x2 + 1; ++x)
        {
            if (cave_floor_bold(y, x))
                return false;
        }
    }

    /* Simple CA grid stored on stack (max ~20x20) */
    bool grid[24][24];
    if (h > 24 || w > 24)
        return false;

    /* Seed noise with a bias to produce irregular shapes */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            grid[y][x] = (rand_int(100) < 45); /* 45% initial fill */

    /* Run several smoothing steps to create rounded blobs */
    int steps = 3;
    for (int step = 0; step < steps; ++step)
    {
        bool next[24][24];
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0)
                            continue;
                        int ny = y + dy;
                        int nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors++;
                        else if (grid[ny][nx])
                            neighbors++;
                    }
                }
                /* Slightly denser survival/birth to keep blobs cohesive */
                if (grid[y][x])
                    next[y][x] = (neighbors >= 4);
                else
                    next[y][x] = (neighbors >= 5);
            }
        }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                grid[y][x] = next[y][x];
    }

    /* Apply to dungeon */
    int floor_count = 0;
    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    /* Clear box to raw granite to avoid rectangular outlines */
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            if (in_bounds_fully(gy, gx))
                cave_set_feat(gy, gx, FEAT_WALL_EXTRA);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (!grid[y][x])
                continue;
            int gy = y1 + y;
            int gx = x1 + x;
            cave_set_feat(gy, gx, FEAT_FLOOR);
            cave_info[gy][gx] |= CAVE_ROOM;
            floor_count++;
            if (gy < min_y)
                min_y = gy;
            if (gy > max_y)
                max_y = gy;
            if (gx < min_x)
                min_x = gx;
            if (gx > max_x)
                max_x = gx;
        }
    }

    /* Ragged edge expansion to break rectangular silhouette */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        {
            for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            {
                if (cave_floor_bold(gy, gx))
                    continue;
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dy || dx)
                        {
                            int ny = gy + dy, nx = gx + dx;
                            if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx))
                                adj++;
                        }
                if (adj >= 3 && one_in_(2 + pass))
                {
                    cave_set_feat(gy, gx, FEAT_FLOOR);
                    cave_info[gy][gx] |= CAVE_ROOM;
                    floor_count++;
                    if (gy < min_y)
                        min_y = gy;
                    if (gy > max_y)
                        max_y = gy;
                    if (gx < min_x)
                        min_x = gx;
                    if (gx > max_x)
                        max_x = gx;
                }
            }
        }
    }

    /* Bleed outward a little to break boxy outlines */
    const int bleed_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
    {
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
        {
            if (!cave_floor_bold(gy, gx))
                continue;
            bool on_edge = (gy == y1 - 1) || (gy == y2 + 1) || (gx == x1 - 1) || (gx == x2 + 1);
            if (!on_edge)
                continue;
            for (int d = 0; d < 4; ++d)
            {
                int ny = gy + bleed_dirs[d][0];
                int nx = gx + bleed_dirs[d][1];
                if (!in_bounds_fully(ny, nx))
                    continue;
                if (cave_floor_bold(ny, nx))
                    continue;
                if (cave_feat[ny][nx] != FEAT_WALL_EXTRA)
                    continue;
                if (one_in_(4))
                {
                    cave_set_feat(ny, nx, FEAT_FLOOR);
                    cave_info[ny][nx] |= CAVE_ROOM;
                    floor_count++;
                    if (ny < min_y)
                        min_y = ny;
                    if (ny > max_y)
                        max_y = ny;
                    if (nx < min_x)
                        min_x = nx;
                    if (nx > max_x)
                        max_x = nx;
                }
            }
        }
    }

    if (floor_count < 8)
        return false;

    /* Set outer walls around floor tiles so tunnels can connect */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            /* Check if this wall borders any floor */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat(gy, gx, FEAT_WALL_OUTER);
            }
        }
    }

    /* Pick a center on a floor tile */
    int cy = min_y, cx = min_x;
    for (int tries = 0; tries < 200; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (cave_floor_bold(ty, tx))
        {
            cy = ty;
            cx = tx;
            break;
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, one_in_(3));

    /* Scatter quartz veins around the cave walls for natural appearance */
    scatter_quartz_veins_in_bounds(min_y, max_y, min_x, max_x, 0);

    log_trace("CA blob anchor: carved floor_count=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d)",
        floor_count, min_y, min_x, max_y, max_x, cy, cx);
    genlog_anchor("CA_BLOB: carved %d floor tiles at (%d,%d)-(%d,%d), center=(%d,%d)",
        floor_count, min_y, min_x, max_y, max_x, cy, cx);
    return true;
}
#endif

/* Carve a chasm area with organic cave shape and islands connected by bridges */
static bool carve_chasm_with_bridges(int y_min, int y_max, int x_min, int x_max,
    int floor_style, int bridge_style)
{
    if (dun->cent_n >= room_capacity_limit())
    {
        genlog_anchor("CHASM: rejected - room capacity limit reached");
        return false;
    }
    
    /* Bounds are inclusive. Keep the local mask dimensions aligned with the
     * generation loops so the temporary cave/platform arrays cover every tile. */
    int avail_h = y_max - y_min + 1;
    int avail_w = x_max - x_min + 1;
    if (avail_h < 16 || avail_w < 20)
    {
        genlog_anchor("CHASM: rejected - bounds too small (%d,%d)-(%d,%d), avail=%dx%d",
                      y_min, x_min, y_max, x_max, avail_h, avail_w);
        return false;
    }
    
    /* Use variable margins to create organic outer boundary */
    int h = avail_h;
    int w = avail_w;
    int y1 = y_min;
    int x1 = x_min;
    int y2 = y_max;
    int x2 = x_max;
    
    /* Check area is basic granite */
    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (in_bounds_fully(y, x) && cave_floor_bold(y, x))
            {
                genlog_anchor("CHASM: rejected - floor already exists at (%d,%d) in bounds (%d,%d)-(%d,%d)",
                              y, x, y1, x1, y2, x2);
                return false;
            }
        }
    }
    
    /* 
     * CHASM GENERATION APPROACH:
     * 1. Use CA to create organic cave boundary (not rectangular)
     * 2. Create multiple platform islands within the cave
     * 3. Fill non-platform areas with chasms
     * 4. Connect platforms with narrow bridges
     */
    
    /* Track what's inside the cave vs wall, and what's platform vs chasm */
    bool* is_cave = mem_alloc_array(h * w, bool);
    bool* is_platform = mem_alloc_array(h * w, bool);
    if (!is_cave || !is_platform) 
    {
        if (is_cave) mem_free(is_cave);
        if (is_platform) mem_free(is_platform);
        return false;
    }
    
    /* Initialize: seed cave shape with multi-center distance + noise */
    int num_cave_centers = 3 + rand_int(3);  /* 3-5 centers for cave shape */
    int cave_cy[6], cave_cx[6];
    for (int c = 0; c < num_cave_centers; ++c)
    {
        cave_cy[c] = rand_range(h / 4, 3 * h / 4);
        cave_cx[c] = rand_range(w / 4, 3 * w / 4);
    }
    
    /* Carve organic cave shape using distance from centers + noise */
    int base_radius = (h + w) / 5;
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            /* Find distance to nearest center */
            int min_dist = 9999;
            for (int c = 0; c < num_cave_centers; ++c)
            {
                int dy = ABS(ly - cave_cy[c]);
                int dx = ABS(lx - cave_cx[c]);
                int dist = dy + (dx * 2 / 3);  /* Wider horizontally */
                if (dist < min_dist) min_dist = dist;
            }
            
            /* Cave extends with noise for organic edges */
            int threshold = base_radius + rand_int(base_radius / 2) - rand_int(base_radius / 3);
            is_cave[ly * w + lx] = (min_dist < threshold);
            is_platform[ly * w + lx] = false;
        }
    }
    
    /* CA smoothing for organic cave boundary */
    bool* next_cave = mem_alloc_array(h * w, bool);
    if (!next_cave) { mem_free(is_cave); mem_free(is_platform); return false; }
    
    for (int step = 0; step < 3; ++step)
    {
        for (int ly = 0; ly < h; ++ly)
        {
            for (int lx = 0; lx < w; ++lx)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0) continue;
                        int ny = ly + dy, nx = lx + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors += 0;  /* Edges are wall */
                        else if (is_cave[ny * w + nx])
                            neighbors++;
                    }
                }
                /* Cave survives with 4+ neighbors, born with 5+ */
                next_cave[ly * w + lx] = is_cave[ly * w + lx] ? (neighbors >= 4) : (neighbors >= 5);
            }
        }
        for (int i = 0; i < h * w; ++i) is_cave[i] = next_cave[i];
    }
    mem_free(next_cave);
    
    /* Ensure cave doesn't touch absolute edges */
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (ly < 2 || ly >= h - 2 || lx < 2 || lx >= w - 2)
                is_cave[ly * w + lx] = false;
        }
    }
    
    /* Now create 5-9 platform islands within the cave area */
    int num_platforms = rand_range(5, 9);
    int plat_cy[10], plat_cx[10], plat_radius[10];
    int platforms_placed = 0;
    int sanctum_cy = -1;
    int sanctum_cx = -1;

    if (!choose_chasm_sanctum_seed(is_cave, h, w, &sanctum_cy, &sanctum_cx))
    {
        mem_free(is_cave);
        mem_free(is_platform);
        genlog_anchor("CHASM: rejected - no buffered central sanctum site");
        return false;
    }

    plat_cy[platforms_placed] = sanctum_cy;
    plat_cx[platforms_placed] = sanctum_cx;
    plat_radius[platforms_placed] = rand_range(3, 4);
    platforms_placed++;
    
    for (int attempt = 0; attempt < 300 && platforms_placed < num_platforms; ++attempt)
    {
        int py = rand_range(4, h - 5);
        int px = rand_range(5, w - 6);
        
        /* Must be inside cave */
        if (!is_cave[py * w + px]) continue;
        
        /* Check distance from other platforms */
        bool too_close = false;
        int min_sep = 5 + rand_int(3);  /* Variable separation */
        for (int i = 0; i < platforms_placed; ++i)
        {
            int dist = ABS(py - plat_cy[i]) + ABS(px - plat_cx[i]);
            if (dist < min_sep)
            {
                too_close = true;
                break;
            }
        }
        if (too_close) continue;
        
        plat_cy[platforms_placed] = py;
        plat_cx[platforms_placed] = px;
        plat_radius[platforms_placed] = rand_range(2, 4);
        platforms_placed++;
    }
    
    /* Create organic platform shapes */
    for (int p = 0; p < platforms_placed; ++p)
    {
        int cy = plat_cy[p];
        int cx = plat_cx[p];
        int base_r = plat_radius[p];
        
        for (int ly = 0; ly < h; ++ly)
        {
            for (int lx = 0; lx < w; ++lx)
            {
                if (!is_cave[ly * w + lx]) continue;
                
                int dy = ABS(ly - cy);
                int dx = ABS(lx - cx);
                int dist = dy + (dx * 2 / 3);
                
                int threshold = base_r + rand_int(2);
                if (dist <= threshold)
                    is_platform[ly * w + lx] = true;
            }
        }
    }

    /* Reserve one buffered 5x5 sanctuary on the center-leaning island so the
     * 3x3 sanctum sits away from chasm edges. */
    for (int dy = -2; dy <= 2; ++dy)
    {
        for (int dx = -2; dx <= 2; ++dx)
        {
            int ly = sanctum_cy + dy;
            int lx = sanctum_cx + dx;

            if (ly < 0 || lx < 0 || ly >= h || lx >= w)
                continue;
            if (!is_cave[ly * w + lx])
                continue;

            is_platform[ly * w + lx] = true;
        }
    }
    
    /* Extend platforms organically */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int ly = 1; ly < h - 1; ++ly)
        {
            for (int lx = 1; lx < w - 1; ++lx)
            {
                if (!is_cave[ly * w + lx]) continue;
                if (is_platform[ly * w + lx]) continue;
                
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if ((dy || dx) && is_platform[(ly+dy) * w + (lx+dx)])
                            adj++;
                
                if (adj >= 3 && one_in_(3))
                    is_platform[ly * w + lx] = true;
            }
        }
    }

    /* Restore the sparse edge nubs that helped the previous bridge layout stay
     * legible without creating a trivial perimeter walkway. */
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (!is_cave[ly * w + lx]) continue;

            bool edge_of_cave = false;
            int adj_platforms = 0;
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = ly + dy, nx = lx + dx;
                    if (ny < 0 || nx < 0 || ny >= h || nx >= w
                        || !is_cave[ny * w + nx])
                        edge_of_cave = true;
                    else if (is_platform[ny * w + nx])
                        adj_platforms++;
                }
            }

            if (edge_of_cave && !is_platform[ly * w + lx]
                && adj_platforms >= 2 && one_in_(4))
            {
                is_platform[ly * w + lx] = true;
            }
        }
    }

    /* Apply to cave: inside cave + platform = floor, inside cave + !platform = chasm */
    int floor_count = 0;
    int chasm_count = 0;
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            int ly = gy - y1, lx = gx - x1;
            
            if (!is_cave[ly * w + lx])
                continue;  /* Leave as granite wall */
            
            if (is_platform[ly * w + lx])
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, floor_style);
                cave_info[gy][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                floor_count++;
            }
            else
            {
                cave_set_feat(gy, gx, FEAT_CHASM);
                cave_info[gy][gx] |= CAVE_CHASM_AREA;
                chasm_count++;
            }
        }
    }
    
    /* Now connect platforms with bridges (MST-style) */
    int global_plat_y[10], global_plat_x[10];
    for (int p = 0; p < platforms_placed; ++p)
    {
        global_plat_y[p] = y1 + plat_cy[p];
        global_plat_x[p] = x1 + plat_cx[p];
    }
    
    bool* connected = mem_alloc_array(platforms_placed, bool);
    if (!connected) { mem_free(is_cave); mem_free(is_platform); return false; }
    for (int i = 0; i < platforms_placed; ++i) connected[i] = false;
    if (platforms_placed > 0) connected[0] = true;
    
    int bridges_built = 0;
    for (int iter = 0; iter < platforms_placed; ++iter)
    {
        int best_from = -1, best_to = -1, best_dist = 9999;
        
        for (int i = 0; i < platforms_placed; ++i)
        {
            if (!connected[i]) continue;
            for (int j = 0; j < platforms_placed; ++j)
            {
                if (connected[j]) continue;
                int dist = distance(global_plat_y[i], global_plat_x[i],
                                   global_plat_y[j], global_plat_x[j]);
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best_from = i;
                    best_to = j;
                }
            }
        }
        
        if (best_to < 0) break;
        
        int sy = global_plat_y[best_from];
        int sx = global_plat_x[best_from];
        int ey = global_plat_y[best_to];
        int ex = global_plat_x[best_to];
        
        /* L-shaped bridge */
        if (one_in_(2))
        {
            int x_lo = MIN(sx, ex), x_hi = MAX(sx, ex);
            for (int gx = x_lo; gx <= x_hi; ++gx)
                if (in_bounds_fully(sy, gx) && cave_feat[sy][gx] == FEAT_CHASM)
                {
                    cave_set_feat_style(sy, gx, FEAT_FLOOR, bridge_style);
                    cave_info[sy][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
            int y_lo = MIN(sy, ey), y_hi = MAX(sy, ey);
            for (int gy = y_lo; gy <= y_hi; ++gy)
                if (in_bounds_fully(gy, ex) && cave_feat[gy][ex] == FEAT_CHASM)
                {
                    cave_set_feat_style(gy, ex, FEAT_FLOOR, bridge_style);
                    cave_info[gy][ex] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
        }
        else
        {
            int y_lo = MIN(sy, ey), y_hi = MAX(sy, ey);
            for (int gy = y_lo; gy <= y_hi; ++gy)
                if (in_bounds_fully(gy, sx) && cave_feat[gy][sx] == FEAT_CHASM)
                {
                    cave_set_feat_style(gy, sx, FEAT_FLOOR, bridge_style);
                    cave_info[gy][sx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
            int x_lo = MIN(sx, ex), x_hi = MAX(sx, ex);
            for (int gx = x_lo; gx <= x_hi; ++gx)
                if (in_bounds_fully(ey, gx) && cave_feat[ey][gx] == FEAT_CHASM)
                {
                    cave_set_feat_style(ey, gx, FEAT_FLOOR, bridge_style);
                    cave_info[ey][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
        }
        
        connected[best_to] = true;
        bridges_built++;
    }

    place_chasm_island_sanctum(y1 + sanctum_cy, x1 + sanctum_cx);
    
    mem_free(connected);
    mem_free(is_cave);
    mem_free(is_platform);
    
    /* Track bounds of just the floor tiles (not chasm) for proper tunnel connectivity */
    int floor_min_y = y2, floor_max_y = y1, floor_min_x = x2, floor_max_x = x1;
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (cave_floor_bold(gy, gx))
            {
                if (gy < floor_min_y) floor_min_y = gy;
                if (gy > floor_max_y) floor_max_y = gy;
                if (gx < floor_min_x) floor_min_x = gx;
                if (gx > floor_max_x) floor_max_x = gx;
            }
        }
    }
    
    /* Set outer walls ONLY around floor tiles (not chasm) for proper tunnel connectivity */
    for (int gy = floor_min_y - 1; gy <= floor_max_y + 1; ++gy)
    {
        for (int gx = floor_min_x - 1; gx <= floor_max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (cave_floor_bold(gy, gx)) continue;
            if (cave_feat[gy][gx] == FEAT_CHASM) continue;  /* Don't convert chasm */
            if (cave_feat[gy][gx] != FEAT_WALL_EXTRA) continue;
            
            /* Only set outer wall if bordering actual floor (not chasm) */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx) &&
                        (cave_info[ny][nx] & CAVE_ROOM))
                        borders_floor = true;
                }
            }
            if (borders_floor)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, floor_style);
        }
    }
    
    /* Find center on a floor tile near an outer wall (better for tunnel connectivity) */
    int cy = (floor_min_y + floor_max_y) / 2;
    int cx = (floor_min_x + floor_max_x) / 2;
    
    /* First try: find floor tile adjacent to outer wall */
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(floor_min_y, floor_max_y);
        int tx = rand_range(floor_min_x, floor_max_x);
        if (!cave_floor_bold(ty, tx)) continue;
        
        /* Check if adjacent to outer wall */
        for (int dy = -1; dy <= 1 && !found_edge; ++dy)
        {
            for (int dx = -1; dx <= 1 && !found_edge; ++dx)
            {
                if (dy == 0 && dx == 0) continue;
                if (in_bounds_fully(ty + dy, tx + dx) && 
                    cave_feat[ty + dy][tx + dx] == FEAT_WALL_OUTER)
                {
                    cy = ty; cx = tx;
                    found_edge = true;
                }
            }
        }
    }
    
    /* Fallback: any floor tile */
    if (!found_edge)
    {
        for (int tries = 0; tries < 100; ++tries)
        {
            int ty = rand_range(floor_min_y, floor_max_y);
            int tx = rand_range(floor_min_x, floor_max_x);
            if (cave_floor_bold(ty, tx))
            {
                cy = ty; cx = tx;
                break;
            }
        }
    }
    
    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    /* Use floor bounds, not full chasm bounds, for tunnel connectivity */
    dun->corner[idx].y1 = floor_min_y;
    dun->corner[idx].x1 = floor_min_x;
    dun->corner[idx].y2 = floor_max_y;
    dun->corner[idx].x2 = floor_max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, false);
    
    log_trace("Chasm organic: %d platforms, %d bridges, %d chasm tiles, floor=(%d,%d)-(%d,%d) center=(%d,%d)",
        platforms_placed, bridges_built, chasm_count, floor_min_y, floor_min_x, floor_max_y, floor_max_x, cy, cx);
    log_trace("Chasm organic extras: sanctum=(%d,%d)",
        y1 + sanctum_cy, x1 + sanctum_cx);
    genlog_anchor("CHASM: %d platforms, %d bridges, %d chasm tiles at (%d,%d)-(%d,%d)",
        platforms_placed, bridges_built, chasm_count, floor_min_y, floor_min_x, floor_max_y, floor_max_x);
    return true;
}

/* Carve a labyrinth-style maze with organic shape using cellular automata */
static bool carve_labyrinth_bounds(int y_min, int y_max, int x_min, int x_max,
    density_level_t density, int style_idx)
{
    if (dun->cent_n >= room_capacity_limit())
    {
        genlog_anchor("LABYRINTH: rejected - room capacity limit reached");
        return false;
    }
    
    int avail_h = y_max - y_min;
    int avail_w = x_max - x_min;
    if (avail_h < 10 || avail_w < 12)
    {
        genlog_anchor("LABYRINTH: rejected - bounds too small (%d,%d)-(%d,%d), avail=%dx%d",
                      y_min, x_min, y_max, x_max, avail_h, avail_w);
        return false;
    }
    
    /* Use small margins to maximize labyrinth size while avoiding partition overlap */
    int margin_y = rand_range(3, 5);
    int margin_x = rand_range(3, 5);
    int y1 = y_min + margin_y;
    int x1 = x_min + margin_x;
    int y2 = y_max - margin_y;
    int x2 = x_max - margin_x;
    int h = y2 - y1 + 1;
    int w = x2 - x1 + 1;
    
    if (h < 8 || w < 10)
    {
        genlog_anchor("LABYRINTH: rejected - after margins too small: h=%d w=%d (margins y=%d x=%d)",
                      h, w, margin_y, margin_x);
        return false;
    }
    
    /* Check area is basic granite - if floor exists, another partition already carved here */
    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (in_bounds_fully(y, x) && cave_floor_bold(y, x))
            {
                genlog_anchor("LABYRINTH: rejected - floor already exists at (%d,%d) in bounds (%d,%d)-(%d,%d)",
                              y, x, y1, x1, y2, x2);
                return false;
            }
        }
    }
    
    /* Use CA to create organic boundary mask - no size caps, use full partition */
    /* Note: h and w already set from margins above, keep them as-is */
    
    bool* mask = mem_alloc_array(h * w, bool);
    if (!mask) return false;
    
    /* Seed with 60% fill for corridors */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            mask[y * w + x] = (rand_int(100) < 60);
    
    /* CA smoothing to create organic boundary */
    bool* next = mem_alloc_array(h * w, bool);
    if (!next) { mem_free(mask); return false; }
    
    for (int step = 0; step < 3; ++step)
    {
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0) continue;
                        int ny = y + dy, nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors++;
                        else if (mask[ny * w + nx])
                            neighbors++;
                    }
                next[y * w + x] = (neighbors >= 4);
            }
        }
        for (int i = 0; i < h * w; ++i) mask[i] = next[i];
    }
    mem_free(next);
    
    /* Carve corridors in a grid pattern, but only within the organic mask */
    int floor_count = 0;
    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    /* Vary corridor spacing by density: sparse=4 (open), normal=3, dense=2 (tight maze) */
    int corridor_spacing = (density == DENSITY_SPARSE) ? 4 : (density == DENSITY_DENSE) ? 2 : 3;
    
    /* Horizontal corridors */
    for (int ly = 1; ly < h - 1; ly += corridor_spacing)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (!mask[ly * w + lx]) continue;
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (in_bounds_fully(gy, gx) && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                cave_info[gy][gx] |= CAVE_ROOM;
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }
    
    /* Vertical corridors */
    for (int lx = 1; lx < w - 1; lx += corridor_spacing)
    {
        for (int ly = 0; ly < h; ++ly)
        {
            if (!mask[ly * w + lx]) continue;
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (in_bounds_fully(gy, gx) && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                cave_info[gy][gx] |= CAVE_ROOM;
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }
    
    mem_free(mask);
    
    /* Block some corridor segments to create dead ends */
    for (int ly = 1; ly < h - 1; ly += corridor_spacing)
    {
        for (int lx = 1; lx < w - 1; lx += corridor_spacing)
        {
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (!in_bounds_fully(gy, gx) || !cave_floor_bold(gy, gx))
                continue;
            
            if (rand_int(100) < 45)
            {
                int block_dir = rand_int(4);
                int dy = (block_dir == 0) ? -1 : (block_dir == 1) ? 1 : 0;
                int dx = (block_dir == 2) ? -1 : (block_dir == 3) ? 1 : 0;
                
                for (int step = 1; step < corridor_spacing; ++step)
                {
                    int ny = gy + dy * step;
                    int nx = gx + dx * step;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx))
                    {
                        cave_set_feat_style(ny, nx, FEAT_WALL_EXTRA, style_idx);
                        cave_info[ny][nx] &= ~CAVE_ROOM;
                        floor_count--;
                    }
                }
            }
        }
    }
    
    /* Add chambers at some intersections */
    int chamber_count = rand_range(2, 5);
    for (int c = 0; c < chamber_count; ++c)
    {
        int cy = rand_range(min_y + 2, max_y - 2);
        int cx = rand_range(min_x + 2, max_x - 2);
        if (!cave_floor_bold(cy, cx)) continue;
        
        int ch_h = rand_range(2, 4);
        int ch_w = rand_range(2, 5);
        
        for (int dy = -ch_h; dy <= ch_h; ++dy)
        {
            for (int dx = -ch_w; dx <= ch_w; ++dx)
            {
                int ty = cy + dy;
                int tx = cx + dx;
                if (!in_bounds_fully(ty, tx)) continue;
                if (cave_feat[ty][tx] != FEAT_WALL_EXTRA) continue;
                
                cave_set_feat_style(ty, tx, FEAT_FLOOR, style_idx);
                cave_info[ty][tx] |= CAVE_ROOM;
                floor_count++;
                if (ty < min_y) min_y = ty;
                if (ty > max_y) max_y = ty;
                if (tx < min_x) min_x = tx;
                if (tx > max_x) max_x = tx;
            }
        }
    }
    
    if (floor_count < 25)
        return false;
    
    /* Set outer walls */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (cave_floor_bold(gy, gx)) continue;
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                        borders_floor = true;
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, style_idx);
        }
    }
    
    /* Pick center - prefer floor tile adjacent to outer wall for tunnel connectivity */
    int center_y = (min_y + max_y) / 2, center_x = (min_x + max_x) / 2;
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (!cave_floor_bold(ty, tx)) continue;
        
        for (int dy = -1; dy <= 1 && !found_edge; ++dy)
        {
            for (int dx = -1; dx <= 1 && !found_edge; ++dx)
            {
                if (dy == 0 && dx == 0) continue;
                if (in_bounds_fully(ty + dy, tx + dx) && 
                    cave_feat[ty + dy][tx + dx] == FEAT_WALL_OUTER)
                {
                    center_y = ty; center_x = tx;
                    found_edge = true;
                }
            }
        }
    }
    /* Fallback: any floor tile */
    if (!found_edge)
    {
        for (int tries = 0; tries < 100; ++tries)
        {
            int ty = rand_range(min_y, max_y);
            int tx = rand_range(min_x, max_x);
            if (cave_floor_bold(ty, tx))
            {
                center_y = ty; center_x = tx; break;
            }
        }
    }
    
    int idx = dun->cent_n++;
    dun->cent[idx].y = center_y;
    dun->cent[idx].x = center_x;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_BSP_SLICE, false);
    
    /* === LABYRINTH STAIR PLACEMENT === */
    /* Place 1-2 stairs inside the labyrinth for navigation */
    int lab_stairs = 1 + (floor_count > 60 ? 1 : 0);
    int stairs_placed = 0;
    
    for (int s = 0; s < lab_stairs; ++s)
    {
        for (int tries = 0; tries < 50; ++tries)
        {
            int sy = rand_range(min_y, max_y);
            int sx = rand_range(min_x, max_x);
            if (!in_bounds_fully(sy, sx)) continue;
            if (!cave_naked_bold(sy, sx)) continue;
            if (!cave_floor_bold(sy, sx)) continue;
            
            /* Avoid placing next to doors */
            if (cave_feat[sy - 1][sx] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy + 1][sx] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy][sx - 1] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy][sx + 1] == FEAT_DOOR_HEAD) continue;
            
            /* Alternate between up and down stairs */
            int feat = (s % 2 == 0) ? FEAT_MORE : FEAT_LESS;
            
            /* At surface, only down; at Morgoth depth, only up */
            if (p_ptr->depth == 0) feat = FEAT_MORE;
            else if (p_ptr->depth >= MORGOTH_DEPTH) feat = FEAT_LESS;
            
            cave_set_feat(sy, sx, feat);
            stairs_placed++;
            break;
        }
    }
    
    log_trace("Labyrinth anchor (organic): bounds=(%d,%d)-(%d,%d) center=(%d,%d) edge=%d floors=%d chambers=%d stairs=%d",
        min_y, min_x, max_y, max_x, center_y, center_x, found_edge, floor_count, chamber_count, stairs_placed);
    genlog_anchor("LABYRINTH: bounds=(%d,%d)-(%d,%d), %d floor tiles, %d chambers, %d stairs",
        min_y, min_x, max_y, max_x, floor_count, chamber_count, stairs_placed);
    return true;
}

#if 0
/* Try to seed a few CA blob anchors in unused granite */
static void seed_ca_blob_anchors(void)
{
    /* Scale CA blobs by map size to add connective floor on big levels */
    int panels = (p_ptr->cur_map_hgt / PANEL_HGT);
    int target = 1 + panels / 3; /* e.g., 9 panels -> 4 blobs */
    if (target > 4) target = 4;
    int placed = 0;
    int max_attempts = target * 8;
    for (int attempt = 0; attempt < max_attempts && placed < target; ++attempt)
    {
        if (carve_ca_blob_anchor())
            placed++;
    }
    log_trace("CA blob seeding complete: placed=%d target=%d attempts=%d", placed, target, max_attempts);
}
#endif

/* Carve a BSP-style sliced region into rooms-like rectangles and register anchor */
#if 0
static bool carve_bsp_slice_anchor(void)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;

    int h = rand_range(10, 18);
    int w = rand_range(12, 24);
    int y1 = rand_range(3, p_ptr->cur_map_hgt - h - 3);
    int x1 = rand_range(3, p_ptr->cur_map_wid - w - 3);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    if (!area_is_basic_granite(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
        return false;

    typedef struct {
        int y1, x1, y2, x2;
    } slice_rect;

    slice_rect rects[12];
    int rect_count = 1;
    rects[0].y1 = y1;
    rects[0].x1 = x1;
    rects[0].y2 = y2;
    rects[0].x2 = x2;

    int splits = rand_range(2, 4);
    for (int s = 0; s < splits && rect_count < 12; ++s)
    {
        int pick = rand_int(rect_count);
        slice_rect r = rects[pick];
        int rw = r.x2 - r.x1 + 1;
        int rh = r.y2 - r.y1 + 1;
        bool vertical = (rw > rh) ? true : (rh > rw ? false : one_in_(2));

        if (vertical && rw > 10)
        {
            int cut = rand_range(r.x1 + rw / 3, r.x2 - rw / 3);
            slice_rect a = {r.y1, r.x1, r.y2, cut};
            slice_rect b = {r.y1, cut + 1, r.y2, r.x2};
            if ((a.x2 - a.x1) >= 5 && (b.x2 - b.x1) >= 5)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
        else if (!vertical && rh > 8)
        {
            int cut = rand_range(r.y1 + rh / 3, r.y2 - rh / 3);
            slice_rect a = {r.y1, r.x1, cut, r.x2};
            slice_rect b = {cut + 1, r.x1, r.y2, r.x2};
            if ((a.y2 - a.y1) >= 4 && (b.y2 - b.y1) >= 4)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
    }

    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    int floor_count = 0;
    for (int i = 0; i < rect_count; ++i)
    {
        slice_rect *r = &rects[i];
        for (int y = r->y1 + 1; y < r->y2; ++y)
        {
            for (int x = r->x1 + 1; x < r->x2; ++x)
            {
                cave_set_feat(y, x, FEAT_FLOOR);
                cave_info[y][x] |= CAVE_ROOM;
                floor_count++;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
            }
        }
    }

    if (floor_count < 20)
        return false;

    int cy = min_y, cx = min_x;
    for (int tries = 0; tries < 200; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (cave_floor_bold(ty, tx))
        {
            cy = ty;
            cx = tx;
            break;
        }
    }

    /* Set outer walls around floor tiles so tunnels can connect */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            /* Check if this wall borders any floor */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat(gy, gx, FEAT_WALL_OUTER);
            }
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_BSP_SLICE, one_in_(4));

    log_trace("BSP slice anchor: carved floor_count=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d) rects=%d",
        floor_count, min_y, min_x, max_y, max_x, cy, cx, rect_count);
    return true;
    return true;
}
#endif

#if 0
/* Try to seed BSP-sliced anchors in spare granite */
static void seed_bsp_slice_anchors(void)
{
    int target = (p_ptr->depth >= 8) ? 1 : 0;
    if (p_ptr->depth >= 20)
        target++;
    int placed = 0;
    for (int attempt = 0; attempt < 16 && placed < target; ++attempt)
    {
        if (carve_bsp_slice_anchor())
            placed++;
    }
    log_trace("BSP slice seeding complete: placed=%d target=%d", placed, target);
}
#endif

/* Place rooms in randomized order within a partition */
static void place_rooms_randomized(int y1, int y2, int x1, int x2, int depth,
                                   int t1_count, int t2_count, int t6_count, int t7_count,
                                   int *budget_t6, int *budget_t7, int *budget_t8,
                                   int *used_t6, int *used_t7, int *used_t8)
{
    /* Build an array of all room placements needed */
    int total = t1_count + t2_count + t6_count + t7_count;
    if (total <= 0) return;
    if (total > 50) total = 50;  /* Safety cap */
    
    int room_types[50];
    int idx = 0;
    for (int i = 0; i < t1_count && idx < 50; ++i) room_types[idx++] = 1;
    for (int i = 0; i < t2_count && idx < 50; ++i) room_types[idx++] = 2;
    for (int i = 0; i < t6_count && idx < 50; ++i) room_types[idx++] = 6;
    for (int i = 0; i < t7_count && idx < 50; ++i) room_types[idx++] = 7;
    
    /* Fisher-Yates shuffle */
    for (int i = total - 1; i > 0; --i)
    {
        int j = rand_int(i + 1);
        int temp = room_types[i];
        room_types[i] = room_types[j];
        room_types[j] = temp;
    }
    
    /* Place rooms in shuffled order */
    for (int i = 0; i < total; ++i)
    {
        int typ = room_types[i];
        int priority = (typ >= 6) ? 3 : 2;
        place_room_with_budget(typ, y1, y2, x1, x2, priority, depth,
                               budget_t6, budget_t7, budget_t8,
                               used_t6, used_t7, used_t8);
    }
}

/* Smallest depth at which a non-quest greater vault can appear */
static int min_nonquest_gv_depth(void)
{
    static int cached_min_depth = -1;
    if (cached_min_depth >= 0)
        return cached_min_depth;

    int min_depth = 127; /* high sentinel */
    for (int i = 0; i < z_info->v_max; ++i)
    {
        vault_type *v_ptr = &v_info[i];
        if (v_ptr->typ != 8)
            continue;
        if (v_ptr->flags & VLT_QUEST)
            continue;
        if (v_ptr->depth < min_depth)
            min_depth = v_ptr->depth;
    }

    /* Fallback to old gating depth if no candidates are present */
    if (min_depth == 127)
        min_depth = 15;

    cached_min_depth = min_depth;
    return cached_min_depth;
}

int vault_type8_generation_rarity(const vault_type* v_ptr, int depth)
{
    int rarity = v_ptr->rarity;

    if ((depth >= 6) && (v_ptr->flags & (VLT_SURFACE)))
    {
        rarity += (1 << depth);
    }

    return rarity;
}

bool quest_vault_surface_roll_allows(const vault_type* v_ptr, int depth)
{
    if (v_ptr->typ == 6)
    {
        if (depth < 6)
        {
            if (!(v_ptr->flags & (VLT_SURFACE)) && !one_in_(4))
                return false;
        }
        else if (v_ptr->flags & (VLT_SURFACE))
        {
            if (!one_in_(1 << depth))
                return false;
        }
    }
    else if ((depth >= 6) && (v_ptr->flags & (VLT_SURFACE)))
    {
        if (!one_in_(1 << depth))
            return false;
    }

    return true;
}

/* Roll whether this level should reserve a greater vault slot based on vault rarities */
static bool gv_level_roll_allows(int depth, int *out_candidates)
{
    int candidate_count = 0;
    bool passed = false;

    for (int i = 0; i < z_info->v_max; ++i)
    {
        vault_type *v_ptr = &v_info[i];
        if (v_ptr->typ != 8) continue;
        if (v_ptr->flags & VLT_QUEST) continue;
        if (v_ptr->depth > depth) continue;
        if (v_ptr->max_depth != 0 && depth > v_ptr->max_depth) continue;

        /* Skip already-used greater vaults to mirror build_type8 checks */
        bool repeated = false;
        for (int j = 0; j < MAX_GREATER_VAULTS; ++j)
        {
            if (p_ptr->greater_vaults[j] == i)
            {
                repeated = true;
                break;
            }
        }
        if (repeated) continue;

        candidate_count++;
        if (!passed && one_in_(vault_type8_generation_rarity(v_ptr, depth)))
        {
            passed = true;
        }
    }

    if (out_candidates) *out_candidates = candidate_count;

    if (candidate_count == 0)
    {
        genlog_partition("GV roll: depth=%d -> no eligible type8 templates (used or quest-only)", depth);
        return false;
    }

    if (passed)
    {
        genlog_partition("GV roll: depth=%d candidates=%d -> PASS (reserve GV this level)", depth, candidate_count);
    }
    else
    {
        genlog_partition("GV roll: depth=%d candidates=%d -> FAIL (no GV this level)", depth, candidate_count);
    }

    return passed;
}

/* Check whether a partition is fully interior (no map-border contact) */
static bool partition_is_interior(int row, int col, int rows, int cols)
{
    return (row > 0) && (row < rows - 1) && (col > 0) && (col < cols - 1);
}

bool generation_escape_tunnel_bold(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;

    return cave_escape_tunnel[y][x];
}

void mark_generation_escape_tunnel(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return;

    cave_escape_tunnel[y][x] = true;
}

/* Pick the partition whose centre is closest to the map centre, preferring interior slots */
static int choose_central_partition_index(int rows, int cols)
{
    if (rows <= 0 || cols <= 0)
        return -1;

    int best_idx = -1;
    int best_score = 1 << 30;
    int map_cy = p_ptr->cur_map_hgt / 2;
    int map_cx = p_ptr->cur_map_wid / 2;

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int pi = row * cols + col;
            int y1, y2, x1, x2;
            if (!compute_partition_bounds(pi, rows, cols, &y1, &y2, &x1, &x2))
                continue;

            int cy = (y1 + y2) / 2;
            int cx = (x1 + x2) / 2;
            int dist = distance(map_cy, map_cx, cy, cx);
            int penalty = partition_is_interior(row, col, rows, cols) ? 0 : 10000;
            int score = dist + penalty;

            if (score < best_score)
            {
                best_score = score;
                best_idx = pi;
            }
        }
    }

    return best_idx;
}

/* Try to drop a greater vault inside the provided partition bounds */
static bool place_gv_in_partition(int y1, int y2, int x1, int x2, int *budget_t8, int *used_t8)
{
    if (!budget_t8 || *budget_t8 <= 0)
        return false;

    if (dun->cent_n >= room_capacity_limit())
        return false;
    if (y2 - y1 < 6 || x2 - x1 < 8)
        return false;

    /* Can only have one greater vault per level */
    if (g_vault_name[0] != '\0')
        return false;

    bool placed = false;
    for (int attempt = 0; attempt < 3 && !placed; ++attempt)
    {
        int cy = rand_range(MAX(5, y1 + 3), MIN(p_ptr->cur_map_hgt - 5, y2 - 3));
        int cx = rand_range(MAX(5, x1 + 3), MIN(p_ptr->cur_map_wid - 5, x2 - 3));
        placed = build_reserved_type8(cy, cx);
    }

    if (!placed)
    {
        int scan_y1 = MAX(5, y1 + 3);
        int scan_y2 = MIN(p_ptr->cur_map_hgt - 5, y2 - 3);
        int scan_x1 = MAX(5, x1 + 3);
        int scan_x2 = MIN(p_ptr->cur_map_wid - 5, x2 - 3);

        if (scan_y1 <= scan_y2 && scan_x1 <= scan_x2)
        {
            log_trace("Greater vault: random partition placement missed, scanning bounds (%d,%d)-(%d,%d)",
                y1, x1, y2, x2);

            for (int cy = scan_y1; cy <= scan_y2 && !placed; ++cy)
            {
                for (int cx = scan_x1; cx <= scan_x2 && !placed; ++cx)
                {
                    placed = build_reserved_type8(cy, cx);
                }
            }
        }
    }

    if (placed)
    {
        (*budget_t8)--;
        if (used_t8)
            (*used_t8)++;
    }

    return placed;
}

/* Place a chest in a random floor location within partition bounds */


/* Dynamic partition-based generation mix */
static void apply_quadrant_generation_modes(void)
{
    /* Determine partition grid based on level size (in blocks) */
    int blocks = p_ptr->cur_map_hgt / PANEL_HGT;  /* Square levels, so hgt == wid */
    int partition_count;
    int grid_rows, grid_cols;
    int depth = p_ptr->depth;
    
    /* Partition scaling - REDUCED partition counts for larger anchors.
     * Each partition should be at least ~40 tiles per side to fit big caves/chasms.
     * 
      * Target partition size: 40-50 tiles per side for optimal anchor fitting.
      * 
      * Scaling by level size:
      *  6 blocks  ( 66x66)  -> 2x2 grid  (4 partitions)  = 33x33 per partition
      *  7 blocks  ( 77x77)  -> 2x2 grid  (4 partitions)  = 38x38 per partition
      *  8 blocks  ( 88x88)  -> 2x2 grid  (4 partitions)  = 44x44 per partition
      *  9 blocks  ( 99x99)  -> 2x2 grid  (4 partitions)  = 49x49 per partition
      * 10 blocks  (110x110) -> 2x3 grid  (6 partitions)  = 55x36 per partition
      * 11 blocks  (121x121) -> 3x3 grid  (9 partitions)  = 40x40 per partition
     * 12 blocks  (132x132) -> 3x3 grid  (9 partitions)  = 44x44 per partition
     * 13 blocks  (143x143) -> 3x3 grid  (9 partitions)  = 47x47 per partition
     * 14 blocks  (154x154) -> 3x4 grid (12 partitions)  = 51x38 per partition
     * 15 blocks  (165x165) -> 4x4 grid (16 partitions)  = 41x41 per partition
     * 16 blocks  (176x176) -> 4x4 grid (16 partitions)  = 44x44 per partition
     * 17 blocks  (187x187) -> 5x4 grid (20 partitions)  = 46x46 per partition
     * 18 blocks  (198x198) -> 5x4 grid (20 partitions)  = 49x49 per partition
     * 19 blocks  (209x209) -> 5x4 grid (20 partitions)  = 52x52 per partition
     * 20 blocks  (220x220) -> 5x4 grid (20 partitions)  = 55x55 per partition
     * 21 blocks  (231x231) -> 5x5 grid (25 partitions)  = 46x46 per partition
     */
    if (blocks <= 9)
    {
        partition_count = 4;
        grid_rows = 2; grid_cols = 2;
    }
    else if (blocks == 10)
    {
        partition_count = 6;
        if (one_in_(2)) { grid_rows = 3; grid_cols = 2; }
        else { grid_rows = 2; grid_cols = 3; }
    }
    else if (blocks <= 13)
    {
        partition_count = 9;
        grid_rows = 3; grid_cols = 3;
    }
    else if (blocks == 14)
    {
        partition_count = 12;
        if (one_in_(2)) { grid_rows = 3; grid_cols = 4; }
        else { grid_rows = 4; grid_cols = 3; }
    }
    else if (blocks <= 16)
    {
        partition_count = 16;
        grid_rows = 4; grid_cols = 4;
    }
    else if (blocks <= 20)
    {
        partition_count = 20;
        if (one_in_(2)) { grid_rows = 5; grid_cols = 4; }
        else { grid_rows = 4; grid_cols = 5; }
    }
    else  /* blocks >= 21 */
    {
        partition_count = 25;
        grid_rows = 5; grid_cols = 5;
    }

    remember_partition_grid(grid_rows, grid_cols, partition_count);
    
    log_trace("Level size %d blocks: using %dx%d partition grid (%d zones)", 
              blocks, grid_rows, grid_cols, partition_count);
    
    /* Generation log: partition grid setup */
    genlog_partition("Grid setup: %d blocks -> %dx%d grid (%d partitions), depth=%d",
                     blocks, grid_rows, grid_cols, partition_count, depth);
    
    /* Allocate mode, style, and density arrays - max 25 partitions now */
    quadrant_mode_t modes[25];
    int partition_styles[25];
    int partition_bridge_styles[25];
    big_cave_type_t partition_big_cave_types[25];
    density_level_t densities[25];
    int gv_partition = -1;
    int gv_min_depth = min_nonquest_gv_depth();
    bool gv_level_allowed = false;

    if (depth >= gv_min_depth)
    {
        if (!cached_gv_level_roll_resolved)
        {
            cached_gv_level_roll_allowed =
                gv_level_roll_allows(depth, &cached_gv_level_roll_candidates);
            cached_gv_level_roll_resolved = true;
        }

        gv_level_allowed = cached_gv_level_roll_allowed;
    }

    if (!gv_level_allowed && depth < gv_min_depth) {
        genlog_partition("GV roll: depth=%d below minimum %d -> no GV this level", depth, gv_min_depth);
    }
    if (morgoth_level_active) {
        gv_level_allowed = false; /* Morgoth's throne room replaces normal GVs */
        morgoth_partition_index = choose_central_partition_index(grid_rows, grid_cols);
        genlog_partition("Morgoth level: reserving central partition idx=%d (grid %dx%d)", morgoth_partition_index, grid_rows, grid_cols);
    }

    /* Depth-aware vault budgets (soft caps; clamped to remaining capacity) */
    /* BOOSTED: More rooms and vaults per partition for denser levels */
    int budget_t6 = MIN(room_capacity_limit(), MAX(20, partition_count * 3 + depth));
    int budget_t7 = (depth >= 4) ? MIN(room_capacity_limit(), MAX(6, partition_count + depth / 2)) : 0;
    int budget_t8 = gv_level_allowed ? 1 : 0;
    if (morgoth_level_active) {
        budget_t8 = 0;
    }
    int capacity_remaining = room_capacity_limit() - dun->cent_n;
    if (budget_t8 > capacity_remaining)
        budget_t8 = capacity_remaining;

    /* Reserve space for the dedicated GV attempt before scaling other budgets */
    int capacity_for_regular = capacity_remaining - budget_t8;
    if (capacity_for_regular < 0)
        capacity_for_regular = 0;

    int budget_total = budget_t6 + budget_t7;
    if (budget_total > capacity_for_regular && budget_total > 0) {
        /* Scale budgets down to fit remaining slots (GV slot already reserved) */
        budget_t6 = (budget_t6 * capacity_for_regular) / budget_total;
        budget_t7 = (budget_t7 * capacity_for_regular) / budget_total;
        if (budget_t6 + budget_t7 < capacity_for_regular) {
            budget_t6 = MIN(capacity_for_regular, budget_t6 + 1); /* keep at least one */
        }
    } else if (capacity_for_regular == 0) {
        budget_t6 = 0;
        budget_t7 = 0;
    }
    
    int mode_counts[6] = {0};
    /* Guarantee minimum ROOMY and CAVEY partitions based on partition count */
    /* ROOMY provides reliable standard rooms that connect well */
    int guaranteed_roomy = 1 + partition_count / 5;  /* At least 1 ROOMY, +1 per 5 partitions */
    int guaranteed_cavey = partition_count / 8;      /* 0 for small, 1+ for larger */
    
    /* Initialize with guaranteed modes first */
    int idx = 0;
    for (int i = 0; i < guaranteed_roomy && idx < partition_count; ++i, ++idx)
    {
        modes[idx] = QUAD_MODE_ROOMY;
        mode_counts[QUAD_MODE_ROOMY]++;
    }
    for (int i = 0; i < guaranteed_cavey && idx < partition_count; ++i, ++idx)
    {
        modes[idx] = QUAD_MODE_CAVEY;
        mode_counts[QUAD_MODE_CAVEY]++;
    }
    
    /* Fill remaining with random modes */
    for (; idx < partition_count; ++idx)
    {
        int weights[6];
        for (int m = 0; m < 6; ++m)
        {
            weights[m] = mode_weight_for_depth(
                (quadrant_mode_t)m, depth, blocks, mode_counts, partition_count);
        }
        modes[idx] = pick_weighted_mode(weights, N_ELEMENTS(weights));
        mode_counts[modes[idx]]++;
    }
    
    /* Shuffle all partitions */
    for (int i = partition_count - 1; i > 0; --i)
    {
        int j = rand_int(i + 1);
        quadrant_mode_t temp = modes[i];
        modes[i] = modes[j];
        modes[j] = temp;
    }
    
    log_trace("%d-partition level: %d ROOMY + %d CAVEY guaranteed, others randomized",
              partition_count, guaranteed_roomy, guaranteed_cavey);
    
    genlog_partition("Mode guarantees: %d ROOMY + %d CAVEY required, %d random",
                     guaranteed_roomy, guaranteed_cavey, partition_count - guaranteed_roomy - guaranteed_cavey);

    /* Never allow Morgoth's throne-room partition to be a special-mode partition.
     * Otherwise, environmental effects (labyrinth view loss, big cave penalties, etc.)
     * can bleed into the endgame setpiece. */
    if (morgoth_level_active && morgoth_partition_index >= 0 && morgoth_partition_index < partition_count)
    {
        if (modes[morgoth_partition_index] == QUAD_MODE_LABYRINTH
            || modes[morgoth_partition_index] == QUAD_MODE_CHASM
            || modes[morgoth_partition_index] == QUAD_MODE_BIG_CAVE)
        {
            log_trace("Morgoth level: forcing partition %d mode from %d to ROOMY",
                      morgoth_partition_index, (int)modes[morgoth_partition_index]);
        }
        modes[morgoth_partition_index] = QUAD_MODE_ROOMY;
    }
    
    /* Pick a random visual style and density for each partition */
    for (int i = 0; i < partition_count; ++i)
    {
        partition_bridge_styles[i] = -1;
        partition_big_cave_types[i] = BIG_CAVE_NONE;

        switch (modes[i])
        {
        case QUAD_MODE_CAVEY:
            partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_CA_BLOB);
            break;
        case QUAD_MODE_LABYRINTH:
            partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_LABYRINTH);
            break;
        case QUAD_MODE_CHASM:
            partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_CHASM_FLOOR);
            partition_bridge_styles[i] = styles_pick_partition_style(depth, PART_STYLE_CHASM_BRIDGE);
            break;
        case QUAD_MODE_BIG_CAVE:
            partition_big_cave_types[i] = big_cave_type_pick_for_depth(depth);
            if (partition_big_cave_types[i] == BIG_CAVE_ICE)
                partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_BIG_CAVE_ICE);
            else if (partition_big_cave_types[i] == BIG_CAVE_FIRE)
                partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_BIG_CAVE_FIRE);
            else
                partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_BIG_CAVE_POIS);
            break;
        case QUAD_MODE_ROOMY:
        case QUAD_MODE_RUINED:
        default:
            partition_styles[i] = styles_pick_random_from_level();
            break;
        }

        /* Fixed density distribution: 30% sparse, 40% normal, 30% dense */
        int sparse_chance = 30;
        int normal_chance = 40;

        int density_roll = rand_int(100);
        if (density_roll < sparse_chance)
            densities[i] = DENSITY_SPARSE;
        else if (density_roll < sparse_chance + normal_chance)
            densities[i] = DENSITY_NORMAL;
        else
            densities[i] = DENSITY_DENSE;
    }

    record_partition_metadata(modes, densities, partition_count);
    for (int i = 0; i < partition_count && i < 25; ++i)
    {
        current_partition_big_cave_types[i] = partition_big_cave_types[i];
        current_partition_bridge_styles[i] = partition_bridge_styles[i];
    }
    
    /* Pre-roll for a dedicated greater vault partition (must be interior) */
    if (budget_t8 > 0)
    {
        int gv_candidates[25];
        int gv_interior_count = 0;
        int gv_preferred[25];
        int gv_preferred_count = 0;
        for (int row = 0; row < grid_rows; ++row)
        {
            for (int col = 0; col < grid_cols; ++col)
            {
                if (!partition_is_interior(row, col, grid_rows, grid_cols))
                    continue;
                int idx = row * grid_cols + col;
                if (idx >= partition_count || gv_interior_count >= 25)
                    continue;
                gv_candidates[gv_interior_count++] = idx;

                /* Prefer a non-special partition for greater vaults so their setpiece
                 * effects don't overlap with LABYRINTH/CHASM/BIG_CAVE zones. */
                quadrant_mode_t m = modes[idx];
                if (m != QUAD_MODE_LABYRINTH && m != QUAD_MODE_CHASM && m != QUAD_MODE_BIG_CAVE)
                {
                    if (gv_preferred_count < 25)
                        gv_preferred[gv_preferred_count++] = idx;
                }
            }
        }

        if (gv_interior_count > 0)
        {
            bool used_preferred = (gv_preferred_count > 0);
            gv_partition = used_preferred
                ? gv_preferred[rand_int(gv_preferred_count)]
                : gv_candidates[rand_int(gv_interior_count)];
            int gv_row = gv_partition / grid_cols;
            int gv_col = gv_partition % grid_cols;
            log_trace("Greater vault partition: %d interior options (%d preferred) -> reserve partition %d (row=%d col=%d grid %dx%d%s)",
                      gv_interior_count, gv_preferred_count, gv_partition, gv_row, gv_col,
                      grid_rows, grid_cols, used_preferred ? "" : " fallback");
            genlog_partition("GV partition reserved (rarity passed): depth=%d min_depth=%d interior=%d preferred=%d -> (%d,%d) idx=%d grid=%dx%d%s",
                             depth, gv_min_depth, gv_interior_count, gv_preferred_count,
                             gv_row, gv_col, gv_partition, grid_rows, grid_cols,
                             used_preferred ? "" : " fallback");
        }
        else
        {
            log_trace("Greater vault partition: no eligible interior partitions for %dx%d grid",
                      grid_rows, grid_cols);
            genlog_partition("GV partition skipped: no interior partitions for grid %dx%d (depth=%d)", grid_rows, grid_cols, depth);
            gv_partition = -1;
            budget_t8 = 0; /* No dedicated slot this level */
        }
    }
    
    /* Mode name strings for logging */
    const char *mode_str[] = {"ROOMY", "CAVEY", "RUINED", "LABYRINTH", "CHASM", "BIG_CAVE"};
    const char *density_str[] = {"SPARSE", "NORMAL", "DENSE"};
    int used_t6 = 0, used_t7 = 0, used_t8 = 0;
    bool gv_partition_attempted = false;
    int partitions_skipped = 0;
    int skipped_soft_fill = 0;
    int skip_cap = MAX(2, partition_count / 5); /* cap outright skips to keep coverage */

    /* Track which partitions have been processed */
    bool partition_done[25];
    for (int i = 0; i < 25; ++i)
        partition_done[i] = false;

    /* TWO-PASS PROCESSING:
     * Pass 1: Process special modes (LABYRINTH, CHASM, BIG_CAVE) first.
     *         These need clear space for anchor carving, so they must run
     *         before ROOMY/CAVEY can place rooms that encroach on neighbors.
     * Pass 2: Process remaining modes (ROOMY, CAVEY, RUINED).
     */
    genlog_partition("Processing special modes first (LABYRINTH, CHASM, BIG_CAVE) to ensure clear space");
    
    /* Pass 1: Special modes only */
    for (int pi = 0; pi < partition_count; ++pi)
    {
        quadrant_mode_t mode = modes[pi];
        bool is_gv_partition = (pi == gv_partition);
        bool is_morgoth_partition = (morgoth_level_active && pi == morgoth_partition_index);
        bool is_special_mode = (mode == QUAD_MODE_LABYRINTH || mode == QUAD_MODE_CHASM || mode == QUAD_MODE_BIG_CAVE);
        if (!is_gv_partition && !is_special_mode && !is_morgoth_partition)
            continue;  /* Skip non-special modes for now */

        if (dun->cent_n >= room_capacity_limit())
        {
            log_trace("Partition gen: room capacity reached (%d/%d), skipping remaining partitions",
                      dun->cent_n, room_capacity_limit());
            break;
        }

        /* Calculate partition boundaries based on grid */
        int before_cent = dun->cent_n;
        int row = pi / grid_cols;
        int col = pi % grid_cols;
        
        int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
        if (!compute_partition_bounds(pi, grid_rows, grid_cols, &y1, &y2, &x1, &x2))
        {
            log_trace("Partition %d [%d,%d]: invalid bounds for grid %dx%d",
                pi, row, col, grid_rows, grid_cols);
            continue;
        }
        
        if (is_morgoth_partition)
        {
            morgoth_partition_bounds.y1 = y1;
            morgoth_partition_bounds.y2 = y2;
            morgoth_partition_bounds.x1 = x1;
            morgoth_partition_bounds.x2 = x2;
            morgoth_vault_center_y = (y1 + y2) / 2;
            morgoth_vault_center_x = (x1 + x2) / 2;
            morgoth_partition_reserved = true;
            
            /* Place and seal Morgoth's throne room IMMEDIATELY to prevent other 
             * partitions from placing content in this area. The permanent wall sealing
             * must happen before any other room/corridor generation. */
            vault_type* v_ptr = NULL;
            int cy = morgoth_vault_center_y;
            int cx = morgoth_vault_center_x;
            
            if (build_type9(cy, cx, &v_ptr))
            {
                carve_morgoth_entry_tunnels(v_ptr, cy, cx);
                seal_morgoth_partition(v_ptr, cy, cx);
                partition_done[pi] = true;
                genlog_partition("Morgoth partition placed and sealed at idx=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d)", 
                                pi, y1, x1, y2, x2, cy, cx);
            }
            else
            {
                log_trace("Morgoth level: failed to build throne room at (%d,%d) in partition %d", cy, cx, pi);
                morgoth_partition_reserved = false;  /* Allow fallback */
            }
            continue;
        }

        /* mode already declared at loop start for the continue check */
        int style_idx = partition_styles[pi];
        int bridge_style = partition_bridge_styles[pi];
        big_cave_type_t cave_type = partition_big_cave_types[pi];
        density_level_t density = densities[pi];
        int area = (y2 - y1 + 1) * (x2 - x1 + 1);
        int area_factor = MAX(1, MIN(3, (area + 1100) / 1200));
        int floor_pct = 0, icky_pct = 0;
        bool reserved = area_is_reserved_or_dense(y1, y2, x1, x2, &floor_pct, &icky_pct);

        log_trace("Partition %d [%d,%d] (pass 1%s): mode=%s density=%s bounds=(%d,%d)-(%d,%d) area=%d floor=%d%% icky=%d%%",
                  pi, row, col, is_gv_partition ? " GV" : "", mode_str[mode], density_str[density], y1, x1, y2, x2, area, floor_pct, icky_pct);

        if (reserved && partitions_skipped >= skip_cap) {
            /* Too many skips already: fall back to a light recipe instead of skipping */
            log_trace("Partition %d [%d,%d]: reserved but skip_cap reached; using soft-fill", pi, row, col);
            reserved = false;
            skipped_soft_fill++;
            /* Downgrade density to sparse to reduce conflicts */
            density = DENSITY_SPARSE;
        }

        if (reserved) {
            log_trace("Partition %d [%d,%d]: skipping (reserved/quest/icky overlap)", pi, row, col);
            if (is_gv_partition) {
                gv_partition = -1;
                budget_t8 = 0;
            }
            partitions_skipped++;
            continue;
        }

        if (is_gv_partition)
        {
            gv_partition_attempted = true;
            bool placed_gv = place_gv_in_partition(y1, y2, x1, x2, &budget_t8, &used_t8);
            if (placed_gv)
            {
                log_trace("Partition %d [%d,%d]: placed greater vault within bounds (%d,%d)-(%d,%d)",
                          pi, row, col, y1, x1, y2, x2);
                genlog_partition("GV placed '%s' in partition [%d,%d] idx=%d bounds=(%d,%d)-(%d,%d) remaining_t8=%d",
                                 g_vault_name[0] ? g_vault_name : level_gen_debug_last_greater_vault_name,
                                 row, col, pi, y1, x1, y2, x2, budget_t8);
                partition_done[pi] = true;
                continue;
            }

            log_trace("Partition %d [%d,%d]: greater vault placement failed, falling back to mode logic",
                      pi, row, col);
            genlog_partition("GV placement failed for '%s' in partition [%d,%d] idx=%d bounds=(%d,%d)-(%d,%d); disabling GV for this attempt",
                             level_gen_debug_last_greater_vault_name[0]
                                 ? level_gen_debug_last_greater_vault_name
                                 : "(unknown)",
                             row, col, pi, y1, x1, y2, x2);
            gv_partition = -1;
            budget_t8 = 0;
            if (!is_special_mode)
                continue;
        }

        /* PARTITION MODE TYPES:
         * - ROOMY: Traditional dungeon - balanced mix of all room types
         * - CAVEY: Natural cave system with CA blobs and minimal rooms
         * - RUINED: Ancient carved BSP passages with rooms
         * - LABYRINTH: Maze corridors with chambers
         * - CHASM: Platforms over chasms connected by bridges
         * - BIG_CAVE: Single massive irregular cavern
         */
        switch (mode)
        {
        case QUAD_MODE_CAVEY:
            {
                /* Natural cave system: CA blobs with quartz veins */
                int area = (y2 - y1) * (x2 - x1);
                int base_blobs = 2 + area / 400;  /* Scale with partition size */
                int blob_target = (density == DENSITY_SPARSE) ? base_blobs : 
                                  (density == DENSITY_DENSE) ? base_blobs + 2 : base_blobs + 1;
                if (blob_target > 6) blob_target = 6;
                int carved_blobs = 0;
                
                for (int b = 0; b < blob_target; ++b)
                    if (carve_ca_blob_anchor_bounds(y1, y2, x1, x2, style_idx))
                        carved_blobs++;
                
                /* Scatter quartz veins for natural cave look */
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, 0);
                
                set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                    0, 100, 0, 0, PARTITION_CHEST_ANCHOR_ANY);
                
                /* Caves with rooms scattered inside */
                /* Sparse: T1=2 T2=1 T6=2 T7=0 | Normal: T1=2 T2=2 T6=2 T7=1 | Dense: T1=2 T2=3 T6=3 T7=1 */
                int std_count = scaled_attempts(2, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : (density == DENSITY_DENSE) ? 1 : 1, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        case QUAD_MODE_LABYRINTH:
            {
                /* Maze corridors - oppressive, fewer rooms */
                bool carved = carve_labyrinth_bounds(y1, y2, x1, x2, density, style_idx);
                if (!carved)
                {
                    /* Fallback: more BSP slices for maze-like feel */
                    int maze_count = (density == DENSITY_SPARSE) ? 6 : 
                                     (density == DENSITY_DENSE) ? 12 : 8;
                    for (int b = 0; b < maze_count; ++b)
                        carve_bsp_slice_anchor_bounds(y1, y2, x1, x2);
                    /* Update partition mode to match fallback generation (use RUINED for BSP slices) */
                    current_partition_modes[pi] = QUAD_MODE_RUINED;
                    style_idx = styles_pick_random_from_level();
                    partition_styles[pi] = style_idx;
                }
                
                /* Add some dead-end interest: occasional rubble in corridors */
                for (int gy = y1; gy <= y2; ++gy)
                {
                    for (int gx = x1; gx <= x2; ++gx)
                    {
                        if (!in_bounds_fully(gy, gx)) continue;
                        if (!cave_floor_bold(gy, gx)) continue;
                        /* Very low rubble chance for claustrophobic feel */
                        if (one_in_(40))
                            cave_set_feat_style(gy, gx, FEAT_RUBBLE, style_idx);
                    }
                }
                
                /* Labyrinth with chambers and vaults */
                /* Sparse: T1=1 T2=0 T6=1 T7=0 | Normal: T1=1 T2=1 T6=1 T7=0 | Dense: T1=1 T2=1 T6=2 T7=1 */
                int std_count = scaled_attempts(1, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : 1, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 2 : 1, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : (density == DENSITY_DENSE) ? 1 : 0, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
                
                /* Place 1 chest in labyrinth partition ONLY if it actually carved */
                if (carved)
                {
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                        1, 0, 100, 0, PARTITION_CHEST_ANCHOR_ANY);
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 1,
                        1, 0, 0, 100, PARTITION_CHEST_ANCHOR_ANY);
                }
            }
            break;
        case QUAD_MODE_CHASM:
            {
                /* Chasm with platforms connected by bridges - no additional rooms */
                bool chasm_carved = carve_chasm_with_bridges(y1, y2, x1, x2,
                    style_idx, bridge_style);
                if (!chasm_carved)
                {
                    /* Fallback: use CA blobs to keep the open feel */
                    int ca_style = styles_pick_partition_style(depth, PART_STYLE_CA_BLOB);
                    int blob_count = (density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3;
                    for (int b = 0; b < blob_count; ++b)
                        carve_ca_blob_anchor_bounds(y1, y2, x1, x2, ca_style);
                    /* Update partition mode to match fallback generation */
                    current_partition_modes[pi] = QUAD_MODE_CAVEY;
                    style_idx = ca_style;
                    partition_styles[pi] = ca_style;
                    bridge_style = -1;
                    partition_bridge_styles[pi] = -1;
                }

                /* Veins in chasm walls for mining (tagged for metal placement) */
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, CAVE_CHASM_AREA);

                /* Place 2 guaranteed chests in chasm partition ONLY if it actually carved */
                if (chasm_carved)
                {
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                        0, 0, 65, 35, PARTITION_CHEST_ANCHOR_ANY);
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 1,
                        0, 0, 65, 35, PARTITION_CHEST_ANCHOR_ANY);
                }
            }
            break;
        case QUAD_MODE_BIG_CAVE:
            {
                /* Single massive cavern - the cave IS the room */
                bool carved = carve_big_cave_bounds(y1, y2, x1, x2, style_idx, cave_type);
                int blob_count = 0;
                int carved_blobs = 0;
                if (!carved)
                {
                    /* Fallback: many overlapping blobs */
                    int ca_style = styles_pick_partition_style(depth, PART_STYLE_CA_BLOB);
                    blob_count = (density == DENSITY_SPARSE) ? 5 : 
                                 (density == DENSITY_DENSE) ? 10 : 7;
                    for (int b = 0; b < blob_count; ++b)
                        if (carve_ca_blob_anchor_bounds(y1, y2, x1, x2, ca_style))
                            carved_blobs++;
                    /* Update partition mode to match fallback generation */
                    current_partition_modes[pi] = QUAD_MODE_CAVEY;
                    current_partition_big_cave_types[pi] = BIG_CAVE_NONE;
                    partition_big_cave_types[pi] = BIG_CAVE_NONE;
                    style_idx = ca_style;
                    partition_styles[pi] = ca_style;
                }
                
                /* Add quartz veins for natural cave look */
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, 0);

                if (!carved)
                {
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                        0, 100, 0, 0, PARTITION_CHEST_ANCHOR_ANY);
                }
                
                /* Add internal pillars/boulders for visual interest (density-scaled) */
                int pillar_target = (density == DENSITY_SPARSE) ? 3 : 
                                    (density == DENSITY_DENSE) ? 10 : 6;
                int pillars_placed = 0;
                for (int tries = 0; tries < 100 && pillars_placed < pillar_target; ++tries)
                {
                    int py = rand_range(y1 + 3, y2 - 3);
                    int px = rand_range(x1 + 3, x2 - 3);
                    if (!in_bounds_fully(py, px)) continue;
                    if (!cave_floor_bold(py, px)) continue;
                    
                    /* Check all neighbors are floor */
                    bool all_floor = true;
                    for (int dy = -1; dy <= 1 && all_floor; ++dy)
                        for (int dx = -1; dx <= 1 && all_floor; ++dx)
                            if (!cave_floor_bold(py + dy, px + dx))
                                all_floor = false;
                    
                    if (all_floor)
                    {
                        cave_set_feat_style(py, px, FEAT_WALL_EXTRA, style_idx);
                        pillars_placed++;
                    }
                }
                
                /* Guarantee two large chests in big caves with default material odds. */
                if (carved)
                {
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                        2, 50, 35, 15, PARTITION_CHEST_ANCHOR_ANY);
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 1,
                        2, 50, 35, 15, PARTITION_CHEST_ANCHOR_ANY);
                }
            }
            break;
        case QUAD_MODE_ROOMY:
        default:
            {
                /* Traditional dungeon - packed with rooms and vaults */
                /* Sparse: T1=2 T2=1 T6=2 T7=1 | Normal: T1=3 T2=2 T6=3 T7=2 | Dense: T1=4 T2=3 T6=4 T7=3 */
                int std_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        }

        /* Per-partition fallback: if nothing landed, drop a simple room to avoid voids */
        if (dun->cent_n == before_cent)
        {
            int fallback_style = styles_pick_random_from_level();
            style_idx = fallback_style;
            partition_styles[pi] = fallback_style;
            if (room_build_in_bounds(1, y1, y2, x1, x2) || room_build_in_bounds(2, y1, y2, x1, x2))
                log_trace("Partition %d [%d,%d]: fallback simple room placed", pi, row, col);
        }

        /* Apply the partition's visual style to its granite walls.
         * Use a jagged/organic boundary instead of a straight line. */
        if (style_idx >= 0)
        {
            int blend_zone = 3;

            for (int y = y1; y <= y2; ++y)
            {
                for (int x = x1; x <= x2; ++x)
                {
                    if (cave_feat[y][x] != FEAT_WALL_EXTRA)
                        continue;

                    int dist_top = y - y1;
                    int dist_bot = y2 - y;
                    int dist_left = x - x1;
                    int dist_right = x2 - x;
                    int dist_edge = MIN(MIN(dist_top, dist_bot), MIN(dist_left, dist_right));

                    if (dist_edge >= blend_zone)
                    {
                        cave_set_feat_with_color(y, x, FEAT_WALL_EXTRA, style_idx);
                    }
                    else
                    {
                        int chance = 20 + (dist_edge * 67 / blend_zone);
                        if (rand_int(100) < chance)
                        {
                            cave_set_feat_with_color(y, x, FEAT_WALL_EXTRA, style_idx);
                        }
                    }
                }
            }
        }

        /* Mark partition as done */
        partition_done[pi] = true;
    }

    /* Pass 2: Process remaining non-special modes (ROOMY, CAVEY, RUINED) */
    genlog_partition("Pass 2: Processing standard modes (ROOMY, CAVEY, RUINED)");
    for (int pi = 0; pi < partition_count; ++pi)
    {
        if (partition_done[pi])
            continue;  /* Already processed in Pass 1 */

        if (dun->cent_n >= room_capacity_limit())
        {
            log_trace("Partition gen: room capacity reached (%d/%d), skipping remaining partitions",
                      dun->cent_n, room_capacity_limit());
            break;
        }

        /* Calculate partition boundaries based on grid */
        int before_cent = dun->cent_n;
        int row = pi / grid_cols;
        int col = pi % grid_cols;
        
        int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
        if (!compute_partition_bounds(pi, grid_rows, grid_cols, &y1, &y2, &x1, &x2))
        {
            log_trace("Partition %d [%d,%d]: invalid bounds for grid %dx%d",
                pi, row, col, grid_rows, grid_cols);
            continue;
        }
        
        quadrant_mode_t mode = modes[pi];
        density_level_t density = densities[pi];
        int style_idx = partition_styles[pi];
        int area = (y2 - y1 + 1) * (x2 - x1 + 1);
        int area_factor = MAX(1, MIN(3, (area + 1100) / 1200));
        int floor_pct = 0, icky_pct = 0;
        bool reserved = area_is_reserved_or_dense(y1, y2, x1, x2, &floor_pct, &icky_pct);

        log_trace("Partition %d [%d,%d] (pass 2): mode=%s density=%s bounds=(%d,%d)-(%d,%d)",
                  pi, row, col, mode_str[mode], density_str[density], y1, x1, y2, x2);

        if (reserved && partitions_skipped >= skip_cap) {
            reserved = false;
            skipped_soft_fill++;
            density = DENSITY_SPARSE;
        }

        if (reserved) {
            log_trace("Partition %d [%d,%d]: skipping (reserved/quest/icky overlap)", pi, row, col);
            partitions_skipped++;
            continue;
        }

        /* Process the partition based on its mode (standard modes only here) */
        switch (mode)
        {
        case QUAD_MODE_CAVEY:
            {
                int blob_target = 2 + (y2 - y1) * (x2 - x1) / 400;
                if (blob_target > 6) blob_target = 6;
                int carved_blobs = 0;
                for (int b = 0; b < blob_target; ++b)
                    if (carve_ca_blob_anchor_bounds(y1, y2, x1, x2, style_idx))
                        carved_blobs++;
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, 0);
                set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                    0, 100, 0, 0, PARTITION_CHEST_ANCHOR_ANY);
                int std_count = scaled_attempts(2, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_DENSE) ? 4 : (density == DENSITY_SPARSE) ? 2 : 3, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 2 : 1, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : (density == DENSITY_DENSE) ? 1 : 1, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        case QUAD_MODE_RUINED:
            {
                int std_count = scaled_attempts(1, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : 1, area_factor);
                int int_count = scaled_attempts((density == DENSITY_DENSE) ? 2 : 1, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_DENSE) ? 1 : 0, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);

                int carve_count = 3 + (y2 - y1) * (x2 - x1) / 500;
                if (carve_count > 10) carve_count = 10;
                for (int b = 0; b < carve_count; ++b)
                    carve_bsp_slice_anchor_bounds(y1, y2, x1, x2);
                
                /* Add rubble to carved floor tiles (5-10-15% based on density) */
                int rubble_chance = (density == DENSITY_SPARSE) ? 3 : 
                                    (density == DENSITY_DENSE) ? 10 : 7;
                for (int gy = y1; gy <= y2; ++gy)
                {
                    for (int gx = x1; gx <= x2; ++gx)
                    {
                        if (!in_bounds_fully(gy, gx)) continue;
                        if (!cave_floor_bold(gy, gx)) continue;
                        if (cave_info[gy][gx] & CAVE_G_VAULT) continue; /* preserve greater vaults only */
                        if (rand_int(100) < rubble_chance)
                            cave_set_feat(gy, gx, FEAT_RUBBLE);
                    }
                }
                
                /* Add broken wall segments */
                for (int gy = y1 + 2; gy <= y2 - 2; ++gy)
                {
                    for (int gx = x1 + 2; gx <= x2 - 2; ++gx)
                    {
                        if (!in_bounds_fully(gy, gx)) continue;
                        if (cave_info[gy][gx] & CAVE_G_VAULT) continue; /* preserve greater vaults only */
                        if (cave_feat[gy][gx] != FEAT_WALL_OUTER) continue;
                        if (rand_int(100) < 30)
                        {
                            cave_set_feat(gy, gx, FEAT_FLOOR);
                            cave_info[gy][gx] |= CAVE_ROOM;
                            if (one_in_(2))
                                cave_set_feat(gy, gx, FEAT_RUBBLE);
                        }
                    }
                }

                set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                    1, 0, 100, 0, PARTITION_CHEST_ANCHOR_BSP_SLICE);
                
            }
            break;
        default:
            {
                /* ROOMY or fallback: Traditional dungeon */
                int std_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        }

        /* Per-partition fallback */
        if (dun->cent_n == before_cent)
        {
            if (room_build_in_bounds(1, y1, y2, x1, x2) || room_build_in_bounds(2, y1, y2, x1, x2))
            {
                log_trace("Partition %d [%d,%d]: fallback simple room placed", pi, row, col);
                /* Update partition mode to ROOMY since we fell back to standard rooms */
                current_partition_modes[pi] = QUAD_MODE_ROOMY;
            }
        }
    }

    /* Log partition generation summary */
    log_debug("Generation summary: %d blocks, %dx%d grid (%d partitions), %d rooms created",
              blocks, grid_rows, grid_cols, partition_count, dun->cent_n);
    log_debug("Partition budgets: used t6=%d/t7=%d/t8=%d remaining t6=%d t7=%d t8=%d skipped_parts=%d soft_fill=%d",
              used_t6, used_t7, used_t8, budget_t6, budget_t7, budget_t8, partitions_skipped, skipped_soft_fill);
    log_trace("Greater vault partition summary: attempted=%s placed=%d",
              gv_partition_attempted ? "yes" : "no", used_t8);
    
    /* Detailed generation log summary */
    genlog_summary("Partition phase complete: %d rooms from %d partitions (%d skipped, %d soft-fill skipped)",
                   dun->cent_n, partition_count, partitions_skipped, skipped_soft_fill);
    genlog_summary("Room budgets - T6: %d used / T7: %d used / T8: %d used",
                   used_t6, used_t7, used_t8);
    
    /* Log mode distribution and persist labyrinth count for monster/stair bonuses */
    {
        int mode_counts_summary[6] = {0};
        for (int mi = 0; mi < partition_count; ++mi)
            mode_counts_summary[modes[mi]]++;
        current_labyrinth_partitions = mode_counts_summary[QUAD_MODE_LABYRINTH];
        genlog_partition("Mode distribution: ROOMY=%d CAVEY=%d RUINED=%d LABYRINTH=%d CHASM=%d BIG_CAVE=%d",
                         mode_counts_summary[0], mode_counts_summary[1], mode_counts_summary[2],
                         mode_counts_summary[3], mode_counts_summary[4], mode_counts_summary[5]);
    }
    
}

/* Carve connection corridors at partition boundaries to ensure inter-partition connectivity.
 * This helps when caves/labyrinths in adjacent partitions don't naturally connect.
 * IMPROVED: Now searches deeper into partitions (15 tiles) and carves longer corridors (8 tiles).
 * Also tries multiple x/y positions per boundary segment. */
#if 0
static void ensure_partition_connectivity(void)
{
    int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
    int grid_rows = current_partition_rows;
    int grid_cols = current_partition_cols;
    
    /* Reuse the grid chosen during generation; fall back if unavailable */
    if (grid_rows <= 0 || grid_cols <= 0) {
        fallback_partition_grid_from_blocks(blocks, &grid_rows, &grid_cols);
    }
    
    int connections_added = 0;
    const int SEARCH_DEPTH = 15;  /* How far into partition to look for floor (was 5) */
    const int CORRIDOR_LEN = 8;   /* How long the carved corridor is (was 3) */
    const int ATTEMPTS_PER_SEGMENT = 3;  /* Try multiple positions per boundary segment */
    
    genlog_connect("ensure_partition_connectivity: %dx%d grid, searching %d deep, carving %d long",
                   grid_rows, grid_cols, SEARCH_DEPTH, CORRIDOR_LEN);
    
    /* Create horizontal boundary connections (between rows) */
    for (int row = 0; row < grid_rows - 1; ++row)
    {
        int boundary_y = ((row + 1) * p_ptr->cur_map_hgt / grid_rows);
        
        for (int col = 0; col < grid_cols; ++col)
        {
            int x1 = (col * p_ptr->cur_map_wid / grid_cols) + 2;
            int x2 = ((col + 1) * p_ptr->cur_map_wid / grid_cols) - 2;
            
            /* Try multiple x positions for better coverage */
            for (int attempt = 0; attempt < ATTEMPTS_PER_SEGMENT; ++attempt)
            {
                int cx = rand_range(x1 + 2, x2 - 2);
                
                /* Find nearest floor above and below the boundary */
                int floor_above_y = -1, floor_above_x = -1;
                int floor_below_y = -1, floor_below_x = -1;
                
                for (int dx = -5; dx <= 5; ++dx)
                {
                    int tx = cx + dx;
                    if (tx < 1 || tx >= p_ptr->cur_map_wid - 1) continue;
                    
                    for (int dy = 1; dy <= SEARCH_DEPTH; ++dy)
                    {
                        if (floor_above_y < 0 && in_bounds_fully(boundary_y - dy, tx) 
                            && cave_floor_bold(boundary_y - dy, tx))
                        {
                            floor_above_y = boundary_y - dy;
                            floor_above_x = tx;
                        }
                        if (floor_below_y < 0 && in_bounds_fully(boundary_y + dy, tx) 
                            && cave_floor_bold(boundary_y + dy, tx))
                        {
                            floor_below_y = boundary_y + dy;
                            floor_below_x = tx;
                        }
                    }
                }
                
                /* If both partitions have floor nearby, check if connection needed */
                if (floor_above_y >= 0 && floor_below_y >= 0)
                {
                    /* Check if boundary is already connected */
                    bool boundary_connected = false;
                    for (int dx = -3; dx <= 3; ++dx)
                    {
                        int tx = cx + dx;
                        for (int dy = -2; dy <= 2; ++dy)
                        {
                            if (in_bounds_fully(boundary_y + dy, tx) && cave_floor_bold(boundary_y + dy, tx))
                            {
                                boundary_connected = true;
                                break;
                            }
                        }
                        if (boundary_connected) break;
                    }
                    
                    if (!boundary_connected)
                    {
                        /* Carve from floor_above to floor_below through the boundary */
                        int mid_x = (floor_above_x + floor_below_x) / 2;
                        
                        /* Carve vertical corridor centered on boundary */
                        for (int dy = -CORRIDOR_LEN; dy <= CORRIDOR_LEN; ++dy)
                        {
                            int ty = boundary_y + dy;
                            if (in_bounds_fully(ty, mid_x) && 
                                (cave_feat[ty][mid_x] == FEAT_WALL_EXTRA || cave_feat[ty][mid_x] == FEAT_WALL_OUTER))
                            {
                                cave_set_feat(ty, mid_x, FEAT_FLOOR);
                            }
                        }
                        connections_added++;
                        genlog_connect("H-boundary row=%d col=%d: carved at x=%d from y=%d to y=%d",
                                       row, col, mid_x, boundary_y - CORRIDOR_LEN, boundary_y + CORRIDOR_LEN);
                        break;  /* Only one connection per segment needed */
                    }
                }
            }
        }
    }
    
    /* Create vertical boundary connections (between columns) */
    for (int col = 0; col < grid_cols - 1; ++col)
    {
        int boundary_x = ((col + 1) * p_ptr->cur_map_wid / grid_cols);
        
        for (int row = 0; row < grid_rows; ++row)
        {
            int y1 = (row * p_ptr->cur_map_hgt / grid_rows) + 2;
            int y2 = ((row + 1) * p_ptr->cur_map_hgt / grid_rows) - 2;
            
            for (int attempt = 0; attempt < ATTEMPTS_PER_SEGMENT; ++attempt)
            {
                int cy = rand_range(y1 + 2, y2 - 2);
                
                int floor_left_y = -1, floor_left_x = -1;
                int floor_right_y = -1, floor_right_x = -1;
                
                for (int dy = -5; dy <= 5; ++dy)
                {
                    int ty = cy + dy;
                    if (ty < 1 || ty >= p_ptr->cur_map_hgt - 1) continue;
                    
                    for (int dx = 1; dx <= SEARCH_DEPTH; ++dx)
                    {
                        if (floor_left_x < 0 && in_bounds_fully(ty, boundary_x - dx) 
                            && cave_floor_bold(ty, boundary_x - dx))
                        {
                            floor_left_y = ty;
                            floor_left_x = boundary_x - dx;
                        }
                        if (floor_right_x < 0 && in_bounds_fully(ty, boundary_x + dx) 
                            && cave_floor_bold(ty, boundary_x + dx))
                        {
                            floor_right_y = ty;
                            floor_right_x = boundary_x + dx;
                        }
                    }
                }
                
                if (floor_left_x >= 0 && floor_right_x >= 0)
                {
                    bool boundary_connected = false;
                    for (int dy = -3; dy <= 3; ++dy)
                    {
                        int ty = cy + dy;
                        for (int dx = -2; dx <= 2; ++dx)
                        {
                            if (in_bounds_fully(ty, boundary_x + dx) && cave_floor_bold(ty, boundary_x + dx))
                            {
                                boundary_connected = true;
                                break;
                            }
                        }
                        if (boundary_connected) break;
                    }
                    
                    if (!boundary_connected)
                    {
                        int mid_y = (floor_left_y + floor_right_y) / 2;
                        
                        for (int dx = -CORRIDOR_LEN; dx <= CORRIDOR_LEN; ++dx)
                        {
                            int tx = boundary_x + dx;
                            if (in_bounds_fully(mid_y, tx) && 
                                (cave_feat[mid_y][tx] == FEAT_WALL_EXTRA || cave_feat[mid_y][tx] == FEAT_WALL_OUTER))
                            {
                                cave_set_feat(mid_y, tx, FEAT_FLOOR);
                            }
                        }
                        connections_added++;
                        genlog_connect("V-boundary row=%d col=%d: carved at y=%d from x=%d to x=%d",
                                       row, col, mid_y, boundary_x - CORRIDOR_LEN, boundary_x + CORRIDOR_LEN);
                        break;
                    }
                }
            }
        }
    }
    
    if (connections_added > 0)
    {
        log_trace("Partition connectivity: added %d boundary connections", connections_added);
        genlog_connect("Partition connectivity: added %d boundary connections total", connections_added);
    }
    else
    {
        genlog_connect("Partition connectivity: no new connections needed");
    }
}
#endif

typedef struct {
    rectangle bounds;
    coord center;
    int rooms[CENT_MAX];
    int room_count;
    int hub_room;
} partition_link_data_t;

int partition_index_from_point(int y, int x, int rows, int cols)
{
    if (rows <= 0 || cols <= 0) return -1;
    if (p_ptr->cur_map_hgt <= 0 || p_ptr->cur_map_wid <= 0) return -1;
    int row = (y * rows) / p_ptr->cur_map_hgt;
    int col = (x * cols) / p_ptr->cur_map_wid;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= rows) row = rows - 1;
    if (col >= cols) col = cols - 1;
    return row * cols + col;
}

static int room_partition_index(int room_idx)
{
    if (room_idx < 0 || room_idx >= dun->cent_n)
        return -1;
    if (current_partition_rows <= 0 || current_partition_cols <= 0
        || current_partition_count <= 0)
    {
        return -1;
    }

    return partition_index_from_point(dun->cent[room_idx].y, dun->cent[room_idx].x,
        current_partition_rows, current_partition_cols);
}

bool tunnel_should_mark_escape(int r1, int r2)
{
    int p1 = room_partition_index(r1);
    int p2 = room_partition_index(r2);
    bool big1 = (p1 >= 0 && p1 < current_partition_count && p1 < 25
        && is_big_partition_mode(current_partition_modes[p1]));
    bool big2 = (p2 >= 0 && p2 < current_partition_count && p2 < 25
        && is_big_partition_mode(current_partition_modes[p2]));

    if (!big1 && !big2)
        return false;

    if (p1 >= 0 && p2 >= 0 && p1 == p2)
        return false;

    return true;
}

static int room_connection_degree(int room_idx)
{
    if (room_idx < 0 || room_idx >= dun->cent_n)
        return 0;
    int deg = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        if (dun->connection[room_idx][i])
            deg++;
    }
    return deg;
}

static bool connect_rooms_with_logging(int r1, int r2, const char *tag, bool allow_desperate)
{
    if (r1 < 0 || r2 < 0 || r1 == r2)
        return false;

    if (dun->connection[r1][r2])
        return true;

    bool ok = connect_two_rooms(r1, r2, true, false);
    if (!ok && allow_desperate)
        ok = connect_two_rooms(r1, r2, true, true);

    if (ok && tag)
    {
        int dist = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y, dun->cent[r2].x);
        genlog_connect("%s: linked room %d -> %d (dist=%d)", tag, r1, r2, dist);
    }
    return ok;
}

static bool is_big_partition_mode(quadrant_mode_t mode)
{
    return (mode == QUAD_MODE_LABYRINTH || mode == QUAD_MODE_BIG_CAVE || mode == QUAD_MODE_CHASM);
}

static bool big_partition_boundary_floor_ok(quadrant_mode_t mode, int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (!cave_floor_bold(y, x))
        return false;
    if (cave_feat[y][x] == FEAT_CHASM)
        return false;
    if (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT))
        return false;
    if (mode == QUAD_MODE_CHASM)
        return ((cave_info[y][x] & (CAVE_ROOM | CAVE_CHASM_AREA))
            == (CAVE_ROOM | CAVE_CHASM_AREA));
    return ((cave_info[y][x] & CAVE_ROOM) != 0);
}

static int partition_bridge_style_for_index(int pi)
{
    if (pi < 0 || pi >= current_partition_count || pi >= 25)
        return -1;
    return current_partition_bridge_styles[pi];
}

/* Shared-boundary fallback connector for adjacent partitions.
 * Standard tunnel rules often reject some otherwise-valid joins, so when two
 * partitions have native walkable floor near the same shared boundary we carve
 * a straight doorway/corridor between those two populated sides. */
static bool carve_straight_big_partition_connector(
    int y1, int x1, int y2, int x2, int r1, int r2, int rows, int cols)
{
    int dy = (y2 > y1) ? 1 : (y2 < y1) ? -1 : 0;
    int dx = (x2 > x1) ? 1 : (x2 < x1) ? -1 : 0;

    /* Must be a straight segment. */
    if (!((dy == 0) ^ (dx == 0)))
        return false;

    if (morgoth_segment_blocked(y1, x1, y2, x2, 2))
        return false;

    bool carved = false;
    int y = y1;
    int x = x1;

    for (;;)
    {
        if (!in_bounds_fully(y, x))
            return false;

        if (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT))
            return false;

        int feat = cave_feat[y][x];
        if (feat == FEAT_WALL_PERM)
            return false;

        if (feat == FEAT_WALL_OUTER)
        {
            cave_set_feat(y, x, FEAT_DOOR_HEAD);
            carved = true;
        }
        else if (feat == FEAT_WALL_EXTRA || feat == FEAT_CHASM)
        {
            if (feat == FEAT_CHASM)
            {
                int pi = partition_index_from_point(y, x, rows, cols);
                int bridge_style = partition_bridge_style_for_index(pi);

                if (pi < 0 || pi >= 25 || pi >= current_partition_count
                    || current_partition_modes[pi] != QUAD_MODE_CHASM)
                {
                    return false;
                }

                cave_set_feat_style(y, x, FEAT_FLOOR, bridge_style);
                cave_info[y][x] |= CAVE_CHASM_AREA;
                cave_info[y][x] &= ~CAVE_ROOM;
            }
            else
            {
                cave_set_feat(y, x, FEAT_FLOOR);
            }
            cave_corridor1[y][x] = r1;
            cave_corridor2[y][x] = r2;
            carved = true;
        }
        else if ((feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL))
        {
            /* Don't carve through inner/solid room walls, rubble walls, etc. */
            if (feat != FEAT_WALL_EXTRA)
                return false;
        }
        else if (feature_is_any_door(feat) || feat == FEAT_FLOOR)
        {
            /* Already passable; keep it. */
        }
        else
        {
            /* Avoid unexpected terrain (stairs, traps, etc.) */
            return false;
        }

        if (!(cave_info[y][x] & CAVE_ROOM))
            mark_generation_escape_tunnel(y, x);

        if (y == y2 && x == x2)
            break;
        y += dy;
        x += dx;
    }

    return carved;
}

static bool connect_adjacent_big_partitions_by_boundary(
    int pi_a, int pi_b, const rectangle *bounds_a, const rectangle *bounds_b,
    int rows, int cols, int hub_a, int hub_b, bool vertical_boundary)
{
    const int SEARCH_DEPTH = 32;
    quadrant_mode_t mode_a = current_partition_modes[pi_a];
    quadrant_mode_t mode_b = current_partition_modes[pi_b];

    if (hub_a < 0 || hub_b < 0)
        return false;

    if (!bounds_a || !bounds_b)
        return false;

    if (vertical_boundary)
    {
        /* A is left of B: boundary at the start column of B. */
        int boundary_x = ((pi_b % cols) * p_ptr->cur_map_wid / cols);
        int y_lo = MAX(bounds_a->y1, bounds_b->y1);
        int y_hi = MIN(bounds_a->y2, bounds_b->y2);

        boundary_x = MAX(1, MIN(p_ptr->cur_map_wid - 2, boundary_x));
        if (y_hi - y_lo < 6)
            return false;

        int best_y = -1;
        int best_left = -1;
        int best_right = -1;
        int best_len = 999999;

        for (int y = y_lo + 2; y <= y_hi - 2; ++y)
        {
            int x_left = -1;
            for (int dx = 1; dx <= SEARCH_DEPTH; ++dx)
            {
                int x = boundary_x - dx;
                if (x < bounds_a->x1)
                    break;
                if (partition_index_from_point(y, x, rows, cols) != pi_a)
                    continue;
                if (big_partition_boundary_floor_ok(mode_a, y, x))
                {
                    x_left = x;
                    break;
                }
            }

            int x_right = -1;
            for (int dx = 0; dx <= SEARCH_DEPTH; ++dx)
            {
                int x = boundary_x + dx;
                if (x > bounds_b->x2)
                    break;
                if (partition_index_from_point(y, x, rows, cols) != pi_b)
                    continue;
                if (big_partition_boundary_floor_ok(mode_b, y, x))
                {
                    x_right = x;
                    break;
                }
            }

            if (x_left >= 0 && x_right >= 0 && x_left < x_right)
            {
                int len = x_right - x_left;
                if (len < best_len || (len == best_len && one_in_(2)))
                {
                    best_len = len;
                    best_y = y;
                    best_left = x_left;
                    best_right = x_right;
                }
            }
        }

        if (best_y < 0)
            return false;

        if (!carve_straight_big_partition_connector(
                best_y, best_left, best_y, best_right,
                hub_a, hub_b, rows, cols))
            return false;

        dun->connection[hub_a][hub_b] = true;
        dun->connection[hub_b][hub_a] = true;
        genlog_connect("Partition boundary: carved H link rooms %d<->%d at y=%d x=%d..%d",
            hub_a, hub_b, best_y, best_left, best_right);
        return true;
    }

    /* Horizontal boundary: A is above B. */
    int boundary_y = ((pi_b / cols) * p_ptr->cur_map_hgt / rows);
    int x_lo = MAX(bounds_a->x1, bounds_b->x1);
    int x_hi = MIN(bounds_a->x2, bounds_b->x2);

    boundary_y = MAX(1, MIN(p_ptr->cur_map_hgt - 2, boundary_y));
    if (x_hi - x_lo < 6)
        return false;

    int best_x = -1;
    int best_up = -1;
    int best_down = -1;
    int best_len = 999999;

    for (int x = x_lo + 2; x <= x_hi - 2; ++x)
    {
        int y_up = -1;
        for (int dy = 1; dy <= SEARCH_DEPTH; ++dy)
        {
            int y = boundary_y - dy;
            if (y < bounds_a->y1)
                break;
            if (partition_index_from_point(y, x, rows, cols) != pi_a)
                continue;
            if (big_partition_boundary_floor_ok(mode_a, y, x))
            {
                y_up = y;
                break;
            }
        }

        int y_down = -1;
        for (int dy = 0; dy <= SEARCH_DEPTH; ++dy)
        {
            int y = boundary_y + dy;
            if (y > bounds_b->y2)
                break;
            if (partition_index_from_point(y, x, rows, cols) != pi_b)
                continue;
            if (big_partition_boundary_floor_ok(mode_b, y, x))
            {
                y_down = y;
                break;
            }
        }

        if (y_up >= 0 && y_down >= 0 && y_up < y_down)
        {
            int len = y_down - y_up;
            if (len < best_len || (len == best_len && one_in_(2)))
            {
                best_len = len;
                best_x = x;
                best_up = y_up;
                best_down = y_down;
            }
        }
    }

    if (best_x < 0)
        return false;

    if (!carve_straight_big_partition_connector(
            best_up, best_x, best_down, best_x,
            hub_a, hub_b, rows, cols))
        return false;

    dun->connection[hub_a][hub_b] = true;
    dun->connection[hub_b][hub_a] = true;
    genlog_connect("Partition boundary: carved V link rooms %d<->%d at x=%d y=%d..%d",
        hub_a, hub_b, best_x, best_up, best_down);
    return true;
}

static void seed_partition_adjacency(const int *room_to_part, int part_count, bool adj[25][25], int degree[25])
{
    for (int i = 0; i < part_count; ++i)
        degree[i] = 0;

    for (int i = 0; i < part_count; ++i)
        for (int j = 0; j < part_count; ++j)
            adj[i][j] = false;

    for (int a = 0; a < dun->cent_n; ++a)
    {
        int pa = (a < CENT_MAX) ? room_to_part[a] : -1;
        if (pa < 0 || pa >= part_count) continue;

        for (int b = a + 1; b < dun->cent_n; ++b)
        {
            if (!dun->connection[a][b]) continue;
            int pb = (b < CENT_MAX) ? room_to_part[b] : -1;
            if (pb < 0 || pb >= part_count || pb == pa) continue;
            if (!adj[pa][pb])
            {
                adj[pa][pb] = adj[pb][pa] = true;
                degree[pa]++;
                degree[pb]++;
            }
        }
    }
}

static void mark_partition_edge(int p1, int p2, bool adj[25][25], int degree[25])
{
    if (p1 < 0 || p2 < 0 || p1 >= 25 || p2 >= 25 || p1 == p2)
        return;
    if (!adj[p1][p2])
    {
        adj[p1][p2] = adj[p2][p1] = true;
        degree[p1]++;
        degree[p2]++;
    }
}

static int choose_partition_hub(const partition_link_data_t *part)
{
    int best = -1;
    int best_rank = -1;
    int best_area = -1;
    int best_dist = 999999;

    int limit = MIN(part->room_count, CENT_MAX);
    for (int i = 0; i < limit; ++i)
    {
        int r = part->rooms[i];
        int area = (dun->corner[r].y2 - dun->corner[r].y1 + 1) *
                   (dun->corner[r].x2 - dun->corner[r].x1 + 1);
        int dist = distance(dun->cent[r].y, dun->cent[r].x, part->center.y, part->center.x);
        int rank = room_anchor_requires_neighbor[r] ? 2 :
                   (room_anchor_kind[r] != LAYOUT_ANCHOR_NONE ? 1 : 0);

        if (rank > best_rank ||
            (rank == best_rank && area > best_area) ||
            (rank == best_rank && area == best_area && dist < best_dist))
        {
            best = r;
            best_rank = rank;
            best_area = area;
            best_dist = dist;
        }
    }
    return best;
}

static int find_anchor_target(int src, const int *room_to_part, const bool *skip, int part_count)
{
    int src_part = (src >= 0 && src < CENT_MAX) ? room_to_part[src] : -1;
    int src_piece = (src >= 0 && src < dun->cent_n) ? dun->piece[src] : -1;
    int best = -1;
    int best_tier = 10;
    int best_dist = 999999;

    for (int r = 0; r < dun->cent_n; ++r)
    {
        if (r == src) continue;
        if (skip && skip[r]) continue;
        if (dun->connection[src][r]) continue;

        int tier = 2;
        if (src_piece > 0 && dun->piece[r] > 0 && dun->piece[r] != src_piece)
            tier = 0;
        else if (room_to_part && r < CENT_MAX && room_to_part[r] != src_part)
            tier = 1;

        if (part_count > 0 && room_to_part && (room_to_part[r] < 0 || room_to_part[r] >= part_count))
            continue;

        int dist = distance(dun->cent[src].y, dun->cent[src].x, dun->cent[r].y, dun->cent[r].x);
        if (tier < best_tier || (tier == best_tier && dist < best_dist))
        {
            best_tier = tier;
            best_dist = dist;
            best = r;
        }
    }
    return best;
}

static void connect_anchor_backbone(const int *room_to_part, int part_count)
{
    if (layout_anchor_count <= 0 || dun->cent_n <= 0)
        return;

    (void)dungeon_pieces();

    int anchors_linked = 0;
    int anchors_considered = 0;

    for (int i = 0; i < layout_anchor_count; ++i)
    {
        int r = layout_anchors[i].room_slot;
        if (r < 0 || r >= dun->cent_n)
            continue;

        anchors_considered++;
        int area = (dun->corner[r].y2 - dun->corner[r].y1 + 1) *
                   (dun->corner[r].x2 - dun->corner[r].x1 + 1);
        int target_degree = 1;
        if (layout_anchors[i].requires_neighbor)
            target_degree = 2;
        if (area >= 600)
            target_degree = MAX(target_degree, 2);
        if (area >= 900)
            target_degree = MAX(target_degree, 3);

        int deg = room_connection_degree(r);
        bool tried[CENT_MAX];
        for (int t = 0; t < CENT_MAX; ++t) tried[t] = false;

        int attempts = 0;
        while (deg < target_degree && attempts < 8)
        {
            attempts++;
            int target = find_anchor_target(r, room_to_part, tried, part_count);
            if (target < 0)
                break;

            tried[target] = true;
            if (connect_rooms_with_logging(r, target, "Anchor backbone", true))
            {
                anchors_linked++;
                deg++;
                (void)dungeon_pieces();
            }
        }
    }

    if (anchors_linked > 0)
    {
        genlog_connect("Anchor backbone: linked %d/%d anchors to reduce isolation", anchors_linked, anchors_considered);
    }
}

/* Add connective tissue between partitions by linking a representative room in each partition,
 * then ensure special anchors have multiple exits to avoid dead ends. */
void connect_partition_hubs(void)
{
    int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
    int rows = current_partition_rows;
    int cols = current_partition_cols;
    int count = current_partition_count;

    if (rows <= 0 || cols <= 0) {
        fallback_partition_grid_from_blocks(blocks, &rows, &cols);
        count = rows * cols;
    }
    if (count <= 1 || rows <= 0 || cols <= 0)
        return;

    partition_link_data_t parts[25];
    int room_to_part[CENT_MAX];
    for (int i = 0; i < CENT_MAX; ++i) room_to_part[i] = -1;

    for (int pi = 0; pi < count && pi < 25; ++pi)
    {
        parts[pi].room_count = 0;
        parts[pi].hub_room = -1;
        int y1, y2, x1, x2;
        if (compute_partition_bounds(pi, rows, cols, &y1, &y2, &x1, &x2))
        {
            parts[pi].bounds.y1 = y1;
            parts[pi].bounds.y2 = y2;
            parts[pi].bounds.x1 = x1;
            parts[pi].bounds.x2 = x2;
            parts[pi].center.y = (y1 + y2) / 2;
            parts[pi].center.x = (x1 + x2) / 2;
        }
    }

    for (int r = 0; r < dun->cent_n && r < CENT_MAX; ++r)
    {
        int pi = partition_index_from_point(dun->cent[r].y, dun->cent[r].x, rows, cols);
        room_to_part[r] = pi;
        if (pi < 0 || pi >= count || pi >= 25)
            continue;
        int idx = parts[pi].room_count++;
        if (idx < CENT_MAX)
            parts[pi].rooms[idx] = r;
    }

    for (int pi = 0; pi < count && pi < 25; ++pi)
        parts[pi].hub_room = choose_partition_hub(&parts[pi]);

    bool adj[25][25];
    int degree[25];
    seed_partition_adjacency(room_to_part, count, adj, degree);

    /* Connect adjacent big partitions (labyrinth, big_cave, chasm) FIRST before regular backbone */
    /* This ensures big partitions get priority connections to each other */
    int big_links = 0;
    int big_adjacencies_found = 0;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int idx = row * cols + col;
            if (idx >= count || idx >= 25)
                continue;
            
            quadrant_mode_t mode = current_partition_modes[idx];
            bool is_big = is_big_partition_mode(mode);
            if (!is_big)
                continue;
            
            int hub_here = parts[idx].hub_room;
            if (hub_here < 0)
                continue;
            
            /* Check right neighbor */
            if (col + 1 < cols)
            {
                int idx_r = row * cols + (col + 1);
                if (idx_r < count && idx_r < 25)
                {
                    quadrant_mode_t mode_r = current_partition_modes[idx_r];
                    bool is_big_r = is_big_partition_mode(mode_r);
                    if (is_big_r && !adj[idx][idx_r])
                    {
                        big_adjacencies_found++;
                        int hub_right = parts[idx_r].hub_room;
                        bool ok = false;
                        if (hub_right >= 0)
                        {
                            ok = connect_rooms_with_logging(hub_here, hub_right, "Big partition bridge H", true);
                            if (!ok)
                            {
                                ok = connect_adjacent_big_partitions_by_boundary(
                                    idx, idx_r, &parts[idx].bounds, &parts[idx_r].bounds,
                                    rows, cols, hub_here, hub_right, true);
                            }
                        }

                        if (ok)
                        {
                            mark_partition_edge(idx, idx_r, adj, degree);
                            big_links++;
                            genlog_connect("Big partition bridge: connected %s at [%d,%d] to %s at [%d,%d] (horizontal)",
                                         mode == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col,
                                         mode_r == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode_r == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col + 1);
                        }
                    }
                }
            }
            
            /* Check down neighbor */
            if (row + 1 < rows)
            {
                int idx_d = (row + 1) * cols + col;
                if (idx_d < count && idx_d < 25)
                {
                    quadrant_mode_t mode_d = current_partition_modes[idx_d];
                    bool is_big_d = is_big_partition_mode(mode_d);
                    if (is_big_d && !adj[idx][idx_d])
                    {
                        big_adjacencies_found++;
                        int hub_down = parts[idx_d].hub_room;
                        bool ok = false;
                        if (hub_down >= 0)
                        {
                            ok = connect_rooms_with_logging(hub_here, hub_down, "Big partition bridge V", true);
                            if (!ok)
                            {
                                ok = connect_adjacent_big_partitions_by_boundary(
                                    idx, idx_d, &parts[idx].bounds, &parts[idx_d].bounds,
                                    rows, cols, hub_here, hub_down, false);
                            }
                        }

                        if (ok)
                        {
                            mark_partition_edge(idx, idx_d, adj, degree);
                            big_links++;
                            genlog_connect("Big partition bridge: connected %s at [%d,%d] to %s at [%d,%d] (vertical)",
                                         mode == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col,
                                         mode_d == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode_d == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row + 1, col);
                        }
                    }
                }
            }
        }
    }
    
    if (big_links > 0)
    {
        log_trace("Big partition bridges: added %d connections between adjacent labyrinths/caves/chasms (found %d adjacencies)", 
                  big_links, big_adjacencies_found);
        genlog_connect("Big partition bridges: connected %d pairs of adjacent big partitions", big_links);
    }

    /* Now run regular partition backbone connections */
    int links = 0;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int idx = row * cols + col;
            int hub_here = (idx < 25) ? parts[idx].hub_room : -1;
            if (hub_here < 0)
                continue;

            if (col + 1 < cols)
            {
                int idx_r = row * cols + (col + 1);
                int hub_right = parts[idx_r].hub_room;
                bool ok = false;
                if (hub_right >= 0)
                {
                    ok = connect_rooms_with_logging(hub_here, hub_right, "Partition backbone H", true);
                    if (!ok)
                        ok = connect_adjacent_big_partitions_by_boundary(
                            idx, idx_r, &parts[idx].bounds, &parts[idx_r].bounds,
                            rows, cols, hub_here, hub_right, true);
                }
                if (ok)
                {
                    mark_partition_edge(idx, idx_r, adj, degree);
                    links++;
                }
            }

            if (row + 1 < rows)
            {
                int idx_d = (row + 1) * cols + col;
                int hub_down = parts[idx_d].hub_room;
                bool ok = false;
                if (hub_down >= 0)
                {
                    ok = connect_rooms_with_logging(hub_here, hub_down, "Partition backbone V", true);
                    if (!ok)
                        ok = connect_adjacent_big_partitions_by_boundary(
                            idx, idx_d, &parts[idx].bounds, &parts[idx_d].bounds,
                            rows, cols, hub_here, hub_down, false);
                }
                if (ok)
                {
                    mark_partition_edge(idx, idx_d, adj, degree);
                    links++;
                }
            }

            if (col + 1 < cols && row + 1 < rows)
            {
                int idx_dr = (row + 1) * cols + (col + 1);
                int hub_diag = parts[idx_dr].hub_room;
                if (hub_diag >= 0 && connect_rooms_with_logging(hub_here, hub_diag, "Partition backbone D", true))
                {
                    mark_partition_edge(idx, idx_dr, adj, degree);
                    links++;
                }
            }
        }
    }

    int target_degree = (count >= 3) ? 2 : 1;
    for (int pi = 0; pi < count && pi < 25; ++pi)
    {
        if (parts[pi].hub_room < 0)
            continue;
        if (degree[pi] >= target_degree)
            continue;

        int attempts = 0;
        bool failed_candidate[25] = {false};
        while (degree[pi] < target_degree && attempts < count)
        {
            attempts++;
            int best = -1;
            int best_dist = 999999;
            for (int pj = 0; pj < count && pj < 25; ++pj)
            {
                if (pj == pi) continue;
                if (parts[pj].hub_room < 0) continue;
                if (adj[pi][pj]) continue;
                if (failed_candidate[pj]) continue;
                int dist = distance(parts[pi].center.y, parts[pi].center.x, parts[pj].center.y, parts[pj].center.x);
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best = pj;
                }
            }

            if (best < 0)
                break;

            if (connect_rooms_with_logging(parts[pi].hub_room, parts[best].hub_room, "Partition backbone fill", true))
            {
                mark_partition_edge(pi, best, adj, degree);
                links++;
            }
            else
            {
                failed_candidate[best] = true;
            }
        }
    }

    if (links > 0)
        log_trace("Partition hub pass: added %d backbone links (grid %dx%d)", links, rows, cols);

    connect_anchor_backbone(room_to_part, count);
}

/* Anchor-aware connector: link nearby anchors to reduce isolation without over-saturating tunnels. */
/* Repair all outer walls after generation - critical for tunnel connectivity.
 * This fixes cases where overlapping room/cave generation overwrote WALL_OUTER
 * tiles back to WALL_EXTRA, breaking tunnel connection logic. */
static void repair_all_outer_walls(void)
{
    int repaired = 0;
    
    /* Scan entire map for wall tiles that border CAVE_ROOM floor */
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            /* Skip if already floor or already outer wall */
            if (cave_floor_bold(y, x))
                continue;
            if (cave_feat[y][x] == FEAT_WALL_OUTER)
                continue;
            if (cave_feat[y][x] != FEAT_WALL_EXTRA)
                continue;
            
            /* Check if this wall borders any CAVE_ROOM floor */
            bool borders_room_floor = false;
            for (int dy = -1; dy <= 1 && !borders_room_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_room_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = y + dy, nx = x + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_room_floor = true;
                    }
                }
            }
            
            if (borders_room_floor)
            {
                cave_set_feat(y, x, FEAT_WALL_OUTER);
                repaired++;
            }
        }
    }
    
    if (repaired > 0)
    {
        log_trace("repair_all_outer_walls: converted %d WALL_EXTRA to WALL_OUTER", repaired);
    }
}

/* Fallback builder to guarantee the minimum room count before connectivity work */
static void ensure_minimum_rooms(void)
{
    if (dun->cent_n >= room_capacity_limit())
        return;
    if (dun->cent_n >= ROOM_MIN)
        return;

    int before = dun->cent_n;
    /* Try a mix of simple rooms near the centre to avoid hard failures */
    for (int attempt = 0; attempt < 50 && dun->cent_n < ROOM_MIN && dun->cent_n < room_capacity_limit(); ++attempt)
    {
        int y = rand_range(4, p_ptr->cur_map_hgt - 4);
        int x = rand_range(4, p_ptr->cur_map_wid - 4);

        /* Alternate basic shapes to improve odds in cramped layouts */
        if (attempt % 3 == 0)
            build_type1(y, x);
        else if (attempt % 3 == 1)
            build_type2(y, x);
        else
            build_type6(y, x, false);
    }

    if (dun->cent_n > before)
    {
        log_trace("Room fallback: added %d emergency rooms (now %d)", dun->cent_n - before, dun->cent_n);
    }
}


static bool cave_gen(void)
{
    int i;

    int l;

    int y, x;

    int room_attempts = 0;

    int is_guaranteed_forge_level = false;
    bool duruin_bastion_forced = false;
    bool shadow_bastion_forced = false;
    bool tulkas_stronghold_forced = false;
    bool is_morgoth_level = (p_ptr->depth == MORGOTH_DEPTH);
    byte varda_shadow_state;

    reset_morgoth_layout_state(is_morgoth_level);
    
    /* Reset labyrinth partition counter for this level */
    current_labyrinth_partitions = 0;
    
    /* Reset quest vault monitoring variables for this level */
    qv_placed_this_level = false;
    qv_stored_y1 = qv_stored_x1 = qv_stored_y2 = qv_stored_x2 = -1;
    
    /* Run quest lottery once per level to determine which quest (if any) gets this level */
    if (is_morgoth_level) {
        quest_lottery_winner = 0;
    } else {
        run_quest_lottery();
    }
    varda_shadow_state = quest_get_state(QUEST_ID_VARDA_SHADOW);
    
    /* Debug: Log entry into cave_gen */
    log_trace("cave_gen: Starting level generation (quest_vault_used=%s, lottery_winner=%d)", 
              p_ptr->quest_vault_used ? "true" : "false", quest_lottery_winner);
    
    /* Varda quest reserves the run to avoid other quest content until complete */
    if (!is_morgoth_level && p_ptr->varda_quest >= VARDA_QUEST_ACTIVE && !p_ptr->quest_reserved[0]) {
        p_ptr->quest_reserved[0] = 1;
        log_trace("Varda quest: === QUEST SLOT RESERVED === Active Varda quest reserves slot (state=%d)", p_ptr->varda_quest);
    }
    
    log_trace("cave_gen: Quest status at level start - quest_reserved[0]=%d, varda_quest=%d, lottery_winner=%d",
              p_ptr->quest_reserved[0], p_ptr->varda_quest, quest_lottery_winner);
    
    /* Varda quest: flag forced bastion placement on first level deeper than 500ft */
    if (!is_morgoth_level && p_ptr->varda_quest == VARDA_QUEST_ACTIVE && !p_ptr->varda_vault_placed && p_ptr->depth > 10) {
        if (!p_ptr->varda_vault_ready) {
            log_trace("Varda quest: Crossing 500ft, setting bastion_ready at depth %d", p_ptr->depth);
        }
        p_ptr->varda_vault_ready = 1;
    }
    if (!is_morgoth_level && varda_shadow_state == QUEST_STATE_ACTIVE && !p_ptr->varda_shadow_placed && p_ptr->depth > 15) {
        if (!p_ptr->varda_shadow_ready) {
            log_trace("Varda shadow quest: Crossing 750ft, setting shadow_bastion_ready at depth %d", p_ptr->depth);
        }
        p_ptr->varda_shadow_ready = 1;
    }
    s16b mon_gen, obj_room_gen;
    memset(dun, 0, sizeof(*dun));

    /* Sil - determine the dungeon size */
    /* Generate square levels: 4*11 to 15*11 (44x44 to 165x165) */
    /* Probability increases with depth, larger sizes more probable */
    
    // Base size: 9 blocks (increased from 7 for larger level sizes)
    // Size increases with depth, with bias toward larger sizes
    // Formula: Use multiple dice rolls and take the maximum (biases upward)
    // Two independent uniform rolls: X1 = dieroll(17) (1..17), X2 = dieroll(14) (1..14)
    int base_size = 9;  // Increased from 7 for larger starting levels
    int depth_factor = p_ptr->depth + dieroll(17);  // Higher ceiling (1-17)
    int bonus1 = depth_factor / 3;  // First roll (uses X1)
    int bonus2 = (p_ptr->depth + dieroll(14)) / 3;  // Second roll (uses X2)
    int depth_bonus = MAX(bonus1, bonus2);  // Take maximum (biases larger)
    
    l = base_size + depth_bonus;
    if (l > MAX_LEVEL_BLOCKS) l = MAX_LEVEL_BLOCKS;  // Hard cap at MAX_LEVEL_BLOCKS
    if (l < 8) l = 8;    // Hard floor at 8 blocks (88x88)

    if (smaller_level_size)
    {
        l -= 3;
        if (l < 6) l = 6; /* Allow 6x6 and 7x7 block maps */
    }

    // Square levels: same dimension for both height and width
    p_ptr->cur_map_hgt = l * (PANEL_HGT);
    p_ptr->cur_map_wid = l * (PANEL_HGT);  // Use PANEL_HGT for both to make square

    /* Fewer room attempts to reduce long regen loops; vault bias handled later */
    room_attempts = l * l * l * 2;
    log_trace("cave_gen: SQUARE map size set to %dx%d (l=%d blocks) room_attempts=%d", 
              p_ptr->cur_map_wid, p_ptr->cur_map_hgt, l, room_attempts);
    
    /* Generation log: level start */
    gen_log_level_start(p_ptr->depth, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
    genlog_summary("Level %d generation starting: %dx%d map (%d blocks), %d room attempts",
                   p_ptr->depth, p_ptr->cur_map_hgt, p_ptr->cur_map_wid, l, room_attempts);
    genlog_quest("Quest lottery winner=%d, quest_vault_used=%s, varda_quest=%d",
                 quest_lottery_winner, p_ptr->quest_vault_used ? "yes" : "no", p_ptr->varda_quest);
    {
        char detail[160];
        strnfmt(detail, sizeof(detail), "%dx%d map, %d blocks, %d room attempts",
            p_ptr->cur_map_hgt, p_ptr->cur_map_wid, l, room_attempts);
        level_gen_screen_set_stage(LEVEL_GEN_STAGE_PLANNING, detail);
    }

    /* Initialize level style weights and start with basic granite */
    level_gen_screen_set_stage(LEVEL_GEN_STAGE_FOUNDATIONS,
        "Resetting styles, granite, and connection tables.");
    styles_init_for_level();
    /*start with basic granite*/
    basic_granite();
    log_trace("cave_gen: after styles_init/basic_granite");

    log_trace("cave_gen: before connection table init (DUN_ROOMS=%d, conn_size=%zu)", DUN_ROOMS, sizeof(dun->connection));
    /* Initialize the connection table */
    for (y = 0; y < DUN_ROOMS; y++)
    {
        if (y == 0 || y == DUN_ROOMS - 1)
            log_trace("cave_gen: init conn row %d start", y);
        for (x = 0; x < DUN_ROOMS; x++)
        {
            dun->connection[y][x] = false;
        }
        log_trace("cave_gen: connection init row=%d done", y);
    }
    log_trace("cave_gen: after connection table init");

    /* No rooms yet */
    dun->cent_n = 0;
    log_trace("cave_gen: cent_n reset to 0");
    layout_anchor_reset();

    /* Verify dun struct sanity */
    log_trace("cave_gen: sanity check dun ptr=%p cent capacity=%d connection[0][0]=%d piece[0]=%d corner[0]=(y1=%d,x1=%d,y2=%d,x2=%d)",
        (void*)dun, DUN_ROOMS, dun->connection[0][0], dun->piece[0],
        dun->corner[0].y1, dun->corner[0].x1, dun->corner[0].y2, dun->corner[0].x2);

    if (cheat_room)
        msg_format("Forge count is %d.", p_ptr->forge_count);

    // guarantee a forge at first entrance to levels 2, 6, 10 (or below if skipped via shaft)
    if (p_ptr->fixed_forge_count < 3)
    {
        int next_guaranteed_forge_level = 2 + (p_ptr->fixed_forge_count * 4);
        is_guaranteed_forge_level = (next_guaranteed_forge_level <= p_ptr->depth);
        log_trace("Forge forcing check: fixed_forge_count=%d, target_level=%d, current_depth=%d, forcing=%s", 
                 p_ptr->fixed_forge_count, next_guaranteed_forge_level, p_ptr->depth, 
                 is_guaranteed_forge_level ? "true" : "false");
    }

    if (cheat_room)
        msg_format("Guaranteed forge: %s.",
            is_guaranteed_forge_level ? "true" : "false");

    log_trace("cave_gen: before guaranteed forge handling");
    if (is_guaranteed_forge_level)
    {
        int y = rand_range(5, p_ptr->cur_map_hgt - 5);
        int x = rand_range(5, p_ptr->cur_map_wid - 5);
        log_trace("cave_gen: attempting guaranteed forge at (%d,%d)", y, x);

        if (cheat_room)
            msg_format("Trying to force a forge:");
        p_ptr->force_forge = true;
        p_ptr->fixed_forge_count++;
        log_trace("cave_gen: force_forge=true, fixed_forge_count=%d", p_ptr->fixed_forge_count);

        if (!build_type6(y, x, true))
        {
            if (cheat_room)
                msg_format("failed.");

            p_ptr->fixed_forge_count--;
            return (false);
        }

        if (cheat_room)
            msg_format("succeeded.");
    }
    log_trace("cave_gen: post guaranteed-forge path cent_n=%d", dun->cent_n);
    log_trace("cave_gen: post guaranteed-forge path cent_n=%d", dun->cent_n);

    if (!is_morgoth_level)
    {
        /* Quest vault determination - Allow re-placement during level regeneration */
        log_trace("Quest vault: ENTERING quest vault logic check (quest_vault_used=%s, force_forge=%s, qv_placed_this_level=%s)", 
                  p_ptr->quest_vault_used ? "true" : "false", 
                  p_ptr->force_forge ? "true" : "false",
                  qv_placed_this_level ? "true" : "false");
        log_trace("Quest vault: Starting quest vault check (quest_vault_used=%s, force_forge=%s)", 
                  p_ptr->quest_vault_used ? "true" : "false", 
                  p_ptr->force_forge ? "true" : "false");
        
        /* If Varda's quest is active and the bastion is due, force its placement first */
        log_trace("Quest vault check: varda_vault_ready=%d, varda_quest=%d (ACTIVE=%d), varda_vault_placed=%d",
                  p_ptr->varda_vault_ready, p_ptr->varda_quest, VARDA_QUEST_ACTIVE, p_ptr->varda_vault_placed);
        
        if (p_ptr->varda_vault_ready && p_ptr->varda_quest == VARDA_QUEST_ACTIVE && !p_ptr->varda_vault_placed) {
            log_trace("Quest vault: === DURUIN BASTION FORCE PLACEMENT === Starting at depth %d", p_ptr->depth);
            if (!place_duruin_bastion()) {
                log_trace("Quest vault: === DURUIN BASTION FAILED === Regenerating level");
                return false;
            }
            log_trace("Quest vault: === DURUIN BASTION SUCCESS === Placed successfully");
            duruin_bastion_forced = true;
        } else if (p_ptr->varda_quest == VARDA_QUEST_ACTIVE) {
            log_trace("Quest vault: Varda quest ACTIVE but bastion not ready (vault_ready=%d, vault_placed=%d)",
                      p_ptr->varda_vault_ready, p_ptr->varda_vault_placed);
        }

        log_trace("Quest vault check: shadow_ready=%d, shadow_state=%d, shadow_placed=%d",
                  p_ptr->varda_shadow_ready, varda_shadow_state, p_ptr->varda_shadow_placed);
        if (p_ptr->varda_shadow_ready && varda_shadow_state == QUEST_STATE_ACTIVE && !p_ptr->varda_shadow_placed) {
            log_trace("Quest vault: === SHADOW BASTION FORCE PLACEMENT === Starting at depth %d", p_ptr->depth);
            if (!place_shadow_bastion()) {
                log_trace("Quest vault: === SHADOW BASTION FAILED === Regenerating level");
                return false;
            }
            log_trace("Quest vault: === SHADOW BASTION SUCCESS === Placed successfully");
            shadow_bastion_forced = true;
        } else if (varda_shadow_state == QUEST_STATE_ACTIVE) {
            log_trace("Quest vault: Varda shadow quest ACTIVE but bastion not ready (vault_ready=%d, vault_placed=%d)",
                      p_ptr->varda_shadow_ready, p_ptr->varda_shadow_placed);
        }

        if (p_ptr->tulkas_stronghold_level > 0 &&
            p_ptr->depth == p_ptr->tulkas_stronghold_level &&
            !p_ptr->tulkas_stronghold_placed) {
            log_trace("Quest vault: === ORC STRONGHOLD FORCE PLACEMENT === Starting at depth %d", p_ptr->depth);
            if (!place_orc_stronghold()) {
                log_trace("Quest vault: === ORC STRONGHOLD FAILED === Regenerating level");
                return false;
            }
            tulkas_stronghold_forced = true;
            p_ptr->quest_reserved[0] = 1;
            log_trace("Quest vault: === ORC STRONGHOLD SUCCESS === Placed successfully");
        }
                  
        /* QUEST VAULT REGENERATION FIX: Allow quest vault re-placement during regeneration */
        /* Quest vaults can be placed if: */
        /* 1. quest_vault_used is false (haven't successfully completed a quest vault this run), OR */
        /* 2. We're in a regeneration scenario (quest vault was placed before but level failed) */
        if (!p_ptr->quest_vault_used && !duruin_bastion_forced && !shadow_bastion_forced && !tulkas_stronghold_forced)
        {
            /* QUEST VAULT REGENERATION FIX: Remove the quest_vault_attempted_this_level check */
            /* to allow quest vault re-placement during level regeneration */
            
            /* Check if any quest is already active - ONE QUEST PER RUN ENFORCEMENT */
            log_trace("Quest vault: Checking one-quest-per-run enforcement:");
            log_trace("Quest vault:   quest_reserved[0]=%d (should block if 1)", p_ptr->quest_reserved[0]);
            log_trace("Quest vault:   tulkas=%d, tulkas_orcs=%d, mandos=%d, aule=%d, varda=%d, shadow=%d, lottery_winner=%d",
                      p_ptr->tulkas_quest, quest_get_state(QUEST_ID_TULKAS_ORCS), p_ptr->mandos_quest, p_ptr->aule_quest,
                      p_ptr->varda_quest, varda_shadow_state, quest_lottery_winner);
            
            if (p_ptr->quest_reserved[0] || 
                quest_lottery_winner > 0 ||
                p_ptr->tulkas_quest != TULKAS_QUEST_NOT_STARTED ||
                quest_get_state(QUEST_ID_TULKAS_ORCS) != QUEST_STATE_NOT_STARTED ||
                p_ptr->tulkas_stronghold_level > 0 ||
                p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED ||
                p_ptr->aule_quest != AULE_QUEST_NOT_STARTED ||
                p_ptr->varda_quest != VARDA_QUEST_NOT_STARTED ||
                varda_shadow_state != QUEST_STATE_NOT_STARTED) {
                log_trace("Quest vault: === BLOCKED === This level already belongs to another quest (tulkas=%d, tulkas_orcs=%d, stronghold=%d, mandos=%d, aule=%d, varda=%d, shadow=%d, reserved=%d, lottery_winner=%d)", 
                         p_ptr->tulkas_quest, quest_get_state(QUEST_ID_TULKAS_ORCS), p_ptr->tulkas_stronghold_level, p_ptr->mandos_quest, p_ptr->aule_quest,
                         p_ptr->varda_quest, varda_shadow_state, p_ptr->quest_reserved[0], quest_lottery_winner);
                /* Don't place any quest vaults - skip to end */
            } else {
                int quest_vault_roll;

                if (cached_quest_vault_roll >= 0)
                {
                    quest_vault_roll = cached_quest_vault_roll;
                    log_trace("Quest vault: Reusing locked roll = %d", quest_vault_roll);
                }
                else
                {
                    quest_vault_roll = dieroll(p_ptr->depth + 5);
                    log_trace("Quest vault: Level determination roll = %d", quest_vault_roll);

                    if (one_in_(5))
                    {
                        int bonus = dieroll(5);
                        quest_vault_roll += bonus;
                        log_trace("Quest vault: Bonus roll (+%d) = %d total", bonus, quest_vault_roll);
                    }

                    cached_quest_vault_roll = quest_vault_roll;
                }

                bool quest_vault_placed = false;
                bool had_eligible_quest_vault = false;
                
                if (quest_vault_roll >= 18)
                {
                    bool type8_eligible = false;
                    bool type7_eligible = false;
                    bool type6_eligible = false;

                    log_trace("Quest vault: Hit greater vault threshold (%d >= 18), trying quest vaults 8->7->6", quest_vault_roll);
                    quest_vault_placed = try_quest_vault_type(8, &type8_eligible)
                        || try_quest_vault_type(7, &type7_eligible)
                        || try_quest_vault_type(6, &type6_eligible);
                    had_eligible_quest_vault = type8_eligible || type7_eligible || type6_eligible;
                    
                    if (!quest_vault_placed && had_eligible_quest_vault) {
                        log_trace("Quest vault: === FAILED TO PLACE REQUIRED QUEST VAULT === Regenerating level");
                        genlog_fail("QUEST VAULT FAILED: required (roll=%d >= 18), could not place type 8/7/6 '%s' - regenerating",
                            quest_vault_roll,
                            level_gen_debug_last_quest_vault_name[0]
                                ? level_gen_debug_last_quest_vault_name
                                : "(unknown)");
                        gen_log_level_end(false, dun->cent_n, 1);
                        return false; /* Force regeneration to guarantee quest vault spawns */
                    }
                    if (!quest_vault_placed) {
                        log_trace("Quest vault: Roll reserved 8/7/6, but no eligible quest vault exists for this character/run");
                    }
                }
                else if (quest_vault_roll >= 13)
                {
                    bool type7_eligible = false;
                    bool type6_eligible = false;

                    log_trace("Quest vault: Hit lesser vault threshold (%d >= 13), trying quest vaults 7->6", quest_vault_roll);
                    quest_vault_placed = try_quest_vault_type(7, &type7_eligible)
                        || try_quest_vault_type(6, &type6_eligible);
                    had_eligible_quest_vault = type7_eligible || type6_eligible;
                    
                    if (!quest_vault_placed && had_eligible_quest_vault) {
                        log_trace("Quest vault: === FAILED TO PLACE REQUIRED QUEST VAULT === Regenerating level");
                        genlog_fail("QUEST VAULT FAILED: required (roll=%d >= 13), could not place type 7/6 '%s' - regenerating",
                            quest_vault_roll,
                            level_gen_debug_last_quest_vault_name[0]
                                ? level_gen_debug_last_quest_vault_name
                                : "(unknown)");
                        gen_log_level_end(false, dun->cent_n, 1);
                        return false; /* Force regeneration to guarantee quest vault spawns */
                    }
                    if (!quest_vault_placed) {
                        log_trace("Quest vault: Roll reserved 7/6, but no eligible quest vault exists for this character/run");
                    }
                }
                else if (quest_vault_roll >= 8)
                {
                    bool type6_eligible = false;

                    log_trace("Quest vault: Hit interesting room threshold (%d >= 8), trying quest vault 6", quest_vault_roll);
                    quest_vault_placed = try_quest_vault_type(6, &type6_eligible);
                    had_eligible_quest_vault = type6_eligible;
                    
                    if (!quest_vault_placed && had_eligible_quest_vault) {
                        log_trace("Quest vault: === FAILED TO PLACE REQUIRED QUEST VAULT === Regenerating level");
                        genlog_fail("QUEST VAULT FAILED: required (roll=%d >= 8), could not place type 6 '%s' - regenerating",
                            quest_vault_roll,
                            level_gen_debug_last_quest_vault_name[0]
                                ? level_gen_debug_last_quest_vault_name
                                : "(unknown)");
                        gen_log_level_end(false, dun->cent_n, 1);
                        return false; /* Force regeneration to guarantee quest vault spawns */
                    }
                    if (!quest_vault_placed) {
                        log_trace("Quest vault: Roll reserved type 6, but no eligible quest vault exists for this character/run");
                    }
                }
                else
                {
                    log_trace("Quest vault: Roll too low (%d < 8), no quest vault this level", quest_vault_roll);
                }
                
                if (quest_vault_placed)
                {
                    log_trace("Quest vault: Successfully placed quest vault, no more quest vaults this run");
                }
                else
                {
                    log_trace("Quest vault: No quest vault placed this level");
                }
            }
        }
        else if (p_ptr->varda_quest >= VARDA_QUEST_ACTIVE)
        {
            log_trace("Quest vault: === VARDA QUEST BLOCKS === No other quest vaults allowed while Varda quest active (state=%d)", p_ptr->varda_quest);
        }
        else if (duruin_bastion_forced || shadow_bastion_forced || tulkas_stronghold_forced)
        {
            log_trace("Quest vault: Bespoke quest vault already placed, skipping other quest vault attempts this level");
        }
        else
        {
            log_trace("Quest vault: Already used this run, skipping quest vault check (quest_vault_used=1)");
        }
    }

    /* Seed a handful of prefab anchors up front to diversify layout */
    level_gen_screen_set_stage(LEVEL_GEN_STAGE_SHAPING,
        "Generating partitions, rooms, and special areas.");
    seed_prefab_anchors();
    /* Apply quadrant generation modes - this is now the primary room generation */
    apply_quadrant_generation_modes();
    /* DISABLED: ensure_partition_connectivity() was creating dead-end corridors.
     * The corridor system and rescue tunnels handle connectivity instead. */
    /* Repair all outer walls - critical fix for tunnel connectivity after overlapping generation */
    repair_all_outer_walls();

    if (!morgoth_level_active && cached_gv_level_roll_allowed
        && (g_vault_name[0] == '\0'))
    {
        genlog_fail("GV FAILED: reserved a greater vault for this level but '%s' was not placed",
            level_gen_debug_last_greater_vault_name[0]
                ? level_gen_debug_last_greater_vault_name
                : "(unknown)");
        return false;
    }

    /* Verify Morgoth's throne room was placed (should have been done in apply_quadrant_generation_modes) */
    if (morgoth_level_active && !morgoth_partition_reserved)
    {
        log_trace("Morgoth level: throne room was not placed during partition generation");
        return false;
    }

    /* Room saturation loop DISABLED - partition system handles room generation
     * The old approach saturated the map with random rooms which conflicted with
     * the partition-based generation that already creates themed areas. */
#if 0
    /* Build some rooms */
    int failed_in_row = 0;
    for (i = 0; i < room_attempts; i++)
    {
        int r = dieroll(p_ptr->depth + 5);
        log_trace("Room generation: depth+5 roll = %d", r);

        if (one_in_(5))
        {
            int bonus = dieroll(5);
            r += bonus;
            log_trace("Room generation: bonus roll (+%d) = %d total", bonus, r);
        }

        // choose a room type based on the level (bias toward vaults)
        if ((r < 4) || one_in_(3))
        {
            // standard room
            log_trace("Room generation: Building standard room (r=%d)", r);
            if (!room_build(1))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else if (r < 7)
        {
            // cross room
            log_trace("Room generation: Building cross room (r=%d)", r);
            if (!room_build(2))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else if ((r < 14) || one_in_(2))
        {
            // interesting room
            log_trace("Room generation: Building interesting room (r=%d)", r);
            if (!room_build(6))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else if (r < 19)
        {
            // lesser vault
            log_trace("Room generation: Building lesser vault (r=%d)", r);
            if (!room_build(7))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else
        {
            // greater vault
            log_trace("Room generation: Building greater vault (r=%d)", r);
            if (!room_build(8))
                failed_in_row++;
            else
                failed_in_row = 0;
        }

        // stop if there are too many rooms
        if (dun->cent_n >= room_capacity_limit())
            break;

        // bail out if we are not making progress to avoid infinite loops
        if (failed_in_row > 200)
        {
            log_trace("Room generation: aborting after %d consecutive failures (cent_n=%d)", failed_in_row, dun->cent_n);
            break;
        }
    }
#endif

    /*set the permanent walls*/
    set_perm_boundry();

    /* Post-partition seeders DISABLED - partition system already handles these
     * CA blob and BSP slice anchors were duplicating work the partitions do */
#if 0
    /* Carve CA blob anchors into remaining granite */
    seed_ca_blob_anchors();
    /* Add BSP-slice anchors for rectangular-but-offset caverns */
    seed_bsp_slice_anchors();
#endif

    /* If generation stalled, force a couple of simple rooms to avoid regen loops */
    ensure_minimum_rooms();

    layout_anchor_capture_existing_rooms();

    /* Log final room count for debugging */
    log_trace("Room generation completed: %d rooms generated (quest_vault_placed=%s)", 
              dun->cent_n, qv_placed_this_level ? "true" : "false");

    /*start over on all levels with less than two rooms due to inevitable
     * crash*/
    /* QUEST VAULT FIX: Use original room requirement, quest vault regeneration will be handled differently */
    if (dun->cent_n < ROOM_MIN)
    {
        if (cheat_room)
            msg_format("Not enough rooms (%d < %d).", dun->cent_n, ROOM_MIN);
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: Only %d rooms generated, minimum %d required", dun->cent_n, ROOM_MIN);
        genlog_fail("NOT ENOUGH ROOMS: %d generated, minimum %d required", dun->cent_n, ROOM_MIN);
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    /* DEBUGGING: Check if quest vault still exists after room generation */
    check_quest_vault_integrity("AFTER_ROOM_GENERATION");

    /* make the tunnels */
    /* Sil - This has been changed considerably */
    level_gen_screen_set_stage(LEVEL_GEN_STAGE_LINKING,
        "Connecting rooms and validating access.");
    if (!connect_rooms_stairs())
    {
        if (cheat_room)
            msg_format("Couldn't connect the rooms.");
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: connect_rooms_stairs() returned false");
        genlog_fail("CONNECTIVITY FAILED: connect_rooms_stairs() could not link rooms (rooms=%d)", dun->cent_n);
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    /* DEBUGGING: Check if quest vault still exists after tunnel making */
    check_quest_vault_integrity("AFTER_TUNNEL_GENERATION");

    if (morgoth_level_active && !connect_morgoth_entry_tunnels())
    {
        if (cheat_room)
            msg_format("Morgoth entry tunnels failed to connect.");
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: connect_morgoth_entry_tunnels() returned false");
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    /* randomise the doors (except those in vaults) */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if ((cave_feat[y][x] == FEAT_DOOR_HEAD)
                && !(cave_info[y][x] & (CAVE_ICKY)))
            {
                if (one_in_(4))
                    cave_set_feat(y, x, FEAT_FLOOR);
                else
                    place_random_door(y, x);
            }
        }
    squash_double_doors();

    if (morgoth_level_active)
    {
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if ((cave_feat[y][x] == FEAT_MORE)
                    || (cave_feat[y][x] == FEAT_MORE_SHAFT))
                {
                    cave_set_feat(y, x, FEAT_LESS);
                }
            }
        }
    }

    /* DEBUGGING: Check if quest vault still exists after door randomization */
    check_quest_vault_integrity("AFTER_DOOR_RANDOMIZATION");

    /* place the stairs, traps, rubble, secret doors, and player */
    level_gen_screen_set_stage(LEVEL_GEN_STAGE_ENTRY,
        "Placing stairs, rubble, doors, and player start.");
    if (!place_rubble_player())
    {
        if (cheat_room)
            msg_format("Couldn't place, rubble, or player.");
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: place_rubble_player() returned false");
        genlog_fail("PLACEMENT FAILED: place_rubble_player() could not place stairs/player");
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    if (p_ptr->depth == 1 && p_ptr->stairs_taken == 0)
        make_patches_of_sunlight();

    // check dungeon connectivity
    if (!check_connectivity())
    {
        if (cheat_room)
            msg_format("Failed connectivity.");
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: check_connectivity() returned false");
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    {
        partition_population_plan plans[PARTITION_META_MAX];
        int plan_count = build_partition_population_plans(plans, PARTITION_META_MAX);
        int obj_corr_gen = 0;
        int special_scatter_placed;
        int room_objects_placed;
        int corr_objects_placed;
        int monsters_placed;

        obj_room_gen = 0;
        mon_gen = 0;

        level_gen_screen_set_stage(LEVEL_GEN_STAGE_TREASURE,
            "Placing objects, treasure, and traps.");

        for (int pi = 0; pi < plan_count; ++pi)
        {
            obj_room_gen += plans[pi].room_objects;
            obj_corr_gen += plans[pi].corr_objects;
            if (!partition_monster_pass_skips_plan(&plans[pi]))
                mon_gen += plans[pi].monsters_total;

            log_trace(
                "Partition plan: pi=%d mode=%d rooms=%d floors=%d room_floors=%d corridor_floors=%d base_mon=%d floor_mon=%d depth_mon=%d precurse_mon=%d curse_mon=%d total_mon=%d room_obj=%d corr_obj=%d",
                plans[pi].pi, plans[pi].mode, plans[pi].room_centers,
                plans[pi].floor_count, plans[pi].room_floor_count,
                plans[pi].corridor_floor_count, plans[pi].monsters_base,
                plans[pi].monsters_floor, plans[pi].monsters_depth,
                plans[pi].monsters_precurse, plans[pi].monsters_curse_bonus,
                plans[pi].monsters_total, plans[pi].room_objects,
                plans[pi].corr_objects);
        }

        special_scatter_placed = run_partition_special_scatter_pass(plans, plan_count);
        room_objects_placed = run_partition_object_pass(plans, plan_count, true);
        corr_objects_placed = run_partition_object_pass(plans, plan_count, false);

        log_trace("Room objects: target=%d placed=%d", obj_room_gen, room_objects_placed);
        log_trace("Corridor objects: target=%d placed=%d", obj_corr_gen, corr_objects_placed);
        log_trace("Special scatter placements: %d", special_scatter_placed);

        /* Keep trap placement ahead of monsters, matching the old occupancy order. */
        place_traps();

        level_gen_screen_set_stage(LEVEL_GEN_STAGE_MONSTERS,
            "Placing the monster population.");
        monsters_placed = run_partition_monster_pass(plans, plan_count);
        log_trace("Partition monster pass: target=%d placed=%d", mon_gen, monsters_placed);
    }

    level_gen_screen_set_stage(LEVEL_GEN_STAGE_FINALIZING,
        "Final quest, boss, and success checks.");
    
    /* Check for Varda quest spawning - lottery-based */
    log_trace("Varda spawn check: lottery_winner=%d (QUEST_ID_VARDA=%d), depth=%d, varda_quest=%d, shadow_state=%d", 
              quest_lottery_winner, QUEST_ID_VARDA, p_ptr->depth, p_ptr->varda_quest, varda_shadow_state);
    
    if (quest_lottery_winner == QUEST_ID_VARDA || quest_lottery_winner == QUEST_ID_VARDA_SHADOW) {
        bool shadow_quest = (quest_lottery_winner == QUEST_ID_VARDA_SHADOW);
        log_trace("Varda spawn: === VARDA WON LOTTERY === Attempting spawn at depth %d for quest %d", p_ptr->depth, quest_lottery_winner);
        log_trace("Varda spawn: Current state - varda_quest=%d, shadow_state=%d, quest_reserved[0]=%d", 
                  p_ptr->varda_quest, varda_shadow_state, p_ptr->quest_reserved[0]);
        
        /* Safety: enforce early-depth requirement even if data is misconfigured */
        if (p_ptr->depth > 3) {
            log_trace("Varda spawn: FAILED - depth %d exceeds allowed range 1-3", p_ptr->depth);
            genlog_quest("VARDA SPAWN FAILED: depth %d > 3, forcing regeneration", p_ptr->depth);
            gen_log_level_end(false, dun->cent_n, 1);
            return false; /* Force regeneration until early depth is honored */
        }
        
        /* Ensure there is at least some sunlight on the level */
        log_trace("Varda spawn: Ensuring sunlight exists on level");
        ensure_sunlight_for_varda();
        log_trace("Varda spawn: Sunlight check complete");
        
        /* Check if Varda already exists on this level */
        log_trace("Varda spawn: Checking if Varda already exists on this level (mon_max=%d)", mon_max);
        bool varda_exists = false;
        for (int j = 1; j < mon_max; j++)
        {
            monster_type *m_ptr = &mon_list[j];
            if (m_ptr->r_idx == R_IDX_VARDA)
            {
                varda_exists = true;
                log_trace("Varda spawn: Found existing Varda at monster index %d", j);
                break;
            }
        }
        
        if (!varda_exists)
        {
            log_trace("Varda spawn: No existing Varda found, attempting placement");
             bool varda_spawned = false;

             int try_y = -1;
             int try_x = -1;
             int total_sunlight = 0;
            int empty_sunlight = 0;
            int spawnable_sunlight = pick_varda_sunlight_spawn_tile(&try_y, &try_x, &total_sunlight, &empty_sunlight);

            log_trace("Varda spawn: Sunlight tiles total=%d, empty=%d, spawnable=%d",
                total_sunlight, empty_sunlight, spawnable_sunlight);

            if (spawnable_sunlight == 0) {
                log_trace("Varda spawn: No spawnable sunlight tiles available, forcing a sunlit tile");
                if (force_varda_sunlight_tile(&try_y, &try_x)) {
                    spawnable_sunlight = 1;
                }
            }

            if (spawnable_sunlight > 0) {
                if (place_monster_one(try_y, try_x, R_IDX_VARDA, true, true, NULL)) {
                    varda_spawned = true;
                } else {
                    log_trace("Varda spawn: Primary sunlight tile rejected, scanning for fallback");

                    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
                    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
                    {
                        for (int x = 0; x < p_ptr->cur_map_wid; x++)
                        {
                            access[y][x] = false;
                        }
                    }
                    flood_access(p_ptr->py, p_ptr->px, access, false);

                    for (int y = 1; y < p_ptr->cur_map_hgt - 1 && !varda_spawned; y++) {
                        for (int x = 1; x < p_ptr->cur_map_wid - 1 && !varda_spawned; x++) {
                            if (!varda_sunlight_tile_ok(y, x, true)) continue;
                            if (!varda_no_rubble_path_tile_ok(y, x, access)) continue;
                            if (place_monster_one(y, x, R_IDX_VARDA, true, true, NULL)) {
                                try_y = y;
                                try_x = x;
                                varda_spawned = true;
                            }
                        }
                    }
                }
            }

            if (varda_spawned) {
                if (shadow_quest) {
                    quest_set_state(QUEST_ID_VARDA_SHADOW, QUEST_STATE_GIVER_PRESENT);
                    p_ptr->varda_shadow_level = p_ptr->depth;
                } else {
                    p_ptr->varda_quest = VARDA_QUEST_GIVER_PRESENT;
                    p_ptr->varda_level = p_ptr->depth;
                }
                p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                level_gen_debug_note_questgiver(shadow_quest ? QUEST_ID_VARDA_SHADOW : QUEST_ID_VARDA);
                log_trace("Varda spawn: === SUCCESS === Placed at (%d,%d) on sunlight tile", try_y, try_x);
                if (shadow_quest) {
                    log_trace("Varda spawn: Shadow quest state set to GIVER_PRESENT, quest_reserved[0]=1");
                } else {
                    log_trace("Varda spawn: Quest state set to GIVER_PRESENT (%d), quest_reserved[0]=1", p_ptr->varda_quest);
                }
            }
            
            if (!varda_spawned)
            {
                log_trace("Varda spawn: === FAILED === Could not find valid sunlight tile after forcing - REGENERATING LEVEL");
                genlog_fail("VARDA SPAWN FAILED: could not find valid sunlight tile after forcing - regenerating");
                gen_log_level_end(false, dun->cent_n, 1);
                return false; /* Force regeneration to honor 100% spawn guarantee */
            }
        }
        else
        {
            log_trace("Varda spawn: Varda already present on level, skipping placement");
        }
    } else {
        log_trace("Varda spawn: SKIPPED - did not win lottery (winner=%d)", quest_lottery_winner);
    }

    /* Check for Tulkas quest spawning - only if it won the lottery */
    int tulkas_completions = metarun_quest_completion_count(METARUN_QUEST_TULKAS);
    log_trace("Tulkas spawn check: quest=%d, depth=%d, metarun_completions=%d, lottery_winner=%d", 
             p_ptr->tulkas_quest, p_ptr->depth, tulkas_completions, quest_lottery_winner);
             
    /* Only attempt Tulkas spawning if it won the lottery */
    if (quest_lottery_winner == 1) { /* Tulkas is quest ID 1 */
        log_trace("Tulkas spawn: Tulkas WON the lottery - attempting spawn");

        if (spawn_tulkas_near_player_with_fallback())
        {
            p_ptr->tulkas_quest = TULKAS_QUEST_GIVER_PRESENT;
            p_ptr->quest_reserved[0] = 1;
            level_gen_debug_note_questgiver(QUEST_ID_TULKAS);
            log_trace("Tulkas spawn: success, quest state set to %d", p_ptr->tulkas_quest);
        }
        else
        {
            log_trace("Failed to spawn Tulkas after all attempts");
        }
    }

    /* Check for Niena room-based spawning - LOTTERY SYSTEM */
    int niena_completions = metarun_quest_completion_count(METARUN_QUEST_NIENA);
    log_trace("Niena spawn check: quest=%d, depth=%d, level_size_l=%d, metarun_completions=%d, lottery_winner=%d", 
             p_ptr->niena_quest, p_ptr->depth, l, niena_completions, quest_lottery_winner);
             
    /* Only attempt Niena spawning if it won the lottery */
    if (quest_lottery_winner == 4) { /* Niena is quest ID 4 */
        log_trace("Niena spawn: Niena WON the lottery - attempting spawn");
        
        /* Check level size requirement: must be maximum size (l >= 5) */
        if (l < 5) {
            log_trace("Niena spawn: FAILED - level too small (l=%d, need l>=5)", l);
            genlog_quest("NIENA SPAWN FAILED: level size %d < 5, forcing regeneration", l);
            gen_log_level_end(false, dun->cent_n, 1);
            return false; /* Force regeneration until we get a big enough level */
        }
        
        /* Niena's challenge is tied to the player's actual start, not the
         * closest arbitrary up/down pair somewhere else on the level. */
        int min_stair_dist = calculate_nearest_down_stair_distance_from(p_ptr->py, p_ptr->px);
        log_trace("Niena spawn: Calculated player-to-nearest-down distance = %d", min_stair_dist);
        
        if (min_stair_dist < 87) {
            log_trace("Niena spawn: FAILED - nearest down stair too close to player start (distance=%d, need >=87)", min_stair_dist);
            genlog_quest("NIENA SPAWN FAILED: player-to-down distance %d < 87, forcing regeneration", min_stair_dist);
            gen_log_level_end(false, dun->cent_n, 1);
            return false; /* Force regeneration until stairs are far enough apart */
        }
        
        log_trace("Niena spawn: Distance check PASSED (player-to-down=%d >= 87)", min_stair_dist);
        
        /* Try to find a room to spawn Niena in near the up stairs */
        int attempts;
        bool niena_spawned = false;
        
        log_trace("Niena spawn: Lottery winner attempting placement at depth %d, level_size=%d, stair_distance=%d", 
                  p_ptr->depth, l, min_stair_dist);
        
        /* Check if Niena already exists on this level */
        bool niena_exists = false;
        int j;
        for (j = 1; j < mon_max; j++)
        {
            monster_type *m_ptr = &mon_list[j];
            if (m_ptr->r_idx == R_IDX_NIENA)
            {
                niena_exists = true;
                break;
            }
        }
        
        if (!niena_exists)
        {
            /* Try to spawn Niena near the player's starting position (up stairs) */
            int player_y = p_ptr->py;
            int player_x = p_ptr->px;
            
            log_trace("Niena spawn: Attempting to place near player at (%d,%d)", player_y, player_x);
            
            /* Verify player has valid coordinates */
            if (player_y > 0 && player_y < p_ptr->cur_map_hgt - 1 &&
                player_x > 0 && player_x < p_ptr->cur_map_wid - 1)
            {
                /* Try to find a spot in the same room as the player first */
                for (attempts = 0; attempts < 50 && !niena_spawned; attempts++)
                {
                    /* Search in a radius around the player */
                    int dy = rand_range(-2, 2);
                    int dx = rand_range(-2, 2);
                    int try_y = player_y + dy;
                    int try_x = player_x + dx;
                    
                    /* Must be valid coordinates, floor in the same room, and not too close to player */
                    if (try_y > 0 && try_y < p_ptr->cur_map_hgt - 1 &&
                        try_x > 0 && try_x < p_ptr->cur_map_wid - 1 &&
                        cave_floor_bold(try_y, try_x) && 
                        (cave_info[try_y][try_x] & CAVE_ROOM) &&
                        !(cave_info[try_y][try_x] & CAVE_ICKY) &&
                        cave_m_idx[try_y][try_x] == 0 &&
                        distance(player_y, player_x, try_y, try_x) >= 2 &&
                        los(player_y, player_x, try_y, try_x))
                    {
                        if (place_monster_one(try_y, try_x, R_IDX_NIENA, true, true, NULL))
                        {
                            p_ptr->niena_quest = NIENA_QUEST_GIVER_PRESENT;
                            p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                            level_gen_debug_note_questgiver(QUEST_ID_NIENA);
                            niena_spawned = true;
                            log_trace("Niena spawned near player at (%d, %d), player at (%d, %d), quest state: %d", 
                                     try_y, try_x, player_y, player_x, p_ptr->niena_quest);
                        }
                    }
                }
            }
            else
            {
                log_trace("Niena spawn: Invalid player coordinates (%d,%d), falling back to any room", player_y, player_x);
            }
            
            /* If that failed, try any room on the level */
            if (!niena_spawned)
            {
                log_trace("Niena spawn: Near-player placement failed, trying any suitable room");
                for (attempts = 0; attempts < 100 && !niena_spawned; attempts++)
                {
                    int room_y = rand_int(p_ptr->cur_map_hgt);
                    int room_x = rand_int(p_ptr->cur_map_wid);
                    
                    /* Must be valid coordinates and a floor in a room */
                    if (room_y > 0 && room_y < p_ptr->cur_map_hgt - 1 &&
                        room_x > 0 && room_x < p_ptr->cur_map_wid - 1 &&
                        cave_floor_bold(room_y, room_x) && 
                        (cave_info[room_y][room_x] & CAVE_ROOM) &&
                        !(cave_info[room_y][room_x] & CAVE_ICKY) &&
                        cave_m_idx[room_y][room_x] == 0)
                    {
                        if (place_monster_one(room_y, room_x, R_IDX_NIENA, true, true, NULL))
                        {
                            p_ptr->niena_quest = NIENA_QUEST_GIVER_PRESENT;
                            p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                            level_gen_debug_note_questgiver(QUEST_ID_NIENA);
                            niena_spawned = true;
                            log_trace("Niena spawned in fallback room at (%d, %d), quest state: %d", 
                                     room_y, room_x, p_ptr->niena_quest);
                        }
                    }
                }
            }

            if (!niena_spawned)
            {
                log_trace("Niena spawn: Random placement failed, scanning the full level for a valid room tile");
                for (int scan_y = 1; scan_y < p_ptr->cur_map_hgt - 1 && !niena_spawned; scan_y++)
                {
                    for (int scan_x = 1; scan_x < p_ptr->cur_map_wid - 1 && !niena_spawned; scan_x++)
                    {
                        if (!cave_floor_bold(scan_y, scan_x))
                            continue;
                        if (!(cave_info[scan_y][scan_x] & CAVE_ROOM))
                            continue;
                        if (cave_info[scan_y][scan_x] & CAVE_ICKY)
                            continue;
                        if (cave_m_idx[scan_y][scan_x] != 0)
                            continue;
                        if (distance(player_y, player_x, scan_y, scan_x) < 2)
                            continue;

                        if (place_monster_one(scan_y, scan_x, R_IDX_NIENA, true, true, NULL))
                        {
                            p_ptr->niena_quest = NIENA_QUEST_GIVER_PRESENT;
                            p_ptr->quest_reserved[0] = 1;
                            level_gen_debug_note_questgiver(QUEST_ID_NIENA);
                            niena_spawned = true;
                            log_trace("Niena spawned by exhaustive scan at (%d, %d), quest state: %d",
                                scan_y, scan_x, p_ptr->niena_quest);
                        }
                    }
                }
            }
            
            if (!niena_spawned)
            {
                log_trace("Niena spawn: FAILED to spawn after all attempts - forcing regeneration");
                genlog_fail("NIENA SPAWN FAILED: could not place monster after all attempts - regenerating");
                gen_log_level_end(false, dun->cent_n, 1);
                return false; /* Force regeneration */
            }
        }
        else
        {
            log_trace("Niena already exists on level, skipping room spawn");
        }
    } else {
        log_trace("Niena spawn: SKIPPED - did not win lottery (winner=%d)", quest_lottery_winner);
    }

    /* Check for Orome quest spawning - only if it won the lottery */
    int orome_completions = metarun_quest_completion_count(METARUN_QUEST_OROME);
    bool orome_blocked = quest_metarun_blocked(QUEST_ID_OROME, METARUN_QUEST_OROME);
    log_trace("Orome spawn check: quest=%d, depth=%d, metarun_completions=%d, lottery_winner=%d, blocked=%s", 
             p_ptr->orome_quest, p_ptr->depth, 
             orome_completions,
             quest_lottery_winner,
             orome_blocked ? "yes" : "no");
             
    /* Only attempt Orome spawning if it won the lottery and isn't blocked by metarun history */
    if (orome_blocked) {
        log_trace("Orome spawn: blocked by metarun state (requires active oath or under cap)");
        quest_lottery_winner = 0; /* Treat level as quest-free if history blocks this quest */
    } else if (quest_lottery_winner == 5) { /* Orome is quest ID 5 */
        log_trace("Orome spawn: Orome WON the lottery - attempting spawn");
        
        /* Try to find a room to spawn Orome in */
        int attempts;
        bool orome_spawned = false;
        
        log_trace("Orome spawn: Lottery winner attempting placement at depth %d", p_ptr->depth);
        
        /* Check if Orome already exists on this level */
        bool orome_exists = false;
        int j;
        for (j = 1; j < mon_max; j++)
        {
            monster_type *m_ptr = &mon_list[j];
            if (m_ptr->r_idx == R_IDX_OROME)
            {
                orome_exists = true;
                break;
            }
        }
        
        if (!orome_exists)
        {
            /* Try to spawn Orome near the player's starting room */
            int player_y = p_ptr->py;
            int player_x = p_ptr->px;
            
            /* Try to find a spot in the same room as the player first */
            for (attempts = 0; attempts < 50 && !orome_spawned; attempts++)
            {
                /* Search in a radius around the player */
                int dy = rand_range(-2, 2);
                int dx = rand_range(-2, 2);
                int try_y = player_y + dy;
                int try_x = player_x + dx;
                
                /* Must be valid coordinates, floor in the same room, and not too close to player */
                if (try_y > 0 && try_y < p_ptr->cur_map_hgt - 1 &&
                    try_x > 0 && try_x < p_ptr->cur_map_wid - 1 &&
                    cave_floor_bold(try_y, try_x) && 
                    (cave_info[try_y][try_x] & CAVE_ROOM) &&
                    !(cave_info[try_y][try_x] & CAVE_ICKY) &&
                    cave_m_idx[try_y][try_x] == 0 &&
                    distance(player_y, player_x, try_y, try_x) >= 2 &&
                    los(player_y, player_x, try_y, try_x))
                {
                    if (place_monster_one(try_y, try_x, R_IDX_OROME, true, true, NULL))
                    {
                        p_ptr->orome_quest = OROME_QUEST_GIVER_PRESENT;
                        p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                        level_gen_debug_note_questgiver(QUEST_ID_OROME);
                        orome_spawned = true;
                        log_trace("Orome spawned near player at (%d, %d), player at (%d, %d), quest state: %d", 
                                 try_y, try_x, player_y, player_x, p_ptr->orome_quest);
                    }
                }
            }
            
            /* If that failed, try any room on the level */
            if (!orome_spawned)
            {
                for (attempts = 0; attempts < 100 && !orome_spawned; attempts++)
                {
                    int room_y = rand_int(p_ptr->cur_map_hgt);
                    int room_x = rand_int(p_ptr->cur_map_wid);
                    
                    /* Must be a floor in a room, not in a vault/interesting room */
                    if (cave_floor_bold(room_y, room_x) && 
                        (cave_info[room_y][room_x] & CAVE_ROOM) &&
                        !(cave_info[room_y][room_x] & CAVE_ICKY) &&
                        cave_m_idx[room_y][room_x] == 0)
                    {
                        if (place_monster_one(room_y, room_x, R_IDX_OROME, true, true, NULL))
                        {
                            p_ptr->orome_quest = OROME_QUEST_GIVER_PRESENT;
                            p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                            level_gen_debug_note_questgiver(QUEST_ID_OROME);
                            orome_spawned = true;
                            log_trace("Orome spawned in fallback room at (%d, %d), quest state: %d", 
                                     room_y, room_x, p_ptr->orome_quest);
                        }
                    }
                }
            }
            
            if (!orome_spawned)
            {
                log_trace("Orome spawn: FAILED - could not place monster after 150 attempts");
                genlog_fail("OROME SPAWN FAILED: could not place monster after 150 attempts - regenerating");
                gen_log_level_end(false, dun->cent_n, 1);
                return false; /* Force regeneration */
            }
        }
        else
        {
            log_trace("Orome already exists on level, skipping room spawn");
        }
    } else {
        log_trace("Orome spawn: SKIPPED - did not win lottery (winner=%d)", quest_lottery_winner);
    }

    // place Morgoth if on the run
    if (p_ptr->on_the_run && !p_ptr->morgoth_slain)
    {
        bool placed = false;
        int sils = silmarils_possessed();
        int max_dist = 50 - (sils * 8);
        int min_dist = 9 - sils;

        if (max_dist < min_dist + 2)
            max_dist = min_dist + 2;

        /* Prefer spawning within a chase radius scaled by Silmarils. */
        for (int pass = 0; pass < 2 && !placed; ++pass)
        {
            bool require_no_los = (pass == 0);

            for (i = 0; i <= 180; i++)
            {
                int dy = rand_range(-max_dist, max_dist);
                int dx = rand_range(-max_dist, max_dist);
                int dist = ABS(dy) + ABS(dx);

                if (dist < min_dist || dist > max_dist)
                    continue;

                y = p_ptr->py + dy;
                x = p_ptr->px + dx;

                if (!in_bounds_fully(y, x))
                    continue;
                if (!cave_empty_bold(y, x))
                    continue;
                if (cave_info[y][x] & (CAVE_ICKY))
                    continue;
                if (require_no_los && los(p_ptr->py, p_ptr->px, y, x))
                    continue;

                if (place_monster_one(y, x, R_IDX_MORGOTH, false, true, NULL))
                {
                    placed = true;
                    break;
                }
            }
        }

        if (!placed)
        {
            for (y = 1; y < p_ptr->cur_map_hgt - 1 && !placed; ++y)
            {
                for (x = 1; x < p_ptr->cur_map_wid - 1 && !placed; ++x)
                {
                    if (!cave_empty_bold(y, x))
                        continue;
                    if (cave_info[y][x] & (CAVE_ICKY))
                        continue;

                    if (place_monster_one(y, x, R_IDX_MORGOTH, false, true, NULL))
                        placed = true;
                }
            }
        }

        if (placed && cave_m_idx[y][x] > 0)
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            if (m_ptr->r_idx == R_IDX_MORGOTH)
            {
                if (m_ptr->alertness < ALERTNESS_ALERT)
                    m_ptr->alertness = ALERTNESS_ALERT;
                m_ptr->min_range = 0;
            }
        }
        else if (!placed)
        {
            log_trace("Morgoth spawn: FAILED to place Morgoth while on the run (depth=%d)", p_ptr->depth);
        }
    }
    p_ptr->force_forge = false;

    /* Level generation successful - log completion */
    genlog_summary("Level %d generation COMPLETE: %d rooms, quest_lottery=%d",
                   p_ptr->depth, dun->cent_n, quest_lottery_winner);
    gen_log_level_end(true, dun->cent_n, 1);
    gen_log_flush();

    return (true);
}

/*
 * Create the gates to Angband level
 */
static void gates_gen(void)
{
    int y, x;
    int i;
    int py = -1, px = -1;

    memset(dun, 0, sizeof(*dun));
    layout_anchor_reset();
    reset_morgoth_layout_state(false);
    current_partition_rows = 0;
    current_partition_cols = 0;
    current_partition_count = 0;
    current_labyrinth_partitions = 0;
    reset_partition_population_metadata();
    for (i = 0; i < PARTITION_META_MAX; ++i)
    {
        current_partition_modes[i] = QUAD_MODE_ROOMY;
        current_partition_densities[i] = DENSITY_NORMAL;
        current_partition_big_cave_types[i] = BIG_CAVE_NONE;
    }

    /* Restrict to single-screen size */
    p_ptr->cur_map_hgt = (3 * PANEL_HGT);
    p_ptr->cur_map_wid = (2 * PANEL_WID_FIXED);

    /* Initialize level style weights for depth 0 */
    styles_init_for_level();
    /* If no primary style was selected (e.g., no rules loaded yet), force style 13 */
    if (styles_get_level_primary_style() < 0) {
        styles_set_loaded_level_primary(13);
        log_info("gates_gen: forced level primary style to 13 for depth 0");
    }

    /*start with basic granite*/
    basic_granite();

    /*set the permanent walls*/
    set_perm_boundry();

    if (!build_type10(17, 33))
    {
        log_error("gates_gen: failed to build Gates of Angband vault");
    }

    /* Find an up staircase */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_feat[y][x] == FEAT_MORE)
            {
                py = y;
                px = x;
            }
        }
    }

    if ((py < 0) || (px < 0))
    {
        msg_format("Failed to find a down staircase in the gates level");
        py = p_ptr->cur_map_hgt / 2;
        px = p_ptr->cur_map_wid / 2;
        for (y = py - 1; y <= py + 1; ++y)
        {
            for (x = px - 1; x <= px + 1; ++x)
            {
                if (!in_bounds(y, x))
                    continue;
                cave_set_feat(y, x, FEAT_FLOOR);
            }
        }
        cave_set_feat(py, px, FEAT_MORE);
    }

    /* Delete any monster on the starting square */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Only get the monster on the same square */
        if ((m_ptr->fy != py) || (m_ptr->fx != px))
            continue;

        /* Delete the monster */
        delete_monster_idx(i);
    }

    /* Place the player */
    player_place(py, px);
}

/*
 * Create the level containing Morgoth's throne room
 */
#if 0
static void throne_gen(void)
{
    int y, x;
    int i;
    int py = 0, px = 0;

    // display the throne poetry
    pause_with_text(throne_poetry, 5, 13, NULL, 0);

    // set the 'truce' in action
    p_ptr->truce = true;

    /* Restrict to single-screen size */
    p_ptr->cur_map_hgt = (3 * PANEL_HGT);
    p_ptr->cur_map_wid = (3 * PANEL_WID_FIXED);

    /*start with basic granite*/
    basic_granite();

    /*set the permanent walls*/
    set_perm_boundry();

    build_type9(16, 38, NULL);

    /* Find an up staircase */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            // Sil-y: assumes the important staircase is at the centre of the
            // level
            if ((cave_feat[y][x] == FEAT_LESS) && (x >= 30) && (x <= 45))
            {
                py = y;
                px = x;
            }
        }
    }

    if ((py == 0) || (px == 0))
    {
        msg_format("Failed to find an up staircase in the throne-room");
    }

    /* Delete any monster on the starting square */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Only get the monster on the same square */
        if ((m_ptr->fy != py) || (m_ptr->fx != px))
            continue;

        /* Delete the monster */
        delete_monster_idx(i);
    }

    /* Place the player */
    player_place(py, px);
}
#endif

/*
 * Spawn Nienna for the Morgoth-hall mercy quest when conditions are met.
 */
static bool spawn_niena_morgoth_hall(void)
{
    byte state = quest_get_state(QUEST_ID_NIENA_MORGOTH);
    bool has_pending_giver = (state == QUEST_STATE_GIVER_PRESENT);

    if (state != QUEST_STATE_NOT_STARTED && !has_pending_giver) {
        return false;
    }
    if (!has_pending_giver && p_ptr->quest_reserved[0]) {
        log_trace("Niena Morgoth quest: blocked by quest_reserved[0]");
        return false;
    }
    if (p_ptr->oath_type != OATH_MERCY || oath_invalid(OATH_MERCY)) {
        log_trace("Niena Morgoth quest: Oath of Mercy not active or broken");
        return false;
    }
    if (p_ptr->niena_quest != NIENA_QUEST_NOT_STARTED &&
        p_ptr->niena_quest != NIENA_QUEST_REWARDED) {
        log_trace("Niena Morgoth quest: primary Niena quest in progress (%d)", p_ptr->niena_quest);
        return false;
    }
    if (!check_quest_eligibility(QUEST_ID_NIENA_MORGOTH, p_ptr->depth)) {
        log_trace("Niena Morgoth quest: eligibility check failed");
        return false;
    }
    if (is_quest_giver_present(R_IDX_NIENA)) {
        log_trace("Niena Morgoth quest: quest giver already present");
        return false;
    }

    for (int attempt = 0; attempt < 50; attempt++) {
        int y = p_ptr->py + rand_range(-2, 2);
        int x = p_ptr->px + rand_range(-2, 2);

        if (in_bounds_fully(y, x) && cave_floor_bold(y, x) && cave_m_idx[y][x] == 0) {
            if (place_monster_one(y, x, R_IDX_NIENA, true, true, NULL)) {
                quest_set_state(QUEST_ID_NIENA_MORGOTH, QUEST_STATE_GIVER_PRESENT);
                p_ptr->niena_level = p_ptr->depth;
                p_ptr->niena_reserved &= ~(NIENA_FLAG_MORGOTH_ATTACKED | NIENA_FLAG_MERCY_GIFT_TEMP);
                p_ptr->quest_reserved[0] = 1;
                level_gen_debug_note_questgiver(QUEST_ID_NIENA_MORGOTH);
                log_trace("Niena Morgoth quest: placed giver at (%d,%d)", y, x);
                return true;
            }
        }
    }

    log_trace("Niena Morgoth quest: failed to place giver near player");
    return false;
}

/*
 * Dungeon generation can set some flags indicating that certain one-off
 * things have happened (artefacts, unique greater vaults, unique forge).
 * But if generation fails, we need to reset these flags.
 *
 * "You can't unring a bell." -- Tom Waits
 */
void unring_a_bell(void)
{
    object_type* o_ptr;
    int y, x, i;

    // look through the dungeon objects for artefacts
    for (i = 1; i < o_max; i++)
    {
        /* Get the object */
        o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];
            a_ptr->cur_num = 0;
        }
    }

    // Look through the map for the unique forge
    for (y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            // Reset the unique forge
            if ((cave_feat[y][x] >= FEAT_FORGE_UNIQUE_HEAD)
                && (cave_feat[y][x] <= FEAT_FORGE_UNIQUE_TAIL))
            {
                p_ptr->unique_forge_made = false;
            }
        }
    }

    /* DEBUGGING: Final check if quest vault still exists at end of generation */
    check_quest_vault_integrity("END_OF_GENERATION");

    // If there is a greater vault...
    if (g_vault_name[0] != '\0')
    {
        // wipe vault name
        g_vault_name[0] = '\0';

        // look for the final greater vault entry
        for (i = 0; i < MAX_GREATER_VAULTS; i++)
        {
            // wipe the final entry
            if (i == MAX_GREATER_VAULTS - 1)
            {
                p_ptr->greater_vaults[i] = 0;
                break;
            }
            else if (p_ptr->greater_vaults[i + 1] == 0)
            {
                p_ptr->greater_vaults[i] = 0;
                break;
            }
        }
    }
}

/*
 * Generate a random dungeon level
 *
 * Hack -- regenerate any "overflow" levels
 *
 * Note that this function resets "cave_feat" and "cave_info" directly.
 */
void generate_cave(void)
{
    int y, x, i;
    bool is_morgoth_level = (p_ptr->depth == MORGOTH_DEPTH);

    log_info("generate_cave: Function entry - about to start");
    log_debug("generate_cave: Starting cave generation");

    /* Reset per-level color cache so depth group re-rolls when entering a new level */
    reset_depth_color_cache();

    /* The dungeon is not ready */
    character_dungeon = false;

    /* Don't know feeling yet */
    do_feeling = 0;

    /*allow uniques to be generated everywhere but in nests/pits*/
    allow_uniques = true;

    /* Never carry the throne-room truce between levels */
    p_ptr->truce = false;

    /* Restrict quest monsters from spawning outside their quest contexts */
    get_mon_num_hook = quest_monster_spawn_okay;

    // display the entry poetry
if (playerturn == 0) {
    char extra[4][100];
    int idx = 0;

    /* Prepare pointers */
    const char *name = c_name + current_character_profile->name;
    const char *alt = c_name + current_character_profile->alt_name;
    const char *start = c_name + current_character_profile->start_string;

    /* Line 1: CharacterName AltName! */
    strnfmt(extra[idx], 100, "%s%s!", name, alt);
    idx++;

    /* Split start string (motto) at first '-' */
    const char *dash_start = strchr(start, '-');
    if (dash_start) {
        /* Line 2: up to and including dash */
        strnfmt(extra[idx], 100, "%.*s",
                (int)(dash_start - start + 1), start);
        idx++;
        /* Line 3: remainder after dash */
        strnfmt(extra[idx], 100, "%s", dash_start + 1);
        idx++;
    } else {
        /* No dash: all in one line */
        strnfmt(extra[idx], 100, "%s", start);
        idx++;
    }

    /* sentinel */
    extra[idx][0] = '\0';

    /* display banner + stanza */
    pause_with_text(entry_poetry, 4, 13, extra, TERM_YELLOW);
}


    /* Safety check: make sure cave_color is allocated */
    if (!cave_color) {
        log_error("generate_cave: cave_color array is not allocated!");
        return;
    }

    level_gen_screen_begin();
    reset_generation_retry_locks();

    // reset smithing leftover (as there is no access to the old forge)
    p_ptr->smithing_leftover = 0;

    // reset the forced skipping of next turn (a bit rough to miss first turn if
    // you fell down)
    p_ptr->skip_next_turn = false;

    bool preserve_run_quest_slot = run_has_consumed_quest_slot();

    while (true)
    {
        bool okay = true;
        bool quest_vault_placed_this_attempt = false; /* Track if quest vault placed in this attempt */

        cptr why = NULL;

        level_gen_screen_start_attempt();
        
        /* QUEST VAULT REGENERATION DEBUG: Log each regeneration attempt */
        log_trace("QUEST VAULT FIX: Starting level generation attempt (quest_vault_used=%s)",
                  p_ptr->quest_vault_used ? "true" : "false");

        /* Reset pending quest state changes at the start of each generation attempt */
        reset_pending_quest_states();
        
        /* Reset quest states that may have been set during previous failed attempts */
        reset_quest_vault_states(preserve_run_quest_slot);

        /* Paranoia: Check that cave_color is allocated */
        if (!cave_color)
        {
            log_error("cave_color array is not allocated!");
            quit("cave_color array not allocated");
        }

        /* Reset */
        o_max = 1;
        mon_max = 1;
        feeling = 0;

        /* Start with a blank cave */
        for (y = 0; y < MAX_DUNGEON_HGT; y++)
        {
            for (x = 0; x < MAX_DUNGEON_WID; x++)
            {
                /* No flags */
                cave_info[y][x] = 0;

                /* No features */
                cave_feat[y][x] = 0;

                /* No colors (use default) */
                cave_color[y][x] = 0;

                /* No objects */
                cave_o_idx[y][x] = 0;

                /* No monsters */
                cave_m_idx[y][x] = 0;

                for (i = 0; i < MAX_FLOWS; i++)
                {
                    cave_cost[i][y][x] = FLOW_MAX_DIST;
                }

                cave_when[y][x] = 0;
            }
        }

    log_debug("generate_cave: Cave initialization completed successfully");

        // reset the wandering monster pauses
        for (i = 0; i < MAX_FLOWS; i++)
        {
            wandering_pause[i] = 0;
        }

        /* Mega-Hack -- no player yet */
        p_ptr->px = p_ptr->py = 0;

        /* Hack -- illegal panel */
        p_ptr->wy = MAX_DUNGEON_HGT;
        p_ptr->wx = MAX_DUNGEON_WID;

        /* Reset the monster generation level */
        monster_level = p_ptr->depth;

        /* Reset the object generation level */
        object_level = p_ptr->depth;

        /* Nothing special here yet */
        good_item_flag = false;

        /* Nothing good here yet */
        rating = 0;

        /* Build the gates to Angband */
        if (!p_ptr->depth)
        {
            level_gen_screen_set_stage(LEVEL_GEN_STAGE_FOUNDATIONS,
                "Preparing the Gates.");
            level_gen_screen_set_stage(LEVEL_GEN_STAGE_SHAPING,
                "Building the Gates of Angband.");
            gates_gen();
            level_gen_screen_set_stage(LEVEL_GEN_STAGE_FINALIZING,
                "Final touches on the Gates.");

            /* Hack -- Clear stairs request */
            p_ptr->create_stair = 0;
        }

        /* Build a real level */
        else
        {
            /* Make a dungeon, or report the failure to make one*/
            if (cave_gen())
            {
                okay = true;
                if (is_morgoth_level)
                {
                    /* Depth 20 uses the partition system; keep entry stairs so the player can retreat. */
                    (void)spawn_niena_morgoth_hall();
                }
                /* Check if quest vault was placed during this level generation */
                if (qv_placed_this_level) {
                    quest_vault_placed_this_attempt = true;
                }
                /* Also check if we have pending quest state changes that indicate a quest vault was placed */
                {
                    bool mandos_nonblocking = (pending_quest_states.has_mandos_change &&
                                               pending_quest_states.mandos_quest_id == QUEST_ID_MANDOS_BETRAYER);
                    if (pending_quest_states.has_aule_change ||
                        pending_quest_states.has_varda_change ||
                        pending_quest_states.has_varda_shadow_change ||
                        pending_quest_states.has_tulkas_change ||
                        (pending_quest_states.has_mandos_change && !mandos_nonblocking)) {
                        quest_vault_placed_this_attempt = true;
                    }
                }
            }
            else
            {
                okay = false;
            }
        }

        /*message*/
        if (!okay)
        {
            if (cheat_room || cheat_hear || cheat_peek || cheat_xtra)
                why = "defective level";

            // Must reset all the artefacts that were generated on the defective
            // level
            for (i = 1; i < o_max; i++)
            {
                /* Get the object */
                object_type* o_ptr = &o_list[i];

                /* Skip dead objects */
                if (!o_ptr->k_idx)
                    continue;

                /* If artefact. */
                if (o_ptr->name1)
                {
                    /* Reset its count */
                    a_info[o_ptr->name1].cur_num = 0;
                    a_info[o_ptr->name1].found_num = 0;
                }
            }
        }

        else
        {
            /* Extract the feeling */
            if (!feeling)
            {
                if (rating > 100)
                    feeling = 2;
                else if (rating > 80)
                    feeling = 3;
                else if (rating > 60)
                    feeling = 4;
                else if (rating > 40)
                    feeling = 5;
                else if (rating > 30)
                    feeling = 6;
                else if (rating > 20)
                    feeling = 7;
                else if (rating > 10)
                    feeling = 8;
                else if (rating > 0)
                    feeling = 9;
                else
                    feeling = 10;

                /* Hack -- Have a special feeling sometimes */
                if (good_item_flag && !(PRESERVE_MODE))
                    feeling = 1;

                /* Hack -- no feeling at the gates */
                if (!p_ptr->depth)
                    feeling = 0;
            }

            /* Prevent object over-flow */
            if (o_max >= z_info->o_max)
            {
                /* Message */
                why = "too many objects";

                /* Message */
                okay = false;
            }

            /* Prevent monster over-flow */
            if (mon_max >= MAX_MONSTERS)
            {
                /* Message */
                why = "too many monsters";

                /* Message */
                okay = false;
            }
        }

        /* Accept */
        if (okay)
        {
            /* QUEST VAULT REGENERATION FIX: Apply pending quest state changes when level generation is COMPLETELY successful */
            apply_pending_quest_states();

            if (p_ptr->tulkas_second_spawn_pending &&
                p_ptr->depth == p_ptr->tulkas_stronghold_level &&
                quest_get_state(QUEST_ID_TULKAS_ORCS) == QUEST_STATE_GIVER_PRESENT)
            {
                if (spawn_tulkas_near_player_with_fallback()) {
                    p_ptr->quest_reserved[0] = 1;
                    p_ptr->tulkas_second_spawn_pending = 0;
                    level_gen_debug_note_questgiver(QUEST_ID_TULKAS_ORCS);
                    log_trace("Tulkas orc quest: Quest giver spawned after stronghold placement");
                } else {
                    log_trace("Tulkas orc quest: Failed to spawn Tulkas after stronghold placement");
                }
            }
            
            /* QUEST VAULT REGENERATION FIX: Only mark quest_vault_used when level generation is COMPLETELY successful */
            /* This ensures quest vaults can be re-placed during regeneration attempts */
            if (quest_vault_placed_this_attempt) {
                p_ptr->quest_vault_used = 1;
                log_trace("QUEST VAULT FIX: Level completely successful - setting quest_vault_used = 1");
            } else {
                log_trace("QUEST VAULT FIX: Level successful but no quest vault placed this attempt");
            }
            log_trace("QUEST VAULT FIX: Breaking from regeneration loop with successful level");
            break;
        }

        level_gen_screen_note_failure(why ? why : level_gen_screen.last_failure);

        if (why)
        {
            log_trace("QUEST VAULT FIX: Level generation failed (%s), regenerating (quest_vault_used=%s)",
                      why, p_ptr->quest_vault_used ? "true" : "false");
        }
        else
        {
            log_trace("QUEST VAULT FIX: Level generation failed (unknown reason), regenerating (quest_vault_used=%s)",
                      p_ptr->quest_vault_used ? "true" : "false");
        }

        // Undo unique things!
        unring_a_bell();

        /* Wipe the objects */
        wipe_o_list();

        /* Wipe the monsters */
        wipe_mon_list();
    }

    /* The dungeon is ready */
    character_dungeon = true;

    /* Reset the number of traps on the level. */
    num_trap_on_level = 0;

    /* Reset per-level skeleton note limits once the layout is finalized */
    skeleton_note_level_reset();

    /* Normalize the chasm-footprint tags after all generation edits. */
    apply_chasm_partition_tags();

    /* Enforce partition/room lighting rules (e.g. labyrinth/CA_BLOB always dark). */
    apply_partition_and_room_glow_rules();

    /* Note any forges generated -- have to do this here in case generation
     * fails earlier */
    for (y = 0; y < MAX_DUNGEON_HGT; y++)
    {
        for (x = 0; x < MAX_DUNGEON_WID; x++)
        {
            if (cave_forge_bold(y, x))
            {
                p_ptr->forge_count++;
            }
        }
    }

    level_gen_screen_finish(true);

    // Valar quest doesn't provide map rewards like the old thrall quest
}


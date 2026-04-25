/* File: ui/ui-story.c */
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

#include "log/log.h"
#include "app/app-ui.h"
#include "platform-frame.h"
#include "platform-input.h"
#include "platform-story-font.h"
#include "platform-time.h"
#include "metarun.h"
#include "ui/story_font.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-story.h"
#include <ctype.h>

#define STORY_BROWSER_WRAP_COLS 72
#define STORY_BROWSER_VISIBLE_LINES 18
#define STORY_BROWSER_MIN_WIDTH 1000
#define STORY_BROWSER_MAX_WIDTH 1800
#define STORY_FADE_DURATION_MS 500u
#define STORY_FADE_FRAME_SLICE_MS 16u
#define STORY_REVEAL_HOLD_MS 1000u

typedef enum story_browser_mode {
    STORY_BROWSER_DOCUMENT = 0,
    STORY_BROWSER_INDEX,
    STORY_BROWSER_CHAPTER
} story_browser_mode;

static int story_count_wrapped_lines(cptr text, int wrap_width, int indent)
{
    if (platform_story_font_enabled())
        return count_wrapped_lines_story(text, wrap_width, indent);

    return count_wrapped_lines(text, wrap_width, indent);
}

static bool story_peek_key(char* out_key)
{
    app_session* session = app_session_current();
    app_input input;

    if (!out_key)
        return false;

    while (app_session_peek_input(session, &input))
    {
        if (input.layer == APP_INPUT_LAYER_LEGACY
            && input.type == APP_INPUT_TYPE_KEY)
        {
            *out_key = (char)(input.payload.key.logical_key & 0xFFu);
            return true;
        }

        (void)app_session_pop_input(session, NULL);
    }

    return false;
}

static void story_consume_peeked_key(char* out_key)
{
    app_session* session = app_session_current();
    app_input input;

    if (!out_key)
        return;

    while (app_session_pop_input(session, &input))
    {
        if (input.layer == APP_INPUT_LAYER_LEGACY
            && input.type == APP_INPUT_TYPE_KEY)
        {
            *out_key = (char)(input.payload.key.logical_key & 0xFFu);
            return;
        }
    }
}

static char story_direction_command_key(const app_ui_command* command)
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

static void story_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    platform_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static void story_build_reveal_prompt(char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (steamdeck_controls_active())
    {
        char skip_label[16];
        char esc_label[16];

        story_prompt_label(' ', "A", skip_label, sizeof(skip_label));
        story_prompt_label(ESCAPE, "ESC", esc_label, sizeof(esc_label));
        strnfmt(buf, buflen, "[%s] skip  *  [%s] fast forward", skip_label,
            esc_label);
    }
    else
    {
        SDL_strlcpy(buf, "[Any key] skip  *  [Esc] fast forward", buflen);
    }
}

static void story_build_final_prompt(char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (steamdeck_controls_active())
    {
        char next_label[16];

        story_prompt_label(' ', "A", next_label, sizeof(next_label));
        strnfmt(buf, buflen, "[%s] continue", next_label);
    }
    else
    {
        SDL_strlcpy(buf, "[Press any key to continue]", buflen);
    }
}

static bool story_append_paragraph(app_ui_scene* scene, app_ui_panel* panel,
    byte attr, byte alpha, cptr text)
{
    if (!scene || !panel || !text || !text[0])
        return true;
    if (!app_ui_panel_begin_rich_paragraph(scene, panel))
        return false;

    return app_ui_panel_add_rich_text_alpha_ex(scene, panel, attr,
        STORY_FLAG_USE, alpha, text);
}

static bool story_add_wrapped_detail_lines(app_ui_panel* panel, byte attr,
    cptr text, int max_lines)
{
    char line[APP_UI_TEXT_MAX];
    int line_pos = 0;
    int lines = 0;
    const char* cursor = text;
    const int wrap = 72;

    if (!panel || !text || !text[0] || max_lines <= 0)
        return true;

    while (*cursor && lines < max_lines)
    {
        while (*cursor == ' ' && line_pos == 0)
            cursor++;

        if (*cursor == '\n')
        {
            line[line_pos] = '\0';
            if (line_pos > 0)
            {
                if (!app_ui_panel_add_detail_line(panel, attr, line))
                    return false;
                lines++;
            }
            line_pos = 0;
            cursor++;
            continue;
        }

        if (line_pos >= wrap || line_pos >= (int)sizeof(line) - 1)
        {
            int break_pos = line_pos;

            while (break_pos > 0 && line[break_pos - 1] != ' ')
                break_pos--;
            if (break_pos <= 0)
                break_pos = line_pos;

            line[break_pos] = '\0';
            if (!app_ui_panel_add_detail_line(panel, attr, line))
                return false;
            lines++;
            if (lines >= max_lines)
                break;

            if (break_pos < line_pos)
            {
                int remaining = line_pos - break_pos;

                while (break_pos < line_pos && line[break_pos] == ' ')
                {
                    break_pos++;
                    remaining--;
                }
                memmove(line, line + break_pos, (size_t)remaining);
                line_pos = remaining;
            }
            else
            {
                line_pos = 0;
            }
            continue;
        }

        line[line_pos++] = *cursor++;
    }

    if (line_pos > 0 && lines < max_lines)
    {
        line[line_pos] = '\0';
        if (!app_ui_panel_add_detail_line(panel, attr, line))
            return false;
    }

    return true;
}

static void story_add_paragraph_line_count(int* total_lines,
    int* paragraph_count, int paragraph_lines)
{
    if (!total_lines || !paragraph_count || paragraph_lines <= 0)
        return;

    if (*paragraph_count > 0)
        (*total_lines)++;
    *total_lines += paragraph_lines;
    (*paragraph_count)++;
}

static bool story_build_browser_scene(app_ui_scene* scene, const int* sel_idx,
    int start, int complete_count, int active_index, byte active_alpha,
    cptr footer_text, byte footer_attr, bool allow_index)
{
    app_ui_panel* panel;
    int total_lines = 0;
    int paragraph_count = 0;

    if (!scene || !sel_idx)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_DOCUMENT;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_icon(panel, TERM_YELLOW, '*');
    app_ui_panel_set_widths(panel, STORY_BROWSER_MIN_WIDTH,
        STORY_BROWSER_MAX_WIDTH);
    app_ui_panel_set_title(panel, TERM_YELLOW, "The Tale So Far");
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        active_index >= 0 ? "Chapter reveal" : "Recovered chapters");

    for (int i = start; i < complete_count; i++)
    {
        story_type* st = &st_info[sel_idx[i]];
        cptr heading = st_name + st->name;
        cptr text = st_text + st->text;

        if (!story_append_paragraph(scene, panel, TERM_L_BLUE, 0xFFu, heading)
            || !story_append_paragraph(scene, panel, TERM_WHITE, 0xFFu, text))
        {
            return false;
        }

        story_add_paragraph_line_count(&total_lines, &paragraph_count, 1);
        story_add_paragraph_line_count(&total_lines, &paragraph_count,
            MAX(1, story_count_wrapped_lines(text, STORY_BROWSER_WRAP_COLS, 0)));
    }

    if (active_index >= start)
    {
        story_type* st = &st_info[sel_idx[active_index]];
        cptr heading = st_name + st->name;
        cptr text = st_text + st->text;

        if (!story_append_paragraph(scene, panel, TERM_L_BLUE, active_alpha,
                heading)
            || !story_append_paragraph(scene, panel, TERM_WHITE, active_alpha,
                text))
        {
            return false;
        }

        story_add_paragraph_line_count(&total_lines, &paragraph_count, 1);
        story_add_paragraph_line_count(&total_lines, &paragraph_count,
            MAX(1, story_count_wrapped_lines(text, STORY_BROWSER_WRAP_COLS, 0)));
    }

    if (total_lines > STORY_BROWSER_VISIBLE_LINES)
    {
        app_ui_panel_set_row_offset(panel,
            (s16b)(total_lines - STORY_BROWSER_VISIBLE_LINES));
    }

    if (footer_text && footer_text[0]
        && !app_ui_panel_add_body_line(panel, footer_attr, footer_text))
    {
        return false;
    }

    if (active_index < 0
        && !app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "Space", "Continue"))
    {
        return false;
    }
    if (active_index < 0 && allow_index
        && !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "i", "Index"))
    {
        return false;
    }

    return true;
}

static bool story_build_index_scene(app_ui_scene* scene, const int* sel_idx,
    int start, int complete_count, int selected)
{
    app_ui_panel* panel;
    int total = complete_count - start;

    if (!scene || !sel_idx || start < 0 || complete_count <= start)
        return false;
    if (selected < start || selected >= complete_count)
        selected = start;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS
        | APP_UI_PANEL_FLAG_SHOW_DETAIL;
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->selected_row = (s16b)(selected - start);
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_icon(panel, TERM_YELLOW, '*');
    app_ui_panel_set_widths(panel, STORY_BROWSER_MIN_WIDTH,
        STORY_BROWSER_MAX_WIDTH);
    app_ui_panel_set_title(panel, TERM_YELLOW, "The Tale So Far");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Chapter index");

    for (int i = start; i < complete_count; i++)
    {
        story_type* st = &st_info[sel_idx[i]];
        char key[APP_UI_KEY_MAX];
        char meta[APP_UI_META_MAX];
        byte attr = (i == selected) ? TERM_L_BLUE : TERM_WHITE;

        strnfmt(key, sizeof(key), "%d", i - start + 1);
        strnfmt(meta, sizeof(meta), "Chapter %d of %d", i - start + 1,
            total);
        if (!app_ui_panel_add_row(panel, (s16b)i, attr, true, i == selected,
                key, st_name + st->name, meta))
        {
            return false;
        }
    }

    {
        story_type* st = &st_info[sel_idx[selected]];

        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, st_name + st->name);
        if (!story_add_wrapped_detail_lines(panel, TERM_WHITE,
                st_text + st->text, 10))
        {
            return false;
        }
        if ((int)panel->detail_line_count >= 10
            && !app_ui_panel_add_detail_line(panel, TERM_SLATE,
                "Open chapter to read the full text."))
        {
            return false;
        }
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Enter", "Open")
        && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "8", "Up")
        && app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "2", "Down")
        && app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "Esc", "Close");
}

static bool story_build_chapter_scene(app_ui_scene* scene, const int* sel_idx,
    int start, int complete_count, int selected)
{
    app_ui_panel* panel;
    story_type* st;
    char subtitle[APP_UI_TEXT_MAX];

    if (!scene || !sel_idx || selected < start || selected >= complete_count)
        return false;

    st = &st_info[sel_idx[selected]];
    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_DOCUMENT;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_icon(panel, TERM_YELLOW, '*');
    app_ui_panel_set_widths(panel, STORY_BROWSER_MIN_WIDTH,
        STORY_BROWSER_MAX_WIDTH);
    app_ui_panel_set_title(panel, TERM_YELLOW, st_name + st->name);
    strnfmt(subtitle, sizeof(subtitle), "Chapter %d of %d",
        selected - start + 1, complete_count - start);
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

    if (!story_append_paragraph(scene, panel, TERM_WHITE, 0xFFu,
            st_text + st->text))
    {
        return false;
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_WHITE,
            selected > start, "4", "Prev")
        && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE,
            selected + 1 < complete_count, "6", "Next")
        && app_ui_panel_add_footer_action(panel, 3, TERM_L_BLUE, true,
            "i", "Index")
        && app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "Esc", "Close");
}

static bool story_present_progress(const int* sel_idx, int start,
    int complete_count, int active_index, byte active_alpha, cptr footer_text,
    byte footer_attr)
{
    app_ui_scene scene;

    if (!story_build_browser_scene(&scene, sel_idx, start, complete_count,
            active_index, active_alpha, footer_text, footer_attr, false))
    {
        return false;
    }

    return ui_information_scene_present_ui(&scene);
}

static char story_wait_browser_key(story_browser_mode mode, int* selected,
    int start, int complete_count)
{
    ui_information_scene_event event;

    while (ui_information_scene_wait_event(&event, 0))
    {
        const app_ui_command* command;

        if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            return (char)event.key;
        if (event.kind != UI_INFORMATION_SCENE_EVENT_COMMAND)
            continue;

        command = &event.command;
        if (command->kind == APP_UI_COMMAND_KIND_CANCEL
            || command->target.action == APP_UI_WIDGET_ACTION_CANCEL)
        {
            return ESCAPE;
        }

        if (command->kind == APP_UI_COMMAND_KIND_SCROLL
            || command->kind == APP_UI_COMMAND_KIND_FOCUS)
        {
            char dir_key = story_direction_command_key(command);

            if (mode == STORY_BROWSER_DOCUMENT)
                return '\0';
            if (dir_key)
                return dir_key;
        }

        if (mode == STORY_BROWSER_INDEX
            && command->target.role == APP_UI_WIDGET_ROLE_LIST_ITEM)
        {
            int row_id = command->target.widget_id;

            if (row_id >= start && row_id < complete_count && selected)
                *selected = row_id;
            if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
                return '\0';
            if (command->kind == APP_UI_COMMAND_KIND_ACTIVATE
                || command->kind == APP_UI_COMMAND_KIND_SELECT
                || command->target.action == APP_UI_WIDGET_ACTION_ACTIVATE
                || command->target.action == APP_UI_WIDGET_ACTION_SELECT)
            {
                return '\r';
            }
            return '\0';
        }

        if (command->target.role == APP_UI_WIDGET_ROLE_BUTTON)
        {
            switch (mode)
            {
            case STORY_BROWSER_DOCUMENT:
                if (command->target.widget_id == 1)
                    return ' ';
                if (command->target.widget_id == 2)
                    return 'i';
                break;
            case STORY_BROWSER_INDEX:
                if (command->target.widget_id == 1)
                    return '\r';
                if (command->target.widget_id == 2)
                    return '8';
                if (command->target.widget_id == 3)
                    return '2';
                if (command->target.widget_id == 4)
                    return ESCAPE;
                break;
            case STORY_BROWSER_CHAPTER:
                if (command->target.widget_id == 1)
                    return '4';
                if (command->target.widget_id == 2)
                    return '6';
                if (command->target.widget_id == 3)
                    return 'i';
                if (command->target.widget_id == 4)
                    return ESCAPE;
                break;
            }
        }
    }

    return ESCAPE;
}

static void story_browse_completed_chapters(const int* sel_idx, int start,
    int complete_count, cptr final_prompt)
{
    story_browser_mode mode = STORY_BROWSER_DOCUMENT;
    int selected = start;

    if (!sel_idx || complete_count <= start)
        return;

    while (true)
    {
        app_ui_scene scene;
        char ch = '\0';

        if (mode == STORY_BROWSER_DOCUMENT)
        {
            if (!story_build_browser_scene(&scene, sel_idx, start,
                    complete_count, -1, 0xFFu, final_prompt, TERM_L_WHITE,
                    true))
            {
                log_warn("story display: final document scene failed");
                return;
            }
        }
        else if (mode == STORY_BROWSER_INDEX)
        {
            if (!story_build_index_scene(&scene, sel_idx, start,
                    complete_count, selected))
            {
                log_warn("story display: chapter index scene failed");
                return;
            }
        }
        else
        {
            if (!story_build_chapter_scene(&scene, sel_idx, start,
                    complete_count, selected))
            {
                log_warn("story display: chapter scene failed");
                return;
            }
        }

        if (!ui_information_scene_present_ui(&scene))
        {
            log_warn("story display: browser scene presentation failed");
            return;
        }

        ch = story_wait_browser_key(mode, &selected, start, complete_count);
        if (!ch)
            continue;

        if (mode == STORY_BROWSER_DOCUMENT)
        {
            if (ch == 'i' || ch == 'I' || ch == '\t')
            {
                mode = STORY_BROWSER_INDEX;
                continue;
            }
            return;
        }

        if (mode == STORY_BROWSER_INDEX)
        {
            if (ch == ESCAPE || ch == 'q' || ch == 'Q')
                return;
            if (ch == '8' || ch == 'k' || ch == '-')
            {
                if (selected > start)
                    selected--;
                continue;
            }
            if (ch == '2' || ch == 'j' || ch == '+')
            {
                if (selected + 1 < complete_count)
                    selected++;
                continue;
            }
            if (ch == '\r' || ch == '\n' || ch == ' ')
            {
                mode = STORY_BROWSER_CHAPTER;
                continue;
            }
            if (isdigit((unsigned char)ch))
            {
                int target = start + (ch - '1');

                if (target >= start && target < complete_count)
                {
                    selected = target;
                    mode = STORY_BROWSER_CHAPTER;
                }
                continue;
            }
            continue;
        }

        if (ch == ESCAPE || ch == 'q' || ch == 'Q')
            return;
        if (ch == 'i' || ch == 'I' || ch == '\t')
        {
            mode = STORY_BROWSER_INDEX;
            continue;
        }
        if (ch == '4' || ch == '8' || ch == 'k' || ch == '-')
        {
            if (selected > start)
                selected--;
            continue;
        }
        if (ch == '6' || ch == '2' || ch == 'j' || ch == '+'
            || ch == ' ' || ch == '\r' || ch == '\n')
        {
            if (selected + 1 < complete_count)
            {
                selected++;
                continue;
            }
            return;
        }
    }
}

static int story_poll_skip_input(void)
{
    char ch;

    if (!story_peek_key(&ch))
        return 0;

    story_consume_peeked_key(&ch);
    return (ch == ESCAPE) ? 2 : 1;
}

static int story_delay_with_skip(u32b total_ms)
{
    u32b elapsed = 0;

    while (elapsed < total_ms)
    {
        u32b slice = MIN((u32b)25, total_ms - elapsed);
        int key_state = story_poll_skip_input();

        if (key_state != 0)
            return key_state;

        platform_frame_delay_ms(slice);
        elapsed += slice;
    }

    return 0;
}

static byte story_fade_alpha(u32b elapsed_ms, u32b duration_ms)
{
    if (duration_ms == 0 || elapsed_ms >= duration_ms)
        return 0xFFu;

    return (byte)((elapsed_ms * 255u) / duration_ms);
}

static int story_render_fade_sequence(const int* sel_idx, int start,
    int complete_count, int active_index, cptr footer_text)
{
    u64b start_ms = platform_monotonic_ms();

    while (true)
    {
        u64b now_ms = platform_monotonic_ms();
        u64b elapsed_ms64 = (now_ms > start_ms) ? (now_ms - start_ms) : 0;
        u32b elapsed_ms = (elapsed_ms64 > STORY_FADE_DURATION_MS)
            ? STORY_FADE_DURATION_MS
            : (u32b)elapsed_ms64;
        byte alpha = story_fade_alpha(elapsed_ms, STORY_FADE_DURATION_MS);
        int key_state;

        if (!story_present_progress(sel_idx, start, complete_count,
                active_index, alpha, footer_text, TERM_SLATE))
        {
            return -1;
        }
        if (elapsed_ms >= STORY_FADE_DURATION_MS)
            break;

        key_state = story_delay_with_skip(MIN(STORY_FADE_FRAME_SLICE_MS,
            STORY_FADE_DURATION_MS - elapsed_ms));
        if (key_state == 2)
            return 2;
        if (key_state == 1)
        {
            if (!story_present_progress(sel_idx, start, complete_count,
                    active_index, 0xFFu, footer_text, TERM_SLATE))
            {
                return -1;
            }
            return 1;
        }
    }

    return story_delay_with_skip(STORY_REVEAL_HOLD_MS);
}

void print_story(int last_parts, bool fade_in,
    bool restore_previous_snapshot)
{
    ui_information_scene_scope info_scope;
    app_session* session = app_session_current();
    bool fast_forward = false;
    bool scene_failed = false;
    bool saved_hide_cursor = false;
    int sils = metar.silmarils;
    byte rt = metar.type;
    int total = 0;
    int max_st = z_info->st_max;
    static int sel_idx[1024];
    int start;
    int complete_count;
    char reveal_prompt[80];
    char final_prompt[64];

    log_debug("=== Starting story display (parts=%d, fade_in=%s) ===",
        last_parts, fade_in ? "true" : "false");
    log_debug("last_parts=%d, fade_in=%s", last_parts,
        fade_in ? "true" : "false");

    if (max_st > (int)N_ELEMENTS(sel_idx))
        max_st = (int)N_ELEMENTS(sel_idx);

    log_debug("Building story list: sils=%d, rt=%d, max_st=%d",
        sils, rt, max_st);

    for (int i = 0; i < max_st; i++)
    {
        story_type* st = &st_info[i];

        if (!st->name && !st->text)
            continue;
        if (st->st_type != 0)
            continue;
        if (!(st->runtypes == 0 ||
            (rt < 32 && (st->runtypes & (1UL << rt)))))
            continue;
        if (st->order <= (byte)sils)
        {
            sel_idx[total++] = i;
            log_trace("Added story %d (order=%d) to selection", i, st->order);
        }
    }

    log_debug("Found %d matching stories for display", total);
    if (total == 0)
    {
        log_debug("No stories match criteria - sils=%d, rt=%d", sils, rt);
        return;
    }

    for (int i = 1; i < total; i++)
    {
        int key = sel_idx[i];
        byte key_ord = st_info[key].order;
        int j = i - 1;

        while (j >= 0 && st_info[sel_idx[j]].order > key_ord)
        {
            sel_idx[j + 1] = sel_idx[j];
            j--;
        }
        sel_idx[j + 1] = key;
    }

    start = (last_parts > 0 && last_parts < total) ? total - last_parts : 0;
    log_debug("Story range: start=%d, total=%d", start, total);

    if (!ui_information_scene_enter(&info_scope))
    {
        log_warn("story display: semantic scene entry required");
        return;
    }

    /* Clear captured legacy key events without forcing an intermediate frame. */
    if (session)
        app_session_clear_inputs(session);

    saved_hide_cursor = inkey_cursor_hidden();
    inkey_set_cursor_hidden(true);

    platform_story_font_enable();
    story_build_reveal_prompt(reveal_prompt, sizeof(reveal_prompt));
    story_build_final_prompt(final_prompt, sizeof(final_prompt));
    complete_count = start;

    for (int idx = start; idx < total; idx++)
    {
        int result = 0;

        if (fade_in && !fast_forward)
        {
            result = story_render_fade_sequence(sel_idx, start, complete_count,
                idx, reveal_prompt);
            if (result < 0)
            {
                log_warn("story display: semantic fade presentation failed");
                scene_failed = true;
                goto cleanup;
            }
        }
        else
        {
            if (!story_present_progress(sel_idx, start, complete_count, idx,
                    0xFFu, reveal_prompt, TERM_SLATE))
            {
                log_warn("story display: semantic paragraph presentation failed");
                scene_failed = true;
                goto cleanup;
            }
            result = fast_forward ? 0 : story_delay_with_skip(
                STORY_REVEAL_HOLD_MS);
        }

        if (result == 2)
        {
            fast_forward = true;
            fade_in = false;
            log_debug("Story display: enabling fast forward mode");
        }

        complete_count = idx + 1;
    }

    story_browse_completed_chapters(sel_idx, start, complete_count,
        final_prompt);

cleanup:
    platform_story_font_disable();
    if (!scene_failed && !restore_previous_snapshot)
        ui_information_scene_leave_without_restore(&info_scope);
    else
        ui_information_scene_leave(&info_scope);
    inkey_set_cursor_hidden(saved_hide_cursor);

    if (scene_failed)
        log_warn("story display exited early because semantic rendering failed");
    log_debug("Story display completed");
}

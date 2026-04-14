/* File: ui/ui-story.c */

#include "angband.h"
#include "externs.h"

#include "log/log.h"
#include "app/app-ui.h"
#include "platform-input.h"
#include "platform-story-font.h"
#include "metarun.h"
#include "ui/story_font.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-story.h"

typedef struct story_semantic_row {
    bool used;
    s16b col;
    byte attr;
    byte story;
    char text[APP_UI_TEXT_MAX];
} story_semantic_row;

typedef struct story_semantic_scene {
    bool active;
    bool failed;
    int wid;
    int hgt;
    story_semantic_row rows[APP_UI_DOCUMENT_OP_MAX];
} story_semantic_scene;

static story_semantic_scene g_story_semantic_scene;

static byte story_current_flags(void)
{
    byte flags = 0;

    if (sdl_is_story_font_enabled())
        flags |= STORY_FLAG_USE;
    if (sdl_is_story_font_grid())
        flags |= STORY_FLAG_CELL_ALIGN;

    return flags;
}

static void story_semantic_end(void)
{
    memset(&g_story_semantic_scene, 0, sizeof(g_story_semantic_scene));
}

static void story_semantic_clear(void)
{
    if (!g_story_semantic_scene.active)
        return;

    memset(g_story_semantic_scene.rows, 0, sizeof(g_story_semantic_scene.rows));
}

static void story_semantic_begin(int wid, int hgt)
{
    story_semantic_end();

    if (wid <= 0 || hgt <= 0 || hgt > (int)N_ELEMENTS(g_story_semantic_scene.rows))
    {
        g_story_semantic_scene.failed = true;
        return;
    }

    g_story_semantic_scene.active = true;
    g_story_semantic_scene.wid = wid;
    g_story_semantic_scene.hgt = hgt;
}

static void story_semantic_clear_row(int row)
{
    if (!g_story_semantic_scene.active || row < 0
        || row >= g_story_semantic_scene.hgt
        || row >= (int)N_ELEMENTS(g_story_semantic_scene.rows))
    {
        return;
    }

    memset(&g_story_semantic_scene.rows[row], 0,
        sizeof(g_story_semantic_scene.rows[row]));
}

static void story_semantic_set_row(int row, int col, byte attr, byte story,
    cptr text)
{
    story_semantic_row* semantic_row;

    if (!g_story_semantic_scene.active || row < 0 || col < 0
        || row >= g_story_semantic_scene.hgt
        || row >= (int)N_ELEMENTS(g_story_semantic_scene.rows))
    {
        return;
    }

    semantic_row = &g_story_semantic_scene.rows[row];
    memset(semantic_row, 0, sizeof(*semantic_row));
    if (!text || !text[0])
        return;

    semantic_row->used = true;
    semantic_row->col = (s16b)col;
    semantic_row->attr = attr;
    semantic_row->story = story;
    SDL_strlcpy(semantic_row->text, text, sizeof(semantic_row->text));
}

static int story_measure_text_width(cptr text, int len, bool use_story,
    int cell_width)
{
    int width;

    if (!text || len <= 0)
        return 0;

    if (!use_story)
        return len * cell_width;

    width = sdl_story_font_text_width(text, len);
    if (width <= 0)
        width = len * cell_width;
    return width;
}

static int story_space_width(bool use_story, int cell_width)
{
    int width;

    if (!use_story)
        return cell_width;

    width = sdl_story_font_text_width(" ", 1);
    if (width <= 0)
        width = cell_width;
    return width;
}

static int story_count_wrapped_lines(cptr text, int wrap_width, int indent)
{
    if (sdl_is_story_font_enabled())
        return count_wrapped_lines_story(text, wrap_width, indent);

    return count_wrapped_lines(text, wrap_width, indent);
}

static bool story_semantic_mirror_wrapped_text(cptr text, int row, int indent,
    int wrap_width, byte attr, byte story)
{
    const char* s = text ? text : "";
    bool use_story = (story & STORY_FLAG_USE) != 0;
    int cell_width = sdl_get_cell_width();
    int wrap_pixels;
    int indent_pixels;
    int current_pixels;
    int space_pixels;
    int current_row = row;
    bool wrote_any = false;
    char line[APP_UI_TEXT_MAX];
    int line_len = 0;

    if (!g_story_semantic_scene.active)
        return true;

    if (cell_width <= 0)
        cell_width = 1;
    if (wrap_width <= 0)
        wrap_width = (g_story_semantic_scene.wid > 0)
            ? g_story_semantic_scene.wid
            : 80;

    wrap_pixels = wrap_width * cell_width;
    indent_pixels = indent * cell_width;
    current_pixels = indent_pixels;
    space_pixels = story_space_width(use_story, cell_width);
    if (space_pixels <= 0)
        space_pixels = cell_width;

    while (*s)
    {
        if (*s == '\n')
        {
            line[line_len] = '\0';
            if (line_len > 0)
                story_semantic_set_row(current_row, indent, attr, story, line);
            else
                story_semantic_clear_row(current_row);
            wrote_any = true;
            current_row++;
            line_len = 0;
            current_pixels = indent_pixels;
            s++;
            continue;
        }

        while (*s == ' ')
        {
            if (line_len < (int)sizeof(line) - 1)
                line[line_len++] = ' ';
            current_pixels += space_pixels;
            s++;

            if (current_pixels >= wrap_pixels)
            {
                line[line_len] = '\0';
                if (line_len > 0)
                    story_semantic_set_row(current_row, indent, attr, story,
                        line);
                else
                    story_semantic_clear_row(current_row);
                wrote_any = true;
                current_row++;
                line_len = 0;
                current_pixels = indent_pixels;
            }
        }

        if (!*s)
            break;
        if (*s == '\n')
            continue;

        {
            const char* word_start = s;
            int word_chars = 0;
            int word_pixels;

            while (s[word_chars] && s[word_chars] != ' ' && s[word_chars] != '\n')
                word_chars++;
            if (word_chars == 0)
                continue;

            word_pixels = story_measure_text_width(word_start, word_chars,
                use_story, cell_width);
            if (current_pixels > indent_pixels
                && (current_pixels + word_pixels) > wrap_pixels)
            {
                line[line_len] = '\0';
                if (line_len > 0)
                    story_semantic_set_row(current_row, indent, attr, story,
                        line);
                else
                    story_semantic_clear_row(current_row);
                wrote_any = true;
                current_row++;
                line_len = 0;
                current_pixels = indent_pixels;
            }

            for (int i = 0; i < word_chars; i++)
            {
                if (line_len < (int)sizeof(line) - 1)
                    line[line_len++] = word_start[i];
            }

            current_pixels += word_pixels;
            s += word_chars;
        }
    }

    line[line_len] = '\0';
    if (line_len > 0 || !wrote_any)
        story_semantic_set_row(current_row, indent, attr, story, line);

    return true;
}

static bool story_semantic_present(void)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    bool wrote_any = false;

    if (!g_story_semantic_scene.active || g_story_semantic_scene.failed)
        return false;

    app_ui_scene_init(&scene);
    panel = app_ui_scene_append_panel(&scene, APP_UI_LAYER_BROWSER);
    if (!panel)
    {
        g_story_semantic_scene.failed = true;
        return false;
    }

    panel->style = APP_UI_PANEL_STYLE_DOCUMENT;
    panel->min_width_px = 0;
    panel->width_cap_px = 0;

    for (int row = 0; row < g_story_semantic_scene.hgt; row++)
    {
        story_semantic_row* semantic_row = &g_story_semantic_scene.rows[row];

        if (!semantic_row->used || !semantic_row->text[0])
            continue;

        if (!app_ui_panel_add_document_text_ex(&scene, panel, (s16b)row,
                semantic_row->col, semantic_row->attr, semantic_row->story,
                semantic_row->text))
        {
            g_story_semantic_scene.failed = true;
            return false;
        }

        wrote_any = true;
    }

    if (!wrote_any)
    {
        if (!app_ui_panel_add_document_text(&scene, panel, 0, 0, TERM_WHITE,
                " "))
        {
            g_story_semantic_scene.failed = true;
            return false;
        }
    }

    if (!ui_information_scene_present_ui(&scene))
    {
        g_story_semantic_scene.failed = true;
        return false;
    }

    return true;
}

static void story_clear_screen(void)
{
    story_semantic_clear();
}

static void story_erase_row(int col, int row, int width)
{
    (void)col;
    (void)width;
    story_semantic_clear_row(row);
}

static void story_putstr(int col, int row, byte attr, cptr text)
{
    if (!text || !text[0])
    {
        story_semantic_clear_row(row);
        return;
    }

    story_semantic_set_row(row, col, attr, story_current_flags(), text);
}

static int story_draw_wrapped_text(byte attr, cptr text, int row, int indent,
    int wrap_width)
{
    int lines = story_count_wrapped_lines(text, wrap_width, indent);

    if (lines < 1)
        lines = 1;
    (void)story_semantic_mirror_wrapped_text(text, row, indent, wrap_width,
        attr, story_current_flags());
    return row + lines - 1;
}

static bool story_present(void)
{
    return story_semantic_present();
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

static char story_wait_key(void)
{
    return (char)ui_information_scene_wait_key();
}

/*
 * Helper: colour fade-in paragraph printer
 * Return values: 0=completed normally, 1=other key pressed (skip paragraph),
 * 2=ESC pressed (fast-forward)
 */
static int print_paragraph_fade(cptr text, int row, int indent, int wrap_width)
{
    const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, TERM_L_WHITE, TERM_WHITE };
    const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));

    for (int s = 0; s < steps; s++)
    {
        char ch;

        if (story_peek_key(&ch))
        {
            story_consume_peeked_key(&ch);
            (void)story_draw_wrapped_text(TERM_WHITE, text, row, indent,
                wrap_width);
            (void)story_present();
            return (ch == ESCAPE) ? 2 : 1;
        }

        (void)story_draw_wrapped_text(fade_cols[s], text, row, indent,
            wrap_width);
        (void)story_present();
        Term_xtra(TERM_XTRA_DELAY, 125);
    }

    {
        char ch;

        if (story_peek_key(&ch))
        {
            story_consume_peeked_key(&ch);
            return (ch == ESCAPE) ? 2 : 1;
        }
    }

    Term_xtra(TERM_XTRA_DELAY, 1000);
    return 0;
}

static void story_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static void story_print_hint(int indent, int h)
{
    if (steamdeck_controls_active()) {
        char next_label[16];
        char esc_label[16];
        char prompt_buf[80];

        story_prompt_label(' ', "A", next_label, sizeof(next_label));
        story_prompt_label(ESCAPE, "ESC", esc_label, sizeof(esc_label));

        strnfmt(prompt_buf, sizeof(prompt_buf),
            "[%s] next  *  [%s] fast forward", next_label, esc_label);
        story_putstr(indent, h - 1, TERM_SLATE, prompt_buf);
    } else {
        story_putstr(indent, h - 1, TERM_SLATE,
            "[Enter] next  *  [Esc] fast forward");
    }
}

void print_story(int last_parts, bool fade_in)
{
    int wid, h;
    const int indent = 2;
    ui_information_scene_scope info_scope;
    bool fast_forward = false;
    bool scene_failed = false;
    bool show_page_instantly = false;
    bool saved_hide_cursor = false;
    int sils = metar.silmarils;
    byte rt = metar.type;
    int total = 0;
    int max_st = z_info->st_max;
    static int sel_idx[1024];
    int start;

    log_debug("=== Starting story display (parts=%d, fade_in=%s) ===",
        last_parts, fade_in ? "true" : "false");
    log_debug("last_parts=%d, fade_in=%s", last_parts,
        fade_in ? "true" : "false");

#define REDRAW_HINT() story_print_hint(indent, h)

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

    Term_get_size(&wid, &h);
    story_semantic_begin(wid, h);
    story_clear_screen();
    saved_hide_cursor = inkey_cursor_hidden();
    inkey_set_cursor_hidden(true);

    sdl_story_font_enable();

    story_putstr(indent, 0, TERM_YELLOW, "=== The Tale So Far ===");

    {
        int row = 2;

        REDRAW_HINT();

        for (int idx = start; idx < total; idx++)
        {
            story_type* st = &st_info[sel_idx[idx]];
            int wrap_width = wid - indent - 1;
            cptr text = st_text + st->text;
            int text_lines;
            int estimated_space_needed;
            bool will_add_blank_line;
            int space_needed;
            bool paginated = false;

            if (wrap_width < 20)
                wrap_width = 20;

            text_lines = story_count_wrapped_lines(text, wrap_width, indent);
            estimated_space_needed = 1 + text_lines + 1;

            if (row + estimated_space_needed >= h - 2)
            {
                if (!fast_forward)
                {
                    char ch;

                    show_page_instantly = false;
                    REDRAW_HINT();
                    if (!story_present())
                    {
                        log_warn("story display: semantic page presentation failed");
                        scene_failed = true;
                        goto cleanup;
                    }
                    ch = story_wait_key();
                    if (ch == ESCAPE)
                    {
                        fast_forward = true;
                        fade_in = false;
                        story_erase_row(0, h - 1, wid);
                        log_debug(
                            "User pressed ESC - enabling fast forward mode");
                    }
                    else
                    {
                        row = 2;
                        story_clear_screen();
                        story_putstr(indent, 0, TERM_YELLOW,
                            "=== The Tale So Far ===");
                        REDRAW_HINT();
                    }
                }
                else
                {
                    row = 2;
                    story_clear_screen();
                    story_putstr(indent, 0, TERM_YELLOW,
                        "=== The Tale So Far ===");
                }
            }

            story_putstr(indent, row, TERM_L_BLUE, st_name + st->name);
            row++;

            if (fade_in && !fast_forward && !show_page_instantly)
            {
                int fade_result =
                    print_paragraph_fade(text, row, indent, wrap_width);

                if (g_story_semantic_scene.failed)
                {
                    log_warn("story display: semantic fade presentation failed");
                    scene_failed = true;
                    goto cleanup;
                }
                if (fade_result == 2)
                {
                    fast_forward = true;
                    fade_in = false;
                    log_debug(
                        "ESC pressed during fade - enabling fast forward mode");
                }
            }
            else
            {
                (void)story_draw_wrapped_text(TERM_WHITE, text, row, indent,
                    wrap_width);
                if (!story_present())
                {
                    log_warn("story display: semantic paragraph presentation failed");
                    scene_failed = true;
                    goto cleanup;
                }
                if (!fast_forward && !show_page_instantly)
                    Term_xtra(TERM_XTRA_DELAY, 1000);
            }

            row += story_count_wrapped_lines(text, wrap_width, indent);

            will_add_blank_line = (idx < total - 1);
            space_needed = will_add_blank_line ? 1 : 0;

            if (row + space_needed >= h - 2)
            {
                paginated = true;
                if (!fast_forward)
                {
                    char ch;

                    show_page_instantly = false;
                    REDRAW_HINT();
                    if (!story_present())
                    {
                        log_warn("story display: semantic pagination prompt failed");
                        scene_failed = true;
                        goto cleanup;
                    }
                    ch = story_wait_key();
                    if (ch == ESCAPE)
                    {
                        fast_forward = true;
                        fade_in = false;
                        story_erase_row(0, h - 1, wid);
                        log_debug(
                            "User pressed ESC - enabling fast forward mode");
                    }
                    else
                    {
                        row = 2;
                        story_clear_screen();
                        sdl_story_font_enable();
                        story_putstr(indent, 0, TERM_YELLOW,
                            "=== The Tale So Far ===");
                        sdl_story_font_disable();
                        REDRAW_HINT();
                        continue;
                    }
                }
                else
                {
                    row = 2;
                    story_clear_screen();
                    story_putstr(indent, 0, TERM_YELLOW,
                        "=== The Tale So Far ===");
                }
            }

            if (will_add_blank_line && !paginated)
            {
                story_putstr(indent, row, TERM_WHITE, "");
                row++;
            }
        }
    }

    story_erase_row(0, h - 1, wid);
    if (steamdeck_controls_active()) {
        char next_label[16];
        char prompt_buf[64];

        story_prompt_label(' ', "A", next_label, sizeof(next_label));
        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] continue", next_label);
        story_putstr(indent, h - 1, TERM_L_WHITE, prompt_buf);
    } else {
        story_putstr(indent, h - 1, TERM_L_WHITE,
            "[Press any key to continue]");
    }
    if (!story_present())
    {
        log_warn("story display: semantic final prompt failed");
        scene_failed = true;
        goto cleanup;
    }
    (void)story_wait_key();

cleanup:
    sdl_story_font_disable();
    ui_information_scene_leave(&info_scope);
    story_semantic_end();
    inkey_set_cursor_hidden(saved_hide_cursor);

    if (scene_failed)
        log_warn("story display exited early because semantic rendering failed");
    log_debug("Story display completed");

#undef REDRAW_HINT
}

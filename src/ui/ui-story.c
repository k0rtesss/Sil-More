/* File: ui/ui-story.c */

#include "angband.h"
#include "externs.h"

#include "log/log.h"
#include "main-sdl.h"
#include "metarun.h"
#include "ui/story_font.h"
#include "ui/ui-story.h"

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

        if (Term_inkey(&ch, false, false) == 0)
        {
            Term_inkey(&ch, false, true);
            text_out_indent = indent;
            text_out_wrap = wrap_width;
            Term_gotoxy(indent, row);
            text_out_to_screen(TERM_WHITE, text);
            text_out_wrap = 0;
            text_out_indent = 0;
            Term_fresh();
            return (ch == ESCAPE) ? 2 : 1;
        }

        text_out_indent = indent;
        text_out_wrap = wrap_width;
        Term_gotoxy(indent, row);
        text_out_to_screen(fade_cols[s], text);
        text_out_wrap = 0;
        text_out_indent = 0;
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 125);
    }

    {
        char ch;

        if (Term_inkey(&ch, false, false) == 0)
        {
            Term_inkey(&ch, false, true);
            return (ch == ESCAPE) ? 2 : 1;
        }
    }

    Term_xtra(TERM_XTRA_DELAY, 1000);
    return 0;
}

static bool banner_messages_use_stairs(void)
{
#ifdef __ANDROID__
    const bool default_value = false;
#else
    const bool default_value = true;
#endif

    if (!op_ptr)
        return default_value;

    return op_ptr->opt[OPT_banner_message_stairs];
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
        Term_putstr(indent, h - 1, -1, TERM_SLATE, prompt_buf);
    } else {
        Term_putstr(indent, h - 1, -1, TERM_SLATE,
            "[Enter] next  *  [Esc] fast forward");
    }
}

void print_fade_centered(cptr text)
{
    enum { MAX_LINES = 32, MAX_LEN = 255 };
    char lines[MAX_LINES][MAX_LEN + 1];
    const char* p = text;
    int wid, h;
    int nlines = 0;
    int start_row;

    if (!text || !*text)
        return;

    Term_get_size(&wid, &h);

    {
        int max_width = wid - 15;

        if (max_width < 10)
            max_width = (wid > 2 ? wid - 2 : wid);
        if (max_width < 1)
            max_width = 1;

        while (*p && nlines < MAX_LINES)
        {
            int linelen = 0;

            lines[nlines][0] = '\0';

            while (*p && isspace((unsigned char)*p))
                p++;

            while (*p)
            {
                const char* w = p;
                int wlen, need;

                while (*p && !isspace((unsigned char)*p))
                    p++;
                wlen = (int)(p - w);

                if (wlen > max_width && linelen == 0)
                {
                    int take = (wlen > max_width) ? max_width : wlen;

                    if (take > MAX_LEN)
                        take = MAX_LEN;
                    memcpy(lines[nlines], w, (size_t)take);
                    linelen = take;
                    lines[nlines][linelen] = '\0';
                    w += take;
                    p = w;
                    break;
                }

                need = (linelen ? 1 : 0) + wlen;
                if (linelen + need <= max_width && linelen + need <= MAX_LEN)
                {
                    if (linelen)
                        lines[nlines][linelen++] = ' ';
                    memcpy(lines[nlines] + linelen, w, (size_t)wlen);
                    linelen += wlen;
                    lines[nlines][linelen] = '\0';
                }
                else
                {
                    p = w;
                    break;
                }

                while (*p && isspace((unsigned char)*p))
                {
                    if (*p == '\n')
                        break;
                    p++;
                }
                if (*p == '\n')
                {
                    p++;
                    break;
                }
            }

            nlines++;
            while (*p == '\n')
                p++;
        }
    }

    if (nlines == 0)
        return;

    start_row = (h - nlines) / 2;
    if (start_row < 0)
        start_row = 0;

    for (int i = 0; i < nlines; i++)
    {
        int len = (int)strlen(lines[i]);
        int indent, wrap_width;

        if (len > wid)
            len = wid;
        indent = (wid - len) / 2;
        if (indent < 0)
            indent = 0;
        wrap_width = wid - indent - 1;
        if (wrap_width < len)
            wrap_width = len;
        (void)print_paragraph_fade(lines[i], start_row + i, indent, wrap_width);
    }
}

void print_fade_centered_at_row(cptr text, int row_start, bool fade_in,
    bool line_delay)
{
    const char* p = text;
    int wid, h;
    int printed_lines = 0;
    bool stair_layout;
    enum { MAX_LINES2 = 32, MAX_LEN2 = 255 };

    if (!text || !*text)
        return;

    stair_layout = banner_messages_use_stairs();
    Term_get_size(&wid, &h);

    if (row_start < 1)
        row_start = 1;
    if (row_start >= h)
        return;

    sdl_story_font_enable();
    log_debug("Depth banner: story font enabled");

    while (*p && printed_lines < MAX_LINES2 && (row_start + printed_lines) < h)
    {
        int indent = 14 + (stair_layout ? (2 * printed_lines) : 0);
        int avail;
        char buf[MAX_LEN2 + 1];
        int linelen = 0;

        if (use_bigtile && (((indent - COL_MAP) & 1) != 0))
            indent++;
        if (indent >= wid - 1)
            break;

        avail = wid - indent - 1;
        if (avail < 8)
            avail = 8;

        buf[0] = '\0';

        while (*p && (unsigned char)*p <= ' ')
        {
            if (*p == '\n')
            {
                p++;
                break;
            }
            p++;
        }

        while (*p)
        {
            const char* w;
            int wlen, need;

            if (*p == '\n')
            {
                p++;
                break;
            }

            w = p;
            while (*p && *p != '\n' && !isspace((unsigned char)*p))
                p++;
            wlen = (int)(p - w);

            if (wlen > avail && linelen == 0)
            {
                int take = (wlen > avail) ? avail : wlen;

                if (take > MAX_LEN2)
                    take = MAX_LEN2;
                memcpy(buf, w, (size_t)take);
                linelen = take;
                buf[linelen] = '\0';
                w += take;
                p = w;
                break;
            }

            need = (linelen ? 1 : 0) + wlen;
            if (linelen + need <= avail && linelen + need <= MAX_LEN2)
            {
                if (linelen)
                    buf[linelen++] = ' ';
                memcpy(buf + linelen, w, (size_t)wlen);
                linelen += wlen;
                buf[linelen] = '\0';
            }
            else
            {
                p = w;
                break;
            }

            while (*p && isspace((unsigned char)*p))
            {
                if (*p == '\n')
                    break;
                p++;
            }
            if (*p == '\n')
            {
                p++;
                break;
            }
        }

        if (linelen == 0)
            break;

        if (fade_in)
            (void)print_paragraph_fade(buf, row_start + printed_lines, indent, avail);
        else
        {
            c_put_str(TERM_ORANGE, buf, row_start + printed_lines, indent);
            Term_fresh();
        }

        printed_lines++;

        if (!fade_in && line_delay && *p && (row_start + printed_lines) < h)
            Term_xtra(TERM_XTRA_DELAY, 800);
    }

    log_debug("Depth banner: story font disabled");
    sdl_story_font_disable();
}

void print_story(int last_parts, bool fade_in)
{
    int wid, h;
    const int indent = 2;
    bool fast_forward = false;
    bool show_page_instantly = false;
    bool saved_cursor_state = false;
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

    Term_get_size(&wid, &h);
    screen_save();
    Term_clear();
    (void)Term_get_cursor(&saved_cursor_state);
    saved_hide_cursor = inkey_cursor_hidden();
    inkey_set_cursor_hidden(true);
    (void)Term_set_cursor(false);

    sdl_story_font_enable();

    Term_putstr(indent, 0, -1, TERM_YELLOW, "=== The Tale So Far ===");

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

            text_lines = count_wrapped_lines_story(text, wrap_width, indent);
            estimated_space_needed = 1 + text_lines + 1;

            if (row + estimated_space_needed >= h - 2)
            {
                if (!fast_forward)
                {
                    char ch;

                    show_page_instantly = false;
                    REDRAW_HINT();
                    ch = inkey();
                    if (ch == ESCAPE)
                    {
                        fast_forward = true;
                        fade_in = false;
                        Term_erase(0, h - 1, wid);
                        log_debug(
                            "User pressed ESC - enabling fast forward mode");
                    }
                    else
                    {
                        row = 2;
                        Term_clear();
                        Term_putstr(indent, 0, -1, TERM_YELLOW,
                            "=== The Tale So Far ===");
                        REDRAW_HINT();
                    }
                }
                else
                {
                    row = 2;
                    Term_clear();
                    Term_putstr(indent, 0, -1, TERM_YELLOW,
                        "=== The Tale So Far ===");
                }
            }

            Term_putstr(indent, row, -1, TERM_L_BLUE, st_name + st->name);
            row++;

            if (fade_in && !fast_forward && !show_page_instantly)
            {
                int fade_result =
                    print_paragraph_fade(text, row, indent, wrap_width);

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
                text_out_indent = indent;
                text_out_wrap = wrap_width;
                Term_gotoxy(indent, row);
                text_out_to_screen(TERM_WHITE, text);
                text_out_wrap = 0;
                text_out_indent = 0;
                if (!fast_forward && !show_page_instantly)
                    Term_xtra(TERM_XTRA_DELAY, 1000);
            }

            {
                int cursor_x, cursor_y;

                Term_locate(&cursor_x, &cursor_y);
                row = cursor_y + 1;
            }

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
                    ch = inkey();
                    if (ch == ESCAPE)
                    {
                        fast_forward = true;
                        fade_in = false;
                        Term_erase(0, h - 1, wid);
                        log_debug(
                            "User pressed ESC - enabling fast forward mode");
                    }
                    else
                    {
                        row = 2;
                        Term_clear();
                        sdl_story_font_enable();
                        Term_putstr(indent, 0, -1, TERM_YELLOW,
                            "=== The Tale So Far ===");
                        sdl_story_font_disable();
                        REDRAW_HINT();
                        continue;
                    }
                }
                else
                {
                    row = 2;
                    Term_clear();
                    Term_putstr(indent, 0, -1, TERM_YELLOW,
                        "=== The Tale So Far ===");
                }
            }

            if (will_add_blank_line && !paginated)
            {
                Term_putstr(indent, row, -1, TERM_WHITE, "");
                row++;
            }
        }
    }

    Term_erase(0, h - 1, wid);
    if (steamdeck_controls_active()) {
        char next_label[16];
        char prompt_buf[64];

        story_prompt_label(' ', "A", next_label, sizeof(next_label));
        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] continue", next_label);
        Term_putstr(indent, h - 1, -1, TERM_L_WHITE, prompt_buf);
    } else {
        Term_putstr(indent, h - 1, -1, TERM_L_WHITE,
            "[Press any key to continue]");
    }
    (void)inkey();

    Term_flush();

    sdl_story_font_disable();
    screen_load();
    (void)Term_set_cursor(saved_cursor_state);
    inkey_set_cursor_hidden(saved_hide_cursor);

    log_debug("Story display completed");

#undef REDRAW_HINT
}

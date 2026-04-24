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
#include "fs/file.h"
#include "support/utf8.h"

ang_file* text_out_file = NULL;
void (*text_out_hook)(byte a, cptr str);
int text_out_wrap = 0;
int text_out_indent = 0;

static void write_text_out_byte(unsigned char value)
{
    char out_byte = (char)value;
    (void)ang_file_write(text_out_file, &out_byte, 1);
}

/*
 * Print some (colored) text to the screen at the current cursor position,
 * automatically "wrapping" existing text.
 */
int count_wrapped_lines(cptr str, int wrap_width, int indent)
{
    int x = indent;
    int lines = 1;
    cptr s;

    for (s = str; s && *s;)
    {
        size_t char_len;
        int width;

        if (*s == '\n')
        {
            x = indent;
            lines++;
            s++;
            continue;
        }

        char_len = utf8_char_len(s);
        if (char_len == 0)
            break;
        width = (int)utf8_strnlen_cells(s, char_len);
        if (x >= wrap_width && *s != ' ')
        {
            x = indent;
            lines++;
        }
        x += width;
        s += char_len;
    }
    return lines;
}

/*
 * Write text to the screen with story font wrapping based on pixel width.
 * This version wraps proportional fonts to fill the available terminal width
 * instead of wrapping based on monospace character count.
 */
void text_out_to_file(byte a, cptr str)
{
    /* Current position on the line */
    static int pos = 0;

    /* Wrap width */
    int wrap = (text_out_wrap ? text_out_wrap : 75);

    /* Current location within "str" */
    cptr s = str;

    /* Unused parameter */
    (void)a;

    /* Process the string */
    while (*s)
    {
        int len;
        int available;
        int l_space = -1;
        int cells = 0;
        int n = 0;

        /* If we are at the start of the line... */
        if (pos == 0)
        {
            int i;

            /* Output the indent */
            for (i = 0; i < text_out_indent; i++)
            {
                write_text_out_byte(' ');
                pos++;
            }
        }

        available = wrap - pos;
        if (available <= 0)
        {
            write_text_out_byte('\n');
            pos = 0;
            continue;
        }

        /* Find length of line up to next newline or end-of-string */
        while (!((s[n] == '\n') || (s[n] == '\0')))
        {
            size_t char_len = utf8_char_len(s + n);
            int width;

            if (char_len == 0)
                break;
            width = (int)utf8_strnlen_cells(s + n, char_len);
            if (cells + width > available)
                break;

            /* Mark the most recent space in the string */
            if (s[n] == ' ')
                l_space = n;

            cells += width;
            n += (int)char_len;
        }
        len = n;

        /* If we have encountered no spaces */
        if ((l_space == -1) && s[n] && s[n] != '\n')
        {
            /* If we are at the start of a new line */
            if (pos == text_out_indent)
            {
                len = n;
            }
            /* HACK - Output punctuation at the end of the line */
            else if ((s[0] == ' ') || (s[0] == ',') || (s[0] == '.'))
            {
                len = 1;
            }
            else
            {
                /* Begin a new line */
                write_text_out_byte('\n');

                /* Reset */
                pos = 0;

                continue;
            }
        }
        else
        {
            /* Wrap at the newline */
            if ((s[n] == '\n') || (s[n] == '\0'))
                len = n;

            /* Wrap at the last space */
            else
                len = l_space;
        }

        /* Write that line to file */
        for (n = 0; n < len; n++)
        {
            /* Write out the character */
            write_text_out_byte((unsigned char)s[n]);
        }
        pos += (int)utf8_strnlen_cells(s, (size_t)len);

        /* Move 's' past the stuff we've written */
        s += len;

        /* If we are at the end of the string, end */
        if (*s == '\0')
            return;

        /* Skip newlines */
        if (*s == '\n')
            s++;

        /* Begin a new line */
        write_text_out_byte('\n');

        /* Reset */
        pos = 0;

        /* Skip whitespace */
        while (*s == ' ')
            s++;
    }
}

/*
 * Output text to the screen or to a file depending on the selected
 * text_out hook.
 */
void text_out(cptr str) { text_out_c(TERM_WHITE, str); }

/*
 * Output text to the screen (in color) or to a file depending on the
 * selected hook.
 */
void text_out_c(byte a, cptr str) { text_out_hook(a, str); }

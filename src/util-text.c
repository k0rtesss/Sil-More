#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "platform-ui.h"
#include "ui/story_font.h"
#include "log/log.h"

static void write_text_out_byte(unsigned char value)
{
    char byte = (char)value;
    (void)sdl_write(text_out_file, &byte, 1);
}

/*
 * Display a string on the screen using an attribute.
 *
 * At the given location, using the given attribute, if allowed,
 * add the given string.  Do not clear the line.
 */
void c_put_str(byte attr, cptr str, int row, int col)
{
    /* Position cursor, Dump the attr/text */
    Term_putstr(col, row, -1, attr, str);
}

/*
 * As above, but in "white"
 */
void put_str(cptr str, int row, int col)
{
    /* Spawn */
    Term_putstr(col, row, -1, TERM_WHITE, str);
}

/*
 * Display a string on the screen using an attribute, and clear
 * to the end of the line.
 */
void c_prt(byte attr, cptr str, int row, int col)
{
    /* Log what we're about to print, especially for line 0 */
    if (row == 0)
    {
        log_debug("c_prt: row=0 col=%d attr=%d str='%s' story_font_active=%d",
            col, attr, str, Term && Term->story_font_active ? 1 : 0);

        /* Log current buffer state before erase */
        if (Term && Term->scr)
        {
            char buffer_content[256];
            byte* scr_story = Term->scr->story[0];
            int i;
            int len = Term->wid;
            for (i = 0; i < len && i < 80; i++)
            {
                char c = Term->scr->c[0][i];
                buffer_content[i] = (c >= 32 && c <= 126) ? c : '.';
            }
            buffer_content[i] = '\0';
            log_debug("c_prt: BEFORE erase row=0 buffer='%s' story_flags[0-10]=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                buffer_content,
                scr_story[0], scr_story[1], scr_story[2], scr_story[3],
                scr_story[4], scr_story[5], scr_story[6], scr_story[7],
                scr_story[8], scr_story[9], scr_story[10]);
        }
    }

    /* Clear line, position cursor */
    Term_erase(col, row, 255);

    /* Log buffer state after erase, before adding text */
    if (row == 0 && Term && Term->scr)
    {
        byte* scr_story = Term->scr->story[0];
        log_debug("c_prt: AFTER erase row=0 story_flags[0-10]=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            scr_story[0], scr_story[1], scr_story[2], scr_story[3],
            scr_story[4], scr_story[5], scr_story[6], scr_story[7],
            scr_story[8], scr_story[9], scr_story[10]);
    }

    /* Dump the attr/text */
    Term_addstr(-1, attr, str);

    /* Log buffer state after adding text */
    if (row == 0 && Term && Term->scr)
    {
        char buffer_content[256];
        byte* scr_story = Term->scr->story[0];
        int i;
        int len = Term->wid;
        for (i = 0; i < len && i < 80; i++)
        {
            char c = Term->scr->c[0][i];
            buffer_content[i] = (c >= 32 && c <= 126) ? c : '.';
        }
        buffer_content[i] = '\0';
        log_debug("c_prt: AFTER addstr row=0 buffer='%s' story_flags[0-10]=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            buffer_content,
            scr_story[0], scr_story[1], scr_story[2], scr_story[3],
            scr_story[4], scr_story[5], scr_story[6], scr_story[7],
            scr_story[8], scr_story[9], scr_story[10]);
    }
}

/*
 * As above, but in "white"
 */
void prt(cptr str, int row, int col)
{
    /* Spawn */
    c_prt(TERM_WHITE, str, row, col);
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

    for (s = str; *s; s++)
    {
        if (*s == '\n')
        {
            x = indent;
            lines++;
            continue;
        }
        /* Printable or space */
        char ch = isprint((unsigned char)*s) ? *s : ' ';
        /* If adding this char exceeds wrap, and it's not a space, wrap */
        if (x >= wrap_width && ch != ' ')
        {
            x = indent;
            lines++;
        }
        /* Advance column */
        x++;
    }
    return lines;
}

void text_out_to_screen(byte a, cptr str)
{
    /* If story font is enabled, use pixel-based wrapping */
    if (sdl_is_story_font_enabled())
    {
        text_out_to_screen_story(a, str);
        return;
    }

    int x, y;

    int wid, h;

    int wrap;

    cptr s;

    /* Obtain the size */
    (void)Term_get_size(&wid, &h);

    /* Obtain the cursor */
    (void)Term_locate(&x, &y);

    /* Use special wrapping boundary? */
    if ((text_out_wrap > 0) && (text_out_wrap < wid))
        wrap = text_out_wrap;
    else
        wrap = wid;

    /* Process the string */
    for (s = str; *s; s++)
    {
        char ch;

        /* Force wrap */
        if (*s == '\n')
        {
            /* Wrap */
            x = text_out_indent;
            y++;

            /* Clear line, move cursor */
            Term_erase(x, y, 255);

            continue;
        }

        /* Clean up the char */
        ch = (isprint((unsigned char)*s) ? *s : ' ');

        /* Wrap words as needed */
        if ((x >= wrap - 1) && (ch != ' '))
        {
            int i, n = 0;

            byte av[256];
            char cv[256];

            /* Wrap word */
            if (x < wrap)
            {
                /* Scan existing text */
                for (i = wrap - 2; i >= 0; i--)
                {
                    /* Grab existing attr/char */
                    Term_what(i, y, &av[i], &cv[i]);

                    /* Break on space */
                    if (cv[i] == ' ')
                        break;

                    /* Track current word */
                    n = i;
                }
            }

            /* Special case */
            if (n == 0)
                n = wrap;

            /* Clear line */
            Term_erase(n, y, 255);

            /* Wrap */
            x = text_out_indent;
            y++;

            /* Clear line, move cursor */
            Term_erase(x, y, 255);

            /* Wrap the word (if any) */
            for (i = n; i < wrap - 1; i++)
            {
                /* Dump */
                Term_addch(av[i], cv[i]);

                /* Advance (no wrap) */
                if (++x > wrap)
                    x = wrap;
            }
        }

        /* Dump */
        Term_addch(a, ch);

        /* Advance */
        if (++x > wrap)
            x = wrap;
    }
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
        char ch;
        int n = 0;
        int len = wrap - pos;
        int l_space = -1;

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

        /* Find length of line up to next newline or end-of-string */
        while ((n < len) && !((s[n] == '\n') || (s[n] == '\0')))
        {
            /* Mark the most recent space in the string */
            if (s[n] == ' ')
                l_space = n;

            /* Increment */
            n++;
        }

        /* If we have encountered no spaces */
        if ((l_space == -1) && (n == len))
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
            /* Ensure the character is printable */
            ch = (isprint((unsigned char)s[n]) ? s[n] : ' ');

            /* Write out the character */
            write_text_out_byte((unsigned char)ch);

            /* Increment */
            pos++;
        }

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

/*
 * Clear part of the screen
 */
void clear_from(int row)
{
    int y;

    /* Erase requested rows */
    for (y = row; y < Term->hgt; y++)
    {
        /* Erase part of the screen */
        Term_erase(0, y, 255);
    }
}

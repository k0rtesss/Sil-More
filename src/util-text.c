#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"

#define UI_TEXT_SURFACE_WID 80
#define UI_TEXT_SURFACE_HGT 24

typedef struct ui_text_surface {
    bool initialized;
    int cursor_x;
    int cursor_y;
    char chars[UI_TEXT_SURFACE_HGT][UI_TEXT_SURFACE_WID];
    byte attrs[UI_TEXT_SURFACE_HGT][UI_TEXT_SURFACE_WID];
} ui_text_surface;

static ui_text_surface g_ui_text_surface;

static void ui_text_surface_publish_topline(void)
{
    char buf[1024];
    int end = UI_TEXT_SURFACE_WID;
    int first = 0;
    byte color = TERM_WHITE;

    while (end > 0 && g_ui_text_surface.chars[0][end - 1] == ' ')
        end--;

    if (end <= 0)
    {
        message_topline_clear_override();
        return;
    }

    while (first < end && g_ui_text_surface.chars[0][first] == ' ')
        first++;
    if (first >= end)
    {
        message_topline_clear_override();
        return;
    }

    color = g_ui_text_surface.attrs[0][first];
    if (end >= (int)sizeof(buf))
        end = (int)sizeof(buf) - 1;

    memcpy(buf, &g_ui_text_surface.chars[0][0], (size_t)end);
    buf[end] = '\0';
    message_topline_override(color, buf);
}

static void ui_text_surface_ensure(void)
{
    if (g_ui_text_surface.initialized)
        return;

    memset(&g_ui_text_surface, 0, sizeof(g_ui_text_surface));
    for (int y = 0; y < UI_TEXT_SURFACE_HGT; y++)
    {
        for (int x = 0; x < UI_TEXT_SURFACE_WID; x++)
        {
            g_ui_text_surface.chars[y][x] = ' ';
            g_ui_text_surface.attrs[y][x] = TERM_WHITE;
        }
    }
    g_ui_text_surface.initialized = true;
}

static void ui_text_surface_set_cursor(int x, int y)
{
    ui_text_surface_ensure();
    if (x < 0)
        x = 0;
    if (x >= UI_TEXT_SURFACE_WID)
        x = UI_TEXT_SURFACE_WID - 1;
    if (y < 0)
        y = 0;
    if (y >= UI_TEXT_SURFACE_HGT)
        y = UI_TEXT_SURFACE_HGT - 1;
    g_ui_text_surface.cursor_x = x;
    g_ui_text_surface.cursor_y = y;
}

static void ui_text_surface_get_cursor(int* x, int* y)
{
    ui_text_surface_ensure();
    if (x)
        *x = g_ui_text_surface.cursor_x;
    if (y)
        *y = g_ui_text_surface.cursor_y;
}

static void ui_text_surface_erase(int col, int row, int width)
{
    int end;

    ui_text_surface_ensure();
    if (row < 0 || row >= UI_TEXT_SURFACE_HGT)
        return;
    if (col < 0)
        col = 0;
    if (col >= UI_TEXT_SURFACE_WID)
        return;

    end = (width <= 0 || width >= 255) ? UI_TEXT_SURFACE_WID
                                        : MIN(UI_TEXT_SURFACE_WID, col + width);
    for (int x = col; x < end; x++)
    {
        g_ui_text_surface.chars[row][x] = ' ';
        g_ui_text_surface.attrs[row][x] = TERM_WHITE;
    }

    ui_text_surface_set_cursor(col, row);
    if (row == 0)
        ui_text_surface_publish_topline();
}

static void ui_text_surface_put_char(int col, int row, byte attr, char ch)
{
    unsigned char uch = (unsigned char)ch;

    ui_text_surface_ensure();
    if (row < 0 || row >= UI_TEXT_SURFACE_HGT || col < 0
        || col >= UI_TEXT_SURFACE_WID)
    {
        return;
    }

    g_ui_text_surface.chars[row][col]
        = (char)(isprint(uch) || ch == ' ' ? ch : ' ');
    g_ui_text_surface.attrs[row][col] = attr;
}

static void ui_text_surface_put_string(int col, int row, byte attr, cptr str)
{
    int x = col;

    ui_text_surface_ensure();
    if (!str || row < 0 || row >= UI_TEXT_SURFACE_HGT || col >= UI_TEXT_SURFACE_WID)
        return;
    if (x < 0)
        x = 0;

    while (*str && x < UI_TEXT_SURFACE_WID)
    {
        ui_text_surface_put_char(x, row, attr, *str);
        x++;
        str++;
    }

    ui_text_surface_set_cursor(x < UI_TEXT_SURFACE_WID ? x : UI_TEXT_SURFACE_WID - 1,
        row);
    if (row == 0)
        ui_text_surface_publish_topline();
}

static void ui_text_surface_add_char(byte attr, char ch)
{
    int x;
    int y;

    ui_text_surface_get_cursor(&x, &y);
    ui_text_surface_put_char(x, y, attr, ch);
    if (x < UI_TEXT_SURFACE_WID - 1)
        x++;
    ui_text_surface_set_cursor(x, y);
    if (y == 0)
        ui_text_surface_publish_topline();
}

static void ui_text_surface_get_cell(int col, int row, byte* attr, char* ch)
{
    ui_text_surface_ensure();
    if (attr)
        *attr = TERM_WHITE;
    if (ch)
        *ch = ' ';
    if (row < 0 || row >= UI_TEXT_SURFACE_HGT || col < 0
        || col >= UI_TEXT_SURFACE_WID)
    {
        return;
    }
    if (attr)
        *attr = g_ui_text_surface.attrs[row][col];
    if (ch)
        *ch = g_ui_text_surface.chars[row][col];
}

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
    ui_text_surface_put_string(col, row, attr, str);
}

/*
 * As above, but in "white"
 */
void put_str(cptr str, int row, int col)
{
    c_put_str(TERM_WHITE, str, row, col);
}

/*
 * Display a string on the screen using an attribute, and clear
 * to the end of the line.
 */
void c_prt(byte attr, cptr str, int row, int col)
{
    ui_text_surface_erase(col, row, 255);
    ui_text_surface_put_string(col, row, attr, str);
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
    int x, y;
    int wid = UI_TEXT_SURFACE_WID;
    int wrap;
    cptr s;

    ui_text_surface_get_cursor(&x, &y);

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
            ui_text_surface_erase(x, y, 255);

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
                    ui_text_surface_get_cell(i, y, &av[i], &cv[i]);

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
            ui_text_surface_erase(n, y, 255);

            /* Wrap */
            x = text_out_indent;
            y++;

            /* Clear line, move cursor */
            ui_text_surface_erase(x, y, 255);
            ui_text_surface_set_cursor(x, y);

            /* Wrap the word (if any) */
            for (i = n; i < wrap - 1; i++)
            {
                /* Dump */
                ui_text_surface_add_char(av[i], cv[i]);

                /* Advance (no wrap) */
                if (++x > wrap)
                    x = wrap;
            }
        }

        /* Dump */
        ui_text_surface_add_char(a, ch);

        /* Advance */
        if (++x > wrap)
            x = wrap;
    }

    ui_text_surface_set_cursor(x, y);
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
    for (y = row; y < UI_TEXT_SURFACE_HGT; y++)
    {
        /* Erase part of the screen */
        ui_text_surface_erase(0, y, 255);
    }
}

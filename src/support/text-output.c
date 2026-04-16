#include "angband.h"
#include "fs/file.h"

ang_file* text_out_file = NULL;
void (*text_out_hook)(byte a, cptr str);
int text_out_wrap = 0;
int text_out_indent = 0;

static void write_text_out_byte(unsigned char value)
{
    char byte = (char)value;
    (void)ang_file_write(text_out_file, &byte, 1);
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


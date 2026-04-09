/* File: ui-file-viewer.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "ui-file-viewer.h"
#include "ui-information-scene.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "log/log.h"
#include <ctype.h>

/*
 * Make a string lower case.
 */
static void string_lower(char* buf)
{
    char* s;

    /* Lowercase the string */
    for (s = buf; *s != 0; s++)
        *s = tolower((unsigned char)*s);
}

static int show_buffer_count_lines(cptr main_buffer)
{
    int count = 0;
    int j;

    for (j = 0; main_buffer[j] != '\0'; j++)
    {
        if (main_buffer[j] == '\n')
            count++;
    }

    if (j > 0 && main_buffer[j - 1] != '\n')
        count++;

    return count;
}

static int show_buffer_clamp_line(int line, int size, int hgt)
{
    int max_top = size - (hgt - 5);

    if (max_top < 0)
        max_top = 0;
    if (line > max_top)
        line = max_top;
    if (line < 0)
        line = 0;

    return line;
}

static void show_buffer_build_information_scene(app_information_scene* scene,
    cptr main_buffer, int line, int size, int hgt)
{
    int i;
    int j;
    int k;
    int next = 0;
    char ch;
    char buf[1024];

    if (!scene)
        return;

    app_information_scene_init(scene);
    line = show_buffer_clamp_line(line, size, hgt);

    for (j = 0; true; j++)
    {
        if (main_buffer[j] == '\n')
            next++;

        if (next == line || main_buffer[j] == '\0')
            break;
    }

    if (main_buffer[j] == '\n')
        j++;

    for (i = 0; i < hgt - 5;)
    {
        k = 0;
        while (true)
        {
            ch = main_buffer[j];

            if (ch == '\0')
                break;
            if (ch == '\n')
            {
                j++;
                break;
            }
            if (k + 1 < (int)sizeof(buf))
                buf[k++] = ch;
            j++;
        }
        buf[k] = '\0';

        (void)app_information_scene_add_text(scene, (s16b)(i + 2), 0,
            TERM_WHITE, buf);
        i++;
    }

    if (size <= hgt - 5)
    {
        (void)app_information_scene_add_text(scene, (s16b)(hgt - 2), 1,
            TERM_SLATE, "(press ESC to exit)");
        (void)app_information_scene_add_text(scene, (s16b)(hgt - 2), 8,
            TERM_L_WHITE, "ESC");
    }
    else
    {
        (void)app_information_scene_add_text(scene, (s16b)(hgt - 2), 1,
            TERM_SLATE,
            "(press ESC to exit, Space for next page, Arrows/Keypad to scroll)");
        (void)app_information_scene_add_text(scene, (s16b)(hgt - 2), 8,
            TERM_L_WHITE, "ESC");
        (void)app_information_scene_add_text(scene, (s16b)(hgt - 2), 21,
            TERM_L_WHITE, "Space");
        (void)app_information_scene_add_text(scene, (s16b)(hgt - 2), 42,
            TERM_L_WHITE, "Arrows");
        (void)app_information_scene_add_text(scene, (s16b)(hgt - 2), 49,
            TERM_L_WHITE, "Keypad");
    }
}

static bool show_buffer_information_scene(cptr main_buffer, int line)
{
    ui_information_scene_scope scope;
    int size;

    if (!ui_information_scene_enter(&scope))
        return false;

    size = show_buffer_count_lines(main_buffer);

    while (true)
    {
        app_information_scene scene;
        int wid;
        int hgt;
        int dir;
        int ch;

        Term_get_size(&wid, &hgt);
        (void)wid;
        line = show_buffer_clamp_line(line, size, hgt);
        show_buffer_build_information_scene(&scene, main_buffer, line, size, hgt);
        if (!ui_information_scene_present_document(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = ui_information_scene_wait_key();
        dir = target_dir(ch);
        if (dir == 8 || dir == 2)
            ch = I2D(dir);

        if (ch == '8' || ch == '=')
        {
            line--;
        }
        else if (ch == '2' || ch == '\n' || ch == '\r')
        {
            line++;
        }
        else if (ch == '3' || ch == ' ')
        {
            line += (hgt - 5);
        }
        else if (ch == ESCAPE)
        {
            break;
        }
    }

    ui_information_scene_leave(&scope);
    return true;
}

static bool show_file_pause_information_scene(
    ui_information_scene_scope* scope)
{
    if (!scope)
        return false;

    ui_information_scene_leave(scope);
    return true;
}

static bool show_file_resume_information_scene(
    ui_information_scene_scope* scope)
{
    if (!scope)
        return false;

    return ui_information_scene_enter(scope);
}

static bool show_file_information_scene(cptr name, cptr what, int line)
{
    ui_information_scene_scope scope;
    int i, k, n;
    char ch;
    int next = 0;
    int size;
    int back = 0;
    bool menu = false;
    bool case_sensitive = false;
    SDL_IOStream* fff = NULL;
    char* find = NULL;
    cptr tag = NULL;
    char finder[80];
    char shower[80];
    char filename[1024];
    char caption[128];
    char path[1024];
    char buf[1024];
    char lc_buf[1024];
    char hook[26][32];
    int wid, hgt;

    if (!ui_information_scene_enter(&scope))
        return false;

    SDL_strlcpy(finder, "", sizeof(finder));
    SDL_strlcpy(shower, "", sizeof(shower));
    SDL_strlcpy(caption, "", sizeof(caption));
    for (i = 0; i < 26; i++)
        hook[i][0] = '\0';

    Term_get_size(&wid, &hgt);

    SDL_strlcpy(filename, name, sizeof(filename));
    n = strlen(filename);
    for (i = 0; i < n; i++)
    {
        if (filename[i] == '#')
        {
            filename[i] = '\0';
            tag = filename + i + 1;
            break;
        }
    }

    name = filename;

    if (what)
    {
        SDL_strlcpy(caption, what, sizeof(caption));
        SDL_strlcpy(path, name, sizeof(path));
        log_debug("Opening help file: %s", path);
        fff = sdl_fopen(path, "r");
    }

    if (!fff)
    {
        ui_information_scene_leave(&scope);
        log_warn("Failed to open help file: %s", name);
        msg_format("Cannot open '%s'.", name);
        message_flush();
        return true;
    }

    log_debug("Successfully opened help file: %s", name);

    while (true)
    {
        if (sdl_fgets(fff, buf, sizeof(buf)))
            break;

        if (prefix(buf, "***** "))
        {
            char b1 = '[';
            char b2 = ']';

            if ((buf[6] == b1) && isalpha((unsigned char)buf[7])
                && (buf[8] == b2) && (buf[9] == ' '))
            {
                menu = true;
                k = A2I(buf[7]);
                if ((k >= 0) && (k < 26))
                    SDL_strlcpy(hook[k], buf + 10, sizeof(hook[0]));
            }
            else if (buf[6] == '<' && tag)
            {
                buf[strlen(buf) - 1] = '\0';
                if (streq(buf + 7, tag))
                    line = next;
            }

            continue;
        }

        next++;
    }

    size = next;

    while (true)
    {
        app_information_scene scene;

        app_information_scene_init(&scene);
        Term_get_size(&wid, &hgt);

        if (line > (size - (hgt - 5)))
            line = size - (hgt - 5);
        if (line < 0)
            line = 0;

        if (next > line)
        {
            sdl_fclose(fff);
            fff = sdl_fopen(path, "r");
            if (!fff)
            {
                ui_information_scene_leave(&scope);
                return true;
            }
            next = 0;
        }

        while (next < line)
        {
            if (sdl_fgets(fff, buf, sizeof(buf)))
                break;

            if (prefix(buf, "***** "))
                continue;

            next++;
        }

        if (caption[0])
            (void)app_information_scene_add_text(&scene, 0, 0, TERM_L_BLUE,
                caption);

        for (i = 0; i < hgt - 5;)
        {
            if (!i)
                line = next;

            if (sdl_fgets(fff, buf, sizeof(buf)))
                break;

            if (prefix(buf, "***** "))
                continue;

            next++;

            SDL_strlcpy(lc_buf, buf, sizeof(lc_buf));
            if (!case_sensitive)
                string_lower(lc_buf);

            if (find && !i && !strstr(lc_buf, find))
                continue;

            find = NULL;

            (void)app_information_scene_add_text(&scene, (s16b)(i + 2), 0,
                TERM_WHITE, buf);

            if (shower[0])
            {
                cptr str = lc_buf;

                while ((str = strstr(str, shower)) != NULL)
                {
                    int len = strlen(shower);
                    char match[APP_INFORMATION_TEXT_MAX];

                    strnfmt(match, sizeof(match), "%.*s", len,
                        &buf[str - lc_buf]);
                    (void)app_information_scene_add_text(&scene,
                        (s16b)(i + 2), (s16b)(str - lc_buf), TERM_YELLOW,
                        match);
                    str += len;
                }
            }

            i++;
        }

        if (find)
        {
            bell("Search string not found!");
            line = back;
            find = NULL;
            continue;
        }

        if (menu)
        {
            (void)app_information_scene_add_text(&scene, (s16b)(hgt - 1), 0,
                TERM_WHITE, "[Press a Number, or ESC to exit.]");
        }
        else if (size <= hgt - 5)
        {
            (void)app_information_scene_add_text(&scene, (s16b)(hgt - 2), 1,
                TERM_SLATE, "(press ESC to exit)");
            (void)app_information_scene_add_text(&scene, (s16b)(hgt - 2), 8,
                TERM_L_WHITE, "ESC");
        }
        else
        {
            (void)app_information_scene_add_text(&scene, (s16b)(hgt - 2), 1,
                TERM_SLATE,
                "(press ESC to exit, Space for next page, Arrows/Keypad to scroll)");
            (void)app_information_scene_add_text(&scene, (s16b)(hgt - 2), 8,
                TERM_L_WHITE, "ESC");
            (void)app_information_scene_add_text(&scene, (s16b)(hgt - 2), 21,
                TERM_L_WHITE, "Space");
            (void)app_information_scene_add_text(&scene, (s16b)(hgt - 2), 42,
                TERM_L_WHITE, "Arrows");
            (void)app_information_scene_add_text(&scene, (s16b)(hgt - 2), 49,
                TERM_L_WHITE, "Keypad");
        }

        if (!ui_information_scene_present_document(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = (char)ui_information_scene_wait_key();

        if (ch == '?')
            break;

        if (ch == '!')
            case_sensitive = !case_sensitive;

        if (ch == '&')
        {
            if (!show_file_pause_information_scene(&scope))
                break;
            (void)askfor_aux(shower, sizeof(shower));
            if (!case_sensitive)
                string_lower(shower);
            if (!show_file_resume_information_scene(&scope))
                break;
        }

        if (ch == '/')
        {
            if (!show_file_pause_information_scene(&scope))
                break;
            if (askfor_aux(finder, sizeof(finder)))
            {
                find = finder;
                back = line;
                line = line + 1;

                if (!case_sensitive)
                    string_lower(finder);

                SDL_strlcpy(shower, finder, sizeof(shower));
            }
            if (!show_file_resume_information_scene(&scope))
                break;
        }

        if (ch == '#')
        {
            char tmp[80];

            if (!show_file_pause_information_scene(&scope))
                break;
            SDL_strlcpy(tmp, "0", sizeof(tmp));
            if (askfor_aux(tmp, sizeof(tmp)))
                line = atoi(tmp);
            if (!show_file_resume_information_scene(&scope))
                break;
        }

        if ((ch == '8') || (ch == '='))
        {
            line = line - 1;
            if (line < 0)
                line = 0;
        }

        if (ch == '_')
            line = line - ((hgt - 5) / 2);

        if ((ch == '9') || (ch == '-'))
            line = line - (hgt - 5);

        if (ch == '7')
            line = 0;

        if ((ch == '2') || (ch == '\n') || (ch == '\r'))
            line = line + 1;

        if (ch == '+')
            line = line + ((hgt - 5) / 2);

        if ((ch == '3') || (ch == ' '))
            line = line + (hgt - 5);

        if (ch == '1')
            line = size;

        if (ch == ESCAPE)
            break;
    }

    sdl_fclose(fff);
    ui_information_scene_leave(&scope);
    return (ch != '?');
}

/*
 * Show the contents of a char buffer on the screen and allow scrolling.
 * Based on show_file.
 */
bool show_buffer(cptr main_buffer, int line)
{
    if (show_buffer_information_scene(main_buffer, line))
        return true;

    int i, j, k;
    int dir;

    char ch;

    int next = 0;

    char buf[1024];

    int wid, hgt;

    /* Get current terminal size */
    Term_get_size(&wid, &hgt);
    if (hgt <= 0)
        hgt = 24;

    /* Count lines in the buffer */
    int size = 0;
    for (j = 0; main_buffer[j] != '\0'; j++)
    {
        if (main_buffer[j] == '\n')
            size++;
    }

    /* Add one more if last line doesn't end with newline */
    if (j > 0 && main_buffer[j - 1] != '\n')
        size++;

    /* Display the file */
    while (true)
    {
        /* Clear screen */
        Term_clear();

        /* Restrict the visible range */
        if (line > (size - (hgt - 5)))
            line = size - (hgt - 5);
        if (line < 0)
            line = 0;

        /* Goto the selected line */
        next = 0;
        for (j = 0; true; j++)
        {
            if (main_buffer[j] == '\n')
                next++;

            if ((next == line) || (main_buffer[j] == '\0'))
                break;
        }

        /* Need to step forward a character when not starting with the first line */
        if (main_buffer[j] == '\n')
            j++;

        /* Dump the next lines of the file */
        for (i = 0; i < hgt - 5;)
        {
            /* Get a line of the file or stop */
            k = 0;
            while (true)
            {
                ch = main_buffer[j];

                if (ch == '\0')
                    break;

                if (ch == '\n')
                {
                    j++;
                    break;
                }

                buf[k] = ch;

                k++;
                j++;
            }
            buf[k] = '\0';

            /* Dump the line */
            Term_putstr(0, i + 2, -1, TERM_WHITE, buf);

            /* Count the printed lines */
            i++;
        }

        /* Prompt -- small files */
        if (size <= hgt - 5)
        {
            Term_putstr(1, hgt - 2, -1, TERM_SLATE, "(press ESC to exit)");
            Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
            Term_putstr(20, hgt - 2, -1, TERM_L_WHITE, "");
        }

        /* Prompt -- large files */
        else
        {
            Term_putstr(1, hgt - 2, -1, TERM_SLATE,
                "(press ESC to exit, Space for next page, Arrows/Keypad to "
                "scroll)");
            Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
            Term_putstr(21, hgt - 2, -1, TERM_L_WHITE, "Space");
            Term_putstr(42, hgt - 2, -1, TERM_L_WHITE, "Arrows");
            Term_putstr(49, hgt - 2, -1, TERM_L_WHITE, "Keypad");
            Term_putstr(67, hgt - 2, -1, TERM_L_WHITE, "");
        }

        /* Get a keypress */
        ch = inkey();

        dir = target_dir(ch);
        if (dir == 8 || dir == 2)
            ch = I2D(dir);

        /* Back up one line */
        if ((ch == '8') || (ch == '='))
        {
            line = line - 1;
            if (line < 0)
                line = 0;
        }

        /* Advance one line */
        if ((ch == '2') || (ch == '\n') || (ch == '\r'))
        {
            line = line + 1;
        }

        /* Advance one full page */
        if ((ch == '3') || (ch == ' '))
        {
            line = line + (hgt - 5);
        }

        /* Exit on escape */
        if (ch == ESCAPE)
            break;
    }

    /* Done */
    return (true);
}

/*
 * Recursive file perusal.
 *
 * Return false on "?", otherwise true.
 *
 * Process various special text in the input file, including the "menu"
 * structures used by the "help file" system.
 */
bool show_file(cptr name, cptr what, int line)
{
    if (show_file_information_scene(name, what, line))
        return true;

    int i, k, n;

    char ch;

    /* Number of "real" lines passed by */
    int next = 0;

    /* Number of "real" lines in the file */
    int size;

    /* Backup value for "line" */
    int back = 0;

    /* This screen has sub-screens */
    bool menu = false;

    /* Case sensitive search */
    bool case_sensitive = false;

    /* Current help file */
    SDL_IOStream* fff = NULL;

    /* Find this string (if any) */
    char* find = NULL;

    /* Jump to this tag */
    cptr tag = NULL;

    /* Hold a string to find */
    char finder[80];

    /* Hold a string to show */
    char shower[80];

    /* Filename */
    char filename[1024];

    /* Describe this thing */
    char caption[128];

    /* Path buffer */
    char path[1024];

    /* General buffer */
    char buf[1024];

    /* Lower case version of the buffer, for searching */
    char lc_buf[1024];

    /* Sub-menu information */
    char hook[26][32];

    int wid, hgt;

    /* Wipe finder */
    SDL_strlcpy(finder, "", sizeof(finder));

    /* Wipe shower */
    SDL_strlcpy(shower, "", sizeof(shower));

    /* Wipe caption */
    SDL_strlcpy(caption, "", sizeof(caption));

    /* Wipe the hooks */
    for (i = 0; i < 26; i++)
        hook[i][0] = '\0';

    /* Get size */
    Term_get_size(&wid, &hgt);

    /* Copy the filename */
    SDL_strlcpy(filename, name, sizeof(filename));

    n = strlen(filename);

    /* Extract the tag from the filename */
    for (i = 0; i < n; i++)
    {
        if (filename[i] == '#')
        {
            filename[i] = '\0';
            tag = filename + i + 1;
            break;
        }
    }

    /* Redirect the name */
    name = filename;

    /* Hack XXX XXX XXX */
    if (what)
    {
        /* Caption */
        SDL_strlcpy(caption, what, sizeof(caption));

        /* Get the filename */
        SDL_strlcpy(path, name, sizeof(path));

        log_debug("Opening help file: %s", path);

        /* Open */
        fff = sdl_fopen(path, "r");
    }

    /* Oops */
    if (!fff)
    {
        log_warn("Failed to open help file: %s", name);

        /* Message */
        msg_format("Cannot open '%s'.", name);
        message_flush();

        /* Oops */
        return (true);
    }

    log_debug("Successfully opened help file: %s", name);

    /* Pre-Parse the file */
    while (true)
    {
        /* Read a line or stop */
        if (sdl_fgets(fff, buf, sizeof(buf)))
            break;

        /* XXX Parse "menu" items */
        if (prefix(buf, "***** "))
        {
            char b1 = '[', b2 = ']';

            /* Notice "menu" requests */
            if ((buf[6] == b1) && isalpha((unsigned char)buf[7])
                && (buf[8] == b2) && (buf[9] == ' '))
            {
                /* This is a menu file */
                menu = true;

                /* Extract the menu item */
                k = A2I(buf[7]);

                /* Store the menu item (if valid) */
                if ((k >= 0) && (k < 26))
                    SDL_strlcpy(hook[k], buf + 10, sizeof(hook[0]));
            }
            /* Notice "tag" requests */
            else if (buf[6] == '<')
            {
                if (tag)
                {
                    /* Remove the closing '>' of the tag */
                    buf[strlen(buf) - 1] = '\0';

                    /* Compare with the requested tag */
                    if (streq(buf + 7, tag))
                    {
                        /* Remember the tagged line */
                        line = next;
                    }
                }
            }

            /* Skip this */
            continue;
        }

        /* Count the "real" lines */
        next++;
    }

    /* Save the number of "real" lines */
    size = next;

    /* Display the file */
    while (true)
    {
        /* Clear screen */
        Term_clear();

        /* Restrict the visible range */
        if (line > (size - (hgt - 5)))
            line = size - (hgt - 5);
        if (line < 0)
            line = 0;

        /* Re-open the file if needed */
        if (next > line)
        {
            /* Close it */
            sdl_fclose(fff);

            /* Hack -- Re-Open the file */
            fff = sdl_fopen(path, "r");

            /* Oops */
            if (!fff)
                return (true);

            /* File has been restarted */
            next = 0;
        }

        /* Goto the selected line */
        while (next < line)
        {
            /* Get a line */
            if (sdl_fgets(fff, buf, sizeof(buf)))
                break;

            /* Skip tags/links */
            if (prefix(buf, "***** "))
                continue;

            /* Count the lines */
            next++;
        }

        /* Dump the next lines of the file */
        for (i = 0; i < hgt - 5;)
        {
            /* Hack -- track the "first" line */
            if (!i)
                line = next;

            /* Get a line of the file or stop */
            if (sdl_fgets(fff, buf, sizeof(buf)))
                break;

            /* Hack -- skip "special" lines */
            if (prefix(buf, "***** "))
                continue;

            /* Count the "real" lines */
            next++;

            /* Make a copy of the current line for searching */
            SDL_strlcpy(lc_buf, buf, sizeof(lc_buf));

            /* Make the line lower case */
            if (!case_sensitive)
                string_lower(lc_buf);

            /* Hack -- keep searching */
            if (find && !i && !strstr(lc_buf, find))
                continue;

            /* Hack -- stop searching */
            find = NULL;

            /* Dump the line */
            Term_putstr(0, i + 2, -1, TERM_WHITE, buf);

            /* Hilite "shower" */
            if (shower[0])
            {
                cptr str = lc_buf;

                /* Display matches */
                while ((str = strstr(str, shower)) != NULL)
                {
                    int len = strlen(shower);

                    /* Display the match */
                    Term_putstr(str - lc_buf, i + 2, len, TERM_YELLOW,
                        &buf[str - lc_buf]);

                    /* Advance */
                    str += len;
                }
            }

            /* Count the printed lines */
            i++;
        }

        /* Hack -- failed search */
        if (find)
        {
            bell("Search string not found!");
            line = back;
            find = NULL;
            continue;
        }

        /* Prompt -- menu screen */
        if (menu)
        {
            prt("[Press a Number, or ESC to exit.]", hgt - 1, 0);
        }

        /* Prompt -- small files */
        else if (size <= hgt - 5)
        {
            Term_putstr(1, hgt - 2, -1, TERM_SLATE, "(press ESC to exit)");
            Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
            Term_putstr(20, hgt - 2, -1, TERM_L_WHITE, "");
        }

        /* Prompt -- large files */
        else
        {
            Term_putstr(1, hgt - 2, -1, TERM_SLATE,
                "(press ESC to exit, Space for next page, Arrows/Keypad to "
                "scroll)");
            Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
            Term_putstr(21, hgt - 2, -1, TERM_L_WHITE, "Space");
            Term_putstr(42, hgt - 2, -1, TERM_L_WHITE, "Arrows");
            Term_putstr(49, hgt - 2, -1, TERM_L_WHITE, "Keypad");
            Term_putstr(67, hgt - 2, -1, TERM_L_WHITE, "");
        }

        /* Get a keypress */
        ch = inkey();

        /* Exit the help */
        if (ch == '?')
            break;

        /* Toggle case sensitive on/off */
        if (ch == '!')
            case_sensitive = !case_sensitive;

        /* Try showing */
        if (ch == '&')
        {
            /* Get "shower" */
            prt("Show: ", hgt - 1, 0);
            (void)askfor_aux(shower, sizeof(shower));

            /* Make the "shower" lowercase */
            if (!case_sensitive)
                string_lower(shower);
        }

        /* Try finding */
        if (ch == '/')
        {
            /* Get "finder" */
            prt("Find: ", hgt - 1, 0);
            if (askfor_aux(finder, sizeof(finder)))
            {
                /* Find it */
                find = finder;
                back = line;
                line = line + 1;

                /* Make the "finder" lowercase */
                if (!case_sensitive)
                    string_lower(finder);

                /* Show it */
                SDL_strlcpy(shower, finder, sizeof(shower));
            }
        }

        /* Go to a specific line */
        if (ch == '#')
        {
            char tmp[80];
            prt("Goto Line: ", hgt - 1, 0);
            SDL_strlcpy(tmp, "0", sizeof(tmp));
            if (askfor_aux(tmp, sizeof(tmp)))
            {
                line = atoi(tmp);
            }
        }

        /* Back up one line */
        if ((ch == '8') || (ch == '='))
        {
            line = line - 1;
            if (line < 0)
                line = 0;
        }

        /* Back up one half page */
        if (ch == '_')
        {
            line = line - ((hgt - 5) / 2);
        }

        /* Back up one full page */
        if ((ch == '9') || (ch == '-'))
        {
            line = line - (hgt - 5);
        }

        /* Back to the top */
        if (ch == '7')
        {
            line = 0;
        }

        /* Advance one line */
        if ((ch == '2') || (ch == '\n') || (ch == '\r'))
        {
            line = line + 1;
        }

        /* Advance one half page */
        if (ch == '+')
        {
            line = line + ((hgt - 5) / 2);
        }

        /* Advance one full page */
        if ((ch == '3') || (ch == ' '))
        {
            line = line + (hgt - 5);
        }

        /* Advance to the bottom */
        if (ch == '1')
        {
            line = size;
        }

        /* Exit on escape */
        if (ch == ESCAPE)
            break;
    }

    /* Close the file */
    sdl_fclose(fff);

    /* Done */
    return (ch != '?');
}

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

static void show_file_document_layout_size(int* wid, int* hgt)
{
    if (wid)
        *wid = 80;
    if (hgt)
        *hgt = 24;
}

static app_ui_panel* show_file_begin_document_scene(app_ui_scene* scene)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_DOCUMENT;
    panel->min_width_px = 0;
    panel->width_cap_px = 0;
    return panel;
}

static bool show_file_scene_add_text(app_ui_scene* scene, app_ui_panel* panel,
    s16b row, s16b col, byte attr, cptr text)
{
    if (!text || !text[0])
        return true;

    return app_ui_panel_add_document_text(scene, panel, row, col, attr, text);
}

static bool show_buffer_build_ui_scene(app_ui_scene* scene,
    cptr main_buffer, int line, int size, int hgt)
{
    app_ui_panel* panel;
    int i;
    int j;
    int k;
    int next = 0;
    char ch;
    char buf[1024];

    if (!scene)
        return false;

    panel = show_file_begin_document_scene(scene);
    if (!panel)
        return false;
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

        if (!show_file_scene_add_text(scene, panel, (s16b)(i + 2), 0,
                TERM_WHITE, buf))
        {
            return false;
        }
        i++;
    }

    if (size <= hgt - 5)
    {
        return show_file_scene_add_text(scene, panel, (s16b)(hgt - 2), 1,
                   TERM_SLATE, "(press ESC to exit)")
            && show_file_scene_add_text(scene, panel, (s16b)(hgt - 2), 8,
                TERM_L_WHITE, "ESC");
    }

    return show_file_scene_add_text(scene, panel, (s16b)(hgt - 2), 1,
               TERM_SLATE,
               "(press ESC to exit, Space for next page, Arrows/Keypad to scroll)")
        && show_file_scene_add_text(scene, panel, (s16b)(hgt - 2), 8,
            TERM_L_WHITE, "ESC")
        && show_file_scene_add_text(scene, panel, (s16b)(hgt - 2), 21,
            TERM_L_WHITE, "Space")
        && show_file_scene_add_text(scene, panel, (s16b)(hgt - 2), 42,
            TERM_L_WHITE, "Arrows")
        && show_file_scene_add_text(scene, panel, (s16b)(hgt - 2), 49,
            TERM_L_WHITE, "Keypad");
}

typedef enum show_file_scene_result {
    SHOW_FILE_SCENE_RESULT_SHOWN = 0,
    SHOW_FILE_SCENE_RESULT_QUERY_EXIT,
    SHOW_FILE_SCENE_RESULT_ERROR
} show_file_scene_result;

static bool show_buffer_information_scene(cptr main_buffer, int line)
{
    ui_information_scene_scope scope;
    int size;

    if (!ui_information_scene_enter(&scope))
        return false;

    size = show_buffer_count_lines(main_buffer);

    while (true)
    {
        app_ui_scene scene;
        int hgt;
        int dir;
        int ch;

        show_file_document_layout_size(NULL, &hgt);
        line = show_buffer_clamp_line(line, size, hgt);
        if (!show_buffer_build_ui_scene(&scene, main_buffer, line, size, hgt)
            || !ui_information_scene_present_ui(&scene))
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

static show_file_scene_result show_file_information_scene(
    cptr name, cptr what, int line)
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
    int hgt;

    if (!ui_information_scene_enter(&scope))
        return SHOW_FILE_SCENE_RESULT_ERROR;

    SDL_strlcpy(finder, "", sizeof(finder));
    SDL_strlcpy(shower, "", sizeof(shower));
    SDL_strlcpy(caption, "", sizeof(caption));
    for (i = 0; i < 26; i++)
        hook[i][0] = '\0';

    show_file_document_layout_size(NULL, &hgt);

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
        return SHOW_FILE_SCENE_RESULT_SHOWN;
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
        app_ui_scene scene;
        app_ui_panel* panel;

        panel = show_file_begin_document_scene(&scene);
        if (!panel)
        {
            ui_information_scene_leave(&scope);
            return SHOW_FILE_SCENE_RESULT_ERROR;
        }
        show_file_document_layout_size(NULL, &hgt);

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
                return SHOW_FILE_SCENE_RESULT_SHOWN;
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
        {
            if (!show_file_scene_add_text(&scene, panel, 0, 0, TERM_L_BLUE,
                    caption))
            {
                ui_information_scene_leave(&scope);
                return SHOW_FILE_SCENE_RESULT_ERROR;
            }
        }

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

            if (!show_file_scene_add_text(&scene, panel, (s16b)(i + 2), 0,
                    TERM_WHITE, buf))
            {
                ui_information_scene_leave(&scope);
                return SHOW_FILE_SCENE_RESULT_ERROR;
            }

            if (shower[0])
            {
                cptr str = lc_buf;

                while ((str = strstr(str, shower)) != NULL)
                {
                    int len = strlen(shower);
                    char match[APP_UI_TEXT_MAX];

                    strnfmt(match, sizeof(match), "%.*s", len,
                        &buf[str - lc_buf]);
                    if (!show_file_scene_add_text(&scene, panel, (s16b)(i + 2),
                            (s16b)(str - lc_buf), TERM_YELLOW, match))
                    {
                        ui_information_scene_leave(&scope);
                        return SHOW_FILE_SCENE_RESULT_ERROR;
                    }
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
            if (!show_file_scene_add_text(&scene, panel, (s16b)(hgt - 1), 0,
                    TERM_WHITE, "[Press a Number, or ESC to exit.]"))
            {
                ui_information_scene_leave(&scope);
                return SHOW_FILE_SCENE_RESULT_ERROR;
            }
        }
        else if (size <= hgt - 5)
        {
            if (!show_file_scene_add_text(&scene, panel, (s16b)(hgt - 2), 1,
                    TERM_SLATE, "(press ESC to exit)")
                || !show_file_scene_add_text(&scene, panel, (s16b)(hgt - 2), 8,
                    TERM_L_WHITE, "ESC"))
            {
                ui_information_scene_leave(&scope);
                return SHOW_FILE_SCENE_RESULT_ERROR;
            }
        }
        else
        {
            if (!show_file_scene_add_text(&scene, panel, (s16b)(hgt - 2), 1,
                    TERM_SLATE,
                    "(press ESC to exit, Space for next page, Arrows/Keypad to scroll)")
                || !show_file_scene_add_text(&scene, panel, (s16b)(hgt - 2), 8,
                    TERM_L_WHITE, "ESC")
                || !show_file_scene_add_text(&scene, panel, (s16b)(hgt - 2), 21,
                    TERM_L_WHITE, "Space")
                || !show_file_scene_add_text(&scene, panel, (s16b)(hgt - 2), 42,
                    TERM_L_WHITE, "Arrows")
                || !show_file_scene_add_text(&scene, panel, (s16b)(hgt - 2), 49,
                    TERM_L_WHITE, "Keypad"))
            {
                ui_information_scene_leave(&scope);
                return SHOW_FILE_SCENE_RESULT_ERROR;
            }
        }

        if (!ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return SHOW_FILE_SCENE_RESULT_ERROR;
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
    return (ch == '?') ? SHOW_FILE_SCENE_RESULT_QUERY_EXIT
                       : SHOW_FILE_SCENE_RESULT_SHOWN;
}

/*
 * Show the contents of a char buffer on the screen and allow scrolling.
 * Based on show_file.
 */
bool show_buffer(cptr main_buffer, int line)
{
    if (!ui_information_scene_supported())
    {
        log_warn("buffer viewer: snapshot renderer required; legacy buffer renderer removed");
        msg_print("Text viewer requires the snapshot UI renderer.");
        return true;
    }

    if (!show_buffer_information_scene(main_buffer, line))
    {
        log_warn("buffer viewer: information-scene presentation failed on the snapshot renderer path");
        msg_print("Text viewer unavailable.");
    }

    return true;
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
    show_file_scene_result result;

    if (!ui_information_scene_supported())
    {
        log_warn("file viewer: snapshot renderer required; legacy file renderer removed");
        msg_print("File viewer requires the snapshot UI renderer.");
        return true;
    }

    result = show_file_information_scene(name, what, line);
    if (result == SHOW_FILE_SCENE_RESULT_ERROR)
    {
        log_warn("file viewer: information-scene presentation failed on the snapshot renderer path");
        msg_print("File viewer unavailable.");
        return true;
    }

    return (result != SHOW_FILE_SCENE_RESULT_QUERY_EXIT);
}

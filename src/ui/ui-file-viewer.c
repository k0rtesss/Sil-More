/* File: ui-file-viewer.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "ui-file-viewer.h"
#include "ui-information-scene.h"
#include "fs/file.h"
#include "fs/resource.h"
#include "log/log.h"
#include "support/utf8.h"
#include "ui/ui-browser-shell.h"
#include <ctype.h>

#define SHOW_FILE_SCROLL_PAGE_LINES 12

typedef enum show_file_footer_action {
    SHOW_FILE_ACTION_UP = 1,
    SHOW_FILE_ACTION_DOWN = 2,
    SHOW_FILE_ACTION_PAGE = 3,
    SHOW_FILE_ACTION_FIND = 4,
    SHOW_FILE_ACTION_NEXT_MATCH = 5,
    SHOW_FILE_ACTION_PREV_MATCH = 6,
    SHOW_FILE_ACTION_CASE = 7,
    SHOW_FILE_ACTION_EXIT = 8
} show_file_footer_action;

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

static void show_file_prepare_search_text(char* dst, size_t dst_size,
    cptr src, bool case_sensitive)
{
    if (!dst || !dst_size)
        return;

    SDL_strlcpy(dst, src ? src : "", dst_size);
    if (!case_sensitive)
        string_lower(dst);
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

static int show_buffer_clamp_line(int line, int size)
{
    int max_top = size - 1;

    if (max_top < 0)
        max_top = 0;
    if (line > max_top)
        line = max_top;
    if (line < 0)
        line = 0;

    return line;
}

static app_ui_panel* show_file_begin_browser_scene(app_ui_scene* scene)
{
    ui_browser_shell_scene_config config;
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    ui_browser_shell_scene_config_init(&config);
    config.scene_flags = APP_UI_SCENE_FLAG_USE_BACKDROP;
    config.style = APP_UI_PANEL_STYLE_DOCUMENT;
    config.accent_attr = TERM_L_BLUE;
    config.panel_flags = APP_UI_PANEL_FLAG_SCROLL_ROWS;
    config.min_width_px = 1180;
    config.width_cap_px = 2800;
    panel = ui_browser_shell_begin(scene, &config);
    if (!panel)
        return NULL;

    app_ui_panel_set_icon(panel, TERM_YELLOW, '?');
    return panel;
}

static bool show_file_add_footer_actions(app_ui_panel* panel, bool can_scroll,
    bool can_search, bool has_search)
{
    ui_browser_shell_footer_action actions[APP_UI_FOOTER_ACTION_MAX];
    size_t count = 0;

    if (!panel)
        return false;

    if (can_scroll)
    {
        actions[count++] = (ui_browser_shell_footer_action){
            SHOW_FILE_ACTION_UP, TERM_WHITE, true, "8", "Up"
        };
        actions[count++] = (ui_browser_shell_footer_action){
            SHOW_FILE_ACTION_DOWN, TERM_WHITE, true, "2", "Down"
        };
        actions[count++] = (ui_browser_shell_footer_action){
            SHOW_FILE_ACTION_PAGE, TERM_L_BLUE, true, "Space", "Page"
        };
    }

    if (can_search)
    {
        actions[count++] = (ui_browser_shell_footer_action){
            SHOW_FILE_ACTION_FIND, TERM_WHITE, true, "/", "Find"
        };
        if (has_search)
        {
            actions[count++] = (ui_browser_shell_footer_action){
                SHOW_FILE_ACTION_NEXT_MATCH, TERM_WHITE, true, "n", "Next"
            };
            actions[count++] = (ui_browser_shell_footer_action){
                SHOW_FILE_ACTION_PREV_MATCH, TERM_WHITE, true, "N", "Prev"
            };
        }
        actions[count++] = (ui_browser_shell_footer_action){
            SHOW_FILE_ACTION_CASE, TERM_WHITE, true, "!", "Case"
        };
    }

    actions[count++] = (ui_browser_shell_footer_action){
        SHOW_FILE_ACTION_EXIT, TERM_WHITE, true, "Esc", "Exit"
    };

    return ui_browser_shell_add_footer_actions(panel, actions, count);
}

static bool show_file_command_to_key(const app_ui_command* command,
    char* out_key)
{
    static const ui_browser_shell_button_key button_keys[] = {
        { SHOW_FILE_ACTION_UP, '8' },
        { SHOW_FILE_ACTION_DOWN, '2' },
        { SHOW_FILE_ACTION_PAGE, ' ' },
        { SHOW_FILE_ACTION_FIND, '/' },
        { SHOW_FILE_ACTION_NEXT_MATCH, 'n' },
        { SHOW_FILE_ACTION_PREV_MATCH, 'N' },
        { SHOW_FILE_ACTION_CASE, '!' },
        { SHOW_FILE_ACTION_EXIT, ESCAPE }
    };
    ui_browser_shell_command_map map;
    ui_browser_shell_command_result result;

    if (out_key)
        *out_key = '\0';
    if (!command || !out_key)
        return false;

    ui_browser_shell_command_map_init(&map);
    map.button_keys = button_keys;
    map.button_key_count = N_ELEMENTS(button_keys);
    map.scroll_keys.left_key = '9';
    map.scroll_keys.right_key = '3';

    if (!ui_browser_shell_translate_command(command, &map, &result))
        return false;

    *out_key = result.key;
    return true;
}

static int show_file_wait_key(void)
{
    ui_information_scene_event event;
    char command_key = '\0';

    if (!ui_information_scene_wait_event(&event, 0))
        return ESCAPE;
    if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
        return event.key;
    if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND
        && show_file_command_to_key(&event.command, &command_key))
    {
        return command_key;
    }

    return 0;
}

static size_t show_file_trim_line(char* buf)
{
    size_t len;

    if (!buf)
        return 0;

    len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
    return len;
}

static bool show_file_scene_add_rich_chunk(app_ui_scene* scene,
    app_ui_panel* panel, byte attr, const char* text, size_t len)
{
    char chunk[APP_UI_TEXT_MAX];
    size_t offset = 0;

    if (!scene || !panel || !text || len == 0)
        return true;

    while (offset < len)
    {
        size_t part = utf8_clip_bytes(text + offset,
            MIN(len - offset, sizeof(chunk) - 1u));
        if (part == 0)
            break;
        memcpy(chunk, text + offset, part);
        chunk[part] = '\0';
        if (!app_ui_panel_add_rich_text(scene, panel, attr, chunk))
            return false;
        offset += part;
    }

    return true;
}

static bool show_file_scene_add_plain_line(app_ui_scene* scene,
    app_ui_panel* panel, const char* text)
{
    if (!text || !text[0])
        return true;

    return show_file_scene_add_rich_chunk(scene, panel, TERM_WHITE, text,
        strlen(text));
}

static bool show_file_scene_add_highlighted_line(app_ui_scene* scene,
    app_ui_panel* panel, const char* text, const char* highlight,
    bool case_sensitive)
{
    char search_line[1024];
    const char* cursor;
    const char* match;
    size_t highlight_len;

    if (!scene || !panel)
        return false;
    if (!text || !text[0])
        return true;
    if (!highlight || !highlight[0])
        return show_file_scene_add_plain_line(scene, panel, text);

    SDL_strlcpy(search_line, text, sizeof(search_line));
    if (!case_sensitive)
        string_lower(search_line);
    highlight_len = strlen(highlight);
    cursor = text;
    match = strstr(search_line, highlight);

    while (match)
    {
        size_t prefix_len = (size_t)(match - search_line) - (size_t)(cursor - text);

        if (prefix_len > 0
            && !show_file_scene_add_rich_chunk(scene, panel, TERM_WHITE,
                cursor, prefix_len))
        {
            return false;
        }
        if (!show_file_scene_add_rich_chunk(scene, panel, TERM_YELLOW,
                text + (match - search_line), highlight_len))
        {
            return false;
        }
        cursor = text + (match - search_line) + highlight_len;
        match = strstr(search_line + (cursor - text), highlight);
    }

    return show_file_scene_add_rich_chunk(scene, panel, TERM_WHITE, cursor,
        strlen(cursor));
}

static bool show_buffer_build_ui_scene(app_ui_scene* scene,
    cptr main_buffer, int line, int size)
{
    app_ui_panel* panel;
    int i;
    int j;
    int k;
    char ch;
    char buf[1024];

    if (!scene)
        return false;

    panel = show_file_begin_browser_scene(scene);
    if (!panel)
        return false;
    app_ui_panel_set_row_offset(panel, (s16b)line);
    if (!app_ui_panel_begin_rich_paragraph(scene, panel))
        return false;
    line = show_buffer_clamp_line(line, size);

    j = 0;

    for (i = 0; ; )
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

        if (i > 0 && !app_ui_panel_add_rich_text(scene, panel, TERM_WHITE, "\n"))
            return false;
        if (!show_file_scene_add_plain_line(scene, panel, buf))
        {
            return false;
        }
        i++;

        if (main_buffer[j] == '\0')
            break;
    }

    if (size <= 1)
    {
        return app_ui_panel_add_body_line(panel, TERM_SLATE, "ESC exit")
            && show_file_add_footer_actions(panel, false, false, false);
    }

    if (!app_ui_panel_add_body_line(panel, TERM_SLATE,
            "ESC exit  Space +12  Arrows/Keypad scroll"))
    {
        return false;
    }

    return app_ui_panel_add_body_line(panel, TERM_SLATE,
        format("[line %d/%d]", line + 1, size))
        && show_file_add_footer_actions(panel, true, false, false);
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
        int dir;
        int ch;
        int max_scroll;

        max_scroll = MAX(0, size - 1);
        line = show_buffer_clamp_line(line, size);
        if (!show_buffer_build_ui_scene(&scene, main_buffer, line, size)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = show_file_wait_key();
        if (!ch)
            continue;
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
            line += SHOW_FILE_SCROLL_PAGE_LINES;
            if (line > max_scroll)
                line = max_scroll;
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

static bool show_file_resolve_path(char* path, size_t max, cptr name)
{
    if (!path || !max || !name || !name[0])
        return false;

    return resource_resolve_path(path, max, RESOURCE_ROOT_HELP, name);
}

static bool show_file_find_next_match(cptr path, cptr find,
    bool case_sensitive, int start_line, int* out_line)
{
    ang_file* fff;
    char buf[1024];
    char lc_buf[1024];
    int next = 0;

    if (!path || !find || !find[0] || !out_line)
        return false;

    fff = ang_file_open(path, "r");
    if (!fff)
        return false;

    while (true)
    {
        if (ang_file_gets(fff, buf, sizeof(buf)))
            break;
        if (prefix(buf, "***** "))
            continue;

        show_file_trim_line(buf);
        SDL_strlcpy(lc_buf, buf, sizeof(lc_buf));
        if (!case_sensitive)
            string_lower(lc_buf);

        if (next >= start_line && strstr(lc_buf, find))
        {
            *out_line = next;
            ang_file_close(fff);
            return true;
        }

        next++;
    }

    ang_file_close(fff);
    return false;
}

static bool show_file_find_previous_match(cptr path, cptr find,
    bool case_sensitive, int start_line, int* out_line)
{
    ang_file* fff;
    char buf[1024];
    char lc_buf[1024];
    int next = 0;
    int found = -1;

    if (!path || !find || !find[0] || !out_line || start_line < 0)
        return false;

    fff = ang_file_open(path, "r");
    if (!fff)
        return false;

    while (true)
    {
        if (ang_file_gets(fff, buf, sizeof(buf)))
            break;
        if (prefix(buf, "***** "))
            continue;
        if (next > start_line)
            break;

        show_file_trim_line(buf);
        SDL_strlcpy(lc_buf, buf, sizeof(lc_buf));
        if (!case_sensitive)
            string_lower(lc_buf);

        if (strstr(lc_buf, find))
            found = next;

        next++;
    }

    ang_file_close(fff);
    if (found < 0)
        return false;

    *out_line = found;
    return true;
}

static bool show_file_build_ui_scene(app_ui_scene* scene, cptr path,
    cptr caption, cptr shower, cptr shower_display, bool case_sensitive,
    bool menu, int size, int line)
{
    ang_file* fff;
    app_ui_panel* panel;
    char buf[1024];
    bool wrote_any = false;
    int scroll_offset = line;

    if (!scene || !path)
        return false;

    fff = ang_file_open(path, "r");
    if (!fff)
        return false;

    panel = show_file_begin_browser_scene(scene);
    if (!panel)
    {
        ang_file_close(fff);
        return false;
    }
    if (caption && caption[0])
        scroll_offset += 2;
    app_ui_panel_set_row_offset(panel, (s16b)scroll_offset);
    if (!app_ui_panel_begin_rich_paragraph(scene, panel))
    {
        ang_file_close(fff);
        return false;
    }

    if (caption && caption[0])
    {
        if (!show_file_scene_add_rich_chunk(scene, panel, TERM_L_BLUE, caption,
                strlen(caption))
            || !app_ui_panel_add_rich_text(scene, panel, TERM_WHITE, "\n\n"))
        {
            ang_file_close(fff);
            return false;
        }
        wrote_any = true;
    }

    while (true)
    {
        if (ang_file_gets(fff, buf, sizeof(buf)))
            break;
        if (prefix(buf, "***** "))
            continue;

        show_file_trim_line(buf);
        if (wrote_any && !app_ui_panel_add_rich_text(scene, panel, TERM_WHITE,
                "\n"))
        {
            ang_file_close(fff);
            return false;
        }
        if (!show_file_scene_add_highlighted_line(scene, panel, buf, shower,
                case_sensitive))
        {
            ang_file_close(fff);
            return false;
        }
        wrote_any = true;
    }

    ang_file_close(fff);
    if (!wrote_any && !app_ui_panel_add_rich_text(scene, panel, TERM_WHITE, " "))
        return false;

    if (menu)
    {
        if (!app_ui_panel_add_body_line(panel, TERM_WHITE,
                "[Press a letter, number, or ESC to exit.]"))
        {
            return false;
        }
    }
    else if (size <= 1)
    {
        if (!app_ui_panel_add_body_line(panel, TERM_SLATE, "ESC exit"))
            return false;
    }
    else
    {
        if (!app_ui_panel_add_body_line(panel, TERM_SLATE,
                "ESC exit  Space +12  Arrows/Keypad scroll"))
        {
            return false;
        }
    }

    if (!menu && shower && shower[0])
    {
        char search_buf[APP_UI_TEXT_MAX];

        strnfmt(search_buf, sizeof(search_buf), "Search: %s%s",
            (shower_display && shower_display[0]) ? shower_display : shower,
            case_sensitive ? " (case-sensitive)" : "");
        if (!app_ui_panel_add_body_line(panel, TERM_L_BLUE, search_buf))
            return false;
    }

    if (size > 1)
    {
        char scroll_buf[32];

        strnfmt(scroll_buf, sizeof(scroll_buf), "[line %d/%d]", line + 1, size);
        if (!app_ui_panel_add_body_line(panel, TERM_SLATE, scroll_buf))
            return false;
    }

    return show_file_add_footer_actions(panel, size > 1, !menu,
        shower && shower[0]);
}

static show_file_scene_result show_file_information_scene(
    cptr name, cptr what, int line)
{
    ui_information_scene_scope scope;
    int i, n;
    char ch;
    int next = 0;
    int size;
    int back = 0;
    bool menu = false;
    bool case_sensitive = false;
    ang_file* fff = NULL;
    char* find = NULL;
    cptr tag = NULL;
    char finder[80];
    char finder_raw[80];
    char shower[80];
    char shower_raw[80];
    char filename[1024];
    char caption[128];
    char path[1024];
    char buf[1024];

    if (!ui_information_scene_enter(&scope))
        return SHOW_FILE_SCENE_RESULT_ERROR;

    SDL_strlcpy(finder, "", sizeof(finder));
    SDL_strlcpy(finder_raw, "", sizeof(finder_raw));
    SDL_strlcpy(shower, "", sizeof(shower));
    SDL_strlcpy(shower_raw, "", sizeof(shower_raw));
    SDL_strlcpy(caption, "", sizeof(caption));

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

    if (!show_file_resolve_path(path, sizeof(path), name))
    {
        ui_information_scene_leave(&scope);
        log_warn("Failed to resolve file viewer path: %s", name);
        msg_format("Cannot open '%s'.", name);
        message_flush();
        return SHOW_FILE_SCENE_RESULT_SHOWN;
    }

    if (what)
        SDL_strlcpy(caption, what, sizeof(caption));

    log_debug("Opening viewer file: %s (requested: %s)", path, name);
    fff = ang_file_open(path, "r");

    if (!fff)
    {
        ui_information_scene_leave(&scope);
        log_warn("Failed to open viewer file: %s", path);
        msg_format("Cannot open '%s'.", name);
        message_flush();
        return SHOW_FILE_SCENE_RESULT_SHOWN;
    }

    log_debug("Successfully opened viewer file: %s", path);

    while (true)
    {
        if (ang_file_gets(fff, buf, sizeof(buf)))
            break;

        if (prefix(buf, "***** "))
        {
            char b1 = '[';
            char b2 = ']';

            if ((buf[6] == b1) && isalpha((unsigned char)buf[7])
                && (buf[8] == b2) && (buf[9] == ' '))
            {
                menu = true;
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
        if (find)
        {
            int found_line = 0;

            if (show_file_find_next_match(path, find, case_sensitive, line,
                    &found_line))
            {
                line = found_line;
            }
            else
            {
                bell("Search string not found!");
                line = back;
            }
            find = NULL;
        }

        line = show_buffer_clamp_line(line, size);
        if (!show_file_build_ui_scene(&scene, path, caption, shower,
                shower_raw, case_sensitive, menu, size, line)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return SHOW_FILE_SCENE_RESULT_ERROR;
        }

        ch = (char)show_file_wait_key();
        if (!ch)
            continue;

        if (ch == '?')
            break;

        if (ch == '!')
        {
            case_sensitive = !case_sensitive;
            show_file_prepare_search_text(finder, sizeof(finder),
                finder_raw, case_sensitive);
            show_file_prepare_search_text(shower, sizeof(shower),
                shower_raw, case_sensitive);
        }

        if (ch == '&')
        {
            if (!show_file_pause_information_scene(&scope))
                break;
            if (askfor_aux(shower_raw, sizeof(shower_raw)))
            {
                show_file_prepare_search_text(shower, sizeof(shower),
                    shower_raw, case_sensitive);
            }
            if (!show_file_resume_information_scene(&scope))
                break;
        }

        if (ch == '/')
        {
            if (!show_file_pause_information_scene(&scope))
                break;
            if (askfor_aux(finder_raw, sizeof(finder_raw)))
            {
                show_file_prepare_search_text(finder, sizeof(finder),
                    finder_raw, case_sensitive);
                find = finder;
                back = line;
                line = line + 1;

                SDL_strlcpy(shower_raw, finder_raw, sizeof(shower_raw));
                SDL_strlcpy(shower, finder, sizeof(shower));
            }
            if (!show_file_resume_information_scene(&scope))
                break;
        }

        if ((ch == 'n') || (ch == 'N'))
        {
            cptr term = finder[0] ? finder : shower;
            int found_line = 0;
            bool found = false;

            if (!term[0])
            {
                bell("No search string.");
            }
            else if (ch == 'n')
            {
                found = show_file_find_next_match(path, term, case_sensitive,
                    line + 1, &found_line);
            }
            else
            {
                found = show_file_find_previous_match(path, term,
                    case_sensitive, line - 1, &found_line);
            }

            if (found)
                line = found_line;
            else if (term[0])
                bell("No further match.");
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
            line = line - (SHOW_FILE_SCROLL_PAGE_LINES / 2);

        if ((ch == '9') || (ch == '-'))
            line = line - SHOW_FILE_SCROLL_PAGE_LINES;

        if (ch == '7')
            line = 0;

        if ((ch == '2') || (ch == '\n') || (ch == '\r'))
            line = line + 1;

        if (ch == '+')
            line = line + (SHOW_FILE_SCROLL_PAGE_LINES / 2);

        if ((ch == '3') || (ch == ' '))
            line = line + SHOW_FILE_SCROLL_PAGE_LINES;

        if (ch == '1')
            line = size;

        if (ch == ESCAPE)
            break;
    }

    if (fff)
        ang_file_close(fff);
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
    if (!show_buffer_information_scene(main_buffer, line))
    {
        log_warn("buffer viewer: information-scene presentation failed");
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

    result = show_file_information_scene(name, what, line);
    if (result == SHOW_FILE_SCENE_RESULT_ERROR)
    {
        log_warn("file viewer: information-scene presentation failed");
        msg_print("File viewer unavailable.");
        return true;
    }

    return (result != SHOW_FILE_SCENE_RESULT_QUERY_EXIT);
}

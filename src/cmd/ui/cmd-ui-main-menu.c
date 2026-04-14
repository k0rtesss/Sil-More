/* File: cmd-ui-main-menu.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "app/app-session.h"
#include "platform-story-font.h"
#include "platform-input.h"
#include "object/object-ui-select.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-oaths.h"
#include "sound-config.h"
#include "platform-audio.h"

extern struct sound_config g_sound_config;
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "runtime/runtime-game.h"
#include "runtime-cli.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "cmd-ui.h"
#include "ui/ui-file-viewer.h"
#include "ui/ui-information-scene.h"

#define MAIN_MENU_CHARACTER 1
#define MAIN_MENU_KNOWLEDGE 2
#define MAIN_MENU_QUEST_STATUS 3
#define MAIN_MENU_HALLS_OF_MANDOS 4
#define MAIN_MENU_RUN_HISTORY 5
#define MAIN_MENU_MAP 6
#define MAIN_MENU_LOG 7
#define MAIN_MENU_COMBAT_HISTORY 8
#define MAIN_MENU_HINT_MESSAGES 9
#define MAIN_MENU_STORY 10
#define MAIN_MENU_OPTIONS 11
#define MAIN_MENU_HELP 12
#define MAIN_MENU_ABOUT 13
#define MAIN_MENU_SAVE 14
#define MAIN_MENU_SAVE_QUIT 15
#define MAIN_MENU_RETURN_GAME 16

#define MAIN_MENU_MAX 16

typedef struct main_menu_about_line {
    byte attr;
    cptr text;
} main_menu_about_line;

typedef struct main_menu_about_span {
    byte attr;
    cptr text;
} main_menu_about_span;

typedef struct main_menu_scene_scope {
    bool active;
} main_menu_scene_scope;

static char main_menu_read_key(void)
{
    app_session* session = app_session_current();
    app_input input;

    if (!session
        || !app_session_has_flag(session, APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT))
    {
        return ESCAPE;
    }

    while (true)
    {
        while (app_session_pop_input(session, &input))
        {
            if (input.layer != APP_INPUT_LAYER_LEGACY
                || input.type != APP_INPUT_TYPE_KEY)
            {
                continue;
            }

            return (char)(input.payload.key.logical_key & 0xFFu);
        }

        (void)Term_xtra(TERM_XTRA_EVENT, 1);
        (void)Term_xtra(TERM_XTRA_FRESH, 0);
    }
}

static int main_menu_calc_width(void)
{
    /* Keep in sync with the menu labels used by the semantic scene builders. */
    static const char* lines[] = {
        "Character sheet      (c)",
        "Known lore           (a)",
        "Quest status         (t)",
        "Halls of Mandos      (d)",
        "Run history          (v)",
        "Map                  (m)",
        "Log                  (l)",
        "Combat history       (x)",
        "Hint messages        (i)",
        "The story so far     (y)",
        "Options and misc     (o)",
        "Help                 (h)",
        "About                (b)",
        "Save                 (s)",
        "Quit with save       (q)",
        "Return to game       (r)",
    };

    int max_w = 0;
    for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++)
    {
        int w = (int)strlen(lines[i]);
        if (w > max_w)
            max_w = w;
    }
    return max_w;
}

static bool main_menu_choice_is_disabled(int choice)
{
    return (choice == MAIN_MENU_SAVE)
        || (choice == MAIN_MENU_SAVE_QUIT);
}

static void main_menu_prompt_label(int binding, const char* fallback,
    char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static int main_menu_about_count_rows(int indent, int wrap_right,
    const main_menu_about_line* lines, const bool* blank_visible)
{
    int total = 0;

    for (int i = 0; lines[i].text; i++)
    {
        if (!lines[i].text[0])
        {
            if (!blank_visible || blank_visible[i])
                total++;
        }
        else
            total += count_wrapped_lines(lines[i].text, wrap_right, indent);
    }

    return total;
}

static bool main_menu_about_drop_bottom_blank(bool* blank_visible,
    const main_menu_about_line* lines, int line_count)
{
    for (int i = line_count - 1; i >= 0; i--)
    {
        if (!lines[i].text[0] && blank_visible[i])
        {
            blank_visible[i] = false;
            return true;
        }
    }

    return false;
}

static bool main_menu_about_scene_add_text_run(app_ui_scene* scene,
    app_ui_panel* panel, int row, int col, byte attr, const char* text, int len)
{
    char buf[APP_UI_TEXT_MAX];
    const int max_chunk = (int)APP_UI_TEXT_MAX - 1;

    if (!scene || !panel || !text || len <= 0)
        return true;

    while (len > 0)
    {
        int chunk = MIN(len, max_chunk);

        memcpy(buf, text, (size_t)chunk);
        buf[chunk] = '\0';
        if (!app_ui_panel_add_document_text(
                scene, panel, (s16b)row, (s16b)col, attr, buf))
        {
            return false;
        }
        col += chunk;
        text += chunk;
        len -= chunk;
    }

    return true;
}

static bool main_menu_about_scene_add_attr_runs(app_ui_scene* scene,
    app_ui_panel* panel, int row, int col, const char* text,
    const byte* attrs, int len)
{
    int start = 0;

    if (!scene || !panel || !text || !attrs || len <= 0)
        return true;

    while (start < len)
    {
        byte attr = attrs[start];
        int end = start + 1;

        while (end < len && attrs[end] == attr)
            end++;
        if (!main_menu_about_scene_add_text_run(
                scene, panel, row, col, attr, text + start, end - start))
        {
            return false;
        }
        col += end - start;
        start = end;
    }

    return true;
}

static int main_menu_about_flatten_spans(const main_menu_about_span* spans,
    int span_count, byte fallback_attr, char* out_text, byte* out_attrs,
    size_t cap)
{
    int len = 0;

    if (!out_text || !out_attrs || cap < 2 || !spans || span_count <= 0)
        return 0;

    for (int i = 0; i < span_count; i++)
    {
        cptr text = spans[i].text ? spans[i].text : "";
        byte attr = spans[i].attr ? spans[i].attr : fallback_attr;

        while (*text && (size_t)(len + 1) < cap)
        {
            out_text[len] = *text++;
            out_attrs[len] = attr;
            len++;
        }
    }

    out_text[len] = '\0';
    return len;
}

static bool main_menu_about_scene_add_wrapped_spans(
    app_ui_scene* scene, app_ui_panel* panel, int* row, int indent,
    int wrap_right,
    const main_menu_about_span* spans, int span_count, byte fallback_attr)
{
    char flat_text[256];
    byte flat_attrs[256];
    int width = wrap_right - indent;
    int total;
    int start = 0;

    if (!scene || !panel || !row || !spans || span_count <= 0)
        return false;

    if (width < 1)
        width = 1;

    total = main_menu_about_flatten_spans(
        spans, span_count, fallback_attr, flat_text, flat_attrs, sizeof(flat_text));
    if (total <= 0)
        return true;

    while (start < total)
    {
        int remaining = total - start;
        int len = MIN(remaining, width);

        if (start + len < total)
        {
            int break_at = -1;

            for (int i = start; i < start + len; i++)
            {
                if (flat_text[i] == ' ')
                    break_at = i;
            }

            if (break_at > start)
                len = break_at - start;
        }

        if (len <= 0)
            len = MIN(remaining, width);
        if (!main_menu_about_scene_add_attr_runs(
                scene, panel, *row, indent, flat_text + start,
                flat_attrs + start, len))
        {
            return false;
        }

        (*row)++;
        start += len;
        while (start < total && flat_text[start] == ' ')
            start++;
    }

    return true;
}

static bool main_menu_about_scene_add_wrapped_line(app_ui_scene* scene,
    app_ui_panel* panel, int* row, int indent, int wrap_right, byte attr,
    cptr text)
{
    const main_menu_about_span span = { attr, text ? text : "" };

    return main_menu_about_scene_add_wrapped_spans(
        scene, panel, row, indent, wrap_right, &span, 1, attr);
}

static bool main_menu_about_build_ui_scene(app_ui_scene* scene)
{
    app_ui_panel* panel;
    int wid = (Term && Term->wid > 0) ? Term->wid : 80;
    int hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int menu_w;
    int box_w;
    int box_left;
    int text_indent;
    int wrap_right;
    int body_rows;
    int row_top;
    int row;
    int prompt_row;
    static const main_menu_about_line about_lines[] = {
        { TERM_WHITE, "Sil-More is an evolution of SilQ, a famous roguelike" },
        { TERM_WHITE, "taking place in the First Age of Beleriand." },
        { TERM_WHITE, "" },
        { TERM_WHITE, "Developers: k0rtess and sinefabula." },
        { TERM_WHITE, "Gamedesigner: k0rtess." },
        { TERM_WHITE, "Tileset: MicroChasm." },
        { TERM_WHITE, "Main music theme: sinefabula." },
        { TERM_WHITE, "Ambient music theme: West Wind." },
        { TERM_WHITE, "Logo: sinefabula." },
        { TERM_WHITE, "" },
        { TERM_WHITE, "Our love to Maedhros aka Carcharos for playing so much," },
        { TERM_WHITE, "finding those pescy bugs and giving cool ideas." },
        { TERM_L_BLUE, "Special thanks to original Sil and SilQ" },
        { TERM_L_BLUE, "developers: half, Scatha and Quirk." },
        { TERM_WHITE, "" },
        { TERM_WHITE, "Honorable mentions:" },
        { TERM_WHITE, "Sound: Kenney, qubodup, TomMusic, LeoHPaz." },
        { TERM_WHITE, "Walls: Wolffius, Pine Druid, Backterria, Ninjikin." },
        { TERM_WHITE, "" },
        { TERM_L_RED, "And our deep love to Tolkien and his timeless creations." },
        { TERM_WHITE, "" },
        { 0, NULL }
    };

    if (!scene)
        return false;

    menu_w = main_menu_calc_width();
    box_w = MIN(MAX(menu_w + 24, 68), 76);
    if (box_w > (wid > 2 ? wid - 2 : wid))
        box_w = (wid > 2) ? (wid - 2) : wid;
    if (box_w < 1)
        box_w = 1;

    box_left = (wid - box_w) / 2;
    if (box_left < 0)
        box_left = 0;

    text_indent = box_left + 2;
    wrap_right = box_left + box_w - 1;
    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;
    panel->style = APP_UI_PANEL_STYLE_DOCUMENT;
    panel->min_width_px = 0;
    panel->width_cap_px = 0;

    {
        int line_count = 0;
        bool blank_visible[sizeof(about_lines) / sizeof(about_lines[0])] = { false };
        int max_body_rows;

        while (about_lines[line_count].text)
            line_count++;

        for (int i = 0; i < line_count; i++)
            blank_visible[i] = true;

        body_rows = main_menu_about_count_rows(text_indent, wrap_right,
            about_lines, blank_visible);

        max_body_rows = (hgt > 2) ? (hgt - 2) : 0;
        while ((body_rows > max_body_rows)
            && main_menu_about_drop_bottom_blank(blank_visible, about_lines,
                line_count))
        {
            body_rows -= 1;
        }

        row_top = (hgt > body_rows + 2) ? 1 : 0;

        {
            cptr title = "About Sil-More";
            int title_x = box_left + MAX((box_w - (int)strlen(title)) / 2 - 2, 0);

            if (!app_ui_panel_add_document_text(
                    scene, panel, (s16b)row_top, (s16b)title_x, TERM_YELLOW,
                    title))
            {
                return false;
            }
        }

        row = row_top + 1;
        for (int i = 0; i < line_count; i++)
        {
            cptr text = about_lines[i].text;

            if (!text[0])
            {
                if (blank_visible[i])
                    row++;
                continue;
            }

            if (i == 0)
            {
                static const main_menu_about_span intro_label_spans[] = {
                    { TERM_VIOLET, "Sil-More" },
                    { TERM_WHITE, " is an evolution of " },
                    { TERM_L_BLUE, "SilQ" },
                    { TERM_WHITE, ", a famous roguelike" },
                };

                if (!main_menu_about_scene_add_wrapped_spans(scene, panel,
                        &row, text_indent, wrap_right, intro_label_spans,
                        (int)(sizeof(intro_label_spans)
                            / sizeof(intro_label_spans[0])),
                        about_lines[i].attr))
                {
                    return false;
                }
            }
            else if ((i >= 3) && (i <= 8))
            {
                static const main_menu_about_span label_spans[][2] = {
                    {
                        { TERM_YELLOW, "Developers:" },
                        { TERM_WHITE, " k0rtess and sinefabula." },
                    },
                    {
                        { TERM_YELLOW, "Gamedesigner:" },
                        { TERM_WHITE, " k0rtess." },
                    },
                    {
                        { TERM_YELLOW, "Tileset:" },
                        { TERM_WHITE, " MicroChasm." },
                    },
                    {
                        { TERM_YELLOW, "Main music theme:" },
                        { TERM_WHITE, " sinefabula." },
                    },
                    {
                        { TERM_YELLOW, "Ambient music theme:" },
                        { TERM_WHITE, " West Wind." },
                    },
                    {
                        { TERM_YELLOW, "Logo:" },
                        { TERM_WHITE, " sinefabula." },
                    },
                };
                int label_index = i - 3;

                if (!main_menu_about_scene_add_wrapped_spans(scene, panel,
                        &row, text_indent, wrap_right,
                        label_spans[label_index], 2,
                        about_lines[i].attr))
                {
                    return false;
                }
            }
            else if (i == 15)
            {
                static const main_menu_about_span mentions_spans[] = {
                    { TERM_YELLOW, "Honorable mentions:" },
                };

                if (!main_menu_about_scene_add_wrapped_spans(scene, panel,
                        &row, text_indent, wrap_right, mentions_spans, 1,
                        about_lines[i].attr))
                {
                    return false;
                }
            }
            else if (!main_menu_about_scene_add_wrapped_line(scene, panel,
                         &row, text_indent, wrap_right, about_lines[i].attr,
                         text))
            {
                return false;
            }
        }
    }

    prompt_row = row;
    if (prompt_row >= hgt)
        prompt_row = hgt - 1;

    if (steamdeck_controls_active())
    {
        char back_label[16];
        char prompt_buf[48];

        main_menu_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] return", back_label);
        return app_ui_panel_add_document_text(
            scene, panel, (s16b)prompt_row, (s16b)text_indent, TERM_L_WHITE,
            prompt_buf);
    }

    return app_ui_panel_add_document_text(scene, panel, (s16b)prompt_row,
        (s16b)text_indent, TERM_L_WHITE, "[Press any key to return]");
}

static void main_menu_about(void)
{
    static const char about_text[] =
        "About Sil-More\n"
        "\n"
        "Sil-More is an evolution of SilQ, a famous roguelike\n"
        "taking place in the First Age of Beleriand.\n"
        "\n"
        "Developers: k0rtess and sinefabula.\n"
        "Gamedesigner: k0rtess.\n"
        "Tileset: MicroChasm.\n"
        "Main music theme: sinefabula.\n"
        "Ambient music theme: West Wind.\n"
        "Logo: sinefabula.\n"
        "\n"
        "Our love to Maedhros aka Carcharos for playing so much,\n"
        "finding those pescy bugs and giving cool ideas.\n"
        "Special thanks to original Sil and SilQ developers:\n"
        "half, Scatha and Quirk.\n"
        "\n"
        "Honorable mentions:\n"
        "Sound: Kenney, qubodup, TomMusic, LeoHPaz.\n"
        "Walls: Wolffius, Pine Druid, Backterria, Ninjikin.\n"
        "\n"
        "And our deep love to Tolkien and his timeless creations.\n";

    ui_information_scene_scope scope;

    if (p_ptr && p_ptr->playing)
        sdl_music_play_death();

    if (ui_information_scene_enter(&scope))
    {
        app_ui_scene scene;

        if (main_menu_about_build_ui_scene(&scene)
            && ui_information_scene_present_ui(&scene))
        {
            (void)ui_information_scene_wait_key_nonrepeat();
            ui_information_scene_leave(&scope);
        }
        else
        {
            ui_information_scene_leave(&scope);
            show_buffer(about_text, 0);
        }
    }
    else
    {
        show_buffer(about_text, 0);
    }

    if (p_ptr && p_ptr->playing)
        sdl_music_stop_main();
}

static void do_cmd_hint_messages(bool* out_pending_look, int* out_look_y,
    int* out_look_x);

static bool main_menu_scene_enter(main_menu_scene_scope* scope)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (!scope || !session)
        return false;

    memset(scope, 0, sizeof(*scope));
    snapshot = app_session_snapshot(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_DUNGEON)
        return false;

    app_session_clear_interaction(session);
    app_session_clear_dungeon_overlay_scene(session);
    scope->active = true;
    return true;
}

static bool main_menu_scene_add_row(app_ui_panel* panel, int id,
    int highlight, bool death_view, cptr label, cptr meta)
{
    byte attr = TERM_WHITE;
    bool enabled = true;

    if (!panel || !label)
        return false;

    if (death_view && main_menu_choice_is_disabled(id))
    {
        attr = TERM_L_DARK;
        enabled = false;
    }
    else if (highlight == id)
    {
        attr = TERM_L_BLUE;
    }

    return app_ui_panel_add_row(panel, (s16b)id, attr, enabled,
        highlight == id, "", label, meta ? meta : "");
}

static bool main_menu_build_ui_scene(app_ui_scene* scene, int highlight,
    bool death_view)
{
    app_ui_panel* panel;

    if (!scene)
        return false;

    if (highlight < 1 || highlight > MAIN_MENU_MAX)
        highlight = MAIN_MENU_CHARACTER;
    if (death_view && main_menu_choice_is_disabled(highlight))
        highlight = MAIN_MENU_RETURN_GAME;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 300, 460);

    return main_menu_scene_add_row(panel, MAIN_MENU_CHARACTER, highlight,
               death_view, "Character sheet      (c)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_KNOWLEDGE, highlight,
            death_view, "Known lore           (a)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_QUEST_STATUS, highlight,
            death_view, "Quest status         (t)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_HALLS_OF_MANDOS, highlight,
            death_view, "Halls of Mandos      (d)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_RUN_HISTORY, highlight,
            death_view, "Run history          (v)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_MAP, highlight,
            death_view, "Map                  (m)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_LOG, highlight,
            death_view, "Log                  (l)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_COMBAT_HISTORY, highlight,
            death_view, "Combat history       (x)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_HINT_MESSAGES, highlight,
            death_view, "Hint messages        (i)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_STORY, highlight,
            death_view, "The story so far     (y)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_OPTIONS, highlight,
            death_view, "Options and misc     (o)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_HELP, highlight,
            death_view, "Help                 (h)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_ABOUT, highlight,
            death_view, "About                (b)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_SAVE, highlight,
            death_view, "Save                 (s)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_SAVE_QUIT, highlight,
            death_view, "Quit with save       (q)", "")
        && main_menu_scene_add_row(panel, MAIN_MENU_RETURN_GAME, highlight,
            death_view, "Return to game       (r)", "");
}

static bool main_menu_scene_present(main_menu_scene_scope* scope, int highlight,
    bool death_view)
{
    app_session* session = app_session_current();
    app_ui_scene scene;

    if (!scope || !scope->active || !session)
        return false;

    if (!main_menu_build_ui_scene(&scene, highlight, death_view))
        return false;

    app_session_clear_interaction(session);
    if (!app_session_publish_dungeon_overlay_scene(session, &scene))
        return false;

    (void)Term_xtra(TERM_XTRA_FRESH, 0);
    return true;
}

static void main_menu_scene_leave(main_menu_scene_scope* scope, bool refresh)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active)
        return;

    if (session)
    {
        app_session_clear_interaction(session);
        app_session_clear_dungeon_overlay_scene(session);
    }
    if (refresh)
        (void)Term_xtra(TERM_XTRA_FRESH, 0);

    scope->active = false;
}

static bool main_menu_action_opens_scene(int actiontype)
{
    return actiontype == MAIN_MENU_CHARACTER
        || actiontype == MAIN_MENU_KNOWLEDGE
        || actiontype == MAIN_MENU_QUEST_STATUS
        || actiontype == MAIN_MENU_HALLS_OF_MANDOS
        || actiontype == MAIN_MENU_RUN_HISTORY
        || actiontype == MAIN_MENU_MAP
        || actiontype == MAIN_MENU_LOG
        || actiontype == MAIN_MENU_COMBAT_HISTORY
        || actiontype == MAIN_MENU_HINT_MESSAGES
        || actiontype == MAIN_MENU_STORY
        || actiontype == MAIN_MENU_OPTIONS
        || actiontype == MAIN_MENU_HELP
        || actiontype == MAIN_MENU_ABOUT;
}

/*
 * Performs the interface and selection work for the main menu.
 */
static int main_menu_aux(int* highlight, main_menu_scene_scope* menu_scene_scope)
{
    char ch;
    bool death_view = death_spectator_active();
    bool steamdeck = steamdeck_controls_active();

    if (!menu_scene_scope || !menu_scene_scope->active)
    {
        log_warn("main menu: snapshot overlay required; legacy branch removed");
        msg_print("Main menu requires active snapshot UI rendering.");
        return (-1);
    }

    if (!main_menu_scene_present(menu_scene_scope, *highlight, death_view))
    {
        log_warn("main menu: failed to republish snapshot overlay");
        msg_print("Main menu requires active snapshot UI rendering.");
        return (-1);
    }

    if (death_view && main_menu_choice_is_disabled(*highlight))
        *highlight = MAIN_MENU_RETURN_GAME;

    /* Get key (while allowing menu commands). */
    ch = main_menu_read_key();

    // choose an option by letter - alphabetical mapping (updated for new order)
    switch (ch)
    {
    case 'c':
        *highlight = 1;
        return (*highlight);  // Character sheet
    case 'a':
        *highlight = 2;
        return (*highlight);  // Known lore
    case 't':
        *highlight = 3;
        return (*highlight);  // Quest status
    case 'd':
        *highlight = 4;
        return (*highlight);  // Halls of Mandos
    case 'v':
        *highlight = 5;
        return (*highlight);  // Run history
    case 'm':
        *highlight = 6;
        return (*highlight);  // Map
    case 'l':
        *highlight = 7;
        return (*highlight);  // Log
    case 'x':
        *highlight = 8;
        return (*highlight); // Combat history
    case 'i':
        *highlight = 9;
        return (*highlight); // Hint messages
    case 'y':
        *highlight = 10;
        return (*highlight); // The story so far
    case 'o':
        *highlight = 11;
        return (*highlight); // Options and misc
    case 'h':
        *highlight = 12;
        return (*highlight); // Help
    case 'b':
        *highlight = MAIN_MENU_ABOUT;
        return (*highlight); // About
    case 's':
        if (death_view) {
            msg_print("You can no longer take that action.");
            break;
        }
        *highlight = MAIN_MENU_SAVE;
        return (*highlight); // Save
    case 'q':
        if (death_view) {
            msg_print("You can no longer take that action.");
            break;
        }
        *highlight = MAIN_MENU_SAVE_QUIT;
        return (*highlight); // Quit with save
    case 'r':
        *highlight = MAIN_MENU_RETURN_GAME;
        return (*highlight); // Return to game
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
        || (steamdeck && ch == steamdeck_confirm_key()))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = MAIN_MENU_MAX;
        while (death_view && main_menu_choice_is_disabled(*highlight))
        {
            if (*highlight > 1)
                (*highlight)--;
            else
                *highlight = MAIN_MENU_MAX;
        }
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < MAIN_MENU_MAX)
            (*highlight)++;
        else if (*highlight == MAIN_MENU_MAX)
            *highlight = 1;
        while (death_view && main_menu_choice_is_disabled(*highlight))
        {
            if (*highlight < MAIN_MENU_MAX)
                (*highlight)++;
            else
                *highlight = 1;
        }
    }

    /* Leave menu */
    if ((ch == ESCAPE) || (ch == '4')
        || (steamdeck && ch == steamdeck_back_key()))
    {
        return (-1);
    }

    return (0);
}

/*
 * Brings up a menu for choosing some of the game's more abstruse options.
 */
void do_cmd_main_menu(void)
{
    app_session* session = app_session_current();
    int actiontype = -1;
    int highlight = 1;
    bool leave_menu = false;
    bool pending_hint_look = false;
    int pending_hint_look_y = -1;
    int pending_hint_look_x = -1;
    main_menu_scene_scope menu_scene_scope;
    bool refresh_on_leave;

    /* Clear any active banner before opening main menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    if (!main_menu_scene_enter(&menu_scene_scope))
    {
        if (app_session_interactions_enabled(session))
            app_session_clear_interaction(session);
        log_warn("main menu: snapshot overlay required; legacy branch removed");
        msg_print("Main menu requires active snapshot UI rendering.");
        return;
    }

    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        actiontype = main_menu_aux(&highlight, &menu_scene_scope);

        if (death_spectator_active() && main_menu_choice_is_disabled(actiontype))
        {
            msg_print("You can no longer take that action.");
            continue;
        }

        switch (actiontype)
        {
        case 1: // Character sheet (c)
        case 2: // Known lore (a)
        case 3: // Quest status (t)
        case 4: // Halls of Mandos (d)
        case 5: // Run history (v)
        case 6: // Map (m)
        case 7: // Log (l)
        case 8: // Combat history (x)
        case 9: // Hint messages (i)
        case 10: // The story so far (y)
        case 11: // Options and misc (o)
        case 12: // Help (h)
        case 13: // About (b)
        case 14: // Save (s)
        case 15: // Quit with save (q)
        case 16: // Return to game (r)
        case -1:
        {
            leave_menu = true;
            break;
        }
        default:
        {
            /* Invalid selection - stay in menu */
            break;
        }
        }
    }

    refresh_on_leave = !main_menu_action_opens_scene(actiontype);

    main_menu_scene_leave(&menu_scene_scope, refresh_on_leave);

    if (app_session_interactions_enabled(session))
        app_session_clear_interaction(session);

    switch (actiontype)
    {
    case 1: // Character sheet (c)
    {
        do_cmd_character_sheet();
        break;
    }
    case 2: // Known lore (a)
    {
        do_cmd_knowledge_browser_page(cmd_ui_knowledge_last_page());
        break;
    }
    case 3: // Quest status (t)
    {
        do_cmd_quest_status();
        break;
    }
    case MAIN_MENU_HALLS_OF_MANDOS: // Halls of Mandos (d)
    {
        log_info("main menu: opening Halls of Mandos view");
        show_scores_interactive(true);
        break;
    }
    case MAIN_MENU_RUN_HISTORY: // Run history (v)
    {
        do_cmd_run_history();
        break;
    }
    case 6: // Map (m)
    {
        do_cmd_view_map();
        break;
    }
    case MAIN_MENU_LOG: // Log (l)
    {
        do_cmd_messages();
        break;
    }
    case MAIN_MENU_COMBAT_HISTORY: // Combat history (x)
    {
        do_cmd_combat_history();
        break;
    }
    case MAIN_MENU_HINT_MESSAGES: // Hint messages (i)
    {
        do_cmd_hint_messages(&pending_hint_look, &pending_hint_look_y,
            &pending_hint_look_x);
        break;
    }
    case 10: // The story so far (y)
    {
        print_story(15, 1);
        break;
    }
    case 11: // Options and misc (o)
    {
        do_cmd_options();
        break;
    }
    case 12: // Help (h)
    {
        do_cmd_help();
        break;
    }
    case MAIN_MENU_ABOUT: // About (b)
    {
        main_menu_about();
        break;
    }
    case MAIN_MENU_SAVE: // Save (s)
    {
        do_cmd_save_game();
        break;
    }
    case MAIN_MENU_SAVE_QUIT: // Quit with save (q)
    {
        /* Stop playing */
        p_ptr->playing = false;

        /* Mark that we want to quit to menu, not exit application */
        p_ptr->quit_to_menu = true;

        /* Leaving */
        p_ptr->leaving = true;
        break;
    }
    case MAIN_MENU_RETURN_GAME: // Return to game (r)
    case -1:
    {
        break;
    }
    default:
    {
        break;
    }
    }

    if (pending_hint_look)
    {
        do_cmd_redraw();
        do_cmd_look_at(pending_hint_look_y, pending_hint_look_x);
    }

}

/*
 * Recall the most recent message
 */
void do_cmd_message_one(void)
{
    /* Recall one message XXX XXX XXX */
    c_prt(message_color(0), format("> %s", message_str(0)), 0, 0);
}

#define HINT_MESSAGE_UI_ROW_WINDOW 48
#define MESSAGE_RECALL_UI_PAGE_SIZE 18
#define MESSAGE_RECALL_UI_OFFSET_STEP 24

static bool hint_message_has_source(const hint_message_meta* meta)
{
    return meta && meta->source_y >= 0 && meta->source_x >= 0
        && meta->source_y < p_ptr->cur_map_hgt && meta->source_x < p_ptr->cur_map_wid;
}

static bool hint_message_is_word_boundary(char ch)
{
    return (ch == '\0') || !isalnum((unsigned char)ch);
}

static bool hint_message_phrase_matches(const char* line, int offset, const char* phrase)
{
    size_t len;

    if (!line || !phrase || !phrase[0])
        return false;

    len = strlen(phrase);
    if (strncmp(line + offset, phrase, len) != 0)
        return false;

    if (offset > 0 && !hint_message_is_word_boundary(line[offset - 1]))
        return false;

    return hint_message_is_word_boundary(line[offset + len]);
}

static int hint_message_match_length(const char* line, int offset,
    const hint_message_meta* meta, byte* out_attr)
{
    int best_len = 0;
    byte best_attr = TERM_WHITE;

    if (!meta)
        return 0;

    for (int cue = 0; cue < meta->cue_count; ++cue)
    {
        const char* dist = meta->cue_dists[cue];
        const char* dir = meta->cue_dirs[cue];

        if (hint_message_phrase_matches(line, offset, dist))
        {
            int len = (int)strlen(dist);
            if (len > best_len)
            {
                best_len = len;
                best_attr = TERM_YELLOW;
            }
        }

        if (hint_message_phrase_matches(line, offset, dir))
        {
            int len = (int)strlen(dir);
            if (len > best_len)
            {
                best_len = len;
                best_attr = TERM_L_BLUE;
            }
        }
    }

    if (out_attr)
        *out_attr = best_attr;

    return best_len;
}

static bool hint_message_append_rich_span(app_ui_scene* scene,
    app_ui_panel* panel, byte attr, byte story, const char* text, size_t len)
{
    char buf[APP_UI_TEXT_MAX];

    if (!scene || !panel || !text || len == 0)
        return true;

    while (len > 0)
    {
        size_t chunk = len;

        if (chunk >= sizeof(buf))
            chunk = sizeof(buf) - 1u;
        memcpy(buf, text, chunk);
        buf[chunk] = '\0';
        if (!app_ui_panel_add_rich_text_ex(scene, panel, attr, story, buf))
            return false;
        text += chunk;
        len -= chunk;
    }

    return true;
}

static bool hint_message_append_colored_rich_line(app_ui_scene* scene,
    app_ui_panel* panel, byte base_attr, byte story, const char* line,
    const hint_message_meta* meta)
{
    int start = 0;
    int len;

    if (!scene || !panel)
        return false;
    if (!line)
        line = "";

    len = (int)strlen(line);
    for (int i = 0; i < len; )
    {
        byte match_attr = base_attr;
        int match_len = hint_message_match_length(line, i, meta, &match_attr);

        if (match_len > 0)
        {
            if (i > start)
            {
                if (!hint_message_append_rich_span(scene, panel, base_attr,
                        story, line + start, (size_t)(i - start)))
                {
                    return false;
                }
            }

            if (!hint_message_append_rich_span(scene, panel, match_attr, story,
                    line + i, (size_t)match_len))
            {
                return false;
            }
            i += match_len;
            start = i;
        }
        else
        {
            ++i;
        }
    }

    if (start < len)
    {
        if (!hint_message_append_rich_span(scene, panel, base_attr, story,
                line + start, (size_t)(len - start)))
        {
            return false;
        }
    }

    return true;
}

static const char* hint_message_title(int index)
{
    byte line_count = hint_messages_message_line_count(index);
    for (int li = 0; li < line_count; ++li)
    {
        const char* line = hint_messages_message_line(index, li);
        if (line && line[0])
            return line;
    }

    return "";
}

static void hint_message_fit_text(char* buf, size_t buf_sz, const char* title,
    int max_len)
{
    if (!buf || buf_sz == 0)
        return;

    if (!title)
        title = "";

    if (max_len < 4 || (int)strlen(title) <= max_len)
    {
        strnfmt(buf, buf_sz, "%s", title);
        return;
    }

    strnfmt(buf, buf_sz, "%.*s...", max_len - 3, title);
}

static void hint_message_build_cue_summary(const hint_message_meta* meta,
    char* buf, size_t buf_sz)
{
    if (!buf || buf_sz == 0)
        return;

    buf[0] = '\0';
    if (!meta || meta->cue_count <= 0)
        return;

    for (int cue = 0; cue < meta->cue_count; ++cue)
    {
        char cue_buf[48];

        cue_buf[0] = '\0';
        if (cue > 0)
            SDL_strlcat(buf, "; ", buf_sz);
        if (meta->cue_dists[cue][0] && meta->cue_dirs[cue][0])
        {
            strnfmt(cue_buf, sizeof(cue_buf), "%s %s",
                meta->cue_dists[cue], meta->cue_dirs[cue]);
        }
        else if (meta->cue_dists[cue][0])
        {
            SDL_strlcpy(cue_buf, meta->cue_dists[cue], sizeof(cue_buf));
        }
        else if (meta->cue_dirs[cue][0])
        {
            SDL_strlcpy(cue_buf, meta->cue_dirs[cue], sizeof(cue_buf));
        }
        SDL_strlcat(buf, cue_buf, buf_sz);
    }
}

static void hint_message_add_list_footer_actions(app_ui_panel* panel,
    bool can_look)
{
    if (!panel)
        return;

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Open");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, can_look,
        "l", "Look");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "Esc", "Back");
}

static void hint_message_add_detail_footer_actions(app_ui_panel* panel,
    bool can_look)
{
    if (!panel)
        return;

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, can_look,
        "l", "Look");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Esc", "Back");
}

static void hint_message_add_list_detail(app_ui_panel* panel, int index)
{
    hint_message_meta meta;
    byte line_count;
    char cue_buf[APP_UI_TEXT_MAX];

    if (!panel)
        return;

    line_count = hint_messages_message_line_count(index);
    if (!line_count)
        return;

    hint_messages_message_meta(index, &meta);
    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected");

    for (int li = 0; li < line_count
        && panel->detail_line_count < APP_UI_DETAIL_LINE_MAX; ++li)
    {
        const char* line = hint_messages_message_line(index, li);

        if (!line || !line[0])
            continue;
        (void)app_ui_panel_add_detail_line_ex(panel,
            (li == 0) ? TERM_L_WHITE : TERM_WHITE,
            STORY_FLAG_USE, line);
    }

    hint_message_build_cue_summary(&meta, cue_buf, sizeof(cue_buf));
    if (cue_buf[0] && panel->detail_line_count < APP_UI_DETAIL_LINE_MAX)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, cue_buf);
    }

    if (hint_message_has_source(&meta)
        && panel->detail_line_count < APP_UI_DETAIL_LINE_MAX)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_L_BLUE,
            "Press l to look at the skeleton.");
    }
}

static bool hint_message_build_ui_list_scene(app_ui_scene* scene, int n,
    int sel)
{
    app_ui_panel* panel;
    int window_start;
    int window_end;
    char subtitle[64];
    hint_message_meta selected_meta;

    if (!scene || n <= 0)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 980, 2048);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Hint Messages");
    strnfmt(subtitle, sizeof(subtitle), "%d remembered on this level", n);
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

    window_start = sel - (HINT_MESSAGE_UI_ROW_WINDOW / 2);
    if (window_start < 0)
        window_start = 0;
    if (window_start > n - HINT_MESSAGE_UI_ROW_WINDOW)
        window_start = MAX(0, n - HINT_MESSAGE_UI_ROW_WINDOW);
    window_end = MIN(n, window_start + HINT_MESSAGE_UI_ROW_WINDOW);

    for (int idx = window_start; idx < window_end; ++idx)
    {
        hint_message_meta meta;
        char title_buf[APP_UI_LABEL_MAX];
        char cue_buf[APP_UI_META_MAX];
        const char* title = hint_message_title(idx);

        hint_messages_message_meta(idx, &meta);
        hint_message_fit_text(title_buf, sizeof(title_buf), title,
            APP_UI_LABEL_MAX - 1);
        hint_message_build_cue_summary(&meta, cue_buf, sizeof(cue_buf));
        if (!app_ui_panel_add_row_ex(panel, (s16b)idx,
                (idx == sel) ? TERM_L_WHITE : TERM_WHITE,
                cue_buf[0] ? TERM_SLATE : TERM_WHITE,
                0, '\0', true, idx == sel, "", title_buf, cue_buf))
        {
            return false;
        }
    }

    hint_messages_message_meta(sel, &selected_meta);
    hint_message_add_list_detail(panel, sel);
    hint_message_add_list_footer_actions(panel,
        hint_message_has_source(&selected_meta));
    return true;
}

static bool hint_message_build_ui_detail_scene(app_ui_scene* scene, int index)
{
    app_ui_panel* panel;
    hint_message_meta meta;
    byte line_count;

    if (!scene)
        return false;

    hint_messages_ensure_level_state();
    line_count = hint_messages_message_line_count(index);
    if (!line_count)
        return false;

    hint_messages_message_meta(index, &meta);
    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED;
    panel->accent_attr = TERM_SLATE;
    app_ui_panel_set_widths(panel, 1500, 2800);
    app_ui_panel_set_title(panel, TERM_L_WHITE, hint_message_title(index));
    if (hint_message_has_source(&meta))
    {
        app_ui_panel_set_subtitle(panel, TERM_L_BLUE,
            "Press l to look at the skeleton.");
    }
    else
    {
        app_ui_panel_set_subtitle(panel, TERM_SLATE,
            "Press Esc to return.");
    }

    for (int li = 0; li < line_count; ++li)
    {
        const char* line = hint_messages_message_line(index, li);
        const hint_message_meta* line_meta = (li == 0) ? NULL : &meta;

        if (!app_ui_panel_begin_rich_paragraph(scene, panel))
            return false;
        if (line && line[0])
        {
            if (!hint_message_append_colored_rich_line(scene, panel,
                    (li == 0) ? TERM_L_WHITE : TERM_WHITE, STORY_FLAG_USE,
                    line, line_meta))
            {
                return false;
            }
        }
        else if (!app_ui_panel_add_rich_text_ex(scene, panel, TERM_WHITE,
                     STORY_FLAG_USE, " "))
        {
            return false;
        }
    }

    hint_message_add_detail_footer_actions(panel, hint_message_has_source(&meta));
    return true;
}

static bool hint_message_show_ui_scene(int index, int* look_y,
    int* look_x, bool* out_request_look)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    hint_message_meta meta;
    byte line_count;

    if (out_request_look)
        *out_request_look = false;

    hint_messages_ensure_level_state();
    line_count = hint_messages_message_line_count(index);
    if (!line_count)
        return false;

    if (!ui_information_scene_enter(&scope))
        return false;

    hint_messages_message_meta(index, &meta);

    while (1)
    {
        char ch;

        if (!hint_message_build_ui_detail_scene(&scene, index))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        if (!ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = (char)ui_information_scene_wait_key_nonrepeat();
        if ((ch == 'l' || ch == 'L') && hint_message_has_source(&meta))
        {
            if (look_y)
                *look_y = meta.source_y;
            if (look_x)
                *look_x = meta.source_x;
            if (out_request_look)
                *out_request_look = true;
            ui_information_scene_leave(&scope);
            return true;
        }

        break;
    }

    ui_information_scene_leave(&scope);
    return true;
}

void show_hint_message_screen(int index)
{
    int look_y = -1;
    int look_x = -1;
    bool request_look = false;

    if (!ui_information_scene_supported())
    {
        log_warn("hint message detail: snapshot renderer required; legacy detail renderer removed");
        msg_print("Hint message viewer requires the snapshot UI renderer.");
        return;
    }

    if (!hint_message_show_ui_scene(index, &look_y, &look_x,
            &request_look))
    {
        log_warn("hint message detail: semantic scene presentation failed on the snapshot renderer path");
        msg_print("Hint message viewer unavailable.");
        return;
    }

    if (request_look)
    {
        do_cmd_redraw();
        do_cmd_look_at(look_y, look_x);
    }
}

static bool do_cmd_hint_messages_information_scene(bool* out_pending_look,
    int* out_look_y, int* out_look_x)
{
    ui_information_scene_scope scope;
    bool pending_look = false;
    int look_y = -1;
    int look_x = -1;
    int n;
    int sel = 0;

    hint_messages_ensure_level_state();
    n = (int)hint_messages_count_for_save();
    if (n <= 0)
        return false;

    if (!ui_information_scene_enter(&scope))
        return false;

    while (1)
    {
        app_ui_scene scene;
        char ch;

        if (sel < 0)
            sel = 0;
        if (sel >= n)
            sel = n - 1;

        if (!hint_message_build_ui_list_scene(&scene, n, sel)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = (char)ui_information_scene_wait_key();

        if (ch == ESCAPE)
            break;

        if (ch == '8')
        {
            sel = (sel > 0) ? (sel - 1) : (n - 1);
            continue;
        }

        if (ch == '2')
        {
            sel = (sel + 1 < n) ? (sel + 1) : 0;
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
        {
            int selected_look_y = -1;
            int selected_look_x = -1;

            bool request_look = false;

            if (!hint_message_show_ui_scene(sel, &selected_look_y,
                    &selected_look_x, &request_look))
            {
                ui_information_scene_leave(&scope);
                return false;
            }

            if (request_look)
            {
                pending_look = true;
                look_y = selected_look_y;
                look_x = selected_look_x;
                break;
            }
            continue;
        }

        if (ch == 'l' || ch == 'L')
        {
            hint_message_meta meta;

            hint_messages_message_meta(sel, &meta);
            if (hint_message_has_source(&meta))
            {
                pending_look = true;
                look_y = meta.source_y;
                look_x = meta.source_x;
                break;
            }

            bell(NULL);
            continue;
        }

        bell(NULL);
    }

    ui_information_scene_leave(&scope);

    if (out_pending_look)
        *out_pending_look = pending_look;
    if (out_look_y)
        *out_look_y = look_y;
    if (out_look_x)
        *out_look_x = look_x;

    return true;
}

static void do_cmd_hint_messages(bool* out_pending_look, int* out_look_y,
    int* out_look_x)
{
    /* Clear any active banner before opening hint messages */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    hint_messages_ensure_level_state();

    int n = (int)hint_messages_count_for_save();
    if (n <= 0)
    {
        msg_print("You recall no hint messages on this level.");
        return;
    }

    if (!ui_information_scene_supported())
    {
        log_warn("hint messages: snapshot renderer required; legacy hint-message renderer removed");
        msg_print("Hint message browser requires the snapshot UI renderer.");
        return;
    }

    if (!do_cmd_hint_messages_information_scene(out_pending_look,
            out_look_y, out_look_x))
    {
        log_warn("hint messages: semantic scene presentation failed on the snapshot renderer path");
        msg_print("Hint message browser unavailable.");
    }
}

/*
 * Show previous messages to the user
 *
 * The screen format uses line 0 and 23 for headers and prompts,
 * skips line 1 and 22, and uses line 2 thru 21 for old messages.
 *
 * This command shows you which commands you are viewing, and allows
 * you to "search" for strings in the recall.
 *
 * Note that messages may be longer than 80 characters, but they are
 * displayed using "infinite" length, with a special sub-command to
 * "slide" the virtual display to the left or right.
 *
 * Attempt to only hilite the matching portions of the string.
 */
static bool do_cmd_messages_information_scene(void)
{
    ui_information_scene_scope scope;
    char shower[80];
    char finder[80];
    int i = 0;
    int q = 0;

    if (!ui_information_scene_enter(&scope))
        return false;

    SDL_strlcpy(finder, "", sizeof(finder));
    SDL_strlcpy(shower, "", sizeof(shower));

    while (true)
    {
        app_ui_scene scene;
        int n = message_num();
        int old_i;
        char ch;
        app_ui_panel* panel;
        char subtitle[APP_UI_TEXT_MAX];
        int shown = 0;
        int last_shown;

        app_ui_scene_init(&scene);
        panel = app_ui_scene_append_panel(&scene, APP_UI_LAYER_BROWSER);
        if (!panel)
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        panel->style = APP_UI_PANEL_STYLE_PLAIN;
        panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
            | APP_UI_PANEL_FLAG_LEFT_ANCHORED;
        panel->accent_attr = TERM_SLATE;
        app_ui_panel_set_widths(panel, 1500, 2800);
        app_ui_panel_set_title(panel, TERM_WHITE, "Message Recall");

        shown = MIN(MESSAGE_RECALL_UI_PAGE_SIZE, MAX(0, n - i));
        last_shown = (shown > 0) ? (i + shown - 1) : i;
        strnfmt(subtitle, sizeof(subtitle), "%d-%d of %d, offset %d",
            i, last_shown, n, q);
        app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);
        if (shower[0])
        {
            char filter_buf[APP_UI_TEXT_MAX];

            strnfmt(filter_buf, sizeof(filter_buf), "Highlight: %s", shower);
            (void)app_ui_panel_add_body_line(panel, TERM_L_BLUE, filter_buf);
        }

        for (shown = shown - 1; shown >= 0; --shown)
        {
            cptr msg = message_str((s16b)(i + shown));
            byte attr = message_color((s16b)(i + shown));
            cptr visible = "";

            if ((int)strlen(msg) >= q)
                visible = msg + q;

            if (!app_ui_panel_begin_rich_paragraph(&scene, panel))
            {
                ui_information_scene_leave(&scope);
                return false;
            }

            if (shower[0] && visible[0])
            {
                cptr cursor = visible;
                size_t needle_len = strlen(shower);

                while (needle_len > 0)
                {
                    cptr match = strstr(cursor, shower);

                    if (!match)
                        break;
                    if (!hint_message_append_rich_span(&scene, panel, attr, 0,
                            cursor, (size_t)(match - cursor))
                        || !hint_message_append_rich_span(&scene, panel,
                            TERM_YELLOW, 0, match, needle_len))
                    {
                        ui_information_scene_leave(&scope);
                        return false;
                    }
                    cursor = match + needle_len;
                }

                if (cursor[0]
                    && !hint_message_append_rich_span(&scene, panel, attr, 0,
                        cursor, strlen(cursor)))
                {
                    ui_information_scene_leave(&scope);
                    return false;
                }
            }
            else if (visible[0]
                && !app_ui_panel_add_rich_text(&scene, panel, attr, visible))
            {
                ui_information_scene_leave(&scope);
                return false;
            }
        }

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "p", "Older");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_L_BLUE, true,
            "n", "Newer");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "4/6", "View");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "=", "Show");
        (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
            "/", "Find");
        (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
            "Esc", "Back");

        if (!ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        ch = (char)ui_information_scene_wait_key();

        if (ch == ESCAPE)
            break;

        old_i = i;

        if (ch == '4')
        {
            q = (q >= MESSAGE_RECALL_UI_OFFSET_STEP)
                ? (q - MESSAGE_RECALL_UI_OFFSET_STEP)
                : 0;
            continue;
        }

        if (ch == '6')
        {
            q += MESSAGE_RECALL_UI_OFFSET_STEP;
            continue;
        }

        if (ch == '=')
        {
            if (!term_get_string("Show: ", shower, sizeof(shower)))
                continue;

            continue;
        }

        if (ch == '/')
        {
            s16b z;

            if (term_get_string("Find: ", finder, sizeof(finder)))
            {
                SDL_strlcpy(shower, finder, sizeof(shower));

                for (z = i + 1; z < n; z++)
                {
                    cptr msg = message_str(z);

                    if (strstr(msg, finder))
                    {
                        i = z;
                        break;
                    }
                }
            }

            continue;
        }

        if ((ch == 'p') || (ch == KTRL('P')) || (ch == ' '))
        {
            if (i + 20 < n)
                i += 20;
        }

        if (ch == '+')
        {
            if (i + 10 < n)
                i += 10;
        }

        if ((ch == '8') || (ch == '\n') || (ch == '\r'))
        {
            if (i + 1 < n)
                i += 1;
        }

        if ((ch == 'n') || (ch == KTRL('N')))
            i = (i >= 20) ? (i - 20) : 0;

        if (ch == '-')
            i = (i >= 10) ? (i - 10) : 0;

        if (ch == '2')
            i = (i >= 1) ? (i - 1) : 0;

        if (i == old_i)
            bell(NULL);
    }

    ui_information_scene_leave(&scope);
    return true;
}

void do_cmd_messages(void)
{
    /* Clear any active banner before opening message history */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    if (!ui_information_scene_supported())
    {
        log_warn("message recall: snapshot renderer required; legacy message-recall renderer removed");
        msg_print("Message recall requires the snapshot UI renderer.");
        return;
    }

    if (!do_cmd_messages_information_scene())
    {
        log_warn("message recall: semantic scene presentation failed on the snapshot renderer path");
        msg_print("Message recall unavailable.");
    }
    return;
}


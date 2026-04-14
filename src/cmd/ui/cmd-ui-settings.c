/* File: cmd-ui-settings.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "platform-audio.h"
#include "platform-config.h"
#include "platform-input.h"
#include "platform-time.h"
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
#include <ctype.h>
#include "h-define.h"
#include "app/app-ui.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "cmd-ui.h"
#include "ui/ui-look-sidebar.h"
#include "ui/ui-information-scene.h"

#define COLOR_SAMPLE "###"

static int settings_sdl_min_terminal_mode(void);
static int settings_ui_named_sdl_cols(void);
static int settings_ui_named_sdl_rows(void);
static void do_cmd_macro_aux(char* buf);
static void do_cmd_macro_aux_keymap(char* buf);
static bool settings_ui_prompt_string(cptr title, cptr prompt, cptr note,
    char* buf, size_t len);
static app_ui_panel* settings_browser_scene_begin_ex(app_ui_scene* scene,
    cptr title, cptr subtitle, int min_width_px, int width_cap_px);
static bool settings_browser_add_pair_row(app_ui_panel* panel, s16b id,
    byte attr, byte meta_attr, bool enabled, bool selected, cptr label,
    cptr meta);

static char settings_ui_read_legacy_key(bool scan)
{
    app_session* session = app_session_current();
    app_input input;

    if (!session)
        return scan ? '\0' : ESCAPE;

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

        if (scan)
            return '\0';

        (void)Term_xtra(TERM_XTRA_EVENT, 1);
        (void)Term_xtra(TERM_XTRA_FRESH, 0);
    }
}

static bool settings_sdl_hide_left_panel(void)
{
    return get_sdl_hide_left_panel();
}

static void settings_sdl_set_hide_left_panel(bool hidden)
{
    set_sdl_hide_left_panel(hidden);
}

static cptr dump_seperator = "#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#";

static void dump_visual_pair(
    SDL_IOStream* fff, const char* tag, int index, byte attr, byte chr)
{
    bool attr_tile = (attr & TILE_FLAG) != 0;
    bool char_tile = (chr & TILE_FLAG) != 0;

    SDL_IOprintf(fff, "%s:%d:", tag, index);
    if (attr_tile)
        SDL_IOprintf(fff, "R%d", TILE_GET_INDEX(attr));
    else
        SDL_IOprintf(fff, "0x%02X", attr);

    SDL_WriteU8(fff, ':');

    if (char_tile)
        SDL_IOprintf(fff, "C%d", TILE_GET_INDEX(chr));
    else
        SDL_IOprintf(fff, "0x%02X", (byte)chr);

SDL_WriteU8(fff, '\n');
SDL_WriteU8(fff, '\n');
}

/*
 * Remove old lines from pref files
 */
static void remove_old_dump(cptr orig_file, cptr mark)
{
    SDL_IOStream* tmp_fff, *orig_fff;

    char tmp_file[1024];
    char buf[1024];
    bool between_marks = false;
    bool changed = false;
    char expected_line[1024];

    /* Open an old dump file in read-only mode */
    orig_fff = sdl_fopen(orig_file, "r");

    /* If original file does not exist, nothing to do */
    if (!orig_fff)
        return;

    /* Open a new temporary file */
    tmp_fff = sdl_fopen_temp(tmp_file, sizeof(tmp_file));

    if (!tmp_fff)
    {
        msg_format("Failed to create temporary file %s.", tmp_file);
        msg_print(NULL);
        return;
    }

    strnfmt(expected_line, sizeof(expected_line), "%s begin %s", dump_seperator,
        mark);

    /* Loop for every line */
    while (true)
    {
        /* Read a line */
        if (sdl_fgets(orig_fff, buf, sizeof(buf)))
        {
            /* End of file but no end marker */
            if (between_marks)
                changed = false;

            break;
        }

        /* Is this line a header/footer? */
        if (strncmp(buf, dump_seperator, strlen(dump_seperator)) == 0)
        {
            /* Found the expected line? */
            if (strcmp(buf, expected_line) == 0)
            {
                if (!between_marks)
                {
                    /* Expect the footer next */
                    strnfmt(expected_line, sizeof(expected_line), "%s end %s",
                        dump_seperator, mark);

                    between_marks = true;

                    /* There are some changes */
                    changed = true;
                }
                else
                {
                    /* Expect a header next - XXX shouldn't happen */
                    strnfmt(expected_line, sizeof(expected_line), "%s begin %s",
                        dump_seperator, mark);

                    between_marks = false;

                    /* Next line */
                    continue;
                }
            }
            /* Found a different line */
            else
            {
                /* Expected a footer and got something different? */
                if (between_marks)
                {
                    /* Abort */
                    changed = false;
                    break;
                }
            }
        }

        if (!between_marks)
        {
            /* Copy orginal line */
            SDL_IOprintf(tmp_fff, "%s\n", buf);
        }
    }

    /* Close files */
    sdl_fclose(orig_fff);
    sdl_fclose(tmp_fff);

    /* If there are changes, overwrite the original file with the new one */
    if (changed)
    {
        /* Copy contents of temporary file */
        tmp_fff = sdl_fopen(tmp_file, "r");
        orig_fff = sdl_fopen(orig_file, "w");

        while (!sdl_fgets(tmp_fff, buf, sizeof(buf)))
        {
            SDL_IOprintf(orig_fff, "%s\n", buf);
        }

        sdl_fclose(orig_fff);
        sdl_fclose(tmp_fff);
    }

    /* Kill the temporary file */
    fd_kill(tmp_file);
}

/*
 * Output the header of a pref-file dump
 */
static void pref_header(SDL_IOStream* fff, cptr mark)
{
    /* Start of dump */
    SDL_IOprintf(fff, "%s begin %s\n", dump_seperator, mark);

    SDL_IOprintf(fff, "# *Warning!*  The lines below are an automatic dump.\n");
    SDL_IOprintf(fff,
        "# Don't edit them; changes will be deleted and replaced "
        "automatically.\n");
}

/*
 * Output the footer of a pref-file dump
 */
static void pref_footer(SDL_IOStream* fff, cptr mark)
{
    SDL_IOprintf(fff, "# *Warning!*  The lines above are an automatic dump.\n");
    SDL_IOprintf(fff,
        "# Don't edit them; changes will be deleted and replaced "
        "automatically.\n");

    /* End of dump */
    SDL_IOprintf(fff, "%s end %s\n", dump_seperator, mark);
}

/*
 * Ask for a "user pref line" and process it
 */
void do_cmd_pref(void)
{
    char tmp[80];

    /* Default */
    SDL_strlcpy(tmp, "", sizeof(tmp));

    /* Ask for a "user pref command" */
    if (!settings_ui_prompt_string("Pref Command",
            "Enter a pref command to process.", "", tmp, sizeof(tmp)))
        return;

    /* Process that pref command */
    (void)process_pref_file_command(tmp);
}

/*
 * Ask for a "user pref file" and process it.
 *
 * This function should only be used by standard interaction commands,
 * in which a standard "Command:" prompt is present on the given row.
 *
 * Allow absolute file names?  XXX XXX XXX
 */
static void do_cmd_pref_file_hack(int row)
{
    char ftmp[80];

    (void)row;

    /* Default filename */
    strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

    /* Ask for a file (or cancel) */
    if (!settings_ui_prompt_string("Pref File",
            "Enter the pref file name to load.", "", ftmp, sizeof(ftmp)))
        return;

    /* Process the given filename */
    if (process_pref_file(ftmp))
    {
        /* Mention failure */
        msg_format("Failed to load '%s'!", ftmp);
    }
    else
    {
        /* Mention success */
        msg_format("Loaded '%s'.", ftmp);
    }
}

void clear_skills_and_abilities()
{
    int i, j;

    /* Clear the base values of the skills */
    for (i = 0; i < A_MAX; i++)
        p_ptr->skill_base[i] = 0;

    /* Clear the abilities */
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            p_ptr->innate_ability[i][j] = false;
            p_ptr->active_ability[i][j] = false;
        }
    }

    ability_log_reset();

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Set the redraw flag for everything */
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
}

/*
 * Interact with some options
 */
struct option_group_marker
{
    int before_index;
    cptr label;
};

static const struct option_group_marker interface_option_groups[] = {
    { 0, "Messages" },
    { 3, "Input" },
    { 7, "Look" },
    { 8, "Layout" },
    { 9, "Warnings" },
    { 10, "Debug" },
    { -1, NULL }
};

static const struct option_group_marker text_option_groups[] = {
    { 0, "Look and Lore" },
    { 3, "Inventory and Equipment" },
    { 7, "Character" },
    { -1, NULL }
};

static const struct option_group_marker gameplay_option_groups[] = {
    { 0, "Combat Behavior" },
    { 3, "Information" },
    { 6, "World Generation" },
    { -1, NULL }
};

static const struct option_group_marker efficiency_option_groups[] = {
    { 0, "Animation" },
    { 2, "Camera" },
    { -1, NULL }
};

static const struct option_group_marker visual_option_groups[] = {
    { 0, "Lists and Overlays" },
    { 3, "Map and Highlights" },
    { 12, "Narrative" },
    { 16, "Debug" },
    { -1, NULL }
};

static const struct option_group_marker challenge_option_groups[] = {
    { 0, "Traversal" },
    { 2, "Content" },
    { -1, NULL }
};

static const struct option_group_marker debug_option_groups[] = {
    { 0, "Generation" },
    { 4, "Knowledge" },
    { 10, "Survival" },
    { -1, NULL }
};

static const struct option_group_marker sound_option_groups[] = {
    { 0, "Effects" },
    { 5, "Effect Volume" },
    { 10, "Music" },
    { 12, "Music Volume" },
    { -1, NULL }
};

static const struct option_group_marker* get_option_groups_for_page(int page)
{
    switch (page)
    {
    case INTERFACE_PAGE: return interface_option_groups;
    case TEXT_PAGE: return text_option_groups;
    case GAMEPLAY_PAGE: return gameplay_option_groups;
    case EFFICIENCY_PAGE: return efficiency_option_groups;
    case VISUAL_PAGE: return visual_option_groups;
    case CHALLENGE_PAGE: return challenge_option_groups;
    case DEBUG_PAGE: return debug_option_groups;
    case SOUND_PAGE: return sound_option_groups;
    default: return NULL;
    }
}

static int option_group_count_before(const struct option_group_marker* groups,
    int option_index)
{
    int count = 0;

    if (!groups)
        return 0;

    for (int i = 0; groups[i].before_index >= 0; i++) {
        if (groups[i].before_index <= option_index)
            count++;
    }

    return count;
}

static int option_group_total_rows(const struct option_group_marker* groups)
{
    int count = 0;

    if (!groups)
        return 0;

    for (int i = 0; groups[i].before_index >= 0; i++)
        count++;

    return count;
}

static bool option_page_uses_app_config(int page)
{
    return (page == INTERFACE_PAGE) || (page == TEXT_PAGE)
        || (page == EFFICIENCY_PAGE) || (page == VISUAL_PAGE);
}

/*
 * Semantic SDL settings scenes should follow the named compact/normal SDL
 * configuration instead of branching off the live term dimensions.
 */
static bool settings_ui_named_sdl_compact_mode(void)
{
    return settings_sdl_min_terminal_mode() == 1;
}

static int settings_ui_named_sdl_cols(void)
{
    return settings_ui_named_sdl_compact_mode() ? 50 : 80;
}

static int settings_ui_named_sdl_rows(void)
{
    return settings_ui_named_sdl_compact_mode() ? 18 : 24;
}

static int settings_ui_named_sdl_line_width(int col)
{
    int width = settings_ui_named_sdl_cols() - col;

    if (width < 1)
        width = 1;

    return width;
}

static int settings_ui_named_sdl_visible_rows(int first_row, int footer_rows,
    int min_rows)
{
    int visible_rows = settings_ui_named_sdl_rows() - footer_rows - first_row;

    if (min_rows < 1)
        min_rows = 1;
    if (visible_rows < min_rows)
        visible_rows = min_rows;

    return visible_rows;
}

static void settings_ui_fit_text(char* buf, size_t buflen, cptr text,
    int max_chars)
{
    if (!buflen)
        return;

    if (!text)
        text = "";

    if (max_chars <= 0)
    {
        buf[0] = '\0';
        return;
    }

    if ((int)strlen(text) <= max_chars)
    {
        SDL_strlcpy(buf, text, buflen);
    }
    else if (max_chars <= 3)
    {
        strnfmt(buf, buflen, "%.*s", max_chars, text);
    }
    else
    {
        strnfmt(buf, buflen, "%.*s...", max_chars - 3, text);
    }
}

static cptr settings_ui_pick_label(int max_chars, cptr long_label,
    cptr medium_label, cptr short_label)
{
    cptr labels[3] = { long_label, medium_label, short_label };

    for (int i = 0; i < 3; i++)
    {
        if (labels[i] && labels[i][0] && (int)strlen(labels[i]) <= max_chars)
            return labels[i];
    }

    if (short_label && short_label[0])
        return short_label;
    if (medium_label && medium_label[0])
        return medium_label;
    if (long_label && long_label[0])
        return long_label;

    return "";
}

static void settings_ui_prompt_format_value(char* buf, size_t buflen,
    cptr value, bool show_cursor)
{
    size_t len;

    if (!buf || !buflen)
        return;

    if (!value)
        value = "";

    if (!value[0])
    {
        SDL_strlcpy(buf, show_cursor ? "_" : "(empty)", buflen);
        return;
    }

    SDL_strlcpy(buf, value, buflen);
    len = strlen(buf);
    if (show_cursor && len + 1 < buflen)
    {
        buf[len] = '_';
        buf[len + 1] = '\0';
    }
}

static bool settings_ui_present_text_prompt_scene(cptr title, cptr prompt,
    cptr value, cptr note)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    char value_buf[APP_UI_TEXT_MAX];

    panel = settings_browser_scene_begin_ex(&scene, title ? title : "",
        prompt ? prompt : "", 980, 1800);
    if (!panel)
        return false;

    if (note && note[0])
        (void)app_ui_panel_add_body_line(panel, TERM_SLATE, note);

    settings_ui_prompt_format_value(value_buf, sizeof(value_buf), value, true);
    (void)settings_browser_add_pair_row(panel, 0, TERM_L_BLUE, TERM_SLATE,
        true, true, "Value", value_buf);
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "Enter", "Accept");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Bksp", "Erase");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "Esc", "Cancel");

    return ui_information_scene_present_ui(&scene);
}

static bool settings_ui_present_text_value_scene(cptr title, cptr prompt,
    cptr note, cptr label, cptr value)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    char value_buf[APP_UI_TEXT_MAX];

    panel = settings_browser_scene_begin_ex(&scene, title ? title : "",
        prompt ? prompt : "", 980, 1800);
    if (!panel)
        return false;

    if (note && note[0])
        (void)app_ui_panel_add_body_line(panel, TERM_SLATE, note);

    settings_ui_prompt_format_value(value_buf, sizeof(value_buf), value, false);
    (void)settings_browser_add_pair_row(panel, 0, TERM_L_BLUE, TERM_SLATE,
        true, true, label ? label : "Value", value_buf);
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "Any key", "Close");

    return ui_information_scene_present_ui(&scene);
}

static bool settings_ui_prompt_string(cptr title, cptr prompt, cptr note,
    char* buf, size_t len)
{
    ui_information_scene_scope scope;
    bool local_scope = false;
    char work[1024];

    if (!buf || len == 0 || len > sizeof(work))
        return false;

    SDL_strlcpy(work, buf, sizeof(work));

    if (!ui_information_scene_is_active())
    {
        if (!ui_information_scene_enter(&scope))
            return false;
        local_scope = true;
    }

    while (true)
    {
        char ch;
        size_t used;

        if (!settings_ui_present_text_prompt_scene(title, prompt, work, note))
        {
            if (local_scope)
                ui_information_scene_leave(&scope);
            return false;
        }

        ch = (char)ui_information_scene_wait_key_nonrepeat();
        used = strlen(work);

        if (ch == ESCAPE)
        {
            if (local_scope)
                ui_information_scene_leave(&scope);
            return false;
        }

        if (ch == '\r' || ch == '\n')
        {
            SDL_strlcpy(buf, work, len);
            if (local_scope)
                ui_information_scene_leave(&scope);
            return true;
        }

        if (ch == '\b' || ch == 127)
        {
            if (used > 0)
                work[used - 1] = '\0';
            continue;
        }

        if ((unsigned char)ch == 21u)
        {
            work[0] = '\0';
            continue;
        }

        if (isprint((unsigned char)ch) && used + 1 < len)
        {
            work[used] = ch;
            work[used + 1] = '\0';
        }
    }
}

static void settings_ui_show_text_value(cptr title, cptr prompt, cptr note,
    cptr label, cptr value)
{
    ui_information_scene_scope scope;
    bool local_scope = false;

    if (!ui_information_scene_is_active())
    {
        if (!ui_information_scene_enter(&scope))
            return;
        local_scope = true;
    }

    if (settings_ui_present_text_value_scene(title, prompt, note, label, value))
        (void)ui_information_scene_wait_key_nonrepeat();

    if (local_scope)
        ui_information_scene_leave(&scope);
}

static bool settings_ui_capture_macro_input(cptr title, cptr prompt,
    cptr note, bool keymap, char* buf, size_t buflen, char* desc,
    size_t desclen)
{
    ui_information_scene_scope scope;
    bool local_scope = false;

    if (!buf || buflen == 0)
        return false;

    if (!ui_information_scene_is_active())
    {
        if (!ui_information_scene_enter(&scope))
            return false;
        local_scope = true;
    }

    if (!settings_ui_present_text_value_scene(title, prompt, note, "Input",
            "Press a key sequence"))
    {
        if (local_scope)
            ui_information_scene_leave(&scope);
        return false;
    }

    if (keymap)
        do_cmd_macro_aux_keymap(buf);
    else
        do_cmd_macro_aux(buf);

    if (desc && desclen > 0)
        ascii_to_text(desc, desclen, buf);

    if (local_scope)
        ui_information_scene_leave(&scope);

    return true;
}

static void settings_ui_format_field(char* buf, size_t buflen, cptr text,
    bool selected)
{
    if (!buf || !buflen)
        return;

    if (!text)
        text = "";

    if (selected)
        strnfmt(buf, buflen, "[%s]", text);
    else
        SDL_strlcpy(buf, text, buflen);
}

static void settings_ui_format_auto_value(char* buf, size_t buflen, int value,
    int max_chars)
{
    char raw_buf[16];
    char auto_long[16];
    char auto_short[8];

    if (!buf || !buflen)
        return;

    if (value > 0)
    {
        strnfmt(raw_buf, sizeof(raw_buf), "%d", value);
        settings_ui_fit_text(buf, buflen, raw_buf, max_chars);
        return;
    }

    SDL_strlcpy(auto_long, "auto", sizeof(auto_long));
    SDL_strlcpy(auto_short, "a", sizeof(auto_short));
    settings_ui_fit_text(buf, buflen,
        settings_ui_pick_label(max_chars, auto_long, auto_long, auto_short),
        max_chars);
}

typedef struct settings_choice_entry {
    int id;
    char hotkey;
    cptr label;
    bool disabled;
} settings_choice_entry;

static app_ui_panel* settings_browser_scene_begin_ex(app_ui_scene* scene,
    cptr title, cptr subtitle, int min_width_px, int width_cap_px)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, (u16b)min_width_px, (u16b)width_cap_px);
    if (title && title[0])
        app_ui_panel_set_title(panel, TERM_L_WHITE, title);
    if (subtitle && subtitle[0])
        app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

    return panel;
}

static app_ui_panel* settings_browser_scene_begin(app_ui_scene* scene,
    cptr title, cptr subtitle)
{
    return settings_browser_scene_begin_ex(scene, title, subtitle, 980, 2048);
}

static bool settings_browser_add_label_row(app_ui_panel* panel, s16b id,
    byte attr, bool enabled, bool selected, cptr label)
{
    return app_ui_panel_add_row_ex(panel, id, attr, attr, 0, '\0', enabled,
        selected, "", label ? label : "", "");
}

static bool settings_browser_add_pair_row(app_ui_panel* panel, s16b id,
    byte attr, byte meta_attr, bool enabled, bool selected, cptr label,
    cptr meta)
{
    return app_ui_panel_add_row_ex(panel, id, attr, meta_attr, 0, '\0',
        enabled, selected, "", label ? label : "", meta ? meta : "");
}

static bool settings_browser_add_section_row(app_ui_panel* panel, cptr label)
{
    app_ui_row* row;

    if (!panel || !label || !label[0])
        return true;

    if (!app_ui_panel_add_row_ex(panel, (s16b)(-1000 - panel->row_count),
            TERM_SLATE, TERM_SLATE, 0, '\0', false, false, "", label, ""))
    {
        return false;
    }

    row = &panel->rows[panel->row_count - 1];
    row->flags |= APP_UI_ITEM_FLAG_SECTION;
    return true;
}

static int settings_choice_find_index_by_id(
    const settings_choice_entry* entries, int entry_count, int id)
{
    int i;

    for (i = 0; i < entry_count; i++)
    {
        if (entries[i].id == id)
            return i;
    }

    return -1;
}

static int settings_choice_find_index_by_hotkey(
    const settings_choice_entry* entries, int entry_count, int ch)
{
    char needle = (char)tolower((unsigned char)ch);
    int i;

    for (i = 0; i < entry_count; i++)
    {
        if ((char)tolower((unsigned char)entries[i].hotkey) == needle)
            return i;
    }

    return -1;
}

static int settings_choice_next_enabled(const settings_choice_entry* entries,
    int entry_count, int start, int step)
{
    int i;
    int index;

    if (!entries || entry_count <= 0)
        return -1;

    index = start % entry_count;
    if (index < 0)
        index += entry_count;

    for (i = 0; i < entry_count; i++)
    {
        if (!entries[index].disabled)
            return index;

        index = (index + step) % entry_count;
        if (index < 0)
            index += entry_count;
    }

    return -1;
}

static bool settings_choice_present_ui_scene(cptr title,
    const settings_choice_entry* entries, int entry_count, int selected)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    char subtitle[64];

    panel = settings_browser_scene_begin(&scene, title ? title : "", "");
    if (!panel)
        return false;

    strnfmt(subtitle, sizeof(subtitle), "%s %s", VERSION_NAME,
        VERSION_STRING);
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);
    if (selected > 4)
        app_ui_panel_set_row_offset(panel, (s16b)(selected - 4));

    for (int i = 0; i < entry_count; i++)
    {
        byte attr = entries[i].disabled ? TERM_L_DARK
            : ((i == selected) ? TERM_L_BLUE : TERM_WHITE);

        if (!settings_browser_add_label_row(panel, (s16b)entries[i].id, attr,
                !entries[i].disabled, i == selected, entries[i].label))
        {
            return false;
        }
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Enter", "Select");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "Esc", "Back");

    return ui_information_scene_present_ui(&scene);
}

static int settings_choice_menu(cptr title,
    const settings_choice_entry* entries, int entry_count, int* highlight,
    int cancel_id)
{
    int selected;
    int hotkey_index;
    int ch;

    if (!entries || entry_count <= 0 || !highlight)
        return 0;

    selected = settings_choice_find_index_by_id(entries, entry_count,
        *highlight);
    if (selected < 0)
        selected = settings_choice_next_enabled(entries, entry_count, 0, 1);
    else if (entries[selected].disabled)
        selected = settings_choice_next_enabled(entries, entry_count,
            selected + 1, 1);

    if (selected < 0)
        return 0;

    *highlight = entries[selected].id;

    if (!settings_choice_present_ui_scene(title, entries, entry_count,
            selected))
    {
        return cancel_id;
    }

    inkey_set_cursor_hidden(true);
    ch = settings_ui_read_legacy_key(false);
    inkey_set_cursor_hidden(false);

    hotkey_index = settings_choice_find_index_by_hotkey(entries, entry_count,
        ch);
    if (hotkey_index >= 0)
    {
        if (entries[hotkey_index].disabled)
        {
            msg_print("You can no longer take that action.");
            return 0;
        }

        *highlight = entries[hotkey_index].id;
        return entries[hotkey_index].id;
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == 'Q'))
    {
        *highlight = cancel_id;
        return cancel_id;
    }

    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (entries[selected].disabled)
        {
            msg_print("You can no longer take that action.");
            return 0;
        }

        return entries[selected].id;
    }

    if (ch == '8')
    {
        selected = settings_choice_next_enabled(entries, entry_count,
            selected - 1, -1);
        if (selected >= 0)
            *highlight = entries[selected].id;
    }

    if (ch == '2')
    {
        selected = settings_choice_next_enabled(entries, entry_count,
            selected + 1, 1);
        if (selected >= 0)
            *highlight = entries[selected].id;
    }

    return 0;
}

static bool option_menu_use_compact_layout(void)
{
    return settings_ui_named_sdl_cols() <= 60;
}

static bool option_menu_use_narrow_layout(void)
{
    return settings_ui_named_sdl_cols() <= 50;
}

static int option_menu_max_line_chars(void)
{
    int wid = settings_ui_named_sdl_cols();

    /* Options start at column 4; keep one cell free for the cursor. */
    wid -= 5;

    if (wid < 8)
        wid = 8;

    return wid;
}

static void option_menu_fit_text(char* buf, size_t buflen, cptr text,
    int max_chars)
{
    settings_ui_fit_text(buf, buflen, text, max_chars);
}

static cptr sound_option_label(int index)
{
    bool compact = option_menu_use_compact_layout();
    bool narrow = option_menu_use_narrow_layout();

    if (compact)
    {
        switch (index)
        {
        case 0: return narrow ? "Sounds" : "Game sounds";
        case 1: return narrow ? "Combat sfx" : "Combat sounds";
        case 2: return narrow ? "Inv sfx" : "Inventory sounds";
        case 3: return narrow ? "Walk sfx" : "Walk sounds";
        case 4: return narrow ? "Door sfx" : "Door sounds";
        case 5: return narrow ? "Combat vol" : "Combat volume";
        case 6: return narrow ? "Inv vol" : "Inventory volume";
        case 7: return narrow ? "Walk vol" : "Walk volume";
        case 8: return narrow ? "Door vol" : "Door volume";
        case 9: return narrow ? "Other vol" : "Other volume";
        case 10: return "Menu music";
        case 11: return "Ambient music";
        case 12: return narrow ? "Menu vol" : "Menu music volume";
        case 13: return narrow ? "Ambient vol" : "Ambient music volume";
        default: return "(unknown sound option)";
        }
    }

    switch (index)
    {
    case 0: return "Enable game sounds";
    case 1: return "Enable combat sounds";
    case 2: return "Enable inventory sounds";
    case 3: return "Enable walk sounds";
    case 4: return "Enable door sounds";
    case 5: return "Combat sounds volume";
    case 6: return "Inventory sounds volume";
    case 7: return "Walk sounds volume";
    case 8: return "Door sounds volume";
    case 9: return "Other sounds volume";
    case 10: return "Enable main menu music";
    case 11: return "Enable ambient dungeon music";
    case 12: return "Main menu music volume";
    case 13: return "Ambient music volume";
    default: return "(unknown sound option)";
    }
}

static cptr option_menu_label(int opt)
{
    bool compact = option_menu_use_compact_layout();
    bool narrow = option_menu_use_narrow_layout();

    switch (opt)
    {
    case OPT_delay_factor:
        return compact ? (narrow ? "Anim delay" : "Animation delay")
                       : "Delay factor for animation (0 to 9)";
    case OPT_hitpoint_warning:
        return compact ? (narrow ? "HP warn" : "HP warning")
                       : "Hitpoint warning threshold (0% to 90%)";
    case OPT_main_combat_rolls:
        return compact ? (narrow ? "Combat lines" : "Combat roll lines")
                       : "Main terminal combat roll lines (0=off, 1-4=lines)";
    case OPT_hide_left_panel:
        return compact ? (narrow ? "Compact panel" : "Compact left panel")
                       : "Hide Left Panel [Alt+P]";
    case OPT_show_level_entry_banner:
        return compact ? (narrow ? "Entry text" : "Entry narrative")
                       : "Level entry narrative";
    case OPT_show_partition_narrative:
        return compact ? (narrow ? "Partition text" : "Partition narrative")
                       : "Partition transition narrative";
    case OPT_ability_desc_mode:
        return compact ? (narrow ? "Ability text" : "Ability descriptions")
                       : "Ability descriptions (0=lore+effect, 1=effect+lore, 2=effect)";
    case OPT_vault_drop_frequency:
        return compact ? "Vault drops" : "Vault drop frequency";
    case OPT_noble_item_spawn_mode:
        return compact ? (narrow ? "Noble items" : "Noble item sources")
                       : "Noble item spawns";
    case OPT_look_objects_sort_by_difficulty:
        return compact ? (narrow ? "Look diff sort" : "Look sort by diff")
                       : "Sort look (L) objects by difficulty only";
    case OPT_look_nearby_filter_default:
        return compact ? (narrow ? "Look near def" : "Look nearby default")
                       : "Default look (l) nearby filter";
    case OPT_intro_style:
        return compact ? (narrow ? "Welcome art" : "Welcome screen")
                       : "Welcome screen style";
    case OPT_banner_message_stairs:
        return compact ? "Banner layout" : "Banner message layout";
    case OPT_unlock_blitz_mode:
        return compact ? (narrow ? "Blitz unlocked" : "Unlock Blitz Mode")
                       : "Unlock Blitz Mode";
    default:
        break;
    }

    if (compact)
    {
        switch (opt)
        {
        case OPT_system_beep: return narrow ? "Beep" : "Error beep";
        case OPT_quick_messages: return narrow ? "Quick prompts" : "Quick prompts";
        case OPT_auto_more: return narrow ? "Auto more" : "Auto -more-";
        case OPT_easy_main_menu: return narrow ? "Esc menu" : "Esc main menu";
        case OPT_hjkl_movement: return narrow ? "hjkl move" : "hjkl movement";
        case OPT_angband_keyset: return narrow ? "Angband keys" : "Angband keyset";
        case OPT_space_acts_as_comma: return narrow ? "Space = comma" : "Space acts as comma";
        case OPT_story_lists: return narrow ? "Story look" : "Story font: look/target";
        case OPT_story_lists_inven: return narrow ? "Story inv" : "Story font: inv menu";
        case OPT_story_lists_equip: return narrow ? "Story equip" : "Story font: equip menu";
        case OPT_story_character_sheet: return narrow ? "Story sheet" : "Story font: char sheet";
        case OPT_story_lists_inven_pane: return narrow ? "Story inv pane" : "Story font: inv pane";
        case OPT_story_lists_equip_pane: return narrow ? "Story eq pane" : "Story font: equip pane";
        case OPT_story_monster_desc: return narrow ? "Story mon desc" : "Story font: monster desc";
        case OPT_story_monster_desc_pane: return narrow ? "Story mon pane" : "Story font: monster pane";
        case OPT_valorous_oath_auto_attack_safety: return narrow ? "Valorous safety" : "Valorous oath safety";
        case OPT_forgo_attacking_unwary: return narrow ? "Skip unwary hits" : "Forgo unwary attacks";
        case OPT_assassination_over_charge: return narrow ? "Stealth over charge" : "Assassination over Charge";
        case OPT_stop_singing_on_rest: return narrow ? "Stop song on rest" : "Stop singing on rest";
        case OPT_know_monster_info: return narrow ? "Know monsters" : "Know monster info";
        case OPT_visual_recognition: return narrow ? "Need light to spot" : "Need light to spot";
        case OPT_disable_skeleton_note_tutorial: return narrow ? "Hide skeleton tips" : "Hide skeleton tutorials";
        case OPT_smaller_level_size: return narrow ? "Smaller levels" : "Smaller level size";
        case OPT_more_stairs: return narrow ? "More stairs" : "Extra stairs";
        case OPT_instant_run: return narrow ? "Fast running" : "Faster running";
        case OPT_center_player: return narrow ? "Center map" : "Center map";
        case OPT_run_avoid_center: return narrow ? "No center on run" : "Avoid centering on run";
        case OPT_auto_display_lists: return narrow ? "Auto lists" : "Auto display lists";
        case OPT_artifact_unique_color: return narrow ? "Yellow artefacts" : "Yellow unique artefacts";
        case OPT_hilite_player: return narrow ? "Cursor on player" : "Highlight player";
        case OPT_hilite_target: return narrow ? "Cursor on target" : "Highlight target";
        case OPT_hilite_unwary: return narrow ? "Mark unwary" : "Highlight unwary";
        case OPT_solid_walls: return narrow ? "Solid walls" : "Solid walls";
        case OPT_hybrid_walls: return narrow ? "Hybrid walls" : "Hybrid walls";
        case OPT_unidentified_items_slate: return narrow ? "Slate unknown items" : "Slate unidentified items";
        case OPT_stealth_vision: return narrow ? "Stealth vision" : "Stealth vision";
        case OPT_sleep_icon: return narrow ? "Sleep icon" : "Sleep icon";
        case OPT_show_smithing_difficulty: return narrow ? "Smith dbg items" : "Debug smithing in items";
        case OPT_show_smithing_difficulty_look: return narrow ? "Smith dbg look" : "Debug smithing in look";
        case OPT_look_nearby_filter_default: return narrow ? "Look near def" : "Look nearby default";
        case OPT_show_level_generation_debug: return narrow ? "Dbg lvl screen" : "Debug level screen";
        case OPT_birth_discon_stair: return narrow ? "Disc. stairs" : "Disconnected stairs";
        case OPT_birth_ironman: return narrow ? "Straight down" : "Straight down";
        case OPT_birth_no_artefacts: return narrow ? "No artefacts" : "No artefacts";
        case OPT_birth_fixed_exp: return narrow ? "Fixed XP" : "Fixed experience";
        case OPT_cheat_peek: return narrow ? "Debug obj gen" : "Debug object gen";
        case OPT_cheat_hear: return narrow ? "Debug mon gen" : "Debug monster gen";
        case OPT_cheat_room: return narrow ? "Debug room gen" : "Debug dungeon gen";
        case OPT_cheat_xtra: return narrow ? "Debug extra" : "Debug extra";
        case OPT_cheat_know: return narrow ? "Debug know mons" : "Debug know monsters";
        case OPT_cheat_monsters: return narrow ? "Debug show mons" : "Debug show monsters";
        case OPT_cheat_noise: return narrow ? "Debug noise" : "Debug noise";
        case OPT_cheat_scent: return narrow ? "Debug scent" : "Debug scent";
        case OPT_cheat_light: return narrow ? "Debug light" : "Debug light";
        case OPT_cheat_skill_rolls: return narrow ? "Debug skill rolls" : "Debug skill rolls";
        case OPT_cheat_live: return narrow ? "Debug no death" : "Debug avoid death";
        case OPT_cheat_timestop: return narrow ? "Debug time stop" : "Debug time stop";
        default:
            break;
        }
    }

    if (option_desc[opt])
        return option_desc[opt];
    if (option_text[opt])
        return option_text[opt];
    return "(unknown option)";
}

static void option_menu_format_line(char* buf, size_t buflen, cptr label,
    cptr value)
{
    if (!option_menu_use_compact_layout())
    {
        strnfmt(buf, buflen, "%-48s: %s", label, value);
    }
    else
    {
        char label_buf[96];
        char value_buf[48];
        int max_chars = option_menu_max_line_chars();
        int value_len;
        int label_budget;

        option_menu_fit_text(value_buf, sizeof(value_buf), value, max_chars);
        value_len = (int)strlen(value_buf);

        if (value_len <= 0)
        {
            option_menu_fit_text(buf, buflen, label, max_chars);
            return;
        }

        label_budget = max_chars - value_len - 2;
        if (label_budget <= 0)
        {
            option_menu_fit_text(buf, buflen, value_buf, max_chars);
            return;
        }

        option_menu_fit_text(label_buf, sizeof(label_buf), label, label_budget);
        strnfmt(buf, buflen, "%s: %s", label_buf, value_buf);
    }
}

static void option_menu_describe_line(bool is_sound_page, const int opt[],
    int index, const struct sound_config* sound_cfg, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (is_sound_page)
    {
        char value_str[32];

        value_str[0] = '\0';
        if (index == 0)
        {
            strnfmt(value_str, sizeof(value_str), "%s",
                sound_cfg->enabled ? "yes" : "no ");
        }
        else if (index == 1)
        {
            strnfmt(value_str, sizeof(value_str), "%s",
                sound_cfg->enable_combat ? "yes" : "no ");
        }
        else if (index == 2)
        {
            strnfmt(value_str, sizeof(value_str), "%s",
                sound_cfg->enable_inventory ? "yes" : "no ");
        }
        else if (index == 3)
        {
            strnfmt(value_str, sizeof(value_str), "%s",
                sound_cfg->enable_walk ? "yes" : "no ");
        }
        else if (index == 4)
        {
            strnfmt(value_str, sizeof(value_str), "%s",
                sound_cfg->enable_doors ? "yes" : "no ");
        }
        else if (index == 5)
        {
            strnfmt(value_str, sizeof(value_str), "%.0f%%",
                sound_cfg->volume_combat * 100.0f);
        }
        else if (index == 6)
        {
            strnfmt(value_str, sizeof(value_str), "%.0f%%",
                sound_cfg->volume_inventory * 100.0f);
        }
        else if (index == 7)
        {
            strnfmt(value_str, sizeof(value_str), "%.0f%%",
                sound_cfg->volume_walk * 100.0f);
        }
        else if (index == 8)
        {
            strnfmt(value_str, sizeof(value_str), "%.0f%%",
                sound_cfg->volume_doors * 100.0f);
        }
        else if (index == 9)
        {
            strnfmt(value_str, sizeof(value_str), "%.0f%%",
                sound_cfg->volume_other * 100.0f);
        }
        else if (index == 10)
        {
            strnfmt(value_str, sizeof(value_str), "%s",
                sound_cfg->music_main_enabled ? "yes" : "no ");
        }
        else if (index == 11)
        {
            strnfmt(value_str, sizeof(value_str), "%s",
                sound_cfg->music_ambient_enabled ? "yes" : "no ");
        }
        else if (index == 12)
        {
            strnfmt(value_str, sizeof(value_str), "%.0f%%",
                sound_cfg->music_main_volume * 100.0f);
        }
        else if (index == 13)
        {
            strnfmt(value_str, sizeof(value_str), "%.0f%%",
                sound_cfg->music_ambient_volume * 100.0f);
        }

        option_menu_format_line(buf, buflen, sound_option_label(index),
            value_str);
        return;
    }

    if (opt[index] == OPT_delay_factor)
    {
        char value_str[32];

        strnfmt(value_str, sizeof(value_str), "%d", op_ptr->delay_factor);
        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            value_str);
    }
    else if (opt[index] == OPT_hitpoint_warning)
    {
        char value_str[32];

        strnfmt(value_str, sizeof(value_str), "%d%%",
            op_ptr->hitpoint_warn * 10);
        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            value_str);
    }
    else if (opt[index] == OPT_hide_left_panel)
    {
        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            settings_sdl_hide_left_panel() ? "yes" : "no ");
    }
    else if (opt[index] == OPT_main_combat_rolls)
    {
        char value_str[32];

        strnfmt(value_str, sizeof(value_str), "%d",
            op_ptr->main_combat_rolls);
        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            value_str);
    }
    else if (opt[index] == OPT_show_level_entry_banner)
    {
        const char* mode_str;
        bool compact = option_menu_use_compact_layout();

        switch (op_ptr->level_entry_narrative_mode)
        {
        case LEVEL_ENTRY_NARRATIVE_BANNER:
            mode_str = compact ? "Banner" : "Banner without delay";
            break;
        case LEVEL_ENTRY_NARRATIVE_MESSAGE: mode_str = "Message"; break;
        case LEVEL_ENTRY_NARRATIVE_OFF:     mode_str = "Off"; break;
        default:
            mode_str = compact ? "Banner delay" : "Banner with delay";
            break;
        }
        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            mode_str);
    }
    else if (opt[index] == OPT_show_partition_narrative)
    {
        const char* mode_str;
        bool compact = option_menu_use_compact_layout();

        switch (op_ptr->partition_narrative_mode)
        {
        case PARTITION_NARRATIVE_BANNER:
            mode_str = compact ? "Banner" : "Banner without delay";
            break;
        case PARTITION_NARRATIVE_OFF: mode_str = "Off"; break;
        default: mode_str = "Message"; break;
        }
        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            mode_str);
    }
    else if (opt[index] == OPT_ability_desc_mode)
    {
        const char* mode_str;
        bool compact = option_menu_use_compact_layout();

        switch (op_ptr->ability_desc_mode)
        {
        case 1:
            mode_str = compact ? "1 effect+lore" : "1 (effect+lore)";
            break;
        case 2:
            mode_str = compact ? "2 effect only" : "2 (effect only)";
            break;
        default:
            mode_str = compact ? "0 lore+effect" : "0 (lore+effect)";
            break;
        }
        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            mode_str);
    }
    else if (opt[index] == OPT_vault_drop_frequency)
    {
        const char* vdf_names[] = {
            "Normal", "Modest", "Scarce", "Meager", "Plentiful"
        };
        char value_str[32];
        byte mode = op_ptr->vault_drop_frequency;

        if (mode > VDF_PLENTIFUL)
            mode = VDF_NORMAL;
        strnfmt(value_str, sizeof(value_str), "%s (%d)", vdf_names[mode],
            mode);
        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            value_str);
    }
    else if (opt[index] == OPT_noble_item_spawn_mode)
    {
        const char* mode_str
            = (op_ptr->noble_item_spawn_mode
                == NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
            ? (option_menu_use_compact_layout()
                ? "1 with vaults"
                : "1 (also &/! vault drops)")
            : (option_menu_use_compact_layout()
                ? "0 restricted"
                : "0 (good+/chests/human+elf skeletons)");

        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            mode_str);
    }
    else if (opt[index] == OPT_intro_style)
    {
        const char* is_names[] = {
            "Flame Imperishable", "Oath of Feanor",
            "Twilight of Valinor", "Song of Luthien",
            "Words of Hurin", "Starlight on Cuivienen",
            "Lament of the Noldor", "Random"
        };
        byte mode = op_ptr->intro_style;

        if (mode > INTRO_STYLE_RANDOM)
            mode = INTRO_STYLE_FLAME;
        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            is_names[mode]);
    }
    else if (opt[index] == OPT_banner_message_stairs)
    {
        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            op_ptr->opt[opt[index]] ? "Stair" : "Straight");
    }
    else
    {
        option_menu_format_line(buf, buflen, option_menu_label(opt[index]),
            op_ptr->opt[opt[index]] ? "yes" : "no ");
    }
}

static bool option_menu_present_ui_scene(int page, cptr info, int n,
    const int opt[], int k, int scroll,
    const struct option_group_marker* groups,
    const struct sound_config* sound_cfg)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    char subtitle[64];
    int group_index = 0;
    bool is_sound_page = (page == SOUND_PAGE);
    bool locked = (page == CHALLENGE_PAGE) && (playerturn != 0);

    panel = settings_browser_scene_begin(&scene, info ? info : "", "");
    if (!panel)
        return false;

    strnfmt(subtitle, sizeof(subtitle), "%d setting%s", n, (n == 1) ? "" : "s");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);
    if (scroll > 0)
        app_ui_panel_set_row_offset(panel, (s16b)scroll);

    for (int i = 0; i < n; i++)
    {
        char buf[160];
        byte attr = (i == k) ? TERM_L_BLUE : TERM_WHITE;

        while (groups && groups[group_index].before_index == i)
        {
            if (!settings_browser_add_section_row(panel,
                    groups[group_index].label))
            {
                return false;
            }
            group_index++;
        }

        option_menu_describe_line(is_sound_page, opt, i, sound_cfg, buf,
            sizeof(buf));
        if (!settings_browser_add_label_row(panel, (s16b)i, attr, !locked,
                i == k, buf))
        {
            return false;
        }
    }

    if (page == CHALLENGE_PAGE)
    {
        (void)app_ui_panel_add_body_line(panel, TERM_L_WHITE,
            "Challenge options only change during character creation.");
        (void)app_ui_panel_add_body_line(panel, TERM_L_WHITE,
            "They can also change on the very first turn.");
        if (locked)
        {
            (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
                "Press Enter or Esc to return.");
        }
    }

    if (!locked)
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "4/6", "Set");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "Space", "Toggle");
    }
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "Esc", "Back");

    return ui_information_scene_present_ui(&scene);
}

static void option_apply_side_effects(int opt)
{
    if (opt == OPT_story_lists_inven_pane || opt == OPT_story_lists_equip_pane)
        redraw_inven_equip_subwindows();
    if (opt == OPT_story_monster_desc_pane)
        redraw_monster_subwindows();
    if (opt == OPT_stealth_vision || opt == OPT_visual_recognition
        || opt == OPT_sleep_icon)
        p_ptr->redraw |= (PR_MAP);
}

extern void do_cmd_options_aux(int page, cptr info)
{
    char ch;

    int i, k = 0, n = 0;
    int scroll = 0;

    int opt[OPT_PAGE_PER];

    int dir;
    
    bool is_sound_page = (page == SOUND_PAGE);
    bool app_page = option_page_uses_app_config(page);
    bool metarun_page = !app_page && !is_sound_page;
    bool app_settings_dirty = false;
    bool metarun_settings_dirty = false;
    bool sound_settings_dirty = false;
    const struct option_group_marker* groups = get_option_groups_for_page(page);
    struct sound_config* sound_cfg = sdl_sound_get_config();

    /* Scan the options */
    for (i = 0; i < OPT_PAGE_PER; i++)
    {
        /* Collect options on this "page" */
        if (option_page[page][i] != OPT_NONE)
        {
            opt[n++] = option_page[page][i];
        }
    }
    
    /* Special case: Sound page uses custom display instead of standard options */
    if (is_sound_page)
    {
        n = 14; /* 5 enable flags + 5 volume controls + 2 music enable + 2 music volume */
    }

    /* Interact with the player */
    while (true)
    {
        int first_row = 3;
        int footer_rows = (page == CHALLENGE_PAGE) ? 4 : 2;
        int visible_rows = settings_ui_named_sdl_visible_rows(first_row,
            footer_rows, 1);
        int total_rows = n + option_group_total_rows(groups);
        int selected_display_row = k + option_group_count_before(groups, k);
        int max_scroll;

        max_scroll = total_rows - visible_rows;
        if (max_scroll < 0)
            max_scroll = 0;

        if (selected_display_row < scroll)
            scroll = selected_display_row;
        else if (selected_display_row >= scroll + visible_rows)
            scroll = selected_display_row - visible_rows + 1;
        if (scroll > max_scroll)
            scroll = max_scroll;

        if (!option_menu_present_ui_scene(page, info, n, opt, k, scroll,
                groups, sound_cfg))
        {
            return;
        }

        /* Get a key */
        inkey_set_cursor_hidden(true);
        ch = settings_ui_read_legacy_key(false);
        inkey_set_cursor_hidden(false);

        /*
         * HACK - Try to translate the key into a direction
         * to allow using the roguelike keys for navigation.
         */
        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        /* Analyze */
        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
        {
            /* Hack -- Notice use of any "cheat" options */
            for (i = OPT_CHEAT; i < OPT_ADULT; i++)
            {
                if (op_ptr->opt[i])
                {
                    /* Set score option */
                    if (!op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)])
                        metarun_settings_dirty = true;
                    op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = true;
                }
            }

            if (sound_settings_dirty)
            {
                sdl_sound_save_config();
                sdl_sound_reload();
            }

            if (app_settings_dirty)
                save_pane_config_to_json();

            if (metarun_settings_dirty)
                metarun_save_persistent_settings();

            return;
        }

        case '-':
        case '8':
        {
            k = (n + k - 1) % n;
            break;
        }

        case '2':
        {
            k = (k + 1) % n;
            break;
        }

        case 't':
        case '5':
        case ' ':
        {
            if ((page != CHALLENGE_PAGE) || (playerturn == 0))
            {
                if (is_sound_page)
                {
                    if (k == 0)
                    {
                        sound_cfg->enabled = !sound_cfg->enabled;
                        use_sound = sound_cfg->enabled;
                    }
                    else if (k == 1) sound_cfg->enable_combat = !sound_cfg->enable_combat;
                    else if (k == 2) sound_cfg->enable_inventory = !sound_cfg->enable_inventory;
                    else if (k == 3) sound_cfg->enable_walk = !sound_cfg->enable_walk;
                    else if (k == 4) sound_cfg->enable_doors = !sound_cfg->enable_doors;
                    else if (k == 10) sound_cfg->music_main_enabled = !sound_cfg->music_main_enabled;
                    else if (k == 11) sound_cfg->music_ambient_enabled = !sound_cfg->music_ambient_enabled;
                    /* Volume controls (5-9, 12-13) don't toggle */
                }
                else if (opt[k] == OPT_delay_factor)
                {
                    op_ptr->delay_factor = (op_ptr->delay_factor < 9)
                        ? op_ptr->delay_factor + 1
                        : 0;
                }
                else if (opt[k] == OPT_hitpoint_warning)
                {
                    op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn < 9)
                        ? op_ptr->hitpoint_warn + 1
                        : 0;
                }
                else if (opt[k] == OPT_hide_left_panel)
                {
                    settings_sdl_set_hide_left_panel(
                        !settings_sdl_hide_left_panel());
                    sdl_apply_config();
                }
                else if (opt[k] == OPT_main_combat_rolls)
                {
                    op_ptr->main_combat_rolls = (op_ptr->main_combat_rolls < 4)
                        ? op_ptr->main_combat_rolls + 1
                        : 0;

                    clear_main_combat_rolls_area();
                    display_main_combat_rolls();
                    p_ptr->redraw |= (PR_MAP);
                }
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode < LEVEL_ENTRY_NARRATIVE_OFF)
                        ? op_ptr->level_entry_narrative_mode + 1
                        : LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode < PARTITION_NARRATIVE_OFF)
                        ? op_ptr->partition_narrative_mode + 1
                        : PARTITION_NARRATIVE_BANNER;
                }
                else if (opt[k] == OPT_intro_style)
                {
                    /* Toggle cycles forward */
                    op_ptr->intro_style
                        = (op_ptr->intro_style < INTRO_STYLE_RANDOM)
                        ? op_ptr->intro_style + 1
                        : INTRO_STYLE_FLAME;
                }
                else if (opt[k] == OPT_ability_desc_mode)
                {
                    op_ptr->ability_desc_mode = (op_ptr->ability_desc_mode < 2)
                        ? op_ptr->ability_desc_mode + 1
                        : 0;
                }
                else if (opt[k] == OPT_vault_drop_frequency)
                {
                    op_ptr->vault_drop_frequency
                        = (op_ptr->vault_drop_frequency < VDF_PLENTIFUL)
                        ? op_ptr->vault_drop_frequency + 1
                        : VDF_NORMAL;
                }
                else if (opt[k] == OPT_noble_item_spawn_mode)
                {
                    op_ptr->noble_item_spawn_mode
                        = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_RESTRICTED)
                        ? NOBLE_ITEM_SPAWN_INCLUDE_VAULTS
                        : NOBLE_ITEM_SPAWN_RESTRICTED;
                }
                else
                {
                    op_ptr->opt[opt[k]] = !op_ptr->opt[opt[k]];
                    option_apply_side_effects(opt[k]);
                }

                if (is_sound_page)
                    sound_settings_dirty = true;
                else if (option_is_app_persistent(opt[k]))
                    app_settings_dirty = true;
                else if (metarun_page)
                    metarun_settings_dirty = true;
            }
            break;
        }

        case 'y':
        case '6':
        {
            if ((page != CHALLENGE_PAGE) || (playerturn == 0))
            {
                if (is_sound_page)
                {
                    if (k == 0)
                    {
                        sound_cfg->enabled = true;
                        use_sound = true;
                    }
                    else if (k == 1) sound_cfg->enable_combat = true;
                    else if (k == 2) sound_cfg->enable_inventory = true;
                    else if (k == 3) sound_cfg->enable_walk = true;
                    else if (k == 4) sound_cfg->enable_doors = true;
                    else if (k == 5) sound_cfg->volume_combat = (sound_cfg->volume_combat < 1.0f) ? sound_cfg->volume_combat + 0.1f : 1.0f;
                    else if (k == 6) sound_cfg->volume_inventory = (sound_cfg->volume_inventory < 1.0f) ? sound_cfg->volume_inventory + 0.1f : 1.0f;
                    else if (k == 7) sound_cfg->volume_walk = (sound_cfg->volume_walk < 1.0f) ? sound_cfg->volume_walk + 0.1f : 1.0f;
                    else if (k == 8) sound_cfg->volume_doors = (sound_cfg->volume_doors < 1.0f) ? sound_cfg->volume_doors + 0.1f : 1.0f;
                    else if (k == 9) sound_cfg->volume_other = (sound_cfg->volume_other < 1.0f) ? sound_cfg->volume_other + 0.1f : 1.0f;
                    else if (k == 10) sound_cfg->music_main_enabled = true;
                    else if (k == 11) sound_cfg->music_ambient_enabled = true;
                    else if (k == 12) {
                        sound_cfg->music_main_volume = (sound_cfg->music_main_volume < 1.0f) ? sound_cfg->music_main_volume + 0.1f : 1.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                    else if (k == 13) {
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume < 1.0f) ? sound_cfg->music_ambient_volume + 0.1f : 1.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                }
                else if (opt[k] == OPT_delay_factor)
                {
                    op_ptr->delay_factor = (op_ptr->delay_factor < 9)
                        ? op_ptr->delay_factor + 1
                        : 9;
                }
                else if (opt[k] == OPT_hitpoint_warning)
                {
                    op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn < 9)
                        ? op_ptr->hitpoint_warn + 1
                        : 9;
                }
                else if (opt[k] == OPT_hide_left_panel)
                {
                    settings_sdl_set_hide_left_panel(true);
                    sdl_apply_config();
                }
                else if (opt[k] == OPT_main_combat_rolls)
                {
                    op_ptr->main_combat_rolls = (op_ptr->main_combat_rolls < 4)
                        ? op_ptr->main_combat_rolls + 1
                        : 4;

                    /* Clear all 4 lines and refresh display when option changes */
                    clear_main_combat_rolls_area();
                    display_main_combat_rolls();
                    p_ptr->redraw |= (PR_MAP);
                }
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode < LEVEL_ENTRY_NARRATIVE_OFF)
                        ? op_ptr->level_entry_narrative_mode + 1
                        : LEVEL_ENTRY_NARRATIVE_OFF;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode < PARTITION_NARRATIVE_OFF)
                        ? op_ptr->partition_narrative_mode + 1
                        : PARTITION_NARRATIVE_OFF;
                }
                else if (opt[k] == OPT_ability_desc_mode)
                {
                    op_ptr->ability_desc_mode = (op_ptr->ability_desc_mode < 2)
                        ? op_ptr->ability_desc_mode + 1
                        : 2;
                }
                else if (opt[k] == OPT_vault_drop_frequency)
                {
                    op_ptr->vault_drop_frequency
                        = (op_ptr->vault_drop_frequency < VDF_PLENTIFUL)
                        ? op_ptr->vault_drop_frequency + 1
                        : VDF_PLENTIFUL;
                }
                else if (opt[k] == OPT_noble_item_spawn_mode)
                {
                    op_ptr->noble_item_spawn_mode
                        = (op_ptr->noble_item_spawn_mode < NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
                        ? op_ptr->noble_item_spawn_mode + 1
                        : NOBLE_ITEM_SPAWN_INCLUDE_VAULTS;
                }
                else if (opt[k] == OPT_intro_style)
                {
                    op_ptr->intro_style
                        = (op_ptr->intro_style < INTRO_STYLE_RANDOM)
                        ? op_ptr->intro_style + 1
                        : INTRO_STYLE_RANDOM;
                }
                else
                {
                    op_ptr->opt[opt[k]] = true;
                    option_apply_side_effects(opt[k]);
                }

                if (is_sound_page)
                    sound_settings_dirty = true;
                else if (option_is_app_persistent(opt[k]))
                    app_settings_dirty = true;
                else if (metarun_page)
                    metarun_settings_dirty = true;
            }
            break;
        }

        case 'n':
        case '4':
        {
            if ((page != CHALLENGE_PAGE) || (playerturn == 0))
            {
                if (is_sound_page)
                {
                    if (k == 0)
                    {
                        sound_cfg->enabled = false;
                        use_sound = false;
                    }
                    else if (k == 1) sound_cfg->enable_combat = false;
                    else if (k == 2) sound_cfg->enable_inventory = false;
                    else if (k == 3) sound_cfg->enable_walk = false;
                    else if (k == 4) sound_cfg->enable_doors = false;
                    else if (k == 5) sound_cfg->volume_combat = (sound_cfg->volume_combat > 0.0f) ? sound_cfg->volume_combat - 0.1f : 0.0f;
                    else if (k == 6) sound_cfg->volume_inventory = (sound_cfg->volume_inventory > 0.0f) ? sound_cfg->volume_inventory - 0.1f : 0.0f;
                    else if (k == 7) sound_cfg->volume_walk = (sound_cfg->volume_walk > 0.0f) ? sound_cfg->volume_walk - 0.1f : 0.0f;
                    else if (k == 8) sound_cfg->volume_doors = (sound_cfg->volume_doors > 0.0f) ? sound_cfg->volume_doors - 0.1f : 0.0f;
                    else if (k == 9) sound_cfg->volume_other = (sound_cfg->volume_other > 0.0f) ? sound_cfg->volume_other - 0.1f : 0.0f;
                    else if (k == 10) sound_cfg->music_main_enabled = false;
                    else if (k == 11) sound_cfg->music_ambient_enabled = false;
                    else if (k == 12) {
                        sound_cfg->music_main_volume = (sound_cfg->music_main_volume > 0.0f) ? sound_cfg->music_main_volume - 0.1f : 0.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                    else if (k == 13) {
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume > 0.0f) ? sound_cfg->music_ambient_volume - 0.1f : 0.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                }
                else if (opt[k] == OPT_delay_factor)
                {
                    op_ptr->delay_factor = (op_ptr->delay_factor > 0)
                        ? op_ptr->delay_factor - 1
                        : 0;
                }
                else if (opt[k] == OPT_hitpoint_warning)
                {
                    op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn > 0)
                        ? op_ptr->hitpoint_warn - 1
                        : 0;
                }
                else if (opt[k] == OPT_hide_left_panel)
                {
                    settings_sdl_set_hide_left_panel(false);
                    sdl_apply_config();
                }
                else if (opt[k] == OPT_main_combat_rolls)
                {
                    op_ptr->main_combat_rolls = (op_ptr->main_combat_rolls > 0)
                        ? op_ptr->main_combat_rolls - 1
                        : 0;

                    /* Clear all 4 lines and refresh display when option changes */
                    clear_main_combat_rolls_area();
                    display_main_combat_rolls();
                    p_ptr->redraw |= (PR_MAP);
                }
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode > LEVEL_ENTRY_NARRATIVE_BANNER_DELAY)
                        ? op_ptr->level_entry_narrative_mode - 1
                        : LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode > PARTITION_NARRATIVE_BANNER)
                        ? op_ptr->partition_narrative_mode - 1
                        : PARTITION_NARRATIVE_BANNER;
                }
                else if (opt[k] == OPT_ability_desc_mode)
                {
                    op_ptr->ability_desc_mode = (op_ptr->ability_desc_mode > 0)
                        ? op_ptr->ability_desc_mode - 1
                        : 0;
                }
                else if (opt[k] == OPT_vault_drop_frequency)
                {
                    op_ptr->vault_drop_frequency
                        = (op_ptr->vault_drop_frequency > VDF_NORMAL)
                        ? op_ptr->vault_drop_frequency - 1
                        : VDF_NORMAL;
                }
                else if (opt[k] == OPT_noble_item_spawn_mode)
                {
                    op_ptr->noble_item_spawn_mode
                        = (op_ptr->noble_item_spawn_mode > NOBLE_ITEM_SPAWN_RESTRICTED)
                        ? op_ptr->noble_item_spawn_mode - 1
                        : NOBLE_ITEM_SPAWN_RESTRICTED;
                }
                else if (opt[k] == OPT_intro_style)
                {
                    op_ptr->intro_style
                        = (op_ptr->intro_style > INTRO_STYLE_FLAME)
                        ? op_ptr->intro_style - 1
                        : INTRO_STYLE_FLAME;
                }
                else
                {
                    op_ptr->opt[opt[k]] = false;
                    option_apply_side_effects(opt[k]);
                }

                if (is_sound_page)
                    sound_settings_dirty = true;
                else if (option_is_app_persistent(opt[k]))
                    app_settings_dirty = true;
                else if (metarun_page)
                    metarun_settings_dirty = true;
            }
            break;
        }

        default:
        {
            bell("Illegal command for normal options!");
            break;
        }
        }

        if (birth_fixed_exp && playerturn == 0 && p_ptr->exp != PY_FIXED_EXP)
        {
            int total_exp = PY_FIXED_EXP;
            p_ptr->new_exp = total_exp;
            p_ptr->exp = total_exp;
            check_experience();
            clear_skills_and_abilities();
        }
        else if (!birth_fixed_exp && playerturn == 0
            && p_ptr->exp >= PY_FIXED_EXP)
        {
            int total_exp = PY_START_EXP;
            p_ptr->new_exp = total_exp;
            p_ptr->exp = total_exp;
            check_experience();
            clear_skills_and_abilities();
        }
    }
}

/*
 * Write all current options to the given preference file in the
 * lib/user directory. Modified from KAmband 1.8.
 */
static errr option_dump(cptr fname)
{
    static cptr mark = "Options Dump";

    int i, j;

    SDL_IOStream* fff;

    char buf[1024];

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("option_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old options */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = sdl_fopen(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    /* Start dumping */
    SDL_IOprintf(fff, "# Automatic option dump\n\n");

    /* Dump options (skip cheat, adult, score) */
    for (i = 0; i < OPT_CHEAT; i++)
    {
        /* Require a real option */
        if (!option_text[i])
            continue;

        /* Comment */
        SDL_IOprintf(fff, "# Option '%s'\n", option_desc[i]);

        /* Dump the option */
        if (op_ptr->opt[i])
        {
            SDL_IOprintf(fff, "Y:%s\n", option_text[i]);
        }
        else
        {
            SDL_IOprintf(fff, "X:%s\n", option_text[i]);
        }

        /* Skip a line */
        SDL_IOprintf(fff, "\n");
    }

    /* Dump window flags */
    for (i = 1; i < ANGBAND_TERM_MAX; i++)
    {
        /* Require a real window */
        if (!angband_term[i])
            continue;

        /* Check each flag */
        for (j = 0; j < 32; j++)
        {
            /* Require a real flag */
            if (!window_flag_desc[j])
                continue;

            /* Comment */
            SDL_IOprintf(fff, "# Window '%s', Flag '%s'\n", angband_term_name[i],
                window_flag_desc[j]);

            /* Dump the flag */
            if (op_ptr->window_flag[i] & (1L << j))
            {
                SDL_IOprintf(fff, "W:%d:%d:1\n", i, j);
            }
            else
            {
                SDL_IOprintf(fff, "W:%d:%d:0\n", i, j);
            }

            /* Skip a line */
            SDL_IOprintf(fff, "\n");
        }
    }

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

/*
 * Display and manage SDL pane settings
 * Interactive menu to edit SDL configuration
 */
static int get_supporting_pane_config_count(void);
static void do_cmd_supporting_pane_layout_editor(bool* settings_changed);
static void do_cmd_supporting_pane_font_editor(bool* settings_changed);
static void do_cmd_touch_pane_button_editor(bool* settings_changed);
static const char* pane_type_short_name(enum pane_type type);
static enum pane_type settings_sdl_pane_type(int idx);
static enum pane_placement settings_sdl_pane_where(int idx);
static bool settings_sdl_pane_enabled(int idx);
static int settings_sdl_pane_rows(int idx);
static int settings_sdl_pane_cols(int idx);
static int settings_sdl_pane_font_size(int idx);
static int settings_sdl_pane_effective_font_size(int idx);
static void settings_sdl_set_pane_rows(int idx, int rows);
static void settings_sdl_set_pane_cols(int idx, int cols);
static void settings_sdl_set_pane_font_size(int idx, int size);
static int settings_sdl_main_view_scale(void);
static int settings_sdl_max_scale(void);
static void settings_sdl_set_main_view_scale(int value);
static int settings_sdl_min_terminal_mode(void);
static void settings_sdl_set_min_terminal_mode(int value);
static int settings_sdl_aux_view_font_size(void);
static int settings_sdl_effective_aux_view_font_size(void);
static void settings_sdl_set_aux_view_font_size(int value);
static int settings_sdl_menu_panel_font_size(void);
static int settings_sdl_effective_menu_panel_font_size(void);
static void settings_sdl_set_menu_panel_font_size(int value);
static int settings_sdl_margin(void);
static void settings_sdl_set_margin(int value);
static bool settings_sdl_fullscreen(void);
static void settings_sdl_set_fullscreen(bool enabled);
static bool settings_sdl_tiles(void);
static void settings_sdl_set_tiles(bool enabled);
static bool settings_sdl_right_panes_enabled(void);
static void settings_sdl_set_right_panes_enabled(bool enabled);
static bool settings_sdl_bottom_panes_enabled(void);
static void settings_sdl_set_bottom_panes_enabled(bool enabled);
static const char* settings_sdl_config_path(void);
static const char* settings_sdl_touch_slot_name(int idx);
static void settings_sdl_touch_panel_name(int panel, char* buf, size_t buflen);
static void settings_sdl_touch_button_label(int panel, int slot, char* buf,
    size_t buflen);
static int settings_sdl_touch_binding(int panel, int slot);
static void settings_sdl_set_touch_binding(int panel, int slot, int binding);

static int settings_sdl_main_view_scale(void)
{
    return get_sdl_main_view_scale();
}

static int settings_sdl_max_scale(void)
{
    return get_sdl_max_scale();
}

static void settings_sdl_set_main_view_scale(int value)
{
    set_sdl_main_view_scale(value);
}

static int settings_sdl_min_terminal_mode(void)
{
    return get_sdl_min_terminal_mode();
}

static void settings_sdl_set_min_terminal_mode(int value)
{
    set_sdl_min_terminal_mode(value);
}

static int settings_sdl_aux_view_font_size(void)
{
    return get_sdl_aux_view_font_size();
}

static int settings_sdl_effective_aux_view_font_size(void)
{
    return get_sdl_effective_aux_view_font_size();
}

static void settings_sdl_set_aux_view_font_size(int value)
{
    set_sdl_aux_view_font_size(value);
}

static int settings_sdl_menu_panel_font_size(void)
{
    return get_sdl_menu_panel_font_size();
}

static int settings_sdl_effective_menu_panel_font_size(void)
{
    return get_sdl_effective_menu_panel_font_size();
}

static void settings_sdl_set_menu_panel_font_size(int value)
{
    set_sdl_menu_panel_font_size(value);
}

static int settings_sdl_margin(void)
{
    return get_sdl_margin();
}

static void settings_sdl_set_margin(int value)
{
    set_sdl_margin(value);
}

static bool settings_sdl_fullscreen(void)
{
    return get_sdl_fullscreen();
}

static void settings_sdl_set_fullscreen(bool enabled)
{
    set_sdl_fullscreen(enabled);
}

static bool settings_sdl_tiles(void)
{
    return get_sdl_tiles();
}

static void settings_sdl_set_tiles(bool enabled)
{
    set_sdl_tiles(enabled);
}

static bool settings_sdl_right_panes_enabled(void)
{
    return get_sdl_enable_right_panes();
}

static void settings_sdl_set_right_panes_enabled(bool enabled)
{
    set_sdl_enable_right_panes(enabled);
}

static bool settings_sdl_bottom_panes_enabled(void)
{
    return get_sdl_enable_bottom_panes();
}

static void settings_sdl_set_bottom_panes_enabled(bool enabled)
{
    set_sdl_enable_bottom_panes(enabled);
}

static const char* settings_sdl_config_path(void)
{
    return get_sdl_config_path();
}

typedef struct settings_sdl_pane_overview {
    int main_view_scale;
    int max_scale;
    int min_terminal_mode;
    int aux_view_font_size;
    int effective_aux_view_font_size;
    int menu_panel_font_size;
    int effective_menu_panel_font_size;
    int margin;
    bool fullscreen;
    bool tiles;
    bool right_panes_enabled;
    bool bottom_panes_enabled;
    cptr config_path;
} settings_sdl_pane_overview;

static void settings_sdl_read_pane_overview(
    settings_sdl_pane_overview* overview)
{
    if (!overview)
        return;

    memset(overview, 0, sizeof(*overview));
    overview->main_view_scale = settings_sdl_main_view_scale();
    overview->max_scale = settings_sdl_max_scale();
    overview->min_terminal_mode = settings_sdl_min_terminal_mode();
    overview->aux_view_font_size = settings_sdl_aux_view_font_size();
    overview->effective_aux_view_font_size
        = settings_sdl_effective_aux_view_font_size();
    overview->menu_panel_font_size = settings_sdl_menu_panel_font_size();
    overview->effective_menu_panel_font_size
        = settings_sdl_effective_menu_panel_font_size();
    overview->margin = settings_sdl_margin();
    overview->fullscreen = settings_sdl_fullscreen();
    overview->tiles = settings_sdl_tiles();
    overview->right_panes_enabled = settings_sdl_right_panes_enabled();
    overview->bottom_panes_enabled = settings_sdl_bottom_panes_enabled();
    overview->config_path = settings_sdl_config_path();
}

static void format_font_size_value(char* buf, size_t buflen, int raw, int effective,
    int max_chars)
{
    char long_buf[24];
    char medium_buf[24];
    char short_buf[16];

    if (!buf || !buflen)
        return;

    if (raw > 0)
    {
        strnfmt(long_buf, sizeof(long_buf), "%d", raw);
        settings_ui_fit_text(buf, buflen, long_buf, max_chars);
        return;
    }

    strnfmt(long_buf, sizeof(long_buf), "auto (%d)", effective);
    strnfmt(medium_buf, sizeof(medium_buf), "auto %d", effective);
    strnfmt(short_buf, sizeof(short_buf), "a%d", effective);
    settings_ui_fit_text(buf, buflen,
        settings_ui_pick_label(max_chars, long_buf, medium_buf, short_buf),
        max_chars);
}

static const char* sdl_min_terminal_mode_label(int mode)
{
    return (mode == 1) ? "compact (50x18)" : "normal (80x24)";
}

static bool pane_settings_present_ui_scene(int k, bool settings_changed,
    const settings_sdl_pane_overview* overview)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    char value_buf[32];
    char font_value[24];
    cptr config_label;
    int row_width;
    int label_hint;

    if (!overview)
        return false;

    config_label = (overview->config_path && overview->config_path[0])
        ? overview->config_path
        : "sil_sdl.json";
    row_width = settings_ui_named_sdl_line_width(0);
    label_hint = MAX(22, MIN(52, row_width - 28));

    panel = settings_browser_scene_begin_ex(&scene, "SDL Pane Settings",
        config_label, 1120, 2200);
    if (!panel)
        return false;

    if (k > 4)
        app_ui_panel_set_row_offset(panel, (s16b)(k - 4));

    strnfmt(value_buf, sizeof(value_buf), "%d", overview->main_view_scale);
    if (!settings_browser_add_pair_row(panel, 0, (k == 0) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 0,
            settings_ui_pick_label(label_hint,
                "Main View Scale (1-max) [Alt++/-]",
                "Main View Scale [Alt++/-]",
                "View Scale"), value_buf))
    {
        return false;
    }

    if (!settings_browser_add_pair_row(panel, 1, (k == 1) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 1,
            settings_ui_pick_label(label_hint,
                "Minimum Terminal Size",
                "Min Terminal Size",
                "Min Terminal"),
            sdl_min_terminal_mode_label(overview->min_terminal_mode)))
    {
        return false;
    }

    format_font_size_value(font_value, sizeof(font_value),
        overview->aux_view_font_size,
        overview->effective_aux_view_font_size,
        MAX(6, MIN(14, row_width / 2)));
    if (!settings_browser_add_pair_row(panel, 2, (k == 2) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 2,
            settings_ui_pick_label(label_hint,
                "Default Aux Font Size (0=auto, 8-48)",
                "Default Aux Font (0=auto)",
                "Aux Font"), font_value))
    {
        return false;
    }

    format_font_size_value(font_value, sizeof(font_value),
        overview->menu_panel_font_size,
        overview->effective_menu_panel_font_size,
        MAX(6, MIN(14, row_width / 2)));
    if (!settings_browser_add_pair_row(panel, 3, (k == 3) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 3,
            settings_ui_pick_label(label_hint,
                "Menu + Left Panel Font (0=auto, 8-64)",
                "Menu + Left Panel Font",
                "Menu Font"), font_value))
    {
        return false;
    }

    strnfmt(value_buf, sizeof(value_buf), "%d", overview->margin);
    if (!settings_browser_add_pair_row(panel, 4, (k == 4) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 4,
            settings_ui_pick_label(label_hint,
                "Margin (0-20)",
                "Margin",
                "Margin"), value_buf))
    {
        return false;
    }

    if (!settings_browser_add_pair_row(panel, 5, (k == 5) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 5, "Fullscreen",
            overview->fullscreen ? "yes" : "no"))
    {
        return false;
    }

    if (!settings_browser_add_pair_row(panel, 6, (k == 6) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 6, "Tiles",
            overview->tiles ? "yes" : "no"))
    {
        return false;
    }

    if (!settings_browser_add_pair_row(panel, 7, (k == 7) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 7,
            settings_ui_pick_label(label_hint,
                "Enable Side Panes [Alt+I]",
                "Side Panes [Alt+I]",
                "Side Panes"),
            overview->right_panes_enabled ? "yes" : "no"))
    {
        return false;
    }

    if (!settings_browser_add_pair_row(panel, 8, (k == 8) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 8,
            settings_ui_pick_label(label_hint,
                "Enable Bottom Panes [Alt+L]",
                "Bottom Panes [Alt+L]",
                "Bottom Panes"),
            overview->bottom_panes_enabled ? "yes" : "no"))
    {
        return false;
    }

    strnfmt(value_buf, sizeof(value_buf), "%d", get_supporting_pane_config_count());
    if (!settings_browser_add_pair_row(panel, 9, (k == 9) ? TERM_L_BLUE
            : TERM_WHITE, TERM_SLATE, true, k == 9,
            settings_ui_pick_label(label_hint,
                "View Pane Configuration",
                "Pane Configuration",
                "Pane Layout"), value_buf))
    {
        return false;
    }

    if (!settings_browser_add_label_row(panel, 10, (k == 10) ? TERM_L_BLUE
            : TERM_WHITE, true, k == 10,
            settings_ui_pick_label(label_hint,
                "Pane Font Sizes",
                "Pane Fonts",
                "Pane Fonts")))
    {
        return false;
    }

    if (!settings_browser_add_label_row(panel, 11, (k == 11) ? TERM_L_BLUE
            : TERM_WHITE, true, k == 11, settings_changed
            ? "Save Changes and Return"
            : "Return to Options Menu"))
    {
        return false;
    }

    if (settings_changed)
    {
        (void)app_ui_panel_add_body_line(panel, TERM_YELLOW,
            "Settings changed. Changes take effect immediately.");
        (void)app_ui_panel_add_body_line(panel, TERM_YELLOW,
            "Changes will be saved to the SDL config file on exit.");
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "4/6", "Set");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE,
        (k == 2) || (k == 3), "0", "Auto");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "Enter", (k == 9 || k == 10) ? "Open" : "Accept");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");

    return ui_information_scene_present_ui(&scene);
}

void do_cmd_pane_settings(void)
{
    int k = 0;
    int n = 12; /* Total number of options */
    bool done = false;
    bool settings_changed = false;
    int dir;

    while (!done)
    {
        settings_sdl_pane_overview overview;
        cptr config_label;

        settings_sdl_read_pane_overview(&overview);
        config_label = (overview.config_path && overview.config_path[0])
            ? overview.config_path
            : "sil_sdl.json";

        if (!pane_settings_present_ui_scene(k, settings_changed, &overview))
        {
            done = true;
            continue;
        }

        /* Get key */
        inkey_set_cursor_hidden(true);
        char ch = settings_ui_read_legacy_key(false);
        inkey_set_cursor_hidden(false);
        
        /* Try to translate the key into a direction */
        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);
        
        /* Process input */
        switch (ch)
        {
        case ESCAPE:
        {
            /* Exit without needing to navigate to the bottom */
            if (settings_changed)
            {
                if (save_pane_config_to_json())
                {
                    msg_format("Settings saved to %s", config_label);
                }
            }
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            /* Enter activates the current option for actions; otherwise accept/exit. */
            if (k == 9) /* Supporting Pane Layout */
            {
                do_cmd_supporting_pane_layout_editor(&settings_changed);
                break;
            }
            if (k == 10) /* Pane Font Sizes */
            {
                do_cmd_supporting_pane_font_editor(&settings_changed);
                break;
            }

            /* Save if changed, then exit */
            if (settings_changed)
            {
                if (save_pane_config_to_json())
                {
                    msg_format("Settings saved to %s", config_label);
                }
            }
            done = true;
            break;
        }
        
        case '-':
        case '8':
        {
            /* Move up */
            k = (n + k - 1) % n;
            break;
        }
        
        case '2':
        {
            /* Move down */
            k = (k + 1) % n;
            break;
        }

        case '0':
        {
            if (k == 2)
            {
                if (overview.aux_view_font_size != 0)
                {
                    settings_sdl_set_aux_view_font_size(0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3)
            {
                if (overview.menu_panel_font_size != 0)
                {
                    settings_sdl_set_menu_panel_font_size(0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else
            {
                bell("0 sets the selected font to auto");
            }
            break;
        }
        
        case 't':
        case '5':
        case ' ':
        {
            /* Toggle or activate current option */
            if (k == 1) /* Minimum Terminal Size */
            {
                settings_sdl_set_min_terminal_mode(overview.min_terminal_mode
                    == 0 ? 1 : 0);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 5) /* Fullscreen */
            {
                settings_sdl_set_fullscreen(!overview.fullscreen);
                settings_changed = true;
            }
            else if (k == 6) /* Tiles */
            {
                settings_sdl_set_tiles(!overview.tiles);
                settings_changed = true;
            }
            else if (k == 7) /* Enable Side Panes */
            {
                settings_sdl_set_right_panes_enabled(
                    !overview.right_panes_enabled);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 8) /* Enable Bottom Panes */
            {
                settings_sdl_set_bottom_panes_enabled(
                    !overview.bottom_panes_enabled);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 9) /* Supporting Pane Layout */
            {
                do_cmd_supporting_pane_layout_editor(&settings_changed);
            }
            else if (k == 10) /* Pane Font Sizes */
            {
                do_cmd_supporting_pane_font_editor(&settings_changed);
            }
            else if (k == 11) /* Save/Return */
            {
                if (settings_changed)
                {
                    if (save_pane_config_to_json())
                    {
                        msg_format("Settings saved to %s", config_label);
                    }
                }
                done = true;
            }
            break;
        }
        
        case 'y':
        case '6':
        {
            /* Increase value or set to yes */
            int val;
            
            if (k == 0) /* Main View Scale */
            {
                val = overview.main_view_scale;
                if (val < overview.max_scale)
                {
                    settings_sdl_set_main_view_scale(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 1) /* Minimum Terminal Size */
            {
                if (overview.min_terminal_mode != 0)
                {
                    settings_sdl_set_min_terminal_mode(0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 2) /* Aux View Font Size */
            {
                val = overview.aux_view_font_size;
                if (val == 0)
                {
                    settings_sdl_set_aux_view_font_size(
                        overview.effective_aux_view_font_size);
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val < 48)
                {
                    settings_sdl_set_aux_view_font_size(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3) /* Menu + Left Panel Font Size */
            {
                val = overview.menu_panel_font_size;
                if (val == 0)
                {
                    settings_sdl_set_menu_panel_font_size(
                        overview.effective_menu_panel_font_size);
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val < 64)
                {
                    settings_sdl_set_menu_panel_font_size(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 4) /* Margin */
            {
                val = overview.margin;
                if (val < 20)
                {
                    settings_sdl_set_margin(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 5) /* Fullscreen */
            {
                if (!overview.fullscreen)
                {
                    settings_sdl_set_fullscreen(true);
                    settings_changed = true;
                }
            }
            else if (k == 6) /* Tiles */
            {
                if (!overview.tiles)
                {
                    settings_sdl_set_tiles(true);
                    settings_changed = true;
                }
            }
            else if (k == 7) /* Enable Side Panes */
            {
                if (!overview.right_panes_enabled)
                {
                    settings_sdl_set_right_panes_enabled(true);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 8) /* Enable Bottom Panes */
            {
                if (!overview.bottom_panes_enabled)
                {
                    settings_sdl_set_bottom_panes_enabled(true);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            break;
        }
        
        case 'n':
        case '4':
        {
            /* Decrease value or set to no */
            int val;
            
            if (k == 0) /* Main View Scale */
            {
                val = overview.main_view_scale;
                if (val > 1)
                {
                    settings_sdl_set_main_view_scale(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 1) /* Minimum Terminal Size */
            {
                if (overview.min_terminal_mode != 1)
                {
                    settings_sdl_set_min_terminal_mode(1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 2) /* Aux View Font Size */
            {
                val = overview.aux_view_font_size;
                if (val == 0)
                {
                    settings_sdl_set_aux_view_font_size(
                        overview.effective_aux_view_font_size);
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val > 8)
                {
                    settings_sdl_set_aux_view_font_size(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3) /* Menu + Left Panel Font Size */
            {
                val = overview.menu_panel_font_size;
                if (val == 0)
                {
                    settings_sdl_set_menu_panel_font_size(
                        overview.effective_menu_panel_font_size);
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val > 8)
                {
                    settings_sdl_set_menu_panel_font_size(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 4) /* Margin */
            {
                val = overview.margin;
                if (val > 0)
                {
                    settings_sdl_set_margin(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 5) /* Fullscreen */
            {
                if (overview.fullscreen)
                {
                    settings_sdl_set_fullscreen(false);
                    settings_changed = true;
                }
            }
            else if (k == 6) /* Tiles */
            {
                if (overview.tiles)
                {
                    settings_sdl_set_tiles(false);
                    settings_changed = true;
                }
            }
            else if (k == 7) /* Enable Side Panes */
            {
                if (overview.right_panes_enabled)
                {
                    settings_sdl_set_right_panes_enabled(false);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 8) /* Enable Bottom Panes */
            {
                if (overview.bottom_panes_enabled)
                {
                    settings_sdl_set_bottom_panes_enabled(false);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            break;
        }
        
        default:
        {
            bell("Illegal command for pane settings!");
            break;
        }
        }
    }
}


typedef struct settings_sdl_pane_state {
    enum pane_type type;
    enum pane_placement where;
    bool enabled;
    int rows;
    int cols;
    int font_size;
    int effective_font_size;
    int current_rows;
    int current_cols;
} settings_sdl_pane_state;

static void settings_sdl_read_pane_state(int idx,
    settings_sdl_pane_state* pane_state);

static const char* pane_type_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INVENTORY";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHARACTER";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MONSTERS";
    case PANE_TOUCH: return "TOUCH";
    default: return "UNKNOWN";
    }
}

static void do_cmd_supporting_pane_font_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = 8 };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)settings_sdl_pane_type(i);
        if (type == PANE_MAIN)
            continue;
        pane_indices[pane_count++] = i;
    }

    if (pane_count <= 0)
    {
        app_ui_scene scene;
        app_ui_panel* panel = settings_browser_scene_begin(&scene,
            "Supporting Pane Fonts", "");

        if (panel)
        {
            (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
                "No supporting panes are configured.");
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE,
                true, "Esc", "Back");
            (void)ui_information_scene_present_ui(&scene);
            (void)settings_ui_read_legacy_key(false);
        }
        return;
    }

    {
        int sel = 0;
        bool done = false;
        bool changed = false;
        int dir;

        while (!done)
        {
            int row_width;
            row_width = settings_ui_named_sdl_line_width(2);
            app_ui_scene scene;
            app_ui_panel* panel = settings_browser_scene_begin(&scene,
                "Supporting Pane Fonts", "");

            if (!panel)
            {
                done = true;
            }
            else
            {
                if (sel > 4)
                    app_ui_panel_set_row_offset(panel, (s16b)(sel - 4));
                for (int i = 0; i < pane_count; i++)
                {
                    int idx = pane_indices[i];
                    settings_sdl_pane_state pane_state;
                    enum pane_type type;
                    bool enabled;
                    int raw_font;
                    int effective_font;
                    byte a;
                    char label_buf[48];
                    char font_value[24];
                    const char* type_label;

                    settings_sdl_read_pane_state(idx, &pane_state);
                    type = pane_state.type;
                    enabled = pane_state.enabled;
                    raw_font = pane_state.font_size;
                    effective_font = pane_state.effective_font_size;
                    a = (i == sel) ? TERM_L_BLUE
                        : (enabled ? TERM_WHITE : TERM_SLATE);
                    type_label = settings_ui_pick_label(MAX(8, row_width / 2),
                        pane_type_name(type), pane_type_name(type),
                        pane_type_short_name(type));

                    format_font_size_value(font_value, sizeof(font_value),
                        raw_font, effective_font,
                        MAX(6, MIN(14, row_width / 2)));
                    strnfmt(label_buf, sizeof(label_buf), "%s %s",
                        type_label, enabled ? "on" : "off");
                    if (!settings_browser_add_pair_row(panel, (s16b)i, a,
                            TERM_SLATE, true, i == sel, label_buf,
                            font_value))
                    {
                        done = true;
                        break;
                    }
                }

                (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
                    "Changes apply immediately.");
                (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE,
                    true, "8/2", "Move");
                (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE,
                    true, "4/6", "Set");
                (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE,
                    true, "0", "Auto");
                (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE,
                    true, "Esc", "Back");
                if (!done && !ui_information_scene_present_ui(&scene))
                    done = true;
            }

            inkey_set_cursor_hidden(true);
            char ch = settings_ui_read_legacy_key(false);
            inkey_set_cursor_hidden(false);

            dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);

            switch (ch)
            {
            case ESCAPE:
            case '\n':
            case '\r':
                done = true;
                break;

            case '-':
            case '8':
                sel = (pane_count + sel - 1) % pane_count;
                break;

            case '2':
                sel = (sel + 1) % pane_count;
                break;

            case '0':
            {
                int idx = pane_indices[sel];
                if (settings_sdl_pane_font_size(idx) != 0)
                {
                    settings_sdl_set_pane_font_size(idx, 0);
                    changed = true;
                    sdl_apply_config();
                }
                break;
            }

            case 'n':
            case '4':
            case 'y':
            case '6':
            {
                int idx = pane_indices[sel];
                int delta = ((ch == 'n') || (ch == '4')) ? -1 : 1;
                int value = settings_sdl_pane_font_size(idx);

                if (value == 0)
                    settings_sdl_set_pane_font_size(idx, settings_sdl_pane_effective_font_size(idx));
                else
                    settings_sdl_set_pane_font_size(idx, value + delta);

                changed = true;
                sdl_apply_config();
                break;
            }

            default:
                bell("Illegal command for pane font editor!");
                break;
            }
        }

        if (changed && settings_changed)
            *settings_changed = true;
    }
}

static const char* pane_type_short_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INV";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHAR";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MON";
    case PANE_TOUCH: return "TOUCH";
    default: return "UNK";
    }
}

static const char* pane_where_short_name(enum pane_placement where)
{
    switch (where)
    {
    case PLACE_RIGHT: return "R";
    case PLACE_LEFT: return "L";
    case PLACE_DOUBLE_RIGHT: return "DR";
    case PLACE_DOUBLE_LEFT: return "DL";
    case PLACE_BOTTOM: return "BOT";
    default: return "?";
    }
}

static enum pane_type settings_sdl_pane_type(int idx)
{
    return (enum pane_type)get_sdl_pane_type(idx);
}

static enum pane_placement settings_sdl_pane_where(int idx)
{
    return (enum pane_placement)get_sdl_pane_where(idx);
}

static bool settings_sdl_pane_enabled(int idx)
{
    return get_sdl_pane_enabled(idx);
}

static int settings_sdl_pane_rows(int idx)
{
    return get_sdl_pane_rows(idx);
}

static int settings_sdl_pane_cols(int idx)
{
    return get_sdl_pane_cols(idx);
}

static int settings_sdl_pane_font_size(int idx)
{
    return get_sdl_pane_font_size(idx);
}

static int settings_sdl_pane_effective_font_size(int idx)
{
    return get_sdl_pane_effective_font_size(idx);
}

static void settings_sdl_set_pane_rows(int idx, int rows)
{
    set_sdl_pane_rows(idx, rows);
}

static void settings_sdl_set_pane_cols(int idx, int cols)
{
    set_sdl_pane_cols(idx, cols);
}

static void settings_sdl_set_pane_font_size(int idx, int size)
{
    set_sdl_pane_font_size(idx, size);
}

static void settings_sdl_set_pane_enabled(int idx, bool enabled)
{
    set_sdl_pane_enabled(idx, enabled);
}

static void settings_sdl_set_pane_where(int idx, enum pane_placement where)
{
    set_sdl_pane_where(idx, where);
}

static int settings_sdl_pane_current_rows(int idx)
{
    return get_sdl_pane_current_rows(idx);
}

static int settings_sdl_pane_current_cols(int idx)
{
    return get_sdl_pane_current_cols(idx);
}

static void settings_sdl_read_pane_state(int idx,
    settings_sdl_pane_state* pane_state)
{
    if (!pane_state)
        return;

    memset(pane_state, 0, sizeof(*pane_state));
    pane_state->type = settings_sdl_pane_type(idx);
    pane_state->where = settings_sdl_pane_where(idx);
    pane_state->enabled = settings_sdl_pane_enabled(idx);
    pane_state->rows = settings_sdl_pane_rows(idx);
    pane_state->cols = settings_sdl_pane_cols(idx);
    pane_state->font_size = settings_sdl_pane_font_size(idx);
    pane_state->effective_font_size = settings_sdl_pane_effective_font_size(
        idx);
    pane_state->current_rows = settings_sdl_pane_current_rows(idx);
    pane_state->current_cols = settings_sdl_pane_current_cols(idx);
}

static int get_supporting_pane_config_count(void)
{
    int count = 0;
    int total = get_pane_config_count();
    for (int i = 0; i < total; i++)
    {
        enum pane_type type = (enum pane_type)settings_sdl_pane_type(i);
        if (type != PANE_MAIN)
            count++;
    }
    return count;
}

static int supporting_pane_master_idx(const int* pane_indices, int pane_count,
    enum pane_placement where)
{
    int fallback = -1;

    for (int i = 0; i < pane_count; i++)
    {
        int idx = pane_indices[i];
        if ((enum pane_placement)settings_sdl_pane_where(idx) != where)
            continue;
        if (fallback < 0)
            fallback = idx;
        if (settings_sdl_pane_enabled(idx))
            return idx;
    }

    return fallback;
}

static bool supporting_pane_rows_locked(const int* pane_indices, int pane_count, int idx)
{
    enum pane_placement where = (enum pane_placement)settings_sdl_pane_where(idx);
    int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

    return (where == PLACE_BOTTOM && idx != master_idx);
}

static bool supporting_pane_cols_locked(const int* pane_indices, int pane_count, int idx)
{
    enum pane_placement where = (enum pane_placement)settings_sdl_pane_where(idx);
    int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

    return (pane_placement_is_side(where) && idx != master_idx);
}

static void supporting_pane_ensure_editable_field(int* field, const int* pane_indices,
    int pane_count, int sel)
{
    int idx;

    if (!field || pane_count <= 0 || sel < 0 || sel >= pane_count)
        return;

    idx = pane_indices[sel];
    while ((*field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
        || (*field == 3 && supporting_pane_cols_locked(pane_indices, pane_count, idx)))
    {
        *field = (*field + 1) % 4;
    }
}

static bool supporting_pane_normalize_shared_sizes(const int* pane_indices, int pane_count)
{
    bool changed = false;

    for (int i = 0; i < pane_count; i++)
    {
        int idx = pane_indices[i];
        enum pane_placement where = (enum pane_placement)settings_sdl_pane_where(idx);
        int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

        if (where == PLACE_BOTTOM && idx != master_idx && settings_sdl_pane_rows(idx) != 0)
        {
            settings_sdl_set_pane_rows(idx, 0);
            changed = true;
        }
        else if (pane_placement_is_side(where) && idx != master_idx
            && settings_sdl_pane_cols(idx) != 0)
        {
            settings_sdl_set_pane_cols(idx, 0);
            changed = true;
        }
    }

    return changed;
}

static void do_cmd_supporting_pane_layout_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = 8 };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)settings_sdl_pane_type(i);
        if (type == PANE_MAIN)
            continue;
        pane_indices[pane_count++] = i;
    }

    int sel = 0;
    int field = 0; /* 0 = enabled, 1 = where, 2 = rows, 3 = cols */
    bool done = false;
    bool changed = false;
    int dir;

    if (pane_count <= 0)
    {
        app_ui_scene scene;
        app_ui_panel* panel = settings_browser_scene_begin(&scene,
            "Supporting Pane Layout", "");

        if (panel)
        {
            (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
                "No supporting panes are configured.");
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE,
                true, "Esc", "Back");
            (void)ui_information_scene_present_ui(&scene);
            (void)settings_ui_read_legacy_key(false);
        }
        return;
    }

    if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
    {
        changed = true;
        sdl_apply_config();
    }
    supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);

        while (!done)
        {
            int row_width = settings_ui_named_sdl_line_width(2);

        app_ui_scene scene;
        app_ui_panel* panel = settings_browser_scene_begin_ex(&scene,
            "Supporting Pane Layout", "", 1180, 2200);

        if (!panel)
        {
            done = true;
        }
        else
        {
                if (sel > 4)
                    app_ui_panel_set_row_offset(panel, (s16b)(sel - 4));
                for (int i = 0; i < pane_count; i++)
                {
                    int idx = pane_indices[i];
                    settings_sdl_pane_state pane_state;
                    enum pane_type type;
                    enum pane_placement where;
                    int master_idx;
                    bool enabled;
                    bool rows_locked = supporting_pane_rows_locked(pane_indices,
                        pane_count, idx);
                    bool cols_locked = supporting_pane_cols_locked(pane_indices,
                        pane_count, idx);
                    int rows;
                    int cols;
                    byte a;
                    char type_buf[24];
                    char enabled_field[12];
                    char where_field[24];
                    char rows_value[16];
                    char rows_field[20];
                    char cols_value[16];
                    char cols_field[20];
                    char line_buf[128];
                    const char* type_label;
                    const char* where_label;

                    settings_sdl_read_pane_state(idx, &pane_state);
                    type = pane_state.type;
                    where = pane_state.where;
                    enabled = pane_state.enabled;
                    rows = pane_state.rows;
                    cols = pane_state.cols;
                    master_idx = supporting_pane_master_idx(pane_indices,
                        pane_count, where);
                    a = (i == sel) ? TERM_L_BLUE
                        : (enabled ? TERM_WHITE : TERM_SLATE);
                    type_label = settings_ui_pick_label(MAX(8, row_width / 3),
                        pane_type_name(type), pane_type_name(type),
                        pane_type_short_name(type));
                    where_label = settings_ui_pick_label(MAX(4, row_width / 4),
                        pane_placement_name(where), pane_placement_name(where),
                        pane_where_short_name(where));

                    settings_ui_fit_text(type_buf, sizeof(type_buf), type_label,
                        MAX(4, row_width / 3));
                    settings_ui_format_field(enabled_field,
                        sizeof(enabled_field), enabled ? "on" : "off",
                        i == sel && field == 0);
                    settings_ui_format_field(where_field, sizeof(where_field),
                        where_label, i == sel && field == 1);

                    if (rows_locked)
                    {
                        int shared_rows = (master_idx >= 0)
                            ? settings_sdl_pane_rows(master_idx)
                            : rows;
                        settings_ui_format_auto_value(rows_value,
                            sizeof(rows_value), shared_rows, 4);
                    }
                    else
                    {
                        settings_ui_format_auto_value(rows_value,
                            sizeof(rows_value), rows, 4);
                    }
                    settings_ui_format_field(rows_field, sizeof(rows_field),
                        rows_value, !rows_locked && i == sel && field == 2);

                    if (cols_locked)
                    {
                        int shared_cols = (master_idx >= 0)
                            ? settings_sdl_pane_cols(master_idx)
                            : cols;
                        settings_ui_format_auto_value(cols_value,
                            sizeof(cols_value), shared_cols, 4);
                    }
                    else
                    {
                        settings_ui_format_auto_value(cols_value,
                            sizeof(cols_value), cols, 4);
                    }
                    settings_ui_format_field(cols_field, sizeof(cols_field),
                        cols_value, !cols_locked && i == sel && field == 3);

                    strnfmt(line_buf, sizeof(line_buf), "%s %s %s r%s c%s",
                        type_buf, where_field, enabled_field, rows_field,
                        cols_field);
                    if (!settings_browser_add_label_row(panel, (s16b)i, a,
                            true, i == sel, line_buf))
                    {
                        done = true;
                        break;
                    }
                }

                (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
                    "Each side slot shares cols with its first pane.");
                (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
                    "Bottom panes share rows.");
                (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE,
                    true, "8/2", "Move");
                (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE,
                    true, "Space", "Field");
                (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE,
                    true, "4/6", "Set");
                (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE,
                    true, "0", "Auto");
                (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE,
                    true, "Esc", "Back");
                if (!done && !ui_information_scene_present_ui(&scene))
                    done = true;
        }

        inkey_set_cursor_hidden(true);
        char ch = settings_ui_read_legacy_key(false);
        inkey_set_cursor_hidden(false);

        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case ' ':
        case 't':
        case '5':
            field = (field + 1) % 4;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '-':
        case '8':
            sel = (pane_count + sel - 1) % pane_count;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '2':
            sel = (sel + 1) % pane_count;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '0':
        {
            int idx = pane_indices[sel];
            if (field == 0 || field == 1)
            {
                bell("Use 4/6 to toggle enabled or cycle placement");
                break;
            }
            if (field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
            {
                bell("Rows are shared for bottom panes");
                break;
            }
            if (field == 3 && supporting_pane_cols_locked(pane_indices, pane_count, idx))
            {
                bell("Cols are shared within each side slot");
                break;
            }

            if (field == 2)
                settings_sdl_set_pane_rows(idx, 0);
            else
                settings_sdl_set_pane_cols(idx, 0);

            if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
                changed = true;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            changed = true;
            sdl_apply_config();
            break;
        }

        case 'n':
        case '4':
        case 'y':
        case '6':
        {
            int idx = pane_indices[sel];
            int delta = ((ch == 'n') || (ch == '4')) ? -1 : 1;
            enum pane_type type = (enum pane_type)settings_sdl_pane_type(idx);
            enum pane_placement where = (enum pane_placement)settings_sdl_pane_where(idx);

            if (field == 0)
            {
                settings_sdl_set_pane_enabled(idx, (delta > 0));
            }
            else if (field == 1)
            {
                settings_sdl_set_pane_where(idx,
                    pane_next_allowed_placement(type, where, delta));
            }
            else if (field == 2)
            {
                int rows = settings_sdl_pane_rows(idx);

                if (supporting_pane_rows_locked(pane_indices, pane_count, idx))
                {
                    bell("Rows are shared for bottom panes");
                    break;
                }
                if (rows == 0)
                    settings_sdl_set_pane_rows(idx,
                        settings_sdl_pane_current_rows(idx));
                else
                    settings_sdl_set_pane_rows(idx, rows + delta);
            }
            else
            {
                int cols = settings_sdl_pane_cols(idx);

                if (supporting_pane_cols_locked(pane_indices, pane_count, idx))
                {
                    bell("Cols are shared within each side slot");
                    break;
                }
                if (cols == 0)
                    settings_sdl_set_pane_cols(idx,
                        settings_sdl_pane_current_cols(idx));
                else
                    settings_sdl_set_pane_cols(idx, cols + delta);
            }

            if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
                changed = true;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            changed = true;
            sdl_apply_config();
            break;
        }

        default:
            bell("Illegal command for pane layout editor!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;
}

static const int touch_pane_main_action_choices[] = {
    GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', 'E', 't', 'p', 'q',
    'F', 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const int touch_pane_second_action_choices[] = {
    TOUCH_PANE_BIND_INHERIT, GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', 'E', 't', 'p', 'q',
    'F', 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const int* touch_pane_action_choices_for_panel(int panel, int* count)
{
    if (count)
        *count = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
            ? (int)N_ELEMENTS(touch_pane_second_action_choices)
            : (int)N_ELEMENTS(touch_pane_main_action_choices);

    return (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? touch_pane_second_action_choices
        : touch_pane_main_action_choices;
}

static int touch_pane_action_choice_index(int panel, int binding)
{
    int count = 0;
    const int* choices = touch_pane_action_choices_for_panel(panel, &count);

    for (int i = 0; i < count; i++)
    {
        if (choices[i] == binding)
            return i;
    }
    return 0;
}

static void touch_pane_action_label_for_panel(int panel, int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (binding == TOUCH_PANE_BIND_INHERIT) {
        SDL_strlcpy(buf, "Main panel button", buflen);
        return;
    }

    if (binding == GAMEPAD_BIND_SHIFT) {
        char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
        settings_sdl_touch_panel_name((panel == SDL_TOUCH_PANE_PANEL_SECOND)
                ? SDL_TOUCH_PANE_PANEL_MAIN
                : SDL_TOUCH_PANE_PANEL_SECOND,
            panel_name, sizeof(panel_name));
        strnfmt(buf, buflen, "Switch to %s panel", panel_name);
        return;
    }

    binding_action_label(binding, buf, buflen);
}

static const char* settings_sdl_touch_slot_name(int idx)
{
    return get_sdl_touch_pane_slot_name(idx);
}

static void settings_sdl_touch_panel_name(int panel, char* buf, size_t buflen)
{
    get_sdl_touch_pane_panel_name(panel, buf, buflen);
}

static void settings_sdl_touch_button_label(int panel, int slot, char* buf,
    size_t buflen)
{
    get_sdl_touch_pane_button_label_for_panel(panel, slot, buf, buflen);
}

static int settings_sdl_touch_binding(int panel, int slot)
{
    return get_sdl_touch_pane_binding_for_panel(panel, slot);
}

static void settings_sdl_set_touch_binding(int panel, int slot, int binding)
{
    set_sdl_touch_pane_binding_for_panel(panel, slot, binding);
}

static void settings_sdl_set_touch_button_label(int panel, int slot,
    cptr label)
{
    set_sdl_touch_pane_button_label_for_panel(panel, slot, label);
}

static void settings_sdl_set_touch_panel_name(int panel, cptr name)
{
    set_sdl_touch_pane_panel_name(panel, name);
}

static int settings_sdl_touch_default_binding(int panel, int slot)
{
    return get_sdl_touch_pane_default_binding_for_panel(panel, slot);
}

static void do_cmd_touch_pane_button_editor(bool* settings_changed)
{
    int highlight = 0;
    int top = 0;
    int panel = SDL_TOUCH_PANE_PANEL_MAIN;
    bool done = false;
    bool changed = false;
    const int list_start_row = 5;

    while (!done)
    {
        int visible_rows = settings_ui_named_sdl_visible_rows(list_start_row,
            6, 5);

        if (highlight < 0)
            highlight = 0;
        if (highlight >= SDL_TOUCH_PANE_BUTTON_COUNT)
            highlight = SDL_TOUCH_PANE_BUTTON_COUNT - 1;

        if (top > highlight)
            top = highlight;
        if (top + visible_rows <= highlight)
            top = highlight - visible_rows + 1;
        if (top < 0)
            top = 0;

        app_ui_scene scene;
        app_ui_panel* ui_panel = settings_browser_scene_begin_ex(&scene,
            "Touch Settings", "", 1100, 2200);

        if (!ui_panel)
        {
            done = true;
        }
        else
        {
            char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
            char info_buf[96];

            settings_sdl_touch_panel_name(panel, panel_name,
                sizeof(panel_name));
            strnfmt(info_buf, sizeof(info_buf), "Editing %s panel%s",
                panel_name,
                (panel == SDL_TOUCH_PANE_PANEL_SECOND)
                    ? " (empty = main panel)"
                    : "");
            app_ui_panel_set_subtitle(ui_panel, TERM_SLATE, info_buf);
            if (top > 0)
                app_ui_panel_set_row_offset(ui_panel, (s16b)top);

            for (int i = 0; i < SDL_TOUCH_PANE_BUTTON_COUNT; i++)
            {
                char action_buf[80];
                char label_buf[SDL_TOUCH_PANE_LABEL_LEN];
                char left_buf[64];
                byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

                settings_sdl_touch_button_label(panel, i,
                    label_buf, sizeof(label_buf));
                touch_pane_action_label_for_panel(panel,
                    settings_sdl_touch_binding(panel, i),
                    action_buf, sizeof(action_buf));

                if (label_buf[0])
                {
                    strnfmt(left_buf, sizeof(left_buf), "%s %s",
                        settings_sdl_touch_slot_name(i), label_buf);
                }
                else
                {
                    strnfmt(left_buf, sizeof(left_buf), "%s",
                        settings_sdl_touch_slot_name(i));
                }

                if (!settings_browser_add_pair_row(ui_panel, (s16b)i, a,
                        TERM_SLATE, true, i == highlight, left_buf,
                        action_buf))
                {
                    done = true;
                    break;
                }
            }

            (void)app_ui_panel_add_footer_action(ui_panel, 1, TERM_WHITE,
                true, "8/2", "Move");
            (void)app_ui_panel_add_footer_action(ui_panel, 2, TERM_WHITE,
                true, "4/6", "Action");
            (void)app_ui_panel_add_footer_action(ui_panel, 3, TERM_WHITE,
                true, "Tab", "Panel");
            (void)app_ui_panel_add_footer_action(ui_panel, 4, TERM_WHITE,
                true, "l/p", "Rename");
            (void)app_ui_panel_add_footer_action(ui_panel, 5, TERM_WHITE,
                true, "r", "Reset");
            (void)app_ui_panel_add_footer_action(ui_panel, 6, TERM_WHITE,
                true, "R", "Reset all");
            (void)app_ui_panel_add_footer_action(ui_panel, 7, TERM_WHITE,
                true, "Esc", "Back");
            if (!done && !ui_information_scene_present_ui(&scene))
                done = true;
        }

        inkey_set_cursor_hidden(true);
        char ch = settings_ui_read_legacy_key(false);
        inkey_set_cursor_hidden(false);

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case '-':
        case '8':
            highlight = (SDL_TOUCH_PANE_BUTTON_COUNT + highlight - 1) % SDL_TOUCH_PANE_BUTTON_COUNT;
            break;

        case '2':
            highlight = (highlight + 1) % SDL_TOUCH_PANE_BUTTON_COUNT;
            break;

        case 'n':
        case '4':
        {
            int choice_count = 0;
            const int* choices = touch_pane_action_choices_for_panel(panel, &choice_count);
            int idx = touch_pane_action_choice_index(panel, settings_sdl_touch_binding(panel, highlight));
            idx = (choice_count + idx - 1) % choice_count;
            settings_sdl_set_touch_binding(panel, highlight, choices[idx]);
            changed = true;
            break;
        }

        case 'y':
        case '6':
        case ' ':
        case 't':
        case '5':
        {
            int choice_count = 0;
            const int* choices = touch_pane_action_choices_for_panel(panel, &choice_count);
            int idx = touch_pane_action_choice_index(panel, settings_sdl_touch_binding(panel, highlight));
            idx = (idx + 1) % choice_count;
            settings_sdl_set_touch_binding(panel, highlight, choices[idx]);
            changed = true;
            break;
        }

        case 'l':
        case 'L':
        {
            char prompt[96];
            char prompt_long[96];
            char prompt_medium[96];
            char prompt_short[64];
            char current_label[SDL_TOUCH_PANE_LABEL_LEN];
            char new_label[SDL_TOUCH_PANE_LABEL_LEN];
            char current_buf[96];

            settings_sdl_touch_button_label(panel, highlight, current_label, sizeof(current_label));
            strnfmt(prompt_long, sizeof(prompt_long),
                "New label for %s (blank = use key label): ",
                settings_sdl_touch_slot_name(highlight));
            strnfmt(prompt_medium, sizeof(prompt_medium),
                "New label for %s (blank = default): ",
                settings_sdl_touch_slot_name(highlight));
            strnfmt(prompt_short, sizeof(prompt_short), "Label for %s: ",
                settings_sdl_touch_slot_name(highlight));
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(settings_ui_named_sdl_line_width(0),
                    prompt_long, prompt_medium, prompt_short));
            strnfmt(current_buf, sizeof(current_buf), "Current label: %s", current_label);
            new_label[0] = '\0';
            if (settings_ui_prompt_string("Touch Settings", prompt,
                    current_buf, new_label, sizeof(new_label)))
            {
                settings_sdl_set_touch_button_label(panel, highlight,
                    new_label);
                changed = true;
            }
            break;
        }

        case '\t':
            panel = (panel == SDL_TOUCH_PANE_PANEL_MAIN)
                ? SDL_TOUCH_PANE_PANEL_SECOND
                : SDL_TOUCH_PANE_PANEL_MAIN;
            break;

        case 'p':
        case 'P':
        {
            char prompt[96];
            char current_name[SDL_TOUCH_PANE_LABEL_LEN];
            char new_name[SDL_TOUCH_PANE_LABEL_LEN];
            char current_buf[96];

            settings_sdl_touch_panel_name(panel, current_name, sizeof(current_name));
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(settings_ui_named_sdl_line_width(0),
                    "Name for current panel (blank = default): ",
                    "Panel name (blank = default): ",
                    "Panel name: "));
            strnfmt(current_buf, sizeof(current_buf), "Current panel name: %s", current_name);
            new_name[0] = '\0';
            if (settings_ui_prompt_string("Touch Settings", prompt,
                    current_buf, new_name, sizeof(new_name)))
            {
                settings_sdl_set_touch_panel_name(panel, new_name);
                changed = true;
            }
            break;
        }

        case 'r':
            settings_sdl_set_touch_binding(panel, highlight,
                settings_sdl_touch_default_binding(panel, highlight));
            clear_sdl_touch_pane_button_label_for_panel(panel, highlight);
            changed = true;
            break;

        case 'R':
            sdl_touch_pane_reset_bindings_to_default();
            changed = true;
            break;

        default:
            bell("Illegal command for touch settings!");
            break;
        }
    }

    if (changed)
    {
        if (settings_changed)
            *settings_changed = true;
    }
}


void do_cmd_controller_settings(void);

static bool legacy_options_choice_is_disabled(int choice)
{
    return (choice == 6);
}

static int legacy_options_menu(int* highlight)
{
    bool death_view = death_spectator_active();
    const settings_choice_entry entries[] = {
        { 1, 'j', "j) Load a 'Pref' File", false },
        { 2, 'k', "k) Append Options to a 'Pref' File", false },
        { 3, 'l', "l) Set Macros", false },
        { 4, 'm', "m) Set Colours", false },
        { 5, 'n', "n) Write a note", false },
        { 6, 's', "s) Suicide", death_view },
        { 7, 'o', "o) Return to Options", false },
    };

    if (death_view && legacy_options_choice_is_disabled(*highlight))
        *highlight = 7;

    return settings_choice_menu("Legacy Options", entries,
        (int)N_ELEMENTS(entries), highlight, 7);
}

static int macros_menu(int* highlight)
{
    const settings_choice_entry entries[] = {
        { 1, '1', "1) Load a user pref file", false },
#ifdef ALLOW_MACROS
        { 2, '2', "2) Append macros to a file", false },
        { 3, '3', "3) Query a macro", false },
        { 4, '4', "4) Create a macro", false },
        { 5, '5', "5) Remove a macro", false },
        { 6, '6', "6) Append keymaps to a file", false },
        { 7, '7', "7) Query a keymap", false },
        { 8, '8', "8) Create a keymap", false },
        { 9, '9', "9) Remove a keymap", false },
        { 10, '0', "0) Enter a new action", false },
#endif
    };

    return settings_choice_menu("Interact with Macros", entries,
        (int)N_ELEMENTS(entries), highlight, 0);
}

static int visuals_menu(int* highlight)
{
    const settings_choice_entry entries[] = {
        { 1, '1', "1) Load a user pref file", false },
#ifdef ALLOW_VISUALS
        { 2, '2', "2) Dump monster attr/chars", false },
        { 3, '3', "3) Dump object attr/chars", false },
        { 4, '4', "4) Dump feature attr/chars", false },
        { 5, '5', "5) Dump flavor attr/chars", false },
        { 6, '6', "6) Change monster attr/chars", false },
        { 7, '7', "7) Change object attr/chars", false },
        { 8, '8', "8) Change feature attr/chars", false },
        { 9, '9', "9) Change flavor attr/chars", false },
#endif
        { 10, '0', "0) Reset visuals", false },
    };

    return settings_choice_menu("Interact with Visuals", entries,
        (int)N_ELEMENTS(entries), highlight, 0);
}

static int colors_menu(int* highlight)
{
    const settings_choice_entry entries[] = {
        { 1, '1', "1) Load a user pref file", false },
#ifdef ALLOW_COLORS
        { 2, '2', "2) Dump colors", false },
        { 3, '3', "3) Modify colors", false },
#endif
    };

    return settings_choice_menu("Interact with Colors", entries,
        (int)N_ELEMENTS(entries), highlight, 0);
}

static void do_cmd_legacy_options(void)
{
    int choice = 0;
    int highlight = 1;
    bool return_to_options = false;
    char ftmp[80];

    while (!return_to_options)
    {
        choice = legacy_options_menu(&highlight);

        switch (choice)
        {
        case 1:
        {
            do_cmd_pref_file_hack(12);
            break;
        }
        case 2:
        {
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            if (!settings_ui_prompt_string("Legacy Options",
                    "Enter the file name to save options.", "", ftmp,
                    sizeof(ftmp)))
                continue;

            if (option_dump(ftmp))
                msg_print("Failed!");
            else
                msg_print("Done.");

            break;
        }
        case 3:
        {
            do_cmd_macros();
            break;
        }
        case 4:
        {
            do_cmd_colors();
            break;
        }
        case 5:
        {
            do_cmd_note("", p_ptr->depth);
            break;
        }
        case 6:
        {
            do_cmd_suicide();
            return_to_options = true;
            break;
        }
        case 7:
        {
            return_to_options = true;
            break;
        }
        }
    }
}

int options_menu(int* highlight)
{
    bool allow_debug_menu = false;
    settings_choice_entry entries[13];
    int entry_count = 0;
#ifdef SHOW_DEBUG_OPTIONS_MENU
    allow_debug_menu = true;
#endif
    entries[entry_count++] = (settings_choice_entry){ 1, 'a',
        "a) Set Keybinds", false };
    entries[entry_count++] = (settings_choice_entry){ 2, 'b',
        "b) Controller Settings", false };
    entries[entry_count++] = (settings_choice_entry){ 3, 'c',
        "c) Touch Settings", false };
    entries[entry_count++] = (settings_choice_entry){ 4, 'd',
        "d) Pane Settings", false };
    entries[entry_count++] = (settings_choice_entry){ 5, 'e',
        "e) Interface Options", false };
    entries[entry_count++] = (settings_choice_entry){ 6, 'f',
        "f) Efficiency Options", false };
    entries[entry_count++] = (settings_choice_entry){ 7, 'g',
        "g) Visual Options", false };
    entries[entry_count++] = (settings_choice_entry){ 8, 't',
        "t) Text Options", false };
    entries[entry_count++] = (settings_choice_entry){ 9, 'h',
        "h) Gameplay Options", false };
    entries[entry_count++] = (settings_choice_entry){ 10, 'i',
        "i) Sound Options", false };
    entries[entry_count++] = (settings_choice_entry){ 11, 'j',
        "j) Legacy Options", false };
    entries[entry_count++] = (settings_choice_entry){ 12, 'o',
        "o) Return to Game", false };

    if (allow_debug_menu && p_ptr->noscore)
    {
        entries[entry_count++] = (settings_choice_entry){ 13, 'p',
            "p) Debugging Options", false };
    }

    return settings_choice_menu("Options and misc", entries, entry_count,
        highlight, 12);
}

/*
 * Set or unset various options.
 *
 * After using this command, a complete redraw should be performed,
 * in case any visual options have been changed.
 */
void do_cmd_options(void)
{
    int choice = 0;
    int highlight = 1;
    bool return_to_game = false;
    ui_information_scene_scope settings_scope;

    /* Clear any active banner before opening options */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    if (!ui_information_scene_enter(&settings_scope))
        return;

    if (p_ptr && p_ptr->playing)
        sdl_music_play_menu_theme();

    /* Process Events until "Return to Game" is selected */
    while (!return_to_game)
    {
        choice = options_menu(&highlight);

        switch (choice)
        {
        case 1:
        {
            do_cmd_keybinds();
            break;
        }
        case 2:
        {
            do_cmd_controller_settings();
            break;
        }
        case 3:
        {
            do_cmd_touch_pane_button_editor(NULL);
            break;
        }
        case 4:
        {
            do_cmd_pane_settings();
            break;
        }
        case 5:
        {
            do_cmd_options_aux(INTERFACE_PAGE, "Interface Options");
            break;
        }
        case 6:
        {
            do_cmd_options_aux(EFFICIENCY_PAGE, "Efficiency Options");
            break;
        }
        case 7:
        {
            do_cmd_options_aux(VISUAL_PAGE, "Visual Options");
            break;
        }
        case 8:
        {
            do_cmd_options_aux(TEXT_PAGE, "Text Options");
            break;
        }
        case 9:
        {
            do_cmd_options_aux(GAMEPLAY_PAGE, "Gameplay Options");
            break;
        }
        case 10:
        {
            do_cmd_options_aux(SOUND_PAGE, "Sound Options");
            break;
        }
        case 11:
        {
            do_cmd_legacy_options();
            if (p_ptr && (p_ptr->leaving || !p_ptr->playing))
                return_to_game = true;
            break;
        }
        case 12:
        {
            /* Return to Game */
            return_to_game = true;
            break;
        }
        case 13:
        {
            /* Debugging Options (only reachable when p_ptr->noscore) */
            do_cmd_options_aux(DEBUG_PAGE, "Debugging Options");
            break;
        }
        }
    }

    /* Flush messages */
    message_flush();

    ui_information_scene_leave(&settings_scope);
    if (p_ptr && p_ptr->playing)
        sdl_music_stop_main();
}

#ifdef ALLOW_MACROS
/* Forward declaration */
static errr keymap_dump(cptr fname);
#endif

/*
 * Helper to turn a single keycode into printable text for the keybind UI.
 */
static void describe_keycode(byte keycode, char* buf, size_t buflen)
{
    char raw[2];

    if (!buf || !buflen)
        return;

    raw[0] = (char)keycode;
    raw[1] = '\0';

    ascii_to_text(buf, buflen, raw);
}

struct keybind_entry
{
    byte key_code;
    cptr extra_default_keys;
    cptr key_name;
    cptr action;
    bool requires_keymap;
};

static bool key_matches_default(const struct keybind_entry* entry, byte key)
{
    if (key == entry->key_code)
        return true;
    if (entry->extra_default_keys && strchr(entry->extra_default_keys, key))
        return true;
    return false;
}

static bool key_provides_action(int mode, byte key, cptr action, bool requires_keymap)
{
    cptr mapping = keymap_act[mode][key];

    if (requires_keymap)
        return (mapping && streq(mapping, action));

    if (!mapping)
        return true;

    return streq(mapping, action);
}

static bool entry_has_binding(int mode, const struct keybind_entry* entry)
{
    int key;

    if (key_provides_action(mode, entry->key_code, entry->action, entry->requires_keymap))
        return true;

    if (entry->extra_default_keys)
    {
        const char* extra = entry->extra_default_keys;
        while (*extra)
        {
            if (key_provides_action(mode, (byte)*extra, entry->action, false))
                return true;
            extra++;
        }
    }

    for (key = 0; key < 256; key++)
    {
        cptr current = keymap_act[mode][key];

        if (!current || !streq(current, entry->action))
            continue;

        if (key_matches_default(entry, (byte)key))
            continue;

        return true;
    }

    return false;
}

/*
 * Build a comma-separated list of keys that trigger the supplied action.
 */
static void describe_action_bindings(int mode, const struct keybind_entry* entry, char* buf,
    size_t buflen)
{
    int key;
    bool found = false;
    size_t current_len = 0;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!entry->action)
    {
        SDL_strlcpy(buf, "(none)", buflen);
        return;
    }

    if (key_provides_action(mode, entry->key_code, entry->action, entry->requires_keymap))
    {
        char key_label[16];
        describe_keycode(entry->key_code, key_label, sizeof(key_label));
        SDL_strlcpy(buf, key_label, buflen);
        current_len = strlen(buf);
        found = true;
    }

    if (entry->extra_default_keys)
    {
        const char* extra = entry->extra_default_keys;
        while (*extra)
        {
            if (key_provides_action(mode, (byte)*extra, entry->action, false))
            {
                char key_label[16];
                describe_keycode((byte)*extra, key_label, sizeof(key_label));
                if (found)
                    strnfcat(buf, buflen, &current_len, ", %s", key_label);
                else
                {
                    SDL_strlcpy(buf, key_label, buflen);
                    current_len = strlen(buf);
                    found = true;
                }
            }
            extra++;
        }
    }

    for (key = 0; key < 256; key++)
    {
        cptr current = keymap_act[mode][key];

        if (!current || !streq(current, entry->action))
            continue;

        if (key_matches_default(entry, (byte)key))
            continue;

        {
            char key_label[16];
            describe_keycode((byte)key, key_label, sizeof(key_label));
            if (found)
                strnfcat(buf, buflen, &current_len, ", %s", key_label);
            else
            {
                SDL_strlcpy(buf, key_label, buflen);
                current_len = strlen(buf);
                found = true;
            }
        }
    }

    if (!found)
        SDL_strlcpy(buf, "(none)", buflen);
}

/*
 * Remove all key bindings that trigger the specified action.
 */
static void unbind_action(int mode, cptr action)
{
    int key;

    if (!action)
        return;

    for (key = 0; key < 256; key++)
    {
        if (keymap_act[mode][key] && streq(keymap_act[mode][key], action))
        {
            keymap_act[mode][key] = str_free(keymap_act[mode][key]);
        }
    }
}

static bool list_missing_primary_bindings(int mode, const struct keybind_entry* entries,
    int count, char* buffer, size_t buflen)
{
    int i;
    bool ok = true;
    size_t cur = 0;

    if (!buffer || !buflen)
        return true;

    buffer[0] = '\0';

    for (i = 0; i < count; i++)
    {
        if (entry_has_binding(mode, &entries[i]))
            continue;

        if (!ok)
            strnfcat(buffer, buflen, &cur, ", ");
        strnfcat(buffer, buflen, &cur, "%s", entries[i].key_name);
        ok = false;
    }

    return ok;
}

static int keybind_active_mode(void)
{
    if (!hjkl_movement && !angband_keyset)
        return KEYMAP_MODE_SIL;
    if (hjkl_movement && !angband_keyset)
        return KEYMAP_MODE_SIL_HJKL;
    if (!hjkl_movement && angband_keyset)
        return KEYMAP_MODE_ANGBAND;

    return KEYMAP_MODE_ANGBAND_HJKL;
}

static void keybind_adjust_view(int entry_count, int visible_rows, int* highlight,
    int* top)
{
    int max_top;

    if (!highlight || !top)
        return;

    if (entry_count <= 0)
    {
        *highlight = 0;
        *top = 0;
        return;
    }

    if (*highlight < 0)
        *highlight = 0;
    if (*highlight >= entry_count)
        *highlight = entry_count - 1;

    if (*top > *highlight)
        *top = *highlight;
    if ((*top + visible_rows) <= *highlight)
        *top = *highlight - visible_rows + 1;
    if (*top < 0)
        *top = 0;

    max_top = entry_count - visible_rows;
    if (max_top < 0)
        max_top = 0;
    if (*top > max_top)
        *top = max_top;
}

static bool keybind_present_ui_scene(bool showing_primary,
    const struct keybind_entry* keybinds, int entry_count, int mode,
    int highlight, int top, bool dirty, bool compact_width, cptr default_file)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    int i;

    panel = settings_browser_scene_begin_ex(&scene, "Keybind Configuration",
        compact_width ? "8/2 move  Enter bind  Tab switch  Esc return"
                      : "Arrow to navigate, Enter to bind, Tab to switch groups, Escape to return",
        1180, 2200);
    if (!panel)
        return false;

    (void)app_ui_panel_add_tab(panel, 1,
        showing_primary ? TERM_L_BLUE : TERM_SLATE, showing_primary,
        "Primary");
    (void)app_ui_panel_add_tab(panel, 2,
        showing_primary ? TERM_SLATE : TERM_L_BLUE, !showing_primary,
        "Supplementary");

    if (top > 0)
        app_ui_panel_set_row_offset(panel, (s16b)top);

    for (i = 0; i < entry_count; i++)
    {
        char binding_buf[80];
        byte attr = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

        describe_action_bindings(mode, &keybinds[i], binding_buf,
            sizeof(binding_buf));
        if (!settings_browser_add_pair_row(panel, (s16b)i, attr, TERM_SLATE,
                true, i == highlight, keybinds[i].key_name, binding_buf))
        {
            return false;
        }
    }

    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        showing_primary ? "Primary commands: essential gameplay actions."
                        : "Supplementary commands: secondary actions and utilities.");
    if (dirty)
    {
        (void)app_ui_panel_add_body_line(panel, TERM_YELLOW,
            "Unsaved changes.");
    }
    else if (compact_width)
    {
        (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
            "Press 's' to save keybinds.");
    }
    else
    {
        char save_buf[96];

        strnfmt(save_buf, sizeof(save_buf), "Press 's' to save keybinds to %s.",
            default_file);
        (void)app_ui_panel_add_body_line(panel, TERM_SLATE, save_buf);
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true, "8/2",
        "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true, "Tab",
        "Group");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true, "Enter",
        "Bind");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true, "r",
        "Reset");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true, "s",
        "Save");
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true, "Esc",
        "Back");

    if (panel->row_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->focus_id = panel->rows[panel->selected_row].id;
    }

    return ui_information_scene_present_ui(&scene);
}

static bool keybind_present_prompt_scene(bool showing_primary,
    const struct keybind_entry* entry, int mode, cptr prompt)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    char binding_buf[80];

    if (!entry)
        return false;

    panel = settings_browser_scene_begin_ex(&scene, "Keybind Configuration",
        prompt ? prompt : "", 1100, 2200);
    if (!panel)
        return false;

    (void)app_ui_panel_add_tab(panel, 1,
        showing_primary ? TERM_L_BLUE : TERM_SLATE, showing_primary,
        "Primary");
    (void)app_ui_panel_add_tab(panel, 2,
        showing_primary ? TERM_SLATE : TERM_L_BLUE, !showing_primary,
        "Supplementary");

    describe_action_bindings(mode, entry, binding_buf, sizeof(binding_buf));
    (void)settings_browser_add_pair_row(panel, 0, TERM_L_BLUE, TERM_SLATE,
        true, true, entry->key_name, binding_buf);
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Press the new key now. Escape cancels.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true, "Any key",
        "Bind");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true, "Esc",
        "Cancel");

    if (panel->row_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->focus_id = panel->rows[panel->selected_row].id;
    }

    return ui_information_scene_present_ui(&scene);
}

/*
 * Keybind configuration menu
 * Allows rebinding of movement commands for players without a numpad
 */
void do_cmd_keybinds(void)
{
    int mode = keybind_active_mode();
    bool done = false;
    bool dirty = false;
    char ch;
    bool showing_primary = true;
    int highlight_primary = 0;
    int highlight_secondary = 0;
    int top_primary = 0;
    int top_secondary = 0;
    const char* default_file = "user.prf";
    const int list_start_row = 5;
    static const struct keybind_entry primary_keybinds[] = {
        {'1', NULL, "Move SW (numpad 1)", ";1", true},
        {'2', NULL, "Move S (numpad 2)", ";2", true},
        {'3', NULL, "Move SE (numpad 3)", ";3", true},
        {'4', NULL, "Move W (numpad 4)", ";4", true},
        {'6', NULL, "Move E (numpad 6)", ";6", true},
        {'7', NULL, "Move NW (numpad 7)", ";7", true},
        {'8', NULL, "Move N (numpad 8)", ";8", true},
        {'9', NULL, "Move NE (numpad 9)", ";9", true},
        {'z', NULL, "Wait (z / numpad 5)", "z", false},
        {'i', NULL, "Inventory", "i", false},
        {'e', NULL, "Equipment", "e", false},
        {'u', NULL, "Use item", "u", false},
        {'x', NULL, "Examine item", "x", false},
        {'s', NULL, "Sing / change song", "s", false},
        {'S', NULL, "Toggle stealth", "S", false},
        {'h', "H@", "Character sheet (h / H / @)", "h", false},
        {'f', NULL, "Fire (primary quiver)", "f", false},
        {'F', NULL, "Fire (secondary quiver)", "F", false},
        {'l', NULL, "Look around", "l", false},
        {'T', NULL, "Tunnel / dig", "T", false},
        {'b', NULL, "Bash door", "b", false},
    };
    
    static const struct keybind_entry secondary_keybinds[] = {
        {'j', NULL, "Supplies overview", "j", false},
        {'.', NULL, "Run (also shift)", ".", false},
        {'/', NULL, "Alt action (also ctrl)", "/", false},
        {'w', NULL, "Wear / wield equipment", "w", false},
        {'r', NULL, "Remove equipment", "r", false},
        {'d', NULL, "Drop item", "d", false},
        {'k', NULL, "Destroy item", "k", false},
        {'g', NULL, "Pick up items", "g", false},
        {'Z', NULL, "Rest", "Z", false},
        {'o', NULL, "Open door / chest", "o", false},
        {'c', NULL, "Close door", "c", false},
        {'D', NULL, "Disarm trap / chest", "D", false},
        {'X', NULL, "Exchange places", "X", false},
        {'-', NULL, "Fletch arrows", "-", false},
        {'{', NULL, "Inscribe item", "{", false},
        {'a', NULL, "Activate staff", "a", false},
        {'E', NULL, "Eat food", "E", false},
        {'t', NULL, "Throw item", "t", false},
        {'p', NULL, "Blow horn", "p", false},
        {'q', NULL, "Quaff potion", "q", false},
        {'M', NULL, "View map", "M", false},
        {'L', NULL, "Pan", "L", false},
        {'0', NULL, "Smithing screen", "0", false},
        {'<', NULL, "Go upstairs", "<", false},
        {'>', NULL, "Go downstairs", ">", false},
        {'m', NULL, "Main menu", "m", false},
        {'?', NULL, "Help", "?", false},
        {'@', NULL, "Character sheet (alternate)", "@", false},
        {'O', NULL, "Options menu", "O", false},
        {':', NULL, "Take notes", ":", false},
        {'~', NULL, "Knowledge browser", "~", false},
        {'[', NULL, "Monster list", "[", false},
        {']', NULL, "Object list", "]", false},
    };
    int primary_count = (int)N_ELEMENTS(primary_keybinds);
    int secondary_count = (int)N_ELEMENTS(secondary_keybinds);

    while (!done)
    {
        const struct keybind_entry* keybinds;
        int num_keybinds;
        int* highlight_ptr;
        int* top_ptr;
        int highlight;
        int visible_rows;
        bool compact_width;
        int row_width;

        visible_rows = settings_ui_named_sdl_visible_rows(list_start_row, 6,
            5);
        compact_width = settings_ui_named_sdl_cols() < 70;
        row_width = settings_ui_named_sdl_line_width(2);

        if (showing_primary)
        {
            keybinds = primary_keybinds;
            num_keybinds = primary_count;
            highlight_ptr = &highlight_primary;
            top_ptr = &top_primary;
        }
        else
        {
            keybinds = secondary_keybinds;
            num_keybinds = secondary_count;
            highlight_ptr = &highlight_secondary;
            top_ptr = &top_secondary;
        }

        keybind_adjust_view(num_keybinds, visible_rows, highlight_ptr, top_ptr);
        highlight = *highlight_ptr;

        if (!keybind_present_ui_scene(showing_primary, keybinds,
                num_keybinds, mode, highlight, *top_ptr, dirty,
                compact_width, default_file))
        {
            done = true;
            continue;
        }

        ch = settings_ui_read_legacy_key(false);

        if (ch == ESCAPE || ch == 'q' || ch == 'Q')
        {
            char missing[256];
            if (!list_missing_primary_bindings(mode, primary_keybinds, primary_count, missing,
                    sizeof(missing)))
            {
                char prompt[512];
                strnfmt(prompt, sizeof(prompt),
                    "Essential commands are unbound (%s). Exit anyway? ", missing);
                if (!get_check(prompt))
                    continue;
            }
            done = true;
        }
        else if (ch == '\t')
        {
            showing_primary = !showing_primary;
            continue;
        }
        else if (ch == '8')
        {
            /* Move up */
            if (num_keybinds > 0)
            {
                highlight = (highlight + num_keybinds - 1) % num_keybinds;
                *highlight_ptr = highlight;
            }
        }
        else if (ch == '2')
        {
            /* Move down */
            if (num_keybinds > 0)
            {
                highlight = (highlight + 1) % num_keybinds;
                *highlight_ptr = highlight;
            }
        }
        else if (ch == '\r' || ch == '\n' || ch == ' ')
        {
            cptr action = keybinds[highlight].action;
            char bind_key;
            char key_label[32];
            char prompt[80];
            char prompt_long[96];
            char prompt_medium[88];
            char prompt_short[80];

            strnfmt(prompt_long, sizeof(prompt_long),
                "Press key to use for %s (Escape to cancel):",
                keybinds[highlight].key_name);
            strnfmt(prompt_medium, sizeof(prompt_medium),
                "Press key for %s (Escape to cancel):",
                keybinds[highlight].key_name);
            strnfmt(prompt_short, sizeof(prompt_short),
                "Bind %s (Esc cancels):", keybinds[highlight].key_name);
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(row_width, prompt_long, prompt_medium,
                    prompt_short));

            (void)keybind_present_prompt_scene(showing_primary,
                &keybinds[highlight], mode, prompt);

            flush();
            bind_key = settings_ui_read_legacy_key(false);

            if (bind_key != ESCAPE && bind_key != 0)
            {
                byte new_key = (byte)bind_key;

                keymap_act[mode][new_key] = str_free(keymap_act[mode][new_key]);
                keymap_act[mode][new_key] = str_dup(action);
                dirty = true;

                describe_keycode(new_key, key_label, sizeof(key_label));
                msg_format("Key %s now performs %s", key_label, keybinds[highlight].key_name);
                message_flush();
            }
        }
        else if (ch == 'r' || ch == 'R')
        {
            byte target_key = keybinds[highlight].key_code;
            char key_label[32];
            cptr action = keybinds[highlight].action;

            unbind_action(mode, action);
            keymap_act[mode][target_key] = str_free(keymap_act[mode][target_key]);
            if (keybinds[highlight].requires_keymap)
                keymap_act[mode][target_key] = str_dup(action);

            dirty = true;
            describe_keycode(target_key, key_label, sizeof(key_label));
            msg_format("Reset %s to default key %s", keybinds[highlight].key_name, key_label);
            message_flush();
        }
        else if (ch == 's' || ch == 'S')
        {
#ifdef ALLOW_MACROS
            char ftmp[80];

            strnfmt(ftmp, sizeof(ftmp), "%s", default_file);

            if (settings_ui_prompt_string("Keybind Configuration",
                    "Enter the file name to save keybinds.", "", ftmp,
                    sizeof(ftmp)))
            {
                if (keymap_dump(ftmp) == 0)
                {
                    msg_format("Keybinds saved to %s.", ftmp);
                    dirty = false;
                }
                else
                {
                    msg_print("Failed to save keybinds.");
                }
                message_flush();
            }
#else
            msg_print("Saving keybinds is not available in this build.");
            message_flush();
#endif
        }

        *highlight_ptr = highlight;
    }

    if (dirty)
    {
        char prompt[80];
        strnfmt(prompt, sizeof(prompt), "Save keybinds to %s? ", default_file);
        if (get_check(prompt))
        {
            if (keymap_dump(default_file) == 0)
            {
                msg_format("Keybinds saved to %s.", default_file);
                message_flush();
            }
            else
            {
                msg_print("Failed to save keybinds.");
                message_flush();
            }
        }
    }
}

typedef enum controller_entry_type {
    CONTROLLER_ENTRY_TOGGLE = 0,
    CONTROLLER_ENTRY_ACTION,
} controller_entry_type;

typedef enum controller_toggle_id {
    CONTROLLER_TOGGLE_ENABLED = 0,
    CONTROLLER_TOGGLE_AUTO_MODE,
    CONTROLLER_TOGGLE_STEAMDECK_MODE,
    CONTROLLER_TOGGLE_DPAD,
    CONTROLLER_TOGGLE_LEFT_STICK,
} controller_toggle_id;

typedef struct controller_entry {
    controller_entry_type type;
    int id;
    const char* label;
} controller_entry;

static bool controller_toggle_value(int toggle_id)
{
    switch (toggle_id) {
    case CONTROLLER_TOGGLE_ENABLED:
        return get_sdl_gamepad_enabled();
    case CONTROLLER_TOGGLE_AUTO_MODE:
        return get_sdl_gamepad_auto_mode();
    case CONTROLLER_TOGGLE_STEAMDECK_MODE:
        return get_sdl_steamdeck_mode();
    case CONTROLLER_TOGGLE_DPAD:
        return get_sdl_gamepad_use_dpad();
    case CONTROLLER_TOGGLE_LEFT_STICK:
        return get_sdl_gamepad_use_left_stick();
    default:
        return false;
    }
}

static int controller_binding_slot_count(int type)
{
    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        return GAMEPAD_BUTTON_COUNT;
    case GAMEPAD_CAPTURE_TRIGGER:
        return GAMEPAD_TRIGGER_COUNT;
    case GAMEPAD_CAPTURE_LEFT_STICK:
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        return GAMEPAD_STICK_DIR_COUNT;
    case GAMEPAD_CAPTURE_SHOULDER_COMBO:
        return 1;
    default:
        return 0;
    }
}

static int controller_binding_value(int type, int id)
{
    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        return get_sdl_gamepad_button_binding(id);
    case GAMEPAD_CAPTURE_TRIGGER:
        return get_sdl_gamepad_trigger_binding(id);
    case GAMEPAD_CAPTURE_LEFT_STICK:
        return get_sdl_gamepad_left_stick_binding(id);
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        return get_sdl_gamepad_right_stick_binding(id);
    case GAMEPAD_CAPTURE_SHOULDER_COMBO:
        return get_sdl_gamepad_shoulder_combo_binding();
    default:
        return GAMEPAD_BIND_NONE;
    }
}

static void controller_set_binding_value(int type, int id, int binding)
{
    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        set_sdl_gamepad_button_binding(id, binding);
        break;
    case GAMEPAD_CAPTURE_TRIGGER:
        set_sdl_gamepad_trigger_binding(id, binding);
        break;
    case GAMEPAD_CAPTURE_LEFT_STICK:
        set_sdl_gamepad_left_stick_binding(id, binding);
        break;
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        set_sdl_gamepad_right_stick_binding(id, binding);
        break;
    case GAMEPAD_CAPTURE_SHOULDER_COMBO:
        set_sdl_gamepad_shoulder_combo_binding(binding);
        break;
    default:
        break;
    }
}

static int controller_default_binding_value(int type, int id)
{
    switch (type) {
    case GAMEPAD_CAPTURE_BUTTON:
        return get_sdl_gamepad_default_button_binding(id);
    case GAMEPAD_CAPTURE_TRIGGER:
        return get_sdl_gamepad_default_trigger_binding(id);
    case GAMEPAD_CAPTURE_LEFT_STICK:
        return get_sdl_gamepad_default_left_stick_binding(id);
    case GAMEPAD_CAPTURE_RIGHT_STICK:
        return get_sdl_gamepad_default_right_stick_binding(id);
    case GAMEPAD_CAPTURE_SHOULDER_COMBO:
        return get_sdl_gamepad_default_shoulder_combo_binding();
    default:
        return GAMEPAD_BIND_NONE;
    }
}

static const char* controller_gamepad_button_label(int button)
{
    switch (button) {
    case GAMEPAD_BUTTON_SOUTH: return "A (South)";
    case GAMEPAD_BUTTON_EAST: return "B (East)";
    case GAMEPAD_BUTTON_WEST: return "X (West)";
    case GAMEPAD_BUTTON_NORTH: return "Y (North)";
    case GAMEPAD_BUTTON_LEFT_SHOULDER: return "L1 (Left Shoulder)";
    case GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R1 (Right Shoulder)";
    case GAMEPAD_BUTTON_LEFT_PADDLE1: return "L4 (Left Paddle 1)";
    case GAMEPAD_BUTTON_LEFT_PADDLE2: return "L5 (Left Paddle 2)";
    case GAMEPAD_BUTTON_RIGHT_PADDLE1: return "R4 (Right Paddle 1)";
    case GAMEPAD_BUTTON_RIGHT_PADDLE2: return "R5 (Right Paddle 2)";
    case GAMEPAD_BUTTON_START: return "Start (Menu)";
    case GAMEPAD_BUTTON_BACK: return "Back (View)";
    case GAMEPAD_BUTTON_LEFT_STICK: return "Left Stick Click";
    case GAMEPAD_BUTTON_RIGHT_STICK: return "Right Stick Click";
    case GAMEPAD_BUTTON_GUIDE: return "Guide (Steam)";
    case GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad Click";
    case GAMEPAD_BUTTON_DPAD_UP: return "D-pad Up";
    case GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad Down";
    case GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad Left";
    case GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad Right";
    case GAMEPAD_BUTTON_MISC1: return "Misc1";
    case GAMEPAD_BUTTON_MISC2: return "Misc2";
    case GAMEPAD_BUTTON_MISC3: return "Misc3";
    case GAMEPAD_BUTTON_MISC4: return "Misc4";
    case GAMEPAD_BUTTON_MISC5: return "Misc5";
    case GAMEPAD_BUTTON_MISC6: return "Misc6";
    default: return "Unknown Button";
    }
}

static const char* controller_gamepad_trigger_label(int index)
{
    if (index == 0)
        return "L2 (Left Trigger)";
    if (index == 1)
        return "R2 (Right Trigger)";
    return "Unknown Trigger";
}

static const char* controller_gamepad_stick_dir_label(int type, int dir)
{
    const char* stick = (type == GAMEPAD_CAPTURE_RIGHT_STICK) ? "Right Stick" : "Left Stick";
    const char* dir_label = NULL;

    switch (dir) {
    case GAMEPAD_STICK_DIR_UP: dir_label = "Up"; break;
    case GAMEPAD_STICK_DIR_DOWN: dir_label = "Down"; break;
    case GAMEPAD_STICK_DIR_LEFT: dir_label = "Left"; break;
    case GAMEPAD_STICK_DIR_RIGHT: dir_label = "Right"; break;
    default: dir_label = "Unknown"; break;
    }

    return format("%s %s", stick, dir_label);
}

static const char* controller_gamepad_combo_label(void)
{
    return "L1+R1 Combo";
}

static void controller_binding_label(int type, int id, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (type == GAMEPAD_CAPTURE_BUTTON) {
        SDL_strlcpy(buf, controller_gamepad_button_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_TRIGGER) {
        SDL_strlcpy(buf, controller_gamepad_trigger_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_LEFT_STICK || type == GAMEPAD_CAPTURE_RIGHT_STICK) {
        SDL_strlcpy(buf, controller_gamepad_stick_dir_label(type, id), buflen);
    } else if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO) {
        SDL_strlcpy(buf, controller_gamepad_combo_label(), buflen);
    } else {
        SDL_strlcpy(buf, "(unknown)", buflen);
    }
}

static int controller_action_binding_count(int binding, int* out_type, int* out_id)
{
    static const int binding_types[] = {
        GAMEPAD_CAPTURE_BUTTON,
        GAMEPAD_CAPTURE_TRIGGER,
        GAMEPAD_CAPTURE_LEFT_STICK,
        GAMEPAD_CAPTURE_RIGHT_STICK,
        GAMEPAD_CAPTURE_SHOULDER_COMBO
    };
    int count = 0;

    for (int t = 0; t < (int)N_ELEMENTS(binding_types); t++) {
        int type = binding_types[t];
        int slot_count = controller_binding_slot_count(type);

        for (int i = 0; i < slot_count; i++) {
            if (controller_binding_value(type, i) != binding)
                continue;
            if (count == 0 && out_type && out_id) {
                *out_type = type;
                *out_id = i;
            }
            count++;
        }
    }

    return count;
}

static void controller_action_binding_label(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    int type = 0;
    int id = 0;
    int count = controller_action_binding_count(binding, &type, &id);
    if (count <= 0) {
        SDL_strlcpy(buf, "(unbound)", buflen);
    } else if (count == 1) {
        controller_binding_label(type, id, buf, buflen);
    } else {
        SDL_strlcpy(buf, "Multiple", buflen);
    }
}

static bool controller_binding_matches_action(int binding, int type, int id)
{
    return controller_binding_value(type, id) == binding;
}

void controller_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple")) {
        SDL_strlcpy(buf, fallback, buflen);
    }
}

static void controller_entry_value(const controller_entry* entry, char* buf, size_t buflen)
{
    if (!entry || !buf || !buflen)
        return;

    switch (entry->type) {
    case CONTROLLER_ENTRY_TOGGLE:
        if (entry->id < CONTROLLER_TOGGLE_ENABLED
            || entry->id > CONTROLLER_TOGGLE_LEFT_STICK)
        {
            SDL_strlcpy(buf, "(unknown)", buflen);
        }
        else
        {
            SDL_strlcpy(buf, controller_toggle_value(entry->id) ? "On" : "Off",
                buflen);
        }
        break;
    case CONTROLLER_ENTRY_ACTION:
        controller_action_binding_label(entry->id, buf, buflen);
        break;
    default:
        SDL_strlcpy(buf, "(unknown)", buflen);
        break;
    }
}

static void controller_set_toggle(int toggle_id, bool value)
{
    switch (toggle_id) {
    case CONTROLLER_TOGGLE_ENABLED:
        set_sdl_gamepad_enabled(value);
        break;
    case CONTROLLER_TOGGLE_AUTO_MODE:
        set_sdl_gamepad_auto_mode(value);
        break;
    case CONTROLLER_TOGGLE_STEAMDECK_MODE:
        set_sdl_steamdeck_mode(value);
        break;
    case CONTROLLER_TOGGLE_DPAD:
        set_sdl_gamepad_use_dpad(value);
        break;
    case CONTROLLER_TOGGLE_LEFT_STICK:
        set_sdl_gamepad_use_left_stick(value);
        break;
    default:
        break;
    }
}

static void controller_clear_action_bindings(int binding, int skip_type, int skip_id)
{
    static const int binding_types[] = {
        GAMEPAD_CAPTURE_BUTTON,
        GAMEPAD_CAPTURE_TRIGGER,
        GAMEPAD_CAPTURE_LEFT_STICK,
        GAMEPAD_CAPTURE_RIGHT_STICK,
        GAMEPAD_CAPTURE_SHOULDER_COMBO
    };

    if (binding == GAMEPAD_BIND_NONE)
        return;

    for (int t = 0; t < (int)N_ELEMENTS(binding_types); t++) {
        int type = binding_types[t];
        int slot_count = controller_binding_slot_count(type);

        for (int i = 0; i < slot_count; i++) {
            if (controller_binding_value(type, i) != binding)
                continue;
            if (skip_type == type && skip_id == i)
                continue;
            controller_set_binding_value(type, i, GAMEPAD_BIND_NONE);
        }
    }
}

static void controller_assign_action_binding(int binding, int type, int id)
{
    controller_clear_action_bindings(binding, type, id);
    controller_set_binding_value(type, id, binding);
}

static bool controller_action_default_binding(int binding, int* out_type, int* out_id)
{
    static const int binding_types[] = {
        GAMEPAD_CAPTURE_BUTTON,
        GAMEPAD_CAPTURE_TRIGGER,
        GAMEPAD_CAPTURE_LEFT_STICK,
        GAMEPAD_CAPTURE_RIGHT_STICK,
        GAMEPAD_CAPTURE_SHOULDER_COMBO
    };

    for (int t = 0; t < (int)N_ELEMENTS(binding_types); t++) {
        int type = binding_types[t];
        int slot_count = controller_binding_slot_count(type);

        for (int i = 0; i < slot_count; i++) {
            if (controller_default_binding_value(type, i) != binding)
                continue;
            if (out_type)
                *out_type = type;
            if (out_id)
                *out_id = i;
            return true;
        }
    }

    return false;
}

void do_cmd_controller_settings(void)
{
    bool done = false;
    int highlight = 0;
    int top = 0;
    const int list_start_row = 5;

    static const controller_entry entries[] = {
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_ENABLED, "Controller Input" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_AUTO_MODE, "Auto Controller Mode" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_STEAMDECK_MODE, "Steam Deck UI Mode" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_DPAD, "D-pad Movement" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_LEFT_STICK, "Left Stick Movement" },
        { CONTROLLER_ENTRY_ACTION, ' ', "Confirm (Space)" },
        { CONTROLLER_ENTRY_ACTION, '\r', "Enter" },
        { CONTROLLER_ENTRY_ACTION, ESCAPE, "Escape" },
        { CONTROLLER_ENTRY_ACTION, '\t', "Abilities (Tab)" },
        { CONTROLLER_ENTRY_ACTION, 'i', "Inventory" },
        { CONTROLLER_ENTRY_ACTION, 'e', "Equipment" },
        { CONTROLLER_ENTRY_ACTION, 'u', "Use item" },
        { CONTROLLER_ENTRY_ACTION, 'x', "Examine item" },
        { CONTROLLER_ENTRY_ACTION, 's', "Sing / change song" },
        { CONTROLLER_ENTRY_ACTION, 'S', "Toggle stealth" },
        { CONTROLLER_ENTRY_ACTION, 'h', "Character sheet" },
        { CONTROLLER_ENTRY_ACTION, 'f', "Fire (primary)" },
        { CONTROLLER_ENTRY_ACTION, 'F', "Fire (secondary)" },
        { CONTROLLER_ENTRY_ACTION, 'l', "Look around" },
        { CONTROLLER_ENTRY_ACTION, 'T', "Tunnel / dig" },
        { CONTROLLER_ENTRY_ACTION, 'b', "Bash door" },
        { CONTROLLER_ENTRY_ACTION, 'z', "Wait" },
        { CONTROLLER_ENTRY_ACTION, 'j', "Supplies overview" },
        { CONTROLLER_ENTRY_ACTION, '.', "Run" },
        { CONTROLLER_ENTRY_ACTION, '/', "Alt action" },
        { CONTROLLER_ENTRY_ACTION, 'w', "Wear / wield" },
        { CONTROLLER_ENTRY_ACTION, 'r', "Remove equipment" },
        { CONTROLLER_ENTRY_ACTION, 'd', "Drop item" },
        { CONTROLLER_ENTRY_ACTION, 'k', "Destroy item" },
        { CONTROLLER_ENTRY_ACTION, 'g', "Pick up items" },
        { CONTROLLER_ENTRY_ACTION, 'Z', "Rest" },
        { CONTROLLER_ENTRY_ACTION, 'o', "Open door / chest" },
        { CONTROLLER_ENTRY_ACTION, 'c', "Close door" },
        { CONTROLLER_ENTRY_ACTION, 'D', "Disarm trap / chest" },
        { CONTROLLER_ENTRY_ACTION, 'X', "Exchange places" },
        { CONTROLLER_ENTRY_ACTION, '-', "Fletch arrows" },
        { CONTROLLER_ENTRY_ACTION, '{', "Inscribe item" },
        { CONTROLLER_ENTRY_ACTION, 'a', "Activate staff" },
        { CONTROLLER_ENTRY_ACTION, 'E', "Eat food" },
        { CONTROLLER_ENTRY_ACTION, 't', "Throw item" },
        { CONTROLLER_ENTRY_ACTION, 'p', "Blow horn" },
        { CONTROLLER_ENTRY_ACTION, 'q', "Quaff potion" },
        { CONTROLLER_ENTRY_ACTION, 'M', "View map" },
        { CONTROLLER_ENTRY_ACTION, 'L', "Pan view" },
        { CONTROLLER_ENTRY_ACTION, '0', "Smithing screen" },
        { CONTROLLER_ENTRY_ACTION, '<', "Go upstairs" },
        { CONTROLLER_ENTRY_ACTION, '>', "Go downstairs" },
        { CONTROLLER_ENTRY_ACTION, 'm', "Main menu" },
        { CONTROLLER_ENTRY_ACTION, '?', "Help" },
        { CONTROLLER_ENTRY_ACTION, 'O', "Options menu" },
        { CONTROLLER_ENTRY_ACTION, ':', "Take notes" },
        { CONTROLLER_ENTRY_ACTION, '~', "Knowledge browser" },
        { CONTROLLER_ENTRY_ACTION, '[', "Monster list" },
        { CONTROLLER_ENTRY_ACTION, ']', "Object list" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_SHIFT, "Shift modifier" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_CTRL, "Ctrl modifier" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_ALT, "Alt modifier" },
    };

    int entry_count = (int)N_ELEMENTS(entries);

    while (!done) {
        bool steamdeck = steamdeck_controls_active();
        bool compact_width;
        int row_width;

        row_width = settings_ui_named_sdl_line_width(2);
        int visible_rows = settings_ui_named_sdl_visible_rows(list_start_row,
            6, 5);
        compact_width = settings_ui_named_sdl_cols() < 70;

        if (highlight < 0)
            highlight = 0;
        if (highlight >= entry_count)
            highlight = entry_count - 1;

        if (top > highlight)
            top = highlight;
        if (top + visible_rows <= highlight)
            top = highlight - visible_rows + 1;
        if (top < 0)
            top = 0;
        if (entry_count > visible_rows) {
            int max_top = entry_count - visible_rows;
            if (top > max_top)
                top = max_top;
        } else {
            top = 0;
        }

        app_ui_scene scene;
        app_ui_panel* panel = settings_browser_scene_begin_ex(&scene,
            "Controller Settings", "", 1180, 2200);

        if (!panel) {
            done = true;
            continue;
        }

        if (steamdeck) {
            char confirm_label[16];
            char back_label[16];
            char prompt_buf[80];

            controller_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            controller_prompt_label(steamdeck_back_key(), "B",
                back_label, sizeof(back_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                compact_width ? "D-pad %s bind  %s back"
                              : "D-pad navigate  %s bind  %s back",
                confirm_label, back_label);
            app_ui_panel_set_subtitle(panel, TERM_SLATE, prompt_buf);
        } else {
            app_ui_panel_set_subtitle(panel, TERM_SLATE,
                compact_width ? "8/2 move  Enter bind  Esc return"
                              : "Arrow to navigate, Enter to bind, Escape to return");
        }

        if (top > 0)
            app_ui_panel_set_row_offset(panel, (s16b)top);

        for (int i = 0; i < entry_count; i++) {
            char value_buf[64];
            byte attr = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            controller_entry_value(&entries[i], value_buf,
                sizeof(value_buf));
            if (!settings_browser_add_pair_row(panel, (s16b)i, attr,
                    TERM_SLATE, true, i == highlight, entries[i].label,
                    value_buf))
            {
                done = true;
                break;
            }
        }

        if (steamdeck) {
            char reset_label[16];
            char reset_all_label[16];
            char prompt_buf[80];

            controller_prompt_label(steamdeck_alt_action_key(), "X",
                reset_label, sizeof(reset_label));
            controller_prompt_label(steamdeck_secondary_key(), "Y",
                reset_all_label, sizeof(reset_all_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                compact_width ? "[%s] reset  [%s] reset all"
                              : "Reset: [%s] selected, [%s] all",
                reset_label, reset_all_label);
            (void)app_ui_panel_add_body_line(panel, TERM_WHITE, prompt_buf);
        } else {
            (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
                compact_width ? "r: reset selected  R: reset all"
                              : "Press 'r' to reset selected binding, 'R' to reset all bindings");
        }
        (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
            compact_width ? "Saves on exit."
                          : "Changes are saved on exit.");
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Enter", "Bind");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "r", "Reset");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "R", "Reset all");
        (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
            "Esc", "Back");

        if (!done && !ui_information_scene_present_ui(&scene)) {
            done = true;
            continue;
        }

        char ch = settings_ui_read_legacy_key(false);

        if (ch == ESCAPE || ch == 'q' || ch == 'Q' || (steamdeck && ch == steamdeck_back_key())) {
            done = true;
        } else if (ch == '8') {
            highlight = (highlight + entry_count - 1) % entry_count;
        } else if (ch == '2') {
            highlight = (highlight + 1) % entry_count;
        } else if (ch == 'r' || (steamdeck && ch == steamdeck_alt_action_key())) {
            if (entries[highlight].type == CONTROLLER_ENTRY_ACTION) {
                int binding_type = 0;
                int binding_id = 0;
                if (controller_action_default_binding(entries[highlight].id, &binding_type, &binding_id)) {
                    controller_assign_action_binding(entries[highlight].id, binding_type, binding_id);
                    msg_print("Binding reset to default.");
                } else {
                    controller_clear_action_bindings(entries[highlight].id, -1, -1);
                    msg_print("No default binding for action.");
                }
                message_flush();
            }
        } else if (ch == 'R' || (steamdeck && ch == steamdeck_secondary_key())) {
            sdl_gamepad_reset_bindings_to_default();
            msg_print("All bindings reset to defaults.");
            message_flush();
        } else if (ch == '\r' || ch == '\n' || ch == ' ') {
            const controller_entry* entry = &entries[highlight];

            if (entry->type == CONTROLLER_ENTRY_TOGGLE) {
                char cur[16];
                controller_entry_value(entry, cur, sizeof(cur));
                controller_set_toggle(entry->id, streq(cur, "Off"));
            } else {
                char prompt[80];
                char prompt_long[96];
                char prompt_medium[80];
                char prompt_short[64];
                int cap_type = 0;
                int cap_id = 0;
                if (steamdeck) {
                    char cancel_label[16];
                    controller_prompt_label(steamdeck_back_key(), "B", cancel_label, sizeof(cancel_label));
                    strnfmt(prompt_long, sizeof(prompt_long),
                        "Press controller button for %s  (%s=cancel)",
                        entry->label, cancel_label);
                    strnfmt(prompt_medium, sizeof(prompt_medium),
                        "Press button for %s  (%s=cancel)",
                        entry->label, cancel_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "Bind %s  (%s cancel)", entry->label, cancel_label);
                } else {
                    strnfmt(prompt_long, sizeof(prompt_long),
                        "Press controller button for %s (Esc=cancel, Backspace=clear)",
                        entry->label);
                    strnfmt(prompt_medium, sizeof(prompt_medium),
                        "Bind %s (Esc=cancel, Bksp=clear)", entry->label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "%s (Esc cancel, Bksp clear)", entry->label);
                }
                strnfmt(prompt, sizeof(prompt), "%s",
                    settings_ui_pick_label(row_width, prompt_long, prompt_medium,
                        prompt_short));
                {
                    app_ui_scene prompt_scene;
                    app_ui_panel* prompt_panel = settings_browser_scene_begin_ex(
                        &prompt_scene, "Controller Settings", prompt, 1100,
                        2200);

                    if (prompt_panel) {
                        char current_value[64];

                        controller_entry_value(entry, current_value,
                            sizeof(current_value));
                        (void)settings_browser_add_pair_row(prompt_panel, 0,
                            TERM_L_BLUE, TERM_SLATE, true, true, entry->label,
                            current_value);
                        (void)app_ui_panel_add_body_line(prompt_panel,
                            TERM_SLATE, steamdeck
                                ? "Press the controller input now."
                                : "Esc cancels. Backspace clears.");
                        (void)ui_information_scene_present_ui(&prompt_scene);
                    }
                }

                flush();
                if (!sdl_gamepad_capture_begin()) {
                    msg_print("No controller detected.");
                    message_flush();
                    continue;
                }

                bool waiting = true;
                while (waiting) {
                    if (sdl_gamepad_capture_poll(&cap_type, &cap_id)) {
                        if (controller_binding_matches_action(ESCAPE, cap_type, cap_id)) {
                            sdl_gamepad_capture_cancel();
                            waiting = false;
                            break;
                        }
                        controller_assign_action_binding(entry->id, cap_type, cap_id);
                        waiting = false;
                        break;
                    }

                    char choice = settings_ui_read_legacy_key(true);
                    if (choice == ESCAPE) {
                        sdl_gamepad_capture_cancel();
                        waiting = false;
                    } else if (choice == '\b' || choice == 127) {
                        sdl_gamepad_capture_cancel();
                        controller_clear_action_bindings(entry->id, -1, -1);
                        waiting = false;
                    } else if (choice == 0) {
                        platform_delay_ms(10);
                    }
                }
            }
        }
    }
}

#ifdef ALLOW_MACROS

/*
 * Hack -- append all current macros to the given file
 */
static errr macro_dump(cptr fname)
{
    static cptr mark = "Macro Dump";

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("macro_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old macros */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = sdl_fopen(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    /* Start dumping */
    SDL_IOprintf(fff, "# Automatic macro dump\n\n");

    /* Dump them */
    for (i = 0; i < macro__num; i++)
    {
        /* Start the macro */
        SDL_IOprintf(fff, "# Macro '%d'\n\n", i);

        /* Extract the macro action */
        ascii_to_text(buf, sizeof(buf), macro__act[i]);

        /* Dump the macro action */
        SDL_IOprintf(fff, "A:%s\n", buf);

        /* Extract the macro pattern */
        ascii_to_text(buf, sizeof(buf), macro__pat[i]);

        /* Dump the macro pattern */
        SDL_IOprintf(fff, "P:%s\n", buf);

        /* End the macro */
        SDL_IOprintf(fff, "\n\n");
    }

    /* Start dumping */
    SDL_IOprintf(fff, "\n\n\n\n");

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

/*
 * Hack -- ask for a "trigger" (see below)
 *
 * Note that both "flush()" calls are extremely important.  This may
 * no longer be true, since input handling is much simpler now.  XXX XXX XXX
 */
static void do_cmd_macro_aux(char* buf)
{
    char ch;

    int n = 0;

    /* Flush */
    flush();

    /* First key */
    ch = settings_ui_read_legacy_key(false);

    /* Read the pattern */
    while (ch != '\0')
    {
        /* Save the key */
        buf[n++] = ch;

        /* Attempt to read a key */
        ch = settings_ui_read_legacy_key(true);
    }

    /* Terminate */
    buf[n] = '\0';

    /* Flush */
    flush();
}

/*
 * Hack -- ask for a keymap "trigger" (see below)
 *
 * Note that both "flush()" calls are extremely important.  This may
 * no longer be true, since "util.c" is much simpler now.  XXX XXX XXX
 */
static void do_cmd_macro_aux_keymap(char* buf)
{
    /* Flush */
    flush();

    /* Get a key */
    buf[0] = settings_ui_read_legacy_key(false);
    buf[1] = '\0';

    /* Flush */
    flush();
}

/*
 * Hack -- Append all keymaps to the given file.
 *
 * Hack -- We only append the keymaps for the "active" mode.
 */
static errr keymap_dump(cptr fname)
{
    static cptr mark = "Keymap Dump";

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    int mode;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("keymap_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old keymaps */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = sdl_fopen(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    /* Start dumping */
    SDL_IOprintf(fff, "# Automatic keymap dump\n\n");

    /* Dump them */
    for (i = 0; i < (int)N_ELEMENTS(keymap_act[mode]); i++)
    {
        char key[2] = "?";

        cptr act;

        /* Loop up the keymap */
        act = keymap_act[mode][i];

        /* Skip empty keymaps */
        if (!act)
            continue;

        /* Encode the action */
        ascii_to_text(buf, sizeof(buf), act);

        /* Dump the keymap action */
        SDL_IOprintf(fff, "A:%s\n", buf);

        /* Convert the key into a string */
        key[0] = i;

        /* Encode the key */
        ascii_to_text(buf, sizeof(buf), key);

        /* Dump the keymap pattern */
        SDL_IOprintf(fff, "C:%d:%s\n", mode, buf);

        /* Skip a line */
        SDL_IOprintf(fff, "\n");
    }

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n\n");

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

#endif

/*
 * Interact with "macros"
 *
 * Could use some helpful instructions on this page.  XXX XXX XXX
 */
void do_cmd_macros(void)
{
    int choice = 0;
    int highlight = 1;
    char tmp[1024];
    char pat[1024];
    char key_desc[128];
    int mode;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Clear any active banner before opening macros menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Process requests until done */
    while (1)
    {
        choice = macros_menu(&highlight);
        if (choice == 0)
            break;

        /* Load a user pref file */
        if (choice == 1)
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(0);
        }

#ifdef ALLOW_MACROS

        /* Save macros */
        else if (choice == 2)
        {
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Ask for a file */
            if (!settings_ui_prompt_string("Interact with Macros",
                    "Enter the file name to append macros to.", "", ftmp,
                    sizeof(ftmp)))
                continue;

            /* Dump the macros */
            (void)macro_dump(ftmp);

            /* Prompt */
            msg_print("Appended macros.");
        }

        /* Query a macro */
        else if (choice == 3)
        {
            int k;

            /* Get a macro trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the macro trigger to query.", "", false, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Get the action */
            k = macro_find_exact(pat);

            /* Nothing found */
            if (k < 0)
            {
                /* Prompt */
                msg_print("Found no macro.");
            }

            /* Found one */
            else
            {
                /* Obtain the action */
                SDL_strlcpy(macro_buffer, macro__act[k], sizeof(macro_buffer));

                /* Analyze the current action */
                ascii_to_text(tmp, sizeof(tmp), macro_buffer);
                settings_ui_show_text_value("Interact with Macros",
                    "Found a macro.", key_desc, "Action", tmp);
            }
        }

        /* Create a macro */
        else if (choice == 4)
        {
            /* Get a macro trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the macro trigger to create.", "", false, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Convert to text */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (settings_ui_prompt_string("Interact with Macros",
                    "Enter the encoded action text for this macro.", key_desc,
                    tmp, 80))
            {
                /* Convert to ascii */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

                /* Link the macro */
                macro_add(pat, macro_buffer);

                /* Prompt */
                msg_print("Added a macro.");
            }
        }

        /* Remove a macro */
        else if (choice == 5)
        {
            /* Get a macro trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the macro trigger to remove.", "", false, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Link the macro */
            macro_add(pat, pat);

            /* Prompt */
            msg_print("Removed a macro.");
        }

        /* Save keymaps */
        else if (choice == 6)
        {
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Ask for a file */
            if (!settings_ui_prompt_string("Interact with Macros",
                    "Enter the file name to append keymaps to.", "", ftmp,
                    sizeof(ftmp)))
                continue;

            /* Dump the macros */
            (void)keymap_dump(ftmp);

            /* Prompt */
            msg_print("Appended keymaps.");
        }

        /* Query a keymap */
        else if (choice == 7)
        {
            cptr act;

            /* Get a keymap trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the keypress to query.", "", true, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Look up the keymap */
            act = keymap_act[mode][(byte)(pat[0])];

            /* Nothing found */
            if (!act)
            {
                /* Prompt */
                msg_print("Found no keymap.");
            }

            /* Found one */
            else
            {
                /* Obtain the action */
                SDL_strlcpy(macro_buffer, act, sizeof(macro_buffer));

                /* Analyze the current action */
                ascii_to_text(tmp, sizeof(tmp), macro_buffer);
                settings_ui_show_text_value("Interact with Macros",
                    "Found a keymap.", key_desc, "Action", tmp);
            }
        }

        /* Create a keymap */
        else if (choice == 8)
        {
            /* Get a keymap trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the keypress to create.", "", true, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Convert to text */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (settings_ui_prompt_string("Interact with Macros",
                    "Enter the encoded action text for this keymap.", key_desc,
                    tmp, 80))
            {
                /* Convert to ascii */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

                /* Free old keymap */
                str_free(keymap_act[mode][(byte)(pat[0])]);

                /* Make new keymap */
                keymap_act[mode][(byte)(pat[0])] = str_dup(macro_buffer);

                /* Prompt */
                msg_print("Added a keymap.");
            }
        }

        /* Remove a keymap */
        else if (choice == 9)
        {
            /* Get a keymap trigger */
            if (!settings_ui_capture_macro_input("Interact with Macros",
                    "Press the keypress to remove.", "", true, pat,
                    sizeof(pat), key_desc, sizeof(key_desc)))
            {
                continue;
            }

            /* Free old keymap */
            str_free(keymap_act[mode][(byte)(pat[0])]);

            /* Make new keymap */
            keymap_act[mode][(byte)(pat[0])] = NULL;

            /* Prompt */
            msg_print("Removed a keymap.");
        }

        /* Enter a new action */
        else if (choice == 10)
        {
            /* Analyze the current action */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (settings_ui_prompt_string("Interact with Macros",
                    "Enter the encoded action text.", "", tmp, 80))
            {
                /* Extract an action */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);
            }
        }

#endif /* ALLOW_MACROS */

        /* Oops */
        else
        {
            /* Oops */
            bell("Illegal command for macros!");
        }

        /* Flush messages */
        message_flush();
    }
}

/*
 * Settings editor helper: add a browser row with a sample icon.
 */
static bool settings_browser_add_icon_pair_row(app_ui_panel* panel, s16b id,
    byte attr, byte meta_attr, byte icon_attr, char icon_char, bool enabled,
    bool selected, cptr label, cptr meta)
{
    return app_ui_panel_add_row_ex(panel, id, attr, meta_attr, icon_attr,
        icon_char, enabled, selected, "", label ? label : "",
        meta ? meta : "");
}

static void settings_format_attr_char_pair(char* buf, size_t buflen, byte attr,
    byte ch)
{
    strnfmt(buf, buflen, "%3u / %3u", attr, ch);
}

static bool settings_visual_present_ui_scene(cptr title, cptr kind_label,
    int index, cptr name, byte default_attr, byte default_char,
    byte current_attr, byte current_char)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    char label_buf[APP_UI_LABEL_MAX];
    char meta_buf[APP_UI_META_MAX];

    panel = settings_browser_scene_begin_ex(&scene, title,
        "n/N move  a/A attr  c/C char  s shade  Esc back", 980, 2048);
    if (!panel)
        return false;

    strnfmt(label_buf, sizeof(label_buf), "%s %d", kind_label, index);
    strnfmt(meta_buf, sizeof(meta_buf), "%s", name ? name : "(unnamed)");
    if (!settings_browser_add_icon_pair_row(panel, 0, TERM_L_BLUE, TERM_SLATE,
            current_attr, (char)current_char, true, true, label_buf, meta_buf))
    {
        return false;
    }

    settings_format_attr_char_pair(meta_buf, sizeof(meta_buf), default_attr,
        default_char);
    if (!settings_browser_add_icon_pair_row(panel, 1, TERM_WHITE, TERM_SLATE,
            default_attr, (char)default_char, true, false,
            "Default attr/char", meta_buf))
    {
        return false;
    }

    settings_format_attr_char_pair(meta_buf, sizeof(meta_buf), current_attr,
        current_char);
    if (!settings_browser_add_icon_pair_row(panel, 2, TERM_WHITE, TERM_SLATE,
            current_attr, (char)current_char, true, false,
            "Current attr/char", meta_buf))
    {
        return false;
    }

    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "The sample icon previews the current glyph.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true, "n/N",
        "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true, "a/A",
        "Attr");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true, "c/C",
        "Char");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true, "s",
        "Shade");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true, "Esc",
        "Back");
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->focus_id = 0;
    return ui_information_scene_present_ui(&scene);
}

static bool settings_color_is_black(byte attr)
{
    return ((int)angband_color_table[attr][1]
            + (int)angband_color_table[attr][2]
            + (int)angband_color_table[attr][3])
        == 0;
}

static byte settings_color_sample_attr(byte attr)
{
    return settings_color_is_black(attr) ? TERM_WHITE : attr;
}

static char settings_color_sample_char(byte attr)
{
    return settings_color_is_black(attr) ? '.' : '#';
}

static bool settings_color_picker_present_ui_scene(cptr title, cptr subtitle,
    byte attr, cptr label)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    char meta_buf[APP_UI_META_MAX];

    panel = settings_browser_scene_begin_ex(&scene, title, subtitle, 960, 1800);
    if (!panel)
        return false;

    strnfmt(meta_buf, sizeof(meta_buf), "attr %d", attr);
    if (!settings_browser_add_icon_pair_row(panel, 0, TERM_L_BLUE, TERM_SLATE,
            settings_color_sample_attr(attr), settings_color_sample_char(attr),
            true, true, label ? label : "", meta_buf))
    {
        return false;
    }

    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "4/6 changes the value. Enter accepts. Escape cancels.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true, "4/6",
        "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true, "Enter",
        "Accept");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true, "Esc",
        "Cancel");
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->focus_id = 0;
    return ui_information_scene_present_ui(&scene);
}

static void settings_color_editor_adjust_view(int total_rows, int visible_rows,
    int current, int* scroll)
{
    int max_scroll;

    if (!scroll)
        return;

    if (visible_rows < 1)
        visible_rows = 1;

    if (current < *scroll)
        *scroll = current;
    else if (current >= *scroll + visible_rows)
        *scroll = current - visible_rows + 1;

    max_scroll = total_rows - visible_rows;
    if (max_scroll < 0)
        max_scroll = 0;
    if (*scroll > max_scroll)
        *scroll = max_scroll;
    if (*scroll < 0)
        *scroll = 0;
}

static void settings_color_format_label(char* buf, size_t buflen, int idx)
{
    strnfmt(buf, buflen, "%2d  %s", idx, get_ext_color_name(idx));
}

static void settings_color_format_meta(char* buf, size_t buflen, int idx)
{
    strnfmt(buf, buflen, "K %3d  RGB %3d,%3d,%3d",
        angband_color_table[idx][0], angband_color_table[idx][1],
        angband_color_table[idx][2], angband_color_table[idx][3]);
}

static bool settings_color_editor_present_ui_scene(int idx, int scroll)
{
    app_ui_scene scene;
    app_ui_panel* panel;

    panel = settings_browser_scene_begin_ex(&scene, "Modify Colors",
        "8/2 move  4/6 jump  k/r/g/b adjust  c copy  v exact  Esc back",
        1100, 2200);
    if (!panel)
        return false;

    if (scroll > 0)
        app_ui_panel_set_row_offset(panel, (s16b)scroll);

    for (int i = 0; i < MAX_COLORS; i++)
    {
        char label_buf[APP_UI_LABEL_MAX];
        char meta_buf[APP_UI_META_MAX];
        byte row_attr = (i == idx) ? TERM_L_BLUE : TERM_WHITE;

        settings_color_format_label(label_buf, sizeof(label_buf), i);
        settings_color_format_meta(meta_buf, sizeof(meta_buf), i);
        if (!settings_browser_add_icon_pair_row(panel, (s16b)i, row_attr,
                TERM_SLATE, settings_color_sample_attr((byte)i),
                settings_color_sample_char((byte)i), true, i == idx,
                label_buf, meta_buf))
        {
            return false;
        }
    }

    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Use k/K, r/R, g/G, and b/B to adjust values.");
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Use c to copy from another color or v to enter exact values.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true, "8/2",
        "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true, "4/6",
        "Jump");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true, "k/r/g/b",
        "Adjust");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true, "c",
        "Copy");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true, "v",
        "Exact");
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true, "Esc",
        "Back");
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->focus_id = (s16b)idx;
    return ui_information_scene_present_ui(&scene);
}

/*
 * Asks to the player for an extended color. It is done in two steps:
 * 1. Asks for the base color.
 * 2. Asks for a specific shade.
 * If the user press ESCAPE no changes are made to attr.
 */
static void askfor_shade(byte* attr, int y)
{
    byte base, shade, temp;
    bool changed = false;
    int ch;

    (void)y;

    /* Start with the given base color */
    base = GET_BASE_COLOR(*attr);

    /* 1. Query for base color */
    while (1)
    {
        if (!settings_color_picker_present_ui_scene("Shade Picker",
                "1/2 Choose base color", base, color_names[base]))
        {
            return;
        }
        ch = settings_ui_read_legacy_key(false);

        /* Cancel */
        if (ch == ESCAPE)
        {
            return;
        }

        /* Accept the current base color */
        if ((ch == '\r') || (ch == '\n'))
            break;

        /* Move to the previous color if possible */
        if ((ch == '4') && (base > 0))
        {
            --base;
            /* Reset the shade, see below */
            changed = true;
            continue;
        }

        /* Move to the next color if possible */
        if ((ch == '6') && (base < MAX_BASE_COLORS - 1))
        {
            ++base;
            /* Reset the shade, see below */
            changed = true;
            continue;
        }
    }

    /* The player selected a different base color, start from shade 0 */
    if (changed)
        shade = 0;
    /* We assume that the player is editing the current shade, go there */
    else
        shade = GET_SHADE(*attr);

    /* 2. Query for specific shade */
    while (1)
    {
        /* Create the real color */
        temp = MAKE_EXTENDED_COLOR(base, shade);
        if (!settings_color_picker_present_ui_scene("Shade Picker",
                "2/2 Choose shade", temp, get_ext_color_name(temp)))
        {
            return;
        }
        ch = settings_ui_read_legacy_key(false);

        /* Cancel */
        if (ch == ESCAPE)
            return;

        /* Accept the current shade */
        if ((ch == '\r') || (ch == '\n'))
            break;

        /* Move to the previous shade if possible */
        if ((ch == '4') && (shade > 0))
        {
            --shade;
            continue;
        }

        /* Move to the next shade if possible */
        if ((ch == '6') && (shade < MAX_SHADES - 1))
        {
            ++shade;
            continue;
        }
    }

    /* Assign the selected shade */
    *attr = temp;
}

/*
 * Interact with "visuals"
 */
void do_cmd_visuals(void)
{
    int choice = 0;
    int highlight = 1;
    int i;
    SDL_IOStream* fff;
    char buf[1024];

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Interact until done */
    while (1)
    {
        choice = visuals_menu(&highlight);
        if (choice == 0)
            break;

        /* Load a user pref file */
        if (choice == 1)
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(15);
        }

#ifdef ALLOW_VISUALS

        /* Dump monster attr/chars */
        else if (choice == 2)
        {
            static cptr mark = "Monster attr/chars";
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!settings_ui_prompt_string("Interact with Visuals",
                    "Enter the file name for the monster visual dump.", "",
                    ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_monsters: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Monster attr/char definitions\n\n");

            /* Dump monsters */
            for (i = 0; i < z_info->r_max; i++)
            {
                monster_race* r_ptr = &r_info[i];

                /* Skip non-entries */
                if (!r_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (r_name + r_ptr->name));

                /* Dump the monster attr/char info */
                dump_visual_pair(fff, "R", i, r_ptr->x_attr, (byte)r_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped monster attr/chars.");
        }

        /* Dump object attr/chars */
        else if (choice == 3)
        {
            static cptr mark = "Object attr/chars";
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!settings_ui_prompt_string("Interact with Visuals",
                    "Enter the file name for the object visual dump.", "",
                    ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_objects: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Object attr/char definitions\n\n");

            /* Dump objects */
            for (i = 0; i < z_info->k_max; i++)
            {
                object_kind* k_ptr = &k_info[i];

                /* Skip non-entries */
                if (!k_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (k_name + k_ptr->name));

                /* Dump the object attr/char info */
                dump_visual_pair(
                    fff, "K", i, k_ptr->x_attr, (byte)k_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped object attr/chars.");
        }

        /* Dump feature attr/chars */
        else if (choice == 4)
        {
            static cptr mark = "Feature attr/chars";
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!settings_ui_prompt_string("Interact with Visuals",
                    "Enter the file name for the feature visual dump.", "",
                    ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_features: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Feature attr/char definitions\n\n");

            /* Dump features */
            for (i = 0; i < z_info->f_max; i++)
            {
                feature_type* f_ptr = &f_info[i];

                /* Skip non-entries */
                if (!f_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (f_name + f_ptr->name));

                /* Dump the feature attr/char info */
                dump_visual_pair(
                    fff, "F", i, f_ptr->x_attr, (byte)f_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped feature attr/chars.");
        }

        /* Dump flavor attr/chars */
        else if (choice == 5)
        {
            static cptr mark = "Flavor attr/chars";
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!settings_ui_prompt_string("Interact with Visuals",
                    "Enter the file name for the flavor visual dump.", "",
                    ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_flavors: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Flavor attr/char definitions\n\n");

            /* Dump flavors */
            for (i = 0; i < z_info->flavor_max; i++)
            {
                flavor_type* flavor_ptr = &flavor_info[i];

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (flavor_text + flavor_ptr->text));

                /* Dump the flavor attr/char info */
                dump_visual_pair(
                    fff, "L", i, flavor_ptr->x_attr, (byte)flavor_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped flavor attr/chars.");
        }

        /* Modify monster attr/chars */
        else if (choice == 6)
        {
            static int r = 0;

            while (1)
            {
                monster_race* r_ptr = &r_info[r];
                byte da = (byte)r_ptr->d_attr;
                byte dc = (byte)r_ptr->d_char;
                byte ca = (byte)r_ptr->x_attr;
                byte cc = (byte)r_ptr->x_char;
                int cx;

                if (!settings_visual_present_ui_scene(
                        "Monster Attr/Char", "Monster", r,
                        r_name + r_ptr->name, da, dc, ca, cc))
                {
                    break;
                }
                cx = settings_ui_read_legacy_key(false);

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    r = (r + z_info->r_max + 1) % z_info->r_max;
                if (cx == 'N')
                    r = (r + z_info->r_max - 1) % z_info->r_max;
                if (cx == 'a')
                    r_ptr->x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    r_ptr->x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    r_ptr->x_char = (byte)(cc + 1);
                if (cx == 'C')
                    r_ptr->x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&r_ptr->x_attr, 22);
                }
            }
        }

        /* Modify object attr/chars */
        else if (choice == 7)
        {
            static int k = 0;

            while (1)
            {
                object_kind* k_ptr = &k_info[k];
                byte da = (byte)k_ptr->d_attr;
                byte dc = (byte)k_ptr->d_char;
                byte ca = (byte)k_ptr->x_attr;
                byte cc = (byte)k_ptr->x_char;
                int cx;

                if (!settings_visual_present_ui_scene(
                        "Object Attr/Char", "Object", k,
                        k_name + k_ptr->name, da, dc, ca, cc))
                {
                    break;
                }
                cx = settings_ui_read_legacy_key(false);

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    k = (k + z_info->k_max + 1) % z_info->k_max;
                if (cx == 'N')
                    k = (k + z_info->k_max - 1) % z_info->k_max;
                if (cx == 'a')
                    k_info[k].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    k_info[k].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    k_info[k].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    k_info[k].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&k_info[k].x_attr, 22);
                }
            }
        }

        /* Modify feature attr/chars */
        else if (choice == 8)
        {
            static int f = 0;

            while (1)
            {
                feature_type* f_ptr = &f_info[f];
                byte da = (byte)f_ptr->d_attr;
                byte dc = (byte)f_ptr->d_char;
                byte ca = (byte)f_ptr->x_attr;
                byte cc = (byte)f_ptr->x_char;
                int cx;

                if (!settings_visual_present_ui_scene(
                        "Feature Attr/Char", "Terrain", f,
                        f_name + f_ptr->name, da, dc, ca, cc))
                {
                    break;
                }
                cx = settings_ui_read_legacy_key(false);

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    f = (f + z_info->f_max + 1) % z_info->f_max;
                if (cx == 'N')
                    f = (f + z_info->f_max - 1) % z_info->f_max;
                if (cx == 'a')
                    f_info[f].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    f_info[f].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    f_info[f].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    f_info[f].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&f_info[f].x_attr, 22);
                }
            }
        }

        /* Modify flavor attr/chars */
        else if (choice == 9)
        {
            static int f = 0;

            while (1)
            {
                flavor_type* flavor_ptr = &flavor_info[f];
                byte da = (byte)flavor_ptr->d_attr;
                byte dc = (byte)flavor_ptr->d_char;
                byte ca = (byte)flavor_ptr->x_attr;
                byte cc = (byte)flavor_ptr->x_char;
                int cx;

                if (!settings_visual_present_ui_scene(
                        "Flavor Attr/Char", "Flavor", f,
                        flavor_text + flavor_ptr->text, da, dc, ca, cc))
                {
                    break;
                }
                cx = settings_ui_read_legacy_key(false);

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    f = (f + z_info->flavor_max + 1) % z_info->flavor_max;
                if (cx == 'N')
                    f = (f + z_info->flavor_max - 1) % z_info->flavor_max;
                if (cx == 'a')
                    flavor_info[f].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    flavor_info[f].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    flavor_info[f].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    flavor_info[f].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&flavor_info[f].x_attr, 22);
                }
            }
        }

#endif /* ALLOW_VISUALS */

        /* Reset visuals */
        else if (choice == 10)
        {
            /* Reset */
            reset_visuals(true);

            /* Message */
            msg_print("Visual attr/char tables reset.");
        }

        /* Unknown option */
        else
        {
            bell("Illegal command for visuals!");
        }

        /* Flush messages */
        message_flush();
    }
}

/*
 * Asks to the user for specific color values.
 * Returns true if the color was modified.
 */
static bool askfor_color_values(int idx)
{
    char str[10];

    int k, r, g, b;

    /* Get the default value */
    strnfmt(str, sizeof(str), "%d", angband_color_table[idx][1]);

    /* Query, check for ESCAPE */
    if (!settings_ui_prompt_string("Modify Colors",
            "Enter red (0-255).", "Current channel value.", str,
            sizeof(str)))
        return false;

    /* Convert to number */
    r = atoi(str);

    /* Check bounds */
    if (r < 0)
        r = 0;
    if (r > 255)
        r = 255;

    /* Get the default value */
    strnfmt(str, sizeof(str), "%d", angband_color_table[idx][2]);

    /* Query, check for ESCAPE */
    if (!settings_ui_prompt_string("Modify Colors",
            "Enter green (0-255).", "Current channel value.", str,
            sizeof(str)))
        return false;

    /* Convert to number */
    g = atoi(str);

    /* Check bounds */
    if (g < 0)
        g = 0;
    if (g > 255)
        g = 255;

    /* Get the default value */
    strnfmt(str, sizeof(str), "%d", angband_color_table[idx][3]);

    /* Query, check for ESCAPE */
    if (!settings_ui_prompt_string("Modify Colors",
            "Enter blue (0-255).", "Current channel value.", str,
            sizeof(str)))
        return false;

    /* Convert to number */
    b = atoi(str);

    /* Check bounds */
    if (b < 0)
        b = 0;
    if (b > 255)
        b = 255;

    /* Get the default value */
    strnfmt(str, sizeof(str), "%d", angband_color_table[idx][0]);

    /* Query, check for ESCAPE */
    if (!settings_ui_prompt_string("Modify Colors",
            "Enter extra (0-255).", "Current channel value.", str,
            sizeof(str)))
        return false;

    /* Convert to number */
    k = atoi(str);

    /* Check bounds */
    if (k < 0)
        k = 0;
    if (k > 255)
        k = 255;

    /* Do nothing if the color is not modified */
    if ((k == angband_color_table[idx][0]) && (r == angband_color_table[idx][1])
        && (g == angband_color_table[idx][2])
        && (b == angband_color_table[idx][3]))
        return false;

    /* Modify the color table */
    angband_color_table[idx][0] = k;
    angband_color_table[idx][1] = r;
    angband_color_table[idx][2] = g;
    angband_color_table[idx][3] = b;

    /* Notify the changes */
    return true;
}

/* These two are used to place elements in the grid */
#define COLOR_X(idx) (((idx) / MAX_BASE_COLORS) * 5 + 1)
#define COLOR_Y(idx) ((idx) % MAX_BASE_COLORS + 6)

/* Hack - Note the cast to "int" to prevent overflow */
#define IS_BLACK(idx)                                                          \
    ((int)angband_color_table[idx][1] + (int)angband_color_table[idx][2]       \
            + (int)angband_color_table[idx][3]                                 \
        == 0)

/* We show black as dots to see the shape of the grid */
#define BLACK_SAMPLE "..."

/*
 * The screen used to modify the color table. Only 128 colors can be modified.
 * The remaining entries of the color table are reserved for graphic mode.
 */
static void modify_colors(void)
{
    int idx = 0;
    int scroll = 0;
    char ch;

    while (1)
    {
        int visible_rows = settings_ui_named_sdl_visible_rows(0, 8, 1);
        int dir;

        settings_color_editor_adjust_view(MAX_COLORS, visible_rows, idx,
            &scroll);

        if (!settings_color_editor_present_ui_scene(idx, scroll))
        {
            return;
        }
        ch = settings_ui_read_legacy_key(false);

        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        switch (ch)
        {
        case ESCAPE:
            return;

        /* Down */
        case '2':
        {
            /* Check bounds */
            if (idx + 1 >= MAX_COLORS)
                break;

            /* Get the new position */
            ++idx;
            break;
        }

        /* Up */
        case '8':
        {
            /* Check bounds */
            if (idx - 1 < 0)
                break;

            /* Get the new position */
            --idx;
            break;
        }

        /* Left */
        case '4':
        {
            /* Check bounds */
            if (idx - 16 < 0)
                break;

            /* Get the new position */
            idx -= MAX_BASE_COLORS;
            break;
        }

            /* Right */
        case '6':
        {
            /* Check bounds */
            if (idx + MAX_BASE_COLORS >= MAX_COLORS)
                break;

            /* Get the new position */
            idx += MAX_BASE_COLORS;
            break;
        }

            /* Copy from color */
        case 'c':
        {
            char str[10];
            int src;

            /* Get the default value, the base color */
            sprintf(str, "%d", GET_BASE_COLOR(idx));

            /* Query, check for ESCAPE */
            if (!settings_ui_prompt_string("Modify Colors",
                    format("Copy from color (0-%d, def. base).",
                        MAX_COLORS - 1),
                    "Enter the source color index.", str, sizeof(str)))
                break;

            /* Convert to number */
            src = atoi(str);

            /* Check bounds */
            if (src < 0)
                src = 0;
            if (src >= MAX_COLORS)
                src = MAX_COLORS - 1;

            /* Do nothing if the colors are the same */
            if (src == idx)
                break;

            /* Modify the color table */
            angband_color_table[idx][0] = angband_color_table[src][0];
            angband_color_table[idx][1] = angband_color_table[src][1];
            angband_color_table[idx][2] = angband_color_table[src][2];
            angband_color_table[idx][3] = angband_color_table[src][3];
            break;
        }

        /* Increase the extra value */
        case 'k':
        {
            /* Get a pointer to the proper value */
            byte* k_ptr = &angband_color_table[idx][0];

            /* Modify the value */
            *k_ptr = (byte)(*k_ptr + 1);
            break;
        }

        /* Decrease the extra value */
        case 'K':
        {
            /* Get a pointer to the proper value */
            byte* k_ptr = &angband_color_table[idx][0];

            /* Modify the value */
            *k_ptr = (byte)(*k_ptr - 1);
            break;
        }

        /* Increase the red value */
        case 'r':
        {
            /* Get a pointer to the proper value */
            byte* r_ptr = &angband_color_table[idx][1];

            /* Modify the value */
            *r_ptr = (byte)(*r_ptr + 1);
            break;
        }

        /* Decrease the red value */
        case 'R':
        {
            /* Get a pointer to the proper value */
            byte* r_ptr = &angband_color_table[idx][1];

            /* Modify the value */
            *r_ptr = (byte)(*r_ptr - 1);
            break;
        }

            /* Increase the green value */
        case 'g':
        {
            /* Get a pointer to the proper value */
            byte* g_ptr = &angband_color_table[idx][2];

            /* Modify the value */
            *g_ptr = (byte)(*g_ptr + 1);
            break;
        }

            /* Decrease the green value */
        case 'G':
        {
            /* Get a pointer to the proper value */
            byte* g_ptr = &angband_color_table[idx][2];

            /* Modify the value */
            *g_ptr = (byte)(*g_ptr - 1);
            break;
        }

            /* Increase the blue value */
        case 'b':
        {
            /* Get a pointer to the proper value */
            byte* b_ptr = &angband_color_table[idx][3];

            /* Modify the value */
            *b_ptr = (byte)(*b_ptr + 1);
            break;
        }

        /* Decrease the blue value */
        case 'B':
        {
            /* Get a pointer to the proper value */
            byte* b_ptr = &angband_color_table[idx][3];

            /* Modify the value */
            *b_ptr = (byte)(*b_ptr - 1);
            break;
        }

        /* Ask for specific values */
        case 'v':
            (void)askfor_color_values(idx);
            break;
        }
    }
}

/*
 * Interact with "colors"
 */
void do_cmd_colors(void)
{
    int choice = 0;
    int highlight = 1;
    int i;
    SDL_IOStream* fff;
    char buf[1024];

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Clear any active banner before opening colors menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Interact until done */
    while (1)
    {
        choice = colors_menu(&highlight);
        if (choice == 0)
            break;

        /* Load a user pref file */
        if (choice == 1)
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(8);
            do_cmd_redraw();
        }

#ifdef ALLOW_COLORS

        /* Dump colors */
        else if (choice == 2)
        {
            static cptr mark = "Colors";
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!settings_ui_prompt_string("Interact with Colors",
                    "Enter the file name for the color dump.", "", ftmp,
                    sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_colors: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old colors */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Color redefinitions\n\n");

            /* Dump colors */
            for (i = 0; i < 256; i++)
            {
                int kv = angband_color_table[i][0];
                int rv = angband_color_table[i][1];
                int gv = angband_color_table[i][2];
                int bv = angband_color_table[i][3];

                cptr name = "unknown";

                /* Skip non-entries */
                if (!kv && !rv && !gv && !bv)
                    continue;

                /* Extract the color name */
                if (i < 16)
                    name = color_names[i];

                /* Dump a comment */
                SDL_IOprintf(fff, "# Color '%s'\n", name);

                /* Dump the monster attr/char info */
                SDL_IOprintf(fff, "V:%d:0x%02X:0x%02X:0x%02X:0x%02X\n\n", i, kv, rv,
                    gv, bv);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped color redefinitions.");
        }

        /* Edit colors */
        else if (choice == 3)
        {
            modify_colors();
        }

#endif /* ALLOW_COLORS */

        /* Unknown option */
        else
        {
            bell("Illegal command for colors!");
        }

        /* Flush messages */
        message_flush();
    }
}



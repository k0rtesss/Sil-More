/* File: cmd-ui-settings.c */
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
#include "app/app-command.h"
#include "cmd-ui-settings.h"
#include "platform-audio.h"
#include "platform-config.h"
#include "platform-input.h"
#include "platform-time.h"
#include "sdl-config.h"
#include "sdl-main-internal.h"
#include "object/object-ui-select.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-oaths.h"
#include "sound-config.h"
#include "platform-audio.h"

extern struct sound_config g_sound_config;
#include "cmd-ui-settings-internal.h"
#include <ctype.h>
#include "h-define.h"
#include "app/app-session.h"
#include "app/app-ui.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "cmd-ui.h"
#include "ui/ui-look-sidebar.h"
#include "ui/ui-information-scene.h"

#define COLOR_SAMPLE "###"

char settings_ui_read_key(bool scan)
{
    app_session* session = app_session_current();
    app_input input;

    if (!session || !ui_information_scene_is_active())
        return scan ? '\0' : ESCAPE;

    while (app_session_pop_input(session, &input))
    {
        if (input.layer != APP_INPUT_LAYER_LEGACY
            || input.type != APP_INPUT_TYPE_KEY)
        {
            continue;
        }

        return (char)(input.payload.key.logical_key & 0xFFu);
    }

    return scan ? '\0' : (char)ui_information_scene_wait_key();
}


const struct sdl_config* settings_sdl_default_config(void)
{
    static bool ready = false;
    static struct sdl_config defaults;

    if (!ready)
    {
        sdl_config_set_defaults(&defaults);
        ready = true;
    }

    return &defaults;
}

static int settings_sdl_get_pane_enabled_int(int index)
{
    return SETTINGS_SDL_GET(pane_enabled)(index) ? 1 : 0;
}

static void settings_sdl_set_pane_enabled_int(int index, int value)
{
    SETTINGS_SDL_SET(pane_enabled)(index, value != 0);
}

static const settings_int_binding settings_sdl_int_bindings
    [SETTINGS_SDL_INT_CONFIG_MAX] = {
        [SETTINGS_SDL_INT_MAIN_VIEW_SCALE]
            = { SETTINGS_SDL_GET(main_view_scale),
                SETTINGS_SDL_SET(main_view_scale) },
        [SETTINGS_SDL_INT_MAX_SCALE] = { SETTINGS_SDL_GET(max_scale), NULL },
        [SETTINGS_SDL_INT_OVERLAY_DENSITY]
            = { SETTINGS_SDL_GET(overlay_density),
                SETTINGS_SDL_SET(overlay_density) },
        [SETTINGS_SDL_INT_MIN_TERMINAL_MODE]
            = { SETTINGS_SDL_GET(min_terminal_mode),
                SETTINGS_SDL_SET(min_terminal_mode) },
        [SETTINGS_SDL_INT_AUX_VIEW_FONT_SIZE]
            = { SETTINGS_SDL_GET(aux_view_font_size),
                SETTINGS_SDL_SET(aux_view_font_size) },
        [SETTINGS_SDL_INT_EFFECTIVE_AUX_VIEW_FONT_SIZE]
            = { SETTINGS_SDL_GET(effective_aux_view_font_size), NULL },
        [SETTINGS_SDL_INT_MENU_PANEL_FONT_SIZE]
            = { SETTINGS_SDL_GET(menu_panel_font_size),
                SETTINGS_SDL_SET(menu_panel_font_size) },
        [SETTINGS_SDL_INT_EFFECTIVE_MENU_PANEL_FONT_SIZE]
            = { SETTINGS_SDL_GET(effective_menu_panel_font_size), NULL },
        [SETTINGS_SDL_INT_PLAIN_MENU_FONT_SIZE]
            = { SETTINGS_SDL_GET(plain_menu_font_size),
                SETTINGS_SDL_SET(plain_menu_font_size) },
        [SETTINGS_SDL_INT_EFFECTIVE_PLAIN_MENU_FONT_SIZE]
            = { SETTINGS_SDL_GET(effective_plain_menu_font_size), NULL },
        [SETTINGS_SDL_INT_BROWSER_MENU_FONT_SIZE]
            = { SETTINGS_SDL_GET(browser_menu_font_size),
                SETTINGS_SDL_SET(browser_menu_font_size) },
        [SETTINGS_SDL_INT_EFFECTIVE_BROWSER_MENU_FONT_SIZE]
            = { SETTINGS_SDL_GET(effective_browser_menu_font_size), NULL },
        [SETTINGS_SDL_INT_CHARACTER_SHEET_FONT_SIZE]
            = { SETTINGS_SDL_GET(character_sheet_font_size),
                SETTINGS_SDL_SET(character_sheet_font_size) },
        [SETTINGS_SDL_INT_EFFECTIVE_CHARACTER_SHEET_FONT_SIZE]
            = { SETTINGS_SDL_GET(effective_character_sheet_font_size), NULL },
        [SETTINGS_SDL_INT_MARGIN]
            = { SETTINGS_SDL_GET(margin), SETTINGS_SDL_SET(margin) },
    };

static const settings_bool_binding settings_sdl_bool_bindings
    [SETTINGS_SDL_BOOL_CONFIG_MAX] = {
        [SETTINGS_SDL_BOOL_HIDE_LEFT_PANEL]
            = { SETTINGS_SDL_GET(hide_left_panel),
                SETTINGS_SDL_SET(hide_left_panel) },
        [SETTINGS_SDL_BOOL_FULLSCREEN]
            = { SETTINGS_SDL_GET(fullscreen), SETTINGS_SDL_SET(fullscreen) },
        [SETTINGS_SDL_BOOL_TILES]
            = { SETTINGS_SDL_GET(tiles), SETTINGS_SDL_SET(tiles) },
        [SETTINGS_SDL_BOOL_USE_UNSAFE_AREA]
            = { SETTINGS_SDL_GET(use_unsafe_area),
                SETTINGS_SDL_SET(use_unsafe_area) },
        [SETTINGS_SDL_BOOL_RIGHT_PANES_ENABLED]
            = { SETTINGS_SDL_GET(enable_right_panes),
                SETTINGS_SDL_SET(enable_right_panes) },
        [SETTINGS_SDL_BOOL_BOTTOM_PANES_ENABLED]
            = { SETTINGS_SDL_GET(enable_bottom_panes),
                SETTINGS_SDL_SET(enable_bottom_panes) },
        [SETTINGS_SDL_BOOL_SHOW_PANE_BORDERS]
            = { SETTINGS_SDL_GET(show_pane_borders),
                SETTINGS_SDL_SET(show_pane_borders) },
    };

static const settings_indexed_int_binding settings_sdl_pane_bindings
    [SETTINGS_SDL_PANE_METRIC_MAX] = {
        [SETTINGS_SDL_PANE_TYPE] = { SETTINGS_SDL_GET(pane_type), NULL },
        [SETTINGS_SDL_PANE_WHERE]
            = { SETTINGS_SDL_GET(pane_where), SETTINGS_SDL_SET(pane_where) },
        [SETTINGS_SDL_PANE_ENABLED]
            = { settings_sdl_get_pane_enabled_int,
                settings_sdl_set_pane_enabled_int },
        [SETTINGS_SDL_PANE_ROWS]
            = { SETTINGS_SDL_GET(pane_rows), SETTINGS_SDL_SET(pane_rows) },
        [SETTINGS_SDL_PANE_COLS]
            = { SETTINGS_SDL_GET(pane_cols), SETTINGS_SDL_SET(pane_cols) },
        [SETTINGS_SDL_PANE_FONT_SIZE]
            = { SETTINGS_SDL_GET(pane_font_size),
                SETTINGS_SDL_SET(pane_font_size) },
        [SETTINGS_SDL_PANE_EFFECTIVE_FONT_SIZE]
            = { SETTINGS_SDL_GET(pane_effective_font_size), NULL },
        [SETTINGS_SDL_PANE_CURRENT_ROWS]
            = { SETTINGS_SDL_GET(pane_current_rows), NULL },
        [SETTINGS_SDL_PANE_CURRENT_COLS]
            = { SETTINGS_SDL_GET(pane_current_cols), NULL },
    };

int settings_sdl_get_int_config(settings_sdl_int_config id)
{
    if (id < 0 || id >= SETTINGS_SDL_INT_CONFIG_MAX
        || !settings_sdl_int_bindings[id].get)
    {
        return 0;
    }

    return settings_sdl_int_bindings[id].get();
}

void settings_sdl_set_int_config(settings_sdl_int_config id, int value)
{
    if (id < 0 || id >= SETTINGS_SDL_INT_CONFIG_MAX
        || !settings_sdl_int_bindings[id].set)
    {
        return;
    }

    settings_sdl_int_bindings[id].set(value);
}

bool settings_sdl_get_bool_config(settings_sdl_bool_config id)
{
    if (id < 0 || id >= SETTINGS_SDL_BOOL_CONFIG_MAX
        || !settings_sdl_bool_bindings[id].get)
    {
        return false;
    }

    return settings_sdl_bool_bindings[id].get();
}

void settings_sdl_set_bool_config(settings_sdl_bool_config id,
    bool value)
{
    if (id < 0 || id >= SETTINGS_SDL_BOOL_CONFIG_MAX
        || !settings_sdl_bool_bindings[id].set)
    {
        return;
    }

    settings_sdl_bool_bindings[id].set(value);
}

int settings_sdl_get_pane_metric(settings_sdl_pane_metric metric,
    int index)
{
    if (metric < 0 || metric >= SETTINGS_SDL_PANE_METRIC_MAX
        || !settings_sdl_pane_bindings[metric].get)
    {
        return 0;
    }

    return settings_sdl_pane_bindings[metric].get(index);
}

void settings_sdl_set_pane_metric(settings_sdl_pane_metric metric,
    int index, int value)
{
    if (metric < 0 || metric >= SETTINGS_SDL_PANE_METRIC_MAX
        || !settings_sdl_pane_bindings[metric].set)
    {
        return;
    }

    settings_sdl_pane_bindings[metric].set(index, value);
}

settings_ui_layout settings_ui_read_layout(void)
{
    settings_ui_layout layout;
    bool compact = settings_sdl_get_int_config(
        SETTINGS_SDL_INT_MIN_TERMINAL_MODE) == 1;

    memset(&layout, 0, sizeof(layout));
    layout.compact = compact;
    layout.narrow = compact;
    layout.list_row_budget = compact ? 18 : 24;
    layout.option_line_chars = compact ? 45 : 75;
    layout.prompt_line_chars = compact ? 50 : 80;
    layout.inset_prompt_line_chars = compact ? 48 : 78;
    layout.pane_overview_label_chars = compact ? 22 : 52;
    layout.pane_overview_value_chars = compact ? 6 : 14;
    layout.supporting_font_label_chars = compact ? 24 : 39;
    layout.supporting_layout_type_chars = compact ? 16 : 26;
    layout.supporting_layout_where_chars = compact ? 12 : 19;

    return layout;
}

int settings_ui_list_visible_rows(const settings_ui_layout* layout,
    int first_row, int footer_rows, int min_rows)
{
    int row_budget = layout ? layout->list_row_budget : 24;
    int visible_rows = row_budget - footer_rows - first_row;

    if (min_rows < 1)
        min_rows = 1;
    if (visible_rows < min_rows)
        visible_rows = min_rows;

    return visible_rows;
}

static void clear_skills_and_abilities(void)
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
    { 14, "Debug" },
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

void settings_ui_fit_text(char* buf, size_t buflen, cptr text,
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

cptr settings_ui_pick_label(int max_chars, cptr long_label,
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

bool settings_ui_prompt_string(cptr title, cptr prompt, cptr note,
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

app_ui_panel* settings_browser_scene_begin_ex(app_ui_scene* scene,
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

app_ui_panel* settings_browser_scene_begin(app_ui_scene* scene,
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

bool settings_browser_add_pair_row(app_ui_panel* panel, s16b id,
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

int settings_choice_find_index_by_id(
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

int settings_choice_menu(cptr title,
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
    ch = settings_ui_read_key(false);
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

static cptr sound_option_label(const settings_ui_layout* layout, int index)
{
    bool compact = layout && layout->compact;
    bool narrow = layout && layout->narrow;

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

static cptr option_menu_label(const settings_ui_layout* layout, int opt)
{
    bool compact = layout && layout->compact;
    bool narrow = layout && layout->narrow;

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
    case OPT_banner_popup_seconds:
        return compact ? (narrow ? "Banner popup" : "Banner duration")
                       : "Narrative banner popup";
    case OPT_ability_desc_mode:
        return compact ? (narrow ? "Ability text" : "Ability descriptions")
                       : "Ability descriptions (0=lore+effect, 1=effect+lore, 2=effect)";
    case OPT_vault_drop_frequency:
        return compact ? "Vault drops" : "Vault drop frequency";
    case OPT_noble_item_spawn_mode:
        return compact ? (narrow ? "Noble items" : "Noble item sources")
                       : "Noble item spawns";
    case OPT_min_depth_timer_mode:
        return compact ? (narrow ? "Depth timer" : "Min-depth timer")
                       : "Minimum-depth timer pace";
    case OPT_pacifist_attack_warning:
        return compact ? (narrow ? "Attack warn" : "Pacifist warning")
                       : "Confirm before direct attacks";
    case OPT_look_objects_sort_by_difficulty:
        return compact ? (narrow ? "Look diff sort" : "Look sort by diff")
                       : "Sort look (L) objects by difficulty only";
    case OPT_look_nearby_filter_default:
        return compact ? (narrow ? "Look near def" : "Look nearby default")
                       : "Default look (l) nearby filter";
    case OPT_show_elemental_item_rolls:
        return compact ? (narrow ? "Dbg elem items" : "Debug elemental items")
                       : "Debug elemental items";
    case OPT_intro_style:
        return compact ? (narrow ? "Welcome art" : "Welcome screen")
                       : "Welcome screen style";
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

static cptr option_menu_describe_value(const settings_ui_layout* layout,
    bool is_sound_page, const int opt[], int index,
    const struct sound_config* sound_cfg, char* value_buf, size_t buflen)
{
    bool compact = layout && layout->compact;
    cptr label;

    if (!value_buf || !buflen)
        return "";

    value_buf[0] = '\0';

    if (is_sound_page)
    {
        label = sound_option_label(layout, index);
        if (index == 0)
        {
            strnfmt(value_buf, buflen, "%s",
                sound_cfg->enabled ? "yes" : "no ");
        }
        else if (index == 1)
        {
            strnfmt(value_buf, buflen, "%s",
                sound_cfg->enable_combat ? "yes" : "no ");
        }
        else if (index == 2)
        {
            strnfmt(value_buf, buflen, "%s",
                sound_cfg->enable_inventory ? "yes" : "no ");
        }
        else if (index == 3)
        {
            strnfmt(value_buf, buflen, "%s",
                sound_cfg->enable_walk ? "yes" : "no ");
        }
        else if (index == 4)
        {
            strnfmt(value_buf, buflen, "%s",
                sound_cfg->enable_doors ? "yes" : "no ");
        }
        else if (index == 5)
        {
            strnfmt(value_buf, buflen, "%.0f%%",
                sound_cfg->volume_combat * 100.0f);
        }
        else if (index == 6)
        {
            strnfmt(value_buf, buflen, "%.0f%%",
                sound_cfg->volume_inventory * 100.0f);
        }
        else if (index == 7)
        {
            strnfmt(value_buf, buflen, "%.0f%%",
                sound_cfg->volume_walk * 100.0f);
        }
        else if (index == 8)
        {
            strnfmt(value_buf, buflen, "%.0f%%",
                sound_cfg->volume_doors * 100.0f);
        }
        else if (index == 9)
        {
            strnfmt(value_buf, buflen, "%.0f%%",
                sound_cfg->volume_other * 100.0f);
        }
        else if (index == 10)
        {
            strnfmt(value_buf, buflen, "%s",
                sound_cfg->music_main_enabled ? "yes" : "no ");
        }
        else if (index == 11)
        {
            strnfmt(value_buf, buflen, "%s",
                sound_cfg->music_ambient_enabled ? "yes" : "no ");
        }
        else if (index == 12)
        {
            strnfmt(value_buf, buflen, "%.0f%%",
                sound_cfg->music_main_volume * 100.0f);
        }
        else if (index == 13)
        {
            strnfmt(value_buf, buflen, "%.0f%%",
                sound_cfg->music_ambient_volume * 100.0f);
        }

        return label;
    }

    label = option_menu_label(layout, opt[index]);
    if (opt[index] == OPT_delay_factor)
    {
        strnfmt(value_buf, buflen, "%d", op_ptr->delay_factor);
    }
    else if (opt[index] == OPT_hitpoint_warning)
    {
        strnfmt(value_buf, buflen, "%d%%",
            op_ptr->hitpoint_warn * 10);
    }
    else if (opt[index] == OPT_hide_left_panel)
    {
        strnfmt(value_buf, buflen, "%s",
            settings_sdl_get_bool_config(SETTINGS_SDL_BOOL_HIDE_LEFT_PANEL)
                ? "yes"
                : "no ");
    }
    else if (opt[index] == OPT_main_combat_rolls)
    {
        strnfmt(value_buf, buflen, "%d",
            op_ptr->main_combat_rolls);
    }
    else if (opt[index] == OPT_banner_popup_seconds)
    {
        if (op_ptr->narrative_banner_seconds == 0)
            strnfmt(value_buf, buflen, "Off");
        else if (compact)
            strnfmt(value_buf, buflen, "%d sec",
                op_ptr->narrative_banner_seconds);
        else
            strnfmt(value_buf, buflen, "%d seconds",
                op_ptr->narrative_banner_seconds);
    }
    else if (opt[index] == OPT_ability_desc_mode)
    {
        const char* mode_str;

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
        strnfmt(value_buf, buflen, "%s", mode_str);
    }
    else if (opt[index] == OPT_vault_drop_frequency)
    {
        const char* vdf_names[] = {
            "Normal", "Modest", "Scarce", "Meager", "Plentiful"
        };
        byte mode = op_ptr->vault_drop_frequency;

        if (mode > VDF_PLENTIFUL)
            mode = VDF_NORMAL;
        strnfmt(value_buf, buflen, "%s (%d)", vdf_names[mode],
            mode);
    }
    else if (opt[index] == OPT_noble_item_spawn_mode)
    {
        const char* mode_str
            = (op_ptr->noble_item_spawn_mode
                == NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
            ? (compact
                ? "1 with vaults"
                : "1 (also &/! vault drops)")
            : (compact
                ? "0 restricted"
                : "0 (good+/chests/human+elf skeletons)");

        strnfmt(value_buf, buflen, "%s", mode_str);
    }
    else if (opt[index] == OPT_min_depth_timer_mode)
    {
        const char* mode_str;

        switch (op_ptr->min_depth_timer_mode)
        {
        case MIN_DEPTH_TIMER_MODE_RELAXED:
            mode_str = compact ? "1 relaxed" : "1 (Relaxed)";
            break;
        case MIN_DEPTH_TIMER_MODE_HARSH:
            mode_str = compact ? "2 harsh" : "2 (Harsh)";
            break;
        default:
            mode_str = compact ? "0 normal" : "0 (Normal)";
            break;
        }
        strnfmt(value_buf, buflen, "%s", mode_str);
    }
    else if (opt[index] == OPT_intro_style)
    {
        const char* is_names[] = {
            "Flame Imperishable", "Oath of Fëanor",
            "Twilight of Valinor", "Song of Lúthien",
            "Words of Húrin", "Starlight on Cuiviénen",
            "Lament of the Noldor", "Random"
        };
        byte mode = op_ptr->intro_style;

        if (mode > INTRO_STYLE_RANDOM)
            mode = INTRO_STYLE_FLAME;
        strnfmt(value_buf, buflen, "%s", is_names[mode]);
    }
    else
    {
        strnfmt(value_buf, buflen, "%s",
            op_ptr->opt[opt[index]] ? "yes" : "no ");
    }

    return label;
}

static bool option_menu_present_ui_scene(int page, cptr info, int n,
    const int opt[], int k, int scroll,
    const struct option_group_marker* groups,
    const struct sound_config* sound_cfg)
{
    settings_ui_layout layout = settings_ui_read_layout();
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
        char value_buf[64];
        cptr label;
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

        label = option_menu_describe_value(&layout, is_sound_page, opt, i,
            sound_cfg, value_buf, sizeof(value_buf));
        if (!settings_browser_add_pair_row(panel, (s16b)i, attr, TERM_SLATE,
                !locked, i == k, label, value_buf))
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
    struct sound_config* sound_cfg = platform_sound_config();

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
        settings_ui_layout layout = settings_ui_read_layout();
        int first_row = 3;
        int footer_rows = (page == CHALLENGE_PAGE) ? 4 : 2;
        int visible_rows = settings_ui_list_visible_rows(&layout, first_row,
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
        ch = settings_ui_read_key(false);
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
                platform_sound_save_config();
                platform_sound_reload();
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
                    settings_sdl_set_bool_config(
                        SETTINGS_SDL_BOOL_HIDE_LEFT_PANEL,
                        !settings_sdl_get_bool_config(
                            SETTINGS_SDL_BOOL_HIDE_LEFT_PANEL));
                    platform_apply_config();
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
                else if (opt[k] == OPT_banner_popup_seconds)
                {
                    op_ptr->narrative_banner_seconds =
                        (op_ptr->narrative_banner_seconds
                            < NARRATIVE_BANNER_SECONDS_MAX)
                        ? op_ptr->narrative_banner_seconds + 1
                        : 0;
                    clear_active_narrative_banner();
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
                else if (opt[k] == OPT_min_depth_timer_mode)
                {
                    op_ptr->min_depth_timer_mode
                        = (op_ptr->min_depth_timer_mode < MIN_DEPTH_TIMER_MODE_MAX)
                        ? op_ptr->min_depth_timer_mode + 1
                        : MIN_DEPTH_TIMER_MODE_NORMAL;
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
                        platform_sound_save_config(); /* Apply volume change immediately */
                    }
                    else if (k == 13) {
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume < 1.0f) ? sound_cfg->music_ambient_volume + 0.1f : 1.0f;
                        platform_sound_save_config(); /* Apply volume change immediately */
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
                    settings_sdl_set_bool_config(
                        SETTINGS_SDL_BOOL_HIDE_LEFT_PANEL, true);
                    platform_apply_config();
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
                else if (opt[k] == OPT_banner_popup_seconds)
                {
                    if (op_ptr->narrative_banner_seconds
                        < NARRATIVE_BANNER_SECONDS_MAX)
                    {
                        op_ptr->narrative_banner_seconds++;
                        clear_active_narrative_banner();
                    }
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
                else if (opt[k] == OPT_min_depth_timer_mode)
                {
                    op_ptr->min_depth_timer_mode
                        = (op_ptr->min_depth_timer_mode < MIN_DEPTH_TIMER_MODE_MAX)
                        ? op_ptr->min_depth_timer_mode + 1
                        : MIN_DEPTH_TIMER_MODE_MAX;
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
                        platform_sound_save_config(); /* Apply volume change immediately */
                    }
                    else if (k == 13) {
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume > 0.0f) ? sound_cfg->music_ambient_volume - 0.1f : 0.0f;
                        platform_sound_save_config(); /* Apply volume change immediately */
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
                    settings_sdl_set_bool_config(
                        SETTINGS_SDL_BOOL_HIDE_LEFT_PANEL, false);
                    platform_apply_config();
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
                else if (opt[k] == OPT_banner_popup_seconds)
                {
                    if (op_ptr->narrative_banner_seconds > 0)
                    {
                        op_ptr->narrative_banner_seconds--;
                        clear_active_narrative_banner();
                    }
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
                else if (opt[k] == OPT_min_depth_timer_mode)
                {
                    op_ptr->min_depth_timer_mode
                        = (op_ptr->min_depth_timer_mode > MIN_DEPTH_TIMER_MODE_NORMAL)
                        ? op_ptr->min_depth_timer_mode - 1
                        : MIN_DEPTH_TIMER_MODE_NORMAL;
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

static int other_options_menu(int* highlight)
{
    bool death_view = death_spectator_active();
    const settings_choice_entry entries[] = {
        { 1, 'm', "m) Choose Palette", false },
        { 2, 'n', "n) Write a note", false },
        { 3, 's', "s) Suicide", death_view },
        { 4, 'o', "o) Return to Options", false },
    };

    if (death_view && *highlight == 3)
        *highlight = 4;

    return settings_choice_menu("Other Options", entries,
        (int)N_ELEMENTS(entries), highlight, 4);
}

static void do_cmd_other_options(void)
{
    int choice = 0;
    int highlight = 1;
    bool return_to_options = false;

    while (!return_to_options)
    {
        choice = other_options_menu(&highlight);

        switch (choice)
        {
        case 1:
            do_cmd_colors();
            break;

        case 2:
            do_cmd_note("", p_ptr->depth);
            break;

        case 3:
            do_cmd_suicide();
            return_to_options = true;
            break;

        case 4:
            return_to_options = true;
            break;
        }
    }
}

static int options_menu(int* highlight)
{
    bool allow_debug_menu = false;
    settings_choice_entry entries[13];
    int entry_count = 0;
#ifdef SHOW_DEBUG_OPTIONS_MENU
    allow_debug_menu = true;
#endif
    entries[entry_count++] = (settings_choice_entry){ 1, 'a',
        "a) Movement Settings", false };
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
        "j) Other Options", false };
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

    if (!ui_information_scene_enter(&settings_scope))
        return;

    if (p_ptr && p_ptr->playing)
        platform_music_play_menu_theme();

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
            do_cmd_other_options();
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
        platform_music_stop_main();
}

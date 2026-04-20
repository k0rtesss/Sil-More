/* File: cmd-ui-nearby.c */
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
#include "app/app-session.h"
#include "platform-frame.h"
#include "platform-input.h"
#include "object/object-ui-select.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-oaths.h"
#include "sound-config.h"
#include "platform-audio.h"

extern struct sound_config g_sound_config;
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "cmd-ui.h"
#include "ui/ui-information-scene.h"
/*
 * Determines the direction from the player and writes it as text into a buffer
 * of at least size 10.
 */
static void write_direction_from_player_to_buffer(
    int y, int x, char* buffer, int buffer_size)
{
    bool north, south, east, west;
    int buffer_offset = 0;

    if (buffer_size < 10)
        return;

    north = p_ptr->py > y;
    south = p_ptr->py < y;
    east = p_ptr->px < x;
    west = p_ptr->px > x;

    if (north)
    {
        strncpy(buffer, "north", 6);
        strcpy(buffer, "north");
        buffer_offset += 5;
    }
    else if (south)
    {
        strncpy(buffer, "south", 6);
        buffer_offset += 5;
    }

    if (east)
    {
        strncpy(buffer + buffer_offset, "east", 5);
        buffer_offset += 4;
    }
    else if (west)
    {
        strncpy(buffer + buffer_offset, "west", 5);
        buffer_offset += 4;
    }
}

#define MAX_VIEW_LINES 50

typedef struct view_monster_data_line view_monster_data_line;
struct view_monster_data_line
{
    int distance;
    char monster_character;
    int monster_color;
    int alert_color;
    char direction[12];
    char name[40];
    char stance[20];
};

typedef struct view_object_data_line view_object_data_line;
struct view_object_data_line
{
    int distance;
    char object_character;
    int object_color;
    int name_color;
    char direction[12];
    char name[80];
};

static byte look_object_name_color(const object_type* o_ptr)
{
    if (weapon_glows(o_ptr))
        return object_display_color(o_ptr, TERM_L_BLUE);

    return object_display_color(o_ptr, object_default_text_color(o_ptr));
}

static void append_look_smithing_debug(char* buf, size_t buf_size,
    const object_type* o_ptr)
{
    char smith_buf[20];

    smith_buf[0] = '\0';
    if (op_ptr->opt[OPT_show_smithing_difficulty_look] && object_known_p(o_ptr)
        && object_uses_smithing_difficulty(o_ptr))
    {
        int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
        int sd = object_smithing_difficulty(o_ptr);
        int wr = object_weight_rarity(o_ptr, depth);

        strnfmt(smith_buf, sizeof(smith_buf), " {%d,%d}", sd, wr);
        SDL_strlcat(buf, smith_buf, buf_size);
    }
}

static void nearby_collect_monster_lines(bool line_of_sight_only,
    view_monster_data_line* lines, int max_lines, int* out_count,
    int* out_longest_name_length, int* out_longest_direction_length,
    int* out_longest_stance_length)
{
    int i;
    int j = 0;
    int longest_name_length = 0;
    int longest_direction_length = 0;
    int longest_stance_length = 0;

    if (!lines || max_lines <= 0 || !out_count || !out_longest_name_length
        || !out_longest_direction_length || !out_longest_stance_length)
    {
        return;
    }

    get_sorted_target_list(TARGET_LIST_MONSTER, 0);

    for (i = 0; i < temp_n; i++)
    {
        int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];
        monster_type* m_ptr = &mon_list[m_idx];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        char m_name[40];

        if (j >= max_lines)
            break;
        if (!m_ptr->ml)
            continue;
        if (!player_has_los_bold(temp_y[i], temp_x[i]) && line_of_sight_only)
            continue;

        memset(&lines[j], 0, sizeof(lines[j]));
        monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);
        write_direction_from_player_to_buffer(temp_y[i], temp_x[i],
            lines[j].direction, sizeof(lines[j].direction));
        if (!get_alertness_text(m_ptr, sizeof(lines[j].stance),
                lines[j].stance, &lines[j].alert_color))
        {
            continue;
        }

        lines[j].monster_character = monster_char(r_ptr);
        lines[j].monster_color = monster_attr(r_ptr);
        lines[j].distance =
            distance(p_ptr->py, p_ptr->px, temp_y[i], temp_x[i]);
        SDL_strlcpy(lines[j].name, m_name, sizeof(lines[j].name));

        longest_name_length = MAX(longest_name_length,
            (int)strlen(lines[j].name));
        longest_direction_length = MAX(longest_direction_length,
            (int)strlen(lines[j].direction));
        longest_stance_length = MAX(longest_stance_length,
            (int)strlen(lines[j].stance));
        j++;
    }

    *out_count = j;
    *out_longest_name_length = longest_name_length;
    *out_longest_direction_length = longest_direction_length;
    *out_longest_stance_length = longest_stance_length;
}

static void nearby_collect_object_lines(bool line_of_sight_only,
    view_object_data_line* lines, int max_lines, int* out_count,
    int* out_longest_name_length, int* out_longest_direction_length)
{
    int i;
    int j = 0;
    int longest_name_length = 0;
    int longest_direction_length = 0;

    if (!lines || max_lines <= 0 || !out_count || !out_longest_name_length
        || !out_longest_direction_length)
    {
        return;
    }

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    for (i = 0; i < temp_n; i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        object_type* o_ptr = &o_list[o_idx];
        char o_name[80];

        if (j >= max_lines)
            break;
        if (!player_can_see_bold(temp_y[i], temp_x[i]) && line_of_sight_only)
            continue;

        memset(&lines[j], 0, sizeof(lines[j]));
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        append_look_smithing_debug(o_name, sizeof(o_name), o_ptr);
        write_direction_from_player_to_buffer(temp_y[i], temp_x[i],
            lines[j].direction, sizeof(lines[j].direction));
        if (lines[j].direction[0] == '\0')
        {
            SDL_strlcpy(lines[j].direction, "underfoot",
                sizeof(lines[j].direction));
        }

        lines[j].distance =
            distance(p_ptr->py, p_ptr->px, temp_y[i], temp_x[i]);
        lines[j].object_character = object_char(o_ptr);
        lines[j].object_color = object_attr(o_ptr);
        lines[j].name_color = look_object_name_color(o_ptr);
        SDL_strlcpy(lines[j].name, o_name, sizeof(lines[j].name));

        longest_name_length = MAX(longest_name_length,
            (int)strlen(lines[j].name));
        longest_direction_length = MAX(longest_direction_length,
            (int)strlen(lines[j].direction));
        j++;
    }

    *out_count = j;
    *out_longest_name_length = longest_name_length;
    *out_longest_direction_length = longest_direction_length;
}

typedef bool (*nearby_scene_build_fn)(bool line_of_sight_only, cptr prompt,
    app_ui_scene* scene);

static bool nearby_snapshot_active(void)
{
    return app_session_interactions_enabled(app_session_current());
}

static char nearby_snapshot_wait_key(void)
{
    return (char)ui_information_scene_wait_key_with_wait_reason(
        APP_WAIT_REASON_INFORMATIONAL_PAUSE);
}

static app_ui_panel* nearby_build_panel(app_ui_scene* scene, cptr prompt,
    cptr empty_text)
{
    app_ui_panel* panel;

    if (!scene || !prompt)
        return NULL;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;

    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return NULL;

    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 420, 840);
    app_ui_panel_set_title(panel, TERM_L_BLUE, prompt);
    if (empty_text && empty_text[0])
        (void)app_ui_panel_add_body_line(panel, TERM_WHITE, empty_text);
    return panel;
}

static void nearby_format_row_label(cptr direction, cptr name, char* label,
    size_t label_size)
{
    if (!label || label_size == 0)
        return;

    if (direction && direction[0])
        strnfmt(label, label_size, "%s %s", direction, name ? name : "");
    else
        SDL_strlcpy(label, name ? name : "", label_size);
}

static bool nearby_monsters_build_scene(bool line_of_sight_only, cptr prompt,
    app_ui_scene* scene)
{
    view_monster_data_line lines[MAX_VIEW_LINES];
    app_ui_panel* panel;
    int count = 0;
    int longest_name_length = 0;
    int longest_direction_length = 0;
    int longest_stance_length = 0;
    int i;

    if (!scene || !prompt)
        return false;

    nearby_collect_monster_lines(line_of_sight_only, lines, MAX_VIEW_LINES,
        &count, &longest_name_length, &longest_direction_length,
        &longest_stance_length);
    (void)longest_name_length;
    (void)longest_direction_length;
    (void)longest_stance_length;

    panel = nearby_build_panel(scene, prompt,
        count ? NULL : "No visible monsters.");
    if (!panel)
        return false;

    for (i = 0; i < count; i++)
    {
        char label[APP_UI_LABEL_MAX];

        nearby_format_row_label(lines[i].direction, lines[i].name, label,
            sizeof(label));
        if (!app_ui_panel_add_row_ex(panel, (s16b)i, TERM_WHITE,
                (byte)lines[i].alert_color,
                (byte)lines[i].monster_color, lines[i].monster_character,
                true, false, "", label, lines[i].stance))
        {
            return false;
        }
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "[", "Toggle LOS");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Esc", "Close");
    return true;
}

static bool nearby_objects_build_scene(bool line_of_sight_only, cptr prompt,
    app_ui_scene* scene)
{
    view_object_data_line lines[MAX_VIEW_LINES];
    app_ui_panel* panel;
    int count = 0;
    int longest_name_length = 0;
    int longest_direction_length = 0;
    int i;

    if (!scene || !prompt)
        return false;

    nearby_collect_object_lines(line_of_sight_only, lines, MAX_VIEW_LINES,
        &count, &longest_name_length, &longest_direction_length);
    (void)longest_name_length;
    (void)longest_direction_length;

    panel = nearby_build_panel(scene, prompt,
        count ? NULL : "No visible objects.");
    if (!panel)
        return false;

    for (i = 0; i < count; i++)
    {
        char label[APP_UI_LABEL_MAX];

        nearby_format_row_label(lines[i].direction, lines[i].name, label,
            sizeof(label));
        if (!app_ui_panel_add_row_ex(panel, (s16b)i,
                (byte)lines[i].name_color, (byte)lines[i].name_color,
                (byte)lines[i].object_color, lines[i].object_character,
                true, false, "", label, ""))
        {
            return false;
        }
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "]", "Toggle LOS");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Esc", "Close");
    return true;
}

static bool nearby_information_scene(nearby_scene_build_fn build_fn,
    cptr los_prompt, cptr screen_prompt, char toggle_key)
{
    app_session* session = app_session_current();
    char ch = toggle_key;
    bool show_los = true;

    if (!build_fn || !los_prompt || !screen_prompt)
        return false;
    if (!nearby_snapshot_active() || !session)
        return false;

    while (ch == toggle_key)
    {
        app_ui_scene scene;
        cptr prompt = show_los ? los_prompt : screen_prompt;

        do_cmd_redraw();
        if (!build_fn(show_los, prompt, &scene)
            || !app_session_publish_dungeon_overlay_scene(session, &scene))
        {
            app_session_clear_dungeon_overlay_scene(session);
            return false;
        }
        platform_frame_present();

        inkey_set_cursor_hidden(true);
        ch = nearby_snapshot_wait_key();
        inkey_set_cursor_hidden(false);

        if (ch == toggle_key)
            show_los = !show_los;
    }

    app_session_clear_dungeon_overlay_scene(session);
    platform_frame_present();
    return true;
}

void do_cmd_view_monsters()
{
    if (nearby_information_scene(nearby_monsters_build_scene,
            "Monsters you can see (press [ to toggle):",
            "Monsters on screen (press [ to toggle):", '['))
    {
        return;
    }

    log_warn("nearby monsters: semantic overlay unavailable");
    bell("Nearby monsters overlay unavailable.");
}

void do_cmd_view_objects()
{
    if (nearby_information_scene(nearby_objects_build_scene,
            "Objects you can see (press ] to toggle):",
            "Objects on screen (press ] to toggle):", ']'))
    {
        return;
    }

    log_warn("nearby objects: semantic overlay unavailable");
    bell("Nearby objects overlay unavailable.");
}

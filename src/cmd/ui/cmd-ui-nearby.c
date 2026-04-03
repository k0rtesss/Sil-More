/* File: cmd-ui-nearby.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "app/app-session.h"
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
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "cmd-ui.h"
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

    return object_display_color(o_ptr,
        tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
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

static int nearby_distance_color(int distance)
{
    if (distance < 5)
        return TERM_WHITE;
    if (distance < 10)
        return TERM_L_WHITE;
    return TERM_L_DARK;
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

void show_nearby_monsters(bool line_of_sight_only)
{
    view_monster_data_line lines[MAX_VIEW_LINES];
    int i, j = 0;
    int col;
    int longest_name_length = 0;
    int longest_direction_length = 0;
    int longest_stance_length = 0;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;

    /* Get terminal height and calculate available space */
    int term_hgt = Term->hgt;
    int max_lines = MIN(MAX_VIEW_LINES, term_hgt - 3); /* Leave space for header and footer */

    nearby_collect_monster_lines(line_of_sight_only, lines, max_lines, &j,
        &longest_name_length, &longest_direction_length,
        &longest_stance_length);

    if (!j)
    {
        int empty_col = MAX(0, (term_wid - 20) / 2);
        Term_erase(0, 1, 255);
        Term_erase(0, 2, 255);
        Term_erase(0, 3, 255);
        Term_putstr(empty_col, 1, term_wid - empty_col, TERM_WHITE,
            "No visible monsters.");
        return;
    }

    col = term_wid - longest_name_length - longest_direction_length
        - longest_stance_length - 9;
    col = MAX(0, col);

    for (i = 0; i < j; ++i)
    {
        int distance_color;
        char monster_char[2];
        int direction_col = col + 6;
        int name_col = direction_col + MAX(longest_direction_length, 1) + 1;
        int stance_col = term_wid - MAX(longest_stance_length, 1) - 1;
        int name_width = stance_col - name_col - 1;
        bool show_stance = true;

        monster_char[0] = lines[i].monster_character;
        monster_char[1] = '\0';

        distance_color = nearby_distance_color(lines[i].distance);

        /* Clear the line */
        Term_erase(col, i + 1, term_wid - col);

        if (name_width < 8)
        {
            show_stance = false;
            name_width = term_wid - name_col - 1;
        }
        if (name_width < 1)
            name_width = 1;

        c_put_str(lines[i].monster_color, monster_char, i + 1, col + 2);
        if (use_bigtile)
        {
            Term_putch(col + 3, i + 1, 255, -1);
        }
        Term_putstr(direction_col, i + 1, MAX(longest_direction_length, 1),
            distance_color, lines[i].direction);
        Term_putstr(name_col, i + 1, name_width, TERM_WHITE, lines[i].name);
        if (show_stance)
        {
            Term_putstr(stance_col, i + 1, term_wid - stance_col,
                lines[i].alert_color, lines[i].stance);
        }
    }

    if (j)
    {
        Term_erase(col, j + 1, term_wid - col);
    }
}

void show_nearby_objects(bool line_of_sight_only)
{
    view_object_data_line lines[MAX_VIEW_LINES];
    int i, j = 0;
    int col;
    int longest_name_length = 0;
    int longest_direction_length = 0;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;

    /* Get terminal height and calculate available space */
    int term_hgt = Term->hgt;
    int max_lines = MIN(MAX_VIEW_LINES, term_hgt - 3); /* Leave space for header and footer */

    nearby_collect_object_lines(line_of_sight_only, lines, max_lines, &j,
        &longest_name_length, &longest_direction_length);

    if (!j)
    {
        int empty_col = MAX(0, (term_wid - 19) / 2);
        Term_erase(0, 1, 255);
        Term_erase(0, 2, 255);
        Term_erase(0, 3, 255);
        Term_putstr(empty_col, 1, term_wid - empty_col, TERM_WHITE,
            "No visible objects.");
        return;
    }

    col = term_wid - longest_name_length - longest_direction_length - 9;
    col = MAX(0, col);

    Term_erase(col, 1, term_wid - col);

    for (i = 0; i < j; ++i)
    {
        int distance_color;
        int direction_col = col + 6;
        int name_col = direction_col + MAX(longest_direction_length, 1) + 1;
        int name_width = term_wid - name_col - 1;

        char o_char[2];

        o_char[0] = lines[i].object_character;
        o_char[1] = '\0';

        distance_color = nearby_distance_color(lines[i].distance);

        /* Clear the line */
        Term_erase(col, i + 1, term_wid - col);

        if (name_width < 1)
            name_width = 1;

        c_put_str(lines[i].object_color, o_char, i + 1, col + 2);
        if (use_bigtile)
        {
            Term_putch(col + 3, i + 1, 255, -1);
        }
        Term_putstr(direction_col, i + 1, MAX(longest_direction_length, 1),
            distance_color, lines[i].direction);
        Term_putstr(name_col, i + 1, name_width, lines[i].name_color,
            lines[i].name);
    }

    if (j)
    {
        Term_erase(col, j + 1, term_wid - col);
    }
}

typedef void (*nearby_scene_draw_fn)(bool line_of_sight_only);
typedef bool (*nearby_scene_build_fn)(bool line_of_sight_only, cptr prompt,
    app_ui_scene* scene);

static bool nearby_snapshot_active(void)
{
    return app_session_interactions_enabled(app_session_current());
}

static char nearby_snapshot_wait_key(void)
{
    app_wait_scope wait_scope;
    app_session* session = app_session_current();
    char ch;

    app_session_push_wait_scope(session, &wait_scope,
        APP_WAIT_REASON_INFORMATIONAL_PAUSE, 0, 0);
    ch = inkey();
    app_session_pop_wait_scope(session, &wait_scope);
    return ch;
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
        if (!app_ui_panel_add_row_ex(panel, (s16b)i, TERM_WHITE,
                (byte)lines[i].alert_color,
                (byte)lines[i].monster_color, lines[i].monster_character,
                true, false, lines[i].direction, lines[i].name,
                lines[i].stance))
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
        if (!app_ui_panel_add_row_ex(panel, (s16b)i,
                (byte)lines[i].name_color, (byte)lines[i].name_color,
                (byte)lines[i].object_color, lines[i].object_character,
                true, false, lines[i].direction, lines[i].name, ""))
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

static bool nearby_information_scene(nearby_scene_draw_fn draw_fn,
    cptr los_prompt, cptr screen_prompt, char toggle_key)
{
    app_session* session = app_session_current();
    nearby_scene_build_fn build_fn;
    char ch = toggle_key;
    bool show_los = true;

    if (!draw_fn || !los_prompt || !screen_prompt)
        return false;
    if (!nearby_snapshot_active() || !session)
        return false;

    build_fn = (draw_fn == show_nearby_monsters)
        ? nearby_monsters_build_scene
        : nearby_objects_build_scene;

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
        (void)Term_xtra(TERM_XTRA_FRESH, 0);

        inkey_set_cursor_hidden(true);
        ch = nearby_snapshot_wait_key();
        inkey_set_cursor_hidden(false);

        if (ch == toggle_key)
            show_los = !show_los;
    }

    app_session_clear_dungeon_overlay_scene(session);
    (void)Term_xtra(TERM_XTRA_FRESH, 0);
    return true;
}

void do_cmd_view_monsters()
{
    char get_char = '[';
    bool show_los = true;

    /* Clear entry level banner when using [ command */
    if (g_banner_force_redraw_remaining > 0)
    {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    if (nearby_information_scene(show_nearby_monsters,
            "Monsters you can see (press [ to toggle):",
            "Monsters on screen (press [ to toggle):", '['))
    {
        return;
    }

    while (get_char == '[')
    {
        screen_save();
        show_nearby_monsters(show_los);
        /* Show the prompt */
        if (show_los)
            prt("Monsters you can see (press [ to toggle):", 0, 0);
        else
            prt("Monsters on screen (press [ to toggle):", 0, 0);
        get_char = nearby_snapshot_wait_key();
        show_los = !show_los;
        screen_load();
    }
}

void do_cmd_view_objects()
{
    char get_char = ']';
    bool show_los = true;

    /* Clear entry level banner when using ] command */
    if (g_banner_force_redraw_remaining > 0)
    {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    if (nearby_information_scene(show_nearby_objects,
            "Objects you can see (press ] to toggle):",
            "Objects on screen (press ] to toggle):", ']'))
    {
        return;
    }

    while (get_char == ']')
    {
        screen_save();
        show_nearby_objects(show_los);
        /* Show the prompt */
        if (show_los)
            prt("Objects you can see (press ] to toggle):", 0, 0);
        else
            prt("Objects on screen (press ] to toggle):", 0, 0);
        get_char = nearby_snapshot_wait_key();
        show_los = !show_los;
        screen_load();
    }
}

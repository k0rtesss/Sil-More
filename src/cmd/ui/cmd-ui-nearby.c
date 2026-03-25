/* File: cmd-ui-nearby.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "platform-ui.h"
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
    char direction[12];
    char name[60];
};

void show_nearby_monsters(bool line_of_sight_only)
{
    view_monster_data_line lines[MAX_VIEW_LINES];

    int i, j;
    int col;
    int longest_name_length = 0;
    int longest_direction_length = 0;
    int longest_stance_length = 0;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    
    /* Get terminal height and calculate available space */
    int term_hgt = Term->hgt;
    int max_lines = MIN(MAX_VIEW_LINES, term_hgt - 3); /* Leave space for header and footer */

    get_sorted_target_list(TARGET_LIST_MONSTER, 0);

    j = 0;
    for (i = 0; i < temp_n; i++)
    {
        int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];
        monster_type* m_ptr = &mon_list[m_idx];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        char m_name[40];
        int name_length;

        if (j >= max_lines)
            break;
        if (!m_ptr->ml)
            continue;
        if (!player_has_los_bold(temp_y[i], temp_x[i]) && line_of_sight_only)
            continue;

        memset(lines[j].direction, '\0', sizeof(lines[j].direction));
        memset(lines[j].name, '\0', sizeof(lines[j].name));
        memset(lines[j].stance, '\0', sizeof(lines[j].stance));

        monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);
        name_length = strlen(m_name);

        longest_name_length = MAX(longest_name_length, name_length);

        write_direction_from_player_to_buffer(temp_y[i], temp_x[i],
            lines[j].direction, sizeof(lines[j].direction));
        if (!get_alertness_text(m_ptr, sizeof(lines[j].stance), lines[j].stance,
                &lines[j].alert_color))
            return;
        longest_direction_length = MAX(longest_direction_length,
            (int)strlen(lines[j].direction));
        longest_stance_length = MAX(longest_stance_length,
            (int)strlen(lines[j].stance));

        lines[j].monster_character = monster_char(r_ptr);
        lines[j].monster_color = monster_attr(r_ptr);

        lines[j].distance
            = distance(p_ptr->py, p_ptr->px, temp_y[i], temp_x[i]);

        strncpy(lines[j].name, m_name, sizeof(lines[j].name));

        j++;
    }

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
        int name_col = direction_col + MAX(longest_direction_length, 1);
        int stance_col = term_wid - MAX(longest_stance_length, 1) - 1;
        int name_width = stance_col - name_col - 1;
        bool show_stance = true;

        monster_char[0] = lines[i].monster_character;
        monster_char[1] = '\0';

        if (lines[i].distance < 5)
            distance_color = TERM_WHITE;
        else if (lines[i].distance < 10)
            distance_color = TERM_L_WHITE;
        else
            distance_color = TERM_L_DARK;

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

    int i, j;
    int col;
    int longest_name_length = 0;
    int longest_direction_length = 0;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    
    /* Get terminal height and calculate available space */
    int term_hgt = Term->hgt;
    int max_lines = MIN(MAX_VIEW_LINES, term_hgt - 3); /* Leave space for header and footer */

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    j = 0;
    for (i = 0; i < temp_n; i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        object_type* o_ptr = &o_list[o_idx];
        char o_name[60];
        int name_length;

        if (j >= max_lines)
            break;
        if (!player_can_see_bold(temp_y[i], temp_x[i]) && line_of_sight_only)
            continue;

        memset(lines[j].direction, '\0', sizeof(lines[j].direction));
        memset(lines[j].name, '\0', sizeof(lines[j].name));
        memset(o_name, '\0', sizeof(o_name));

        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        name_length = strlen(o_name);

        longest_name_length = MAX(longest_name_length, name_length);

        write_direction_from_player_to_buffer(temp_y[i], temp_x[i],
            lines[j].direction, sizeof(lines[j].direction));
        longest_direction_length = MAX(longest_direction_length,
            (int)strlen(lines[j].direction));

        lines[j].distance
            = distance(p_ptr->py, p_ptr->px, temp_y[i], temp_x[i]);

        if (strlen(lines[j].direction) == 0)
            strcpy(lines[j].direction, "underfoot"); 

        lines[j].object_character = object_char(o_ptr);
        lines[j].object_color = object_attr(o_ptr);

        strncpy(lines[j].name, o_name, sizeof(lines[j].name));

        j++;
    }

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
        int name_col = direction_col + MAX(longest_direction_length, 1);
        int name_width = term_wid - name_col - 1;

        char o_char[2];

        o_char[0] = lines[i].object_character;
        o_char[1] = '\0';

        if (lines[i].distance < 5)
            distance_color = TERM_WHITE;
        else if (lines[i].distance < 10)
            distance_color = TERM_L_WHITE;
        else
            distance_color = TERM_L_DARK;

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
        Term_putstr(name_col, i + 1, name_width, TERM_WHITE, lines[i].name);
    }

    if (j)
    {
        Term_erase(col, j + 1, term_wid - col);
    }
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

    while (get_char == '[')
    {
        screen_save();
        show_nearby_monsters(show_los);
        /* Show the prompt */
        if (show_los)
            prt("Monsters you can see (press [ to toggle):", 0, 0);
        else
            prt("Monsters on screen (press [ to toggle):", 0, 0);
        get_char = inkey();
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

    while (get_char == ']')
    {
        screen_save();
        show_nearby_objects(show_los);
        /* Show the prompt */
        if (show_los)
            prt("Objects you can see (press ] to toggle):", 0, 0);
        else
            prt("Objects on screen (press ] to toggle):", 0, 0);
        get_char = inkey();
        show_los = !show_los;
        screen_load();
    }
}

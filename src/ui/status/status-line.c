#include "angband.h"
#include "ui/ui-language.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "support/utf8.h"
#include "supplies.h"
#include "item_set.h"
#include "ui/status/status-internal.h"

/*
 * Prints player's current song (if any)
 */
void prt_song(void)
{
    if (ui_status_pane_owns_left_panel_statuses())
    {
        if (!ui_hide_left_panel())
        {
            Term_erase(COL_SONG, ROW_SONG, LEFT_PANEL_CONTENT_WID);
            if (!ui_compact_height())
                Term_erase(COL_SONG, ROW_SONG + 1, LEFT_PANEL_CONTENT_WID);
        }
        return;
    }

    if (ui_compact_status_line_handles_song())
    {
        prt_status_line_compact();
        return;
    }

    // wipe old songs
    Term_erase(COL_SONG, ROW_SONG, LEFT_PANEL_CONTENT_WID);
    if (!ui_compact_height())
        Term_erase(COL_SONG, ROW_SONG + 1, LEFT_PANEL_CONTENT_WID);

    char* song1_name
        = b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;
    char* song2_name
        = b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;

    sdl_story_font_enable();

    if (ui_compact_height())
    {
        /* Compact height: render a single combined song line. */
        char buf[32] = "";
        if (p_ptr->song1 != SNG_NOTHING && p_ptr->song2 != SNG_NOTHING)
            strnfmt(buf, sizeof(buf), "%s+%s", song1_name + 8, song2_name + 8);
        else if (p_ptr->song1 != SNG_NOTHING)
            SDL_strlcpy(buf, song1_name + 8, sizeof(buf));
        else if (p_ptr->song2 != SNG_NOTHING)
            SDL_strlcpy(buf, song2_name + 8, sizeof(buf));

        if (buf[0])
            Term_putstr(COL_SONG, ROW_SONG, LEFT_PANEL_CONTENT_WID,
                panel_touch_zone_attr(SDL_PANEL_CLICK_SONG, ROW_SONG,
                    TERM_L_BLUE),
                buf);
    }
    else
    {
        // show the first song
        if (p_ptr->song1 != SNG_NOTHING)
        {
            Term_putstr(COL_SONG, ROW_SONG, LEFT_PANEL_CONTENT_WID,
                panel_touch_zone_attr(SDL_PANEL_CLICK_SONG, ROW_SONG,
                    TERM_L_BLUE),
                song1_name + 8);
        }

        // show the second song
        if (p_ptr->song2 != SNG_NOTHING)
        {
            Term_putstr(COL_SONG, ROW_SONG + 1, LEFT_PANEL_CONTENT_WID,
                panel_touch_zone_attr(SDL_PANEL_CLICK_SONG, ROW_SONG + 1,
                    TERM_BLUE),
                song2_name + 8);
        }
    }

    sdl_story_font_disable();
}

/*
 * Prints depth in stat area
 */
void prt_depth(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    if (ui_depth_menu_owns_left_panel_depth())
        return;

    char depths[32];
    s16b attr = TERM_WHITE;

    if (!p_ptr->depth)
    {
        SDL_strlcpy(depths, ui_text("Surface"), sizeof(depths));
    }
    else
    {
        strnfmt(depths, sizeof(depths), ui_text("%d ft"), p_ptr->depth * 50);
    }
    depths[utf8_safe_prefix_len(depths, (int)strlen(depths))] = '\0';

    /* Get color of level based on feeling  -JSV- */
    if ((p_ptr->depth) && (do_feeling))
    {
        if (feeling == 1)
            attr = TERM_VIOLET;
        else if (feeling == 2)
            attr = TERM_RED;
        else if (feeling == 3)
            attr = TERM_L_RED;
        else if (feeling == 4)
            attr = TERM_ORANGE;
        else if (feeling == 5)
            attr = TERM_ORANGE;
        else if (feeling == 6)
            attr = TERM_YELLOW;
        else if (feeling == 7)
            attr = TERM_YELLOW;
        else if (feeling == 8)
            attr = TERM_WHITE;
        else if (feeling == 9)
            attr = TERM_WHITE;
        else if (feeling == 10)
            attr = TERM_L_WHITE;
        else if (feeling >= LEV_THEME_HEAD)
            attr = TERM_BLUE;
    }

    attr = status_touch_zone_attr(SDL_STATUS_CLICK_MAP, COL_DEPTH, 7,
        (byte)attr);

    sdl_story_font_enable();

    /* Right-Adjust the "depth", and clear old values */
    c_prt(attr, format("%7s", depths), ROW_DEPTH, COL_DEPTH);

    sdl_story_font_disable();

    prt_status_line_view_button();
}

/*
 * Prints status of hunger
 */
void prt_hunger(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    sdl_story_font_enable();

    /* Fainting / Starving */
    if (p_ptr->food < PY_FOOD_STARVE)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_RED), ui_text("Starving"), ROW_HUNGRY, COL_HUNGRY);
    }

    /* Weak */
    else if (p_ptr->food < PY_FOOD_WEAK)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_ORANGE), ui_text("Weak    "), ROW_HUNGRY,
            COL_HUNGRY);
    }

    /* Hungry */
    else if (p_ptr->food < PY_FOOD_ALERT)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_YELLOW), ui_text("Hungry  "), ROW_HUNGRY,
            COL_HUNGRY);
    }

    /* Normal */
    else if (p_ptr->food < PY_FOOD_FULL)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_L_GREEN), "        ", ROW_HUNGRY,
            COL_HUNGRY);
    }

    /* Full */
    else if (p_ptr->food < PY_FOOD_MAX)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_L_GREEN), ui_text("Full    "), ROW_HUNGRY,
            COL_HUNGRY);
    }

    else
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_GREEN), ui_text("Full    "), ROW_HUNGRY,
            COL_HUNGRY);
    }

    sdl_story_font_disable();
}

/*
 * Prints Blind status
 */
void prt_blind(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    sdl_story_font_enable();

    if (p_ptr->blind)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_BLIND, 5, TERM_ORANGE), "Blind", ROW_BLIND, COL_BLIND);
    }
    else
    {
        put_str("     ", ROW_BLIND, COL_BLIND);
    }

    sdl_story_font_disable();
}

/*
 * Prints Confusion status
 */
void prt_confused(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_CONFUSED, ROW_CONFUSED, 8);

    if (p_ptr->confused)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_CONFUSED, 8, TERM_ORANGE), ui_text("Confused"), ROW_CONFUSED,
            COL_CONFUSED);
        sdl_story_font_disable();
    }
}

/*
 * Prints Fear status
 */
void prt_afraid(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_AFRAID, ROW_AFRAID, 6);

    if (p_ptr->afraid)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_AFRAID, 6, TERM_ORANGE), ui_text("Afraid"), ROW_AFRAID,
            COL_AFRAID);
        sdl_story_font_disable();
    }
}

/*
 *  Displays the amount of bleeding.
 *  This is a bit tricky as it is in the same row as poison, *unless* you have
 * both. In which case it is the row above.
 */

void prt_cut(void)
{
    if (ui_compact_status_line_handles_wounds())
    {
        prt_status_line_compact();
        return;
    }

    if (ui_hide_left_panel())
        return;

    if (ui_status_pane_owns_left_panel_statuses())
    {
        Term_erase(COL_CUT, ROW_CUT - 1, 12);
        Term_erase(COL_CUT, ROW_CUT, 12);
        return;
    }

    if (ui_status_system_compact() && ui_wound_rows_overlap_status_line())
        return;

    if (ui_compact_height())
    {
        prt_cut_poisoned_compact();
        return;
    }

    int c = p_ptr->cut;
    char num_buf[8];

    int r = ROW_CUT;

    if (p_ptr->poisoned)
        r--;

    /* Clear both possible rows (story font has variable widths) */
    Term_erase(COL_CUT, ROW_CUT - 1, 12);
    if (!p_ptr->poisoned)
        Term_erase(COL_CUT, ROW_CUT, 12);

    if (c > 100)
    {
        sdl_story_font_enable();
        c_put_str(TERM_RED, ui_text("Mortal wound"), r, COL_CUT);
        sdl_story_font_disable();
    }
    else if (c > 20)
    {
        sdl_story_font_enable();
        c_put_str(TERM_RED, ui_text("Bleeding"), r, COL_CUT);
        sdl_story_font_disable();
        sprintf(num_buf, " %-2d", c);
        c_put_str(TERM_RED, num_buf, r, COL_CUT + 8);
    }
    else if (c > 0)
    {
        sdl_story_font_enable();
        c_put_str(TERM_L_RED, ui_text("Bleeding"), r, COL_CUT);
        sdl_story_font_disable();
        sprintf(num_buf, " %-2d", c);
        c_put_str(TERM_L_RED, num_buf, r, COL_CUT + 8);
    }
}

/*
 * Prints Poisoned status
 */
void prt_poisoned(void)
{
    if (ui_compact_status_line_handles_wounds())
    {
        prt_status_line_compact();
        return;
    }

    if (ui_hide_left_panel())
        return;

    if (ui_status_pane_owns_left_panel_statuses())
    {
        Term_erase(COL_POISONED, ROW_POISONED, 12);
        return;
    }

    if (ui_status_system_compact() && ui_wound_rows_overlap_status_line())
        return;

    if (ui_compact_height())
    {
        prt_cut_poisoned_compact();
        return;
    }

    int p = p_ptr->poisoned;
    char num_buf[8];

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_POISONED, ROW_POISONED, 12);

    if (p > 20)
    {
        sdl_story_font_enable();
        c_put_str(TERM_L_GREEN, ui_text("Poisoned"), ROW_POISONED, COL_POISONED);
        sdl_story_font_disable();
        sprintf(num_buf, " %-3d", p);
        c_put_str(TERM_L_GREEN, num_buf, ROW_POISONED, COL_POISONED + 8);
    }
    else if (p > 0)
    {
        sdl_story_font_enable();
        c_put_str(TERM_GREEN, ui_text("Poisoned"), ROW_POISONED, COL_POISONED);
        sdl_story_font_disable();
        sprintf(num_buf, " %-3d", p);
        c_put_str(TERM_GREEN, num_buf, ROW_POISONED, COL_POISONED + 8);
    }
}

enum { STATUS_STATE_WIDTH = COL_SPEED - COL_STATE };

static void status_count_text(char *text, size_t size, cptr label, cptr count)
{
    int label_len = (int)strlen(label);
    while (label_len && label[label_len - 1] == ' ')
        label_len--;

    /* The terminal grid stores UTF-8 bytes. Reserve room for the counter
     * and keep the original right alignment when the label fits. */
    int minimum_count_width = 1 + (int)strlen(count);
    int prefix_len = utf8_safe_prefix_len(label,
        MIN(label_len, (int)size - 1 - minimum_count_width));
    strnfmt(text, size, "%.*s%*s", prefix_len, label,
        MAX(10 - prefix_len, minimum_count_width), count);
}

static void status_rest_text(char *text, size_t size, int turns)
{
    char count[6] = "";
    if (turns == -1)
        SDL_strlcpy(count, "*****", sizeof(count));
    else if (turns == -2)
        SDL_strlcpy(count, "&&&&&", sizeof(count));
    else if (turns > 0)
        strnfmt(count, sizeof(count), "%d", turns >= 1000 ? turns / 100 * 100 : turns);
    status_count_text(text, size, ui_text("Rest      "), count);
}

/* Prints the action or condition in the legacy status row. */
void prt_state(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    byte attr = TERM_WHITE;

    char text[STATUS_STATE_WIDTH + 1];

    /* Entrancement */
    if (p_ptr->entranced)
    {
        attr = TERM_RED;

        SDL_strlcpy(text, ui_text("Entranced!"), sizeof(text));
    }

    /* Searching the Pack */
    if (player_pack_action_pending())
    {
        strnfmt(text, sizeof(text), ui_text("Pack %5d"),
            player_pack_action_turns_left());
    }

    /* Smithing */
    else if (p_ptr->smithing)
    {
        SDL_strlcpy(text, ui_text("Smithing  "), sizeof(text));
    }

    if (p_ptr->fletching)
    {
        SDL_strlcpy(text, ui_text("Fletching "), sizeof(text));
    }
    else if (p_ptr->rage)
    {
        attr = TERM_RED;
        SDL_strlcpy(text, ui_text("Rage      "), sizeof(text));
    }

    /* Resting */
    else if (p_ptr->resting)
    {
        status_rest_text(text, sizeof(text), p_ptr->resting);
    }

    /* Repeating */
    else if (p_ptr->command_rep)
    {
        int repeat = p_ptr->command_rep;
        const char *format = repeat > 999 ? "Rep. %3d00" : "Repeat %3d";
        int count = repeat > 999 ? repeat / 100 : repeat;
        int len = snprintf(text, sizeof(text), ui_text(format), count);
        if (len < 0 || len >= (int)sizeof(text))
        {
            char count_text[7];
            strnfmt(count_text, sizeof(count_text), "%d", repeat > 999 ? count * 100 : count);
            status_count_text(text, sizeof(text), ui_text("Repeat"), count_text);
        }
    }

    /* Disease remains visible even while walking in stealth mode. */
    else if (p_ptr->diseased)
    {
        attr = TERM_ORANGE;
        SDL_strlcpy(text, ui_text("Diseased"), sizeof(text));
    }

    /* Stealth mode */
    else if (p_ptr->stealth_mode)
    {
        SDL_strlcpy(text, ui_text("Stealth   "), sizeof(text));
    }

    /* Nothing interesting */
    else
    {
        text[0] = '\0';
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_STATE, ROW_STATE, STATUS_STATE_WIDTH);

    /* Display the info if any */
    if (text[0])
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_STATE, STATUS_STATE_WIDTH, attr), text, ROW_STATE, COL_STATE);
        sdl_story_font_disable();
    }
    else
    {
        prt_status_line_main_menu_hint(false);
    }
}

/*
 * Prints the speed of a character.			-CJS-
 */
void prt_speed(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    int i = player_current_movement_speed();

    byte attr = TERM_WHITE;
    char buf[32] = "";

    /* Fast */
    if (i > 2)
    {
        attr = TERM_L_GREEN;
        SDL_strlcpy(buf, ui_text("Fast"), sizeof(buf));
    }

    /* Very slow */
    else if (i < 1)
    {
        attr = TERM_ORANGE;
        SDL_strlcpy(buf, ui_text("VSlow"), sizeof(buf));
    }

    /* Slow */
    else if (i < 2)
    {
        attr = TERM_ORANGE;
        SDL_strlcpy(buf, ui_text("Slow"), sizeof(buf));
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_SPEED, ROW_SPEED, 5);

    /* Display the speed if not normal */
    if (buf[0])
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_SPEED, 5, attr), buf, ROW_SPEED, COL_SPEED);
        sdl_story_font_disable();
    }
}

static const char* partition_abbrev_for_point(int y, int x)
{
    switch (level_partition_kind_for_point(y, x))
    {
    case LEVEL_PART_ROOMY:
        return "Room";
    case LEVEL_PART_RUINED:
        return "Ruin";
    case LEVEL_PART_CAVEY:
        return "Caves";
    case LEVEL_PART_BIG_CAVE:
        return "BigCa";
    case LEVEL_PART_LABYRINTH:
        return "Labir";
    case LEVEL_PART_CHASM:
        return "Chasm";
    default:
        return "";
    }
}

void prt_partition(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    if (!p_ptr)
        return;

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_PARTITION, ROW_PARTITION, 5);

    const char* label = ui_text(partition_abbrev_for_point(p_ptr->py, p_ptr->px));
    if (!label[0])
        return;

    sdl_story_font_enable();
    c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAP, COL_PARTITION,
        (int)strlen(label), TERM_WHITE), label, ROW_PARTITION, COL_PARTITION);
    sdl_story_font_disable();
}

/*
 * Prints message regarding difficult terrain
 */
void prt_terrain(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_TERRAIN, ROW_TERRAIN, 5);

    if (cave_pit_bold(p_ptr->py, p_ptr->px))
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_TERRAIN, 3, TERM_ORANGE), ui_text("Pit"), ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_TERRAIN, 3, TERM_ORANGE), ui_text("Web"), ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_TERRAIN, 3, TERM_YELLOW), ui_text("Sun"), ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }

    prt_partition();
}

void prt_cut_poisoned_compact(void)
{
    if (!Term || !p_ptr)
        return;

    const int row = ROW_CUT;
    const int col = COL_CUT;
    const int width = 12;

    Term_erase(col, row, width);

    int x = col;

    int c = p_ptr->cut;
    int p = p_ptr->poisoned;

    if (c > 0)
    {
        byte cut_attr = (c > 20) ? TERM_RED : TERM_L_RED;
        char cut_buf[16];

        if (c > 100)
        {
            cut_attr = TERM_RED;
            SDL_strlcpy(cut_buf, "MW", sizeof(cut_buf));
        }
        else
        {
            strnfmt(cut_buf, sizeof(cut_buf), ui_text("Bld:%d"), c);
        }

        int len = (int)strlen(cut_buf);
        if (x + len > col + width)
            len = (col + width) - x;
        if (len > 0)
            Term_putstr(x, row, len, cut_attr, cut_buf);
        x += len;
    }

    if (p > 0 && x < col + width)
    {
        if (c > 0 && x < col + width)
        {
            Term_putstr(x, row, 1, TERM_WHITE, " ");
            x++;
        }

        byte pois_attr = (p > 20) ? TERM_L_GREEN : TERM_GREEN;
        char pois_buf[16];
        strnfmt(pois_buf, sizeof(pois_buf), ui_text("Poi:%d"), p);
        int len = (int)strlen(pois_buf);
        if (x + len > col + width)
            len = (col + width) - x;
        if (len > 0)
            Term_putstr(x, row, len, pois_attr, pois_buf);
    }
}

void prt_stun(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    int s = p_ptr->stun;

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_STUN, ROW_STUN, 12);

    if (s > 100)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_STUN, 11, TERM_RED), ui_text("Knocked out"), ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
    else if (s > 50)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_STUN, 10, TERM_ORANGE), ui_text("Heavy stun"), ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
    else if (s)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_STUN, 4, TERM_ORANGE), ui_text("Stun"), ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
}

typedef struct {
    const char* long_text;
    const char* short_text;
    byte attr;
    bool required;
    int click_action;
} status_seg;

static int status_line_len(const status_seg* segs, int count, bool use_long,
                           const bool* include)
{
    int len = 0;
    int shown = 0;
    for (int i = 0; i < count; i++)
    {
        if (include && !include[i])
            continue;
        const char* t = use_long ? segs[i].long_text : segs[i].short_text;
        if (!t || !t[0])
            continue;
        if (shown > 0)
            len += 1;
        len += (int)strlen(t);
        shown++;
    }
    return len;
}

static byte status_depth_attr(void)
{
    s16b attr = TERM_WHITE;

    if ((p_ptr->depth) && (do_feeling))
    {
        if (feeling == 1)
            attr = TERM_VIOLET;
        else if (feeling == 2)
            attr = TERM_RED;
        else if (feeling == 3)
            attr = TERM_L_RED;
        else if (feeling == 4)
            attr = TERM_ORANGE;
        else if (feeling == 5)
            attr = TERM_ORANGE;
        else if (feeling == 6)
            attr = TERM_YELLOW;
        else if (feeling == 7)
            attr = TERM_YELLOW;
        else if (feeling == 8)
            attr = TERM_WHITE;
        else if (feeling == 9)
            attr = TERM_WHITE;
        else if (feeling == 10)
            attr = TERM_L_WHITE;
        else if (feeling >= LEV_THEME_HEAD)
            attr = TERM_BLUE;
    }

    return (byte)attr;
}

bool status_state_text(char* out_long, size_t out_long_sz,
                              char* out_short, size_t out_short_sz,
                              byte* out_attr)
{
    if (!p_ptr)
        return false;

    out_long[0] = '\0';
    out_short[0] = '\0';
    if (out_attr)
        *out_attr = TERM_WHITE;

    if (p_ptr->entranced)
    {
        if (out_attr)
            *out_attr = TERM_RED;
        SDL_strlcpy(out_long, ui_text("Entranced"), out_long_sz);
        SDL_strlcpy(out_short, ui_text("En"), out_short_sz);
        return true;
    }

    if (player_pack_action_pending())
    {
        strnfmt(out_long, out_long_sz, ui_text("Pack %d"),
            player_pack_action_turns_left());
        strnfmt(out_short, out_short_sz, ui_text("Pk%d"),
            player_pack_action_turns_left());
        return true;
    }

    if (p_ptr->smithing)
    {
        SDL_strlcpy(out_long, ui_text("Smithing"), out_long_sz);
        SDL_strlcpy(out_short, "Sm", out_short_sz);
        return true;
    }

    if (p_ptr->fletching)
    {
        SDL_strlcpy(out_long, ui_text("Fletching"), out_long_sz);
        SDL_strlcpy(out_short, "Fl", out_short_sz);
        return true;
    }

    if (p_ptr->rage)
    {
        if (out_attr)
            *out_attr = TERM_RED;
        SDL_strlcpy(out_long, ui_text("Rage"), out_long_sz);
        SDL_strlcpy(out_short, "Rg", out_short_sz);
        return true;
    }

    if (p_ptr->resting)
    {
        int n = p_ptr->resting;
        if (n == -1)
        {
            SDL_strlcpy(out_long, ui_text("Rest*"), out_long_sz);
            SDL_strlcpy(out_short, "R*", out_short_sz);
        }
        else if (n == -2)
        {
            SDL_strlcpy(out_long, ui_text("Rest&"), out_long_sz);
            SDL_strlcpy(out_short, ui_text("R&"), out_short_sz);
        }
        else if (n >= 1000)
        {
            strnfmt(out_long, out_long_sz, ui_text("Rest %d"), n);
            strnfmt(out_short, out_short_sz, ui_text("R%dk"), n / 1000);
        }
        else
        {
            strnfmt(out_long, out_long_sz, ui_text("Rest %d"), n);
            strnfmt(out_short, out_short_sz, "R%d", n);
        }
        return true;
    }

    if (p_ptr->command_rep)
    {
        strnfmt(out_long, out_long_sz, ui_text("Repeat %d"), p_ptr->command_rep);
        strnfmt(out_short, out_short_sz, "Rp%d", p_ptr->command_rep);
        return true;
    }

    if (p_ptr->stealth_mode)
    {
        SDL_strlcpy(out_long, ui_text("Stealth"), out_long_sz);
        SDL_strlcpy(out_short, ui_text("St"), out_short_sz);
        return true;
    }

    return false;
}

static bool status_state_is_critical(cptr state)
{
    if (!state || !state[0])
        return false;

    return streq(state, ui_text("Entranced")) || streq(state, ui_text("Stealth"));
}

static const char* status_partition_short(const char* long_label)
{
    if (!long_label || !long_label[0])
        return "";
    if (!strcmp(long_label, "Room"))
        return "Rm";
    if (!strcmp(long_label, "Ruin"))
        return "Ru";
    if (!strcmp(long_label, "Caves"))
        return "Cv";
    if (!strcmp(long_label, "BigCa"))
        return "BC";
    if (!strcmp(long_label, "Labir"))
        return "Lb";
    if (!strcmp(long_label, "Chasm"))
        return "Ch";
    return long_label;
}

static bool status_line_span_blank(int row, int col, int width)
{
    if (!Term || !Term->scr || !Term->scr->c)
        return false;
    if (row < 0 || row >= Term->hgt)
        return false;
    if (col < 0 || width <= 0 || col + width > Term->wid)
        return false;

    for (int i = 0; i < width; i++)
    {
        unsigned char ch = (unsigned char)Term->scr->c[row][col + i];

        if (ch && ch != (unsigned char)Term->char_blank && ch != ' ')
            return false;
    }

    return true;
}

void prt_status_line_main_menu_hint(bool compact_centered)
{
    const char* label = ui_text(STATUS_MAIN_MENU_HINT);
    int len = (int)strlen(label);
    int row = ROW_STATE;
    int col = COL_STATE;
    byte attr = status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
        -1, 0, TERM_SLATE);

    if (!Term || !p_ptr || len <= 0)
        return;
    if (row < 0 || row >= Term->hgt)
        return;
    if (Term->wid < len)
        return;

    if (compact_centered)
    {
        if (Term->wid < len + 2)
            return;

        col = (Term->wid - len) / 2;
        if (!status_line_span_blank(row, col - 1, len + 2))
            return;
    }
    else if (col < 0 || col + len > Term->wid)
    {
        return;
    }

    if (!compact_centered)
        sdl_story_font_enable();
    Term_putstr(col, row, len, attr, label);
    if (!compact_centered)
        sdl_story_font_disable();
}

static bool status_line_choose_view_button(cptr* label, int* col, int* len)
{
    cptr labels[] = { ui_text(STATUS_VIEW_LABEL), ui_text(STATUS_VIEW_LABEL_SHORT) };

    if (!Term || !label || !col || !len)
        return false;
    if (ROW_STATUS < 0 || ROW_STATUS >= Term->hgt)
        return false;

    for (size_t i = 0; i < N_ELEMENTS(labels); i++)
    {
        int label_len = (int)strlen(labels[i]);
        int label_col = Term->wid - label_len;
        int clear_col = label_col > 0 ? label_col - 1 : label_col;
        int clear_width = label_len + (label_col > 0 ? 1 : 0);

        if (label_len <= 0 || label_col < 0)
            continue;
        if (!status_line_span_blank(ROW_STATUS, clear_col, clear_width))
            continue;

        *label = labels[i];
        *col = label_col;
        *len = label_len;
        return true;
    }

    return false;
}

static bool status_line_right_label_matches(cptr label)
{
    int len;
    int col;

    if (!Term || !Term->scr || !Term->scr->c || !label)
        return false;
    if (ROW_STATUS < 0 || ROW_STATUS >= Term->hgt)
        return false;

    len = (int)strlen(label);
    col = Term->wid - len;
    if (len <= 0 || col < 0)
        return false;

    for (int i = 0; i < len; i++)
    {
        unsigned char actual = (unsigned char)Term->scr->c[ROW_STATUS][col + i];
        unsigned char expected = (unsigned char)label[i];

        if (!actual || actual == (unsigned char)Term->char_blank)
            actual = ' ';
        if (actual != expected)
            return false;
    }

    return true;
}

static void prt_status_line_clear_view_button(void)
{
    cptr labels[] = { ui_text(STATUS_VIEW_LABEL), ui_text(STATUS_VIEW_LABEL_SHORT) };

    if (!Term || ui_status_system_compact())
        return;

    for (size_t i = 0; i < N_ELEMENTS(labels); i++)
    {
        int len = (int)strlen(labels[i]);
        int col = Term->wid - len;

        if (col >= 0 && status_line_right_label_matches(labels[i]))
            Term_erase(col, ROW_STATUS, len);
    }
}

void prt_status_line_view_button(void)
{
    cptr label = NULL;
    int col = 0;
    int len = 0;
    byte attr;

    prt_status_line_clear_view_button();

    if (!status_line_choose_view_button(&label, &col, &len))
        return;

    attr = status_touch_zone_attr(SDL_STATUS_CLICK_VIEW, col, len,
        TERM_L_BLUE);

    if (ui_status_system_compact())
    {
        Term_putstr(col, ROW_STATUS, len, attr, label);
    }
    else
    {
        sdl_story_font_enable();
        c_put_str(attr, label, ROW_STATUS, col);
        sdl_story_font_disable();
    }
}

void prt_status_line_compact(void)
{
    if (!Term || !p_ptr)
        return;

    const int row = ROW_STATE;
    if (row < 0)
        return;

    Term_erase(0, row, Term->wid);

    status_seg segs[16];
    int seg_count = 0;
    bool fold_song = ui_compact_status_line_handles_song();
    bool fold_wounds = ui_compact_status_line_handles_wounds();

    char hunger_long[16] = "";
    char hunger_short[8] = "";
    byte hunger_attr = TERM_WHITE;
    bool hunger_required = false;

    if (p_ptr->food < PY_FOOD_STARVE) {
        SDL_strlcpy(hunger_long, ui_text("Starving"), sizeof(hunger_long));
        SDL_strlcpy(hunger_short, ui_text("St"), sizeof(hunger_short));
        hunger_attr = TERM_RED;
        hunger_required = true;
    } else if (p_ptr->food < PY_FOOD_WEAK) {
        SDL_strlcpy(hunger_long, ui_text("Weak"), sizeof(hunger_long));
        SDL_strlcpy(hunger_short, "Wk", sizeof(hunger_short));
        hunger_attr = TERM_ORANGE;
        hunger_required = true;
    } else if (p_ptr->food < PY_FOOD_ALERT) {
        SDL_strlcpy(hunger_long, ui_text("Hungry"), sizeof(hunger_long));
        SDL_strlcpy(hunger_short, "Hu", sizeof(hunger_short));
        hunger_attr = TERM_YELLOW;
        hunger_required = true;
    } else if (p_ptr->food >= PY_FOOD_FULL) {
        SDL_strlcpy(hunger_long, ui_text("Full"), sizeof(hunger_long));
        SDL_strlcpy(hunger_short, ui_text("Fu"), sizeof(hunger_short));
        hunger_attr = TERM_L_GREEN;
    }

    char stun_long[16] = "";
    char stun_short[8] = "";
    byte stun_attr = TERM_WHITE;
    if (p_ptr->stun > 100) {
        SDL_strlcpy(stun_long, ui_text("Knocked out"), sizeof(stun_long));
        SDL_strlcpy(stun_short, "KO", sizeof(stun_short));
        stun_attr = TERM_RED;
    } else if (p_ptr->stun > 50) {
        SDL_strlcpy(stun_long, ui_text("Heavy stun"), sizeof(stun_long));
        SDL_strlcpy(stun_short, "HS", sizeof(stun_short));
        stun_attr = TERM_ORANGE;
    } else if (p_ptr->stun) {
        SDL_strlcpy(stun_long, ui_text("Stun"), sizeof(stun_long));
        SDL_strlcpy(stun_short, ui_text("St"), sizeof(stun_short));
        stun_attr = TERM_ORANGE;
    }

    char state_long[24] = "";
    char state_short[12] = "";
    byte state_attr = TERM_WHITE;
    (void)status_state_text(state_long, sizeof(state_long), state_short,
        sizeof(state_short), &state_attr);
    bool state_required = status_state_is_critical(state_long);

    char cut_long[16] = "";
    char cut_short[8] = "";
    byte cut_attr = TERM_WHITE;
    if (fold_wounds)
    {
        if (p_ptr->cut > 100) {
            SDL_strlcpy(cut_long, ui_text("Mortal"), sizeof(cut_long));
            SDL_strlcpy(cut_short, "MW", sizeof(cut_short));
            cut_attr = TERM_RED;
        } else if (p_ptr->cut > 20) {
            strnfmt(cut_long, sizeof(cut_long), ui_text("Bleed %d"), p_ptr->cut);
            strnfmt(cut_short, sizeof(cut_short), "B%d", p_ptr->cut);
            cut_attr = TERM_RED;
        } else if (p_ptr->cut > 0) {
            strnfmt(cut_long, sizeof(cut_long), ui_text("Bleed %d"), p_ptr->cut);
            strnfmt(cut_short, sizeof(cut_short), "B%d", p_ptr->cut);
            cut_attr = TERM_L_RED;
        }
    }

    char pois_long[16] = "";
    char pois_short[8] = "";
    byte pois_attr = TERM_WHITE;
    if (fold_wounds)
    {
        if (p_ptr->poisoned > 20) {
            strnfmt(pois_long, sizeof(pois_long), ui_text("Poison %d"), p_ptr->poisoned);
            strnfmt(pois_short, sizeof(pois_short), "P%d", p_ptr->poisoned);
            pois_attr = TERM_L_GREEN;
        } else if (p_ptr->poisoned > 0) {
            strnfmt(pois_long, sizeof(pois_long), ui_text("Poison %d"), p_ptr->poisoned);
            strnfmt(pois_short, sizeof(pois_short), "P%d", p_ptr->poisoned);
            pois_attr = TERM_GREEN;
        }
    }

    char speed_long[16] = "";
    char speed_short[8] = "";
    byte speed_attr = TERM_WHITE;
    int movement_speed = player_current_movement_speed();
    if (movement_speed > 2) {
        SDL_strlcpy(speed_long, ui_text("Fast"), sizeof(speed_long));
        SDL_strlcpy(speed_short, ui_text("Fa"), sizeof(speed_short));
        speed_attr = TERM_L_GREEN;
    } else if (movement_speed < 1) {
        SDL_strlcpy(speed_long, ui_text("Very slow"), sizeof(speed_long));
        SDL_strlcpy(speed_short, "VS", sizeof(speed_short));
        speed_attr = TERM_ORANGE;
    } else if (movement_speed < 2) {
        SDL_strlcpy(speed_long, ui_text("Slow"), sizeof(speed_long));
        SDL_strlcpy(speed_short, "Sl", sizeof(speed_short));
        speed_attr = TERM_ORANGE;
    }

    char terrain_long[8] = "";
    char terrain_short[4] = "";
    byte terrain_attr = TERM_ORANGE;
    if (cave_pit_bold(p_ptr->py, p_ptr->px)) {
        SDL_strlcpy(terrain_long, ui_text("Pit"), sizeof(terrain_long));
        SDL_strlcpy(terrain_short, "Pt", sizeof(terrain_short));
    } else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB) {
        SDL_strlcpy(terrain_long, ui_text("Web"), sizeof(terrain_long));
        SDL_strlcpy(terrain_short, "Wb", sizeof(terrain_short));
    } else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT) {
        SDL_strlcpy(terrain_long, ui_text("Sun"), sizeof(terrain_long));
        SDL_strlcpy(terrain_short, "Sn", sizeof(terrain_short));
        terrain_attr = TERM_YELLOW;
    }

    const char* part_long = partition_abbrev_for_point(p_ptr->py, p_ptr->px);
    const char* part_short = status_partition_short(part_long);

    char depth_long[16] = "";
    char depth_short[16] = "";
    int feet = p_ptr->depth * 50;
    if (!p_ptr->depth) {
        SDL_strlcpy(depth_long, ui_text("Surface"), sizeof(depth_long));
        SDL_strlcpy(depth_short, "0'", sizeof(depth_short));
    } else {
        strnfmt(depth_long, sizeof(depth_long), ui_text("%d ft"), feet);
        strnfmt(depth_short, sizeof(depth_short), "%d'", feet);
    }
    byte depth_attr = status_depth_attr();

    char song_long[32] = "";
    char song_short[12] = "";
    if (fold_song && (p_ptr->song1 != SNG_NOTHING || p_ptr->song2 != SNG_NOTHING))
    {
        char* song1_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;
        char* song2_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;

        if (p_ptr->song1 != SNG_NOTHING && p_ptr->song2 != SNG_NOTHING)
            strnfmt(song_long, sizeof(song_long), "%s+%s", song1_name + 8,
                song2_name + 8);
        else if (p_ptr->song1 != SNG_NOTHING)
            SDL_strlcpy(song_long, song1_name + 8, sizeof(song_long));
        else if (p_ptr->song2 != SNG_NOTHING)
            SDL_strlcpy(song_long, song2_name + 8, sizeof(song_long));

        if (song_long[0])
            strnfmt(song_short, sizeof(song_short), "S:%.*s", 6, song_long);
    }

    #define ADD_SEG(LTXT, STXT, ATTR, REQ, ACTION) \
        do { \
            if ((LTXT)[0]) { \
                segs[seg_count].long_text = (LTXT); \
                segs[seg_count].short_text = (STXT)[0] ? (STXT) : (LTXT); \
                segs[seg_count].attr = (ATTR); \
                segs[seg_count].required = (REQ); \
                segs[seg_count].click_action = (ACTION); \
                seg_count++; \
            } \
        } while (0)

    ADD_SEG(hunger_long, hunger_short, hunger_attr, hunger_required,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(p_ptr->blind ? "Blind" : "", "Bl", TERM_ORANGE, true,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(p_ptr->confused ? ui_text("Confused") : "", ui_text("Cn"), TERM_ORANGE, true,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(cut_long, cut_short, cut_attr, true, SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(pois_long, pois_short, pois_attr, true, SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(p_ptr->diseased ? ui_text("Diseased") : "", ui_text("Dis"), TERM_ORANGE, true,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(stun_long, stun_short, stun_attr, true, SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(p_ptr->afraid ? ui_text("Afraid") : "", ui_text("Af"), TERM_ORANGE, true,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(song_long, song_short, TERM_L_BLUE, false, SDL_STATUS_CLICK_SONG);
    ADD_SEG(state_long, state_short, state_attr, state_required,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(speed_long, speed_short, speed_attr, true,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(terrain_long, terrain_short, terrain_attr, true,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(ui_text(part_long), ui_text(part_short), TERM_WHITE, false,
        SDL_STATUS_CLICK_MAP);
    ADD_SEG(depth_long, depth_short, depth_attr, false, SDL_STATUS_CLICK_MAP);

    #undef ADD_SEG

    int max_w = Term->wid;
    if (max_w <= 0)
        return;

    bool include[16];
    for (int i = 0; i < seg_count; i++)
        include[i] = true;

    bool use_long = (status_line_len(segs, seg_count, true, include) <= max_w);
    if (!use_long)
    {
        while (status_line_len(segs, seg_count, false, include) > max_w)
        {
            bool dropped = false;
            for (int i = seg_count - 1; i >= 0; i--)
            {
                if (!include[i])
                    continue;
                if (segs[i].required)
                    continue;
                include[i] = false;
                dropped = true;
                break;
            }
            if (!dropped)
                break;
        }
    }

    int x = 0;
    bool first = true;
    for (int i = 0; i < seg_count; i++)
    {
        if (!include[i])
            continue;
        const char* t = use_long ? segs[i].long_text : segs[i].short_text;
        if (!t || !t[0])
            continue;

        if (!first)
        {
            if (x < max_w)
                Term_putstr(x, row, 1, TERM_WHITE, " ");
            x++;
        }

        int remaining = max_w - x;
        if (remaining <= 0)
            break;
        int n = (int)strlen(t);
        if (n > remaining)
            n = remaining;
        if (n > 0)
            Term_putstr(x, row, n,
                status_touch_zone_attr(segs[i].click_action, x, n,
                    segs[i].attr),
                t);
        x += n;
        first = false;
    }

    prt_status_line_view_button();
    prt_status_line_main_menu_hint(true);
}

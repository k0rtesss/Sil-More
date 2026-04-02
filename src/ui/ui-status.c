/* File: ui/ui-status.c */

#include "angband.h"
#include "app/app-session.h"
#include "externs.h"

#include "ui/ui-character-screen.h"
#include "ui/ui-status.h"

#include "log/log.h"
#include "platform-config.h"
#include "platform-story-font.h"
#include "metarun.h"
#include "player/player-calc.h"
#include "player/identification.h"
#include "runtime-cli.h"

static bool ui_compact_width(void)
{
    return (Term && (Term->wid < 80));
}

static bool ui_hide_left_panel(void)
{
    return get_sdl_hide_left_panel();
}

static bool ui_compact_height(void)
{
    return SIL_UI_COMPACT_HEIGHT;
}

static bool ui_compact_status_line_handles_song(void)
{
    return ui_compact_width() && (ROW_SONG >= ROW_STATE);
}

static bool ui_compact_status_line_handles_wounds(void)
{
    return ui_compact_width() && (ROW_CUT >= ROW_STATE);
}

typedef struct hidden_overlay_line {
    char text[32];
    byte attr;
} hidden_overlay_line;

static void prt_status_line_compact(void);
static void prt_cut_poisoned_compact(void);
static void prt_hidden_top_vitals(void);
static bool status_state_text(char* out_long, size_t out_long_sz,
                              char* out_short, size_t out_short_sz,
                              byte* out_attr);
static void hidden_left_panel_add_line(hidden_overlay_line* lines, int* count,
                                       int max_lines, byte attr, cptr text);
static int hidden_left_panel_build_lines(hidden_overlay_line* lines, int max_lines);

/*
 * Converts stat num into a two-char (right justified) string
 * Sil: rather pointless since stats no longer have and 18/XYZ format
 */
void cnv_stat(int val, char* out_val) { sprintf(out_val, "%2d", val); }

/*
 * Print character info at given row, column in a 13 char field
 */
static void prt_field(cptr info, int row, int col)
{
    /* Dump 13 spaces to clear */
    c_put_str(TERM_WHITE, "             ", row, col);

    sdl_story_font_enable();
    /* Dump the info itself */
    c_put_str(TERM_L_BLUE, info, row, col);
    
    sdl_story_font_disable();
}

/*
 * Print character stat in given row, column
 */
static void prt_stat(int stat)
{
    char tmp[32];
    char trimmed_label[32];
    const char* stat_label;
    int len;

    /* Clear the line */
    put_str("             ", ROW_STAT + stat, 0);

    /* Get the stat name */
    if (p_ptr->stat_drain[stat] < 0)
    {
        stat_label = stat_names_reduced[stat];
    }
    else
    {
        stat_label = stat_names[stat];
    }
    
    /* Trim trailing spaces for story font rendering */
    SDL_strlcpy(trimmed_label, stat_label, sizeof(trimmed_label));
    len = strlen(trimmed_label);
    while (len > 0 && trimmed_label[len-1] == ' ') {
        trimmed_label[--len] = '\0';
    }

    log_trace("prt_stat: Rendering stat %d ('%s' trimmed to '%s')", stat, stat_label, trimmed_label);

    /* Display stat name with story font */
    log_trace("prt_stat: Enabling story font for stat label");
    sdl_story_font_enable();

    log_trace("prt_stat: Calling put_str('%s', %d, %d)", trimmed_label, ROW_STAT + stat, 0);
    put_str(trimmed_label, ROW_STAT + stat, 0);

    int cursor_x, cursor_y;
    Term_locate(&cursor_x, &cursor_y);
    log_trace("prt_stat: After put_str, cursor at (%d, %d)", cursor_x, cursor_y);
    log_trace("prt_stat: Disabling story font");
    sdl_story_font_disable();

    /* Display stat value with monospace font */
    cnv_stat(p_ptr->stat_use[stat], tmp);
    len = strlen(tmp);
    log_trace("prt_stat: Calling c_put_str('%s', %d, %d) for stat value", tmp, ROW_STAT + stat, COL_STAT + 12 - len);
    if (p_ptr->stat_drain[stat] < 0)
    {
        c_put_str(TERM_YELLOW, tmp, ROW_STAT + stat, COL_STAT + 12 - len);
    }
    else
    {
        c_put_str(TERM_L_GREEN, tmp, ROW_STAT + stat, COL_STAT + 12 - len);
    }

    /* Indicate temporary modifiers - clear first, then conditionally display */
    if ((stat == A_STR) && p_ptr->tmp_str)
        put_str("*", ROW_STAT + stat, 3);
    else if ((stat == A_DEX) && p_ptr->tmp_dex)
        put_str("*", ROW_STAT + stat, 3);
    else if ((stat == A_CON) && p_ptr->tmp_con)
        put_str("*", ROW_STAT + stat, 3);
    else if ((stat == A_GRA) && p_ptr->tmp_gra)
        put_str("*", ROW_STAT + stat, 3);
}

/*
 * Display the experience
 */
static void prt_exp(void)
{
    char out_val[32];
    byte attr;
    int len;

    attr = TERM_L_GREEN;

    /* Clear the whole field so shorter values don't leave stale characters */
    Term_erase(COL_EXP, ROW_EXP, 12);

    sdl_story_font_enable();

    /*Print experience label*/
    put_str("Exp", ROW_EXP, 0);

    sdl_story_font_disable();

    comma_number(out_val, p_ptr->new_exp);
    len = strlen(out_val);

    c_put_str(attr, out_val, ROW_EXP, COL_EXP + 12 - len);
}

/*
 * Prints current mel
 */
static void prt_mel(void)
{
    char buf[32];
    int mod = 0;

    if (((&inventory[INVEN_ARM])->k_idx)
        && ((&inventory[INVEN_ARM])->tval != TV_SHIELD))
        mod = -1;

    /* Clear both rows since melee can shift up/down and shrink in width */
    Term_erase(COL_MEL, ROW_MEL - 1, 12);
    Term_erase(COL_MEL, ROW_MEL, 12);

    /* Melee attacks */
    int meleeColour
        = p_ptr->active_ability[S_MEL][MEL_SMITE] ? TERM_L_RED : TERM_L_WHITE;
    strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_MEL],
        p_ptr->mdd, p_ptr->mds);
    c_put_str(meleeColour, buf, ROW_MEL + mod, COL_MEL + 12 - strlen(buf));

    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        c_put_str(TERM_WHITE, "2x", ROW_MEL + mod, COL_MEL);
    }

    if (mod == -1)
    {
        strnfmt(buf, sizeof(buf), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod, p_ptr->mdd2,
            p_ptr->mds2);
        c_put_str(TERM_L_WHITE, buf, ROW_MEL, COL_MEL + 12 - strlen(buf));
    }
}

/*
 * Prints current arc
 */
static void prt_arc(void)
{
    char buf[32];

    /* Clear the line so shorter values don't leave stale characters */
    Term_erase(COL_ARC, ROW_ARC, 12);

    /* Range attacks */
    if ((&inventory[INVEN_BOW])->k_idx)
    {
        if (p_ptr->active_ability[S_ARC][ARC_DEADLY_HAIL]
            && p_ptr->killed_enemy_with_arrow)
        {
            strnfmt(buf, sizeof(buf), ")");
            c_put_str(TERM_UMBER, buf, ROW_ARC, COL_ARC + 12 - strlen(buf));
            strnfmt(buf, sizeof(buf), "%dd%d", 2 * p_ptr->add, p_ptr->ads);
            c_put_str(TERM_RED, buf, ROW_ARC, COL_ARC + 11 - strlen(buf));
            strnfmt(buf, sizeof(buf), "(%+d,", p_ptr->skill_use[S_ARC]);
            if (p_ptr->ads > 9)
                c_put_str(TERM_UMBER, buf, ROW_ARC, COL_ARC + 7 - strlen(buf));
            else
                c_put_str(TERM_UMBER, buf, ROW_ARC, COL_ARC + 8 - strlen(buf));
        }
        else
        {
            strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_ARC],
                p_ptr->add, p_ptr->ads);
            c_put_str(TERM_UMBER, buf, ROW_ARC, COL_ARC + 12 - strlen(buf));
        }
    }

}

/*
 * Prints current quiver status (current/max for both quivers)
 * Right-aligned to 12 character width, like other stats
 * Same type: icon in middle between counts
 * Different: icon before each count
 */
static void prt_quiver(void)
{
    char buf1[16];
    char buf2[16];
    object_type* q1_ptr = &inventory[INVEN_QUIVER1];
    object_type* q2_ptr = &inventory[INVEN_QUIVER2];
    int q1_current = 0;
    int q1_max = 0;
    int q2_current = 0;
    int q2_max = 0;
    bool same_type = false;
    int total_width;
    int start_col;

    /* Clear the entire line (12 characters) */
    Term_erase(COL_QUIVER, ROW_QUIVER, 12);

    /* Get quiver 1 info */
    if (q1_ptr->k_idx)
    {
        q1_current = q1_ptr->number;
        q1_max = object_stack_limit(q1_ptr);
    }

    /* Get quiver 2 info */
    if (q2_ptr->k_idx)
    {
        q2_current = q2_ptr->number;
        q2_max = object_stack_limit(q2_ptr);
    }

    /* Check if both quivers have the same item type */
    if (q1_ptr->k_idx && q2_ptr->k_idx)
    {
        if (q1_ptr->tval == q2_ptr->tval && q1_ptr->sval == q2_ptr->sval)
        {
            same_type = true;
        }
    }

    /* Format the count strings */
    strnfmt(buf1, sizeof(buf1), "%d/%d", q1_current, q1_max);
    strnfmt(buf2, sizeof(buf2), "%d/%d", q2_current, q2_max);
    
    /* Calculate total width */
    if (same_type)
    {
        /* Layout: "11/48[→][→]7/7" */
        total_width = strlen(buf1) + (use_bigtile ? 2 : 2) + strlen(buf2);
    }
    else
    {
        /* Layout: "[|][|]11/48[/][/]7/7" */
        total_width = 0;
        if (q1_ptr->k_idx)
            total_width += (use_bigtile ? 2 : 2) + strlen(buf1);
        if (q2_ptr->k_idx)
            total_width += (use_bigtile ? 2 : 2) + strlen(buf2);
    }
    
    /* Right-align: start at column that makes it end at column 11 */
    start_col = COL_QUIVER + 12 - total_width;
    if (start_col < COL_QUIVER) start_col = COL_QUIVER;
    
    int col = start_col;

    if (same_type)
    {
        /* Same type: counts with icon in middle */
        byte attr = object_attr(q1_ptr);
        char icon = object_char(q1_ptr);
        
        /* Q1 count */
        Term_putstr(col, ROW_QUIVER, -1, TERM_L_WHITE, buf1);
        col += strlen(buf1);
        
        /* Icon in middle */
        Term_putch(col, ROW_QUIVER, attr, icon);
        col++;
        if (use_bigtile)
        {
            Term_putch(col, ROW_QUIVER, 255, -1);
            col++;
        }
        else
        {
            Term_putch(col, ROW_QUIVER, attr, icon);
            col++;
        }
        
        /* Q2 count */
        Term_putstr(col, ROW_QUIVER, -1, TERM_L_WHITE, buf2);
    }
    else
    {
        /* Different types: icon before each count */
        if (q1_ptr->k_idx)
        {
            /* Q1: "[icon][icon]cur/max" */
            byte attr = object_attr(q1_ptr);
            char icon = object_char(q1_ptr);
            Term_putch(col, ROW_QUIVER, attr, icon);
            col++;
            if (use_bigtile)
            {
                Term_putch(col, ROW_QUIVER, 255, -1);
                col++;
            }
            else
            {
                Term_putch(col, ROW_QUIVER, attr, icon);
                col++;
            }
            
            Term_putstr(col, ROW_QUIVER, -1, TERM_L_WHITE, buf1);
            col += strlen(buf1);
        }
        
        if (q2_ptr->k_idx)
        {
            /* Q2: "[icon][icon]cur/max" */
            byte attr = object_attr(q2_ptr);
            char icon = object_char(q2_ptr);
            Term_putch(col, ROW_QUIVER, attr, icon);
            col++;
            if (use_bigtile)
            {
                Term_putch(col, ROW_QUIVER, 255, -1);
                col++;
            }
            else
            {
                Term_putch(col, ROW_QUIVER, attr, icon);
                col++;
            }
            
            Term_putstr(col, ROW_QUIVER, -1, TERM_L_WHITE, buf2);
        }
    }
}

/*
 * Prints current evn
 */
static void prt_evn(void)
{
    char buf[32];

    /* Clear the line so shorter values don't leave stale characters */
    Term_erase(COL_EVN, ROW_EVN, 12);

    // Toggle blocking on and off so we don't show the blocking value in
    // the armor total
    bool block = p_ptr->active_ability[S_EVN][EVN_BLOCKING];
    p_ptr->active_ability[S_EVN][EVN_BLOCKING] = false;
    /* Total Armor */
    strnfmt(buf, sizeof(buf), "[%+d,%d-%d]", p_ptr->skill_use[S_EVN],
        p_min(GF_HURT, true), p_max(GF_HURT, true));
    c_put_str(TERM_SLATE, buf, ROW_EVN, COL_EVN + 12 - strlen(buf));
    p_ptr->active_ability[S_EVN][EVN_BLOCKING] = block;
}

/*
 * Prints Cur/Max hit points
 */
static void prt_hp(void)
{
    char tmp[32];
    byte color;

    /* Clear the line */
    put_str("             ", ROW_HP, COL_HP);

    sdl_story_font_enable();

    if (p_ptr->mhp >= 100)
    {
        put_str("Hth", ROW_HP, COL_HP);
    }
    else
    {
        put_str("Health", ROW_HP, COL_HP);
    }

    sdl_story_font_disable();

    /* Get color for current HP */
    color = health_attr(p_ptr->chp, p_ptr->mhp);

    /* Calculate lengths for left (current) and right (max) parts */
    int chp_len = sprintf(tmp, "%d", p_ptr->chp);
    int mhp_len = sprintf(tmp, "%d", p_ptr->mhp);
    int total_len = chp_len + 1 + mhp_len; /* +1 for the slash */

    /* Print current HP in color */
    sprintf(tmp, "%d", p_ptr->chp);
    c_put_str(color, tmp, ROW_HP, COL_HP + 12 - total_len);

    /* Print slash in green */
    c_put_str(TERM_L_GREEN, "/", ROW_HP, COL_HP + 12 - total_len + chp_len);

    /* Print max HP in green */
    sprintf(tmp, "%d", p_ptr->mhp);
    c_put_str(TERM_L_GREEN, tmp, ROW_HP, COL_HP + 12 - total_len + chp_len + 1);
}

/*
 * Prints a small, monospace graphical health bar under the name.
 * Uses 'x' characters up to 12 symbols to represent current HP proportionally.
 * Colour matches health_attr() (green/yellow/red, etc).
 */
static void prt_char_health_graphic(void)
{
    char bar[13]; /* 12 symbols + NUL */
    int max_symbols = 12;
    int filled = 0;
    byte color;

    /* Clear the line first (12 chars) */
    c_put_str(TERM_WHITE, "            ", ROW_NAME + 1, COL_NAME);

    /* Defensive: avoid division by zero */
    if (p_ptr->mhp <= 0)
        return;

    /* Scale current HP to number of symbols (ceiling) */
    filled = (max_symbols * p_ptr->chp + p_ptr->mhp - 1) / p_ptr->mhp;
    if (filled < 0)
        filled = 0;
    if (filled > max_symbols)
        filled = max_symbols;

    /* Build the bar using 'x' for filled and spaces for remainder */
    for (int i = 0; i < filled; i++)
        bar[i] = 'x';
    for (int i = filled; i < max_symbols; i++)
        bar[i] = ' ';
    bar[max_symbols] = '\0';

    /* Colour according to health */
    color = health_attr(p_ptr->chp, p_ptr->mhp);

    /* Print using a monospace field (no story font) */
    c_put_str(color, format("%12s", bar), ROW_NAME + 1, COL_NAME);
}

static void prt_light(void)
{
    object_type* o_ptr = &inventory[INVEN_LITE];
    int icon_col = COL_LIGHT;

    /* Clear the line */
    Term_erase(icon_col, ROW_LIGHT, 13);

    /* Nothing equipped */
    if (!o_ptr->k_idx)
        return;

    byte attr = object_attr(o_ptr);
    char icon = object_char(o_ptr);

    /* Draw the icon (supporting bigtile visuals) */
    Term_putch(icon_col, ROW_LIGHT, attr, icon);
    if (use_bigtile)
    {
        Term_putch(icon_col + 1, ROW_LIGHT, 255, -1);
    }
    else
    {
        Term_putch(icon_col + 1, ROW_LIGHT, attr, icon);
    }

    Term_putch(icon_col + 2, ROW_LIGHT, TERM_WHITE, ' ');

    bool infinite = false;
    long fuel = 0;
    byte fuel_attr = TERM_L_WHITE;
    char buf[16];

    if (o_ptr->tval == TV_LIGHT)
    {
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
        case SV_LIGHT_LANTERN:
        case SV_LIGHT_MALLORN:
            fuel = o_ptr->timeout;
            break;
        default:
            infinite = true;
            break;
        }
    }
    else
    {
        u32b f1, f2, f3;
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f2 & TR2_LIGHT)
            infinite = true;
    }

    if (infinite)
    {
        SDL_strlcpy(buf, "inf", sizeof(buf));
        fuel_attr = TERM_L_GREEN;
    }
    else
    {
        if (fuel < 0)
            fuel = 0;

        if (fuel == 0)
            fuel_attr = TERM_RED;
        else if (fuel <= 100)
            fuel_attr = TERM_ORANGE;

        strnfmt(buf, sizeof(buf), "%ld", fuel);
    }

    c_put_str(fuel_attr, buf, ROW_LIGHT, icon_col + 12 - strlen(buf));
}

/*
 * Prints player's max/cur spell points
 */
static void prt_sp(void)
{
    char tmp[32];
    byte color;
    int len;

    /* Clear the line */
    put_str("             ", ROW_SP, COL_SP);

    sdl_story_font_enable();

    if (p_ptr->msp >= 100)
        put_str("Vce", ROW_SP, COL_SP);
    else
        put_str("Voice", ROW_SP, COL_SP);

    sdl_story_font_disable();

    len = sprintf(tmp, "%d:%d", p_ptr->csp, p_ptr->msp);

    c_put_str(TERM_L_GREEN, tmp, ROW_SP, COL_SP + 12 - len);

    /* Done? */
    if (p_ptr->csp >= p_ptr->msp)
        return;

    if (p_ptr->csp > (p_ptr->msp * op_ptr->hitpoint_warn) / 10)
    {
        color = TERM_YELLOW;
    }
    else
    {
        color = TERM_RED;
    }

    /* Show current mana using another color */
    sprintf(tmp, "%d", p_ptr->csp);

    c_put_str(color, tmp, ROW_SP, COL_SP + 12 - len);
}

static void hidden_left_panel_add_line(hidden_overlay_line* lines, int* count,
                                       int max_lines, byte attr, cptr text)
{
    if (!lines || !count || !text || !text[0])
        return;
    if (*count >= max_lines)
        return;

    SDL_strlcpy(lines[*count].text, text, sizeof(lines[*count].text));
    lines[*count].attr = attr;
    (*count)++;
}

static int hidden_left_panel_build_lines(hidden_overlay_line* lines, int max_lines)
{
    int count = 0;
    char buf[32];
    byte hp_color;
    byte voice_color;

    if (!lines || !Term || !p_ptr || max_lines <= 0)
        return 0;

    hp_color = health_attr(p_ptr->chp, p_ptr->mhp);
    if (p_ptr->csp >= p_ptr->msp)
        voice_color = TERM_L_GREEN;
    else if (p_ptr->csp > (p_ptr->msp * op_ptr->hitpoint_warn) / 10)
        voice_color = TERM_YELLOW;
    else
        voice_color = TERM_RED;

    strnfmt(buf, sizeof(buf), "HP %3d", MIN(p_ptr->chp, 999));
    hidden_left_panel_add_line(lines, &count, max_lines, hp_color, buf);

    strnfmt(buf, sizeof(buf), "VC %3d", MIN(p_ptr->csp, 999));
    hidden_left_panel_add_line(lines, &count, max_lines, voice_color, buf);

    if (p_ptr->cut > 100)
    {
        hidden_left_panel_add_line(lines, &count, max_lines, TERM_RED,
            "MW !!!");
    }
    else if (p_ptr->cut > 20)
    {
        strnfmt(buf, sizeof(buf), "BL %3d", MIN(p_ptr->cut, 999));
        hidden_left_panel_add_line(lines, &count, max_lines, TERM_RED, buf);
    }
    else if (p_ptr->cut > 0)
    {
        strnfmt(buf, sizeof(buf), "BL %3d", MIN(p_ptr->cut, 999));
        hidden_left_panel_add_line(lines, &count, max_lines, TERM_L_RED, buf);
    }

    if (p_ptr->poisoned > 20)
    {
        strnfmt(buf, sizeof(buf), "PS %3d", MIN(p_ptr->poisoned, 999));
        hidden_left_panel_add_line(lines, &count, max_lines, TERM_L_GREEN, buf);
    }
    else if (p_ptr->poisoned > 0)
    {
        strnfmt(buf, sizeof(buf), "PS %3d", MIN(p_ptr->poisoned, 999));
        hidden_left_panel_add_line(lines, &count, max_lines, TERM_GREEN, buf);
    }

    if (p_ptr->song1 != SNG_NOTHING || p_ptr->song2 != SNG_NOTHING)
    {
        char* song1_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;
        char* song2_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;
        buf[0] = '\0';

        if (p_ptr->song1 != SNG_NOTHING && p_ptr->song2 != SNG_NOTHING)
            strnfmt(buf, sizeof(buf), "%s+%s", song1_name + 8, song2_name + 8);
        else if (p_ptr->song1 != SNG_NOTHING)
            SDL_strlcpy(buf, song1_name + 8, sizeof(buf));
        else if (p_ptr->song2 != SNG_NOTHING)
            SDL_strlcpy(buf, song2_name + 8, sizeof(buf));

        hidden_left_panel_add_line(lines, &count, max_lines, TERM_L_BLUE, buf);
    }

    if (p_ptr->health_who
        && mon_list[p_ptr->health_who].ml
        && !p_ptr->image
        && (mon_list[p_ptr->health_who].hp > 0))
    {
        monster_type* m_ptr = &mon_list[p_ptr->health_who];
        int len;
        byte attr;

        attr = health_attr(m_ptr->hp, m_ptr->maxhp);
        len = (8 * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;
        if (len < 0)
            len = 0;
        if (len > 8)
            len = 8;

        for (int i = 0; i < len; i++)
            buf[i] = '*';
        buf[len] = '\0';

        hidden_left_panel_add_line(lines, &count, max_lines, attr, buf);
    }

    return count;
}

static void prt_hidden_top_vitals(void)
{
    hidden_overlay_line lines[16];
    int line_count;

    if (!Term || !p_ptr)
        return;

    line_count = hidden_left_panel_build_lines(lines, 16);

    for (int i = 0; i < line_count && (ROW_NAME + i) < Term->hgt - 1; i++)
    {
        int row = ROW_NAME + i;
        int width = (int)strlen(lines[i].text);

        if (width <= 0)
            continue;
        if (width > Term->wid)
            width = Term->wid;

        Term_erase(0, row, width);
        Term_putstr(0, row, width, lines[i].attr, lines[i].text);
    }
}

/*
 * Prints player's current song (if any)
 */
static void prt_song(void)
{
    if (ui_compact_status_line_handles_song())
    {
        prt_status_line_compact();
        return;
    }

    char* song1_name
        = b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;
    char* song2_name
        = b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;

    // wipe old songs
    put_str("             ", ROW_SONG, COL_SONG);
    if (!ui_compact_height())
        put_str("             ", ROW_SONG + 1, COL_SONG);

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
            c_put_str(TERM_L_BLUE, buf, ROW_SONG, COL_SONG);
    }
    else
    {
        // show the first song
        if (p_ptr->song1 != SNG_NOTHING)
        {
            c_put_str(TERM_L_BLUE, song1_name + 8, ROW_SONG, COL_SONG);
        }

        // show the second song
        if (p_ptr->song2 != SNG_NOTHING)
        {
            c_put_str(TERM_BLUE, song2_name + 8, ROW_SONG + 1, COL_SONG);
        }
    }

    sdl_story_font_disable();
}

/*
 * Prints depth in stat area
 */
static void prt_depth(void)
{
    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    char depths[32];
    s16b attr = TERM_WHITE;

    if (!p_ptr->depth)
    {
        SDL_strlcpy(depths, "Surface", sizeof(depths));
    }
    else
    {
        sprintf(depths, "%d ft", p_ptr->depth * 50);
    }

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

    sdl_story_font_enable();

    /* Right-Adjust the "depth", and clear old values */
    c_prt(attr, format("%7s", depths), ROW_DEPTH, COL_DEPTH);

    sdl_story_font_disable();
}

/*
 * Prints status of hunger
 */
static void prt_hunger(void)
{
    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    sdl_story_font_enable();

    /* Fainting / Starving */
    if (p_ptr->food < PY_FOOD_STARVE)
    {
        c_put_str(TERM_RED, "Starving", ROW_HUNGRY, COL_HUNGRY);
    }

    /* Weak */
    else if (p_ptr->food < PY_FOOD_WEAK)
    {
        c_put_str(TERM_ORANGE, "Weak    ", ROW_HUNGRY, COL_HUNGRY);
    }

    /* Hungry */
    else if (p_ptr->food < PY_FOOD_ALERT)
    {
        c_put_str(TERM_YELLOW, "Hungry  ", ROW_HUNGRY, COL_HUNGRY);
    }

    /* Normal */
    else if (p_ptr->food < PY_FOOD_FULL)
    {
        c_put_str(TERM_L_GREEN, "        ", ROW_HUNGRY, COL_HUNGRY);
    }

    /* Full */
    else if (p_ptr->food < PY_FOOD_MAX)
    {
        c_put_str(TERM_L_GREEN, "Full    ", ROW_HUNGRY, COL_HUNGRY);
    }

    else
    {
        c_put_str(TERM_GREEN, "Full    ", ROW_HUNGRY, COL_HUNGRY);
    }

    sdl_story_font_disable();
}

/*
 * Prints Blind status
 */
static void prt_blind(void)
{
    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    sdl_story_font_enable();

    if (p_ptr->blind)
    {
        c_put_str(TERM_ORANGE, "Blind", ROW_BLIND, COL_BLIND);
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
static void prt_confused(void)
{
    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_CONFUSED, ROW_CONFUSED, 8);

    if (p_ptr->confused)
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Confused", ROW_CONFUSED, COL_CONFUSED);
        sdl_story_font_disable();
    }
}

/*
 * Prints Fear status
 */
static void prt_afraid(void)
{
    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_AFRAID, ROW_AFRAID, 6);

    if (p_ptr->afraid)
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Afraid", ROW_AFRAID, COL_AFRAID);
        sdl_story_font_disable();
    }
}

/*
 *  Displays the amount of bleeding.
 *  This is a bit tricky as it is in the same row as poison, *unless* you have
 * both. In which case it is the row above.
 */

static void prt_cut(void)
{
    if (ui_hide_left_panel())
        return;

    if (ui_compact_status_line_handles_wounds())
    {
        prt_status_line_compact();
        return;
    }

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
        c_put_str(TERM_RED, "Mortal wound", r, COL_CUT);
        sdl_story_font_disable();
    }
    else if (c > 20)
    {
        sdl_story_font_enable();
        c_put_str(TERM_RED, "Bleeding", r, COL_CUT);
        sdl_story_font_disable();
        sprintf(num_buf, " %-2d", c);
        c_put_str(TERM_RED, num_buf, r, COL_CUT + 8);
    }
    else if (c > 0)
    {
        sdl_story_font_enable();
        c_put_str(TERM_L_RED, "Bleeding", r, COL_CUT);
        sdl_story_font_disable();
        sprintf(num_buf, " %-2d", c);
        c_put_str(TERM_L_RED, num_buf, r, COL_CUT + 8);
    }
}

/*
 * Prints Poisoned status
 */
static void prt_poisoned(void)
{
    if (ui_hide_left_panel())
        return;

    if (ui_compact_status_line_handles_wounds())
    {
        prt_status_line_compact();
        return;
    }

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
        c_put_str(TERM_L_GREEN, "Poisoned", ROW_POISONED, COL_POISONED);
        sdl_story_font_disable();
        sprintf(num_buf, " %-3d", p);
        c_put_str(TERM_L_GREEN, num_buf, ROW_POISONED, COL_POISONED + 8);
    }
    else if (p > 0)
    {
        sdl_story_font_enable();
        c_put_str(TERM_GREEN, "Poisoned", ROW_POISONED, COL_POISONED);
        sdl_story_font_disable();
        sprintf(num_buf, " %-3d", p);
        c_put_str(TERM_GREEN, num_buf, ROW_POISONED, COL_POISONED + 8);
    }
}

/*
 * Prints Searching, Resting, Entrancement, Smithing, or 'count' status
 * Display is always exactly 10 characters wide (see below)
 *
 * This function was a major bottleneck when resting, so a lot of
 * the text formatting code was optimized in place below.
 */
static void prt_state(void)
{
    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    byte attr = TERM_WHITE;

    char text[16];

    /* Entrancement */
    if (p_ptr->entranced)
    {
        attr = TERM_RED;

        SDL_strlcpy(text, "Entranced!", sizeof(text));
    }

    /* Smithing */
    if (p_ptr->smithing)
    {
        SDL_strlcpy(text, "Smithing  ", sizeof(text));
    }

    if (p_ptr->fletching)
    {
        SDL_strlcpy(text, "Fletching ", sizeof(text));
    }
    else if (p_ptr->rage)
    {
        attr = TERM_RED;
        SDL_strlcpy(text, "Rage      ", sizeof(text));
    }

    /* Resting */
    else if (p_ptr->resting)
    {
        int i;
        int n = p_ptr->resting;

        /* Start with "Rest" */
        SDL_strlcpy(text, "Rest      ", sizeof(text));

        /* Extensive (timed) rest */
        if (n >= 1000)
        {
            i = n / 100;
            text[9] = '0';
            text[8] = '0';
            text[7] = I2D(i % 10);
            if (i >= 10)
            {
                i = i / 10;
                text[6] = I2D(i % 10);
                if (i >= 10)
                {
                    text[5] = I2D(i / 10);
                }
            }
        }

        /* Long (timed) rest */
        else if (n >= 100)
        {
            i = n;
            text[9] = I2D(i % 10);
            i = i / 10;
            text[8] = I2D(i % 10);
            text[7] = I2D(i / 10);
        }

        /* Medium (timed) rest */
        else if (n >= 10)
        {
            i = n;
            text[9] = I2D(i % 10);
            text[8] = I2D(i / 10);
        }

        /* Short (timed) rest */
        else if (n > 0)
        {
            i = n;
            text[9] = I2D(i);
        }

        /* Rest until healed */
        else if (n == -1)
        {
            text[5] = text[6] = text[7] = text[8] = text[9] = '*';
        }

        /* Rest until done */
        else if (n == -2)
        {
            text[5] = text[6] = text[7] = text[8] = text[9] = '&';
        }
    }

    /* Repeating */
    else if (p_ptr->command_rep)
    {
        if (p_ptr->command_rep > 999)
        {
            sprintf(text, "Rep. %3d00", p_ptr->command_rep / 100);
        }
        else
        {
            sprintf(text, "Repeat %3d", p_ptr->command_rep);
        }
    }

    /* Stealth mode */
    else if (p_ptr->stealth_mode)
    {
        SDL_strlcpy(text, "Stealth   ", sizeof(text));
    }

    /* Nothing interesting */
    else
    {
        text[0] = '\0';
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_STATE, ROW_STATE, 10);

    /* Display the info if any */
    if (text[0])
    {
        sdl_story_font_enable();
        c_put_str(attr, text, ROW_STATE, COL_STATE);
        sdl_story_font_disable();
    }
}

/*
 * Prints the speed of a character.			-CJS-
 */
static void prt_speed(void)
{
    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    int i = p_ptr->pspeed;

    byte attr = TERM_WHITE;
    char buf[32] = "";

    /* Fast */
    if (i > 2)
    {
        attr = TERM_L_GREEN;
        sprintf(buf, "Fast");
    }

    /* Slow */
    else if (i < 2)
    {
        attr = TERM_ORANGE;
        sprintf(buf, "Slow");
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_SPEED, ROW_SPEED, 4);

    /* Display the speed if not normal */
    if (buf[0])
    {
        sdl_story_font_enable();
        c_put_str(attr, buf, ROW_SPEED, COL_SPEED);
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
        return "Cave";
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

static void prt_partition(void)
{
    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    if (!p_ptr)
        return;

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_PARTITION, ROW_PARTITION, 5);

    const char* label = partition_abbrev_for_point(p_ptr->py, p_ptr->px);
    if (!label[0])
        return;

    sdl_story_font_enable();
    c_put_str(TERM_WHITE, label, ROW_PARTITION, COL_PARTITION);
    sdl_story_font_disable();
}

/*
 * Prints message regarding difficult terrain
 */
static void prt_terrain(void)
{
    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_TERRAIN, ROW_TERRAIN, 5);

    if (cave_pit_bold(p_ptr->py, p_ptr->px))
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Pit", ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Web", ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT)
    {
        sdl_story_font_enable();
        c_put_str(TERM_YELLOW, "Sun", ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }

    prt_partition();
}

static void prt_cut_poisoned_compact(void)
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
            strnfmt(cut_buf, sizeof(cut_buf), "Bld:%d", c);
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
        strnfmt(pois_buf, sizeof(pois_buf), "Poi:%d", p);
        int len = (int)strlen(pois_buf);
        if (x + len > col + width)
            len = (col + width) - x;
        if (len > 0)
            Term_putstr(x, row, len, pois_attr, pois_buf);
    }
}

static void prt_stun(void)
{
    if (ui_compact_width())
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
        c_put_str(TERM_RED, "Knocked out", ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
    else if (s > 50)
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Heavy stun", ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
    else if (s)
    {
        sdl_story_font_enable();
        c_put_str(TERM_ORANGE, "Stun", ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
}

typedef struct {
    const char* long_text;
    const char* short_text;
    byte attr;
    bool required;
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

static bool status_state_text(char* out_long, size_t out_long_sz,
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
        SDL_strlcpy(out_long, "Entranced", out_long_sz);
        SDL_strlcpy(out_short, "En", out_short_sz);
        return true;
    }

    if (p_ptr->smithing)
    {
        SDL_strlcpy(out_long, "Smithing", out_long_sz);
        SDL_strlcpy(out_short, "Sm", out_short_sz);
        return true;
    }

    if (p_ptr->fletching)
    {
        SDL_strlcpy(out_long, "Fletching", out_long_sz);
        SDL_strlcpy(out_short, "Fl", out_short_sz);
        return true;
    }

    if (p_ptr->rage)
    {
        if (out_attr)
            *out_attr = TERM_RED;
        SDL_strlcpy(out_long, "Rage", out_long_sz);
        SDL_strlcpy(out_short, "Rg", out_short_sz);
        return true;
    }

    if (p_ptr->resting)
    {
        int n = p_ptr->resting;
        if (n == -1)
        {
            SDL_strlcpy(out_long, "Rest*", out_long_sz);
            SDL_strlcpy(out_short, "R*", out_short_sz);
        }
        else if (n == -2)
        {
            SDL_strlcpy(out_long, "Rest&", out_long_sz);
            SDL_strlcpy(out_short, "R&", out_short_sz);
        }
        else if (n >= 1000)
        {
            strnfmt(out_long, out_long_sz, "Rest %d", n);
            strnfmt(out_short, out_short_sz, "R%dk", n / 1000);
        }
        else
        {
            strnfmt(out_long, out_long_sz, "Rest %d", n);
            strnfmt(out_short, out_short_sz, "R%d", n);
        }
        return true;
    }

    if (p_ptr->command_rep)
    {
        strnfmt(out_long, out_long_sz, "Repeat %d", p_ptr->command_rep);
        strnfmt(out_short, out_short_sz, "Rp%d", p_ptr->command_rep);
        return true;
    }

    if (p_ptr->stealth_mode)
    {
        SDL_strlcpy(out_long, "Stealth", out_long_sz);
        SDL_strlcpy(out_short, "St", out_short_sz);
        return true;
    }

    return false;
}

static const char* status_partition_short(const char* long_label)
{
    if (!long_label || !long_label[0])
        return "";
    if (!strcmp(long_label, "Room"))
        return "Rm";
    if (!strcmp(long_label, "Ruin"))
        return "Ru";
    if (!strcmp(long_label, "Cave"))
        return "Cv";
    if (!strcmp(long_label, "BigCa"))
        return "BC";
    if (!strcmp(long_label, "Labir"))
        return "Lb";
    if (!strcmp(long_label, "Chasm"))
        return "Ch";
    return long_label;
}

static void prt_status_line_compact(void)
{
    if (!Term || !p_ptr)
        return;

    const int row = Term->hgt - 1;
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

    if (p_ptr->food < PY_FOOD_STARVE) {
        SDL_strlcpy(hunger_long, "Starving", sizeof(hunger_long));
        SDL_strlcpy(hunger_short, "St", sizeof(hunger_short));
        hunger_attr = TERM_RED;
    } else if (p_ptr->food < PY_FOOD_WEAK) {
        SDL_strlcpy(hunger_long, "Weak", sizeof(hunger_long));
        SDL_strlcpy(hunger_short, "Wk", sizeof(hunger_short));
        hunger_attr = TERM_ORANGE;
    } else if (p_ptr->food < PY_FOOD_ALERT) {
        SDL_strlcpy(hunger_long, "Hungry", sizeof(hunger_long));
        SDL_strlcpy(hunger_short, "Hu", sizeof(hunger_short));
        hunger_attr = TERM_YELLOW;
    } else if (p_ptr->food >= PY_FOOD_FULL) {
        SDL_strlcpy(hunger_long, "Full", sizeof(hunger_long));
        SDL_strlcpy(hunger_short, "Fu", sizeof(hunger_short));
        hunger_attr = TERM_L_GREEN;
    }

    char stun_long[16] = "";
    char stun_short[8] = "";
    byte stun_attr = TERM_WHITE;
    if (p_ptr->stun > 100) {
        SDL_strlcpy(stun_long, "Knocked out", sizeof(stun_long));
        SDL_strlcpy(stun_short, "KO", sizeof(stun_short));
        stun_attr = TERM_RED;
    } else if (p_ptr->stun > 50) {
        SDL_strlcpy(stun_long, "Heavy stun", sizeof(stun_long));
        SDL_strlcpy(stun_short, "HS", sizeof(stun_short));
        stun_attr = TERM_ORANGE;
    } else if (p_ptr->stun) {
        SDL_strlcpy(stun_long, "Stun", sizeof(stun_long));
        SDL_strlcpy(stun_short, "St", sizeof(stun_short));
        stun_attr = TERM_ORANGE;
    }

    char state_long[24] = "";
    char state_short[12] = "";
    byte state_attr = TERM_WHITE;
    (void)status_state_text(state_long, sizeof(state_long), state_short,
        sizeof(state_short), &state_attr);

    char cut_long[16] = "";
    char cut_short[8] = "";
    byte cut_attr = TERM_WHITE;
    if (fold_wounds)
    {
        if (p_ptr->cut > 100) {
            SDL_strlcpy(cut_long, "Mortal", sizeof(cut_long));
            SDL_strlcpy(cut_short, "MW", sizeof(cut_short));
            cut_attr = TERM_RED;
        } else if (p_ptr->cut > 20) {
            strnfmt(cut_long, sizeof(cut_long), "Bleed %d", p_ptr->cut);
            strnfmt(cut_short, sizeof(cut_short), "B%d", p_ptr->cut);
            cut_attr = TERM_RED;
        } else if (p_ptr->cut > 0) {
            strnfmt(cut_long, sizeof(cut_long), "Bleed %d", p_ptr->cut);
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
            strnfmt(pois_long, sizeof(pois_long), "Poison %d", p_ptr->poisoned);
            strnfmt(pois_short, sizeof(pois_short), "P%d", p_ptr->poisoned);
            pois_attr = TERM_L_GREEN;
        } else if (p_ptr->poisoned > 0) {
            strnfmt(pois_long, sizeof(pois_long), "Poison %d", p_ptr->poisoned);
            strnfmt(pois_short, sizeof(pois_short), "P%d", p_ptr->poisoned);
            pois_attr = TERM_GREEN;
        }
    }

    char speed_long[8] = "";
    char speed_short[4] = "";
    byte speed_attr = TERM_WHITE;
    if (p_ptr->pspeed > 2) {
        SDL_strlcpy(speed_long, "Fast", sizeof(speed_long));
        SDL_strlcpy(speed_short, "Fa", sizeof(speed_short));
        speed_attr = TERM_L_GREEN;
    } else if (p_ptr->pspeed < 2) {
        SDL_strlcpy(speed_long, "Slow", sizeof(speed_long));
        SDL_strlcpy(speed_short, "Sl", sizeof(speed_short));
        speed_attr = TERM_ORANGE;
    }

    char terrain_long[8] = "";
    char terrain_short[4] = "";
    byte terrain_attr = TERM_ORANGE;
    if (cave_pit_bold(p_ptr->py, p_ptr->px)) {
        SDL_strlcpy(terrain_long, "Pit", sizeof(terrain_long));
        SDL_strlcpy(terrain_short, "Pt", sizeof(terrain_short));
    } else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB) {
        SDL_strlcpy(terrain_long, "Web", sizeof(terrain_long));
        SDL_strlcpy(terrain_short, "Wb", sizeof(terrain_short));
    } else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT) {
        SDL_strlcpy(terrain_long, "Sun", sizeof(terrain_long));
        SDL_strlcpy(terrain_short, "Sn", sizeof(terrain_short));
        terrain_attr = TERM_YELLOW;
    }

    const char* part_long = partition_abbrev_for_point(p_ptr->py, p_ptr->px);
    const char* part_short = status_partition_short(part_long);

    char depth_long[16] = "";
    char depth_short[16] = "";
    int feet = p_ptr->depth * 50;
    if (!p_ptr->depth) {
        SDL_strlcpy(depth_long, "Surface", sizeof(depth_long));
        SDL_strlcpy(depth_short, "0'", sizeof(depth_short));
    } else {
        strnfmt(depth_long, sizeof(depth_long), "%d ft", feet);
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

    #define ADD_SEG(LTXT, STXT, ATTR, REQ) \
        do { \
            if ((LTXT)[0]) { \
                segs[seg_count].long_text = (LTXT); \
                segs[seg_count].short_text = (STXT)[0] ? (STXT) : (LTXT); \
                segs[seg_count].attr = (ATTR); \
                segs[seg_count].required = (REQ); \
                seg_count++; \
            } \
        } while (0)

    ADD_SEG(hunger_long, hunger_short, hunger_attr, true);
    ADD_SEG(p_ptr->blind ? "Blind" : "", "Bl", TERM_ORANGE, true);
    ADD_SEG(p_ptr->confused ? "Confused" : "", "Cn", TERM_ORANGE, true);
    ADD_SEG(cut_long, cut_short, cut_attr, true);
    ADD_SEG(pois_long, pois_short, pois_attr, true);
    ADD_SEG(stun_long, stun_short, stun_attr, true);
    ADD_SEG(p_ptr->afraid ? "Afraid" : "", "Af", TERM_ORANGE, true);
    ADD_SEG(song_long, song_short, TERM_L_BLUE, false);
    ADD_SEG(state_long, state_short, state_attr, false);
    ADD_SEG(speed_long, speed_short, speed_attr, false);
    ADD_SEG(terrain_long, terrain_short, terrain_attr, false);
    ADD_SEG(part_long, part_short, TERM_WHITE, false);
    ADD_SEG(depth_long, depth_short, depth_attr, true);

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
            Term_putstr(x, row, n, segs[i].attr, t);
        x += n;
        first = false;
    }
}

/*
 *  Represents the different levels of health.
 *  Note that it is a bit odd with fewer health levels in the SOMEWHAT_WOUNDED
 * category. This is due to a rounding off tension between the natural way to do
 * the colours (perfect having its own) and the natural way to do the stars for
 * the health bar (zero having its own). It should be unnoticeable to the
 * player.
 */
int health_level(int current, int max)
{
    int level;

    if (current == max)
    {
        level = HEALTH_UNHURT; // 100%
    }

    else
    {
        switch ((4 * current + max - 1) / max)
        {
        case 4:
            level = HEALTH_SOMEWHAT_WOUNDED;
            break; //  76% - 99%
        case 3:
            level = HEALTH_WOUNDED;
            break; //  51% - 75%
        case 2:
            level = HEALTH_BADLY_WOUNDED;
            break; //  26% - 50%
        case 1:
            level = HEALTH_ALMOST_DEAD;
            break; //   1% - 25%
        default:
            level = HEALTH_DEAD;
            break; //   0%
        }
    }

    return (level);
}

/*
 *  Assigns colours to the health levels.
 */
byte health_attr(int current, int max)
{
    byte a;

    switch (health_level(current, max))
    {
    case HEALTH_UNHURT:
        a = TERM_L_GREEN;
        break; // 100%
    case HEALTH_SOMEWHAT_WOUNDED:
        a = TERM_YELLOW;
        break; //  76% - 99%
    case HEALTH_WOUNDED:
        a = TERM_ORANGE;
        break; //  51% - 75%
    case HEALTH_BADLY_WOUNDED:
        a = TERM_L_RED;
        break; //  26% - 50%
    case HEALTH_ALMOST_DEAD:
        a = TERM_RED;
        break; //   1% - 25%
    default:
        a = TERM_RED;
        break; //   0%
    }

    return (a);
}

/*
 * Gets a text string denoting the alertness level / stance into a buffer, along
 * with the associated colour.
 */
bool get_alertness_text(
    monster_type* m_ptr, int text_size, char* text, int* color)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    if (m_ptr->alertness < ALERTNESS_UNWARY)
    {
        SDL_strlcpy(text, "Sleeping", text_size);
        *color = TERM_BLUE;
    }
    else if (m_ptr->alertness < ALERTNESS_ALERT)
    {
        SDL_strlcpy(text, "Unwary", text_size);
        *color = TERM_L_BLUE;
    }
    else
    {
        if (r_ptr->flags2 & (RF2_MINDLESS))
        {
            SDL_strlcpy(text, "Mindless", text_size);
            *color = TERM_L_DARK;
        }
        else
        {
            char morale_buf[8];

            if (m_ptr->stance == STANCE_FLEEING)
            {
                SDL_strlcpy(text, "Fleeing", text_size);
                *color = TERM_VIOLET;
            }
            else if (m_ptr->stance == STANCE_CONFIDENT)
            {
                SDL_strlcpy(text, "Confident", text_size);
                *color = TERM_L_WHITE;
            }
            else if (m_ptr->stance == STANCE_AGGRESSIVE)
            {
                SDL_strlcpy(text, "Aggress", text_size);
                *color = TERM_L_WHITE;
            }

            // sometimes (only in debugging?) we are looking at a monster before
            // it has a stance in this case return false so we don't print the
            // strings
            else
            {
                return false;
            }

            if (m_ptr->morale >= 0)
                sprintf(morale_buf, " %d", (m_ptr->morale + 9) / 10);
            else
                sprintf(morale_buf, " %d", m_ptr->morale / 10);

            strncat(text, morale_buf, text_size - strlen(text));
        }
    }

    return true;
}

/*
 * Redraw the "monster health bar"
 *
 * The "monster health bar" provides visual feedback on the "health"
 * of the monster currently being "tracked".  There are several ways
 * to "track" a monster, including targetting it, attacking it, and
 * affecting it (and nobody else) with a ranged attack.  When nothing
 * is being tracked, we clear the health bar.  If the monster being
 * tracked is not currently visible, a special health bar is shown.
 */
static void health_redraw(void)
{
    if (ui_hide_left_panel())
        return;

    /* Not tracking */
    if (!p_ptr->health_who)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking an unseen monster */
    else if (!mon_list[p_ptr->health_who].ml)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking a hallucinatory monster */
    else if (p_ptr->image)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking a dead monster (?) */
    else if (mon_list[p_ptr->health_who].hp <= 0)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking a visible monster */
    else
    {
        int len;
        int color;
        char buf[20];

        monster_type* m_ptr = &mon_list[p_ptr->health_who];

        /* Default to almost dead */
        byte attr = health_attr(m_ptr->hp, m_ptr->maxhp);

        /* Afraid */
        // if (m_ptr->stance == STANCE_FLEEING) attr = TERM_VIOLET;

        /* Convert into health bar (using ceiling for length) */
        len = (8 * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;

        /* Default to "unknown" */
        Term_putstr(COL_INFO, ROW_INFO, 12, TERM_L_DARK, "  --------  ");

        /* Dump the current "health" (handle monster stunning, confusion) */

        if (m_ptr->confused && m_ptr->stunned)
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "cscscscs");
        else if (m_ptr->confused)
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "cccccccc");
        else if (m_ptr->stunned)
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "ssssssss");
        else
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "********");

        Term_erase(COL_INFO, ROW_INFO + 1, 12);

        if (!get_alertness_text(m_ptr, sizeof(buf), buf, &color))
            return;

        Term_putstr(COL_INFO + (13 - strlen(buf)) / 2, ROW_INFO + 1,
            MIN(strlen(buf), 12), color, buf);
    }
}

/*
 * Display basic info (mostly left of map)
 */
static void prt_frame_basic(void)
{
    int i;

    if (ui_hide_left_panel())
    {
        prt_depth();
        return;
    }

    /* Name */
    if (strlen(op_ptr->full_name) <= 12)
    {
        prt_field(op_ptr->full_name, ROW_NAME, COL_NAME);
    }

    /* Small monospace health graphic under the name */
    prt_char_health_graphic();

    /* Level/Experience */
    prt_exp();

    /* All Stats */
    for (i = 0; i < A_MAX; i++)
        prt_stat(i);

    /* Hitpoints */
    prt_hp();

    /* Spellpoints */
    prt_sp();

    /* Light */
    prt_light();

    /* Melee */
    prt_mel();

    /* Archery */
    prt_arc();

    /* Quiver */
    prt_quiver();

    /* Evasion */
    prt_evn();

    /* Song */
    prt_song();

    /* Current depth */
    prt_depth();

    /* redraw monster health */
    health_redraw();
}

/*
 * Display extra info (mostly below map)
 */
static void prt_frame_extra(void)
{
    if (ui_compact_width())
    {
        /* Compact width: bottom status is rendered as a single packed line. */
        if (!ui_compact_status_line_handles_wounds())
        {
            prt_poisoned();
            prt_cut();
        }
        prt_status_line_compact();
        return;
    }

    /* Stun */
    prt_stun();

    /* Food */
    prt_hunger();

    /* Various */
    prt_blind();
    prt_confused();
    prt_afraid();
    prt_poisoned();
    prt_cut();
    prt_terrain();

    /* State */
    prt_state();

    /* Speed */
    prt_speed();
}

/*
 * Hack -- display inventory in sub-windows
 */
static void fix_inven(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_INVEN)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display inventory */
        display_inven();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display monsters in sub-windows
 */
static void fix_monlist(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_MONLIST)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display visible monsters */
        display_monlist();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display combat rolls in sub-windows
 */
static void fix_combat_rolls(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_COMBAT_ROLLS)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display visible monsters */
        display_combat_rolls();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display equipment in sub-windows
 */
static void fix_equip(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_EQUIP)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display equipment */
        display_equip();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display player in sub-windows (mode 0)
 */
static void fix_player_0(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_PLAYER_0)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display player */
        display_player(0);

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display recent messages in sub-windows
 *
 * Adjust for width and split messages.  XXX XXX XXX
 */
static void fix_message(void)
{
    int j, i;
    int w, h;
    int x, y;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_MESSAGE)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Get size */
        Term_get_size(&w, &h);

        /* Dump messages */
        for (i = 0; i < h; i++)
        {
            byte color = message_color((s16b)i);

            /* Dump the message on the appropriate line */
            Term_putstr(0, (h - 1) - i, -1, color, message_str((s16b)i));

            /* Cursor */
            Term_locate(&x, &y);

            /* Clear to end of line */
            Term_erase(x, y, 255);
        }

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display monster recall in sub-windows
 */
static void fix_monster(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_MONSTER)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display monster race info */
        if (p_ptr->monster_race_idx)
            display_roff(p_ptr->monster_race_idx, NULL);

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Calculate maximum voice.
 *
 * This function induces status messages.
 */

/*
 * Handle "p_ptr->notice"
 */
void notice_stuff(void)
{
    /* Notice stuff */
    if (!p_ptr->notice)
        return;

    /* Combine the pack */
    if (p_ptr->notice & (PN_COMBINE))
    {
        p_ptr->notice &= ~(PN_COMBINE);
        combine_pack();
    }

    /* Reorder the pack */
    if (p_ptr->notice & (PN_REORDER))
    {
        p_ptr->notice &= ~(PN_REORDER);
        reorder_pack(true);
    }

    if (p_ptr->notice & PN_AUTOINSCRIBE)
    {
        p_ptr->notice &= ~(PN_AUTOINSCRIBE);
        autoinscribe_pack();
        autoinscribe_ground();
    }
}

/*
 * Handle "p_ptr->update"
 */
void update_stuff(void)
{
    player_update_lore();

    /* Update stuff */
    if (!p_ptr->update) {
        // log_trace("update_stuff: no updates needed");
        return;
    }

    log_trace("update_stuff: processing updates 0x%08X", p_ptr->update);

    if (p_ptr->update & (PU_BONUS))
    {
        p_ptr->update &= ~(PU_BONUS);
        // log_trace("update_stuff: calculating bonuses");
        calc_bonuses();
    }

    if (p_ptr->update & (PU_HP))
    {
        p_ptr->update &= ~(PU_HP);
        // log_trace("update_stuff: calculating hitpoints");
        calc_hitpoints();
    }

    if (p_ptr->update & (PU_MANA))
    {
        p_ptr->update &= ~(PU_MANA);
        // log_trace("update_stuff: calculating voice/mana");
        calc_voice();
    }

    /* Character is not ready yet, no screen updates */
    if (!character_generated) {
        // log_trace("update_stuff: character not generated yet, skipping screen updates");
        return;
    }

    // log_trace("update_stuff: character_icky=%d", character_icky);

    /* Character is in "icky" mode, no screen updates */
    if (character_icky) {
        // log_trace("update_stuff: character in icky mode (value=%d), skipping screen updates", character_icky);
        return;
    }

    if (p_ptr->update & (PU_FORGET_VIEW))
    {
        p_ptr->update &= ~(PU_FORGET_VIEW);
        log_trace("update_stuff: forgetting view");
        forget_view();
    }

    if (p_ptr->update & (PU_UPDATE_VIEW))
    {
        p_ptr->update &= ~(PU_UPDATE_VIEW);
        log_trace("update_stuff: updating view");
        update_view();
        
        /* Check artifact visibility after view update */
        check_artifact_visibility();
    }

    if (p_ptr->update & (PU_DISTANCE))
    {
        p_ptr->update &= ~(PU_DISTANCE);
        p_ptr->update &= ~(PU_MONSTERS);
        log_trace("update_stuff: updating distances and monsters");
        update_monsters(true);
    }

    if (p_ptr->update & (PU_MONSTERS))
    {
        p_ptr->update &= ~(PU_MONSTERS);
        update_monsters(false);
    }

    if (p_ptr->update & (PU_PANEL))
    {
        p_ptr->update &= ~(PU_PANEL);
        verify_panel();
    }

    /* Check quest completion status for metarun tracking */
    // log_trace("update_stuff: About to call metarun_check_and_update_quests()");
    metarun_check_and_update_quests();
    // log_trace("update_stuff: Finished calling metarun_check_and_update_quests()");

    // log_trace("update_stuff: completed all updates");
}

/*
 * Handle "p_ptr->redraw"
 */
void redraw_stuff(void)
{
    bool hidden_overlay_needs_refresh = false;

    /* Redraw stuff */
    if (!p_ptr->redraw) {
        // log_trace("redraw_stuff: no redraws needed");
        return;
    }

    // log_trace("redraw_stuff: processing redraws 0x%08X", p_ptr->redraw);

    /* Character is not ready yet, no screen updates */
    if (!character_generated)
        return;

    // log_trace("redraw_stuff: character_icky=%d, character_generated=%s", 
            //   character_icky, character_generated ? "true" : "false");

    /* Character is in "icky" mode, no screen updates */
    if (character_icky && !p_ptr->is_dead) {
        // log_trace("redraw_stuff: character in icky mode (value=%d), skipping screen updates", character_icky);
        return;
    }

    if (p_ptr->redraw & (PR_MAP))
    {
        p_ptr->redraw &= ~(PR_MAP);
        log_trace("redraw_stuff: redrawing map");
        prt_map();
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_BASIC))
    {
        p_ptr->redraw &= ~(PR_BASIC);
        p_ptr->redraw &= ~(PR_STATS);
        p_ptr->redraw &= ~(PR_MEL | PR_EXP | PR_ARC | PR_QUIVER);
        p_ptr->redraw &= ~(PR_ARMOR | PR_HP | PR_VOICE | PR_SONG | PR_LIGHT);
        p_ptr->redraw &= ~(PR_DEPTH | PR_HEALTHBAR);
        p_ptr->redraw &= ~(PR_RESIST);
        prt_frame_basic();
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_MISC))
    {
        p_ptr->redraw &= ~(PR_MISC);

        if (!ui_hide_left_panel())
        {
            /* Name */
            c_put_str(TERM_WHITE, "            ", ROW_NAME, COL_NAME);
            if (strlen(op_ptr->full_name) <= 12)
            {
                prt_field(op_ptr->full_name, ROW_NAME, COL_NAME);
            }
        }
    }

    if (p_ptr->redraw & (PR_EXP))
    {
        p_ptr->redraw &= ~(PR_EXP);
        if (!ui_hide_left_panel())
            prt_exp();
    }

    if (p_ptr->redraw & (PR_STATS))
    {
        p_ptr->redraw &= ~(PR_STATS);
        if (!ui_hide_left_panel())
        {
            prt_stat(A_STR);
            prt_stat(A_DEX);
            prt_stat(A_CON);
            prt_stat(A_GRA);
        }
    }

    if (p_ptr->redraw & (PR_MEL))
    {
        p_ptr->redraw &= ~(PR_MEL);
        if (!ui_hide_left_panel())
            prt_mel();
    }

    if (p_ptr->redraw & (PR_ARC))
    {
        p_ptr->redraw &= ~(PR_ARC);
        if (!ui_hide_left_panel())
            prt_arc();
    }

    if (p_ptr->redraw & (PR_QUIVER))
    {
        p_ptr->redraw &= ~(PR_QUIVER);
        if (!ui_hide_left_panel())
            prt_quiver();
    }

    if (p_ptr->redraw & (PR_ARMOR))
    {
        p_ptr->redraw &= ~(PR_ARMOR);
        if (!ui_hide_left_panel())
            prt_evn();
    }

    if (p_ptr->redraw & (PR_HP))
    {
        p_ptr->redraw &= ~(PR_HP);
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
        else
            prt_hp();

        /*
         * hack:  redraw player, since the player's color
         * now indicates approximate health.
         */
        if (runtime_cli_graphics_mode() == GRAPHICS_NONE)
        {
            lite_spot(p_ptr->py, p_ptr->px);
        }

        if (!ui_hide_left_panel())
        {
            /* Also update the monospace character health graphic */
            prt_char_health_graphic();
        }
    }

    if (p_ptr->redraw & (PR_VOICE))
    {
        p_ptr->redraw &= ~(PR_VOICE);
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
        else
            prt_sp();
    }

    if (p_ptr->redraw & (PR_LIGHT))
    {
        p_ptr->redraw &= ~(PR_LIGHT);
        if (!ui_hide_left_panel())
            prt_light();
    }

    /* Sil - Hack: always redraw song (really should invent redraw flag for it
     * etc. */
    if (p_ptr->redraw & (PR_SONG))
    {
        p_ptr->redraw &= ~(PR_SONG);
        if (!ui_hide_left_panel())
            prt_song();
        else
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_DEPTH))
    {
        p_ptr->redraw &= ~(PR_DEPTH);
        prt_depth();
    }

    if (p_ptr->redraw & (PR_HEALTHBAR))
    {
        p_ptr->redraw &= ~(PR_HEALTHBAR);
        if (!ui_hide_left_panel())
            health_redraw();
        else
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_EXTRA))
    {
        p_ptr->redraw &= ~(PR_EXTRA);
        p_ptr->redraw &= ~(PR_CUT | PR_STUN);
        p_ptr->redraw &= ~(PR_HUNGER);
        p_ptr->redraw &= ~(PR_BLIND | PR_CONFUSED);
        p_ptr->redraw &= ~(PR_AFRAID | PR_POISONED);
        p_ptr->redraw &= ~(PR_STATE | PR_SPEED);
        prt_frame_extra();
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_CUT))
    {
        p_ptr->redraw &= ~(PR_CUT);
        if (!ui_hide_left_panel())
            prt_cut();
        else
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_STUN))
    {
        p_ptr->redraw &= ~(PR_STUN);
        prt_stun();
    }

    if (p_ptr->redraw & (PR_HUNGER))
    {
        p_ptr->redraw &= ~(PR_HUNGER);
        prt_hunger();
    }

    if (p_ptr->redraw & (PR_BLIND))
    {
        p_ptr->redraw &= ~(PR_BLIND);
        prt_blind();
    }

    if (p_ptr->redraw & (PR_CONFUSED))
    {
        p_ptr->redraw &= ~(PR_CONFUSED);
        prt_confused();
    }

    if (p_ptr->redraw & (PR_AFRAID))
    {
        p_ptr->redraw &= ~(PR_AFRAID);
        prt_afraid();
    }

    if (p_ptr->redraw & (PR_POISONED))
    {
        p_ptr->redraw &= ~(PR_POISONED);
        if (!ui_hide_left_panel())
            prt_poisoned();
        else
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_STATE))
    {
        p_ptr->redraw &= ~(PR_STATE);
        prt_state();
    }

    if (p_ptr->redraw & (PR_SPEED))
    {
        p_ptr->redraw &= ~(PR_SPEED);
        prt_speed();
    }

    if (p_ptr->redraw & (PR_TERRAIN))
    {
        p_ptr->redraw &= ~(PR_TERRAIN);
        prt_terrain();
    }

    if (ui_hide_left_panel() && hidden_overlay_needs_refresh)
        prt_hidden_top_vitals();

    // log_trace("redraw_stuff: completed all redraws");
}

/*
 * Handle "p_ptr->window"
 */
void window_stuff(void)
{
    int j;

    u32b mask = 0L;

    /* Nothing to do */
    if (!p_ptr->window) {
        // log_trace("window_stuff: no window updates needed");
        return;
    }

    log_trace("window_stuff: processing windows 0x%08X", p_ptr->window);

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Save usable flags */
        if (angband_term[j])
        {
            /* Build the mask */
            mask |= op_ptr->window_flag[j];
        }
    }

    /* Apply usable flags */
    p_ptr->window &= (mask);

    /* Nothing to do */
    if (!p_ptr->window)
        return;

    /* Display inventory */
    if (p_ptr->window & (PW_INVEN))
    {
        p_ptr->window &= ~(PW_INVEN);
        fix_inven();
    }

    /* Display monster list */
    if (p_ptr->window & (PW_MONLIST))
    {
        p_ptr->window &= ~(PW_MONLIST);
        fix_monlist();
    }

    /* Display equipment */
    if (p_ptr->window & (PW_EQUIP))
    {
        log_trace("window_stuff: PW_EQUIP flag set, calling fix_equip()");
        p_ptr->window &= ~(PW_EQUIP);
        fix_equip();
        log_trace("window_stuff: fix_equip() completed");
        
        /* Also trigger quiver redraw since quiver is part of equipment */
        p_ptr->redraw |= (PR_QUIVER);
    }

    /* Display player (mode 0) */
    if (p_ptr->window & (PW_PLAYER_0))
    {
        p_ptr->window &= ~(PW_PLAYER_0);
        fix_player_0();
    }

    /* Display combat rolls */
    if (p_ptr->window & (PW_COMBAT_ROLLS))
    {
        p_ptr->window &= ~(PW_COMBAT_ROLLS);
        fix_combat_rolls();
    }

    /* Display message recall */
    if (p_ptr->window & (PW_MESSAGE))
    {
        p_ptr->window &= ~(PW_MESSAGE);
        fix_message();
    }

    /* Display monster recall */
    if (p_ptr->window & (PW_MONSTER))
    {
        p_ptr->window &= ~(PW_MONSTER);
        fix_monster();
    }

    // log_trace("window_stuff: completed all window updates");
}

/*
 * Handle "p_ptr->update" and "p_ptr->redraw" and "p_ptr->window"
 */
void handle_stuff(void)
{
    u32b update_mask = p_ptr->update;
    u32b redraw_mask = p_ptr->redraw;
    u32b window_mask = p_ptr->window;

    log_trace("handle_stuff: starting (update=0x%08X, redraw=0x%08X, window=0x%08X)", 
              p_ptr->update, p_ptr->redraw, p_ptr->window);

    /* Update stuff */
    if (p_ptr->update)
        update_stuff();

    /* Redraw stuff */
    if (p_ptr->redraw)
        redraw_stuff();

    /* Window stuff */
    if (p_ptr->window)
        window_stuff();

    if (character_generated && p_ptr->playing)
    {
        app_session* session = app_session_current();

        if (session && (app_session_snapshot(session)->scene
                == APP_SCENE_KIND_DUNGEON))
        {
            (void)app_session_build_dungeon_snapshot(session, update_mask,
                redraw_mask, window_mask);
        }
    }

    log_trace("handle_stuff: completed");
}

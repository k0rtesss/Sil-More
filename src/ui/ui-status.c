/* File: ui/ui-status.c */

#include "angband.h"
#include "app/app-scene-dungeon.h"
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
    return g_hide_left_panel;
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

static void prt_status_line_compact(void);
static void prt_cut_poisoned_compact(void);
static bool ui_semantic_dungeon_snapshot_active(void);

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
    app_status_quiver_live quiver;
    char buf1[16];
    char buf2[16];
    int total_width;
    int start_col;

    /* Clear the entire line (12 characters) */
    Term_erase(COL_QUIVER, ROW_QUIVER, 12);
    if (!app_status_quiver_live_build(&quiver))
        return;

    /* Format the count strings */
    strnfmt(buf1, sizeof(buf1), "%d/%d", quiver.q1_current, quiver.q1_max);
    strnfmt(buf2, sizeof(buf2), "%d/%d", quiver.q2_current, quiver.q2_max);
    
    /* Calculate total width */
    if (quiver.same_type)
    {
        /* Layout: "11/48[→][→]7/7" */
        total_width = strlen(buf1) + (use_bigtile ? 2 : 2) + strlen(buf2);
    }
    else
    {
        /* Layout: "[|][|]11/48[/][/]7/7" */
        total_width = 0;
        if (quiver.q1_active)
            total_width += (use_bigtile ? 2 : 2) + strlen(buf1);
        if (quiver.q2_active)
            total_width += (use_bigtile ? 2 : 2) + strlen(buf2);
    }
    
    /* Right-align: start at column that makes it end at column 11 */
    start_col = COL_QUIVER + 12 - total_width;
    if (start_col < COL_QUIVER) start_col = COL_QUIVER;
    
    int col = start_col;

    if (quiver.same_type)
    {
        /* Same type: counts with icon in middle */
        byte attr = quiver.q1_attr;
        char icon = quiver.q1_char;
        
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
        if (quiver.q1_active)
        {
            /* Q1: "[icon][icon]cur/max" */
            byte attr = quiver.q1_attr;
            char icon = quiver.q1_char;
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
        
        if (quiver.q2_active)
        {
            /* Q2: "[icon][icon]cur/max" */
            byte attr = quiver.q2_attr;
            char icon = quiver.q2_char;
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
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;

    /* Clear the line so shorter values don't leave stale characters */
    Term_erase(COL_EVN, ROW_EVN, 12);
    if (!app_status_text_live(APP_STATUS_TEXT_EVASION, text, sizeof(text), &attr))
        return;

    c_put_str(attr, text, ROW_EVN, COL_EVN + 12 - strlen(text));
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
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_L_WHITE;

    /* Clear the line */
    Term_erase(icon_col, ROW_LIGHT, 13);

    /* Nothing equipped */
    if (!o_ptr->k_idx)
        return;

    byte icon_attr = object_attr(o_ptr);
    char icon = object_char(o_ptr);

    /* Draw the icon (supporting bigtile visuals) */
    Term_putch(icon_col, ROW_LIGHT, icon_attr, icon);
    if (use_bigtile)
    {
        Term_putch(icon_col + 1, ROW_LIGHT, 255, -1);
    }
    else
    {
        Term_putch(icon_col + 1, ROW_LIGHT, icon_attr, icon);
    }

    Term_putch(icon_col + 2, ROW_LIGHT, TERM_WHITE, ' ');
    if (!app_status_text_live(APP_STATUS_TEXT_LIGHT, text, sizeof(text), &attr))
        return;

    c_put_str(attr, text, ROW_LIGHT, icon_col + 12 - strlen(text));
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

static bool ui_semantic_dungeon_snapshot_active(void)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (!session)
        return false;

    snapshot = app_session_snapshot(session);
    return snapshot && (snapshot->scene == APP_SCENE_KIND_DUNGEON);
}

/*
 * Prints player's current song (if any)
 */
static void prt_song(void)
{
    char primary[APP_DUNGEON_STATUS_TEXT_MAX];
    char secondary[APP_DUNGEON_STATUS_TEXT_MAX];
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte primary_attr = TERM_L_BLUE;
    byte secondary_attr = TERM_BLUE;

    if (ui_compact_status_line_handles_song())
    {
        prt_status_line_compact();
        return;
    }

    // wipe old songs
    put_str("             ", ROW_SONG, COL_SONG);
    if (!ui_compact_height())
        put_str("             ", ROW_SONG + 1, COL_SONG);

    sdl_story_font_enable();

    if (ui_compact_height())
    {
        text[0] = '\0';
        if (app_status_text_live(APP_STATUS_TEXT_SONG, text, sizeof(text),
                NULL))
        {
            c_put_str(TERM_L_BLUE, text, ROW_SONG, COL_SONG);
        }
    }
    else
    {
        primary[0] = '\0';
        secondary[0] = '\0';
        (void)app_status_song_lines_live(primary, sizeof(primary),
            &primary_attr, secondary, sizeof(secondary), &secondary_attr);

        if (primary[0])
        {
            c_put_str(primary_attr, primary, ROW_SONG, COL_SONG);
        }

        if (secondary[0])
        {
            c_put_str(secondary_attr, secondary, ROW_SONG + 1, COL_SONG);
        }
    }

    sdl_story_font_disable();
}

/*
 * Prints depth in stat area
 */
static void prt_depth(void)
{
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    byte attr = TERM_WHITE;
    text[0] = '\0';
    (void)app_status_text_live(APP_STATUS_TEXT_DEPTH, text, sizeof(text), &attr);

    sdl_story_font_enable();

    /* Right-Adjust the "depth", and clear old values */
    c_prt(attr, format("%7s", text), ROW_DEPTH, COL_DEPTH);

    sdl_story_font_disable();
}

/*
 * Prints status of hunger
 */
static void prt_hunger(void)
{
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    char padded[9];
    byte attr = TERM_L_GREEN;

    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    sdl_story_font_enable();
    text[0] = '\0';
    if (!app_status_text_live(APP_STATUS_TEXT_HUNGER, text, sizeof(text), &attr))
        text[0] = '\0';
    strnfmt(padded, sizeof(padded), "%-8s", text);
    c_put_str(attr, padded, ROW_HUNGRY, COL_HUNGRY);

    sdl_story_font_disable();
}

/*
 * Prints Blind status
 */
static void prt_blind(void)
{
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;

    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    sdl_story_font_enable();
    if (app_status_text_live(APP_STATUS_TEXT_BLIND, text, sizeof(text), &attr))
        c_put_str(attr, text, ROW_BLIND, COL_BLIND);
    else
        put_str("     ", ROW_BLIND, COL_BLIND);

    sdl_story_font_disable();
}

/*
 * Prints Confusion status
 */
static void prt_confused(void)
{
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;

    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_CONFUSED, ROW_CONFUSED, 8);

    if (app_status_text_live(APP_STATUS_TEXT_CONFUSED, text, sizeof(text),
            &attr))
    {
        sdl_story_font_enable();
        c_put_str(attr, text, ROW_CONFUSED, COL_CONFUSED);
        sdl_story_font_disable();
    }
}

/*
 * Prints Fear status
 */
static void prt_afraid(void)
{
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;

    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_AFRAID, ROW_AFRAID, 6);

    if (app_status_text_live(APP_STATUS_TEXT_AFRAID, text, sizeof(text),
            &attr))
    {
        sdl_story_font_enable();
        c_put_str(attr, text, ROW_AFRAID, COL_AFRAID);
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
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;
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

    int r = ROW_CUT;

    if (p_ptr->poisoned)
        r--;

    /* Clear both possible rows (story font has variable widths) */
    Term_erase(COL_CUT, ROW_CUT - 1, 12);
    if (!p_ptr->poisoned)
        Term_erase(COL_CUT, ROW_CUT, 12);

    if (app_status_text_live(APP_STATUS_TEXT_CUT, text, sizeof(text), &attr))
    {
        sdl_story_font_enable();
        c_put_str(attr, text, r, COL_CUT);
        sdl_story_font_disable();
    }
}

/*
 * Prints Poisoned status
 */
static void prt_poisoned(void)
{
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;
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

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_POISONED, ROW_POISONED, 12);

    if (app_status_text_live(APP_STATUS_TEXT_POISONED, text, sizeof(text),
            &attr))
    {
        sdl_story_font_enable();
        c_put_str(attr, text, ROW_POISONED, COL_POISONED);
        sdl_story_font_disable();
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
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;

    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_STATE, ROW_STATE, 10);

    /* Display the info if any */
    if (app_status_text_live(APP_STATUS_TEXT_STATE, text, sizeof(text), &attr))
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
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;

    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_SPEED, ROW_SPEED, 4);

    /* Display the speed if not normal */
    if (app_status_text_live(APP_STATUS_TEXT_SPEED, text, sizeof(text), &attr))
    {
        sdl_story_font_enable();
        c_put_str(attr, text, ROW_SPEED, COL_SPEED);
        sdl_story_font_disable();
    }
}

static void prt_partition(void)
{
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;

    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    if (!p_ptr)
        return;

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_PARTITION, ROW_PARTITION, 5);

    if (!app_status_text_live(APP_STATUS_TEXT_PARTITION, text, sizeof(text),
            &attr))
    {
        return;
    }

    sdl_story_font_enable();
    c_put_str(attr, text, ROW_PARTITION, COL_PARTITION);
    sdl_story_font_disable();
}

/*
 * Prints message regarding difficult terrain
 */
static void prt_terrain(void)
{
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;

    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_TERRAIN, ROW_TERRAIN, 5);

    if (app_status_text_live(APP_STATUS_TEXT_TERRAIN, text, sizeof(text),
            &attr))
    {
        sdl_story_font_enable();
        c_put_str(attr, text, ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }

    prt_partition();
}

static void prt_cut_poisoned_compact(void)
{
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;
    if (!Term || !p_ptr)
        return;

    const int row = ROW_CUT;
    const int col = COL_CUT;
    const int width = 12;

    Term_erase(col, row, width);

    int x = col;

    if (app_status_text_live(APP_STATUS_TEXT_CUT, text, sizeof(text), &attr))
    {
        char cut_buf[16];

        if (!strcmp(text, "Mortal wound"))
        {
            SDL_strlcpy(cut_buf, "MW", sizeof(cut_buf));
        }
        else
        {
            strnfmt(cut_buf, sizeof(cut_buf), "Bld:%d", p_ptr->cut);
        }

        int len = (int)strlen(cut_buf);
        if (x + len > col + width)
            len = (col + width) - x;
        if (len > 0)
            Term_putstr(x, row, len, attr, cut_buf);
        x += len;
    }

    if (app_status_text_live(APP_STATUS_TEXT_POISONED, text, sizeof(text),
            &attr)
        && x < col + width)
    {
        if (x > col && x < col + width)
        {
            Term_putstr(x, row, 1, TERM_WHITE, " ");
            x++;
        }

        char pois_buf[16];
        strnfmt(pois_buf, sizeof(pois_buf), "Poi:%d", p_ptr->poisoned);
        int len = (int)strlen(pois_buf);
        if (x + len > col + width)
            len = (col + width) - x;
        if (len > 0)
            Term_putstr(x, row, len, attr, pois_buf);
    }
}

static void prt_stun(void)
{
    char text[APP_DUNGEON_STATUS_TEXT_MAX];
    byte attr = TERM_WHITE;

    if (ui_compact_width())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_STUN, ROW_STUN, 12);

    if (app_status_text_live(APP_STATUS_TEXT_STUN, text, sizeof(text), &attr))
    {
        sdl_story_font_enable();
        c_put_str(attr, text, ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
}

static int status_line_len(const app_status_compact_segment* segments, int count,
                           bool use_long, const bool* include)
{
    int len = 0;
    int shown = 0;

    for (int i = 0; i < count; i++)
    {
        if (include && !include[i])
            continue;
        const char* t = use_long ? segments[i].long_text
                                 : segments[i].short_text;

        if (!t || !t[0])
            continue;
        if (shown > 0)
            len += 1;
        len += (int)strlen(t);
        shown++;
    }
    return len;
}

static void prt_status_line_compact(void)
{
    app_status_compact_line compact;
    bool fold_song;
    bool fold_wounds;
    int max_w;
    bool include[APP_DUNGEON_COMPACT_SEGMENT_MAX];
    bool use_long;
    int x = 0;
    bool first = true;

    if (!Term || !p_ptr)
        return;

    const int row = Term->hgt - 1;
    if (row < 0)
        return;

    Term_erase(0, row, Term->wid);

    fold_song = ui_compact_status_line_handles_song();
    fold_wounds = ui_compact_status_line_handles_wounds();
    if (!app_status_compact_line_build_live(&compact, fold_song, fold_wounds))
        return;

    max_w = Term->wid;
    if (max_w <= 0)
        return;

    for (int i = 0; i < compact.segment_count; i++)
        include[i] = true;

    use_long = (status_line_len(compact.segments, compact.segment_count, true,
        include) <= max_w);
    if (!use_long)
    {
        while (status_line_len(compact.segments, compact.segment_count, false,
                include) > max_w)
        {
            bool dropped = false;
            for (int i = compact.segment_count - 1; i >= 0; i--)
            {
                if (!include[i])
                    continue;
                if (compact.segments[i].required)
                    continue;
                include[i] = false;
                dropped = true;
                break;
            }
            if (!dropped)
                break;
        }
    }

    for (int i = 0; i < compact.segment_count; i++)
    {
        const app_status_compact_segment* segment;
        if (!include[i])
            continue;

        segment = &compact.segments[i];
        const char* t = use_long ? segment->long_text : segment->short_text;
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
            Term_putstr(x, row, n, segment->attr, t);
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
    app_status_tracked_monster_live tracked;
    if (ui_hide_left_panel())
        return;

    if (!app_status_tracked_monster_live_build(&tracked))
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
        byte attr = tracked.hp_attr;

        /* Afraid */
        // if (m_ptr->stance == STANCE_FLEEING) attr = TERM_VIOLET;

        /* Convert into health bar (using ceiling for length) */
        len = (8 * tracked.hp_cur + tracked.hp_max - 1) / tracked.hp_max;

        /* Default to "unknown" */
        Term_putstr(COL_INFO, ROW_INFO, 12, TERM_L_DARK, "  --------  ");

        /* Dump the current "health" (handle monster stunning, confusion) */

        if (tracked.confused && tracked.stunned)
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "cscscscs");
        else if (tracked.confused)
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "cccccccc");
        else if (tracked.stunned)
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "ssssssss");
        else
            Term_putstr(COL_INFO + 2, ROW_INFO, len, attr, "********");

        Term_erase(COL_INFO, ROW_INFO + 1, 12);

        if (!tracked.alertness[0])
            return;

        Term_putstr(COL_INFO + (13 - strlen(tracked.alertness)) / 2,
            ROW_INFO + 1, MIN(strlen(tracked.alertness), 12),
            tracked.alertness_attr, tracked.alertness);
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
    bool render_main_term_chrome;

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

    render_main_term_chrome = !ui_semantic_dungeon_snapshot_active();

    if (p_ptr->redraw & (PR_MAP))
    {
        p_ptr->redraw &= ~(PR_MAP);
        log_trace("redraw_stuff: redrawing map");
        prt_map();
    }

    if (p_ptr->redraw & (PR_BASIC))
    {
        p_ptr->redraw &= ~(PR_BASIC);
        p_ptr->redraw &= ~(PR_STATS);
        p_ptr->redraw &= ~(PR_MEL | PR_EXP | PR_ARC | PR_QUIVER);
        p_ptr->redraw &= ~(PR_ARMOR | PR_HP | PR_VOICE | PR_SONG | PR_LIGHT);
        p_ptr->redraw &= ~(PR_DEPTH | PR_HEALTHBAR);
        p_ptr->redraw &= ~(PR_RESIST);
        if (render_main_term_chrome)
            prt_frame_basic();
    }

    if (p_ptr->redraw & (PR_MISC))
    {
        p_ptr->redraw &= ~(PR_MISC);

        if (render_main_term_chrome && !ui_hide_left_panel())
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
        if (render_main_term_chrome && !ui_hide_left_panel())
            prt_exp();
    }

    if (p_ptr->redraw & (PR_STATS))
    {
        p_ptr->redraw &= ~(PR_STATS);
        if (render_main_term_chrome && !ui_hide_left_panel())
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
        if (render_main_term_chrome && !ui_hide_left_panel())
            prt_mel();
    }

    if (p_ptr->redraw & (PR_ARC))
    {
        p_ptr->redraw &= ~(PR_ARC);
        if (render_main_term_chrome && !ui_hide_left_panel())
            prt_arc();
    }

    if (p_ptr->redraw & (PR_QUIVER))
    {
        p_ptr->redraw &= ~(PR_QUIVER);
        if (render_main_term_chrome && !ui_hide_left_panel())
            prt_quiver();
    }

    if (p_ptr->redraw & (PR_ARMOR))
    {
        p_ptr->redraw &= ~(PR_ARMOR);
        if (render_main_term_chrome && !ui_hide_left_panel())
            prt_evn();
    }

    if (p_ptr->redraw & (PR_HP))
    {
        p_ptr->redraw &= ~(PR_HP);
        if (render_main_term_chrome && !ui_hide_left_panel())
            prt_hp();

        /*
         * hack:  redraw player, since the player's color
         * now indicates approximate health.
         */
        if (runtime_cli_graphics_mode() == GRAPHICS_NONE)
        {
            lite_spot(p_ptr->py, p_ptr->px);
        }

        if (render_main_term_chrome && !ui_hide_left_panel())
        {
            /* Also update the monospace character health graphic */
            prt_char_health_graphic();
        }
    }

    if (p_ptr->redraw & (PR_VOICE))
    {
        p_ptr->redraw &= ~(PR_VOICE);
        if (render_main_term_chrome && !ui_hide_left_panel())
            prt_sp();
    }

    if (p_ptr->redraw & (PR_LIGHT))
    {
        p_ptr->redraw &= ~(PR_LIGHT);
        if (render_main_term_chrome && !ui_hide_left_panel())
            prt_light();
    }

    /* Sil - Hack: always redraw song (really should invent redraw flag for it
     * etc. */
    if (p_ptr->redraw & (PR_SONG))
    {
        p_ptr->redraw &= ~(PR_SONG);
        if (render_main_term_chrome && !ui_hide_left_panel())
            prt_song();
    }

    if (p_ptr->redraw & (PR_DEPTH))
    {
        p_ptr->redraw &= ~(PR_DEPTH);
        if (render_main_term_chrome)
            prt_depth();
    }

    if (p_ptr->redraw & (PR_HEALTHBAR))
    {
        p_ptr->redraw &= ~(PR_HEALTHBAR);
        if (render_main_term_chrome && !ui_hide_left_panel())
            health_redraw();
    }

    if (p_ptr->redraw & (PR_EXTRA))
    {
        p_ptr->redraw &= ~(PR_EXTRA);
        p_ptr->redraw &= ~(PR_CUT | PR_STUN);
        p_ptr->redraw &= ~(PR_HUNGER);
        p_ptr->redraw &= ~(PR_BLIND | PR_CONFUSED);
        p_ptr->redraw &= ~(PR_AFRAID | PR_POISONED);
        p_ptr->redraw &= ~(PR_STATE | PR_SPEED);
        if (render_main_term_chrome)
            prt_frame_extra();
    }

    if (p_ptr->redraw & (PR_CUT))
    {
        p_ptr->redraw &= ~(PR_CUT);
        if (render_main_term_chrome && !ui_hide_left_panel())
            prt_cut();
    }

    if (p_ptr->redraw & (PR_STUN))
    {
        p_ptr->redraw &= ~(PR_STUN);
        if (render_main_term_chrome)
            prt_stun();
    }

    if (p_ptr->redraw & (PR_HUNGER))
    {
        p_ptr->redraw &= ~(PR_HUNGER);
        if (render_main_term_chrome)
            prt_hunger();
    }

    if (p_ptr->redraw & (PR_BLIND))
    {
        p_ptr->redraw &= ~(PR_BLIND);
        if (render_main_term_chrome)
            prt_blind();
    }

    if (p_ptr->redraw & (PR_CONFUSED))
    {
        p_ptr->redraw &= ~(PR_CONFUSED);
        if (render_main_term_chrome)
            prt_confused();
    }

    if (p_ptr->redraw & (PR_AFRAID))
    {
        p_ptr->redraw &= ~(PR_AFRAID);
        if (render_main_term_chrome)
            prt_afraid();
    }

    if (p_ptr->redraw & (PR_POISONED))
    {
        p_ptr->redraw &= ~(PR_POISONED);
        if (render_main_term_chrome && !ui_hide_left_panel())
            prt_poisoned();
    }

    if (p_ptr->redraw & (PR_STATE))
    {
        p_ptr->redraw &= ~(PR_STATE);
        if (render_main_term_chrome)
            prt_state();
    }

    if (p_ptr->redraw & (PR_SPEED))
    {
        p_ptr->redraw &= ~(PR_SPEED);
        if (render_main_term_chrome)
            prt_speed();
    }

    if (p_ptr->redraw & (PR_TERRAIN))
    {
        p_ptr->redraw &= ~(PR_TERRAIN);
        if (render_main_term_chrome)
            prt_terrain();
    }

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

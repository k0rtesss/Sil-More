/* File: cmd-ui-abilities.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "main-sdl.h"
#include "object/object-ui-select.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-oaths.h"
#include "sound-config.h"
#include "sdl-sound.h"

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

#define COL_SKILL 2
#define COL_ABILITY 16
#define COL_DESCRIPTION 41
#define ABILITY_MENU_LIST_WIDTH (COL_DESCRIPTION - COL_ABILITY)

static bool ability_menu_use_compact_layout(void)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    return (wid < 80);
}
static int ability_menu_list_col(void)
{
    return ability_menu_use_compact_layout() ? COL_SKILL : COL_ABILITY;
}

static int ability_menu_description_col(void)
{
    return ability_menu_use_compact_layout()
        ? COL_SKILL + ABILITY_MENU_LIST_WIDTH
        : COL_DESCRIPTION;
}

static int ability_menu_description_wrap(int desc_col)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    if (wid <= desc_col)
        return desc_col + 1;

    return wid - 1;
}

static int ability_menu_text_width(int desc_col, int indent)
{
    int wrap = ability_menu_description_wrap(desc_col);
    int start = desc_col + indent;

    if (wrap < start)
        return 1;

    return wrap - start + 1;
}

static void ability_menu_format_amount_line(char* buf, size_t buflen,
    cptr long_label, cptr short_label, int need, int have, int max_width)
{
    if (max_width <= 30)
        strnfmt(buf, buflen, "%s %d / %d", short_label, need, have);
    else
        strnfmt(buf, buflen, "%d %s (you have %d)", need, long_label, have);
}

static int ability_menu_next_row_after_text(int desc_col, int fallback_row)
{
    int x = desc_col;
    int y = fallback_row;

    Term_locate(&x, &y);

    if (x > desc_col)
        y++;

    return y;
}

static void ability_menu_render_prerequisites_block(int skilltype,
    const ability_type* b_ptr, int desc_col)
{
    int j;
    int row = ability_menu_next_row_after_text(desc_col, 3);
    int info_width = ability_menu_text_width(desc_col, 2);
    char buf[80];

    Term_putstr(desc_col, row, -1, TERM_YELLOW, "Prerequisites:");

    ability_menu_format_amount_line(buf, sizeof(buf), "skill points", "Skill",
        b_ptr->level, p_ptr->skill_base[skilltype], info_width);

    Term_putstr(desc_col + 2, row + 1, -1,
        (b_ptr->level <= p_ptr->skill_base[skilltype]) ? TERM_L_GREEN
                                                       : TERM_L_DARK,
        buf);

    row += 2;

    if (!p_ptr->active_ability[S_PER][PER_QUICK_STUDY])
    {
        for (j = 0; j < b_ptr->prereqs; j++)
        {
            if (j == 0)
            {
                strnfmt(buf, sizeof(buf), "%s",
                    b_name
                        + (&b_info[ability_index(b_ptr->prereq_skilltype[j],
                               b_ptr->prereq_abilitynum[j])])
                              ->name);
            }
            else
            {
                strnfmt(buf, sizeof(buf), "or %s",
                    b_name
                        + (&b_info[ability_index(b_ptr->prereq_skilltype[j],
                               b_ptr->prereq_abilitynum[j])])
                              ->name);
            }

            Term_putstr(j == 0 ? desc_col + 2 : desc_col + 5, row + j, -1,
                p_ptr->innate_ability[b_ptr->prereq_skilltype[j]]
                                 [b_ptr->prereq_abilitynum[j]]
                    ? TERM_L_GREEN
                    : TERM_L_DARK,
                buf);
        }

        row += b_ptr->prereqs;
    }
    else if (b_ptr->prereqs > 0)
    {
        Term_putstr(desc_col + 2, row, -1, TERM_GREEN, "Quick Study");
        row++;
    }

    if (skilltype != S_SPC && prereqs(skilltype, b_ptr->abilitynum))
    {
        int is_free = (c_info[p_ptr->pcharacter].flags & RHF_FREE) ? 1 : 0;
        int unit_cost = 500 - 200 * is_free;
        int exp_cost = (abilities_in_skill(skilltype) + 1) * unit_cost;

        exp_cost -= unit_cost * affinity_level(skilltype);

        if (skilltype == S_SNG)
            exp_cost -= unit_cost * minstrel_level();

        if (exp_cost < 0)
            exp_cost = 0;

        Term_putstr(desc_col, row, -1, TERM_YELLOW, "Current price:");

        ability_menu_format_amount_line(buf, sizeof(buf), "experience", "Exp",
            exp_cost, p_ptr->new_exp, info_width);
        Term_putstr(desc_col + 2, row + 1, -1,
            (exp_cost <= p_ptr->new_exp) ? TERM_L_GREEN : TERM_L_DARK, buf);

        row += 2;
    }

    Term_gotoxy(desc_col, row);
}

/* ------------------------------------------------------------------
 * add_random_curse()
 *   ï¿½ Marks the item cursed
 *   ï¿½ Gives it random negative modifiers
 *   Compatible with SIL-QH object_type (no flags1/2/3 fields)
 * ------------------------------------------------------------------ */
void add_random_curse(object_type *o_ptr)
{
    /* 1. make it show up as {cursed} right away */
    o_ptr->ident |= IDENT_CURSED;

    /* 2. negative pval / attack / evasion */
    int old_pval = o_ptr->pval;
    if (o_ptr->pval > 0)  o_ptr->pval = -(rand_int(3) + 1); /* ï¿½1 ï¿½ ï¿½3 */
    int pval_delta = o_ptr->pval - old_pval;
    if (pval_delta != 0)
        object_apply_pval_delta_with_mask(o_ptr, object_pval_flags1(o_ptr), pval_delta);
    if (o_ptr->att > 0) o_ptr->att = -(rand_int(3) + 1);
    if (o_ptr->evn > 0) o_ptr->evn = -(rand_int(3) + 1);

    /* 3. very small chance to damage dice on weapons / armour */
    if (one_in_(8))
    {
        if (o_ptr->dd) o_ptr->dd = MAX(1, o_ptr->dd - 1);
        if (o_ptr->pd) o_ptr->pd = MAX(1, o_ptr->pd - 1);
    }
}


static char song_menu_letter(int song_index)
{
    char letter = (char)('a' + song_index);

    if (letter >= 's')
        letter++;

    return letter;
}

static int song_index_from_menu_letter(char letter)
{
    if (letter < 'a' || letter > 'z')
        return -1;

    if (letter == 's')
        return -1;

    if (letter > 's')
        letter--;

    return (int)(letter - 'a');
}

/*
 * Display the available songs (modelled on show_inven) with optional highlighting.
 */
void show_songs_with_highlight(int highlight)
{
    int i, j, k = 0;
    int current_line = 0;

    int col = 26;

    char tmp_val[80];

    int out_index[24];
    char out_desc[24][80];

    /* Display the songs */
    for (k = 0, i = 0; i < SNG_MAX; i++)
    {
        /* Skip Woven Themes (not a singable song) */
        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
            continue;

        /* Is this song acceptable? */
        if (!p_ptr->active_ability[S_SNG][i])
            continue;

        /* Save the index */
        out_index[k] = i;

        /* Save the song name */
        SDL_strlcpy(out_desc[k],
            b_name + (&b_info[ability_index(S_SNG, i)])->name,
            sizeof(out_desc[0]));

        /* Advance to next "line" */
        k++;
    }

    // add a line for the 'stop singing' command

    /* Clear the line */
    prt("", 1, col - 2);

    /* Clear the line with the (possibly indented) index */
    put_str("s)", 1, col);

    /* Display the entry itself - highlight if selected */
    if (highlight == current_line)
        c_put_str(TERM_L_BLUE, "Stop Singing", 1, col + 3);
    else
        c_put_str(TERM_SLATE, "Stop Singing", 1, col + 3);
    current_line++;

    /* Output each entry */
    for (j = 0; j < k; j++)
    {
        /* Get the index */
        i = out_index[j];

        /* Clear the line */
        prt("", j + 2, col - 2);

        /* Prepare an index --(-- */
        sprintf(tmp_val, "%c)", song_menu_letter(i));

        /* Clear the line with the (possibly indented) index */
        put_str(tmp_val, j + 2, col);

        /* Display the entry itself - highlight if selected */
        if (highlight == current_line)
            c_put_str(TERM_L_BLUE, out_desc[j], j + 2, col + 3);
        else
            c_put_str(TERM_L_WHITE, out_desc[j], j + 2, col + 3);
        current_line++;
    }

    // add a line for the 'exchange themes' command
    if (p_ptr->song2 != SNG_NOTHING)
    {
        /* Clear the line */
        prt("", j + 2, col - 2);

        /* Clear the line with the (possibly indented) index */
        put_str("x)", j + 2, col);

        /* Display the entry itself - highlight if selected */
        if (highlight == current_line)
            c_put_str(TERM_L_BLUE, "Exchange themes", j + 2, col + 3);
        else
            c_put_str(TERM_L_BLUE, "Exchange themes", j + 2, col + 3);

        j++;
    }

    /* Make a "shadow" below the list (only if needed) */
    if (j && (j < 23))
        prt("", j + 2, col - 2);
}

/*
 * Display the available songs (modelled on show_inven).
 */
void show_songs(void)
{
    show_songs_with_highlight(-1); // No highlighting
}

void do_cmd_change_song()
{
    int i;
    bool done = false;

    int options = 0;
    int song_choice = -1;
    int highlight = 0; // Add highlight tracking

    char out_val[80];
    char tmp_val[80];

    char which;

    log_debug("Player opening song selection menu");

    // Check for song lockout timer first
    if (p_ptr->song_lockout_timer > 0)
    {
        msg_format("You cannot sing for %d more turn%s.", 
            p_ptr->song_lockout_timer,
            (p_ptr->song_lockout_timer == 1) ? "" : "s");
        return;
    }

    // count the abilities
    for (i = 0; i < SNG_MAX; i++)
    {
        /* Skip Woven Themes (not a singable song) */
        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
            continue;

        // keep track of the number of options and final song
        if (p_ptr->active_ability[S_SNG][i])
        {
            options += 1;
        }
    }

    // abort if you know no songs
    if (options == 0)
    {
        log_trace("No songs available - player knows no songs of power");
        msg_print("You do not know any songs of power.");
        return;
    }
    
    log_debug("Player has %d songs available", options);

    /* Flush the prompt */
    Term_fresh();

    /* Option to always show a list */
    if (auto_display_lists)
    {
        p_ptr->command_see = true;
    }

    /* Start out in "display" mode */
    if (p_ptr->command_see)
    {
        /* Save screen */
        screen_save();
    }

    /* Repeat until done */
    while (!done)
    {
        /* Redraw if needed */
        if (p_ptr->command_see)
            show_songs_with_highlight(highlight);

        /* Begin the prompt */
        sprintf(out_val, "Songs: s");

        // count the abilities
        for (i = 0; i < SNG_MAX; i++)
        {
            /* Skip Woven Themes (not a singable song) */
            if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                continue;

            // keep track of the number of options
            if (p_ptr->active_ability[S_SNG][i])
            {
                SDL_strlcat(out_val, ",", sizeof(out_val));
                sprintf(tmp_val, "%c", song_menu_letter(i));

                /* Append */
                SDL_strlcat(out_val, tmp_val, sizeof(out_val));
            }
        }

        // add an 'x' option if using woven themes
        if (p_ptr->song2 != SNG_NOTHING)
        {
            /* Append */
            SDL_strlcat(out_val, ",x", sizeof(out_val));
        }

        /* Indicate ability to "view" */
        if (!p_ptr->command_see)
            SDL_strlcat(out_val, ", * to see", sizeof(out_val));

        /* Build the prompt */
        strnfmt(tmp_val, sizeof(tmp_val), "(%s) Sing which song: ", out_val);

        /* Show the prompt */
        prt(tmp_val, 0, 0);

        /* Get a key */
        which = inkey();

        /* Parse it */
        switch (which)
        {
        case ESCAPE:
        {
            log_trace("Song selection cancelled by player");
            done = true;
            break;
        }

        case '\r': // Enter - select highlighted item when menu is visible, otherwise exit
        {
            if (p_ptr->command_see)
            {
                // Convert highlight to appropriate song choice (same logic as '6' and Space keys)
                if (highlight == 0)
                {
                    song_choice = SNG_NOTHING; // Stop singing
                }
                else
                {
                    // Find the i-th available song
                    int song_count = 1;
                    for (i = 0; i < SNG_MAX; i++)
                    {
                        /* Skip Woven Themes (not a singable song) */
                        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                            continue;

                        if (p_ptr->active_ability[S_SNG][i])
                        {
                            if (song_count == highlight)
                            {
                                song_choice = i;
                                break;
                            }
                            song_count++;
                        }
                    }
                    // Check for exchange themes option
                    if (song_choice == -1 && p_ptr->song2 != SNG_NOTHING && highlight == song_count)
                    {
                        song_choice = SNG_EXCHANGE_THEMES;
                    }
                }
                
                if (song_choice >= 0)
                {
                    done = true;
                }
            }
            else
            {
                log_trace("Song selection cancelled by player");
                done = true;
            }
            break;
        }

        case '*':
        case '?':
        {
            /* Hide the list */
            if (p_ptr->command_see)
            {
                /* Flip flag */
                p_ptr->command_see = false;

                /* Load screen */
                screen_load();
            }

            /* Show the list */
            else
            {
                /* Save screen */
                screen_save();

                /* Flip flag */
                p_ptr->command_see = true;
            }

            break;
        }

        case ' ': // Space - select highlighted item when menu is visible, otherwise toggle menu
        {
            if (p_ptr->command_see)
            {
                // Convert highlight to appropriate song choice (same logic as '6' key)
                if (highlight == 0)
                {
                    song_choice = SNG_NOTHING; // Stop singing
                }
                else
                {
                    // Find the i-th available song
                    int song_count = 1;
                    for (i = 0; i < SNG_MAX; i++)
                    {
                        /* Skip Woven Themes (not a singable song) */
                        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                            continue;

                        if (p_ptr->active_ability[S_SNG][i])
                        {
                            if (song_count == highlight)
                            {
                                song_choice = i;
                                break;
                            }
                            song_count++;
                        }
                    }
                    // Check for exchange themes option
                    if (song_choice == -1 && p_ptr->song2 != SNG_NOTHING && highlight == song_count)
                    {
                        song_choice = SNG_EXCHANGE_THEMES;
                    }
                }
                
                if (song_choice >= 0)
                {
                    done = true;
                }
            }
            else
            {
                /* Show the list */
                /* Save screen */
                screen_save();

                /* Flip flag */
                p_ptr->command_see = true;
            }
            break;
        }

        case '2': // Down arrow / scroll down
        {
            if (p_ptr->command_see)
            {
                // Get total available songs + stop singing + exchange themes
                int total_options = 1; // "Stop Singing"
                for (i = 0; i < SNG_MAX; i++)
                {
                    /* Skip Woven Themes (not a singable song) */
                    if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                        continue;

                    if (p_ptr->active_ability[S_SNG][i])
                        total_options++;
                }
                if (p_ptr->song2 != SNG_NOTHING)
                    total_options++; // "Exchange themes"

                highlight = (highlight + 1) % total_options;
            }
            break;
        }

        case '8': // Up arrow / scroll up
        {
            if (p_ptr->command_see)
            {
                // Get total available songs + stop singing + exchange themes
                int total_options = 1; // "Stop Singing"
                for (i = 0; i < SNG_MAX; i++)
                {
                    /* Skip Woven Themes (not a singable song) */
                    if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                        continue;

                    if (p_ptr->active_ability[S_SNG][i])
                        total_options++;
                }
                if (p_ptr->song2 != SNG_NOTHING)
                    total_options++; // "Exchange themes"

                highlight = (highlight - 1 + total_options) % total_options;
            }
            break;
        }

        case '6': // Right arrow / select highlighted
        {
            if (p_ptr->command_see)
            {
                // Convert highlight to appropriate song choice
                if (highlight == 0)
                {
                    song_choice = SNG_NOTHING; // Stop singing
                }
                else
                {
                    // Find the i-th available song
                    int song_count = 1;
                    for (i = 0; i < SNG_MAX; i++)
                    {
                        /* Skip Woven Themes (not a singable song) */
                        if (i == SNG_WOVEN_THEMES || i == SNG_GRA)
                            continue;

                        if (p_ptr->active_ability[S_SNG][i])
                        {
                            if (song_count == highlight)
                            {
                                song_choice = i;
                                break;
                            }
                            song_count++;
                        }
                    }
                    // Check for exchange themes option
                    if (song_choice == -1 && p_ptr->song2 != SNG_NOTHING && highlight == song_count)
                    {
                        song_choice = SNG_EXCHANGE_THEMES;
                    }
                }
                
                if (song_choice >= 0)
                {
                    done = true;
                }
            }
            break;
        }

        case 's':
        {
            log_debug("Player selected to stop singing");
            song_choice = SNG_NOTHING;
            done = true;
            break;
        }

        case 'x':
        {
            if (p_ptr->song2 != SNG_NOTHING)
            {
                log_debug("Player exchanging woven themes");
                song_choice = SNG_EXCHANGE_THEMES;
                done = true;
                break;
            }
            else
            {
                log_trace("Illegal song choice - no second theme to exchange");
                bell("Illegal song choice.");
                break;
            }
        }

        default:
        {
            song_choice = song_index_from_menu_letter(which);

            if (song_choice >= 0 && song_choice < SNG_MAX)
            {
                /* Skip Woven Themes (not a singable song) */
                if (song_choice == SNG_WOVEN_THEMES || song_choice == SNG_GRA)
                {
                    song_choice = -1;
                }
                else if (p_ptr->active_ability[S_SNG][song_choice])
                {
                    log_debug("Player selected song %d", song_choice);
                    done = true;
                    break;
                }
                else
                {
                    song_choice = -1;
                }
            }

            log_trace("Illegal song choice attempted");
            bell("Illegal song choice.");
            break;
        }
        }
    }

    /* Fix the screen if necessary */
    if (p_ptr->command_see)
    {
        /* Load screen */
        screen_load();

        /* Hack -- Cancel "display" */
        p_ptr->command_see = false;
    }

    /* Clear the prompt line */
    prt("", 0, 0);

    if (song_choice >= 0)
    {
        if (song_choice != SNG_NOTHING)
        {
            if (chosen_oath(OATH_SILENCE) && !oath_invalid(OATH_SILENCE))
            {
                /* Use oath-specific confirmation prompt */
                char* prompt = oath_confirmation_prompt(OATH_SILENCE);
                if (!prompt || !prompt[0]) prompt = "Are you certain you wish to break your Oath of Silence?";
                
                if (get_check_oath_multiline(prompt))
                {
                    log_info("Player broke oath of silence to sing");
                    
                    /* Curse message and selection handled by apply_oath_breaking_curse */
                    do_cmd_note("Broke your oath", p_ptr->depth);
                    
                    /* Apply oath breaking consequences */
                    apply_oath_breaking_curse(OATH_SILENCE);
                    
                    /* Only mark oath as broken if player actually has it */
                    p_ptr->oaths_broken |= OATH_SILENCE_FLAG;
                }
                else
                {
                    log_debug("Player cancelled song due to oath of silence");
                    return;
                }
            }
        }

        log_info("Player changed song to %s", song_choice == SNG_NOTHING ? "silence" : 
                 song_choice == SNG_EXCHANGE_THEMES ? "exchange themes" : "new song");
        change_song(song_choice);
    }
}

void wipe_screen_from(int col)
{
    int i;
    int wid = Term ? Term->wid : 80;
    int hgt = Term ? Term->hgt : 24;

    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;
    if (col >= wid)
        return;

    for (i = 1; i < hgt; i++)
        Term_erase(col, i, wid - col);
}

int bane_menu(int* highlight)
{
    int i, k;

    int ch;
    int options;

    char buf[80];

    byte attr;

    // bane title
    Term_putstr(COL_DESCRIPTION, 2, -1, TERM_WHITE, "Enemy types");

    // clear the description area
    wipe_screen_from(COL_DESCRIPTION);

    // list the enemies
    for (i = 1; i < PLAYER_BANE_TYPES; i++)
    {
        k = bane_type_killed(i);

        // Determine the appropriate colour
        if (k >= 4)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
        }

        strnfmt(buf, 80, "%c) %s", (char)'a' + i - 1, bane_name[i]);
        Term_putstr(COL_DESCRIPTION, i + 3, -1, attr, buf);

        if (*highlight == i)
        {
            // highlight the label
            strnfmt(buf, 80, "%c)", (char)'a' + i - 1);
            Term_putstr(COL_DESCRIPTION, i + 3, -1, TERM_L_BLUE, buf);

            /* Indent output by 2 character, and wrap at column 70 */
            text_out_wrap = 79;
            text_out_indent = COL_DESCRIPTION;

            Term_gotoxy(text_out_indent, PLAYER_BANE_TYPES + 4);

            /* Information */
            if (k >= 4)
            {
                strnfmt(buf, 80, "You have slain %d of these foes.", k);
                text_out_to_screen(TERM_SLATE, buf);
            }
            else
            {
                strnfmt(buf, 80,
                    "You have slain %d of these foes,   and need to slay %d "
                    "more.",
                    k, 4 - k);
                text_out_to_screen(TERM_L_DARK, buf);
            }

            /* Reset text_out() vars */
            text_out_wrap = 0;
            text_out_indent = 0;
        }

        // keep track of the number of options
        options = i;
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(COL_DESCRIPTION, 3 + *highlight);

    /* Get key (while allowing menu commands) */
    inkey_set_cursor_hidden(true);
    ch = inkey();
    inkey_set_cursor_hidden(false);

    if ((ch >= 'a') && (ch <= (char)'a' + options - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        bane_menu(highlight);

        return (*highlight);
    }

    if ((ch >= 'A') && (ch <= (char)'A' + options - 1))
    {
        *highlight = (int)ch - 'A' + 1;
        return (*highlight);
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4'))
    {
        return (PLAYER_BANE_TYPES + 1);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        *highlight = (*highlight + (options - 2)) % options + 1;
    }

    /* Next item */
    if (ch == '2')
    {
        *highlight = *highlight % options + 1;
    }

    return (0);
}

#define OATH_TYPES 6

char* oath_desc1[] = {
    "Nothing",
    "to leave Angband without shedding blood of Man or Elf",
    "to leave Angband as you came, grim and silent",
    "that none will daunt you from facing Morgoth forthwith",
    "to craft all blades and armour by thine own hand",
    "to face your enemy while it has the heart to fight",
    "to bear the light of the stars and refuse all shadowed gear",
};

char* oath_desc2[] = {
    "Nothing",
    "attack Men or Elves",
    "sing",
    "go up stairs without a Silmaril",
    "pick up weapons or armour from the ground",
    "attack or deal damage to enemies that are fleeing in terror",
    "wear items that dim or shroud your light",
};

char* oath_reward[] = {
    "Nothing",
    "+1 Grace",
    "+1 Strength",
    "+2 Constitution",
    "+5 Smithing",
    "+1 Dexterity",
    "+1 Light Radius",
};

static const char* oath_name_short(int oath_id)
{
    if (oath_id < 0 || oath_id > OATH_TYPES) return "Unknown";
    return oath_name[oath_id];
}

static const char* oath_desc2_short(int oath_id)
{
    if (oath_id < 0 || oath_id >= (int)N_ELEMENTS(oath_desc2)) return "";
    return oath_desc2[oath_id];
}

static const char* oath_reward_short(int oath_id)
{
    if (oath_id < 0 || oath_id >= (int)N_ELEMENTS(oath_reward)) return "";
    return oath_reward[oath_id];
}

static int oath_menu_put_wrapped(int desc_col, int row, byte attr, cptr text)
{
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;

    text_out_wrap = ability_menu_description_wrap(desc_col);
    text_out_indent = desc_col;
    Term_gotoxy(desc_col, row);
    text_out_to_screen(attr, text);

    row = ability_menu_next_row_after_text(desc_col, row);
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;

    return row;
}

int oath_menu(int* highlight)
{
    int i, ch;
    int visible_count = 0;
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    bool compact_layout = ability_menu_use_compact_layout();
    int ability_col = ability_menu_list_col();
    int desc_col = ability_menu_description_col();
    int nav_row_1 = MAX(0, term_hgt - 2);
    int nav_row_2 = MAX(0, term_hgt - 1);
    /* Support up to 16 oaths without realloc. */
    int visible_oaths[16]; // Map display letters to oath indices
    char buf[80];
    byte attr;
    
    /* Tolkien-themed descriptions for better immersion */
    char* oath_tolkien_desc[] = {
        "",
        "\"Let no blood of the Children stain thy blade in these halls of sorrow\"",
        "\"In silence came I, and in silence shall I depart, as befits the wise\"", 
        "\"Though darkness gather and Balrogs rise, I shall not yield nor turn aside\"",
        "\"By mine own hand shall all blades be wrought, and no other's craft shall I bear\"",
        "\"Valor guards the fallen foe; the honorable blade stays when terror takes them\"",
        "\"I will carry unsullied starlight, shunning the shadowed tools that would dim it\""
    };

    // Clear the abilities and description area (following abilities_menu2 pattern)
    wipe_screen_from(ability_col);

    // Title in the abilities column
    Term_putstr(ability_col, 2, -1, TERM_WHITE, "Oaths");

    // Build visible oaths list and display them (1..OATH_TYPES)
    for (i = 1; i <= OATH_TYPES; i++)
    {
        if (visible_count >= (int)N_ELEMENTS(visible_oaths)) break;

        // Map this visible oath to its position  
        visible_oaths[visible_count] = i;
        
        // Determine display color based on oath status
        if (oath_invalid(i))
        {
            attr = TERM_L_RED; // Broken oaths in red
        }
        else
        {
            attr = (*highlight == visible_count + 1) ? TERM_L_BLUE : TERM_WHITE;
        }
        
        // Format oath name with status indicator
        strnfmt(buf, 80, "%c) %s", (char)'a' + visible_count, oath_name_short(i));
        
        // Display in abilities column with proper spacing
        Term_putstr(ability_col, 4 + visible_count, -1, attr, buf);
        visible_count++;
    }

    // Display detailed description for highlighted oath in description column
    if (*highlight >= 1 && *highlight <= visible_count)
    {
        int oath_idx = visible_oaths[*highlight - 1];
        
        // Clear description area first
        int row = 4;

        wipe_screen_from(desc_col);
        
        // Oath title
        Term_putstr(desc_col, 2, -1, TERM_WHITE, "Oath Details");
        
        if (oath_invalid(oath_idx))
        {
            // Menacing text for broken oaths
            Term_putstr(desc_col, row++, term_wid - desc_col, TERM_L_RED,
                "OATH BROKEN");
            row++;
            row = oath_menu_put_wrapped(desc_col, row, TERM_RED,
                "\"Thy oath lies shattered, thy word worthless as dust.\"");
            row++;
            row = oath_menu_put_wrapped(desc_col, row, TERM_L_RED,
                "\"No Valar shall hear thy voice, no light shall guide thy path.\"");
            row++;
            (void)oath_menu_put_wrapped(desc_col, row, TERM_RED,
                "Forever marked as oathbreaker in this age.");
        }
        else
        {
            // Tolkien-themed quote
            char* quote = (oath_idx < (int)N_ELEMENTS(oath_tolkien_desc)) ? oath_tolkien_desc[oath_idx] : "";

            Term_putstr(desc_col, row++, term_wid - desc_col, TERM_YELLOW,
                "Quote:");
            row = oath_menu_put_wrapped(desc_col, row, TERM_SLATE, quote);
            
            // Oath vow
            Term_putstr(desc_col, row++, term_wid - desc_col, TERM_WHITE,
                "Vow:");
            row = oath_menu_put_wrapped(desc_col, row, TERM_SLATE,
                (oath_idx >= 0 && oath_idx < (int)N_ELEMENTS(oath_desc1))
                    ? oath_desc1[oath_idx]
                    : "");
            
            // Restriction
            if (row < nav_row_1)
            {
                Term_putstr(desc_col, row++, term_wid - desc_col, TERM_L_RED,
                    "Restriction:");
                row = oath_menu_put_wrapped(desc_col, row, TERM_L_RED,
                    oath_desc2_short(oath_idx));
            }
            
            // Reward
            if (row < nav_row_1)
            {
                Term_putstr(desc_col, row++, term_wid - desc_col, TERM_L_GREEN,
                    "Reward:");
                (void)oath_menu_put_wrapped(desc_col, row, TERM_L_GREEN,
                    oath_reward_short(oath_idx));
            }
        }
        
        // Navigation instructions at bottom
        Term_putstr(desc_col, nav_row_1, term_wid - desc_col, TERM_SLATE,
            compact_layout ? "8/2 - Navigate" : "2/8 - Navigate");
        Term_putstr(desc_col, nav_row_2, term_wid - desc_col, TERM_SLATE,
            compact_layout ? "Enter Select  Esc Back"
                           : "Enter - Select  ESC - Back");
    }

    // Ensure highlight is within valid range
    if (*highlight < 1) *highlight = 1;
    if (*highlight > visible_count) *highlight = visible_count;

    /* Flush the prompt */
    Term_fresh();

    /* Get key (while allowing menu commands) */
    inkey_set_cursor_hidden(true);
    ch = inkey();
    inkey_set_cursor_hidden(false);

    /* Handle letter selection (a-z) for immediate highlighting */
    if ((ch >= 'a') && (ch < 'a' + visible_count))
    {
        *highlight = (int)ch - 'a' + 1;
        return oath_menu(highlight); // Recursive call to update display
    }

    /* Handle capital letter selection (A-Z) for immediate selection */
    if ((ch >= 'A') && (ch < 'A' + visible_count))
    {
        *highlight = (int)ch - 'A' + 1;
        return visible_oaths[*highlight - 1]; // Return actual oath index
    }

    /* ESC or 'q' - exit menu */
    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4'))
    {
        /* Return a sentinel that's outside valid oath indices */
        return OATH_TYPES + 1;
    }

    /* Enter or Space - select current highlighted oath */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (visible_count <= 0) return OATH_TYPES + 1;
        return visible_oaths[*highlight - 1]; // Return actual oath index
    }

    /* Navigation: Up (8) */
    if (ch == '8')
    {
        (*highlight)--;
        if (*highlight < 1) *highlight = visible_count;
    }

    /* Navigation: Down (2) */
    if (ch == '2')
    {
    (*highlight)++;
        if (*highlight > visible_count) *highlight = 1;
    }

    /* Recursive call to continue menu interaction */
    return oath_menu(highlight);
}

int abilities_menu1(int* highlight)
{
    int i;
    int ch;
    int options = S_MAX;
    bool show_special = false;

    // Determine if any special abilities are present (owned or active)
    for (i = 0; i < ABILITIES_MAX; i++) {
        if (p_ptr->have_ability[S_SPC][i]) { 
            show_special = true; 
            break; 
        }
    }
    
    // Debug: Always show special menu for unique bane status
    if (p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE]) {
        show_special = true;
    }
    
    if (!show_special) {
        options = S_MAX - 1; // hide Special category
    }

    char buf[80];

    // Clear the whole screen body so compact-layout submenu rows do not
    // linger when returning from an ability list to the skills list.
    wipe_screen_from(COL_SKILL);

    // title
    Term_putstr(COL_SKILL, 2, -1, TERM_WHITE, "Skills");

    // list the skills
    for (i = 0; i < options; i++)
    {
        strnfmt(buf, 80, "%c) %s", (char)'a' + i, skill_names_full[i]);

        // Highlight the entire line if selected
        Term_putstr(COL_SKILL, i + 4, -1,
            (*highlight == i + 1) ? TERM_L_BLUE : TERM_WHITE, buf);
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(COL_SKILL, 3 + *highlight);

    /* Get key (while allowing menu commands) */
    inkey_set_cursor_hidden(true);
    ch = inkey();
    inkey_set_cursor_hidden(false);

    if ((ch >= 'a') && (ch <= (char)'a' + options - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        // relist the skills
    for (i = 0; i < options; i++)
        {
            strnfmt(buf, 80, "%c) %s", (char)'a' + i, skill_names_full[i]);

            Term_putstr(COL_SKILL, i + 4, -1,
                (*highlight == i + 1) ? TERM_L_BLUE : TERM_WHITE, buf);
        }

        return (*highlight);
    }

    if ((ch >= 'A') && (ch <= (char)'A' + options - 1))
    {
        *highlight = (int)ch - 'A' + 1;

        // relist the skills
    for (i = 0; i < options; i++)
        {
            strnfmt(buf, 80, "%c) %s", (char)'a' + i, skill_names_full[i]);

            Term_putstr(COL_SKILL, i + 4, -1,
                (*highlight == i + 1) ? TERM_L_BLUE : TERM_WHITE, buf);
        }

        return (*highlight);
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '\t'))
    {
        return (S_MAX + 1);  // Always return S_MAX + 1 to exit, regardless of options
    }

    if (ch == 'i')
    {
        return (S_MAX + 2);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        *highlight = (*highlight + (options - 2)) % options + 1;
    }

    /* Next item */
    if (ch == '2')
    {
        *highlight = *highlight % options + 1;
    }

    return (0);
}

int abilities_menu2(int skilltype, int* highlight)
{
    int i;
    bool compact_layout = ability_menu_use_compact_layout();
    int ability_col = ability_menu_list_col();
    int desc_col = ability_menu_description_col();
    int list_first_row = 3;
    int list_rows = (Term && Term->hgt > list_first_row) ? (Term->hgt - list_first_row) : 1;

    ability_type* b_ptr;
    ability_type* visible_entries[ABILITIES_MAX];
    byte visible_attrs[ABILITIES_MAX];

    int ch;
    int visible_count = 0; // Count of actually visible abilities
    int visible_abilities[ABILITIES_MAX]; // Map display letters to ability numbers
    int top_visible = 0;
    int highlight_display_index = -1;

    char buf[80];

    byte attr;

    // In compact layout the abilities list reuses the skills column.
    wipe_screen_from(compact_layout ? COL_SKILL : COL_ABILITY);

    // abilities title with color
    Term_putstr(ability_col, 1, -1, TERM_L_BLUE, "Abilities");

    // For special abilities, we may need to adjust highlight to first visible ability
    int first_visible_ability = -1;

    /* Pre-scan for Special abilities to adjust highlight before display */
    if (skilltype == S_SPC)
    {
        int temp_visible_count = 0;
        int temp_first_visible = -1;
        
        for (i = 0; i < z_info->b_max; i++)
        {
            b_ptr = &b_info[i];
            if (!b_ptr->name || b_ptr->skilltype != skilltype) continue;
            
            if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
            {
                if (temp_first_visible == -1)
                {
                    temp_first_visible = b_ptr->abilitynum;
                }
                temp_visible_count++;
            }
        }
        
        /* Adjust highlight before display if needed */
        if (temp_visible_count > 0 && temp_first_visible != -1)
        {
            /* Check if current highlight corresponds to a visible ability */
            int current_ability_num = *highlight - 1; /* Convert 1-based to 0-based */
            bool highlight_is_visible = false;
            
            for (i = 0; i < z_info->b_max; i++)
            {
                b_ptr = &b_info[i];
                if (!b_ptr->name || b_ptr->skilltype != skilltype) continue;
                
                if (b_ptr->abilitynum == current_ability_num && p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                {
                    highlight_is_visible = true;
                    break;
                }
            }
            
            if (!highlight_is_visible)
            {
                *highlight = temp_first_visible + 1; /* Convert back to 1-based */
            }
        }
    }

    // list the abilities
    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skilltype)
            continue;

        /* For special abilities, only show granted abilities */
        if (skilltype == S_SPC && !p_ptr->have_ability[skilltype][b_ptr->abilitynum])
        {
            continue;
        }

        /* Hide deprecated WIL_OATH ability from menu (now handled at birth) */
        if (skilltype == S_WIL && b_ptr->abilitynum == WIL_OATH)
            continue;

        // Safety check for ability number bounds
        if (b_ptr->abilitynum >= ABILITIES_MAX) {
            continue;
        }

        // Safety check for array bounds
        if (visible_count >= ABILITIES_MAX) {
            break;
        }

        /* Determine the appropriate colour. */
        if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
        {
            if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
            {
                if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
                    attr = TERM_WHITE;
                else
                    attr = TERM_RED;
            }
            else
            {
                if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
                    attr = TERM_L_GREEN;
                else
                    attr = TERM_RED;
            }
        }
        else if (prereqs(skilltype, b_ptr->abilitynum))
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
        }

        visible_entries[visible_count] = b_ptr;
        visible_attrs[visible_count] = attr;

        // Map this visible ability to its position
        visible_abilities[visible_count] = b_ptr->abilitynum;
        
        // Track first visible ability for highlight adjustment
        if (first_visible_ability == -1) {
            first_visible_ability = b_ptr->abilitynum;
        }

        visible_count++;
    }

    /* Safety check: if no abilities are visible, show message and exit */
    if (visible_count == 0) {
        Term_putstr(ability_col, 4, -1, TERM_L_DARK, "No abilities available for this skill.");
        Term_fresh();
        inkey(); /* Wait for keypress */
        return (ABILITIES_MAX + 1); /* Return to skills menu */
    }

    for (i = 0; i < visible_count; i++)
    {
        if (visible_abilities[i] == *highlight - 1)
        {
            highlight_display_index = i;
            break;
        }
    }

    if (highlight_display_index < 0)
        highlight_display_index = 0;

    if (list_rows < 1)
        list_rows = 1;

    if (highlight_display_index < top_visible)
        top_visible = highlight_display_index;
    if (highlight_display_index >= top_visible + list_rows)
        top_visible = highlight_display_index - list_rows + 1;
    if (top_visible < 0)
        top_visible = 0;
    if (top_visible > visible_count - list_rows)
        top_visible = visible_count - list_rows;
    if (top_visible < 0)
        top_visible = 0;

    if (visible_count > list_rows)
    {
        strnfmt(buf, sizeof(buf), "[%d-%d/%d]", top_visible + 1,
            MIN(top_visible + list_rows, visible_count), visible_count);
        Term_putstr(ability_col, 2, -1, TERM_SLATE, buf);
    }

    for (i = top_visible; i < visible_count && i < top_visible + list_rows; i++)
    {
        int display_row = list_first_row + (i - top_visible);

        b_ptr = visible_entries[i];
        attr = visible_attrs[i];

        if ((skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
            && (p_ptr->bane_type > 0))
        {
            strnfmt(buf, 80, "%c) %s-%s", (char)'a' + i,
                bane_name[p_ptr->bane_type], (b_name + b_ptr->name));
        }
        else if ((skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
            && (p_ptr->oath_type > 0))
        {
            strnfmt(buf, 80, "%c) %s: %s", (char)'a' + i,
                (b_name + b_ptr->name), oath_name_short(p_ptr->oath_type));
        }
        else
        {
            strnfmt(buf, 80, "%c) %s", (char)'a' + i, (b_name + b_ptr->name));
        }

        Term_putstr(ability_col, display_row, -1, attr, buf);

        if (*highlight == b_ptr->abilitynum + 1)
        {
            /* Highlight the label with bright blue */
            strnfmt(buf, 80, "%c)", (char)'a' + i);
            Term_putstr(ability_col, display_row, -1, TERM_L_BLUE, buf);

            /* Print the description of the highlighted ability. */
            /* (ability_type::text is an offset, so it's always non-negative) */
            /* Determine compact mode from terminal height; use single newline between
             * sections when space is tight, double newline when there is room. */
            int term_hgt_ab = Term ? Term->hgt : 24;
            bool compact_mode = (term_hgt_ab < 28);
            const char *desc_sep = compact_mode ? "\n" : "\n\n";
            int post_desc_row = 3; /* updated after description renders */
            {
                /* Check if this is a broken oath ability and use Q: text instead */
                char* description_text = NULL;
                bool use_death_message = false;
                
                if (skilltype == S_SPC && 
                    (b_ptr->abilitynum == SPC_OATH_MERCY || 
                     b_ptr->abilitynum == SPC_OATH_SILENCE || 
                     b_ptr->abilitynum == SPC_OATH_IRON ||
                     b_ptr->abilitynum == SPC_OATH_SMITH ||
                     b_ptr->abilitynum == SPC_OATH_VALOROUS ||
                     b_ptr->abilitynum == SPC_OATH_LIGHT))
                {
                    /* Check if this oath is broken */
                    int oath_id = 0;
                    if (b_ptr->abilitynum == SPC_OATH_MERCY) oath_id = OATH_MERCY;
                    else if (b_ptr->abilitynum == SPC_OATH_SILENCE) oath_id = OATH_SILENCE;
                    else if (b_ptr->abilitynum == SPC_OATH_IRON) oath_id = OATH_IRON;
                    else if (b_ptr->abilitynum == SPC_OATH_SMITH) oath_id = OATH_SMITH;
                    else if (b_ptr->abilitynum == SPC_OATH_VALOROUS) oath_id = OATH_VALOROUS;
                    else if (b_ptr->abilitynum == SPC_OATH_LIGHT) oath_id = OATH_LIGHT;
                    
                    if (oath_id > 0 && oath_invalid(oath_id))
                    {
                        description_text = oath_death_message(oath_id);
                        use_death_message = true;
                    }
                }
                
                /* Clear description area first */
                wipe_screen_from(desc_col);
                
                /* Display ability name in description area with appropriate color */
                Term_putstr(desc_col, 1, -1, TERM_YELLOW, b_name + b_ptr->name);
                
                /* Wrap to the active terminal width so compact layouts do not overflow. */
                text_out_wrap = ability_menu_description_wrap(desc_col);
                text_out_indent = desc_col;

                /* Description starts at row 3 for more space */
                Term_gotoxy(text_out_indent, 3);
                
                if (use_death_message && description_text && description_text[0])
                {
                    /* Display Q: text in red for broken oaths */
                    text_out_to_screen(TERM_RED, description_text);
                }
                else
                {
                    /* Display ability description based on ability_desc_mode */
                    const char *desc_text = (b_ptr->text) ? b_text + b_ptr->text : NULL;
                    const char *effect_text = (b_ptr->effect) ? b_text + b_ptr->effect : NULL;
                    bool has_desc = desc_text && desc_text[0];
                    bool has_effect = effect_text && effect_text[0];

                    switch (op_ptr->ability_desc_mode)
                    {
                    case 1: /* Effect first, then description */
                        if (has_effect) text_out_to_screen(TERM_L_WHITE, effect_text);
                        if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                        {
                            if (has_effect) text_out_to_screen(TERM_L_WHITE, desc_sep);
                            ability_menu_render_prerequisites_block(skilltype,
                                b_ptr, desc_col);
                        }
                        if (has_desc) {
                            if (has_effect
                                || !p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                                text_out_to_screen(TERM_L_WHITE, desc_sep);
                            text_out_to_screen(TERM_SLATE, desc_text);
                        }
                        break;
                    case 2: /* Effect only */
                        if (has_effect) text_out_to_screen(TERM_L_WHITE, effect_text);
                        else if (has_desc) text_out_to_screen(TERM_L_WHITE, desc_text);
                        if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                        {
                            if (has_effect || has_desc)
                                text_out_to_screen(TERM_L_WHITE, desc_sep);
                            ability_menu_render_prerequisites_block(skilltype,
                                b_ptr, desc_col);
                        }
                        break;
                    default: /* 0: Description first, then effect */
                        if (has_desc) text_out_to_screen(TERM_SLATE, desc_text);
                        if (has_effect) {
                            if (has_desc) text_out_to_screen(TERM_L_WHITE, desc_sep);
                            text_out_to_screen(TERM_L_WHITE, effect_text);
                        }
                        if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                        {
                            if (has_desc || has_effect)
                                text_out_to_screen(TERM_L_WHITE, desc_sep);
                            ability_menu_render_prerequisites_block(skilltype,
                                b_ptr, desc_col);
                        }
                        break;
                    }

                    /* For Nienna's Gift of Mercy, show current bonus */
                    if (skilltype == S_SPC && b_ptr->abilitynum == SPC_NIENA_MERCY && 
                        p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY])
                    {
                        /* Calculate current stealth bonus (same logic as in xtra1.c) */
                        int total_monsters_seen = 0;
                        int total_monsters_killed = 0;
                        
                        /* Sum up global monster tracking (excluding uniques) */
                        for (int i = 1; i < z_info->r_max; i++)
                        {
                            monster_lore *l_ptr = &l_list[i];
                            monster_race *r_ptr = &r_info[i];
                            
                            if (r_ptr->flags1 & RF1_UNIQUE) continue;
                            
                            total_monsters_seen += l_ptr->psights;
                            total_monsters_killed += l_ptr->pkills;
                        }
                        
                        if (total_monsters_seen > 0)
                        {
                            /* Calculate stealth bonus: 10*(seen-killed)/seen, rounded up */
                            int mercy_ratio_times_10 = (10 * (total_monsters_seen - total_monsters_killed));
                            int stealth_bonus = (mercy_ratio_times_10 + total_monsters_seen - 1) / total_monsters_seen;
                            
                            char bonus_text[100];
                            strnfmt(bonus_text, sizeof(bonus_text), 
                                   "\n\nCurrent bonus: +%d stealth (%d seen, %d spared)",
                                   stealth_bonus, total_monsters_seen, 
                                   total_monsters_seen - total_monsters_killed);
                            text_out_to_screen(TERM_L_GREEN, bonus_text);
                        }
                        else
                        {
                            text_out_to_screen(TERM_SLATE, "\n\nCurrent bonus: +0 stealth (no monsters encountered yet)");
                        }
                    }
                }

                /* Capture the row where description text ended for dynamic placement */
                {
                    int pdx;
                    Term_locate(&pdx, &post_desc_row);
                    if (pdx > text_out_indent) post_desc_row++;
                }

                /* Reset text_out() vars */
                text_out_wrap = 0;
                text_out_indent = 0;
            }

            // if you have the ability and it is Bane...
            if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
                && (skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
                && (p_ptr->bane_type > 0))
            {
                int killed = bane_type_killed(p_ptr->bane_type);
                int current_bonus = bane_bonus_for_type(p_ptr->bane_type);
                int next_threshold = 2;
                
                // Calculate next threshold using same formula as bane
                int threshold = 2;
                while (threshold <= killed)
                {
                    threshold *= 2;
                }
                next_threshold = threshold;  // This is the next power of 2
                
                /* Place bane stats dynamically after description text */
                int bane_row = post_desc_row + (compact_mode ? 1 : 2);
                Term_putstr(desc_col, bane_row, -1, TERM_WHITE,
                    format("%s-Bane:", bane_name[p_ptr->bane_type]));
                Term_putstr(desc_col, bane_row + 2, -1, TERM_WHITE,
                    format("  %d slain, giving a %+d bonus", killed, current_bonus));
                    
                if (current_bonus == 0 && killed < 2) {
                    Term_putstr(desc_col, bane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d slain)", next_threshold));
                } else if (next_threshold <= 64) {  // Don't show if threshold is too high
                    Term_putstr(desc_col, bane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d slain)", next_threshold));
                }
            }
            else if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
                && (skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
                && (p_ptr->oath_type > 0))
            {
                /* Place oath info dynamically after description text */
                int oath_row = post_desc_row + (compact_mode ? 1 : 2);
                Term_putstr(desc_col, oath_row, -1, TERM_WHITE, "Oath:");
                Term_putstr(desc_col + 6, oath_row, -1, TERM_L_BLUE,
                    oath_name_short(p_ptr->oath_type));

                /* Wrap to the active terminal width here too. */
                text_out_wrap = ability_menu_description_wrap(desc_col);
                text_out_indent = desc_col;

                /* History */
                Term_gotoxy(text_out_indent, oath_row + 1);
                strnfmt(buf, 80, "You have sworn not to %s.",
                    oath_desc2_short(p_ptr->oath_type));
                text_out_to_screen(TERM_L_WHITE, buf);

                /* Reset text_out() vars */
                text_out_wrap = 0;
                text_out_indent = 0;

                if (oath_invalid(p_ptr->oath_type))
                    Term_putstr(desc_col, oath_row + 4, -1, TERM_RED,
                        "You are an oathbreaker.");
                else
                    Term_putstr(desc_col, oath_row + 4, -1, TERM_WHITE,
                        format("Bonus: %s.", oath_reward_short(p_ptr->oath_type)));
            }
            // if you have the unique bane special ability
            else if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
                && (skilltype == S_SPC) && (b_ptr->abilitynum == SPC_UNIQUE_BANE))
            {
                int uniques_killed = unique_bane_type_killed();
                int current_bonus = 0;
                int next_threshold = 2;
                
                // Calculate current bonus using same formula as bane
                int threshold = 2;
                while (threshold <= uniques_killed)
                {
                    threshold *= 2;
                    current_bonus++;
                }
                
                // Calculate next threshold
                if (current_bonus == 0) {
                    next_threshold = 2;
                } else {
                    next_threshold = threshold;  // This is the next power of 2
                }
                
                /* Place unique bane stats dynamically after description text */
                int ubane_row = post_desc_row + (compact_mode ? 1 : 2);
                Term_putstr(desc_col, ubane_row, -1, TERM_WHITE, "Unique Bane:");
                Term_putstr(desc_col, ubane_row + 2, -1, TERM_WHITE,
                    format("  %d uniques slain, giving a %+d bonus", 
                           uniques_killed, current_bonus));
                           
                if (current_bonus == 0 && uniques_killed < 2) {
                    Term_putstr(desc_col, ubane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d uniques)", next_threshold));
                } else if (next_threshold <= 64) {  // Don't show if threshold is too high
                    Term_putstr(desc_col, ubane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d uniques)", next_threshold));
                }
            }
        }

    }
    
    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice - single column layout */
    if (highlight_display_index >= 0)
    {
        int cursor_row = list_first_row + (highlight_display_index - top_visible);
        if (cursor_row >= list_first_row && cursor_row < list_first_row + list_rows)
            Term_gotoxy(ability_col, cursor_row);
    }

    /* Get key (while allowing menu commands) */
    inkey_set_cursor_hidden(true);
    ch = inkey();
    inkey_set_cursor_hidden(false);

    if ((ch >= 'a') && (ch <= (char)'a' + visible_count - 1))
    {
        int selected_index = (int)ch - 'a';
        /* Bounds check for safety */
        if (selected_index >= 0 && selected_index < visible_count) {
            *highlight = visible_abilities[selected_index] + 1;
            return abilities_menu2(skilltype, highlight);
        }
    }

    if ((ch >= 'A') && (ch <= (char)'A' + visible_count - 1))
    {
        int selected_index = (int)ch - 'A';
        /* Bounds check for safety */
        if (selected_index >= 0 && selected_index < visible_count) {
            *highlight = visible_abilities[selected_index] + 1;
            return abilities_menu2(skilltype, highlight);
        }
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4'))
    {
        return (ABILITIES_MAX + 1);
    }

    if (ch == '\t')
    {
        return (ABILITIES_MAX + 2);
    }

    if (ch == 'i')
    {
        return (ABILITIES_MAX + 3);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        /* Only navigate if there are visible abilities */
        if (visible_count > 0) {
            /* Find current visible index */
            int current_visible_index = -1;
            for (int i = 0; i < visible_count; i++) {
                if (visible_abilities[i] + 1 == *highlight) {
                    current_visible_index = i;
                    break;
                }
            }
            
            /* Move to previous visible ability */
            if (current_visible_index > 0) {
                *highlight = visible_abilities[current_visible_index - 1] + 1;
            } else if (current_visible_index == 0) {
                *highlight = visible_abilities[visible_count - 1] + 1;
            } else {
                /* Fallback if not found - go to first visible */
                *highlight = visible_abilities[0] + 1;
            }
        }
    }

    /* Next item */
    if (ch == '2')
    {
        /* Only navigate if there are visible abilities */
        if (visible_count > 0) {
            /* Find current visible index */
            int current_visible_index = -1;
            for (int i = 0; i < visible_count; i++) {
                if (visible_abilities[i] + 1 == *highlight) {
                    current_visible_index = i;
                    break;
                }
            }
            
            /* Move to next visible ability */
            if (current_visible_index >= 0 && current_visible_index < visible_count - 1) {
                *highlight = visible_abilities[current_visible_index + 1] + 1;
            } else if (current_visible_index == visible_count - 1) {
                *highlight = visible_abilities[0] + 1;
            } else {
                /* Fallback if not found - go to first visible */
                *highlight = visible_abilities[0] + 1;
            }
        }
    }

    return (0);
}

/*
 * Hack -- ability screen
 */
void do_cmd_ability_screen(void)
{
    int skilltype = -1;
    int abilitynum = -1;
    int banechoice = -1;
    int oathchoice = -1;

    int highlight1 = 1;
    int highlight2 = 1;
    int highlight3 = 1;

    bool return_to_game = false;
    bool return_to_skills = false;
    bool return_to_abilities = false;

    bool skip_purchase = false;

    log_trace("ABILITY_SCREEN: Entering ability screen");

    /* Save screen */
    screen_save();

    /* Clear screen */
    Term_clear();

    log_trace("ABILITY_SCREEN: Starting main ability loop");

    /* Process Events until "Return to Game" is selected */
    while (!return_to_game)
    {
        int menu1_choice;

        log_trace("ABILITY_SCREEN: Calling abilities_menu1 with highlight1=%d", highlight1);
        menu1_choice = abilities_menu1(&highlight1);

        if (menu1_choice == (S_MAX + 2))
        {
            log_trace("ABILITY_SCREEN: in-menu skill increase requested (menu1)");
            (void)gain_skills();
            p_ptr->redraw |= (PR_EXP | PR_BASIC);
            p_ptr->update |= (PU_BONUS | PU_MANA);
            handle_stuff();
            continue;
        }

        skilltype = menu1_choice - 1;

        log_trace("ABILITY_SCREEN: abilities_menu1 returned skilltype=%d", skilltype);

        // if a skill has been selected...
        if ((skilltype >= 0) && (skilltype < S_MAX))
        {
            log_trace("ABILITY_SCREEN: Valid skill selected (%d), entering abilities loop", skilltype);
            
            /* Reset highlight2 to 1 when entering a new skill category */
            highlight2 = 1;
            
            while (!return_to_skills)
            {
                int menu2_choice;

                log_trace("ABILITY_SCREEN: Calling abilities_menu2 for skilltype=%d with highlight2=%d", skilltype, highlight2);
                menu2_choice = abilities_menu2(skilltype, &highlight2);

                if (menu2_choice == (ABILITIES_MAX + 3))
                {
                    log_trace("ABILITY_SCREEN: in-menu skill increase requested (menu2)");
                    (void)gain_skills();
                    p_ptr->redraw |= (PR_EXP | PR_BASIC);
                    p_ptr->update |= (PU_BONUS | PU_MANA);
                    handle_stuff();
                    continue;
                }

                abilitynum = menu2_choice - 1;

                log_trace("ABILITY_SCREEN: abilities_menu2 returned abilitynum=%d", abilitynum);

                if ((abilitynum >= 0) && (abilitynum < ABILITIES_MAX))
                {
                    if (!p_ptr->have_ability[skilltype][abilitynum])
                    {
                        // Special abilities cannot be purchased
                        if (skilltype == S_SPC) {
                            bell("This special ability cannot be purchased.");
                            continue;
                        }
                        ability_type* b_ptr = &b_info[ability_index(skilltype, abilitynum)];
                        bool has_skill_prereq = (p_ptr->skill_base[skilltype] >= b_ptr->level);
                        bool has_ability_prereq
                            = ability_prereqs_met(skilltype, abilitynum);

                        if (has_skill_prereq && has_ability_prereq)
                        {
                            // Normalize flag check to 0 or 1
                            int is_free = (c_info[p_ptr->pcharacter].flags & RHF_FREE) ? 1 : 0;
                            int unit_cost = 500 - 200 * is_free;

                            // Calculate base cost
                            int exp_cost = (abilities_in_skill(skilltype) + 1) * unit_cost;

                            // Subtract free abilities granted by affinity
                            exp_cost -= unit_cost * affinity_level(skilltype);

                            // For song abilities, also subtract minstrel bonus (uncapped)
                            if (skilltype == S_SNG)
                                exp_cost -= unit_cost * minstrel_level();

                            // Clamp to zero
                            if (exp_cost < 0)
                                exp_cost = 0;

                            if (exp_cost > p_ptr->new_exp)
                            {
                                bell("You do not have enough experience to "
                                     "acquire this "
                                     "ability.");
                            }
                            else
                            {
                                // special menu for bane
                                if ((skilltype == S_PER)
                                    && (abilitynum == PER_BANE))
                                {
                                    while (!return_to_abilities)
                                    {
                                        skip_purchase = false;

                                        banechoice = bane_menu(&highlight3);

                                        if ((banechoice >= 1)
                                            && (banechoice <= PLAYER_BANE_TYPES))
                                        {
                                            if (bane_type_killed(banechoice)
                                                < 4)
                                            {
                                                return_to_abilities = false;
                                                skip_purchase = true;
                                                bell("Insufficient kills to "
                                                     "become a bane.");
                                            }
                                            else
                                            {
                                                return_to_abilities = true;
                                            }
                                        }
                                        else if (banechoice
                                            == PLAYER_BANE_TYPES + 1)
                                        {
                                            return_to_abilities = true;
                                            return_to_skills = true;
                                            return_to_game = true;
                                            skip_purchase = true;
                                        }
                                    }

                                    return_to_abilities = false;
                                }
                                // special menu for Oath //XXX Oaths
                                if ((skilltype == S_WIL)
                                    && (abilitynum == WIL_OATH))
                                {
                                    while (!return_to_abilities)
                                    {
                                        skip_purchase = false;

                                        oathchoice = oath_menu(&highlight3);

                                        if ((oathchoice >= 1)
                                            && (oathchoice <= OATH_TYPES))
                                        {
                                            if (oath_invalid(oathchoice))
                                            {
                                                return_to_abilities = false;
                                                skip_purchase = true;
                                                bell("This oath was broken "
                                                     "before it was made.");
                                            }
                                            else
                                            {
                                                return_to_abilities = true;
                                            }
                                        }
                                        else if (oathchoice == OATH_TYPES + 1)
                                        {
                                            return_to_abilities = true;
                                            return_to_skills = true;
                                            return_to_game = true;
                                            skip_purchase = true;
                                        }
                                    }

                                    return_to_abilities = false;
                                }

                                // Block purchasing Masterpiece if Aule's Forge already owned
                                if (skilltype == S_SMT && abilitynum == SMT_MASTERPIECE && p_ptr->have_ability[S_SPC][SPC_AULE]) {
                                    bell("Aule's Forge supersedes Masterpiece; you cannot purchase it.");
                                    skip_purchase = true;
                                }

                                if (!skip_purchase)
                                {
                                    if (get_check("Are you sure you wish to "
                                                  "gain this ability? "))
                                    {
                                        p_ptr->innate_ability[skilltype]
                                                             [abilitynum]
                                            = true;
                                        p_ptr->have_ability[skilltype]
                                                           [abilitynum]
                                            = true;
                                        p_ptr->active_ability[skilltype]
                                                             [abilitynum]
                                            = true;
                                        ability_log_record_gain(skilltype, abilitynum);
                                        Term_putstr(0, 0, -1, TERM_WHITE,
                                            "Ability gained.");
                                        p_ptr->new_exp -= exp_cost;

                                        if (banechoice <= 0 && oathchoice <= 0)
                                        {
                                            // make a note in the notes file
                                            do_cmd_note(
                                                format("(%s)",
                                                    b_name
                                                        + (&b_info[ability_index(
                                                               skilltype,
                                                               abilitynum)])
                                                              ->name),
                                                p_ptr->depth);
                                        }
                                        else if (oathchoice <= 0)
                                        {
                                            // set the new bane type
                                            p_ptr->bane_type = banechoice;

                                            // and make a note in the notes file
                                            do_cmd_note(
                                                format("(%s-%s)",
                                                    bane_name[banechoice],
                                                    b_name
                                                        + (&b_info[ability_index(
                                                               skilltype,
                                                               abilitynum)])
                                                              ->name),
                                                p_ptr->depth);
                                        }
                                        else
                                        {
                                            // set the new bane type
                                            p_ptr->oath_type = oathchoice;
                                            
                                            /* Activate the matching oath ability */
                                            int oath_special = -1;
                                            switch (oathchoice) {
                                                case OATH_MERCY: oath_special = SPC_OATH_MERCY; break;
                                                case OATH_SILENCE: oath_special = SPC_OATH_SILENCE; break;
                                                case OATH_IRON: oath_special = SPC_OATH_IRON; break;
                                                case OATH_SMITH: oath_special = SPC_OATH_SMITH; break;
                                                case OATH_VALOROUS: oath_special = SPC_OATH_VALOROUS; break;
                                                case OATH_LIGHT: oath_special = SPC_OATH_LIGHT; break;
                                            }
                                            if (oath_special >= 0) {
                                                p_ptr->have_ability[S_SPC][oath_special] = true;
                                                p_ptr->innate_ability[S_SPC][oath_special] = true;
                                                p_ptr->active_ability[S_SPC][oath_special] = true;
                                                ability_log_record_gain(S_SPC, oath_special);
                                            }

                                            // and make a note in the notes file
                                            do_cmd_note(
                                                format("(%s: %s)",
                                                    b_name
                                                        + (&b_info[ability_index(
                                                               skilltype,
                                                               abilitynum)])
                                                              ->name,
                                                    oath_name_short(oathchoice)),
                                                p_ptr->depth);
                                        }

                                        /* Set the redraw flag for everything */
                                        p_ptr->redraw |= (PR_EXP | PR_BASIC);

                                        /* Recalculate bonuses */
                                        p_ptr->update |= (PU_BONUS);
                                        p_ptr->update |= (PU_MANA);
                                    }
                                }
                                skip_purchase = false;
                                banechoice = -1;
                                oathchoice = -1;
                            }
                        }
                        else
                        {
                            if (!has_skill_prereq)
                                bell("Insufficient skill points for ability!");
                            else
                                bell("Insufficient prerequisite abilities for ability!");
                        }
                    }

                    // if you already have the ability...
                    else
                    {
                        // Prevent oath special abilities from being deactivated or reactivated when broken
                        if (skilltype == S_SPC && (abilitynum == SPC_OATH_MERCY || 
                                                   abilitynum == SPC_OATH_SILENCE || 
                                                   abilitynum == SPC_OATH_IRON ||
                                                   abilitynum == SPC_OATH_SMITH ||
                                                   abilitynum == SPC_OATH_VALOROUS ||
                                                   abilitynum == SPC_OATH_LIGHT))
                        {
                            /* Check if oath is broken */
                            bool oath_broken = false;
                            if (abilitynum == SPC_OATH_MERCY && oath_invalid(OATH_MERCY)) oath_broken = true;
                            if (abilitynum == SPC_OATH_SILENCE && oath_invalid(OATH_SILENCE)) oath_broken = true;
                            if (abilitynum == SPC_OATH_IRON && oath_invalid(OATH_IRON)) oath_broken = true;
                            if (abilitynum == SPC_OATH_SMITH && oath_invalid(OATH_SMITH)) oath_broken = true;
                            if (abilitynum == SPC_OATH_VALOROUS && oath_invalid(OATH_VALOROUS)) oath_broken = true;
                            if (abilitynum == SPC_OATH_LIGHT && oath_invalid(OATH_LIGHT)) oath_broken = true;
                            
                            if (p_ptr->active_ability[skilltype][abilitynum])
                            {
                                Term_putstr(0, 0, -1, TERM_WHITE,
                                    "Sacred oaths cannot be deactivated once sworn.");
                            }
                            else if (oath_broken)
                            {
                                Term_putstr(0, 0, -1, TERM_RED,
                                    "Broken oaths cannot be reactivated. They are lost forever.");
                            }
                            else
                            {
                                p_ptr->active_ability[skilltype][abilitynum] = true;
                                Term_putstr(0, 0, -1, TERM_WHITE,
                                    "Oath ability reactivated.");
                            }
                        }
                        else
                        {
                            // toggle its activity for non-oath abilities
                            if (p_ptr->active_ability[skilltype][abilitynum])
                            {
                                p_ptr->active_ability[skilltype][abilitynum]
                                    = false;
                                Term_putstr(0, 0, -1, TERM_WHITE,
                                    "Ability now switched off.");

                                // need to cancel second song in some cases
                                if ((skilltype == S_SNG)
                                    && (abilitynum == SNG_WOVEN_THEMES))
                                {
                                    p_ptr->song2 = SNG_NOTHING;
                                }
                            }
                            else
                            {
                                p_ptr->active_ability[skilltype][abilitynum] = true;
                                Term_putstr(0, 0, -1, TERM_WHITE,
                                    "Ability now switched on. ");
                            }
                        }

                        /* Set the redraw flag for everything */
                        p_ptr->redraw |= (PR_EXP | PR_BASIC);

                        /* Recalculate bonuses */
                        p_ptr->update |= (PU_BONUS);
                        p_ptr->update |= (PU_MANA);
                    }
                }
                else if (abilitynum == ABILITIES_MAX)
                {
                    return_to_skills = true;
                }
                else if (abilitynum == ABILITIES_MAX + 1)
                {
                    return_to_skills = true;
                    return_to_game = true;
                }
            }

            // reset some things for the next time around
            highlight2 = 1;
            return_to_skills = false;
        }
        else if (skilltype >= S_MAX)
        {
            return_to_game = true;
        }
    }

    /* Flush messages */
    // message_flush();

    /* Load screen */
    screen_load();
}

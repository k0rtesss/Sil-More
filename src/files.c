/* File: files.c */

/*
 * Transitional facade after the WP71 score/runtime split.
 *
 * Remaining ownership here:
 *   - privilege helpers still used broadly across save/load/score code
 *   - escape/suicide commands
 *   - character dump and miniature screenshot helpers
 */

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "platform-ui.h"
#include "metarun.h"
#include "player/killer.h"
#include "reliability-checks.h"
#include "score/score_entry.h"
#include "score/score_logic.h"
#include "scorefile.h"
#include "ui/ui-character-screen.h"
#include "z-term.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Mini screenshot buffers (local to this module) */
static char mini_screenshot_char[7][7];
static byte mini_screenshot_attr[7][7];

void safe_setuid_drop(void)
{
#ifdef SET_UID

#ifdef SAFE_SETUID

#ifdef HAVE_SETEGID

    if (setegid(getgid()) != 0)
    {
        quit("setegid(): cannot set permissions correctly!");
    }

#else /* HAVE_SETEGID */

#ifdef SAFE_SETUID_POSIX

    if (setgid(getgid()) != 0)
    {
        quit("setgid(): cannot set permissions correctly!");
    }

#else /* SAFE_SETUID_POSIX */

    if (setregid(getegid(), getgid()) != 0)
    {
        quit("setregid(): cannot set permissions correctly!");
    }

#endif /* SAFE_SETUID_POSIX */

#endif /* HAVE_SETEGID */

#endif /* SAFE_SETUID */

#endif /* SET_UID */
}

void safe_setuid_grab(void)
{
#ifdef SET_UID

#ifdef SAFE_SETUID

#ifdef HAVE_SETEGID

    if (setegid(player_egid) != 0)
    {
        quit("setegid(): cannot set permissions correctly!");
    }

#else /* HAVE_SETEGID */

#ifdef SAFE_SETUID_POSIX

    if (setgid(player_egid) != 0)
    {
        quit("setgid(): cannot set permissions correctly!");
    }

#else /* SAFE_SETUID_POSIX */

    if (setregid(getegid(), getgid()) != 0)
    {
        quit("setregid(): cannot set permissions correctly!");
    }

#endif /* SAFE_SETUID_POSIX */

#endif /* HAVE_SETEGID */

#endif /* SAFE_SETUID */

#endif /* SET_UID */
}

void do_cmd_escape(int silmarils)
{
    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[120];

    p_ptr->escaped = true;
    flush();
    p_ptr->is_dead = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;

    (void)strftime(long_day, 40, "%d %B %Y", localtime(&ct));

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));
    sprintf(buf, "You escaped the Iron Hells on %s.", long_day);
    do_cmd_note(buf, p_ptr->depth);

    switch (silmarils)
    {
    case 0:
        do_cmd_note("You returned empty handed.", p_ptr->depth);
        break;
    case 1:
        do_cmd_note("You brought back a Silmaril from Morgoth's crown!",
            p_ptr->depth);
        break;
    case 2:
        do_cmd_note("You brought back two Silmarils from Morgoth's crown!",
            p_ptr->depth);
        break;
    case 3:
        do_cmd_note(
            "You brought back all three Silmarils from Morgoth's crown!",
            p_ptr->depth);
        break;
    default:
        do_cmd_note("You brought back so many Silmarils that people should be suspicious!",
            p_ptr->depth);
        break;
    }

    if (p_ptr->oath_type > 0)
    {
        if (oath_invalid(p_ptr->oath_type))
        {
            char* death_msg = oath_death_message(p_ptr->oath_type);
            if (death_msg && death_msg[0]) {
                do_cmd_note(death_msg, p_ptr->depth);
            } else {
                do_cmd_note(
                    "You passed from the world, but the stain of a faithless heart remains. You will be remembered not for your deeds, but as a shameful Oathbreaker.",
                    p_ptr->depth);
            }
        }
        else
        {
            do_cmd_note("You kept your oath to the very end.", p_ptr->depth);
        }
    }

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));
    SDL_strlcpy(p_ptr->died_from, "ripe old age", sizeof(p_ptr->died_from));

    log_info("Player escaped with %d Silmarils", silmarils);
    if (run_mode_is_blitz())
        blitz_show_end_summary((byte)MAX(silmarils, 0));
    else
        metarun_update_on_exit(false, true, silmarils, 0);
}

void do_cmd_suicide(void)
{
    char ch;

    flush();

    if (!get_check("This will destroy the current character: are you sure? "))
        return;

    prt("Please verify ABORTING by typing the '@' sign: ", 0, 0);
    flush();
    ch = inkey();
    prt("", 0, 0);
    if (ch != '@')
        return;

    p_ptr->is_dead = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;

    SDL_strlcpy(p_ptr->died_from, "their own hand", sizeof(p_ptr->died_from));
    killer_mark_other(SCORE_KILLER_SELF);
    killer_commit(p_ptr->died_from);
}

errr file_character(cptr name, bool full)
{
    int i, x, y;
    byte a;
#define SDL_IOprintf SDL_IOprintf
    char c;
    SDL_IOStream* fd;
    SDL_IOStream* fff = NULL;
    char o_name[80];
    char buf[1024];
    ability_type* b_ptr;
    int holder;
    bool challenges = false;
    high_score the_score;

    (void)full;

    path_build(buf, sizeof(buf), ANGBAND_DIR_USER, name);
    FILE_TYPE(FILE_TYPE_TEXT);

    fd = sdl_fopen(buf, "rb");
    if (fd)
    {
        char out_val[160];
        sdl_fclose(fd);
        strnfmt(out_val, sizeof(out_val), "Replace existing file %s? ", buf);
        if (get_check(out_val))
            fd = NULL;
    }

    if (!fd)
        fff = sdl_fopen(buf, "w");
    if (!fff)
        return -1;

    text_out_hook = text_out_to_file;
    text_out_file = fff;

    SDL_IOprintf(fff, "  [%s %s Character Dump]\n\n", VERSION_NAME, VERSION_STRING);

    display_player(0);

    for (y = 2; y < 23; y++)
    {
        for (x = 0; x < 79; x++)
        {
            (void)(Term_what(x, y, &a, &c));
            buf[x] = c;
        }

        while ((x > 0) && (buf[x - 1] == ' '))
            --x;

        buf[x] = '\0';
        SDL_IOprintf(fff, "%s\n", buf);
    }

    if (p_ptr->is_dead)
    {
        i = message_num();
        if (i > 15)
            i = 15;
        SDL_IOprintf(fff, "\n  [Last Messages]\n\n");
        while (i-- > 0)
            SDL_IOprintf(fff, "> %s\n", message_str((s16b)i));
        SDL_IOprintf(fff, "\n");

        SDL_IOprintf(fff, "\n  [Screenshot]\n\n");
        if (!p_ptr->escaped)
        {
            for (y = 0; y <= 6; y++)
            {
                SDL_IOprintf(fff, "  ");
                for (x = 0; x <= 6; x++)
                    SDL_IOprintf(fff, "%c", mini_screenshot_char[y][x]);
                SDL_IOprintf(fff, "\n");
            }
        }
        else
        {
            SDL_IOprintf(fff, "  .......\n");
            SDL_IOprintf(fff, "  ~...#..\n");
            SDL_IOprintf(fff, "  ~~.....\n");
            SDL_IOprintf(fff, "  .~.@...\n");
            SDL_IOprintf(fff, "  .~~...#\n");
            SDL_IOprintf(fff, "  ..~~...\n");
            SDL_IOprintf(fff, "  ...~...\n");
        }
        SDL_IOprintf(fff, "\n");
    }

    if (p_ptr->equip_cnt)
    {
        SDL_IOprintf(fff, "\n  [Equipment]\n\n");
        for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            object_type* o_ptr = &inventory[i];
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

            if (o_ptr->weight
                && ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM)
                    || (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING)
                    || (o_ptr->tval == TV_BOW)))
            {
                int wgt = o_ptr->weight * o_ptr->number;
                char wgt_buf[80];
                sprintf(wgt_buf, " %d.%1d lb", wgt / 10, wgt % 10);
                SDL_strlcat(o_name, wgt_buf, sizeof(o_name));
            }

            SDL_IOprintf(fff, "%c) %s\n", index_to_label(i), o_name);
            identify_random_gen(o_ptr);
        }
        SDL_IOprintf(fff, "\n\n");
    }

    SDL_IOprintf(fff, "  [Inventory]\n\n");
    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx)
            break;

        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        if (o_ptr->weight
            && ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM)
                || (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING)
                || (o_ptr->tval == TV_BOW)))
        {
            int wgt = o_ptr->weight * o_ptr->number;
            char wgt_buf[80];
            sprintf(wgt_buf, " %d.%1d lb", wgt / 10, wgt % 10);
            SDL_strlcat(o_name, wgt_buf, sizeof(o_name));
        }

        SDL_IOprintf(fff, "%c) %s\n", index_to_label(i), o_name);
        identify_random_gen(o_ptr);
    }

    SDL_IOprintf(fff, "\n\n  [Abilities]\n\n");
    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        if (!b_ptr->name)
            continue;

        if (p_ptr->innate_ability[b_ptr->skilltype][b_ptr->abilitynum])
        {
            if (b_ptr->skilltype == S_PER && b_ptr->abilitynum == PER_BANE
                && p_ptr->bane_type > 0)
            {
                SDL_IOprintf(fff, "%s-%s\n", bane_name[p_ptr->bane_type],
                    (b_name + b_ptr->name));
            }
            else if (b_ptr->skilltype == S_WIL && b_ptr->abilitynum == WIL_OATH
                && p_ptr->oath_type > 0)
            {
                if (oath_invalid(p_ptr->oath_type))
                    SDL_IOprintf(fff, "%s: %s (Broken)\n", (b_name + b_ptr->name),
                        oath_name[p_ptr->oath_type]);
                else
                    SDL_IOprintf(fff, "%s: %s\n", (b_name + b_ptr->name),
                        oath_name[p_ptr->oath_type]);
            }
            else
                SDL_IOprintf(fff, "%s\n", (b_name + b_ptr->name));
        }
    }

    SDL_IOprintf(fff, "\n\n  [Enemies]\n\n");
    for (i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        if (!l_ptr->psights && !l_ptr->pkills)
            continue;

        if (r_ptr->flags1 & (RF1_UNIQUE))
            SDL_IOprintf(fff, "  %-7s %s \n", l_ptr->pkills ? "(slain)" : "(seen)",
                (r_name + r_ptr->name));
        else
            SDL_IOprintf(fff, "%3d /%3d  %-40s\n", l_ptr->pkills, l_ptr->psights,
                (r_name + r_ptr->name));
    }

    if (p_ptr->is_dead)
    {
        SDL_IOprintf(fff, "\n\n  [Artefacts]\n\n");

        for (i = 0; i < z_info->art_norm_max; i++)
        {
            char art_name[120];
            artefact_type* a_ptr;
            object_type* o_ptr;
            object_type object_type_body;
            o_ptr = &object_type_body;

            a_ptr = &a_info[i];
            if (a_ptr->cur_num == 0)
                continue;

            make_fake_artefact(o_ptr, i);
            object_desc_spoil(art_name, sizeof(art_name), o_ptr, true, 0);

            SDL_IOprintf(fff, "%s %s\n", art_name,
                a_ptr->found_num > 0 ? "(found)" : "");
        }
    }

    SDL_IOprintf(fff, "\n\n  [Notes]\n\n");

    i = 0;
    holder = notes_buffer[i];
    while (holder != '\0')
    {
        holder = notes_buffer[i];
        if (holder != '\0')
            SDL_IOprintf(fff, "%c", holder);
        i++;
    }

    SDL_IOprintf(fff, "\n");

    for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
    {
        if (option_desc[i] && op_ptr->opt[i])
            challenges = true;
    }

    if (challenges)
    {
        SDL_IOprintf(fff, "  [Challenges]\n\n");

        for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
        {
            if (option_desc[i] && op_ptr->opt[i])
                SDL_IOprintf(fff, "%-45s\n", option_desc[i]);
        }
    }

    SDL_IOprintf(fff, "\n\n");
    create_score(&the_score);
    SDL_IOprintf(fff, "  ['Score' %.9d]\n\n", score_points(&the_score));

    sdl_fclose(fff);

#undef SDL_IOprintf

    return 0;
}

static void get_tile(int row, int col, byte* a_def, char* c_def)
{
    byte a;
    char c;

    a = Term->scr->a[row][col];
    c = Term->scr->c[row][col];

    *a_def = a;
    *c_def = c;
}

void mini_screenshot(void)
{
    int x, y, wid, hgt;
    byte a;
    char c;
    int player_y = -1, player_x = -1;
    int sample_y, sample_x;
    int max_hgt, max_wid;
    char screen_char[100][200];
    byte screen_attr[100][200];

    Term_get_size(&wid, &hgt);

    for (y = 0; y < 100; y++)
    {
        for (x = 0; x < 200; x++)
        {
            screen_char[y][x] = ' ';
            screen_attr[y][x] = TERM_DARK;
        }
    }

    max_hgt = MIN(hgt, (int)N_ELEMENTS(screen_char));
    max_wid = MIN(wid, (int)N_ELEMENTS(screen_char[0]));

    for (y = 0; y < max_hgt; y++)
    {
        for (x = 0; x < max_wid; x++)
        {
            get_tile(y, x, &a, &c);

            if ((c == '@')
                && ((a == TERM_WHITE) || (a == TERM_YELLOW)
                    || (a == TERM_ORANGE) || (a == TERM_L_RED)
                    || (a == TERM_RED)))
            {
                player_x = x;
                player_y = y;
            }

            screen_char[y][x] = c;
            screen_attr[y][x] = a;
        }
    }

    if (player_y >= 0 && player_x >= 0)
    {
        for (y = 0; y <= 6; y++)
        {
            for (x = 0; x <= 6; x++)
            {
                if (reliability_sample_square_point(player_y, player_x, 3, y,
                        x, max_hgt, max_wid, &sample_y, &sample_x))
                {
                    mini_screenshot_char[y][x] = screen_char[sample_y][sample_x];
                    mini_screenshot_attr[y][x] = screen_attr[sample_y][sample_x];
                }
                else
                {
                    mini_screenshot_char[y][x] = ' ';
                    mini_screenshot_attr[y][x] = TERM_DARK;
                }
            }
        }
    }
    else
    {
        for (y = 0; y <= 6; y++)
        {
            for (x = 0; x <= 6; x++)
            {
                mini_screenshot_char[y][x] = ' ';
                mini_screenshot_attr[y][x] = TERM_DARK;
            }
        }
    }
}

void prt_mini_screenshot(int col, int row)
{
    int x, y;

    if (!p_ptr->escaped)
    {
        for (y = 0; y <= 6; y++)
        {
            for (x = 0; x <= 6; x++)
            {
                if ((x == 3) && (y == 3))
                    Term_putch(col + x, row + y, TERM_RED, mini_screenshot_char[y][x]);
                else
                    Term_putch(col + x, row + y, mini_screenshot_attr[y][x],
                        mini_screenshot_char[y][x]);
            }
        }
    }
    else
    {
        Term_putstr(col, row, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 1, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 2, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 3, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 4, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 5, -1, TERM_L_GREEN, ".......");
        Term_putstr(col, row + 6, -1, TERM_L_GREEN, ".......");

        Term_putch(col, row + 1, TERM_BLUE, '~');
        Term_putch(col, row + 2, TERM_BLUE, '~');
        Term_putch(col + 1, row + 2, TERM_L_BLUE, '~');
        Term_putch(col + 1, row + 3, TERM_BLUE, '~');
        Term_putch(col + 1, row + 4, TERM_L_BLUE, '~');
        Term_putch(col + 2, row + 4, TERM_BLUE, '~');
        Term_putch(col + 2, row + 5, TERM_BLUE, '~');
        Term_putch(col + 3, row + 5, TERM_L_BLUE, '~');
        Term_putch(col + 3, row + 6, TERM_BLUE, '~');

        Term_putch(col + 4, row + 1, TERM_GREEN, '#');
        Term_putch(col + 6, row + 4, TERM_GREEN, '#');

        Term_putch(col + 3, row + 3, TERM_WHITE, '@');
    }
}

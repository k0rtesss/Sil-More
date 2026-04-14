/* File: files.c */

/*
 * Transitional facade after the WP71 score/runtime split.
 *
 * Remaining ownership here:
 *   - privilege helpers still used broadly across save/load/score code
 *   - escape/suicide commands
 *   - character dump and miniature screenshot helpers
 */

#ifndef WINDOWS
#define _DEFAULT_SOURCE  /* For DT_DIR and other POSIX extensions */
#define _BSD_SOURCE      /* For setregid on older systems */
#endif

#include "angband.h"
#include "app/app-session.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
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

#ifdef WINDOWS
#include <windows.h>
#include <direct.h>  /* For _mkdir */
#else
#include <sys/stat.h>  /* For mkdir */
#include <dirent.h>    /* For directory operations */
#include <unistd.h>    /* For setregid, getgid, etc. */
#include <signal.h>    /* For kill, SIGSTOP */
#endif

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

    /* Defer metarun exit processing until close_game_aux() has recorded the
     * final score. Otherwise the rollover can start a fresh metarun before
     * this escape is written, and the winning character lands in the new run.
     */
    log_info("Player escaped with %d Silmarils (metarun processing deferred until close_game_aux)", silmarils);
}

void do_cmd_suicide(void)
{
    char ch;

    flush();

    if (!get_check("This will destroy the current character: are you sure? "))
        return;

    /* Special Verification for suicide */
    if (!get_com("Please verify ABORTING by typing the '~' sign: ", &ch))
        return;
    if (ch != '~')
        return;

    p_ptr->is_dead = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;

    SDL_strlcpy(p_ptr->died_from, "their own hand", sizeof(p_ptr->died_from));
    killer_mark_other(SCORE_KILLER_SELF);
    killer_commit(p_ptr->died_from);
}

static const app_ui_panel* file_character_sheet_panel(const app_ui_scene* scene)
{
    size_t i;

    if (!scene)
        return NULL;

    for (i = 0; i < scene->panel_count; i++)
    {
        if (scene->panels[i].style == APP_UI_PANEL_STYLE_CHARACTER_SHEET)
            return &scene->panels[i];
    }

    if (scene->panel_count > 0)
        return &scene->panels[0];

    return NULL;
}

static void file_character_append_field(char* buf, size_t buf_size, cptr text)
{
    if (!buf || buf_size == 0 || !text || !text[0])
        return;

    if (buf[0])
        SDL_strlcat(buf, " ", buf_size);
    SDL_strlcat(buf, text, buf_size);
}

static void file_character_write_metric(SDL_IOStream* fff,
    const app_ui_character_metric* metric)
{
    char line[256];

    if (!fff || !metric)
        return;

    if (!metric->label[0] && !metric->value[0] && !metric->secondary[0])
    {
        SDL_IOprintf(fff, "\n");
        return;
    }

    line[0] = '\0';
    if (metric->label[0])
        strnfmt(line, sizeof(line), "%-12s %s", metric->label, metric->value);
    else
        SDL_strlcpy(line, metric->value, sizeof(line));

    if (metric->separator && metric->secondary[0])
    {
        char suffix[96];

        strnfmt(suffix, sizeof(suffix), " %c %s", metric->separator,
            metric->secondary);
        SDL_strlcat(line, suffix, sizeof(line));
    }
    else if (metric->secondary[0])
    {
        file_character_append_field(line, sizeof(line), metric->secondary);
    }

    SDL_IOprintf(fff, "%s\n", line);
}

static void file_character_write_stat(SDL_IOStream* fff,
    const app_ui_character_stat* stat)
{
    char line[256];

    if (!fff || !stat)
        return;

    if (!stat->label[0] && !stat->value[0] && !stat->base[0] && !stat->mod1[0]
        && !stat->mod2[0] && !stat->mod3[0])
    {
        SDL_IOprintf(fff, "\n");
        return;
    }

    line[0] = '\0';
    if (stat->label[0])
        strnfmt(line, sizeof(line), "%-12s %s", stat->label, stat->value);
    else
        SDL_strlcpy(line, stat->value, sizeof(line));

    if (stat->separator && stat->base[0])
    {
        char suffix[64];

        strnfmt(suffix, sizeof(suffix), " %c %s", stat->separator, stat->base);
        SDL_strlcat(line, suffix, sizeof(line));
    }

    file_character_append_field(line, sizeof(line), stat->mod1);
    file_character_append_field(line, sizeof(line), stat->mod2);
    file_character_append_field(line, sizeof(line), stat->mod3);

    SDL_IOprintf(fff, "%s\n", line);
}

static void file_character_write_history(SDL_IOStream* fff,
    const app_ui_scene* scene, const app_ui_panel* panel)
{
    void (*old_hook)(byte, cptr) = text_out_hook;
    ang_file* old_file = text_out_file;
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;
    size_t i;

    if (!fff || !scene || !panel || panel->rich_paragraph_count == 0)
        return;

    text_out_hook = text_out_to_file;
    text_out_file = fff;
    text_out_wrap = 75;
    text_out_indent = 2;

    for (i = 0; i < panel->rich_paragraph_count; i++)
    {
        u16b paragraph_index = (u16b)(panel->rich_paragraph_first + i);
        const app_ui_rich_paragraph* paragraph;
        size_t j;

        if (paragraph_index >= scene->rich_paragraph_count)
            break;

        paragraph = &scene->rich_paragraphs[paragraph_index];
        for (j = 0; j < paragraph->run_count; j++)
        {
            u16b run_index = (u16b)(paragraph->run_first + j);

            if (run_index >= scene->rich_run_count)
                break;

            text_out_c(scene->rich_runs[run_index].attr,
                scene->rich_runs[run_index].text);
        }

        text_out("\n\n");
    }

    text_out_hook = old_hook;
    text_out_file = old_file;
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;
}

static bool file_character_write_semantic_sheet(SDL_IOStream* fff)
{
    app_ui_scene scene;
    const app_ui_panel* panel;
    size_t i;

    if (!fff)
        return false;
    if (!build_character_sheet_ui_scene(&scene, NULL))
        return false;

    panel = file_character_sheet_panel(&scene);
    if (!panel)
        return false;

    if (panel->title[0])
        SDL_IOprintf(fff, "  %s\n\n", panel->title);

    for (i = 0; i < panel->character_metric_count; i++)
        file_character_write_metric(fff, &panel->character_metrics[i]);

    if (panel->detail_line_count > 0)
    {
        SDL_IOprintf(fff, "\n");
        for (i = 0; i < panel->detail_line_count; i++)
            SDL_IOprintf(fff, "%s\n",
                panel->detail_lines[i].text[0] ? panel->detail_lines[i].text : "");
    }

    if (panel->character_stat_count > 0)
    {
        SDL_IOprintf(fff, "\n");
        for (i = 0; i < panel->character_stat_count; i++)
            file_character_write_stat(fff, &panel->character_stats[i]);
    }

    if (panel->rich_paragraph_count > 0)
    {
        SDL_IOprintf(fff, "\n");
        file_character_write_history(fff, &scene, panel);
    }

    return true;
}

errr file_character(cptr name, bool full)
{
    int i, x, y;
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

    SDL_IOprintf(fff, "  [%s %s Character Dump]\n\n", VERSION_NAME, VERSION_STRING);

    if (!file_character_write_semantic_sheet(fff))
    {
        sdl_fclose(fff);
        return -1;
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

    return 0;
}

static void mini_screenshot_clear_buffers(void)
{
    int x, y;

    for (y = 0; y <= 6; y++)
    {
        for (x = 0; x <= 6; x++)
        {
            mini_screenshot_char[y][x] = ' ';
            mini_screenshot_attr[y][x] = TERM_DARK;
        }
    }
}

static bool mini_screenshot_from_snapshot(const app_dungeon_snapshot* snapshot)
{
    const app_map_snapshot* map;
    int player_y;
    int player_x;
    int sample_y;
    int sample_x;
    int x, y;

    if (!snapshot || !snapshot->map_data)
        return false;

    map = (const app_map_snapshot*)snapshot->map_data;
    if (!map->width || !map->height)
        return false;

    player_y = map->player_y - map->panel_y;
    player_x = map->player_x - map->panel_x;
    if (player_y < 0 || player_y >= map->height || player_x < 0
        || player_x >= map->width)
    {
        return false;
    }

    mini_screenshot_clear_buffers();

    for (y = 0; y <= 6; y++)
    {
        for (x = 0; x <= 6; x++)
        {
            if (reliability_sample_square_point(player_y, player_x, 3, y, x,
                    map->height, map->width, &sample_y, &sample_x))
            {
                size_t index = ((size_t)sample_y * map->width) + (size_t)sample_x;
                const app_map_cell_snapshot* cell = &map->cells[index];

                mini_screenshot_char[y][x] = cell->ch ? cell->ch : ' ';
                mini_screenshot_attr[y][x] = cell->attr;
            }
        }
    }

    return true;
}

void mini_screenshot(void)
{
    app_session* session = app_session_current();
    const app_dungeon_snapshot* snapshot = NULL;

    mini_screenshot_clear_buffers();

    if (session && app_session_build_dungeon_snapshot(session, 0, 0, 0))
        snapshot = app_session_dungeon_snapshot(session);

    if (!mini_screenshot_from_snapshot(snapshot))
        log_warn("mini_screenshot: dungeon snapshot unavailable");
}

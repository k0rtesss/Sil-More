/* File: cmd4.c */

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

/* String used to show a color sample */
#define COLOR_SAMPLE "###"

/*max length of note output*/
#define LINEWRAP 75

/*used for knowledge display*/
#define BROWSER_ROWS 16

/* Option changes that affect list rendering should refresh subwindows immediately. */
static void redraw_inven_equip_subwindows(void);
static void redraw_monster_subwindows(void);
static void controller_prompt_label(int binding, const char* fallback, char* buf, size_t buflen);

typedef struct knowledge_browser_layout knowledge_browser_layout;
typedef struct knowledge_browser_state knowledge_browser_state;

/*
 *  Header and footer marker string for pref file dumps
 */
static cptr dump_seperator = "#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#";

typedef struct monster_list_entry monster_list_entry;
/*
 * Structure for building monster "lists"
 */
struct monster_list_entry
{
    s16b r_idx; /* Monster race index */

    byte amount;
};

typedef struct object_list_entry object_list_entry;
struct object_list_entry
{
    enum
    {
        OBJ_NONE,
        OBJ_NORMAL,
        OBJ_SPECIAL
    } type;
    int idx;
    int e_idx;
    int tval, sval;
};

typedef struct supply_list_entry supply_list_entry;

struct supply_list_entry
{
    int item_idx;   /* First inventory slot containing this kind */
    int k_idx;      /* Object kind index */
    int total;      /* Total quantity across the pack */
    int supply_idx; /* Index inside the supply cache (-1 if not present) */
};

struct knowledge_browser_layout
{
    int term_wid;
    int term_hgt;
    int title_row;
    int tabs_row;
    int header_row;
    int divider_row;
    int list_row;
    int list_rows;
    int status_row;
    int prompt_row;
    int group_col;
    int group_w;
    int divider_col;
    int list_col;
    int list_w;
};

struct knowledge_browser_state
{
    int column[4];
    int group_cur[4];
    int group_top[4];
    int entry_cur[4];
    int entry_top[4];
    bool tabs_focus;
};

static int g_knowledge_last_page = KNOWLEDGE_PAGE_ARTEFACTS;

static void dump_visual_pair(
    SDL_IOStream* fff, const char* tag, int index, byte attr, byte chr)
{
    bool attr_tile = (attr & TILE_FLAG) != 0;
    bool char_tile = (chr & TILE_FLAG) != 0;

    SDL_IOprintf(fff, "%s:%d:", tag, index);
    if (attr_tile)
        SDL_IOprintf(fff, "R%d", TILE_GET_INDEX(attr));
    else
        SDL_IOprintf(fff, "0x%02X", attr);

    SDL_WriteU8(fff, ':');

    if (char_tile)
        SDL_IOprintf(fff, "C%d", TILE_GET_INDEX(chr));
    else
        SDL_IOprintf(fff, "0x%02X", (byte)chr);

    SDL_WriteU8(fff, '\n');
    SDL_WriteU8(fff, '\n');
}


static bool supplies_menu_use_entry(supply_list_entry* entry)
{
    if (!entry || entry->supply_idx < 0)
        return false;

    object_type* o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    supplies_begin_action(entry->supply_idx);

    switch (o_ptr->tval)
    {
    case TV_FOOD:
        do_cmd_eat_food(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_POTION:
        do_cmd_quaff_potion(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_STAFF:
        do_cmd_activate_staff(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_GEM:
        do_cmd_use_gem(o_ptr, SUPPLIES_INDEX);
        break;
    default:
        supplies_end_action();
        bell("Cannot use that item here!");
        msg_print("Cannot use that item here.");
        return false;
    }

    supplies_end_action();
    return true;
}

static bool supplies_menu_drop_entry(supply_list_entry* entry)
{
    if (!entry || entry->supply_idx < 0)
        return false;

    object_type* o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    int max_amt = o_ptr->number;
    if (max_amt <= 0)
        return false;

    int actual_amt = get_quantity(NULL, max_amt);
    if (actual_amt <= 0)
        return false;
    supplies_begin_action(entry->supply_idx);
    bool dropped = supplies_drop_amount(entry->supply_idx, actual_amt);
    supplies_end_action();

    if (dropped)
        handle_stuff();

    return dropped;
}

static cptr supply_group_text[SUPPLY_GROUP_MAX + 1] = {
    "Herbs",
    "Potions",
    "Gems",
    NULL
};

/*
 * Remove old lines from pref files
 */
static void remove_old_dump(cptr orig_file, cptr mark)
{
    SDL_IOStream* tmp_fff, *orig_fff;

    char tmp_file[1024];
    char buf[1024];
    bool between_marks = false;
    bool changed = false;
    char expected_line[1024];

    /* Open an old dump file in read-only mode */
    orig_fff = sdl_fopen(orig_file, "r");

    /* If original file does not exist, nothing to do */
    if (!orig_fff)
        return;

    /* Open a new temporary file */
    tmp_fff = sdl_fopen_temp(tmp_file, sizeof(tmp_file));

    if (!tmp_fff)
    {
        msg_format("Failed to create temporary file %s.", tmp_file);
        msg_print(NULL);
        return;
    }

    strnfmt(expected_line, sizeof(expected_line), "%s begin %s", dump_seperator,
        mark);

    /* Loop for every line */
    while (true)
    {
        /* Read a line */
        if (sdl_fgets(orig_fff, buf, sizeof(buf)))
        {
            /* End of file but no end marker */
            if (between_marks)
                changed = false;

            break;
        }

        /* Is this line a header/footer? */
        if (strncmp(buf, dump_seperator, strlen(dump_seperator)) == 0)
        {
            /* Found the expected line? */
            if (strcmp(buf, expected_line) == 0)
            {
                if (!between_marks)
                {
                    /* Expect the footer next */
                    strnfmt(expected_line, sizeof(expected_line), "%s end %s",
                        dump_seperator, mark);

                    between_marks = true;

                    /* There are some changes */
                    changed = true;
                }
                else
                {
                    /* Expect a header next - XXX shouldn't happen */
                    strnfmt(expected_line, sizeof(expected_line), "%s begin %s",
                        dump_seperator, mark);

                    between_marks = false;

                    /* Next line */
                    continue;
                }
            }
            /* Found a different line */
            else
            {
                /* Expected a footer and got something different? */
                if (between_marks)
                {
                    /* Abort */
                    changed = false;
                    break;
                }
            }
        }

        if (!between_marks)
        {
            /* Copy orginal line */
            SDL_IOprintf(tmp_fff, "%s\n", buf);
        }
    }

    /* Close files */
    sdl_fclose(orig_fff);
    sdl_fclose(tmp_fff);

    /* If there are changes, overwrite the original file with the new one */
    if (changed)
    {
        /* Copy contents of temporary file */
        tmp_fff = sdl_fopen(tmp_file, "r");
        orig_fff = sdl_fopen(orig_file, "w");

        while (!sdl_fgets(tmp_fff, buf, sizeof(buf)))
        {
            SDL_IOprintf(orig_fff, "%s\n", buf);
        }

        sdl_fclose(orig_fff);
        sdl_fclose(tmp_fff);
    }

    /* Kill the temporary file */
    fd_kill(tmp_file);
}

/*
 * Output the header of a pref-file dump
 */
static void pref_header(SDL_IOStream* fff, cptr mark)
{
    /* Start of dump */
    SDL_IOprintf(fff, "%s begin %s\n", dump_seperator, mark);

    SDL_IOprintf(fff, "# *Warning!*  The lines below are an automatic dump.\n");
    SDL_IOprintf(fff,
        "# Don't edit them; changes will be deleted and replaced "
        "automatically.\n");
}

/*
 * Output the footer of a pref-file dump
 */
static void pref_footer(SDL_IOStream* fff, cptr mark)
{
    SDL_IOprintf(fff, "# *Warning!*  The lines above are an automatic dump.\n");
    SDL_IOprintf(fff,
        "# Don't edit them; changes will be deleted and replaced "
        "automatically.\n");

    /* End of dump */
    SDL_IOprintf(fff, "%s end %s\n", dump_seperator, mark);
}

/*
 * Hack -- redraw the screen
 *
 * This command performs various low level updates, clears all the "extra"
 * windows, does a total redraw of the main window, and requests all of the
 * interesting updates and redraws that I can think of.
 *
 * This command is also used to "instantiate" the results of the user
 * selecting various things, such as graphics mode, so it must call
 * the "TERM_XTRA_REACT" hook before redrawing the windows.
 */
void do_cmd_redraw(void)
{
    int j;

    term* old = Term;

    /* Low level flush */
    Term_flush();

    /* Reset "inkey()" */
    flush();

    if (g_banner_force_redraw_remaining <= 0)
        clear_active_narrative_banner();

    /* Hack -- React to changes */
    Term_xtra(TERM_XTRA_REACT, 0);

    /* Combine and Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Update stuff */
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw everything */
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIPPY | PR_RESIST);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    /* Window stuff */
    p_ptr->window
        |= (PW_MESSAGE | PW_OVERHEAD | PW_MONSTER | PW_OBJECT | PW_MONLIST);

    /* Clear screen */
    Term_clear();

    /* Hack -- update */
    handle_stuff();

    /* Redraw every window */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Dead window */
        if (!angband_term[j])
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Redraw */
        Term_redraw();

        /* Refresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- character sheet
 */
static void character_sheet_put_prompt_fit(int col, int row, int wid, byte attr, cptr text)
{
    char buf[256];
    int max_len;

    if (!text)
        return;

    if (wid < 1)
        wid = 80;

    max_len = wid - col - 1;
    if (max_len < 1)
        return;

    SDL_strlcpy(buf, text, sizeof(buf));
    if ((int)strlen(buf) > max_len)
        buf[max_len] = '\0';

    Term_putstr(col, row, -1, attr, buf);
}

static bool character_sheet_prompt_append(char* buf, size_t buflen, cptr token, int max_width)
{
    size_t cur_len;
    size_t tok_len;
    int sep = 0;

    if (!buf || !buflen || !token || !token[0])
        return true;

    cur_len = strlen(buf);
    tok_len = strlen(token);
    if (cur_len > 0)
        sep = 2;

    if ((int)(cur_len + sep + tok_len) > max_width)
        return false;

    if (sep)
        SDL_strlcat(buf, "  ", buflen);
    SDL_strlcat(buf, token, buflen);
    return true;
}

static void character_sheet_build_prompt(bool steamdeck, bool include_curses,
    int wid, char* out, size_t outsz)
{
    int max_width;

    if (!out || !outsz)
        return;

    out[0] = '\0';

    if (wid < 1)
        wid = 80;

    max_width = wid - 2;
    if (max_width < 1)
        return;

    if (steamdeck)
    {
        char notes_label[16], story_label[16], file_label[16];
        char abilities_label[16], increase_label[16], help_label[16], back_label[16];
        char token[7][64];

        controller_prompt_label('n', "n", notes_label, sizeof(notes_label));
        controller_prompt_label(steamdeck_secondary_key(), "Y", story_label, sizeof(story_label));
        controller_prompt_label('e', "L1", file_label, sizeof(file_label));
        controller_prompt_label(steamdeck_alt_action_key(), "X", abilities_label, sizeof(abilities_label));
        controller_prompt_label(steamdeck_confirm_key(), "A", increase_label, sizeof(increase_label));
        controller_prompt_label(steamdeck_info_key(), "RS", help_label, sizeof(help_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));

        strnfmt(token[0], sizeof(token[0]), "%s abilities", abilities_label);
        strnfmt(token[1], sizeof(token[1]), "%s increase", increase_label);
        strnfmt(token[2], sizeof(token[2]), "%s help", help_label);
        strnfmt(token[3], sizeof(token[3]), "%s back", back_label);
        strnfmt(token[4], sizeof(token[4]), "%s notes", notes_label);
        strnfmt(token[5], sizeof(token[5]), "%s story", story_label);
        strnfmt(token[6], sizeof(token[6]), "%s file", file_label);

        for (int i = 0; i < 4; i++)
            (void)character_sheet_prompt_append(out, outsz, token[i], max_width);
        for (int i = 4; i < 7; i++)
            (void)character_sheet_prompt_append(out, outsz, token[i], max_width);
    }
    else
    {
        const char* essential[] = {
            "a abilities", "Space/i increase", "? help", "ESC back"
        };
        const char* optional[] = {
            "n notes", "s story", "f file"
        };

        for (int i = 0; i < (int)(sizeof(essential) / sizeof(essential[0])); i++)
            (void)character_sheet_prompt_append(out, outsz, essential[i], max_width);

        if (include_curses)
            (void)character_sheet_prompt_append(out, outsz, "c curses", max_width);

        for (int i = 0; i < (int)(sizeof(optional) / sizeof(optional[0])); i++)
            (void)character_sheet_prompt_append(out, outsz, optional[i], max_width);
    }

    if (!out[0])
    {
        if (steamdeck)
            SDL_strlcpy(out, "B back", outsz);
        else
            SDL_strlcpy(out, "ESC back", outsz);
    }
}

static void character_sheet_draw_page_indicator(int sheet_page, int compact_pages,
    int wid, int row, bool use_story_font)
{
    char page_buf[32];
    int col;

    if (compact_pages <= 1)
        return;

    if (wid < 1)
        wid = 80;

    strnfmt(page_buf, sizeof(page_buf), "%d/%d", sheet_page + 1, compact_pages);
    col = wid - (int)strlen(page_buf) - 1;
    if (col < 0)
        col = 0;

    if (use_story_font)
        sdl_story_font_enable();

    Term_putstr(col, row, -1, TERM_SLATE, page_buf);

    if (use_story_font)
        sdl_story_font_disable();
}

void do_cmd_character_sheet(void)
{
    char ch;

    int mode = 0;
    int sheet_page = 1;
    int body_scroll = 0;
    int last_sheet_page = -1;

    enum {
        CHAR_SHEET_MODE_COMPACT_DESC_FLAGS = 100,
        CHAR_SHEET_MODE_COMPACT_STATS_SKILLS = 101,
    };

    /* Clear any active banner before opening character sheet */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Forever */
    while (1)
    {
        int wid = 80;
        int hgt = 24;
        int prompt_row;
        int indicator_row = 1;
        bool compact_sheet;
        int compact_pages;
        int max_body_scroll = 0;
        bool steamdeck = steamdeck_controls_active();

        Term_get_size(&wid, &hgt);
        if (wid < 1)
            wid = 80;
        if (hgt < 1)
            hgt = 24;

        compact_sheet = (wid < 80);
        compact_pages = compact_sheet ? 2 : 1;
        if (sheet_page >= compact_pages)
            sheet_page = compact_pages - 1;
        if (sheet_page < 0)
            sheet_page = 0;
        if (sheet_page != last_sheet_page)
        {
            body_scroll = 0;
            last_sheet_page = sheet_page;
        }

        if (compact_sheet)
        {
            switch (sheet_page)
            {
            case 0: mode = CHAR_SHEET_MODE_COMPACT_DESC_FLAGS; break;
            case 1: mode = CHAR_SHEET_MODE_COMPACT_STATS_SKILLS; break;
            default: mode = CHAR_SHEET_MODE_COMPACT_DESC_FLAGS; break;
            }
        }
        else
        {
            mode = 0;
        }

        if (compact_sheet && sheet_page == 0)
            display_player_compact_set_scroll(body_scroll);
        else
            display_player_compact_set_scroll(0);

        /* Display the player */
        display_player(mode);
        if (compact_sheet && sheet_page == 0)
        {
            max_body_scroll = display_player_compact_get_max_scroll();
            if (body_scroll > max_body_scroll)
            {
                body_scroll = max_body_scroll;
                display_player_compact_set_scroll(body_scroll);
                display_player(mode);
            }
        }

        if (compact_sheet && hgt <= 18)
            indicator_row = 0;

        prompt_row = hgt - 1;
        if (prompt_row < 0)
            prompt_row = 0;
        Term_erase(0, prompt_row, 255);

        /* Prompt - dynamic, width-aware, and user-friendly for new players */
        {
            char prompt_buf[256];
#ifdef DEBUG_CURSES
            const bool include_curses = true;
#else
            const bool include_curses = false;
#endif

            character_sheet_build_prompt(steamdeck, include_curses, wid, prompt_buf, sizeof(prompt_buf));

            if (story_character_enabled())
                sdl_story_font_enable();

            character_sheet_put_prompt_fit(1, prompt_row, wid, TERM_L_WHITE, prompt_buf);

            character_sheet_draw_page_indicator(sheet_page, compact_pages, wid,
                indicator_row,
                story_character_enabled());

            if (story_character_enabled())
                sdl_story_font_disable();
        }

        Term_fresh();  /* Render commands */

        if (story_character_enabled()) {
            sdl_story_font_disable();
        }

        /* Query */
        ch = inkey();

        /* Exit - B button (back) or ESC */
        if (ch == ESCAPE || (steamdeck && ch == steamdeck_back_key()))
            break;
        if ((ch == '\r') || (ch == '\n') || (ch == 'q') || (ch == 'Q'))
            break;

        if (compact_pages > 1)
        {
            if (sheet_page == 0 && max_body_scroll > 0)
            {
                if (ch == '8')
                {
                    if (body_scroll > 0)
                        body_scroll--;
                    continue;
                }
                if (ch == '2')
                {
                    if (body_scroll < max_body_scroll)
                        body_scroll++;
                    continue;
                }
            }

            if ((ch == '4') || ((ch == '8') && !(sheet_page == 0 && max_body_scroll > 0)))
            {
                sheet_page = (sheet_page + compact_pages - 1) % compact_pages;
                continue;
            }
            if ((ch == '6') || ((ch == '2') && !(sheet_page == 0 && max_body_scroll > 0)))
            {
                sheet_page = (sheet_page + 1) % compact_pages;
                continue;
            }
        }

        /* Increase skills - 'i', Space, or confirm button */
        if (ch == 'i' || ch == ' ' || ch == INPUT_BIND_CONFIRM
            || (steamdeck && ch == steamdeck_confirm_key()))
        {
            gain_skills();
            /* Force redraw after skill changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* Show notes - 'n' */
        else if (ch == 'n')
        {
            do_cmd_knowledge_notes();
        }

        /* Story stats - 's' or Y button */
        else if (ch == 's' || (steamdeck && ch == steamdeck_secondary_key()))
        {
            print_metarun_stats();
        }

#ifdef DEBUG_CURSES
        /* Curses Menu */
        else if (ch == 'c')
        {
            dbg_show_active_flags();
        }
#endif

        /* Abilities - 'a', Tab, or X button */
        else if ((ch == 'a') || (ch == '\t') || (steamdeck && ch == steamdeck_alt_action_key()))
        {
            (void)do_cmd_ability_screen();
            /* Force redraw after ability changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* File dump - 'f' or L1 ('e') */
        else if (ch == 'f' || (steamdeck && ch == 'e'))
        {
            char ftmp[80];

            strnfmt(ftmp, sizeof(ftmp), "%s.txt", op_ptr->base_name);

            if (term_get_string("File name: ", ftmp, sizeof(ftmp)))
            {
                if (ftmp[0] && (ftmp[0] != ' '))
                {
                    if (file_character(ftmp, false))
                    {
                        msg_print("Character dump failed!");
                    }
                    else
                    {
                        msg_print("Character dump successful.");
                    }
                }
            }
        }

        /* Tutorial / Help - '?' or RS Right */
        else if (ch == '?' || (steamdeck && ch == steamdeck_info_key()))
        {
            display_character_tutorial();
        }

        /* Oops */
        else
        {
            bell("Illegal command for character sheet!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_load();

    /* Force redraw after screen restore if skills/abilities were changed */
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
    handle_stuff();
}

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
 *   � Marks the item cursed
 *   � Gives it random negative modifiers
 *   Compatible with SIL-QH object_type (no flags1/2/3 fields)
 * ------------------------------------------------------------------ */
void add_random_curse(object_type *o_ptr)
{
    /* 1. make it show up as {cursed} right away */
    o_ptr->ident |= IDENT_CURSED;

    /* 2. negative pval / attack / evasion */
    int old_pval = o_ptr->pval;
    if (o_ptr->pval > 0)  o_ptr->pval = -(rand_int(3) + 1); /* �1 � �3 */
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

#define MAIN_MENU_RETURN 1
#define MAIN_MENU_CHARACTER 2
#define MAIN_MENU_KNOWLEDGE 3
#define MAIN_MENU_QUEST_STATUS 4
#define MAIN_MENU_SCORES 5
#define MAIN_MENU_NOTE 6
#define MAIN_MENU_MAP 7
#define MAIN_MENU_MESSAGES 8
#define MAIN_MENU_SCREENSHOT 9
#define MAIN_MENU_STORY 10
#define MAIN_MENU_OPTIONS 11
#define MAIN_MENU_HELP 12
#define MAIN_MENU_ABORT 13
#define MAIN_MENU_SAVE 14
#define MAIN_MENU_SAVE_QUIT 15

#define MAIN_MENU_MAX 16

static int main_menu_calc_width(void)
{
    /* Keep in sync with the strings printed in main_menu_aux(). */
    static const char* lines[] = {
        "Character sheet      (c)",
        "Known lore           (a)",
        "Quest status         (t)",
        "Halls of Mandos      (d)",
        "Run history          (v)",
        "Map                  (m)",
        "Log                  (l)",
        "Combat history       (x)",
        "Hint messages        (i)",
        "The story so far     (y)",
        "Options and misc     (o)",
        "Help                 (h)",
        "Suicide              (k)",
        "Save                 (s)",
        "Quit with save       (q)",
        "Return to game       (r)",
    };

    int max_w = 0;
    for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++)
    {
        int w = (int)strlen(lines[i]);
        if (w > max_w)
            max_w = w;
    }
    return max_w;
}

static void do_cmd_hint_messages(bool* out_pending_look, int* out_look_y,
    int* out_look_x);

/*
 * Performs the interface and selection work for the main menu.
 */
int main_menu_aux(int* highlight)
{
    char ch;
    int i;
    bool death_view = death_spectator_active();

    int menu_w = main_menu_calc_width();
    const int top_pad = 1;
    const int bottom_pad = (Term && (Term->hgt <= 18)) ? 0 : 1;
    const int row_first = top_pad;
    int menu_h = MAIN_MENU_MAX + top_pad + bottom_pad;
    int col_main = 0;
    int row_top = 0;
    if (Term)
    {
        col_main = (Term->wid - menu_w) / 2;
        if (col_main < 0)
            col_main = 0;

        /* Keep the menu fixed vertically.
         * At height 20, start at row 0 so all menu rows fit.
         * Otherwise keep row 0 for message bar and start menu at row 1. */
        if (Term->hgt <= 18)
            row_top = 0;
        else
            row_top = (Term->hgt > 1) ? 1 : 0;
    }

    if (death_view && (*highlight >= 13) && (*highlight <= 15))
        *highlight = 16;

    for (i = 0; i < menu_h; i++)
    {
        int y = row_top + i;
        if (!Term || y < 0 || y >= Term->hgt)
            continue;

        int clear_x = col_main - 2;
        if (clear_x < 0)
            clear_x = 0;
        int clear_w = menu_w + 4;
        if (clear_x + clear_w > Term->wid)
            clear_w = Term->wid - clear_x;
        if (clear_w > 0)
            Term_erase(clear_x, y, clear_w);
    }

    Term_putstr(col_main, row_top + row_first + 0, -1,
        (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE,
        "Character sheet      (c)");
    Term_putstr(col_main, row_top + row_first + 1, -1,
        (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE,
        "Known lore           (a)");
    Term_putstr(col_main, row_top + row_first + 2, -1,
        (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE,
        "Quest status         (t)");
    Term_putstr(col_main, row_top + row_first + 3, -1,
        (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE,
        "Halls of Mandos      (d)");
    Term_putstr(col_main, row_top + row_first + 4, -1,
        (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE,
        "Run history          (v)");
    Term_putstr(col_main, row_top + row_first + 5, -1,
        (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE,
        "Map                  (m)");
    Term_putstr(col_main, row_top + row_first + 6, -1,
        (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE,
        "Log                  (l)");
    Term_putstr(col_main, row_top + row_first + 7, -1,
        (*highlight == 8) ? TERM_L_BLUE : TERM_WHITE,
        "Combat history       (x)");
    Term_putstr(col_main, row_top + row_first + 8, -1,
        (*highlight == 9) ? TERM_L_BLUE : TERM_WHITE,
        "Hint messages        (i)");
    Term_putstr(col_main, row_top + row_first + 9, -1,
        (*highlight == 10) ? TERM_L_BLUE : TERM_WHITE,
        "The story so far     (y)");
    Term_putstr(col_main, row_top + row_first + 10, -1,
        (*highlight == 11) ? TERM_L_BLUE : TERM_WHITE,
        "Options and misc     (o)");
    Term_putstr(col_main, row_top + row_first + 11, -1,
        (*highlight == 12) ? TERM_L_BLUE : TERM_WHITE,
        "Help                 (h)");
    byte suicide_color = death_view ? TERM_L_DARK
        : ((*highlight == 13) ? TERM_L_BLUE : TERM_WHITE);
    Term_putstr(col_main, row_top + row_first + 12, -1, suicide_color,
        "Suicide              (k)");
    byte save_color = death_view ? TERM_L_DARK
        : ((*highlight == 14) ? TERM_L_BLUE : TERM_WHITE);
    Term_putstr(col_main, row_top + row_first + 13, -1, save_color,
        "Save                 (s)");
    byte quit_color = death_view ? TERM_L_DARK
        : ((*highlight == 15) ? TERM_L_BLUE : TERM_WHITE);
    Term_putstr(col_main, row_top + row_first + 14, -1, quit_color,
        "Quit with save       (q)");
    Term_putstr(col_main, row_top + row_first + 15, -1,
        (*highlight == 16) ? TERM_L_BLUE : TERM_WHITE,
        "Return to game       (r)");

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    {
        int cursor_y = row_top + row_first + (*highlight - 1);
        if (Term)
        {
            if (cursor_y < 0)
                cursor_y = 0;
            if (cursor_y >= Term->hgt)
                cursor_y = Term->hgt - 1;
        }
        Term_gotoxy(col_main, cursor_y);
    }

    /* Get key (while allowing menu commands) */
    inkey_set_cursor_hidden(true);
    ch = inkey();
    inkey_set_cursor_hidden(false);

    // choose an option by letter - alphabetical mapping (updated for new order)
    switch (ch)
    {
    case 'c':
        *highlight = 1;
        return (*highlight);  // Character sheet
    case 'a':
        *highlight = 2;
        return (*highlight);  // Known lore
    case 't':
        *highlight = 3;
        return (*highlight);  // Quest status
    case 'd':
        *highlight = 4;
        return (*highlight);  // Halls of Mandos
    case 'v':
        *highlight = 5;
        return (*highlight);  // Run history
    case 'm':
        *highlight = 6;
        return (*highlight);  // Map
    case 'l':
        *highlight = 7;
        return (*highlight);  // Log
    case 'x':
        *highlight = 8;
        return (*highlight); // Combat history
    case 'i':
        *highlight = 9;
        return (*highlight); // Hint messages
    case 'y':
        *highlight = 10;
        return (*highlight); // The story so far
    case 'o':
        *highlight = 11;
        return (*highlight); // Options and misc
    case 'h':
        *highlight = 12;
        return (*highlight); // Help
    case 'k':
        if (death_view) {
            msg_print("You can no longer take that action.");
            break;
        }
        *highlight = 13;
        return (*highlight); // Suicide
    case 's':
        if (death_view) {
            msg_print("You can no longer take that action.");
            break;
        }
        *highlight = 14;
        return (*highlight); // Save
    case 'q':
        if (death_view) {
            msg_print("You can no longer take that action.");
            break;
        }
        *highlight = 15;
        return (*highlight); // Quit with save
    case 'r':
        *highlight = 16;
        return (*highlight); // Return to game
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = MAIN_MENU_MAX;
        while (death_view && (*highlight >= 13) && (*highlight <= 15))
        {
            if (*highlight > 1)
                (*highlight)--;
            else
                *highlight = MAIN_MENU_MAX;
        }
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < MAIN_MENU_MAX)
            (*highlight)++;
        else if (*highlight == MAIN_MENU_MAX)
            *highlight = 1;
        while (death_view && (*highlight >= 13) && (*highlight <= 15))
        {
            if (*highlight < MAIN_MENU_MAX)
                (*highlight)++;
            else
                *highlight = 1;
        }
    }

    /* Leave menu */
    if ((ch == ESCAPE) || (ch == '4'))
    {
        return (-1);
    }

    return (0);
}

/*
 * Brings up a menu for choosing some of the game's more abstruse options.
 */
void do_cmd_main_menu(void)
{
    int actiontype = -1;
    int highlight = 1;
    bool leave_menu = false;
    bool pending_hint_look = false;
    int pending_hint_look_y = -1;
    int pending_hint_look_x = -1;

    /* Clear any active banner before opening main menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        actiontype = main_menu_aux(&highlight);

        if (death_spectator_active() && (actiontype >= 13) && (actiontype <= 15))
        {
            msg_print("You can no longer take that action.");
            continue;
        }

        // if an action has been selected...
        switch (actiontype)
        {
        case 1: // Character sheet (c)
        {
            do_cmd_character_sheet();
            leave_menu = true;
            break;
        }
        case 2: // Known lore (a)
        {
            do_cmd_knowledge_browser_page(g_knowledge_last_page);
            leave_menu = true;
            break;
        }
        case 3: // Quest status (t)
        {
            do_cmd_quest_status();
            leave_menu = true;
            break;
        }
        case 4: // Halls of Mandos (d)
        {
            log_info("main menu: opening Halls of Mandos view");
            show_scores_interactive(true);
            leave_menu = true;
            break;
        }
        case 5: // Run history (v)
        {
            do_cmd_run_history();
            leave_menu = true;
            break;
        }
        case 6: // Map (m)
        {
            do_cmd_view_map();
            leave_menu = true;
            break;
        }
        case 7: // Log (l)
        {
            do_cmd_messages();
            leave_menu = true;
            break;
        }
        case 8: // Combat history (x)
        {
            do_cmd_combat_history();
            leave_menu = true;
            break;
        }
        case 9: // Hint messages (i)
        {
            do_cmd_hint_messages(&pending_hint_look, &pending_hint_look_y,
                &pending_hint_look_x);
            leave_menu = true;
            break;
        }
        case 10: // The story so far (y)
        {
            /* Save screen before showing story */
            screen_save();
            print_story(15, 1);
            /* Load screen after story */
            screen_load();
            leave_menu = true;
            break;
        }
        case 11: // Options and misc (o)
        {
            do_cmd_options();
            leave_menu = true;
            break;
        }
        case 12: // Help (h)
        {
            do_cmd_help();
            leave_menu = true;
            break;
        }
        case 13: // Suicide (k)
        {
            do_cmd_suicide();
            leave_menu = true;
            break;
        }
        case 14: // Save (s)
        {
            do_cmd_save_game();
            leave_menu = true;
            break;
        }
        case 15: // Quit with save (q)
        {
            do_cmd_save_game();

            /* Stop playing */
            p_ptr->playing = false;

            /* Mark that we want to quit to menu, not exit application */
            p_ptr->quit_to_menu = true;

            /* Leaving */
            p_ptr->leaving = true;
            leave_menu = true;
            break;
        }
        case 16: // Return to game (r)
        {
            leave_menu = true;
            break;
        }
        case -1:
        {
            leave_menu = true;
            break;
        }
        default:
        {
            /* Invalid selection - stay in menu */
            break;
        }
        }
    }

    /* Load screen */
    screen_load();

    if (pending_hint_look)
    {
        do_cmd_redraw();
        do_cmd_look_at(pending_hint_look_y, pending_hint_look_x);
    }

}

/*
 * Recall the most recent message
 */
void do_cmd_message_one(void)
{
    /* Recall one message XXX XXX XXX */
    c_prt(message_color(0), format("> %s", message_str(0)), 0, 0);
}

static bool hint_message_has_source(const hint_message_meta* meta)
{
    return meta && meta->source_y >= 0 && meta->source_x >= 0
        && meta->source_y < p_ptr->cur_map_hgt && meta->source_x < p_ptr->cur_map_wid;
}

static bool hint_message_is_word_boundary(char ch)
{
    return (ch == '\0') || !isalnum((unsigned char)ch);
}

static bool hint_message_phrase_matches(const char* line, int offset, const char* phrase)
{
    size_t len;

    if (!line || !phrase || !phrase[0])
        return false;

    len = strlen(phrase);
    if (strncmp(line + offset, phrase, len) != 0)
        return false;

    if (offset > 0 && !hint_message_is_word_boundary(line[offset - 1]))
        return false;

    return hint_message_is_word_boundary(line[offset + len]);
}

static int hint_message_match_length(const char* line, int offset,
    const hint_message_meta* meta, byte* out_attr)
{
    int best_len = 0;
    byte best_attr = TERM_WHITE;

    if (!meta)
        return 0;

    for (int cue = 0; cue < meta->cue_count; ++cue)
    {
        const char* dist = meta->cue_dists[cue];
        const char* dir = meta->cue_dirs[cue];

        if (hint_message_phrase_matches(line, offset, dist))
        {
            int len = (int)strlen(dist);
            if (len > best_len)
            {
                best_len = len;
                best_attr = TERM_YELLOW;
            }
        }

        if (hint_message_phrase_matches(line, offset, dir))
        {
            int len = (int)strlen(dir);
            if (len > best_len)
            {
                best_len = len;
                best_attr = TERM_L_BLUE;
            }
        }
    }

    if (out_attr)
        *out_attr = best_attr;

    return best_len;
}

static void hint_message_put_segment(int row, int col, byte attr, const char* text)
{
    if (!text || !text[0])
        return;

    if (sdl_is_story_font_enabled())
        story_print_text(row, col, 0, attr, text);
    else
        Term_putstr(col, row, -1, attr, text);
}

static void hint_message_draw_colored_line(int row, int col, byte base_attr,
    const char* line, const hint_message_meta* meta)
{
    int start = 0;
    int cursor = col;
    int len;

    if (!line)
        line = "";

    len = (int)strlen(line);
    for (int i = 0; i < len; )
    {
        byte match_attr = base_attr;
        int match_len = hint_message_match_length(line, i, meta, &match_attr);
        if (match_len > 0)
        {
            if (i > start)
            {
                char plain[100];
                int plain_len = i - start;
                memcpy(plain, line + start, plain_len);
                plain[plain_len] = '\0';
                hint_message_put_segment(row, cursor, base_attr, plain);
                cursor += plain_len;
            }

            {
                char special[HINT_MESSAGE_CUE_TEXT_MAX + 1];
                memcpy(special, line + i, match_len);
                special[match_len] = '\0';
                hint_message_put_segment(row, cursor, match_attr, special);
            }

            cursor += match_len;
            i += match_len;
            start = i;
        }
        else
        {
            ++i;
        }
    }

    if (start < len)
    {
        char tail[100];
        int tail_len = len - start;
        memcpy(tail, line + start, tail_len);
        tail[tail_len] = '\0';
        hint_message_put_segment(row, cursor, base_attr, tail);
    }
}

static const char* hint_message_title(int index)
{
    byte line_count = hint_messages_message_line_count(index);
    for (int li = 0; li < line_count; ++li)
    {
        const char* line = hint_messages_message_line(index, li);
        if (line && line[0])
            return line;
    }

    return "";
}

static void hint_message_build_title(char* buf, size_t buf_sz, const char* title,
    int max_len)
{
    if (!buf || buf_sz == 0)
        return;

    if (!title)
        title = "";

    if (max_len < 4 || (int)strlen(title) <= max_len)
    {
        strnfmt(buf, buf_sz, "%s", title);
        return;
    }

    strnfmt(buf, buf_sz, "%.*s...", max_len - 3, title);
}

static void hint_message_draw_list_row(int row, int idx, bool selected, int wid)
{
    hint_message_meta meta;
    char prefix[8];
    char title_buf[96];
    const char* title = hint_message_title(idx);
    byte prefix_attr = selected ? TERM_L_BLUE : TERM_WHITE;
    byte title_attr = selected ? TERM_L_WHITE : TERM_WHITE;
    byte chrome_attr = TERM_SLATE;
    int col = 0;
    int title_room;

    hint_messages_message_meta(idx, &meta);

    Term_erase(0, row, 255);

    strnfmt(prefix, sizeof(prefix), "%2d) ", idx + 1);
    Term_putstr(col, row, -1, prefix_attr, prefix);
    col += (int)strlen(prefix);

    title_room = MAX(8, wid - col - 1);
    if (meta.cue_count > 0)
        title_room = MIN(title_room, MAX(wid / 2, 24));
    hint_message_build_title(title_buf, sizeof(title_buf), title, title_room);
    Term_putstr(col, row, -1, title_attr, title_buf);
    col += (int)strlen(title_buf);

    if (meta.cue_count <= 0 || col >= wid - 4)
        return;

    Term_putstr(col, row, -1, chrome_attr, " [");
    col += 2;

    for (int cue = 0; cue < meta.cue_count && col < wid - 1; ++cue)
    {
        if (cue > 0)
        {
            Term_putstr(col, row, -1, chrome_attr, "; ");
            col += 2;
        }

        if (meta.cue_dists[cue][0])
        {
            Term_putstr(col, row, -1, TERM_YELLOW, meta.cue_dists[cue]);
            col += (int)strlen(meta.cue_dists[cue]);
        }

        if (meta.cue_dists[cue][0] && meta.cue_dirs[cue][0] && col < wid - 1)
        {
            Term_putstr(col, row, -1, chrome_attr, " ");
            col += 1;
        }

        if (meta.cue_dirs[cue][0] && col < wid - 1)
        {
            Term_putstr(col, row, -1, TERM_L_BLUE, meta.cue_dirs[cue]);
            col += (int)strlen(meta.cue_dirs[cue]);
        }
    }

    if (col < wid - 1)
        Term_putstr(col, row, -1, chrome_attr, "]");
}

static bool hint_message_show_internal(int index, int* look_y, int* look_x,
    bool manage_screen)
{
    int wid = 80;
    int hgt = 24;
    int row = 4;
    int col = 8;
    char ch;
    hint_message_meta meta;
    byte line_count;
    bool request_look = false;

    hint_messages_ensure_level_state();
    line_count = hint_messages_message_line_count(index);
    if (!line_count)
        return false;

    hint_messages_message_meta(index, &meta);

    if (manage_screen)
        screen_save();

    sdl_story_font_enable();

    while (1)
    {
        Term_clear();
        Term_get_size(&wid, &hgt);

        for (int li = 0; li < line_count && row + li < hgt - 1; ++li)
        {
            const char* line = hint_messages_message_line(index, li);
            byte base_attr = (li == 0) ? TERM_L_WHITE : TERM_WHITE;
            hint_message_draw_colored_line(row + li, col, base_attr, line,
                (li == 0) ? NULL : &meta);
        }

        if (hint_message_has_source(&meta))
        {
            prt("[Press any key to continue, or 'l' to look at the skeleton]",
                hgt - 1, 0);
        }
        else
        {
            prt("[Press any key to continue]", hgt - 1, 0);
        }

        Term_fresh();

        inkey_set_cursor_hidden(true);
        ch = inkey();
        inkey_set_cursor_hidden(false);

        if ((ch == 'l' || ch == 'L') && hint_message_has_source(&meta))
        {
            if (look_y)
                *look_y = meta.source_y;
            if (look_x)
                *look_x = meta.source_x;
            request_look = true;
            break;
        }

        break;
    }

    sdl_story_font_disable();
    if (manage_screen)
        screen_load();

    return request_look;
}

void show_hint_message_screen(int index)
{
    int look_y = -1;
    int look_x = -1;

    if (hint_message_show_internal(index, &look_y, &look_x, true))
    {
        do_cmd_redraw();
        do_cmd_look_at(look_y, look_x);
    }
}

static void do_cmd_hint_messages(bool* out_pending_look, int* out_look_y,
    int* out_look_x)
{
    char ch;

    int wid, hgt;
    bool pending_look = false;
    int look_y = -1;
    int look_x = -1;

    /* Clear any active banner before opening hint messages */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    hint_messages_ensure_level_state();

    int n = (int)hint_messages_count_for_save();
    if (n <= 0)
    {
        msg_print("You recall no hint messages on this level.");
        return;
    }

    int sel = 0;
    int top = 0;

    Term_get_size(&wid, &hgt);

    /* Save screen */
    screen_save();

    while (1)
    {
        Term_clear();

        int rows = hgt - 4;
        if (rows < 1)
            rows = 1;

        if (sel < 0)
            sel = 0;
        if (sel >= n)
            sel = n - 1;

        if (sel < top)
            top = sel;
        if (sel >= top + rows)
            top = sel - rows + 1;
        if (top < 0)
            top = 0;
        if (top > n - rows)
            top = n - rows;
        if (top < 0)
            top = 0;

        prt(format("Hint Messages (%d)", n), 0, 0);
        prt("[Press '8'/'2' to move, Enter to read, 'l' to look, or ESCAPE]",
            hgt - 1, 0);

        for (int row = 0; row < rows && top + row < n; ++row)
        {
            int idx = top + row;
            hint_message_draw_list_row(2 + row, idx, idx == sel, wid);
        }

        Term_fresh();
        ch = inkey();

        if (ch == ESCAPE)
            break;

        if (ch == '8')
        {
            sel = (sel > 0) ? (sel - 1) : (n - 1);
            continue;
        }

        if (ch == '2')
        {
            sel = (sel + 1 < n) ? (sel + 1) : 0;
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
        {
            int selected_look_y = -1;
            int selected_look_x = -1;

            if (hint_message_show_internal(sel, &selected_look_y, &selected_look_x, false))
            {
                pending_look = true;
                look_y = selected_look_y;
                look_x = selected_look_x;
                break;
            }
            continue;
        }

        if (ch == 'l' || ch == 'L')
        {
            hint_message_meta meta;
            hint_messages_message_meta(sel, &meta);
            if (hint_message_has_source(&meta))
            {
                pending_look = true;
                look_y = meta.source_y;
                look_x = meta.source_x;
                break;
            }

            bell(NULL);
            continue;
        }

        bell(NULL);
    }

    /* Load screen */
    screen_load();

    if (out_pending_look)
        *out_pending_look = pending_look;
    if (out_look_y)
        *out_look_y = look_y;
    if (out_look_x)
        *out_look_x = look_x;
}

/*
 * Show previous messages to the user
 *
 * The screen format uses line 0 and 23 for headers and prompts,
 * skips line 1 and 22, and uses line 2 thru 21 for old messages.
 *
 * This command shows you which commands you are viewing, and allows
 * you to "search" for strings in the recall.
 *
 * Note that messages may be longer than 80 characters, but they are
 * displayed using "infinite" length, with a special sub-command to
 * "slide" the virtual display to the left or right.
 *
 * Attempt to only hilite the matching portions of the string.
 */
void do_cmd_messages(void)
{
    char ch;

    int i, j, n, q;
    int wid, hgt;

    char shower[80];
    char finder[80];

    /* Clear any active banner before opening message history */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Wipe finder */
    SDL_strlcpy(finder, "", sizeof(finder));

    /* Wipe shower */
    SDL_strlcpy(shower, "", sizeof(shower));

    /* Total messages */
    n = message_num();

    /* Start on first message */
    i = 0;

    /* Start at leftmost edge */
    q = 0;

    /* Get size */
    Term_get_size(&wid, &hgt);

    /* Save screen */
    screen_save();

    /* Process requests until done */
    while (1)
    {
        /* Clear screen */
        Term_clear();

        /* Dump messages */
        for (j = 0; (j < hgt - 4) && (i + j < n); j++)
        {
            cptr msg = message_str((s16b)(i + j));
            byte attr = message_color((s16b)(i + j));

            /* Apply horizontal scroll */
            msg = ((int)strlen(msg) >= q) ? (msg + q) : "";

            /* Dump the messages, bottom to top */
            Term_putstr(0, hgt - 3 - j, -1, attr, msg);

            /* Hilite "shower" */
            if (shower[0])
            {
                cptr str = msg;

                /* Display matches */
                while ((str = strstr(str, shower)) != NULL)
                {
                    int len = strlen(shower);

                    /* Display the match */
                    Term_putstr(
                        str - msg, hgt - 3 - j, len, TERM_YELLOW, shower);

                    /* Advance */
                    str += len;
                }
            }
        }

        /* Display header XXX XXX XXX */
        prt(format(
                "Message Recall (%d-%d of %d), Offset %d", i, i + j - 1, n, q),
            0, 0);

        /* Display prompt (not very informative) */
        prt("[Press 'p' for older, 'n' for newer, ..., or ESCAPE]", hgt - 1, 0);

        /* Get a command */
        ch = inkey();

        /* Exit on Escape */
        if (ch == ESCAPE)
            break;

        /* Hack -- Save the old index */
        j = i;

        /* Horizontal scroll */
        if (ch == '4')
        {
            /* Scroll left */
            q = (q >= wid / 2) ? (q - wid / 2) : 0;

            /* Success */
            continue;
        }

        /* Horizontal scroll */
        if (ch == '6')
        {
            /* Scroll right */
            q = q + wid / 2;

            /* Success */
            continue;
        }

        /* Hack -- handle show */
        if (ch == '=')
        {
            /* Prompt */
            prt("Show: ", hgt - 1, 0);

            /* Get a "shower" string, or continue */
            if (!askfor_aux(shower, sizeof(shower)))
                continue;

            /* Okay */
            continue;
        }

        /* Hack -- handle find */
        if (ch == '/')
        {
            s16b z;

            /* Prompt */
            prt("Find: ", hgt - 1, 0);

            /* Get a "finder" string, or continue */
            if (!askfor_aux(finder, sizeof(finder)))
                continue;

            /* Show it */
            SDL_strlcpy(shower, finder, sizeof(shower));

            /* Scan messages */
            for (z = i + 1; z < n; z++)
            {
                cptr msg = message_str(z);

                /* Search for it */
                if (strstr(msg, finder))
                {
                    /* New location */
                    i = z;

                    /* Done */
                    break;
                }
            }
        }

        /* Recall 20 older messages */
        if ((ch == 'p') || (ch == KTRL('P')) || (ch == ' '))
        {
            /* Go older if legal */
            if (i + 20 < n)
                i += 20;
        }

        /* Recall 10 older messages */
        if (ch == '+')
        {
            /* Go older if legal */
            if (i + 10 < n)
                i += 10;
        }

        /* Recall 1 older message */
        if ((ch == '8') || (ch == '\n') || (ch == '\r'))
        {
            /* Go newer if legal */
            if (i + 1 < n)
                i += 1;
        }

        /* Recall 20 newer messages */
        if ((ch == 'n') || (ch == KTRL('N')))
        {
            /* Go newer (if able) */
            i = (i >= 20) ? (i - 20) : 0;
        }

        /* Recall 10 newer messages */
        if (ch == '-')
        {
            /* Go newer (if able) */
            i = (i >= 10) ? (i - 10) : 0;
        }

        /* Recall 1 newer messages */
        if (ch == '2')
        {
            /* Go newer (if able) */
            i = (i >= 1) ? (i - 1) : 0;
        }

        /* Hack -- Error of some kind */
        if (i == j)
            bell(NULL);
    }

    /* Load screen */
    screen_load();
}

/*
 * Ask for a "user pref line" and process it
 */
void do_cmd_pref(void)
{
    char tmp[80];

    /* Default */
    SDL_strlcpy(tmp, "", sizeof(tmp));

    /* Ask for a "user pref command" */
    if (!term_get_string("Pref: ", tmp, sizeof(tmp)))
        return;

    /* Process that pref command */
    (void)process_pref_file_command(tmp);
}

/*
 * Ask for a "user pref file" and process it.
 *
 * This function should only be used by standard interaction commands,
 * in which a standard "Command:" prompt is present on the given row.
 *
 * Allow absolute file names?  XXX XXX XXX
 */
static void do_cmd_pref_file_hack(int row)
{
    char ftmp[80];

    /* Prompt */
    Term_putstr(2, row + 2, -1, TERM_SLATE, "(Escape to cancel)");

    /* Prompt */
    prt("File: ", row, 2);

    /* Default filename */
    strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

    /* Ask for a file (or cancel) */
    if (!askfor_aux(ftmp, sizeof(ftmp)))
        return;

    /* Process the given filename */
    if (process_pref_file(ftmp))
    {
        /* Mention failure */
        msg_format("Failed to load '%s'!", ftmp);
    }
    else
    {
        /* Mention success */
        msg_format("Loaded '%s'.", ftmp);
    }
}

void clear_skills_and_abilities()
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
    { 16, "Debug" },
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

static int settings_ui_term_wid(void)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    return wid;
}

static int settings_ui_line_width(int col)
{
    int width = settings_ui_term_wid() - col;

    if (width < 1)
        width = 1;

    return width;
}

static void settings_ui_fit_text(char* buf, size_t buflen, cptr text,
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

static cptr settings_ui_pick_label(int max_chars, cptr long_label,
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

static void settings_ui_format_pair_line(char* buf, size_t buflen, cptr label,
    cptr value, int max_chars, int min_value_chars)
{
    char label_buf[128];
    char value_buf[96];
    int desired_value;
    int value_budget;
    int label_budget;

    if (!buflen)
        return;

    if (!label)
        label = "";
    if (!value)
        value = "";

    if (max_chars <= 0)
    {
        buf[0] = '\0';
        return;
    }

    if (!value[0])
    {
        settings_ui_fit_text(buf, buflen, label, max_chars);
        return;
    }

    desired_value = (int)strlen(value);
    value_budget = MIN(max_chars - 4,
        MAX(min_value_chars, MIN(desired_value, (max_chars * 3) / 5)));

    if (value_budget < 1)
        value_budget = MIN(max_chars, MAX(1, max_chars / 2));

    settings_ui_fit_text(value_buf, sizeof(value_buf), value, value_budget);
    label_budget = max_chars - (int)strlen(value_buf) - 2;

    if (label_budget < 4)
    {
        settings_ui_fit_text(buf, buflen, value, max_chars);
        return;
    }

    settings_ui_fit_text(label_buf, sizeof(label_buf), label, label_budget);
    strnfmt(buf, buflen, "%s: %s", label_buf, value_buf);
}

static void settings_ui_put_fitted(int row, int col, byte attr, cptr text)
{
    char buf[160];
    int width = settings_ui_line_width(col);

    settings_ui_fit_text(buf, sizeof(buf), text, width);
    Term_putstr(col, row, width, attr, buf);
}

static void settings_ui_format_field(char* buf, size_t buflen, cptr text,
    bool selected)
{
    if (!buf || !buflen)
        return;

    if (!text)
        text = "";

    if (selected)
        strnfmt(buf, buflen, "[%s]", text);
    else
        SDL_strlcpy(buf, text, buflen);
}

static void settings_ui_format_auto_value(char* buf, size_t buflen, int value,
    int max_chars)
{
    char raw_buf[16];
    char auto_long[16];
    char auto_short[8];

    if (!buf || !buflen)
        return;

    if (value > 0)
    {
        strnfmt(raw_buf, sizeof(raw_buf), "%d", value);
        settings_ui_fit_text(buf, buflen, raw_buf, max_chars);
        return;
    }

    SDL_strlcpy(auto_long, "auto", sizeof(auto_long));
    SDL_strlcpy(auto_short, "a", sizeof(auto_short));
    settings_ui_fit_text(buf, buflen,
        settings_ui_pick_label(max_chars, auto_long, auto_long, auto_short),
        max_chars);
}

static bool option_menu_use_compact_layout(void)
{
    return Term && (Term->wid > 0) && (Term->wid <= 60);
}

static bool option_menu_use_narrow_layout(void)
{
    return Term && (Term->wid > 0) && (Term->wid <= 50);
}

static int option_menu_max_line_chars(void)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    /* Options start at column 4; keep one cell free for the cursor. */
    wid -= 5;

    if (wid < 8)
        wid = 8;

    return wid;
}

static void option_menu_fit_text(char* buf, size_t buflen, cptr text,
    int max_chars)
{
    settings_ui_fit_text(buf, buflen, text, max_chars);
}

static cptr sound_option_label(int index)
{
    bool compact = option_menu_use_compact_layout();
    bool narrow = option_menu_use_narrow_layout();

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

static cptr option_menu_label(int opt)
{
    bool compact = option_menu_use_compact_layout();
    bool narrow = option_menu_use_narrow_layout();

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
    case OPT_show_level_entry_banner:
        return compact ? (narrow ? "Entry text" : "Entry narrative")
                       : "Level entry narrative";
    case OPT_show_partition_narrative:
        return compact ? (narrow ? "Partition text" : "Partition narrative")
                       : "Partition transition narrative";
    case OPT_ability_desc_mode:
        return compact ? (narrow ? "Ability text" : "Ability descriptions")
                       : "Ability descriptions (0=lore+effect, 1=effect+lore, 2=effect)";
    case OPT_vault_drop_frequency:
        return compact ? "Vault drops" : "Vault drop frequency";
    case OPT_noble_item_spawn_mode:
        return compact ? (narrow ? "Noble items" : "Noble item sources")
                       : "Noble item spawns";
    case OPT_look_objects_sort_by_difficulty:
        return compact ? (narrow ? "Look diff sort" : "Look sort by diff")
                       : "Sort look (L) objects by difficulty only";
    case OPT_intro_style:
        return compact ? (narrow ? "Welcome art" : "Welcome screen")
                       : "Welcome screen style";
    case OPT_banner_message_stairs:
        return compact ? "Banner layout" : "Banner message layout";
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

static void option_menu_format_line(char* buf, size_t buflen, cptr label,
    cptr value)
{
    if (!option_menu_use_compact_layout())
    {
        strnfmt(buf, buflen, "%-48s: %s", label, value);
    }
    else
    {
        char label_buf[96];
        char value_buf[48];
        int max_chars = option_menu_max_line_chars();
        int value_len;
        int label_budget;

        option_menu_fit_text(value_buf, sizeof(value_buf), value, max_chars);
        value_len = (int)strlen(value_buf);

        if (value_len <= 0)
        {
            option_menu_fit_text(buf, buflen, label, max_chars);
            return;
        }

        label_budget = max_chars - value_len - 2;
        if (label_budget <= 0)
        {
            option_menu_fit_text(buf, buflen, value_buf, max_chars);
            return;
        }

        option_menu_fit_text(label_buf, sizeof(label_buf), label, label_budget);
        strnfmt(buf, buflen, "%s: %s", label_buf, value_buf);
    }
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

    char buf[160];

    int dir;
    
    bool is_sound_page = (page == SOUND_PAGE);
    bool app_page = option_page_uses_app_config(page);
    bool metarun_page = !app_page && !is_sound_page;
    bool app_settings_dirty = false;
    bool metarun_settings_dirty = false;
    bool sound_settings_dirty = false;
    const struct option_group_marker* groups = get_option_groups_for_page(page);
    struct sound_config* sound_cfg = sdl_sound_get_config();

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
        int first_row = 3;
        int footer_rows = (page == CHALLENGE_PAGE) ? 4 : 2;
        int visible_rows = Term->hgt - footer_rows - first_row;
        int total_rows = n + option_group_total_rows(groups);
        int selected_display_row = k + option_group_count_before(groups, k);
        int group_index = 0;
        int display_row = 0;
        int max_scroll;

        if (visible_rows < 1)
            visible_rows = 1;

        max_scroll = total_rows - visible_rows;
        if (max_scroll < 0)
            max_scroll = 0;

        if (selected_display_row < scroll)
            scroll = selected_display_row;
        else if (selected_display_row >= scroll + visible_rows)
            scroll = selected_display_row - visible_rows + 1;
        if (scroll > max_scroll)
            scroll = max_scroll;

        Term_clear();

        /* Prompt XXX XXX XXX */
        strnfmt(buf, sizeof(buf), "%s", info);
        settings_ui_put_fitted(1, 2, TERM_WHITE, buf);

        /* Display the options */
        for (i = 0; i < n; i++)
        {
            byte a = TERM_WHITE;
            int row;

            while (groups && groups[group_index].before_index == i)
            {
                row = first_row + display_row - scroll;
                if (row >= first_row && row < first_row + visible_rows)
                    Term_putstr(2, row, -1, TERM_SLATE, groups[group_index].label);
                display_row++;
                group_index++;
            }

            /* Color current option */
            if (i == k)
                a = TERM_L_BLUE;

            /* Display the option text */
            buf[0] = '\0';
            if (is_sound_page)
            {
                char value_str[32];

                if (i == 0)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enabled ? "yes" : "no ");
                }
                else if (i == 1)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_combat ? "yes" : "no ");
                }
                else if (i == 2)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_inventory ? "yes" : "no ");
                }
                else if (i == 3)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_walk ? "yes" : "no ");
                }
                else if (i == 4)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->enable_doors ? "yes" : "no ");
                }
                else if (i == 5)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_combat * 100.0f);
                }
                else if (i == 6)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_inventory * 100.0f);
                }
                else if (i == 7)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_walk * 100.0f);
                }
                else if (i == 8)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_doors * 100.0f);
                }
                else if (i == 9)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->volume_other * 100.0f);
                }
                else if (i == 10)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->music_main_enabled ? "yes" : "no ");
                }
                else if (i == 11)
                {
                    strnfmt(value_str, sizeof(value_str), "%s",
                        sound_cfg->music_ambient_enabled ? "yes" : "no ");
                }
                else if (i == 12)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->music_main_volume * 100.0f);
                }
                else if (i == 13)
                {
                    strnfmt(value_str, sizeof(value_str), "%.0f%%",
                        sound_cfg->music_ambient_volume * 100.0f);
                }

                option_menu_format_line(buf, sizeof(buf), sound_option_label(i),
                    value_str);
            }
            else if (opt[i] == OPT_delay_factor)
            {
                char value_str[32];
                strnfmt(value_str, sizeof(value_str), "%d", op_ptr->delay_factor);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_hitpoint_warning)
            {
                char value_str[32];
                strnfmt(value_str, sizeof(value_str), "%d%%",
                    op_ptr->hitpoint_warn * 10);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_hide_left_panel)
            {
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    get_sdl_hide_left_panel() ? "yes" : "no ");
            }
            else if (opt[i] == OPT_main_combat_rolls)
            {
                char value_str[32];
                strnfmt(value_str, sizeof(value_str), "%d",
                    op_ptr->main_combat_rolls);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_show_level_entry_banner)
            {
                const char *mode_str;
                bool compact = option_menu_use_compact_layout();
                switch (op_ptr->level_entry_narrative_mode)
                {
                case LEVEL_ENTRY_NARRATIVE_BANNER:
                    mode_str = compact ? "Banner" : "Banner without delay";
                    break;
                case LEVEL_ENTRY_NARRATIVE_MESSAGE: mode_str = "Message"; break;
                case LEVEL_ENTRY_NARRATIVE_OFF:     mode_str = "Off"; break;
                default:
                    mode_str = compact ? "Banner delay" : "Banner with delay";
                    break;
                }
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_show_partition_narrative)
            {
                const char *mode_str;
                bool compact = option_menu_use_compact_layout();
                switch (op_ptr->partition_narrative_mode)
                {
                case PARTITION_NARRATIVE_BANNER:
                    mode_str = compact ? "Banner" : "Banner without delay";
                    break;
                case PARTITION_NARRATIVE_OFF:     mode_str = "Off"; break;
                default:                          mode_str = "Message"; break;
                }
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_ability_desc_mode)
            {
                const char *mode_str;
                bool compact = option_menu_use_compact_layout();
                switch (op_ptr->ability_desc_mode)
                {
                case 1:  mode_str = compact ? "1 effect+lore" : "1 (effect+lore)"; break;
                case 2:  mode_str = compact ? "2 effect only" : "2 (effect only)"; break;
                default: mode_str = compact ? "0 lore+effect" : "0 (lore+effect)"; break;
                }
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_vault_drop_frequency)
            {
                const char *vdf_names[] = { "Normal", "Modest", "Scarce", "Meager", "Plentiful" };
                char value_str[32];
                byte mode = op_ptr->vault_drop_frequency;
                if (mode > VDF_PLENTIFUL)
                    mode = VDF_NORMAL;
                strnfmt(value_str, sizeof(value_str), "%s (%d)", vdf_names[mode],
                    mode);
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    value_str);
            }
            else if (opt[i] == OPT_noble_item_spawn_mode)
            {
                const char *mode_str
                    = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
                    ? (option_menu_use_compact_layout() ? "1 with vaults" : "1 (also &/! vault drops)")
                    : (option_menu_use_compact_layout() ? "0 restricted" : "0 (good+/chests/human+elf skeletons)");
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    mode_str);
            }
            else if (opt[i] == OPT_intro_style)
            {
                const char *is_names[] = {
                    "Flame Imperishable", "Oath of Feanor",
                    "Twilight of Valinor", "Song of Luthien",
                    "Words of Hurin", "Random"
                };
                byte m = op_ptr->intro_style;
                if (m > INTRO_STYLE_RANDOM) m = INTRO_STYLE_FLAME;
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    is_names[m]);
            }
            else if (opt[i] == OPT_banner_message_stairs)
            {
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    op_ptr->opt[opt[i]] ? "Stair" : "Straight");
            }
            else
            {
                option_menu_format_line(buf, sizeof(buf), option_menu_label(opt[i]),
                    op_ptr->opt[opt[i]] ? "yes" : "no ");
            }

            row = first_row + display_row - scroll;
            if (row >= first_row && row < first_row + visible_rows)
                c_prt(a, buf, row, 4);
            display_row++;
        }

        if (total_rows > visible_rows)
        {
            strnfmt(buf, sizeof(buf), "(scroll: rows %d-%d of %d)",
                scroll + 1, MIN(scroll + visible_rows, total_rows), total_rows);
            settings_ui_put_fitted(Term->hgt - 2, 2, TERM_SLATE, buf);
        }

        if (page == CHALLENGE_PAGE)
        {
            settings_ui_put_fitted(Term->hgt - 4, 2, TERM_L_WHITE,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "Challenge options can only be changed during character creation",
                    "Challenge options only change during character creation",
                    "Challenge options only change at birth"));
            settings_ui_put_fitted(Term->hgt - 3, 2, TERM_L_WHITE,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "or on the very first turn",
                    "or on the first turn",
                    "or on turn 1"));

            if (playerturn == 0)
            {
                settings_ui_put_fitted(Term->hgt - 1, 2, TERM_SLATE,
                    settings_ui_pick_label(settings_ui_line_width(2),
                        "(direction keys to set, Return/Escape to accept)",
                        "(direction keys to set, Enter/Esc to accept)",
                        "(arrows set, Enter/Esc accept)"));
            }
            else
            {
                settings_ui_put_fitted(Term->hgt - 1, 2, TERM_SLATE,
                    settings_ui_pick_label(settings_ui_line_width(2),
                        "(press Return to go back)",
                        "(press Enter to go back)",
                        "(Enter goes back)"));
            }
        }
        else
        {
            settings_ui_put_fitted(Term->hgt - 1, 2, TERM_SLATE,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "(direction keys to set, Return/Escape to accept)",
                    "(direction keys to set, Enter/Esc to accept)",
                    "(arrows set, Enter/Esc accept)"));
        }

        /* Hilite current option */
        move_cursor(first_row + selected_display_row - scroll,
            MIN(54, Term->wid - 1));

        /* Get a key */
        inkey_set_cursor_hidden(true);
        ch = inkey();
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
                sdl_sound_save_config();
                sdl_sound_reload();
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
                    set_sdl_hide_left_panel(!get_sdl_hide_left_panel());
                    sdl_apply_config();
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
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode < LEVEL_ENTRY_NARRATIVE_OFF)
                        ? op_ptr->level_entry_narrative_mode + 1
                        : LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode < PARTITION_NARRATIVE_OFF)
                        ? op_ptr->partition_narrative_mode + 1
                        : PARTITION_NARRATIVE_BANNER;
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
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                    else if (k == 13) {
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume < 1.0f) ? sound_cfg->music_ambient_volume + 0.1f : 1.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
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
                    set_sdl_hide_left_panel(true);
                    sdl_apply_config();
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
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode < LEVEL_ENTRY_NARRATIVE_OFF)
                        ? op_ptr->level_entry_narrative_mode + 1
                        : LEVEL_ENTRY_NARRATIVE_OFF;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode < PARTITION_NARRATIVE_OFF)
                        ? op_ptr->partition_narrative_mode + 1
                        : PARTITION_NARRATIVE_OFF;
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
                        sdl_sound_save_config(); /* Apply volume change immediately */
                    }
                    else if (k == 13) {
                        sound_cfg->music_ambient_volume = (sound_cfg->music_ambient_volume > 0.0f) ? sound_cfg->music_ambient_volume - 0.1f : 0.0f;
                        sdl_sound_save_config(); /* Apply volume change immediately */
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
                    set_sdl_hide_left_panel(false);
                    sdl_apply_config();
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
                else if (opt[k] == OPT_show_level_entry_banner)
                {
                    op_ptr->level_entry_narrative_mode =
                        (op_ptr->level_entry_narrative_mode > LEVEL_ENTRY_NARRATIVE_BANNER_DELAY)
                        ? op_ptr->level_entry_narrative_mode - 1
                        : LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
                }
                else if (opt[k] == OPT_show_partition_narrative)
                {
                    op_ptr->partition_narrative_mode =
                        (op_ptr->partition_narrative_mode > PARTITION_NARRATIVE_BANNER)
                        ? op_ptr->partition_narrative_mode - 1
                        : PARTITION_NARRATIVE_BANNER;
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

/*
 * Write all current options to the given preference file in the
 * lib/user directory. Modified from KAmband 1.8.
 */
static errr option_dump(cptr fname)
{
    static cptr mark = "Options Dump";

    int i, j;

    SDL_IOStream* fff;

    char buf[1024];

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("option_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old options */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = sdl_fopen(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    /* Start dumping */
    SDL_IOprintf(fff, "# Automatic option dump\n\n");

    /* Dump options (skip cheat, adult, score) */
    for (i = 0; i < OPT_CHEAT; i++)
    {
        /* Require a real option */
        if (!option_text[i])
            continue;

        /* Comment */
        SDL_IOprintf(fff, "# Option '%s'\n", option_desc[i]);

        /* Dump the option */
        if (op_ptr->opt[i])
        {
            SDL_IOprintf(fff, "Y:%s\n", option_text[i]);
        }
        else
        {
            SDL_IOprintf(fff, "X:%s\n", option_text[i]);
        }

        /* Skip a line */
        SDL_IOprintf(fff, "\n");
    }

    /* Dump window flags */
    for (i = 1; i < ANGBAND_TERM_MAX; i++)
    {
        /* Require a real window */
        if (!angband_term[i])
            continue;

        /* Check each flag */
        for (j = 0; j < 32; j++)
        {
            /* Require a real flag */
            if (!window_flag_desc[j])
                continue;

            /* Comment */
            SDL_IOprintf(fff, "# Window '%s', Flag '%s'\n", angband_term_name[i],
                window_flag_desc[j]);

            /* Dump the flag */
            if (op_ptr->window_flag[i] & (1L << j))
            {
                SDL_IOprintf(fff, "W:%d:%d:1\n", i, j);
            }
            else
            {
                SDL_IOprintf(fff, "W:%d:%d:0\n", i, j);
            }

            /* Skip a line */
            SDL_IOprintf(fff, "\n");
        }
    }

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

/*
 * Display and manage SDL pane settings
 * Interactive menu to edit SDL configuration
 */
static int get_supporting_pane_config_count(void);
static void do_cmd_supporting_pane_layout_editor(bool* settings_changed);
static void do_cmd_supporting_pane_font_editor(bool* settings_changed);
static void do_cmd_touch_pane_button_editor(bool* settings_changed);
static const char* pane_type_short_name(enum pane_type type);
static void format_font_size_value(char* buf, size_t buflen, int raw, int effective,
    int max_chars)
{
    char long_buf[24];
    char medium_buf[24];
    char short_buf[16];

    if (!buf || !buflen)
        return;

    if (raw > 0)
    {
        strnfmt(long_buf, sizeof(long_buf), "%d", raw);
        settings_ui_fit_text(buf, buflen, long_buf, max_chars);
        return;
    }

    strnfmt(long_buf, sizeof(long_buf), "auto (%d)", effective);
    strnfmt(medium_buf, sizeof(medium_buf), "auto %d", effective);
    strnfmt(short_buf, sizeof(short_buf), "a%d", effective);
    settings_ui_fit_text(buf, buflen,
        settings_ui_pick_label(max_chars, long_buf, medium_buf, short_buf),
        max_chars);
}

static const char* sdl_min_terminal_mode_label(int mode)
{
    return (mode == 1) ? "compact (50x18)" : "normal (80x24)";
}

void do_cmd_pane_settings(void)
{
    int k = 0;
    int n = 11; /* Total number of options */
    bool done = false;
    bool settings_changed = false;
    int dir;
    const char* config_path = get_sdl_config_path();
    const char* config_label = (config_path && config_path[0]) ? config_path : "sil_sdl.json";
    
    /* Save screen */
    screen_save();
    
    while (!done)
    {
        int row_width;
        int label_hint;

        /* Clear screen */
        Term_clear();

        /* Display title */
        settings_ui_put_fitted(1, 2, TERM_WHITE, "SDL Pane Settings");

        /* Display current settings */
        char buf[96];
        char value_buf[32];
        int y0 = 3;
        byte a;
        char font_value[24];
        row_width = settings_ui_line_width(2);
        label_hint = MAX(10, row_width - 12);

        /* Option 0: Main View Scale */
        a = (k == 0) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(value_buf, sizeof(value_buf), "%d", get_sdl_main_view_scale());
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Main View Scale (1-max) [Alt++/-]",
                "Main View Scale [Alt++/-]",
                "View Scale"),
            value_buf, row_width, 3);
        c_prt(a, buf, y0 + 0, 2);

        /* Option 1: Minimum Terminal Size */
        a = (k == 1) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Minimum Terminal Size",
                "Min Terminal Size",
                "Min Terminal"),
            sdl_min_terminal_mode_label(get_sdl_min_terminal_mode()),
            row_width, 10);
        c_prt(a, buf, y0 + 1, 2);

        /* Option 2: Aux View Font Size */
        a = (k == 2) ? TERM_L_BLUE : TERM_WHITE;
        format_font_size_value(font_value, sizeof(font_value),
            get_sdl_aux_view_font_size(), get_sdl_effective_aux_view_font_size(),
            MAX(6, MIN(14, row_width / 2)));
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Default Aux Font Size (0=auto, 8-48)",
                "Default Aux Font (0=auto)",
                "Aux Font"),
            font_value, row_width, 6);
        c_prt(a, buf, y0 + 2, 2);

        /* Option 3: Margin */
        a = (k == 3) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(value_buf, sizeof(value_buf), "%d", get_sdl_margin());
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Margin (0-20)",
                "Margin",
                "Margin"),
            value_buf, row_width, 3);
        c_prt(a, buf, y0 + 3, 2);

        /* Option 4: Fullscreen */
        a = (k == 4) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf), "Fullscreen",
            get_sdl_fullscreen() ? "yes" : "no", row_width, 3);
        c_prt(a, buf, y0 + 4, 2);

        /* Option 5: Tiles */
        a = (k == 5) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf), "Tiles",
            get_sdl_tiles() ? "yes" : "no", row_width, 3);
        c_prt(a, buf, y0 + 5, 2);

        /* Option 6: Enable Side Panes */
        a = (k == 6) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Enable Side Panes [Alt+I]",
                "Side Panes [Alt+I]",
                "Side Panes"),
            get_sdl_enable_right_panes() ? "yes" : "no",
            row_width, 3);
        c_prt(a, buf, y0 + 6, 2);

        /* Option 7: Enable Bottom Panes */
        a = (k == 7) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_format_pair_line(buf, sizeof(buf),
            settings_ui_pick_label(label_hint,
                "Enable Bottom Panes [Alt+L]",
                "Bottom Panes [Alt+L]",
                "Bottom Panes"),
            get_sdl_enable_bottom_panes() ? "yes" : "no",
            row_width, 3);
        c_prt(a, buf, y0 + 7, 2);

        /* Option 8: View Pane Configuration (supporting panes only) */
        a = (k == 8) ? TERM_L_BLUE : TERM_WHITE;
        strnfmt(buf, sizeof(buf), "%s (%d)",
            settings_ui_pick_label(row_width,
                "View Pane Configuration",
                "Pane Configuration",
                "Pane Layout"),
            get_supporting_pane_config_count());
        {
            char fitted_buf[96];
            settings_ui_fit_text(fitted_buf, sizeof(fitted_buf), buf, row_width);
            SDL_strlcpy(buf, fitted_buf, sizeof(buf));
        }
        c_prt(a, buf, y0 + 8, 2);

        /* Option 9: Pane Font Sizes */
        a = (k == 9) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_fit_text(buf, sizeof(buf),
            settings_ui_pick_label(row_width,
                "Pane Font Sizes",
                "Pane Fonts",
                "Pane Fonts"),
            row_width);
        c_prt(a, buf, y0 + 9, 2);

        /* Option 10: Save/Return */
        a = (k == 10) ? TERM_L_BLUE : TERM_WHITE;
        settings_ui_fit_text(buf, sizeof(buf),
            settings_changed ? "Save Changes and Return"
                             : "Return to Options Menu",
            row_width);
        c_prt(a, buf, y0 + 10, 2);

        /* Display help */
        int y = Term->hgt - 3;
        if (settings_changed)
        {
            settings_ui_put_fitted(y++, 2, TERM_YELLOW,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "Settings changed - changes take effect immediately.",
                    "Settings changed - active immediately.",
                    "Changes apply immediately."));
            settings_ui_put_fitted(y++, 2, TERM_YELLOW,
                settings_ui_pick_label(settings_ui_line_width(2),
                    "Will be saved to your SDL config file on exit.",
                    "Saved to your SDL config on exit.",
                    "Saved on exit."));
        }
        settings_ui_put_fitted(y++, 2, TERM_SLATE,
            settings_ui_pick_label(settings_ui_line_width(2),
                "(direction keys to set, 0 = auto font, Return/Escape to accept)",
                "(arrows move, 4/6 or y/n set, 0 auto, Enter/Esc exit)",
                "(arrows move, 4/6 set, 0 auto, Enter/Esc)"));

        /* Get key */
        inkey_set_cursor_hidden(true);
        char ch = inkey();
        inkey_set_cursor_hidden(false);
        
        /* Try to translate the key into a direction */
        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);
        
        /* Process input */
        switch (ch)
        {
        case ESCAPE:
        {
            /* Exit without needing to navigate to the bottom */
            if (settings_changed)
            {
                if (save_pane_config_to_json())
                {
                    msg_format("Settings saved to %s", config_label);
                }
            }
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            /* Enter activates the current option for actions; otherwise accept/exit. */
            if (k == 8) /* Supporting Pane Layout */
            {
                do_cmd_supporting_pane_layout_editor(&settings_changed);
                break;
            }
            if (k == 9) /* Pane Font Sizes */
            {
                do_cmd_supporting_pane_font_editor(&settings_changed);
                break;
            }

            /* Save if changed, then exit */
            if (settings_changed)
            {
                if (save_pane_config_to_json())
                {
                    msg_format("Settings saved to %s", config_label);
                }
            }
            done = true;
            break;
        }
        
        case '-':
        case '8':
        {
            /* Move up */
            k = (n + k - 1) % n;
            break;
        }
        
        case '2':
        {
            /* Move down */
            k = (k + 1) % n;
            break;
        }

        case '0':
        {
            if (k == 2)
            {
                if (get_sdl_aux_view_font_size() != 0)
                {
                    set_sdl_aux_view_font_size(0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else
            {
                bell("0 sets the default aux font to auto");
            }
            break;
        }
        
        case 't':
        case '5':
        case ' ':
        {
            /* Toggle or activate current option */
            if (k == 1) /* Minimum Terminal Size */
            {
                set_sdl_min_terminal_mode(get_sdl_min_terminal_mode() == 0 ? 1 : 0);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 4) /* Fullscreen */
            {
                set_sdl_fullscreen(!get_sdl_fullscreen());
                settings_changed = true;
            }
            else if (k == 5) /* Tiles */
            {
                set_sdl_tiles(!get_sdl_tiles());
                settings_changed = true;
            }
            else if (k == 6) /* Enable Side Panes */
            {
                set_sdl_enable_right_panes(!get_sdl_enable_right_panes());
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 7) /* Enable Bottom Panes */
            {
                set_sdl_enable_bottom_panes(!get_sdl_enable_bottom_panes());
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 8) /* Supporting Pane Layout */
            {
                do_cmd_supporting_pane_layout_editor(&settings_changed);
            }
            else if (k == 9) /* Pane Font Sizes */
            {
                do_cmd_supporting_pane_font_editor(&settings_changed);
            }
            else if (k == 10) /* Save/Return */
            {
                if (settings_changed)
                {
                    if (save_pane_config_to_json())
                    {
                        msg_format("Settings saved to %s", config_label);
                    }
                }
                done = true;
            }
            break;
        }
        
        case 'y':
        case '6':
        {
            /* Increase value or set to yes */
            int val;
            
            if (k == 0) /* Main View Scale */
            {
                val = get_sdl_main_view_scale();
                int max_scale = get_sdl_max_scale();
                if (val < max_scale)
                {
                    set_sdl_main_view_scale(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 1) /* Minimum Terminal Size */
            {
                if (get_sdl_min_terminal_mode() != 0)
                {
                    set_sdl_min_terminal_mode(0);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 2) /* Aux View Font Size */
            {
                val = get_sdl_aux_view_font_size();
                if (val == 0)
                {
                    set_sdl_aux_view_font_size(get_sdl_effective_aux_view_font_size());
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val < 48)
                {
                    set_sdl_aux_view_font_size(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3) /* Margin */
            {
                val = get_sdl_margin();
                if (val < 20)
                {
                    set_sdl_margin(val + 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 4) /* Fullscreen */
            {
                set_sdl_fullscreen(true);
                settings_changed = true;
            }
            else if (k == 5) /* Tiles */
            {
                set_sdl_tiles(true);
                settings_changed = true;
            }
            else if (k == 6) /* Enable Side Panes */
            {
                set_sdl_enable_right_panes(true);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 7) /* Enable Bottom Panes */
            {
                set_sdl_enable_bottom_panes(true);
                settings_changed = true;
                sdl_apply_config();
            }
            break;
        }
        
        case 'n':
        case '4':
        {
            /* Decrease value or set to no */
            int val;
            
            if (k == 0) /* Main View Scale */
            {
                val = get_sdl_main_view_scale();
                if (val > 1)
                {
                    set_sdl_main_view_scale(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 1) /* Minimum Terminal Size */
            {
                if (get_sdl_min_terminal_mode() != 1)
                {
                    set_sdl_min_terminal_mode(1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 2) /* Aux View Font Size */
            {
                val = get_sdl_aux_view_font_size();
                if (val == 0)
                {
                    set_sdl_aux_view_font_size(get_sdl_effective_aux_view_font_size());
                    settings_changed = true;
                    sdl_apply_config();
                }
                else if (val > 8)
                {
                    set_sdl_aux_view_font_size(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 3) /* Margin */
            {
                val = get_sdl_margin();
                if (val > 0)
                {
                    set_sdl_margin(val - 1);
                    settings_changed = true;
                    sdl_apply_config();
                }
            }
            else if (k == 4) /* Fullscreen */
            {
                set_sdl_fullscreen(false);
                settings_changed = true;
            }
            else if (k == 5) /* Tiles */
            {
                set_sdl_tiles(false);
                settings_changed = true;
            }
            else if (k == 6) /* Enable Side Panes */
            {
                set_sdl_enable_right_panes(false);
                settings_changed = true;
                sdl_apply_config();
            }
            else if (k == 7) /* Enable Bottom Panes */
            {
                set_sdl_enable_bottom_panes(false);
                settings_changed = true;
                sdl_apply_config();
            }
            break;
        }
        
        default:
        {
            bell("Illegal command for pane settings!");
            break;
        }
        }
    }
    
    /* Restore screen */
    screen_load();
}


static const char* pane_type_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INVENTORY";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHARACTER";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MONSTERS";
    case PANE_TOUCH: return "TOUCH";
    default: return "UNKNOWN";
    }
}

static void do_cmd_supporting_pane_font_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = 8 };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        if (type == PANE_MAIN)
            continue;
        pane_indices[pane_count++] = i;
    }

    screen_save();

    if (pane_count <= 0)
    {
        Term_clear();
        Term_putstr(2, 1, -1, TERM_L_BLUE, "Supporting Pane Fonts");
        Term_putstr(2, 3, -1, TERM_WHITE, "No supporting panes are configured.");
        Term_putstr(2, Term->hgt - 1, -1, TERM_L_BLUE, "Press any key to return...");
        Term_fresh();
        (void)inkey();
        screen_load();
        return;
    }

    {
        int sel = 0;
        bool done = false;
        bool changed = false;
        int dir;

        while (!done)
        {
            int y0 = 4;
            int row_width;
            int term_wid;

            Term_clear();
            term_wid = settings_ui_term_wid();
            row_width = settings_ui_line_width(2);
            settings_ui_put_fitted(1, 2, TERM_L_BLUE, "Supporting Pane Fonts");
            settings_ui_put_fitted(2, 2, TERM_WHITE, "=====================");

            for (int i = 0; i < pane_count && (y0 + i) < Term->hgt - 5; i++)
            {
                int idx = pane_indices[i];
                enum pane_type type = (enum pane_type)get_sdl_pane_type(idx);
                bool enabled = get_sdl_pane_enabled(idx);
                int raw_font = get_sdl_pane_font_size(idx);
                int effective_font = get_sdl_pane_effective_font_size(idx);
                byte a = (i == sel) ? TERM_L_BLUE : (enabled ? TERM_WHITE : TERM_SLATE);
                char line_buf[96];
                char label_buf[48];
                char font_value[24];
                char font_field[28];
                const char* type_label = settings_ui_pick_label(MAX(8, row_width / 2),
                    pane_type_name(type), pane_type_name(type),
                    pane_type_short_name(type));

                format_font_size_value(font_value, sizeof(font_value), raw_font,
                    effective_font, MAX(6, MIN(14, row_width / 2)));
                settings_ui_format_field(font_field, sizeof(font_field), font_value,
                    i == sel);
                strnfmt(label_buf, sizeof(label_buf), "%s %s", type_label,
                    enabled ? "on" : "off");
                settings_ui_format_pair_line(line_buf, sizeof(line_buf), label_buf,
                    font_field, row_width, 6);
                c_prt(a, line_buf, y0 + i, 2);
            }

            {
                int y = Term->hgt - 4;
                settings_ui_put_fitted(y++, 2, TERM_SLATE,
                    settings_ui_pick_label(term_wid - 2,
                        "Up/Down: select pane   4/6 (or n/y): change font size",
                        "Up/Down select pane   4/6 set font size",
                        "Up/Down select   4/6 set"));
                settings_ui_put_fitted(y++, 2, TERM_SLATE,
                    settings_ui_pick_label(term_wid - 2,
                        "0: auto (uses default aux font / auto main-based size)",
                        "0: auto font size",
                        "0 auto font"));
                settings_ui_put_fitted(y++, 2, TERM_SLATE,
                    settings_ui_pick_label(term_wid - 2,
                        "Changes apply immediately",
                        "Changes apply immediately",
                        "Changes apply now"));
            }

            Term_fresh();

            inkey_set_cursor_hidden(true);
            char ch = inkey();
            inkey_set_cursor_hidden(false);

            dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);

            switch (ch)
            {
            case ESCAPE:
            case '\n':
            case '\r':
                done = true;
                break;

            case '-':
            case '8':
                sel = (pane_count + sel - 1) % pane_count;
                break;

            case '2':
                sel = (sel + 1) % pane_count;
                break;

            case '0':
            {
                int idx = pane_indices[sel];
                if (get_sdl_pane_font_size(idx) != 0)
                {
                    set_sdl_pane_font_size(idx, 0);
                    changed = true;
                    sdl_apply_config();
                }
                break;
            }

            case 'n':
            case '4':
            case 'y':
            case '6':
            {
                int idx = pane_indices[sel];
                int delta = ((ch == 'n') || (ch == '4')) ? -1 : 1;
                int value = get_sdl_pane_font_size(idx);

                if (value == 0)
                    set_sdl_pane_font_size(idx, get_sdl_pane_effective_font_size(idx));
                else
                    set_sdl_pane_font_size(idx, value + delta);

                changed = true;
                sdl_apply_config();
                break;
            }

            default:
                bell("Illegal command for pane font editor!");
                break;
            }
        }

        if (changed && settings_changed)
            *settings_changed = true;
    }

    screen_load();
}

static const char* pane_type_short_name(enum pane_type type)
{
    switch (type)
    {
    case PANE_MAIN: return "MAIN";
    case PANE_INVENTORY: return "INV";
    case PANE_WORN: return "WORN";
    case PANE_ROLLS: return "ROLLS";
    case PANE_INFO: return "INFO";
    case PANE_CHARACTER: return "CHAR";
    case PANE_LOG: return "LOG";
    case PANE_MONSTERS: return "MON";
    case PANE_TOUCH: return "TOUCH";
    default: return "UNK";
    }
}

static const char* pane_where_short_name(enum pane_placement where)
{
    switch (where)
    {
    case PLACE_RIGHT: return "R";
    case PLACE_LEFT: return "L";
    case PLACE_DOUBLE_RIGHT: return "DR";
    case PLACE_DOUBLE_LEFT: return "DL";
    case PLACE_BOTTOM: return "BOT";
    default: return "?";
    }
}

static int get_supporting_pane_config_count(void)
{
    int count = 0;
    int total = get_pane_config_count();
    for (int i = 0; i < total; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        if (type != PANE_MAIN)
            count++;
    }
    return count;
}

static int supporting_pane_master_idx(const int* pane_indices, int pane_count,
    enum pane_placement where)
{
    int fallback = -1;

    for (int i = 0; i < pane_count; i++)
    {
        int idx = pane_indices[i];
        if ((enum pane_placement)get_sdl_pane_where(idx) != where)
            continue;
        if (fallback < 0)
            fallback = idx;
        if (get_sdl_pane_enabled(idx))
            return idx;
    }

    return fallback;
}

static bool supporting_pane_rows_locked(const int* pane_indices, int pane_count, int idx)
{
    enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
    int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

    return (where == PLACE_BOTTOM && idx != master_idx);
}

static bool supporting_pane_cols_locked(const int* pane_indices, int pane_count, int idx)
{
    enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
    int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

    return (pane_placement_is_side(where) && idx != master_idx);
}

static void supporting_pane_ensure_editable_field(int* field, const int* pane_indices,
    int pane_count, int sel)
{
    int idx;

    if (!field || pane_count <= 0 || sel < 0 || sel >= pane_count)
        return;

    idx = pane_indices[sel];
    while ((*field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
        || (*field == 3 && supporting_pane_cols_locked(pane_indices, pane_count, idx)))
    {
        *field = (*field + 1) % 4;
    }
}

static bool supporting_pane_normalize_shared_sizes(const int* pane_indices, int pane_count)
{
    bool changed = false;

    for (int i = 0; i < pane_count; i++)
    {
        int idx = pane_indices[i];
        enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
        int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);

        if (where == PLACE_BOTTOM && idx != master_idx && get_sdl_pane_rows(idx) != 0)
        {
            set_sdl_pane_rows(idx, 0);
            changed = true;
        }
        else if (pane_placement_is_side(where) && idx != master_idx
            && get_sdl_pane_cols(idx) != 0)
        {
            set_sdl_pane_cols(idx, 0);
            changed = true;
        }
    }

    return changed;
}

static void do_cmd_supporting_pane_layout_editor(bool* settings_changed)
{
    enum { MAX_PANES_LOCAL = 8 };
    int pane_indices[MAX_PANES_LOCAL];
    int pane_count = 0;
    int total = get_pane_config_count();

    for (int i = 0; i < total && pane_count < MAX_PANES_LOCAL; i++)
    {
        enum pane_type type = (enum pane_type)get_sdl_pane_type(i);
        if (type == PANE_MAIN)
            continue;
        pane_indices[pane_count++] = i;
    }

    screen_save();

    int sel = 0;
    int field = 0; /* 0 = enabled, 1 = where, 2 = rows, 3 = cols */
    bool done = false;
    bool changed = false;
    int dir;

    if (pane_count <= 0)
    {
        Term_clear();
        Term_putstr(2, 1, -1, TERM_L_BLUE, "Supporting Pane Layout");
        Term_putstr(2, 3, -1, TERM_WHITE, "No supporting panes are configured.");
        Term_putstr(2, Term->hgt - 1, -1, TERM_L_BLUE, "Press any key to return...");
        Term_fresh();
        (void)inkey();
        screen_load();
        return;
    }

    if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
    {
        changed = true;
        sdl_apply_config();
    }
    supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);

    while (!done)
    {
        int y0 = 4;
        int term_wid;
        int row_width;

        Term_clear();
        term_wid = settings_ui_term_wid();
        row_width = settings_ui_line_width(2);
        settings_ui_put_fitted(1, 2, TERM_L_BLUE, "Supporting Pane Layout");
        settings_ui_put_fitted(2, 2, TERM_WHITE, "======================");

        for (int i = 0; i < pane_count && (y0 + i) < Term->hgt - 5; i++)
        {
            int idx = pane_indices[i];
            enum pane_type type = (enum pane_type)get_sdl_pane_type(idx);
            enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);
            int master_idx = supporting_pane_master_idx(pane_indices, pane_count, where);
            bool enabled = get_sdl_pane_enabled(idx);
            bool rows_locked = supporting_pane_rows_locked(pane_indices, pane_count, idx);
            bool cols_locked = supporting_pane_cols_locked(pane_indices, pane_count, idx);
            int rows = get_sdl_pane_rows(idx);
            int cols = get_sdl_pane_cols(idx);
            byte a = (i == sel) ? TERM_L_BLUE : (enabled ? TERM_WHITE : TERM_SLATE);
            char type_buf[24];
            char enabled_field[12];
            char where_field[24];
            char rows_value[16];
            char rows_field[20];
            char cols_value[16];
            char cols_field[20];
            char line_buf[128];
            const char* type_label = settings_ui_pick_label(MAX(8, row_width / 3),
                pane_type_name(type), pane_type_name(type), pane_type_short_name(type));
            const char* where_label = settings_ui_pick_label(MAX(4, row_width / 4),
                pane_placement_name(where), pane_placement_name(where),
                pane_where_short_name(where));

            settings_ui_fit_text(type_buf, sizeof(type_buf), type_label,
                MAX(4, row_width / 3));
            settings_ui_format_field(enabled_field, sizeof(enabled_field),
                enabled ? "on" : "off", i == sel && field == 0);
            settings_ui_format_field(where_field, sizeof(where_field), where_label,
                i == sel && field == 1);

            if (rows_locked)
            {
                int shared_rows = (master_idx >= 0) ? get_sdl_pane_rows(master_idx) : rows;
                settings_ui_format_auto_value(rows_value, sizeof(rows_value),
                    shared_rows, 4);
            }
            else
                settings_ui_format_auto_value(rows_value, sizeof(rows_value), rows, 4);
            settings_ui_format_field(rows_field, sizeof(rows_field), rows_value,
                !rows_locked && i == sel && field == 2);

            if (cols_locked)
            {
                int shared_cols = (master_idx >= 0) ? get_sdl_pane_cols(master_idx) : cols;
                settings_ui_format_auto_value(cols_value, sizeof(cols_value),
                    shared_cols, 4);
            }
            else
                settings_ui_format_auto_value(cols_value, sizeof(cols_value), cols, 4);
            settings_ui_format_field(cols_field, sizeof(cols_field), cols_value,
                !cols_locked && i == sel && field == 3);

            strnfmt(line_buf, sizeof(line_buf), "%s %s %s r%s c%s", type_buf,
                where_field, enabled_field, rows_field, cols_field);
            settings_ui_put_fitted(y0 + i, 2, a, line_buf);
        }

        {
            int y = Term->hgt - 4;
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "Up/Down: select pane   Space: choose on/off, where, rows, cols",
                    "Up/Down select pane   Space switch field",
                    "Up/Down select   Space field"));
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "4/6 (or n/y): toggle, cycle, or +/- value   0: set rows/cols to auto",
                    "4/6 or y/n: toggle, cycle, or +/- value   0: auto",
                    "4/6 cycle/set   0 auto"));
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "Each side slot shares cols with its first pane; bottom panes share rows",
                    "Side slots share cols; bottom panes share rows",
                    "Side slots share cols; bottom shares rows"));
            settings_ui_put_fitted(y++, 2, TERM_SLATE,
                settings_ui_pick_label(term_wid - 2,
                    "ESC/Enter: return (changes apply immediately)",
                    "ESC/Enter: return",
                    "Esc/Enter return"));
        }

        Term_fresh();

        inkey_set_cursor_hidden(true);
        char ch = inkey();
        inkey_set_cursor_hidden(false);

        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case ' ':
        case 't':
        case '5':
            field = (field + 1) % 4;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '-':
        case '8':
            sel = (pane_count + sel - 1) % pane_count;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '2':
            sel = (sel + 1) % pane_count;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            break;

        case '0':
        {
            int idx = pane_indices[sel];
            if (field == 0 || field == 1)
            {
                bell("Use 4/6 to toggle enabled or cycle placement");
                break;
            }
            if (field == 2 && supporting_pane_rows_locked(pane_indices, pane_count, idx))
            {
                bell("Rows are shared for bottom panes");
                break;
            }
            if (field == 3 && supporting_pane_cols_locked(pane_indices, pane_count, idx))
            {
                bell("Cols are shared within each side slot");
                break;
            }

            if (field == 2)
                set_sdl_pane_rows(idx, 0);
            else
                set_sdl_pane_cols(idx, 0);

            if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
                changed = true;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            changed = true;
            sdl_apply_config();
            break;
        }

        case 'n':
        case '4':
        case 'y':
        case '6':
        {
            int idx = pane_indices[sel];
            int delta = ((ch == 'n') || (ch == '4')) ? -1 : 1;
            enum pane_type type = (enum pane_type)get_sdl_pane_type(idx);
            enum pane_placement where = (enum pane_placement)get_sdl_pane_where(idx);

            if (field == 0)
            {
                set_sdl_pane_enabled(idx, (delta > 0));
            }
            else if (field == 1)
            {
                set_sdl_pane_where(idx, pane_next_allowed_placement(type, where, delta));
            }
            else if (field == 2)
            {
                int rows = get_sdl_pane_rows(idx);

                if (supporting_pane_rows_locked(pane_indices, pane_count, idx))
                {
                    bell("Rows are shared for bottom panes");
                    break;
                }
                if (rows == 0)
                    set_sdl_pane_rows(idx, get_sdl_pane_current_rows(idx));
                else
                    set_sdl_pane_rows(idx, rows + delta);
            }
            else
            {
                int cols = get_sdl_pane_cols(idx);

                if (supporting_pane_cols_locked(pane_indices, pane_count, idx))
                {
                    bell("Cols are shared within each side slot");
                    break;
                }
                if (cols == 0)
                    set_sdl_pane_cols(idx, get_sdl_pane_current_cols(idx));
                else
                    set_sdl_pane_cols(idx, cols + delta);
            }

            if (supporting_pane_normalize_shared_sizes(pane_indices, pane_count))
                changed = true;
            supporting_pane_ensure_editable_field(&field, pane_indices, pane_count, sel);
            changed = true;
            sdl_apply_config();
            break;
        }

        default:
            bell("Illegal command for pane layout editor!");
            break;
        }
    }

    if (changed && settings_changed)
        *settings_changed = true;

    screen_load();
}

static const int touch_pane_main_action_choices[] = {
    GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', 'E', 't', 'p', 'q',
    'F', 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const int touch_pane_second_action_choices[] = {
    TOUCH_PANE_BIND_INHERIT, GAMEPAD_BIND_NONE,
    ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT, INPUT_BIND_CONFIRM,
    'e', 'i', 'j',
    'u', 's', 'f',
    '7', '8', '9',
    '4', '5', '6',
    '1', '2', '3',
    'a', 'x', 'd',
    'M', 'h', '\t',
    'z', '.', '/',
    'w', 'r', 'k', 'g', 'Z',
    'o', 'c', 'D', 'X',
    '-', '{', 'a', 'E', 't', 'p', 'q',
    'F', 'S', 'l', 'b', 'L',
    '0', '<', '>', '?', 'O', ':', '~', '[', ']', '@',
};

static const int* touch_pane_action_choices_for_panel(int panel, int* count)
{
    if (count)
        *count = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
            ? (int)N_ELEMENTS(touch_pane_second_action_choices)
            : (int)N_ELEMENTS(touch_pane_main_action_choices);

    return (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? touch_pane_second_action_choices
        : touch_pane_main_action_choices;
}

static int touch_pane_action_choice_index(int panel, int binding)
{
    int count = 0;
    const int* choices = touch_pane_action_choices_for_panel(panel, &count);

    for (int i = 0; i < count; i++)
    {
        if (choices[i] == binding)
            return i;
    }
    return 0;
}

static void touch_pane_action_label_for_panel(int panel, int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (binding == TOUCH_PANE_BIND_INHERIT) {
        SDL_strlcpy(buf, "Main panel button", buflen);
        return;
    }

    if (binding == GAMEPAD_BIND_SHIFT) {
        char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
        get_sdl_touch_pane_panel_name((panel == SDL_TOUCH_PANE_PANEL_SECOND)
                ? SDL_TOUCH_PANE_PANEL_MAIN
                : SDL_TOUCH_PANE_PANEL_SECOND,
            panel_name, sizeof(panel_name));
        strnfmt(buf, buflen, "Switch to %s panel", panel_name);
        return;
    }

    binding_action_label(binding, buf, buflen);
}

static void do_cmd_touch_pane_button_editor(bool* settings_changed)
{
    int highlight = 0;
    int top = 0;
    int panel = SDL_TOUCH_PANE_PANEL_MAIN;
    bool done = false;
    bool changed = false;
    int term_w, term_h;
    const int list_start_row = 5;

    screen_save();

    while (!done)
    {
        int row;
        int visible_rows;
        int row_width;

        Term_get_size(&term_w, &term_h);
        row_width = settings_ui_line_width(2);
        visible_rows = term_h - list_start_row - 6;
        if (visible_rows < 5)
            visible_rows = 5;

        if (highlight < 0)
            highlight = 0;
        if (highlight >= SDL_TOUCH_PANE_BUTTON_COUNT)
            highlight = SDL_TOUCH_PANE_BUTTON_COUNT - 1;

        if (top > highlight)
            top = highlight;
        if (top + visible_rows <= highlight)
            top = highlight - visible_rows + 1;
        if (top < 0)
            top = 0;

        Term_clear();
        settings_ui_put_fitted(1, 2, TERM_L_BLUE, "Touch Settings");
        settings_ui_put_fitted(2, 2, TERM_WHITE, "==============");

        row = list_start_row;
        for (int i = top; i < SDL_TOUCH_PANE_BUTTON_COUNT && i < top + visible_rows; i++)
        {
            char action_buf[80];
            char label_buf[SDL_TOUCH_PANE_LABEL_LEN];
            char left_buf[64];
            char line_buf[128];
            byte a = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            get_sdl_touch_pane_button_label_for_panel(panel, i, label_buf, sizeof(label_buf));
            touch_pane_action_label_for_panel(panel,
                get_sdl_touch_pane_binding_for_panel(panel, i), action_buf, sizeof(action_buf));

            if (label_buf[0])
                strnfmt(left_buf, sizeof(left_buf), "%s %s",
                    get_sdl_touch_pane_slot_name(i), label_buf);
            else
                strnfmt(left_buf, sizeof(left_buf), "%s",
                    get_sdl_touch_pane_slot_name(i));

            settings_ui_format_pair_line(line_buf, sizeof(line_buf), left_buf,
                action_buf, row_width, 14);
            c_prt(a, line_buf, row++, 2);
        }

        row = list_start_row + visible_rows + 1;
        {
            char panel_name[SDL_TOUCH_PANE_LABEL_LEN];
            char info_buf[96];

            get_sdl_touch_pane_panel_name(panel, panel_name, sizeof(panel_name));
            strnfmt(info_buf, sizeof(info_buf), "Editing %s panel%s",
                panel_name, (panel == SDL_TOUCH_PANE_PANEL_SECOND) ? " (empty = main panel)" : "");
            settings_ui_put_fitted(3, 2, TERM_SLATE, info_buf);
        }
        settings_ui_put_fitted(row++, 2, TERM_SLATE,
            settings_ui_pick_label(row_width,
                "Up/Down: select button   4/6: previous/next action   l: rename slot",
                "Up/Down select   4/6 action   l rename slot",
                "Up/Down select   4/6 action"));
        settings_ui_put_fitted(row++, 2, TERM_SLATE,
            settings_ui_pick_label(row_width,
                "Tab: switch panel   p: rename panel   r: reset selected   R: reset all",
                "Tab switch panel   p rename panel   r/R reset",
                "Tab switch   p rename   r/R reset"));
        settings_ui_put_fitted(row++, 2, TERM_SLATE,
            settings_ui_pick_label(row_width,
                "ESC/Enter: return",
                "Esc/Enter: return",
                "Esc/Enter return"));

        Term_fresh();

        inkey_set_cursor_hidden(true);
        char ch = inkey();
        inkey_set_cursor_hidden(false);

        {
            int dir = target_dir(ch);
            if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
                ch = I2D(dir);
        }

        switch (ch)
        {
        case ESCAPE:
        case '\n':
        case '\r':
            done = true;
            break;

        case '-':
        case '8':
            highlight = (SDL_TOUCH_PANE_BUTTON_COUNT + highlight - 1) % SDL_TOUCH_PANE_BUTTON_COUNT;
            break;

        case '2':
            highlight = (highlight + 1) % SDL_TOUCH_PANE_BUTTON_COUNT;
            break;

        case 'n':
        case '4':
        {
            int choice_count = 0;
            const int* choices = touch_pane_action_choices_for_panel(panel, &choice_count);
            int idx = touch_pane_action_choice_index(panel, get_sdl_touch_pane_binding_for_panel(panel, highlight));
            idx = (choice_count + idx - 1) % choice_count;
            set_sdl_touch_pane_binding_for_panel(panel, highlight, choices[idx]);
            changed = true;
            break;
        }

        case 'y':
        case '6':
        case ' ':
        case 't':
        case '5':
        {
            int choice_count = 0;
            const int* choices = touch_pane_action_choices_for_panel(panel, &choice_count);
            int idx = touch_pane_action_choice_index(panel, get_sdl_touch_pane_binding_for_panel(panel, highlight));
            idx = (idx + 1) % choice_count;
            set_sdl_touch_pane_binding_for_panel(panel, highlight, choices[idx]);
            changed = true;
            break;
        }

        case 'l':
        case 'L':
        {
            char prompt[96];
            char prompt_long[96];
            char prompt_medium[96];
            char prompt_short[64];
            char current_label[SDL_TOUCH_PANE_LABEL_LEN];
            char new_label[SDL_TOUCH_PANE_LABEL_LEN];
            char current_buf[96];

            get_sdl_touch_pane_button_label_for_panel(panel, highlight, current_label, sizeof(current_label));
            strnfmt(prompt_long, sizeof(prompt_long),
                "New label for %s (blank = use key label): ",
                get_sdl_touch_pane_slot_name(highlight));
            strnfmt(prompt_medium, sizeof(prompt_medium),
                "New label for %s (blank = default): ",
                get_sdl_touch_pane_slot_name(highlight));
            strnfmt(prompt_short, sizeof(prompt_short), "Label for %s: ",
                get_sdl_touch_pane_slot_name(highlight));
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(settings_ui_line_width(0),
                    prompt_long, prompt_medium, prompt_short));
            strnfmt(current_buf, sizeof(current_buf), "Current label: %s", current_label);
            settings_ui_put_fitted(4, 2, TERM_SLATE, current_buf);
            new_label[0] = '\0';
            if (term_get_string(prompt, new_label, sizeof(new_label)))
            {
                set_sdl_touch_pane_button_label_for_panel(panel, highlight, new_label);
                changed = true;
            }
            break;
        }

        case '\t':
            panel = (panel == SDL_TOUCH_PANE_PANEL_MAIN)
                ? SDL_TOUCH_PANE_PANEL_SECOND
                : SDL_TOUCH_PANE_PANEL_MAIN;
            break;

        case 'p':
        case 'P':
        {
            char prompt[96];
            char current_name[SDL_TOUCH_PANE_LABEL_LEN];
            char new_name[SDL_TOUCH_PANE_LABEL_LEN];
            char current_buf[96];

            get_sdl_touch_pane_panel_name(panel, current_name, sizeof(current_name));
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(settings_ui_line_width(0),
                    "Name for current panel (blank = default): ",
                    "Panel name (blank = default): ",
                    "Panel name: "));
            strnfmt(current_buf, sizeof(current_buf), "Current panel name: %s", current_name);
            settings_ui_put_fitted(4, 2, TERM_SLATE, current_buf);
            new_name[0] = '\0';
            if (term_get_string(prompt, new_name, sizeof(new_name)))
            {
                set_sdl_touch_pane_panel_name(panel, new_name);
                changed = true;
            }
            break;
        }

        case 'r':
            set_sdl_touch_pane_binding_for_panel(panel, highlight,
                get_sdl_touch_pane_default_binding_for_panel(panel, highlight));
            clear_sdl_touch_pane_button_label_for_panel(panel, highlight);
            changed = true;
            break;

        case 'R':
            sdl_touch_pane_reset_bindings_to_default();
            changed = true;
            break;

        default:
            bell("Illegal command for touch settings!");
            break;
        }
    }

    if (changed)
    {
        if (settings_changed)
            *settings_changed = true;
    }

    screen_load();
}


void do_cmd_controller_settings(void);

int options_menu(int* highlight)
{
    int ch;
    int options = 16;
    int term_wid = 80;
    int term_hgt = 24;
    int title_row = 1;
    int row;
    bool allow_debug_menu = false;
#ifdef SHOW_DEBUG_OPTIONS_MENU
    allow_debug_menu = true;
#endif
    if (allow_debug_menu && p_ptr->noscore)
        options++;

    Term_get_size(&term_wid, &term_hgt);
    if (term_hgt < 20)
        title_row = 0;

    row = title_row + 2;

    Term_putstr(2, title_row, -1, TERM_WHITE, "Options and misc");

    Term_putstr(2, row++, -1, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE,
        "a) Set Keybinds");
    Term_putstr(2, row++, -1, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE,
        "b) Controller Settings");
    Term_putstr(2, row++, -1, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE,
        "c) Touch Settings");
    Term_putstr(2, row++, -1, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE,
        "d) Pane Settings");
    Term_putstr(2, row++, -1, (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE,
        "e) Interface Options");
    Term_putstr(2, row++, -1, (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE,
        "f) Efficiency Options");
    Term_putstr(2, row++, -1, (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE,
        "g) Visual Options");
    Term_putstr(2, row++, -1, (*highlight == 8) ? TERM_L_BLUE : TERM_WHITE,
        "t) Text Options");
    Term_putstr(2, row++, -1, (*highlight == 9) ? TERM_L_BLUE : TERM_WHITE,
        "h) Gameplay Options");
    Term_putstr(2, row++, -1, (*highlight == 10) ? TERM_L_BLUE : TERM_WHITE,
        "i) Sound Options");
    Term_putstr(2, row++, -1, (*highlight == 11) ? TERM_L_BLUE : TERM_WHITE,
        "j) Load a 'Pref' File");
    Term_putstr(2, row++, -1, (*highlight == 12) ? TERM_L_BLUE : TERM_WHITE,
        "k) Append Options to a 'Pref' File");
    Term_putstr(2, row++, -1, (*highlight == 13) ? TERM_L_BLUE : TERM_WHITE,
        "l) Set Macros");
    Term_putstr(2, row++, -1, (*highlight == 14) ? TERM_L_BLUE : TERM_WHITE,
        "m) Set Colours");
    Term_putstr(2, row++, -1, (*highlight == 15) ? TERM_L_BLUE : TERM_WHITE,
        "n) Write a note");
    Term_putstr(2, row++, -1, (*highlight == 16) ? TERM_L_BLUE : TERM_WHITE,
        "o) Return to Game");

    if (allow_debug_menu && p_ptr->noscore)
    {
        Term_putstr(2, row++, -1, (*highlight == 17) ? TERM_L_BLUE : TERM_WHITE,
            "p) Debugging Options");
    }

    /* Show product name and version on the bottom of the menu */
    {
        char verbuf[128];
        strnfmt(verbuf, sizeof(verbuf), "%s %s", VERSION_NAME, VERSION_STRING);
        if (row < term_hgt)
            Term_putstr(2, row, term_wid - 2, TERM_SLATE, verbuf);
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(2, title_row + 1 + *highlight);

    /* Get key (while allowing menu commands) */
    inkey_set_cursor_hidden(true);
    ch = inkey();
    inkey_set_cursor_hidden(false);

    if ((ch == 'a') || (ch == 'A'))
    {
        *highlight = 1;
        return (1);
    }

    if ((ch == 'b') || (ch == 'B'))
    {
        *highlight = 2;
        return (2);
    }

    if ((ch == 'c') || (ch == 'C'))
    {
        *highlight = 3;
        return (3);
    }

    if ((ch == 'd') || (ch == 'D'))
    {
        *highlight = 4;
        return (4);
    }

    if ((ch == 'e') || (ch == 'E'))
    {
        *highlight = 5;
        return (5);
    }

    if ((ch == 'f') || (ch == 'F'))
    {
        *highlight = 6;
        return (6);
    }

    if ((ch == 'g') || (ch == 'G'))
    {
        *highlight = 7;
        return (7);
    }

    if ((ch == 't') || (ch == 'T'))
    {
        *highlight = 8;
        return (8);
    }

    if ((ch == 'h') || (ch == 'H'))
    {
        *highlight = 9;
        return (9);
    }

    if ((ch == 'i') || (ch == 'I'))
    {
        *highlight = 10;
        return (10);
    }

    if ((ch == 'j') || (ch == 'J'))
    {
        *highlight = 11;
        return (11);
    }

    if ((ch == 'k') || (ch == 'K'))
    {
        *highlight = 12;
        return (12);
    }

    if ((ch == 'l') || (ch == 'L'))
    {
        *highlight = 13;
        return (13);
    }

    if ((ch == 'm') || (ch == 'M'))
    {
        *highlight = 14;
        return (14);
    }

    if ((ch == 'n') || (ch == 'N'))
    {
        *highlight = 15;
        return (15);
    }

    if ((ch == 'o') || (ch == 'O') || (ch == ESCAPE) || (ch == 'q'))
    {
        *highlight = 16;
        return (16);
    }

    if (allow_debug_menu && p_ptr->noscore && ((ch == 'p') || (ch == 'P')))
    {
        *highlight = 17;
        return (17);
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

    char ftmp[80];

    bool return_to_game = false;

    /* Clear any active banner before opening options */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Clear screen */
    Term_clear();

    /* Process Events until "Return to Game" is selected */
    while (!return_to_game)
    {
        choice = options_menu(&highlight);

        switch (choice)
        {
        case 1:
        {
            do_cmd_keybinds();
            Term_clear();
            break;
        }
        case 2:
        {
            do_cmd_controller_settings();
            Term_clear();
            break;
        }
        case 3:
        {
            do_cmd_touch_pane_button_editor(NULL);
            Term_clear();
            break;
        }
        case 4:
        {
            do_cmd_pane_settings();
            Term_clear();
            break;
        }
        case 5:
        {
            do_cmd_options_aux(INTERFACE_PAGE, "Interface Options");
            Term_clear();
            break;
        }
        case 6:
        {
            do_cmd_options_aux(EFFICIENCY_PAGE, "Efficiency Options");
            Term_clear();
            break;
        }
        case 7:
        {
            do_cmd_options_aux(VISUAL_PAGE, "Visual Options");
            Term_clear();
            break;
        }
        case 8:
        {
            do_cmd_options_aux(TEXT_PAGE, "Text Options");
            Term_clear();
            break;
        }
        case 9:
        {
            do_cmd_options_aux(GAMEPLAY_PAGE, "Gameplay Options");
            Term_clear();
            break;
        }
        case 10:
        {
            do_cmd_options_aux(SOUND_PAGE, "Sound Options");
            Term_clear();
            break;
        }
        case 11:
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(12);
            Term_clear();
            break;
        }
        case 12:
        {
            /* Prompt */
            Term_putstr(2, 14, -1, TERM_SLATE, "(Escape to cancel)");

            /* Prompt */
            prt("File: ", 12, 2);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Ask for a file */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
            {
                Term_clear();
                continue;
            }

            /* Dump the options */
            if (option_dump(ftmp))
            {
                /* Failure */
                msg_print("Failed!");
            }
            else
            {
                /* Success */
                msg_print("Done.");
            }

            Term_clear();
            break;
        }
        case 13:
        {
            do_cmd_macros();
            Term_clear();
            break;
        }
        case 14:
        {
            do_cmd_colors();
            Term_clear();
            break;
        }
        case 15:
        {
            do_cmd_note("", p_ptr->depth);
            Term_clear();
            break;
        }
        case 16:
        {
            /* Return to Game */
            return_to_game = true;
            Term_clear();
            break;
        }
        case 17:
        {
            /* Debugging Options (only reachable when p_ptr->noscore) */
            do_cmd_options_aux(DEBUG_PAGE, "Debugging Options");
            Term_clear();
            break;
        }
        }
    }

    /* Flush messages */
    message_flush();

    /* Load screen */
    screen_load();
}

#ifdef ALLOW_MACROS
/* Forward declaration */
static errr keymap_dump(cptr fname);
#endif

/*
 * Helper to turn a single keycode into printable text for the keybind UI.
 */
static void describe_keycode(byte keycode, char* buf, size_t buflen)
{
    char raw[2];

    if (!buf || !buflen)
        return;

    raw[0] = (char)keycode;
    raw[1] = '\0';

    ascii_to_text(buf, buflen, raw);
}

struct keybind_entry
{
    byte key_code;
    cptr extra_default_keys;
    cptr key_name;
    cptr action;
    bool requires_keymap;
};

static bool key_matches_default(const struct keybind_entry* entry, byte key)
{
    if (key == entry->key_code)
        return true;
    if (entry->extra_default_keys && strchr(entry->extra_default_keys, key))
        return true;
    return false;
}

static bool key_provides_action(int mode, byte key, cptr action, bool requires_keymap)
{
    cptr mapping = keymap_act[mode][key];

    if (requires_keymap)
        return (mapping && streq(mapping, action));

    if (!mapping)
        return true;

    return streq(mapping, action);
}

static bool entry_has_binding(int mode, const struct keybind_entry* entry)
{
    int key;

    if (key_provides_action(mode, entry->key_code, entry->action, entry->requires_keymap))
        return true;

    if (entry->extra_default_keys)
    {
        const char* extra = entry->extra_default_keys;
        while (*extra)
        {
            if (key_provides_action(mode, (byte)*extra, entry->action, false))
                return true;
            extra++;
        }
    }

    for (key = 0; key < 256; key++)
    {
        cptr current = keymap_act[mode][key];

        if (!current || !streq(current, entry->action))
            continue;

        if (key_matches_default(entry, (byte)key))
            continue;

        return true;
    }

    return false;
}

/*
 * Build a comma-separated list of keys that trigger the supplied action.
 */
static void describe_action_bindings(int mode, const struct keybind_entry* entry, char* buf,
    size_t buflen)
{
    int key;
    bool found = false;
    size_t current_len = 0;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!entry->action)
    {
        SDL_strlcpy(buf, "(none)", buflen);
        return;
    }

    if (key_provides_action(mode, entry->key_code, entry->action, entry->requires_keymap))
    {
        char key_label[16];
        describe_keycode(entry->key_code, key_label, sizeof(key_label));
        SDL_strlcpy(buf, key_label, buflen);
        current_len = strlen(buf);
        found = true;
    }

    if (entry->extra_default_keys)
    {
        const char* extra = entry->extra_default_keys;
        while (*extra)
        {
            if (key_provides_action(mode, (byte)*extra, entry->action, false))
            {
                char key_label[16];
                describe_keycode((byte)*extra, key_label, sizeof(key_label));
                if (found)
                    strnfcat(buf, buflen, &current_len, ", %s", key_label);
                else
                {
                    SDL_strlcpy(buf, key_label, buflen);
                    current_len = strlen(buf);
                    found = true;
                }
            }
            extra++;
        }
    }

    for (key = 0; key < 256; key++)
    {
        cptr current = keymap_act[mode][key];

        if (!current || !streq(current, entry->action))
            continue;

        if (key_matches_default(entry, (byte)key))
            continue;

        {
            char key_label[16];
            describe_keycode((byte)key, key_label, sizeof(key_label));
            if (found)
                strnfcat(buf, buflen, &current_len, ", %s", key_label);
            else
            {
                SDL_strlcpy(buf, key_label, buflen);
                current_len = strlen(buf);
                found = true;
            }
        }
    }

    if (!found)
        SDL_strlcpy(buf, "(none)", buflen);
}

/*
 * Remove all key bindings that trigger the specified action.
 */
static void unbind_action(int mode, cptr action)
{
    int key;

    if (!action)
        return;

    for (key = 0; key < 256; key++)
    {
        if (keymap_act[mode][key] && streq(keymap_act[mode][key], action))
        {
            keymap_act[mode][key] = str_free(keymap_act[mode][key]);
        }
    }
}

static bool list_missing_primary_bindings(int mode, const struct keybind_entry* entries,
    int count, char* buffer, size_t buflen)
{
    int i;
    bool ok = true;
    size_t cur = 0;

    if (!buffer || !buflen)
        return true;

    buffer[0] = '\0';

    for (i = 0; i < count; i++)
    {
        if (entry_has_binding(mode, &entries[i]))
            continue;

        if (!ok)
            strnfcat(buffer, buflen, &cur, ", ");
        strnfcat(buffer, buflen, &cur, "%s", entries[i].key_name);
        ok = false;
    }

    return ok;
}

/*
 * Keybind configuration menu
 * Allows rebinding of movement commands for players without a numpad
 */
void do_cmd_keybinds(void)
{
    int mode;
    bool done = false;
    bool dirty = false;
    char ch;
    bool showing_primary = true;
    int highlight_primary = 0;
    int highlight_secondary = 0;
    int top_primary = 0;
    int top_secondary = 0;
    const char* default_file = "user.prf";
    const int list_start_row = 5;
    int term_w, term_h;
    int visible_rows;
    static const struct keybind_entry primary_keybinds[] = {
        {'1', NULL, "Move SW (numpad 1)", ";1", true},
        {'2', NULL, "Move S (numpad 2)", ";2", true},
        {'3', NULL, "Move SE (numpad 3)", ";3", true},
        {'4', NULL, "Move W (numpad 4)", ";4", true},
        {'6', NULL, "Move E (numpad 6)", ";6", true},
        {'7', NULL, "Move NW (numpad 7)", ";7", true},
        {'8', NULL, "Move N (numpad 8)", ";8", true},
        {'9', NULL, "Move NE (numpad 9)", ";9", true},
        {'z', NULL, "Wait (z / numpad 5)", "z", false},
        {'i', NULL, "Inventory", "i", false},
        {'e', NULL, "Equipment", "e", false},
        {'u', NULL, "Use item", "u", false},
        {'x', NULL, "Examine item", "x", false},
        {'s', NULL, "Sing / change song", "s", false},
        {'S', NULL, "Toggle stealth", "S", false},
        {'h', "H@", "Character sheet (h / H / @)", "h", false},
        {'f', NULL, "Fire (primary quiver)", "f", false},
        {'F', NULL, "Fire (secondary quiver)", "F", false},
        {'l', NULL, "Look around", "l", false},
        {'T', NULL, "Tunnel / dig", "T", false},
        {'b', NULL, "Bash door", "b", false},
    };
    
    static const struct keybind_entry secondary_keybinds[] = {
        {'j', NULL, "Supplies overview", "j", false},
        {'.', NULL, "Run (also shift)", ".", false},
        {'/', NULL, "Alt action (also ctrl)", "/", false},
        {'w', NULL, "Wear / wield equipment", "w", false},
        {'r', NULL, "Remove equipment", "r", false},
        {'d', NULL, "Drop item", "d", false},
        {'k', NULL, "Destroy item", "k", false},
        {'g', NULL, "Pick up items", "g", false},
        {'Z', NULL, "Rest", "Z", false},
        {'o', NULL, "Open door / chest", "o", false},
        {'c', NULL, "Close door", "c", false},
        {'D', NULL, "Disarm trap / chest", "D", false},
        {'X', NULL, "Exchange places", "X", false},
        {'-', NULL, "Fletch arrows", "-", false},
        {'{', NULL, "Inscribe item", "{", false},
        {'a', NULL, "Activate staff", "a", false},
        {'E', NULL, "Eat food", "E", false},
        {'t', NULL, "Throw item", "t", false},
        {'p', NULL, "Blow horn", "p", false},
        {'q', NULL, "Quaff potion", "q", false},
        {'M', NULL, "View map", "M", false},
        {'L', NULL, "Pan", "L", false},
        {'0', NULL, "Smithing screen", "0", false},
        {'<', NULL, "Go upstairs", "<", false},
        {'>', NULL, "Go downstairs", ">", false},
        {'m', NULL, "Main menu", "m", false},
        {'?', NULL, "Help", "?", false},
        {'@', NULL, "Character sheet (alternate)", "@", false},
        {'O', NULL, "Options menu", "O", false},
        {':', NULL, "Take notes", ":", false},
        {'~', NULL, "Knowledge browser", "~", false},
        {'[', NULL, "Monster list", "[", false},
        {']', NULL, "Object list", "]", false},
    };
    
    Term_get_size(&term_w, &term_h);
    visible_rows = term_h - list_start_row - 6;
    if (visible_rows < 5)
        visible_rows = 5;
    
    int primary_count = (int)N_ELEMENTS(primary_keybinds);
    int secondary_count = (int)N_ELEMENTS(secondary_keybinds);
    
    /* Determine the keyset mode */
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;
    
    /* Save screen */
    screen_save();
    
    while (!done)
    {
        const struct keybind_entry* keybinds;
        int num_keybinds;
        int* highlight_ptr;
        int* top_ptr;
        int highlight;
        int display_end;
        int row;
        int i;
        bool compact_width;
        char binding_buf[80];
        char line_buf[128];
        int row_width;

        Term_get_size(&term_w, &term_h);
        visible_rows = term_h - list_start_row - 6;
        if (visible_rows < 5)
            visible_rows = 5;
        compact_width = (term_w < 70);
        row_width = settings_ui_line_width(2);
        
        if (showing_primary)
        {
            keybinds = primary_keybinds;
            num_keybinds = primary_count;
            highlight_ptr = &highlight_primary;
            top_ptr = &top_primary;
        }
        else
        {
            keybinds = secondary_keybinds;
            num_keybinds = secondary_count;
            highlight_ptr = &highlight_secondary;
            top_ptr = &top_secondary;
        }
        
        if (*highlight_ptr >= num_keybinds)
            *highlight_ptr = num_keybinds - 1;
        if (*highlight_ptr < 0)
            *highlight_ptr = 0;
        
        if (*top_ptr > *highlight_ptr)
            *top_ptr = *highlight_ptr;
        if (*top_ptr + visible_rows <= *highlight_ptr)
            *top_ptr = *highlight_ptr - visible_rows + 1;
        if (*top_ptr < 0)
            *top_ptr = 0;
        if (num_keybinds > visible_rows)
        {
            int max_top = num_keybinds - visible_rows;
            if (*top_ptr > max_top)
                *top_ptr = max_top;
        }
        else
        {
            *top_ptr = 0;
        }
        
        highlight = *highlight_ptr;
        
        /* Clear screen */
        Term_clear();

        /* Title */
        settings_ui_put_fitted(1, 0, TERM_WHITE, "Keybind Configuration");
        if (compact_width)
        {
            settings_ui_put_fitted(2, 0, TERM_WHITE,
                "8/2 move  Enter bind  Tab switch  Esc return");
            settings_ui_put_fitted(3, 0, TERM_WHITE,
                showing_primary ? "Primary commands" : "Supplementary commands");
        }
        else
        {
            settings_ui_put_fitted(2, 0, TERM_WHITE,
                "Arrow to navigate, Enter to bind, Tab to switch groups, Escape to return");
            settings_ui_put_fitted(3, 0, TERM_WHITE,
                showing_primary ? "Primary Commands: Essential for the gameplay"
                                : "Supplementary Commands");
        }
        
        /* List visible keybinds */
        display_end = *top_ptr + visible_rows;
        if (display_end > num_keybinds)
            display_end = num_keybinds;
        for (i = *top_ptr; i < display_end; i++)
        {
            int entry_row = list_start_row + (i - *top_ptr);
            describe_action_bindings(mode, &keybinds[i], binding_buf, sizeof(binding_buf));
            settings_ui_format_pair_line(line_buf, sizeof(line_buf),
                keybinds[i].key_name, binding_buf, row_width, 12);

            /* Display the keybind */
            if (i == highlight)
            {
                /* Highlighted */
                c_prt(TERM_L_BLUE, line_buf, entry_row, 2);
            }
            else
            {
                /* Normal */
                prt(line_buf, entry_row, 2);
            }
        }
        
        /* Clear any leftover rows */
        for (i = display_end; i < *top_ptr + visible_rows; i++)
        {
            row = list_start_row + (i - *top_ptr);
            Term_erase(2, row, term_w > 2 ? term_w - 2 : 0);
        }
        
        /* Instructions at bottom */
        if (compact_width)
        {
            settings_ui_put_fitted(list_start_row + visible_rows + 1, 2, TERM_WHITE,
                "s: save keybinds");
            settings_ui_put_fitted(list_start_row + visible_rows + 2, 2, TERM_WHITE,
                "r: reset selected");
        }
        else
        {
            strnfmt(line_buf, sizeof(line_buf), "Press 's' to save keybinds to %s",
                default_file);
            settings_ui_put_fitted(list_start_row + visible_rows + 1, 2, TERM_WHITE,
                line_buf);
            settings_ui_put_fitted(list_start_row + visible_rows + 2, 2, TERM_WHITE,
                "Press 'r' to reset selected keybind to default");
        }
        if (dirty)
            c_prt(TERM_YELLOW, "Unsaved changes", list_start_row + visible_rows + 3, 2);
        else
            Term_erase(2, list_start_row + visible_rows + 3,
                term_w > 2 ? term_w - 2 : 0);
        
        /* Get input */
        ch = inkey();
        
        /* Handle input */
        if (ch == ESCAPE || ch == 'q' || ch == 'Q')
        {
            char missing[256];
            if (!list_missing_primary_bindings(mode, primary_keybinds, primary_count, missing,
                    sizeof(missing)))
            {
                char prompt[512];
                strnfmt(prompt, sizeof(prompt),
                    "Essential commands are unbound (%s). Exit anyway? ", missing);
                if (!get_check(prompt))
                    continue;
            }
            done = true;
        }
        else if (ch == '\t')
        {
            showing_primary = !showing_primary;
            continue;
        }
        else if (ch == '8')
        {
            /* Move up */
            if (num_keybinds > 0)
            {
                highlight = (highlight + num_keybinds - 1) % num_keybinds;
                *highlight_ptr = highlight;
            }
        }
        else if (ch == '2')
        {
            /* Move down */
            if (num_keybinds > 0)
            {
                highlight = (highlight + 1) % num_keybinds;
                *highlight_ptr = highlight;
            }
        }
        else if (ch == '\r' || ch == '\n' || ch == ' ')
        {
            /* Rebind the selected key */
            cptr action = keybinds[highlight].action;
            char key_label[32];
            char prompt[80];
            char prompt_long[96];
            char prompt_short[80];
            int entry_row = list_start_row + (highlight - *top_ptr);

            /* Clear the action area */
            Term_erase(2, entry_row, 255);
            
            /* Prompt for new binding */
            strnfmt(prompt_long, sizeof(prompt_long),
                "Press key to use for %s (Escape to cancel):",
                keybinds[highlight].key_name);
            strnfmt(prompt_short, sizeof(prompt_short),
                "Bind %s (Esc cancels):", keybinds[highlight].key_name);
            strnfmt(prompt, sizeof(prompt), "%s",
                settings_ui_pick_label(row_width, prompt_long, prompt_short,
                    prompt_short));
            settings_ui_put_fitted(entry_row, 2, TERM_YELLOW, prompt);
            Term_fresh();
            
            /* Get the key to bind */
            flush();
            char bind_key = inkey();
            
            if (bind_key != ESCAPE && bind_key != 0)
            {
                byte new_key = (byte)bind_key;
                
                /* Clear any existing action on the chosen key */
                keymap_act[mode][new_key] = str_free(keymap_act[mode][new_key]);
                keymap_act[mode][new_key] = str_dup(action);
                dirty = true;
                
                describe_keycode(new_key, key_label, sizeof(key_label));
                msg_format("Key %s now performs %s", key_label, keybinds[highlight].key_name);
                message_flush();
            }
        }
        else if (ch == 'r' || ch == 'R')
        {
            /* Reset to default */
            byte target_key = keybinds[highlight].key_code;
            char key_label[32];
            cptr action = keybinds[highlight].action;

            /* Remove the action from any custom keys */
            unbind_action(mode, action);
            
            /* Restore default action */
            keymap_act[mode][target_key] = str_free(keymap_act[mode][target_key]);
            if (keybinds[highlight].requires_keymap)
                keymap_act[mode][target_key] = str_dup(action);
            
            dirty = true;
            
            describe_keycode(target_key, key_label, sizeof(key_label));
            msg_format("Reset %s to default key %s", keybinds[highlight].key_name, key_label);
            message_flush();
        }
        else if (ch == 's' || ch == 'S')
        {
#ifdef ALLOW_MACROS
            /* Save keybinds to file */
            char ftmp[80];
            
            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s", default_file);
            
            /* Clear prompt area */
            prt("                                                              ", list_start_row + visible_rows + 1, 2);
            prt("File: ", list_start_row + visible_rows + 1, 2);
            
            /* Ask for a file */
            if (askfor_aux(ftmp, sizeof(ftmp)))
            {
                /* Dump the keymaps */
                if (keymap_dump(ftmp) == 0)
                {
                    msg_format("Keybinds saved to %s.", ftmp);
                    dirty = false;
                }
                else
                {
                    msg_print("Failed to save keybinds.");
                }
                message_flush();
            }
#else
            msg_print("Saving keybinds is not available in this build.");
            message_flush();
#endif
        }
        
        /* Store updated highlight for the active group */
        *highlight_ptr = highlight;
    }
    
    /* Load screen */
    screen_load();

    if (dirty)
    {
        char prompt[80];
        strnfmt(prompt, sizeof(prompt), "Save keybinds to %s? ", default_file);
        if (get_check(prompt))
        {
            if (keymap_dump(default_file) == 0)
            {
                msg_format("Keybinds saved to %s.", default_file);
                message_flush();
            }
            else
            {
                msg_print("Failed to save keybinds.");
                message_flush();
            }
        }
    }
}

typedef enum controller_entry_type {
    CONTROLLER_ENTRY_TOGGLE = 0,
    CONTROLLER_ENTRY_ACTION,
} controller_entry_type;

typedef enum controller_toggle_id {
    CONTROLLER_TOGGLE_ENABLED = 0,
    CONTROLLER_TOGGLE_AUTO_MODE,
    CONTROLLER_TOGGLE_STEAMDECK_MODE,
    CONTROLLER_TOGGLE_DPAD,
    CONTROLLER_TOGGLE_LEFT_STICK,
} controller_toggle_id;

typedef struct controller_entry {
    controller_entry_type type;
    int id;
    const char* label;
} controller_entry;

static const char* controller_gamepad_button_label(int button)
{
    switch (button) {
    case GAMEPAD_BUTTON_SOUTH: return "A (South)";
    case GAMEPAD_BUTTON_EAST: return "B (East)";
    case GAMEPAD_BUTTON_WEST: return "X (West)";
    case GAMEPAD_BUTTON_NORTH: return "Y (North)";
    case GAMEPAD_BUTTON_LEFT_SHOULDER: return "L1 (Left Shoulder)";
    case GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R1 (Right Shoulder)";
    case GAMEPAD_BUTTON_LEFT_PADDLE1: return "L4 (Left Paddle 1)";
    case GAMEPAD_BUTTON_LEFT_PADDLE2: return "L5 (Left Paddle 2)";
    case GAMEPAD_BUTTON_RIGHT_PADDLE1: return "R4 (Right Paddle 1)";
    case GAMEPAD_BUTTON_RIGHT_PADDLE2: return "R5 (Right Paddle 2)";
    case GAMEPAD_BUTTON_START: return "Start (Menu)";
    case GAMEPAD_BUTTON_BACK: return "Back (View)";
    case GAMEPAD_BUTTON_LEFT_STICK: return "Left Stick Click";
    case GAMEPAD_BUTTON_RIGHT_STICK: return "Right Stick Click";
    case GAMEPAD_BUTTON_GUIDE: return "Guide (Steam)";
    case GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad Click";
    case GAMEPAD_BUTTON_DPAD_UP: return "D-pad Up";
    case GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad Down";
    case GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad Left";
    case GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad Right";
    case GAMEPAD_BUTTON_MISC1: return "Misc1";
    case GAMEPAD_BUTTON_MISC2: return "Misc2";
    case GAMEPAD_BUTTON_MISC3: return "Misc3";
    case GAMEPAD_BUTTON_MISC4: return "Misc4";
    case GAMEPAD_BUTTON_MISC5: return "Misc5";
    case GAMEPAD_BUTTON_MISC6: return "Misc6";
    default: return "Unknown Button";
    }
}

static const char* controller_gamepad_trigger_label(int index)
{
    if (index == 0)
        return "L2 (Left Trigger)";
    if (index == 1)
        return "R2 (Right Trigger)";
    return "Unknown Trigger";
}

static const char* controller_gamepad_stick_dir_label(int type, int dir)
{
    const char* stick = (type == GAMEPAD_CAPTURE_RIGHT_STICK) ? "Right Stick" : "Left Stick";
    const char* dir_label = NULL;

    switch (dir) {
    case GAMEPAD_STICK_DIR_UP: dir_label = "Up"; break;
    case GAMEPAD_STICK_DIR_DOWN: dir_label = "Down"; break;
    case GAMEPAD_STICK_DIR_LEFT: dir_label = "Left"; break;
    case GAMEPAD_STICK_DIR_RIGHT: dir_label = "Right"; break;
    default: dir_label = "Unknown"; break;
    }

    return format("%s %s", stick, dir_label);
}

static const char* controller_gamepad_combo_label(void)
{
    return "L1+R1 Combo";
}

static void controller_binding_label(int type, int id, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (type == GAMEPAD_CAPTURE_BUTTON) {
        SDL_strlcpy(buf, controller_gamepad_button_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_TRIGGER) {
        SDL_strlcpy(buf, controller_gamepad_trigger_label(id), buflen);
    } else if (type == GAMEPAD_CAPTURE_LEFT_STICK || type == GAMEPAD_CAPTURE_RIGHT_STICK) {
        SDL_strlcpy(buf, controller_gamepad_stick_dir_label(type, id), buflen);
    } else if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO) {
        SDL_strlcpy(buf, controller_gamepad_combo_label(), buflen);
    } else {
        SDL_strlcpy(buf, "(unknown)", buflen);
    }
}

static int controller_action_binding_count(int binding, int* out_type, int* out_id)
{
    int count = 0;

    for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++) {
        if (get_sdl_gamepad_button_binding(i) == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_BUTTON;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (get_sdl_gamepad_trigger_binding(i) == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_TRIGGER;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_left_stick_binding(i) == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_LEFT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_right_stick_binding(i) == binding) {
            if (count == 0 && out_type && out_id) {
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
                *out_id = i;
            }
            count++;
        }
    }

    if (get_sdl_gamepad_shoulder_combo_binding() == binding) {
        if (count == 0 && out_type && out_id) {
            *out_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
            *out_id = 0;
        }
        count++;
    }

    return count;
}

static void controller_action_binding_label(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    int type = 0;
    int id = 0;
    int count = controller_action_binding_count(binding, &type, &id);
    if (count <= 0) {
        SDL_strlcpy(buf, "(unbound)", buflen);
    } else if (count == 1) {
        controller_binding_label(type, id, buf, buflen);
    } else {
        SDL_strlcpy(buf, "Multiple", buflen);
    }
}

static bool controller_binding_matches_action(int binding, int type, int id)
{
    if (type == GAMEPAD_CAPTURE_BUTTON)
        return get_sdl_gamepad_button_binding(id) == binding;
    if (type == GAMEPAD_CAPTURE_TRIGGER)
        return get_sdl_gamepad_trigger_binding(id) == binding;
    if (type == GAMEPAD_CAPTURE_LEFT_STICK)
        return get_sdl_gamepad_left_stick_binding(id) == binding;
    if (type == GAMEPAD_CAPTURE_RIGHT_STICK)
        return get_sdl_gamepad_right_stick_binding(id) == binding;
    if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO)
        return get_sdl_gamepad_shoulder_combo_binding() == binding;
    return false;
}

static void controller_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple")) {
        SDL_strlcpy(buf, fallback, buflen);
    }
}

static void controller_entry_value(const controller_entry* entry, char* buf, size_t buflen)
{
    if (!entry || !buf || !buflen)
        return;

    switch (entry->type) {
    case CONTROLLER_ENTRY_TOGGLE:
        switch (entry->id) {
        case CONTROLLER_TOGGLE_ENABLED:
            SDL_strlcpy(buf, get_sdl_gamepad_enabled() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_AUTO_MODE:
            SDL_strlcpy(buf, get_sdl_gamepad_auto_mode() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_STEAMDECK_MODE:
            SDL_strlcpy(buf, get_sdl_steamdeck_mode() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_DPAD:
            SDL_strlcpy(buf, get_sdl_gamepad_use_dpad() ? "On" : "Off", buflen);
            break;
        case CONTROLLER_TOGGLE_LEFT_STICK:
            SDL_strlcpy(buf, get_sdl_gamepad_use_left_stick() ? "On" : "Off", buflen);
            break;
        default:
            SDL_strlcpy(buf, "(unknown)", buflen);
            break;
        }
        break;
    case CONTROLLER_ENTRY_ACTION:
        controller_action_binding_label(entry->id, buf, buflen);
        break;
    default:
        SDL_strlcpy(buf, "(unknown)", buflen);
        break;
    }
}

static void controller_set_toggle(int toggle_id, bool value)
{
    switch (toggle_id) {
    case CONTROLLER_TOGGLE_ENABLED:
        set_sdl_gamepad_enabled(value);
        break;
    case CONTROLLER_TOGGLE_AUTO_MODE:
        set_sdl_gamepad_auto_mode(value);
        break;
    case CONTROLLER_TOGGLE_STEAMDECK_MODE:
        set_sdl_steamdeck_mode(value);
        break;
    case CONTROLLER_TOGGLE_DPAD:
        set_sdl_gamepad_use_dpad(value);
        break;
    case CONTROLLER_TOGGLE_LEFT_STICK:
        set_sdl_gamepad_use_left_stick(value);
        break;
    default:
        break;
    }
}

static void controller_clear_action_bindings(int binding, int skip_type, int skip_id)
{
    if (binding == GAMEPAD_BIND_NONE)
        return;

    for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++) {
        if (get_sdl_gamepad_button_binding(i) == binding) {
            if (skip_type == GAMEPAD_CAPTURE_BUTTON && skip_id == i)
                continue;
            set_sdl_gamepad_button_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (get_sdl_gamepad_trigger_binding(i) == binding) {
            if (skip_type == GAMEPAD_CAPTURE_TRIGGER && skip_id == i)
                continue;
            set_sdl_gamepad_trigger_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_left_stick_binding(i) == binding) {
            if (skip_type == GAMEPAD_CAPTURE_LEFT_STICK && skip_id == i)
                continue;
            set_sdl_gamepad_left_stick_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_right_stick_binding(i) == binding) {
            if (skip_type == GAMEPAD_CAPTURE_RIGHT_STICK && skip_id == i)
                continue;
            set_sdl_gamepad_right_stick_binding(i, GAMEPAD_BIND_NONE);
        }
    }

    if (get_sdl_gamepad_shoulder_combo_binding() == binding) {
        if (!(skip_type == GAMEPAD_CAPTURE_SHOULDER_COMBO))
            set_sdl_gamepad_shoulder_combo_binding(GAMEPAD_BIND_NONE);
    }
}

static void controller_assign_action_binding(int binding, int type, int id)
{
    controller_clear_action_bindings(binding, type, id);

    if (type == GAMEPAD_CAPTURE_BUTTON) {
        set_sdl_gamepad_button_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_TRIGGER) {
        set_sdl_gamepad_trigger_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_LEFT_STICK) {
        set_sdl_gamepad_left_stick_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_RIGHT_STICK) {
        set_sdl_gamepad_right_stick_binding(id, binding);
    } else if (type == GAMEPAD_CAPTURE_SHOULDER_COMBO) {
        set_sdl_gamepad_shoulder_combo_binding(binding);
    }
}

static bool controller_action_default_binding(int binding, int* out_type, int* out_id)
{
    for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++) {
        if (get_sdl_gamepad_default_button_binding(i) == binding) {
            if (out_type)
                *out_type = GAMEPAD_CAPTURE_BUTTON;
            if (out_id)
                *out_id = i;
            return true;
        }
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (get_sdl_gamepad_default_trigger_binding(i) == binding) {
            if (out_type)
                *out_type = GAMEPAD_CAPTURE_TRIGGER;
            if (out_id)
                *out_id = i;
            return true;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_default_left_stick_binding(i) == binding) {
            if (out_type)
                *out_type = GAMEPAD_CAPTURE_LEFT_STICK;
            if (out_id)
                *out_id = i;
            return true;
        }
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (get_sdl_gamepad_default_right_stick_binding(i) == binding) {
            if (out_type)
                *out_type = GAMEPAD_CAPTURE_RIGHT_STICK;
            if (out_id)
                *out_id = i;
            return true;
        }
    }

    if (get_sdl_gamepad_default_shoulder_combo_binding() == binding) {
        if (out_type)
            *out_type = GAMEPAD_CAPTURE_SHOULDER_COMBO;
        if (out_id)
            *out_id = 0;
        return true;
    }

    return false;
}

void do_cmd_controller_settings(void)
{
    bool done = false;
    int highlight = 0;
    int top = 0;
    int term_w, term_h;
    const int list_start_row = 5;

    static const controller_entry entries[] = {
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_ENABLED, "Controller Input" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_AUTO_MODE, "Auto Controller Mode" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_STEAMDECK_MODE, "Steam Deck UI Mode" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_DPAD, "D-pad Movement" },
        { CONTROLLER_ENTRY_TOGGLE, CONTROLLER_TOGGLE_LEFT_STICK, "Left Stick Movement" },
        { CONTROLLER_ENTRY_ACTION, ' ', "Confirm (Space)" },
        { CONTROLLER_ENTRY_ACTION, '\r', "Enter" },
        { CONTROLLER_ENTRY_ACTION, ESCAPE, "Escape" },
        { CONTROLLER_ENTRY_ACTION, '\t', "Abilities (Tab)" },
        { CONTROLLER_ENTRY_ACTION, 'i', "Inventory" },
        { CONTROLLER_ENTRY_ACTION, 'e', "Equipment" },
        { CONTROLLER_ENTRY_ACTION, 'u', "Use item" },
        { CONTROLLER_ENTRY_ACTION, 'x', "Examine item" },
        { CONTROLLER_ENTRY_ACTION, 's', "Sing / change song" },
        { CONTROLLER_ENTRY_ACTION, 'S', "Toggle stealth" },
        { CONTROLLER_ENTRY_ACTION, 'h', "Character sheet" },
        { CONTROLLER_ENTRY_ACTION, 'f', "Fire (primary)" },
        { CONTROLLER_ENTRY_ACTION, 'F', "Fire (secondary)" },
        { CONTROLLER_ENTRY_ACTION, 'l', "Look around" },
        { CONTROLLER_ENTRY_ACTION, 'T', "Tunnel / dig" },
        { CONTROLLER_ENTRY_ACTION, 'b', "Bash door" },
        { CONTROLLER_ENTRY_ACTION, 'z', "Wait" },
        { CONTROLLER_ENTRY_ACTION, 'j', "Supplies overview" },
        { CONTROLLER_ENTRY_ACTION, '.', "Run" },
        { CONTROLLER_ENTRY_ACTION, '/', "Alt action" },
        { CONTROLLER_ENTRY_ACTION, 'w', "Wear / wield" },
        { CONTROLLER_ENTRY_ACTION, 'r', "Remove equipment" },
        { CONTROLLER_ENTRY_ACTION, 'd', "Drop item" },
        { CONTROLLER_ENTRY_ACTION, 'k', "Destroy item" },
        { CONTROLLER_ENTRY_ACTION, 'g', "Pick up items" },
        { CONTROLLER_ENTRY_ACTION, 'Z', "Rest" },
        { CONTROLLER_ENTRY_ACTION, 'o', "Open door / chest" },
        { CONTROLLER_ENTRY_ACTION, 'c', "Close door" },
        { CONTROLLER_ENTRY_ACTION, 'D', "Disarm trap / chest" },
        { CONTROLLER_ENTRY_ACTION, 'X', "Exchange places" },
        { CONTROLLER_ENTRY_ACTION, '-', "Fletch arrows" },
        { CONTROLLER_ENTRY_ACTION, '{', "Inscribe item" },
        { CONTROLLER_ENTRY_ACTION, 'a', "Activate staff" },
        { CONTROLLER_ENTRY_ACTION, 'E', "Eat food" },
        { CONTROLLER_ENTRY_ACTION, 't', "Throw item" },
        { CONTROLLER_ENTRY_ACTION, 'p', "Blow horn" },
        { CONTROLLER_ENTRY_ACTION, 'q', "Quaff potion" },
        { CONTROLLER_ENTRY_ACTION, 'M', "View map" },
        { CONTROLLER_ENTRY_ACTION, 'L', "Pan view" },
        { CONTROLLER_ENTRY_ACTION, '0', "Smithing screen" },
        { CONTROLLER_ENTRY_ACTION, '<', "Go upstairs" },
        { CONTROLLER_ENTRY_ACTION, '>', "Go downstairs" },
        { CONTROLLER_ENTRY_ACTION, 'm', "Main menu" },
        { CONTROLLER_ENTRY_ACTION, '?', "Help" },
        { CONTROLLER_ENTRY_ACTION, 'O', "Options menu" },
        { CONTROLLER_ENTRY_ACTION, ':', "Take notes" },
        { CONTROLLER_ENTRY_ACTION, '~', "Knowledge browser" },
        { CONTROLLER_ENTRY_ACTION, '[', "Monster list" },
        { CONTROLLER_ENTRY_ACTION, ']', "Object list" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_SHIFT, "Shift modifier" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_CTRL, "Ctrl modifier" },
        { CONTROLLER_ENTRY_ACTION, GAMEPAD_BIND_ALT, "Alt modifier" },
    };

    int entry_count = (int)N_ELEMENTS(entries);

    screen_save();

    while (!done) {
        char value_buf[64];
        char line_buf[128];
        int row;
        bool steamdeck = steamdeck_controls_active();
        bool compact_width;
        int row_width;

        Term_get_size(&term_w, &term_h);
        row_width = settings_ui_line_width(2);
        int visible_rows = term_h - list_start_row - 6;
        if (visible_rows < 5)
            visible_rows = 5;
        compact_width = (term_w < 70);

        if (highlight < 0)
            highlight = 0;
        if (highlight >= entry_count)
            highlight = entry_count - 1;

        if (top > highlight)
            top = highlight;
        if (top + visible_rows <= highlight)
            top = highlight - visible_rows + 1;
        if (top < 0)
            top = 0;
        if (entry_count > visible_rows) {
            int max_top = entry_count - visible_rows;
            if (top > max_top)
                top = max_top;
        } else {
            top = 0;
        }

        Term_clear();
        settings_ui_put_fitted(1, 0, TERM_WHITE, "Controller Settings");
        if (steamdeck) {
            char confirm_label[16];
            char back_label[16];
            char prompt_buf[80];
            /* Steam Deck UI: A=bind, B=back */
            controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
            controller_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                compact_width ? "D-pad %s bind  %s back"
                              : "D-pad navigate  %s bind  %s back",
                confirm_label, back_label);
            settings_ui_put_fitted(2, 0, TERM_WHITE, prompt_buf);
        } else {
            settings_ui_put_fitted(2, 0, TERM_WHITE,
                compact_width ? "8/2 move  Enter bind  Esc return"
                              : "Arrow to navigate, Enter to bind, Escape to return");
        }

        for (int i = top; i < entry_count && i < top + visible_rows; i++) {
            int entry_row = list_start_row + (i - top);
            controller_entry_value(&entries[i], value_buf, sizeof(value_buf));
            settings_ui_format_pair_line(line_buf, sizeof(line_buf), entries[i].label,
                value_buf, row_width, 12);

            if (i == highlight) {
                c_prt(TERM_L_BLUE, line_buf, entry_row, 2);
            } else {
                prt(line_buf, entry_row, 2);
            }
        }

        for (row = list_start_row + (entry_count - top); row < list_start_row + visible_rows; row++) {
            Term_erase(2, row, term_w > 2 ? term_w - 2 : 0);
        }

        if (steamdeck) {
            char reset_label[16];
            char reset_all_label[16];
            char prompt_buf[80];
            /* Steam Deck UI: X=reset selected, Y=reset all */
            controller_prompt_label(steamdeck_alt_action_key(), "X", reset_label, sizeof(reset_label));
            controller_prompt_label(steamdeck_secondary_key(), "Y", reset_all_label, sizeof(reset_all_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                compact_width ? "[%s] reset  [%s] reset all"
                              : "Reset: [%s] selected, [%s] all",
                reset_label, reset_all_label);
            settings_ui_put_fitted(list_start_row + visible_rows + 1, 2, TERM_WHITE,
                prompt_buf);
        } else {
            settings_ui_put_fitted(list_start_row + visible_rows + 1, 2, TERM_WHITE,
                compact_width ? "r: reset selected  R: reset all"
                              : "Press 'r' to reset selected binding, 'R' to reset all bindings");
        }
        settings_ui_put_fitted(list_start_row + visible_rows + 2, 2, TERM_WHITE,
            compact_width ? "Saves on exit." : "Changes are saved on exit.");

        char ch = inkey();

        if (ch == ESCAPE || ch == 'q' || ch == 'Q' || (steamdeck && ch == steamdeck_back_key())) {
            done = true;
        } else if (ch == '8') {
            highlight = (highlight + entry_count - 1) % entry_count;
        } else if (ch == '2') {
            highlight = (highlight + 1) % entry_count;
        } else if (ch == 'r' || (steamdeck && ch == steamdeck_alt_action_key())) {
            if (entries[highlight].type == CONTROLLER_ENTRY_ACTION) {
                int binding_type = 0;
                int binding_id = 0;
                if (controller_action_default_binding(entries[highlight].id, &binding_type, &binding_id)) {
                    controller_assign_action_binding(entries[highlight].id, binding_type, binding_id);
                    msg_print("Binding reset to default.");
                } else {
                    controller_clear_action_bindings(entries[highlight].id, -1, -1);
                    msg_print("No default binding for action.");
                }
                message_flush();
            }
        } else if (ch == 'R' || (steamdeck && ch == steamdeck_secondary_key())) {
            sdl_gamepad_reset_bindings_to_default();
            msg_print("All bindings reset to defaults.");
            message_flush();
        } else if (ch == '\r' || ch == '\n' || ch == ' ') {
            const controller_entry* entry = &entries[highlight];
            int entry_row = list_start_row + (highlight - top);

            if (entry->type == CONTROLLER_ENTRY_TOGGLE) {
                char cur[16];
                controller_entry_value(entry, cur, sizeof(cur));
                controller_set_toggle(entry->id, streq(cur, "Off"));
            } else {
                char prompt[80];
                char prompt_long[96];
                char prompt_medium[80];
                char prompt_short[64];
                int cap_type = 0;
                int cap_id = 0;
                Term_erase(2, entry_row, 255);
                if (steamdeck) {
                    char cancel_label[16];
                    controller_prompt_label(steamdeck_back_key(), "B", cancel_label, sizeof(cancel_label));
                    strnfmt(prompt_long, sizeof(prompt_long),
                        "Press controller button for %s  (%s=cancel)",
                        entry->label, cancel_label);
                    strnfmt(prompt_medium, sizeof(prompt_medium),
                        "Press button for %s  (%s=cancel)",
                        entry->label, cancel_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "Bind %s  (%s cancel)", entry->label, cancel_label);
                } else {
                    strnfmt(prompt_long, sizeof(prompt_long),
                        "Press controller button for %s (Esc=cancel, Backspace=clear)",
                        entry->label);
                    strnfmt(prompt_medium, sizeof(prompt_medium),
                        "Bind %s (Esc=cancel, Bksp=clear)", entry->label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "%s (Esc cancel, Bksp clear)", entry->label);
                }
                strnfmt(prompt, sizeof(prompt), "%s",
                    settings_ui_pick_label(row_width, prompt_long, prompt_medium,
                        prompt_short));
                settings_ui_put_fitted(entry_row, 2, TERM_YELLOW, prompt);
                Term_fresh();

                flush();
                if (!sdl_gamepad_capture_begin()) {
                    msg_print("No controller detected.");
                    message_flush();
                    continue;
                }

                bool waiting = true;
                while (waiting) {
                    if (sdl_gamepad_capture_poll(&cap_type, &cap_id)) {
                        if (controller_binding_matches_action(ESCAPE, cap_type, cap_id)) {
                            sdl_gamepad_capture_cancel();
                            waiting = false;
                            break;
                        }
                        controller_assign_action_binding(entry->id, cap_type, cap_id);
                        waiting = false;
                        break;
                    }

                    inkey_set_scan(true);
                    char choice = inkey();
                    if (choice == ESCAPE) {
                        sdl_gamepad_capture_cancel();
                        waiting = false;
                    } else if (choice == '\b' || choice == 127) {
                        sdl_gamepad_capture_cancel();
                        controller_clear_action_bindings(entry->id, -1, -1);
                        waiting = false;
                    } else if (choice == 0) {
                        Term_xtra(TERM_XTRA_DELAY, 10);
                    }
                }
            }
        }
    }

    screen_load();
}

#ifdef ALLOW_MACROS

/*
 * Hack -- append all current macros to the given file
 */
static errr macro_dump(cptr fname)
{
    static cptr mark = "Macro Dump";

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("macro_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old macros */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = sdl_fopen(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    /* Start dumping */
    SDL_IOprintf(fff, "# Automatic macro dump\n\n");

    /* Dump them */
    for (i = 0; i < macro__num; i++)
    {
        /* Start the macro */
        SDL_IOprintf(fff, "# Macro '%d'\n\n", i);

        /* Extract the macro action */
        ascii_to_text(buf, sizeof(buf), macro__act[i]);

        /* Dump the macro action */
        SDL_IOprintf(fff, "A:%s\n", buf);

        /* Extract the macro pattern */
        ascii_to_text(buf, sizeof(buf), macro__pat[i]);

        /* Dump the macro pattern */
        SDL_IOprintf(fff, "P:%s\n", buf);

        /* End the macro */
        SDL_IOprintf(fff, "\n\n");
    }

    /* Start dumping */
    SDL_IOprintf(fff, "\n\n\n\n");

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

/*
 * Hack -- ask for a "trigger" (see below)
 *
 * Note the complex use of the "inkey()" function from "util.c".
 *
 * Note that both "flush()" calls are extremely important.  This may
 * no longer be true, since "util.c" is much simpler now.  XXX XXX XXX
 */
static void do_cmd_macro_aux(char* buf)
{
    char ch;

    int n = 0;

    char tmp[1024];

    /* Flush */
    flush();

    /* Do not process macros */
    inkey_set_base(true);

    /* First key */
    ch = inkey();

    /* Read the pattern */
    while (ch != '\0')
    {
        /* Save the key */
        buf[n++] = ch;

        /* Do not process macros */
        inkey_set_base(true);

        /* Do not wait for keys */
        inkey_set_scan(true);

        /* Attempt to read a key */
        ch = inkey();
    }

    /* Terminate */
    buf[n] = '\0';

    /* Flush */
    flush();

    /* Convert the trigger */
    ascii_to_text(tmp, sizeof(tmp), buf);

    /* Hack -- display the trigger */
    Term_addstr(-1, TERM_WHITE, tmp);
}

/*
 * Hack -- ask for a keymap "trigger" (see below)
 *
 * Note that both "flush()" calls are extremely important.  This may
 * no longer be true, since "util.c" is much simpler now.  XXX XXX XXX
 */
static void do_cmd_macro_aux_keymap(char* buf)
{
    char tmp[1024];

    /* Flush */
    flush();

    /* Get a key */
    buf[0] = inkey();
    buf[1] = '\0';

    /* Convert to ascii */
    ascii_to_text(tmp, sizeof(tmp), buf);

    /* Hack -- display the trigger */
    Term_addstr(-1, TERM_WHITE, tmp);

    /* Flush */
    flush();
}

/*
 * Hack -- Append all keymaps to the given file.
 *
 * Hack -- We only append the keymaps for the "active" mode.
 */
static errr keymap_dump(cptr fname)
{
    static cptr mark = "Keymap Dump";

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    int mode;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* Build the filename */
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname))
    {
        log_error("keymap_dump: failed to build path for '%s'", fname);
        return (-1);
    }

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Remove old keymaps */
    remove_old_dump(buf, mark);

    /* Append to the file */
    fff = sdl_fopen(buf, "a");

    /* Failure */
    if (!fff)
        return (-1);

    /* Output header */
    pref_header(fff, mark);

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n");

    /* Start dumping */
    SDL_IOprintf(fff, "# Automatic keymap dump\n\n");

    /* Dump them */
    for (i = 0; i < (int)N_ELEMENTS(keymap_act[mode]); i++)
    {
        char key[2] = "?";

        cptr act;

        /* Loop up the keymap */
        act = keymap_act[mode][i];

        /* Skip empty keymaps */
        if (!act)
            continue;

        /* Encode the action */
        ascii_to_text(buf, sizeof(buf), act);

        /* Dump the keymap action */
        SDL_IOprintf(fff, "A:%s\n", buf);

        /* Convert the key into a string */
        key[0] = i;

        /* Encode the key */
        ascii_to_text(buf, sizeof(buf), key);

        /* Dump the keymap pattern */
        SDL_IOprintf(fff, "C:%d:%s\n", mode, buf);

        /* Skip a line */
        SDL_IOprintf(fff, "\n");
    }

    /* Skip some lines */
    SDL_IOprintf(fff, "\n\n\n");

    /* Output footer */
    pref_footer(fff, mark);

    /* Close */
    sdl_fclose(fff);

    /* Success */
    return (0);
}

#endif

/*
 * Interact with "macros"
 *
 * Could use some helpful instructions on this page.  XXX XXX XXX
 */
void do_cmd_macros(void)
{
    char ch;

    char tmp[1024];

    char pat[1024];

    int mode;

    // Determine the keyset
    if (!hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL;
    else if (hjkl_movement && !angband_keyset)
        mode = KEYMAP_MODE_SIL_HJKL;
    else if (!hjkl_movement && angband_keyset)
        mode = KEYMAP_MODE_ANGBAND;
    else
        mode = KEYMAP_MODE_ANGBAND_HJKL;

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Clear any active banner before opening macros menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Process requests until done */
    while (1)
    {
        int term_wid = 80;
        int term_hgt = 24;
        int title_row = 1;
        int menu_row = 3;
        int action_label_row;
        int action_row;
        int command_row;
        int input_row;

        Term_get_size(&term_wid, &term_hgt);
        action_label_row = MAX(menu_row + 11, term_hgt - 4);
        action_row = MIN(term_hgt - 2, action_label_row + 1);
        command_row = MAX(action_row + 1, term_hgt - 2);
        input_row = MAX(command_row + 1, term_hgt - 1);

        /* Clear screen */
        Term_clear();

        /* Describe */
        prt("Interact with Macros", title_row, 0);

        /* Describe that action */
        prt("Current action:", action_label_row, 0);

        /* Analyze the current action */
        ascii_to_text(tmp, sizeof(tmp), macro_buffer);

        /* Display the current action */
        Term_putstr(0, action_row, term_wid, TERM_WHITE, tmp);

        /* Selections */
        prt("(1) Load a user pref file", menu_row, 5);
#ifdef ALLOW_MACROS
        prt("(2) Append macros to a file", menu_row + 1, 5);
        prt("(3) Query a macro", menu_row + 2, 5);
        prt("(4) Create a macro", menu_row + 3, 5);
        prt("(5) Remove a macro", menu_row + 4, 5);
        prt("(6) Append keymaps to a file", menu_row + 5, 5);
        prt("(7) Query a keymap", menu_row + 6, 5);
        prt("(8) Create a keymap", menu_row + 7, 5);
        prt("(9) Remove a keymap", menu_row + 8, 5);
        prt("(0) Enter a new action", menu_row + 9, 5);
#endif /* ALLOW_MACROS */

        /* Prompt */
        prt("Command: ", command_row, 0);

        /* Get a command */
        ch = inkey();

        /* Leave */
        if (ch == ESCAPE)
            break;

        /* Load a user pref file */
        if (ch == '1')
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(command_row);
        }

#ifdef ALLOW_MACROS

        /* Save macros */
        else if (ch == '2')
        {
            char ftmp[80];

            /* Prompt */
            prt("Command: Append macros to a file", command_row, 0);

            /* Prompt */
            prt("File: ", input_row, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Ask for a file */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Dump the macros */
            (void)macro_dump(ftmp);

            /* Prompt */
            msg_print("Appended macros.");
        }

        /* Query a macro */
        else if (ch == '3')
        {
            int k;

            /* Prompt */
            prt("Command: Query a macro", command_row, 0);

            /* Prompt */
            prt("Trigger: ", input_row, 0);

            /* Get a macro trigger */
            do_cmd_macro_aux(pat);

            /* Get the action */
            k = macro_find_exact(pat);

            /* Nothing found */
            if (k < 0)
            {
                /* Prompt */
                msg_print("Found no macro.");
            }

            /* Found one */
            else
            {
                /* Obtain the action */
                SDL_strlcpy(macro_buffer, macro__act[k], sizeof(macro_buffer));

                /* Analyze the current action */
                ascii_to_text(tmp, sizeof(tmp), macro_buffer);

                /* Display the current action */
                Term_putstr(0, action_row, term_wid, TERM_WHITE, tmp);

                /* Prompt */
                msg_print("Found a macro.");
            }
        }

        /* Create a macro */
        else if (ch == '4')
        {
            /* Prompt */
            prt("Command: Create a macro", command_row, 0);

            /* Prompt */
            prt("Trigger: ", input_row, 0);

            /* Get a macro trigger */
            do_cmd_macro_aux(pat);

            /* Clear */
            clear_from(action_label_row);

            /* Prompt */
            prt("Action: ", action_row, 0);

            /* Convert to text */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (askfor_aux(tmp, 80))
            {
                /* Convert to ascii */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

                /* Link the macro */
                macro_add(pat, macro_buffer);

                /* Prompt */
                msg_print("Added a macro.");
            }
        }

        /* Remove a macro */
        else if (ch == '5')
        {
            /* Prompt */
            prt("Command: Remove a macro", command_row, 0);

            /* Prompt */
            prt("Trigger: ", input_row, 0);

            /* Get a macro trigger */
            do_cmd_macro_aux(pat);

            /* Link the macro */
            macro_add(pat, pat);

            /* Prompt */
            msg_print("Removed a macro.");
        }

        /* Save keymaps */
        else if (ch == '6')
        {
            char ftmp[80];

            /* Prompt */
            prt("Command: Append keymaps to a file", command_row, 0);

            /* Prompt */
            prt("File: ", input_row, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Ask for a file */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Dump the macros */
            (void)keymap_dump(ftmp);

            /* Prompt */
            msg_print("Appended keymaps.");
        }

        /* Query a keymap */
        else if (ch == '7')
        {
            cptr act;

            /* Prompt */
            prt("Command: Query a keymap", command_row, 0);

            /* Prompt */
            prt("Keypress: ", input_row, 0);

            /* Get a keymap trigger */
            do_cmd_macro_aux_keymap(pat);

            /* Look up the keymap */
            act = keymap_act[mode][(byte)(pat[0])];

            /* Nothing found */
            if (!act)
            {
                /* Prompt */
                msg_print("Found no keymap.");
            }

            /* Found one */
            else
            {
                /* Obtain the action */
                SDL_strlcpy(macro_buffer, act, sizeof(macro_buffer));

                /* Analyze the current action */
                ascii_to_text(tmp, sizeof(tmp), macro_buffer);

                /* Display the current action */
                Term_putstr(0, action_row, term_wid, TERM_WHITE, tmp);

                /* Prompt */
                msg_print("Found a keymap.");
            }
        }

        /* Create a keymap */
        else if (ch == '8')
        {
            /* Prompt */
            prt("Command: Create a keymap", command_row, 0);

            /* Prompt */
            prt("Keypress: ", input_row, 0);

            /* Get a keymap trigger */
            do_cmd_macro_aux_keymap(pat);

            /* Clear */
            clear_from(action_label_row);

            /* Prompt */
            prt("Action: ", action_row, 0);

            /* Convert to text */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (askfor_aux(tmp, 80))
            {
                /* Convert to ascii */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

                /* Free old keymap */
                str_free(keymap_act[mode][(byte)(pat[0])]);

                /* Make new keymap */
                keymap_act[mode][(byte)(pat[0])] = str_dup(macro_buffer);

                /* Prompt */
                msg_print("Added a keymap.");
            }
        }

        /* Remove a keymap */
        else if (ch == '9')
        {
            /* Prompt */
            prt("Command: Remove a keymap", command_row, 0);

            /* Prompt */
            prt("Keypress: ", input_row, 0);

            /* Get a keymap trigger */
            do_cmd_macro_aux_keymap(pat);

            /* Free old keymap */
            str_free(keymap_act[mode][(byte)(pat[0])]);

            /* Make new keymap */
            keymap_act[mode][(byte)(pat[0])] = NULL;

            /* Prompt */
            msg_print("Removed a keymap.");
        }

        /* Enter a new action */
        else if (ch == '0')
        {
            /* Prompt */
            prt("Command: Enter a new action", command_row, 0);

            /* Go to the correct location */
            Term_gotoxy(0, action_row);

            /* Analyze the current action */
            ascii_to_text(tmp, sizeof(tmp), macro_buffer);

            /* Get an encoded action */
            if (askfor_aux(tmp, 80))
            {
                /* Extract an action */
                text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);
            }
        }

#endif /* ALLOW_MACROS */

        /* Oops */
        else
        {
            /* Oops */
            bell("Illegal command for macros!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_load();
}

/*
 * Asks to the player for an extended color. It is done in two steps:
 * 1. Asks for the base color.
 * 2. Asks for a specific shade.
 * It erases the given line.
 * If the user press ESCAPE no changes are made to attr.
 */
static void askfor_shade(byte* attr, int y)
{
    byte base, shade, temp;
    bool changed = false;
    char *msg, *pos;
    int ch;

    /* Start with the given base color */
    base = GET_BASE_COLOR(*attr);

    /* 1. Query for base color */
    while (1)
    {
        /* Clear the line */
        Term_erase(0, y, 255);

        /* Format the query */
        msg = format("1. Choose base color (use arrows) " COLOR_SAMPLE
                     " %s (attr = %d) ",
            color_names[base], base);

        /* Display it */
        c_put_str(TERM_WHITE, msg, y, 0);

        /* Find the sample */
        pos = strstr(msg, COLOR_SAMPLE);

        /* Show it using the proper color */
        c_put_str(base, COLOR_SAMPLE, y, pos - msg);

        /* Place the cursor at the end of the message */
        Term_gotoxy(strlen(msg), y);

        /* Get a command */
        ch = inkey();

        /* Cancel */
        if (ch == ESCAPE)
        {
            /* Clear the line */
            Term_erase(0, y, 255);
            return;
        }

        /* Accept the current base color */
        if ((ch == '\r') || (ch == '\n'))
            break;

        /* Move to the previous color if possible */
        if ((ch == '4') && (base > 0))
        {
            --base;
            /* Reset the shade, see below */
            changed = true;
            continue;
        }

        /* Move to the next color if possible */
        if ((ch == '6') && (base < MAX_BASE_COLORS - 1))
        {
            ++base;
            /* Reset the shade, see below */
            changed = true;
            continue;
        }
    }

    /* The player selected a different base color, start from shade 0 */
    if (changed)
        shade = 0;
    /* We assume that the player is editing the current shade, go there */
    else
        shade = GET_SHADE(*attr);

    /* 2. Query for specific shade */
    while (1)
    {
        /* Clear the line */
        Term_erase(0, y, 255);

        /* Create the real color */
        temp = MAKE_EXTENDED_COLOR(base, shade);

        /* Format the message */
        msg = format("2. Choose shade (use arrows) " COLOR_SAMPLE
                     " %s (attr = %d) ",
            get_ext_color_name(temp), temp);

        /* Display it */
        c_put_str(TERM_WHITE, msg, y, 0);

        /* Find the sample */
        pos = strstr(msg, COLOR_SAMPLE);

        /* Show it using the proper color */
        c_put_str(temp, COLOR_SAMPLE, y, pos - msg);

        /* Place the cursor at the end of the message */
        Term_gotoxy(strlen(msg), y);

        /* Get a command */
        ch = inkey();

        /* Cancel */
        if (ch == ESCAPE)
        {
            /* Clear the line */
            Term_erase(0, y, 255);
            return;
        }

        /* Accept the current shade */
        if ((ch == '\r') || (ch == '\n'))
            break;

        /* Move to the previous shade if possible */
        if ((ch == '4') && (shade > 0))
        {
            --shade;
            continue;
        }

        /* Move to the next shade if possible */
        if ((ch == '6') && (shade < MAX_SHADES - 1))
        {
            ++shade;
            continue;
        }
    }

    /* Assign the selected shade */
    *attr = temp;

    /* Clear the line. It is needed to fit in the current UI */
    Term_erase(0, y, 255);
}

/*
 * Interact with "visuals"
 */
void do_cmd_visuals(void)
{
    int ch;
    int cx;

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Save screen */
    screen_save();

    /* Interact until done */
    while (1)
    {
        /* Clear screen */
        Term_clear();

        /* Ask for a choice */
        prt("Interact with Visuals", 2, 0);

        /* Give some choices */
        prt("(1) Load a user pref file", 4, 5);
#ifdef ALLOW_VISUALS
        prt("(2) Dump monster attr/chars", 5, 5);
        prt("(3) Dump object attr/chars", 6, 5);
        prt("(4) Dump feature attr/chars", 7, 5);
        prt("(5) Dump flavor attr/chars", 8, 5);
        prt("(6) Change monster attr/chars", 9, 5);
        prt("(7) Change object attr/chars", 10, 5);
        prt("(8) Change feature attr/chars", 11, 5);
        prt("(9) Change flavor attr/chars", 12, 5);
#endif
        prt("(0) Reset visuals", 13, 5);

        /* Prompt */
        prt("Command: ", 15, 0);

        /* Prompt */
        ch = inkey();

        /* Done */
        if (ch == ESCAPE)
            break;

        if ((ch >= '6') && (ch <= '9'))
        {
            int term_wid = 80;
            int term_hgt = 24;

            Term_get_size(&term_wid, &term_hgt);
            if ((term_wid < 60) || (term_hgt < 21))
            {
                msg_print("The attr/char editor requires a larger window than compact mode.");
                message_flush();
                continue;
            }
        }

        /* Load a user pref file */
        if (ch == '1')
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(15);
        }

#ifdef ALLOW_VISUALS

        /* Dump monster attr/chars */
        else if (ch == '2')
        {
            static cptr mark = "Monster attr/chars";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump monster attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_monsters: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Monster attr/char definitions\n\n");

            /* Dump monsters */
            for (i = 0; i < z_info->r_max; i++)
            {
                monster_race* r_ptr = &r_info[i];

                /* Skip non-entries */
                if (!r_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (r_name + r_ptr->name));

                /* Dump the monster attr/char info */
                dump_visual_pair(fff, "R", i, r_ptr->x_attr, (byte)r_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped monster attr/chars.");
        }

        /* Dump object attr/chars */
        else if (ch == '3')
        {
            static cptr mark = "Object attr/chars";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump object attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_objects: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Object attr/char definitions\n\n");

            /* Dump objects */
            for (i = 0; i < z_info->k_max; i++)
            {
                object_kind* k_ptr = &k_info[i];

                /* Skip non-entries */
                if (!k_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (k_name + k_ptr->name));

                /* Dump the object attr/char info */
                dump_visual_pair(
                    fff, "K", i, k_ptr->x_attr, (byte)k_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped object attr/chars.");
        }

        /* Dump feature attr/chars */
        else if (ch == '4')
        {
            static cptr mark = "Feature attr/chars";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump feature attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_features: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Feature attr/char definitions\n\n");

            /* Dump features */
            for (i = 0; i < z_info->f_max; i++)
            {
                feature_type* f_ptr = &f_info[i];

                /* Skip non-entries */
                if (!f_ptr->name)
                    continue;

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (f_name + f_ptr->name));

                /* Dump the feature attr/char info */
                dump_visual_pair(
                    fff, "F", i, f_ptr->x_attr, (byte)f_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped feature attr/chars.");
        }

        /* Dump flavor attr/chars */
        else if (ch == '5')
        {
            static cptr mark = "Flavor attr/chars";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump flavor attr/chars", 15, 0);

            /* Prompt */
            prt("File: ", 17, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_flavors: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Flavor attr/char definitions\n\n");

            /* Dump flavors */
            for (i = 0; i < z_info->flavor_max; i++)
            {
                flavor_type* flavor_ptr = &flavor_info[i];

                /* Dump a comment */
                SDL_IOprintf(fff, "# %s\n", (flavor_text + flavor_ptr->text));

                /* Dump the flavor attr/char info */
                dump_visual_pair(
                    fff, "L", i, flavor_ptr->x_attr, (byte)flavor_ptr->x_char);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped flavor attr/chars.");
        }

        /* Modify monster attr/chars */
        else if (ch == '6')
        {
            static int r = 0;

            /* Prompt */
            prt("Command: Change monster attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                monster_race* r_ptr = &r_info[r];

                byte da = (byte)(r_ptr->d_attr);
                byte dc = (byte)(r_ptr->d_char);
                byte ca = (byte)(r_ptr->x_attr);
                byte cc = (byte)(r_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Monster = %d, Name = %-40.40s", r,
                        (r_name + r_ptr->name)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3u / %3u", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3u / %3u", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = inkey();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    r = (r + z_info->r_max + 1) % z_info->r_max;
                if (cx == 'N')
                    r = (r + z_info->r_max - 1) % z_info->r_max;
                if (cx == 'a')
                    r_ptr->x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    r_ptr->x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    r_ptr->x_char = (byte)(cc + 1);
                if (cx == 'C')
                    r_ptr->x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&r_ptr->x_attr, 22);
                }
            }
        }

        /* Modify object attr/chars */
        else if (ch == '7')
        {
            static int k = 0;

            /* Prompt */
            prt("Command: Change object attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                object_kind* k_ptr = &k_info[k];

                byte da = (byte)(k_ptr->d_attr);
                byte dc = (byte)(k_ptr->d_char);
                byte ca = (byte)(k_ptr->x_attr);
                byte cc = (byte)(k_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Object = %d, Name = %-40.40s", k,
                        (k_name + k_ptr->name)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3d / %3d", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3d / %3d", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = inkey();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    k = (k + z_info->k_max + 1) % z_info->k_max;
                if (cx == 'N')
                    k = (k + z_info->k_max - 1) % z_info->k_max;
                if (cx == 'a')
                    k_info[k].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    k_info[k].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    k_info[k].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    k_info[k].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&k_info[k].x_attr, 22);
                }
            }
        }

        /* Modify feature attr/chars */
        else if (ch == '8')
        {
            static int f = 0;

            /* Prompt */
            prt("Command: Change feature attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                feature_type* f_ptr = &f_info[f];

                byte da = (byte)(f_ptr->d_attr);
                byte dc = (byte)(f_ptr->d_char);
                byte ca = (byte)(f_ptr->x_attr);
                byte cc = (byte)(f_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Terrain = %d, Name = %-40.40s", f,
                        (f_name + f_ptr->name)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3d / %3d", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3d / %3d", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = inkey();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    f = (f + z_info->f_max + 1) % z_info->f_max;
                if (cx == 'N')
                    f = (f + z_info->f_max - 1) % z_info->f_max;
                if (cx == 'a')
                    f_info[f].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    f_info[f].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    f_info[f].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    f_info[f].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&f_info[f].x_attr, 22);
                }
            }
        }

        /* Modify flavor attr/chars */
        else if (ch == '9')
        {
            static int f = 0;

            /* Prompt */
            prt("Command: Change flavor attr/chars", 15, 0);

            /* Hack -- query until done */
            while (1)
            {
                flavor_type* flavor_ptr = &flavor_info[f];

                byte da = (byte)(flavor_ptr->d_attr);
                byte dc = (byte)(flavor_ptr->d_char);
                byte ca = (byte)(flavor_ptr->x_attr);
                byte cc = (byte)(flavor_ptr->x_char);

                /* Label the object */
                Term_putstr(5, 17, -1, TERM_WHITE,
                    format("Flavor = %d, Text = %-40.40s", f,
                        (flavor_text + flavor_ptr->text)));

                /* Label the Default values */
                Term_putstr(10, 19, -1, TERM_WHITE,
                    format("Default attr/char = %3d / %3d", da, dc));
                Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 19, da, dc);
                Term_putch(43, 19, da, dc);

                if (use_bigtile)
                {
                    if (da & 0x80)
                        Term_putch(44, 19, 255, -1);
                    else
                        Term_putch(44, 19, 0, ' ');
                }

                /* Label the Current values */
                Term_putstr(10, 20, -1, TERM_WHITE,
                    format("Current attr/char = %3d / %3d", ca, cc));
                Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
                Term_putch(43, 20, ca, cc);

                if (use_bigtile)
                {
                    if (ca & 0x80)
                        Term_putch(44, 20, 255, -1);
                    else
                        Term_putch(44, 20, 0, ' ');
                }

                /* Prompt */
                Term_putstr(
                    0, 22, -1, TERM_WHITE, "Command (n/N/a/A/c/C/'s'hade): ");

                /* Get a command */
                cx = inkey();

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    f = (f + z_info->flavor_max + 1) % z_info->flavor_max;
                if (cx == 'N')
                    f = (f + z_info->flavor_max - 1) % z_info->flavor_max;
                if (cx == 'a')
                    flavor_info[f].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    flavor_info[f].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    flavor_info[f].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    flavor_info[f].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&flavor_info[f].x_attr, 22);
                }
            }
        }

#endif /* ALLOW_VISUALS */

        /* Reset visuals */
        else if (ch == '0')
        {
            /* Reset */
            reset_visuals(true);

            /* Message */
            msg_print("Visual attr/char tables reset.");
        }

        /* Unknown option */
        else
        {
            bell("Illegal command for visuals!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_load();
}

/*
 * Asks to the user for specific color values.
 * Returns true if the color was modified.
 */
static bool askfor_color_values(int idx)
{
    char str[10];

    int k, r, g, b;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][1]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Red (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    r = atoi(str);

    /* Check bounds */
    if (r < 0)
        r = 0;
    if (r > 255)
        r = 255;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][2]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Green (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    g = atoi(str);

    /* Check bounds */
    if (g < 0)
        g = 0;
    if (g > 255)
        g = 255;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][3]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Blue (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    b = atoi(str);

    /* Check bounds */
    if (b < 0)
        b = 0;
    if (b > 255)
        b = 255;

    /* Get the default value */
    sprintf(str, "%d", angband_color_table[idx][0]);

    /* Query, check for ESCAPE */
    if (!term_get_string("Extra (0-255) ", str, sizeof(str)))
        return false;

    /* Convert to number */
    k = atoi(str);

    /* Check bounds */
    if (k < 0)
        k = 0;
    if (k > 255)
        k = 255;

    /* Do nothing if the color is not modified */
    if ((k == angband_color_table[idx][0]) && (r == angband_color_table[idx][1])
        && (g == angband_color_table[idx][2])
        && (b == angband_color_table[idx][3]))
        return false;

    /* Modify the color table */
    angband_color_table[idx][0] = k;
    angband_color_table[idx][1] = r;
    angband_color_table[idx][2] = g;
    angband_color_table[idx][3] = b;

    /* Notify the changes */
    return true;
}

/* These two are used to place elements in the grid */
#define COLOR_X(idx) (((idx) / MAX_BASE_COLORS) * 5 + 1)
#define COLOR_Y(idx) ((idx) % MAX_BASE_COLORS + 6)

/* Hack - Note the cast to "int" to prevent overflow */
#define IS_BLACK(idx)                                                          \
    ((int)angband_color_table[idx][1] + (int)angband_color_table[idx][2]       \
            + (int)angband_color_table[idx][3]                                 \
        == 0)

/* We show black as dots to see the shape of the grid */
#define BLACK_SAMPLE "..."

/*
 * The screen used to modify the color table. Only 128 colors can be modified.
 * The remaining entries of the color table are reserved for graphic mode.
 */
static void modify_colors(void)
{
    int x, y, idx, old_idx;
    char ch;
    char msg[100];

    /* Flags */
    bool do_move, do_update;

    /* Clear the screen */
    Term_clear();

    /* Draw the color table */
    for (idx = 0; idx < MAX_COLORS; idx++)
    {
        /* Get coordinates, the x value is adjusted to show a fake cursor */
        x = COLOR_X(idx) + 1;
        y = COLOR_Y(idx);

        /* Show a sample of the color */
        if (IS_BLACK(idx))
            c_put_str(TERM_WHITE, BLACK_SAMPLE, y, x);
        else
            c_put_str(idx, COLOR_SAMPLE, y, x);
    }

    /* Show screen commands and help */
    y = 2;
    x = 42;
    c_put_str(TERM_WHITE, "Commands:", y, x);
    c_put_str(TERM_WHITE, "ESC: Return", y + 2, x);
    c_put_str(TERM_WHITE, "Arrows: Move to color", y + 3, x);
    c_put_str(TERM_WHITE, "k,K: Incr,Decr extra value", y + 4, x);
    c_put_str(TERM_WHITE, "r,R: Incr,Decr red value", y + 5, x);
    c_put_str(TERM_WHITE, "g,G: Incr,Decr green value", y + 6, x);
    c_put_str(TERM_WHITE, "b,B: Incr,Decr blue value", y + 7, x);
    c_put_str(TERM_WHITE, "c: Copy from color", y + 8, x);
    c_put_str(TERM_WHITE, "v: Set specific values", y + 9, x);
    c_put_str(TERM_WHITE, "First column: base colors", y + 11, x);
    c_put_str(TERM_WHITE, "Second column: first shade, etc.", y + 12, x);

    c_put_str(
        TERM_WHITE, "Shades look like base colors in 16 color ports.", 23, 0);

    /* Hack - We want to show the fake cursor */
    do_move = true;
    do_update = true;

    /* Start with the first color */
    idx = 0;

    /* Used to erase the old position of the fake cursor */
    old_idx = -1;

    while (1)
    {
        /* Movement request */
        if (do_move)
        {
            /* Erase the old fake cursor */
            if (old_idx >= 0)
            {
                /* Get coordinates */
                x = COLOR_X(old_idx);
                y = COLOR_Y(old_idx);

                /* Draw spaces */
                c_put_str(TERM_WHITE, " ", y, x);
                c_put_str(TERM_WHITE, " ", y, x + 4);
            }

            /* Show the current fake cursor */
            /* Get coordinates */
            x = COLOR_X(idx);
            y = COLOR_Y(idx);

            /* Draw the cursor */
            c_put_str(TERM_WHITE, ">", y, x);
            c_put_str(TERM_WHITE, "<", y, x + 4);

            /* Format the name of the color */
            SDL_strlcpy(msg,
                format("Color = %d (0x%02X), Name = %s", idx, idx,
                    get_ext_color_name(idx)),
                sizeof(msg));

            /* Show the name and some whitespace */
            c_put_str(TERM_WHITE, format("%-40s", msg), 2, 0);
        }

        /* Color update request */
        if (do_update)
        {
            /* Get coordinates, adjust x */
            x = COLOR_X(idx) + 1;
            y = COLOR_Y(idx);

            /* Hack - Redraw the sample if needed */
            if (IS_BLACK(idx))
                c_put_str(TERM_WHITE, BLACK_SAMPLE, y, x);
            else
                c_put_str(idx, COLOR_SAMPLE, y, x);

            /* Notify the changes in the color table to the terminal */
            Term_xtra(TERM_XTRA_REACT, 0);

            /* The user is playing with white, redraw all */
            if (idx == TERM_WHITE)
                Term_redraw();

            /* Or reduce flickering by redrawing the changes only */
            else
                Term_redraw_section(x, y, x + 2, y);
        }

        /* Common code, show the values in the color table */
        if (do_move || do_update)
        {
            /* Format the view of the color values */
            SDL_strlcpy(msg,
                format("K = %d / R,G,B = %d, %d, %d",
                    angband_color_table[idx][0], angband_color_table[idx][1],
                    angband_color_table[idx][2], angband_color_table[idx][3]),
                sizeof(msg));

            /* Show color values and some whitespace */
            c_put_str(TERM_WHITE, format("%-40s", msg), 4, 0);
        }

        /* Reset flags */
        do_move = false;
        do_update = false;
        old_idx = -1;

        /* Get a command */
        if (!get_com("Command: Modify colors ", &ch))
            break;

        switch (ch)
        {
        /* Down */
        case '2':
        {
            /* Check bounds */
            if (idx + 1 >= MAX_COLORS)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            ++idx;

            /* Request movement */
            do_move = true;
            break;
        }

        /* Up */
        case '8':
        {
            /* Check bounds */
            if (idx - 1 < 0)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            --idx;

            /* Request movement */
            do_move = true;
            break;
        }

        /* Left */
        case '4':
        {
            /* Check bounds */
            if (idx - 16 < 0)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            idx -= 16;

            /* Request movement */
            do_move = true;
            break;
        }

            /* Right */
        case '6':
        {
            /* Check bounds */
            if (idx + 16 >= MAX_COLORS)
                break;

            /* Erase the old cursor */
            old_idx = idx;

            /* Get the new position */
            idx += 16;

            /* Request movement */
            do_move = true;
            break;
        }

            /* Copy from color */
        case 'c':
        {
            char str[10];
            int src;

            /* Get the default value, the base color */
            sprintf(str, "%d", GET_BASE_COLOR(idx));

            /* Query, check for ESCAPE */
            if (!term_get_string(format("Copy from color (0-%d, def. base) ",
                                     MAX_COLORS - 1),
                    str, sizeof(str)))
                break;

            /* Convert to number */
            src = atoi(str);

            /* Check bounds */
            if (src < 0)
                src = 0;
            if (src >= MAX_COLORS)
                src = MAX_COLORS - 1;

            /* Do nothing if the colors are the same */
            if (src == idx)
                break;

            /* Modify the color table */
            angband_color_table[idx][0] = angband_color_table[src][0];
            angband_color_table[idx][1] = angband_color_table[src][1];
            angband_color_table[idx][2] = angband_color_table[src][2];
            angband_color_table[idx][3] = angband_color_table[src][3];

            /* Request update */
            do_update = true;
            break;
        }

        /* Increase the extra value */
        case 'k':
        {
            /* Get a pointer to the proper value */
            byte* k_ptr = &angband_color_table[idx][0];

            /* Modify the value */
            *k_ptr = (byte)(*k_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Decrease the extra value */
        case 'K':
        {
            /* Get a pointer to the proper value */
            byte* k_ptr = &angband_color_table[idx][0];

            /* Modify the value */
            *k_ptr = (byte)(*k_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Increase the red value */
        case 'r':
        {
            /* Get a pointer to the proper value */
            byte* r_ptr = &angband_color_table[idx][1];

            /* Modify the value */
            *r_ptr = (byte)(*r_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Decrease the red value */
        case 'R':
        {
            /* Get a pointer to the proper value */
            byte* r_ptr = &angband_color_table[idx][1];

            /* Modify the value */
            *r_ptr = (byte)(*r_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Increase the green value */
        case 'g':
        {
            /* Get a pointer to the proper value */
            byte* g_ptr = &angband_color_table[idx][2];

            /* Modify the value */
            *g_ptr = (byte)(*g_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Decrease the green value */
        case 'G':
        {
            /* Get a pointer to the proper value */
            byte* g_ptr = &angband_color_table[idx][2];

            /* Modify the value */
            *g_ptr = (byte)(*g_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Increase the blue value */
        case 'b':
        {
            /* Get a pointer to the proper value */
            byte* b_ptr = &angband_color_table[idx][3];

            /* Modify the value */
            *b_ptr = (byte)(*b_ptr + 1);

            /* Request update */
            do_update = true;
            break;
        }

        /* Decrease the blue value */
        case 'B':
        {
            /* Get a pointer to the proper value */
            byte* b_ptr = &angband_color_table[idx][3];

            /* Modify the value */
            *b_ptr = (byte)(*b_ptr - 1);

            /* Request update */
            do_update = true;
            break;
        }

            /* Ask for specific values */
        case 'v':
        {
            do_update = askfor_color_values(idx);
            break;
        }
        }
    }
}

/*
 * Interact with "colors"
 */
void do_cmd_colors(void)
{
    int ch;

    int i;

    SDL_IOStream* fff;

    char buf[1024];

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Clear any active banner before opening colors menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Interact until done */
    while (1)
    {
        /* Clear screen */
        Term_clear();

        /* Ask for a choice */
        prt("Interact with Colors", 2, 0);

        /* Give some choices */
        prt("(1) Load a user pref file", 4, 5);
#ifdef ALLOW_COLORS
        prt("(2) Dump colors", 5, 5);
        prt("(3) Modify colors", 6, 5);
#endif /* ALLOW_COLORS */

        /* Prompt */
        prt("Command: ", 8, 0);

        /* Prompt */
        ch = inkey();

        /* Done */
        if (ch == ESCAPE)
            break;

        /* Load a user pref file */
        if (ch == '1')
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(8);

            /* Could skip the following if loading cancelled XXX XXX XXX */

            /* Mega-Hack -- React to color changes */
            Term_xtra(TERM_XTRA_REACT, 0);

            /* Mega-Hack -- Redraw physical windows */
            Term_redraw();
        }

#ifdef ALLOW_COLORS

        /* Dump colors */
        else if (ch == '2')
        {
            static cptr mark = "Colors";
            char ftmp[80];

            /* Prompt */
            prt("Command: Dump colors", 8, 0);

            /* Prompt */
            prt("File: ", 10, 0);

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!askfor_aux(ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_colors: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old colors */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = sdl_fopen(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            SDL_IOprintf(fff, "\n\n");

            /* Start dumping */
            SDL_IOprintf(fff, "# Color redefinitions\n\n");

            /* Dump colors */
            for (i = 0; i < 256; i++)
            {
                int kv = angband_color_table[i][0];
                int rv = angband_color_table[i][1];
                int gv = angband_color_table[i][2];
                int bv = angband_color_table[i][3];

                cptr name = "unknown";

                /* Skip non-entries */
                if (!kv && !rv && !gv && !bv)
                    continue;

                /* Extract the color name */
                if (i < 16)
                    name = color_names[i];

                /* Dump a comment */
                SDL_IOprintf(fff, "# Color '%s'\n", name);

                /* Dump the monster attr/char info */
                SDL_IOprintf(fff, "V:%d:0x%02X:0x%02X:0x%02X:0x%02X\n\n", i, kv, rv,
                    gv, bv);
            }

            /* All done */
            SDL_IOprintf(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            sdl_fclose(fff);

            /* Message */
            msg_print("Dumped color redefinitions.");
        }

        /* Edit colors */
        else if (ch == '3')
        {
            modify_colors();
        }

#endif /* ALLOW_COLORS */

        /* Unknown option */
        else
        {
            bell("Illegal command for colors!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_load();
}

/*
 * Take notes.  There are two ways this can happen, either in the message recall
 * or a file.  The command can also be passed a string, which will automatically
 * be written. -CK-
 */
void do_cmd_note(char* note, int what_depth)
{
    char buf[120];
    char turn_string[16];

    int length, length_info;
    char info_note[40];
    char depths[10];

    /* Default */
    SDL_strlcpy(buf, "", sizeof(buf));

    /* If a note is passed, use that, otherwise accept user input. */
    if (streq(note, ""))
    {
        if (!term_get_string("Note: ", buf, 57))
            return;
    }
    else
    {
        SDL_strlcpy(buf, note, sizeof(buf));
    }

    /* Ignore empty notes */
    if (!buf[0] || (buf[0] == ' '))
        return;

    /* write it to the notes file */

    /*Artefacts use depth artefact created.  All others use player depth.*/

    /*get depth for recording\
     */
    if (what_depth == 0)
    {
        SDL_strlcpy(depths, "   Gates", sizeof(depths));
    }
    else if (what_depth == CHEST_LEVEL)
    {
        SDL_strlcpy(depths, "   Chest", sizeof(depths));
    }
    else if (what_depth == SKELETON_LEVEL)
    {
        SDL_strlcpy(depths, "   Skeleton", sizeof(depths));
    }
    else
    {
        comma_number(depths, what_depth * 50);
        strnfmt(depths, sizeof(depths), "%5s ft", depths);
    }

    comma_number(turn_string, playerturn);

    /* Make preliminary part of note */
    strnfmt(info_note, sizeof(info_note), "%7s  %s   ", turn_string, depths);

    /*write the info note*/
    SDL_strlcat(notes_buffer, info_note, sizeof(notes_buffer));

    /*get the length of the notes*/
    length_info = strlen(info_note);
    length = strlen(buf);

    /*break up long notes*/
    if ((length + length_info) > LINEWRAP)
    {
        bool keep_going = true;
        int startpoint = 0;
        int endpoint, n;

        while (keep_going)
        {
            /*don't print more than the set linewrap amount*/
            endpoint = startpoint + LINEWRAP - strlen(info_note) + 1;

            /*find a breaking point*/
            while (true)
            {
                /*are we at the end of the line?*/
                if (endpoint >= length)
                {
                    /*print to the end*/
                    endpoint = length;
                    keep_going = false;
                    break;
                }

                /* Mark the most recent space or dash in the string */
                else if ((buf[endpoint] == ' ') || (buf[endpoint] == '-'))
                    break;

                /*no spaces in the line, so break in the middle of text*/
                else if (endpoint == startpoint)
                {
                    endpoint = startpoint + LINEWRAP - strlen(info_note) + 1;
                    break;
                }

                /* check previous char */
                endpoint--;
            }

            /*make a continued note if applicable*/
            if (startpoint)
                SDL_strlcat(
                    notes_buffer, "                    ", sizeof(notes_buffer));

            /* Write that line to file */
            for (n = startpoint; n <= endpoint; n++)
            {
                char ch;

                /* Ensure the character is printable */
                ch = (isprint(buf[n]) ? buf[n] : ' ');

                /* Write out the character */
                SDL_strlcat(notes_buffer, format("%c", ch), sizeof(notes_buffer));
            }

            /*break the line*/
            SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

            /*prepare for the next line*/
            startpoint = endpoint + 1;
        }
    }

    /* Add note to buffer */
    else
    {
        SDL_strlcat(notes_buffer, format("%s\n", buf), sizeof(notes_buffer));
    }
}

/*
 * Mention the current version
 */
void do_cmd_version(void)
{
    /* Silly message - use msg_print so message is shown immediately */
    char verbuf[128];
    strnfmt(verbuf, sizeof(verbuf), "You are playing %s %s.  Type '?' for more info.",
        VERSION_NAME, VERSION_STRING);
    msg_print(verbuf);
}

/*
 * Array of feeling strings
 */
static cptr do_cmd_feeling_text[LEV_THEME_HEAD]
    = { "Looks like any other level.",
          "You feel there is something special about this level.",
          "You have a superb feeling about this level.",
          "You have an excellent feeling...", "You have a very good feeling...",
          "You have a good feeling...", "You feel strangely lucky...",
          "You feel your luck is turning...",
          "You like the look of this place...",
          "This level can't be all bad...", "What a boring place..." };

/*
 * Note that "feeling" is set to zero unless some time has passed.
 * Note that this is done when the level is GENERATED, not entered.
 */
void do_cmd_feeling(void)
{
    /* No useful feeling on the surface */
    if (!p_ptr->depth)
    {
        msg_print("You stand once again upon the surface. Freedom awaits.");
        return;
    }

    /* No useful feelings until enough time has passed */
    if (!do_feeling)
    {
        msg_print("You are still uncertain about this level...");
        return;
    }

    /* Display the feeling */
    else
        msg_print(do_cmd_feeling_text[feeling]);
}

/*
 * Array of feeling strings
 */
static cptr do_cmd_challenge_text[14]
    = { "challenges you from beyond the grave!",
          "thunders 'Prove worthy of your traditions - or die ashamed!'.",
          "desires to test your mettle!",
          "has risen from the dead to test you!",
          "roars 'Fight, or know yourself for a coward!'.",
          "summons you to a duel of life and death!",
          "desires you to know that you face a mighty champion of yore!",
          "demands that you prove your worthiness in combat!",
          "calls you unworthy of your ancestors!",
          "challenges you to a deathmatch!", "walks Middle-Earth once more!",
          "challenges you to demonstrate your prowess!",
          "demands you prove yourself here and now!",
          "asks 'Can ye face the best of those who came before?'." };

/*
 * Personalize, randomize, and announce the challenge of a player ghost. -LM-
 */
void ghost_challenge(void)
{
    monster_race* r_ptr = &r_info[r_ghost];

    /*paranoia*/
    /* Check there is a name/ghost first */
    if (ghost_name[0] == '\0')
    {
        /*there wasn't a ghost*/
        bones_selector = 0;
        return;
    }

    msg_format("%^s, the %^s %s", ghost_name, r_name + r_ptr->name,
        do_cmd_challenge_text[rand_int(14)]);

    message_flush();
}

/*display the notes file*/
void do_cmd_knowledge_notes(void) { show_buffer(notes_buffer, 0); }

/*
 * Display oath status information
 */
void do_cmd_knowledge_oaths(void)
{
    SDL_IOStream* fff;
    char file_name[1024];
    
    /* Temporary file */
    if (!path_temp(file_name, sizeof(file_name)))
        return;

    /* Open a new file */
    fff = sdl_fopen(file_name, "w");

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Scan the oaths */
    SDL_IOprintf(fff, "Oath Status\n\n");
    
    /* Check current character oath */
    if (p_ptr->have_ability[S_SPC][SPC_OATH_MERCY])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_MERCY])
            SDL_IOprintf(fff, "Current Oath: Oath of Mercy (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Mercy (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_SILENCE])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_SILENCE])
            SDL_IOprintf(fff, "Current Oath: Oath of Silence (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Silence (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_IRON])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_IRON])
            SDL_IOprintf(fff, "Current Oath: Oath of Iron (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Iron (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_SMITH])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_SMITH])
            SDL_IOprintf(fff, "Current Oath: Oath of the Smith (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of the Smith (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_VALOROUS])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_VALOROUS])
            SDL_IOprintf(fff, "Current Oath: Oath of Valorous Heart (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Valorous Heart (Broken)\n\n");
    }
    else
    {
        SDL_IOprintf(fff, "Current Oath: None\n\n");
    }
    
    /* Display metarun oath status */
    SDL_IOprintf(fff, "Metarun Oath Status:\n");
    
    /* Check unlocked oaths */
    bool has_unlocked = false;
    if (oath_unlocked(OATH_MERCY)) 
    {
        SDL_IOprintf(fff, "  Oath of Mercy: Unlocked");
        if (oath_banned(OATH_MERCY))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_SILENCE)) 
    {
        SDL_IOprintf(fff, "  Oath of Silence: Unlocked");
        if (oath_banned(OATH_SILENCE))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_IRON)) 
    {
        SDL_IOprintf(fff, "  Oath of Iron: Unlocked");
        if (oath_banned(OATH_IRON))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_SMITH)) 
    {
        SDL_IOprintf(fff, "  Oath of the Smith: Unlocked");
        if (oath_banned(OATH_SMITH))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_VALOROUS)) 
    {
        SDL_IOprintf(fff, "  Oath of Valorous Heart: Unlocked");
        if (oath_banned(OATH_VALOROUS))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }
    
    if (!has_unlocked)
    {
        SDL_IOprintf(fff, "  No oaths unlocked yet.\n");
        SDL_IOprintf(fff, "  Complete Valar quests to unlock new oaths.\n");
    }
    
    /* Close the file */
    sdl_fclose(fff);

    /* Display the file contents */
    show_file(file_name, "Oath Status", 0);

    /* Remove the file */
    fd_kill(file_name);
}

/*
 * Description of each object group.
 */
static cptr object_group_text[]
    = { "Herbs", "Potions", "Rings", "Amulets", "Staves", "Horns", "Swords",
          "Axes & Polearms", "Blunt Weapons", "Diggers", "Bows",
          //	"Arrows",
          "Light Sources", "Soft Armour", "Mail", "Shields", "Cloaks", "Gloves",
          "Helms", "Crowns", "Boots", "Chests", NULL };

/*
 * TVALs of items in each group
 */
static byte object_group_tval[] = { TV_FOOD, TV_POTION, TV_RING, TV_AMULET,
    TV_STAFF, TV_HORN, TV_SWORD, TV_POLEARM, TV_HAFTED, TV_DIGGING, TV_BOW,
    //	TV_ARROW,
    TV_LIGHT, TV_SOFT_ARMOR, TV_MAIL, TV_SHIELD, TV_CLOAK, TV_GLOVES, TV_HELM,
    TV_CROWN, TV_BOOTS, TV_CHEST, 0 };

/*
 * Build a list of objects indexes in the given group. Return the number
 * of objects in the group. object_idx[] must be one element larger than the
 * largest number of objects that will be collected.
 *  (Incorporates some code from jdh)
 */
static int collect_objects(int grp_cur, object_list_entry object_idx[])
{
    int i, j, k, object_cnt = 0;
    int max_sval = -1;

    /* Get a list of x_char in this group */
    byte group_tval = object_group_tval[grp_cur];

    /* Check every object */
    for (i = 0; i < z_info->k_max; i++)
    {
        /* Access the object type */
        object_kind* k_ptr = &k_info[i];

        /*used to check for allocation*/
        k = 0;

        /* Skip empty objects */
        if (!k_ptr->name)
            continue;

        /* Skip items with no distribution (including special artefacts) */
        /* Scan allocation pairs */
        for (j = 0; j < 4; j++)
        {
            /*add the rarity, if there is one*/
            k += k_ptr->chance[j];
        }
        /*not in allocation table*/
        if (!(k))
            continue;

        /* Require objects ever seen*/
        // if (!(k_ptr->aware && k_ptr->everseen)) continue;
        if (!(k_ptr->everseen))
            continue;

        /* Check for object in the group */
        if (k_ptr->tval == group_tval)
        {
            /* Save the highest sval in the group for later */
            if (k_ptr->sval > max_sval)
            {
                max_sval = k_ptr->sval;
            }

            /* Add the object type */
            if (object_idx)
            {
                object_idx[object_cnt].type = OBJ_NORMAL;
                object_idx[object_cnt].idx = i;
            }

            object_cnt++;
        }
    }

    /* Add special items to the list */
    /* Skip this part if we don't know any normal items */
    for (i = 0; object_cnt > 0 && i < z_info->e_max; i++)
    {
        /* Access the object type */
        ego_item_type* e_ptr = &e_info[i];

        /* Skip empty objects */
        if (!e_ptr->name)
            continue;

        /* Require objects ever seen*/
        if (!(e_ptr->everseen))
            continue;

        /* Check for object in the group */
        for (j = 0; j < EGO_TVALS_MAX; j++)
        {
            if (e_ptr->tval[j] == group_tval)
            {
                if (object_idx)
                {
                    object_idx[object_cnt].type = OBJ_SPECIAL;
                    object_idx[object_cnt].idx = -1;
                    object_idx[object_cnt].e_idx = i;
                    object_idx[object_cnt].tval = group_tval;
                    object_idx[object_cnt].sval = -1;
                }
                object_cnt++;

                break;
            }
        }
    }

    /* Terminate the list */
    if (object_idx)
        object_idx[object_cnt].type = OBJ_NONE;

    /* Return the number of object types */
    return object_cnt;
}

/*
 * Build a list of artefact indexes in the given group. Return the number
 * of eligible artefacts in that group.
 */
static int collect_artefacts(int grp_cur, int object_idx[])
{
    int i, object_cnt = 0;
    bool* okay;
    bool know_all = cheat_know;

    /* Get a list of x_char in this group */
    byte group_tval = object_group_tval[grp_cur];

    /*make a list of artefacts not found*/
    /* Allocate the "object_idx" array */
    okay = mem_alloc_array(z_info->art_max, bool);

    /* Default first,  */
    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        bool revealed = (a_ptr->seen & ART_SEEN_REVEALED) != 0;

        /*start with false*/
        okay[i] = false;

        /* Skip "empty" artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Skip "unfound" artefacts, unless in wizard mode, cheating,
         * or revealed via quests/lore. */
        if (!know_all && !p_ptr->wizard && !a_ptr->found_num && !revealed)
            continue;

        /* Skip "ungenerated" artefacts, unless cheating or quest-revealed. */
        if (!know_all && !revealed && !a_ptr->cur_num)
            continue;

        /* Skip the later versions of the Iron Crown */
        if ((i == ART_MORGOTH_0) || (i == ART_MORGOTH_1)
            || (i == ART_MORGOTH_2))
            continue;

        /* Skip the special smithing template artefacts */
        if ((i >= ART_ULTIMATE) && (i <= z_info->art_norm_max))
            continue;

        /*assume all created artefacts are good at this point*/
        okay[i] = true;
    }

    /* Finally, go through the list of artefacts and categorize the good ones */
    for (i = 0; i < z_info->art_max; i++)
    {
        /* Access the artefact */
        artefact_type* a_ptr = &a_info[i];

        /* Skip empty artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Require artefacts ever seen*/
        if (okay[i] == false)
            continue;

        /* Check for race in the group */
        if (a_ptr->tval == group_tval)
        {
            /* Add the race */
            object_idx[object_cnt++] = i;
        }
    }

    /* Terminate the list */
    object_idx[object_cnt] = 0;

    /*clear the array*/
    mem_free_null(okay);

    /* Return the number of races */
    return object_cnt;
}

static bool supply_kind_matches(int group, int tval, int sval)
{
    switch (group)
    {
    case SUPPLY_GROUP_HERBS:
        return (tval == TV_FOOD) && (sval <= SV_FOOD_SICKNESS);
    case SUPPLY_GROUP_POTIONS:
        return (tval == TV_POTION);
    case SUPPLY_GROUP_GEMS:
        return (tval == TV_GEM);
    default:
        return false;
    }
}

static bool supply_item_matches(int group, const object_type* o_ptr)
{
    if (!o_ptr)
        return false;

    return supply_kind_matches(group, o_ptr->tval, o_ptr->sval);
}

static void compute_supply_group_totals(int totals[SUPPLY_GROUP_MAX])
{
    int i;

    for (i = 0; i < SUPPLY_GROUP_MAX; i++)
        totals[i] = 0;

    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        if ((o_ptr->tval == TV_FOOD) && (o_ptr->sval <= SV_FOOD_SICKNESS))
            totals[SUPPLY_GROUP_HERBS] += o_ptr->number;
        else if (o_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += o_ptr->number;
        else if (o_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += o_ptr->number;
    }

    for (i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        if (!s_ptr || !s_ptr->k_idx)
            continue;

        if ((s_ptr->tval == TV_FOOD) && (s_ptr->sval <= SV_FOOD_SICKNESS))
            totals[SUPPLY_GROUP_HERBS] += s_ptr->number;
        else if (s_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += s_ptr->number;
        else if (s_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += s_ptr->number;
    }
}

static bool supply_kind_is_known(const object_kind* k_ptr)
{
    if (!k_ptr)
        return false;

    if (cheat_know || p_ptr->wizard)
        return true;

    return k_ptr->aware || k_ptr->everseen || k_ptr->tried;
}

static int collect_supply_entries(int group_idx, supply_list_entry entries[])
{
    int count = 0;
    int capacity = z_info->k_max;
    int i;

    if (!entries)
        return 0;

    memset(entries, 0, sizeof(supply_list_entry) * capacity);

    /* Aggregate carried items first */
    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];
        int j;

        if (!o_ptr->k_idx)
            continue;

        if (!supply_item_matches(group_idx, o_ptr))
            continue;

        int value = o_ptr->number;

        for (j = 0; j < count; j++)
        {
            if (entries[j].k_idx == o_ptr->k_idx)
            {
                entries[j].total += value;
                if (entries[j].item_idx < 0)
                    entries[j].item_idx = i;
                break;
            }
        }

        if (j == count)
        {
            if (count >= capacity)
                break;

            entries[count].k_idx = o_ptr->k_idx;
            entries[count].item_idx = i;
            entries[count].total = value;
            entries[count].supply_idx = -1;
            count++;
        }
    }

    /* Aggregate supplies from the cache */
    for (i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        int j;

        if (!s_ptr || !s_ptr->k_idx)
            continue;

        if (!supply_item_matches(group_idx, s_ptr))
            continue;

        int value = s_ptr->number;

        for (j = 0; j < count; j++)
        {
            if (entries[j].k_idx == s_ptr->k_idx)
            {
                entries[j].total += value;
                if (entries[j].item_idx < 0)
                    entries[j].item_idx = SUPPLIES_INDEX;
                entries[j].supply_idx = i;
                break;
            }
        }

        if (j == count)
        {
            if (count >= capacity)
                break;

            entries[count].k_idx = s_ptr->k_idx;
            entries[count].item_idx = SUPPLIES_INDEX;
            entries[count].total = value;
            entries[count].supply_idx = i;
            count++;
        }
    }

    /* Add known kinds even when none are carried */
    for (i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];
        int j;

        if (!k_ptr->name)
            continue;

        if (!supply_kind_matches(group_idx, k_ptr->tval, k_ptr->sval))
            continue;

        if (!supply_kind_is_known(k_ptr))
            continue;

        for (j = 0; j < count; j++)
        {
            if (entries[j].k_idx == i)
                break;
        }

        if (j == count)
        {
            if (count >= capacity)
                break;

            entries[count].k_idx = i;
            entries[count].item_idx = -1;
            entries[count].total = 0;
            entries[count].supply_idx = -1;
            count++;
        }
    }

    if (count < capacity)
    {
        entries[count].k_idx = -1;
        entries[count].item_idx = -1;
        entries[count].total = 0;
        entries[count].supply_idx = -1;
    }

    return count;
}

static byte get_supply_item_color(int k_idx, bool aware)
{
    object_kind* k_ptr;

    if (k_idx < 0 || k_idx >= z_info->k_max)
        return TERM_WHITE;

    k_ptr = &k_info[k_idx];

    /* Unidentified items all use slate color */
    if (!aware)
        return TERM_SLATE;

    /* Color by specific item type */
    switch (k_ptr->tval)
    {
        case TV_FOOD: /* Herbs */
            switch (k_ptr->sval)
            {
                case SV_FOOD_RAGE:         return TERM_RED;    /* Red for rage */
                case SV_FOOD_SUSTENANCE:   return TERM_GREEN;    /* Green for sustenance */
                case SV_FOOD_TERROR:       return TERM_VIOLET;   /* Violet for fear */
                case SV_FOOD_HEALING:      return TERM_L_GREEN;  /* Light green for healing */
                case SV_FOOD_RESTORATION:  return TERM_BLUE;     /* Blue for restoration */
                case SV_FOOD_HUNGER:       return TERM_UMBER;    /* Brown for hunger */
                case SV_FOOD_VISIONS:      return TERM_L_UMBER;  /* Light brown for visions */
                case SV_FOOD_ENTRANCEMENT: return TERM_VIOLET;   /* Violet for entrancement */
                case SV_FOOD_WEAKNESS:     return TERM_SLATE;    /* Grey for weakness */
                case SV_FOOD_SICKNESS:     return TERM_L_DARK;   /* Dark grey for sickness */
                default:                   return TERM_WHITE;
            }

        case TV_POTION:
            switch (k_ptr->sval)
            {
                case SV_POTION_MIRUVOR:          return TERM_WHITE;  /* White for Miruvor */
                case SV_POTION_ORCISH_LIQUOR:    return TERM_UMBER;    /* Brown for liquor */
                case SV_POTION_ESGALDUIN:        return TERM_VIOLET;   /* Violet for Esgalduin */
                case SV_POTION_CLARITY:          return TERM_L_UMBER;  /* Light brown for clarity */
                case SV_POTION_HEALING:          return TERM_L_GREEN;  /* Light green for healing */
                case SV_POTION_VOICE:            return TERM_L_BLUE;  /* White for voice */
                case SV_POTION_true_SIGHT:       return TERM_BLUE;     /* Blue for true sight */
                case SV_POTION_ANTIDOTE:         return TERM_GREEN;    /* Green for antidote */
                case SV_POTION_QUICKNESS:        return TERM_ORANGE;  /* Light brown for speed */
                case SV_POTION_ELEM_RESISTANCE:  return TERM_L_BLUE;   /* Orange for resistance */
                case SV_POTION_STR:              return TERM_RED;      /* Red for strength */
                case SV_POTION_DEX:              return TERM_GREEN;    /* Green for dexterity */
                case SV_POTION_CON:              return TERM_L_RED;     /* Blue for constitution */
                case SV_POTION_GRA:              return TERM_BLUE;   /* Violet for grace */
                case SV_POTION_SLOWNESS:         return TERM_SLATE;    /* Grey for slowness */
                case SV_POTION_POISON:           return TERM_L_DARK;   /* Dark for poison */
                case SV_POTION_BLINDNESS:        return TERM_L_DARK;   /* Dark for blindness */
                case SV_POTION_CONFUSION:        return TERM_SLATE;    /* Grey for confusion */
                case SV_POTION_DEC_DEX:          return TERM_SLATE;    /* Grey for decrease dex */
                case SV_POTION_DEC_GRA:          return TERM_SLATE;    /* Grey for decrease grace */
                default:                         return TERM_WHITE;
            }

        case TV_GEM:
            switch (k_ptr->sval)
            {
                case SV_GEM_FREEDOM:         return TERM_WHITE;  /* White for freedom */
                case SV_GEM_LIGHT:           return TERM_ORANGE;   /* Orange for light */
                case SV_GEM_SANCTITY:        return TERM_L_UMBER;  /* Light brown for sanctity */
                case SV_GEM_UNDERSTANDING:   return TERM_BLUE;     /* Blue for understanding */
                case SV_GEM_REVELATIONS:     return TERM_L_BLUE;   /* Violet for revelations */
                case SV_GEM_TREASURES:       return TERM_ORANGE;   /* Orange for treasures */
                case SV_GEM_FOES:            return TERM_RED;      /* Red for foes */
                case SV_GEM_SELF_KNOWLEDGE:  return TERM_GREEN;  /* Light green for self-knowledge */
                case SV_GEM_WARDING:         return TERM_VIOLET;  /* Light brown for warding */
                case SV_GEM_RECHARGING:      return TERM_BLUE;     /* Blue for recharging */
                case SV_GEM_SHADOWS:         return TERM_L_DARK;   /* Dark for shadows */
                default:                     return TERM_WHITE;
            }

        default:
            return TERM_WHITE;
    }
}

static void display_supply_group_list(int col, int row, int wid, int per_page,
    int grp_idx[], int grp_cur, int grp_top, int group_totals[])
{
    int i;
    int total_col = col + wid - 3;

    for (i = 0; i < per_page && (grp_idx[i] >= 0); i++)
    {
        int grp = grp_idx[grp_top + i];
        byte base_color;
        byte attr;
        char buf[8];

        /* Assign color based on group type */
        switch (grp)
        {
            case SUPPLY_GROUP_HERBS:   base_color = TERM_GREEN; break;
            case SUPPLY_GROUP_POTIONS: base_color = TERM_VIOLET;  break;
            case SUPPLY_GROUP_GEMS:    base_color = TERM_BLUE;    break;
            default:                   base_color = TERM_WHITE;   break;
        }

        /* Highlight cursor with white, dim if empty */
        if (grp_top + i == grp_cur)
            attr = TERM_L_WHITE;
        else if (group_totals[grp] == 0)
            attr = TERM_L_DARK;
        else
            attr = base_color;

        Term_erase(col, row + i, wid);
        c_put_str(attr, supply_group_text[grp], row + i, col);

        strnfmt(buf, sizeof(buf), "%3d", group_totals[grp]);
        c_put_str(attr, buf, row + i, total_col);
    }
}

static void display_supply_list(int col, int row, int per_page,
    supply_list_entry entries[], int entry_cnt, int entry_cur, int entry_top,
    int count_col, int sym_col, int current_group, int column)
{
    int i;

    (void)current_group; /* Not used since we color by specific item type now */

    for (i = 0; i < per_page; i++)
    {
        int idx = entry_top + i;
        int y = row + i;

        Term_erase(col, y, 255);

        if (idx >= entry_cnt)
            continue;

        supply_list_entry* entry = &entries[idx];
        object_type* o_ptr;
        object_type fake;
        object_kind* k_ptr;
        bool aware;
        byte base_attr, cursor_attr, attr;
        byte sym_attr;
        char sym_char;
        char name[80];
        char count_buf[8];

        if (entry->k_idx < 0 || entry->k_idx >= z_info->k_max)
            continue;

        k_ptr = &k_info[entry->k_idx];
        aware = k_ptr->aware;
        /* Items with 0 count should be grey */
        if (entry->total == 0)
        {
            base_attr = TERM_L_DARK;
            cursor_attr = TERM_SLATE;
        }
        else
        {
            /* Get color based on specific item type */
            base_attr = get_supply_item_color(entry->k_idx, aware);
            cursor_attr = aware ? TERM_L_WHITE : TERM_WHITE;
        }
        /* Only highlight when right panel is active (column == 1) */
        attr = (column == 1 && idx == entry_cur) ? cursor_attr : base_attr;

        if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
        {
            o_ptr = &inventory[entry->item_idx];
        }
        else
        {
            object_wipe(&fake);
            object_prep(&fake, entry->k_idx);
            if (aware)
                fake.ident |= IDENT_KNOWN;
            fake.number = (entry->total > 0) ? entry->total : 1;
            o_ptr = &fake;
        }

        object_desc(name, sizeof(name), o_ptr, true, 3);
        c_prt(attr, name, y, col);

        strnfmt(count_buf, sizeof(count_buf), "x%-3d", entry->total);
        c_put_str(attr, count_buf, y, count_col);

        sym_attr = object_attr(o_ptr);
        sym_char = object_char(o_ptr);
        Term_putch(sym_col, y, sym_attr, sym_char);
        if (use_bigtile)
        {
            if (sym_attr & 0x80)
                Term_putch(sym_col + 1, y, 255, -1);
            else
                Term_putch(sym_col + 1, y, 0, ' ');
        }
    }

    for (; i < per_page; i++)
    {
        Term_erase(col, row + i, 255);
    }
}

/*
 * Move the cursor in a browser window
 */
static void browser_cursor_with_rows(char ch, int* column, int* grp_cur,
    int grp_cnt, int* list_cur, int list_cnt, int page_rows)
{
    int d;
    int col = *column;
    int grp = *grp_cur;
    int list = *list_cur;
    int page_jump = (page_rows > 0) ? page_rows : BROWSER_ROWS;

    /* Extract direction */
    d = target_dir(ch);

    if (!d)
        return;

    /* Diagonals - hack */
    if ((ddx[d] > 0) && ddy[d])
    {
        /* Browse group list */
        if (!col)
        {
            int old_grp = grp;

            /* Move up or down */
            grp += ddy[d] * page_jump;

            /* Verify */
            if (grp >= grp_cnt)
                grp = grp_cnt - 1;
            if (grp < 0)
                grp = 0;
            if (grp != old_grp)
                list = 0;
        }

        /* Browse sub-list list */
        else
        {
            /* Move up or down */
            list += ddy[d] * page_jump;

            /* Verify */
            if (list >= list_cnt)
                list = list_cnt - 1;
            if (list < 0)
                list = 0;
        }

        (*grp_cur) = grp;
        (*list_cur) = list;

        return;
    }

    if (ddx[d])
    {
        col += ddx[d];
        if (col < 0)
            col = 0;
        if (col > 1)
            col = 1;

        (*column) = col;

        return;
    }

    /* Browse group list */
    if (!col)
    {
        int old_grp = grp;

        /* Move up or down */
        grp += ddy[d];

        /* Verify */
        if (grp >= grp_cnt)
            grp = grp_cnt - 1;
        if (grp < 0)
            grp = 0;
        if (grp != old_grp)
            list = 0;
    }

    /* Browse sub-list list */
    else
    {
        /* Move up or down */
        list += ddy[d];

        /* Verify */
        if (list >= list_cnt)
            list = list_cnt - 1;
        if (list < 0)
            list = 0;
    }

    (*grp_cur) = grp;
    (*list_cur) = list;
}

/*
 * Hack -- Create a "forged" artefact
 */
static bool prepare_fake_artefact(object_type* o_ptr, byte name1)
{
    s16b i;

    artefact_type* a_ptr = &a_info[name1];

    /* Ignore "empty" artefacts */
    if (a_ptr->tval + a_ptr->sval == 0)
        return false;

    /* Get the "kind" index */
    i = lookup_kind(a_ptr->tval, a_ptr->sval);

    /* Oops */
    if (!i)
        return (false);

    /* Create the artefact */
    object_prep(o_ptr, i);

    /* Save the name */
    o_ptr->name1 = name1;

    /* Extract the fields */
    o_ptr->pval = a_ptr->pval;
    o_ptr->att = a_ptr->att;
    o_ptr->dd = a_ptr->dd;
    o_ptr->ds = a_ptr->ds;
    o_ptr->evn = a_ptr->evn;
    o_ptr->pd = a_ptr->pd;
    o_ptr->ps = a_ptr->ps;
    o_ptr->weight = a_ptr->weight;

    memcpy(o_ptr->stat_bonus, a_ptr->stat_bonus, sizeof(o_ptr->stat_bonus));
    memcpy(o_ptr->skill_bonus, a_ptr->skill_bonus, sizeof(o_ptr->skill_bonus));

    // add the abilities
    for (i = 0; i < a_ptr->abilities; i++)
    {
        o_ptr->skilltype[i + o_ptr->abilities] = a_ptr->skilltype[i];
        o_ptr->abilitynum[i + o_ptr->abilities] = a_ptr->abilitynum[i];
        o_ptr->bane_type[i + o_ptr->abilities] = a_ptr->bane_type[i];
    }
    o_ptr->abilities += a_ptr->abilities;

    /*identify it*/
    object_known(o_ptr);

    /*make it a spoiler item*/
    o_ptr->ident |= IDENT_SPOIL;

    /* Hack -- extract the "cursed" flag */
    if (a_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);

    /* Success */
    return (true);
}

/*
 * Describe fake artefact
 */
void desc_art_fake(int a_idx)
{
    object_type* i_ptr;
    object_type object_type_body;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Wipe the object */
    object_wipe(i_ptr);

    /* Make fake artefact */
    prepare_fake_artefact(i_ptr, a_idx);

    /* Hack -- Handle stuff */
    handle_stuff();

    /* Reset the cursor */
    Term_gotoxy(0, 0);

    object_info_screen(i_ptr);
}

/*
 * Display known artefacts
 */
void do_cmd_knowledge_artefacts(void)
{
    log_debug("Player opened artifacts knowledge screen");
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_ARTEFACTS);
}

/*
 * Description of each monster group.
 */
static cptr monster_group_text[] = { "Uniques", /*All uniques, all letters*/
    /*Unused*/ /*'a'*/
    /*Unused*/ /*'A'*/
    "Bats & Birds", /*'b'*/
    /*Unused*/ /*'B'*/
    /*Unused*/ /*'c'*/
    "Canines", /*'C'*/
    "Young Dragons", /*'d'*/
    "Great Dragons", /*'D'*/
    /*Unused*/ /*'e'*/
    /*Unused*/ /*'E'*/
    "Felines", /*'f'*/
    /*Unused*/ /*'F'*/
    /*Unused*/ /*'g'*/
    "Giants", /*'G'*/
    /*Unused*/ /*'h'*/
    "Horrors", /*'H'*/
    /*Unused*/ /*'i'*/
    "Insects", /*'I'*/
    /*Unused*/ /*'j'*/
    /*Unused*/ /*'J'*/
    /*Unused*/ /*'k'*/
    /*Unused*/ /*'K'*/
    /*Unused*/ /*'l'*/
    /*Unused*/ /*'L'*/
    "Young Spiders", /*'m'*/
    "Spiders", /*'M'*/
    /*Unused*/ /*'n'*/
    "Nameless Things", /*'N'*/
    "Orcs", /*'o'*/
    /*Unused*/ /*'O'*/
    /*Unused*/ /*'p'*/
    /*Unused*/ /*'P'*/
    /*Unused*/ /*'q'*/
    /*Unused*/ /*'Q'*/
    /*Unused*/ /*'r'*/
    "Raukar", /*'R'*/
    "Serpents", /*'s'*/
    "Ancient Serpents", /*'S'*/
    /*Unused*/ /*'t'*/
    "Trolls", /*'T'*/
    /*Unused*/ /*'u'*/
    /*Unused*/ /*'U'*/
    "Vampires", /*'v'*/
    "Valar", /*'V'*/
    "Creeping Shadows", /*'w'*/
    "Wights and Wraiths", /*'W'*/
    /*Unused*/ /*'x'*/
    /*Unused*/ /*'X'*/
    /*Unused*/ /*'y'*/
    /*Unused*/ /*'Y'*/
    /*Unused*/ /*'Z'*/
    /*Unused*/ /*'Z'*/
    "Plants", /*'&'*/
    "People", /*'@'*/
    NULL };

/*
 * Symbols of monsters in each group. Note the "Uniques" group
 * is handled differently.
 */
static cptr monster_group_char[] = { (char*)-1L,
    /*"a", Unused*/
    /*"A", Unused*/
    "b",
    /*"B", Unused*/
    /*"c", Unused*/
    "C", "d", "D",
    /*"e", Unused*/
    /*"E", Unused*/
    "f",
    /*"F", Unused*/
    /*"g", Unused*/
    "G",
    /*"h", Unused*/
    "H",
    /*"i", Unused*/
    "I",
    /*"j", Unused*/
    /*"J", Unused*/
    /*"k", Unused*/
    /*"K", Unused*/
    /*"l", Unused*/
    /*"L", Unused*/
    "m", "M",
    /*"n", Unused*/
    "N", "o",
    /*"O", Unused*/
    /*"p", Unused*/
    /*"P", Unused*/
    /*"q", Unused*/
    /*"Q", Unused*/
    /*"r", Unused*/
    "R", "s", "S",
    /*"t", Unused*/
    "T",
    /*"u", Unused*/
    /*"U", Unused*/
    "v", "V", "w", "W",
    /*"x", Unused*/
    /*"X", Unused*/
    /*"y", Unused*/
    /*"Y", Unused*/
    /*"z", Unused*/
    /*"Z", Unused*/
    "&", // plants
    "@", // human/elf/dwarf
    NULL };

/*
 * Build a list of monster indexes in the given group. Return the number
 * of monsters in the group.
 */
static int collect_monsters(int grp_cur, monster_list_entry* mon_idx, int mode)
{
    int i, mon_count = 0;

    /* Get a list of x_char in this group */
    cptr group_char = monster_group_char[grp_cur];

    /* XXX Hack -- Check if this is the "Uniques" group */
    bool grp_unique = (monster_group_char[grp_cur] == (char*)-1L);

    /* Check every race */
    for (i = 1; i < z_info->r_max; i++)
    {
        /* Access the race */
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Is this a unique? */
        bool unique = (r_ptr->flags1 & (RF1_UNIQUE));

        /* Skip empty race */
        if (!r_ptr->name)
            continue;

        if (grp_unique && !(unique))
            continue;

        /* Require known monsters */
        if (!(mode & 0x02) && (!cheat_know) && (!know_monster_info)
            && (!(l_ptr->tsights)))
            continue;

        // Ignore monsters that can't be generated
        if (r_ptr->level > 25)
            continue;

        /* Check for race in the group */
        if ((grp_unique) || (strchr(group_char, r_ptr->d_char)))
        {
            /* Add the race */
            mon_idx[mon_count++].r_idx = i;

            /* XXX Hack -- Just checking for non-empty group */
            if (mode & 0x01)
                break;
        }
    }

    /* Terminate the list */
    mon_idx[mon_count].r_idx = 0;

    /* Return the number of races */
    return (mon_count);
}

#if 0
/*
 * Display the monsters in a group.
 */
static void display_monster_list(int col, int row, int per_page,
    monster_list_entry* mon_idx, int mon_cur, int mon_top, int grp_cur)
{
    int i;

    u32b known_uniques, dead_uniques, slay_count;

    /* Start with 0 kills*/
    known_uniques = dead_uniques = slay_count = 0;

    /* Count up monster kill counts */
    for (i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        // skip monsters that cannot be generated
        if ((r_ptr->rarity == 0) || (r_ptr->level > 25))
            continue;

        /* Require non-unique monsters */
        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            /*Count if we have seen the unique*/
            if (l_ptr->tsights)
            {
                known_uniques++;

                /*Count if the unique is dead*/
                if (r_ptr->max_num == 0)
                {
                    dead_uniques++;
                    slay_count++;
                }
            }

            // increase the uniques count anyway for forewarned or cheaters
            else if (know_monster_info || cheat_know)
            {
                known_uniques++;
            }
        }

        /* Collect "appropriate" monsters */
        else
            slay_count += l_ptr->pkills;
    }

    /* Display lines until done */
    for (i = 0; i < per_page && mon_idx[i].r_idx; i++)
    {
        byte attr;

        /* Get the race index */
        int r_idx = mon_idx[mon_top + i].r_idx;

        /* Access the race */
        monster_race* r_ptr = &r_info[r_idx];
        monster_lore* l_ptr = &l_list[r_idx];

        char race_name[80];

        /* Get the monster race name (singular)*/
        monster_desc_race(race_name, sizeof(race_name), r_idx);

        /* Choose a color */
        attr = ((i + mon_top == mon_cur) ? TERM_L_BLUE : TERM_WHITE);

        /* Display the name */
        c_prt(attr, race_name, row + i, col);

        if (cheat_know)
        {
            c_prt(attr, format("%d", r_idx), row + i, 60);
        }

        /* Display symbol */
        Term_putch(68, row + i, r_ptr->x_attr, r_ptr->x_char);
        if (use_bigtile)
        {
            if ((byte)(r_ptr->x_attr) & 0x80)
                Term_putch(69, row + i, 255, -1);
            else
                Term_putch(69, row + i, 0, ' ');
        }

        /* Display kills */
        if (r_ptr->flags1 & (RF1_UNIQUE))
        {
            /*use alive/dead for uniques*/
            put_str(format("%s", (r_ptr->max_num == 0) ? " dead" : "alive"),
                row + i, 73);
        }
        else
            put_str(format("%5d", l_ptr->pkills), row + i, 73);
    }

    /* Clear remaining lines */
    for (; i < per_page; i++)
    {
        Term_erase(col, row + i, 255);
    }

    /*Clear the monster count line*/
    Term_erase(0, 22, 255);

    if (monster_group_char[grp_cur] != (char*)-1L)
    {
        c_put_str(TERM_L_BLUE,
            format("Total Creatures Slain: %d. ", slay_count), 22, col + 2);
    }
    else
    {
        c_put_str(TERM_L_BLUE,
            format("Known Uniques: %d, Slain Uniques: %d.", known_uniques,
                dead_uniques),
            22, col + 2);
    }
}
#endif

/*
 * Display known monsters.
 */
void do_cmd_knowledge_monsters(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_MONSTERS);
}

/*
 * Add a pval so the object descriptions don't look strange*
 */
void apply_magic_fake(object_type* o_ptr)
{
    s16b old_pval = o_ptr->pval;

    /* Analyze type */
    switch (o_ptr->tval)
    {
    case TV_DIGGING:
    {
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;
        break;
    }

    /*many rings need a pval*/
    case TV_RING:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        /* Strength, Dexterity */
        case SV_RING_STR:
        case SV_RING_DEX:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;

            break;
        }

        /* Ring of Accuracy */
        case SV_RING_ACCURACY:
        {
            /* Bonus to hit */
            if (o_ptr->att < 1)
                o_ptr->att = 1;

            break;
        }

        /* Ring of Evasion */
        case SV_RING_EVASION:
        {
            /* Bonus to evasion */
            if (o_ptr->evn < 1)
                o_ptr->evn = 1;

            break;
        }

        /* Ring of Secrets */
        case SV_RING_SECRETS:
        {
            /* Bonus to perception */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;

            break;
        }

        /* Ring of Ered Luin */
        case SV_RING_ERED_LUIN:
        {
            /* Bonus to will */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Ring of the Laiquendi */
        case SV_RING_LAIQUENDI:
        {
            /* Bonus to stealth and archery */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }
        }

        /*break for TVAL-Rings*/
        break;
    }

    case TV_AMULET:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        /* Various amulets */
        case SV_AMULET_CON:
        case SV_AMULET_GRA:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Amulet of Protection */
        case SV_AMULET_PROTECTION:
        {
            if (o_ptr->pd < 1)
                o_ptr->pd = 1;
            if (o_ptr->ps < 1)
                o_ptr->ps = 1;
            break;
        }

        /* Amulet of the Blessed Realm */
        case SV_AMULET_BLESSED_REALM:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Amulet of the Vigilant Eye */
        case SV_AMULET_VIGILANT_EYE:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        default:
            break;
        }
        /*break for TVAL-Amulets*/
        break;
    }

    case TV_LIGHT:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
        case SV_LIGHT_MALLORN:
        case SV_LIGHT_LANTERN:
        {
            o_ptr->timeout = 0;

            break;
        }
        }
        /*break for TVAL-Lights*/
        break;
    }

    /*give them one charge*/
    case TV_STAFF:
    {
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;

        break;
    }
    }

    int pval_delta = (int)o_ptr->pval - (int)old_pval;
    if (pval_delta != 0)
        object_apply_pval_delta_with_mask(o_ptr, object_pval_flags1(o_ptr), pval_delta);
}

/*
 * Describe fake object
 */
static void desc_obj_fake(int k_idx)
{
    object_type* i_ptr;
    object_type object_type_body;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Wipe the object */
    object_wipe(i_ptr);

    /* Create the object */
    object_prep(i_ptr, k_idx);

    /*add minimum bonuses so the descriptions don't look strange*/
    apply_magic_fake(i_ptr);

    /* It's fully known */
    i_ptr->ident |= IDENT_KNOWN;

    /* Hack -- Handle stuff */
    handle_stuff();

    /* Reset the cursor */
    Term_gotoxy(0, 0);

    object_info_screen(i_ptr);
}

#if 0
/*
 * Display the objects in a group. (Incorporates some code from jdh)
 */
static void display_object_list(int col, int row, int per_page,
    object_list_entry object_idx[], int object_cur, int object_top)
{
    int i;

    /* Display lines until done */
    for (i = 0; i < per_page && object_idx[i].type != OBJ_NONE; i++)
    {
        char buf[80];

        /* Get the object index */
        int oidx = object_top + i;
        object_list_entry* obj = &object_idx[oidx];
        object_kind* k_ptr;
        ego_item_type* e_ptr;
        byte attr, cursor;

        switch (obj->type)
        {
        case OBJ_NORMAL:
            /* Access the object */
            k_ptr = &k_info[obj->idx];

            /* Choose a color */
            attr = ((k_ptr->aware) ? TERM_WHITE : TERM_SLATE);
            cursor = ((k_ptr->aware) ? TERM_L_BLUE : TERM_BLUE);
            attr = ((oidx == object_cur) ? cursor : attr);

            /* Acquire the basic "name" of the object*/
            strip_name(buf, obj->idx);

            /* Display the name */
            c_prt(attr, buf, row + i, col);

            if (cheat_know)
                c_prt(attr, format("%d", obj->idx), row + i, 70);

            if (k_ptr->aware)
            {
                /* Obtain attr/char */
                byte a = k_ptr->flavor ? (flavor_info[k_ptr->flavor].x_attr)
                                       : k_ptr->d_attr;
                byte c = k_ptr->flavor ? (flavor_info[k_ptr->flavor].x_char)
                                       : k_ptr->d_char;

                /* Display symbol */
                Term_putch(76, row + i, a, c);
            }

            break;

        case OBJ_SPECIAL:
            e_ptr = &e_info[obj->e_idx];

            /* Choose a color */
            attr = ((e_ptr->aware) ? TERM_WHITE : TERM_SLATE);
            cursor = ((e_ptr->aware) ? TERM_L_BLUE : TERM_BLUE);
            attr = ((oidx == object_cur) ? cursor : attr);

            if (obj->sval == -1)
            {
                buf[0] = '\0';
                snprintf(buf, sizeof(buf), "  %s", &e_name[e_ptr->name]);
            }
            else
            {
                int j;
                char buf2[80];

                /* Find the specific type */
                buf[0] = '\0';
                buf2[0] = '\0';
                for (j = 0; j < z_info->k_max; ++j)
                {
                    if ((k_info[j].tval == obj->tval)
                        && (k_info[j].sval == obj->sval))
                    {
                        strip_name(buf2, j);
                        break;
                    }
                }

                snprintf(buf, sizeof(buf), "%s %s", buf2, &e_name[e_ptr->name]);
            }

            c_prt(attr, buf, row + i, col);

            break;

        case OBJ_NONE:
        default:
            break;
        }
    }

    /* Clear remaining lines */
    for (; i < per_page; i++)
    {
        Term_erase(col, row + i, 255);
    }
}
#endif

static cptr knowledge_page_name(int page)
{
    switch (page)
    {
    case KNOWLEDGE_PAGE_ARTEFACTS:
        return "Artefacts";
    case KNOWLEDGE_PAGE_OBJECTS:
        return "Objects";
    case KNOWLEDGE_PAGE_MONSTERS:
        return "Monsters";
    case KNOWLEDGE_PAGE_CURSES:
        return "Curses";
    default:
        return "Known";
    }
}

static int knowledge_normalize_page(int page)
{
    if (page < KNOWLEDGE_PAGE_ARTEFACTS || page > KNOWLEDGE_PAGE_CURSES)
        return g_knowledge_last_page;

    return page;
}

static cptr knowledge_tab_label(int page)
{
    static const cptr labels[] = {
        "Arts",
        "Objs",
        "Mons",
        "Curses"
    };

    if (page < KNOWLEDGE_PAGE_ARTEFACTS || page > KNOWLEDGE_PAGE_CURSES)
        return "";

    return labels[page];
}

static int knowledge_tab_col(int page)
{
    int i;
    int col = 0;

    for (i = KNOWLEDGE_PAGE_ARTEFACTS; i < page; i++)
        col += (int)strlen(knowledge_tab_label(i)) + 1;

    return col;
}

static void knowledge_init_layout(knowledge_browser_layout* layout,
    int max_group_len, bool has_groups)
{
    int min_group_w = 8;
    int min_list_w = 16;

    Term_get_size(&layout->term_wid, &layout->term_hgt);

    if (layout->term_wid < 1)
        layout->term_wid = 80;
    if (layout->term_hgt < 1)
        layout->term_hgt = 24;

    layout->title_row = 0;
    layout->tabs_row = (layout->term_hgt > 1) ? 1 : 0;
    layout->header_row = (layout->term_hgt > 2) ? 2 : layout->tabs_row;
    layout->divider_row = (layout->term_hgt > 3) ? 3 : layout->header_row;
    layout->list_row = layout->divider_row + 1;
    layout->prompt_row = layout->term_hgt - 1;
    layout->status_row = (layout->prompt_row > layout->list_row)
        ? (layout->prompt_row - 1)
        : layout->prompt_row;
    layout->list_rows = layout->status_row - layout->list_row;
    if (layout->list_rows < 1)
        layout->list_rows = 1;

    if (!has_groups)
    {
        layout->group_col = 0;
        layout->group_w = 0;
        layout->divider_col = -1;
        layout->list_col = 0;
        layout->list_w = layout->term_wid;
        return;
    }

    layout->group_col = 0;
    layout->group_w = max_group_len;
    if (layout->group_w < 10)
        layout->group_w = 10;
    if (layout->group_w > layout->term_wid / 3)
        layout->group_w = layout->term_wid / 3;
    if (layout->group_w < min_group_w)
        layout->group_w = min_group_w;

    while ((layout->group_w > min_group_w)
        && (layout->term_wid - (layout->group_w + 3) < min_list_w))
    {
        layout->group_w--;
    }

    if (layout->term_wid - (layout->group_w + 3) < min_list_w)
    {
        layout->group_w = layout->term_wid - min_list_w - 3;
        if (layout->group_w < min_group_w)
            layout->group_w = min_group_w;
    }

    layout->divider_col = layout->group_w + 1;
    layout->list_col = layout->divider_col + 2;
    layout->list_w = layout->term_wid - layout->list_col;
    if (layout->list_w < 1)
        layout->list_w = 1;
}

static void knowledge_draw_tabs(const knowledge_browser_layout* layout, int page,
    bool tabs_focus)
{
    int i;
    int col = 0;

    Term_erase(0, layout->tabs_row, 255);

    for (i = KNOWLEDGE_PAGE_ARTEFACTS; i <= KNOWLEDGE_PAGE_CURSES; i++)
    {
        cptr label = knowledge_tab_label(i);
        byte attr = TERM_SLATE;
        int remaining = layout->term_wid - col;
        int len;

        if (remaining <= 0)
            break;

        if (i == page)
            attr = tabs_focus ? TERM_YELLOW : TERM_L_BLUE;

        len = (int)strlen(label);
        Term_putstr(col, layout->tabs_row, remaining, attr, label);
        col += len;

        if ((i < KNOWLEDGE_PAGE_CURSES) && (col < layout->term_wid))
        {
            Term_putstr(col, layout->tabs_row, layout->term_wid - col,
                TERM_SLATE, " ");
            col++;
        }
    }
}

static void knowledge_draw_frame(const knowledge_browser_layout* layout, int page,
    bool has_groups, cptr list_label, bool tabs_focus)
{
    int i;
    char title[64];
    char page_buf[16];

    Term_clear();

    strnfmt(title, sizeof(title), "Known lore - %s", knowledge_page_name(page));
    Term_putstr(0, layout->title_row, layout->term_wid, TERM_L_WHITE + TERM_SHADE,
        title);

    strnfmt(page_buf, sizeof(page_buf), "%d/4", page + 1);
    if ((int)strlen(page_buf) < layout->term_wid)
    {
        int page_col = layout->term_wid - (int)strlen(page_buf);
        Term_putstr(page_col, layout->title_row, layout->term_wid - page_col,
            TERM_SLATE, page_buf);
    }

    knowledge_draw_tabs(layout, page, tabs_focus);

    Term_erase(0, layout->header_row, 255);
    if (has_groups)
    {
        Term_putstr(layout->group_col, layout->header_row, layout->group_w,
            TERM_SLATE, "Group");
        Term_putstr(layout->list_col, layout->header_row, layout->list_w,
            TERM_SLATE, list_label);
    }
    else
    {
        Term_putstr(0, layout->header_row, layout->term_wid, TERM_SLATE,
            list_label);
    }

    for (i = 0; i < layout->term_wid; i++)
    {
        Term_putch(i, layout->divider_row, TERM_L_DARK, '=');
    }

    if (has_groups && layout->divider_col >= 0)
    {
        for (i = 0; i < layout->list_rows; i++)
        {
            Term_putch(layout->divider_col, layout->list_row + i, TERM_L_DARK, '|');
        }
    }

    if (layout->status_row != layout->prompt_row)
        Term_erase(0, layout->status_row, 255);
    Term_erase(0, layout->prompt_row, 255);
}

static void knowledge_draw_prompt(const knowledge_browser_layout* layout)
{
    char prompt[128];

    if (steamdeck_controls_active())
    {
        char prev_label[16];
        char next_label[16];
        char confirm_label[16];
        char recall_label[16];
        char back_label[16];

        controller_prompt_label('e', "L1", prev_label, sizeof(prev_label));
        controller_prompt_label('i', "R1", next_label, sizeof(next_label));
        controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        controller_prompt_label(steamdeck_info_key(), "RS", recall_label,
            sizeof(recall_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(prompt, sizeof(prompt),
            "D-pad move  [%s/%s] page  [%s/%s] recall  [%s] back",
            prev_label, next_label, confirm_label, recall_label, back_label);
        Term_putstr(0, layout->prompt_row, layout->term_wid, TERM_L_DARK, prompt);
    }
    else
    {
        SDL_strlcpy(prompt, "Dir move  e/i page  Up at top=tabs  Space/r recall  Esc",
            sizeof(prompt));
        Term_putstr(0, layout->prompt_row, layout->term_wid, TERM_SLATE, prompt);
    }
}

static void knowledge_clamp_group_state(int* column, int* grp_cur, int* grp_top,
    int grp_cnt, int* entry_cur, int* entry_top, int entry_cnt, int per_page)
{
    if (grp_cnt <= 0)
    {
        *column = 0;
        *grp_cur = 0;
        *grp_top = 0;
        *entry_cur = 0;
        *entry_top = 0;
        return;
    }

    if (*grp_cur >= grp_cnt)
        *grp_cur = grp_cnt - 1;
    if (*grp_cur < 0)
        *grp_cur = 0;
    if (*grp_top > *grp_cur)
        *grp_top = *grp_cur;
    if (*grp_cur >= *grp_top + per_page)
        *grp_top = *grp_cur - per_page + 1;
    if (*grp_top < 0)
        *grp_top = 0;

    if (entry_cnt <= 0)
    {
        *column = 0;
        *entry_cur = 0;
        *entry_top = 0;
    }
    else
    {
        if (*entry_cur >= entry_cnt)
            *entry_cur = entry_cnt - 1;
        if (*entry_cur < 0)
            *entry_cur = 0;
        if (*entry_top > *entry_cur)
            *entry_top = *entry_cur;
        if (*entry_cur >= *entry_top + per_page)
            *entry_top = *entry_cur - per_page + 1;
        if (*entry_top < 0)
            *entry_top = 0;
    }

    if (*column < 0)
        *column = 0;
    if (*column > 1)
        *column = 1;
    if (entry_cnt <= 0)
        *column = 0;
}

static void knowledge_clamp_list_state(int* cur, int* top, int count, int per_page)
{
    if (count <= 0)
    {
        *cur = 0;
        *top = 0;
        return;
    }

    if (*cur >= count)
        *cur = count - 1;
    if (*cur < 0)
        *cur = 0;
    if (*top > *cur)
        *top = *cur;
    if (*cur >= *top + per_page)
        *top = *cur - per_page + 1;
    if (*top < 0)
        *top = 0;
}

static void knowledge_display_groups(const knowledge_browser_layout* layout,
    int grp_idx[], cptr group_text[], int grp_cnt, int grp_cur, int grp_top)
{
    int i;

    for (i = 0; i < layout->list_rows; i++)
    {
        int y = layout->list_row + i;
        int idx = grp_top + i;

        Term_erase(layout->group_col, y, layout->group_w);

        if (idx >= grp_cnt)
            continue;

        Term_putstr(layout->group_col, y, layout->group_w,
            (idx == grp_cur) ? TERM_L_BLUE : TERM_WHITE,
            group_text[grp_idx[idx]]);
    }
}

static void knowledge_display_artefacts(const knowledge_browser_layout* layout,
    int artefact_idx[], int artefact_cnt, int artefact_cur, int artefact_top)
{
    bool show_debug = cheat_know && (layout->term_wid >= 78);
    int idx_col = layout->term_wid - 12;
    int dep_col = layout->term_wid - 8;
    int rar_col = layout->term_wid - 4;
    int name_w = layout->list_w;
    int i;

    if (show_debug)
    {
        name_w = idx_col - layout->list_col - 1;
        if (name_w < 12)
            show_debug = false;
    }

    if (show_debug)
    {
        Term_putstr(idx_col, layout->header_row, 3, TERM_SLATE, "Idx");
        Term_putstr(dep_col, layout->header_row, 3, TERM_SLATE, "Dep");
        Term_putstr(rar_col, layout->header_row, 3, TERM_SLATE, "Rar");
    }

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int idx = artefact_top + i;
        object_type object_type_body;
        object_type* i_ptr = &object_type_body;
        char o_name[80];
        byte attr;

        Term_erase(layout->list_col, row, 255);

        if (idx >= artefact_cnt)
            continue;

        attr = (idx == artefact_cur) ? TERM_L_BLUE : TERM_WHITE;
        object_wipe(i_ptr);
        prepare_fake_artefact(i_ptr, artefact_idx[idx]);
        object_desc(o_name, sizeof(o_name), i_ptr, true, 0);
        Term_putstr(layout->list_col, row, name_w, attr, o_name);

        if (show_debug)
        {
            artefact_type* a_ptr = &a_info[artefact_idx[idx]];
            c_prt(attr, format("%3d", artefact_idx[idx]), row, idx_col);
            c_prt(attr, format("%3d", a_ptr->level), row, dep_col);
            c_prt(attr, format("%3d", a_ptr->rarity), row, rar_col);
        }
    }
}

static void knowledge_display_objects(const knowledge_browser_layout* layout,
    object_list_entry object_idx[], int object_cnt, int object_cur, int object_top)
{
    bool show_idx = cheat_know && (layout->term_wid >= 70);
    bool show_sym = (layout->term_wid >= 44);
    int idx_col = layout->term_wid - 5;
    int sym_col = layout->term_wid - (use_bigtile ? 2 : 1);
    int name_w = layout->list_w;
    int i;

    if (show_idx)
    {
        name_w = idx_col - layout->list_col - 1;
        if (name_w < 12)
            show_idx = false;
    }

    if (show_sym)
    {
        int sym_name_w = sym_col - layout->list_col - 1;
        if (sym_name_w < name_w)
            name_w = sym_name_w;
        if (name_w < 12)
            show_sym = false;
    }

    if (show_idx)
        Term_putstr(idx_col, layout->header_row, 3, TERM_SLATE, "Idx");
    if (show_sym)
        Term_putstr(sym_col, layout->header_row, 3, TERM_SLATE, "Sym");

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int oidx = object_top + i;
        object_list_entry* obj;
        object_kind* k_ptr;
        ego_item_type* e_ptr;
        byte attr;
        byte cursor;
        char buf[80];

        Term_erase(layout->list_col, row, 255);

        if (oidx >= object_cnt)
            continue;

        obj = &object_idx[oidx];

        switch (obj->type)
        {
        case OBJ_NORMAL:
            k_ptr = &k_info[obj->idx];
            attr = k_ptr->aware ? TERM_WHITE : TERM_SLATE;
            cursor = k_ptr->aware ? TERM_L_BLUE : TERM_BLUE;
            attr = (oidx == object_cur) ? cursor : attr;

            strip_name(buf, obj->idx);
            Term_putstr(layout->list_col, row, name_w, attr, buf);

            if (show_idx)
                c_prt(attr, format("%d", obj->idx), row, idx_col);

            if (show_sym && k_ptr->aware)
            {
                byte a = k_ptr->flavor ? flavor_info[k_ptr->flavor].x_attr : k_ptr->d_attr;
                byte c = k_ptr->flavor ? flavor_info[k_ptr->flavor].x_char : k_ptr->d_char;
                Term_putch(sym_col, row, a, c);
                if (use_bigtile)
                {
                    if (a & 0x80)
                        Term_putch(sym_col + 1, row, 255, -1);
                    else
                        Term_putch(sym_col + 1, row, 0, ' ');
                }
            }
            break;

        case OBJ_SPECIAL:
            e_ptr = &e_info[obj->e_idx];
            attr = e_ptr->aware ? TERM_WHITE : TERM_SLATE;
            cursor = e_ptr->aware ? TERM_L_BLUE : TERM_BLUE;
            attr = (oidx == object_cur) ? cursor : attr;

            if (obj->sval == -1)
            {
                strnfmt(buf, sizeof(buf), "  %s", &e_name[e_ptr->name]);
            }
            else
            {
                int j;
                char buf2[80];

                buf[0] = '\0';
                buf2[0] = '\0';
                for (j = 0; j < z_info->k_max; ++j)
                {
                    if ((k_info[j].tval == obj->tval) && (k_info[j].sval == obj->sval))
                    {
                        strip_name(buf2, j);
                        break;
                    }
                }

                strnfmt(buf, sizeof(buf), "%s %s", buf2, &e_name[e_ptr->name]);
            }

            Term_putstr(layout->list_col, row, name_w, attr, buf);
            break;

        case OBJ_NONE:
        default:
            break;
        }
    }
}

static void knowledge_monster_summary(char* buf, size_t buflen, int grp_cur)
{
    int i;
    u32b known_uniques = 0;
    u32b dead_uniques = 0;
    u32b slay_count = 0;

    for (i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        if ((r_ptr->rarity == 0) || (r_ptr->level > 25))
            continue;

        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            if (l_ptr->tsights)
            {
                known_uniques++;
                if (r_ptr->max_num == 0)
                {
                    dead_uniques++;
                    slay_count++;
                }
            }
            else if (know_monster_info || cheat_know)
            {
                known_uniques++;
            }
        }
        else
        {
            slay_count += l_ptr->pkills;
        }
    }

    if (monster_group_char[grp_cur] != (char*)-1L)
    {
        strnfmt(buf, buflen, "Total creatures slain: %u.", (unsigned)slay_count);
    }
    else
    {
        strnfmt(buf, buflen, "Known uniques: %u, slain uniques: %u.",
            (unsigned)known_uniques, (unsigned)dead_uniques);
    }
}

static void knowledge_display_monsters(const knowledge_browser_layout* layout,
    monster_list_entry mon_idx[], int mon_cnt, int mon_cur, int mon_top)
{
    bool show_sym = (layout->term_wid >= 44);
    bool show_kills = (layout->term_wid >= 56);
    int kills_col = layout->term_wid - 5;
    int sym_col = show_kills ? (kills_col - 2) : (layout->term_wid - (use_bigtile ? 2 : 1));
    int name_w = layout->list_w;
    int i;

    if (show_sym)
    {
        int sym_name_w = sym_col - layout->list_col - 1;
        if (sym_name_w < name_w)
            name_w = sym_name_w;
        if (name_w < 12)
            show_sym = false;
    }

    if (show_sym)
        Term_putstr(sym_col, layout->header_row, 3, TERM_SLATE, "Sym");
    if (show_kills)
        Term_putstr(kills_col, layout->header_row, 5, TERM_SLATE, "Kills");

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int idx = mon_top + i;
        int r_idx;
        monster_race* r_ptr;
        monster_lore* l_ptr;
        byte attr;
        char race_name[80];

        Term_erase(layout->list_col, row, 255);

        if (idx >= mon_cnt)
            continue;

        r_idx = mon_idx[idx].r_idx;
        r_ptr = &r_info[r_idx];
        l_ptr = &l_list[r_idx];
        attr = (idx == mon_cur) ? TERM_L_BLUE : TERM_WHITE;

        monster_desc_race(race_name, sizeof(race_name), r_idx);
        Term_putstr(layout->list_col, row, name_w, attr, race_name);

        if (show_sym)
        {
            Term_putch(sym_col, row, r_ptr->x_attr, r_ptr->x_char);
            if (use_bigtile)
            {
                if ((byte)(r_ptr->x_attr) & 0x80)
                    Term_putch(sym_col + 1, row, 255, -1);
                else
                    Term_putch(sym_col + 1, row, 0, ' ');
            }
        }

        if (show_kills)
        {
            if (r_ptr->flags1 & RF1_UNIQUE)
                put_str((r_ptr->max_num == 0) ? " dead" : "alive", row, kills_col);
            else
                put_str(format("%5d", l_ptr->pkills), row, kills_col);
        }
    }
}

static int knowledge_collect_curses(int curse_idx[])
{
    int id;
    int count = 0;

    for (id = 0; id < (int)z_info->cu_max; id++)
    {
        if (CURSE_SEEN(id))
            curse_idx[count++] = id;
    }

    return count;
}

static cptr knowledge_curse_display_name(int idx)
{
    cptr raw = cu_name + cu_info[idx].name;

    if (strncmp(raw, "Curse of ", 9) == 0)
        raw += 9;
    else if (strncmp(raw, "Burden of ", 10) == 0)
        raw += 10;
    else if (strncmp(raw, "Sorrow of ", 10) == 0)
        raw += 10;
    else if (strncmp(raw, "Doom of ", 8) == 0)
        raw += 8;

    return raw;
}

static cptr knowledge_blessing_display_name(int idx)
{
    if (cu_info[idx].blessing_name)
    {
        cptr raw = cu_name + cu_info[idx].blessing_name;

        if (strncmp(raw, "Blessing of ", 12) == 0)
            raw += 12;

        return raw;
    }

    return knowledge_curse_display_name(idx);
}

static void knowledge_display_curses(const knowledge_browser_layout* layout,
    int curse_idx[], int curse_cnt, int curse_cur, int curse_top)
{
    int i;

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int idx = curse_top + i;
        int id;
        byte attr;

        Term_erase(0, row, 255);

        if (idx >= curse_cnt)
            continue;

        id = curse_idx[idx];
        attr = (idx == curse_cur) ? TERM_L_BLUE : TERM_L_RED;
        Term_putstr(0, row, layout->term_wid, attr,
            knowledge_curse_display_name(id));
    }
}

static void knowledge_detail_prompt(int row, bool steamdeck, cptr title,
    cptr accept_label)
{
    Term_erase(0, row, 255);
    if (steamdeck)
    {
        char hint_buf[48];
        strnfmt(hint_buf, sizeof(hint_buf), "(press %s)", accept_label);
        Term_putstr(1, row, -1, TERM_L_WHITE, hint_buf);
    }
    else
    {
        Term_putstr(1, row, -1, TERM_L_WHITE, "(press any key)");
    }

    (void)inkey();
    Term_clear();
    Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, title);
}

static void knowledge_show_curse_detail(int curse_id)
{
    int row = 2;
    int wrap_width = Term->wid - 4;
    int page_limit = Term->hgt - 3;
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    curse_type* c = &cu_info[curse_id];
    cptr cname = cu_name + c->name;
    cptr cdesc = cu_text + c->text;
    cptr cpower = cu_text + c->power;
    cptr bname = knowledge_blessing_display_name(curse_id);
    cptr bdesc = (c->blessing_text) ? (cu_text + c->blessing_text) : "";
    cptr bpower = (c->blessing_power) ? (cu_text + c->blessing_power) : "";
    bool has_blessing_text = bdesc && *bdesc;
    bool has_blessing_effect = bpower && *bpower;
    bool has_blessing_info = has_blessing_text || has_blessing_effect
        || (c->blessing_name != 0);
    char effect_line[256];

    if (wrap_width < 20)
        wrap_width = 20;

    if (steamdeck)
    {
        controller_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }

    screen_save();
    Term_clear();
    Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, "Known Curse:");

    text_out_hook = text_out_to_screen;
    text_out_wrap = wrap_width;

    c_put_str(TERM_L_RED, cname, row++, 1);

    if (row + count_wrapped_lines(cdesc, text_out_wrap, 3) >= page_limit)
        knowledge_detail_prompt(row, steamdeck, "Known Curse:", accept_label);
    Term_gotoxy(3, row);
    text_out_c(TERM_WHITE, cdesc);
    row += count_wrapped_lines(cdesc, text_out_wrap, 3);

    strnfmt(effect_line, sizeof(effect_line), "Effect: %s",
        (*cpower) ? cpower : "[no additional effect listed]");
    if (row + count_wrapped_lines(effect_line, text_out_wrap, 3) >= page_limit)
        knowledge_detail_prompt(row, steamdeck, "Known Curse:", accept_label);
    Term_gotoxy(3, row);
    text_out_c(TERM_RED, "Effect: ");
    text_out_c(TERM_L_DARK, (*cpower) ? cpower : "[no additional effect listed]");
    row += count_wrapped_lines(effect_line, text_out_wrap, 3);

    row++;

    if (has_blessing_info)
    {
        char blessing_line[256];

        if (row + 1 >= page_limit)
            knowledge_detail_prompt(row, steamdeck, "Known Curse:", accept_label);

        Term_putstr(3, row++, -1, TERM_L_GREEN, format("Blessing: %s", bname));

        if (has_blessing_text)
        {
            if (row + count_wrapped_lines(bdesc, text_out_wrap, 5) >= page_limit)
                knowledge_detail_prompt(row, steamdeck, "Known Curse:",
                    accept_label);
            Term_gotoxy(5, row);
            text_out_c(TERM_WHITE, bdesc);
            row += count_wrapped_lines(bdesc, text_out_wrap, 5);
        }

        strnfmt(blessing_line, sizeof(blessing_line), "Effect: %s",
            has_blessing_effect ? bpower : "[no additional effect listed]");
        if (row + count_wrapped_lines(blessing_line, text_out_wrap, 5) >= page_limit)
            knowledge_detail_prompt(row, steamdeck, "Known Curse:",
                accept_label);
        Term_gotoxy(5, row);
        text_out_c(TERM_L_GREEN, "Effect: ");
        text_out_c(TERM_WHITE,
            has_blessing_effect ? bpower : "[no additional effect listed]");
        row += count_wrapped_lines(blessing_line, text_out_wrap, 5);
    }

    if (row + 1 >= Term->hgt)
        row = Term->hgt - 2;

    knowledge_detail_prompt(row + 1, steamdeck, "Known Curse:", accept_label);
    screen_load();
}

static bool knowledge_handle_page_input(char ch, int* page)
{
    int next_page = *page;

    switch (ch)
    {
    case 'A':
    case 'a':
        next_page = KNOWLEDGE_PAGE_ARTEFACTS;
        break;
    case 'B':
    case 'b':
        next_page = KNOWLEDGE_PAGE_OBJECTS;
        break;
    case 'N':
    case 'n':
        next_page = KNOWLEDGE_PAGE_MONSTERS;
        break;
    case 'U':
    case 'u':
        next_page = KNOWLEDGE_PAGE_CURSES;
        break;
    case '\t':
    case ']':
    case 'I':
    case 'i':
        next_page = (*page + 1) % 4;
        break;
    case '[':
    case 'E':
    case 'e':
        next_page = (*page + 3) % 4;
        break;
    default:
        return false;
    }

    *page = next_page;
    g_knowledge_last_page = next_page;
    return true;
}

static bool knowledge_handle_tab_navigation(char ch, int* page, bool* tabs_focus,
    bool can_focus_tabs)
{
    int d = target_dir(ch);

    if (!*tabs_focus)
    {
        if (can_focus_tabs && d && !ddx[d] && (ddy[d] < 0))
        {
            *tabs_focus = true;
            return true;
        }

        return false;
    }

    if (d)
    {
        if (ddx[d] > 0)
        {
            *page = (*page + 1) % 4;
            g_knowledge_last_page = *page;
            return true;
        }
        if (ddx[d] < 0)
        {
            *page = (*page + 3) % 4;
            g_knowledge_last_page = *page;
            return true;
        }
        if (ddy[d] > 0)
        {
            *tabs_focus = false;
            return true;
        }
        if (ddy[d] < 0)
        {
            return true;
        }
    }

    return false;
}

static bool knowledge_is_recall_input(int ch)
{
    int confirm_key = steamdeck_confirm_key();

    if (ch == ' ' || ch == 'R' || ch == 'r' || ch == 'X' || ch == 'x'
        || ch == INPUT_BIND_CONFIRM)
    {
        return true;
    }

    if (confirm_key != GAMEPAD_BIND_NONE && ch == confirm_key)
        return true;

    return false;
}

void do_cmd_knowledge_browser_page(int page)
{
    int i;
    int artefact_grp_idx[100];
    int object_grp_idx[100];
    int monster_grp_idx[100];
    int* artefact_idx = mem_alloc_array(z_info->art_max, int);
    object_list_entry* object_idx =
        mem_alloc_array(z_info->k_max + z_info->e_max + 1, object_list_entry);
    monster_list_entry* mon_idx =
        mem_alloc_array(z_info->r_max, monster_list_entry);
    int* curse_idx = mem_alloc_array(z_info->cu_max, int);
    int artefact_grp_cnt = 0;
    int object_grp_cnt = 0;
    int monster_grp_cnt = 0;
    int artefact_group_w = 0;
    int object_group_w = 0;
    int monster_group_w = 0;
    int curse_cnt = 0;
    int artefact_old = -1;
    int object_old = -1;
    int monster_old = -1;
    knowledge_browser_state state = { 0 };
    bool done = false;

    page = knowledge_normalize_page(page);
    g_knowledge_last_page = page;

    FILE_TYPE(FILE_TYPE_TEXT);

    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0)
    {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    for (i = 0; object_group_text[i] != NULL; i++)
    {
        int len = (int)strlen(object_group_text[i]);

        if (len > artefact_group_w)
            artefact_group_w = len;
        if (len > object_group_w)
            object_group_w = len;

        if (collect_artefacts(i, artefact_idx))
            artefact_grp_idx[artefact_grp_cnt++] = i;
        if (collect_objects(i, NULL))
            object_grp_idx[object_grp_cnt++] = i;
    }

    for (i = 0; monster_group_text[i] != NULL; i++)
    {
        int len = (int)strlen(monster_group_text[i]);

        if (len > monster_group_w)
            monster_group_w = len;
        if ((monster_group_char[i] == (char*)-1L)
            || collect_monsters(i, mon_idx, 0x01))
        {
            monster_grp_idx[monster_grp_cnt++] = i;
        }
    }

    curse_cnt = knowledge_collect_curses(curse_idx);

    screen_save();

    while (!done)
    {
        knowledge_browser_layout layout;
        int ch;

        switch (page)
        {
        case KNOWLEDGE_PAGE_ARTEFACTS:
        {
            int artefact_cnt = 0;
            int selected_artefact = -1;
            char status[96];

            knowledge_init_layout(&layout, artefact_group_w, true);
            if (artefact_grp_cnt > 0)
                artefact_cnt = collect_artefacts(
                    artefact_grp_idx[state.group_cur[page]], artefact_idx);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], artefact_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], artefact_cnt, layout.list_rows);
            if (artefact_grp_cnt > 0)
                artefact_cnt = collect_artefacts(
                    artefact_grp_idx[state.group_cur[page]], artefact_idx);

            knowledge_draw_frame(&layout, page, true, "Artefact",
                state.tabs_focus);
            knowledge_display_groups(&layout, artefact_grp_idx, object_group_text,
                artefact_grp_cnt, state.group_cur[page], state.group_top[page]);
            knowledge_display_artefacts(&layout, artefact_idx, artefact_cnt,
                state.entry_cur[page], state.entry_top[page]);

            if (artefact_cnt > 0)
            {
                selected_artefact = artefact_idx[state.entry_cur[page]];
                strnfmt(status, sizeof(status), "%d artefact%s in %s.",
                    artefact_cnt, (artefact_cnt == 1) ? "" : "s",
                    object_group_text[artefact_grp_idx[state.group_cur[page]]]);
            }
            else
            {
                SDL_strlcpy(status, "No known artefacts yet.", sizeof(status));
            }
            if (layout.status_row != layout.prompt_row)
                Term_putstr(0, layout.status_row, layout.term_wid, TERM_L_BLUE, status);
            knowledge_draw_prompt(&layout);

            if (selected_artefact != artefact_old)
            {
                handle_stuff();
                artefact_old = selected_artefact;
            }

            if (state.tabs_focus)
            {
                Term_gotoxy(knowledge_tab_col(page), layout.tabs_row);
            }
            else if (artefact_grp_cnt > 0)
            {
                if (state.column[page] == 0)
                    Term_gotoxy(0, layout.list_row
                        + (state.group_cur[page] - state.group_top[page]));
                else
                    Term_gotoxy(layout.list_col, layout.list_row
                        + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (artefact_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (artefact_cnt > 0)
                    desc_art_fake(artefact_idx[state.entry_cur[page]]);
                else
                    bell("Nothing to recall.");
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], artefact_grp_cnt,
                    &state.entry_cur[page], artefact_cnt, layout.list_rows);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_OBJECTS:
        {
            int object_cnt = 0;
            int tracked_kind = 0;
            char status[112];

            knowledge_init_layout(&layout, object_group_w, true);
            if (object_grp_cnt > 0)
                object_cnt = collect_objects(
                    object_grp_idx[state.group_cur[page]], object_idx);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], object_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], object_cnt, layout.list_rows);
            if (object_grp_cnt > 0)
                object_cnt = collect_objects(
                    object_grp_idx[state.group_cur[page]], object_idx);

            knowledge_draw_frame(&layout, page, true, "Object",
                state.tabs_focus);
            knowledge_display_groups(&layout, object_grp_idx, object_group_text,
                object_grp_cnt, state.group_cur[page], state.group_top[page]);
            knowledge_display_objects(&layout, object_idx, object_cnt,
                state.entry_cur[page], state.entry_top[page]);

            if ((object_cnt > 0)
                && (object_idx[state.entry_cur[page]].type == OBJ_NORMAL))
            {
                tracked_kind = object_idx[state.entry_cur[page]].idx;
            }

            if (object_cnt > 0)
            {
                object_list_entry* obj = &object_idx[state.entry_cur[page]];
                if ((obj->type == OBJ_NORMAL) && k_info[obj->idx].aware)
                {
                    strnfmt(status, sizeof(status), "%d object%s in %s. Recall available.",
                        object_cnt, (object_cnt == 1) ? "" : "s",
                        object_group_text[object_grp_idx[state.group_cur[page]]]);
                }
                else
                {
                    strnfmt(status, sizeof(status),
                        "%d object%s in %s. Recall works for identified base items.",
                        object_cnt, (object_cnt == 1) ? "" : "s",
                        object_group_text[object_grp_idx[state.group_cur[page]]]);
                }
            }
            else
            {
                SDL_strlcpy(status, "No known objects yet.", sizeof(status));
            }
            if (layout.status_row != layout.prompt_row)
                Term_putstr(0, layout.status_row, layout.term_wid, TERM_L_BLUE, status);
            knowledge_draw_prompt(&layout);

            if (tracked_kind != object_old)
            {
                object_kind_track(tracked_kind);
                handle_stuff();
                object_old = tracked_kind;
            }

            if (state.tabs_focus)
            {
                Term_gotoxy(knowledge_tab_col(page), layout.tabs_row);
            }
            else if (object_grp_cnt > 0)
            {
                if (state.column[page] == 0)
                    Term_gotoxy(0, layout.list_row
                        + (state.group_cur[page] - state.group_top[page]));
                else
                    Term_gotoxy(layout.list_col, layout.list_row
                        + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (object_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if ((object_cnt > 0)
                    && (object_idx[state.entry_cur[page]].type == OBJ_NORMAL)
                    && k_info[object_idx[state.entry_cur[page]].idx].aware)
                {
                    desc_obj_fake(object_idx[state.entry_cur[page]].idx);
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], object_grp_cnt,
                    &state.entry_cur[page], object_cnt, layout.list_rows);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_MONSTERS:
        {
            int monster_cnt = 0;
            int selected_r_idx = 0;
            char status[96];

            knowledge_init_layout(&layout, monster_group_w, true);
            if (monster_grp_cnt > 0)
                monster_cnt = collect_monsters(
                    monster_grp_idx[state.group_cur[page]], mon_idx, 0x00);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], monster_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], monster_cnt, layout.list_rows);
            if (monster_grp_cnt > 0)
                monster_cnt = collect_monsters(
                    monster_grp_idx[state.group_cur[page]], mon_idx, 0x00);

            knowledge_draw_frame(&layout, page, true, "Monster",
                state.tabs_focus);
            knowledge_display_groups(&layout, monster_grp_idx, monster_group_text,
                monster_grp_cnt, state.group_cur[page], state.group_top[page]);
            knowledge_display_monsters(&layout, mon_idx, monster_cnt,
                state.entry_cur[page], state.entry_top[page]);

            if (monster_cnt > 0)
            {
                selected_r_idx = mon_idx[state.entry_cur[page]].r_idx;
                knowledge_monster_summary(status, sizeof(status),
                    monster_grp_idx[state.group_cur[page]]);
            }
            else
            {
                SDL_strlcpy(status, "No known monsters in this group yet.",
                    sizeof(status));
            }
            if (layout.status_row != layout.prompt_row)
                Term_putstr(0, layout.status_row, layout.term_wid, TERM_L_BLUE, status);
            knowledge_draw_prompt(&layout);

            if (selected_r_idx != monster_old)
            {
                monster_race_track(selected_r_idx);
                handle_stuff();
                monster_old = selected_r_idx;
            }

            if (state.tabs_focus)
            {
                Term_gotoxy(knowledge_tab_col(page), layout.tabs_row);
            }
            else if (monster_grp_cnt > 0)
            {
                if (state.column[page] == 0)
                    Term_gotoxy(0, layout.list_row
                        + (state.group_cur[page] - state.group_top[page]));
                else
                    Term_gotoxy(layout.list_col, layout.list_row
                        + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (monster_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (monster_cnt > 0)
                {
                    screen_roff(mon_idx[state.entry_cur[page]].r_idx, NULL);
                    (void)inkey();
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], monster_grp_cnt,
                    &state.entry_cur[page], monster_cnt, layout.list_rows);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_CURSES:
        default:
        {
            char status[256];

            knowledge_init_layout(&layout, 0, false);
            knowledge_clamp_list_state(&state.entry_cur[page], &state.entry_top[page],
                curse_cnt, layout.list_rows);
            knowledge_draw_frame(&layout, page, false, "Known curses",
                state.tabs_focus);
            knowledge_display_curses(&layout, curse_idx, curse_cnt,
                state.entry_cur[page], state.entry_top[page]);

            if (curse_cnt > 0)
            {
                curse_type* c = &cu_info[curse_idx[state.entry_cur[page]]];
                cptr cpower = cu_text + c->power;
                strnfmt(status, sizeof(status), "Effect: %s",
                    (*cpower) ? cpower : "[no additional effect listed]");
            }
            else
            {
                SDL_strlcpy(status, "No known curses yet.", sizeof(status));
            }
            if (layout.status_row != layout.prompt_row)
                Term_putstr(0, layout.status_row, layout.term_wid, TERM_L_BLUE, status);
            knowledge_draw_prompt(&layout);

            if (state.tabs_focus)
            {
                Term_gotoxy(knowledge_tab_col(page), layout.tabs_row);
            }
            else if (curse_cnt > 0)
            {
                Term_gotoxy(0, layout.list_row
                    + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus, (curse_cnt <= 0) || (state.entry_cur[page] == 0)))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (curse_cnt > 0)
                    knowledge_show_curse_detail(curse_idx[state.entry_cur[page]]);
                else
                    bell("Nothing to recall.");
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
            {
                int d = target_dir(ch);
                int page_jump = (layout.list_rows > 0) ? layout.list_rows : 1;

                if (curse_cnt <= 0)
                {
                    state.entry_cur[page] = 0;
                    break;
                }

                if (!d)
                    break;

                if (ddx[d] && ddy[d])
                    state.entry_cur[page] += ddy[d] * page_jump;
                else if (ddy[d])
                    state.entry_cur[page] += ddy[d];

                if (state.entry_cur[page] < 0)
                    state.entry_cur[page] = 0;
                if (state.entry_cur[page] >= curse_cnt)
                    state.entry_cur[page] = curse_cnt - 1;
                break;
            }
            }
            break;
        }
        }
    }

    mem_free_null(curse_idx);
    mem_free_null(mon_idx);
    mem_free_null(object_idx);
    mem_free_null(artefact_idx);

    screen_load();
}

/*
 * Display known objects
 */
bool do_cmd_knowledge_supplies(const supply_menu_request* request)
{
    int i;
    int max = 0;
    int grp_cnt = SUPPLY_GROUP_MAX;
    int grp_idx[SUPPLY_GROUP_MAX + 1];
    int group_totals[SUPPLY_GROUP_MAX];
    supply_list_entry* entries;
    int grp_cur = 0;
    int grp_top = 0;
    int entry_cur = 0;
    int entry_top = 0;
    int column = 0;
    bool flag = false;
    bool redraw = true;
    supply_menu_action forced_action = SUPPLY_MENU_ACTION_NONE;
    bool hotkey_mode = false;
    bool acted = false;
    bool refresh_after_close = false;

    if (request)
    {
        forced_action = request->action;
        hotkey_mode = request->hotkey_mode;
        if (request->focus_group && request->group >= 0 && request->group < SUPPLY_GROUP_MAX)
            grp_cur = request->group;
        if (forced_action != SUPPLY_MENU_ACTION_NONE)
            column = 1;
    }

    for (i = 0; i < SUPPLY_GROUP_MAX; i++)
    {
        int len = strlen(supply_group_text[i]) + 5;
        if (len > max)
            max = len;
        grp_idx[i] = i;
    }
    grp_idx[grp_cnt] = -1;
    max += 2;

    entries = mem_alloc_array(z_info->k_max, supply_list_entry);

    screen_save();

    while (!flag)
    {
        int entry_cnt;
        knowledge_browser_layout layout;
        int count_col;
        int sym_col;
        int used_weight;
        int max_weight;
        char weight_buf[80];

        compute_supply_group_totals(group_totals);
        knowledge_init_layout(&layout, max, true);
        count_col = layout.term_wid - 6;
        sym_col = layout.term_wid - (use_bigtile ? 2 : 1);
        used_weight = supplies_total_weight();
        max_weight = supplies_current_weight_cap();
        strnfmt(weight_buf, sizeof(weight_buf),
            "Supply weight: %d.%1d/%d.%1d lb used",
            used_weight / 10, used_weight % 10,
            max_weight / 10, max_weight % 10);

        if (count_col <= layout.list_col + 8)
            count_col = layout.list_col + 8;
        if (sym_col <= count_col + 4)
            sym_col = count_col + 4;
        if (sym_col >= layout.term_wid)
            sym_col = layout.term_wid - (use_bigtile ? 2 : 1);
        if (count_col >= sym_col)
            count_col = sym_col - 4;

        if (grp_cur >= grp_cnt)
            grp_cur = grp_cnt - 1;
        if (grp_cur < 0)
            grp_cur = 0;

        entry_cnt = collect_supply_entries(grp_idx[grp_cur], entries);

        if (entry_cnt == 0)
        {
            entry_cur = 0;
            entry_top = 0;
            if (column)
                column = 0;
        }
        else
        {
            if (entry_cur >= entry_cnt)
                entry_cur = entry_cnt - 1;
            if (entry_cur < 0)
                entry_cur = 0;

            if (entry_cur < entry_top)
                entry_top = entry_cur;
            if (entry_cur >= entry_top + layout.list_rows)
                entry_top = entry_cur - layout.list_rows + 1;
            if (entry_top < 0)
                entry_top = 0;
        }

        if (grp_cur < grp_top)
            grp_top = grp_cur;
        if (grp_cur >= grp_top + layout.list_rows)
            grp_top = grp_cur - layout.list_rows + 1;
        if (grp_top < 0)
            grp_top = 0;

        if (redraw)
        {
            Term_clear();
            Term_putstr(0, layout.title_row, layout.term_wid, TERM_L_WHITE + TERM_SHADE,
                "Supplies - Herbs, Potions, Gems");
            Term_putstr(0, layout.tabs_row, layout.term_wid, TERM_SLATE, weight_buf);
            Term_putstr(0, layout.header_row, layout.group_w, TERM_SLATE, "Group");
            Term_putstr(layout.list_col, layout.header_row, layout.list_w, TERM_SLATE,
                "Name");
            Term_putstr(count_col, layout.header_row, 3, TERM_SLATE, "Qty");
            Term_putstr(sym_col, layout.header_row, 3, TERM_SLATE, "Sym");

            for (i = 0; i < layout.term_wid; i++)
                Term_putch(i, layout.divider_row, TERM_L_DARK, '=');

            for (i = 0; i < layout.list_rows; i++)
                Term_putch(layout.divider_col, layout.list_row + i, TERM_L_DARK, '|');

            redraw = false;
        }

        display_supply_group_list(0, layout.list_row, layout.group_w, layout.list_rows, grp_idx,
            grp_cur, grp_top, group_totals);
        display_supply_list(layout.list_col, layout.list_row, layout.list_rows,
            entries, entry_cnt, entry_cur, entry_top, count_col, sym_col,
            grp_idx[grp_cur], column);

        /* Bottom bar: grey text with white first letters */
        Term_erase(0, layout.prompt_row, 255);
        if (steamdeck_controls_active()) {
            char recall_label[16];
            char use_label[16];
            char confirm_label[16];
            char drop_label[16];
            char back_label[16];
            char prompt_buf[160];

            /* Steam Deck UI: RS Right=recall, X=use, A=confirm, d=drop, B=back */
            controller_prompt_label(steamdeck_info_key(), "RS Right", recall_label, sizeof(recall_label));
            controller_prompt_label(steamdeck_alt_action_key(), "X", use_label, sizeof(use_label));
            controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
            controller_prompt_label('d', "d", drop_label, sizeof(drop_label));
            controller_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));

            strnfmt(prompt_buf, sizeof(prompt_buf),
                "D-pad move  [%s] recall  [%s/%s] use  [%s] drop  [%s] back",
                recall_label, use_label, confirm_label, drop_label, back_label);
            Term_putstr(1, layout.prompt_row, -1, TERM_L_DARK, prompt_buf);
        } else {
            Term_putstr(0, layout.prompt_row, layout.term_wid, TERM_SLATE,
                "Dir move  r recall  u/Space use  d drop  Esc");
        }

        if (!column)
            Term_gotoxy(0, layout.list_row + (grp_cur - grp_top));
        else if (entry_cnt)
            Term_gotoxy(layout.list_col, layout.list_row + (entry_cur - entry_top));
        else
            Term_gotoxy(0, layout.list_row + (grp_cur - grp_top));

        char ch = inkey();
        if (steamdeck_controls_active() && ch == steamdeck_back_key())
            ch = ESCAPE;

        if ((ch == '\r' || ch == '\n' || (steamdeck_controls_active() && ch == steamdeck_confirm_key())) && column && entry_cnt)
        {
            if (forced_action == SUPPLY_MENU_ACTION_USE)
                ch = 'u';
            else if (forced_action == SUPPLY_MENU_ACTION_DROP)
                ch = 'd';
        }

        switch (ch)
        {
        case ESCAPE:
            flag = true;
            break;

        case 'R':
        case 'r':
        case 'X':
        case 'x':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    object_info_screen(&inventory[entry->item_idx]);
                    redraw = true;
                }
                else if (entry->k_idx >= 0)
                {
                    object_kind* k_ptr = &k_info[entry->k_idx];
                    if (k_ptr->aware)
                    {
                        desc_obj_fake(entry->k_idx);
                        redraw = true;
                    }
                    else
                    {
                        bell("You have not identified that yet.");
                        msg_print("You have not identified that yet.");
                    }
                }
            }
            break;

        case 'u':
        case 'U':
        case ' ':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                bool handled = false;

                if (death_spectator_active())
                {
                    msg_print("You can no longer take that action.");
                    break;
                }

                if (entry->item_idx == SUPPLIES_INDEX && entry->supply_idx >= 0)
                {
                    handled = supplies_menu_use_entry(entry);
                }
                else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    object_type* o_ptr = &inventory[entry->item_idx];

                    switch (o_ptr->tval)
                    {
                    case TV_FOOD:
                        do_cmd_eat_food(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_POTION:
                        do_cmd_quaff_potion(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_STAFF:
                        do_cmd_activate_staff(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_GEM:
                        do_cmd_use_gem(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    default:
                        bell("Cannot use that item here!");
                        break;
                    }

                    if (handled)
                        handle_stuff();
                }
                else
                {
                    bell("You do not have any of that item.");
                    msg_print("You do not have any of that item.");
                }

                if (handled)
                {
                    acted = true;
                    redraw = true;
                    refresh_after_close = true;
                    if (hotkey_mode || forced_action == SUPPLY_MENU_ACTION_USE)
                        flag = true;
                }
            }
            break;

        case 'd':
        case 'D':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                bool dropped = false;

                if (death_spectator_active())
                {
                    msg_print("You can no longer take that action.");
                    break;
                }

                if (entry->item_idx == SUPPLIES_INDEX && entry->supply_idx >= 0)
                {
                    dropped = supplies_menu_drop_entry(entry);
                }
                else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    do_cmd_drop_item_by_index(entry->item_idx);
                    dropped = true;
                }
                else
                {
                    bell("Nothing to drop here.");
                    msg_print("Nothing to drop here.");
                }

                if (dropped)
                {
                    acted = true;
                    redraw = true;
                    handle_stuff();
                    refresh_after_close = true;
                    if (hotkey_mode || forced_action == SUPPLY_MENU_ACTION_DROP)
                        flag = true;
                }
            }
            break;

        default:
            browser_cursor_with_rows(ch, &column, &grp_cur, grp_cnt, &entry_cur,
                entry_cnt, layout.list_rows);
            break;
        }
    }

    mem_free_null(entries);
    screen_load();

    if (refresh_after_close)
    {
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_MESSAGE);
        handle_stuff();
        Term_fresh();
    }

    return acted;
}

void do_cmd_knowledge_objects(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_OBJECTS);
}

/*
 * Display kill counts
 */
void do_cmd_knowledge_kills(void)
{
    int n, i;

    SDL_IOStream* fff;

    char file_name[1024];

    u16b* who;
    //	u16b why = 4;

    /* Temporary file */
    fff = sdl_fopen_temp(file_name, sizeof(file_name));

    /* Failure */
    if (!fff)
        return;

    /* Allocate the "who" array */
    who = mem_alloc_array(z_info->r_max, u16b);

    /* Collect matching monsters */
    for (n = 0, i = 1; i < z_info->r_max - 1; i++)
    {
        // monster_race *r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Require non-unique monsters */
        // if (r_ptr->flags1 & RF1_UNIQUE) continue;

        /* Collect "appropriate" monsters */
        if (l_ptr->pkills > 0)
            who[n++] = i;
    }

    /* Select the sort method */
    // ang_sort_comp = ang_sort_comp_hook;
    // ang_sort_swap = ang_sort_swap_hook;

    /* Sort by kills (and level) */
    // ang_sort(who, &why, n);

    /* Print the monsters (highest kill counts first) */
    for (i = n - 1; i >= 0; i--)
    {
        monster_race* r_ptr = &r_info[who[i]];
        monster_lore* l_ptr = &l_list[who[i]];

        if (r_ptr->flags1 & (RF1_UNIQUE))
        {
            /* Print a message */
            SDL_IOprintf(fff, "         %-40s\n", (r_name + r_ptr->name));
        }
        else
        {
            /* Print a message */
            SDL_IOprintf(
                fff, "  %5d  %-40s\n", l_ptr->pkills, (r_name + r_ptr->name));
        }
    }

    /* Free the "who" array */
    mem_free_null(who);

    /* Close the file */
    sdl_fclose(fff);

    /* Display the file contents */
    show_file(file_name, "Kill counts", 0);

    /* Remove the file */
    fd_kill(file_name);
}

/*
 * Interact with "knowledge"
 */
void do_cmd_knowledge(void)
{
    char ch;

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Clear any active banner before opening knowledge menu */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();

    /* Interact until done */
    while (1)
    {
        /* Clear screen */
        Term_clear();

        /* Ask for a choice */
        prt("Display current knowledge", 2, 0);

        /* Give some choices */
        prt("(1) Display known lore browser", 4, 5);
        prt("(2) Display supplies overview", 5, 5);
        prt("(3) Display names of the fallen", 6, 5);
        prt("(4) Display kill counts", 7, 5);

        /*allow the player to see the notes taken if that option is selected*/
        c_put_str(TERM_WHITE, "(5) Display character notes file", 8, 5);
        prt("(6) Display oath status", 9, 5);

        /* Prompt */
        prt("Command: ", 11, 0);

        /* Prompt */
        ch = inkey();

        /* Done */
        if (ch == ESCAPE)
            break;

        /* Known lore browser */
        if (ch == '1')
        {
            do_cmd_knowledge_browser_page(g_knowledge_last_page);
        }

        /* Scores */
        else if (ch == '2')
        {
            do_cmd_knowledge_supplies(NULL);
        }

        /* Scores */
        else if (ch == '3')
        {
            show_scores_interactive(true);
        }

        /* Kill counts */
        else if (ch == '4')
        {
            do_cmd_knowledge_kills();
        }

        /* Notes file, if one exists */
        else if (ch == '5')
        {
            /* Spawn */
            do_cmd_knowledge_notes();
        }

        /* Oath status */
        else if (ch == '6')
        {
            do_cmd_knowledge_oaths();
        }

        /* Unknown option */
        else
        {
            bell("Illegal command for knowledge!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    screen_load();
}

/*
 * Determines the direction from the player and writes it as text into a buffer
 * of at least size 10.
 */
void write_direction_from_player_to_buffer(
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

static int unified_sidebar_object_group(const object_type* o_ptr)
{
    if (!o_ptr)
        return LOOK_GROUP_OTHER;

    if (artefact_p(o_ptr))
        return LOOK_GROUP_ARTIFACT;

    switch (o_ptr->tval)
    {
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOW:
    case TV_DIGGING:
    case TV_ARROW:
        return LOOK_GROUP_WEAPON;

    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        return LOOK_GROUP_ARMOUR;

    case TV_RING:
    case TV_AMULET:
    case TV_HORN:
    case TV_STAFF:
        return LOOK_GROUP_JEWELRY;

    case TV_EASTER:
        return LOOK_GROUP_HERBS;

    case TV_POTION:
        return LOOK_GROUP_POTIONS;

    case TV_GEM:
        return LOOK_GROUP_GEMS;

    case TV_FOOD:
        if (o_ptr->sval < SV_FOOD_MIN_FOOD)
            return LOOK_GROUP_CONSUMABLE;
        break;
    }

    return LOOK_GROUP_OTHER;
}

typedef struct unified_sidebar_sorted_object {
    int o_idx;
    int y, x;
    object_type* o_ptr;
    bool is_artifact;
    int difficulty;
    int level;
    int group;
    int distance;
    int original_index;
} unified_sidebar_sorted_object;

static bool unified_sidebar_object_should_swap(
    const unified_sidebar_sorted_object* a,
    const unified_sidebar_sorted_object* b)
{
    bool sort_by_difficulty_only = look_objects_sort_by_difficulty ? true : false;
    bool a_known = object_known_p(a->o_ptr) ? true : false;
    bool b_known = object_known_p(b->o_ptr) ? true : false;

    if (!sort_by_difficulty_only && a->group != b->group)
        return (b->group < a->group);

    /* Unidentified items stay at the top of the section/list. */
    if (a_known != b_known)
        return (!b_known && a_known);

    if (!a_known)
    {
        if (b->distance < a->distance)
            return true;
        if ((b->distance == a->distance) && (b->original_index < a->original_index))
            return true;
        return false;
    }

    if (b->difficulty > a->difficulty)
        return true;
    if ((b->difficulty == a->difficulty) && (b->distance < a->distance))
        return true;
    if ((b->difficulty == a->difficulty) && (b->distance == a->distance)
        && (b->original_index < a->original_index))
        return true;

    return false;
}

static int unified_sidebar_collect_sorted_objects(const unified_look_state* state,
    unified_sidebar_sorted_object objects[], int max_objects)
{
    int i;
    int valid_objects = 0;

    if (!state || !objects || (max_objects <= 0))
        return 0;

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    for (i = 0; (i < temp_n) && (valid_objects < max_objects); i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        object_type* o_ptr;
        unified_sidebar_sorted_object* entry;

        if (!o_idx)
            continue;

        if (!grid_info_is_available(temp_y[i], temp_x[i]))
            continue;

        o_ptr = &o_list[o_idx];

        /* Only show marked (memorized) objects that the player has actually seen. */
        if (!o_ptr->marked)
            continue;

        if ((o_ptr->tval == TV_ARROW) && (o_ptr->number < 10))
            continue;

        entry = &objects[valid_objects];
        entry->o_idx = o_idx;
        entry->y = temp_y[i];
        entry->x = temp_x[i];
        entry->o_ptr = o_ptr;
        entry->is_artifact = artefact_p(o_ptr) ? true : false;
        entry->difficulty = object_difficulty(o_ptr);
        entry->level = k_info[o_ptr->k_idx].level;
        entry->group = unified_sidebar_object_group(o_ptr);
        if ((state->object_group_filter >= 0)
            && (entry->group != state->object_group_filter))
            continue;
        entry->distance = distance(p_ptr->py, p_ptr->px, entry->y, entry->x);
        entry->original_index = i;

        valid_objects++;
    }

    for (i = 0; i < valid_objects - 1; i++) {
        for (int j = i + 1; j < valid_objects; j++) {
            if (unified_sidebar_object_should_swap(&objects[i], &objects[j]))
            {
                unified_sidebar_sorted_object temp = objects[i];
                objects[i] = objects[j];
                objects[j] = temp;
            }
        }
    }

    return valid_objects;
}

int unified_look_find_cursor_selection(const unified_look_state* state, int cursor_y,
    int cursor_x)
{
    int i;
    int entity_index = 0;

    if (!state)
        return -1;

    if (state->show_monsters)
    {
        get_sorted_target_list(TARGET_LIST_MONSTER, 0);

        for (i = 0; i < temp_n; i++)
        {
            int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];

            if (!m_idx)
                continue;
            if (!grid_info_is_available(temp_y[i], temp_x[i]))
                continue;
            if (!mon_list[m_idx].ml)
                continue;

            if ((temp_y[i] == cursor_y) && (temp_x[i] == cursor_x))
                return entity_index;

            entity_index++;
        }
    }

    if (state->show_objects)
    {
        int group_display_counts[LOOK_GROUP_COUNT] = {0};
        get_sorted_target_list(TARGET_LIST_OBJECT, 0);
        int object_capacity = (temp_n > 0) ? temp_n : 1;
        unified_sidebar_sorted_object objects[object_capacity];
        int valid_objects = unified_sidebar_collect_sorted_objects(state, objects,
            object_capacity);

        for (i = 0; i < valid_objects; i++)
        {
            unified_sidebar_sorted_object* entry = &objects[i];

            if (state->limit_objects_top_five
                && (group_display_counts[entry->group] >= 5))
                continue;

            group_display_counts[entry->group]++;

            if ((entry->y == cursor_y) && (entry->x == cursor_x))
                return entity_index;

            entity_index++;
        }
    }

    return -1;
}

static void redraw_inven_equip_subwindows(void)
{
    for (int j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        if (!angband_term[j])
            continue;

        /* Don't overwrite the current options/menu term. */
        if (angband_term[j] == old)
            continue;

        u32b flags = op_ptr->window_flag[j];
        if (!(flags & (PW_INVEN | PW_EQUIP)))
            continue;

        Term_activate(angband_term[j]);

        if (flags & PW_INVEN)
            display_inven();
        if (flags & PW_EQUIP)
            display_equip();

        Term_fresh();
        Term_activate(old);
    }
}

static void redraw_monster_subwindows(void)
{
    for (int j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        if (!angband_term[j])
            continue;

        /* Don't overwrite the current options/menu term. */
        if (angband_term[j] == old)
            continue;

        u32b flags = op_ptr->window_flag[j];
        if (!(flags & (PW_MONSTER)))
            continue;

        Term_activate(angband_term[j]);

        if (p_ptr->monster_race_idx)
            display_roff(p_ptr->monster_race_idx, NULL);

        Term_fresh();
        Term_activate(old);
    }
}

static void sidebar_trim_spaces(char* s)
{
    if (!s) return;

    char* start = s;
    while (*start && isspace((unsigned char)*start))
        ++start;

    if (start != s)
        memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

static int sidebar_find_stats_pos(const char* s)
{
    if (!s) return -1;
    
    /* Stats section typically appears after the item name, preceded by a space.
     * Format: "Item Name (dice) [bonus] <pval> {inscription}"
     * We search for the first space-delimited bracket that looks like stats.
     */
    
    int first_stats_pos = -1;
    
    /* Look for the first bracket that follows a space or starts the string */
    for (int i = 0; s[i]; ++i)
    {
        char c = s[i];
        
        /* Found a potential stats delimiter */
        if (c == '(' || c == '[' || c == '<' || c == '{')
        {
            /* Check if this is preceded by a space (or is at start) */
            if (i == 0 || s[i-1] == ' ')
            {
                /* This looks like the start of stats section */
                first_stats_pos = i;
                break;
            }
            /* If preceded by a letter/digit, it might be part of the name */
            /* Keep searching */
        }
    }
    
    /* If we found stats position at start (i==0), that means NO base name!
     * This shouldn't happen with properly formatted object_desc output.
     * If it does, we should treat the whole thing as base name, not stats.
     */
    if (first_stats_pos == 0)
    {
        log_trace("sidebar_find_stats_pos: stats at position 0 for '%s' - treating as name", s);
        return -1;
    }
    
    return first_stats_pos;
}

static void sidebar_compact_name(const char* src, int max_len, char* dest, size_t dest_sz)
{
    if (!dest_sz) return;
    dest[0] = 0;

    if (!src) return;

    int src_len = (int)strlen(src);
    if (max_len < 1)
    {
        log_trace("sidebar_compact_name: max_len < 1 for src='%s'", src);
        return;
    }

    if (src_len <= max_len)
    {
        strnfmt(dest, dest_sz, "%s", src);
        log_trace("sidebar_compact_name: no shortening needed src='%s' len=%d max=%d", src, src_len, max_len);
        return;
    }

    int stats_pos = sidebar_find_stats_pos(src);
    log_trace("sidebar_compact_name: shortening src='%s' len=%d max=%d stats_pos=%d", src, src_len, max_len, stats_pos);

    if (stats_pos < 0)
    {
        strnfmt(dest, dest_sz, "%.*s", max_len, src);
        sidebar_trim_spaces(dest);
        log_trace("sidebar_compact_name: no stats segment, result='%s'", dest);
        return;
    }

    int stats_len = src_len - stats_pos;
    
    /* If stats are very long and would fill the whole space,
     * prioritize showing at least SOME of the base name rather than stats-only.
     */
    if (stats_len >= max_len)
    {
        /* Try to show at least a portion of the base name, even if truncated */
        int base_space = max_len / 2; /* Give half space to name */
        if (base_space < 3) base_space = 3; /* Minimum name chars */
        if (base_space > stats_pos) base_space = stats_pos; /* Don't exceed available name */
        
        int stats_space = max_len - base_space;
        if (stats_space < 3) stats_space = 3; /* Minimum stats chars */
        
        /* Extract truncated base name */
        char base_truncated[64];
        strnfmt(base_truncated, sizeof(base_truncated), "%.*s", base_space, src);
        sidebar_trim_spaces(base_truncated);
        
        /* Extract beginning of stats */
        char stats_truncated[64];
        strnfmt(stats_truncated, sizeof(stats_truncated), "%.*s", stats_space, src + stats_pos);
        
        /* Combine them */
        if (base_truncated[0])
        {
            strnfmt(dest, dest_sz, "%s %s", base_truncated, stats_truncated);
        }
        else
        {
            strnfmt(dest, dest_sz, "%s", stats_truncated);
        }
        sidebar_trim_spaces(dest);
        log_trace("sidebar_compact_name: long stats, showing truncated name+stats result='%s'", dest);
        return;
    }

    int base_space = max_len - stats_len;
    if (base_space < 0) base_space = 0;

    char base_full[128];
    char base_compact[128];
    base_full[0] = 0;
    base_compact[0] = 0;

    if (stats_pos > 0)
    {
        strnfmt(base_full, sizeof(base_full), "%.*s", stats_pos, src);
        sidebar_trim_spaces(base_full);
    }

    if (base_space > 0 && base_full[0])
    {
        int base_full_len = (int)strlen(base_full);
        if (base_full_len <= base_space)
        {
            SDL_strlcpy(base_compact, base_full, sizeof(base_compact));
        }
        else
        {
            const char* word_start[16];
            int word_len[16];
            int word_count = 0;
            const char* p = base_full;

            while (*p && word_count < 16)
            {
                while (*p && isspace((unsigned char)*p))
                    ++p;
                if (!*p)
                    break;

                word_start[word_count] = p;
                const char* q = p;
                while (*q && !isspace((unsigned char)*q))
                    ++q;
                word_len[word_count] = (int)(q - p);
                ++word_count;
                p = q;
            }

            int remaining = base_space;
            bool first_word = true;

            for (int i = 0; i < word_count && remaining > 0; ++i)
            {
                int needed_space = first_word ? 0 : 1;
                if (remaining <= needed_space)
                    break;

                if (!first_word)
                {
                    SDL_strlcat(base_compact, " ", sizeof(base_compact));
                    --remaining;
                }

                int take = word_len[i];
                if (take > remaining)
                {
                    if (first_word)
                    {
                        take = remaining;
                        if (take > 0)
                        {
                            char temp[64];
                            strnfmt(temp, sizeof(temp), "%.*s", take, word_start[i]);
                            SDL_strlcat(base_compact, temp, sizeof(base_compact));
                            remaining -= take;
                        }
                    }
                    else if (remaining > 1)
                    {
                        char temp[64];
                        int partial = remaining;
                        strnfmt(temp, sizeof(temp), "%.*s", partial, word_start[i]);
                        SDL_strlcat(base_compact, temp, sizeof(base_compact));
                        remaining = 0;
                    }
                    else
                    {
                        size_t len = strlen(base_compact);
                        if (len && base_compact[len - 1] == ' ')
                            base_compact[len - 1] = '\0';
                        break;
                    }
                }
                else
                {
                    char temp[64];
                    strnfmt(temp, sizeof(temp), "%.*s", take, word_start[i]);
                    SDL_strlcat(base_compact, temp, sizeof(base_compact));
                    remaining -= take;
                }

                first_word = false;
            }

            sidebar_trim_spaces(base_compact);

            if (!base_compact[0] && base_space > 0)
            {
                int take = (base_space < base_full_len) ? base_space : base_full_len;
                strnfmt(base_compact, sizeof(base_compact), "%.*s", take, base_full);
                sidebar_trim_spaces(base_compact);
            }
        }
    }

    dest[0] = 0;
    if (base_compact[0])
    {
        SDL_strlcpy(dest, base_compact, dest_sz);
        size_t len = strlen(dest);
        if (len && dest[len - 1] != ' ')
            SDL_strlcat(dest, " ", dest_sz);
    }

    SDL_strlcat(dest, src + stats_pos, dest_sz);
    sidebar_trim_spaces(dest);
    log_trace("sidebar_compact_name: combined result='%s'", dest);
}

/*
 * Show unified sidebar with monsters and objects
 */
void show_unified_sidebar(unified_look_state* state)
{
    int sidebar_col = 0; /* Left side of screen - column 0 */
    int line = 1;
    int i;
    int monster_count = 0;
    int object_count = 0;
    char clear_line[256];
    int clear_width;
    char entity_char[2];
    entity_char[1] = '\0';
    static int previous_line_count = 0; /* Track previous display size */
    static int prev_name_len[256];
    const int prev_array_capacity = (int)(sizeof(prev_name_len) / sizeof(prev_name_len[0]));
    bool has_sidebar_selection;

    
    /* Get terminal height and calculate available space */
    int term_hgt = Term->hgt;
    int max_display_line = term_hgt - 2; /* Leave space for bottom line */
    
    /* Calculate layout positions once for both monsters and objects */
    int term_wid = Term->wid;
    int available_width = term_wid - sidebar_col - 3;
    int name_width = available_width - 8 - 3 - 2; /* -2 for spaces */
    
    /* Adjust for bigtile mode - pictogram takes extra space */
    if (use_bigtile) {
        name_width = name_width - 1;  /* Reduce name width by 1 for bigtile */
    }
    
    if (name_width < 4) name_width = 4; /* minimum name width */
    
    /* Calculate exact positions */
    int pictogram_col = sidebar_col;
    int name_col = sidebar_col + 2;  /* Name starts right after pictogram (at column 2) */
    
    /* Prepare clearing string */
    clear_width = Term->wid - (sidebar_col - 1);
    if (clear_width > 255) clear_width = 255;
    memset(clear_line, ' ', clear_width);
    clear_line[clear_width] = '\0';
    
    log_trace("show_unified_sidebar: previous_line_count=%d, term_hgt=%d, max_display_line=%d", 
              previous_line_count, term_hgt, max_display_line);
    log_trace("show_unified_sidebar: sidebar_col=%d, Term->wid=%d, clear_start=%d, clear_width=%d", 
              sidebar_col, Term->wid, sidebar_col - 1, clear_width);
    log_trace("show_unified_sidebar: show_monsters=%d, show_objects=%d", 
              state->show_monsters ? 1 : 0, state->show_objects ? 1 : 0);

    if ((state->look_mode == 0) && !state->in_sidebar_mode
        && (state->selected_entity < 0)
        && ((state->cursor_y != p_ptr->py) || (state->cursor_x != p_ptr->px)))
    {
        if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
        {
            highlight_entity_on_map(state->highlighted_y, state->highlighted_x, false);
        }

        state->highlighted_y = -1;
        state->highlighted_x = -1;
        state->highlighted_entity_type = 0;
        previous_line_count = 0;
        memset(prev_name_len, 0, sizeof(prev_name_len));
        return;
    }

    has_sidebar_selection = (state->selected_entity >= 0)
        && (state->in_sidebar_mode || (state->look_mode == 0));
    
    /* Don't clear anything - let screen_save/screen_load handle restoration */
    log_trace("show_unified_sidebar: skipping clear - letting screen management handle it");
    
    /* Show monsters section */
    if (state->show_monsters)
    {
        log_trace("show_unified_sidebar: displaying MONSTERS header at line %d", line);
        c_put_str(TERM_WHITE, "MONSTERS:    ", line++, sidebar_col);
        
        /* Get monster list */
        get_sorted_target_list(TARGET_LIST_MONSTER, 0);
        
        for (i = 0; i < temp_n && line < max_display_line; i++)
        {
            int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];
            monster_type* m_ptr = &mon_list[m_idx];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];
            char m_name[40];
            char morale_text[8];
            
            /* Show only visible monsters on screen (like the [ monsters menu) */
            /* Skip empty monster slots */
            if (!m_idx) continue;

            if (!grid_info_is_available(temp_y[i], temp_x[i])) continue;

            /* Skip monsters that are not visible to the player */
            if (!m_ptr->ml) continue;
            
            /* Generate monster name without articles using race name function */
            monster_desc_race(m_name, sizeof(m_name), m_ptr->r_idx);
            
            /* Create HP bar with asterisks */
            int hp_len = 0;
            if (m_ptr->maxhp > 0) {
                hp_len = (8 * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;
            }
            char hp_bar[10];
            
            /* Build health bar with status indicators */
            if (m_ptr->confused && m_ptr->stunned)
            {
                strncpy(hp_bar, "cscscscs", hp_len);
            }
            else if (m_ptr->confused)
            {
                strncpy(hp_bar, "cccccccc", hp_len);
            }
            else if (m_ptr->stunned)
            {
                strncpy(hp_bar, "ssssssss", hp_len);
            }
            else
            {
                strncpy(hp_bar, "********", hp_len);
            }
            hp_bar[hp_len] = '\0';
            
            /* Create morale number with proper color */
            int morale_color = TERM_WHITE;
            int morale_num = 0;
            
            if (m_ptr->alertness < ALERTNESS_UNWARY)
            {
                morale_color = TERM_BLUE;
                morale_num = m_ptr->alertness;
            }
            else if (m_ptr->alertness < ALERTNESS_ALERT)
            {
                morale_color = TERM_L_BLUE;
                morale_num = m_ptr->alertness;
            }
            else
            {
                /* Get proper morale display using alertness function */
                char dummy_text[20];
                if (!get_alertness_text(m_ptr, sizeof(dummy_text), dummy_text, &morale_color))
                {
                    /* Fallback if stance not initialized - use white and calculate from morale */
                    morale_color = TERM_WHITE;
                }
                
                /* Calculate morale number */
                if (m_ptr->morale >= 0)
                    morale_num = (m_ptr->morale + 9) / 10;
                else
                    morale_num = m_ptr->morale / 10;
            }
            
            strnfmt(morale_text, sizeof(morale_text), "%d", morale_num);
            
            /* Use pictogram (tile) appropriate for graphics mode */
            entity_char[0] = monster_char(r_ptr);
            
            /* Build the complete display string: name + health + morale */
            char display_name[128];
            char hp_display[12];
            char morale_display[12];
            
            /* Format health and morale as compact strings */
            strnfmt(hp_display, sizeof(hp_display), " %s", hp_bar);
            strnfmt(morale_display, sizeof(morale_display), " %s", morale_text);
            
            /* Calculate available width for the whole line */
            int available_width = term_wid - name_col - 2;
            if (available_width < 10) available_width = 10;
            
            int hp_display_len = (int)strlen(hp_display);
            int morale_display_len = (int)strlen(morale_display);
            int max_name_len = available_width - hp_display_len - morale_display_len;
            if (max_name_len < 4) max_name_len = 4;
            if (max_name_len > (int)sizeof(display_name) - hp_display_len - morale_display_len - 1)
                max_name_len = (int)sizeof(display_name) - hp_display_len - morale_display_len - 1;
            
            /* Truncate monster name if needed */
            char truncated_name[80];
            memset(truncated_name, 0, sizeof(truncated_name));
            SDL_strlcpy(truncated_name, m_name, sizeof(truncated_name));
            if (strlen(truncated_name) > (size_t)max_name_len) {
                truncated_name[max_name_len] = '\0';
            }
            
            /* Build complete display string: name + health (without morale) */
            SDL_strlcpy(display_name, truncated_name, sizeof(display_name));
            SDL_strlcat(display_name, hp_display, sizeof(display_name));
            
            int name_hp_len = (int)strlen(display_name);
            int total_span = name_hp_len + morale_display_len;
            if (use_bigtile)
            {
                const int min_sidebar_span = 13;
                if (total_span < min_sidebar_span)
                {
                    int pad_needed = min_sidebar_span - total_span;

                    while (pad_needed > 0 && name_hp_len + 1 < (int)sizeof(display_name))
                    {
                        display_name[name_hp_len++] = ' ';
                        pad_needed--;
                    }
                    display_name[name_hp_len] = '\0';
                    total_span = name_hp_len + morale_display_len;

                    while (pad_needed > 0 && morale_display_len + 1 < (int)sizeof(morale_display))
                    {
                        morale_display[morale_display_len++] = ' ';
                        pad_needed--;
                    }
                    morale_display[morale_display_len] = '\0';
                    total_span = name_hp_len + morale_display_len;
                }

                if ((total_span % 2) == 0)
                {
                    if (morale_display_len + 1 < (int)sizeof(morale_display))
                    {
                        morale_display[morale_display_len++] = ' ';
                        morale_display[morale_display_len] = '\0';
                    }
                    else if (name_hp_len + 1 < (int)sizeof(display_name))
                    {
                        display_name[name_hp_len++] = ' ';
                        display_name[name_hp_len] = '\0';
                    }
                    total_span = name_hp_len + morale_display_len;
                }
            }
            
            /* Calculate column for morale display */
            int morale_col = name_col + name_hp_len;
            
            /* Highlight if selected with cursor-style highlighting only */
            bool highlight_this_monster = (has_sidebar_selection
                && (state->selected_entity == monster_count));
            
            if (highlight_this_monster)
            {
                log_trace("Highlighting monster %d at (%d,%d)", monster_count, temp_y[i], temp_x[i]);
                
                /* Clear only the exact area where text will be displayed */
                Term_erase(pictogram_col, line, 2);  /* Clear pictogram area (1-2 chars) */
                
                /* Show pictogram in natural color */
                c_put_str(monster_attr(r_ptr), entity_char, line, pictogram_col);
                if (use_bigtile)
                {
                    Term_putch(pictogram_col + 1, line, 255, -1);
                }
                
                /* Display name+health in highlighted color */
                Term_putstr(name_col, line, name_hp_len, TERM_L_BLUE, display_name);
                
                /* Display morale in highlighted color (overrides morale_color when highlighted) */
                Term_putstr(morale_col, line, morale_display_len, TERM_L_BLUE, morale_display);
                
                /* Update highlighted position and cursor */
                state->highlighted_y = temp_y[i];
                state->highlighted_x = temp_x[i];
                state->highlighted_entity_type = 1; /* Monster */
                state->cursor_y = temp_y[i];
                state->cursor_x = temp_x[i];
                highlight_entity_on_map_type(temp_y[i], temp_x[i], true, 1); /* Prefer monster display */
            }
            else
            {
                /* Clear only the exact area where text will be displayed */
                Term_erase(pictogram_col, line, 2);  /* Clear pictogram area (1-2 chars) */
                
                /* Normal display - show pictogram and name with proper colors */
                c_put_str(monster_attr(r_ptr), entity_char, line, pictogram_col);
                if (use_bigtile)
                {
                    Term_putch(pictogram_col + 1, line, 255, -1);
                }
                
                /* Display name+health in white */
                Term_putstr(name_col, line, name_hp_len, TERM_WHITE, display_name);
                
                /* Display morale in its proper color */
                Term_putstr(morale_col, line, morale_display_len, morale_color, morale_display);
            }
            
            line++;
            monster_count++;
        }
    }
    
    /* Show objects section */
    if (state->show_objects)
    {
        const char* filter_tag = "ALL";
        switch (state->object_group_filter)
        {
        case LOOK_GROUP_ARTIFACT:   filter_tag = "ART"; break;
        case LOOK_GROUP_WEAPON:     filter_tag = "WEAP"; break;
        case LOOK_GROUP_ARMOUR:     filter_tag = "ARM"; break;
        case LOOK_GROUP_JEWELRY:    filter_tag = "JEWL"; break;
        case LOOK_GROUP_HERBS:      filter_tag = "HERB"; break;
        case LOOK_GROUP_POTIONS:    filter_tag = "POT"; break;
        case LOOK_GROUP_GEMS:       filter_tag = "GEM"; break;
        case LOOK_GROUP_CONSUMABLE: filter_tag = "CONS"; break;
        case LOOK_GROUP_OTHER:      filter_tag = "OTHER"; break;
        default:                    filter_tag = "ALL"; break;
        }

        char header_buf[32];
        strnfmt(header_buf, sizeof(header_buf), "OBJECTS: %s", filter_tag);
        c_put_str(TERM_WHITE, header_buf, line++, sidebar_col);
        
        get_sorted_target_list(TARGET_LIST_OBJECT, 0);
        int object_capacity = (temp_n > 0) ? temp_n : 1;
        unified_sidebar_sorted_object objects[object_capacity];
        int valid_objects = unified_sidebar_collect_sorted_objects(state, objects,
            object_capacity);

        int group_display_counts[LOOK_GROUP_COUNT] = {0};
        int object_start = (state->show_monsters) ? monster_count : 0;
        for (i = 0; i < valid_objects && line < max_display_line; i++)
        {
            unified_sidebar_sorted_object* entry = &objects[i];
            object_type* o_ptr = entry->o_ptr;
            char o_name[60];
            char name_source[80];
            byte base_color = TERM_WHITE;

            if (state->limit_objects_top_five && group_display_counts[entry->group] >= 5)
                continue;

            group_display_counts[entry->group]++;

            /* Generate object name with stats but without articles (mode 4) 
             * Mode 4 applies shortening logic that sidebar_compact_name expects.
             * Fixed mode 4 to never produce stats-only output.
             */
            object_desc_floor(o_name, sizeof(o_name), o_ptr, false, 4);

            SDL_strlcpy(name_source, o_name, sizeof(name_source));
            /* Only show asterisk for artifacts that are identified */
            if (entry->is_artifact && object_known_p(o_ptr))
            {
                size_t len = strlen(name_source);
                if (len + 1 < sizeof(name_source))
                {
                    memmove(name_source + 1, name_source, len + 1);
                    name_source[0] = '*';
                }
            }

            base_color = weapon_glows(o_ptr) 
                ? object_display_color(o_ptr, TERM_L_BLUE) 
                : object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

            entity_char[0] = object_char(o_ptr);

            int weight_total = o_ptr->weight * o_ptr->number;
            char weight_buf[16];
            strnfmt(weight_buf, sizeof(weight_buf), " %d.%1d", weight_total / 10, weight_total % 10);

            char smith_buf[16];
            smith_buf[0] = '\0';
            if (op_ptr->opt[OPT_show_smithing_difficulty_look]
                && object_known_p(o_ptr)
                && object_uses_smithing_difficulty(o_ptr))
            {
                int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
                int sd = object_smithing_difficulty(o_ptr);
                int wr = object_weight_rarity(o_ptr, depth);
                strnfmt(smith_buf, sizeof(smith_buf), " {%d,%d}", sd, wr);
            }

            /* Calculate available width for name + weight (+ optional smithing debug) */
            int available_name_width = term_wid - name_col - 2; /* Leave some margin */
            if (available_name_width < 10) available_name_width = 10;
            
            int weight_len = (int)strlen(weight_buf);
            int smith_len = (int)strlen(smith_buf);
            int max_name_len = available_name_width - weight_len - smith_len - 1; /* Reserve space for suffixes */
            if (max_name_len < 4) max_name_len = 4;

            char display_name[128];
            if (max_name_len > (int)sizeof(display_name) - weight_len - 1) 
                max_name_len = (int)sizeof(display_name) - weight_len - 1;

            sidebar_compact_name(name_source, max_name_len, display_name, sizeof(display_name));
            
            /* Append weight right after name */
            SDL_strlcat(display_name, weight_buf, sizeof(display_name));

            /* Append optional smithing debug right after weight */
            if (smith_buf[0])
                SDL_strlcat(display_name, smith_buf, sizeof(display_name));
            int final_name_len = (int)strlen(display_name);
            int original_name_len = (int)strlen(name_source);
            bool shortened = (original_name_len != final_name_len) || (original_name_len > max_name_len);
            log_trace("sidebar object: idx=%d name='%s' compact='%s' color=%d orig_len=%d compact_len=%d max_len=%d name_col=%d weight_len=%d shortened=%d",
                entry->o_idx, name_source, display_name, base_color, original_name_len, final_name_len, max_name_len, name_col, weight_len, shortened ? 1 : 0);

            if (use_bigtile)
            {
                const int min_sidebar_span = 13;
                if (final_name_len < min_sidebar_span)
                {
                    int pad_needed = min_sidebar_span - final_name_len;
                    while (pad_needed > 0 && final_name_len + 1 < (int)sizeof(display_name))
                    {
                        display_name[final_name_len++] = ' ';
                        pad_needed--;
                    }
                    display_name[final_name_len] = '\0';
                }

                if ((final_name_len % 2) == 0 && (final_name_len + 1 < (int)sizeof(display_name)))
                {
                    display_name[final_name_len++] = ' ';
                    display_name[final_name_len] = '\0';
                }
            }

            int row_index = line;
            if (row_index < 0) row_index = 0;
            if (row_index >= prev_array_capacity) row_index = prev_array_capacity - 1;

            int old_name_len = prev_name_len[row_index];
            if (old_name_len > final_name_len)
            {
                int diff = old_name_len - final_name_len;
                if (diff > 0)
                {
                    char blank[128];
                    if (diff >= (int)sizeof(blank)) diff = (int)sizeof(blank) - 1;
                    memset(blank, ' ', diff);
                    blank[diff] = '\0';
                    Term_putstr(name_col + final_name_len, line, diff, TERM_WHITE, blank);
                }
            }

            bool highlight_this_object = (has_sidebar_selection
                && (state->selected_entity == (object_start + object_count)));

            byte name_attr = highlight_this_object ? TERM_L_BLUE : base_color;

            if (highlight_this_object)
            {
                log_trace("Highlighting object %d at (%d,%d)", object_start + object_count, entry->y, entry->x);

                Term_erase(pictogram_col, line, 2);
                
                c_put_str(object_attr(o_ptr), entity_char, line, pictogram_col);
                if (use_bigtile)
                {
                    Term_putch(pictogram_col + 1, line, 255, -1);
                }
                
                Term_putstr(name_col, line, final_name_len, name_attr, display_name);

                state->highlighted_y = entry->y;
                state->highlighted_x = entry->x;
                state->highlighted_entity_type = 2; /* Object */
                state->cursor_y = entry->y;
                state->cursor_x = entry->x;
                highlight_entity_on_map_type(entry->y, entry->x, true, 2); /* Prefer object display */
            }
            else
            {
                Term_erase(pictogram_col, line, 2);
                
                c_put_str(object_attr(o_ptr), entity_char, line, pictogram_col);
                if (use_bigtile)
                {
                    Term_putch(pictogram_col + 1, line, 255, -1);
                }
                
                Term_putstr(name_col, line, final_name_len, name_attr, display_name);
            }

            prev_name_len[row_index] = final_name_len;

            line++;
            object_count++;
        for (int idx = line; idx < prev_array_capacity && idx <= previous_line_count; ++idx)
        {
            prev_name_len[idx] = 0;
        }

        }
    }

    /* Save current line count for next clearing operation */
    int current_line_count = line - 1;
    
    log_trace("show_unified_sidebar: current_line_count=%d, previous_line_count=%d", 
              current_line_count, previous_line_count);
    
    /* If the new display is shorter than the previous one, don't clear - let screen_load handle it */
    if (previous_line_count > current_line_count)
    {
        log_trace("show_unified_sidebar: display got shorter (%d->%d) but not clearing - screen_load will restore", 
                  previous_line_count, current_line_count);
    }
    
    previous_line_count = current_line_count;
    log_trace("show_unified_sidebar: function complete, set previous_line_count=%d", previous_line_count);
}

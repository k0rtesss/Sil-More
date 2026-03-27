/* File: init2.c */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "runtime-cli.h"
#include "platform-config.h"
#include "platform-time.h"
#include "platform-story-font.h"
#include "metarun.h"
#include "item_set.h"
#include "init/init2-internal.h"

void autoinscribe_clean(void)
{
    if (inscriptions)
    {
        mem_free_null(inscriptions);
    }

    inscriptions = 0;
    inscriptionsCount = 0;
}

void autoinscribe_init(void)
{
    autoinscribe_clean();
    inscriptions = mem_alloc_array(AUTOINSCRIPTIONS_MAX, autoinscription);
}

/*
 * Reinitialize some things between games
 *
 * Needed because rerunning the whole of init_angband() causes crashes.
 */
void re_init_some_things(void)
{
    int i;

    run_mode_reset();

    memset(p_ptr, 0, sizeof(player_type));

    rp_ptr = &p_info[0];
    current_character_profile = &c_info[0];

    reset_dungeon_state();
    reset_hint_skeleton_state();

    savefile[0] = '\0';
    playerturn = 0;
    min_depth_counter = 0;
    op_ptr->full_name[0] = '\0';

    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        term* old = Term;

        if (!angband_term[i])
            continue;

        Term_activate(angband_term[i]);
        Term_clear();
        Term_fresh();
        Term_activate(old);
    }

    autoinscribe_clean();
    autoinscribe_init();

    sdl_story_font_enable();
    display_introduction();
    sdl_story_font_reset();

    mem_free_null(view_g);
    view_g = mem_alloc_array(VIEW_MAX, u16b);

    mem_free_null(temp_g);
    temp_g = mem_alloc_array(TEMP_MAX, u16b);

    mem_free_null(temp_y);
    mem_free_null(temp_x);
    temp_y = mem_alloc_array(TEMP_MAX, byte);
    temp_x = mem_alloc_array(TEMP_MAX, byte);

    mem_free_null(cave_info);
    cave_info = mem_alloc_array(MAX_DUNGEON_HGT, u16b_256);

    mem_free_null(cave_feat);
    cave_feat = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    mem_free_null(cave_color);
    cave_color = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    mem_free_null(cave_light);
    cave_light = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);

    mem_free_null(cave_o_idx);
    mem_free_null(cave_m_idx);
    cave_o_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);
    cave_m_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);

    mem_free_null(cave_when);
    cave_when = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    (void)vinfo_init();

    mem_free_null(o_list);
    o_list = mem_alloc_array(z_info->o_max, object_type);

    mem_free_null(mon_list);
    mon_list = mem_alloc_array(MAX_MONSTERS, monster_type);

    mem_free_null(l_list);
    l_list = mem_alloc_array(z_info->r_max, monster_lore);

    mem_free_null(inventory);
    inventory = mem_alloc_array(INVEN_TOTAL, object_type);

    for (i = 0; i < OPT_MAX; i++)
    {
        op_ptr->opt[i] = option_norm[i];
    }

    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        op_ptr->window_flag[i] = 0L;
    }

    op_ptr->window_flag[WINDOW_INVEN] |= (PW_INVEN);
    op_ptr->window_flag[WINDOW_EQUIP] |= (PW_EQUIP);
    op_ptr->window_flag[WINDOW_COMBAT_ROLLS] |= (PW_COMBAT_ROLLS);
    op_ptr->window_flag[WINDOW_MONSTER] |= (PW_MONSTER);
    op_ptr->window_flag[WINDOW_PLAYER_0] |= (PW_PLAYER_0);
    op_ptr->window_flag[WINDOW_MESSAGE] |= (PW_MESSAGE);
    op_ptr->window_flag[WINDOW_MONLIST] |= (PW_MONLIST);

    platform_load_app_options();

    if (init_k_info())
        quit("Cannot initialize objects");
    if (init_flavor_info())
        quit("Cannot initialize flavors");
    if (init_effect_info())
        quit("Cannot initialize effects");
    if (init_skeleton_note_info())
        quit("Cannot initialize skeleton notes");
    if (init_e_info())
        quit("Cannot initialize special items");
    if (init_oath_info())
        quit("Cannot initialize oaths");
}

static errr init_other(void)
{
    int i;

    (void)macro_init();
    (void)quarks_init();
    (void)autoinscribe_init();
    (void)messages_init();

    view_g = mem_alloc_array(VIEW_MAX, u16b);
    temp_g = mem_alloc_array(TEMP_MAX, u16b);
    temp_y = mem_alloc_array(TEMP_MAX, byte);
    temp_x = mem_alloc_array(TEMP_MAX, byte);

    cave_info = mem_alloc_array(MAX_DUNGEON_HGT, u16b_256);
    cave_feat = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);
    cave_color = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);
    cave_light = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);
    cave_o_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);
    cave_m_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);
    cave_when = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    (void)vinfo_init();

    o_list = mem_alloc_array(z_info->o_max, object_type);
    mon_list = mem_alloc_array(MAX_MONSTERS, monster_type);
    l_list = mem_alloc_array(z_info->r_max, monster_lore);
    inventory = mem_alloc_array(INVEN_TOTAL, object_type);

    for (i = 0; i < OPT_MAX; i++)
    {
        op_ptr->opt[i] = option_norm[i];
    }

    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        op_ptr->window_flag[i] = 0L;
    }

    op_ptr->window_flag[WINDOW_INVEN] |= (PW_INVEN);
    op_ptr->window_flag[WINDOW_EQUIP] |= (PW_EQUIP);
    op_ptr->window_flag[WINDOW_COMBAT_ROLLS] |= (PW_COMBAT_ROLLS);
    op_ptr->window_flag[WINDOW_MONSTER] |= (PW_MONSTER);
    op_ptr->window_flag[WINDOW_PLAYER_0] |= (PW_PLAYER_0);
    op_ptr->window_flag[WINDOW_MESSAGE] |= (PW_MESSAGE);
    op_ptr->window_flag[WINDOW_MONLIST] |= (PW_MONLIST);

    platform_load_app_options();
    (void)format("%s", MAINTAINER);

    return (0);
}

static errr init_alloc(void)
{
    int i, j;
    object_kind* k_ptr;
    monster_race* r_ptr;
    ego_item_type* e_ptr;
    alloc_entry* table;
    s16b num[MAX_DEPTH];
    s16b aux[MAX_DEPTH];

    memset(aux, 0, sizeof(s16b) * MAX_DEPTH);
    memset(num, 0, sizeof(s16b) * MAX_DEPTH);

    alloc_kind_size = 0;

    for (i = 1; i < z_info->k_max; i++)
    {
        k_ptr = &k_info[i];

        for (j = 0; j < 4; j++)
        {
            if (k_ptr->chance[j])
            {
                alloc_kind_size++;
                num[k_ptr->locale[j]]++;
            }
        }
    }

    for (i = 1; i < MAX_DEPTH; i++)
    {
        num[i] += num[i - 1];
    }

    alloc_kind_table = mem_alloc_array(alloc_kind_size, alloc_entry);
    table = alloc_kind_table;

    for (i = 1; i < z_info->k_max; i++)
    {
        k_ptr = &k_info[i];

        for (j = 0; j < 4; j++)
        {
            if (k_ptr->chance[j])
            {
                int p, x, y, z;

                x = k_ptr->locale[j];
                p = k_ptr->chance[j];
                y = (x > 0) ? num[x - 1] : 0;
                z = y + aux[x];

                table[z].index = i;
                table[z].level = x;
                table[z].prob1 = p;
                table[z].prob2 = p;
                table[z].prob3 = p;

                aux[x]++;
            }
        }
    }

    memset(aux, 0, sizeof(s16b) * MAX_DEPTH);
    memset(num, 0, sizeof(s16b) * MAX_DEPTH);

    alloc_race_size = 0;

    for (i = 1; i < z_info->r_max; i++)
    {
        r_ptr = &r_info[i];

        if (r_ptr->rarity)
        {
            alloc_race_size++;
            num[r_ptr->level]++;
        }
    }

    for (i = 1; i < MAX_DEPTH; i++)
    {
        num[i] += num[i - 1];
    }

    alloc_race_table = mem_alloc_array(alloc_race_size, alloc_entry);
    table = alloc_race_table;

    for (i = 1; i < z_info->r_max; i++)
    {
        r_ptr = &r_info[i];

        if (r_ptr->rarity)
        {
            int p, x, y, z;

            x = r_ptr->level;
            p = r_ptr->rarity;
            y = (x > 0) ? num[x - 1] : 0;
            z = y + aux[x];

            table[z].index = i;
            table[z].level = x;
            table[z].prob1 = p;
            table[z].prob2 = p;
            table[z].prob3 = p;

            aux[x]++;
        }
    }

    memset(aux, 0, sizeof(s16b) * MAX_DEPTH);
    memset(num, 0, sizeof(s16b) * MAX_DEPTH);

    alloc_ego_size = 0;

    for (i = 1; i < z_info->e_max; i++)
    {
        e_ptr = &e_info[i];

        if (e_ptr->rarity)
        {
            alloc_ego_size++;
            num[e_ptr->level]++;
        }
    }

    for (i = 1; i < MAX_DEPTH; i++)
    {
        num[i] += num[i - 1];
    }

    alloc_ego_table = mem_alloc_array(alloc_ego_size, alloc_entry);
    table = alloc_ego_table;

    for (i = 1; i < z_info->e_max; i++)
    {
        e_ptr = &e_info[i];

        if (e_ptr->rarity)
        {
            int p, x, y, z;

            x = e_ptr->level;
            p = e_ptr->rarity;
            y = (x > 0) ? num[x - 1] : 0;
            z = y + aux[x];

            table[z].index = i;
            table[z].level = x;
            table[z].prob1 = p;
            table[z].prob2 = p;
            table[z].prob3 = p;

            aux[x]++;
        }
    }

    return (0);
}

static void note(cptr str)
{
    int term_wid = 80;
    int term_hgt = 24;
    int col;
    int row;

    Term_get_size(&term_wid, &term_hgt);
    row = term_hgt - 1;
    col = MAX(0, (term_wid - (int)strlen(str)) / 2);

    Term_erase(0, row, 255);
    Term_putstr(col, row, term_wid - col, TERM_SLATE, str);
    Term_fresh();
    (void)Term_xtra(TERM_XTRA_EVENT, 0);
}

static void init_angband_aux(cptr why)
{
    quit(format("%s\n\n%s", why,
        "The 'lib' directory is probably missing or broken.\n"
        "Perhaps the archive was not extracted correctly.\n"
        "See the manual for more information."));
}

typedef struct welcome_intro_layout {
    int top_pad;
    bool drop_gap_1;
    bool drop_gap_2;
    bool drop_gap_3;
} welcome_intro_layout;

static void display_introduction_with_layout(
    const welcome_intro_layout* layout);
static int welcome_screen_base_col(void);
static int welcome_screen_intro_row(int rel_row,
    const welcome_intro_layout* layout);
static int welcome_screen_intro_last_row(const welcome_intro_layout* layout);
static int welcome_screen_intro_total_rows(const welcome_intro_layout* layout);
static int welcome_screen_footer_rows(bool show_wizard, bool show_sep,
    bool show_blank, bool show_prompt);
static void welcome_screen_compute_layout(int hgt, bool show_wizard,
    welcome_intro_layout* out_layout, bool* out_show_sep,
    bool* out_show_blank, bool* out_show_prompt);

void display_introduction(void)
{
    welcome_intro_layout layout = { 1, false, false, false };
    display_introduction_with_layout(&layout);
}

static int welcome_screen_base_col(void)
{
    int wid = 80;
    int hgt = 24;
    const int legacy_term_wid = 80;
    const int legacy_base_col = 14;
    const int compact_block_wid = 43;
    int shift;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = legacy_term_wid;

    if (wid < legacy_term_wid)
    {
        shift = (wid - compact_block_wid) / 2;
        if (shift < 0)
            shift = 0;

        return shift;
    }

    shift = (wid - legacy_term_wid) / 2;
    if (legacy_base_col + shift < 0)
        shift = -legacy_base_col;

    return legacy_base_col + shift;
}

static int welcome_screen_intro_row(int rel_row,
    const welcome_intro_layout* layout)
{
    int row = rel_row;

    if (!layout)
        return row;

    if (layout->drop_gap_1 && row >= 6)
        row--;
    if (layout->drop_gap_2 && row >= 9)
        row--;
    if (layout->drop_gap_3 && row >= 14)
        row--;

    return row;
}

static int welcome_screen_intro_last_row(const welcome_intro_layout* layout)
{
    return welcome_screen_intro_row(16, layout);
}

static int welcome_screen_intro_total_rows(const welcome_intro_layout* layout)
{
    int top_pad = 1;

    if (layout)
        top_pad = layout->top_pad;
    if (top_pad < 0)
        top_pad = 0;

    return top_pad + welcome_screen_intro_last_row(layout);
}

static int welcome_screen_footer_rows(bool show_wizard, bool show_sep,
    bool show_blank, bool show_prompt)
{
    int rows = 0;

    if (show_prompt)
        rows++;
    if (show_blank && show_prompt)
        rows++;
    if (show_sep)
        rows++;
    if (show_wizard)
        rows++;

    return rows;
}

static void welcome_screen_compute_layout(int hgt, bool show_wizard,
    welcome_intro_layout* out_layout, bool* out_show_sep,
    bool* out_show_blank, bool* out_show_prompt)
{
    welcome_intro_layout layout = { 1, false, false, false };
    bool show_sep = true;
    bool show_blank = true;
    bool show_prompt = true;

    if (hgt < 1)
        hgt = 24;

#define FITS_NOW() \
    (welcome_screen_intro_total_rows(&layout) \
        + welcome_screen_footer_rows(show_wizard, show_sep, show_blank, show_prompt) \
        <= hgt)

    if (!FITS_NOW())
        layout.top_pad = 0;

    if (!FITS_NOW())
        show_blank = false;

    if (!FITS_NOW())
        show_sep = false;

    if (!FITS_NOW())
        layout.drop_gap_3 = true;
    if (!FITS_NOW())
        layout.drop_gap_2 = true;
    if (!FITS_NOW())
        layout.drop_gap_1 = true;

    if (!FITS_NOW())
        show_prompt = false;

    if (!show_prompt)
        show_blank = false;

#undef FITS_NOW

    if (out_layout)
        *out_layout = layout;
    if (out_show_sep)
        *out_show_sep = show_sep;
    if (out_show_blank)
        *out_show_blank = show_blank;
    if (out_show_prompt)
        *out_show_prompt = show_prompt;
}

static void display_introduction_with_layout(
    const welcome_intro_layout* layout)
{
    int term_wid = 80;
    int term_hgt = 24;
    int top_pad = 1;

    if (layout)
        top_pad = layout->top_pad;
    if (top_pad < 0)
        top_pad = 0;

    const int y = top_pad;
    const int intro_col = welcome_screen_base_col();
    const int subtitle_col = intro_col + 6;
    const int title_col = intro_col + 8;
    const int quote_attr_col = intro_col + 20;
    const int song_attr_col = intro_col + 14;

    Term_get_size(&term_wid, &term_hgt);
    if (term_hgt < 1)
        term_hgt = 24;

#define INTRO_ROW(_rel) (y + welcome_screen_intro_row((_rel), layout) - 1)

    Term_clear();

    bool saved_cursor_state = false;
    (void)Term_get_cursor(&saved_cursor_state);
    (void)Term_set_cursor(false);

    int intro_style;
    if (sdl_config_should_force_intro_flame())
        intro_style = INTRO_STYLE_FLAME;
    else if (op_ptr->intro_style == INTRO_STYLE_RANDOM)
        intro_style = (int)(platform_monotonic_ms() % 5u);
    else
        intro_style = (int)op_ptr->intro_style;

    switch (intro_style)
    {
    case 0:
    default:
        Term_putstr(intro_col, INTRO_ROW(1), -1, TERM_L_BLUE,
            "\"In the beginning Eru, the One,");
        Term_putstr(intro_col, INTRO_ROW(2), -1, TERM_L_BLUE,
            "  made the Ainur of his thought;");
        Term_putstr(intro_col, INTRO_ROW(3), -1, TERM_L_BLUE,
            "  and they sang, and he was glad.\"");
        Term_putstr(quote_attr_col, INTRO_ROW(4), -1, TERM_SLATE,
            "-- Ainulindale");

        Term_putstr(title_col, INTRO_ROW(6), -1, TERM_WHITE,
            "S I L - M O R E");
        Term_putstr(subtitle_col, INTRO_ROW(7), -1, TERM_L_BLUE,
            "~ Shining  Darkness ~");

        Term_putstr(intro_col, INTRO_ROW(9), -1, TERM_WHITE,
            "In the deeps of Angband, beyond");
        Term_putstr(intro_col, INTRO_ROW(10), -1, TERM_WHITE,
            "gates of iron and pits of flame,");
        Term_putstr(intro_col, INTRO_ROW(11), -1, TERM_WHITE,
            "Morgoth hoards the Silmarils --");
        Term_putstr(intro_col, INTRO_ROW(12), -1, TERM_WHITE,
            "three jewels of living light.");

        Term_putstr(intro_col, INTRO_ROW(14), -1, TERM_YELLOW,
            "Take up blade and burden. Descend.");
        Term_putstr(intro_col, INTRO_ROW(15), -1, TERM_YELLOW,
            "Oaths, quests, blessings of the Valar");
        Term_putstr(intro_col, INTRO_ROW(16), -1, TERM_YELLOW,
            "await in the First Age reborn.");
        break;

    case 1:
        Term_putstr(intro_col, INTRO_ROW(1), -1, TERM_L_BLUE,
            "\"Be he foe or friend,");
        Term_putstr(intro_col, INTRO_ROW(2), -1, TERM_L_BLUE,
            "  be he foul or clean...");
        Term_putstr(intro_col, INTRO_ROW(3), -1, TERM_L_BLUE,
            "  he shall defend, shall be held mine.\"");
        Term_putstr(quote_attr_col, INTRO_ROW(4), -1, TERM_SLATE,
            "-- Oath of Feanor");

        Term_putstr(title_col, INTRO_ROW(6), -1, TERM_WHITE,
            "S I L - M O R E");
        Term_putstr(subtitle_col, INTRO_ROW(7), -1, TERM_L_BLUE,
            "~ Shining  Darkness ~");

        Term_putstr(intro_col, INTRO_ROW(9), -1, TERM_WHITE,
            "In the pits beneath the mountains");
        Term_putstr(intro_col, INTRO_ROW(10), -1, TERM_WHITE,
            "Morgoth broods upon his throne.");
        Term_putstr(intro_col, INTRO_ROW(11), -1, TERM_WHITE,
            "Three jewels burn upon his crown --");
        Term_putstr(intro_col, INTRO_ROW(12), -1, TERM_WHITE,
            "stolen light that is not his own.");

        Term_putstr(intro_col, INTRO_ROW(14), -1, TERM_YELLOW,
            "Take up blade and burden. Descend.");
        Term_putstr(intro_col, INTRO_ROW(15), -1, TERM_YELLOW,
            "Oaths, quests, blessings of the Valar");
        Term_putstr(intro_col, INTRO_ROW(16), -1, TERM_YELLOW,
            "await in the First Age reborn.");
        break;

    case 2:
        Term_putstr(title_col, INTRO_ROW(1), -1, TERM_WHITE,
            "S I L - M O R E");
        Term_putstr(subtitle_col, INTRO_ROW(2), -1, TERM_L_BLUE,
            "~ Shining  Darkness ~");

        Term_putstr(intro_col, INTRO_ROW(4), -1, TERM_WHITE,
            "Before the Sun and Moon were wrought");
        Term_putstr(intro_col, INTRO_ROW(5), -1, TERM_WHITE,
            "the Eldar walked by starlight alone.");
        Term_putstr(intro_col, INTRO_ROW(6), -1, TERM_WHITE,
            "Now shadow stirs beneath the earth");
        Term_putstr(intro_col, INTRO_ROW(7), -1, TERM_WHITE,
            "where Morgoth sits upon his throne.");

        Term_putstr(intro_col, INTRO_ROW(9), -1, TERM_WHITE,
            "Three jewels blaze upon his crown --");
        Term_putstr(intro_col, INTRO_ROW(10), -1, TERM_WHITE,
            "stolen fire none may reclaim...");
        Term_putstr(intro_col, INTRO_ROW(11), -1, TERM_WHITE,
            "unless one dares the iron dark");
        Term_putstr(intro_col, INTRO_ROW(12), -1, TERM_WHITE,
            "and walks through everlasting flame.");

        Term_putstr(intro_col, INTRO_ROW(14), -1, TERM_L_BLUE,
            "\"...and the light that blazed in them");
        Term_putstr(intro_col, INTRO_ROW(15), -1, TERM_L_BLUE,
            "  no power could dim or mar.\"");
        Term_putstr(quote_attr_col, INTRO_ROW(16), -1, TERM_SLATE,
            "-- Of the Silmarils");
        break;

    case 3:
        Term_putstr(intro_col, INTRO_ROW(1), -1, TERM_L_BLUE,
            "\"The leaves were long, the grass was green,");
        Term_putstr(intro_col, INTRO_ROW(2), -1, TERM_L_BLUE,
            "  the hemlock-umbels tall and fair,");
        Term_putstr(intro_col, INTRO_ROW(3), -1, TERM_L_BLUE,
            "  and in the glade a light was seen");
        Term_putstr(intro_col, INTRO_ROW(4), -1, TERM_L_BLUE,
            "  of stars in shadow shimmering.\"");
        Term_putstr(song_attr_col, INTRO_ROW(5), -1, TERM_SLATE,
            "-- Of Beren and Luthien");

        Term_putstr(title_col, INTRO_ROW(7), -1, TERM_WHITE,
            "S I L - M O R E");
        Term_putstr(subtitle_col, INTRO_ROW(8), -1, TERM_L_BLUE,
            "~ Shining  Darkness ~");

        Term_putstr(intro_col, INTRO_ROW(10), -1, TERM_WHITE,
            "Even in the deepest dark, a song");
        Term_putstr(intro_col, INTRO_ROW(11), -1, TERM_WHITE,
            "may still undo the mightiest door.");
        Term_putstr(intro_col, INTRO_ROW(12), -1, TERM_WHITE,
            "Dare the throne-hall of the Enemy");
        Term_putstr(intro_col, INTRO_ROW(13), -1, TERM_WHITE,
            "and seize what Morgoth stole of old.");

        Term_putstr(intro_col, INTRO_ROW(15), -1, TERM_YELLOW,
            "Oaths, quests, blessings of the Valar");
        Term_putstr(intro_col, INTRO_ROW(16), -1, TERM_YELLOW,
            "await in the First Age reborn.");
        break;

    case 4:
        Term_putstr(intro_col, INTRO_ROW(1), -1, TERM_L_BLUE,
            "\"The day shall come again when you");
        Term_putstr(intro_col, INTRO_ROW(2), -1, TERM_L_BLUE,
            "  shall see the Sun once more.\"");
        Term_putstr(quote_attr_col, INTRO_ROW(3), -1, TERM_SLATE,
            "-- Words of Hurin");

        Term_putstr(title_col, INTRO_ROW(5), -1, TERM_WHITE,
            "S I L - M O R E");
        Term_putstr(subtitle_col, INTRO_ROW(6), -1, TERM_L_BLUE,
            "~ Shining  Darkness ~");

        Term_putstr(intro_col, INTRO_ROW(8), -1, TERM_WHITE,
            "No chain can hold a will unbroken.");
        Term_putstr(intro_col, INTRO_ROW(9), -1, TERM_WHITE,
            "Though Morgoth's shadow covers all,");
        Term_putstr(intro_col, INTRO_ROW(10), -1, TERM_WHITE,
            "the free may still defy the dark");
        Term_putstr(intro_col, INTRO_ROW(11), -1, TERM_WHITE,
            "and wrest a jewel from his crown.");

        Term_putstr(intro_col, INTRO_ROW(13), -1, TERM_YELLOW,
            "Take up blade and burden. Descend.");
        Term_putstr(intro_col, INTRO_ROW(14), -1, TERM_YELLOW,
            "Oaths, quests, blessings of the Valar");
        Term_putstr(intro_col, INTRO_ROW(15), -1, TERM_YELLOW,
            "await in the First Age reborn.");

        Term_putstr(intro_col, INTRO_ROW(16), -1, TERM_L_BLUE,
            "\"Aure entuluva!\"");
        break;
    }

    Term_fresh();
    (void)Term_set_cursor(saved_cursor_state);

#undef INTRO_ROW
}

void init_angband(void)
{
    SDL_IOStream* fd;
    int mode = 0644;
    char buf[1024];
    int i;

    platform_load_app_options();
    run_mode_reset();

    sdl_story_font_enable();
    display_introduction();
    sdl_story_font_reset();

#ifdef SIL_USE_LOCAL_DATA
    path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, "scores.raw");
#else
    if (ANGBAND_DIR_METARUN && *ANGBAND_DIR_METARUN)
    {
        char meta_dir[1024];
        SDL_strlcpy(meta_dir, ANGBAND_DIR_METARUN, sizeof(meta_dir));
        char* last_sep = strrchr(meta_dir, PATH_SEP[0]);
        if (last_sep)
            *last_sep = '\0';
        path_build(buf, sizeof(buf), meta_dir, "scores.raw");
    }
    else
    {
        path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, "scores.raw");
    }
#endif

    fd = sdl_fopen(buf, "rb");

    if (!fd)
    {
        FILE_TYPE(FILE_TYPE_DATA);
        safe_setuid_grab();
        fd = sdl_fmake(buf, mode);
        safe_setuid_drop();

        if (!fd)
        {
            char why[1024];
            strnfmt(why, sizeof(why), "Cannot create the '%s' file!", buf);
            init_angband_aux(why);
        }
        else
        {
            score_file_header score_hdr;
            score_hdr.version_major = SCORE_FILE_VERSION_MAJOR;
            score_hdr.version_minor = SCORE_FILE_VERSION_MINOR;
            score_hdr.version_patch = SCORE_FILE_VERSION_PATCH;
            score_hdr.version_extra = SCORE_FILE_VERSION_EXTRA;
            score_hdr.entry_count = 0;
            score_hdr.reserved[0] = 0;
            score_hdr.reserved[1] = 0;

            sdl_write(fd, (cptr)&score_hdr, sizeof(score_hdr));
        }
    }

    sdl_fclose(fd);

    log_info("Loading metarun...");
    if (load_metaruns(1) != 0)
    {
        init_angband_aux("Cannot load or create metarun file!");
    }

    note("[Initializing array sizes...]");
    if (init_z_info())
        quit("Cannot initialize sizes");

    note("[Initializing arrays. (runtypes)]");
    if (init_rt_info())
        quit("Cannot initialise run types");

    note("[Initializing arrays... (features)]");
    if (init_f_info())
        quit("Cannot initialize features");

    note("[Initializing arrays... (objects)]");
    if (init_k_info())
        quit("Cannot initialize objects");

    note("[Initializing arrays... (abilities)]");
    if (init_b_info())
        quit("Cannot initialize abilities");

    note("[Initializing arrays... (artefacts)]");
    if (init_a_info())
        quit("Cannot initialize artefacts");
    ensure_artifact_guids();
    ensure_artifact_spawn_numbers();

    note("[Initializing arrays... (item sets)]");
    {
        SDL_IOStream* fp;
        char path[1024];
        char linebuf[1024];
        header set_head;
        errr err;

        init_header(&set_head, 1, 1);
        item_sets_reset();

        path_build(path, sizeof(path), ANGBAND_DIR_EDIT, format("%s.txt", "set"));
        fp = sdl_fopen(path, "r");
        if (!fp)
        {
            log_warn("init_angband: No set.txt found at '%s' (item sets disabled)",
                path);
        }
        else
        {
            err = init_info_txt(fp, linebuf, &set_head, parse_set_info);
            sdl_fclose(fp);

            if (err)
                display_parse_error("set", err, linebuf);

            err = item_sets_finalize();
            if (err)
                display_parse_error("set", err, "set validation");
        }
    }

    note("[Initializing arrays... (special items)]");
    if (init_e_info())
        quit("Cannot initialize special items");

    note("[Initializing arrays... (monsters)]");
    if (init_r_info())
        quit("Cannot initialize monsters");

    if (!r_base)
        r_base = mem_alloc_array(z_info->r_max, monster_race);
    for (i = 0; i < z_info->r_max; i++)
        r_base[i] = r_info[i];

    note("[Initializing arrays... (vaults)]");
    if (init_v_info())
        quit("Cannot initialize vaults");

    note("[Initializing arrays... (histories)]");
    if (init_h_info())
        quit("Cannot initialize histories");

    note("[Initializing arrays... (stories)]");
    if (init_st_info())
        quit("Cannot initialize stories");

    note("[Initializing arrays... (styles)]");
    if (init_style_info())
        quit("Cannot initialize styles");
    style_info = (style_type*)style_head.info_ptr;
    style_name = style_head.name_ptr;
    if (init_partition_info())
        quit("Cannot initialize partition rules");

    note("[Initializing arrays... (curses)]");
    if (init_cu_info())
        quit("Cannot initialize curses");

    note("[Initializing arrays... (blessings)]");
    if (init_mb_info())
        quit("Cannot initialize major blessings");
    metarun_apply_runtime_effects();

    note("[Initializing arrays... (races)]");
    if (init_p_info())
        quit("Cannot initialize races");

    note("[Initializing arrays... (characters)]");
    if (init_c_info())
        quit("Cannot initialize characters");

    note("[Initializing arrays... (flavors)]");
    if (init_flavor_info())
        quit("Cannot initialize flavors");

    note("[Initializing arrays... (effects)]");
    if (init_effect_info())
        quit("Cannot initialize effects");

    note("[Initializing arrays... (skeleton notes)]");
    if (init_skeleton_note_info())
        quit("Cannot initialize skeleton notes");

    note("[Initializing arrays... (quests)]");
    if (init_quest_info())
        quit("Cannot initialize quests");

    note("[Initializing arrays... (oaths)]");
    if (init_oath_info())
        quit("Cannot initialize oaths");

    note("[Initializing arrays... (other)]");
    if (init_other())
        quit("Cannot initialize other stuff");

    note("[Initializing arrays... (alloc)]");
    if (init_alloc())
        quit("Cannot initialize alloc stuff");

    note("[Loading basic user pref file...]");
    (void)process_pref_file("pref.prf");

    note("[Initializing Random Artefact Tables...]");
    if (init_n_info())
        quit("Cannot initialize random name generator stuff");

    build_randart_tables();

    if (metarun_created)
        cleanup_old_game_files();

    note("                                              ");
}

NavResult initial_menu(bool* start_new)
{
    log_info("initial_menu: ENTERED - showing main menu");
    int ch;
    NavResult result = NAV_BACK;
    bool intro_story_font = true;
    sdl_story_font_enable();

    int wid, hgt;
    Term_get_size(&wid, &hgt);
    (void)wid;

    welcome_intro_layout intro_layout;
    bool show_sep;
    bool show_blank;
    bool show_prompt;
    bool show_wizard_line = runtime_cli_wizard();

    welcome_screen_compute_layout(hgt, show_wizard_line, &intro_layout,
        &show_sep, &show_blank, &show_prompt);
    display_introduction_with_layout(&intro_layout);

    {
        const int x = welcome_screen_base_col();
        const char* wizard_line =
            "Resurrecting a character is a form of cheating.";
        const char* sep_line = "- - - - - - - - - - - -";
        const char* menu_line =
            (metarun_created == true)
                ? "[Space] Begin    [Q/Esc] Quit"
                : "[Space] Continue  [Q/Esc] Quit";
        int row = hgt - 1;

        if (show_prompt && row >= 0 && row < hgt)
        {
            Term_putstr(x, row, -1, TERM_SLATE, menu_line);
            row--;
        }

        if (show_blank && row >= 0)
            row--;

        if (show_sep && row >= 0 && row < hgt)
        {
            Term_putstr(x, row, -1, TERM_L_DARK, sep_line);
            row--;
        }

        if (show_wizard_line && row >= 0 && row < hgt)
            Term_putstr(x, row, 60, TERM_BLUE, wizard_line);
    }

    Term_fresh();

    bool saved_hide_cursor = inkey_cursor_hidden();
    inkey_set_cursor_hidden(true);
    ch = inkey();
    inkey_set_cursor_hidden(saved_hide_cursor);

    if (ch == '\n' || ch == '\r' || ch == ' ')
    {
        log_info("initial_menu: User pressed space/enter - starting game");
        run_mode_set_pending(RUN_MODE_STORY);
        *start_new = true;
        result = NAV_OK;
        goto menu_done;
    }

    if (ch == 'q' || ch == ESCAPE)
    {
        result = NAV_QUIT;
        goto menu_done;
    }

menu_done:
    log_info("initial_menu: EXITING with result=%d", result);
    if (sdl_config_should_force_intro_flame())
    {
        sdl_config_mark_intro_seen();
        save_pane_config_to_json();
    }
    if (intro_story_font)
        sdl_story_font_reset();
    return result;
}

void cleanup_angband(void)
{
    macro_free();
    macro_trigger_free();

    mem_free_null(alloc_ego_table);
    mem_free_null(alloc_race_table);
    mem_free_null(alloc_kind_table);

    mem_free_null(inventory);

    autoinscribe_clean();

    mem_free_null(l_list);
    mem_free_null(mon_list);
    mem_free_null(o_list);

    mem_free_null(cave_when);
    mem_free_null(cave_o_idx);
    mem_free_null(cave_m_idx);
    mem_free_null(cave_feat);
    mem_free_null(cave_color);
    mem_free_null(cave_info);
    mem_free_null(cave_light);

    mem_free_null(view_g);
    mem_free_null(temp_g);
    mem_free_null(temp_y);
    mem_free_null(temp_x);

    messages_free();
    quarks_free();
    free_randart_tables();

    free_info(&flavor_head);
    free_info(&g_head);
    free_info(&b_head);
    free_info(&c_head);
    free_info(&p_head);
    free_info(&h_head);
    free_info(&v_head);
    free_info(&r_head);
    free_info(&e_head);
    free_info(&a_head);
    free_info(&k_head);
    free_info(&f_head);
    free_info(&z_head);
    free_info(&n_head);
    free_info(&style_head);
    free_info(&skeleton_note_head);

    str_free(ANGBAND_DIR);
    str_free(ANGBAND_DIR_APEX);
    str_free(ANGBAND_DIR_METARUN);
    str_free(ANGBAND_DIR_BONE);
    str_free(ANGBAND_DIR_DATA);
    str_free(ANGBAND_DIR_EDIT);
    str_free(ANGBAND_DIR_FILE);
    str_free(ANGBAND_DIR_HELP);
    str_free(ANGBAND_DIR_INFO);
    str_free(ANGBAND_DIR_SAVE);
    str_free(ANGBAND_DIR_PREF);
    str_free(ANGBAND_DIR_USER);
    str_free(ANGBAND_DIR_XTRA);
    str_free(ANGBAND_DIR_SCRIPT);
}

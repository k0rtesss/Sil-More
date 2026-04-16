/* File: init2.c */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "app/app-session.h"
#include "blitz.h"
#include "fs/file.h"
#include "fs/path.h"
#include "init/init-lifecycle.h"
#include "log/log.h"
#include "platform-audio.h"
#include "runtime-cli.h"
#include "platform-config.h"
#include "platform-input.h"
#include "platform-time.h"
#include "platform-story-font.h"
#include "metarun.h"
#include "item_set.h"
#include "init/init2-internal.h"
#include "ui/ui-information-scene.h"

static int g_welcome_scene_intro_style = -1;

static void display_introduction(void);
static void welcome_screen_begin_session(void);
static bool welcome_screen_present_ui(cptr status_line, bool show_footer);
static int welcome_screen_current_intro_style(void);
static void welcome_prompt_label(int binding, const char* fallback,
    char* buf, size_t buflen);

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

    display_introduction();

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

    (void)quarks_init();
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
    (void)welcome_screen_present_ui(str, false);
}

static void init_angband_aux(cptr why)
{
    quit(format("%s\n\n%s", why,
        "The 'lib' directory is probably missing or broken.\n"
        "Perhaps the archive was not extracted correctly.\n"
        "See the manual for more information."));
}

typedef struct welcome_ui_line {
    byte attr;
    byte story;
    byte col;
    cptr text;
} welcome_ui_line;

#define WELCOME_STORY(_col, _attr, _text) { (_attr), STORY_FLAG_USE, (_col), (_text) }
#define WELCOME_TEXT(_col, _attr, _text) { (_attr), 0, (_col), (_text) }
#define WELCOME_BLANK() { TERM_WHITE, 0, 0, "" }
#define WELCOME_END() { 0, 0, 0, NULL }

static const welcome_ui_line welcome_intro_lines_0[] = {
    WELCOME_STORY(14, TERM_L_BLUE, "\"In the beginning Eru, the One,"),
    WELCOME_STORY(14, TERM_L_BLUE, "  made the Ainur of his thought;"),
    WELCOME_STORY(14, TERM_L_BLUE, "  and they sang, and he was glad.\""),
    WELCOME_STORY(34, TERM_SLATE, "-- Ainulindale"),
    WELCOME_BLANK(),
    WELCOME_STORY(22, TERM_WHITE, "S I L - M O R E"),
    WELCOME_STORY(20, TERM_L_BLUE, "~ Shining  Darkness ~"),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_WHITE, "In the deeps of Angband, beyond"),
    WELCOME_STORY(14, TERM_WHITE, "gates of iron and pits of flame,"),
    WELCOME_STORY(14, TERM_WHITE, "Morgoth hoards the Silmarils --"),
    WELCOME_STORY(14, TERM_WHITE, "three jewels of living light."),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_YELLOW, "Take up blade and burden. Descend."),
    WELCOME_STORY(14, TERM_YELLOW, "Oaths, quests, blessings of the Valar"),
    WELCOME_STORY(14, TERM_YELLOW, "await in the First Age reborn."),
    WELCOME_END()
};

static const welcome_ui_line welcome_intro_lines_1[] = {
    WELCOME_STORY(14, TERM_L_BLUE, "\"Be he foe or friend,"),
    WELCOME_STORY(14, TERM_L_BLUE, "  be he foul or clean..."),
    WELCOME_STORY(14, TERM_L_BLUE, "  he shall defend, shall be held mine.\""),
    WELCOME_STORY(34, TERM_SLATE, "-- Oath of Feanor"),
    WELCOME_BLANK(),
    WELCOME_STORY(22, TERM_WHITE, "S I L - M O R E"),
    WELCOME_STORY(20, TERM_L_BLUE, "~ Shining  Darkness ~"),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_WHITE, "In the pits beneath the mountains"),
    WELCOME_STORY(14, TERM_WHITE, "Morgoth broods upon his throne."),
    WELCOME_STORY(14, TERM_WHITE, "Three jewels burn upon his crown --"),
    WELCOME_STORY(14, TERM_WHITE, "stolen light that is not his own."),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_YELLOW, "Take up blade and burden. Descend."),
    WELCOME_STORY(14, TERM_YELLOW, "Oaths, quests, blessings of the Valar"),
    WELCOME_STORY(14, TERM_YELLOW, "await in the First Age reborn."),
    WELCOME_END()
};

static const welcome_ui_line welcome_intro_lines_2[] = {
    WELCOME_STORY(22, TERM_WHITE, "S I L - M O R E"),
    WELCOME_STORY(20, TERM_L_BLUE, "~ Shining  Darkness ~"),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_WHITE, "Before the Sun and Moon were wrought"),
    WELCOME_STORY(14, TERM_WHITE, "the Eldar walked by starlight alone."),
    WELCOME_STORY(14, TERM_WHITE, "Now shadow stirs beneath the earth"),
    WELCOME_STORY(14, TERM_WHITE, "where Morgoth sits upon his throne."),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_WHITE, "Three jewels blaze upon his crown --"),
    WELCOME_STORY(14, TERM_WHITE, "stolen fire none may reclaim..."),
    WELCOME_STORY(14, TERM_WHITE, "unless one dares the iron dark"),
    WELCOME_STORY(14, TERM_WHITE, "and walks through everlasting flame."),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_L_BLUE, "\"...and the light that blazed in them"),
    WELCOME_STORY(14, TERM_L_BLUE, "  no power could dim or mar.\""),
    WELCOME_STORY(34, TERM_SLATE, "-- Of the Silmarils"),
    WELCOME_END()
};

static const welcome_ui_line welcome_intro_lines_3[] = {
    WELCOME_STORY(14, TERM_L_BLUE, "\"The leaves were long, the grass was green,"),
    WELCOME_STORY(14, TERM_L_BLUE, "  the hemlock-umbels tall and fair,"),
    WELCOME_STORY(14, TERM_L_BLUE, "  and in the glade a light was seen"),
    WELCOME_STORY(14, TERM_L_BLUE, "  of stars in shadow shimmering.\""),
    WELCOME_STORY(28, TERM_SLATE, "-- Of Beren and Luthien"),
    WELCOME_BLANK(),
    WELCOME_STORY(22, TERM_WHITE, "S I L - M O R E"),
    WELCOME_STORY(20, TERM_L_BLUE, "~ Shining  Darkness ~"),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_WHITE, "Even in the deepest dark, a song"),
    WELCOME_STORY(14, TERM_WHITE, "may still undo the mightiest door."),
    WELCOME_STORY(14, TERM_WHITE, "Dare the throne-hall of the Enemy"),
    WELCOME_STORY(14, TERM_WHITE, "and seize what Morgoth stole of old."),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_YELLOW, "Oaths, quests, blessings of the Valar"),
    WELCOME_STORY(14, TERM_YELLOW, "await in the First Age reborn."),
    WELCOME_END()
};

static const welcome_ui_line welcome_intro_lines_4[] = {
    WELCOME_STORY(14, TERM_L_BLUE, "\"The day shall come again when you"),
    WELCOME_STORY(14, TERM_L_BLUE, "  shall see the Sun once more.\""),
    WELCOME_STORY(34, TERM_SLATE, "-- Words of Hurin"),
    WELCOME_BLANK(),
    WELCOME_STORY(22, TERM_WHITE, "S I L - M O R E"),
    WELCOME_STORY(20, TERM_L_BLUE, "~ Shining  Darkness ~"),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_WHITE, "No chain can hold a will unbroken."),
    WELCOME_STORY(14, TERM_WHITE, "Though Morgoth's shadow covers all,"),
    WELCOME_STORY(14, TERM_WHITE, "the free may still defy the dark"),
    WELCOME_STORY(14, TERM_WHITE, "and wrest a jewel from his crown."),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_YELLOW, "Take up blade and burden. Descend."),
    WELCOME_STORY(14, TERM_YELLOW, "Oaths, quests, blessings of the Valar"),
    WELCOME_STORY(14, TERM_YELLOW, "await in the First Age reborn."),
    WELCOME_STORY(14, TERM_L_BLUE, "\"Aure entuluva!\""),
    WELCOME_END()
};

static const welcome_ui_line welcome_intro_lines_5[] = {
    WELCOME_STORY(22, TERM_WHITE, "S I L - M O R E"),
    WELCOME_STORY(20, TERM_L_BLUE, "~ Shining  Darkness ~"),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_WHITE, "By silver waters Elves first woke"),
    WELCOME_STORY(14, TERM_WHITE, "beneath the stars ere morning broke."),
    WELCOME_STORY(14, TERM_WHITE, "No sun had risen, no moon shone --"),
    WELCOME_STORY(14, TERM_WHITE, "just heaven's light on lake and stone."),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_WHITE, "Then Morgoth's shadow veiled the land"),
    WELCOME_STORY(14, TERM_WHITE, "and stole the Light with iron hand."),
    WELCOME_STORY(14, TERM_WHITE, "Yet still a whisper stirs the deep:"),
    WELCOME_STORY(14, TERM_WHITE, "what darkness took, the bold may reap."),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_L_BLUE, "\"...the starlight glittered"),
    WELCOME_STORY(14, TERM_L_BLUE, "  on the waters of Cuivienen.\""),
    WELCOME_STORY(34, TERM_SLATE, "-- Of the Coming of the Elves"),
    WELCOME_END()
};

static const welcome_ui_line welcome_intro_lines_6[] = {
    WELCOME_STORY(22, TERM_WHITE, "S I L - M O R E"),
    WELCOME_STORY(20, TERM_L_BLUE, "~ Shining  Darkness ~"),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_WHITE, "In Valinor the Two Trees shone"),
    WELCOME_STORY(14, TERM_WHITE, "with gold and silver, leaf and bough."),
    WELCOME_STORY(14, TERM_WHITE, "Their mingled light is dead and gone --"),
    WELCOME_STORY(14, TERM_WHITE, "the world lies under shadow now."),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_WHITE, "Across the ice the exiles came,"),
    WELCOME_STORY(14, TERM_WHITE, "the Noldor burning with their oath."),
    WELCOME_STORY(14, TERM_WHITE, "They traded bliss for grief and flame"),
    WELCOME_STORY(14, TERM_WHITE, "and lost the blessing of them both."),
    WELCOME_BLANK(),
    WELCOME_STORY(14, TERM_L_BLUE, "\"...and the Noldor wept"),
    WELCOME_STORY(14, TERM_L_BLUE, "  for the beauty of Telperion and Laurelin.\""),
    WELCOME_STORY(34, TERM_SLATE, "-- Of the Darkening of Valinor"),
    WELCOME_END()
};

static const welcome_ui_line* welcome_screen_intro_lines_for_style(int style)
{
    switch (style)
    {
    case 1:
        return welcome_intro_lines_1;
    case 2:
        return welcome_intro_lines_2;
    case 3:
        return welcome_intro_lines_3;
    case 4:
        return welcome_intro_lines_4;
    case 5:
        return welcome_intro_lines_5;
    case 6:
        return welcome_intro_lines_6;
    case 0:
    default:
        return welcome_intro_lines_0;
    }
}

static int welcome_screen_current_intro_style(void)
{
    if (sdl_config_should_force_intro_flame())
        return INTRO_STYLE_FLAME;
    if (op_ptr->intro_style == INTRO_STYLE_RANDOM)
        return (int)(platform_monotonic_ms() % 7u);
    return (int)op_ptr->intro_style;
}

static void welcome_prompt_label(int binding, const char* fallback,
    char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static int welcome_screen_active_intro_style(void)
{
    if (g_welcome_scene_intro_style < 0 || g_welcome_scene_intro_style > 6)
        g_welcome_scene_intro_style = welcome_screen_current_intro_style();

    return g_welcome_scene_intro_style;
}

static void welcome_screen_begin_session(void)
{
    g_welcome_scene_intro_style = welcome_screen_current_intro_style();
}

static bool welcome_screen_status_has_visible_text(cptr text)
{
    if (!text)
        return false;

    while (*text)
    {
        if (!isspace((unsigned char)(*text)))
            return true;
        text++;
    }

    return false;
}

static bool welcome_screen_append_intro_line(app_ui_panel* panel,
    const welcome_ui_line* line)
{
    app_ui_text_line* target;

    if (!panel || !line || panel->body_line_count >= APP_UI_BODY_LINE_MAX)
        return false;

    target = &panel->body_lines[panel->body_line_count++];
    memset(target, 0, sizeof(*target));
    target->attr = line->attr;
    target->story = line->story;
    target->flags = (u16b)(line->col & APP_UI_TEXT_FLAG_WELCOME_COL_MASK);
    if (!line->text || !line->text[0])
        target->flags |= APP_UI_TEXT_FLAG_WELCOME_BLANK;
    else
        SDL_strlcpy(target->text, line->text, sizeof(target->text));

    return true;
}

static bool welcome_screen_add_intro_content(app_ui_panel* panel,
    int intro_style)
{
    const welcome_ui_line* lines = welcome_screen_intro_lines_for_style(
        intro_style);
    int i;

    if (!panel || !lines)
        return false;

    for (i = 0; lines[i].text; i++)
    {
        if (!welcome_screen_append_intro_line(panel, &lines[i]))
        {
            return false;
        }
    }

    return true;
}

static bool welcome_screen_add_footer_actions(app_ui_panel* panel)
{
    bool steamdeck = steamdeck_controls_active();

    if (!panel)
        return false;

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        welcome_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        welcome_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                   confirm_label,
                   metarun_created ? "Begin" : "Continue")
            && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Quit");
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
               "Space", metarun_created ? "Begin" : "Continue")
        && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Q/Esc", "Quit");
}

static bool welcome_screen_build_ui_scene(app_ui_scene* scene,
    cptr status_line, bool show_footer)
{
    app_ui_panel* panel;
    bool show_status = welcome_screen_status_has_visible_text(status_line);

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_WELCOME;

    if (!welcome_screen_add_intro_content(panel, welcome_screen_active_intro_style()))
    {
        return false;
    }

    if (show_footer && runtime_cli_wizard())
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_BLUE,
                "Resurrecting a character is a form of cheating."))
        {
            return false;
        }
    }

    if (show_footer && !welcome_screen_add_footer_actions(panel))
        return false;

    if (show_status)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_SLATE, status_line))
            return false;
    }

    return true;
}

static bool welcome_screen_present_ui(cptr status_line, bool show_footer)
{
    app_ui_scene scene;

    if (!welcome_screen_build_ui_scene(&scene, status_line, show_footer))
        return false;

    return ui_information_scene_present_ui(&scene);
}

static void display_introduction(void)
{
    welcome_screen_begin_session();
    (void)welcome_screen_present_ui(NULL, false);
}

void init_angband(void)
{
    ang_file* fd;
    int mode = 0644;
    char buf[1024];
    int i;

    platform_load_app_options();
    run_mode_reset();

    display_introduction();

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

    fd = ang_file_open(buf, "rb");

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

    ang_file_close(fd);

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
        ang_file* fp;
        char path[1024];
        char linebuf[1024];
        header set_head;
        errr err;

        init_header(&set_head, 1, 1);
        item_sets_reset();

        path_build(path, sizeof(path), ANGBAND_DIR_EDIT, format("%s.txt", "set"));
        fp = ang_file_open(path, "r");
        if (!fp)
        {
            log_warn("init_angband: No set.txt found at '%s' (item sets disabled)",
                path);
        }
        else
        {
            err = init_info_txt(fp, linebuf, &set_head, parse_set_info);
            ang_file_close(fp);

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

    note("[Initializing display defaults...]");

    note("[Initializing Random Artefact Tables...]");
    if (init_n_info())
        quit("Cannot initialize random name generator stuff");

    build_randart_tables();

    /* Snapshot the fully initialized monster templates. This baseline must be
     * taken after prefs and mon_power setup so load can restore clean runtime
     * race data without replaying stale pre-init values. */
    if (!r_base)
    {
        r_base = mem_alloc_array(z_info->r_max, monster_race);
    }
    for (i = 0; i < z_info->r_max; i++)
    {
        r_base[i] = r_info[i];
    }

    /* Clean up old files if this is a fresh start (no existing metarun) */
    if (metarun_created)
        cleanup_old_game_files();

    note("                                              ");
}

NavResult initial_menu(bool* start_new)
{
    log_info("initial_menu: ENTERED - showing main menu");
    if (sdl_music_consume_welcome_main_once()
        || score_count_alive_entries() > 0)
        sdl_music_play_main();
    else
        sdl_music_play_main_full();

    int ch;
    NavResult result = NAV_BACK;
    bool steamdeck = steamdeck_controls_active();
    ui_information_scene_scope welcome_scope;

    if (!welcome_screen_present_ui(NULL, true))
    {
        log_error("initial_menu: failed to present semantic welcome screen");
        return NAV_QUIT;
    }
    if (!ui_information_scene_claim_input(&welcome_scope,
            APP_WAIT_REASON_BOOTSTRAP))
    {
        log_error("initial_menu: failed to claim semantic welcome input");
        return NAV_QUIT;
    }

    bool saved_hide_cursor = inkey_cursor_hidden();
    inkey_set_cursor_hidden(true);
    ch = ui_information_scene_wait_key_nonrepeat();
    inkey_set_cursor_hidden(saved_hide_cursor);
    ui_information_scene_leave(&welcome_scope);

    if (ch == '\n' || ch == '\r' || ch == ' '
        || (steamdeck && ch == steamdeck_confirm_key()))
    {
        log_info("initial_menu: User pressed space/enter - starting game");
        run_mode_set_pending(RUN_MODE_STORY);
        *start_new = true;
        result = NAV_OK;
        goto menu_done;
    }

    if (ch == 'q' || ch == ESCAPE
        || (steamdeck && ch == steamdeck_back_key()))
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
    return result;
}

void cleanup_angband(void)
{
    mem_free_null(alloc_ego_table);
    mem_free_null(alloc_race_table);
    mem_free_null(alloc_kind_table);

    mem_free_null(inventory);

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

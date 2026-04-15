/* File: squelch.c */

/*
 * Copyright (c) ???
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "ui/ui-information-scene.h"

static void do_qual_squelch(void);
static int do_ego_item_squelch(void);

typedef struct tval_insc_desc tval_insc_desc;

typedef struct squelch_layout
{
    int term_wid;
    int term_hgt;
    bool compact;
    int footer_row;
    int status_row;
} squelch_layout;

struct tval_insc_desc
{
    int tval;
    cptr desc;
};

/*
 * This stores the various squelch levels for the secondary squelching.
 * It is currently hardcoded at 24 bytes, but since there are only 20
 * applicable tvals there shouldn't be a problem.
 */

byte squelch_level[SQUELCH_BYTES];

#define SQUELCH_PANEL_MIN_WIDTH 1180
#define SQUELCH_PANEL_MAX_WIDTH 2200
#define SQUELCH_PAGE_STEP 10
#define LINES_PER_COLUMN 19

static bool squelch_prompt_text_input(cptr prompt, char* buf, size_t len)
{
    return prompt_text_input(prompt,
        "Enter accepts, Esc cancels, Backspace erases.", buf, len, false);
}

static char squelch_wait_key(void)
{
    return (char)ui_information_scene_wait_key_hidden_with_wait_reason(
        APP_WAIT_REASON_LIST_SELECTION);
}

static void squelch_get_layout(squelch_layout* layout)
{
    if (!layout)
        return;

    layout->term_wid = 80;
    layout->term_hgt = 24;
    if (platform_frame_active_view_cols() > 0)
        layout->term_wid = platform_frame_active_view_cols();
    if (platform_frame_active_view_rows() > 0)
        layout->term_hgt = platform_frame_active_view_rows();
    if (layout->term_wid < 1)
        layout->term_wid = 80;
    if (layout->term_hgt < 1)
        layout->term_hgt = 24;

    layout->compact = (layout->term_wid < 70 || layout->term_hgt < 20);
    layout->footer_row = layout->term_hgt - 1;
    layout->status_row = layout->term_hgt - 2;
}

static void squelch_put_fit(byte attr, cptr text, int row, int col,
    const squelch_layout* layout)
{
    int max;
    size_t clip_size;
    char clipped[1024];

    if (!layout || !text || row < 0 || col < 0 || row >= layout->term_hgt
        || col >= layout->term_wid)
    {
        return;
    }

    max = layout->term_wid - col;
    if (max < 1)
        return;

    clip_size = (size_t)max + 1;
    if (clip_size > sizeof(clipped))
        clip_size = sizeof(clipped);

    SDL_strlcpy(clipped, text, clip_size);
    c_put_str(attr, clipped, row, col);
}

static bool squelch_present_ui_scene(const app_ui_scene* scene, int* out_key)
{
    ui_information_scene_scope scope;

    if (!scene || !out_key)
        return false;
    if (!ui_information_scene_enter(&scope))
        return false;
    if (!ui_information_scene_present_ui(scene))
    {
        ui_information_scene_leave(&scope);
        return false;
    }

    *out_key = squelch_wait_key();
    ui_information_scene_leave(&scope);
    return true;
}

static app_ui_panel* squelch_begin_browser_scene(app_ui_scene* scene,
    cptr title, cptr subtitle)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SHOW_DETAIL
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, SQUELCH_PANEL_MIN_WIDTH,
        SQUELCH_PANEL_MAX_WIDTH);
    app_ui_panel_set_title(panel, TERM_L_WHITE, title);
    if (subtitle && subtitle[0])
        app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

    return panel;
}

static void squelch_show_status_message(cptr msg)
{
    if (!msg || !msg[0])
        return;

    msg_print(msg);
    pause_line(0);
}

/*
 * These are the base types for automatic squelching on creation.
 * I've combined some of the tvals to make this list a little more
 * reasonable.
 */

#define TYPE_AMMO 1
#define TYPE_BOW 2
#define TYPE_WEAPON1 3
#define TYPE_WEAPON2 4
#define TYPE_BODY 5
#define TYPE_CLOAK 6
#define TYPE_SHIELD 7
#define TYPE_HELM 8
#define TYPE_GLOVES 9
#define TYPE_BOOTS 10
#define TYPE_RING 11
#define TYPE_STAFF 12
#define TYPE_WAND 13
#define TYPE_HORN 14
#define TYPE_SCROLL 15
#define TYPE_POTION 16
#define TYPE_AMULET 17
#define TYPE_BOOK 18
#define TYPE_FOOD 19
#define TYPE_MISC 20

/*
 * This (admittedly hacky) stores the mapping from tval to typeval
 * and is reinitialized every time do_cmd_squelch is called.  This
 * can certainly be done more cleanly.
 */
static int tv_to_type[100];

/*
 * These structures are lifted from wizard2.c where they were used in
 * the 'create item' command.  I have adapted them for my code.
 */

typedef struct tval_desc
{
    int tval;
    cptr desc;
} tval_desc;

static char head[4] = { 'a', 'A', '0', ':' };

/*
 * Here are the categories for squelch-on-creation.
 */
static tval_desc typevals[] = { { TYPE_AMMO, "Arrows" }, { TYPE_BOW, "Bows" },
    { TYPE_WEAPON1, "Weapons (Blades)" },
    { TYPE_WEAPON2, "Weapons (Non Blades)" }, { TYPE_BODY, "Body Armor" },
    { TYPE_CLOAK, "Cloaks" }, { TYPE_SHIELD, "Shields" },
    { TYPE_HELM, "Helmets" }, { TYPE_GLOVES, "Gloves" },
    { TYPE_BOOTS, "Boots" }, { TYPE_AMULET, "Amulets" }, { TYPE_RING, "Rings" },
    { TYPE_STAFF, "Staves" }, { TYPE_HORN, "Horns" },
    { TYPE_POTION, "Potions" }, { TYPE_FOOD, "Food Items" },
    { TYPE_MISC, "Miscellaneous" }, { 0, NULL }

};

/*
 * Here are the categories for squelch-on-identification.
 * This array is lifted (and edited_ from wizard2.c, hence
 * the spacy formatting.
 */

static tval_desc tvals[] = { { TV_SWORD, "Sword" },
    { TV_POLEARM, "Axe or Polearm" }, { TV_HAFTED, "Blunt Weapon" },
    { TV_BOW, "Bow" }, { TV_ARROW, "Arrows" }, { TV_SHIELD, "Shield" },
    { TV_CROWN, "Crown" }, { TV_HELM, "Helm" }, { TV_GLOVES, "Gloves" },
    { TV_BOOTS, "Boots" }, { TV_CLOAK, "Cloak" }, { TV_MAIL, "Mail" },
    { TV_SOFT_ARMOR, "Soft Armor" }, { TV_DIGGING, "Diggers" },
    { TV_RING, "Rings" }, { TV_AMULET, "Amulets" }, { TV_CHEST, "Open Chests" },
    { TV_LIGHT, "Light Sources" }, { 0, NULL } };

static bool squelch_choice_hotkey(int index, char* out)
{
    if (!out)
        return false;

    *out = '\0';
    if (index < 0)
        return false;
    if (index < 26)
    {
        *out = (char)(head[0] + index);
        return true;
    }
    if (index < 52)
    {
        *out = (char)(head[1] + index - 26);
        return true;
    }
    if (index < 63)
    {
        *out = (char)(head[2] + index - 52);
        return true;
    }

    return false;
}

static int squelch_choice_index_for_key(char ch, int max_num)
{
    int index = -1;

    if (ch >= head[0] && ch < head[0] + 26)
        index = ch - head[0];
    else if (ch >= head[1] && ch < head[1] + 26)
        index = ch - head[1] + 26;
    else if (ch >= head[2] && ch < head[2] + 17)
        index = ch - head[2] + 52;

    if (index < 0 || index >= max_num)
        return -1;

    return index;
}

static void squelch_format_row_key(int index, char* buf, size_t buf_size)
{
    char hotkey;

    if (!buf || buf_size == 0)
        return;

    buf[0] = '\0';
    if (!squelch_choice_hotkey(index, &hotkey))
        return;

    strnfmt(buf, buf_size, "%c)", hotkey);
}

static const char* squelch_pickup_mode_label(int squelch)
{
    switch (squelch)
    {
    case SQUELCH_ALWAYS:
        return "Squelch";
    case NO_SQUELCH_NEVER_PICKUP:
        return "Never pickup";
    case NO_SQUELCH_ALWAYS_PICKUP:
        return "Always pickup";
    case SQUELCH_NEVER:
    default:
        return "No squelch";
    }
}

static byte squelch_pickup_mode_color(int squelch)
{
    switch (squelch)
    {
    case SQUELCH_ALWAYS:
        return TERM_L_RED;
    case NO_SQUELCH_NEVER_PICKUP:
        return TERM_L_GREEN;
    case NO_SQUELCH_ALWAYS_PICKUP:
        return TERM_L_UMBER;
    case SQUELCH_NEVER:
    default:
        return TERM_L_BLUE;
    }
}

static const char* squelch_quality_label(byte level)
{
    switch (level)
    {
    case SQUELCH_CURSED:
        return "Cursed";
    case SQUELCH_AVERAGE:
        return "Average and below";
    case SQUELCH_GOOD_STRONG:
        return "Good (strong pseudo-ID)";
    case SQUELCH_GOOD_WEAK:
        return "Good (weak pseudo-ID)";
    case SQUELCH_ALL:
        return "All but artefacts";
    case SQUELCH_OPENED_CHESTS:
        return "Opened chests";
    case SQUELCH_NONE:
    default:
        return "Nothing";
    }
}

static char squelch_quality_letter(byte level)
{
    switch (level)
    {
    case SQUELCH_CURSED:
        return 'C';
    case SQUELCH_AVERAGE:
        return 'V';
    case SQUELCH_GOOD_STRONG:
        return 'G';
    case SQUELCH_GOOD_WEAK:
        return 'W';
    case SQUELCH_ALL:
        return 'A';
    case SQUELCH_OPENED_CHESTS:
        return 'O';
    case SQUELCH_NONE:
    default:
        return 'N';
    }
}

static byte squelch_quality_color(byte level)
{
    switch (level)
    {
    case SQUELCH_CURSED:
        return TERM_WHITE;
    case SQUELCH_AVERAGE:
        return TERM_YELLOW;
    case SQUELCH_GOOD_STRONG:
        return TERM_L_GREEN;
    case SQUELCH_GOOD_WEAK:
        return TERM_GREEN;
    case SQUELCH_ALL:
        return TERM_L_RED;
    case SQUELCH_OPENED_CHESTS:
        return TERM_ORANGE;
    case SQUELCH_NONE:
    default:
        return TERM_SLATE;
    }
}

static bool squelch_quality_allows_level(int index, byte level)
{
    if (level == SQUELCH_NONE)
        return true;
    if (level == SQUELCH_OPENED_CHESTS)
        return index == CHEST_INDEX;
    if (index == CHEST_INDEX)
        return false;
    if (index == 18 || index == 19)
        return level == SQUELCH_CURSED || level == SQUELCH_ALL;

    return true;
}

static void squelch_quality_apply_single(int index, byte level)
{
    if (index < 0 || index >= SQUELCH_BYTES)
        return;
    if (!squelch_quality_allows_level(index, level))
        return;

    squelch_level[index] = level;
}

static void squelch_quality_apply_all(byte level)
{
    int i;

    if (level == SQUELCH_OPENED_CHESTS)
    {
        squelch_level[CHEST_INDEX] = SQUELCH_OPENED_CHESTS;
        return;
    }

    for (i = 0; i < SQUELCH_BYTES; i++)
    {
        if (!squelch_quality_allows_level(i, level))
            continue;
        squelch_level[i] = level;
    }
}

static cptr get_autoinscription(s16b kindIdx)
{
    int i;

    for (i = 0; i < inscriptionsCount; i++)
    {
        if (kindIdx == inscriptions[i].kindIdx)
        {
            return quark_str(inscriptions[i].inscriptionIdx);
        }
    }

    return 0;
}

extern int do_cmd_autoinscribe_item(s16b k_idx)
{
    char tmp[80] = "";
    cptr curInscription = get_autoinscription(k_idx);

    if (curInscription)
    {
        SDL_strlcpy(tmp, curInscription, sizeof(tmp));
        tmp[sizeof(tmp) - 1] = 0;
    }

    /* Get a new inscription (possibly empty) */
    if (squelch_prompt_text_input("Autoinscription: ", tmp, sizeof(tmp)))
    {
        /* Save the inscription */
        add_autoinscription(k_idx, tmp);

        /* Inscribe stuff */
        p_ptr->notice |= (PN_AUTOINSCRIBE);
        p_ptr->window |= (PW_INVEN | PW_EQUIP);

        return 1;
    }

    return 0;
}

static void squelch_save_values_to_file(void)
{
    int i;
    char ftmp[80];
    char buf[1024];
    SDL_IOStream* fff;

    sprintf(ftmp, "%s.squ", op_ptr->base_name);
    if (!askfor_aux(ftmp, sizeof(ftmp)))
        return;
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
    {
        log_error("squelch: failed to build squelch path '%s'", ftmp);
        squelch_show_status_message("Failed to resolve squelch file path.");
        return;
    }

    safe_setuid_drop();
    fff = sdl_fopen(buf, "a");
    safe_setuid_grab();
    if (!fff)
    {
        log_error("squelch: failed to open squelch file '%s' for append", buf);
        squelch_show_status_message("Failed to open squelch file.");
        return;
    }

    SDL_IOprintf(fff, "\n\n# Squelch bits\n\n");
    for (i = 1; i < z_info->k_max; i++)
    {
        int tval = k_info[i].tval;
        int sval = k_info[i].sval;
        int squelch = k_info[i].squelch;

        if (tval || sval)
            SDL_IOprintf(fff, "Q:%d:%d:%d:%d\n", i, tval, sval, squelch);
    }

    SDL_IOprintf(fff, "\n\n# squelch_level array\n\n");
    for (i = 0; i < SQUELCH_BYTES; i++)
        SDL_IOprintf(fff, "Q:%d:%d\n", i, squelch_level[i]);
    SDL_IOprintf(fff, "\n\n");

    sdl_fclose(fff);
    squelch_show_status_message("Squelch file saved successfully.");
}

static void squelch_load_values_from_file(void)
{
    char ftmp[80];

    sprintf(ftmp, "%s.squ", op_ptr->base_name);
    if (!askfor_aux(ftmp, sizeof(ftmp)))
        return;

    if (process_pref_file(ftmp))
        squelch_show_status_message("Failed to load squelch file.");
    else
        squelch_show_status_message("Squelch data loaded.");
}

static void squelch_save_autoinscriptions_to_file(void)
{
    int i;
    char ftmp[80];
    char buf[1024];
    SDL_IOStream* fff;

    SDL_strlcpy(ftmp, op_ptr->base_name, sizeof(ftmp));
    if (!askfor_aux(ftmp, sizeof(ftmp)))
        return;
    if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
    {
        log_error("squelch: failed to build autoinscription path '%s'", ftmp);
        squelch_show_status_message(
            "Failed to resolve autoinscription file path.");
        return;
    }

    safe_setuid_drop();
    fff = sdl_fopen(buf, "w");
    safe_setuid_grab();
    if (!fff)
    {
        log_error("squelch: failed to open autoinscription file '%s' for write",
            buf);
        squelch_show_status_message("Failed to save autoinscriptions.");
        return;
    }
    if (!inscriptions)
    {
        sdl_fclose(fff);
        log_warn("squelch: no inscriptions available to save");
        squelch_show_status_message("No autoinscriptions to save.");
        return;
    }

    SDL_IOprintf(fff, "# Format: B:[Item Kind]:[Inscription]\n\n");
    for (i = 0; i < inscriptionsCount; i++)
    {
        object_kind* k_ptr = &k_info[inscriptions[i].kindIdx];

        SDL_IOprintf(fff, "# Autoinscription for %s\n", k_name + k_ptr->name);
        SDL_IOprintf(fff, "B:%d:%s\n\n", inscriptions[i].kindIdx,
            quark_str(inscriptions[i].inscriptionIdx));
    }

    sdl_fclose(fff);
    squelch_show_status_message("Autoinscribe file saved successfully.");
}

static void squelch_load_autoinscriptions_from_file(void)
{
    char ftmp[80];

    SDL_strlcpy(ftmp, op_ptr->base_name, sizeof(ftmp));
    if (!askfor_aux(ftmp, sizeof(ftmp)))
        return;

    if (process_pref_file(ftmp))
        squelch_show_status_message("Failed to load autoinscribe file.");
    else
        squelch_show_status_message("Autoinscribe data loaded.");
}

static int squelch_type_count(void)
{
    int count = 0;

    while (typevals[count].tval)
        count++;

    return count;
}

static void squelch_step_selection(int* index, int delta, int max_num)
{
    if (!index || max_num <= 0)
        return;

    *index += delta;
    while (*index < 0)
        *index += max_num;
    while (*index >= max_num)
        *index -= max_num;
}

static bool squelch_build_main_menu_scene(app_ui_scene* scene,
    int selected_index)
{
    app_ui_panel* panel;
    char key[APP_UI_KEY_MAX];
    int count = squelch_type_count();

    panel = squelch_begin_browser_scene(scene, "Squelch & Autoinscription",
        "Choose an item category or a utility action.");
    if (!panel)
        return false;

    for (int i = 0; i < count; i++)
    {
        squelch_format_row_key(i, key, sizeof(key));
        if (!app_ui_panel_add_row_ex(panel, (s16b)i,
                i == selected_index ? TERM_YELLOW : TERM_WHITE, TERM_SLATE,
                TERM_WHITE, '\0', true, i == selected_index, key,
                typevals[i].desc, "Browse"))
        {
            return false;
        }
    }

    if (selected_index >= 0 && selected_index < count)
    {
        char prompt[APP_UI_TEXT_MAX];

        app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
            typevals[selected_index].desc);
        strnfmt(prompt, sizeof(prompt), "Press Enter or %c to open this list.",
            head[0] + selected_index);
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, prompt);
    }

    (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
        "Q opens quality squelch. E opens special item squelch.");
    (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
        "S/L save or load squelch rules. B/G save or load autoinscriptions.");

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Open");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Q", "Quality");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "E", "Special");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "S/L", "Rules");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "B/G", "Autoinsc");
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
        "Esc", "Back");

    return true;
}

static void squelch_collect_object_kind_choices(int typeval, int* choice,
    int* out_count)
{
    int i;
    int num = 0;

    if (out_count)
        *out_count = 0;
    if (!choice)
        return;

    for (i = 1; num < 63 && i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        if (tv_to_type[k_ptr->tval] != typeval)
            continue;
        if (k_ptr->flags3 & TR3_INSTA_ART)
            continue;
        if (!k_ptr->name || !k_ptr->everseen)
            continue;

        choice[num++] = i;
    }

    for (i = 0; i < num; i++)
    {
        for (int j = i; j < num; j++)
        {
            if ((k_info[choice[i]].tval > k_info[choice[j]].tval)
                || ((k_info[choice[i]].tval == k_info[choice[j]].tval)
                    && (k_info[choice[i]].cost > k_info[choice[j]].cost)))
            {
                int temp = choice[i];
                choice[i] = choice[j];
                choice[j] = temp;
            }
        }
    }

    if (out_count)
        *out_count = num;
}

static bool squelch_build_object_kind_scene(app_ui_scene* scene, cptr type_desc,
    const int* choice, int max_num, int active)
{
    app_ui_panel* panel;
    char key[APP_UI_KEY_MAX];

    panel = squelch_begin_browser_scene(scene, type_desc,
        "Pickup rules and autoinscriptions.");
    if (!panel)
        return false;

    if (max_num <= 0)
    {
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "No known objects");
        return app_ui_panel_add_body_line(panel, TERM_L_RED,
            "No known objects of this type.");
    }

    for (int i = 0; i < max_num; i++)
    {
        object_kind* k_ptr = &k_info[choice[i]];
        char label[APP_UI_LABEL_MAX];

        squelch_format_row_key(i, key, sizeof(key));
        strip_name(label, choice[i]);
        if (!app_ui_panel_add_row_ex(panel, (s16b)i,
                i == active ? TERM_YELLOW : TERM_WHITE,
                squelch_pickup_mode_color(k_ptr->squelch), k_ptr->d_attr,
                k_ptr->d_char, true, i == active, key, label,
                squelch_pickup_mode_label(k_ptr->squelch)))
        {
            return false;
        }
    }

    if (active >= 0 && active < max_num)
    {
        object_kind* k_ptr = &k_info[choice[active]];
        char label[APP_UI_LABEL_MAX];
        cptr cur = get_autoinscription(choice[active]);
        char detail[APP_UI_TEXT_MAX];

        strip_name(label, choice[active]);
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, label);
        strnfmt(detail, sizeof(detail), "Pickup rule: %s",
            squelch_pickup_mode_label(k_ptr->squelch));
        (void)app_ui_panel_add_detail_line(panel,
            squelch_pickup_mode_color(k_ptr->squelch), detail);
        strnfmt(detail, sizeof(detail), "Autoinscription: %s",
            cur ? cur : "[None]");
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, detail);
    }

    (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
        "Enter edits autoinscription. Ctrl-N/L/A/S sets pickup rules.");
    (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
        "+/- cycles the current rule. 4/6 pages. Letter hotkeys jump.");

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Inscribe");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "+/-", "Cycle");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "^N/^L", "No/Never");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "^A/^S", "Always/Squelch");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");

    return true;
}

static int squelch_show_object_kind_menu(int typeval, cptr type_desc)
{
    int choice[63];
    int max_num = 0;
    int active = 0;

    squelch_collect_object_kind_choices(typeval, choice, &max_num);
    while (true)
    {
        app_ui_scene scene;
        int ch;

        if (max_num > 0 && active >= max_num)
            active = max_num - 1;
        if (active < 0)
            active = 0;
        if (!squelch_build_object_kind_scene(&scene, type_desc, choice, max_num,
                active)
            || !squelch_present_ui_scene(&scene, &ch))
        {
            log_warn("squelch: failed to present object-kind scene");
            msg_print("Squelch browser unavailable.");
            return 0;
        }

        if (ch == ESCAPE)
            return 1;
        if (max_num <= 0)
        {
            bell("");
            continue;
        }

        if (ch == KTRL('N'))
            k_info[choice[active]].squelch = SQUELCH_NEVER;
        else if (ch == KTRL('L'))
            k_info[choice[active]].squelch = NO_SQUELCH_NEVER_PICKUP;
        else if (ch == KTRL('A'))
            k_info[choice[active]].squelch = NO_SQUELCH_ALWAYS_PICKUP;
        else if (ch == KTRL('S'))
            k_info[choice[active]].squelch = SQUELCH_ALWAYS;
        else if (ch == '-')
        {
            if (k_info[choice[active]].squelch <= SQUELCH_HEAD)
                k_info[choice[active]].squelch = SQUELCH_TAIL;
            else
                k_info[choice[active]].squelch -= 1;
        }
        else if (ch == '+')
        {
            if (k_info[choice[active]].squelch >= SQUELCH_TAIL)
                k_info[choice[active]].squelch = SQUELCH_HEAD;
            else
                k_info[choice[active]].squelch += 1;
        }
        else if (ch == '8')
            squelch_step_selection(&active, -1, max_num);
        else if (ch == '2')
            squelch_step_selection(&active, 1, max_num);
        else if (ch == '4')
            squelch_step_selection(&active, -SQUELCH_PAGE_STEP, max_num);
        else if (ch == '6')
            squelch_step_selection(&active, SQUELCH_PAGE_STEP, max_num);
        else if (ch == '\r' || ch == '\n')
            (void)do_cmd_autoinscribe_item(choice[active]);
        else
        {
            int selected = squelch_choice_index_for_key((char)ch, max_num);

            if (selected >= 0)
                active = selected;
            else
                bell("");
        }
    }
}

/*
 * This subroutine actually handles the squelching menus.
 */

static int do_cmd_squelch_aux(void)
{
    int i, j, temp, num, max_num;
    int tval, sval, squelch;
    int col, row;
    int rows_per_col;
    int col_step;
    int typeval;
    cptr tval_desc2;
    char ch, sq;
    squelch_layout layout;

    int choice[60];

    char ftmp[80];
    SDL_IOStream* fff;
    char buf[80];

    squelch_get_layout(&layout);

    rows_per_col = layout.compact ? MAX(6, layout.term_hgt - 8) : 20;
    col_step = layout.compact ? MAX(20, layout.term_wid / 2) : 30;

    /* Clear screen */
    clear_from(0);

    /*
     * Print all typeval's and their descriptions
     *
     * This uses the above arrays.  I combined a few of the
     * tvals into single typevals.
     */

    for (num = 0; (num < 60) && typevals[num].tval; num++)
    {
        row = (layout.compact ? 1 : 3) + (num % rows_per_col);
        col = col_step * (num / rows_per_col);
        ch = head[num / 26] + num % 26;
        squelch_put_fit(TERM_WHITE, format("[%c] %s", ch, typevals[num].desc),
            row, col, &layout);
    }

    /* Me need to know the maximal possible tval_index */
    max_num = num;

    if (layout.compact)
    {
        int help_row = rows_per_col + 2;

        squelch_put_fit(TERM_WHITE, "a-t item list", help_row, 0, &layout);
        squelch_put_fit(TERM_WHITE, "Q quality  E special  Esc back",
            help_row + 1, 0, &layout);
        squelch_put_fit(TERM_WHITE, "S/L squelch file  B/G autoinsc file",
            help_row + 2, 0, &layout);
        squelch_put_fit(TERM_SLATE, "*quality includes opened chests",
            help_row + 3, 0, &layout);
    }
    else
    {
        prt("Commands:", 3, 30);
        prt("[a-t]: Go to item squelching and autoinscribing sub-menu.", 5, 30);
        prt("Q    : Go to quality squelching sub-menu*.", 6, 30);
        prt("E    : Go to special item squelching sub_menu.", 7, 30);
        prt("S    : Save squelch values to pref file.", 8, 30);
        prt("L    : Load squelch values from pref file.", 9, 30);
        prt("B    : Save autoinscriptions to pref file.", 10, 30);
        prt("G    : Load autoinscriptions from pref file.", 11, 30);

        prt("ESC  : Back to options menu.", 12, 30);
        prt("     :*includes squelching opened chests.", 14, 30);
    }

    /* Choose! */
    if (!get_com("Item Squelching and Autoinscription Main Menu: ", &ch))
        return (0);

    if (ch == 'Q')
    {
        /* Switch to secondary squelching menu */
        do_qual_squelch();
    }

    else if (ch == 'E')
    {
        /* Switch to special item squelching menu */
        do_ego_item_squelch();
    }

    else if (ch == 'S')
    {
        /* Prompt */
        prt("Command: Dump Squelch Info", layout.compact ? layout.status_row : 17,
            layout.compact ? 0 : 30);

        /* Prompt */
        prt("File: ", layout.compact ? layout.footer_row : 18,
            layout.compact ? 0 : 30);

        /* Default filename */
        sprintf(ftmp, "%s.squ", op_ptr->base_name);

        /* Get a filename */
        if (askfor_aux(ftmp, 80))
        {
            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("do_cmd_squelch_aux: failed to build squelch path '%s'", ftmp);
                prt("Failed to resolve squelch file path.  (Hit a key.)",
                    layout.compact ? layout.status_row : 17,
                    layout.compact ? 0 : 30);
                get_com("", &sq);
                return (0);
            }
            else
            {
                /* Drop priv's */
                safe_setuid_drop();

                /* Append to the file */
                fff = sdl_fopen(buf, "a");

                /* Grab priv's */
                safe_setuid_grab();

                /* Test for success */
                if (!fff)
                {
                    log_error("do_cmd_squelch_aux: failed to open squelch file '%s' for append", buf);
                    prt("Failed to open squelch file.  (Hit a key.)",
                        layout.compact ? layout.status_row : 17,
                        layout.compact ? 0 : 30);
                    get_com("", &sq);
                    return (0);
                }

                /* Skip some lines */
                SDL_IOprintf(fff, "\n\n");

                /* Start dumping */
                SDL_IOprintf(fff, "# Squelch bits\n\n");

                /* Dump squelch bits */
                for (i = 1; i < z_info->k_max; i++)
                {
                    tval = k_info[i].tval;
                    sval = k_info[i].sval;
                    squelch = (k_info[i].squelch);

                    /* Dump the squelch info */
                    if (tval || sval)
                        SDL_IOprintf(fff, "Q:%d:%d:%d:%d\n", i, tval, sval, squelch);
                }

                SDL_IOprintf(fff, "\n\n# squelch_level array\n\n");

                for (i = 0; i < SQUELCH_BYTES; i++)
                    SDL_IOprintf(fff, "Q:%d:%d\n", i, squelch_level[i]);

                /* All done */
                SDL_IOprintf(fff, "\n\n\n\n");

                /* Close */
                sdl_fclose(fff);

                /* Ending message */
                prt("Squelch file saved successfully.  (Hit a key.)",
                    layout.compact ? layout.status_row : 17,
                    layout.compact ? 0 : 30);
                get_com("", &sq);
            }
        }
    }

    else if (ch == 'L')
    {
        /* Prompt */
        prt("Command: Load squelch info from file",
            layout.compact ? layout.status_row : 16,
            layout.compact ? 0 : 30);

        /* Prompt */
        prt("File: ", layout.compact ? layout.footer_row : 17,
            layout.compact ? 0 : 30);

        /* Default filename */
        sprintf(ftmp, "%s.squ", op_ptr->base_name);

        /* Ask for a file (or cancel) */
        if (askfor_aux(ftmp, 80))
        {
            /* Process the given filename */
            if (process_pref_file(ftmp))
            {
                /* Mention failure */
                prt("Failed to load squelch file!  (Hit a key.)",
                    layout.compact ? layout.status_row : 17,
                    layout.compact ? 0 : 30);
            }
            else
            {
                /* Mention success */
                prt("Squelch data loaded!  (Hit a key.)",
                    layout.compact ? layout.status_row : 17,
                    layout.compact ? 0 : 30);
            }
            get_com("", &sq);
        }
    }

    if (ch == 'B')
    {
        /* Prompt */
        prt("Command: Dump Autoinscribe Info",
            layout.compact ? layout.status_row : 16,
            layout.compact ? 0 : 30);

        /* Prompt */
        prt("File: ", layout.compact ? layout.footer_row : 17,
            layout.compact ? 0 : 30);

        /* Default filename */
        SDL_strlcpy(ftmp, op_ptr->base_name, sizeof(ftmp));

        /* Get a filename */
        if (askfor_aux(ftmp, 80))
        {
            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("do_cmd_squelch_aux: failed to build autoinscription path '%s'", ftmp);
                prt("Failed to resolve autoinscription file path.  (Hit a key.)",
                    layout.compact ? layout.status_row : 16,
                    layout.compact ? 0 : 30);
                get_com("", &sq);
                return (0);
            }
            else
            {
                /* Drop priv's */
                safe_setuid_drop();

                /* Overwrite the file */
                fff = sdl_fopen(buf, "w");

                /* Grab priv's */
                safe_setuid_grab();

                /* Test for success */
                if (!fff)
                {
                    log_error("do_cmd_squelch_aux: failed to open autoinscription file '%s' for write", buf);
                    prt("Failed to save autoinscriptions.  (Hit a key.)",
                        layout.compact ? layout.status_row : 16,
                        layout.compact ? 0 : 30);
                    get_com("", &sq);
                    return (0);
                }

                if (!inscriptions)
                {
                    log_warn("do_cmd_squelch_aux: no inscriptions available to save");
                    prt("No autoinscriptions to save.  (Hit a key.)",
                        layout.compact ? layout.status_row : 16,
                        layout.compact ? 0 : 30);
                    get_com("", &sq);
                    sdl_fclose(fff);
                    return (0);
                }

                /* Start dumping */
                SDL_IOprintf(fff, "# Format: B:[Item Kind]:[Inscription]\n\n");

                for (i = 0; i < inscriptionsCount; i++)
                {
                    object_kind* k_ptr = &k_info[inscriptions[i].kindIdx];

                    /* Write a comment for the autoinscription*/
                    SDL_IOprintf(fff, "# Autoinscription for %s\n",
                        k_name + k_ptr->name);
                    /* Dump the autoinscribe info */
                    SDL_IOprintf(fff, "B:%d:%s\n\n", inscriptions[i].kindIdx,
                        quark_str(inscriptions[i].inscriptionIdx));
                }

                /* Close */
                sdl_fclose(fff);

                prt("Autoinscribe file saved successfully.  (Hit a key.)",
                    layout.compact ? layout.status_row : 16,
                    layout.compact ? 0 : 30);
                get_com("", &sq);
            }
        }
    }
    else if (ch == 'G')
    {
        /* Prompt */
        prt("Command: Load Autoinscribe info from file",
            layout.compact ? layout.status_row : 16,
            layout.compact ? 0 : 30);

        /* Prompt */
        prt("File: ", layout.compact ? layout.footer_row : 17,
            layout.compact ? 0 : 30);

        /* Default filename */
        SDL_strlcpy(ftmp, op_ptr->base_name, sizeof(ftmp));

        /* Ask for a file (or cancel) */
        if (askfor_aux(ftmp, 80))
        {
            /* Process the given filename */
            if (process_pref_file(ftmp))
            {
                /* Mention failure */
                prt("Failed to load autoinscribe file!  (Hit a key.)",
                    layout.compact ? layout.status_row : 16,
                    layout.compact ? 0 : 30);
            }

            else
            {
                /* Mention success */
                prt("Autoinscribe data loaded!  (Hit a key.)",
                    layout.compact ? layout.status_row : 16,
                    layout.compact ? 0 : 30);
            }
            get_com("", &sq);
        }
    }

    else
    {
        int active = 0;
        int rows_per_page = layout.compact ? MAX(5, layout.term_hgt - 8)
                                           : LINES_PER_COLUMN;
        int col_width = layout.compact ? MAX(20, layout.term_wid / 2) : 30;
        int visible_cols = MAX(1, ((layout.term_wid - 1) / col_width) + 1);
        int page_size = rows_per_page * visible_cols;

        /*
         * One variable is enough, but I used two to make
         * the code more readable -DG
         */
        int old_active = -1;
        int display_all = 1;

        /* Analyze choice */
        num = ch - 'a';

        /* Bail out if choice is illegal */
        if ((num < 0) || (num >= max_num))
            return (0);

        /* Base object type chosen, fill in tval */
        typeval = typevals[num].tval;
        tval_desc2 = typevals[num].desc;

        /* Moved out the sorting code of the while loop */

        /* First sort based on value */
        /* Step 1: Read into choice array */

        for (num = 0, i = 1; (num < 63) && (i < z_info->k_max); i++)
        {
            object_kind* k_ptr = &k_info[i];

            if (tv_to_type[k_ptr->tval] == typeval)
            {
                if (k_ptr->flags3 & (TR3_INSTA_ART))
                    continue;

                /*skip empty objects*/
                if (!k_ptr->name)
                    continue;

                /*haven't seen the item yet*/
                if (!k_ptr->everseen)
                    continue;

                choice[num++] = i;
            }
        }

        max_num = num;

        /* Step 2: Simple bubble sort */
        for (i = 0; i < max_num; i++)
        {
            for (j = i; j < max_num; j++)
            {
                if ((k_info[choice[i]].tval > k_info[choice[j]].tval)
                    || ((k_info[choice[i]].tval == k_info[choice[j]].tval)
                        && (k_info[choice[i]].cost > k_info[choice[j]].cost)))
                {
                    temp = choice[i];
                    choice[i] = choice[j];
                    choice[j] = temp;
                }
            }
        }

        /*** And now we go for k_idx ***/

        /* Clear screen */

        while (true)
        {
            int page_base = (page_size > 0) ? ((active / page_size) * page_size) : 0;
            int page_end = MIN(max_num, page_base + page_size);

            if (display_all)
                clear_from(0);

            /*no objects found*/
            if (!max_num)
            {
                if (display_all)
                    c_put_str(TERM_RED, "No known objects of this type.", 5, 0);
            }

            else
            {
                byte color;

                for (num = 0; num < max_num; num++)
                {
                    object_kind* k_ptr = &k_info[choice[num]];
                    cptr curStr;
                    int display_idx;
                    int text_col;

                    /* Reduce flickering */
                    if (!display_all && num != active && num != old_active)
                        continue;

                    if (num < page_base || num >= page_end)
                        continue;

                    /* Prepare it */
                    display_idx = num - page_base;
                    row = 5 + (display_idx % rows_per_page);
                    col = col_width * (display_idx / rows_per_page);
                    text_col = col + 6;
                    ch = head[num / 26] + (num % 26);

                    /* Acquire the "name" of object "i" */
                    strip_name(buf, choice[num]);

                    /*Print out the autoinscription*/
                    if (num == active)
                    {
                        curStr = get_autoinscription(choice[active]);
                        squelch_put_fit(TERM_WHITE,
                            format(layout.compact
                                    ? "Autoinscription: %s"
                                    : "Current Autoinscription: %-40s",
                                curStr ? curStr : "[None]"),
                            4, layout.compact ? 0 : 39, &layout);
                    }

                    /*get the color and character*/

                    /*
                     * Items player wants to squelch when they walk over them
                     */
                    if (k_ptr->squelch == SQUELCH_ALWAYS)
                    {
                        /*use a 'S' for always squelch*/
                        sq = 'S';
                        color = TERM_L_RED;
                    }

                    /*
                     * Items player doesn't want to squelch, but doesn't want to
                     * auto-destroy either
                     */

                    else if (k_ptr->squelch == NO_SQUELCH_NEVER_PICKUP)
                    {
                        /*use a 'S' for always squelch*/
                        sq = 'L';
                        color = TERM_L_GREEN;
                    }

                    /*
                     * Items player always wants to pickup, regardless of
                     * other options.
                     */

                    else if (k_ptr->squelch == NO_SQUELCH_ALWAYS_PICKUP)
                    {
                        /*use a 'S' for always squelch*/
                        sq = 'A';
                        color = TERM_L_UMBER;
                    }

                    /* Never Squelch */
                    else
                    {
                        /*use a 'S' for never squelch*/
                        sq = 'N';
                        color = TERM_L_BLUE;
                    }

                    /* Print it */
                    squelch_put_fit(((num == active) ? TERM_YELLOW : TERM_WHITE),
                        format("%c)'%c'", ch, sq), row, col, &layout);
                    {
                        char clipped[160];
                        size_t clip_size = (size_t)MAX(1, col_width - 7) + 1;

                        if (clip_size > sizeof(clipped))
                            clip_size = sizeof(clipped);
                        SDL_strlcpy(clipped, buf, clip_size);
                        squelch_put_fit(color, clipped, row, text_col,
                            &layout);
                    }
                }
            }

            /*header text*/
            if (display_all)
            {
                if (layout.compact)
                {
                    squelch_put_fit(TERM_L_BLUE, "CTRL-N none  CTRL-S squelch",
                        1, 0, &layout);
                    squelch_put_fit(TERM_L_GREEN, "CTRL-L never  CTRL-A always",
                        2, 0, &layout);
                    squelch_put_fit(TERM_WHITE,
                        "Enter inscribe  +/- toggle  8/2 move  4/6 page",
                        3, 0, &layout);
                    squelch_put_fit(TERM_SLATE,
                        format("Page %d/%d", (page_base / page_size) + 1,
                            MAX(1, (max_num + page_size - 1) / page_size)),
                        layout.footer_row, 0, &layout);
                }
                else
                {
                    c_put_str(TERM_L_BLUE,
                        "CTRL-N: No Squelch - defer to Never_pickup option", 1, 0);
                    prt("Esc   : Return", 1, 55);
                    c_put_str(TERM_L_GREEN, "CTRL-L: Never Pickup", 2, 0);
                    c_put_str(TERM_L_UMBER, "CTRL-A: Always Pickup", 2, 22);
                    prt("+/-     Toggle Selection", 2, 55);
                    c_put_str(TERM_L_RED, "CTRL-S: Squelch", 3, 0);
                    prt("Use direction keys to Navigate list; or enter a letter", 3,
                        22);
                    /*header text*/
                    c_put_str(TERM_WHITE, "Enter: New autoinscription", 4, 10);
                }
            }

            display_all = 0;
            old_active = -1;

            /* Choose! */
            if (!get_com(format("%s : Command? ", tval_desc2), &ch))
                return (1);

            /* Bugfix - Avoid crashes */
            if (!max_num)
                continue;

            /*Switch to Never Squelch Option*/
            if (ch == KTRL('N'))
            {
                k_info[choice[active]].squelch = SQUELCH_NEVER;
            }

            /*Switch to Never Pickup Option*/
            else if (ch == KTRL('L'))
            {
                k_info[choice[active]].squelch = NO_SQUELCH_NEVER_PICKUP;
            }

            /*Switch to Never Pickup Option*/
            else if (ch == KTRL('A'))
            {
                k_info[choice[active]].squelch = NO_SQUELCH_ALWAYS_PICKUP;
            }

            /*Switch to Squelch Option*/
            else if (ch == KTRL('S'))
            {
                k_info[choice[active]].squelch = SQUELCH_ALWAYS;
            }

            /*toggle choice down one*/
            else if (ch == '-')
            {
                /*boundry control*/
                if (k_info[choice[active]].squelch <= SQUELCH_HEAD)
                {
                    k_info[choice[active]].squelch = SQUELCH_TAIL;
                }

                else
                    k_info[choice[active]].squelch -= 1;
            }

            /*toggle choice up one*/
            else if (ch == '+')
            {
                /*boundry control*/
                if (k_info[choice[active]].squelch >= SQUELCH_TAIL)
                {
                    k_info[choice[active]].squelch = SQUELCH_HEAD;
                }

                else
                    k_info[choice[active]].squelch += 1;
            }

            else if (ch == '8')
            {
                /* Redraw the current active */
                old_active = active;

                /*move up one row*/
                active -= 1;

                /*boundry control*/
                if (active < 0)
                    active = max_num - 1;
            }

            else if (ch == '2')
            {
                /* Redraw the current active */
                old_active = active;

                /*move down one row*/
                active += 1;

                /*boundry control*/
                if (active > (max_num - 1))
                    active = 0;
            }

            else if (ch == '6')
            {
                /*move one column to right, but check first*/
                if ((active + rows_per_page) <= max_num - 1)
                {
                    /* Redraw the current active */
                    old_active = active;

                    active += rows_per_page;
                }

                else
                    bell("");
            }

            else if (ch == 13)
            {
                do_cmd_autoinscribe_item(choice[active]);
            }

            else if (ch == '4')
            {
                /*move one column to left, but check first*/
                if ((active - rows_per_page) >= 0)
                {
                    /* Redraw the current active */
                    old_active = active;

                    active -= rows_per_page;
                }

                else
                    bell("");
            }

            else
            {
                /*save the old choice*/
                int old_num = active;

                /* Analyze choice */
                active = -1;
                if ((ch >= head[0]) && (ch < head[0] + 26))
                    active = ch - head[0];
                if ((ch >= head[1]) && (ch < head[1] + 26))
                    active = ch - head[1] + 26;
                if ((ch >= head[2]) && (ch < head[2] + 17))
                    active = ch - head[2] + 52;

                /* Bail out if choice is "illegal" */
                if ((active < 0) || (active >= max_num))
                    active = old_num;
                else
                    old_active = old_num;
            }
        }
    }

    /* And return successful */
    return (1);
}

/*
 * This command handles the secondary squelch menu.
 */
static void do_qual_squelch(void)
{
    int i, num, max_num, index;
    int col, row;
    int rows_per_col;
    int col_step;
    char ch;
    squelch_layout layout;
/* the index for the rings*/
#define RING_INDEX 18
/* the index for the amulets*/
#define AMULET_INDEX 19
    /* - open chest TVAL in defines*/

    const char squelch_str[] = "NCVGWAO";

    int old_index = -1;
    int display_all = 1;

    squelch_get_layout(&layout);
    rows_per_col = layout.compact ? MAX(6, layout.term_hgt - 8) : 22;
    col_step = layout.compact ? MAX(20, layout.term_wid / 2) : 30;

    index = 0;
    while (1)
    {
        /* Clear screen */
        if (display_all)
            clear_from(0);

        /* Print all tval's and their descriptions */
        for (num = 0; (num < 60) && tvals[num].tval; num++)
        {
            /* Reduce flickering */
            if (!display_all && num != index && num != old_index)
                continue;

            row = 2 + (num % rows_per_col);
            col = col_step * (num / rows_per_col);
            squelch_put_fit(TERM_WHITE,
                format("(%c): %s", squelch_str[squelch_level[num]],
                    tvals[num].desc),
                row, col, &layout);
        }

        if (display_all)
        {
            /* Print out the rest of the screen */
            prt("Secondary Squelching Menu", 0, 0);

            if (layout.compact)
            {
                squelch_put_fit(TERM_WHITE,
                    "n/c/v/g/w/a/o set current  N/C/V/G/W/A/O set all",
                    layout.status_row, 0, &layout);
                squelch_put_fit(TERM_WHITE,
                    "8/2 move  4/6 cycle  Esc back",
                    layout.footer_row, 0, &layout);
            }
            else
            {
                prt("Legend:", 2, 30);

                prt("N  : Squelch Nothing", 4, 30);
                prt("C  : Squelch Cursed Items", 5, 30);
                prt("V  : Squelch Average and Below", 6, 30);
                prt("G  : Squelch Good (Strong Pseudo_ID and Identify)", 7, 30);
                prt("W  : Squelch Good (Weak Pseudo-ID)", 8, 30);
                prt("A  : Squelch All but Artefacts", 9, 30);
                prt("O  : Squelch Chests After Opening", 10, 30);

                prt("Commands:", 12, 30);
                prt("Arrows: Move and adjust settings", 14, 30);
                prt("ncvgao : Change a single setting", 15, 30);
                prt("NCVGWAO : Change all allowable settings", 16, 30);
                prt("ESC   : Exit Secondary Menu", 17, 30);
                prt("Rings:   N, C or A only", 19, 30);
                prt("Amulets: N, C or A only", 20, 30);
                prt("Opened Chests: N or O only", 21, 30);
            }
        }

        display_all = 0;
        old_index = -1;

        /* Need to know maximum index */
        max_num = num;

        /* Place the cursor */
        move_cursor(2 + (index % rows_per_col), 1 + ((index / rows_per_col) * col_step));

        /* Get a key */
        ch = squelch_wait_key();

        /* Analyze */
        switch (ch)
        {
        case ESCAPE:
        {
            return;
        }

        case 'n':
        {
            squelch_level[index] = SQUELCH_NONE;
            break;
        }
        case 'N':
        {
            for (i = 0; i < SQUELCH_BYTES; i++)
            {
                squelch_level[i] = SQUELCH_NONE;
            }
            display_all = 1;
            break;
        }

        case 'c':
        {
            if (index != CHEST_INDEX)
                squelch_level[index] = SQUELCH_CURSED;
            break;
        }

        case 'C':
        {
            for (i = 0; i < SQUELCH_BYTES; i++)
            {
                /*HACK - don't check chests as cursed*/
                if (i != CHEST_INDEX)
                    squelch_level[i] = SQUELCH_CURSED;
            }
            display_all = 1;
            break;
        }

        case 'v':
        {
            if ((index != CHEST_INDEX) && (index != AMULET_INDEX)
                && (index != RING_INDEX))
                squelch_level[index] = SQUELCH_AVERAGE;
            break;
        }

        case 'V':
        {
            for (i = 0; i < SQUELCH_BYTES; i++)
            {
                if ((i != CHEST_INDEX) && (i != AMULET_INDEX)
                    && (i != RING_INDEX))
                    squelch_level[i] = SQUELCH_AVERAGE;
            }
            display_all = 1;
            break;
        }

        case 'g':
        {
            if ((index != CHEST_INDEX) && (index != AMULET_INDEX)
                && (index != RING_INDEX))
                squelch_level[index] = SQUELCH_GOOD_STRONG;
            break;
        }

        case 'G':
        {
            for (i = 0; i < SQUELCH_BYTES; i++)
            {
                if ((i != CHEST_INDEX) && (i != AMULET_INDEX)
                    && (i != RING_INDEX))
                    squelch_level[i] = SQUELCH_GOOD_STRONG;
            }
            display_all = 1;
            break;
        }

        case 'w':
        {
            if ((index != CHEST_INDEX) && (index != AMULET_INDEX)
                && (index != RING_INDEX))
                squelch_level[index] = SQUELCH_GOOD_WEAK;
            break;
        }
        case 'W':
        {
            for (i = 0; i < SQUELCH_BYTES; i++)
            {
                if ((i != CHEST_INDEX) && (i != AMULET_INDEX)
                    && (i != RING_INDEX))
                    squelch_level[i] = SQUELCH_GOOD_WEAK;
            }
            display_all = 1;
            break;
        }

        case 'a':
        {
            if (index != CHEST_INDEX)
                squelch_level[index] = SQUELCH_ALL;
            break;
        }

        case 'A':
        {
            for (i = 0; i < SQUELCH_BYTES; i++)
            {
                /*HACK - don't check chests as destroy all*/
                if (i != CHEST_INDEX)
                    squelch_level[i] = SQUELCH_ALL;
            }
            display_all = 1;
            break;
        }

        /*hack "O" works from anywhere only on open chests*/
        case 'O':
        case 'o':
        {
            squelch_level[(CHEST_INDEX)] = SQUELCH_OPENED_CHESTS;
            display_all = 1;
            break;
        }
        case '-':
        case '8':
        {
            old_index = index;

            index = (max_num + index - 1) % max_num;
            break;
        }

        case ' ':
        case '\n':
        case '\r':
        case '2':
        {
            old_index = index;

            index = (index + 1) % max_num;
            break;
        }

        case '4':
        {
            /*HACK - only allowable  options to be toggled through*/

            /*first do the rings and amulets*/
            if ((index == AMULET_INDEX) || (index == RING_INDEX))
            {
                /*amulets and rings can only be none, cursed, and all but
                 * artefact*/
                if (squelch_level[index] > 1)
                    squelch_level[index] = SQUELCH_CURSED;
                else
                    squelch_level[index] = SQUELCH_NONE;
                break;
            }

            /* now do the chests*/
            else if (index == CHEST_INDEX)
            {
                squelch_level[index] = SQUELCH_NONE;
                break;
            }

            /*then toggle all else*/
            else
            {
                if (squelch_level[index] >= SQUELCH_ALL)
                    squelch_level[index] = SQUELCH_GOOD_WEAK;
                else if (squelch_level[index] > 0)
                    squelch_level[index] -= 1;
                else
                    squelch_level[index] = 0;
                break;
            }
        }

        case '6':
        {
            /*HACK - only allowable  options to be toggled through*/

            /*first do the rings and amulets*/
            if ((index == AMULET_INDEX) || (index == RING_INDEX))
            {
                /*amulets and rings can only be none, cursed, and all but
                 * artefact*/
                if (squelch_level[index] > 0)
                    squelch_level[index] = SQUELCH_ALL;
                else
                    squelch_level[index] = SQUELCH_CURSED;
                break;
            }

            /* now do the chests*/
            else if (index == CHEST_INDEX)
            {
                squelch_level[index] = SQUELCH_OPENED_CHESTS;
                break;
            }

            /*then toggle all else*/
            else
            {
                if (squelch_level[index] >= SQUELCH_ALL)
                    squelch_level[index] = SQUELCH_ALL;
                else
                    squelch_level[index] += 1;
                break;
            }
        }

        default:
        {
            bell("");
            break;
        }
        }
    }

    return;
}

static int squelch_quality_count(void)
{
    int count = 0;

    while (tvals[count].tval)
        count++;

    return count;
}

static const char* squelch_quality_restriction(int index)
{
    if (index == CHEST_INDEX)
        return "Opened chests only allow Nothing or Opened chests.";
    if (index == 18)
        return "Rings only allow Nothing, Cursed, or All.";
    if (index == 19)
        return "Amulets only allow Nothing, Cursed, or All.";

    return "Use 4/6 to cycle the selected rule. Uppercase sets all.";
}

static bool squelch_build_quality_scene(app_ui_scene* scene, int index)
{
    app_ui_panel* panel;
    int count = squelch_quality_count();

    panel = squelch_begin_browser_scene(scene, "Quality Squelch",
        "Rules applied on identify and pseudo-identify.");
    if (!panel)
        return false;

    for (int i = 0; i < count; i++)
    {
        byte level = squelch_level[i];
        byte color = squelch_quality_color(level);

        if (!app_ui_panel_add_row_ex(panel, (s16b)i,
                i == index ? TERM_YELLOW : TERM_WHITE, color, color,
                squelch_quality_letter(level), true, i == index, "",
                tvals[i].desc, squelch_quality_label(level)))
        {
            return false;
        }
    }

    if (index >= 0 && index < count)
    {
        char detail[APP_UI_TEXT_MAX];

        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, tvals[index].desc);
        strnfmt(detail, sizeof(detail), "Current rule: %s",
            squelch_quality_label(squelch_level[index]));
        (void)app_ui_panel_add_detail_line(panel,
            squelch_quality_color(squelch_level[index]), detail);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            squelch_quality_restriction(index));
    }

    (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
        "n/c/v/g/w/a/o sets the current row. N/C/V/G/W/A/O applies broadly.");

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "4/6", "Cycle");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "n..o", "Set");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "N..O", "Set All");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "Esc", "Back");

    return true;
}

static void squelch_run_quality_browser(void)
{
    int index = 0;
    int count = squelch_quality_count();

    while (true)
    {
        app_ui_scene scene;
        int ch;

        if (!squelch_build_quality_scene(&scene, index)
            || !squelch_present_ui_scene(&scene, &ch))
        {
            log_warn("squelch: failed to present quality scene");
            msg_print("Quality squelch unavailable.");
            return;
        }

        switch (ch)
        {
        case ESCAPE:
            return;
        case 'n':
            squelch_quality_apply_single(index, SQUELCH_NONE);
            break;
        case 'N':
            squelch_quality_apply_all(SQUELCH_NONE);
            break;
        case 'c':
            squelch_quality_apply_single(index, SQUELCH_CURSED);
            break;
        case 'C':
            squelch_quality_apply_all(SQUELCH_CURSED);
            break;
        case 'v':
            squelch_quality_apply_single(index, SQUELCH_AVERAGE);
            break;
        case 'V':
            squelch_quality_apply_all(SQUELCH_AVERAGE);
            break;
        case 'g':
            squelch_quality_apply_single(index, SQUELCH_GOOD_STRONG);
            break;
        case 'G':
            squelch_quality_apply_all(SQUELCH_GOOD_STRONG);
            break;
        case 'w':
            squelch_quality_apply_single(index, SQUELCH_GOOD_WEAK);
            break;
        case 'W':
            squelch_quality_apply_all(SQUELCH_GOOD_WEAK);
            break;
        case 'a':
            squelch_quality_apply_single(index, SQUELCH_ALL);
            break;
        case 'A':
            squelch_quality_apply_all(SQUELCH_ALL);
            break;
        case 'o':
        case 'O':
            squelch_quality_apply_all(SQUELCH_OPENED_CHESTS);
            break;
        case '-':
        case '8':
            squelch_step_selection(&index, -1, count);
            break;
        case ' ':
        case '\r':
        case '\n':
        case '2':
            squelch_step_selection(&index, 1, count);
            break;
        case '4':
            if (index == CHEST_INDEX)
            {
                squelch_level[index] = SQUELCH_NONE;
            }
            else if (index == 18 || index == 19)
            {
                squelch_level[index] = (squelch_level[index] > 1)
                    ? SQUELCH_CURSED
                    : SQUELCH_NONE;
            }
            else if (squelch_level[index] >= SQUELCH_ALL)
            {
                squelch_level[index] = SQUELCH_GOOD_WEAK;
            }
            else if (squelch_level[index] > 0)
            {
                squelch_level[index] -= 1;
            }
            break;
        case '6':
            if (index == CHEST_INDEX)
            {
                squelch_level[index] = SQUELCH_OPENED_CHESTS;
            }
            else if (index == 18 || index == 19)
            {
                squelch_level[index] = (squelch_level[index] > 0)
                    ? SQUELCH_ALL
                    : SQUELCH_CURSED;
            }
            else if (squelch_level[index] < SQUELCH_ALL)
            {
                squelch_level[index] += 1;
            }
            break;
        default:
            bell("");
            break;
        }
    }
}

#define MAX_EGO_ROWS 19

static tval_desc raw_tvals[] = {
    { TV_SKELETON, "Skeletons" },
    { TV_METAL, "Pieces of Metal" },
    { TV_CHEST, "Chests" },
    { TV_ARROW, "Arrows" },
    { TV_BOW, "Bows" },
    { TV_DIGGING, "Diggers" },
    { TV_HAFTED, "Blunt Weapons" },
    { TV_POLEARM, "Axes & Polearms" },
    { TV_SWORD, "Swords" },
    { TV_BOOTS, "Boots" },
    { TV_GLOVES, "Gloves" },
    { TV_HELM, "Helmets" },
    { TV_CROWN, "Crowns" },
    { TV_SHIELD, "Shields" },
    { TV_CLOAK, "Cloaks" },
    { TV_SOFT_ARMOR, "Soft Armor" },
    { TV_MAIL, "Mail" },
    { TV_LIGHT, "Lights" },
    { TV_AMULET, "Amulets" },
    { TV_RING, "Rings" },
    { TV_STAFF, "Staves" },
    { TV_GEM, "Gems" },
    { TV_HORN, "Horns" },
    { TV_POTION, "Potions" },
    { TV_FLASK, "Flasks" },
    { TV_FOOD, "Food" },
};

#define NUM_RAW_TVALS (sizeof(raw_tvals) / sizeof(raw_tvals[0]))

/*
 * Skip common prefixes in special item names.
 */
static const char* strip_ego_name(const char* name)
{
    if (prefix(name, "of the "))
        return name + 7;
    if (prefix(name, "of "))
        return name + 3;
    return name;
}

/*
 * Utility function used to find/sort tval names.
 */
static int tval_comp_func(const void* a_ptr, const void* b_ptr)
{
    int a = ((tval_desc*)a_ptr)->tval;
    int b = ((tval_desc*)b_ptr)->tval;
    return a - b;
}

/*
 * Display an special item type on the screen.
 */
static void display_ego_item(ego_item_type* e_ptr, int y, int x, bool active)
{
    int tval_table[EGO_TVALS_MAX], i, n = 0;
    char buf[100];
    const char *str, *name;

    /* Fast appending, and easier to code ;) */
    editing_buffer ebuf, *ebuf_ptr = &ebuf;

    /* Copy the valid tvals of this special item type */
    for (i = 0; i < EGO_TVALS_MAX; i++)
    {
        /* Ignore "empty" entries */
        if (e_ptr->tval[i] < 1)
            continue;

        tval_table[n++] = e_ptr->tval[i];
    }

    /* Hack - Sort the tvals using bubbles */
    for (i = 0; i < n; i++)
    {
        int j;

        for (j = i + 1; j < n; j++)
        {
            if (tval_table[i] > tval_table[j])
            {
                int temp = tval_table[i];
                tval_table[i] = tval_table[j];
                tval_table[j] = temp;
            }
        }
    }

    /* Initialize the editing_buffer structure */
    editing_buffer_init(ebuf_ptr, "[ ] ", 100);

    /* Concatenate the tval' names */
    for (i = 0; i < n; i++)
    {
        /* Fast searching */
        tval_desc key, *result;

        /* Find the tval's name using binary search */
        key.tval = tval_table[i];
        key.desc = NULL;
        result = bsearch(&key, raw_tvals, NUM_RAW_TVALS, sizeof(raw_tvals[0]),
            tval_comp_func);
        if (result)
            name = result->desc;
        /* Paranoia */
        else
            name = "????";

        /* Append the respective separator first, if any */
        if (i > 0)
        {
            if (i < n - 1)
                editing_buffer_put_str(ebuf_ptr, ", ", -1);
            else
                editing_buffer_put_str(ebuf_ptr, " and ", -1);
        }
        /* Append the name */
        editing_buffer_put_str(ebuf_ptr, name, -1);
    }

    /* Append one  extra space */
    editing_buffer_put_chr(ebuf_ptr, ' ');

    /* Hack - Find common special item name' prefixes */
    name = e_name + e_ptr->name;
    str = strip_ego_name(name);

    /* Append the prefix to the buffer, if any */
    editing_buffer_put_str(ebuf_ptr, name, str - name);

    /* Get the buffer */
    editing_buffer_get_all(ebuf_ptr, buf, sizeof(buf));

    /* Free resources */
    editing_buffer_destroy(ebuf_ptr);

    /* Show the buffer */
    c_put_str(active ? TERM_YELLOW : TERM_WHITE, buf, y, x);

    if (e_ptr->squelch)
        c_put_str(TERM_L_RED, "*", y, x + 1);

    /* Show the stripped special item name with another colour */
    c_put_str(
        e_ptr->squelch ? TERM_L_RED : TERM_L_BLUE, str, y, x + strlen(buf));
}

/*
 * Utility function used for sorting an array of special item indices by
 * special item name.
 */
static int ego_comp_func(const void* a_ptr, const void* b_ptr)
{
    s16b a = *(s16b*)a_ptr;
    s16b b = *(s16b*)b_ptr;

    /* Note the removal of common prefixes */
    return strcmp(strip_ego_name(e_name + e_info[a].name),
        strip_ego_name(e_name + e_info[b].name));
}

/*
 * Handle the squelching of special items.
 */
static int do_ego_item_squelch(void)
{
    int i, idx, max_num = 0, first, last, active, old_active;
    int rows_per_page;
    bool display_all;
    char ch, *msg;
    ego_item_type* e_ptr;
    s16b* choice;
    squelch_layout layout;

    /* Hack - Used to sort the tval table for the first time */
    static bool sort_tvals = true;

    /* Sort the tval table if needed */
    if (sort_tvals)
    {
        qsort(raw_tvals, NUM_RAW_TVALS, sizeof(raw_tvals[0]), tval_comp_func);
        sort_tvals = false;
    }

    /* Alloc the array of ego indices */
    choice = mem_alloc_array(alloc_ego_size, s16b);
    squelch_get_layout(&layout);
    rows_per_page = layout.compact ? MAX(5, layout.term_hgt - 5) : MAX_EGO_ROWS;

    /* Get the valid special items */
    for (i = 0; i < alloc_ego_size; i++)
    {
        idx = alloc_ego_table[i].index;

        e_ptr = &e_info[idx];

        /* Only valid known special items allowed */
        if (!e_ptr->name || !e_ptr->everseen)
            continue;

        /* Append the index */
        choice[max_num++] = idx;
    }

    /* Quickly sort the array by special item name */
    qsort(choice, max_num, sizeof(choice[0]), ego_comp_func);

    /* Display the whole screen */
    display_all = true;
    active = 0;
    old_active = -1;

    /* Determine the first special item to display in the screen */
    first = 0;

    /* Determine the last special item to display in the screen */
    /* Note that if "max_num" is 0, "last" will be -1 */
    last = MIN(first + rows_per_page - 1, max_num - 1);

    while (1)
    {
        if (display_all)
        {
            /* Clear the screen */
            clear_from(0);

            /* Header */
            c_put_str(TERM_WHITE, "[ ]:", 1, 0);
            c_put_str(TERM_L_RED, "*", 1, 1);
            c_put_str(TERM_L_RED, "Squelch", 1, 5);

            c_put_str(TERM_WHITE, "[ ]:", 1, 15);
            c_put_str(TERM_L_BLUE, "No squelch", 1, 20);

            /* No special items */
            msg = "You have not seen any special items yet.";
            if (max_num < 1)
                c_put_str(TERM_RED, msg, 3, 0);

            /* Hack - Make the UI more friendly if needed */
            if (first > 0)
                c_put_str(TERM_WHITE, "-more-", 2, 4);
            if (last < max_num - 1)
                c_put_str(TERM_WHITE, "-more-", layout.footer_row - 1, 4);

            /* Page foot */
            msg = layout.compact
                ? "2/8 move 3/9 page 1/7 ends  Enter toggle"
                : "Navigation: 2, 8, 3, 9, 1, 7 or movement keys - Shorcut: First letter";

            c_put_str(TERM_WHITE, msg, layout.footer_row, 0);
        }

        /* Only show a portion of the list */
        for (i = first; i <= last; i++)
        {
            /* Avoid flickering */
            if (!display_all && (i != active) && (i != old_active))
                continue;

            e_ptr = &e_info[choice[i]];

            /* Show the entry */
            display_ego_item(e_ptr, i - first + 3, 0, i == active);
        }

        /* Reset some flags */
        display_all = false;
        old_active = -1;

        /* Get a command */
        msg = layout.compact
            ? "Command? (Enter toggles, Esc returns) "
            : "Command? (SPACE, RET: Toggle selection - ESC: Return) ";
        if (!get_com(msg, &ch))
            break;

        /* Avoid crash */
        if (max_num < 1)
            continue;

        /* Get the selected special item type */
        e_ptr = &e_info[choice[active]];

        /* Process the command */
        switch (ch)
        {
        case ' ':
        case '\r':
        case '\n':
        {
            /* Toggle the "squelch" flag */
            e_ptr->squelch = !e_ptr->squelch;
            break;
        }
        case '2':
        {
            /* Advance a position */
            old_active = active;
            active = MIN(active + 1, max_num - 1);
            if (active > last)
            {
                ++first;
                ++last;
                /* Redraw all */
                display_all = 1;
            }
            break;
        }
        case '8':
        {
            /* Retrocede a position */
            old_active = active;
            active = MAX(active - 1, 0);
            if (active < first)
            {
                --first;
                --last;

                /* Redraw all */
                display_all = 1;
            }

            break;
        }

        case '3':
        {
            /* Advance one "screen" */
            active = MIN(active + rows_per_page, max_num - 1);
            last = MIN(last + rows_per_page, max_num - 1);
            first = MAX(last - rows_per_page + 1, 0);

            /* Redraw all */
            display_all = 1;
            break;
        }

        case '9':
        {
            /* Retrocede one "screen" */
            active = MAX(active - rows_per_page, 0);
            first = MAX(first - rows_per_page, 0);
            last = MIN(first + rows_per_page - 1, max_num - 1);

            /* Redraw all */
            display_all = 1;
            break;
        }
        case '1':
        {
            /* Go the last special item */
            active = last = max_num - 1;
            first = MAX(last - rows_per_page + 1, 0);

            /* Redraw all */
            display_all = 1;
            break;
        }

        case '7':
        {
            /* Go to the first special item */
            active = first = 0;
            last = MIN(first + rows_per_page - 1, max_num - 1);

            /* Redraw all */
            display_all = 1;
            break;
        }

        /* Compare with the first letter of special item names */
        default:
        {
            const char* name;

            /* Ignore strange characters */
            if (!isgraph((unsigned char)ch))
                break;

            /* Check for seen special items */
            for (i = 0; i < max_num; i++)
            {
                /* Get the special item */
                e_ptr = &e_info[choice[i]];

                /* Get its name */
                name = e_name + e_ptr->name;

                /* Strip the name */
                name = strip_ego_name(name);

                /* Compare first letter, case insen. */
                if (toupper((unsigned char)name[0])
                    == toupper((unsigned char)ch))
                    break;
            }

            /* Found one? */
            if (i >= max_num)
                break;

            /* Jump there */
            active = i;
            /* Adjust visual bounds */
            /* Try to put the found ego in the first row */
            last = MIN(active + rows_per_page - 1, max_num - 1);
            first = MAX(last - rows_per_page + 1, 0);
            /* Redraw all */
            display_all = 1;
            break;
        }
        }
    }
    /* Free resources */
    mem_free_null(choice);
    return 0;
}

static void squelch_ensure_raw_tvals_sorted(void)
{
    static bool sorted = false;

    if (sorted)
        return;

    qsort(raw_tvals, NUM_RAW_TVALS, sizeof(raw_tvals[0]), tval_comp_func);
    sorted = true;
}

static void squelch_build_ego_type_list(const ego_item_type* e_ptr, char* buf,
    size_t buf_size)
{
    int tval_table[EGO_TVALS_MAX];
    int count = 0;

    if (!buf || buf_size == 0)
        return;

    buf[0] = '\0';
    if (!e_ptr)
        return;

    for (int i = 0; i < EGO_TVALS_MAX; i++)
    {
        if (e_ptr->tval[i] < 1)
            continue;
        tval_table[count++] = e_ptr->tval[i];
    }

    for (int i = 0; i < count; i++)
    {
        for (int j = i + 1; j < count; j++)
        {
            if (tval_table[i] > tval_table[j])
            {
                int temp = tval_table[i];
                tval_table[i] = tval_table[j];
                tval_table[j] = temp;
            }
        }
    }

    for (int i = 0; i < count; i++)
    {
        tval_desc key;
        tval_desc* result;
        const char* name;

        key.tval = tval_table[i];
        key.desc = NULL;
        result = bsearch(&key, raw_tvals, NUM_RAW_TVALS, sizeof(raw_tvals[0]),
            tval_comp_func);
        name = result ? result->desc : "????";

        if (i > 0)
            SDL_strlcat(buf, (i < count - 1) ? ", " : " and ", buf_size);
        SDL_strlcat(buf, name, buf_size);
    }
}

static bool squelch_build_ego_scene(app_ui_scene* scene, const s16b* choice,
    int max_num, int active)
{
    app_ui_panel* panel;

    panel = squelch_begin_browser_scene(scene, "Special Item Squelch",
        "Toggle known ego-item pickup rules.");
    if (!panel)
        return false;

    if (max_num <= 0)
    {
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "No known special items");
        return app_ui_panel_add_body_line(panel, TERM_L_RED,
            "You have not seen any special items yet.");
    }

    for (int i = 0; i < max_num; i++)
    {
        ego_item_type* e_ptr = &e_info[choice[i]];
        const char* full_name = e_name + e_ptr->name;
        const char* name = strip_ego_name(full_name);
        char meta[APP_UI_META_MAX];

        squelch_build_ego_type_list(e_ptr, meta, sizeof(meta));
        if (!app_ui_panel_add_row_ex(panel, (s16b)i,
                i == active ? TERM_YELLOW
                            : (e_ptr->squelch ? TERM_L_RED : TERM_WHITE),
                TERM_SLATE, e_ptr->squelch ? TERM_L_RED : TERM_L_BLUE,
                e_ptr->squelch ? '*' : '+', true, i == active, "", name,
                meta))
        {
            return false;
        }
    }

    if (active >= 0 && active < max_num)
    {
        ego_item_type* e_ptr = &e_info[choice[active]];
        const char* full_name = e_name + e_ptr->name;
        const char* name = strip_ego_name(full_name);
        char detail[APP_UI_TEXT_MAX];

        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, name);
        squelch_build_ego_type_list(e_ptr, detail, sizeof(detail));
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, detail);
        (void)app_ui_panel_add_detail_line(panel,
            e_ptr->squelch ? TERM_L_RED : TERM_L_BLUE,
            e_ptr->squelch ? "Currently squelched." : "Currently kept.");
    }

    (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
        "Enter or Space toggles. 3/9 pages. 1/7 jumps to ends.");
    (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
        "Typing a letter jumps to the first matching special item.");

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Toggle");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "3/9", "Page");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "1/7", "Ends");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");

    return true;
}

static void squelch_run_ego_browser(void)
{
    int i;
    int idx;
    int max_num = 0;
    int active = 0;
    s16b* choice;

    squelch_ensure_raw_tvals_sorted();
    choice = mem_alloc_array(alloc_ego_size, s16b);
    if (!choice)
        return;

    for (i = 0; i < alloc_ego_size; i++)
    {
        ego_item_type* e_ptr;

        idx = alloc_ego_table[i].index;
        e_ptr = &e_info[idx];
        if (!e_ptr->name || !e_ptr->everseen)
            continue;
        choice[max_num++] = idx;
    }

    qsort(choice, max_num, sizeof(choice[0]), ego_comp_func);
    while (true)
    {
        app_ui_scene scene;
        int ch;

        if (max_num > 0 && active >= max_num)
            active = max_num - 1;
        if (active < 0)
            active = 0;
        if (!squelch_build_ego_scene(&scene, choice, max_num, active)
            || !squelch_present_ui_scene(&scene, &ch))
        {
            log_warn("squelch: failed to present ego-item scene");
            msg_print("Special item squelch unavailable.");
            break;
        }

        if (ch == ESCAPE)
            break;
        if (max_num <= 0)
        {
            bell("");
            continue;
        }

        if (ch == ' ' || ch == '\r' || ch == '\n')
        {
            e_info[choice[active]].squelch = !e_info[choice[active]].squelch;
        }
        else if (ch == '8')
        {
            squelch_step_selection(&active, -1, max_num);
        }
        else if (ch == '2')
        {
            squelch_step_selection(&active, 1, max_num);
        }
        else if (ch == '3')
        {
            squelch_step_selection(&active, SQUELCH_PAGE_STEP, max_num);
        }
        else if (ch == '9')
        {
            squelch_step_selection(&active, -SQUELCH_PAGE_STEP, max_num);
        }
        else if (ch == '1')
        {
            active = max_num - 1;
        }
        else if (ch == '7')
        {
            active = 0;
        }
        else if (isgraph((unsigned char)ch))
        {
            for (i = 0; i < max_num; i++)
            {
                ego_item_type* e_ptr = &e_info[choice[i]];
                const char* name = strip_ego_name(e_name + e_ptr->name);

                if (toupper((unsigned char)name[0])
                    == toupper((unsigned char)ch))
                {
                    active = i;
                    break;
                }
            }
            if (i >= max_num)
                bell("");
        }
        else
        {
            bell("");
        }
    }

    mem_free_null(choice);
}

/*
 * Hack -- initialize the mapping from tvals to typevals.
 * This is currently called every time the squelch menus are
 * accessed.  This can certainly be improved.
 */

void init_tv_to_type(void)
{
    tv_to_type[TV_SKELETON] = TYPE_MISC;
    tv_to_type[TV_CHEST] = TYPE_MISC;
    tv_to_type[TV_ARROW] = TYPE_AMMO;
    tv_to_type[TV_BOW] = TYPE_BOW;
    tv_to_type[TV_DIGGING] = TYPE_WEAPON2;
    tv_to_type[TV_HAFTED] = TYPE_WEAPON2;
    tv_to_type[TV_POLEARM] = TYPE_WEAPON2;
    tv_to_type[TV_SWORD] = TYPE_WEAPON1;
    tv_to_type[TV_BOOTS] = TYPE_BOOTS;
    tv_to_type[TV_GLOVES] = TYPE_GLOVES;
    tv_to_type[TV_HELM] = TYPE_HELM;
    tv_to_type[TV_CROWN] = TYPE_HELM;
    tv_to_type[TV_SHIELD] = TYPE_SHIELD;
    tv_to_type[TV_CLOAK] = TYPE_CLOAK;
    tv_to_type[TV_SOFT_ARMOR] = TYPE_BODY;
    tv_to_type[TV_MAIL] = TYPE_BODY;
    tv_to_type[TV_LIGHT] = TYPE_MISC;
    tv_to_type[TV_AMULET] = TYPE_AMULET;
    tv_to_type[TV_RING] = TYPE_RING;
    tv_to_type[TV_STAFF] = TYPE_STAFF;
    tv_to_type[TV_HORN] = TYPE_HORN;
    tv_to_type[TV_POTION] = TYPE_POTION;
    tv_to_type[TV_FLASK] = TYPE_MISC;
    tv_to_type[TV_FOOD] = TYPE_FOOD;
}

static void squelch_run_browser_menu(void)
{
    int selected_index = 0;
    int type_count = squelch_type_count();

    while (true)
    {
        app_ui_scene scene;
        int ch;

        if (!squelch_build_main_menu_scene(&scene, selected_index)
            || !squelch_present_ui_scene(&scene, &ch))
        {
            log_warn("squelch: failed to present main menu scene");
            msg_print("Squelch menu unavailable.");
            return;
        }

        if (ch == ESCAPE)
            return;
        if (ch == '8')
        {
            squelch_step_selection(&selected_index, -1, type_count);
            continue;
        }
        if (ch == '2')
        {
            squelch_step_selection(&selected_index, 1, type_count);
            continue;
        }
        if (ch == '\r' || ch == '\n')
        {
            (void)squelch_show_object_kind_menu(typevals[selected_index].tval,
                typevals[selected_index].desc);
            continue;
        }

        switch (ch)
        {
        case 'Q':
            squelch_run_quality_browser();
            break;
        case 'E':
            squelch_run_ego_browser();
            break;
        case 'S':
            squelch_save_values_to_file();
            break;
        case 'L':
            squelch_load_values_from_file();
            break;
        case 'B':
            squelch_save_autoinscriptions_to_file();
            break;
        case 'G':
            squelch_load_autoinscriptions_from_file();
            break;
        default:
        {
            int category = squelch_choice_index_for_key((char)ch, type_count);

            if (category >= 0)
            {
                selected_index = category;
                (void)squelch_show_object_kind_menu(typevals[category].tval,
                    typevals[category].desc);
            }
            else
            {
                bell("");
            }
            break;
        }
        }
    }
}

void do_cmd_squelch_autoinsc(void)
{
    int x, y;
    init_tv_to_type();
    squelch_run_browser_menu();

    /* Rearrange all the stacks to reflect squelch menus were touched. */
    for (x = 0; x < p_ptr->cur_map_wid; x++)
    {
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            rearrange_stack(y, x);
        }
    }

    /* Restore the screen */
    do_cmd_redraw();

    return;
}

/*
 * Determines if an object is going to be squelched on identification.
 * Input:
 *  o_ptr   : This is a pointer to the object type being identified.
 *  feeling : This is the feeling of the object if it is being
 *            pseudoidentified or 0 if the object is being identified.
 *  fullid  : Is the object is being identified?
 *
 * Output: One of the three above values.
 */

int squelch_itemp(object_type* o_ptr, byte feelings, bool fullid)
{
    int i, num, result;
    byte feel;

    /* default */
    result = SQUELCH_NO;

    /* Squelch some ego items if known */
    if (fullid && ego_item_p(o_ptr))
    {
        byte ego_pfx = object_ego_prefix(o_ptr);
        byte ego_sfx = object_ego_suffix(o_ptr);
        bool should_squelch = false;

        if (ego_pfx && e_info[ego_pfx].squelch)
            should_squelch = true;
        if (ego_sfx && e_info[ego_sfx].squelch)
            should_squelch = true;

        if (should_squelch)
            return ((o_ptr->obj_note) ? SQUELCH_FAILED : SQUELCH_YES);
    }

    /* Check to see if the object is eligible for squelching on id. */
    num = -1;

    /*find the appropriate squelch group*/
    for (i = 0; tvals[i].tval; i++)
    {
        if (tvals[i].tval == o_ptr->tval)
        {
            num = i;
        }
    }

    /*never squelched*/
    if (num == -1)
        return result;

    /*
     * Get the "feeling" of the object.  If the object is being identified
     * get the feeling returned by a heavy pseudoid.
     */
    feel = feelings;

    /*handle fully identified objects*/
    if (fullid)
        feel = value_check_aux1(o_ptr);

    /* Get result based on the feeling and the squelch_level */
    switch (squelch_level[num])
    {
    case SQUELCH_NONE:
    {
        return result;
        break;
    }

    case SQUELCH_CURSED:
    {
        result
            = (((feel == INSCRIP_BROKEN) || (feel == INSCRIP_TERRIBLE)
                   || (feel == INSCRIP_WORTHLESS) || (feel == INSCRIP_CURSED))
                    ? SQUELCH_YES
                    : SQUELCH_NO);
        break;
    }

    case SQUELCH_AVERAGE:
    {
        result = (((feel == INSCRIP_BROKEN) || (feel == INSCRIP_TERRIBLE)
                      || (feel == INSCRIP_WORTHLESS) || (feel == INSCRIP_CURSED)
                      || (feel == INSCRIP_AVERAGE))
                ? SQUELCH_YES
                : SQUELCH_NO);
        break;
    }

    case SQUELCH_GOOD_STRONG:
    {
        result = (((feel == INSCRIP_BROKEN) || (feel == INSCRIP_TERRIBLE)
                      || (feel == INSCRIP_WORTHLESS) || (feel == INSCRIP_CURSED)
                      || (feel == INSCRIP_AVERAGE)
                      || (feel == INSCRIP_GOOD_STRONG))
                ? SQUELCH_YES
                : SQUELCH_NO);
        break;
    }

    case SQUELCH_GOOD_WEAK:
    {
        result
            = (((feel == INSCRIP_BROKEN) || (feel == INSCRIP_TERRIBLE)
                   || (feel == INSCRIP_WORTHLESS) || (feel == INSCRIP_CURSED)
                   || (feel == INSCRIP_AVERAGE) || (feel == INSCRIP_GOOD_STRONG)
                   || (feel == INSCRIP_GOOD_WEAK))
                    ? SQUELCH_YES
                    : SQUELCH_NO);
        break;
    }

    case SQUELCH_ALL:
    {
        result = SQUELCH_YES;
        break;
    }
    }

    if (result == SQUELCH_NO)
        return result;

    /* Squelching will fail on an artefact */
    if ((artefact_p(o_ptr)) || (o_ptr->obj_note))
        result = SQUELCH_FAILED;

    return result;
}

/*
 * This performs the squelch, actually removing the item from the
 * game.  It returns 1 if the item was squelched, and 0 otherwise.
 * This return value is never actually used.
 */
int do_squelch_item(int squelch, int item, object_type* o_ptr)
{
    if (squelch != SQUELCH_YES)
        return 0;

    if (item >= 0)
    {
        inven_item_increase(item, -o_ptr->number);
        inven_item_optimize(item);
    }

    else
    {
        floor_item_increase(0 - item, -o_ptr->number);
        floor_item_optimize(0 - item);
    }

    return 1;
}

void rearrange_stack(int y, int x)
{
    s16b o_idx, next_o_idx;
    s16b first_bad_idx, first_good_idx, cur_bad_idx, cur_good_idx;

    object_type* o_ptr;

    bool sq_flag = false;

    /* Initialize */
    first_bad_idx = 0;
    first_good_idx = 0;
    cur_bad_idx = 0;
    cur_good_idx = 0;

    /*go through all the objects*/
    for (o_idx = cave_o_idx[y][x]; o_idx; o_idx = next_o_idx)
    {
        o_ptr = &(o_list[o_idx]);
        next_o_idx = o_ptr->next_o_idx;

        /*is it marked for squelching*/
        sq_flag = ((k_info[o_ptr->k_idx].squelch == SQUELCH_ALWAYS)
            && (k_info[o_ptr->k_idx].aware));

        if (sq_flag)
        {
            if (first_bad_idx == 0)
            {
                first_bad_idx = o_idx;
                cur_bad_idx = o_idx;
            }

            else
            {
                o_list[cur_bad_idx].next_o_idx = o_idx;
                cur_bad_idx = o_idx;
            }
        }

        else

        {
            if (first_good_idx == 0)
            {
                first_good_idx = o_idx;
                cur_good_idx = o_idx;
            }

            else
            {
                o_list[cur_good_idx].next_o_idx = o_idx;
                cur_good_idx = o_idx;
            }
        }
    }

    if (first_good_idx != 0)
    {
        cave_o_idx[y][x] = first_good_idx;
        o_list[cur_good_idx].next_o_idx = first_bad_idx;
        o_list[cur_bad_idx].next_o_idx = 0;
    }

    else
    {
        cave_o_idx[y][x] = first_bad_idx;
    }
}

void do_squelch_pile(int y, int x)
{
    s16b o_idx, next_o_idx;
    object_type* o_ptr;
    bool sq_flag = false;

    for (o_idx = cave_o_idx[y][x]; o_idx; o_idx = next_o_idx)
    {
        o_ptr = &(o_list[o_idx]);

        next_o_idx = o_ptr->next_o_idx;

        sq_flag = ((k_info[o_ptr->k_idx].squelch == SQUELCH_ALWAYS)
            && (k_info[o_ptr->k_idx].aware));

        /*hack - never squelch artefacts*/
        if artefact_p (o_ptr)
            sq_flag = false;

        /*always squelch "&nothing*/
        if (!o_ptr->k_idx)
            sq_flag = true;

        if ((sq_flag))
        {
            delete_object_idx(o_idx);
        }
    }
}

int get_autoinscription_index(s16b k_idx)
{
    int i;

    for (i = 0; i < inscriptionsCount; i++)
    {
        if (k_idx == inscriptions[i].kindIdx)
        {
            return i;
        }
    }

    return -1;
}

/*Put the autoinscription on an object*/
int apply_autoinscription(object_type* o_ptr)
{
    cptr note = get_autoinscription(o_ptr->k_idx);
    cptr existingInscription = quark_str(o_ptr->obj_note);

    /* Don't inscribe objects if there is no autoinscription to do! */
    if (!note)
    {
        return (0);
    }

    /* Don't re-inscribe if it's already correctly inscribed */
    if (existingInscription && streq(note, existingInscription))
    {
        return (0);
    }

    o_ptr->obj_note = note[0] == 0 ? 0 : quark_add(note);

    return (1);
}

int remove_autoinscription(s16b kind)
{
    int i = get_autoinscription_index(kind);

    /* It's not here, */
    if (i == -1)
        return 0;

    while (i < inscriptionsCount - 1)
    {
        inscriptions[i] = inscriptions[i + 1];
        i++;
    }

    inscriptionsCount--;

    return 1;
}

/*
 *  Uninscribes an object if its inscription matches the given autoinscription
 */
void unapply_autoinscription(object_type* o_ptr, cptr note)
{
    cptr existingInscription = quark_str(o_ptr->obj_note);

    /* Remove the inscription if it matches the autoinscription */
    if (existingInscription && streq(note, existingInscription))
    {
        /* Remove the inscription */
        o_ptr->obj_note = 0;
    }

    return;
}

/*
 *  Removes an autoinscription from the database and from all objects of that
 * kind
 */
extern void obliterate_autoinscription(s16b kind)
{
    int i;
    int j = get_autoinscription_index(kind);
    cptr note = get_autoinscription(kind);
    object_type* o_ptr;

    /* Abort if there is no autoinscription for that object kind */
    if (j == -1)
        return;

    // Go through all objects in the dungeon and inventory...
    for (i = 1; i < o_max; i++)
    {
        /* Get the next object from the dungeon */
        o_ptr = &o_list[i];

        // Don't remove inscriptions from different object kinds.
        if (o_ptr->k_idx != kind)
            continue;

        /* Apply an autoinscription */
        unapply_autoinscription(o_ptr, note);
    }
    for (i = INVEN_PACK; i > 0; i--)
    {
        // Don't remove inscriptions from different object kinds.
        if (inventory[i].k_idx != kind)
            continue;

        unapply_autoinscription(&inventory[i], note);
    }

    remove_autoinscription(kind);

    return;
}

void autoinscribe_dungeon(void)
{
    int i;
    object_type* o_ptr;

    for (i = 1; i < o_max; i++)
    {
        /* Get the next object from the dungeon */
        o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Apply an autoinscription */
        apply_autoinscription(o_ptr);
    }
}

void autoinscribe_ground(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    s16b this_o_idx, next_o_idx = 0;

    /* Scan the pile of objects */
    for (this_o_idx = cave_o_idx[py][px]; this_o_idx; this_o_idx = next_o_idx)
    {
        /* Get the next object */
        next_o_idx = o_list[this_o_idx].next_o_idx;

        /* Apply an autoinscription */
        apply_autoinscription(&o_list[this_o_idx]);
    }
}

void autoinscribe_pack(void)
{
    int i;

    for (i = INVEN_PACK; i > 0; i--)
    {
        /* Skip empty items */
        if (!inventory[i].k_idx)
            continue;

        apply_autoinscription(&inventory[i]);
    }
}

int add_autoinscription(s16b kind, cptr inscription)
{
    int index;

    if (kind == 0)
    {
        /* paranoia */
        return 0;
    }

    if (!inscription || inscription[0] == 0)
    {
        return remove_autoinscription(kind);
    }

    index = get_autoinscription_index(kind);

    if (index == -1)
    {
        index = inscriptionsCount;
    }

    if (index >= AUTOINSCRIPTIONS_MAX)
    {
        msg_format("This inscription (%s) cannot be added, "
                   "because the inscription array is full!",
            inscription);
        return 0;
    }

    inscriptions[index].kindIdx = kind;
    inscriptions[index].inscriptionIdx = quark_add(inscription);

    if (index == inscriptionsCount)
    {
        /* Only increment count if inscription added to end of array */
        inscriptionsCount++;
    }

    // add inscriptions to pack and dungeon
    autoinscribe_pack();
    autoinscribe_dungeon();

    return 1;
}

/* Convert the values returned by squelch_itemp to string */
char* squelch_to_label(int squelch)
{
    if (squelch == SQUELCH_YES)
        return ("(Squelched)");

    if (squelch == SQUELCH_FAILED)
        return ("(Squelch Failed)");

    return ("");
}






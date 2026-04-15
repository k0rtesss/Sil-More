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

typedef struct tval_insc_desc tval_insc_desc;

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







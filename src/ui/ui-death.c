/* File: ui/ui-death.c */

#include "angband.h"
#include "externs.h"

#include "blitz.h"
#include "log/log.h"
#include "object/object-display.h"
#include "object/object-slot.h"
#include "object/object-ui-display.h"
#include "player/killer.h"
#include "player/player-oaths.h"
#include "score/score_ui.h"
#include "supplies.h"
#include "ui/ui-character-screen.h"
#include "ui/ui-death.h"
#include "ui/ui-information-scene.h"
#include "ui/story_font.h"

static void death_get_term_size(int* wid, int* hgt)
{
    int term_wid = 80;
    int term_hgt = 24;

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 1)
        term_hgt = 24;

    if (wid)
        *wid = term_wid;
    if (hgt)
        *hgt = term_hgt;
}

static app_ui_panel* death_begin_document_scene(app_ui_scene* scene,
    bool use_backdrop, bool dim_backdrop)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    if (use_backdrop)
        scene->flags |= APP_UI_SCENE_FLAG_USE_BACKDROP;
    if (dim_backdrop)
        scene->flags |= APP_UI_SCENE_FLAG_DIM_BACKDROP;

    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_DOCUMENT;
    panel->min_width_px = 0;
    panel->width_cap_px = 0;
    return panel;
}

static bool death_scene_add_text(app_ui_scene* scene, app_ui_panel* panel,
    s16b row, s16b col, byte attr, byte story, cptr text)
{
    if (!text || !text[0])
        return true;

    return app_ui_panel_add_document_text_ex(scene, panel, row, col, attr,
        story, text);
}

static int death_item_tile_width(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return 0;
    if (use_graphics == GRAPHICS_NONE || use_graphics == GRAPHICS_PSEUDO)
        return 0;

    return use_bigtile ? 2 : 1;
}

static bool death_scene_add_item_cell(app_ui_scene* scene, app_ui_panel* panel,
    s16b row, s16b col, const object_type* o_ptr)
{
    int width = death_item_tile_width(o_ptr);

    if (width <= 0)
        return true;

    return app_ui_panel_add_document_cell_ex(scene, panel, row, col,
        object_attr(o_ptr), object_char(o_ptr), 0, 0, 0, (byte)width);
}

static bool death_present_ui_page(const app_ui_scene* scene, bool* out_escape)
{
    int key;

    if (out_escape)
        *out_escape = false;
    if (!scene)
        return false;
    if (!ui_information_scene_present_ui(scene))
        return false;

    key = ui_information_scene_wait_key_nonrepeat();
    if (out_escape)
        *out_escape = (key == ESCAPE);
    return true;
}

static bool death_build_inventory_scene(app_ui_scene* scene)
{
    app_ui_panel* panel;
    int term_wid;
    int term_hgt;
    int menu_wid;
    int i;
    int j;
    int k;
    int l;
    int z = 0;
    int col;
    int len;
    int lim;
    int max_rows;
    int effective_max_items;
    int weight_col;
    int label_col;
    int prompt_row;
    int out_index[24];
    byte out_color[24];
    char out_desc[24][80];
    char o_name[80];
    char tmp_val[80];
    bool include_supplies;
    bool use_story_font = story_inventory_enabled();
    byte story = use_story_font ? STORY_FLAG_USE : 0;
    byte grid_story = use_story_font ? (STORY_FLAG_USE | STORY_FLAG_CELL_ALIGN)
                                     : 0;

    panel = death_begin_document_scene(scene, false, false);
    if (!panel)
        return false;

    death_get_term_size(&term_wid, &term_hgt);
    menu_wid = menu_term_width();
    weight_col = menu_weight_col_for_width(menu_wid);
    label_col = menu_label_col_for_width(menu_wid, show_weights);

    len = 29;
    lim = menu_wid - 3;
    if (lim < 0)
        lim = 0;
    if (show_weights && lim > (weight_col - 1))
        lim = weight_col - 1;
    if (lim < 0)
        lim = 0;

    include_supplies = !inventory_menu_get_include_equip()
        && supplies_visible_for_current_filter();

    for (i = 0; i < INVEN_PACK; i++)
    {
        if (inventory[i].k_idx)
            z = i + 1;
    }

    max_rows = term_hgt - 1;
    if (max_rows < 1)
        max_rows = INVEN_PACK;

    effective_max_items = include_supplies ? (max_rows - 1) : max_rows;
    if (z > effective_max_items)
        z = effective_max_items;

    k = 0;
    if (include_supplies && k < (int)N_ELEMENTS(out_index))
    {
        format_supply_summary(out_desc[k], sizeof(out_desc[0]));
        out_index[k] = SUPPLIES_INDEX;
        out_color[k] = TERM_L_WHITE;
        l = menu_inventory_row_width(out_desc[k], NULL, show_weights);
        if (l > len)
            len = l;
        k++;
    }

    for (i = 0; i < z && k < (int)N_ELEMENTS(out_index); i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!item_tester_okay(o_ptr))
            continue;

        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
        o_name[lim] = '\0';

        out_index[k] = i;
        out_color[k] = weapon_glows(o_ptr)
            ? object_display_color(o_ptr, TERM_L_BLUE)
            : object_display_color(o_ptr,
                tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
        SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));

        l = menu_inventory_row_width(out_desc[k], o_ptr, show_weights);
        if (l > len)
            len = l;
        k++;
    }

    col = menu_center_col_for_len(menu_wid, len);

    if (!death_scene_add_text(scene, panel, 0, 0, TERM_WHITE, 0,
            "You are carrying:"))
    {
        return false;
    }

    for (j = 0; j < k; j++)
    {
        int row = j + 1;
        int idx = out_index[j];
        bool is_supply = (idx == SUPPLIES_INDEX);
        object_type* o_ptr = is_supply ? NULL : &inventory[idx];
        int text_col = col;

        if (!is_supply && o_ptr && o_ptr->k_idx)
        {
            if (!death_scene_add_item_cell(scene, panel, (s16b)row,
                    (s16b)text_col, o_ptr))
            {
                return false;
            }
            text_col += death_item_tile_width(o_ptr);
        }

        if (!death_scene_add_text(scene, panel, (s16b)row, (s16b)text_col,
                out_color[j], story, out_desc[j]))
        {
            return false;
        }

        if (show_weights)
        {
            int wgt;

            if (is_supply)
                wgt = supplies_total_weight();
            else
                wgt = o_ptr->weight * o_ptr->number;

            strnfmt(tmp_val, sizeof(tmp_val), "%3d.%1d lb", wgt / 10,
                wgt % 10);
            if (!death_scene_add_text(scene, panel, (s16b)row,
                    (s16b)weight_col, out_color[j], grid_story, tmp_val))
            {
                return false;
            }
        }

        if (is_supply)
        {
            char label = supplies_label_char();
            int slot = supplies_virtual_slot();

            if (!label && slot >= 0)
                label = index_to_label(slot);
            if (!label)
                label = 'a';
            strnfmt(tmp_val, sizeof(tmp_val), " (%c)", label);
        }
        else
        {
            strnfmt(tmp_val, sizeof(tmp_val), " (%c)", index_to_label(idx));
        }

        if (!death_scene_add_text(scene, panel, (s16b)row, (s16b)label_col,
                TERM_WHITE, story, tmp_val))
        {
            return false;
        }
    }

    prompt_row = MIN(p_ptr->inven_cnt + 2, term_hgt - 2);
    if (prompt_row < 0)
        prompt_row = 0;

    return death_scene_add_text(scene, panel, (s16b)prompt_row,
        (s16b)MAX(0, term_wid - 18), TERM_L_WHITE, 0, "(press any key)");
}

static bool death_build_equipment_scene(app_ui_scene* scene)
{
    app_ui_panel* panel;
    int term_wid;
    int term_hgt;
    int menu_wid;
    int i;
    int j;
    int k;
    int l;
    int col;
    int len;
    int lim;
    int weight_col;
    int label_col;
    int armour_weight = 0;
    int out_index[24];
    byte out_color[24];
    char out_desc[24][80];
    char o_name[80];
    char tmp_val[80];
    bool use_story_font = story_equipment_enabled();
    byte story = use_story_font ? STORY_FLAG_USE : 0;
    byte grid_story = use_story_font ? (STORY_FLAG_USE | STORY_FLAG_CELL_ALIGN)
                                     : 0;

    panel = death_begin_document_scene(scene, false, false);
    if (!panel)
        return false;

    death_get_term_size(&term_wid, &term_hgt);
    menu_wid = menu_term_width();
    weight_col = menu_weight_col_for_width(menu_wid);
    label_col = menu_label_col_for_width(menu_wid, show_weights);

    len = 29;
    lim = menu_wid - 3;
    if (lim < 0)
        lim = 0;
    lim -= (14 + 2);
    if (show_weights)
        lim -= 9;
    if (lim < 0)
        lim = 0;

    for (k = 0, i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        bool is_empty = !o_ptr->k_idx;

        if (!item_tester_okay(o_ptr))
            continue;

        if (is_empty)
        {
            SDL_strlcpy(o_name, describe_empty_slot(i), sizeof(o_name));
            out_color[k] = TERM_L_DARK;
        }
        else
        {
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
            out_color[k] = object_display_color(o_ptr,
                tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
        }

        o_name[lim] = '\0';
        out_index[k] = i;
        SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));

        l = menu_equipment_row_width(out_desc[k], is_empty ? NULL : o_ptr,
            show_weights);
        if (l > len)
            len = l;
        k++;
    }

    col = menu_center_col_for_len(menu_wid, len);

    if (!death_scene_add_text(scene, panel, 0, 0, TERM_WHITE, 0,
            "You are using:"))
    {
        return false;
    }

    for (j = 0; j < k; j++)
    {
        int row = j + 1;
        int slot = out_index[j];
        object_type* o_ptr = &inventory[slot];
        bool has_object = o_ptr->k_idx != 0;
        int text_col = col + 12 + 2;
        const char* desc_ptr = out_desc[j];
        char prefix_buf[32];
        char label_buf[8];
        char weight_buf[16];
        char combined_desc[160];
        byte weight_attr = out_color[j];

        strnfmt(prefix_buf, sizeof(prefix_buf), "%-12s: ", mention_use(slot));
        strnfmt(label_buf, sizeof(label_buf), " (%c)", index_to_label(slot));

        if (!death_scene_add_text(scene, panel, (s16b)row, (s16b)col,
                TERM_WHITE, story, prefix_buf))
        {
            return false;
        }

        if (has_object)
        {
            if (!death_scene_add_item_cell(scene, panel, (s16b)row,
                    (s16b)text_col, o_ptr))
            {
                return false;
            }
            text_col += death_item_tile_width(o_ptr);
        }

        if (use_story_font)
        {
            story_prepare_equipment_desc(combined_desc, sizeof(combined_desc),
                desc_ptr, slot, has_object,
                menu_desc_limit(text_col, label_col, weight_col, show_weights));
            desc_ptr = combined_desc;
        }

        if (!death_scene_add_text(scene, panel, (s16b)row, (s16b)text_col,
                out_color[j], story, desc_ptr))
        {
            return false;
        }

        if (show_weights && o_ptr->weight)
        {
            int wgt = o_ptr->weight * o_ptr->number;

            strnfmt(weight_buf, sizeof(weight_buf), "%2d.%1d lb", wgt / 10,
                wgt % 10);
            if ((slot >= INVEN_BODY) && (slot <= INVEN_FEET))
            {
                weight_attr = TERM_SLATE;
                armour_weight += wgt;
            }

            if (!death_scene_add_text(scene, panel, (s16b)row,
                    (s16b)weight_col, weight_attr, grid_story, weight_buf))
            {
                return false;
            }
        }

        if (!use_story_font && slot == INVEN_QUIVER2)
        {
            int note_col = col + 12 + 2 + (int)strlen(out_desc[j]);

            if (!death_scene_add_text(scene, panel, (s16b)row, (s16b)note_col,
                    TERM_L_DARK, 0, " (keeps passive bonuses)"))
            {
                return false;
            }
        }

        if (!death_scene_add_text(scene, panel, (s16b)row, (s16b)label_col,
                TERM_WHITE, story, label_buf))
        {
            return false;
        }
    }

    if (armour_weight)
    {
        int total_row = INVEN_TOTAL - INVEN_WIELD + 1;
        int text_row = total_row + 1;

        if (!death_scene_add_text(scene, panel, (s16b)total_row,
                (s16b)weight_col, TERM_L_DARK, grid_story, "--------"))
        {
            return false;
        }

        strnfmt(tmp_val, sizeof(tmp_val), "armour: %3d.%1d lb",
            armour_weight / 10, armour_weight % 10);
        if (!death_scene_add_text(scene, panel, (s16b)text_row,
                (s16b)MAX(0, weight_col - 8), TERM_SLATE, grid_story, tmp_val))
        {
            return false;
        }
    }

    return death_scene_add_text(scene, panel, (s16b)MAX(0, term_hgt - 2),
        (s16b)MAX(0, term_wid - 18), TERM_L_WHITE, 0, "(press any key)");
}

static bool death_build_final_menu_scene(app_ui_scene* scene, int highlight,
    bool morgoth_victory)
{
    app_ui_panel* panel;
    int term_wid;
    int term_hgt;
    int separator_row;
    int option_row;
    char separator[96];
    const char* option_a = morgoth_victory ? "a) Review the Valar's record"
                                           : "a) View scores";
    const char* option_b = morgoth_victory ? "b) Survey Angband one last time"
                                           : "b) Final look";
    const char* option_c = morgoth_victory ? "c) Rehear the proclamations"
                                           : "c) View final messages";
    const char* option_d = morgoth_victory ? "d) Review your legend"
                                           : "d) View character sheet";
    const char* option_e = morgoth_victory ? "e) Append to the annals"
                                           : "e) Add comment to notes";
    const char* option_f = morgoth_victory ? "f) Archive your legend"
                                           : "f) Save character sheet";
    const char* option_g = "g) Exit";

    panel = death_begin_document_scene(scene, true, true);
    if (!panel)
        return false;

    death_get_term_size(&term_wid, &term_hgt);
    separator_row = (term_hgt < 20) ? 9 : 10;
    option_row = separator_row + 2;
    memset(separator, '_', sizeof(separator) - 1);
    separator[MIN((int)sizeof(separator) - 1, MAX(1, term_wid - 6))] = '\0';

    return death_scene_add_text(scene, panel, (s16b)separator_row, 3,
               TERM_L_DARK, 0, separator)
        && death_scene_add_text(scene, panel, (s16b)option_row++, 15,
            (highlight == 1) ? TERM_L_BLUE : TERM_WHITE, 0, option_a)
        && death_scene_add_text(scene, panel, (s16b)option_row++, 15,
            (highlight == 2) ? TERM_L_BLUE : TERM_WHITE, 0, option_b)
        && death_scene_add_text(scene, panel, (s16b)option_row++, 15,
            (highlight == 3) ? TERM_L_BLUE : TERM_WHITE, 0, option_c)
        && death_scene_add_text(scene, panel, (s16b)option_row++, 15,
            (highlight == 4) ? TERM_L_BLUE : TERM_WHITE, 0, option_d)
        && death_scene_add_text(scene, panel, (s16b)option_row++, 15,
            (highlight == 5) ? TERM_L_BLUE : TERM_WHITE, 0, option_e)
        && death_scene_add_text(scene, panel, (s16b)option_row++, 15,
            (highlight == 6) ? TERM_L_BLUE : TERM_WHITE, 0, option_f)
        && death_scene_add_text(scene, panel, (s16b)option_row, 15,
            (highlight == 7) ? TERM_L_BLUE : TERM_WHITE, 0, option_g);
}

static bool death_show_character_sheet_legacy(void)
{
    int term_wid;
    int term_hgt;

    death_get_term_size(&term_wid, &term_hgt);
    display_player(0);
    Term_putstr(MAX(0, term_wid - 18), term_hgt - 2, -1, TERM_L_WHITE,
        "(press any key)");
    Term_fresh();

    return inkey() == ESCAPE;
}

static bool death_show_equipment_legacy(void)
{
    int term_wid;
    int term_hgt;
    bool escaped;
    bool saved_item_tester_full = item_tester_full;

    death_get_term_size(&term_wid, &term_hgt);
    Term_clear();
    item_tester_full = true;
    show_equip();
    item_tester_full = saved_item_tester_full;
    prt("You are using:", 0, 0);
    Term_putstr(MAX(0, term_wid - 18), term_hgt - 2, -1, TERM_L_WHITE,
        "(press any key)");
    Term_fresh();

    escaped = (inkey() == ESCAPE);
    return escaped;
}

static bool death_show_inventory_legacy(void)
{
    int term_wid;
    int term_hgt;
    bool escaped;
    bool saved_item_tester_full = item_tester_full;

    death_get_term_size(&term_wid, &term_hgt);
    Term_clear();
    item_tester_full = true;
    show_inven();
    item_tester_full = saved_item_tester_full;
    prt("You are carrying:", 0, 0);
    Term_putstr(MAX(0, term_wid - 18),
        MIN(p_ptr->inven_cnt + 2, term_hgt - 2), -1, TERM_L_WHITE,
        "(press any key)");
    Term_fresh();

    escaped = (inkey() == ESCAPE);
    return escaped;
}

static char death_wait_final_menu_key_legacy(int highlight,
    bool morgoth_victory)
{
    char ch;
    int term_wid;
    int term_hgt;
    int separator_row;
    int option_row;
    char separator[96];
    const char* option_a = morgoth_victory ? "a) Review the Valar's record"
                                           : "a) View scores";
    const char* option_b = morgoth_victory ? "b) Survey Angband one last time"
                                           : "b) Final look";
    const char* option_c = morgoth_victory ? "c) Rehear the proclamations"
                                           : "c) View final messages";
    const char* option_d = morgoth_victory ? "d) Review your legend"
                                           : "d) View character sheet";
    const char* option_e = morgoth_victory ? "e) Append to the annals"
                                           : "e) Add comment to notes";
    const char* option_f = morgoth_victory ? "f) Archive your legend"
                                           : "f) Save character sheet";
    const char* option_exit = "g) Exit";

    death_get_term_size(&term_wid, &term_hgt);
    separator_row = (term_hgt < 20) ? 9 : 10;
    option_row = separator_row + 2;
    memset(separator, '_', sizeof(separator) - 1);
    separator[MIN((int)sizeof(separator) - 1, MAX(1, term_wid - 6))] = '\0';

    Term_putstr(3, separator_row, term_wid - 6, TERM_L_DARK, separator);
    Term_putstr(15, option_row++, term_wid - 15,
        (highlight == 1) ? TERM_L_BLUE : TERM_WHITE, option_a);
    Term_putstr(15, option_row++, term_wid - 15,
        (highlight == 2) ? TERM_L_BLUE : TERM_WHITE, option_b);
    Term_putstr(15, option_row++, term_wid - 15,
        (highlight == 3) ? TERM_L_BLUE : TERM_WHITE, option_c);
    Term_putstr(15, option_row++, term_wid - 15,
        (highlight == 4) ? TERM_L_BLUE : TERM_WHITE, option_d);
    Term_putstr(15, option_row++, term_wid - 15,
        (highlight == 5) ? TERM_L_BLUE : TERM_WHITE, option_e);
    Term_putstr(15, option_row++, term_wid - 15,
        (highlight == 6) ? TERM_L_BLUE : TERM_WHITE, option_f);
    Term_putstr(15, option_row, term_wid - 15,
        (highlight == 7) ? TERM_L_BLUE : TERM_WHITE, option_exit);
    Term_fresh();
    Term_gotoxy(10, separator_row + 1 + highlight);

    inkey_set_cursor_hidden(true);
    ch = inkey();
    inkey_set_cursor_hidden(false);
    return ch;
}

void do_cmd_morgoth_victory(void)
{
    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[160];

    p_ptr->morgoth_slain = true;
    flush();
    p_ptr->is_dead = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;
    p_ptr->escaped = false;

    (void)strftime(long_day, sizeof(long_day), "%d %B %Y", localtime(&ct));

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    strnfmt(buf, sizeof(buf),
        "On %s you broke the illusion binding Morgoth to his throne.",
        long_day);
    do_cmd_note(buf, p_ptr->depth);

    do_cmd_note(
        "The Valar hail your impossible triumph and pour out their blessing.",
        p_ptr->depth);

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

    SDL_strlcpy(p_ptr->died_from, "Morgoth's illusory defeat",
        sizeof(p_ptr->died_from));

    killer_mark_other(SCORE_KILLER_OTHER);
    killer_commit(p_ptr->died_from);

    if (run_mode_is_blitz())
        blitz_show_end_summary(3);
}

void ui_death_print_tomb(struct high_score* the_score)
{
    if (p_ptr->escaped)
    {
        if (p_ptr->oath_type > 0 && !oath_invalid(p_ptr->oath_type))
            Term_putstr(
                15, 2, -1, TERM_L_BLUE, "You have escaped and kept your oath");
        else
            Term_putstr(15, 2, -1, TERM_L_BLUE, "You have escaped");
    }
    else if (p_ptr->morgoth_slain)
    {
        Term_putstr(15, 2, -1, TERM_YELLOW,
            "You are acclaimed as the Slayer of Morgoth");
    }
    else
    {
        Term_putstr(15, 2, -1, TERM_L_BLUE, "You have been slain");
    }

    display_single_score(TERM_WHITE, 1, 0, 0, false, the_score);
}

void ui_death_show_character_info(void)
{
    app_ui_scene character_scene;
    app_ui_scene equipment_scene;
    app_ui_scene inventory_scene;
    bool have_equipment = (p_ptr->equip_cnt > 0);
    bool have_inventory = (p_ptr->inven_cnt > 0);
    bool have_character_scene;
    bool have_equipment_scene = true;
    bool have_inventory_scene = true;
    ui_information_scene_scope scope;
    bool escaped = false;

    have_character_scene = build_character_sheet_ui_scene(&character_scene,
        "(press any key)");

    if (have_equipment)
    {
        bool saved_item_tester_full = item_tester_full;

        item_tester_full = true;
        have_equipment_scene = death_build_equipment_scene(&equipment_scene);
        item_tester_full = saved_item_tester_full;
    }

    if (have_inventory)
    {
        bool saved_item_tester_full = item_tester_full;

        item_tester_full = true;
        have_inventory_scene = death_build_inventory_scene(&inventory_scene);
        item_tester_full = saved_item_tester_full;
    }

    if (have_character_scene && have_equipment_scene && have_inventory_scene
        && ui_information_scene_enter(&scope))
    {
        if (death_present_ui_page(&character_scene, &escaped))
        {
            if (escaped)
            {
                ui_information_scene_leave(&scope);
                return;
            }

            if (have_equipment)
            {
                if (!death_present_ui_page(&equipment_scene, &escaped))
                {
                    ui_information_scene_leave(&scope);
                }
                else if (escaped)
                {
                    ui_information_scene_leave(&scope);
                    return;
                }
                else if (have_inventory)
                {
                    if (!death_present_ui_page(&inventory_scene, &escaped))
                    {
                        ui_information_scene_leave(&scope);
                    }
                    else
                    {
                        ui_information_scene_leave(&scope);
                        if (escaped)
                            return;
                        do_cmd_knowledge_notes();
                        return;
                    }
                }
                else
                {
                    ui_information_scene_leave(&scope);
                    do_cmd_knowledge_notes();
                    return;
                }
            }
            else if (have_inventory)
            {
                if (death_present_ui_page(&inventory_scene, &escaped))
                {
                    ui_information_scene_leave(&scope);
                    if (escaped)
                        return;
                    do_cmd_knowledge_notes();
                    return;
                }

                ui_information_scene_leave(&scope);
            }
            else
            {
                ui_information_scene_leave(&scope);
                do_cmd_knowledge_notes();
                return;
            }
        }
        else
        {
            ui_information_scene_leave(&scope);
        }
    }

    if (death_show_character_sheet_legacy())
        return;
    if (have_equipment && death_show_equipment_legacy())
        return;
    if (have_inventory && death_show_inventory_legacy())
        return;

    do_cmd_knowledge_notes();
}

int ui_death_final_menu(int* highlight)
{
    char ch;
    bool morgoth_victory = (p_ptr->morgoth_slain && !p_ptr->escaped);
    app_ui_scene scene;
    ui_information_scene_scope scope;
    bool scene_active = false;

    if (death_build_final_menu_scene(&scene, *highlight, morgoth_victory)
        && ui_information_scene_enter(&scope))
    {
        if (ui_information_scene_present_ui(&scene))
        {
            scene_active = true;
        }
        else
        {
            ui_information_scene_leave(&scope);
        }
    }

    if (scene_active)
    {
        inkey_set_cursor_hidden(true);
        ch = (char)ui_information_scene_wait_key();
        inkey_set_cursor_hidden(false);
        ui_information_scene_leave(&scope);
    }
    else
    {
        ch = death_wait_final_menu_key_legacy(*highlight, morgoth_victory);
    }

    if (ch == 'a')
    {
        *highlight = 1;
        return 1;
    }
    if (ch == 'b')
    {
        *highlight = 2;
        return 2;
    }
    if (ch == 'c')
    {
        *highlight = 3;
        return 3;
    }
    if (ch == 'd')
    {
        *highlight = 4;
        return 4;
    }
    if (ch == 'e')
    {
        *highlight = 5;
        return 5;
    }
    if (ch == 'f')
    {
        *highlight = 6;
        return 6;
    }
    if ((ch == 'g') || (ch == 'q') || (ch == 'Q'))
    {
        *highlight = 7;
        return 7;
    }
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
        return *highlight;
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = 7;
    }
    if (ch == '2')
    {
        if (*highlight < 7)
            (*highlight)++;
        else if (*highlight == 7)
            *highlight = 1;
    }

    return 0;
}

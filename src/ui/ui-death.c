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

static bool death_add_body_line(app_ui_panel* panel, byte attr, cptr text)
{
    if (!panel || !text || !text[0])
        return true;

    return app_ui_panel_add_body_line(panel, attr, text);
}

static byte death_status_title_attr(void)
{
    if (p_ptr->morgoth_slain && !p_ptr->escaped)
        return TERM_YELLOW;

    return TERM_L_BLUE;
}

static cptr death_status_title_text(void)
{
    if (p_ptr->escaped)
    {
        if (p_ptr->oath_type > 0 && !oath_invalid(p_ptr->oath_type))
            return "You have escaped and kept your oath";

        return "You have escaped";
    }
    if (p_ptr->morgoth_slain)
        return "You are acclaimed as the Slayer of Morgoth";

    return "You have been slain";
}

static void death_format_score_day(const high_score* score, char* out,
    size_t out_size)
{
    const char* when;

    if (!out || !out_size)
        return;

    out[0] = '\0';
    if (!score)
        return;

    when = score->day;
    while (*when && isspace((unsigned char)*when))
        when++;

    if ((*when == '@') && strlen(when) == 9)
    {
        char month_num[3];
        char month[4];

        SDL_strlcpy(month_num, when + 5, sizeof(month_num));
        atomonth(atoi(month_num), month);
        if (*(when + 7) == '0')
            strnfmt(out, out_size, "%.1s %.3s %.4s", when + 8, month,
                when + 1);
        else
            strnfmt(out, out_size, "%.2s %.3s %.4s", when + 7, month,
                when + 1);
        return;
    }

    SDL_strlcpy(out, when, out_size);
}

static void death_build_name_line(const high_score* score, char* out,
    size_t out_size)
{
    int ph;
    char score_commas[16];
    const char* suffix = "";

    if (!out || !out_size)
        return;

    out[0] = '\0';
    if (!score)
        return;

    ph = atoi(score->p_h);
    if (ph >= 0 && ph < z_info->c_max)
        suffix = c_name + c_info[ph].alt_name;

    comma_number(score_commas, score_points(score));
    strnfmt(out, out_size, "%s%s  [%s pts]", score->who, suffix, score_commas);
}

static bool death_build_curse_line(const high_score* score, char* out,
    size_t out_size, byte* out_attr)
{
    int curses;

    if (out_attr)
        *out_attr = TERM_WHITE;
    if (!out || !out_size)
        return false;

    out[0] = '\0';
    if (!score || !scores_version_has_curses(score_file_global_ctx()))
        return false;

    curses = parse_score_int(score->pts, sizeof(score->pts), 0);
    if (curses > 0)
    {
        strnfmt(out, out_size, "%d curse%s endured", curses,
            (curses == 1) ? "" : "s");
        if (out_attr)
            *out_attr = TERM_L_RED;
        return true;
    }
    if (curses < 0)
    {
        strnfmt(out, out_size, "%d blessing%s claimed", -curses,
            (curses == -1) ? "" : "s");
        if (out_attr)
            *out_attr = TERM_L_GREEN;
        return true;
    }

    return false;
}

static void death_build_outcome_line(const high_score* score, char* out,
    size_t out_size)
{
    int silmarils;

    if (!out || !out_size)
        return;

    out[0] = '\0';
    if (!score)
        return;

    silmarils = atoi(score->silmarils);
    if (score->escaped[0] == 't')
    {
        SDL_strlcpy(out, "Escaped the iron hells", out_size);
        if ((score->morgoth_slain[0] == 't') || (silmarils > 0))
            SDL_strlcat(out, " and brought back the light of Valinor",
                out_size);
        else
            SDL_strlcat(out, " empty-handed", out_size);
        return;
    }

    if (score->morgoth_slain[0] == 't')
    {
        strnfmt(out, out_size, "Victorious over Morgoth's illusion (%s)",
            score->how);
        return;
    }

    strnfmt(out, out_size, "Slain by %s", score->how);
    if (silmarils > 0)
        SDL_strlcat(out, " during a daring escape", out_size);
}

static void death_build_run_line(const high_score* score, char* out,
    size_t out_size)
{
    char turns_commas[16];
    char depth_commas[16];
    char day[32];
    int turns;
    int depth;

    if (!out || !out_size)
        return;

    out[0] = '\0';
    if (!score)
        return;

    turns = atoi(score->turns);
    depth = atoi(score->cur_dun) * 50;
    comma_number(turns_commas, turns);
    comma_number(depth_commas, depth);
    death_format_score_day(score, day, sizeof(day));

    if (day[0])
    {
        strnfmt(out, out_size, "Depth %s ft, after %s turns. (%s)",
            depth_commas, turns_commas, day);
    }
    else
    {
        strnfmt(out, out_size, "Depth %s ft, after %s turns.",
            depth_commas, turns_commas);
    }
}

static bool death_build_trophy_line(const high_score* score, char* out,
    size_t out_size)
{
    int silmarils;

    if (!out || !out_size)
        return false;

    out[0] = '\0';
    if (!score)
        return false;

    silmarils = atoi(score->silmarils);
    if (score->morgoth_slain[0] == 't')
        SDL_strlcat(out, "V", out_size);
    if (silmarils >= 1)
        SDL_strlcat(out, out[0] ? "  *" : "*", out_size);
    if (silmarils >= 2)
        SDL_strlcat(out, " *", out_size);
    if (silmarils >= 3)
        SDL_strlcat(out, " *", out_size);

    if (!out[0])
        return false;

    {
        char decorated[APP_UI_TEXT_MAX];

        strnfmt(decorated, sizeof(decorated), "Trophies: %s", out);
        SDL_strlcpy(out, decorated, out_size);
    }
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

static bool death_build_final_menu_scene(app_ui_scene* scene,
    const high_score* score, int highlight, bool morgoth_victory)
{
    app_ui_panel* panel;
    char summary[APP_UI_TEXT_MAX];
    char outcome[APP_UI_TEXT_MAX];
    char run_line[APP_UI_TEXT_MAX];
    char trophy_line[APP_UI_TEXT_MAX];
    char curse_line[APP_UI_TEXT_MAX];
    byte curse_attr = TERM_WHITE;
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

    if (!scene || !score)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 900, 1360);
    app_ui_panel_set_title(panel, death_status_title_attr(),
        death_status_title_text());

    death_build_name_line(score, summary, sizeof(summary));
    if (summary[0])
        app_ui_panel_set_subtitle(panel, TERM_WHITE, summary);

    if (death_build_curse_line(score, curse_line, sizeof(curse_line),
            &curse_attr))
    {
        if (!death_add_body_line(panel, curse_attr, curse_line))
            return false;
    }

    death_build_outcome_line(score, outcome, sizeof(outcome));
    death_build_run_line(score, run_line, sizeof(run_line));

    if (!death_add_body_line(panel, TERM_WHITE, outcome)
        || !death_add_body_line(panel, TERM_SLATE, run_line)
        || !death_add_body_line(panel, TERM_L_DARK,
            "________________________________________"))
        return false;

    if (death_build_trophy_line(score, trophy_line, sizeof(trophy_line))
        && !death_add_body_line(panel, TERM_YELLOW, trophy_line))
    {
        return false;
    }

    if (!death_add_body_line(panel,
            (highlight == 1) ? TERM_L_BLUE : TERM_WHITE, option_a)
        || !death_add_body_line(panel,
            (highlight == 2) ? TERM_L_BLUE : TERM_WHITE, option_b)
        || !death_add_body_line(panel,
            (highlight == 3) ? TERM_L_BLUE : TERM_WHITE, option_c)
        || !death_add_body_line(panel,
            (highlight == 4) ? TERM_L_BLUE : TERM_WHITE, option_d)
        || !death_add_body_line(panel,
            (highlight == 5) ? TERM_L_BLUE : TERM_WHITE, option_e)
        || !death_add_body_line(panel,
            (highlight == 6) ? TERM_L_BLUE : TERM_WHITE, option_f)
        || !death_add_body_line(panel,
            (highlight == 7) ? TERM_L_BLUE : TERM_WHITE, option_g))
    {
        return false;
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Choose");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "a-g", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "Esc", "Exit");

    return true;
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

    if (!have_character_scene || !have_equipment_scene || !have_inventory_scene)
    {
        log_warn("death character info: semantic scenes required");
        return;
    }

    if (!ui_information_scene_enter(&scope))
    {
        log_warn("death character info: semantic scene entry required");
        return;
    }

        if (!death_present_ui_page(&character_scene, &escaped))
        {
            ui_information_scene_leave(&scope);
            log_warn("death character info: failed to present character sheet scene");
            return;
        }

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
            log_warn("death character info: failed to present equipment scene");
            return;
        }
        if (escaped)
        {
            ui_information_scene_leave(&scope);
            return;
        }
    }

    if (have_inventory)
    {
        if (!death_present_ui_page(&inventory_scene, &escaped))
        {
            ui_information_scene_leave(&scope);
            log_warn("death character info: failed to present inventory scene");
            return;
        }
        if (escaped)
        {
            ui_information_scene_leave(&scope);
            return;
        }
    }

    ui_information_scene_leave(&scope);
    if (escaped)
        return;

    do_cmd_knowledge_notes();
}

int ui_death_final_menu(const high_score* score, int* highlight)
{
    char ch;
    bool morgoth_victory = (p_ptr->morgoth_slain && !p_ptr->escaped);
    app_ui_scene scene;
    ui_information_scene_scope scope = { 0 };

    if (!death_build_final_menu_scene(&scene, score, *highlight,
            morgoth_victory)
        || !ui_information_scene_enter(&scope)
        || !ui_information_scene_present_ui(&scene))
    {
        if (scope.active)
            ui_information_scene_leave(&scope);
        log_warn("death final menu: semantic presentation required");
        return 7;
    }

    inkey_set_cursor_hidden(true);
    ch = (char)ui_information_scene_wait_key();
    inkey_set_cursor_hidden(false);
    ui_information_scene_leave(&scope);

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

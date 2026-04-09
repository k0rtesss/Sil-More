/* File: object-ui-enhanced.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "platform-input.h"
#include "object/object-desc.h"
#include "object/object-display.h"
#include "object/object-slot.h"
#include "object/object-ui-display.h"
#include "object/object-ui-enhanced.h"
#include "player/identification.h"
#include "supplies.h"
#include "ui/story_font.h"

#define ENHANCED_MAX_LIST 80
#define MAX_COMPARE_LINES 2

static void inventory_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static bool death_spectator_allow_menu_action(void)
{
    if (!death_spectator_active())
        return true;

    msg_print("You can no longer take that action.");
    return false;
}

int enhanced_menu_action = ENHANCED_ACTION_NONE;
int enhanced_inventory_selected_item = -1;
char current_menu_command = 0;
int current_menu_state = 0;
int enhanced_equip_action = ENHANCED_ACTION_NONE;
int enhanced_equipment_selected_item = -1;

static void append_compare_slot(int* slots, int* count, int slot)
{
    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
        return;

    for (int i = 0; i < *count; i++)
    {
        if (slots[i] == slot)
            return;
    }

    if (*count < MAX_COMPARE_LINES)
        slots[(*count)++] = slot;
}

void describe_item_with_comparisons(int item_index, bool include_comparisons)
{
    const object_type* objects[MAX_COMPARE_LINES + 1];
    const char* headings[MAX_COMPARE_LINES + 1];
    char heading_texts[MAX_COMPARE_LINES + 1][64];
    int count = 0;
    object_type* base_obj;
    bool is_floor = (item_index < 0);

    if (item_index == -1)
        return;

    if (is_floor)
    {
        int floor_idx = 0 - item_index;
        if (floor_idx <= 0 || floor_idx >= o_max)
            return;
        base_obj = &o_list[floor_idx];
    }
    else
    {
        if (item_index < 0 || item_index >= INVEN_TOTAL)
            return;
        base_obj = &inventory[item_index];
    }

    if (!base_obj->k_idx)
        return;

    if (object_uses_smithing_difficulty(base_obj) && !object_known_p(base_obj))
    {
        bool is_equipped = (!is_floor && item_index >= INVEN_WIELD);
        (void)player_try_identify_smithing_object_on_examine(base_obj,
            is_equipped);
    }

    strnfmt(heading_texts[count], sizeof(heading_texts[count]), "%s:",
        is_floor ? "Selected item (floor)" : "Selected item");
    headings[count] = heading_texts[count];
    objects[count++] = base_obj;

    if (include_comparisons)
    {
        int slots[MAX_COMPARE_LINES];
        int slot_count = 0;

        append_compare_slot(slots, &slot_count, wield_slot(base_obj));

        if (base_obj->tval == TV_RING)
        {
            append_compare_slot(slots, &slot_count, INVEN_LEFT);
            append_compare_slot(slots, &slot_count, INVEN_RIGHT);
        }
        else if (base_obj->tval == TV_ARROW)
        {
            append_compare_slot(slots, &slot_count, INVEN_QUIVER1);
            append_compare_slot(slots, &slot_count, INVEN_QUIVER2);
        }

        for (int i = 0; i < slot_count && count < MAX_COMPARE_LINES + 1; i++)
        {
            int slot = slots[i];
            object_type* equip_obj;

            if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
                continue;

            equip_obj = &inventory[slot];
            strnfmt(heading_texts[count], sizeof(heading_texts[count]), "%s:",
                mention_use(slot));
            headings[count] = heading_texts[count];
            objects[count++] = equip_obj->k_idx ? equip_obj : NULL;
        }
    }

    object_info_screen_multi(objects, headings, count);
}

void show_inven_enhanced(void)
{
    int which;
    bool done = false;
    char out_val[160];
    bool use_story_font = story_inventory_enabled();
    story_font_term_state story_state;
    int story_term_w = 0;
    int i, k, l, z;
    int col, len, lim;
    int term_wid = menu_term_width();
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int weight_col = menu_weight_col_for_width(term_wid);
    int label_col_base = menu_label_col_for_width(term_wid, show_weights);
    int highlight_row = -1;
    bool highlight_active = false;
    int previous_total_rows = 0;
    int previous_compare_count = 0;
    int previous_highlight_row = -1;
    bool first_render = true;
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;
    bool has_floor_items = false;
    bool include_supplies = !inventory_menu_get_include_equip()
        && supplies_visible_for_current_filter();
    object_type* o_ptr;
    char o_name[80];
    char tmp_val[80];
    int out_index[ENHANCED_MAX_LIST];
    byte out_color[ENHANCED_MAX_LIST];
    char out_desc[ENHANCED_MAX_LIST][80];
    bool out_is_floor[ENHANCED_MAX_LIST];
    bool out_is_supply[ENHANCED_MAX_LIST];

    set_story_inventory_list_active(use_story_font);
    story_font_term_push(use_story_font, false, &story_state);
    if (use_story_font)
    {
        int story_term_h = 0;
        Term_get_size(&story_term_w, &story_term_h);
    }

    len = 29;
    lim = term_wid - 3;
    if (lim < 0)
        lim = 0;
    if (show_weights && lim > (weight_col - 1))
        lim = weight_col - 1;
    if (lim < 0)
        lim = 0;

    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px,
        0x00);
    for (i = 0; i < floor_num; i++)
    {
        if (item_tester_okay(&o_list[floor_list[i]]))
        {
            has_floor_items = true;
            break;
        }
    }

    z = 0;
    for (i = 0; i < INVEN_PACK; i++)
    {
        o_ptr = &inventory[i];
        if (o_ptr->k_idx)
            z = i + 1;
    }

    if (include_supplies)
    {
        int max_items = INVEN_PACK - 1;
        if (z > max_items)
            z = max_items;
    }

    k = 0;
    if (has_floor_items)
    {
        for (i = 0; i < floor_num; i++)
        {
            o_ptr = &o_list[floor_list[i]];
            if (!item_tester_okay(o_ptr))
                continue;

            object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
            o_name[lim] = '\0';

            out_index[k] = 0 - floor_list[i];
            out_is_floor[k] = true;
            out_is_supply[k] = false;
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
    }

    if (include_supplies && k < (int)N_ELEMENTS(out_index))
    {
        char supply_desc[80];
        format_supply_summary(supply_desc, sizeof(supply_desc));
        out_index[k] = SUPPLIES_INDEX;
        out_is_floor[k] = false;
        out_is_supply[k] = true;
        out_color[k] = TERM_L_WHITE;
        SDL_strlcpy(out_desc[k], supply_desc, sizeof(out_desc[0]));

        l = menu_inventory_row_width(out_desc[k], NULL, show_weights);
        if (l > len)
            len = l;

        k++;
    }

    for (i = 0; i < z && k < (int)N_ELEMENTS(out_index); i++)
    {
        o_ptr = &inventory[i];
        if (!item_tester_okay(o_ptr))
            continue;

        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        o_name[lim] = '\0';

        out_index[k] = i;
        out_is_floor[k] = false;
        out_is_supply[k] = false;
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

    col = menu_center_col_for_len(term_wid, len);
    if (k > 0)
    {
        highlight_row = 0;
        highlight_active = true;
    }

    while (!done)
    {
        const bool portable_controls = portable_controls_active();
        bool allow_compare = (current_menu_command == 'u'
            || current_menu_command == 'x');
        int compare_count = 0;
        char compare_label[MAX_COMPARE_LINES][4];
        char compare_prefix[MAX_COMPARE_LINES][20];
        char compare_desc[MAX_COMPARE_LINES][80];
        byte compare_attr[MAX_COMPARE_LINES];
        bool compare_has_weight[MAX_COMPARE_LINES];
        int compare_weight[MAX_COMPARE_LINES];
        object_type* compare_obj[MAX_COMPARE_LINES];
        int redraw_y1 = -1;
        int redraw_y2 = -1;

        if (portable_controls)
        {
            char confirm_label[16];
            char desc_label[16];
            char cycle_label[16];

            inventory_prompt_label(' ', "A", confirm_label,
                sizeof(confirm_label));
            inventory_prompt_label('x', "RS Right", desc_label,
                sizeof(desc_label));
            if (current_menu_command == 'u')
            {
                inventory_prompt_label('u', "X", cycle_label,
                    sizeof(cycle_label));
                strnfmt(out_val, sizeof(out_val),
                    "%s-use  %s-desc  <- drop  %s-cycle (Inventory)",
                    confirm_label, desc_label, cycle_label);
            }
            else if (current_menu_command == 'x')
            {
                inventory_prompt_label('x', "RS Right", cycle_label,
                    sizeof(cycle_label));
                strnfmt(out_val, sizeof(out_val),
                    "%s-examine  %s-desc  <- drop  %s-cycle (Inventory)",
                    confirm_label, desc_label, cycle_label);
            }
            else
            {
                strnfmt(out_val, sizeof(out_val),
                    "%s-use  %s-desc  <- drop (Inventory)",
                    confirm_label, desc_label);
            }
        }
        else if (current_menu_command == 'u')
        {
            sprintf(out_val,
                "Space-Use, -> description, %c again-cycle  (Inventory)",
                current_menu_command);
        }
        else if (current_menu_command == 'x')
        {
            sprintf(out_val,
                "Space-Examine, -> description, %c again - cycle  (Inventory)",
                current_menu_command);
        }
        else
        {
            sprintf(out_val, "Space-Use, -> description, <- drop  (Inventory)");
        }
        prt(out_val, 0, 0);

        for (i = 0; i < MAX_COMPARE_LINES; i++)
        {
            compare_label[i][0] = '\0';
            compare_prefix[i][0] = '\0';
            compare_desc[i][0] = '\0';
            compare_attr[i] = TERM_SLATE;
            compare_has_weight[i] = false;
            compare_weight[i] = 0;
            compare_obj[i] = NULL;
        }

        if (use_story_font && allow_compare && !first_render)
        {
            int erase_w = label_col_base + 7 - col;
            if (erase_w < 0)
                erase_w = 0;
            if (story_term_w > 80)
                erase_w = (erase_w * story_term_w) / 80;

            if (previous_highlight_row >= 0 && previous_highlight_row < k)
            {
                int base = 1 + previous_highlight_row;
                for (i = 0; i <= previous_compare_count; i++)
                {
                    int rr = base + i;
                    if (erase_w > 0)
                        Term_erase(col, rr, erase_w);
                    if (show_weights)
                        Term_erase(weight_col, rr, 9);
                    if (redraw_y1 < 0 || rr < redraw_y1)
                        redraw_y1 = rr;
                    if (redraw_y2 < 0 || rr > redraw_y2)
                        redraw_y2 = rr;
                }
            }

            if (highlight_active && highlight_row >= 0 && highlight_row < k)
            {
                int base = 1 + highlight_row;
                for (i = 0; i <= MAX_COMPARE_LINES; i++)
                {
                    int rr = base + i;
                    if (erase_w > 0)
                        Term_erase(col, rr, erase_w);
                    if (show_weights)
                        Term_erase(weight_col, rr, 9);
                    if (redraw_y1 < 0 || rr < redraw_y1)
                        redraw_y1 = rr;
                    if (redraw_y2 < 0 || rr > redraw_y2)
                        redraw_y2 = rr;
                }
            }
        }

        if (allow_compare && highlight_active && highlight_row >= 0
            && highlight_row < k && !out_is_supply[highlight_row])
        {
            int slot_candidates[MAX_COMPARE_LINES];
            int slot_count = 0;
            object_type* highlighted_obj = out_is_floor[highlight_row]
                ? &o_list[0 - out_index[highlight_row]]
                : &inventory[out_index[highlight_row]];

            if (highlighted_obj && highlighted_obj->k_idx)
            {
                append_compare_slot(slot_candidates, &slot_count,
                    wield_slot(highlighted_obj));
                if (highlighted_obj->tval == TV_RING)
                {
                    append_compare_slot(slot_candidates, &slot_count, INVEN_LEFT);
                    append_compare_slot(slot_candidates, &slot_count, INVEN_RIGHT);
                }
                else if (highlighted_obj->tval == TV_ARROW)
                {
                    append_compare_slot(slot_candidates, &slot_count,
                        INVEN_QUIVER1);
                    append_compare_slot(slot_candidates, &slot_count,
                        INVEN_QUIVER2);
                }

                for (i = 0; i < slot_count; i++)
                {
                    int slot = slot_candidates[i];
                    object_type* equipped_obj = &inventory[slot];
                    int compare_lim = (show_weights ? weight_col : label_col_base)
                        - (col + 12 + 2 + 3);

                    strnfmt(compare_label[i], sizeof(compare_label[i]), "%c",
                        index_to_label(slot));
                    strnfmt(compare_prefix[i], sizeof(compare_prefix[i]),
                        "%-12s: ", mention_use(slot));

                    if (compare_lim < 0)
                        compare_lim = 0;
                    if (compare_lim >= (int)sizeof(compare_desc[i]))
                        compare_lim = (int)sizeof(compare_desc[i]) - 1;

                    if (equipped_obj->k_idx)
                    {
                        compare_obj[i] = equipped_obj;
                        object_desc(compare_desc[i], sizeof(compare_desc[i]),
                            equipped_obj, true, 3);
                        compare_desc[i][compare_lim] = '\0';
                        compare_attr[i] = weapon_glows(equipped_obj)
                            ? object_display_color(equipped_obj, TERM_L_BLUE)
                            : object_display_color(equipped_obj,
                                tval_to_attr[equipped_obj->tval
                                    % N_ELEMENTS(tval_to_attr)]);
                        if (show_weights && equipped_obj->weight)
                        {
                            compare_has_weight[i] = true;
                            compare_weight[i]
                                = equipped_obj->weight * equipped_obj->number;
                        }
                    }
                    else
                    {
                        SDL_strlcpy(compare_desc[i], describe_empty_slot(slot),
                            sizeof(compare_desc[i]));
                        compare_desc[i][compare_lim] = '\0';
                        compare_attr[i] = TERM_SLATE;
                    }
                }

                compare_count = slot_count;
            }
        }

        z = 1;
        for (i = 0; i < k; i++)
        {
            int row = z;
            int label_col = label_col_base;
            int text_col = col;
            bool is_floor_item = out_is_floor[i];
            bool is_supply_item = out_is_supply[i];
            bool is_highlight = highlight_active && highlight_row == i;
            byte line_attr = is_highlight ? TERM_L_BLUE : out_color[i];
            object_type* line_obj = NULL;

            if (is_floor_item)
                line_obj = &o_list[0 - out_index[i]];
            else if (!is_supply_item)
                line_obj = &inventory[out_index[i]];

            if (use_story_font)
            {
                if (!allow_compare)
                {
                    int erase_w = 255;
                    if (story_term_w > 80)
                        erase_w = (erase_w * story_term_w) / 80;
                    Term_erase(col, row, erase_w);
                }
                if (is_highlight)
                    story_fill_rect(row, col, term_wid - col, TERM_L_BLUE);
            }
            else
            {
                prt("", row, col);
            }

            if (line_obj && line_obj->k_idx)
                text_col = draw_item_tile(col, row, line_obj);

            if (use_story_font)
            {
                int desc_limit = menu_desc_limit(text_col, label_col,
                    weight_col, show_weights);
                if (story_term_w > 80)
                    desc_limit = (desc_limit * story_term_w) / 80;
                story_print_text(row, text_col, desc_limit, line_attr,
                    out_desc[i]);
            }
            else
            {
                c_put_str(line_attr, out_desc[i], row, text_col);
            }

            if (show_weights)
            {
                int wgt = is_supply_item ? supplies_total_weight()
                    : (line_obj ? line_obj->weight * line_obj->number : 0);
                strnfmt(tmp_val, sizeof(tmp_val), "%2d.%1d lb", wgt / 10,
                    wgt % 10);
                if (use_story_font)
                {
                    int weight_width = label_col - weight_col;
                    if (weight_width < 1)
                        weight_width = 1;
                    story_print_text_grid(row, weight_col, weight_width,
                        line_attr, tmp_val);
                }
                else
                {
                    c_put_str(line_attr, tmp_val, row, weight_col);
                }
            }

            if (is_floor_item)
            {
                strnfmt(tmp_val, sizeof(tmp_val), " (-)");
            }
            else if (is_supply_item)
            {
                char label = supplies_label_char();
                int slot = supplies_virtual_slot();
                if (!label && slot >= 0)
                    label = index_to_label(slot);
                if (!label)
                    label = 'a';
                strnfmt(tmp_val, sizeof(tmp_val), "(%c)", label);
            }
            else
            {
                strnfmt(tmp_val, sizeof(tmp_val), "(%c)",
                    index_to_label(out_index[i]));
            }

            if (use_story_font)
                story_print_text(row, label_col, 6,
                    is_highlight ? TERM_L_BLUE : TERM_WHITE, tmp_val);
            else if (is_highlight)
                c_put_str(TERM_L_BLUE, tmp_val, row, label_col);
            else
                put_str(tmp_val, row, label_col);

            z++;

            if (compare_count > 0 && i == highlight_row)
            {
                for (int idx = 0; idx < compare_count; idx++)
                {
                    int compare_row = z;
                    int compare_text_col = col + 12 + 2;

                    if (use_story_font && !allow_compare)
                    {
                        int erase_w = 255;
                        if (story_term_w > 80)
                            erase_w = (erase_w * story_term_w) / 80;
                        Term_erase(col, compare_row, erase_w);
                    }
                    else if (!use_story_font)
                    {
                        prt("", compare_row, col);
                    }

                    if (use_story_font)
                        story_print_equipment_prefix(compare_row, col, TERM_WHITE,
                            compare_prefix[idx]);
                    else
                        c_put_str(TERM_WHITE, compare_prefix[idx], compare_row,
                            col);

                    if (compare_obj[idx] && compare_obj[idx]->k_idx)
                        compare_text_col = draw_item_tile(col + 12 + 2,
                            compare_row, compare_obj[idx]);

                    if (use_story_font)
                    {
                        int desc_limit = (show_weights ? weight_col : label_col)
                            - compare_text_col;
                        if (desc_limit < 1)
                            desc_limit = 1;
                        if (story_term_w > 80)
                            desc_limit = (desc_limit * story_term_w) / 80;
                        story_print_text(compare_row, compare_text_col,
                            desc_limit, compare_attr[idx], compare_desc[idx]);
                    }
                    else
                    {
                        c_put_str(compare_attr[idx], compare_desc[idx],
                            compare_row, compare_text_col);
                    }

                    if (show_weights)
                    {
                        if (compare_has_weight[idx])
                        {
                            strnfmt(tmp_val, sizeof(tmp_val), "%2d.%1d lb",
                                compare_weight[idx] / 10,
                                compare_weight[idx] % 10);
                            if (use_story_font)
                            {
                                int weight_width = label_col - weight_col;
                                if (weight_width < 1)
                                    weight_width = 1;
                                story_print_text_grid(compare_row, weight_col,
                                    weight_width, compare_attr[idx], tmp_val);
                            }
                            else
                            {
                                c_put_str(compare_attr[idx], tmp_val,
                                    compare_row, weight_col);
                            }
                        }
                        else if (use_story_font)
                            Term_erase(weight_col, compare_row, 9);
                        else
                            prt("", compare_row, weight_col);
                    }

                    strnfmt(tmp_val, sizeof(tmp_val), "(%s)", compare_label[idx]);
                    if (use_story_font)
                        story_print_text(compare_row, label_col, 6,
                            compare_attr[idx], tmp_val);
                    else
                        c_put_str(compare_attr[idx], tmp_val, compare_row,
                            label_col);

                    z++;
                }
            }
        }

        floor_num = z - 1;
        if (use_story_font && allow_compare)
        {
            if (compare_count > 0 && highlight_row >= 0 && highlight_row < k)
            {
                int base = 1 + highlight_row;
                if (redraw_y1 < 0 || base < redraw_y1)
                    redraw_y1 = base;
                if (redraw_y2 < 0 || floor_num > redraw_y2)
                    redraw_y2 = floor_num;
            }
            if (floor_num < previous_total_rows)
            {
                if (redraw_y1 < 0 || floor_num + 1 < redraw_y1)
                    redraw_y1 = floor_num + 1;
                if (redraw_y2 < 0 || previous_total_rows > redraw_y2)
                    redraw_y2 = previous_total_rows;
            }
            if (redraw_y1 > 0 && redraw_y2 >= redraw_y1)
            {
                int max_col = label_col_base + 6;
                if (max_col > Term->wid - 1)
                    max_col = Term->wid - 1;
                Term_redraw_section(col, redraw_y1, max_col, redraw_y2);
            }
        }

        if (floor_num && floor_num < term_hgt - 1)
        {
            if (use_story_font)
            {
                int erase_w = 255;
                if (story_term_w > 80)
                    erase_w = (erase_w * story_term_w) / 80;
                Term_erase(col, floor_num + 1, erase_w);
            }
            else
                prt("", floor_num + 1, col);
        }

        if (floor_num < previous_total_rows)
        {
            for (i = floor_num + 1; i <= previous_total_rows; i++)
            {
                if (use_story_font)
                {
                    int erase_w = 255;
                    int weight_erase_w = 9;
                    if (story_term_w > 80)
                    {
                        erase_w = (erase_w * story_term_w) / 80;
                        weight_erase_w = (weight_erase_w * story_term_w) / 80;
                    }
                    Term_erase(col, i, erase_w);
                    if (show_weights)
                        Term_erase(weight_col, i, weight_erase_w);
                }
                else
                {
                    prt("", i, col);
                    if (show_weights)
                        prt("", i, weight_col);
                }
            }
        }

        previous_total_rows = floor_num;
        previous_compare_count = compare_count;
        previous_highlight_row = highlight_row;
        first_render = false;

        which = inkey();
        switch (which)
        {
        case ESCAPE:
            enhanced_menu_action = ENHANCED_ACTION_NONE;
            done = true;
            break;
        case 'i':
            break;
        case 'e':
            if (current_menu_command != 0)
            {
                if (portable_controls)
                {
                    enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                    done = true;
                }
                else
                    goto default_case;
            }
            else if (portable_controls)
            {
                enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                done = true;
            }
            else
            {
                goto default_case;
            }
            break;
        case 'u':
            if (current_menu_command == which)
            {
                enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                done = true;
            }
            break;
        case 'x':
            if (current_menu_command == which)
            {
                enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                done = true;
            }
            else if (portable_controls && highlight_active
                && highlight_row >= 0 && highlight_row < k)
            {
                enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                enhanced_inventory_selected_item = out_index[highlight_row];
                done = true;
            }
            break;
        case '/':
        case KTRL('I'):
        case KTRL('E'):
            enhanced_menu_action = ENHANCED_ACTION_SWITCH;
            done = true;
            break;
        case '8':
            if (highlight_active && k > 0)
                highlight_row = (highlight_row + k - 1) % k;
            break;
        case '2':
            if (highlight_active && k > 0)
                highlight_row = (highlight_row + 1) % k;
            break;
        case ' ':
        case '\r':
        case '\n':
            if (highlight_active && highlight_row >= 0 && highlight_row < k)
            {
                if (current_menu_command == 'x')
                {
                    enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                    enhanced_inventory_selected_item = out_index[highlight_row];
                }
                else
                {
                    if (!death_spectator_allow_menu_action())
                        break;
                    enhanced_menu_action = ENHANCED_ACTION_USE;
                    enhanced_inventory_selected_item = out_index[highlight_row];
                }
                done = true;
            }
            break;
        case '6':
            if (highlight_active && highlight_row >= 0 && highlight_row < k)
            {
                enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                enhanced_inventory_selected_item = out_index[highlight_row];
                done = true;
            }
            break;
        case '4':
            if (highlight_active && highlight_row >= 0 && highlight_row < k)
            {
                if (!out_is_floor[highlight_row])
                {
                    if (!death_spectator_allow_menu_action())
                        break;
                    enhanced_menu_action = ENHANCED_ACTION_DROP;
                    enhanced_inventory_selected_item = out_index[highlight_row];
                    done = true;
                }
                else
                    bell("Cannot drop floor items!");
            }
            break;
        default:
        default_case:
            if ((which >= 'a' && which <= 'z')
                || (which >= 'A' && which <= 'Z') || which == '-')
            {
                bool item_found = false;
                if (portable_controls)
                {
                    bell("Use arrow keys and Space to select items in this mode");
                    break;
                }
                if (death_spectator_active())
                {
                    death_spectator_allow_menu_action();
                    break;
                }

                if (which == '-' && has_floor_items)
                {
                    for (i = 0; i < k; i++)
                    {
                        if (!out_is_floor[i])
                            continue;
                        done = true;
                        item_found = true;
                        if (current_menu_command != 0)
                        {
                            if (current_menu_command == 'u')
                            {
                                if (!death_spectator_allow_menu_action())
                                {
                                    done = false;
                                    break;
                                }
                                enhanced_menu_action = ENHANCED_ACTION_USE;
                                enhanced_inventory_selected_item = out_index[i];
                            }
                            else if (current_menu_command == 'x')
                            {
                                enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                                enhanced_inventory_selected_item = out_index[i];
                            }
                        }
                        else if (!death_spectator_allow_menu_action())
                        {
                            done = false;
                        }
                        else
                        {
                            enhanced_menu_action = ENHANCED_ACTION_USE;
                            enhanced_inventory_selected_item = out_index[i];
                        }
                        break;
                    }
                }

                if (!item_found && which != '-')
                {
                    int item = label_to_inven(which);
                    if (item >= 0 && (inventory[item].k_idx
                        || (throw_slot_menu_active && throw_slot_enabled[item])))
                    {
                        for (i = 0; i < k; i++)
                        {
                            if (out_is_floor[i] || out_index[i] != item)
                                continue;
                            done = true;
                            item_found = true;
                            if (current_menu_command != 0)
                            {
                                if (current_menu_command == 'u')
                                {
                                    if (!death_spectator_allow_menu_action())
                                    {
                                        done = false;
                                        break;
                                    }
                                    enhanced_menu_action = ENHANCED_ACTION_USE;
                                    enhanced_inventory_selected_item = item;
                                }
                                else if (current_menu_command == 'x')
                                {
                                    enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                                    enhanced_inventory_selected_item = item;
                                }
                            }
                            else if (!death_spectator_allow_menu_action())
                            {
                                done = false;
                            }
                            else
                            {
                                p_ptr->command_new = which;
                                p_ptr->command_see = true;
                            }
                            break;
                        }
                    }
                }

                if (!item_found)
                    bell("Illegal object choice!");
            }
            else
                bell("Invalid command!");
            break;
        }
    }

    story_font_term_pop(&story_state);
}

void show_equip_enhanced(void)
{
    int which;
    bool done = false;
    char out_val[160];
    bool use_story_font = story_equipment_enabled();
    story_font_term_state story_state;
    int story_term_w = 0;
    int i, k, l;
    int clear_col;
    int col, len, lim;
    int term_wid = menu_term_width();
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int weight_col = menu_weight_col_for_width(term_wid);
    int label_col_base = menu_label_col_for_width(term_wid, show_weights);
    int highlight_index = -1;
    bool highlight_active = false;
    object_type* o_ptr;
    char tmp_val[80];
    char o_name[80];
    int out_index[24];
    byte out_color[24];
    char out_desc[24][80];
    int armour_weight = 0;

    set_story_equipment_list_active(use_story_font);
    story_font_term_push(use_story_font, false, &story_state);
    if (use_story_font)
    {
        int story_term_h = 0;
        Term_get_size(&story_term_w, &story_term_h);
    }

    len = 29;
    lim = term_wid - 3;
    if (lim < 0)
        lim = 0;
    lim -= 16;
    if (show_weights)
        lim -= 9;
    if (lim < 0)
        lim = 0;

    for (k = 0, i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];
        if (!item_tester_okay(o_ptr))
            continue;

        if (!o_ptr->k_idx)
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

        if (show_weights && o_ptr->weight && i >= INVEN_BODY && i <= INVEN_FEET)
            armour_weight += o_ptr->weight * o_ptr->number;

        l = menu_equipment_row_width(out_desc[k], o_ptr->k_idx ? o_ptr : NULL,
            show_weights);
        if (l > len)
            len = l;

        k++;
    }

    col = menu_center_col_for_len(term_wid, len);
    clear_col = menu_overlay_clear_col(col);
    if (k > 0)
    {
        highlight_index = 0;
        highlight_active = true;
    }

    while (!done)
    {
        const bool portable_controls = portable_controls_active();

        if (use_story_font)
        {
            draw_equipment_story_rows(col, k, out_index, out_color, out_desc,
                highlight_active, highlight_index, show_weights, story_term_w);
            if (armour_weight)
            {
                int total_row = INVEN_TOTAL - INVEN_WIELD + 1;
                int text_row = total_row + 1;

                Term_erase(clear_col, total_row, 255);
                Term_erase(clear_col, text_row, 255);
                story_print_text_grid(total_row, weight_col, 8, TERM_L_DARK,
                    "--------");
                strnfmt(tmp_val, sizeof(tmp_val), "armour: %3d.%1d lb",
                    armour_weight / 10, armour_weight % 10);
                story_print_text_grid(text_row, MAX(0, weight_col - 8), 16,
                    TERM_SLATE, tmp_val);
                if (k && (k + 3 < term_hgt - 1))
                    Term_erase(clear_col, k + 3, 255);
            }
        }
        else
        {
            show_equip();
        }

        if (portable_controls)
        {
            char confirm_label[16];
            char desc_label[16];
            char cycle_label[16];

            inventory_prompt_label(' ', "A", confirm_label,
                sizeof(confirm_label));
            inventory_prompt_label('x', "RS Right", desc_label,
                sizeof(desc_label));
            if (current_menu_command == 'u')
            {
                inventory_prompt_label('u', "X", cycle_label,
                    sizeof(cycle_label));
                strnfmt(out_val, sizeof(out_val),
                    "%s-remove  %s-desc  <- drop  %s-cycle (Equipment)",
                    confirm_label, desc_label, cycle_label);
            }
            else if (current_menu_command == 'x')
            {
                inventory_prompt_label('x', "RS Right", cycle_label,
                    sizeof(cycle_label));
                strnfmt(out_val, sizeof(out_val),
                    "%s-remove  %s-desc  <- drop  %s-cycle (Equipment)",
                    confirm_label, desc_label, cycle_label);
            }
            else
            {
                strnfmt(out_val, sizeof(out_val),
                    "%s-remove  %s-desc  <- drop (Equipment)",
                    confirm_label, desc_label);
            }
        }
        else if (current_menu_command == 'u' || current_menu_command == 'x')
        {
            sprintf(out_val, "Space-Remove, %c again - cycle  (Equipment)",
                current_menu_command);
        }
        else
        {
            sprintf(out_val,
                "Space-Remove, -> description, <- drop  (Equipment)");
        }

        if (use_story_font)
            story_print_text(0, 0, 0, TERM_WHITE, out_val);
        else
            prt(out_val, 0, 0);

        if (!use_story_font && highlight_active && highlight_index >= 0
            && highlight_index < k)
        {
            int display_row = -1;
            int current_row = 1;
            int highlighted_slot = out_index[highlight_index];

            for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
            {
                if (item_tester_okay(&inventory[i]))
                {
                    if (i == highlighted_slot)
                    {
                        display_row = current_row;
                        break;
                    }
                    current_row++;
                }
            }

            if (display_row > 0)
            {
                o_ptr = &inventory[highlighted_slot];
                Term_erase(clear_col, display_row, 255);
                strnfmt(tmp_val, sizeof(tmp_val), "%-12s: ",
                    mention_use(highlighted_slot));
                c_put_str(TERM_L_BLUE, tmp_val, display_row, col);

                i = col + 14;
                if (o_ptr->k_idx)
                    i = draw_item_tile(col + 14, display_row, o_ptr);

                c_put_str(TERM_L_BLUE, out_desc[highlight_index], display_row, i);

                if (show_weights && o_ptr->weight)
                {
                    int wgt = o_ptr->weight * o_ptr->number;
                    sprintf(tmp_val, "%3d.%1d lb", wgt / 10, wgt % 10);
                    c_put_str(TERM_L_BLUE, tmp_val, display_row, weight_col);
                }

                if (highlighted_slot == INVEN_QUIVER2)
                    c_put_str(TERM_L_DARK, " (keeps passive bonuses)",
                        display_row, i + (int)strlen(out_desc[highlight_index]));

                sprintf(tmp_val, " (%c)", index_to_label(highlighted_slot));
                c_put_str(TERM_L_BLUE, tmp_val, display_row, label_col_base);
            }
        }

        which = inkey();
        switch (which)
        {
        case ESCAPE:
            enhanced_equip_action = ENHANCED_ACTION_NONE;
            done = true;
            break;
        case 'e':
            break;
        case 'i':
            if (current_menu_command != 0)
            {
                if (portable_controls)
                {
                    enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                    done = true;
                }
                else
                    goto equip_default_case;
            }
            else if (portable_controls)
            {
                enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                done = true;
            }
            else
            {
                goto equip_default_case;
            }
            break;
        case 'u':
            if (current_menu_command == which)
            {
                enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                done = true;
            }
            break;
        case 'x':
            if (current_menu_command == which)
            {
                enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                done = true;
            }
            else if (portable_controls && highlight_active
                && highlight_index >= 0 && highlight_index < k)
            {
                enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                enhanced_equipment_selected_item = out_index[highlight_index];
                done = true;
            }
            break;
        case '/':
        case KTRL('I'):
        case KTRL('E'):
            enhanced_equip_action = ENHANCED_ACTION_SWITCH;
            done = true;
            break;
        case '8':
            if (highlight_active && k > 0)
                highlight_index = (highlight_index + k - 1) % k;
            break;
        case '2':
            if (highlight_active && k > 0)
                highlight_index = (highlight_index + 1) % k;
            break;
        case ' ':
        case '\r':
        case '\n':
            if (highlight_active && highlight_index >= 0 && highlight_index < k)
            {
                if (current_menu_command == 'x')
                {
                    enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                    enhanced_equipment_selected_item = out_index[highlight_index];
                }
                else
                {
                    if (!death_spectator_allow_menu_action())
                        break;
                    enhanced_equip_action = ENHANCED_ACTION_USE;
                    enhanced_equipment_selected_item = out_index[highlight_index];
                }
                done = true;
            }
            break;
        case '6':
            if (highlight_active && highlight_index >= 0 && highlight_index < k)
            {
                enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                enhanced_equipment_selected_item = out_index[highlight_index];
                done = true;
            }
            break;
        case '4':
            if (highlight_active && highlight_index >= 0 && highlight_index < k)
            {
                if (!death_spectator_allow_menu_action())
                    break;
                enhanced_equip_action = ENHANCED_ACTION_DROP;
                enhanced_equipment_selected_item = out_index[highlight_index];
                done = true;
            }
            break;
        default:
        equip_default_case:
            if ((which >= 'a' && which <= 'z')
                || (which >= 'A' && which <= 'Z'))
            {
                int item;
                if (portable_controls)
                {
                    bell("Use arrow keys and Space to select items in this mode");
                    break;
                }
                if (death_spectator_active())
                {
                    death_spectator_allow_menu_action();
                    break;
                }

                item = label_to_equip(which);
                if (item >= INVEN_WIELD && item < INVEN_TOTAL
                    && (inventory[item].k_idx
                        || (throw_slot_menu_active && throw_slot_enabled[item])))
                {
                    done = true;
                    if (current_menu_command != 0)
                    {
                        if (current_menu_command == 'u')
                        {
                            if (!death_spectator_allow_menu_action())
                            {
                                done = false;
                                break;
                            }
                            enhanced_equip_action = ENHANCED_ACTION_USE;
                            enhanced_equipment_selected_item = item;
                        }
                        else if (current_menu_command == 'x')
                        {
                            enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                            enhanced_equipment_selected_item = item;
                        }
                    }
                    else if (!death_spectator_allow_menu_action())
                    {
                        done = false;
                    }
                    else
                    {
                        p_ptr->command_new = which;
                        p_ptr->command_see = true;
                    }
                }
                else
                    bell("Illegal object choice!");
            }
            else
                bell("Invalid command!");
            break;
        }
    }

    story_font_term_pop(&story_state);
}

#undef MAX_COMPARE_LINES
#undef ENHANCED_MAX_LIST

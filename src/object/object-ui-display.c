/* File: object-ui-display.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "platform-story-font.h"
#include "object/object-desc.h"
#include "object/object-display.h"
#include "object/object-slot.h"
#include "object/object-ui-display.h"
#include "supplies.h"
#include "ui/story_font.h"
#include <ctype.h>

static bool story_inventory_list_active = false;
static bool story_equipment_list_active = false;

enum
{
    MENU_LABEL_FIELD_WIDTH = 4,
    MENU_WEIGHT_FIELD_WIDTH = 8
};

bool get_story_inventory_list_active(void)
{
    return story_inventory_list_active;
}

void set_story_inventory_list_active(bool active)
{
    story_inventory_list_active = active;
}

bool get_story_equipment_list_active(void)
{
    return story_equipment_list_active;
}

void set_story_equipment_list_active(bool active)
{
    story_equipment_list_active = active;
}

void story_print_equipment_prefix(int row, int col, byte attr, cptr prefix)
{
    const int prefix_core_width = 12;
    char label_buf[32];

    if (!prefix) prefix = "";

    const char* colon = strchr(prefix, ':');
    size_t len = colon ? (size_t)(colon - prefix) : strlen(prefix);
    if (len >= sizeof(label_buf))
        len = sizeof(label_buf) - 1;

    memcpy(label_buf, prefix, len);
    label_buf[len] = '\0';

    while (len > 0 && isspace((unsigned char)label_buf[len - 1]))
        label_buf[--len] = '\0';

    story_print_text(row, col, prefix_core_width, attr, label_buf);
    story_print_text_grid(row, col + prefix_core_width, 2, attr, ": ");
}

void story_prepare_equipment_desc(char* dest, size_t dest_size, cptr src,
    int slot, bool has_object, int max_cols)
{
    if (!dest || dest_size == 0)
        return;

    if (!src)
        src = "";

    SDL_strlcpy(dest, src, dest_size);

    if (slot == INVEN_QUIVER2 && !has_object)
    {
        char base[160];
        SDL_strlcpy(base, dest, sizeof(base));
        if (base[0])
            strnfmt(dest, dest_size, "%s (keeps passive bonuses)", base);
        else
            SDL_strlcpy(dest, "(keeps passive bonuses)", dest_size);
    }

    if (max_cols > 0 && sdl_is_story_font_enabled())
    {
        int cell_width = sdl_get_cell_width();
        int max_pixels = max_cols * cell_width;
        size_t len = strlen(dest);

        while (len > 0 && sdl_story_font_text_width(dest, (int)len) > max_pixels)
        {
            dest[--len] = '\0';
            while (len > 0 && isspace((unsigned char)dest[len - 1]))
                dest[--len] = '\0';
        }
    }
}

bool supplies_visible_for_current_filter(void)
{
    if (supplies_entry_count() <= 0)
        return false;

    if (item_tester_full)
        return true;

    if (!item_tester_tval && !item_tester_hook)
        return true;

    if (supplies_has_pending_action())
        return true;

    return supplies_any_match_item_tester();
}

void format_supply_summary(char* buf, size_t len)
{
    int herbs = 0;
    int food = 0;
    int potions = 0;
    int gems = 0;
    int lights = 0;
    bool first = true;
    char segment[32];

    if (!buf || len == 0)
        return;

    supplies_count_totals(&herbs, &food, &potions, &gems, &lights);

    SDL_strlcpy(buf, "Supplies", len);

    if (herbs <= 0 && food <= 0 && potions <= 0 && gems <= 0 && lights <= 0)
        return;

    SDL_strlcat(buf, " (", len);

    if (herbs > 0)
    {
        strnfmt(segment, sizeof(segment), "%d herb%s", herbs,
            (herbs == 1) ? "" : "s");
        SDL_strlcat(buf, segment, len);
        first = false;
    }

    if (food > 0)
    {
        if (!first)
            SDL_strlcat(buf, ", ", len);
        strnfmt(segment, sizeof(segment), "%d food", food);
        SDL_strlcat(buf, segment, len);
        first = false;
    }

    if (potions > 0)
    {
        if (!first)
            SDL_strlcat(buf, ", ", len);
        strnfmt(segment, sizeof(segment), "%d potion%s", potions,
            (potions == 1) ? "" : "s");
        SDL_strlcat(buf, segment, len);
        first = false;
    }

    if (gems > 0)
    {
        if (!first)
            SDL_strlcat(buf, ", ", len);
        strnfmt(segment, sizeof(segment), "%d gem%s", gems,
            (gems == 1) ? "" : "s");
        SDL_strlcat(buf, segment, len);
        first = false;
    }

    if (lights > 0)
    {
        if (!first)
            SDL_strlcat(buf, ", ", len);
        strnfmt(segment, sizeof(segment), "%d light%s", lights,
            (lights == 1) ? "" : "s");
        SDL_strlcat(buf, segment, len);
    }

    SDL_strlcat(buf, ")", len);
}

int draw_item_tile(int x, int y, object_type* o_ptr)
{
    if (use_graphics != GRAPHICS_NONE && use_graphics != GRAPHICS_PSEUDO && o_ptr && o_ptr->k_idx)
    {
        byte attr = object_attr(o_ptr);

        Term_putch(x, y, attr, object_char(o_ptr));

        if (use_bigtile)
        {
            Term_putch(x + 1, y, 255, -1);
            return x + 2;
        }

        return x + 1;
    }

    return x;
}

static int menu_item_tile_width(const object_type* o_ptr)
{
    if (use_graphics != GRAPHICS_NONE && use_graphics != GRAPHICS_PSEUDO
        && o_ptr && o_ptr->k_idx)
    {
        return use_bigtile ? 2 : 1;
    }

    return 0;
}

int menu_term_width(void)
{
    if (Term && Term->wid > 0)
        return Term->wid;

    return 80;
}

int menu_weight_col_for_width(int term_wid)
{
    int col = term_wid - (MENU_LABEL_FIELD_WIDTH + MENU_WEIGHT_FIELD_WIDTH);

    if (col < 0)
        col = 0;

    return col;
}

int menu_label_col_for_width(int term_wid, bool display_weights)
{
    (void)display_weights;

    int col = term_wid - MENU_LABEL_FIELD_WIDTH;

    if (col < 0)
        col = 0;

    return col;
}

int menu_center_col_for_len(int term_wid, int len)
{
    if (len >= term_wid)
        return 0;

    return (term_wid - len) / 2;
}

int menu_overlay_clear_col(int col)
{
    /* Keep a one-cell gutter so centered overlays stay visually separate. */
    if (col > 0)
        return col - 1;

    return 0;
}

int menu_desc_limit(int text_col, int label_col, int weight_col,
    bool display_weights)
{
    int right_edge = display_weights ? weight_col : label_col;
    int limit = right_edge - text_col;

    if (limit < 1)
        limit = 1;

    return limit;
}

int menu_inventory_row_width(cptr desc, const object_type* o_ptr,
    bool display_weights)
{
    int desc_len = desc ? (int)strlen(desc) : 0;
    int suffix_width = MENU_LABEL_FIELD_WIDTH;

    if (display_weights)
        suffix_width += MENU_WEIGHT_FIELD_WIDTH;

    return menu_item_tile_width(o_ptr) + desc_len + suffix_width;
}

int menu_equipment_row_width(cptr desc, const object_type* o_ptr,
    bool display_weights)
{
    const int prefix_width = 12 + 2;

    return prefix_width + menu_inventory_row_width(desc, o_ptr,
        display_weights);
}

void story_render_inventory_entry(int row, int base_col, int label_col,
    cptr desc, byte desc_attr, bool display_weights, cptr weight_text,
    byte weight_attr, cptr label_text, byte label_attr, const object_type* o_ptr,
    bool highlight, int story_term_w)
{
    int term_wid = (story_term_w > 0) ? story_term_w : menu_term_width();
    int highlight_cols = term_wid;
    int weight_col = display_weights ? MAX(0, label_col - 8) : label_col;
    const int label_width = MENU_LABEL_FIELD_WIDTH;

    Term_erase(base_col, row, 255);
    if (highlight)
        story_fill_rect(row, base_col, highlight_cols - base_col, TERM_L_BLUE);

    int text_col = base_col;
    if (o_ptr && o_ptr->k_idx)
        text_col = draw_item_tile(base_col, row, (object_type*)o_ptr);

    int desc_limit = menu_desc_limit(text_col, label_col, weight_col,
        display_weights);
    story_print_text(row, text_col, desc_limit, desc_attr, desc);

    if (display_weights && weight_text && weight_text[0])
    {
        int weight_width = label_col - weight_col;
        if (weight_width < 1)
            weight_width = 1;
        story_print_text_grid(row, weight_col, weight_width, weight_attr,
            weight_text);
    }

    if (label_text && label_text[0])
        story_print_text(row, label_col, label_width, label_attr, label_text);
}

void story_render_equipment_entry(int row, int col, int slot, cptr prefix,
    byte prefix_attr, cptr desc, byte desc_attr, bool display_weights,
    cptr weight_text, byte weight_attr, cptr label_text, byte label_attr,
    const object_type* o_ptr, bool highlight, int story_term_w)
{
    int clear_col = menu_overlay_clear_col(col);
    int term_wid = (story_term_w > 0) ? story_term_w : menu_term_width();
    int highlight_cols = term_wid;
    int label_col = menu_label_col_for_width(term_wid, display_weights);
    int weight_col = menu_weight_col_for_width(term_wid);
    const int label_width = MENU_LABEL_FIELD_WIDTH;
    bool has_object = (o_ptr && o_ptr->k_idx);

    Term_erase(clear_col, row, 255);
    if (highlight)
        story_fill_rect(row, col, highlight_cols - col, TERM_L_BLUE);

    story_print_equipment_prefix(row, col, prefix_attr, prefix);

    int text_col = col + 12 + 2;
    if (has_object)
        text_col = draw_item_tile(col + 12 + 2, row, (object_type*)o_ptr);

    int desc_limit = menu_desc_limit(text_col, label_col, weight_col,
        display_weights);

    char combined_desc[160];
    story_prepare_equipment_desc(combined_desc, sizeof(combined_desc), desc,
        slot, has_object, desc_limit);
    story_print_text(row, text_col, desc_limit, desc_attr, combined_desc);

    if (display_weights && weight_text && weight_text[0])
    {
        int weight_width = label_col - weight_col;
        if (weight_width < 1)
            weight_width = 1;
        story_print_text_grid(row, weight_col, weight_width, weight_attr,
            weight_text);
    }

    if (label_text && label_text[0])
        story_print_text(row, label_col, label_width, label_attr, label_text);
}

void draw_equipment_story_rows(int col, int entry_count, int* out_index,
    byte* out_color, char out_desc[][80], bool highlight_active,
    int highlight_index, bool display_weights, int story_term_w)
{
    int clear_col = menu_overlay_clear_col(col);
    int term_wid = (story_term_w > 0) ? story_term_w : menu_term_width();
    int label_col_base = menu_label_col_for_width(term_wid, display_weights);
    int weight_col = menu_weight_col_for_width(term_wid);
    int highlight_cols = term_wid;
    const int label_width = MENU_LABEL_FIELD_WIDTH;

    log_trace("draw_equipment_story_rows: entry_count=%d, highlight_active=%d, highlight_index=%d",
        entry_count, highlight_active, highlight_index);

    for (int idx = 0; idx < entry_count; idx++)
    {
        int row = idx + 1;
        bool is_highlight = highlight_active && idx == highlight_index;
        byte line_attr = is_highlight ? TERM_L_BLUE : out_color[idx];
        int slot = out_index[idx];
        object_type* o_ptr = &inventory[slot];
        bool has_object = o_ptr->k_idx != 0;

        if (is_highlight)
        {
            log_trace("draw_equipment_story_rows: Drawing HIGHLIGHTED row %d, slot=%d, has_object=%d, desc='%s'",
                row, slot, has_object, out_desc[idx]);
        }

        Term_erase(clear_col, row, 255);
        if (is_highlight)
        {
            log_trace("draw_equipment_story_rows: Filling highlight rect at row %d", row);
            story_fill_rect(row, col, highlight_cols - col, TERM_L_BLUE);
        }

        char prefix[32];
        strnfmt(prefix, sizeof(prefix), "%-12s: ", mention_use(slot));
        byte prefix_attr = is_highlight ? TERM_L_BLUE : TERM_WHITE;
        log_trace("draw_equipment_story_rows: Row %d - printing prefix '%s' at col=%d", row, prefix, col);
        story_print_equipment_prefix(row, col, prefix_attr, prefix);

        int text_col = col + 12 + 2;
        log_trace("draw_equipment_story_rows: Row %d - text_col calculated as %d (col=%d + 12 + 2)", row, text_col, col);
        if (has_object)
        {
            int tile_end_col = draw_item_tile(text_col, row, o_ptr);
            log_trace("draw_equipment_story_rows: Row %d - drew tile, text_col updated from %d to %d", row, text_col, tile_end_col);
            text_col = tile_end_col;
        }

        int label_col = label_col_base;
        int desc_limit = menu_desc_limit(text_col, label_col, weight_col,
            display_weights);

        char combined_desc[160];
        story_prepare_equipment_desc(combined_desc, sizeof(combined_desc),
            out_desc[idx], slot, has_object, desc_limit);

        log_trace("draw_equipment_story_rows: Row %d - printing desc '%s' at col=%d limit=%d",
            row, combined_desc, text_col, desc_limit);
        story_print_text(row, text_col, desc_limit, line_attr, combined_desc);

        if (display_weights && has_object && o_ptr->weight)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            char weight_buf[16];
            strnfmt(weight_buf, sizeof(weight_buf), "%2d.%1d lb", wgt / 10, wgt % 10);
            int weight_width = label_col - weight_col;
            if (weight_width < 1)
                weight_width = 1;
            log_trace("draw_equipment_story_rows: Row %d - printing weight '%s' at col=%d width=%d", row, weight_buf, weight_col, weight_width);
            story_print_text_grid(row, weight_col, weight_width, line_attr,
                weight_buf);
        }

        char label_buf[8];
        strnfmt(label_buf, sizeof(label_buf), "(%c)", index_to_label(slot));
        byte label_attr = is_highlight ? TERM_L_BLUE : TERM_WHITE;
        log_trace("draw_equipment_story_rows: Row %d - printing label '%s' at col=%d width=%d (label_col_base=%d)", row, label_buf, label_col, label_width, label_col_base);
        story_print_text(row, label_col, label_width, label_attr, label_buf);
    }

    log_trace("draw_equipment_story_rows: Finished drawing all rows");
}

/*
 * Legacy subwindow renderer for PW_INVEN. SDL snapshot item selection no
 * longer draws through this helper.
 */
void display_inven(void)
{
    register int i, n, z = 0;

    object_type* o_ptr;

    byte attr;
    bool use_story_font = story_inventory_enabled();
    story_font_term_state story_state;

    char tmp_val[80];
    char o_name[80];
    bool floor_item = false;

    int w = Term->wid;
    int col = w - 11;
    if (col < 0) col = 0;
    int offset = use_bigtile ? 6 : 5;

    story_font_term_push(use_story_font, false, &story_state);

    for (i = 0; i < INVEN_PACK; i++)
    {
        o_ptr = &inventory[i];
        if (!o_ptr->k_idx)
            continue;
        z = i + 1;
    }

    for (i = 0; i <= z; i++)
    {
        if (i == z)
        {
            o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
            i = INVEN_WIELD;
            if (!o_ptr->k_idx)
                continue;
            floor_item = true;
        }
        else
        {
            o_ptr = &inventory[i];
        }

        tmp_val[0] = tmp_val[1] = tmp_val[2] = ' ';
        tmp_val[3] = '\0';

        if (item_tester_okay(o_ptr))
        {
            if (!floor_item)
            {
                if ((p_ptr->get_item_mode == 0)
                    || (p_ptr->get_item_mode & (USE_INVEN)))
                {
                    tmp_val[0] = index_to_label(i);
                    tmp_val[1] = ')';
                }
            }
            else
            {
                if ((p_ptr->get_item_mode == 0)
                    || (p_ptr->get_item_mode & (USE_FLOOR)))
                {
                    tmp_val[0] = '-';
                    tmp_val[1] = ')';
                }
            }
        }

        if ((p_ptr->command_wrk == 0) || (p_ptr->command_wrk & (USE_INVEN)))
            attr = TERM_WHITE;
        else
            attr = TERM_SLATE;

        Term_erase(0, i, 255);

        if (use_story_font)
            story_print_text(i, 0, 3, attr, tmp_val);
        else
            Term_putstr(0, i, 3, attr, tmp_val);

        Term_putch(3, i, object_attr(o_ptr), object_char(o_ptr));
        if (use_bigtile)
        {
            Term_putch(4, i, 255, -1);
        }

        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        int max_desc = w - offset - 1;
        if (show_weights && col > offset)
            max_desc = col - offset - 1;
        if (max_desc < 1) max_desc = 1;
        if (max_desc >= (int)sizeof(o_name)) max_desc = (int)sizeof(o_name) - 1;
        o_name[max_desc] = '\0';

        n = (int)strlen(o_name);

        if (weapon_glows(o_ptr))
            attr = object_display_color(o_ptr, TERM_L_BLUE);
        else
            attr = object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

        Term_putch(offset - 1, i, attr, ' ');
        if (use_story_font)
            story_print_text(i, offset, max_desc, attr, o_name);
        else
            Term_putstr(offset, i, n, attr, o_name);

        if (o_ptr->weight)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            strnfmt(tmp_val, sizeof(tmp_val), "%3d.%1d lb", wgt / 10,
                wgt % 10);
            if (use_story_font)
                story_print_text_grid(i, col, 8, attr, tmp_val);
            else
                Term_putstr(col, i, -1, attr, tmp_val);
        }
    }

    for (i = z; i < Term->hgt; i++)
    {
        if ((i != INVEN_WIELD) || !floor_item)
        {
            Term_erase(0, i, 255);
        }
    }

    story_font_term_pop(&story_state);
}

/*
 * Legacy subwindow renderer for PW_EQUIP. SDL snapshot item selection no
 * longer draws through this helper.
 */
void display_equip(void)
{
    register int i, n;
    object_type* o_ptr;
    byte attr;
    int armour_weight = 0;

    char tmp_val[80];
    char o_name[80];

    int w = Term->wid;
    int col = w - 11;
    if (col < 0) col = 0;
    int offset = use_bigtile ? 6 : 5;

    bool use_story_font = story_equipment_enabled();
    story_font_term_state story_state;
    story_font_term_push(use_story_font, false, &story_state);

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        tmp_val[0] = tmp_val[1] = tmp_val[2] = ' ';
        tmp_val[3] = '\0';

        if (item_tester_okay(o_ptr))
        {
            if ((p_ptr->get_item_mode == 0)
                || (p_ptr->get_item_mode & (USE_EQUIP)))
            {
                tmp_val[0] = index_to_label(i);
                tmp_val[1] = ')';
            }
        }

        if ((p_ptr->command_wrk == 0) || (p_ptr->command_wrk & (USE_EQUIP)))
            attr = TERM_WHITE;
        else
            attr = TERM_SLATE;

        Term_erase(0, i - INVEN_WIELD, 255);

        if (use_story_font)
            story_print_text(i - INVEN_WIELD, 0, 3, attr, tmp_val);
        else
            Term_putstr(0, i - INVEN_WIELD, 3, attr, tmp_val);

        if (!o_ptr->tval)
        {
            Term_putch(3, i - INVEN_WIELD, attr, ' ');
            if (use_bigtile)
            {
                Term_putch(4, i - INVEN_WIELD, attr, ' ');
            }
        }
        else
        {
            Term_putch(3, i - INVEN_WIELD, object_attr(o_ptr), object_char(o_ptr));
            if (use_bigtile)
            {
                Term_putch(4, i - INVEN_WIELD, 255, -1);
            }
        }

        if (o_ptr->tval)
        {
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        }
        else
        {
            SDL_strlcpy(o_name, describe_empty_slot(i), sizeof(o_name));
        }

        int max_desc = w - offset - 1;
        if (show_weights && col > offset)
            max_desc = col - offset - 1;
        if (max_desc < 1) max_desc = 1;
        if (max_desc >= (int)sizeof(o_name)) max_desc = (int)sizeof(o_name) - 1;
        o_name[max_desc] = '\0';
        n = (int)strlen(o_name);

        if (!o_ptr->tval)
            attr = TERM_L_DARK;
        else if (weapon_glows(o_ptr))
            attr = object_display_color(o_ptr, TERM_L_BLUE);
        else
            attr = object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

        Term_putch(offset - 1, i - INVEN_WIELD, attr, ' ');
        if (use_story_font)
            story_print_text(i - INVEN_WIELD, offset, max_desc, attr, o_name);
        else
            Term_putstr(offset, i - INVEN_WIELD, n, attr, o_name);

        if (o_ptr->weight)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            strnfmt(tmp_val, sizeof(tmp_val), "%3d.%1d lb ", wgt / 10,
                wgt % 10);
            if ((i >= INVEN_BODY) && (i <= INVEN_FEET))
            {
                if (use_story_font)
                    story_print_text_grid(i - INVEN_WIELD, col, 8, TERM_SLATE, tmp_val);
                else
                    Term_putstr(col, i - INVEN_WIELD, -1, TERM_SLATE, tmp_val);
                armour_weight += wgt;
            }
            else
            {
                if (use_story_font)
                    story_print_text_grid(i - INVEN_WIELD, col, 8, attr, tmp_val);
                else
                    Term_putstr(col, i - INVEN_WIELD, -1, attr, tmp_val);
            }
        }
    }

    if (armour_weight)
    {
        int total_row = INVEN_TOTAL - INVEN_WIELD;
        int text_row = total_row + 1;

        if (use_story_font)
        {
            Term_erase(col, total_row, 255);
            Term_erase(col, text_row, 255);

            story_print_text_grid(total_row, col, 8, TERM_L_DARK, "--------");
            strnfmt(tmp_val, sizeof(tmp_val), "armour: %3d.%1d lb",
                armour_weight / 10, armour_weight % 10);
            {
                int armour_col = col - 8;
                if (armour_col < 0) armour_col = 0;
                story_print_text_grid(text_row, armour_col, 16, TERM_SLATE, tmp_val);
            }
        }
        else
        {
            Term_putstr(col, total_row, -1, TERM_L_DARK, "--------");
            strnfmt(tmp_val, sizeof(tmp_val), "armour: %3d.%1d lb",
                armour_weight / 10, armour_weight % 10);
            {
                int armour_col = col - 8;
                if (armour_col < 0) armour_col = 0;
                Term_putstr(armour_col, text_row, -1, TERM_SLATE, tmp_val);
            }
        }
    }

    int erase_start = armour_weight ? (INVEN_TOTAL - INVEN_WIELD + 2) : (INVEN_TOTAL - INVEN_WIELD);
    for (i = erase_start; i < Term->hgt; i++)
    {
        Term_erase(0, i, 255);
    }

    story_font_term_pop(&story_state);
}


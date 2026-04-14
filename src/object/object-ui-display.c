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
    char glyph_buf[2];
    byte glyph_attr;

    if (use_graphics != GRAPHICS_NONE && use_graphics != GRAPHICS_PSEUDO && o_ptr && o_ptr->k_idx)
    {
        glyph_buf[0] = object_char_default(o_ptr);
        glyph_buf[1] = '\0';
        glyph_attr = object_display_color(o_ptr, object_attr_default(o_ptr));
        story_print_text_grid(y, x, 1, glyph_attr, glyph_buf);

        if (use_bigtile)
        {
            story_print_text_grid(y, x + 1, 1, TERM_WHITE, " ");
            return x + 2;
        }

        return x + 1;
    }

    return x;
}

static void clear_item_row_segment(int row, int col, int width)
{
    if (width <= 0)
        return;

    story_print_text(row, col, width, TERM_WHITE, "");
}

static void clear_item_row(int row, int term_wid)
{
    if (term_wid <= 0)
        return;

    clear_item_row_segment(row, 0, term_wid);
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

static int subwindow_weight_col(int term_wid)
{
    int col = term_wid - 11;

    if (col < 0)
        col = 0;

    return col;
}

static void draw_subwindow_item_icon(int row, int icon_col,
    const object_type* o_ptr)
{
    char icon_buf[2];

    if (!o_ptr || !o_ptr->k_idx)
        return;

    if (use_graphics != GRAPHICS_NONE && use_graphics != GRAPHICS_PSEUDO)
    {
        (void)draw_item_tile(icon_col, row, (object_type*)o_ptr);
        return;
    }

    icon_buf[0] = object_char(o_ptr);
    icon_buf[1] = '\0';
    story_print_text_grid(row, icon_col, 1, object_attr(o_ptr), icon_buf);
}

static int subwindow_desc_col(int icon_col)
{
    return icon_col + (use_bigtile ? 3 : 2);
}

static void truncate_display_desc(char* desc, size_t desc_size, int term_wid,
    int desc_col, bool display_weights)
{
    int max_desc;
    int weight_col = subwindow_weight_col(term_wid);

    if (!desc || !desc_size)
        return;

    max_desc = term_wid - desc_col - 1;
    if (display_weights && weight_col > desc_col)
        max_desc = weight_col - desc_col - 1;
    if (max_desc < 1)
        max_desc = 1;
    if (max_desc >= (int)desc_size)
        max_desc = (int)desc_size - 1;
    desc[max_desc] = '\0';
}

static void display_inventory_subwindow_row(int row, int term_wid,
    cptr label_text, byte label_attr, const object_type* o_ptr)
{
    char o_name[80];
    char weight_text[16];
    byte desc_attr;
    int desc_col;
    int weight_col = subwindow_weight_col(term_wid);

    if (!o_ptr || !o_ptr->k_idx)
        return;

    clear_item_row(row, term_wid);
    story_print_text(row, 0, 3, label_attr, label_text ? label_text : "");

    draw_subwindow_item_icon(row, 3, o_ptr);
    desc_col = subwindow_desc_col(3);

    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
    truncate_display_desc(o_name, sizeof(o_name), term_wid, desc_col,
        show_weights);

    if (weapon_glows(o_ptr))
        desc_attr = object_display_color(o_ptr, TERM_L_BLUE);
    else
        desc_attr = object_display_color(o_ptr,
            tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

    story_print_text(row, desc_col, term_wid - desc_col, desc_attr, o_name);

    if (o_ptr->weight)
    {
        int wgt = o_ptr->weight * o_ptr->number;
        strnfmt(weight_text, sizeof(weight_text), "%3d.%1d lb", wgt / 10,
            wgt % 10);
        story_print_text_grid(row, weight_col, 8, desc_attr, weight_text);
    }
}

static int display_equipment_subwindow_row(int row, int slot, int term_wid,
    cptr label_text, byte label_attr, const object_type* o_ptr)
{
    char o_name[80];
    char weight_text[16];
    byte desc_attr;
    int desc_col;
    int weight_col = subwindow_weight_col(term_wid);
    int armour_weight = 0;

    clear_item_row(row, term_wid);
    story_print_text(row, 0, 3, label_attr, label_text ? label_text : "");

    draw_subwindow_item_icon(row, 3, o_ptr);
    desc_col = subwindow_desc_col(3);

    if (o_ptr && o_ptr->tval)
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
    else
        SDL_strlcpy(o_name, describe_empty_slot(slot), sizeof(o_name));
    truncate_display_desc(o_name, sizeof(o_name), term_wid, desc_col,
        show_weights);

    if (!o_ptr || !o_ptr->tval)
        desc_attr = TERM_L_DARK;
    else if (weapon_glows(o_ptr))
        desc_attr = object_display_color(o_ptr, TERM_L_BLUE);
    else
        desc_attr = object_display_color(o_ptr,
            tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

    story_print_text(row, desc_col, term_wid - desc_col, desc_attr, o_name);

    if (o_ptr && o_ptr->weight)
    {
        int wgt = o_ptr->weight * o_ptr->number;
        byte weight_attr = desc_attr;

        strnfmt(weight_text, sizeof(weight_text), "%3d.%1d lb ", wgt / 10,
            wgt % 10);
        if ((slot >= INVEN_BODY) && (slot <= INVEN_FEET))
        {
            weight_attr = TERM_SLATE;
            armour_weight = wgt;
        }
        story_print_text_grid(row, weight_col, 9, weight_attr, weight_text);
    }

    return armour_weight;
}

/*
 * Legacy subwindow renderer for PW_INVEN. SDL snapshot item selection no
 * longer draws through this helper.
 */
void display_inven(void)
{
    register int i, z = 0;
    byte label_attr;
    bool use_story_font = story_inventory_enabled();
    story_font_term_state story_state;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    bool floor_item = false;

    story_font_term_push(use_story_font, false, &story_state);

    for (i = 0; i < INVEN_PACK; i++)
    {
        if (!inventory[i].k_idx)
            continue;
        z = i + 1;
    }

    for (i = 0; i < z; i++)
    {
        char label_text[4] = "   ";
        object_type* o_ptr = &inventory[i];

        if (item_tester_okay(o_ptr))
        {
            if ((p_ptr->get_item_mode == 0)
                || (p_ptr->get_item_mode & (USE_INVEN)))
            {
                label_text[0] = index_to_label(i);
                label_text[1] = ')';
            }
        }

        if ((p_ptr->command_wrk == 0) || (p_ptr->command_wrk & (USE_INVEN)))
            label_attr = TERM_WHITE;
        else
            label_attr = TERM_SLATE;

        display_inventory_subwindow_row(i, term_wid, label_text, label_attr,
            o_ptr);
    }

    {
        object_type* floor_o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];

        if (floor_o_ptr->k_idx)
        {
            char label_text[4] = "   ";

            floor_item = true;
            if (item_tester_okay(floor_o_ptr)
                && ((p_ptr->get_item_mode == 0)
                    || (p_ptr->get_item_mode & (USE_FLOOR))))
            {
                label_text[0] = '-';
                label_text[1] = ')';
            }

            if ((p_ptr->command_wrk == 0) || (p_ptr->command_wrk & (USE_INVEN)))
                label_attr = TERM_WHITE;
            else
                label_attr = TERM_SLATE;

            display_inventory_subwindow_row(INVEN_WIELD, term_wid, label_text,
                label_attr, floor_o_ptr);
        }
    }

    for (i = z; i < Term->hgt; i++)
    {
        if ((i != INVEN_WIELD) || !floor_item)
            clear_item_row(i, term_wid);
    }

    story_font_term_pop(&story_state);
}

/*
 * Legacy subwindow renderer for PW_EQUIP. SDL snapshot item selection no
 * longer draws through this helper.
 */
void display_equip(void)
{
    register int i;
    byte label_attr;
    int armour_weight = 0;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    int weight_col = subwindow_weight_col(term_wid);

    bool use_story_font = story_equipment_enabled();
    story_font_term_state story_state;
    story_font_term_push(use_story_font, false, &story_state);

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        char label_text[4] = "   ";
        object_type* o_ptr = &inventory[i];

        if (item_tester_okay(o_ptr))
        {
            if ((p_ptr->get_item_mode == 0)
                || (p_ptr->get_item_mode & (USE_EQUIP)))
            {
                label_text[0] = index_to_label(i);
                label_text[1] = ')';
            }
        }

        if ((p_ptr->command_wrk == 0) || (p_ptr->command_wrk & (USE_EQUIP)))
            label_attr = TERM_WHITE;
        else
            label_attr = TERM_SLATE;

        armour_weight += display_equipment_subwindow_row(i - INVEN_WIELD, i,
            term_wid, label_text, label_attr, o_ptr);
    }

    if (armour_weight)
    {
        int total_row = INVEN_TOTAL - INVEN_WIELD;
        int text_row = total_row + 1;
        char tmp_val[80];

        clear_item_row(total_row, term_wid);
        clear_item_row(text_row, term_wid);
        story_print_text_grid(total_row, weight_col, 8, TERM_L_DARK,
            "--------");
        strnfmt(tmp_val, sizeof(tmp_val), "armour: %3d.%1d lb",
            armour_weight / 10, armour_weight % 10);
        {
            int armour_col = weight_col - 8;
            if (armour_col < 0)
                armour_col = 0;
            story_print_text_grid(text_row, armour_col, 16, TERM_SLATE,
                tmp_val);
        }
    }

    int erase_start = armour_weight ? (INVEN_TOTAL - INVEN_WIELD + 2)
        : (INVEN_TOTAL - INVEN_WIELD);
    for (i = erase_start; i < Term->hgt; i++)
        clear_item_row(i, term_wid);

    story_font_term_pop(&story_state);
}


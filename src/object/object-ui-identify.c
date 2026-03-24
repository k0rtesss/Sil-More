/* File: object-ui-identify.c */

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
#include "object/object-desc.h"
#include "object/object-display.h"
#include "object/object-slot.h"
#include "object/object-ui-display.h"
#include "object/object-ui-identify.h"
#include "supplies.h"

typedef enum
{
    IDENT_ENTRY_INVEN,
    IDENT_ENTRY_EQUIP,
    IDENT_ENTRY_FLOOR,
    IDENT_ENTRY_SUPPLY
} ident_entry_type;

typedef struct
{
    ident_entry_type type;
    int index;
    int supply_index;
    int floor_o_idx;
    object_type* o_ptr;
    char label[6];
    char prefix[24];
    char desc[80];
    byte color;
} ident_entry;

#define MAX_IDENT_SUPPLY 256
#define MAX_IDENT_ENTRIES (INVEN_PACK + (INVEN_TOTAL - INVEN_WIELD) + MAX_FLOOR_STACK + MAX_IDENT_SUPPLY)

static void build_ident_entry_label(int order, char out[6])
{
    char label = index_to_label(order);
    out[0] = label;
    out[1] = ')';
    out[2] = '\0';
}

static void draw_ident_line(const ident_entry* entry, int row, int col,
    int weight_col, bool highlight)
{
    byte attr = highlight ? TERM_L_BLUE : entry->color;
    byte label_attr = highlight ? TERM_L_BLUE : TERM_WHITE;
    int offset = col + 3;
    char weight_buf[16];

    prt("", row, col);

    if (highlight)
        c_put_str(label_attr, entry->label, row, col);
    else
        put_str(entry->label, row, col);

    if (entry->prefix[0] != '\0')
    {
        if (highlight)
            c_put_str(attr, entry->prefix, row, offset);
        else
            put_str(entry->prefix, row, offset);
        offset += (int)strlen(entry->prefix);
    }

    c_put_str(attr, entry->desc, row, offset);

    if (show_weights)
    {
        int wgt = entry->o_ptr->weight * entry->o_ptr->number;
        if (entry->o_ptr->weight || entry->type != IDENT_ENTRY_EQUIP)
        {
            strnfmt(weight_buf, sizeof(weight_buf), "%2d.%1d lb", wgt / 10,
                wgt % 10);
            c_put_str(attr, weight_buf, row, weight_col);
        }
    }
}

bool display_unified_identify_menu(bool include_floor, int* out_item,
    object_type** out_object)
{
    ident_entry entries[MAX_IDENT_ENTRIES];
    int entry_count = 0;
    int term_wid = menu_term_width();
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int weight_col = menu_weight_col_for_width(term_wid);
    int len = 29;
    const int base_lim = term_wid - 3;
    const int lim_no_weight = base_lim - (show_weights ? 9 : 0);
    int floor_list[MAX_FLOOR_STACK];
    int floor_num = 0;
    int supply_count = supplies_entry_count();

    if (include_floor)
        floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py,
            p_ptr->px, 0x00);

    if (include_floor)
    {
        for (int i = 0; i < floor_num && entry_count < MAX_IDENT_ENTRIES; i++)
        {
            int o_idx = floor_list[i];
            object_type* o_ptr = &o_list[o_idx];
            if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
                continue;

            ident_entry* entry = &entries[entry_count];
            entry->type = IDENT_ENTRY_FLOOR;
            entry->index = 0;
            entry->supply_index = -1;
            entry->floor_o_idx = o_idx;
            entry->o_ptr = o_ptr;
            strnfmt(entry->label, sizeof(entry->label), "-)");
            entry->prefix[0] = '\0';

            object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
            if (lim_no_weight >= 0)
                entry->desc[lim_no_weight] = '\0';

            entry->color = weapon_glows(o_ptr)
                ? object_display_color(o_ptr, TERM_L_BLUE)
                : object_display_color(o_ptr,
                    tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

            int row_len = (int)strlen(entry->desc) + 5;
            if (show_weights)
                row_len += 9;
            if (row_len > len)
                len = row_len;

            entry_count++;
        }
    }

    for (int i = 0; i < supply_count && entry_count < MAX_IDENT_ENTRIES; i++)
    {
        object_type* o_ptr = supplies_entry_at(i);
        if (!o_ptr || !o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
            continue;

        ident_entry* entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_SUPPLY;
        entry->index = i;
        entry->supply_index = i;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, entry->label);

        const char* supply_prefix = "Supplies: ";
        if (o_ptr->tval == TV_POTION)
            supply_prefix = "Supplies (potions): ";
        else if (o_ptr->tval == TV_GEM)
            supply_prefix = "Supplies (gems): ";
        else if (o_ptr->tval == TV_FOOD && o_ptr->sval <= SV_FOOD_SICKNESS)
            supply_prefix = "Supplies (herbs): ";

        strnfmt(entry->prefix, sizeof(entry->prefix), "%s", supply_prefix);

        int prefix_len = (int)strlen(entry->prefix);
        int desc_lim = lim_no_weight - prefix_len;
        if (desc_lim < 0)
            desc_lim = 0;

        object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
        entry->desc[desc_lim] = '\0';

        entry->color = weapon_glows(o_ptr)
            ? object_display_color(o_ptr, TERM_L_BLUE)
            : object_display_color(o_ptr,
                tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

        int row_len = prefix_len + (int)strlen(entry->desc) + 5;
        if (show_weights && o_ptr->weight)
            row_len += 9;
        if (row_len > len)
            len = row_len;

        entry_count++;
    }

    for (int i = 0; i < INVEN_PACK && entry_count < MAX_IDENT_ENTRIES; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
            continue;

        ident_entry* entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_INVEN;
        entry->index = i;
        entry->supply_index = -1;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, entry->label);
        entry->prefix[0] = '\0';

        object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
        if (lim_no_weight >= 0)
            entry->desc[lim_no_weight] = '\0';

        entry->color = weapon_glows(o_ptr)
            ? TERM_L_BLUE
            : tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];

        int row_len = (int)strlen(entry->desc) + 5;
        if (show_weights)
            row_len += 9;
        if (row_len > len)
            len = row_len;

        entry_count++;
    }

    for (int i = INVEN_WIELD; i < INVEN_TOTAL && entry_count < MAX_IDENT_ENTRIES;
        i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
            continue;

        ident_entry* entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_EQUIP;
        entry->index = i;
        entry->supply_index = -1;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, entry->label);
        strnfmt(entry->prefix, sizeof(entry->prefix), "%-12s: ",
            mention_use(i));

        int prefix_len = (int)strlen(entry->prefix);
        int desc_lim = lim_no_weight - prefix_len;
        if (desc_lim < 0)
            desc_lim = 0;

        object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
        entry->desc[desc_lim] = '\0';

        entry->color = tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];

        int row_len = prefix_len + (int)strlen(entry->desc) + 5;
        if (show_weights && o_ptr->weight)
            row_len += 9;
        if (row_len > len)
            len = row_len;

        entry_count++;
    }

    if (entry_count == 0)
    {
        msg_print("There is nothing unidentified here.");
        return false;
    }

    int col = menu_center_col_for_len(term_wid, len);
    int highlight = 0;
    bool done = false;
    bool success = false;

    screen_save();

    int clear_start = (col > 1) ? (col - 2) : col;
    int clear_width = term_wid - clear_start;
    if (clear_width < 0)
        clear_width = 0;
    int base_rows = MIN(term_hgt - 1, entry_count + 1);
    int rows_to_clear = base_rows;

    log_trace(
        "display_unified_identify_menu: init clear entry_count=%d, start_col=%d, width=%d, rows=%d",
        entry_count, clear_start, clear_width, rows_to_clear);

    while (!done)
    {
        Term_erase(0, 0, 255);

        for (int row = 1; row <= rows_to_clear && row < term_hgt; row++)
            Term_erase(clear_start, row, clear_width);

        log_trace(
            "display_unified_identify_menu: redraw cleared rows 1-%d from col %d width %d",
            MIN(rows_to_clear, term_hgt - 1), clear_start, clear_width);

        char prompt[80];
        strnfmt(prompt, sizeof(prompt),
            "Identify: Space, <- Inspect, ESC to cancel");
        prt(prompt, 0, 0);

        for (int i = 0; i < entry_count; i++)
            draw_ident_line(&entries[i], i + 1, col, weight_col,
                (i == highlight));

        if (entry_count && entry_count < term_hgt - 1)
            prt("", entry_count + 1, col);

        rows_to_clear = base_rows;

        int key = inkey();

        switch (key)
        {
        case ESCAPE:
            done = true;
            success = false;
            break;

        case '8':
        case 'k':
        case 'K':
            highlight = (highlight + entry_count - 1) % entry_count;
            break;

        case '2':
        case 'j':
        case 'J':
            highlight = (highlight + 1) % entry_count;
            break;

        case '4':
        case 'h':
        case 'H':
            object_info_screen(entries[highlight].o_ptr);
            break;

        case ' ':
        case 13:
        case 10:
            success = true;
            done = true;
            break;

        default:
            bell("Invalid command!");
            break;
        }
    }

    screen_load();

    if (!success)
        return false;

    ident_entry* chosen = &entries[highlight];

    if (chosen->type == IDENT_ENTRY_FLOOR)
    {
        *out_item = 0 - chosen->floor_o_idx;
        *out_object = &o_list[chosen->floor_o_idx];
    }
    else if (chosen->type == IDENT_ENTRY_SUPPLY)
    {
        *out_item = SUPPLIES_INDEX + chosen->supply_index;
        *out_object = chosen->o_ptr;
    }
    else
    {
        *out_item = chosen->index;
        *out_object = &inventory[chosen->index];
    }

    return true;
}

#undef MAX_IDENT_ENTRIES

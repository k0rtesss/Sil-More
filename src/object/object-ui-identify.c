/* File: object-ui-identify.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "angband.h"
#include "log/log.h"
#include "object/object-desc.h"
#include "object/object-display.h"
#include "object/object-slot.h"
#include "object/object-ui-display.h"
#include "object/object-ui-identify.h"
#include "platform-input.h"
#include "supplies.h"
#include "ui/ui-information-scene.h"

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

static void ident_entry_build_row_label(const ident_entry* entry, char* buf,
    size_t buf_size)
{
    if (!buf || buf_size == 0)
        return;

    if (!entry)
    {
        buf[0] = '\0';
        return;
    }

    if (entry->prefix[0] != '\0')
        strnfmt(buf, buf_size, "%s%s", entry->prefix, entry->desc);
    else
        strnfmt(buf, buf_size, "%s", entry->desc);
}

static void ident_entry_build_row_key(const ident_entry* entry, char* buf,
    size_t buf_size)
{
    if (!buf || buf_size == 0)
        return;

    buf[0] = '\0';
    if (!entry || !entry->label[0])
        return;

    strnfmt(buf, buf_size, "%c", entry->label[0]);
}

static void ident_entry_build_row_meta(const ident_entry* entry, char* buf,
    size_t buf_size)
{
    if (!buf || buf_size == 0)
        return;

    buf[0] = '\0';
    if (!entry || !show_weights || !entry->o_ptr)
        return;

    if (entry->o_ptr->weight || entry->type != IDENT_ENTRY_EQUIP)
    {
        int wgt = entry->o_ptr->weight * entry->o_ptr->number;

        strnfmt(buf, buf_size, "%2d.%1d lb", wgt / 10, wgt % 10);
    }
}

static void ident_entry_append_detail(app_ui_panel* panel,
    const ident_entry* entry)
{
    char buf[APP_UI_TEXT_MAX];

    if (!panel || !entry || !entry->o_ptr)
        return;

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected");
    ident_entry_build_row_label(entry, buf, sizeof(buf));
    (void)app_ui_panel_add_detail_line(panel, entry->color, buf);

    if (entry->type == IDENT_ENTRY_FLOOR)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            "Location: floor");
    }
    else if (entry->type == IDENT_ENTRY_SUPPLY)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            "Location: shared supplies");
    }
    else if (entry->type == IDENT_ENTRY_EQUIP)
    {
        strnfmt(buf, sizeof(buf), "Slot: %s", mention_use(entry->index));
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "Slot: %c", entry->label[0]);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
    }

    ident_entry_build_row_meta(entry, buf, sizeof(buf));
    if (buf[0])
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);

    (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
        "Inspect before identifying.");
}

static bool display_unified_identify_menu_scene_build(app_ui_scene* scene,
    const ident_entry* entries, int entry_count, int highlight)
{
    app_ui_panel* panel;
    bool steamdeck = steamdeck_controls_active();
    int i;

    if (!scene || !entries || entry_count <= 0)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 860, 1320);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Identify");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Unidentified items");
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Select an item to identify or inspect.");

    for (i = 0; i < entry_count; i++)
    {
        char key[APP_UI_KEY_MAX];
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        byte row_attr = (i == highlight) ? TERM_L_BLUE : entries[i].color;
        byte meta_attr = (i == highlight) ? TERM_L_BLUE : TERM_SLATE;
        byte icon_attr = 0;
        char icon_char = '\0';

        ident_entry_build_row_key(&entries[i], key, sizeof(key));
        ident_entry_build_row_label(&entries[i], label, sizeof(label));
        ident_entry_build_row_meta(&entries[i], meta, sizeof(meta));
        if (entries[i].o_ptr && entries[i].o_ptr->k_idx)
        {
            icon_attr = object_attr(entries[i].o_ptr);
            icon_char = object_char(entries[i].o_ptr);
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, row_attr, meta_attr,
                icon_attr, icon_char, true, i == highlight, key, label, meta))
        {
            return false;
        }
    }

    ident_entry_append_detail(panel, &entries[highlight]);

    if (steamdeck)
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "D-pad", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "A", "Identify");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "RS", "Inspect");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "B", "Cancel");
    }
    else
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Enter", "Identify");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "4/x", "Inspect");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            "Esc", "Cancel");
    }

    return true;
}

typedef enum ident_scene_result {
    IDENT_SCENE_ERROR = -1,
    IDENT_SCENE_CANCEL = 0,
    IDENT_SCENE_SELECT = 1
} ident_scene_result;

static char ident_scene_scroll_command_key(const app_ui_command* command)
{
    if (!command)
        return '\0';
    if (ABS(command->scroll_y) >= ABS(command->scroll_x)
        && command->scroll_y != 0)
    {
        return (command->scroll_y > 0) ? '8' : '2';
    }
    if (command->scroll_x != 0)
        return (command->scroll_x < 0) ? '4' : '6';

    return '\0';
}

static bool ident_scene_set_highlight(int entry_count, int* highlight,
    int row)
{
    if (!highlight || row < 0 || row >= entry_count)
        return false;

    *highlight = row;
    return true;
}

static bool ident_scene_command_to_key(const app_ui_command* command,
    int entry_count, int* highlight, char* out_key)
{
    const app_ui_widget_ref* target;

    if (out_key)
        *out_key = '\0';
    if (!command || !out_key)
        return false;

    target = &command->target;

    if (command->kind == APP_UI_COMMAND_KIND_CANCEL
        || target->action == APP_UI_WIDGET_ACTION_CANCEL)
    {
        *out_key = ESCAPE;
        return true;
    }

    if (command->kind == APP_UI_COMMAND_KIND_SCROLL
        || target->role == APP_UI_WIDGET_ROLE_SCROLL_REGION)
    {
        *out_key = ident_scene_scroll_command_key(command);
        return true;
    }

    if (target->role == APP_UI_WIDGET_ROLE_LIST_ITEM)
    {
        (void)ident_scene_set_highlight(entry_count, highlight,
            target->widget_id);
        if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
            return true;
        if (command->kind == APP_UI_COMMAND_KIND_INSPECT
            || command->kind == APP_UI_COMMAND_KIND_CONTEXT
            || target->action == APP_UI_WIDGET_ACTION_INSPECT)
        {
            *out_key = 'x';
            return true;
        }
        *out_key = '\r';
        return true;
    }

    if (target->role == APP_UI_WIDGET_ROLE_BUTTON)
    {
        if (command->kind == APP_UI_COMMAND_KIND_FOCUS)
            return true;

        switch (target->widget_id)
        {
        case 2:
            *out_key = '\r';
            return true;
        case 3:
            *out_key = 'x';
            return true;
        case 4:
            *out_key = ESCAPE;
            return true;
        default:
            return true;
        }
    }

    return false;
}

static ident_scene_result display_unified_identify_menu_scene(
    const ident_entry* entries, int entry_count, int* highlight_io)
{
    ui_information_scene_scope scope;
    bool steamdeck = steamdeck_controls_active();
    int highlight = 0;

    if (!entries || entry_count <= 0 || !highlight_io)
        return IDENT_SCENE_ERROR;

    if (*highlight_io >= 0 && *highlight_io < entry_count)
        highlight = *highlight_io;
    if (!ui_information_scene_enter(&scope))
        return IDENT_SCENE_ERROR;

    while (true)
    {
        app_ui_scene scene;
        int ch;
        int dir;

        if (!display_unified_identify_menu_scene_build(&scene, entries,
                entry_count, highlight)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return IDENT_SCENE_ERROR;
        }

        {
            ui_information_scene_event event;
            char command_key = '\0';

            ch = 0;
            if (!ui_information_scene_wait_event(&event, 0))
            {
                ch = ESCAPE;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_KEY)
            {
                ch = event.key;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND
                && ident_scene_command_to_key(&event.command, entry_count,
                    &highlight, &command_key))
            {
                ch = command_key;
                if (!ch)
                    continue;
            }
            else if (event.kind == UI_INFORMATION_SCENE_EVENT_COMMAND
                && !ch)
            {
                continue;
            }
        }
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        else if (steamdeck && ch == steamdeck_confirm_key())
            ch = '\r';
        else if (steamdeck && ch == steamdeck_info_key())
            ch = 'x';

        if (ch == ESCAPE)
        {
            *highlight_io = highlight;
            ui_information_scene_leave(&scope);
            return IDENT_SCENE_CANCEL;
        }

        if (ch == ' ' || ch == '\r' || ch == '\n')
        {
            *highlight_io = highlight;
            ui_information_scene_leave(&scope);
            return IDENT_SCENE_SELECT;
        }

        if (ch == '4' || ch == 'h' || ch == 'H' || ch == 'x' || ch == 'X')
        {
            *highlight_io = highlight;
            ui_information_scene_leave(&scope);
            (void)player_try_identify_smithing_object_on_examine(
                entries[highlight].o_ptr,
                (entries[highlight].type == IDENT_ENTRY_EQUIP));
            object_info_screen(entries[highlight].o_ptr);
            if (!ui_information_scene_enter(&scope))
                return IDENT_SCENE_ERROR;
            continue;
        }

        dir = target_dir((char)ch);
        if (dir == 8)
        {
            highlight = (highlight + entry_count - 1) % entry_count;
        }
        else if (dir == 2)
        {
            highlight = (highlight + 1) % entry_count;
        }
        else if (dir == 4)
        {
            *highlight_io = highlight;
            ui_information_scene_leave(&scope);
            (void)player_try_identify_smithing_object_on_examine(
                entries[highlight].o_ptr,
                (entries[highlight].type == IDENT_ENTRY_EQUIP));
            object_info_screen(entries[highlight].o_ptr);
            if (!ui_information_scene_enter(&scope))
                return IDENT_SCENE_ERROR;
        }
    }
}

bool display_unified_identify_menu(bool include_floor, int* out_item,
    object_type** out_object)
{
    ident_entry entries[MAX_IDENT_ENTRIES];
    int entry_count = 0;
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

            entry->color = weapon_glows(o_ptr)
                ? object_display_color(o_ptr, TERM_L_BLUE)
                : object_display_color(o_ptr, object_default_text_color(o_ptr));

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

        object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);

        entry->color = weapon_glows(o_ptr)
            ? object_display_color(o_ptr, TERM_L_BLUE)
            : object_display_color(o_ptr, object_default_text_color(o_ptr));

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

        entry->color = weapon_glows(o_ptr)
            ? TERM_L_BLUE
            : object_default_text_color(o_ptr);

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

        object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);

        entry->color = object_default_text_color(o_ptr);

        entry_count++;
    }

    if (entry_count == 0)
    {
        msg_print("There is nothing unidentified here.");
        return false;
    }

    int highlight = 0;
    ident_scene_result scene_result;

    scene_result = display_unified_identify_menu_scene(entries, entry_count,
        &highlight);
    if (scene_result == IDENT_SCENE_CANCEL)
        return false;
    if (scene_result != IDENT_SCENE_SELECT)
    {
        log_warn("identify menu: semantic identify scene unavailable");
        msg_print("Identify menu unavailable.");
        return false;
    }

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

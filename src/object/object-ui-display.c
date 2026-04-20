/* File: object-ui-display.c */
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

#define OBJECT_SUBWINDOW_BODY_LINE_MAX 4
#define OBJECT_SUBWINDOW_ROW_MAX (INVEN_TOTAL + 2)

typedef struct object_subwindow_row_state {
    s16b id;
    byte key_attr;
    byte label_attr;
    byte meta_attr;
    byte icon_attr;
    byte flags;
    char icon_char;
    const object_type* icon_object;
    char key[APP_UI_KEY_MAX];
    char label[APP_UI_LABEL_MAX];
    char meta[APP_UI_META_MAX];
} object_subwindow_row_state;

typedef struct object_subwindow_body_line_state {
    byte attr;
    char text[APP_UI_TEXT_MAX];
} object_subwindow_body_line_state;

typedef struct object_subwindow_content {
    bool use_story_font;
    byte accent_attr;
    char title[APP_UI_TITLE_MAX];
    char subtitle[APP_UI_TEXT_MAX];
    int row_count;
    int body_line_count;
    object_subwindow_row_state rows[OBJECT_SUBWINDOW_ROW_MAX];
    object_subwindow_body_line_state body_lines[OBJECT_SUBWINDOW_BODY_LINE_MAX];
} object_subwindow_content;

static void object_subwindow_content_init(object_subwindow_content* content,
    bool use_story_font, byte accent_attr, cptr title, cptr subtitle)
{
    if (!content)
        return;

    memset(content, 0, sizeof(*content));
    content->use_story_font = use_story_font;
    content->accent_attr = accent_attr;
    SDL_strlcpy(content->title, title ? title : "", sizeof(content->title));
    SDL_strlcpy(content->subtitle, subtitle ? subtitle : "",
        sizeof(content->subtitle));
}

static bool object_subwindow_add_row(object_subwindow_content* content, s16b id,
    byte key_attr, byte label_attr, byte meta_attr,
    const object_type* icon_object, byte icon_attr, char icon_char, cptr key,
    cptr label, cptr meta, bool story_label)
{
    object_subwindow_row_state* row;

    if (!content || content->row_count >= OBJECT_SUBWINDOW_ROW_MAX)
        return false;

    row = &content->rows[content->row_count++];
    memset(row, 0, sizeof(*row));
    row->id = id;
    row->key_attr = key_attr;
    row->label_attr = label_attr;
    row->meta_attr = meta_attr;
    row->icon_attr = icon_attr;
    row->icon_char = icon_char;
    row->icon_object = icon_object;
    if (story_label)
        row->flags |= APP_UI_ITEM_FLAG_STORY_LABEL;
    SDL_strlcpy(row->key, key ? key : "", sizeof(row->key));
    SDL_strlcpy(row->label, label ? label : "", sizeof(row->label));
    SDL_strlcpy(row->meta, meta ? meta : "", sizeof(row->meta));
    return true;
}

static bool object_subwindow_add_body_line(object_subwindow_content* content,
    byte attr, int col, cptr text)
{
    object_subwindow_body_line_state* line;

    (void)col;

    if (!content || !text || !text[0]
        || content->body_line_count >= OBJECT_SUBWINDOW_BODY_LINE_MAX)
    {
        return false;
    }

    line = &content->body_lines[content->body_line_count++];
    memset(line, 0, sizeof(*line));
    line->attr = attr;
    SDL_strlcpy(line->text, text, sizeof(line->text));
    return true;
}

static byte object_subwindow_item_attr(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return TERM_L_DARK;

    if (weapon_glows(o_ptr))
        return object_display_color(o_ptr, TERM_L_BLUE);

    return object_display_color(o_ptr, object_default_text_color(o_ptr));
}

static void object_subwindow_format_weight(char* buf, size_t buf_size,
    const object_type* o_ptr)
{
    int wgt;

    if (!buf || buf_size == 0)
        return;

    buf[0] = '\0';
    if (!o_ptr || !o_ptr->weight)
        return;

    wgt = o_ptr->weight * o_ptr->number;
    strnfmt(buf, buf_size, "%3d.%1d lb", wgt / 10, wgt % 10);
}

static bool object_subwindow_content_build_scene(app_ui_scene* scene,
    const object_subwindow_content* content)
{
    app_ui_panel* panel;
    int i;

    if (!scene || !content)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    if (content->row_count > 0)
        panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = content->accent_attr;
    app_ui_panel_set_widths(panel, 420, 900);
    app_ui_panel_set_title(panel, TERM_L_WHITE, content->title);
    if (content->subtitle[0])
        app_ui_panel_set_subtitle(panel, TERM_SLATE, content->subtitle);

    for (i = 0; i < content->row_count; i++)
    {
        const object_subwindow_row_state* row = &content->rows[i];

        if (!app_ui_panel_add_row_ex(panel, row->id, row->label_attr,
                row->meta_attr, row->icon_attr, row->icon_char, true, false,
                row->key, row->label, row->meta))
        {
            return false;
        }
        panel->rows[panel->row_count - 1].flags |= row->flags;
    }

    for (i = 0; i < content->body_line_count; i++)
    {
        if (!app_ui_panel_add_body_line(panel, content->body_lines[i].attr,
                content->body_lines[i].text))
        {
            return false;
        }
    }

    return true;
}

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

static bool object_build_inventory_subwindow_content(
    object_subwindow_content* content)
{
    int i;
    int z = 0;

    if (!content)
        return false;

    object_subwindow_content_init(content, story_inventory_enabled(),
        TERM_WHITE, "Inventory", "");

    for (i = 0; i < INVEN_PACK; i++)
    {
        if (inventory[i].k_idx)
            z = i + 1;
    }

    for (i = 0; i < z; i++)
    {
        object_type* o_ptr = &inventory[i];
        char key[APP_UI_KEY_MAX] = "";
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        byte key_attr = ((p_ptr->command_wrk == 0)
            || (p_ptr->command_wrk & (USE_INVEN))) ? TERM_WHITE : TERM_SLATE;

        if (!o_ptr->k_idx)
            continue;

        if (item_tester_okay(o_ptr) && ((p_ptr->get_item_mode == 0)
                || (p_ptr->get_item_mode & (USE_INVEN))))
        {
            strnfmt(key, sizeof(key), "%c)", index_to_label(i));
        }

        object_desc(label, sizeof(label), o_ptr, true, 3);
        object_subwindow_format_weight(meta, sizeof(meta), o_ptr);
        if (!object_subwindow_add_row(content, (s16b)i, key_attr,
                object_subwindow_item_attr(o_ptr),
                object_subwindow_item_attr(o_ptr), o_ptr, object_attr(o_ptr),
                object_char(o_ptr), key, label, meta,
                content->use_story_font))
        {
            return false;
        }
    }

    {
        int floor_o_idx = cave_o_idx[p_ptr->py][p_ptr->px];
        object_type* floor_o_ptr = &o_list[floor_o_idx];

        if (floor_o_ptr->k_idx)
        {
            char key[APP_UI_KEY_MAX] = "";
            char label[APP_UI_LABEL_MAX];
            char meta[APP_UI_META_MAX];
            byte key_attr = ((p_ptr->command_wrk == 0)
                || (p_ptr->command_wrk & (USE_INVEN))) ? TERM_WHITE
                                                       : TERM_SLATE;

            if (item_tester_okay(floor_o_ptr) && ((p_ptr->get_item_mode == 0)
                    || (p_ptr->get_item_mode & (USE_FLOOR))))
            {
                SDL_strlcpy(key, "-)", sizeof(key));
            }

            while (content->row_count < INVEN_WIELD)
            {
                if (!object_subwindow_add_row(content,
                        (s16b)content->row_count, TERM_WHITE, TERM_WHITE,
                        TERM_WHITE, NULL, 0, '\0', "", " ", "", false))
                {
                    return false;
                }
            }

            object_desc(label, sizeof(label), floor_o_ptr, true, 3);
            object_subwindow_format_weight(meta, sizeof(meta), floor_o_ptr);
            if (!object_subwindow_add_row(content, (s16b)(0 - floor_o_idx),
                    key_attr, object_subwindow_item_attr(floor_o_ptr),
                    object_subwindow_item_attr(floor_o_ptr), floor_o_ptr,
                    object_attr(floor_o_ptr), object_char(floor_o_ptr), key,
                    label, meta, content->use_story_font))
            {
                return false;
            }
        }
    }

    return true;
}

static bool object_build_equipment_subwindow_content(
    object_subwindow_content* content)
{
    int i;
    int armour_weight = 0;

    if (!content)
        return false;

    object_subwindow_content_init(content, story_equipment_enabled(),
        TERM_WHITE, "Equipment", "");

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        bool has_object = (o_ptr->tval != 0) ? true : false;
        char key[APP_UI_KEY_MAX] = "";
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        byte key_attr = ((p_ptr->command_wrk == 0)
            || (p_ptr->command_wrk & (USE_EQUIP))) ? TERM_WHITE : TERM_SLATE;
        byte label_attr = has_object ? object_subwindow_item_attr(o_ptr)
                                     : TERM_L_DARK;
        byte meta_attr = label_attr;
        byte icon_attr = 0;
        char icon_char = '\0';

        if (item_tester_okay(o_ptr) && ((p_ptr->get_item_mode == 0)
                || (p_ptr->get_item_mode & (USE_EQUIP))))
        {
            strnfmt(key, sizeof(key), "%c)", index_to_label(i));
        }

        if (has_object)
        {
            object_desc(label, sizeof(label), o_ptr, true, 3);
            object_subwindow_format_weight(meta, sizeof(meta), o_ptr);
            icon_attr = object_attr(o_ptr);
            icon_char = object_char(o_ptr);
            if (o_ptr->weight && i >= INVEN_BODY && i <= INVEN_FEET)
            {
                meta_attr = TERM_SLATE;
                armour_weight += o_ptr->weight * o_ptr->number;
            }
        }
        else
        {
            SDL_strlcpy(label, describe_empty_slot(i), sizeof(label));
            meta[0] = '\0';
        }

        if (!object_subwindow_add_row(content, (s16b)i, key_attr, label_attr,
                meta_attr, has_object ? o_ptr : NULL, icon_attr, icon_char, key,
                label, meta, content->use_story_font))
        {
            return false;
        }
    }

    if (armour_weight > 0)
    {
        char line[APP_UI_TEXT_MAX];

        if (!object_subwindow_add_body_line(content, TERM_L_DARK, -1,
                "--------"))
        {
            return false;
        }
        strnfmt(line, sizeof(line), "armour: %3d.%1d lb", armour_weight / 10,
            armour_weight % 10);
        if (!object_subwindow_add_body_line(content, TERM_SLATE, -2, line))
            return false;
    }

    return true;
}

bool build_inventory_subwindow_ui_scene(app_ui_scene* scene)
{
    object_subwindow_content content;

    if (!object_build_inventory_subwindow_content(&content))
        return false;

    return object_subwindow_content_build_scene(scene, &content);
}

bool build_equipment_subwindow_ui_scene(app_ui_scene* scene)
{
    object_subwindow_content content;

    if (!object_build_equipment_subwindow_content(&content))
        return false;

    return object_subwindow_content_build_scene(scene, &content);
}

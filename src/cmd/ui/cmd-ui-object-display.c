/* File: cmd-ui-object-display.c */
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

static bool object_recall_prepare_fake_kind(int k_idx, object_type* o_ptr)
{
    if (!o_ptr || k_idx <= 0 || !z_info || k_idx >= z_info->k_max)
        return false;

    object_wipe(o_ptr);
    object_prep(o_ptr, k_idx);
    return true;
}

static bool object_recall_prepare_title(int k_idx, object_type* o_ptr,
    char* title, size_t title_size)
{
    if (!title || title_size == 0
        || !object_recall_prepare_fake_kind(k_idx, o_ptr))
    {
        return false;
    }

    object_desc_spoil(title, title_size, o_ptr, false, 0);
    return true;
}

static bool object_recall_append_rich_text(app_ui_scene* scene,
    app_ui_panel* panel, byte attr, cptr text)
{
    cptr cursor = text ? text : "";

    if (!scene || !panel || !cursor[0])
        return true;

    while (true)
    {
        char buf[APP_UI_TEXT_MAX];
        cptr newline = strchr(cursor, '\n');
        size_t len = newline ? (size_t)(newline - cursor) : strlen(cursor);

        while (len > 0)
        {
            size_t chunk_len = len;

            if (chunk_len >= sizeof(buf))
                chunk_len = sizeof(buf) - 1u;
            memcpy(buf, cursor, chunk_len);
            buf[chunk_len] = '\0';
            if (!app_ui_panel_add_rich_text(scene, panel, attr, buf))
                return false;
            cursor += chunk_len;
            len -= chunk_len;
        }

        if (!newline)
            break;
        if (!app_ui_panel_begin_rich_paragraph(scene, panel))
            return false;
        cursor = newline + 1;
    }

    return true;
}

static void object_recall_trim_empty_rich_tail(app_ui_scene* scene,
    app_ui_panel* panel)
{
    if (!scene || !panel)
        return;

    while (panel->rich_paragraph_count > 0)
    {
        u16b paragraph_index = (u16b)(panel->rich_paragraph_first
            + panel->rich_paragraph_count - 1);
        app_ui_rich_paragraph* paragraph = &scene->rich_paragraphs[
            paragraph_index];

        if (paragraph->run_count > 0)
            break;

        panel->rich_paragraph_count--;
        if (scene->rich_paragraph_count > paragraph_index)
            scene->rich_paragraph_count = paragraph_index;
    }
}

bool build_object_kind_recall_ui_scene(app_ui_scene* scene, int k_idx,
    cptr prompt, bool overlay_dungeon)
{
    app_ui_panel* panel;
    object_type fake_object;
    char title[APP_UI_TITLE_MAX];
    cptr lore = NULL;

    if (!scene || !object_recall_prepare_title(k_idx, &fake_object, title,
            sizeof(title)))
    {
        return false;
    }

    if (k_text)
        lore = k_text + k_info[k_idx].text;

    app_ui_scene_init(scene);
    if (overlay_dungeon)
        scene->flags |= APP_UI_SCENE_FLAG_USE_BACKDROP;

    panel = app_ui_scene_append_panel(scene,
        overlay_dungeon ? APP_UI_LAYER_TRANSIENT : APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED;
    panel->accent_attr = TERM_SLATE;
    panel->min_width_px = overlay_dungeon ? 900 : 840;
    panel->width_cap_px = overlay_dungeon ? 1600 : 1320;

    app_ui_panel_set_title(panel, TERM_WHITE, title);
    app_ui_panel_set_icon(panel, object_type_attr(k_idx), object_type_char(k_idx));

    if (lore && lore[0])
    {
        if (!object_recall_append_rich_text(scene, panel, TERM_WHITE, lore))
            return false;
    }
    else if (!app_ui_panel_add_body_line(panel, TERM_SLATE, "No lore recorded."))
    {
        return false;
    }

    object_recall_trim_empty_rich_tail(scene, panel);
    if (prompt && prompt[0])
    {
        if (!app_ui_panel_begin_rich_paragraph(scene, panel)
            || !app_ui_panel_add_rich_text(scene, panel, TERM_SLATE, prompt))
        {
            return false;
        }
    }
    object_recall_trim_empty_rich_tail(scene, panel);
    return true;
}



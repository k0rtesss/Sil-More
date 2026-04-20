/* File: cave-map-ui.c */
/*
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

#include "app/app-ui.h"
#include "cave/cave-internal.h"
#include "log/log.h"
#include "ui/ui-information-scene.h"

typedef struct cave_map_bounds {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int explored_wid;
    int explored_hgt;
    bool use_full_map;
} cave_map_bounds;

static bool cave_map_collect_bounds(cave_map_bounds* bounds,
    bool expand_to_viewport);

/*
 * Keep sparse map views from zooming past the current dungeon viewport scale.
 */
static void view_map_expand_bounds_to_viewport(int* min_y, int* max_y,
    int* min_x, int* max_x)
{
    int viewport_min_y;
    int viewport_max_y;
    int viewport_min_x;
    int viewport_max_x;
    int viewport_hgt;
    int viewport_wid;
    int explored_hgt;
    int explored_wid;

    if (!min_y || !max_y || !min_x || !max_x)
        return;

    explored_hgt = *max_y - *min_y + 1;
    explored_wid = *max_x - *min_x + 1;
    if (explored_hgt < 1 || explored_wid < 1)
        return;
    if (SCREEN_HGT <= 0 || SCREEN_WID <= 0)
        return;

    viewport_min_y = p_ptr->wy;
    viewport_min_x = p_ptr->wx;
    if (viewport_min_y < 0)
        viewport_min_y = 0;
    if (viewport_min_x < 0)
        viewport_min_x = 0;

    viewport_max_y = viewport_min_y + SCREEN_HGT - 1;
    viewport_max_x = viewport_min_x + SCREEN_WID - 1;
    if (viewport_max_y >= p_ptr->cur_map_hgt)
        viewport_max_y = p_ptr->cur_map_hgt - 1;
    if (viewport_max_x >= p_ptr->cur_map_wid)
        viewport_max_x = p_ptr->cur_map_wid - 1;
    if (viewport_max_y < viewport_min_y || viewport_max_x < viewport_min_x)
        return;

    viewport_hgt = viewport_max_y - viewport_min_y + 1;
    viewport_wid = viewport_max_x - viewport_min_x + 1;

    if (explored_hgt < viewport_hgt)
    {
        if (viewport_min_y < *min_y)
            *min_y = viewport_min_y;
        if (viewport_max_y > *max_y)
            *max_y = viewport_max_y;
    }
    if (explored_wid < viewport_wid)
    {
        if (viewport_min_x < *min_x)
            *min_x = viewport_min_x;
        if (viewport_max_x > *max_x)
            *max_x = viewport_max_x;
    }
}

static bool cave_map_collect_bounds(cave_map_bounds* bounds,
    bool expand_to_viewport)
{
    int y;

    if (!bounds || p_ptr->cur_map_wid <= 0 || p_ptr->cur_map_hgt <= 0)
        return false;

    memset(bounds, 0, sizeof(*bounds));
    bounds->min_x = p_ptr->cur_map_wid;
    bounds->min_y = p_ptr->cur_map_hgt;

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        int x;

        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!(cave_info[y][x] & CAVE_MARK))
                continue;

            if (x < bounds->min_x)
                bounds->min_x = x;
            if (x > bounds->max_x)
                bounds->max_x = x;
            if (y < bounds->min_y)
                bounds->min_y = y;
            if (y > bounds->max_y)
                bounds->max_y = y;
        }
    }

    bounds->explored_wid = bounds->max_x - bounds->min_x + 1;
    bounds->explored_hgt = bounds->max_y - bounds->min_y + 1;
    if (bounds->explored_wid < 1 || bounds->explored_hgt < 1)
    {
        bounds->min_x = 0;
        bounds->min_y = 0;
        bounds->max_x = p_ptr->cur_map_wid - 1;
        bounds->max_y = p_ptr->cur_map_hgt - 1;
        bounds->explored_wid = p_ptr->cur_map_wid;
        bounds->explored_hgt = p_ptr->cur_map_hgt;
        bounds->use_full_map = true;
    }
    else if (expand_to_viewport)
    {
        view_map_expand_bounds_to_viewport(&bounds->min_y, &bounds->max_y,
            &bounds->min_x, &bounds->max_x);
        bounds->explored_wid = bounds->max_x - bounds->min_x + 1;
        bounds->explored_hgt = bounds->max_y - bounds->min_y + 1;
    }

    if (bounds->explored_wid < 1 || bounds->explored_hgt < 1)
        return false;

    return true;
}

static bool cave_map_add_minimap(app_ui_scene* scene, app_ui_panel* panel,
    bool expand_to_viewport)
{
    cave_map_bounds bounds;
    int player_x;
    int player_y;
    byte player_attr = TERM_L_BLUE;
    app_ui_minimap_cell* cells = NULL;
    bool ok = true;
    int y;

    if (!scene || !panel)
        return false;
    if (!cave_map_collect_bounds(&bounds, expand_to_viewport))
        return true;
    if (((size_t)bounds.explored_wid * (size_t)bounds.explored_hgt)
        > APP_UI_MINIMAP_CELL_MAX)
    {
        return false;
    }

    cells = mem_alloc_array((size_t)bounds.explored_wid
            * (size_t)bounds.explored_hgt,
        app_ui_minimap_cell);
    if (!cells)
        return false;

    for (y = bounds.min_y; y <= bounds.max_y; y++)
    {
        int x;

        for (x = bounds.min_x; x <= bounds.max_x; x++)
        {
            size_t index = (size_t)(y - bounds.min_y)
                * (size_t)bounds.explored_wid + (size_t)(x - bounds.min_x);

            if (!bounds.use_full_map && !(cave_info[y][x] & CAVE_MARK))
            {
                cells[index].attr = TERM_DARK;
                cells[index].ch = ' ';
                cells[index].terrain_attr = 0;
                cells[index].terrain_char = 0;
                continue;
            }

            map_info(y, x, &cells[index].attr, &cells[index].ch,
                &cells[index].terrain_attr, &cells[index].terrain_char);
        }
    }

    if (r_info[0].x_attr)
        player_attr = (byte)(r_info[0].x_attr & 0x0Fu);

    player_x = p_ptr->px - bounds.min_x;
    player_y = p_ptr->py - bounds.min_y;
    ok = app_ui_panel_set_minimap(scene, panel, (u16b)bounds.explored_wid,
        (u16b)bounds.explored_hgt, (s16b)player_x, (s16b)player_y,
        TERM_SLATE, player_attr, cells);
    mem_free_null(cells);
    return ok;
}

static bool cave_map_build_ui_scene(app_ui_scene* scene, cptr title,
    cptr prompt, bool expand_to_viewport, u16b min_width_px,
    u16b width_cap_px)
{
    app_ui_panel* panel;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_MINIMAP;
    panel->accent_attr = TERM_SLATE;
    if (min_width_px || width_cap_px)
        app_ui_panel_set_widths(panel, min_width_px, width_cap_px);
    app_ui_panel_set_title(panel, TERM_WHITE, title ? title : "");

    if (!cave_map_add_minimap(scene, panel, expand_to_viewport))
        return false;
    if (prompt && prompt[0]
        && !app_ui_panel_add_body_line(panel, TERM_SLATE, prompt))
    {
        return false;
    }

    return true;
}

bool build_overhead_subwindow_ui_scene(struct app_ui_scene* scene)
{
    return cave_map_build_ui_scene((app_ui_scene*)scene, "Map", NULL, false,
        420, 900);
}

static bool view_map_build_ui_scene(app_ui_scene* scene, cptr prompt)
{
    return cave_map_build_ui_scene(scene, "Map", prompt, true, 0, 0);
}

/*
 * Display a "small-scale" map of the dungeon.
 *
 * Note that the "player" is always displayed on the map.
 */
void do_cmd_view_map(void)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    cptr prompt = "Hit any key to continue";

    if (!ui_information_scene_enter(&scope))
        return;

    if (!view_map_build_ui_scene(&scene, prompt)
        || !ui_information_scene_present_ui(&scene))
    {
        ui_information_scene_leave(&scope);
        log_warn("map view: semantic scene presentation failed");
        msg_print("Map view unavailable.");
        return;
    }

    (void)ui_information_scene_wait_key_nonrepeat();
    ui_information_scene_leave(&scope);
    do_cmd_redraw();
}

/* File: level-generation-legendary.c */
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
#include "level-generation/level-generation-internal.h"
#include "log/log.h"
#include "metarun/metarun-meta-state.h"

static bool legendary_area_existing_map_has_id(u16b area_id)
{
    if (!legendary_area_id)
        return false;

    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (legendary_area_id[y][x] == area_id)
                return true;
        }
    }

    return false;
}

static bool legendary_area_fit_at(const meta_dungeon_area* area, int y0, int x0)
{
    for (int yy = 0; yy < area->record.hgt; yy++)
    {
        for (int xx = 0; xx < area->record.wid; xx++)
        {
            int y = y0 + yy;
            int x = x0 + xx;
            byte feat;

            if (!meta_dungeon_area_cell_at(area, yy, xx, &feat, NULL, NULL))
                continue;
            if (!in_bounds_fully(y, x))
                return false;
            if (cave_info[y][x] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
                return false;
            if (cave_perma_bold(y, x) || cave_trap_bold(y, x)
                || cave_stair_bold(y, x))
            {
                return false;
            }
            if (cave_o_idx[y][x] || cave_m_idx[y][x])
                return false;
            if (feat == FEAT_WALL_PERM || cave_stair_bold(y, x)
                || cave_trap_bold(y, x))
            {
                return false;
            }
        }
    }

    return true;
}

static void legendary_area_apply_at(const meta_dungeon_area* area, int y0,
    int x0)
{
    int cy = -1;
    int cx = -1;
    int y1 = y0 + area->record.hgt - 1;
    int x1 = x0 + area->record.wid - 1;
    u16b area_id = META_DUNGEON_LEGENDARY_AREA_ID_PRIMARY;

    for (int yy = 0; yy < area->record.hgt; yy++)
    {
        for (int xx = 0; xx < area->record.wid; xx++)
        {
            int y = y0 + yy;
            int x = x0 + xx;
            byte feat;
            byte color;
            byte role;

            if (!meta_dungeon_area_cell_at(area, yy, xx, &feat, &color, &role))
                continue;

            cave_set_feat(y, x, feat);
            cave_color[y][x] = color;
            cave_info[y][x] &=
                ~(CAVE_ROOM | CAVE_ICKY | CAVE_GLOW | CAVE_CHASM_AREA);
            if (role & META_DUNGEON_TILE_ROLE_ROOM)
                cave_info[y][x] |= CAVE_ROOM;
            if (role & META_DUNGEON_TILE_ROLE_ICKY)
                cave_info[y][x] |= CAVE_ICKY;
            if (role & META_DUNGEON_TILE_ROLE_GLOW)
                cave_info[y][x] |= CAVE_GLOW;
            if (role & META_DUNGEON_TILE_ROLE_CHASM)
                cave_info[y][x] |= CAVE_CHASM_AREA;
            legendary_area_id[y][x] = area_id;

            if (cy < 0 && cave_floor_bold(y, x)
                && !cave_any_closed_door_bold(y, x))
            {
                cy = y;
                cx = x;
            }
        }
    }

    if (cy < 0)
    {
        cy = y0 + area->record.singer_y;
        cx = x0 + area->record.singer_x;
        if (!in_bounds_fully(cy, cx))
        {
            cy = y0 + area->record.hgt / 2;
            cx = x0 + area->record.wid / 2;
        }
        cave_set_feat(cy, cx, FEAT_FLOOR);
        legendary_area_id[cy][cx] = area_id;
    }

    if (dun->cent_n < room_capacity_limit())
    {
        int idx = dun->cent_n++;

        dun->corner[idx].y1 = (byte)y0;
        dun->corner[idx].x1 = (byte)x0;
        dun->corner[idx].y2 = (byte)y1;
        dun->corner[idx].x2 = (byte)x1;
        dun->cent[idx].y = (byte)cy;
        dun->cent[idx].x = (byte)cx;
        dun->kind[idx] = ROOM_KIND_CLASSIC;
        dun->is_quest[idx] = false;
    }

    legendary_area_note_spawned(area_id, area);
}

static bool legendary_area_try_place(const meta_dungeon_area* area)
{
    int max_y;
    int max_x;

    if (!area || !legendary_area_map_ensure())
        return false;
    if (legendary_area_existing_map_has_id(
            META_DUNGEON_LEGENDARY_AREA_ID_PRIMARY))
    {
        return false;
    }
    if (!meta_dungeon_record_is_valid(&area->record, area->tile_blob_size))
        return false;
    if (area->record.depth != p_ptr->depth)
        return false;
    if (area->record.hgt >= p_ptr->cur_map_hgt - 2
        || area->record.wid >= p_ptr->cur_map_wid - 2)
    {
        return false;
    }

    max_y = p_ptr->cur_map_hgt - area->record.hgt - 2;
    max_x = p_ptr->cur_map_wid - area->record.wid - 2;
    if (max_y < 1 || max_x < 1)
        return false;

    for (int attempt = 0; attempt < 300; attempt++)
    {
        int y0 = rand_range(1, max_y);
        int x0 = rand_range(1, max_x);

        if (!legendary_area_fit_at(area, y0, x0))
            continue;

        legendary_area_apply_at(area, y0, x0);
        log_debug(
            "legendary area: spawned song=%u depth=%u at (%d,%d) size=%ux%u",
            (unsigned)area->record.song_id, (unsigned)area->record.depth, y0,
            x0, (unsigned)area->record.wid, (unsigned)area->record.hgt);
        return true;
    }

    return false;
}

bool place_legendary_area_for_depth(void)
{
    meta_dungeon_area* areas = NULL;
    u32b count = 0;
    bool placed = false;
    int eligible = 0;
    int pick;

    if (p_ptr->depth <= 0 || p_ptr->depth >= MORGOTH_DEPTH)
        return false;
    if (!meta_dungeon_load_for_current_metarun(&areas, &count))
        return false;

    for (u32b i = 0; i < count; i++)
    {
        if (areas[i].record.depth == p_ptr->depth
            && meta_dungeon_record_is_valid(
                &areas[i].record, areas[i].tile_blob_size))
        {
            eligible++;
        }
    }

    if (eligible <= 0)
        goto cleanup;

    pick = rand_int(eligible);
    for (u32b i = 0; i < count; i++)
    {
        if (areas[i].record.depth != p_ptr->depth
            || !meta_dungeon_record_is_valid(
                &areas[i].record, areas[i].tile_blob_size))
        {
            continue;
        }
        if (pick-- > 0)
            continue;
        placed = legendary_area_try_place(&areas[i]);
        break;
    }

cleanup:
    meta_dungeon_areas_free(areas, count);
    return placed;
}

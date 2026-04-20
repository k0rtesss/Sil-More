/* File: level-generation-layout-morgoth.c */
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
#include "level-generation-internal.h"

bool morgoth_region_active(void)
{
    return morgoth_level_active && morgoth_partition_reserved;
}

bool coord_in_morgoth_region(int y, int x, int margin)
{
    if (!morgoth_region_active())
        return false;

    return (y >= morgoth_partition_bounds.y1 - margin)
        && (y <= morgoth_partition_bounds.y2 + margin)
        && (x >= morgoth_partition_bounds.x1 - margin)
        && (x <= morgoth_partition_bounds.x2 + margin);
}

bool morgoth_segment_blocked(int y1, int x1, int y2, int x2, int margin)
{
    if (!morgoth_region_active())
        return false;

    if (y1 < morgoth_partition_bounds.y1 - margin
        && y2 < morgoth_partition_bounds.y1 - margin)
        return false;
    if (y1 > morgoth_partition_bounds.y2 + margin
        && y2 > morgoth_partition_bounds.y2 + margin)
        return false;
    if (x1 < morgoth_partition_bounds.x1 - margin
        && x2 < morgoth_partition_bounds.x1 - margin)
        return false;
    if (x1 > morgoth_partition_bounds.x2 + margin
        && x2 > morgoth_partition_bounds.x2 + margin)
        return false;

    if (y1 == y2)
    {
        int y = y1;
        int xa = MIN(x1, x2);
        int xb = MAX(x1, x2);
        int rx1 = morgoth_partition_bounds.x1 - margin;
        int rx2 = morgoth_partition_bounds.x2 + margin;
        int ry1 = morgoth_partition_bounds.y1 - margin;
        int ry2 = morgoth_partition_bounds.y2 + margin;

        if (y >= ry1 && y <= ry2 && xb >= rx1 && xa <= rx2)
            return true;
    }

    if (x1 == x2)
    {
        int x = x1;
        int ya = MIN(y1, y2);
        int yb = MAX(y1, y2);
        int rx1 = morgoth_partition_bounds.x1 - margin;
        int rx2 = morgoth_partition_bounds.x2 + margin;
        int ry1 = morgoth_partition_bounds.y1 - margin;
        int ry2 = morgoth_partition_bounds.y2 + margin;

        if (x >= rx1 && x <= rx2 && yb >= ry1 && ya <= ry2)
            return true;
    }

    return false;
}

void reset_morgoth_layout_state(bool active)
{
    morgoth_level_active = active;
    morgoth_partition_reserved = false;
    morgoth_partition_index = -1;
    morgoth_partition_bounds.y1 = 0;
    morgoth_partition_bounds.y2 = 0;
    morgoth_partition_bounds.x1 = 0;
    morgoth_partition_bounds.x2 = 0;
    morgoth_vault_center_y = 0;
    morgoth_vault_center_x = 0;
}

void seal_morgoth_partition(const vault_type* v_ptr, int y0, int x0)
{
    int top_y;
    int bot_y;
    int left_x;
    int right_x;
    int tunnel_limit;
    int margin;
    int y1;
    int y2;
    int x1;
    int x2;

    if (!morgoth_region_active() || !v_ptr)
        return;

    top_y = y0 - v_ptr->hgt / 2;
    bot_y = top_y + v_ptr->hgt - 1;
    left_x = x0 - v_ptr->wid / 2;
    right_x = left_x + v_ptr->wid - 1;

    tunnel_limit = morgoth_partition_bounds.y1 - 2;
    if (tunnel_limit < 1)
        tunnel_limit = 1;

    margin = 4;
    y1 = MAX(1, MIN(morgoth_partition_bounds.y1, tunnel_limit));
    y2 = MIN(morgoth_partition_bounds.y2, bot_y + margin);
    x1 = MAX(morgoth_partition_bounds.x1, left_x - margin);
    x2 = MIN(morgoth_partition_bounds.x2, right_x + margin);

    morgoth_partition_bounds.y1 = y1;
    morgoth_partition_bounds.y2 = y2;
    morgoth_partition_bounds.x1 = x1;
    morgoth_partition_bounds.x2 = x2;

    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (cave_info[y][x] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
                continue;

            if (cave_floor_bold(y, x)
                || (cave_feat[y][x] >= FEAT_DOOR_HEAD
                    && cave_feat[y][x] <= FEAT_DOOR_TAIL))
                continue;

            cave_set_feat(y, x, FEAT_WALL_PERM);
            cave_info[y][x] &= ~(CAVE_ROOM | CAVE_ICKY);
        }
    }
}

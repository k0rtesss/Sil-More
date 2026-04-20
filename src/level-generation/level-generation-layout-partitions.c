/* File: level-generation-layout-partitions.c */
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
#include "level-generation/gen-log.h"
#include "log/log.h"
#include "level-generation/level-generation-internal.h"

/* Carve a chasm area with organic cave shape and islands connected by bridges */
static bool carve_chasm_with_bridges(int y_min, int y_max, int x_min, int x_max,
    int floor_style, int bridge_style)
{
    if (dun->cent_n >= room_capacity_limit())
    {
        genlog_anchor("CHASM: rejected - room capacity limit reached");
        return false;
    }
    
    /* Bounds are inclusive. Keep the local mask dimensions aligned with the
     * generation loops so the temporary cave/platform arrays cover every tile. */
    int avail_h = y_max - y_min + 1;
    int avail_w = x_max - x_min + 1;
    if (avail_h < 16 || avail_w < 20)
    {
        genlog_anchor("CHASM: rejected - bounds too small (%d,%d)-(%d,%d), avail=%dx%d",
                      y_min, x_min, y_max, x_max, avail_h, avail_w);
        return false;
    }
    
    /* Use variable margins to create organic outer boundary */
    int h = avail_h;
    int w = avail_w;
    int y1 = y_min;
    int x1 = x_min;
    int y2 = y_max;
    int x2 = x_max;
    
    /* Check area is basic granite */
    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (in_bounds_fully(y, x) && cave_floor_bold(y, x))
            {
                genlog_anchor("CHASM: rejected - floor already exists at (%d,%d) in bounds (%d,%d)-(%d,%d)",
                              y, x, y1, x1, y2, x2);
                return false;
            }
        }
    }
    
    /* 
     * CHASM GENERATION APPROACH:
     * 1. Use CA to create organic cave boundary (not rectangular)
     * 2. Create multiple platform islands within the cave
     * 3. Fill non-platform areas with chasms
     * 4. Connect platforms with narrow bridges
     */
    
    /* Track what's inside the cave vs wall, and what's platform vs chasm */
    bool* is_cave = mem_alloc_array(h * w, bool);
    bool* is_platform = mem_alloc_array(h * w, bool);
    if (!is_cave || !is_platform) 
    {
        if (is_cave) mem_free(is_cave);
        if (is_platform) mem_free(is_platform);
        return false;
    }
    
    /* Initialize: seed cave shape with multi-center distance + noise */
    int num_cave_centers = 3 + rand_int(3);  /* 3-5 centers for cave shape */
    int cave_cy[6], cave_cx[6];
    for (int c = 0; c < num_cave_centers; ++c)
    {
        cave_cy[c] = rand_range(h / 4, 3 * h / 4);
        cave_cx[c] = rand_range(w / 4, 3 * w / 4);
    }
    
    /* Carve organic cave shape using distance from centers + noise */
    int base_radius = (h + w) / 5;
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            /* Find distance to nearest center */
            int min_dist = 9999;
            for (int c = 0; c < num_cave_centers; ++c)
            {
                int dy = ABS(ly - cave_cy[c]);
                int dx = ABS(lx - cave_cx[c]);
                int dist = dy + (dx * 2 / 3);  /* Wider horizontally */
                if (dist < min_dist) min_dist = dist;
            }
            
            /* Cave extends with noise for organic edges */
            int threshold = base_radius + rand_int(base_radius / 2) - rand_int(base_radius / 3);
            is_cave[ly * w + lx] = (min_dist < threshold);
            is_platform[ly * w + lx] = false;
        }
    }
    
    /* CA smoothing for organic cave boundary */
    bool* next_cave = mem_alloc_array(h * w, bool);
    if (!next_cave) { mem_free(is_cave); mem_free(is_platform); return false; }
    
    for (int step = 0; step < 3; ++step)
    {
        for (int ly = 0; ly < h; ++ly)
        {
            for (int lx = 0; lx < w; ++lx)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0) continue;
                        int ny = ly + dy, nx = lx + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors += 0;  /* Edges are wall */
                        else if (is_cave[ny * w + nx])
                            neighbors++;
                    }
                }
                /* Cave survives with 4+ neighbors, born with 5+ */
                next_cave[ly * w + lx] = is_cave[ly * w + lx] ? (neighbors >= 4) : (neighbors >= 5);
            }
        }
        for (int i = 0; i < h * w; ++i) is_cave[i] = next_cave[i];
    }
    mem_free(next_cave);
    
    /* Ensure cave doesn't touch absolute edges */
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (ly < 2 || ly >= h - 2 || lx < 2 || lx >= w - 2)
                is_cave[ly * w + lx] = false;
        }
    }
    
    /* Now create 5-9 platform islands within the cave area */
    int num_platforms = rand_range(5, 9);
    int plat_cy[10], plat_cx[10], plat_radius[10];
    int platforms_placed = 0;
    int sanctum_cy = -1;
    int sanctum_cx = -1;

    if (!choose_chasm_sanctum_seed(is_cave, h, w, &sanctum_cy, &sanctum_cx))
    {
        mem_free(is_cave);
        mem_free(is_platform);
        genlog_anchor("CHASM: rejected - no buffered central sanctum site");
        return false;
    }

    plat_cy[platforms_placed] = sanctum_cy;
    plat_cx[platforms_placed] = sanctum_cx;
    plat_radius[platforms_placed] = rand_range(3, 4);
    platforms_placed++;
    
    for (int attempt = 0; attempt < 300 && platforms_placed < num_platforms; ++attempt)
    {
        int py = rand_range(4, h - 5);
        int px = rand_range(5, w - 6);
        
        /* Must be inside cave */
        if (!is_cave[py * w + px]) continue;
        
        /* Check distance from other platforms */
        bool too_close = false;
        int min_sep = 5 + rand_int(3);  /* Variable separation */
        for (int i = 0; i < platforms_placed; ++i)
        {
            int dist = ABS(py - plat_cy[i]) + ABS(px - plat_cx[i]);
            if (dist < min_sep)
            {
                too_close = true;
                break;
            }
        }
        if (too_close) continue;
        
        plat_cy[platforms_placed] = py;
        plat_cx[platforms_placed] = px;
        plat_radius[platforms_placed] = rand_range(2, 4);
        platforms_placed++;
    }
    
    /* Create organic platform shapes */
    for (int p = 0; p < platforms_placed; ++p)
    {
        int cy = plat_cy[p];
        int cx = plat_cx[p];
        int base_r = plat_radius[p];
        
        for (int ly = 0; ly < h; ++ly)
        {
            for (int lx = 0; lx < w; ++lx)
            {
                if (!is_cave[ly * w + lx]) continue;
                
                int dy = ABS(ly - cy);
                int dx = ABS(lx - cx);
                int dist = dy + (dx * 2 / 3);
                
                int threshold = base_r + rand_int(2);
                if (dist <= threshold)
                    is_platform[ly * w + lx] = true;
            }
        }
    }

    /* Reserve one buffered 5x5 sanctuary on the center-leaning island so the
     * 3x3 sanctum sits away from chasm edges. */
    for (int dy = -2; dy <= 2; ++dy)
    {
        for (int dx = -2; dx <= 2; ++dx)
        {
            int ly = sanctum_cy + dy;
            int lx = sanctum_cx + dx;

            if (ly < 0 || lx < 0 || ly >= h || lx >= w)
                continue;
            if (!is_cave[ly * w + lx])
                continue;

            is_platform[ly * w + lx] = true;
        }
    }
    
    /* Extend platforms organically */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int ly = 1; ly < h - 1; ++ly)
        {
            for (int lx = 1; lx < w - 1; ++lx)
            {
                if (!is_cave[ly * w + lx]) continue;
                if (is_platform[ly * w + lx]) continue;
                
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if ((dy || dx) && is_platform[(ly+dy) * w + (lx+dx)])
                            adj++;
                
                if (adj >= 3 && one_in_(3))
                    is_platform[ly * w + lx] = true;
            }
        }
    }

    /* Restore the sparse edge nubs that helped the previous bridge layout stay
     * legible without creating a trivial perimeter walkway. */
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (!is_cave[ly * w + lx]) continue;

            bool edge_of_cave = false;
            int adj_platforms = 0;
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = ly + dy, nx = lx + dx;
                    if (ny < 0 || nx < 0 || ny >= h || nx >= w
                        || !is_cave[ny * w + nx])
                        edge_of_cave = true;
                    else if (is_platform[ny * w + nx])
                        adj_platforms++;
                }
            }

            if (edge_of_cave && !is_platform[ly * w + lx]
                && adj_platforms >= 2 && one_in_(4))
            {
                is_platform[ly * w + lx] = true;
            }
        }
    }

    /* Apply to cave: inside cave + platform = floor, inside cave + !platform = chasm */
    int floor_count = 0;
    int chasm_count = 0;
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            int ly = gy - y1, lx = gx - x1;
            
            if (!is_cave[ly * w + lx])
                continue;  /* Leave as granite wall */
            
            if (is_platform[ly * w + lx])
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, floor_style);
                cave_info[gy][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                floor_count++;
            }
            else
            {
                cave_set_feat(gy, gx, FEAT_CHASM);
                cave_info[gy][gx] |= CAVE_CHASM_AREA;
                chasm_count++;
            }
        }
    }
    
    /* Now connect platforms with bridges (MST-style) */
    int global_plat_y[10], global_plat_x[10];
    for (int p = 0; p < platforms_placed; ++p)
    {
        global_plat_y[p] = y1 + plat_cy[p];
        global_plat_x[p] = x1 + plat_cx[p];
    }
    
    bool* connected = mem_alloc_array(platforms_placed, bool);
    if (!connected) { mem_free(is_cave); mem_free(is_platform); return false; }
    for (int i = 0; i < platforms_placed; ++i) connected[i] = false;
    if (platforms_placed > 0) connected[0] = true;
    
    int bridges_built = 0;
    for (int iter = 0; iter < platforms_placed; ++iter)
    {
        int best_from = -1, best_to = -1, best_dist = 9999;
        
        for (int i = 0; i < platforms_placed; ++i)
        {
            if (!connected[i]) continue;
            for (int j = 0; j < platforms_placed; ++j)
            {
                if (connected[j]) continue;
                int dist = distance(global_plat_y[i], global_plat_x[i],
                                   global_plat_y[j], global_plat_x[j]);
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best_from = i;
                    best_to = j;
                }
            }
        }
        
        if (best_to < 0) break;
        
        int sy = global_plat_y[best_from];
        int sx = global_plat_x[best_from];
        int ey = global_plat_y[best_to];
        int ex = global_plat_x[best_to];
        
        /* L-shaped bridge */
        if (one_in_(2))
        {
            int x_lo = MIN(sx, ex), x_hi = MAX(sx, ex);
            for (int gx = x_lo; gx <= x_hi; ++gx)
                if (in_bounds_fully(sy, gx) && cave_feat[sy][gx] == FEAT_CHASM)
                {
                    cave_set_feat_style(sy, gx, FEAT_FLOOR, bridge_style);
                    cave_info[sy][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
            int y_lo = MIN(sy, ey), y_hi = MAX(sy, ey);
            for (int gy = y_lo; gy <= y_hi; ++gy)
                if (in_bounds_fully(gy, ex) && cave_feat[gy][ex] == FEAT_CHASM)
                {
                    cave_set_feat_style(gy, ex, FEAT_FLOOR, bridge_style);
                    cave_info[gy][ex] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
        }
        else
        {
            int y_lo = MIN(sy, ey), y_hi = MAX(sy, ey);
            for (int gy = y_lo; gy <= y_hi; ++gy)
                if (in_bounds_fully(gy, sx) && cave_feat[gy][sx] == FEAT_CHASM)
                {
                    cave_set_feat_style(gy, sx, FEAT_FLOOR, bridge_style);
                    cave_info[gy][sx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
            int x_lo = MIN(sx, ex), x_hi = MAX(sx, ex);
            for (int gx = x_lo; gx <= x_hi; ++gx)
                if (in_bounds_fully(ey, gx) && cave_feat[ey][gx] == FEAT_CHASM)
                {
                    cave_set_feat_style(ey, gx, FEAT_FLOOR, bridge_style);
                    cave_info[ey][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
        }
        
        connected[best_to] = true;
        bridges_built++;
    }

    place_chasm_island_sanctum(y1 + sanctum_cy, x1 + sanctum_cx);
    
    mem_free(connected);
    mem_free(is_cave);
    mem_free(is_platform);
    
    /* Track bounds of just the floor tiles (not chasm) for proper tunnel connectivity */
    int floor_min_y = y2, floor_max_y = y1, floor_min_x = x2, floor_max_x = x1;
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (cave_floor_bold(gy, gx))
            {
                if (gy < floor_min_y) floor_min_y = gy;
                if (gy > floor_max_y) floor_max_y = gy;
                if (gx < floor_min_x) floor_min_x = gx;
                if (gx > floor_max_x) floor_max_x = gx;
            }
        }
    }
    
    /* Set outer walls ONLY around floor tiles (not chasm) for proper tunnel connectivity */
    for (int gy = floor_min_y - 1; gy <= floor_max_y + 1; ++gy)
    {
        for (int gx = floor_min_x - 1; gx <= floor_max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (cave_floor_bold(gy, gx)) continue;
            if (cave_feat[gy][gx] == FEAT_CHASM) continue;  /* Don't convert chasm */
            if (cave_feat[gy][gx] != FEAT_WALL_EXTRA) continue;
            
            /* Only set outer wall if bordering actual floor (not chasm) */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx) &&
                        (cave_info[ny][nx] & CAVE_ROOM))
                        borders_floor = true;
                }
            }
            if (borders_floor)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, floor_style);
        }
    }
    
    /* Find center on a floor tile near an outer wall (better for tunnel connectivity) */
    int cy = (floor_min_y + floor_max_y) / 2;
    int cx = (floor_min_x + floor_max_x) / 2;
    
    /* First try: find floor tile adjacent to outer wall */
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(floor_min_y, floor_max_y);
        int tx = rand_range(floor_min_x, floor_max_x);
        if (!cave_floor_bold(ty, tx)) continue;
        
        /* Check if adjacent to outer wall */
        for (int dy = -1; dy <= 1 && !found_edge; ++dy)
        {
            for (int dx = -1; dx <= 1 && !found_edge; ++dx)
            {
                if (dy == 0 && dx == 0) continue;
                if (in_bounds_fully(ty + dy, tx + dx) && 
                    cave_feat[ty + dy][tx + dx] == FEAT_WALL_OUTER)
                {
                    cy = ty; cx = tx;
                    found_edge = true;
                }
            }
        }
    }
    
    /* Fallback: any floor tile */
    if (!found_edge)
    {
        for (int tries = 0; tries < 100; ++tries)
        {
            int ty = rand_range(floor_min_y, floor_max_y);
            int tx = rand_range(floor_min_x, floor_max_x);
            if (cave_floor_bold(ty, tx))
            {
                cy = ty; cx = tx;
                break;
            }
        }
    }
    
    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    /* Use floor bounds, not full chasm bounds, for tunnel connectivity */
    dun->corner[idx].y1 = floor_min_y;
    dun->corner[idx].x1 = floor_min_x;
    dun->corner[idx].y2 = floor_max_y;
    dun->corner[idx].x2 = floor_max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, false);
    
    log_trace("Chasm organic: %d platforms, %d bridges, %d chasm tiles, floor=(%d,%d)-(%d,%d) center=(%d,%d)",
        platforms_placed, bridges_built, chasm_count, floor_min_y, floor_min_x, floor_max_y, floor_max_x, cy, cx);
    log_trace("Chasm organic extras: sanctum=(%d,%d)",
        y1 + sanctum_cy, x1 + sanctum_cx);
    genlog_anchor("CHASM: %d platforms, %d bridges, %d chasm tiles at (%d,%d)-(%d,%d)",
        platforms_placed, bridges_built, chasm_count, floor_min_y, floor_min_x, floor_max_y, floor_max_x);
    return true;
}

/* Carve a labyrinth-style maze with organic shape using cellular automata */
static bool carve_labyrinth_bounds(int y_min, int y_max, int x_min, int x_max,
    density_level_t density, int style_idx)
{
    if (dun->cent_n >= room_capacity_limit())
    {
        genlog_anchor("LABYRINTH: rejected - room capacity limit reached");
        return false;
    }
    
    int avail_h = y_max - y_min;
    int avail_w = x_max - x_min;
    if (avail_h < 10 || avail_w < 12)
    {
        genlog_anchor("LABYRINTH: rejected - bounds too small (%d,%d)-(%d,%d), avail=%dx%d",
                      y_min, x_min, y_max, x_max, avail_h, avail_w);
        return false;
    }
    
    /* Use small margins to maximize labyrinth size while avoiding partition overlap */
    int margin_y = rand_range(3, 5);
    int margin_x = rand_range(3, 5);
    int y1 = y_min + margin_y;
    int x1 = x_min + margin_x;
    int y2 = y_max - margin_y;
    int x2 = x_max - margin_x;
    int h = y2 - y1 + 1;
    int w = x2 - x1 + 1;
    
    if (h < 8 || w < 10)
    {
        genlog_anchor("LABYRINTH: rejected - after margins too small: h=%d w=%d (margins y=%d x=%d)",
                      h, w, margin_y, margin_x);
        return false;
    }
    
    /* Check area is basic granite - if floor exists, another partition already carved here */
    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (in_bounds_fully(y, x) && cave_floor_bold(y, x))
            {
                genlog_anchor("LABYRINTH: rejected - floor already exists at (%d,%d) in bounds (%d,%d)-(%d,%d)",
                              y, x, y1, x1, y2, x2);
                return false;
            }
        }
    }
    
    /* Use CA to create organic boundary mask - no size caps, use full partition */
    /* Note: h and w already set from margins above, keep them as-is */
    
    bool* mask = mem_alloc_array(h * w, bool);
    if (!mask) return false;
    
    /* Seed with 60% fill for corridors */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            mask[y * w + x] = (rand_int(100) < 60);
    
    /* CA smoothing to create organic boundary */
    bool* next = mem_alloc_array(h * w, bool);
    if (!next) { mem_free(mask); return false; }
    
    for (int step = 0; step < 3; ++step)
    {
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0) continue;
                        int ny = y + dy, nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors++;
                        else if (mask[ny * w + nx])
                            neighbors++;
                    }
                next[y * w + x] = (neighbors >= 4);
            }
        }
        for (int i = 0; i < h * w; ++i) mask[i] = next[i];
    }
    mem_free(next);
    
    /* Carve corridors in a grid pattern, but only within the organic mask */
    int floor_count = 0;
    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    /* Vary corridor spacing by density: sparse=4 (open), normal=3, dense=2 (tight maze) */
    int corridor_spacing = (density == DENSITY_SPARSE) ? 4 : (density == DENSITY_DENSE) ? 2 : 3;
    
    /* Horizontal corridors */
    for (int ly = 1; ly < h - 1; ly += corridor_spacing)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (!mask[ly * w + lx]) continue;
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (in_bounds_fully(gy, gx) && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                cave_info[gy][gx] |= CAVE_ROOM;
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }
    
    /* Vertical corridors */
    for (int lx = 1; lx < w - 1; lx += corridor_spacing)
    {
        for (int ly = 0; ly < h; ++ly)
        {
            if (!mask[ly * w + lx]) continue;
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (in_bounds_fully(gy, gx) && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                cave_info[gy][gx] |= CAVE_ROOM;
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }
    
    mem_free(mask);
    
    /* Block some corridor segments to create dead ends */
    for (int ly = 1; ly < h - 1; ly += corridor_spacing)
    {
        for (int lx = 1; lx < w - 1; lx += corridor_spacing)
        {
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (!in_bounds_fully(gy, gx) || !cave_floor_bold(gy, gx))
                continue;
            
            if (rand_int(100) < 45)
            {
                int block_dir = rand_int(4);
                int dy = (block_dir == 0) ? -1 : (block_dir == 1) ? 1 : 0;
                int dx = (block_dir == 2) ? -1 : (block_dir == 3) ? 1 : 0;
                
                for (int step = 1; step < corridor_spacing; ++step)
                {
                    int ny = gy + dy * step;
                    int nx = gx + dx * step;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx))
                    {
                        cave_set_feat_style(ny, nx, FEAT_WALL_EXTRA, style_idx);
                        cave_info[ny][nx] &= ~CAVE_ROOM;
                        floor_count--;
                    }
                }
            }
        }
    }
    
    /* Add chambers at some intersections */
    int chamber_count = rand_range(2, 5);
    for (int c = 0; c < chamber_count; ++c)
    {
        int cy = rand_range(min_y + 2, max_y - 2);
        int cx = rand_range(min_x + 2, max_x - 2);
        if (!cave_floor_bold(cy, cx)) continue;
        
        int ch_h = rand_range(2, 4);
        int ch_w = rand_range(2, 5);
        
        for (int dy = -ch_h; dy <= ch_h; ++dy)
        {
            for (int dx = -ch_w; dx <= ch_w; ++dx)
            {
                int ty = cy + dy;
                int tx = cx + dx;
                if (!in_bounds_fully(ty, tx)) continue;
                if (cave_feat[ty][tx] != FEAT_WALL_EXTRA) continue;
                
                cave_set_feat_style(ty, tx, FEAT_FLOOR, style_idx);
                cave_info[ty][tx] |= CAVE_ROOM;
                floor_count++;
                if (ty < min_y) min_y = ty;
                if (ty > max_y) max_y = ty;
                if (tx < min_x) min_x = tx;
                if (tx > max_x) max_x = tx;
            }
        }
    }
    
    if (floor_count < 25)
        return false;
    
    /* Set outer walls */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (cave_floor_bold(gy, gx)) continue;
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                        borders_floor = true;
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, style_idx);
        }
    }
    
    /* Pick center - prefer floor tile adjacent to outer wall for tunnel connectivity */
    int center_y = (min_y + max_y) / 2, center_x = (min_x + max_x) / 2;
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (!cave_floor_bold(ty, tx)) continue;
        
        for (int dy = -1; dy <= 1 && !found_edge; ++dy)
        {
            for (int dx = -1; dx <= 1 && !found_edge; ++dx)
            {
                if (dy == 0 && dx == 0) continue;
                if (in_bounds_fully(ty + dy, tx + dx) && 
                    cave_feat[ty + dy][tx + dx] == FEAT_WALL_OUTER)
                {
                    center_y = ty; center_x = tx;
                    found_edge = true;
                }
            }
        }
    }
    /* Fallback: any floor tile */
    if (!found_edge)
    {
        for (int tries = 0; tries < 100; ++tries)
        {
            int ty = rand_range(min_y, max_y);
            int tx = rand_range(min_x, max_x);
            if (cave_floor_bold(ty, tx))
            {
                center_y = ty; center_x = tx; break;
            }
        }
    }
    
    int idx = dun->cent_n++;
    dun->cent[idx].y = center_y;
    dun->cent[idx].x = center_x;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_BSP_SLICE, false);
    
    /* === LABYRINTH STAIR PLACEMENT === */
    /* Place 1-2 stairs inside the labyrinth for navigation */
    int lab_stairs = 1 + (floor_count > 60 ? 1 : 0);
    int stairs_placed = 0;
    
    for (int s = 0; s < lab_stairs; ++s)
    {
        for (int tries = 0; tries < 50; ++tries)
        {
            int sy = rand_range(min_y, max_y);
            int sx = rand_range(min_x, max_x);
            if (!in_bounds_fully(sy, sx)) continue;
            if (!cave_naked_bold(sy, sx)) continue;
            if (!cave_floor_bold(sy, sx)) continue;
            
            /* Avoid placing next to doors */
            if (cave_feat[sy - 1][sx] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy + 1][sx] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy][sx - 1] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy][sx + 1] == FEAT_DOOR_HEAD) continue;
            
            /* Alternate between up and down stairs */
            int feat = (s % 2 == 0) ? FEAT_MORE : FEAT_LESS;
            
            /* At surface, only down; at Morgoth depth, only up */
            if (p_ptr->depth == 0) feat = FEAT_MORE;
            else if (p_ptr->depth >= MORGOTH_DEPTH) feat = FEAT_LESS;
            
            cave_set_feat(sy, sx, feat);
            stairs_placed++;
            break;
        }
    }
    
    log_trace("Labyrinth anchor (organic): bounds=(%d,%d)-(%d,%d) center=(%d,%d) edge=%d floors=%d chambers=%d stairs=%d",
        min_y, min_x, max_y, max_x, center_y, center_x, found_edge, floor_count, chamber_count, stairs_placed);
    genlog_anchor("LABYRINTH: bounds=(%d,%d)-(%d,%d), %d floor tiles, %d chambers, %d stairs",
        min_y, min_x, max_y, max_x, floor_count, chamber_count, stairs_placed);
    return true;
}


/* Carve a BSP-style sliced region into rooms-like rectangles and register anchor */


/* Place rooms in randomized order within a partition */
static void place_rooms_randomized(int y1, int y2, int x1, int x2, int depth,
                                   int t1_count, int t2_count, int t6_count, int t7_count,
                                   int *budget_t6, int *budget_t7, int *budget_t8,
                                   int *used_t6, int *used_t7, int *used_t8)
{
    /* Build an array of all room placements needed */
    int total = t1_count + t2_count + t6_count + t7_count;
    if (total <= 0) return;
    if (total > 50) total = 50;  /* Safety cap */
    
    int room_types[50];
    int idx = 0;
    for (int i = 0; i < t1_count && idx < 50; ++i) room_types[idx++] = 1;
    for (int i = 0; i < t2_count && idx < 50; ++i) room_types[idx++] = 2;
    for (int i = 0; i < t6_count && idx < 50; ++i) room_types[idx++] = 6;
    for (int i = 0; i < t7_count && idx < 50; ++i) room_types[idx++] = 7;
    
    /* Fisher-Yates shuffle */
    for (int i = total - 1; i > 0; --i)
    {
        int j = rand_int(i + 1);
        int temp = room_types[i];
        room_types[i] = room_types[j];
        room_types[j] = temp;
    }
    
    /* Place rooms in shuffled order */
    for (int i = 0; i < total; ++i)
    {
        int typ = room_types[i];
        int priority = (typ >= 6) ? 3 : 2;
        place_room_with_budget(typ, y1, y2, x1, x2, priority, depth,
                               budget_t6, budget_t7, budget_t8,
                               used_t6, used_t7, used_t8);
    }
}

/* Smallest depth at which a non-quest greater vault can appear */
static int min_nonquest_gv_depth(void)
{
    static int cached_min_depth = -1;
    if (cached_min_depth >= 0)
        return cached_min_depth;

    int min_depth = 127; /* high sentinel */
    for (int i = 0; i < z_info->v_max; ++i)
    {
        vault_type *v_ptr = &v_info[i];
        if (v_ptr->typ != 8)
            continue;
        if (v_ptr->flags & VLT_QUEST)
            continue;
        if (v_ptr->depth < min_depth)
            min_depth = v_ptr->depth;
    }

    /* Fallback to old gating depth if no candidates are present */
    if (min_depth == 127)
        min_depth = 15;

    cached_min_depth = min_depth;
    return cached_min_depth;
}

/* Roll whether this level should reserve a greater vault slot based on vault rarities */
static bool gv_level_roll_allows(int depth, int *out_candidates)
{
    int candidate_count = 0;
    bool passed = false;

    for (int i = 0; i < z_info->v_max; ++i)
    {
        vault_type *v_ptr = &v_info[i];
        if (v_ptr->typ != 8) continue;
        if (v_ptr->flags & VLT_QUEST) continue;
        if (v_ptr->depth > depth) continue;
        if (v_ptr->max_depth != 0 && depth > v_ptr->max_depth) continue;

        /* Skip already-used greater vaults to mirror build_type8 checks */
        bool repeated = false;
        for (int j = 0; j < MAX_GREATER_VAULTS; ++j)
        {
            if (p_ptr->greater_vaults[j] == i)
            {
                repeated = true;
                break;
            }
        }
        if (repeated) continue;

        candidate_count++;
        if (!passed && one_in_(vault_type8_generation_rarity(v_ptr, depth)))
        {
            passed = true;
        }
    }

    if (out_candidates) *out_candidates = candidate_count;

    if (candidate_count == 0)
    {
        genlog_partition("GV roll: depth=%d -> no eligible type8 templates (used or quest-only)", depth);
        return false;
    }

    if (passed)
    {
        genlog_partition("GV roll: depth=%d candidates=%d -> PASS (reserve GV this level)", depth, candidate_count);
    }
    else
    {
        genlog_partition("GV roll: depth=%d candidates=%d -> FAIL (no GV this level)", depth, candidate_count);
    }

    return passed;
}

/* Check whether a partition is fully interior (no map-border contact) */
static bool partition_is_interior(int row, int col, int rows, int cols)
{
    return (row > 0) && (row < rows - 1) && (col > 0) && (col < cols - 1);
}

bool generation_escape_tunnel_bold(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;

    return cave_escape_tunnel[y][x];
}

void mark_generation_escape_tunnel(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return;

    cave_escape_tunnel[y][x] = true;
}

/* Pick the partition whose centre is closest to the map centre, preferring interior slots */
static int choose_central_partition_index(int rows, int cols)
{
    if (rows <= 0 || cols <= 0)
        return -1;

    int best_idx = -1;
    int best_score = 1 << 30;
    int map_cy = p_ptr->cur_map_hgt / 2;
    int map_cx = p_ptr->cur_map_wid / 2;

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int pi = row * cols + col;
            int y1, y2, x1, x2;
            if (!compute_partition_bounds(pi, rows, cols, &y1, &y2, &x1, &x2))
                continue;

            int cy = (y1 + y2) / 2;
            int cx = (x1 + x2) / 2;
            int dist = distance(map_cy, map_cx, cy, cx);
            int penalty = partition_is_interior(row, col, rows, cols) ? 0 : 10000;
            int score = dist + penalty;

            if (score < best_score)
            {
                best_score = score;
                best_idx = pi;
            }
        }
    }

    return best_idx;
}

/* Try to drop a greater vault inside the provided partition bounds */
static bool place_gv_in_partition(int y1, int y2, int x1, int x2, int *budget_t8, int *used_t8)
{
    if (!budget_t8 || *budget_t8 <= 0)
        return false;

    if (dun->cent_n >= room_capacity_limit())
        return false;
    if (y2 - y1 < 6 || x2 - x1 < 8)
        return false;

    /* Can only have one greater vault per level */
    if (g_vault_name[0] != '\0')
        return false;

    bool placed = false;
    for (int attempt = 0; attempt < 3 && !placed; ++attempt)
    {
        int cy = rand_range(MAX(5, y1 + 3), MIN(p_ptr->cur_map_hgt - 5, y2 - 3));
        int cx = rand_range(MAX(5, x1 + 3), MIN(p_ptr->cur_map_wid - 5, x2 - 3));
        placed = build_reserved_type8(cy, cx);
    }

    if (!placed)
    {
        int scan_y1 = MAX(5, y1 + 3);
        int scan_y2 = MIN(p_ptr->cur_map_hgt - 5, y2 - 3);
        int scan_x1 = MAX(5, x1 + 3);
        int scan_x2 = MIN(p_ptr->cur_map_wid - 5, x2 - 3);

        if (scan_y1 <= scan_y2 && scan_x1 <= scan_x2)
        {
            log_trace("Greater vault: random partition placement missed, scanning bounds (%d,%d)-(%d,%d)",
                y1, x1, y2, x2);

            for (int cy = scan_y1; cy <= scan_y2 && !placed; ++cy)
            {
                for (int cx = scan_x1; cx <= scan_x2 && !placed; ++cx)
                {
                    placed = build_reserved_type8(cy, cx);
                }
            }
        }
    }

    if (placed)
    {
        (*budget_t8)--;
        if (used_t8)
            (*used_t8)++;
    }

    return placed;
}

/* Place a chest in a random floor location within partition bounds */


/* Dynamic partition-based generation mix */
void apply_quadrant_generation_modes(void)
{
    /* Determine partition grid based on level size (in blocks) */
    int blocks = p_ptr->cur_map_hgt / PANEL_HGT;  /* Square levels, so hgt == wid */
    int partition_count;
    int grid_rows, grid_cols;
    int depth = p_ptr->depth;
    
    /* Partition scaling - REDUCED partition counts for larger anchors.
     * Each partition should be at least ~40 tiles per side to fit big caves/chasms.
     * 
      * Target partition size: 40-50 tiles per side for optimal anchor fitting.
      * 
      * Scaling by level size:
      *  6 blocks  ( 66x66)  -> 2x2 grid  (4 partitions)  = 33x33 per partition
      *  7 blocks  ( 77x77)  -> 2x2 grid  (4 partitions)  = 38x38 per partition
      *  8 blocks  ( 88x88)  -> 2x2 grid  (4 partitions)  = 44x44 per partition
      *  9 blocks  ( 99x99)  -> 2x2 grid  (4 partitions)  = 49x49 per partition
      * 10 blocks  (110x110) -> 2x3 grid  (6 partitions)  = 55x36 per partition
      * 11 blocks  (121x121) -> 3x3 grid  (9 partitions)  = 40x40 per partition
     * 12 blocks  (132x132) -> 3x3 grid  (9 partitions)  = 44x44 per partition
     * 13 blocks  (143x143) -> 3x3 grid  (9 partitions)  = 47x47 per partition
     * 14 blocks  (154x154) -> 3x4 grid (12 partitions)  = 51x38 per partition
     * 15 blocks  (165x165) -> 4x4 grid (16 partitions)  = 41x41 per partition
     * 16 blocks  (176x176) -> 4x4 grid (16 partitions)  = 44x44 per partition
     * 17 blocks  (187x187) -> 5x4 grid (20 partitions)  = 46x46 per partition
     * 18 blocks  (198x198) -> 5x4 grid (20 partitions)  = 49x49 per partition
     * 19 blocks  (209x209) -> 5x4 grid (20 partitions)  = 52x52 per partition
     * 20 blocks  (220x220) -> 5x4 grid (20 partitions)  = 55x55 per partition
     * 21 blocks  (231x231) -> 5x5 grid (25 partitions)  = 46x46 per partition
     */
    if (blocks <= 9)
    {
        partition_count = 4;
        grid_rows = 2; grid_cols = 2;
    }
    else if (blocks == 10)
    {
        partition_count = 6;
        if (one_in_(2)) { grid_rows = 3; grid_cols = 2; }
        else { grid_rows = 2; grid_cols = 3; }
    }
    else if (blocks <= 13)
    {
        partition_count = 9;
        grid_rows = 3; grid_cols = 3;
    }
    else if (blocks == 14)
    {
        partition_count = 12;
        if (one_in_(2)) { grid_rows = 3; grid_cols = 4; }
        else { grid_rows = 4; grid_cols = 3; }
    }
    else if (blocks <= 16)
    {
        partition_count = 16;
        grid_rows = 4; grid_cols = 4;
    }
    else if (blocks <= 20)
    {
        partition_count = 20;
        if (one_in_(2)) { grid_rows = 5; grid_cols = 4; }
        else { grid_rows = 4; grid_cols = 5; }
    }
    else  /* blocks >= 21 */
    {
        partition_count = 25;
        grid_rows = 5; grid_cols = 5;
    }

    remember_partition_grid(grid_rows, grid_cols, partition_count);
    
    log_trace("Level size %d blocks: using %dx%d partition grid (%d zones)", 
              blocks, grid_rows, grid_cols, partition_count);
    
    /* Generation log: partition grid setup */
    genlog_partition("Grid setup: %d blocks -> %dx%d grid (%d partitions), depth=%d",
                     blocks, grid_rows, grid_cols, partition_count, depth);
    
    /* Allocate mode, style, and density arrays - max 25 partitions now */
    quadrant_mode_t modes[25];
    int partition_styles[25];
    int partition_bridge_styles[25];
    big_cave_type_t partition_big_cave_types[25];
    density_level_t densities[25];
    int gv_partition = -1;
    int gv_min_depth = min_nonquest_gv_depth();
    bool gv_level_allowed = false;

    if (depth >= gv_min_depth)
    {
        if (!cached_gv_level_roll_resolved)
        {
            cached_gv_level_roll_allowed =
                gv_level_roll_allows(depth, &cached_gv_level_roll_candidates);
            cached_gv_level_roll_resolved = true;
        }

        gv_level_allowed = cached_gv_level_roll_allowed;
    }

    if (!gv_level_allowed && depth < gv_min_depth) {
        genlog_partition("GV roll: depth=%d below minimum %d -> no GV this level", depth, gv_min_depth);
    }
    if (morgoth_level_active) {
        gv_level_allowed = false; /* Morgoth's throne room replaces normal GVs */
        morgoth_partition_index = choose_central_partition_index(grid_rows, grid_cols);
        genlog_partition("Morgoth level: reserving central partition idx=%d (grid %dx%d)", morgoth_partition_index, grid_rows, grid_cols);
    }

    /* Depth-aware vault budgets (soft caps; clamped to remaining capacity) */
    /* BOOSTED: More rooms and vaults per partition for denser levels */
    int budget_t6 = MIN(room_capacity_limit(), MAX(20, partition_count * 3 + depth));
    int budget_t7 = (depth >= 4) ? MIN(room_capacity_limit(), MAX(6, partition_count + depth / 2)) : 0;
    int budget_t8 = gv_level_allowed ? 1 : 0;
    if (morgoth_level_active) {
        budget_t8 = 0;
    }
    int capacity_remaining = room_capacity_limit() - dun->cent_n;
    if (budget_t8 > capacity_remaining)
        budget_t8 = capacity_remaining;

    /* Reserve space for the dedicated GV attempt before scaling other budgets */
    int capacity_for_regular = capacity_remaining - budget_t8;
    if (capacity_for_regular < 0)
        capacity_for_regular = 0;

    int budget_total = budget_t6 + budget_t7;
    if (budget_total > capacity_for_regular && budget_total > 0) {
        /* Scale budgets down to fit remaining slots (GV slot already reserved) */
        budget_t6 = (budget_t6 * capacity_for_regular) / budget_total;
        budget_t7 = (budget_t7 * capacity_for_regular) / budget_total;
        if (budget_t6 + budget_t7 < capacity_for_regular) {
            budget_t6 = MIN(capacity_for_regular, budget_t6 + 1); /* keep at least one */
        }
    } else if (capacity_for_regular == 0) {
        budget_t6 = 0;
        budget_t7 = 0;
    }
    
    int mode_counts[6] = {0};
    /* Guarantee minimum ROOMY and CAVEY partitions based on partition count */
    /* ROOMY provides reliable standard rooms that connect well */
    int guaranteed_roomy = 1 + partition_count / 5;  /* At least 1 ROOMY, +1 per 5 partitions */
    int guaranteed_cavey = partition_count / 8;      /* 0 for small, 1+ for larger */
    
    /* Initialize with guaranteed modes first */
    int idx = 0;
    for (int i = 0; i < guaranteed_roomy && idx < partition_count; ++i, ++idx)
    {
        modes[idx] = QUAD_MODE_ROOMY;
        mode_counts[QUAD_MODE_ROOMY]++;
    }
    for (int i = 0; i < guaranteed_cavey && idx < partition_count; ++i, ++idx)
    {
        modes[idx] = QUAD_MODE_CAVEY;
        mode_counts[QUAD_MODE_CAVEY]++;
    }
    
    /* Fill remaining with random modes */
    for (; idx < partition_count; ++idx)
    {
        int weights[6];
        for (int m = 0; m < 6; ++m)
        {
            weights[m] = mode_weight_for_depth(
                (quadrant_mode_t)m, depth, blocks, mode_counts, partition_count);
        }
        modes[idx] = pick_weighted_mode(weights, N_ELEMENTS(weights));
        mode_counts[modes[idx]]++;
    }
    
    /* Shuffle all partitions */
    for (int i = partition_count - 1; i > 0; --i)
    {
        int j = rand_int(i + 1);
        quadrant_mode_t temp = modes[i];
        modes[i] = modes[j];
        modes[j] = temp;
    }
    
    log_trace("%d-partition level: %d ROOMY + %d CAVEY guaranteed, others randomized",
              partition_count, guaranteed_roomy, guaranteed_cavey);
    
    genlog_partition("Mode guarantees: %d ROOMY + %d CAVEY required, %d random",
                     guaranteed_roomy, guaranteed_cavey, partition_count - guaranteed_roomy - guaranteed_cavey);

    /* Never allow Morgoth's throne-room partition to be a special-mode partition.
     * Otherwise, environmental effects (labyrinth view loss, big cave penalties, etc.)
     * can bleed into the endgame setpiece. */
    if (morgoth_level_active && morgoth_partition_index >= 0 && morgoth_partition_index < partition_count)
    {
        if (modes[morgoth_partition_index] == QUAD_MODE_LABYRINTH
            || modes[morgoth_partition_index] == QUAD_MODE_CHASM
            || modes[morgoth_partition_index] == QUAD_MODE_BIG_CAVE)
        {
            log_trace("Morgoth level: forcing partition %d mode from %d to ROOMY",
                      morgoth_partition_index, (int)modes[morgoth_partition_index]);
        }
        modes[morgoth_partition_index] = QUAD_MODE_ROOMY;
    }
    
    /* Pick a random visual style and density for each partition */
    for (int i = 0; i < partition_count; ++i)
    {
        partition_bridge_styles[i] = -1;
        partition_big_cave_types[i] = BIG_CAVE_NONE;

        switch (modes[i])
        {
        case QUAD_MODE_CAVEY:
            partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_CA_BLOB);
            break;
        case QUAD_MODE_LABYRINTH:
            partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_LABYRINTH);
            break;
        case QUAD_MODE_CHASM:
            partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_CHASM_FLOOR);
            partition_bridge_styles[i] = styles_pick_partition_style(depth, PART_STYLE_CHASM_BRIDGE);
            break;
        case QUAD_MODE_BIG_CAVE:
            partition_big_cave_types[i] = big_cave_type_pick_for_depth(depth);
            if (partition_big_cave_types[i] == BIG_CAVE_ICE)
                partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_BIG_CAVE_ICE);
            else if (partition_big_cave_types[i] == BIG_CAVE_FIRE)
                partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_BIG_CAVE_FIRE);
            else
                partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_BIG_CAVE_POIS);
            break;
        case QUAD_MODE_ROOMY:
        case QUAD_MODE_RUINED:
        default:
            partition_styles[i] = styles_pick_random_from_level();
            break;
        }

        /* Fixed density distribution: 30% sparse, 40% normal, 30% dense */
        int sparse_chance = 30;
        int normal_chance = 40;

        int density_roll = rand_int(100);
        if (density_roll < sparse_chance)
            densities[i] = DENSITY_SPARSE;
        else if (density_roll < sparse_chance + normal_chance)
            densities[i] = DENSITY_NORMAL;
        else
            densities[i] = DENSITY_DENSE;
    }

    record_partition_metadata(modes, densities, partition_count);
    for (int i = 0; i < partition_count && i < 25; ++i)
    {
        current_partition_big_cave_types[i] = partition_big_cave_types[i];
        current_partition_bridge_styles[i] = partition_bridge_styles[i];
    }
    
    /* Pre-roll for a dedicated greater vault partition (must be interior) */
    if (budget_t8 > 0)
    {
        int gv_candidates[25];
        int gv_interior_count = 0;
        int gv_preferred[25];
        int gv_preferred_count = 0;
        for (int row = 0; row < grid_rows; ++row)
        {
            for (int col = 0; col < grid_cols; ++col)
            {
                if (!partition_is_interior(row, col, grid_rows, grid_cols))
                    continue;
                int partition_idx = row * grid_cols + col;
                if (partition_idx >= partition_count || gv_interior_count >= 25)
                    continue;
                gv_candidates[gv_interior_count++] = partition_idx;

                /* Prefer a non-special partition for greater vaults so their setpiece
                 * effects don't overlap with LABYRINTH/CHASM/BIG_CAVE zones. */
                quadrant_mode_t m = modes[partition_idx];
                if (m != QUAD_MODE_LABYRINTH && m != QUAD_MODE_CHASM && m != QUAD_MODE_BIG_CAVE)
                {
                    if (gv_preferred_count < 25)
                        gv_preferred[gv_preferred_count++] = partition_idx;
                }
            }
        }

        if (gv_interior_count > 0)
        {
            bool used_preferred = (gv_preferred_count > 0);
            gv_partition = used_preferred
                ? gv_preferred[rand_int(gv_preferred_count)]
                : gv_candidates[rand_int(gv_interior_count)];
            int gv_row = gv_partition / grid_cols;
            int gv_col = gv_partition % grid_cols;
            log_trace("Greater vault partition: %d interior options (%d preferred) -> reserve partition %d (row=%d col=%d grid %dx%d%s)",
                      gv_interior_count, gv_preferred_count, gv_partition, gv_row, gv_col,
                      grid_rows, grid_cols, used_preferred ? "" : " fallback");
            genlog_partition("GV partition reserved (rarity passed): depth=%d min_depth=%d interior=%d preferred=%d -> (%d,%d) idx=%d grid=%dx%d%s",
                             depth, gv_min_depth, gv_interior_count, gv_preferred_count,
                             gv_row, gv_col, gv_partition, grid_rows, grid_cols,
                             used_preferred ? "" : " fallback");
        }
        else
        {
            log_trace("Greater vault partition: no eligible interior partitions for %dx%d grid",
                      grid_rows, grid_cols);
            genlog_partition("GV partition skipped: no interior partitions for grid %dx%d (depth=%d)", grid_rows, grid_cols, depth);
            gv_partition = -1;
            budget_t8 = 0; /* No dedicated slot this level */
        }
    }
    
    /* Mode name strings for logging */
    const char *mode_str[] = {"ROOMY", "CAVEY", "RUINED", "LABYRINTH", "CHASM", "BIG_CAVE"};
    const char *density_str[] = {"SPARSE", "NORMAL", "DENSE"};
    int used_t6 = 0, used_t7 = 0, used_t8 = 0;
    bool gv_partition_attempted = false;
    int partitions_skipped = 0;
    int skipped_soft_fill = 0;
    int skip_cap = MAX(2, partition_count / 5); /* cap outright skips to keep coverage */

    /* Track which partitions have been processed */
    bool partition_done[25];
    for (int i = 0; i < 25; ++i)
        partition_done[i] = false;

    /* TWO-PASS PROCESSING:
     * Pass 1: Process special modes (LABYRINTH, CHASM, BIG_CAVE) first.
     *         These need clear space for anchor carving, so they must run
     *         before ROOMY/CAVEY can place rooms that encroach on neighbors.
     * Pass 2: Process remaining modes (ROOMY, CAVEY, RUINED).
     */
    genlog_partition("Processing special modes first (LABYRINTH, CHASM, BIG_CAVE) to ensure clear space");
    
    /* Pass 1: Special modes only */
    for (int pi = 0; pi < partition_count; ++pi)
    {
        quadrant_mode_t mode = modes[pi];
        bool is_gv_partition = (pi == gv_partition);
        bool is_morgoth_partition = (morgoth_level_active && pi == morgoth_partition_index);
        bool is_special_mode = (mode == QUAD_MODE_LABYRINTH || mode == QUAD_MODE_CHASM || mode == QUAD_MODE_BIG_CAVE);
        if (!is_gv_partition && !is_special_mode && !is_morgoth_partition)
            continue;  /* Skip non-special modes for now */

        if (dun->cent_n >= room_capacity_limit())
        {
            log_trace("Partition gen: room capacity reached (%d/%d), skipping remaining partitions",
                      dun->cent_n, room_capacity_limit());
            break;
        }

        /* Calculate partition boundaries based on grid */
        int before_cent = dun->cent_n;
        int row = pi / grid_cols;
        int col = pi % grid_cols;
        
        int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
        if (!compute_partition_bounds(pi, grid_rows, grid_cols, &y1, &y2, &x1, &x2))
        {
            log_trace("Partition %d [%d,%d]: invalid bounds for grid %dx%d",
                pi, row, col, grid_rows, grid_cols);
            continue;
        }
        
        if (is_morgoth_partition)
        {
            morgoth_partition_bounds.y1 = y1;
            morgoth_partition_bounds.y2 = y2;
            morgoth_partition_bounds.x1 = x1;
            morgoth_partition_bounds.x2 = x2;
            morgoth_vault_center_y = (y1 + y2) / 2;
            morgoth_vault_center_x = (x1 + x2) / 2;
            morgoth_partition_reserved = true;
            
            /* Place and seal Morgoth's throne room IMMEDIATELY to prevent other 
             * partitions from placing content in this area. The permanent wall sealing
             * must happen before any other room/corridor generation. */
            vault_type* v_ptr = NULL;
            int cy = morgoth_vault_center_y;
            int cx = morgoth_vault_center_x;
            
            if (build_type9(cy, cx, &v_ptr))
            {
                carve_morgoth_entry_tunnels(v_ptr, cy, cx);
                seal_morgoth_partition(v_ptr, cy, cx);
                partition_done[pi] = true;
                genlog_partition("Morgoth partition placed and sealed at idx=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d)", 
                                pi, y1, x1, y2, x2, cy, cx);
            }
            else
            {
                log_trace("Morgoth level: failed to build throne room at (%d,%d) in partition %d", cy, cx, pi);
                morgoth_partition_reserved = false;  /* Allow fallback */
            }
            continue;
        }

        /* mode already declared at loop start for the continue check */
        int style_idx = partition_styles[pi];
        int bridge_style = partition_bridge_styles[pi];
        big_cave_type_t cave_type = partition_big_cave_types[pi];
        density_level_t density = densities[pi];
        int area = (y2 - y1 + 1) * (x2 - x1 + 1);
        int area_factor = MAX(1, MIN(3, (area + 1100) / 1200));
        int floor_pct = 0, icky_pct = 0;
        bool reserved = area_is_reserved_or_dense(y1, y2, x1, x2, &floor_pct, &icky_pct);

        log_trace("Partition %d [%d,%d] (pass 1%s): mode=%s density=%s bounds=(%d,%d)-(%d,%d) area=%d floor=%d%% icky=%d%%",
                  pi, row, col, is_gv_partition ? " GV" : "", mode_str[mode], density_str[density], y1, x1, y2, x2, area, floor_pct, icky_pct);

        if (reserved && partitions_skipped >= skip_cap) {
            /* Too many skips already: fall back to a light recipe instead of skipping */
            log_trace("Partition %d [%d,%d]: reserved but skip_cap reached; using soft-fill", pi, row, col);
            reserved = false;
            skipped_soft_fill++;
            /* Downgrade density to sparse to reduce conflicts */
            density = DENSITY_SPARSE;
        }

        if (reserved) {
            log_trace("Partition %d [%d,%d]: skipping (reserved/quest/icky overlap)", pi, row, col);
            if (is_gv_partition) {
                gv_partition = -1;
                budget_t8 = 0;
            }
            partitions_skipped++;
            continue;
        }

        if (is_gv_partition)
        {
            gv_partition_attempted = true;
            bool placed_gv = place_gv_in_partition(y1, y2, x1, x2, &budget_t8, &used_t8);
            if (placed_gv)
            {
                log_trace("Partition %d [%d,%d]: placed greater vault within bounds (%d,%d)-(%d,%d)",
                          pi, row, col, y1, x1, y2, x2);
                genlog_partition("GV placed '%s' in partition [%d,%d] idx=%d bounds=(%d,%d)-(%d,%d) remaining_t8=%d",
                                 g_vault_name[0] ? g_vault_name : level_gen_debug_last_greater_vault_name,
                                 row, col, pi, y1, x1, y2, x2, budget_t8);
                partition_done[pi] = true;
                continue;
            }

            log_trace("Partition %d [%d,%d]: greater vault placement failed, falling back to mode logic",
                      pi, row, col);
            genlog_partition("GV placement failed for '%s' in partition [%d,%d] idx=%d bounds=(%d,%d)-(%d,%d); disabling GV for this attempt",
                             level_gen_debug_last_greater_vault_name[0]
                                 ? level_gen_debug_last_greater_vault_name
                                 : "(unknown)",
                             row, col, pi, y1, x1, y2, x2);
            gv_partition = -1;
            budget_t8 = 0;
            if (!is_special_mode)
                continue;
        }

        /* PARTITION MODE TYPES:
         * - ROOMY: Traditional dungeon - balanced mix of all room types
         * - CAVEY: Natural cave system with CA blobs and minimal rooms
         * - RUINED: Ancient carved BSP passages with rooms
         * - LABYRINTH: Maze corridors with chambers
         * - CHASM: Platforms over chasms connected by bridges
         * - BIG_CAVE: Single massive irregular cavern
         */
        switch (mode)
        {
        case QUAD_MODE_CAVEY:
            {
                /* Natural cave system: CA blobs with quartz veins */
                int partition_area = (y2 - y1) * (x2 - x1);
                int base_blobs
                    = 2 + partition_area / 400;  /* Scale with partition size */
                int blob_target = (density == DENSITY_SPARSE) ? base_blobs : 
                                  (density == DENSITY_DENSE) ? base_blobs + 2 : base_blobs + 1;
                if (blob_target > 6) blob_target = 6;
                int carved_blobs = 0;
                
                for (int b = 0; b < blob_target; ++b)
                    if (carve_ca_blob_anchor_bounds(y1, y2, x1, x2, style_idx))
                        carved_blobs++;
                
                /* Scatter quartz veins for natural cave look */
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, 0);
                
                set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                    0, 100, 0, 0, PARTITION_CHEST_ANCHOR_ANY);
                
                /* Caves with rooms scattered inside */
                /* Sparse: T1=2 T2=1 T6=2 T7=0 | Normal: T1=2 T2=2 T6=2 T7=1 | Dense: T1=2 T2=3 T6=3 T7=1 */
                int std_count = scaled_attempts(2, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : (density == DENSITY_DENSE) ? 1 : 1, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        case QUAD_MODE_LABYRINTH:
            {
                /* Maze corridors - oppressive, fewer rooms */
                bool carved = carve_labyrinth_bounds(y1, y2, x1, x2, density, style_idx);
                if (!carved)
                {
                    /* Fallback: more BSP slices for maze-like feel */
                    int maze_count = (density == DENSITY_SPARSE) ? 6 : 
                                     (density == DENSITY_DENSE) ? 12 : 8;
                    for (int b = 0; b < maze_count; ++b)
                        carve_bsp_slice_anchor_bounds(y1, y2, x1, x2);
                    /* Update partition mode to match fallback generation (use RUINED for BSP slices) */
                    current_partition_modes[pi] = QUAD_MODE_RUINED;
                    style_idx = styles_pick_random_from_level();
                    partition_styles[pi] = style_idx;
                }
                
                /* Add some dead-end interest: occasional rubble in corridors */
                for (int gy = y1; gy <= y2; ++gy)
                {
                    for (int gx = x1; gx <= x2; ++gx)
                    {
                        if (!in_bounds_fully(gy, gx)) continue;
                        if (!cave_floor_bold(gy, gx)) continue;
                        /* Very low rubble chance for claustrophobic feel */
                        if (one_in_(40))
                            cave_set_feat_style(gy, gx, FEAT_RUBBLE, style_idx);
                    }
                }
                
                /* Labyrinth with chambers and vaults */
                /* Sparse: T1=1 T2=0 T6=1 T7=0 | Normal: T1=1 T2=1 T6=1 T7=0 | Dense: T1=1 T2=1 T6=2 T7=1 */
                int std_count = scaled_attempts(1, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : 1, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 2 : 1, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : (density == DENSITY_DENSE) ? 1 : 0, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
                
                /* Place 1 chest in labyrinth partition ONLY if it actually carved */
                if (carved)
                {
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                        1, 0, 100, 0, PARTITION_CHEST_ANCHOR_ANY);
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 1,
                        1, 0, 0, 100, PARTITION_CHEST_ANCHOR_ANY);
                }
            }
            break;
        case QUAD_MODE_CHASM:
            {
                /* Chasm with platforms connected by bridges - no additional rooms */
                bool chasm_carved = carve_chasm_with_bridges(y1, y2, x1, x2,
                    style_idx, bridge_style);
                if (!chasm_carved)
                {
                    /* Fallback: use CA blobs to keep the open feel */
                    int ca_style = styles_pick_partition_style(depth, PART_STYLE_CA_BLOB);
                    int blob_count = (density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3;
                    for (int b = 0; b < blob_count; ++b)
                        carve_ca_blob_anchor_bounds(y1, y2, x1, x2, ca_style);
                    /* Update partition mode to match fallback generation */
                    current_partition_modes[pi] = QUAD_MODE_CAVEY;
                    style_idx = ca_style;
                    partition_styles[pi] = ca_style;
                    bridge_style = -1;
                    partition_bridge_styles[pi] = -1;
                }

                /* Veins in chasm walls for mining (tagged for metal placement) */
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, CAVE_CHASM_AREA);

                /* Place 2 guaranteed chests in chasm partition ONLY if it actually carved */
                if (chasm_carved)
                {
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                        0, 0, 65, 35, PARTITION_CHEST_ANCHOR_ANY);
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 1,
                        0, 0, 65, 35, PARTITION_CHEST_ANCHOR_ANY);
                }
            }
            break;
        case QUAD_MODE_BIG_CAVE:
            {
                /* Single massive cavern - the cave IS the room */
                bool carved = carve_big_cave_bounds(y1, y2, x1, x2, style_idx, cave_type);
                int blob_count = 0;
                int carved_blobs = 0;
                if (!carved)
                {
                    /* Fallback: many overlapping blobs */
                    int ca_style = styles_pick_partition_style(depth, PART_STYLE_CA_BLOB);
                    blob_count = (density == DENSITY_SPARSE) ? 5 : 
                                 (density == DENSITY_DENSE) ? 10 : 7;
                    for (int b = 0; b < blob_count; ++b)
                        if (carve_ca_blob_anchor_bounds(y1, y2, x1, x2, ca_style))
                            carved_blobs++;
                    /* Update partition mode to match fallback generation */
                    current_partition_modes[pi] = QUAD_MODE_CAVEY;
                    current_partition_big_cave_types[pi] = BIG_CAVE_NONE;
                    partition_big_cave_types[pi] = BIG_CAVE_NONE;
                    style_idx = ca_style;
                    partition_styles[pi] = ca_style;
                }
                
                /* Add quartz veins for natural cave look */
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, 0);

                if (!carved)
                {
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                        0, 100, 0, 0, PARTITION_CHEST_ANCHOR_ANY);
                }
                
                /* Add internal pillars/boulders for visual interest (density-scaled) */
                int pillar_target = (density == DENSITY_SPARSE) ? 3 : 
                                    (density == DENSITY_DENSE) ? 10 : 6;
                int pillars_placed = 0;
                for (int tries = 0; tries < 100 && pillars_placed < pillar_target; ++tries)
                {
                    int py = rand_range(y1 + 3, y2 - 3);
                    int px = rand_range(x1 + 3, x2 - 3);
                    if (!in_bounds_fully(py, px)) continue;
                    if (!cave_floor_bold(py, px)) continue;
                    
                    /* Check all neighbors are floor */
                    bool all_floor = true;
                    for (int dy = -1; dy <= 1 && all_floor; ++dy)
                        for (int dx = -1; dx <= 1 && all_floor; ++dx)
                            if (!cave_floor_bold(py + dy, px + dx))
                                all_floor = false;
                    
                    if (all_floor)
                    {
                        cave_set_feat_style(py, px, FEAT_WALL_EXTRA, style_idx);
                        pillars_placed++;
                    }
                }
                
                /* Guarantee two large chests in big caves with default material odds. */
                if (carved)
                {
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                        2, 50, 35, 15, PARTITION_CHEST_ANCHOR_ANY);
                    set_partition_chest_recipe(&current_partition_population_meta[pi], 1,
                        2, 50, 35, 15, PARTITION_CHEST_ANCHOR_ANY);
                }
            }
            break;
        case QUAD_MODE_ROOMY:
        default:
            {
                /* Traditional dungeon - packed with rooms and vaults */
                /* Sparse: T1=2 T2=1 T6=2 T7=1 | Normal: T1=3 T2=2 T6=3 T7=2 | Dense: T1=4 T2=3 T6=4 T7=3 */
                int std_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        }

        /* Per-partition fallback: if nothing landed, drop a simple room to avoid voids */
        if (dun->cent_n == before_cent)
        {
            int fallback_style = styles_pick_random_from_level();
            style_idx = fallback_style;
            partition_styles[pi] = fallback_style;
            if (room_build_in_bounds(1, y1, y2, x1, x2) || room_build_in_bounds(2, y1, y2, x1, x2))
                log_trace("Partition %d [%d,%d]: fallback simple room placed", pi, row, col);
        }

        /* Apply the partition's visual style to its granite walls.
         * Use a jagged/organic boundary instead of a straight line. */
        if (style_idx >= 0)
        {
            int blend_zone = 3;

            for (int y = y1; y <= y2; ++y)
            {
                for (int x = x1; x <= x2; ++x)
                {
                    if (cave_feat[y][x] != FEAT_WALL_EXTRA)
                        continue;

                    int dist_top = y - y1;
                    int dist_bot = y2 - y;
                    int dist_left = x - x1;
                    int dist_right = x2 - x;
                    int dist_edge = MIN(MIN(dist_top, dist_bot), MIN(dist_left, dist_right));

                    if (dist_edge >= blend_zone)
                    {
                        cave_set_feat_with_color(y, x, FEAT_WALL_EXTRA, style_idx);
                    }
                    else
                    {
                        int chance = 20 + (dist_edge * 67 / blend_zone);
                        if (rand_int(100) < chance)
                        {
                            cave_set_feat_with_color(y, x, FEAT_WALL_EXTRA, style_idx);
                        }
                    }
                }
            }
        }

        /* Mark partition as done */
        partition_done[pi] = true;
    }

    /* Pass 2: Process remaining non-special modes (ROOMY, CAVEY, RUINED) */
    genlog_partition("Pass 2: Processing standard modes (ROOMY, CAVEY, RUINED)");
    for (int pi = 0; pi < partition_count; ++pi)
    {
        if (partition_done[pi])
            continue;  /* Already processed in Pass 1 */

        if (dun->cent_n >= room_capacity_limit())
        {
            log_trace("Partition gen: room capacity reached (%d/%d), skipping remaining partitions",
                      dun->cent_n, room_capacity_limit());
            break;
        }

        /* Calculate partition boundaries based on grid */
        int before_cent = dun->cent_n;
        int row = pi / grid_cols;
        int col = pi % grid_cols;
        
        int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
        if (!compute_partition_bounds(pi, grid_rows, grid_cols, &y1, &y2, &x1, &x2))
        {
            log_trace("Partition %d [%d,%d]: invalid bounds for grid %dx%d",
                pi, row, col, grid_rows, grid_cols);
            continue;
        }
        
        quadrant_mode_t mode = modes[pi];
        density_level_t density = densities[pi];
        int style_idx = partition_styles[pi];
        int area = (y2 - y1 + 1) * (x2 - x1 + 1);
        int area_factor = MAX(1, MIN(3, (area + 1100) / 1200));
        int floor_pct = 0, icky_pct = 0;
        bool reserved = area_is_reserved_or_dense(y1, y2, x1, x2, &floor_pct, &icky_pct);

        log_trace("Partition %d [%d,%d] (pass 2): mode=%s density=%s bounds=(%d,%d)-(%d,%d)",
                  pi, row, col, mode_str[mode], density_str[density], y1, x1, y2, x2);

        if (reserved && partitions_skipped >= skip_cap) {
            reserved = false;
            skipped_soft_fill++;
            density = DENSITY_SPARSE;
        }

        if (reserved) {
            log_trace("Partition %d [%d,%d]: skipping (reserved/quest/icky overlap)", pi, row, col);
            partitions_skipped++;
            continue;
        }

        /* Process the partition based on its mode (standard modes only here) */
        switch (mode)
        {
        case QUAD_MODE_CAVEY:
            {
                int blob_target = 2 + (y2 - y1) * (x2 - x1) / 400;
                if (blob_target > 6) blob_target = 6;
                int carved_blobs = 0;
                for (int b = 0; b < blob_target; ++b)
                    if (carve_ca_blob_anchor_bounds(y1, y2, x1, x2, style_idx))
                        carved_blobs++;
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, 0);
                set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                    0, 100, 0, 0, PARTITION_CHEST_ANCHOR_ANY);
                int std_count = scaled_attempts(2, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_DENSE) ? 4 : (density == DENSITY_SPARSE) ? 2 : 3, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 2 : 1, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : (density == DENSITY_DENSE) ? 1 : 1, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        case QUAD_MODE_RUINED:
            {
                int std_count = scaled_attempts(1, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : 1, area_factor);
                int int_count = scaled_attempts((density == DENSITY_DENSE) ? 2 : 1, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_DENSE) ? 1 : 0, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);

                int carve_count = 3 + (y2 - y1) * (x2 - x1) / 500;
                if (carve_count > 10) carve_count = 10;
                for (int b = 0; b < carve_count; ++b)
                    carve_bsp_slice_anchor_bounds(y1, y2, x1, x2);
                
                /* Add rubble to carved floor tiles (5-10-15% based on density) */
                int rubble_chance = (density == DENSITY_SPARSE) ? 3 : 
                                    (density == DENSITY_DENSE) ? 10 : 7;
                for (int gy = y1; gy <= y2; ++gy)
                {
                    for (int gx = x1; gx <= x2; ++gx)
                    {
                        if (!in_bounds_fully(gy, gx)) continue;
                        if (!cave_floor_bold(gy, gx)) continue;
                        if (cave_info[gy][gx] & CAVE_G_VAULT) continue; /* preserve greater vaults only */
                        if (rand_int(100) < rubble_chance)
                            cave_set_feat(gy, gx, FEAT_RUBBLE);
                    }
                }
                
                /* Add broken wall segments */
                for (int gy = y1 + 2; gy <= y2 - 2; ++gy)
                {
                    for (int gx = x1 + 2; gx <= x2 - 2; ++gx)
                    {
                        if (!in_bounds_fully(gy, gx)) continue;
                        if (cave_info[gy][gx] & CAVE_G_VAULT) continue; /* preserve greater vaults only */
                        if (cave_feat[gy][gx] != FEAT_WALL_OUTER) continue;
                        if (rand_int(100) < 30)
                        {
                            cave_set_feat(gy, gx, FEAT_FLOOR);
                            cave_info[gy][gx] |= CAVE_ROOM;
                            if (one_in_(2))
                                cave_set_feat(gy, gx, FEAT_RUBBLE);
                        }
                    }
                }

                set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
                    1, 0, 100, 0, PARTITION_CHEST_ANCHOR_BSP_SLICE);
                
            }
            break;
        default:
            {
                /* ROOMY or fallback: Traditional dungeon */
                int std_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        }

        /* Per-partition fallback */
        if (dun->cent_n == before_cent)
        {
            if (room_build_in_bounds(1, y1, y2, x1, x2) || room_build_in_bounds(2, y1, y2, x1, x2))
            {
                log_trace("Partition %d [%d,%d]: fallback simple room placed", pi, row, col);
                /* Update partition mode to ROOMY since we fell back to standard rooms */
                current_partition_modes[pi] = QUAD_MODE_ROOMY;
            }
        }
    }

    /* Log partition generation summary */
    log_debug("Generation summary: %d blocks, %dx%d grid (%d partitions), %d rooms created",
              blocks, grid_rows, grid_cols, partition_count, dun->cent_n);
    log_debug("Partition budgets: used t6=%d/t7=%d/t8=%d remaining t6=%d t7=%d t8=%d skipped_parts=%d soft_fill=%d",
              used_t6, used_t7, used_t8, budget_t6, budget_t7, budget_t8, partitions_skipped, skipped_soft_fill);
    log_trace("Greater vault partition summary: attempted=%s placed=%d",
              gv_partition_attempted ? "yes" : "no", used_t8);
    
    /* Detailed generation log summary */
    genlog_summary("Partition phase complete: %d rooms from %d partitions (%d skipped, %d soft-fill skipped)",
                   dun->cent_n, partition_count, partitions_skipped, skipped_soft_fill);
    genlog_summary("Room budgets - T6: %d used / T7: %d used / T8: %d used",
                   used_t6, used_t7, used_t8);
    
    /* Log mode distribution and persist labyrinth count for monster/stair bonuses */
    {
        int mode_counts_summary[6] = {0};
        for (int mi = 0; mi < partition_count; ++mi)
            mode_counts_summary[modes[mi]]++;
        current_labyrinth_partitions = mode_counts_summary[QUAD_MODE_LABYRINTH];
        genlog_partition("Mode distribution: ROOMY=%d CAVEY=%d RUINED=%d LABYRINTH=%d CHASM=%d BIG_CAVE=%d",
                         mode_counts_summary[0], mode_counts_summary[1], mode_counts_summary[2],
                         mode_counts_summary[3], mode_counts_summary[4], mode_counts_summary[5]);
    }
    
}

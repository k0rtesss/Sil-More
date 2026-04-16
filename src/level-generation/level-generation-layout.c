/* File: level-generation-layout.c */

#include "angband.h"
#include "level-generation/gen-log.h"
#include "log/log.h"
#include "level-generation/level-generation-internal.h"

#define LABYRINTH_START_DEPTH 7
#define BIG_CAVE_START_DEPTH 10
#define CHASM_START_DEPTH 14
#define SPECIAL_CAP_STEP 5
#define SPECIAL_CAP_MAX 3

bool room_kind_is_vault(byte kind)
{
    return (kind >= ROOM_KIND_INTERESTING);
}

void record_partition_metadata(
    const quadrant_mode_t* modes, const density_level_t* densities, int count)
{
    int capped = MIN(count, 25);
    for (int i = 0; i < capped; ++i)
    {
        current_partition_modes[i] = modes[i];
        current_partition_densities[i] = densities[i];
    }
}

void fallback_partition_grid_from_blocks(int blocks, int *rows, int *cols)
{
    if (blocks <= 9) {
        *rows = 2; *cols = 2;
    } else if (blocks == 10) {
        *rows = 3; *cols = 2;
    } else if (blocks <= 13) {
        *rows = 3; *cols = 3;
    } else if (blocks == 14) {
        *rows = 3; *cols = 4;
    } else if (blocks <= 16) {
        *rows = 4; *cols = 4;
    } else if (blocks <= 20) {
        *rows = 5; *cols = 4;
    } else {
        *rows = 5; *cols = 5;
    }
}

bool area_is_reserved_or_dense(int y1, int y2, int x1, int x2,
    int *floor_pct_out, int *icky_pct_out)
{
    int tiles = 0;
    int icky = 0;
    int floors = 0;

    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (!in_bounds_fully(y, x))
                continue;
            tiles++;
            if (cave_info[y][x] & CAVE_ICKY)
                icky++;
            if (cave_floor_bold(y, x))
                floors++;
        }
    }

    int floor_pct = (tiles > 0) ? (floors * 100) / tiles : 0;
    int icky_pct = (tiles > 0) ? (icky * 100) / tiles : 0;
    if (floor_pct_out) *floor_pct_out = floor_pct;
    if (icky_pct_out) *icky_pct_out = icky_pct;

    if (floor_pct >= 80) return true;
    if (icky_pct >= 60) return true;
    return false;
}

bool compute_partition_bounds(int pi, int rows, int cols, int *y1, int *y2,
    int *x1, int *x2)
{
    if (rows <= 0 || cols <= 0)
        return false;
    int total = rows * cols;
    if (pi < 0 || pi >= total)
        return false;

    int row = pi / cols;
    int col = pi % cols;

    /* Keep inclusive partition bounds aligned with partition_index_from_point():
     * each partition owns [start, next_start), so the inclusive max is next_start - 1.
     * Without that, adjacent partitions overlap by one row/column during carving
     * while runtime lookups only assign those tiles to one side. */
    int ly1 = (row * p_ptr->cur_map_hgt / rows);
    int ly2 = (((row + 1) * p_ptr->cur_map_hgt) / rows) - 1;
    int lx1 = (col * p_ptr->cur_map_wid / cols);
    int lx2 = (((col + 1) * p_ptr->cur_map_wid) / cols) - 1;

    if (ly1 < 1) ly1 = 1;
    if (lx1 < 1) lx1 = 1;
    if (ly2 >= p_ptr->cur_map_hgt - 1) ly2 = p_ptr->cur_map_hgt - 2;
    if (lx2 >= p_ptr->cur_map_wid - 1) lx2 = p_ptr->cur_map_wid - 2;
    if (ly2 < ly1 || lx2 < lx1)
        return false;

    *y1 = ly1;
    *y2 = ly2;
    *x1 = lx1;
    *x2 = lx2;
    return true;
}

bool level_has_chasm_partition(void)
{
    for (int pi = 0; pi < current_partition_count; ++pi)
    {
        if (current_partition_modes[pi] == QUAD_MODE_CHASM)
            return true;
    }

    return false;
}

void apply_chasm_partition_tags(void)
{
    if (current_partition_rows <= 0 || current_partition_cols <= 0
        || current_partition_count <= 0)
    {
        return;
    }

    for (int pi = 0; pi < current_partition_count; ++pi)
    {
        if (current_partition_modes[pi] != QUAD_MODE_CHASM)
            continue;

        int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
        if (!compute_partition_bounds(pi, current_partition_rows,
                current_partition_cols, &y1, &y2, &x1, &x2))
        {
            continue;
        }

        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                if (!in_bounds(y, x))
                    continue;

                if (cave_feat[y][x] == FEAT_CHASM
                    || ((cave_info[y][x] & (CAVE_ROOM | CAVE_CHASM_AREA))
                        == (CAVE_ROOM | CAVE_CHASM_AREA)))
                {
                    cave_info[y][x] |= CAVE_CHASM_AREA;
                }
                else
                {
                    cave_info[y][x] &= ~CAVE_CHASM_AREA;
                }
            }
        }
    }
}

int scaled_attempts(int base, int area_factor)
{
    if (area_factor <= 1)
        return base;
    int extra = (base + 1) / 2;
    return base + extra * (area_factor - 1);
}

quadrant_mode_t pick_weighted_mode(const int *weights, int count)
{
    int total = 0;
    for (int i = 0; i < count; ++i)
        total += MAX(0, weights[i]);
    if (total <= 0)
        return QUAD_MODE_ROOMY;

    int roll = rand_int(total);
    for (int i = 0; i < count; ++i)
    {
        int w = MAX(0, weights[i]);
        if (roll < w)
            return (quadrant_mode_t)i;
        roll -= w;
    }

    return QUAD_MODE_ROOMY;
}

static int special_mode_start_depth(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_LABYRINTH:
        return LABYRINTH_START_DEPTH;
    case QUAD_MODE_BIG_CAVE:
        return BIG_CAVE_START_DEPTH;
    case QUAD_MODE_CHASM:
        return CHASM_START_DEPTH;
    default:
        return 0;
    }
}

static int mode_cap_for_depth(
    quadrant_mode_t mode, int depth, int partition_count)
{
    int start = special_mode_start_depth(mode);
    if (start <= 0)
        return partition_count;
    if (depth < start)
        return 0;

    int cap = 1 + (depth - start) / SPECIAL_CAP_STEP;
    if (cap > SPECIAL_CAP_MAX)
        cap = SPECIAL_CAP_MAX;
    return cap;
}

int mode_weight_for_depth(quadrant_mode_t mode, int depth, int blocks,
    const int* mode_counts, int partition_count)
{
    int cap = mode_cap_for_depth(mode, depth, partition_count);
    if (cap == 0)
        return 0;
    if (mode_counts && mode_counts[mode] >= cap)
        return 0;

    (void)blocks;

    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        return 25;
    case QUAD_MODE_CAVEY:
        if (depth <= 12)
            return 15 + depth;
        else
            return MAX(5, 27 - (depth - 12));
    case QUAD_MODE_RUINED:
        return MAX(5, 15 + 10 - depth);
    case QUAD_MODE_LABYRINTH:
        return 10 + MAX(0, depth - LABYRINTH_START_DEPTH);
    case QUAD_MODE_BIG_CAVE:
        return 8 + MAX(0, depth - BIG_CAVE_START_DEPTH);
    case QUAD_MODE_CHASM:
        return 8 + MAX(0, depth - CHASM_START_DEPTH);
    default:
        return 0;
    }
}

bool room_build_in_bounds(int typ, int y1, int y2, int x1, int x2)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;
    if (y2 - y1 < 6 || x2 - x1 < 8)
        return false;

    int y = rand_range(MAX(5, y1 + 3), MIN(p_ptr->cur_map_hgt - 5, y2 - 3));
    int x = rand_range(MAX(5, x1 + 3), MIN(p_ptr->cur_map_wid - 5, x2 - 3));

    switch (typ)
    {
    case 8: return build_type8(y, x);
    case 7: return build_type7(y, x);
    case 6: return build_type6(y, x, false);
    case 2: return build_type2(y, x);
    case 1: return build_type1(y, x);
    default: return false;
    }
}

bool place_room_with_budget(int typ, int y1, int y2, int x1, int x2,
    int max_tries, int depth, int *budget_t6, int *budget_t7,
    int *budget_t8, int *used_t6, int *used_t7, int *used_t8)
{
    int actual = typ;
    (void)depth;

    if (actual == 8 && (!budget_t8 || *budget_t8 <= 0))
        actual = (budget_t7 && *budget_t7 > 0) ? 7 :
            ((budget_t6 && *budget_t6 > 0) ? 6 : 2);
    if (actual == 7 && budget_t7 && *budget_t7 <= 0)
        actual = (budget_t6 && *budget_t6 > 0) ? 6 : 2;
    if (actual == 6 && budget_t6 && *budget_t6 <= 0)
        actual = 2;

    for (int attempt = 0; attempt < max_tries; ++attempt)
    {
        if (room_build_in_bounds(actual, y1, y2, x1, x2))
        {
            if (actual == 6 && budget_t6 && *budget_t6 > 0)
            {
                (*budget_t6)--;
                if (used_t6) (*used_t6)++;
            }
            else if (actual == 7 && budget_t7 && *budget_t7 > 0)
            {
                (*budget_t7)--;
                if (used_t7) (*used_t7)++;
            }
            else if (actual == 8 && budget_t8 && *budget_t8 > 0)
            {
                (*budget_t8)--;
                if (used_t8) (*used_t8)++;
            }
            return true;
        }
    }
    return false;
}

static bool area_is_basic_granite(int y1, int x1, int y2, int x2)
{
    if (x2 >= MAX_DUNGEON_WID || y2 >= MAX_DUNGEON_HGT || y1 < 0 || x1 < 0)
        return false;

    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (cave_feat[y][x] != FEAT_WALL_EXTRA)
                return false;
        }
    }
    return true;
}

void cave_set_feat_style(int y, int x, int feat, int style_idx)
{
    if (style_idx >= 0)
        cave_set_feat_with_color(y, x, feat, style_idx);
    else
        cave_set_feat(y, x, feat);
}

void scatter_quartz_veins_in_bounds(int y1, int y2, int x1, int x2,
    u16b info_flag)
{
    int vein_count = 0;

    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
    {
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;

            int feat = cave_feat[gy][gx];
            if (feat < FEAT_WALL_EXTRA || feat > FEAT_WALL_SOLID)
                continue;

            bool adj_cave_floor = false;
            for (int dy = -1; dy <= 1 && !adj_cave_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !adj_cave_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        adj_cave_floor = true;
                    }
                }
            }

            if (adj_cave_floor && (rand_int(100) < 30))
            {
                cave_set_feat(gy, gx, FEAT_QUARTZ);
                cave_info[gy][gx] |= (CAVE_ROOM | info_flag);
                vein_count++;
            }
        }
    }

    if (vein_count > 0)
    {
        log_trace("scatter_quartz_veins: placed %d veins in bounds (%d,%d)-(%d,%d)",
            vein_count, y1, x1, y2, x2);
    }
}

bool bounds_have_chasm_tag(int y1, int y2, int x1, int x2)
{
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx) && (cave_info[gy][gx] & CAVE_CHASM_AREA))
                return true;
        }
    }
    return false;
}

bool carve_ca_blob_anchor_bounds(int y_min, int y_max, int x_min, int x_max,
    int style_idx)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;
    int old_h = p_ptr->cur_map_hgt;
    int old_w = p_ptr->cur_map_wid;
    if (y_max - y_min < 8 || x_max - x_min < 8)
        return false;
    int h = rand_range(8, MIN(14, y_max - y_min));
    int w = rand_range(10, MIN(16, x_max - x_min));
    int y1 = rand_range(y_min + 1, y_max - h);
    int x1 = rand_range(x_min + 1, x_max - w);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    if (y1 < 1 || x1 < 1 || y2 >= old_h - 1 || x2 >= old_w - 1)
        return false;
    for (int y = y1 - 1; y <= y2 + 1; ++y)
        for (int x = x1 - 1; x <= x2 + 1; ++x)
            if (cave_floor_bold(y, x))
                return false;

    bool grid[24][24];
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            grid[y][x] = (rand_int(100) < 45);

    int steps = 3;
    for (int step = 0; step < steps; ++step)
    {
        bool next[24][24];
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0)
                            continue;
                        int ny = y + dy, nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors++;
                        else if (grid[ny][nx])
                            neighbors++;
                    }
                }
                next[y][x] = grid[y][x] ? (neighbors >= 4) : (neighbors >= 5);
            }
        }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                grid[y][x] = next[y][x];
    }

    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    int floor_count = 0;
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            if (in_bounds_fully(gy, gx))
                cave_set_feat_style(gy, gx, FEAT_WALL_EXTRA, style_idx);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (!grid[y][x])
                continue;
            int gy = y1 + y;
            int gx = x1 + x;
            cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
            cave_info[gy][gx] |= CAVE_ROOM;
            floor_count++;
            if (gy < min_y) min_y = gy;
            if (gy > max_y) max_y = gy;
            if (gx < min_x) min_x = gx;
            if (gx > max_x) max_x = gx;
        }
    }
    if (floor_count < 8)
        return false;

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        {
            for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            {
                if (cave_floor_bold(gy, gx))
                    continue;
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dy || dx)
                        {
                            int ny = gy + dy, nx = gx + dx;
                            if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx))
                                adj++;
                        }
                if (adj >= 3 && one_in_(2 + pass))
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
    }

    static const int bleed_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
    {
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
        {
            if (!cave_floor_bold(gy, gx))
                continue;
            bool on_edge = (gy == y1 - 1) || (gy == y2 + 1)
                || (gx == x1 - 1) || (gx == x2 + 1);
            if (!on_edge)
                continue;
            for (int d = 0; d < 4; ++d)
            {
                int ny = gy + bleed_dirs[d][0];
                int nx = gx + bleed_dirs[d][1];
                if (!in_bounds_fully(ny, nx))
                    continue;
                if (cave_floor_bold(ny, nx))
                    continue;
                if (cave_feat[ny][nx] != FEAT_WALL_EXTRA)
                    continue;
                if (one_in_(4))
                {
                    cave_set_feat_style(ny, nx, FEAT_FLOOR, style_idx);
                    cave_info[ny][nx] |= CAVE_ROOM;
                    floor_count++;
                    if (ny < min_y) min_y = ny;
                    if (ny > max_y) max_y = ny;
                    if (nx < min_x) min_x = nx;
                    if (nx > max_x) max_x = nx;
                }
            }
        }
    }

    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, style_idx);
        }
    }

    int cy = min_y, cx = min_x;
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (!cave_floor_bold(ty, tx))
            continue;

        for (int dy = -1; dy <= 1 && !found_edge; ++dy)
        {
            for (int dx = -1; dx <= 1 && !found_edge; ++dx)
            {
                if (dy == 0 && dx == 0)
                    continue;
                if (in_bounds_fully(ty + dy, tx + dx)
                    && cave_feat[ty + dy][tx + dx] == FEAT_WALL_OUTER)
                {
                    cy = ty;
                    cx = tx;
                    found_edge = true;
                }
            }
        }
    }
    if (!found_edge)
    {
        for (int tries = 0; tries < 100; ++tries)
        {
            int ty = rand_range(min_y, max_y);
            int tx = rand_range(min_x, max_x);
            if (cave_floor_bold(ty, tx))
            {
                cy = ty;
                cx = tx;
                break;
            }
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, one_in_(4));

    scatter_quartz_veins_in_bounds(min_y, max_y, min_x, max_x, 0);

    log_trace("CA blob (bounded) anchor: bounds=(%d,%d)-(%d,%d) center=(%d,%d) floors=%d",
        min_y, min_x, max_y, max_x, cy, cx, floor_count);
    return true;
}

bool carve_bsp_slice_anchor_bounds(int y_min, int y_max, int x_min, int x_max)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;
    if (y_max - y_min < 8 || x_max - x_min < 10)
        return false;

    int h = rand_range(8, MIN(16, y_max - y_min));
    int w = rand_range(10, MIN(20, x_max - x_min));
    int y1 = rand_range(y_min + 1, y_max - h);
    int x1 = rand_range(x_min + 1, x_max - w);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    if (!area_is_basic_granite(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
        return false;

    typedef struct { int y1, x1, y2, x2; } slice_rect;
    slice_rect rects[10];
    int rect_count = 1;
    rects[0] = (slice_rect){y1, x1, y2, x2};

    int splits = rand_range(1, 3);
    for (int s = 0; s < splits && rect_count < 10; ++s)
    {
        int pick = rand_int(rect_count);
        slice_rect r = rects[pick];
        int rw = r.x2 - r.x1 + 1;
        int rh = r.y2 - r.y1 + 1;
        bool vertical = (rw > rh) ? true : (rh > rw ? false : one_in_(2));
        if (vertical && rw > 8)
        {
            int cut = rand_range(r.x1 + rw / 3, r.x2 - rw / 3);
            slice_rect a = {r.y1, r.x1, r.y2, cut};
            slice_rect b = {r.y1, cut + 1, r.y2, r.x2};
            if ((a.x2 - a.x1) >= 4 && (b.x2 - b.x1) >= 4)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
        else if (!vertical && rh > 6)
        {
            int cut = rand_range(r.y1 + rh / 3, r.y2 - rh / 3);
            slice_rect a = {r.y1, r.x1, cut, r.x2};
            slice_rect b = {cut + 1, r.x1, r.y2, r.x2};
            if ((a.y2 - a.y1) >= 3 && (b.y2 - b.y1) >= 3)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
    }

    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    int floor_count = 0;
    for (int i = 0; i < rect_count; ++i)
    {
        slice_rect *r = &rects[i];
        for (int y = r->y1 + 1; y < r->y2; ++y)
        {
            for (int x = r->x1 + 1; x < r->x2; ++x)
            {
                cave_set_feat(y, x, FEAT_FLOOR);
                cave_info[y][x] |= CAVE_ROOM;
                floor_count++;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
            }
        }
    }
    if (floor_count < 15)
        return false;

    int cy = min_y, cx = min_x;
    for (int tries = 0; tries < 200; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (cave_floor_bold(ty, tx))
        {
            cy = ty;
            cx = tx;
            break;
        }
    }

    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
                cave_set_feat(gy, gx, FEAT_WALL_OUTER);
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_BSP_SLICE, one_in_(5));
    log_trace("BSP slice (bounded) anchor: bounds=(%d,%d)-(%d,%d) center=(%d,%d) floor=%d rects=%d",
        min_y, min_x, max_y, max_x, cy, cx, floor_count, rect_count);
    return true;
}

static int prune_big_cave_detached_components(
    int y1, int y2, int x1, int x2, int style_idx)
{
    static u16b component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};
    int next_component = 1;
    int best_component = 0;
    int best_size = 0;
    int pruned = 0;

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            component[y][x] = 0;

    for (int sy = y1; sy <= y2; ++sy)
    {
        for (int sx = x1; sx <= x2; ++sx)
        {
            if (component[sy][sx] != 0)
                continue;
            if (!cave_floor_bold(sy, sx) || !(cave_info[sy][sx] & CAVE_ROOM))
                continue;

            int head = 0;
            int tail = 0;
            int size = 0;

            component[sy][sx] = (u16b)next_component;
            queue[tail++] = sy * MAX_DUNGEON_WID + sx;

            while (head < tail)
            {
                int cur = queue[head++];
                int cy = cur / MAX_DUNGEON_WID;
                int cx = cur % MAX_DUNGEON_WID;

                size++;

                for (int d = 0; d < 4; ++d)
                {
                    int ny = cy + ddy4[d];
                    int nx = cx + ddx4[d];

                    if (ny < y1 || ny > y2 || nx < x1 || nx > x2)
                        continue;
                    if (component[ny][nx] != 0)
                        continue;
                    if (!cave_floor_bold(ny, nx) || !(cave_info[ny][nx] & CAVE_ROOM))
                        continue;

                    component[ny][nx] = (u16b)next_component;
                    queue[tail++] = ny * MAX_DUNGEON_WID + nx;
                }
            }

            if (size > best_size)
            {
                best_size = size;
                best_component = next_component;
            }

            next_component++;
        }
    }

    if (best_component == 0)
        return 0;

    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (component[y][x] == 0 || component[y][x] == best_component)
                continue;

            cave_set_feat_style(y, x, FEAT_WALL_EXTRA, style_idx);
            cave_info[y][x] &= ~(CAVE_ROOM | CAVE_CHASM_AREA);
            pruned++;
        }
    }

    if (pruned > 0)
    {
        log_trace("Big cave cleanup: pruned %d detached floor tiles in bounds (%d,%d)-(%d,%d)",
            pruned, y1, x1, y2, x2);
        genlog_anchor("BIG_CAVE: pruned %d detached floor tiles in bounds (%d,%d)-(%d,%d)",
            pruned, y1, x1, y2, x2);
    }

    return pruned;
}

bool carve_big_cave_bounds(int y_min, int y_max, int x_min, int x_max,
    int style_idx, big_cave_type_t cave_type)
{
    (void)cave_type;

    if (dun->cent_n >= room_capacity_limit())
    {
        genlog_anchor("BIG_CAVE: rejected - room capacity limit reached");
        return false;
    }

    int avail_h = y_max - y_min + 1;
    int avail_w = x_max - x_min + 1;
    if (avail_h < 15 || avail_w < 20)
    {
        genlog_anchor("BIG_CAVE: rejected - bounds too small (%d,%d)-(%d,%d), avail=%dx%d",
            y_min, x_min, y_max, x_max, avail_h, avail_w);
        return false;
    }

    int margin_y1 = rand_range(2, MAX(4, avail_h / 5));
    int margin_y2 = rand_range(2, MAX(4, avail_h / 5));
    int margin_x1 = rand_range(2, MAX(4, avail_w / 5));
    int margin_x2 = rand_range(2, MAX(4, avail_w / 5));
    int y1 = y_min + margin_y1;
    int x1 = x_min + margin_x1;
    int y2 = y_max - margin_y2;
    int x2 = x_max - margin_x2;
    int h = y2 - y1 + 1;
    int w = x2 - x1 + 1;

    if (h < 10 || w < 12)
    {
        genlog_anchor("BIG_CAVE: rejected - after margins too small: h=%d w=%d",
            h, w);
        return false;
    }

    for (int y = y1 - 1; y <= y2 + 1; ++y)
    {
        for (int x = x1 - 1; x <= x2 + 1; ++x)
        {
            if (in_bounds_fully(y, x) && cave_floor_bold(y, x))
            {
                genlog_anchor("BIG_CAVE: rejected - floor already exists at (%d,%d) in bounds (%d,%d)-(%d,%d)",
                    y, x, y1, x1, y2, x2);
                return false;
            }
        }
    }

    int floor_count = 0;
    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;

    int num_centers = 4 + (h * w) / 150;
    if (num_centers > 12) num_centers = 12;

    int centers_y[12], centers_x[12];
    for (int c = 0; c < num_centers; ++c)
    {
        centers_y[c] = rand_range(y1 + 2, y2 - 2);
        centers_x[c] = rand_range(x1 + 2, x2 - 2);
    }

    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;

            int min_dist = 9999;
            for (int c = 0; c < num_centers; ++c)
            {
                int dy = ABS(gy - centers_y[c]);
                int dx = ABS(gx - centers_x[c]);
                int dist = dy + dx;
                if (dist < min_dist) min_dist = dist;
            }

            int threshold = (h + w) / 4;
            int noise = rand_int(threshold / 2);

            if (min_dist < threshold - noise)
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

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int gy = min_y; gy <= max_y; ++gy)
        {
            for (int gx = min_x; gx <= max_x; ++gx)
            {
                if (!in_bounds_fully(gy, gx) || cave_floor_bold(gy, gx))
                    continue;

                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if ((dy || dx) && in_bounds_fully(gy + dy, gx + dx)
                            && cave_floor_bold(gy + dy, gx + dx))
                        {
                            adj++;
                        }

                if (adj >= 6)
                {
                    cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                    cave_info[gy][gx] |= CAVE_ROOM;
                    floor_count++;
                }
            }
        }
    }

    int bite_count = 2 + rand_int(3);
    for (int bite = 0; bite < bite_count; ++bite)
    {
        int side = rand_int(4);
        int by = 0;
        int bx = 0;
        int radius_y = rand_range(3, MAX(4, h / 5));
        int radius_x = rand_range(4, MAX(5, w / 5));

        switch (side)
        {
        case 0:
            by = y1 + rand_range(0, 2);
            bx = rand_range(x1 + MAX(3, w / 6), x2 - MAX(3, w / 6));
            break;
        case 1:
            by = y2 - rand_range(0, 2);
            bx = rand_range(x1 + MAX(3, w / 6), x2 - MAX(3, w / 6));
            break;
        case 2:
            by = rand_range(y1 + MAX(3, h / 6), y2 - MAX(3, h / 6));
            bx = x1 + rand_range(0, 2);
            break;
        default:
            by = rand_range(y1 + MAX(3, h / 6), y2 - MAX(3, h / 6));
            bx = x2 - rand_range(0, 2);
            break;
        }

        for (int gy = y1; gy <= y2; ++gy)
        {
            for (int gx = x1; gx <= x2; ++gx)
            {
                int dy = ABS(gy - by);
                int dx = ABS(gx - bx);

                if (!cave_floor_bold(gy, gx) || !(cave_info[gy][gx] & CAVE_ROOM))
                    continue;
                if (dy > radius_y || dx > radius_x)
                    continue;

                int metric = (dy * 100) / MAX(1, radius_y)
                    + (dx * 100) / MAX(1, radius_x);
                if (metric > 125 + rand_int(20))
                    continue;

                cave_set_feat_style(gy, gx, FEAT_WALL_EXTRA, style_idx);
                cave_info[gy][gx] &= ~CAVE_ROOM;
                floor_count--;
            }
        }
    }

    for (int gy = min_y; gy <= max_y; ++gy)
    {
        for (int gx = min_x; gx <= max_x; ++gx)
        {
            if (!in_bounds_fully(gy, gx) || !cave_floor_bold(gy, gx))
                continue;

            int walls = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if ((dy || dx) && in_bounds_fully(gy + dy, gx + dx)
                        && !cave_floor_bold(gy + dy, gx + dx))
                    {
                        walls++;
                    }

            if (walls >= 3 && rand_int(100) < 45)
            {
                cave_set_feat_style(gy, gx, FEAT_WALL_EXTRA, style_idx);
                cave_info[gy][gx] &= ~CAVE_ROOM;
                floor_count--;
            }
        }
    }

    prune_big_cave_detached_components(y1, y2, x1, x2, style_idx);

    min_y = y2; max_y = y1; min_x = x2; max_x = x1;
    floor_count = 0;
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (cave_floor_bold(gy, gx) && (cave_info[gy][gx] & CAVE_ROOM))
            {
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }

    if (floor_count < 40)
        return false;

    int pillar_count = floor_count / 60;
    for (int p = 0; p < pillar_count; ++p)
    {
        for (int tries = 0; tries < 20; ++tries)
        {
            int py = rand_range(min_y + 2, max_y - 2);
            int px = rand_range(min_x + 2, max_x - 2);
            if (cave_floor_bold(py, px))
            {
                bool all_floor = true;
                for (int dy = -1; dy <= 1 && all_floor; ++dy)
                    for (int dx = -1; dx <= 1 && all_floor; ++dx)
                        if (!cave_floor_bold(py + dy, px + dx))
                            all_floor = false;
                if (all_floor)
                {
                    cave_set_feat_style(py, px, FEAT_WALL_EXTRA, style_idx);
                    break;
                }
            }
        }
    }

    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx) || cave_floor_bold(gy, gx))
                continue;
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, style_idx);
        }
    }

    int cy = (min_y + max_y) / 2;
    int cx = (min_x + max_x) / 2;
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (!cave_floor_bold(ty, tx))
            continue;

        for (int dy = -1; dy <= 1 && !found_edge; ++dy)
        {
            for (int dx = -1; dx <= 1 && !found_edge; ++dx)
            {
                if (dy == 0 && dx == 0)
                    continue;
                if (in_bounds_fully(ty + dy, tx + dx)
                    && cave_feat[ty + dy][tx + dx] == FEAT_WALL_OUTER)
                {
                    cy = ty;
                    cx = tx;
                    found_edge = true;
                }
            }
        }
    }
    if (!found_edge)
    {
        for (int tries = 0; tries < 100; ++tries)
        {
            int ty = rand_range(min_y, max_y);
            int tx = rand_range(min_x, max_x);
            if (cave_floor_bold(ty, tx))
            {
                cy = ty;
                cx = tx;
                break;
            }
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, false);

    scatter_quartz_veins_in_bounds(min_y, max_y, min_x, max_x, 0);

    log_trace("Big cave anchor: bounds=(%d,%d)-(%d,%d) center=(%d,%d) edge=%d floors=%d pillars=%d",
        min_y, min_x, max_y, max_x, cy, cx, found_edge, floor_count,
        pillar_count);
    genlog_anchor("BIG_CAVE: bounds=(%d,%d)-(%d,%d), %d floor tiles, %d pillars",
        min_y, min_x, max_y, max_x, floor_count, pillar_count);
    return true;
}

void apply_partition_and_room_glow_rules(void)
{
    if (current_partition_rows > 0 && current_partition_cols > 0
        && current_partition_count > 0)
    {
        for (int pi = 0; pi < current_partition_count; ++pi)
        {
            if (current_partition_modes[pi] != QUAD_MODE_LABYRINTH)
                continue;

            int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
            if (!compute_partition_bounds(pi, current_partition_rows,
                    current_partition_cols, &y1, &y2, &x1, &x2))
            {
                continue;
            }

            for (int y = y1; y <= y2; ++y)
            {
                for (int x = x1; x <= x2; ++x)
                    cave_info[y][x] &= ~(CAVE_GLOW);
            }
        }
    }

    for (int r = 0; r < dun->cent_n; ++r)
    {
        if (room_anchor_kind[r] != LAYOUT_ANCHOR_CA_BLOB)
            continue;

        int y1 = dun->corner[r].y1 - 1;
        int y2 = dun->corner[r].y2 + 1;
        int x1 = dun->corner[r].x1 - 1;
        int x2 = dun->corner[r].x2 + 1;

        if (y1 < 0) y1 = 0;
        if (x1 < 0) x1 = 0;
        if (y2 >= MAX_DUNGEON_HGT) y2 = MAX_DUNGEON_HGT - 1;
        if (x2 >= MAX_DUNGEON_WID) x2 = MAX_DUNGEON_WID - 1;

        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                if (!in_bounds(y, x))
                    continue;
                cave_info[y][x] &= ~(CAVE_GLOW);
            }
        }
    }
}

void remember_partition_grid(int rows, int cols, int count)
{
    current_partition_rows = rows;
    current_partition_cols = cols;
    current_partition_count = count;
    reset_partition_population_metadata();
    for (int i = 0; i < 25; ++i)
    {
        current_partition_modes[i] = QUAD_MODE_ROOMY;
        current_partition_densities[i] = DENSITY_NORMAL;
        current_partition_big_cave_types[i] = BIG_CAVE_NONE;
        current_partition_bridge_styles[i] = -1;
    }
}

/* Partition helper: compute bounds for a given partition index */
void apply_quadrant_generation_modes(void);
void repair_all_outer_walls(void);
static bool carve_chasm_with_bridges(int y_min, int y_max, int x_min, int x_max,
    int floor_style, int bridge_style);
static int room_connection_degree(int room_idx);
static bool connect_rooms_with_logging(int r1, int r2, const char *tag, bool allow_desperate);
static bool is_big_partition_mode(quadrant_mode_t mode);
bool generation_escape_tunnel_bold(int y, int x);

/* Disabled helpers kept for reference (see #if 0 blocks near usage sites). */
#if 0
static void seed_ca_blob_anchors(void);
static void seed_bsp_slice_anchors(void);
static void ensure_partition_connectivity(void);
#endif

/* Carve a small cellular-automata style blob and register it as an anchor */
#if 0
static bool carve_ca_blob_anchor(void)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;

    /* Pick blob dimensions (moderate footprint to avoid over-densifying) */
    int h = rand_range(8, 12);
    int w = rand_range(10, 16);
    int y1 = rand_range(3, p_ptr->cur_map_hgt - h - 3);
    int x1 = rand_range(3, p_ptr->cur_map_wid - w - 3);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    /* Ensure we are carving into untouched granite */
    /* Allow slight overlap with walls but not existing floors */
    if (y1 < 1 || x1 < 1 || y2 >= p_ptr->cur_map_hgt - 1 || x2 >= p_ptr->cur_map_wid - 1)
        return false;
    for (int y = y1 - 1; y <= y2 + 1; ++y)
    {
        for (int x = x1 - 1; x <= x2 + 1; ++x)
        {
            if (cave_floor_bold(y, x))
                return false;
        }
    }

    /* Simple CA grid stored on stack (max ~20x20) */
    bool grid[24][24];
    if (h > 24 || w > 24)
        return false;

    /* Seed noise with a bias to produce irregular shapes */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            grid[y][x] = (rand_int(100) < 45); /* 45% initial fill */

    /* Run several smoothing steps to create rounded blobs */
    int steps = 3;
    for (int step = 0; step < steps; ++step)
    {
        bool next[24][24];
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0)
                            continue;
                        int ny = y + dy;
                        int nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors++;
                        else if (grid[ny][nx])
                            neighbors++;
                    }
                }
                /* Slightly denser survival/birth to keep blobs cohesive */
                if (grid[y][x])
                    next[y][x] = (neighbors >= 4);
                else
                    next[y][x] = (neighbors >= 5);
            }
        }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                grid[y][x] = next[y][x];
    }

    /* Apply to dungeon */
    int floor_count = 0;
    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    /* Clear box to raw granite to avoid rectangular outlines */
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            if (in_bounds_fully(gy, gx))
                cave_set_feat(gy, gx, FEAT_WALL_EXTRA);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (!grid[y][x])
                continue;
            int gy = y1 + y;
            int gx = x1 + x;
            cave_set_feat(gy, gx, FEAT_FLOOR);
            cave_info[gy][gx] |= CAVE_ROOM;
            floor_count++;
            if (gy < min_y)
                min_y = gy;
            if (gy > max_y)
                max_y = gy;
            if (gx < min_x)
                min_x = gx;
            if (gx > max_x)
                max_x = gx;
        }
    }

    /* Ragged edge expansion to break rectangular silhouette */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        {
            for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            {
                if (cave_floor_bold(gy, gx))
                    continue;
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dy || dx)
                        {
                            int ny = gy + dy, nx = gx + dx;
                            if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx))
                                adj++;
                        }
                if (adj >= 3 && one_in_(2 + pass))
                {
                    cave_set_feat(gy, gx, FEAT_FLOOR);
                    cave_info[gy][gx] |= CAVE_ROOM;
                    floor_count++;
                    if (gy < min_y)
                        min_y = gy;
                    if (gy > max_y)
                        max_y = gy;
                    if (gx < min_x)
                        min_x = gx;
                    if (gx > max_x)
                        max_x = gx;
                }
            }
        }
    }

    /* Bleed outward a little to break boxy outlines */
    const int bleed_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
    {
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
        {
            if (!cave_floor_bold(gy, gx))
                continue;
            bool on_edge = (gy == y1 - 1) || (gy == y2 + 1) || (gx == x1 - 1) || (gx == x2 + 1);
            if (!on_edge)
                continue;
            for (int d = 0; d < 4; ++d)
            {
                int ny = gy + bleed_dirs[d][0];
                int nx = gx + bleed_dirs[d][1];
                if (!in_bounds_fully(ny, nx))
                    continue;
                if (cave_floor_bold(ny, nx))
                    continue;
                if (cave_feat[ny][nx] != FEAT_WALL_EXTRA)
                    continue;
                if (one_in_(4))
                {
                    cave_set_feat(ny, nx, FEAT_FLOOR);
                    cave_info[ny][nx] |= CAVE_ROOM;
                    floor_count++;
                    if (ny < min_y)
                        min_y = ny;
                    if (ny > max_y)
                        max_y = ny;
                    if (nx < min_x)
                        min_x = nx;
                    if (nx > max_x)
                        max_x = nx;
                }
            }
        }
    }

    if (floor_count < 8)
        return false;

    /* Set outer walls around floor tiles so tunnels can connect */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            /* Check if this wall borders any floor */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat(gy, gx, FEAT_WALL_OUTER);
            }
        }
    }

    /* Pick a center on a floor tile */
    int cy = min_y, cx = min_x;
    for (int tries = 0; tries < 200; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (cave_floor_bold(ty, tx))
        {
            cy = ty;
            cx = tx;
            break;
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, one_in_(3));

    /* Scatter quartz veins around the cave walls for natural appearance */
    scatter_quartz_veins_in_bounds(min_y, max_y, min_x, max_x, 0);

    log_trace("CA blob anchor: carved floor_count=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d)",
        floor_count, min_y, min_x, max_y, max_x, cy, cx);
    genlog_anchor("CA_BLOB: carved %d floor tiles at (%d,%d)-(%d,%d), center=(%d,%d)",
        floor_count, min_y, min_x, max_y, max_x, cy, cx);
    return true;
}
#endif

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

#if 0
/* Try to seed a few CA blob anchors in unused granite */
static void seed_ca_blob_anchors(void)
{
    /* Scale CA blobs by map size to add connective floor on big levels */
    int panels = (p_ptr->cur_map_hgt / PANEL_HGT);
    int target = 1 + panels / 3; /* e.g., 9 panels -> 4 blobs */
    if (target > 4) target = 4;
    int placed = 0;
    int max_attempts = target * 8;
    for (int attempt = 0; attempt < max_attempts && placed < target; ++attempt)
    {
        if (carve_ca_blob_anchor())
            placed++;
    }
    log_trace("CA blob seeding complete: placed=%d target=%d attempts=%d", placed, target, max_attempts);
}
#endif

/* Carve a BSP-style sliced region into rooms-like rectangles and register anchor */
#if 0
static bool carve_bsp_slice_anchor(void)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;

    int h = rand_range(10, 18);
    int w = rand_range(12, 24);
    int y1 = rand_range(3, p_ptr->cur_map_hgt - h - 3);
    int x1 = rand_range(3, p_ptr->cur_map_wid - w - 3);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    if (!area_is_basic_granite(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
        return false;

    typedef struct {
        int y1, x1, y2, x2;
    } slice_rect;

    slice_rect rects[12];
    int rect_count = 1;
    rects[0].y1 = y1;
    rects[0].x1 = x1;
    rects[0].y2 = y2;
    rects[0].x2 = x2;

    int splits = rand_range(2, 4);
    for (int s = 0; s < splits && rect_count < 12; ++s)
    {
        int pick = rand_int(rect_count);
        slice_rect r = rects[pick];
        int rw = r.x2 - r.x1 + 1;
        int rh = r.y2 - r.y1 + 1;
        bool vertical = (rw > rh) ? true : (rh > rw ? false : one_in_(2));

        if (vertical && rw > 10)
        {
            int cut = rand_range(r.x1 + rw / 3, r.x2 - rw / 3);
            slice_rect a = {r.y1, r.x1, r.y2, cut};
            slice_rect b = {r.y1, cut + 1, r.y2, r.x2};
            if ((a.x2 - a.x1) >= 5 && (b.x2 - b.x1) >= 5)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
        else if (!vertical && rh > 8)
        {
            int cut = rand_range(r.y1 + rh / 3, r.y2 - rh / 3);
            slice_rect a = {r.y1, r.x1, cut, r.x2};
            slice_rect b = {cut + 1, r.x1, r.y2, r.x2};
            if ((a.y2 - a.y1) >= 4 && (b.y2 - b.y1) >= 4)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
    }

    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    int floor_count = 0;
    for (int i = 0; i < rect_count; ++i)
    {
        slice_rect *r = &rects[i];
        for (int y = r->y1 + 1; y < r->y2; ++y)
        {
            for (int x = r->x1 + 1; x < r->x2; ++x)
            {
                cave_set_feat(y, x, FEAT_FLOOR);
                cave_info[y][x] |= CAVE_ROOM;
                floor_count++;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
            }
        }
    }

    if (floor_count < 20)
        return false;

    int cy = min_y, cx = min_x;
    for (int tries = 0; tries < 200; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (cave_floor_bold(ty, tx))
        {
            cy = ty;
            cx = tx;
            break;
        }
    }

    /* Set outer walls around floor tiles so tunnels can connect */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            /* Check if this wall borders any floor */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat(gy, gx, FEAT_WALL_OUTER);
            }
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_BSP_SLICE, one_in_(4));

    log_trace("BSP slice anchor: carved floor_count=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d) rects=%d",
        floor_count, min_y, min_x, max_y, max_x, cy, cx, rect_count);
    return true;
    return true;
}
#endif

#if 0
/* Try to seed BSP-sliced anchors in spare granite */
static void seed_bsp_slice_anchors(void)
{
    int target = (p_ptr->depth >= 8) ? 1 : 0;
    if (p_ptr->depth >= 20)
        target++;
    int placed = 0;
    for (int attempt = 0; attempt < 16 && placed < target; ++attempt)
    {
        if (carve_bsp_slice_anchor())
            placed++;
    }
    log_trace("BSP slice seeding complete: placed=%d target=%d", placed, target);
}
#endif

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

int vault_type8_generation_rarity(const vault_type* v_ptr, int depth)
{
    int rarity = v_ptr->rarity;

    if ((depth >= 6) && (v_ptr->flags & (VLT_SURFACE)))
    {
        rarity += (1 << depth);
    }

    return rarity;
}

bool quest_vault_surface_roll_allows(const vault_type* v_ptr, int depth)
{
    if (v_ptr->typ == 6)
    {
        if (depth < 6)
        {
            if (!(v_ptr->flags & (VLT_SURFACE)) && !one_in_(4))
                return false;
        }
        else if (v_ptr->flags & (VLT_SURFACE))
        {
            if (!one_in_(1 << depth))
                return false;
        }
    }
    else if ((depth >= 6) && (v_ptr->flags & (VLT_SURFACE)))
    {
        if (!one_in_(1 << depth))
            return false;
    }

    return true;
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
                int idx = row * grid_cols + col;
                if (idx >= partition_count || gv_interior_count >= 25)
                    continue;
                gv_candidates[gv_interior_count++] = idx;

                /* Prefer a non-special partition for greater vaults so their setpiece
                 * effects don't overlap with LABYRINTH/CHASM/BIG_CAVE zones. */
                quadrant_mode_t m = modes[idx];
                if (m != QUAD_MODE_LABYRINTH && m != QUAD_MODE_CHASM && m != QUAD_MODE_BIG_CAVE)
                {
                    if (gv_preferred_count < 25)
                        gv_preferred[gv_preferred_count++] = idx;
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
                int area = (y2 - y1) * (x2 - x1);
                int base_blobs = 2 + area / 400;  /* Scale with partition size */
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

/* Carve connection corridors at partition boundaries to ensure inter-partition connectivity.
 * This helps when caves/labyrinths in adjacent partitions don't naturally connect.
 * IMPROVED: Now searches deeper into partitions (15 tiles) and carves longer corridors (8 tiles).
 * Also tries multiple x/y positions per boundary segment. */
#if 0
static void ensure_partition_connectivity(void)
{
    int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
    int grid_rows = current_partition_rows;
    int grid_cols = current_partition_cols;
    
    /* Reuse the grid chosen during generation; fall back if unavailable */
    if (grid_rows <= 0 || grid_cols <= 0) {
        fallback_partition_grid_from_blocks(blocks, &grid_rows, &grid_cols);
    }
    
    int connections_added = 0;
    const int SEARCH_DEPTH = 15;  /* How far into partition to look for floor (was 5) */
    const int CORRIDOR_LEN = 8;   /* How long the carved corridor is (was 3) */
    const int ATTEMPTS_PER_SEGMENT = 3;  /* Try multiple positions per boundary segment */
    
    genlog_connect("ensure_partition_connectivity: %dx%d grid, searching %d deep, carving %d long",
                   grid_rows, grid_cols, SEARCH_DEPTH, CORRIDOR_LEN);
    
    /* Create horizontal boundary connections (between rows) */
    for (int row = 0; row < grid_rows - 1; ++row)
    {
        int boundary_y = ((row + 1) * p_ptr->cur_map_hgt / grid_rows);
        
        for (int col = 0; col < grid_cols; ++col)
        {
            int x1 = (col * p_ptr->cur_map_wid / grid_cols) + 2;
            int x2 = ((col + 1) * p_ptr->cur_map_wid / grid_cols) - 2;
            
            /* Try multiple x positions for better coverage */
            for (int attempt = 0; attempt < ATTEMPTS_PER_SEGMENT; ++attempt)
            {
                int cx = rand_range(x1 + 2, x2 - 2);
                
                /* Find nearest floor above and below the boundary */
                int floor_above_y = -1, floor_above_x = -1;
                int floor_below_y = -1, floor_below_x = -1;
                
                for (int dx = -5; dx <= 5; ++dx)
                {
                    int tx = cx + dx;
                    if (tx < 1 || tx >= p_ptr->cur_map_wid - 1) continue;
                    
                    for (int dy = 1; dy <= SEARCH_DEPTH; ++dy)
                    {
                        if (floor_above_y < 0 && in_bounds_fully(boundary_y - dy, tx) 
                            && cave_floor_bold(boundary_y - dy, tx))
                        {
                            floor_above_y = boundary_y - dy;
                            floor_above_x = tx;
                        }
                        if (floor_below_y < 0 && in_bounds_fully(boundary_y + dy, tx) 
                            && cave_floor_bold(boundary_y + dy, tx))
                        {
                            floor_below_y = boundary_y + dy;
                            floor_below_x = tx;
                        }
                    }
                }
                
                /* If both partitions have floor nearby, check if connection needed */
                if (floor_above_y >= 0 && floor_below_y >= 0)
                {
                    /* Check if boundary is already connected */
                    bool boundary_connected = false;
                    for (int dx = -3; dx <= 3; ++dx)
                    {
                        int tx = cx + dx;
                        for (int dy = -2; dy <= 2; ++dy)
                        {
                            if (in_bounds_fully(boundary_y + dy, tx) && cave_floor_bold(boundary_y + dy, tx))
                            {
                                boundary_connected = true;
                                break;
                            }
                        }
                        if (boundary_connected) break;
                    }
                    
                    if (!boundary_connected)
                    {
                        /* Carve from floor_above to floor_below through the boundary */
                        int mid_x = (floor_above_x + floor_below_x) / 2;
                        
                        /* Carve vertical corridor centered on boundary */
                        for (int dy = -CORRIDOR_LEN; dy <= CORRIDOR_LEN; ++dy)
                        {
                            int ty = boundary_y + dy;
                            if (in_bounds_fully(ty, mid_x) && 
                                (cave_feat[ty][mid_x] == FEAT_WALL_EXTRA || cave_feat[ty][mid_x] == FEAT_WALL_OUTER))
                            {
                                cave_set_feat(ty, mid_x, FEAT_FLOOR);
                            }
                        }
                        connections_added++;
                        genlog_connect("H-boundary row=%d col=%d: carved at x=%d from y=%d to y=%d",
                                       row, col, mid_x, boundary_y - CORRIDOR_LEN, boundary_y + CORRIDOR_LEN);
                        break;  /* Only one connection per segment needed */
                    }
                }
            }
        }
    }
    
    /* Create vertical boundary connections (between columns) */
    for (int col = 0; col < grid_cols - 1; ++col)
    {
        int boundary_x = ((col + 1) * p_ptr->cur_map_wid / grid_cols);
        
        for (int row = 0; row < grid_rows; ++row)
        {
            int y1 = (row * p_ptr->cur_map_hgt / grid_rows) + 2;
            int y2 = ((row + 1) * p_ptr->cur_map_hgt / grid_rows) - 2;
            
            for (int attempt = 0; attempt < ATTEMPTS_PER_SEGMENT; ++attempt)
            {
                int cy = rand_range(y1 + 2, y2 - 2);
                
                int floor_left_y = -1, floor_left_x = -1;
                int floor_right_y = -1, floor_right_x = -1;
                
                for (int dy = -5; dy <= 5; ++dy)
                {
                    int ty = cy + dy;
                    if (ty < 1 || ty >= p_ptr->cur_map_hgt - 1) continue;
                    
                    for (int dx = 1; dx <= SEARCH_DEPTH; ++dx)
                    {
                        if (floor_left_x < 0 && in_bounds_fully(ty, boundary_x - dx) 
                            && cave_floor_bold(ty, boundary_x - dx))
                        {
                            floor_left_y = ty;
                            floor_left_x = boundary_x - dx;
                        }
                        if (floor_right_x < 0 && in_bounds_fully(ty, boundary_x + dx) 
                            && cave_floor_bold(ty, boundary_x + dx))
                        {
                            floor_right_y = ty;
                            floor_right_x = boundary_x + dx;
                        }
                    }
                }
                
                if (floor_left_x >= 0 && floor_right_x >= 0)
                {
                    bool boundary_connected = false;
                    for (int dy = -3; dy <= 3; ++dy)
                    {
                        int ty = cy + dy;
                        for (int dx = -2; dx <= 2; ++dx)
                        {
                            if (in_bounds_fully(ty, boundary_x + dx) && cave_floor_bold(ty, boundary_x + dx))
                            {
                                boundary_connected = true;
                                break;
                            }
                        }
                        if (boundary_connected) break;
                    }
                    
                    if (!boundary_connected)
                    {
                        int mid_y = (floor_left_y + floor_right_y) / 2;
                        
                        for (int dx = -CORRIDOR_LEN; dx <= CORRIDOR_LEN; ++dx)
                        {
                            int tx = boundary_x + dx;
                            if (in_bounds_fully(mid_y, tx) && 
                                (cave_feat[mid_y][tx] == FEAT_WALL_EXTRA || cave_feat[mid_y][tx] == FEAT_WALL_OUTER))
                            {
                                cave_set_feat(mid_y, tx, FEAT_FLOOR);
                            }
                        }
                        connections_added++;
                        genlog_connect("V-boundary row=%d col=%d: carved at y=%d from x=%d to x=%d",
                                       row, col, mid_y, boundary_x - CORRIDOR_LEN, boundary_x + CORRIDOR_LEN);
                        break;
                    }
                }
            }
        }
    }
    
    if (connections_added > 0)
    {
        log_trace("Partition connectivity: added %d boundary connections", connections_added);
        genlog_connect("Partition connectivity: added %d boundary connections total", connections_added);
    }
    else
    {
        genlog_connect("Partition connectivity: no new connections needed");
    }
}
#endif

typedef struct {
    rectangle bounds;
    coord center;
    int rooms[CENT_MAX];
    int room_count;
    int hub_room;
} partition_link_data_t;

int partition_index_from_point(int y, int x, int rows, int cols)
{
    if (rows <= 0 || cols <= 0) return -1;
    if (p_ptr->cur_map_hgt <= 0 || p_ptr->cur_map_wid <= 0) return -1;
    int row = (y * rows) / p_ptr->cur_map_hgt;
    int col = (x * cols) / p_ptr->cur_map_wid;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= rows) row = rows - 1;
    if (col >= cols) col = cols - 1;
    return row * cols + col;
}

static int room_partition_index(int room_idx)
{
    if (room_idx < 0 || room_idx >= dun->cent_n)
        return -1;
    if (current_partition_rows <= 0 || current_partition_cols <= 0
        || current_partition_count <= 0)
    {
        return -1;
    }

    return partition_index_from_point(dun->cent[room_idx].y, dun->cent[room_idx].x,
        current_partition_rows, current_partition_cols);
}

bool tunnel_should_mark_escape(int r1, int r2)
{
    int p1 = room_partition_index(r1);
    int p2 = room_partition_index(r2);
    bool big1 = (p1 >= 0 && p1 < current_partition_count && p1 < 25
        && is_big_partition_mode(current_partition_modes[p1]));
    bool big2 = (p2 >= 0 && p2 < current_partition_count && p2 < 25
        && is_big_partition_mode(current_partition_modes[p2]));

    if (!big1 && !big2)
        return false;

    if (p1 >= 0 && p2 >= 0 && p1 == p2)
        return false;

    return true;
}

static int room_connection_degree(int room_idx)
{
    if (room_idx < 0 || room_idx >= dun->cent_n)
        return 0;
    int deg = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        if (dun->connection[room_idx][i])
            deg++;
    }
    return deg;
}

static bool connect_rooms_with_logging(int r1, int r2, const char *tag, bool allow_desperate)
{
    if (r1 < 0 || r2 < 0 || r1 == r2)
        return false;

    if (dun->connection[r1][r2])
        return true;

    bool ok = connect_two_rooms(r1, r2, true, false);
    if (!ok && allow_desperate)
        ok = connect_two_rooms(r1, r2, true, true);

    if (ok && tag)
    {
        int dist = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y, dun->cent[r2].x);
        genlog_connect("%s: linked room %d -> %d (dist=%d)", tag, r1, r2, dist);
    }
    return ok;
}

static bool is_big_partition_mode(quadrant_mode_t mode)
{
    return (mode == QUAD_MODE_LABYRINTH || mode == QUAD_MODE_BIG_CAVE || mode == QUAD_MODE_CHASM);
}

static bool big_partition_boundary_floor_ok(quadrant_mode_t mode, int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (!cave_floor_bold(y, x))
        return false;
    if (cave_feat[y][x] == FEAT_CHASM)
        return false;
    if (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT))
        return false;
    if (mode == QUAD_MODE_CHASM)
        return ((cave_info[y][x] & (CAVE_ROOM | CAVE_CHASM_AREA))
            == (CAVE_ROOM | CAVE_CHASM_AREA));
    return ((cave_info[y][x] & CAVE_ROOM) != 0);
}

static int partition_bridge_style_for_index(int pi)
{
    if (pi < 0 || pi >= current_partition_count || pi >= 25)
        return -1;
    return current_partition_bridge_styles[pi];
}

/* Organic cave/blob anchors should meet corridors as open floor, not doors.
 * Chasm anchors reuse the same kind marker for shaping, so exclude them. */
static bool room_prefers_floor_thresholds(int room_idx)
{
    int cy, cx;

    if (room_idx < 0 || room_idx >= dun->cent_n || room_idx >= CENT_MAX)
        return false;
    if (room_anchor_kind[room_idx] != LAYOUT_ANCHOR_CA_BLOB)
        return false;

    cy = dun->cent[room_idx].y;
    cx = dun->cent[room_idx].x;
    if (!in_bounds_fully(cy, cx))
        return false;

    return ((cave_info[cy][cx] & CAVE_CHASM_AREA) == 0);
}

static bool tunnel_prefers_floor_thresholds(int r1, int r2)
{
    return room_prefers_floor_thresholds(r1)
        || room_prefers_floor_thresholds(r2);
}

static void carve_floor_threshold(
    int y, int x, int r1, int r2, bool mark_escape)
{
    cave_set_feat(y, x, FEAT_FLOOR);
    cave_corridor1[y][x] = r1;
    cave_corridor2[y][x] = r2;
    if (mark_escape)
        mark_generation_escape_tunnel(y, x);
}

/* Shared-boundary fallback connector for adjacent partitions.
 * Standard tunnel rules often reject some otherwise-valid joins, so when two
 * partitions have native walkable floor near the same shared boundary we carve
 * a straight doorway/corridor between those two populated sides. */
static bool carve_straight_big_partition_connector(
    int y1, int x1, int y2, int x2, int r1, int r2, int rows, int cols)
{
    int dy = (y2 > y1) ? 1 : (y2 < y1) ? -1 : 0;
    int dx = (x2 > x1) ? 1 : (x2 < x1) ? -1 : 0;
    bool floor_thresholds = tunnel_prefers_floor_thresholds(r1, r2);

    /* Must be a straight segment. */
    if (!((dy == 0) ^ (dx == 0)))
        return false;

    if (morgoth_segment_blocked(y1, x1, y2, x2, 2))
        return false;

    bool carved = false;
    int y = y1;
    int x = x1;

    for (;;)
    {
        if (!in_bounds_fully(y, x))
            return false;

        if (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT))
            return false;

        int feat = cave_feat[y][x];
        if (feat == FEAT_WALL_PERM)
            return false;

        if (feat == FEAT_WALL_OUTER)
        {
            if (floor_thresholds)
            {
                carve_floor_threshold(y, x, r1, r2, false);
            }
            else
            {
                cave_set_feat(y, x, FEAT_DOOR_HEAD);
            }
            carved = true;
        }
        else if (feat == FEAT_WALL_EXTRA || feat == FEAT_CHASM)
        {
            if (feat == FEAT_CHASM)
            {
                int pi = partition_index_from_point(y, x, rows, cols);
                int bridge_style = partition_bridge_style_for_index(pi);

                if (pi < 0 || pi >= 25 || pi >= current_partition_count
                    || current_partition_modes[pi] != QUAD_MODE_CHASM)
                {
                    return false;
                }

                cave_set_feat_style(y, x, FEAT_FLOOR, bridge_style);
                cave_info[y][x] |= CAVE_CHASM_AREA;
                cave_info[y][x] &= ~CAVE_ROOM;
            }
            else
            {
                cave_set_feat(y, x, FEAT_FLOOR);
            }
            cave_corridor1[y][x] = r1;
            cave_corridor2[y][x] = r2;
            carved = true;
        }
        else if ((feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL))
        {
            /* Don't carve through inner/solid room walls, rubble walls, etc. */
            if (feat != FEAT_WALL_EXTRA)
                return false;
        }
        else if (feature_is_any_door(feat) || feat == FEAT_FLOOR)
        {
            /* Already passable; keep it. */
        }
        else
        {
            /* Avoid unexpected terrain (stairs, traps, etc.) */
            return false;
        }

        if (!(cave_info[y][x] & CAVE_ROOM))
            mark_generation_escape_tunnel(y, x);

        if (y == y2 && x == x2)
            break;
        y += dy;
        x += dx;
    }

    return carved;
}

static bool connect_adjacent_big_partitions_by_boundary(
    int pi_a, int pi_b, const rectangle *bounds_a, const rectangle *bounds_b,
    int rows, int cols, int hub_a, int hub_b, bool vertical_boundary)
{
    const int SEARCH_DEPTH = 32;
    quadrant_mode_t mode_a = current_partition_modes[pi_a];
    quadrant_mode_t mode_b = current_partition_modes[pi_b];

    if (hub_a < 0 || hub_b < 0)
        return false;

    if (!bounds_a || !bounds_b)
        return false;

    if (vertical_boundary)
    {
        /* A is left of B: boundary at the start column of B. */
        int boundary_x = ((pi_b % cols) * p_ptr->cur_map_wid / cols);
        int y_lo = MAX(bounds_a->y1, bounds_b->y1);
        int y_hi = MIN(bounds_a->y2, bounds_b->y2);

        boundary_x = MAX(1, MIN(p_ptr->cur_map_wid - 2, boundary_x));
        if (y_hi - y_lo < 6)
            return false;

        int best_y = -1;
        int best_left = -1;
        int best_right = -1;
        int best_len = 999999;

        for (int y = y_lo + 2; y <= y_hi - 2; ++y)
        {
            int x_left = -1;
            for (int dx = 1; dx <= SEARCH_DEPTH; ++dx)
            {
                int x = boundary_x - dx;
                if (x < bounds_a->x1)
                    break;
                if (partition_index_from_point(y, x, rows, cols) != pi_a)
                    continue;
                if (big_partition_boundary_floor_ok(mode_a, y, x))
                {
                    x_left = x;
                    break;
                }
            }

            int x_right = -1;
            for (int dx = 0; dx <= SEARCH_DEPTH; ++dx)
            {
                int x = boundary_x + dx;
                if (x > bounds_b->x2)
                    break;
                if (partition_index_from_point(y, x, rows, cols) != pi_b)
                    continue;
                if (big_partition_boundary_floor_ok(mode_b, y, x))
                {
                    x_right = x;
                    break;
                }
            }

            if (x_left >= 0 && x_right >= 0 && x_left < x_right)
            {
                int len = x_right - x_left;
                if (len < best_len || (len == best_len && one_in_(2)))
                {
                    best_len = len;
                    best_y = y;
                    best_left = x_left;
                    best_right = x_right;
                }
            }
        }

        if (best_y < 0)
            return false;

        if (!carve_straight_big_partition_connector(
                best_y, best_left, best_y, best_right,
                hub_a, hub_b, rows, cols))
            return false;

        dun->connection[hub_a][hub_b] = true;
        dun->connection[hub_b][hub_a] = true;
        genlog_connect("Partition boundary: carved H link rooms %d<->%d at y=%d x=%d..%d",
            hub_a, hub_b, best_y, best_left, best_right);
        return true;
    }

    /* Horizontal boundary: A is above B. */
    int boundary_y = ((pi_b / cols) * p_ptr->cur_map_hgt / rows);
    int x_lo = MAX(bounds_a->x1, bounds_b->x1);
    int x_hi = MIN(bounds_a->x2, bounds_b->x2);

    boundary_y = MAX(1, MIN(p_ptr->cur_map_hgt - 2, boundary_y));
    if (x_hi - x_lo < 6)
        return false;

    int best_x = -1;
    int best_up = -1;
    int best_down = -1;
    int best_len = 999999;

    for (int x = x_lo + 2; x <= x_hi - 2; ++x)
    {
        int y_up = -1;
        for (int dy = 1; dy <= SEARCH_DEPTH; ++dy)
        {
            int y = boundary_y - dy;
            if (y < bounds_a->y1)
                break;
            if (partition_index_from_point(y, x, rows, cols) != pi_a)
                continue;
            if (big_partition_boundary_floor_ok(mode_a, y, x))
            {
                y_up = y;
                break;
            }
        }

        int y_down = -1;
        for (int dy = 0; dy <= SEARCH_DEPTH; ++dy)
        {
            int y = boundary_y + dy;
            if (y > bounds_b->y2)
                break;
            if (partition_index_from_point(y, x, rows, cols) != pi_b)
                continue;
            if (big_partition_boundary_floor_ok(mode_b, y, x))
            {
                y_down = y;
                break;
            }
        }

        if (y_up >= 0 && y_down >= 0 && y_up < y_down)
        {
            int len = y_down - y_up;
            if (len < best_len || (len == best_len && one_in_(2)))
            {
                best_len = len;
                best_x = x;
                best_up = y_up;
                best_down = y_down;
            }
        }
    }

    if (best_x < 0)
        return false;

    if (!carve_straight_big_partition_connector(
            best_up, best_x, best_down, best_x,
            hub_a, hub_b, rows, cols))
        return false;

    dun->connection[hub_a][hub_b] = true;
    dun->connection[hub_b][hub_a] = true;
    genlog_connect("Partition boundary: carved V link rooms %d<->%d at x=%d y=%d..%d",
        hub_a, hub_b, best_x, best_up, best_down);
    return true;
}

static void seed_partition_adjacency(const int *room_to_part, int part_count, bool adj[25][25], int degree[25])
{
    for (int i = 0; i < part_count; ++i)
        degree[i] = 0;

    for (int i = 0; i < part_count; ++i)
        for (int j = 0; j < part_count; ++j)
            adj[i][j] = false;

    for (int a = 0; a < dun->cent_n; ++a)
    {
        int pa = (a < CENT_MAX) ? room_to_part[a] : -1;
        if (pa < 0 || pa >= part_count) continue;

        for (int b = a + 1; b < dun->cent_n; ++b)
        {
            if (!dun->connection[a][b]) continue;
            int pb = (b < CENT_MAX) ? room_to_part[b] : -1;
            if (pb < 0 || pb >= part_count || pb == pa) continue;
            if (!adj[pa][pb])
            {
                adj[pa][pb] = adj[pb][pa] = true;
                degree[pa]++;
                degree[pb]++;
            }
        }
    }
}

static void mark_partition_edge(int p1, int p2, bool adj[25][25], int degree[25])
{
    if (p1 < 0 || p2 < 0 || p1 >= 25 || p2 >= 25 || p1 == p2)
        return;
    if (!adj[p1][p2])
    {
        adj[p1][p2] = adj[p2][p1] = true;
        degree[p1]++;
        degree[p2]++;
    }
}

static int choose_partition_hub(const partition_link_data_t *part)
{
    int best = -1;
    int best_rank = -1;
    int best_area = -1;
    int best_dist = 999999;

    int limit = MIN(part->room_count, CENT_MAX);
    for (int i = 0; i < limit; ++i)
    {
        int r = part->rooms[i];
        int area = (dun->corner[r].y2 - dun->corner[r].y1 + 1) *
                   (dun->corner[r].x2 - dun->corner[r].x1 + 1);
        int dist = distance(dun->cent[r].y, dun->cent[r].x, part->center.y, part->center.x);
        int rank = room_anchor_requires_neighbor[r] ? 2 :
                   (room_anchor_kind[r] != LAYOUT_ANCHOR_NONE ? 1 : 0);

        if (rank > best_rank ||
            (rank == best_rank && area > best_area) ||
            (rank == best_rank && area == best_area && dist < best_dist))
        {
            best = r;
            best_rank = rank;
            best_area = area;
            best_dist = dist;
        }
    }
    return best;
}

static int find_anchor_target(int src, const int *room_to_part, const bool *skip, int part_count)
{
    int src_part = (src >= 0 && src < CENT_MAX) ? room_to_part[src] : -1;
    int src_piece = (src >= 0 && src < dun->cent_n) ? dun->piece[src] : -1;
    int best = -1;
    int best_tier = 10;
    int best_dist = 999999;

    for (int r = 0; r < dun->cent_n; ++r)
    {
        if (r == src) continue;
        if (skip && skip[r]) continue;
        if (dun->connection[src][r]) continue;

        int tier = 2;
        if (src_piece > 0 && dun->piece[r] > 0 && dun->piece[r] != src_piece)
            tier = 0;
        else if (room_to_part && r < CENT_MAX && room_to_part[r] != src_part)
            tier = 1;

        if (part_count > 0 && room_to_part && (room_to_part[r] < 0 || room_to_part[r] >= part_count))
            continue;

        int dist = distance(dun->cent[src].y, dun->cent[src].x, dun->cent[r].y, dun->cent[r].x);
        if (tier < best_tier || (tier == best_tier && dist < best_dist))
        {
            best_tier = tier;
            best_dist = dist;
            best = r;
        }
    }
    return best;
}

static void connect_anchor_backbone(const int *room_to_part, int part_count)
{
    if (layout_anchor_count <= 0 || dun->cent_n <= 0)
        return;

    (void)dungeon_pieces();

    int anchors_linked = 0;
    int anchors_considered = 0;

    for (int i = 0; i < layout_anchor_count; ++i)
    {
        int r = layout_anchors[i].room_slot;
        if (r < 0 || r >= dun->cent_n)
            continue;

        anchors_considered++;
        int area = (dun->corner[r].y2 - dun->corner[r].y1 + 1) *
                   (dun->corner[r].x2 - dun->corner[r].x1 + 1);
        int target_degree = 1;
        if (layout_anchors[i].requires_neighbor)
            target_degree = 2;
        if (area >= 600)
            target_degree = MAX(target_degree, 2);
        if (area >= 900)
            target_degree = MAX(target_degree, 3);

        int deg = room_connection_degree(r);
        bool tried[CENT_MAX];
        for (int t = 0; t < CENT_MAX; ++t) tried[t] = false;

        int attempts = 0;
        while (deg < target_degree && attempts < 8)
        {
            attempts++;
            int target = find_anchor_target(r, room_to_part, tried, part_count);
            if (target < 0)
                break;

            tried[target] = true;
            if (connect_rooms_with_logging(r, target, "Anchor backbone", true))
            {
                anchors_linked++;
                deg++;
                (void)dungeon_pieces();
            }
        }
    }

    if (anchors_linked > 0)
    {
        genlog_connect("Anchor backbone: linked %d/%d anchors to reduce isolation", anchors_linked, anchors_considered);
    }
}

/* Add connective tissue between partitions by linking a representative room in each partition,
 * then ensure special anchors have multiple exits to avoid dead ends. */
void connect_partition_hubs(void)
{
    int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
    int rows = current_partition_rows;
    int cols = current_partition_cols;
    int count = current_partition_count;

    if (rows <= 0 || cols <= 0) {
        fallback_partition_grid_from_blocks(blocks, &rows, &cols);
        count = rows * cols;
    }
    if (count <= 1 || rows <= 0 || cols <= 0)
        return;

    partition_link_data_t parts[25];
    int room_to_part[CENT_MAX];
    for (int i = 0; i < CENT_MAX; ++i) room_to_part[i] = -1;

    for (int pi = 0; pi < count && pi < 25; ++pi)
    {
        parts[pi].room_count = 0;
        parts[pi].hub_room = -1;
        int y1, y2, x1, x2;
        if (compute_partition_bounds(pi, rows, cols, &y1, &y2, &x1, &x2))
        {
            parts[pi].bounds.y1 = y1;
            parts[pi].bounds.y2 = y2;
            parts[pi].bounds.x1 = x1;
            parts[pi].bounds.x2 = x2;
            parts[pi].center.y = (y1 + y2) / 2;
            parts[pi].center.x = (x1 + x2) / 2;
        }
    }

    for (int r = 0; r < dun->cent_n && r < CENT_MAX; ++r)
    {
        int pi = partition_index_from_point(dun->cent[r].y, dun->cent[r].x, rows, cols);
        room_to_part[r] = pi;
        if (pi < 0 || pi >= count || pi >= 25)
            continue;
        int idx = parts[pi].room_count++;
        if (idx < CENT_MAX)
            parts[pi].rooms[idx] = r;
    }

    for (int pi = 0; pi < count && pi < 25; ++pi)
        parts[pi].hub_room = choose_partition_hub(&parts[pi]);

    bool adj[25][25];
    int degree[25];
    seed_partition_adjacency(room_to_part, count, adj, degree);

    /* Connect adjacent big partitions (labyrinth, big_cave, chasm) FIRST before regular backbone */
    /* This ensures big partitions get priority connections to each other */
    int big_links = 0;
    int big_adjacencies_found = 0;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int idx = row * cols + col;
            if (idx >= count || idx >= 25)
                continue;
            
            quadrant_mode_t mode = current_partition_modes[idx];
            bool is_big = is_big_partition_mode(mode);
            if (!is_big)
                continue;
            
            int hub_here = parts[idx].hub_room;
            if (hub_here < 0)
                continue;
            
            /* Check right neighbor */
            if (col + 1 < cols)
            {
                int idx_r = row * cols + (col + 1);
                if (idx_r < count && idx_r < 25)
                {
                    quadrant_mode_t mode_r = current_partition_modes[idx_r];
                    bool is_big_r = is_big_partition_mode(mode_r);
                    if (is_big_r && !adj[idx][idx_r])
                    {
                        big_adjacencies_found++;
                        int hub_right = parts[idx_r].hub_room;
                        bool ok = false;
                        if (hub_right >= 0)
                        {
                            ok = connect_rooms_with_logging(hub_here, hub_right, "Big partition bridge H", true);
                            if (!ok)
                            {
                                ok = connect_adjacent_big_partitions_by_boundary(
                                    idx, idx_r, &parts[idx].bounds, &parts[idx_r].bounds,
                                    rows, cols, hub_here, hub_right, true);
                            }
                        }

                        if (ok)
                        {
                            mark_partition_edge(idx, idx_r, adj, degree);
                            big_links++;
                            genlog_connect("Big partition bridge: connected %s at [%d,%d] to %s at [%d,%d] (horizontal)",
                                         mode == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col,
                                         mode_r == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode_r == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col + 1);
                        }
                    }
                }
            }
            
            /* Check down neighbor */
            if (row + 1 < rows)
            {
                int idx_d = (row + 1) * cols + col;
                if (idx_d < count && idx_d < 25)
                {
                    quadrant_mode_t mode_d = current_partition_modes[idx_d];
                    bool is_big_d = is_big_partition_mode(mode_d);
                    if (is_big_d && !adj[idx][idx_d])
                    {
                        big_adjacencies_found++;
                        int hub_down = parts[idx_d].hub_room;
                        bool ok = false;
                        if (hub_down >= 0)
                        {
                            ok = connect_rooms_with_logging(hub_here, hub_down, "Big partition bridge V", true);
                            if (!ok)
                            {
                                ok = connect_adjacent_big_partitions_by_boundary(
                                    idx, idx_d, &parts[idx].bounds, &parts[idx_d].bounds,
                                    rows, cols, hub_here, hub_down, false);
                            }
                        }

                        if (ok)
                        {
                            mark_partition_edge(idx, idx_d, adj, degree);
                            big_links++;
                            genlog_connect("Big partition bridge: connected %s at [%d,%d] to %s at [%d,%d] (vertical)",
                                         mode == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col,
                                         mode_d == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode_d == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row + 1, col);
                        }
                    }
                }
            }
        }
    }
    
    if (big_links > 0)
    {
        log_trace("Big partition bridges: added %d connections between adjacent labyrinths/caves/chasms (found %d adjacencies)", 
                  big_links, big_adjacencies_found);
        genlog_connect("Big partition bridges: connected %d pairs of adjacent big partitions", big_links);
    }

    /* Now run regular partition backbone connections */
    int links = 0;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int idx = row * cols + col;
            int hub_here = (idx < 25) ? parts[idx].hub_room : -1;
            if (hub_here < 0)
                continue;

            if (col + 1 < cols)
            {
                int idx_r = row * cols + (col + 1);
                int hub_right = parts[idx_r].hub_room;
                bool ok = false;
                if (hub_right >= 0)
                {
                    ok = connect_rooms_with_logging(hub_here, hub_right, "Partition backbone H", true);
                    if (!ok)
                        ok = connect_adjacent_big_partitions_by_boundary(
                            idx, idx_r, &parts[idx].bounds, &parts[idx_r].bounds,
                            rows, cols, hub_here, hub_right, true);
                }
                if (ok)
                {
                    mark_partition_edge(idx, idx_r, adj, degree);
                    links++;
                }
            }

            if (row + 1 < rows)
            {
                int idx_d = (row + 1) * cols + col;
                int hub_down = parts[idx_d].hub_room;
                bool ok = false;
                if (hub_down >= 0)
                {
                    ok = connect_rooms_with_logging(hub_here, hub_down, "Partition backbone V", true);
                    if (!ok)
                        ok = connect_adjacent_big_partitions_by_boundary(
                            idx, idx_d, &parts[idx].bounds, &parts[idx_d].bounds,
                            rows, cols, hub_here, hub_down, false);
                }
                if (ok)
                {
                    mark_partition_edge(idx, idx_d, adj, degree);
                    links++;
                }
            }

            if (col + 1 < cols && row + 1 < rows)
            {
                int idx_dr = (row + 1) * cols + (col + 1);
                int hub_diag = parts[idx_dr].hub_room;
                if (hub_diag >= 0 && connect_rooms_with_logging(hub_here, hub_diag, "Partition backbone D", true))
                {
                    mark_partition_edge(idx, idx_dr, adj, degree);
                    links++;
                }
            }
        }
    }

    int target_degree = (count >= 3) ? 2 : 1;
    for (int pi = 0; pi < count && pi < 25; ++pi)
    {
        if (parts[pi].hub_room < 0)
            continue;
        if (degree[pi] >= target_degree)
            continue;

        int attempts = 0;
        bool failed_candidate[25] = {false};
        while (degree[pi] < target_degree && attempts < count)
        {
            attempts++;
            int best = -1;
            int best_dist = 999999;
            for (int pj = 0; pj < count && pj < 25; ++pj)
            {
                if (pj == pi) continue;
                if (parts[pj].hub_room < 0) continue;
                if (adj[pi][pj]) continue;
                if (failed_candidate[pj]) continue;
                int dist = distance(parts[pi].center.y, parts[pi].center.x, parts[pj].center.y, parts[pj].center.x);
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best = pj;
                }
            }

            if (best < 0)
                break;

            if (connect_rooms_with_logging(parts[pi].hub_room, parts[best].hub_room, "Partition backbone fill", true))
            {
                mark_partition_edge(pi, best, adj, degree);
                links++;
            }
            else
            {
                failed_candidate[best] = true;
            }
        }
    }

    if (links > 0)
        log_trace("Partition hub pass: added %d backbone links (grid %dx%d)", links, rows, cols);

    connect_anchor_backbone(room_to_part, count);
}

/* Anchor-aware connector: link nearby anchors to reduce isolation without over-saturating tunnels. */
/* Repair all outer walls after generation - critical for tunnel connectivity.
 * This fixes cases where overlapping room/cave generation overwrote WALL_OUTER
 * tiles back to WALL_EXTRA, breaking tunnel connection logic. */
void repair_all_outer_walls(void)
{
    int repaired = 0;
    
    /* Scan entire map for wall tiles that border CAVE_ROOM floor */
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            /* Skip if already floor or already outer wall */
            if (cave_floor_bold(y, x))
                continue;
            if (cave_feat[y][x] == FEAT_WALL_OUTER)
                continue;
            if (cave_feat[y][x] != FEAT_WALL_EXTRA)
                continue;
            
            /* Check if this wall borders any CAVE_ROOM floor */
            bool borders_room_floor = false;
            for (int dy = -1; dy <= 1 && !borders_room_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_room_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = y + dy, nx = x + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_room_floor = true;
                    }
                }
            }
            
            if (borders_room_floor)
            {
                cave_set_feat(y, x, FEAT_WALL_OUTER);
                repaired++;
            }
        }
    }
    
    if (repaired > 0)
    {
        log_trace("repair_all_outer_walls: converted %d WALL_EXTRA to WALL_OUTER", repaired);
    }
}

/* Fallback builder to guarantee the minimum room count before connectivity work */
void ensure_minimum_rooms(void)
{
    if (dun->cent_n >= room_capacity_limit())
        return;
    if (dun->cent_n >= ROOM_MIN)
        return;

    int before = dun->cent_n;
    /* Try a mix of simple rooms near the centre to avoid hard failures */
    for (int attempt = 0; attempt < 50 && dun->cent_n < ROOM_MIN &&
        dun->cent_n < room_capacity_limit(); ++attempt)
    {
        int y = rand_range(4, p_ptr->cur_map_hgt - 4);
        int x = rand_range(4, p_ptr->cur_map_wid - 4);

        if (attempt % 3 == 0)
            build_type1(y, x);
        else if (attempt % 3 == 1)
            build_type2(y, x);
        else
            build_type6(y, x, false);
    }

    if (dun->cent_n > before)
    {
        log_trace("Room fallback: added %d emergency rooms (now %d)",
            dun->cent_n - before, dun->cent_n);
    }
}

/* File: level-generation-layout.c */

#include "angband.h"
#include "level-generation/gen-log.h"
#include "log/log.h"
#include "level-generation/level-generation-internal.h"

static bool is_big_partition_mode(quadrant_mode_t mode);

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

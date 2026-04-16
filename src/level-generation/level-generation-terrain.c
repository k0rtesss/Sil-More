/* File: level-generation-terrain.c */

/*
 * Shared terrain and map primitive helpers for level generation.
 */

#include "angband.h"
#include "log/log.h"
#include "level-generation/level-generation-internal.h"

void set_perm_boundry(void)
{
    int y, x;

    for (x = 0; x < p_ptr->cur_map_wid; x++)
    {
        y = 0;
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }

    for (x = 0; x < p_ptr->cur_map_wid; x++)
    {
        y = p_ptr->cur_map_hgt - 1;
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        x = 0;
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        x = p_ptr->cur_map_wid - 1;
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }
}

void basic_granite(void)
{
    int y, x;
    int depth_color = get_depth_color(p_ptr->depth);

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            cave_set_feat_with_color(y, x, FEAT_WALL_EXTRA, depth_color);
            cave_corridor1[y][x] = -1;
            cave_corridor2[y][x] = -1;
            cave_escape_tunnel[y][x] = false;
        }
    }
}

void make_patch_of_sunlight(int y, int x)
{
    int m, n, floor;

    if (cave_info[y][x] & CAVE_GLOW)
    {
        floor = 0;
        for (n = (y - 1); n <= (y + 1); n++)
        {
            for (m = (x - 1); m <= (x + 1); m++)
            {
                if (cave_feat[n][m] == FEAT_FLOOR)
                    floor++;
            }
        }
        if (floor > 6)
        {
            if (cave_feat[y][x] == FEAT_FLOOR
                && !((y == p_ptr->py) && (x == p_ptr->px)))
                cave_set_feat(y, x, FEAT_RUBBLE);
            for (n = (y - 1); n <= (y + 1); n++)
            {
                for (m = (x - 1); m <= (x + 1); m++)
                {
                    if ((n == p_ptr->py) && (m == p_ptr->px))
                        continue;
                    if ((cave_info[n][m] & CAVE_GLOW)
                        && cave_feat[n][m] == FEAT_FLOOR && one_in_(4))
                    {
                        if (cave_feat[n][m] == FEAT_FLOOR)
                            cave_set_feat(n, m, FEAT_SUNLIGHT);
                    }
                }
            }
        }
    }
}

void make_patches_of_sunlight(void)
{
    int i, x, y;

    for (i = 0; i < 40; ++i)
    {
        y = rand_range(MAX(p_ptr->py - 5, 1),
            MIN(p_ptr->py + 5, p_ptr->cur_map_hgt - 2));
        x = rand_range(MAX(p_ptr->px - 5, 1),
            MIN(p_ptr->px + 5, p_ptr->cur_map_wid - 2));
        make_patch_of_sunlight(y, x);
    }

    for (i = 0; i < 20; ++i)
    {
        y = rand_range(10, p_ptr->cur_map_hgt - 10);
        x = rand_range(10, p_ptr->cur_map_wid - 10);
        make_patch_of_sunlight(y, x);
    }
}

bool varda_sunlight_tile_ok(int y, int x, bool require_empty)
{
    if (!in_bounds_fully(y, x)) return false;
    if (cave_feat[y][x] != FEAT_SUNLIGHT) return false;
    if (!cave_floor_bold(y, x)) return false;
    if (cave_info[y][x] & CAVE_ICKY) return false;
    if (require_empty && cave_m_idx[y][x] != 0) return false;

    return true;
}

static void varda_make_sunlight_pool(int y, int x)
{
    for (int ny = y - 1; ny <= y + 1; ny++)
    {
        for (int nx = x - 1; nx <= x + 1; nx++)
        {
            if (!in_bounds_fully(ny, nx)) continue;
            if (cave_info[ny][nx] & CAVE_ICKY) continue;
            if (cave_feat[ny][nx] != FEAT_FLOOR && cave_feat[ny][nx] != FEAT_RAGE_FLOOR
                && cave_feat[ny][nx] != FEAT_SUNLIGHT) continue;
            cave_set_feat(ny, nx, FEAT_SUNLIGHT);
        }
    }
}

bool varda_no_rubble_path_tile_ok(int y, int x,
    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    if (!access[y][x]) return false;
    if (distance(p_ptr->py, p_ptr->px, y, x) < 2) return false;

    return true;
}

int pick_varda_sunlight_spawn_tile(int* out_y, int* out_x,
    int* out_total_sunlight, int* out_empty_sunlight)
{
    int total = 0;
    int empty = 0;
    int spawnable = 0;
    int pick_y = -1;
    int pick_x = -1;

    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            access[y][x] = false;
        }
    }
    flood_access(p_ptr->py, p_ptr->px, access, false);

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (!varda_sunlight_tile_ok(y, x, false)) continue;
            total++;

            if (!varda_sunlight_tile_ok(y, x, true)) continue;
            empty++;

            if (!varda_no_rubble_path_tile_ok(y, x, access)) continue;
            spawnable++;
            if (one_in_(spawnable)) {
                pick_y = y;
                pick_x = x;
            }
        }
    }

    if (out_total_sunlight) *out_total_sunlight = total;
    if (out_empty_sunlight) *out_empty_sunlight = empty;
    if (spawnable > 0 && out_y && out_x) {
        *out_y = pick_y;
        *out_x = pick_x;
    }

    return spawnable;
}

bool force_varda_sunlight_tile(int* out_y, int* out_x)
{
    int count = 0;
    int pick_y = -1;
    int pick_x = -1;

    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            access[y][x] = false;
        }
    }
    flood_access(p_ptr->py, p_ptr->px, access, false);

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (cave_info[y][x] & CAVE_ICKY) continue;
            if (!varda_no_rubble_path_tile_ok(y, x, access)) continue;
            if (!cave_empty_bold(y, x)) continue;
            if (cave_feat[y][x] != FEAT_FLOOR && cave_feat[y][x] != FEAT_RAGE_FLOOR) continue;

            count++;
            if (one_in_(count)) {
                pick_y = y;
                pick_x = x;
            }
        }
    }

    if (count == 0) return false;

    varda_make_sunlight_pool(pick_y, pick_x);
    if (out_y) {
        *out_y = pick_y;
        *out_x = pick_x;
    }

    return true;
}

void ensure_sunlight_for_varda(void)
{
    if (p_ptr->depth > 3) return;

    int total_sunlight = 0;
    int empty_sunlight = 0;
    int spawnable_sunlight = pick_varda_sunlight_spawn_tile(
        NULL, NULL, &total_sunlight, &empty_sunlight);

    if (spawnable_sunlight == 0) {
        log_trace("Varda spawn: No valid sunlight spawn locations detected (total=%d, empty=%d), seeding patches",
            total_sunlight, empty_sunlight);
        make_patches_of_sunlight();

        total_sunlight = 0;
        empty_sunlight = 0;
        spawnable_sunlight = pick_varda_sunlight_spawn_tile(
            NULL, NULL, &total_sunlight, &empty_sunlight);

        if (spawnable_sunlight > 0) {
            log_trace("Varda spawn: Verified sunlight after patching (total=%d, empty=%d, spawnable=%d)",
                total_sunlight, empty_sunlight, spawnable_sunlight);
            return;
        }

        int forced_y = -1;
        int forced_x = -1;
        if (force_varda_sunlight_tile(&forced_y, &forced_x)) {
            log_trace("Varda spawn: Forced sunlight at (%d,%d) to guarantee spawn", forced_y, forced_x);
            return;
        }

        log_trace("Varda spawn: WARNING - No valid sunlight locations after patching or forcing!");
    }
}

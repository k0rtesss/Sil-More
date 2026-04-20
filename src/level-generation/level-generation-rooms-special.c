/* File: level-generation-rooms-special.c */
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
#include "level-generation/level-generation-internal.h"
#include "log/log.h"
#include "level-generation/gen-log.h"
#include "metarun.h"
#include "thrall_quest.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

static bool chasm_mask_has_square_space(
    const bool* mask, int h, int w, int cy, int cx, int radius)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            int ny = cy + dy;
            int nx = cx + dx;

            if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                return false;
            if (!mask[ny * w + nx])
                return false;
        }
    }

    return true;
}

bool choose_chasm_sanctum_seed(
    const bool* is_cave, int h, int w, int* out_y, int* out_x)
{
    int center_y = h / 2;
    int center_x = w / 2;
    int best_score = 0;
    bool found = false;

    for (int y = 2; y < h - 2; ++y)
    {
        for (int x = 2; x < w - 2; ++x)
        {
            int score;

            if (!is_cave[y * w + x])
                continue;
            if (!chasm_mask_has_square_space(is_cave, h, w, y, x, 2))
                continue;

            score = distance(y, x, center_y, center_x);
            if (!found || score < best_score)
            {
                best_score = score;
                *out_y = y;
                *out_x = x;
                found = true;
            }
        }
    }

    return found;
}

static bool place_exact_skeleton_at(int y, int x, byte sval)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    s16b k_idx;

    if (!in_bounds_fully(y, x))
        return false;
    if (cave_feat[y][x] != FEAT_FLOOR)
        return false;
    if (cave_o_idx[y][x] != 0)
        return false;

    k_idx = lookup_kind(TV_SKELETON, sval);
    if (!k_idx)
        return false;

    object_wipe(i_ptr);
    object_prep(i_ptr, k_idx);
    i_ptr->pval = 1;

    return (floor_carry(y, x, i_ptr) != 0);
}

static bool place_chasm_sanctum_drop_at(int y, int x)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;

    if (!in_bounds_fully(y, x))
        return false;
    if (cave_feat[y][x] != FEAT_FLOOR)
        return false;
    if (cave_o_idx[y][x] != 0)
        return false;

    object_wipe(i_ptr);
    if (!drop_generate_chasm_sanctum_object(p_ptr->depth, i_ptr))
        return false;

    i_ptr->ident |= IDENT_CHASM_SANCTUM_ITEM | IDENT_CHASM_SANCTUM_DROP;

    return (floor_carry(y, x, i_ptr) != 0);
}

void place_chasm_island_sanctum(int cy, int cx)
{
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            int ny = cy + dy;
            int nx = cx + dx;

            if (dy == 0 && dx == 0)
                continue;
            if (!in_bounds_fully(ny, nx))
                continue;
            if (cave_feat[ny][nx] != FEAT_FLOOR)
                continue;

            cave_set_feat(ny, nx, FEAT_GLYPH);
            cave_info[ny][nx] |= (CAVE_ROOM | CAVE_CHASM_AREA);
        }
    }

    if (!place_chasm_sanctum_drop_at(cy, cx))
    {
        log_warn("Chasm sanctum: failed to place EVIL drop at (%d,%d), falling back to elf skeleton",
            cy, cx);
        (void)place_exact_skeleton_at(cy, cx, SV_SKELETON_ELF);
    }
}

/* Carve two 3-wide tunnels from the north face of the throne room up toward the partition edge */
void carve_morgoth_entry_tunnels(const vault_type* v_ptr, int y0, int x0)
{
    if (!v_ptr)
        return;

    int top_y = y0 - v_ptr->hgt / 2;
    int left_x = x0 - v_ptr->wid / 2;
    int right_x = left_x + v_ptr->wid - 1;

    /* Collect contiguous '$' runs on the top row (stored as FEAT_WALL_OUTER) */
    int seg_start[4];
    int seg_end[4];
    int segs = 0;

    for (int x = left_x; x <= right_x; x++)
    {
        if (cave_feat[top_y][x] == FEAT_WALL_OUTER)
        {
            if (segs == 0 || x != seg_end[segs - 1] + 1)
            {
                if (segs >= 4)
                    break;
                seg_start[segs] = seg_end[segs] = x;
                segs++;
            }
            else
            {
                seg_end[segs - 1] = x;
            }
        }
    }

    if (segs == 0)
        return;

    int tunnel_limit = morgoth_partition_reserved ? morgoth_partition_bounds.y1 - 2 : top_y - 20;
    if (tunnel_limit < 1)
        tunnel_limit = 1;
    if (tunnel_limit > top_y)
        tunnel_limit = top_y;

    /* Track which segments have joined independently */
    bool seg_joined[4] = {false, false, false, false};

    for (int s = 0; s < segs; s++)
    {
        int x1 = seg_start[s];
        int x2 = seg_end[s];

        /* Place forced closed doors in the vault's outer wall (end of corridor) */
        for (int x = x1; x <= x2; x++)
        {
            if (!in_bounds_fully(top_y, x))
                continue;
            cave_set_feat(top_y, x, FEAT_DOOR_HEAD + 0x00);
        }

        /* Carve a tunnel northwards from just outside the doors */
        for (int y = top_y - 1; y >= tunnel_limit; y--)
        {
            /* Skip if this segment already joined */
            if (seg_joined[s])
                break;

            bool this_seg_joined = false;

            for (int x = x1; x <= x2; x++)
            {
                if (!in_bounds_fully(y, x))
                    continue;

                /* Stop this segment once it reaches existing open floor outside the reserved region */
                if (!morgoth_region_active() || !coord_in_morgoth_region(y, x, 0))
                {
                    if (cave_floor_bold(y, x) && !(cave_info[y][x] & CAVE_ICKY))
                    {
                        this_seg_joined = true;
                        continue;
                    }
                }

                if (cave_feat[y][x] == FEAT_WALL_PERM)
                    continue;

                cave_set_feat(y, x, FEAT_FLOOR);
                cave_info[y][x] &= ~(CAVE_G_VAULT | CAVE_ICKY);
                cave_info[y][x] |= CAVE_MORGOTH_TUNNEL;
            }

            if (this_seg_joined)
            {
                seg_joined[s] = true;
                break;
            }
        }
    }
}

/* Extend the carved entry tunnels so both connect to the main level. */
static bool morgoth_tunnel_traversable(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (cave_feat[y][x] == FEAT_WALL_PERM)
        return false;
    if ((cave_info[y][x] & CAVE_ICKY) && !coord_in_morgoth_region(y, x, 0))
        return false;
    return true;
}

static bool morgoth_tunnel_target(int y, int x)
{
    if (coord_in_morgoth_region(y, x, 0))
        return false;
    if (cave_info[y][x] & CAVE_ICKY)
        return false;
    return player_passable(y, x, true);
}

static bool connect_morgoth_tunnel_component(int start_y, int start_x)
{
    static int prev[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    int head = 0;
    int tail = 0;
    int found_y = -1;
    int found_x = -1;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
            prev[y][x] = -1;

    int start_idx = start_y * MAX_DUNGEON_WID + start_x;
    prev[start_y][start_x] = start_idx;
    queue[tail++] = start_idx;

    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};

    while (head < tail)
    {
        int cur = queue[head++];
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        for (int d = 0; d < 4; ++d)
        {
            int ny = cy + ddy4[d];
            int nx = cx + ddx4[d];
            if (!in_bounds_fully(ny, nx))
                continue;
            if (prev[ny][nx] != -1)
                continue;
            if (!morgoth_tunnel_traversable(ny, nx))
                continue;

            int nidx = ny * MAX_DUNGEON_WID + nx;
            prev[ny][nx] = cur;
            if (tail < (int)N_ELEMENTS(queue))
                queue[tail++] = nidx;

            if (morgoth_tunnel_target(ny, nx))
            {
                found_y = ny;
                found_x = nx;
                head = tail;
                break;
            }
        }
    }

    if (found_y < 0 || found_x < 0)
        return false;

    int cur = found_y * MAX_DUNGEON_WID + found_x;
    int safety = 0;
    while (safety++ < (int)N_ELEMENTS(queue))
    {
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        if (cave_feat[cy][cx] != FEAT_WALL_PERM)
        {
            if (!cave_floor_bold(cy, cx)
                && (cave_feat[cy][cx] < FEAT_DOOR_HEAD
                    || cave_feat[cy][cx] > FEAT_DOOR_TAIL))
            {
                cave_set_feat(cy, cx, FEAT_FLOOR);
            }

            if (coord_in_morgoth_region(cy, cx, 0))
                cave_info[cy][cx] |= CAVE_MORGOTH_TUNNEL;
        }

        if (cur == prev[start_y][start_x])
            break;
        int p = prev[cy][cx];
        if (p == cur)
            break;
        cur = p;
        if (cur == start_idx)
            break;
    }

    return true;
}

bool place_monster_by_flag_try(int y, int x, int flagset, u32b flag, bool allow_unique, int max_depth)
{
    if (cave_m_idx[y][x] != 0)
        return false;
    place_monster_by_flag(y, x, flagset, flag, allow_unique, max_depth);
    return (cave_m_idx[y][x] != 0);
}

bool place_monster_by_letter_try(int y, int x, char letter, bool allow_unique, int max_depth)
{
    int tries = 0;
    int depth = max_depth;

    while (depth > 0)
    {
        int r_idx = get_mon_num(depth, false, true, true);
        monster_race* r_ptr = &r_info[r_idx];

        if (r_ptr->d_char == letter
            && (allow_unique || !(r_ptr->flags1 & (RF1_UNIQUE))))
        {
            return place_monster_one(y, x, r_idx, true, false, NULL);
        }

        tries++;
        if (tries >= 100)
        {
            tries = 0;
            depth--;
        }
    }

    return false;
}

static const int chasm_sanctum_ambush_offsets[8][2] = {
    {-1, -1}, {-1, 0}, {-1, 1},
    {0, -1},           {0, 1},
    {1, -1},  {1, 0},  {1, 1},
};

static bool chasm_theme_monster_ok(
    int r_idx, int min_depth, int max_depth, bool allow_unique, bool unique_only)
{
    monster_race* r_ptr = &r_info[r_idx];

    if (!r_ptr->name || !r_ptr->rarity)
        return false;
    if (!allow_unique && (r_ptr->flags1 & RF1_UNIQUE))
        return false;
    if (unique_only && !(r_ptr->flags1 & RF1_UNIQUE))
        return false;
    if (r_ptr->flags3 & RF3_SPECIAL_VAULT_ONLY)
        return false;
    if (r_ptr->flags1 & RF1_SPECIAL_GEN)
        return false;
    if (r_ptr->light >= 0)
        return false;
    if (r_ptr->level < min_depth || r_ptr->level > max_depth)
        return false;
    if ((r_ptr->flags1 & RF1_FORCE_DEPTH) && (r_ptr->level > p_ptr->depth))
        return false;
    if (r_ptr->cur_num >= r_ptr->max_num)
        return false;

    return true;
}

static s16b choose_chasm_theme_monster(
    int min_depth, int max_depth, bool allow_unique, bool unique_only)
{
    alloc_entry* table = alloc_race_table;
    long total = 0L;

    if (min_depth < 1)
        min_depth = 1;
    if (max_depth > MORGOTH_DEPTH + 3)
        max_depth = MORGOTH_DEPTH + 3;
    if (min_depth > max_depth)
        return 0;

    for (int i = 0; i < alloc_race_size; ++i)
    {
        int r_idx = table[i].index;

        if (!chasm_theme_monster_ok(
                r_idx, min_depth, max_depth, allow_unique, unique_only))
        {
            continue;
        }

        total += table[i].prob1;
    }

    if (total <= 0)
        return 0;

    {
        long value = rand_int(total);

        for (int i = 0; i < alloc_race_size; ++i)
        {
            int r_idx = table[i].index;

            if (!chasm_theme_monster_ok(
                    r_idx, min_depth, max_depth, allow_unique, unique_only))
            {
                continue;
            }

            if (value < table[i].prob1)
                return r_idx;

            value -= table[i].prob1;
        }
    }

    return 0;
}

bool place_chasm_theme_monster_at(int y, int x, int r_idx)
{
    bool had_glyph = false;

    if (!in_bounds_fully(y, x))
        return false;

    if (cave_feat[y][x] == FEAT_GLYPH)
    {
        cave_set_feat(y, x, FEAT_FLOOR);
        had_glyph = true;
    }

    if (!cave_empty_bold(y, x))
    {
        if (had_glyph)
            cave_set_feat(y, x, FEAT_GLYPH);
        return false;
    }

    if (!place_monster_one(y, x, r_idx, true, false, NULL))
    {
        if (had_glyph)
            cave_set_feat(y, x, FEAT_GLYPH);
        return false;
    }

    {
        int m_idx = cave_m_idx[y][x];

        if (m_idx > 0)
        {
            monster_type* m_ptr = &mon_list[m_idx];

            m_ptr->alertness = MAX(m_ptr->alertness, ALERTNESS_ALERT);
            m_ptr->skip_next_turn = false;
            m_ptr->mflag |= MFLAG_ACTV;
            m_ptr->min_range = 0;
        }
    }

    if (had_glyph)
        cave_set_feat(y, x, FEAT_GLYPH);

    return true;
}

static bool chasm_sanctum_drop_present(int y, int x);

static bool chasm_sanctum_ambush_tile(int y, int x)
{
    for (int i = 0; i < 8; ++i)
    {
        int cy = y - chasm_sanctum_ambush_offsets[i][0];
        int cx = x - chasm_sanctum_ambush_offsets[i][1];

        if (!in_bounds_fully(cy, cx))
            continue;
        if (chasm_sanctum_drop_present(cy, cx))
            return true;
    }

    return false;
}

static bool chasm_sanctum_drop_present(int y, int x)
{
    for (object_type* o_ptr = get_first_object(y, x); o_ptr;
        o_ptr = get_next_object(o_ptr))
    {
        if (o_ptr->ident & IDENT_CHASM_SANCTUM_ITEM)
            return true;
    }

    return false;
}

static void clear_chasm_sanctum_drop_marker(int y, int x)
{
    for (object_type* o_ptr = get_first_object(y, x); o_ptr;
        o_ptr = get_next_object(o_ptr))
    {
        o_ptr->ident &= ~IDENT_CHASM_SANCTUM_ITEM;
    }
}

static bool relocate_chasm_sanctum_blocker(int y, int x)
{
    int m_idx = cave_m_idx[y][x];

    if (m_idx <= 0)
        return true;

    for (int tries = 0; tries < 200; ++tries)
    {
        int ny = rand_spread(y, 6);
        int nx = rand_spread(x, 6);
        monster_type* m_ptr = &mon_list[m_idx];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        if (!in_bounds_fully(ny, nx))
            continue;
        if (!cave_empty_bold(ny, nx))
            continue;
        if (cave_glyph(ny, nx))
            continue;
        if (chasm_sanctum_ambush_tile(ny, nx))
            continue;
        if (!cave_exist_mon(r_ptr, ny, nx, false, false))
            continue;

        monster_swap(y, x, ny, nx);
        return true;
    }

    teleport_away(m_idx, 10);
    return (cave_m_idx[y][x] == 0);
}

void trigger_chasm_sanctum_ambush_if_needed(int y, int x)
{
    int min_depth = 0;
    int max_depth = 0;
    s16b base_r_idx = 0;
    s16b unique_r_idx = 0;
    int unique_slot = -1;
    int placed = 0;

    if (!in_bounds_fully(y, x))
        return;
    if (!chasm_sanctum_drop_present(y, x))
        return;

    clear_chasm_sanctum_drop_marker(y, x);

    msg_print("The evil artefact calls to its own.");
    msg_print("A cry goes up from the deeps, and black shadows gather.");

    partition_theme_depth_band(p_ptr->depth, &min_depth, &max_depth);
    base_r_idx = choose_chasm_theme_monster(min_depth, max_depth, false, false);
    if (!base_r_idx)
    {
        log_warn("Chasm sanctum ambush: no themed monster available at depth=%d band=[%d,%d]",
            p_ptr->depth, min_depth, max_depth);
        return;
    }

    if (base_r_idx && one_in_(CHASM_AMBUSH_UNIQUE_SUB_PERCENT))
    {
        unique_r_idx = choose_chasm_theme_monster(min_depth, max_depth, true, true);
        if (unique_r_idx)
            unique_slot = rand_int(8);
    }

    for (int i = 0; i < 8; ++i)
    {
        int ny = y + chasm_sanctum_ambush_offsets[i][0];
        int nx = x + chasm_sanctum_ambush_offsets[i][1];
        bool slot_placed = false;

        if (!in_bounds_fully(ny, nx))
            continue;
        if (!relocate_chasm_sanctum_blocker(ny, nx))
            continue;

        if (base_r_idx)
        {
            s16b r_idx = (unique_r_idx && (i == unique_slot))
                ? unique_r_idx
                : base_r_idx;

            slot_placed = place_chasm_theme_monster_at(ny, nx, r_idx);
            if (!slot_placed && unique_r_idx && (i == unique_slot))
                slot_placed = place_chasm_theme_monster_at(ny, nx, base_r_idx);
        }
        if (slot_placed)
            placed++;
    }

    (void)explosion(-1, 1, y, x, 0, 0, 0, GF_DARK_WEAK);
    (void)set_darkened(MAX(p_ptr->darkened, 8));
    monster_perception(true, false, -15);
    break_truce(true);

    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS | PU_DISTANCE);
    p_ptr->redraw |= (PR_MAP);
    handle_stuff();

    log_trace("Chasm sanctum ambush: triggered at (%d,%d), placed=%d",
        y, x, placed);
}

bool place_big_cave_elemental_monster(int y, int x, big_cave_type_t cave_type, int max_depth)
{
    if (cave_type == BIG_CAVE_FIRE)
    {
        if (place_monster_by_flag_try(y, x, 4, RF4_BRTH_FIRE, true, max_depth))
            return true;
        return place_monster_by_flag_try(y, x, 3, RF3_RES_FIRE, true, max_depth);
    }
    if (cave_type == BIG_CAVE_ICE)
    {
        if (place_monster_by_flag_try(y, x, 4, RF4_BRTH_COLD, true, max_depth))
            return true;
        return place_monster_by_flag_try(y, x, 3, RF3_RES_COLD, true, max_depth);
    }
    if (cave_type == BIG_CAVE_POIS)
    {
        if (place_monster_by_flag_try(y, x, 4, RF4_BRTH_POIS, true, max_depth))
            return true;
        return place_monster_by_flag_try(y, x, 3, RF3_RES_POIS, true, max_depth);
    }
    return false;
}

bool place_big_cave_troll_or_giant(int y, int x, int max_depth)
{
    if (one_in_(2))
    {
        if (place_monster_by_flag_try(y, x, 3, RF3_TROLL, true, max_depth))
            return true;
        return place_monster_by_letter_try(y, x, 'G', true, max_depth);
    }
    if (place_monster_by_letter_try(y, x, 'G', true, max_depth))
        return true;
    return place_monster_by_flag_try(y, x, 3, RF3_TROLL, true, max_depth);
}



bool connect_morgoth_entry_tunnels(void)
{
    if (!morgoth_region_active())
        return true;

    static bool visited[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    int components_found = 0;
    int components_connected = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
            visited[y][x] = false;

    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            if (!(cave_info[y][x] & CAVE_MORGOTH_TUNNEL))
                continue;
            if (visited[y][x])
                continue;

            components_found++;
            int head = 0;
            int tail = 0;
            int min_y = y;
            int min_x = x;
            int max_x = x;
            bool start_found = false;

            int start_idx = y * MAX_DUNGEON_WID + x;
            queue[tail++] = start_idx;
            visited[y][x] = true;

            while (head < tail)
            {
                int cur = queue[head++];
                int cy = cur / MAX_DUNGEON_WID;
                int cx = cur % MAX_DUNGEON_WID;

                if (cy < min_y)
                {
                    min_y = cy;
                    min_x = cx;
                    max_x = cx;
                    start_found = true;
                }
                else if (cy == min_y)
                {
                    if (!start_found)
                    {
                        min_x = cx;
                        max_x = cx;
                        start_found = true;
                    }
                    else
                    {
                        min_x = MIN(min_x, cx);
                        max_x = MAX(max_x, cx);
                    }
                }

                for (int d = 0; d < 4; ++d)
                {
                    int ny = cy + ddy4[d];
                    int nx = cx + ddx4[d];
                    if (!in_bounds_fully(ny, nx))
                        continue;
                    if (visited[ny][nx])
                        continue;
                    if (!(cave_info[ny][nx] & CAVE_MORGOTH_TUNNEL))
                        continue;
                    visited[ny][nx] = true;
                    if (tail < (int)N_ELEMENTS(queue))
                        queue[tail++] = ny * MAX_DUNGEON_WID + nx;
                }
            }

            int start_y = min_y;
            int start_x = (min_x + max_x) / 2;
            if (!(cave_info[start_y][start_x] & CAVE_MORGOTH_TUNNEL))
            {
                bool found = false;
                for (int tx = min_x; tx <= max_x; ++tx)
                {
                    if (cave_info[start_y][tx] & CAVE_MORGOTH_TUNNEL)
                    {
                        start_x = tx;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    start_x = x;
            }

            if (connect_morgoth_tunnel_component(start_y, start_x))
            {
                components_connected++;
            }
            else
            {
                log_trace("connect_morgoth_entry_tunnels: component at (%d,%d) could not reach outside-region floor",
                    start_y, start_x);
            }
        }
    }

    if (components_found == 0)
    {
        log_trace("connect_morgoth_entry_tunnels: no tunnel components found");
        genlog_fail("CONNECTIVITY FAILED: Morgoth entry tunnels missing");
        return false;
    }

    if (components_connected != components_found)
    {
        log_trace("connect_morgoth_entry_tunnels: connected %d/%d tunnel components",
            components_connected, components_found);
        genlog_fail("CONNECTIVITY FAILED: Morgoth entry tunnels connected %d/%d components",
            components_connected, components_found);
        return false;
    }

    log_trace("connect_morgoth_entry_tunnels: connected %d/%d tunnel components",
        components_connected, components_found);
    genlog_connect("Morgoth entry tunnels: connected %d/%d components",
        components_connected, components_found);
    return true;
}

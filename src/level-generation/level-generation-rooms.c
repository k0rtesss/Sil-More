/* File: level-generation-rooms.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "level-generation/level-generation-internal.h"
#include "log/log.h"
#include "gen-log.h"
#include "metarun.h"
/* Ensure C library prototypes are visible for tools */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

/* Quest vault debug instrumentation */
#define DEBUG_QUEST_VAULT 0
#if DEBUG_QUEST_VAULT
static int qv_y1 = -1, qv_x1 = -1, qv_y2 = -1, qv_x2 = -1;
static int qv_h = 0, qv_w = 0;
static unsigned short *qv_feat_snapshot = NULL;

static void qv_capture(void) {
    int y,x;
    if (qv_y1 < 0) return;
    qv_h = qv_y2 - qv_y1 + 1;
    qv_w = qv_x2 - qv_x1 + 1;
    mem_free_null(qv_feat_snapshot);
    qv_feat_snapshot = mem_alloc_array(qv_h * qv_w, unsigned short);
    for (y = qv_y1; y <= qv_y2; ++y)
        for (x = qv_x1; x <= qv_x2; ++x)
            qv_feat_snapshot[(y - qv_y1) * qv_w + (x - qv_x1)] = cave_feat[y][x];
    log_trace("Quest vault DEBUG: snapshot captured (%d x %d) bounds (%d,%d)-(%d,%d)", qv_h, qv_w, qv_y1, qv_x1, qv_y2, qv_x2);
}

static char qv_glyph(int f) {
    switch (f) {
        case FEAT_FLOOR: return '.'; case FEAT_WALL_OUTER: return '#';
        case FEAT_WALL_INNER: return '+'; case FEAT_WALL_EXTRA: return 'X';
#ifdef FEAT_DOOR_CLOSED
        case FEAT_DOOR_CLOSED: return 'D';
#endif
        case FEAT_FORGE_HEAD: case FEAT_FORGE_TAIL: return 'F';
        default: return '?';
    }
}

static void qv_dump(const char *phase) {
    if (qv_y1 < 0) return;
    int y,x; char row[256];
    log_trace("Quest vault DEBUG: layout (%s) bounds (%d,%d)-(%d,%d)", phase, qv_y1, qv_x1, qv_y2, qv_x2);
    for (y = qv_y1; y <= qv_y2; ++y) {
        int idx=0;
        for (x = qv_x1; x <= qv_x2 && idx < (int)sizeof(row)-2; ++x)
            row[idx++] = qv_glyph(cave_feat[y][x]);
        row[idx]='\0';
        log_trace("Quest vault DEBUG ROW %2d: %s", y, row);
    }
}

static void qv_compare(void) {
    if (!qv_feat_snapshot) return;
    int diffs=0, y,x;
    for (y = qv_y1; y <= qv_y2; ++y) for (x = qv_x1; x <= qv_x2; ++x) {
        unsigned short before = qv_feat_snapshot[(y - qv_y1) * qv_w + (x - qv_x1)];
        unsigned short now = cave_feat[y][x];
        if (before != now) {
            log_trace("Quest vault DEBUG: tile changed (%d,%d) %d->%d", y, x, before, now);
            if (++diffs >= 50) goto done_diffs;
        }
    }
done_diffs:
    if (!diffs) log_trace("Quest vault DEBUG: no tile changes detected since snapshot");
}
#endif

typedef struct vault_monster_spec {
    char symbol;
    const char* guid_text;
    u64b guid;
    bool guid_cached;
    bool start_sleeping;
    bool ignore_depth;
} vault_monster_spec;

static vault_monster_spec vault_monster_table[] = {
    {'C', "9cbdbb88fd4f59dc", 0, false, true, true},
    {'H', "c790972955718680", 0, false, true, false},
    {'@', "4acd2c9fcc5cd6e5", 0, false, true, false},
    {'o', "88ef7547642967b2", 0, false, true, false},
    {'O', "2c739cdb1be99f2c", 0, false, true, false},
    {'Z', "05f49e29acf49a93", 0, false, true, true},
    {'f', "3c10b33361f6f136", 0, false, true, false},
    {'F', "9a6fbc6e7b46f502", 0, false, true, false},
    {'T', "b39a82dfdc1c5ef9", 0, false, true, false},
    {'W', "c92f7e02e189e1bd", 0, false, true, true},
    {'y', "2f6ec4ab45007365", 0, false, true, false},
    {'Y', "0af151dfe09fe455", 0, false, true, false},
    {'A', "ed37fc4fce32643f", 0, false, true, true},
    {'L', "d27e36edf5c2f432", 0, false, true, true},
    {'N', "f134bcd795c27d4f", 0, false, true, true},
    {'D', "3ab7e216cb871fec", 0, false, true, true},
    {'K', "4da7998251196a35", 0, false, true, true}, /* Ancalagon the Black */
    {'I', "7a94fd98505d6076", 0, false, true, true}, /* Flying cold-drake */
    {'J', "49c954b30d9f0406", 0, false, true, true}, /* Flying fire-drake */
    {'R', "0e0f11695f8a443d", 0, false, true, true},
    {'U', "c2485b83ba33934d", 0, false, true, true},
    {'G', "7b038638b2981d20", 0, false, true, true},
    {'V', "58d8cf770bfcbe6f", 0, false, true, true},
    {'B', "9c44dec3f9d6d14c", 0, false, false, true}, /* Duruin, Least of the Balrogs */
    {'q', "ccff426ff2ef0318", 0, false, true, true},  /* Whispering shadow */
    {'j', "d5e4892102e9b48a", 0, false, true, true},  /* Shadow spider */
    {'k', "d2d2f0b7edcf4cf6", 0, false, true, true},  /* Lurking horror */
    {'n', "7783062d13500802", 0, false, true, true},  /* Nightthorn */
};

static int current_build_vault_type = 0;
static bool current_build_vault_exact_token = false;

bool monster_special_vault_selection_allowed(void)
{
    if (current_build_vault_exact_token)
        return true;

    return current_build_vault_type == 9;
}

bool monster_special_vault_only_allowed_at(int y, int x)
{
    if (current_build_vault_exact_token)
        return true;

    if (!in_bounds(y, x))
        return false;

    if (current_build_vault_type == 9)
        return true;

    return coord_in_morgoth_region(y, x, 0)
        && ((cave_info[y][x] & CAVE_G_VAULT) != 0);
}

void monster_special_vault_debug_context(
    int* build_vault_type, bool* exact_token)
{
    if (build_vault_type)
        *build_vault_type = current_build_vault_type;
    if (exact_token)
        *exact_token = current_build_vault_exact_token;
}

bool place_vault_monster_token(char symbol, int y, int x)
{
    for (size_t i = 0; i < N_ELEMENTS(vault_monster_table); i++)
    {
        vault_monster_spec* spec = &vault_monster_table[i];
        if (spec->symbol != symbol)
            continue;

        if (!spec->guid_cached)
        {
            spec->guid_cached = true;
            if (!parse_u64b_hex(spec->guid_text, &spec->guid))
            {
                spec->guid = 0;
                log_error("Vault: invalid GUID '%s' for token '%c'",
                    spec->guid_text, symbol);
            }
        }

        if (!spec->guid)
        {
            log_warn("Vault: GUID missing for monster token '%c'", symbol);
            return false;
        }

        bool old_exact_token = current_build_vault_exact_token;
        bool placed;

        log_trace(
            "SPECIAL_VAULT_ONLY exact-token attempt: token='%c' guid=%s depth=%d at=(%d,%d) build_vault_type=%d",
            symbol, spec->guid_text, p_ptr->depth, y, x,
            current_build_vault_type);

        current_build_vault_exact_token = true;
        placed = place_monster_by_guid(
            y, x, spec->guid, spec->start_sleeping, spec->ignore_depth, NULL);
        current_build_vault_exact_token = old_exact_token;

        if (!placed)
        {
            log_warn("Vault: failed to place monster for token '%c'", symbol);
            return false;
        }

        {
            s16b r_idx = monster_lookup_guid(spec->guid);
            const char* monster_name =
                (r_idx > 0) ? (r_name + r_info[r_idx].name) : "<unknown>";
            log_trace(
                "SPECIAL_VAULT_ONLY exact-token placed: token='%c' monster='%s' r_idx=%d depth=%d at=(%d,%d) build_vault_type=%d",
                symbol, monster_name, r_idx, p_ptr->depth, y, x,
                current_build_vault_type);
        }

        return true;
    }

    return false;
}

static bool is_vault_monster_token(char symbol)
{
    for (size_t i = 0; i < N_ELEMENTS(vault_monster_table); i++)
    {
        if (vault_monster_table[i].symbol == symbol)
            return true;
    }

    return false;
}

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



/*
 * Generate helper -- test a rectangle to see if it is all rock (i.e. not floor
 * and not icky)
 */
static bool solid_rock(int y1, int x1, int y2, int x2)
{
    int y, x;

    if (x2 >= MAX_DUNGEON_WID || y2 >= MAX_DUNGEON_HGT)
        return (false);

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
                return (false);
            if (cave_info[y][x] & CAVE_ICKY)
                return (false);
        }
    }
    return (true);
}

/*
 * Sil
 * Generate helper -- test around a rectangle to see if there would be a doubled
 * wall
 *
 * eg:
 *       ######
 * #######....#
 * #....##....#
 * #....#######
 * ######
 */
static bool doubled_wall(int y1, int x1, int y2, int x2)
{
    int y, x;

    /* check top wall */
    for (x = x1; x < x2; x++)
    {
        if ((cave_feat[y1 - 2][x] == FEAT_WALL_OUTER)
            && (cave_feat[y1 - 2][x + 1] == FEAT_WALL_OUTER))
            return (true);
    }

    /* check bottom wall */
    for (x = x1; x < x2; x++)
    {
        if ((cave_feat[y2 + 2][x] == FEAT_WALL_OUTER)
            && (cave_feat[y2 + 2][x + 1] == FEAT_WALL_OUTER))
            return (true);
    }

    /* check left wall */
    for (y = y1; y < y2; y++)
    {
        if ((cave_feat[y][x1 - 2] == FEAT_WALL_OUTER)
            && (cave_feat[y + 1][x1 - 2] == FEAT_WALL_OUTER))
            return (true);
    }

    /* check right wall */
    for (y = y1; y < y2; y++)
    {
        if ((cave_feat[y][x2 + 2] == FEAT_WALL_OUTER)
            && (cave_feat[y + 1][x2 + 2] == FEAT_WALL_OUTER))
            return (true);
    }

    return (false);
}

/*
 * Generate helper -- create a new room with optional light
 */
static void generate_room(int y1, int x1, int y2, int x2, int light)
{
    int y, x;

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            cave_info[y][x] |= (CAVE_ROOM);
            if (light)
                cave_info[y][x] |= (CAVE_GLOW);
        }
    }
}

/*
 * Generate helper -- fill a rectangle with a feature
 */
static void generate_fill(int y1, int x1, int y2, int x2, int feat)
{
    int y, x;

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            cave_set_feat(y, x, feat);
        }
    }
}

/*
 * Generate helper -- draw a rectangle with a feature
 */
static void generate_draw(int y1, int x1, int y2, int x2, int feat)
{
    int y, x;

    for (y = y1; y <= y2; y++)
    {
        cave_set_feat(y, x1, feat);
        cave_set_feat(y, x2, feat);
    }

    for (x = x1; x <= x2; x++)
    {
        cave_set_feat(y1, x, feat);
        cave_set_feat(y2, x, feat);
    }
}

/*
 * Generate helper -- split a rectangle with a feature
 */
static void generate_plus(int y1, int x1, int y2, int x2, int feat)
{
    int y, x;
    int y0, x0;

    /* Center */
    y0 = (y1 + y2) / 2;
    x0 = (x1 + x2) / 2;

    for (y = y1; y <= y2; y++)
    {
        cave_set_feat(y, x0, feat);
    }

    for (x = x1; x <= x2; x++)
    {
        cave_set_feat(y0, x, feat);
    }
}

/*
 * Room building routines.
 *
 * Six basic room types:
 *   1 -- normal
 *   2 -- cross shaped
 *   3 -- (removed)
 *   4 -- large room with features (removed)
 *   5 -- monster nests (removed)
 *   6 -- least vaults (formerly: monster pits)
 *   7 -- lesser vaults
 *   8 -- greater vaults
 */

/*
 * Forward declaration for quest vault helper
 */
static bool solid_rock_reduced_padding(int y1, int x1, int y2, int x2);
static bool place_room_forced(int y0, int x0, vault_type* v_ptr);
bool try_quest_vault_type(int vault_type, bool *had_eligible_candidate);

/*
 * Type 1 -- normal rectangular rooms
 */
bool build_type1(int y0, int x0)
{
    int y, x;

    int y1, x1, y2, x2;

    int light = false;

    // Occasional light - chance of darkness starts very small and
    // increases quadratically until always dark at 950 ft
    if ((p_ptr->depth < dieroll(MORGOTH_DEPTH - 1))
        || (p_ptr->depth < dieroll(MORGOTH_DEPTH - 1)))
    {
        light = true;
    }

    /* Pick a room size */
    y1 = y0 - dieroll(3);
    x1 = x0 - dieroll(5);
    y2 = y0 + dieroll(3);
    x2 = x0 + dieroll(4) + 1;

    /* Sil: bounds checking */
    if ((y1 <= 3) || (x1 <= 3) || (y2 >= p_ptr->cur_map_hgt - 3)
        || (x2 >= p_ptr->cur_map_wid - 3))
    {
        return (false);
    }

    /* Check to see if the location is all plain rock */
    if (!solid_rock(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
    {
        return (false);
    }

    if (doubled_wall(y1, x1, y2, x2))
    {
        return (false);
    }

    /* Save the corner locations */
    dun->corner[dun->cent_n].y1 = y1;
    dun->corner[dun->cent_n].x1 = x1;
    dun->corner[dun->cent_n].y2 = y2;
    dun->corner[dun->cent_n].x2 = x2;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = ROOM_KIND_CLASSIC;
    dun->is_quest[dun->cent_n] = false;
    dun->cent_n++;

    /* Generate new room */
    generate_room(y1 - 1, x1 - 1, y2 + 1, x2 + 1, light);

    /* Generate outer walls */
    generate_draw(y1 - 1, x1 - 1, y2 + 1, x2 + 1, FEAT_WALL_OUTER);

    /* Generate inner floors */
    generate_fill(y1, x1, y2, x2, FEAT_FLOOR);

    /* Hack -- Occasional pillar room */
    if (one_in_(20) && ((x2 - x1) % 2 == 0) && ((y2 - y1) % 2 == 0))
    {
        for (y = y1 + 1; y <= y2; y += 2)
        {
            for (x = x1 + 1; x <= x2; x += 2)
            {
                cave_set_feat(y, x, FEAT_WALL_INNER);
            }
        }
    }

    /* Hack -- Occasional pillar-lined room */
    if (one_in_(10) && ((x2 - x1) % 2 == 0) && ((y2 - y1) % 2 == 0))
    {
        for (y = y1 + 1; y <= y2; y += 2)
        {
            for (x = x1 + 1; x <= x2; x += 2)
            {
                if ((x == x1 + 1) || (x == x2 - 1) || (y == y1 + 1)
                    || (y == y2 - 1))
                {
                    cave_set_feat(y, x, FEAT_WALL_INNER);
                }
            }
        }
    }

    return (true);
}

/*
 * Type 2 -- Cross shaped rooms
 */
bool build_type2(int y0, int x0)
{
    int y, x;

    int y1h, x1h, y2h, x2h;
    int y1v, x1v, y2v, x2v;

    int h_hgt, h_wid, v_hgt, v_wid;

    int light = false;

    /* Occasional light - always at level 1 through to never at Morgoth's level
     */
    if (p_ptr->depth < dieroll(MORGOTH_DEPTH))
        light = true;

    /* Pick a room size */

    h_hgt = 1; /* 3 */
    h_wid = rand_range(5, 7); /* 11, 13, 15 */

    y1h = y0 - h_hgt;
    x1h = x0 - h_wid;
    y2h = y0 + h_hgt;
    x2h = x0 + h_wid;

    v_hgt = rand_range(3, 6); /* 7, 9, 11, 13 */
    v_wid = rand_range(1, 2); /* 3, 5 */

    y1v = y0 - v_hgt;
    x1v = x0 - v_wid;
    y2v = y0 + v_hgt;
    x2v = x0 + v_wid;

    /* Sil: bounds checking */
    if ((y1v <= 3) || (x1h <= 3) || (y2v >= p_ptr->cur_map_hgt - 3)
        || (x2h >= p_ptr->cur_map_wid - 3))
    {
        return (false);
    }

    /* Check to see if the location is all plain rock */
    if (!solid_rock(y1v - 1, x1h - 1, y2v + 1, x2h + 1))
    {
        return (false);
    }

    if (doubled_wall(y1v, x1h, y2v, x2h))
    {
        return (false);
    }

    /* Save the corner locations */
    dun->corner[dun->cent_n].y1 = y1v;
    dun->corner[dun->cent_n].x1 = x1h;
    dun->corner[dun->cent_n].y2 = y2v;
    dun->corner[dun->cent_n].x2 = x2h;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = ROOM_KIND_CROSS;
    dun->is_quest[dun->cent_n] = false;
    dun->cent_n++;

    /* Generate new room */
    generate_room(y1h - 1, x1h - 1, y2h + 1, x2h + 1, light);
    generate_room(y1v - 1, x1v - 1, y2v + 1, x2v + 1, light);

    /* Generate outer walls */
    generate_draw(y1h - 1, x1h - 1, y2h + 1, x2h + 1, FEAT_WALL_OUTER);
    generate_draw(y1v - 1, x1v - 1, y2v + 1, x2v + 1, FEAT_WALL_OUTER);

    /* Generate inner floors */
    generate_fill(y1h, x1h, y2h, x2h, FEAT_FLOOR);
    generate_fill(y1v, x1v, y2v, x2v, FEAT_FLOOR);

    /* Hack -- Occasional pillar room */

    switch (dieroll(7))
    {
    case 1:
    {
        if ((v_wid == 2) && (v_hgt == 6))
        {
            for (y = y1v + 1; y <= y2v; y += 2)
            {
                for (x = x1v + 1; x <= x2v; x += 2)
                {
                    cave_set_feat(y, x, FEAT_WALL_INNER);
                }
            }
            {
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode_source_cfg(
                        drop_mode_for_point(y0, x0), PARTITION_DROP_SOURCE_FLOOR);
                place_object_with_profile_params(y0, x0, object_level, object_level,
                    DROP_QUALITY_NORMAL, DROP_TYPE_CHEST, false, 1, 0, &active_profile);
            }
        }
        break;
    }
    case 2:
    {
        if ((v_wid == 1) && (h_hgt == 1))
        {
            generate_plus(y0 - 1, x0 - 1, y0 + 1, x0 + 1, FEAT_WALL_INNER);
        }
        break;
    }
    case 3:
    {
        if ((v_wid == 1) && (h_hgt == 1))
        {
            cave_set_feat(y0 - 1, x0 - 1, FEAT_WALL_INNER);
            cave_set_feat(y0 + 1, x0 - 1, FEAT_WALL_INNER);
            cave_set_feat(y0 - 1, x0 + 1, FEAT_WALL_INNER);
            cave_set_feat(y0 + 1, x0 + 1, FEAT_WALL_INNER);
        }
        break;
    }
    case 4:
    {
        if ((v_wid == 1) && (h_hgt == 1))
        {
            cave_set_feat(y0, x0 - 1, FEAT_WALL_INNER);
            cave_set_feat(y0, x0 + 1, FEAT_WALL_INNER);
            cave_set_feat(y0 - 1, x0, FEAT_WALL_INNER);
            cave_set_feat(y0 + 1, x0, FEAT_WALL_INNER);
        }
        break;
    }
    default:
    {
        break;
    }
    }

    return (true);
}

/*
 *  Has a very good go at placing a monster of kind represented by a flag
 *  (eg RF3_DRAGON) at (y,x). It is goverened by a maximum depth and tries
 *  100 times at this depth and each depth below it.
 */
extern void place_monster_by_flag(
    int y, int x, int flagset, u32b f, bool allow_unique, int max_depth)
{
    bool got_r_idx = false;
    int tries = 0;
    int r_idx;
    monster_race* r_ptr;
    int depth = max_depth;

    while (!got_r_idx && (depth > 0))
    {
        r_idx = get_mon_num(depth, false, true, true);
        r_ptr = &r_info[r_idx];

        if (allow_unique || !(r_ptr->flags1 & (RF1_UNIQUE)))
        {
            if (((flagset == 1) && (r_ptr->flags1 & (f)))
                || ((flagset == 2) && (r_ptr->flags2 & (f)))
                || ((flagset == 3) && (r_ptr->flags3 & (f)))
                || ((flagset == 4) && (r_ptr->flags4 & (f))))
            {
                got_r_idx = true;
                break;
            }
        }

        tries++;
        if (tries >= 100)
        {
            tries = 0;
            depth--;
        }
    }

    // place a monster of that type if you could find one
    if (got_r_idx)
        place_monster_one(y, x, r_idx, true, false, NULL);
}

/*
 *  Has a very good go at placing a monster of kind represented by its racial
 * letter (eg 'v' for vampire) at (y,x). It is goverened by a maximum depth and
 * tries 100 times at this depth and each depth below it.
 */
void place_monster_by_letter(
    int y, int x, char c, bool allow_unique, int max_depth)
{
    bool got_r_idx = false;
    int tries = 0;
    int r_idx;
    monster_race* r_ptr;
    int depth = max_depth;

    while (!got_r_idx && (depth > 0))
    {
        r_idx = get_mon_num(depth, false, true, true);
        r_ptr = &r_info[r_idx];
        if ((r_ptr->d_char == c)
            && (allow_unique || !(r_ptr->flags1 & (RF1_UNIQUE))))
        {
            got_r_idx = true;
            break;
        }

        tries++;
        if (tries >= 100)
        {
            tries = 0;
            depth--;
        }
    }

    // place a monster of that type if you could find one
    if (got_r_idx)
        place_monster_one(y, x, r_idx, true, false, NULL);
}

/*
 * Vault drop frequency gating Ã¢â‚¬â€ controls how many items spawn per vault symbol.
 * Driven by op_ptr->vault_drop_frequency (VDF_NORMAL..VDF_PLENTIFUL).
 */
typedef enum vault_drop_gate_kind {
    VDG_NORMAL = 0,
    VDG_GOOD,
    VDG_GREAT,
    VDG_CHEST
} vault_drop_gate_kind;

static int vault_drop_gate_percent(vault_drop_gate_kind kind)
{
    switch (op_ptr->vault_drop_frequency)
    {
    case VDF_PLENTIFUL:
        return 100;
    case VDF_NORMAL:
        switch (kind)
        {
        case VDG_NORMAL: return 40;
        case VDG_GOOD:   return 66;
        case VDG_GREAT:  return 100;
        case VDG_CHEST:  return 100;
        }
        break;
    case VDF_MODEST:
        switch (kind)
        {
        case VDG_NORMAL: return 20;
        case VDG_GOOD:   return 50;
        case VDG_GREAT:  return 75;
        case VDG_CHEST:  return 100;
        }
        break;
    case VDF_SCARCE:
        switch (kind)
        {
        case VDG_NORMAL: return 10;
        case VDG_GOOD:   return 25;
        case VDG_GREAT:  return 40;
        case VDG_CHEST:  return 66;
        }
        break;
    case VDF_MEAGER:
        switch (kind)
        {
        case VDG_NORMAL: return 0;
        case VDG_GOOD:   return 10;
        case VDG_GREAT:  return 20;
        case VDG_CHEST:  return 33;
        }
        break;
    }

    return 100;
}

static bool vault_drop_passes(vault_drop_gate_kind kind)
{
    int chance = vault_drop_gate_percent(kind);

    if (chance <= 0)
        return false;
    if (chance >= 100)
        return true;

    return percent_chance(chance);
}

/*
 * Hack -- fill in "vault" rooms
 */
static bool build_vault(int y0, int x0, vault_type* v_ptr, bool flip_d)
{
    int ymax = v_ptr->hgt;
    int xmax = v_ptr->wid;
    cptr data = v_text + v_ptr->text;
    int dx, dy, x, y;
    int ax, ay;
    bool flip_v = false;
    bool flip_h = false;
    int multiplier;

    int original_monster_level = monster_level;

    log_trace("build_vault: Building vault '%s' with color=%d at center (%d,%d), size %dx%d", 
              v_name + v_ptr->name, v_ptr->color, y0, x0, xmax, ymax);
    log_trace("build_vault: Vault flags = 0x%x, flip_d = %s", v_ptr->flags, flip_d ? "true" : "false");
    
    /* DEBUGGING: Check if this is a quest vault */
    if (v_ptr->flags & VLT_QUEST) {
        log_trace("build_vault: *** QUEST VAULT DETECTED *** Building '%s'", v_name + v_ptr->name);
    }

    cptr t;

    // Check that the vault doesn't contain invalid things for its depth
    for (t = data, dy = 0; dy < ymax; dy++)
    {
        for (dx = 0; dx < xmax; dx++, t++)
        {
            // Barrow wights can't be deeper than level 13
            if ((*t == 'W') && (p_ptr->depth > 13))
            {
                log_debug("Skipped a barrow wight vault.");
                return (false);
            }

            // chasms can't occur at 1000 ft
            if ((*t == '7') && (p_ptr->depth >= MORGOTH_DEPTH))
            {
                return (false);
            }
        }
    }

    // reflections
    if ((p_ptr->depth > 0) && (p_ptr->depth < MORGOTH_DEPTH))
    {
        // reflect it vertically half the time
        if (one_in_(2))
            flip_v = true;

        // reflect it horizontally half the time
        if (one_in_(2))
            flip_h = true;
    }

    /* Begin the vault style context now that the vault is accepted */
    styles_begin_vault(-1, 0);
    /* If vault has explicit style list, use it (support '*'=-1, '$'=-2) */
    styles_reset_vault_weights();
    if (v_ptr->style_count > 0) {
        for (int si = 0; si < v_ptr->style_count; ++si) {
            int sidx = v_ptr->style_idx[si];
            int w = v_ptr->style_weight[si];
            if (sidx == -1) {
                int lp = styles_get_level_primary_style();
                if (lp >= 0) styles_add_vault_weight(lp, w);
            } else if (sidx == -2) {
                /* '$' token: pick one random style from the current level's
                 * available list and add it with the specified weight. */
                int rs = styles_pick_random_from_level();
                if (rs >= 0) styles_add_vault_weight(rs, w);
            } else {
                styles_add_vault_weight(sidx, w);
            }
        }
    } else {
        /* No S: provided -- choose a random style from the depth-available list */
        int rs = styles_pick_random_from_level();
        if (rs >= 0) styles_add_vault_weight(rs, 1);
    }
    /* Choose one primary style for the entire vault */
    styles_select_vault_primary();
    log_debug("build_vault: level_primary=%d vault_primary=%d",
        styles_get_level_primary_style(), styles_get_vault_primary_style());

    /* Place dungeon features and objects */
    int vault_primary_sidx_for_encoding = styles_get_vault_primary_style();
    int v_min_y = 32767, v_min_x = 32767, v_max_y = -32768, v_max_x = -32768; /* track vault bbox */
    for (t = data, dy = 0; dy < ymax; dy++)
    {
        if (flip_v)
            ay = ymax - 1 - dy;
        else
            ay = dy;

        for (dx = 0; dx < xmax; dx++, t++)
        {
            if (flip_h)
                ax = xmax - 1 - dx;
            else
                ax = dx;

            /* Extract the location */
            x = x0 - (xmax / 2) + ax;
            y = y0 - (ymax / 2) + ay;

            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

            /* Hack -- skip "non-grids" but still advance bbox only on placed tiles */
            if (*t == ' ')
                continue;

            /* Track bbox of actual vault content */
            if (y < v_min_y) v_min_y = y;
            if (y > v_max_y) v_max_y = y;
            if (x < v_min_x) v_min_x = x;
            if (x > v_max_x) v_max_x = x;

            /* Lay down a floor, encoding the vault style and forcing first variant */
            if (vault_primary_sidx_for_encoding >= 0) {
                int enc = COLOR_STYLE_BASE + COLOR_STYLE_FLAG_FIRSTVAR + (vault_primary_sidx_for_encoding & (COLOR_STYLE_SLOT_MAX - 1));
                cave_set_feat_with_color(y, x, FEAT_FLOOR, enc);
            } else {
                cave_set_feat(y, x, FEAT_FLOOR);
            }

            /* Part of a vault */
            cave_info[y][x] |= (CAVE_ROOM | CAVE_ICKY);

            /* Analyze the grid */
            switch (*t)
            {
            /* Granite wall (outer) */
            case '$':
            {
                cave_set_feat_with_color(y, x, FEAT_WALL_OUTER, 0);
                break;
            }
            /* Granite wall (inner) */
            case '#':
            {
                cave_set_feat_with_color(y, x, FEAT_WALL_INNER, 0);
                break;
            }

            /* Quartz vein */
            case '%':
            {
                cave_set_feat_with_color(y, x, FEAT_QUARTZ, 0);
                break;
            }

            /* Rubble */
            case ':':
            {
                cave_set_feat_with_color(y, x, FEAT_RUBBLE, 0);
                break;
            }

            /* Glyph of warding */
            case ';':
            {
                cave_set_feat(y, x, FEAT_GLYPH);
                break;
            }

                /* Down staircase */
            case '>':
            {
                cave_set_feat(y, x, FEAT_MORE);
                break;
            }

            /* Up staircase */
            case '<':
            {
                cave_set_feat(y, x, FEAT_LESS);
                break;
            }

            /* Visible door */
            case '+':
            {
                place_closed_door(y, x);
                break;
            }

            /* Secret door */
            case 's':
            {
                place_secret_door(y, x);
                break;
            }

            /* Trap */
            case '^':
            {
                if (one_in_(2))
                    place_trap(y, x);
                break;
            }

            /* Forge */
            case '0':
            {
                place_forge(y, x);
                break;
            }

            /* Chasm */
            case '7':
            {
                cave_set_feat(y, x, FEAT_CHASM);
                break;
            }

            /* Sunlight */
            case ',':
            {
                cave_set_feat(y, x, FEAT_SUNLIGHT);
                break;
            }

            /* Not actually part of the vault after all */
            case ' ':
            {
                // remove room and vault flags
                cave_info[y][x] &= ~(CAVE_ROOM | CAVE_ICKY);
                break;
            }
            }
        }
    }

    /* After placement, apply a 1-tile style halo so adjacent walls/floors match the vault style.
     * Refined: do NOT recolor corridor floor tiles that sit just outside a vault door.
     * We only halo floors when adjacent to a vault wall (not a door), to keep vault
     * entrances blending into the corridor style. Doors themselves remain excluded. */
    if (v_min_y <= v_max_y && v_min_x <= v_max_x) {
        int ay0 = MAX(1, v_min_y - 1);
        int ax0 = MAX(1, v_min_x - 1);
        int ay1 = MIN(p_ptr->cur_map_hgt - 2, v_max_y + 1);
        int ax1 = MIN(p_ptr->cur_map_wid - 2, v_max_x + 1);
        for (int yy = ay0; yy <= ay1; ++yy) {
            for (int xx = ax0; xx <= ax1; ++xx) {
                /* Skip squares that are already part of the vault */
                if (cave_info[yy][xx] & (CAVE_ICKY)) continue;

                /* Only halo cells adjacent to vault content (8-directional),
                 * and classify what kind of vault neighbor it is. */
                bool near_vault_any = false;
                bool near_vault_wall = false;
                bool near_vault_door = false;
                for (int dy2 = -1; dy2 <= 1; ++dy2) {
                    for (int dx2 = -1; dx2 <= 1; ++dx2) {
                        if (dy2 == 0 && dx2 == 0) continue;
                        int ny = yy + dy2, nx = xx + dx2;
                        if (!(cave_info[ny][nx] & (CAVE_ICKY))) continue;
                        near_vault_any = true;
                        int nfeat = cave_feat[ny][nx];
                        /* Door features */
                        if (nfeat == FEAT_OPEN || nfeat == FEAT_BROKEN ||
                            (nfeat >= FEAT_DOOR_HEAD && nfeat <= FEAT_DOOR_TAIL)) {
                            near_vault_door = true;
                        }
                        /* Walls and wall-like */
                        else if ((nfeat >= FEAT_WALL_HEAD && nfeat <= FEAT_WALL_TAIL) ||
                                 nfeat == FEAT_QUARTZ || nfeat == FEAT_RUBBLE) {
                            near_vault_wall = true;
                        }
                    }
                }
                if (!near_vault_any) continue;

                int feat = cave_feat[yy][xx];
                /* Skip doors; let corridor/door visuals remain level-styled */
                if (feat == FEAT_OPEN || feat == FEAT_BROKEN ||
                    (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)) {
                    continue;
                }

                /* Apply to floors only when adjacent to vault walls and NOT adjacent to vault doors */
                if (cave_floorlike_bold(yy, xx)) {
                    if (!(near_vault_wall && !near_vault_door)) continue;
                }
                /* Apply to walls/veins/rubble regardless, to blend the boundary */
                else if ((cave_info[yy][xx] & (CAVE_WALL)) || feat == FEAT_QUARTZ || feat == FEAT_RUBBLE) {
                    /* ok */
                } else {
                    continue;
                }

                {
                    /* Re-encode color to the vault primary style, forcing first variant */
                    int sidx = styles_get_vault_primary_style();
                    if (sidx < 0) sidx = styles_get_level_primary_style();
                    int enc = COLOR_STYLE_BASE + COLOR_STYLE_FLAG_FIRSTVAR + (sidx & (COLOR_STYLE_SLOT_MAX - 1));
                    cave_set_feat_with_color(yy, xx, feat, enc);
                }
            }
        }
    }

    /* Restore level styles after vault placement */
    styles_end_vault();

    /* Place dungeon monsters and objects */
    {
    int previous_build_vault_type = current_build_vault_type;
    current_build_vault_type = v_ptr->typ;
    log_trace(
        "SPECIAL_VAULT_ONLY context enter: vault='%s' type=%d depth=%d previous_type=%d",
        v_name + v_ptr->name, v_ptr->typ, p_ptr->depth,
        previous_build_vault_type);

    for (t = data, dy = 0; dy < ymax; dy++)
    {
        if (flip_v)
            ay = ymax - 1 - dy;
        else
            ay = dy;

        for (dx = 0; dx < xmax; dx++, t++)
        {
            if (flip_h)
                ax = xmax - 1 - dx;
            else
                ax = dx;

            x = x0 - (xmax / 2) + ax;
            y = y0 - (ymax / 2) + ay;

            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

            if (*t == ' ')
                continue;

            if (is_vault_monster_token(*t))
                place_vault_monster_token(*t, y, x);
        }
    }

    for (t = data, dy = 0; dy < ymax; dy++)
    {
        if (flip_v)
            ay = ymax - 1 - dy;
        else
            ay = dy;

        for (dx = 0; dx < xmax; dx++, t++)
        {
            if (flip_h)
                ax = xmax - 1 - dx;
            else
                ax = dx;

            /* Extract the grid */
            x = x0 - (xmax / 2) + ax;
            y = y0 - (ymax / 2) + ay;

            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

            /* Hack -- skip "non-grids" */
            if (*t == ' ')
                continue;

            if (is_vault_monster_token(*t))
                continue;

            /* Analyze the symbol */
            switch (*t)
            {
            /* A monster from 1 level deeper */
            case '1':
            {
                monster_level = player_generation_depth() + 1;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* A monster from 2 levels deeper */
            case '2':
            {
                monster_level = player_generation_depth() + 2;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* A monster from 3 levels deeper */
            case '3':
            {
                monster_level = player_generation_depth() + 3;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* A monster from 4 levels deeper */
            case '4':
            {
                monster_level = player_generation_depth() + 4;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* An object from 1-5 levels deeper (min-depth penalty only) */
            case '*':
            {
                /* Vault loot tuning: reduce item clutter based on drop frequency setting */
                if (!vault_drop_passes(VDG_NORMAL))
                    break;

                int base_depth = player_generation_depth();
                int penalty_depth = base_depth + dieroll(5);
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode_source_cfg(
                        drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
                place_object_with_profile_params(
                    y, x, base_depth, penalty_depth, DROP_QUALITY_NORMAL,
                    DROP_TYPE_NOT_DAMAGED, false, 1, 0, &active_profile);
                break;
            }

            /* A good object from 1-5 levels deeper (min-depth penalty only) */
            case '&':
            {
                if (!vault_drop_passes(VDG_GOOD))
                    break;

                int base_depth = player_generation_depth();
                int penalty_depth = base_depth + dieroll(5);
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode_source_cfg(
                        drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
                bool old_allow_noble_from_quality = drop_allow_noble_from_quality;
                drop_allow_noble_from_quality
                    = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_INCLUDE_VAULTS);
                place_object_with_profile_params(
                    y, x, base_depth, penalty_depth, DROP_QUALITY_GOOD,
                    DROP_TYPE_NOT_DAMAGED, false, 1, 0, &active_profile);
                drop_allow_noble_from_quality = old_allow_noble_from_quality;
                break;
            }

            /* A great object from 1-5 levels deeper (min-depth penalty only) */
            case '!':
            {
                if (!vault_drop_passes(VDG_GREAT))
                    break;

                int base_depth = player_generation_depth();
                int penalty_depth = base_depth + dieroll(5);
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode_source_cfg(
                        drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
                bool old_allow_noble_from_quality = drop_allow_noble_from_quality;
                drop_allow_noble_from_quality
                    = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_INCLUDE_VAULTS);
                place_object_with_profile_params(
                    y, x, base_depth, penalty_depth, DROP_QUALITY_GREAT,
                    DROP_TYPE_NOT_DAMAGED, true,
                    DROP_GREAT_ARTEFACT_WEIGHT_MULTIPLIER,
                    IDENT_HOARD_DROP, &active_profile);
                drop_allow_noble_from_quality = old_allow_noble_from_quality;
                break;
            }

            /* A chest from 5 levels deeper */
            case '~':
            {
                if (!vault_drop_passes(VDG_CHEST))
                    break;

                int chest_depth = player_generation_depth() + 5;

                /* Set vault type context for chest material distribution */
                drop_set_chest_vault_type(v_ptr->typ);
                
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode_source_cfg(
                        drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
                place_object_with_profile_params(
                    y, x, chest_depth, chest_depth, DROP_QUALITY_NORMAL,
                    DROP_TYPE_CHEST, false, 1, 0, &active_profile);
                break;
            }

            /* A skeleton */
            case 'S':
            {
                object_type* i_ptr;
                object_type object_type_body;
                s16b k_idx;

                // make a skeleton 1/2 of the time
                if (one_in_(2))
                {
                    /* Get local object */
                    i_ptr = &object_type_body;

                    /* Wipe the object */
                    object_wipe(i_ptr);

                    if (one_in_(3))
                        k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_HUMAN);
                    else
                        k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ELF);

                    /* Prepare the item */
                    object_prep(i_ptr, k_idx);

                    i_ptr->pval = 1;

                    /* Drop it in the dungeon */
                    drop_near(i_ptr, -1, y, x);
                }
                break;
            }

            /* A human skeleton */
            case 'h':
            {
                object_type* i_ptr;
                object_type object_type_body;
                s16b k_idx;

                /* Get local object */
                i_ptr = &object_type_body;

                /* Wipe the object */
                object_wipe(i_ptr);

                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_HUMAN);

                /* Prepare the item */
                object_prep(i_ptr, k_idx);

                i_ptr->pval = 1;

                /* Drop it in the dungeon */
                drop_near(i_ptr, -1, y, x);
                break;
            }

            /* An orc skeleton */
            case 'e':
            {
                object_type* i_ptr;
                object_type object_type_body;
                s16b k_idx;

                /* Get local object */
                i_ptr = &object_type_body;

                /* Wipe the object */
                object_wipe(i_ptr);

                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ORC);

                /* Prepare the item */
                object_prep(i_ptr, k_idx);

                i_ptr->pval = 1;

                /* Drop it in the dungeon */
                drop_near(i_ptr, -1, y, x);
                break;
            }

            /* A web */
            case 'w':
            {
                /* Place a web trap */
                cave_set_feat(y, x, FEAT_TRAP_WEB);
                break;
            }

            /* Monster and/or object from 1 level deeper */
            case '?':
            {
                int r = dieroll(3);

                if (r <= 2)
                {
                    monster_level = player_generation_depth() + 1;
                    place_monster(y, x, true, true, true);
                    monster_level = original_monster_level;
                }
                if (r >= 2)
                {
                    /* Vault loot tuning: reduce item clutter based on drop frequency setting */
                    if (!vault_drop_passes(VDG_NORMAL))
                        break;

                    int base_depth = player_generation_depth();
                    int penalty_depth = base_depth + 1;
                    partition_drop_profile active_profile =
                        partition_drop_profile_for_mode_source_cfg(
                            drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
                    place_object_with_profile_params(
                        y, x, base_depth, penalty_depth, DROP_QUALITY_NORMAL,
                        DROP_TYPE_UNTHEMED, false, 1, 0, &active_profile);
                }
                break;
            }

            /* Carcharoth */
            case 'C':
            {
                place_vault_monster_token('C', y, x);
                break;
            }

            /* silent watcher */
            case 'H':
            {
                place_vault_monster_token('H', y, x);
                break;
            }

            /* easterling spy */
            case '@':
            {
                place_vault_monster_token('@', y, x);
                break;
            }

            /* orc champion */
            case 'o':
            {
                place_vault_monster_token('o', y, x);
                break;
            }

            /* orc captain */
            case 'O':
            {
                place_vault_monster_token('O', y, x);
                break;
            }

            /* Tulkas Unclad */
            case 'P':
            {
                // Vault-based Tulkas spawning disabled - using room-based spawning only
                log_trace("Vault generation: Found 'P' character for Tulkas but vault spawning disabled");
                break;
            }

            case 'z':
            {
                /* Randomly spawn human or elf thrall */
                /* 5% chance for alert thrall (with quest), 95% for dejected thrall */
                int thrall_r_idx;
                if (one_in_(20))
                {
                    /* Alert thrall with quest */
                    thrall_r_idx = one_in_(2) ? R_IDX_ALERT_HUMAN_THRALL : R_IDX_ALERT_ELF_THRALL;
                }
                else
                {
                    /* Dejected thrall (no quest) */
                    thrall_r_idx = one_in_(2) ? R_IDX_HUMAN_THRALL : R_IDX_ELF_THRALL;
                }
                place_monster_one(y, x, thrall_r_idx, true, true, NULL);
                
                /* Initialize quest for alert thralls */
                if (thrall_r_idx == R_IDX_ALERT_HUMAN_THRALL || thrall_r_idx == R_IDX_ALERT_ELF_THRALL)
                {
                    int m_idx = cave_m_idx[y][x];
                    if (m_idx > 0)
                    {
                        init_thrall_quest(&mon_list[m_idx]);
                    }
                }
                break;
            }

            case 'Z':
            {
                place_vault_monster_token('Z', y, x);
                break;
            }

            /* cat warrior */
            case 'f':
            {
                place_vault_monster_token('f', y, x);
                break;
            }

            /* cat assassin */
            case 'F':
            {
                place_vault_monster_token('F', y, x);
                break;
            }

            /* troll guard */
            case 'T':
            {
                place_vault_monster_token('T', y, x);
                break;
            }

            /* Troll (any monster with RF3_TROLL) */
            case 't':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_TROLL, true,
                    player_generation_depth() + rand_range(1, 4));
                break;
            }

            /* The Mail Corslet of Durin (INSTA_ART; vault-only) */
            case 'u':
            {
                create_chosen_artefact(ART_DURIN, y, x, false);
                break;
            }

            /* barrow wight */
            case 'W':
            {
                place_vault_monster_token('W', y, x);
                break;
            }

            /* dragon */
            case 'd':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_DRAGON, true, player_generation_depth() + 4);
                break;
            }

            /* young cold drake */
            case 'y':
            {
                place_vault_monster_token('y', y, x);
                break;
            }

            /* young fire drake */
            case 'Y':
            {
                place_vault_monster_token('Y', y, x);
                break;
            }

            /* Spider */
            case 'M':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_SPIDER, true,
                    player_generation_depth() + rand_range(1, 4));
                break;
            }

            /* Vampire */
            case 'v':
            {
                place_monster_by_letter(
                    y, x, 'v', true, player_generation_depth() + rand_range(1, 4));
                break;
            }

            /* Wight/Wraith */
            case 'g':
            {
                place_monster_by_letter(
                    y, x, 'W', true, player_generation_depth() + rand_range(1, 4));
                break;
            }

                /* Archer */
            case 'a':
            {
                place_monster_by_flag(
                    y, x, 4, (RF4_ARROW1 | RF4_ARROW2), true,
                    player_generation_depth() + 1);
                break;
            }

                /* Flier */
            case 'b':
            {
                place_monster_by_flag(
                    y, x, 2, (RF2_FLYING), true, player_generation_depth() + 1);
                break;
            }

            /* Wolf */
            case 'c':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_WOLF, true,
                    player_generation_depth() + rand_range(1, 4));
                break;
            }

            /* Rauko */
            case 'r':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_RAUKO, true,
                    player_generation_depth() + rand_range(1, 4));
                break;
            }

                /* Aldor */
            case 'A':
            {
                place_vault_monster_token('A', y, x);
                break;
            }
            /* Aule (quest giver) */
            case 'L':
            {
                place_vault_monster_token('L', y, x);
                break;
            }
            /* Mandos (quest giver) */
            case 'N':
            {
                place_vault_monster_token('N', y, x);
                break;
            }

            /* Glaurung */
            case 'D':
            {
                place_vault_monster_token('D', y, x);
                break;
            }

            /* Ancalagon the Black */
            case 'K':
            {
                place_vault_monster_token('K', y, x);
                break;
            }

            /* Flying cold-drake */
            case 'I':
            {
                place_vault_monster_token('I', y, x);
                break;
            }

            /* Flying fire-drake */
            case 'J':
            {
                place_vault_monster_token('J', y, x);
                break;
            }

            /* Gothmog */
            case 'R':
            {
                place_vault_monster_token('R', y, x);
                break;
            }

            /* Ungoliant */
            case 'U':
            {
                place_vault_monster_token('U', y, x);
                break;
            }

            /* Gorthaur */
            case 'G':
            {
                place_vault_monster_token('G', y, x);
                break;
            }

            /* Morgoth */
            case 'V':
            {
                place_vault_monster_token('V', y, x);
                break;
            }
            
            /* Duruin (Least of the Balrogs) */
            case 'B':
            {
                place_vault_monster_token('B', y, x);
                break;
            }
            
            /* Whispering shadow */
            case 'q':
            {
                place_vault_monster_token('q', y, x);
                break;
            }
            
            /* Shadow spider */
            case 'j':
            {
                place_vault_monster_token('j', y, x);
                break;
            }
            
            /* Lurking horror */
            case 'k':
            {
                place_vault_monster_token('k', y, x);
                break;
            }
            
            /* Nightthorn */
            case 'n':
            {
                place_vault_monster_token('n', y, x);
                break;
            }
            }
        }
    }

    /* Place dungeon features and objects */
    for (t = data, dy = 0; dy < ymax; dy++)
    {
        if (flip_v)
            ay = ymax - 1 - dy;
        else
            ay = dy;

        for (dx = 0; dx < xmax; dx++, t++)
        {
            if (flip_h)
                ax = xmax - 1 - dx;
            else
                ax = dx;

            /* Extract the location */
            x = x0 - (xmax / 2) + ax;
            y = y0 - (ymax / 2) + ay;

            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

            /* Hack -- skip "non-grids" */
            if (*t == ' ')
                continue;

            // some vaults are always lit
            if (v_ptr->flags & (VLT_LIGHT))
            {
                cave_info[y][x] |= (CAVE_GLOW);
            }

            // traps are usually 5 times as likely in vaults, but are 10 times
            // as likely if the TRAPS flag is set
            multiplier = (v_ptr->flags & (VLT_TRAPS)) ? 10 : 5;

            // another chance to place traps, with 4 times the normal chance
            // so traps in interesting rooms and vaults are a total of 5 times
            // more likely webbed vaults also have a large chance of receiving
            // webs
            if ((v_ptr->flags & (VLT_WEBS)))
            {
                if (cave_naked_bold(y, x) && one_in_(20))
                {
                    /* Place a web trap */
                    cave_set_feat(y, x, FEAT_TRAP_WEB);

                    // Hide it half the time
                    if (one_in_(2))
                    {
                        cave_info[y][x] |= (CAVE_HIDDEN);
                    }
                }
            }
            else if (dieroll(1000)
                <= trap_placement_chance(y, x) * (multiplier - 1))
            {
                place_trap(y, x);
            }
        }
    }

    current_build_vault_type = previous_build_vault_type;
    log_trace(
        "SPECIAL_VAULT_ONLY context leave: vault='%s' restored_type=%d depth=%d",
        v_name + v_ptr->name, current_build_vault_type, p_ptr->depth);
    }

    log_trace("build_vault: Successfully built vault '%s' at (%d,%d)", v_name + v_ptr->name, y0, x0);
    return (true);
}

/*
 * Generate helper -- test a rectangle to see if it is all rock with reduced padding
 * (i.e. not floor and not icky) - used for quest vaults to reduce placement failures
 */
static bool solid_rock_reduced_padding(int y1, int x1, int y2, int x2)
{
    int y, x;

    if (x2 >= MAX_DUNGEON_WID || y2 >= MAX_DUNGEON_HGT)
        return (false);

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
                return (false);
            if (cave_info[y][x] & CAVE_ICKY)
                return (false);
        }
    }
    return (true);
}

/*
 * Compute vault bounds for a candidate placement, accounting for diagonal rotation.
 */
static void compute_vault_bounds(
    int y0, int x0, const vault_type* v_ptr, bool flip_d,
    int* y1, int* x1, int* y2, int* x2)
{
    if (flip_d)
    {
        /* determine the coordinates with height/width flipped */
        *y1 = y0 - (v_ptr->wid / 2);
        *x1 = x0 - (v_ptr->hgt / 2);
        *y2 = *y1 + v_ptr->wid - 1;
        *x2 = *x1 + v_ptr->hgt - 1;
    }
    else
    {
        /* determine the coordinates */
        *y1 = y0 - (v_ptr->hgt / 2);
        *x1 = x0 - (v_ptr->wid / 2);
        *y2 = *y1 + v_ptr->hgt - 1;
        *x2 = *x1 + v_ptr->wid - 1;
    }
}

/*
 * Place a room using forced placement strategy with reduced padding for quest vaults.
 * The caller can suppress failure logging when doing an exhaustive fit scan.
 */
static bool place_room_forced_internal(
    int y0, int x0, vault_type* v_ptr, bool flip_d, bool log_failures)
{
    int y1, x1, y2, x2;

    compute_vault_bounds(y0, x0, v_ptr, flip_d, &y1, &x1, &y2, &x2);

    if (log_failures)
    {
        log_trace(
            "place_room_forced: Attempting quest vault '%s' at center (%d,%d), size %dx%d, flip_d=%s",
            v_name + v_ptr->name, y0, x0, v_ptr->hgt, v_ptr->wid,
            flip_d ? "true" : "false");
    }

    /* make sure that the location is within the map bounds */
    if ((y1 <= 2) || (x1 <= 2) || (y2 >= p_ptr->cur_map_hgt - 2)
        || (x2 >= p_ptr->cur_map_wid - 2))
    {
        if (log_failures)
        {
            log_trace(
                "place_room_forced: Vault bounds check failed - y1=%d x1=%d y2=%d x2=%d (map size %dx%d)",
                y1, x1, y2, x2, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
        }
        return (false);
    }

    /* make sure that the location is empty using reduced padding (1 cell instead of 2) */
    if (!solid_rock_reduced_padding(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
    {
        if (log_failures)
        {
            log_trace(
                "place_room_forced: solid_rock_reduced_padding check failed - area not empty around (%d,%d)-(%d,%d)",
                y1 - 1, x1 - 1, y2 + 1, x2 + 1);
        }
        return (false);
    }

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, flip_d))
    {
        if (log_failures)
        {
            log_trace(
                "place_room_forced: build_vault failed for quest vault '%s' at (%d,%d)",
                v_name + v_ptr->name, y0, x0);
        }
        return (false);
    }

    if (log_failures)
    {
        log_trace(
            "place_room_forced: Successfully built quest vault '%s' at (%d,%d) with reduced padding",
            v_name + v_ptr->name, y0, x0);
    }

    /* save the corner locations */
    dun->corner[dun->cent_n].y1 = y1 + 1;
    dun->corner[dun->cent_n].x1 = x1 + 1;
    dun->corner[dun->cent_n].y2 = y2 - 1;
    dun->corner[dun->cent_n].x2 = x2 - 1;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = (byte)v_ptr->typ;
    dun->is_quest[dun->cent_n] = (v_ptr->flags & VLT_QUEST) ? true : false;
    dun->cent_n++;

    /* Cause a special feeling */
    good_item_flag = true;

    log_trace("build_vault: *** SUCCESSFULLY COMPLETED *** vault '%s' at (%d,%d)", 
              v_name + v_ptr->name, y0, x0);
    
    /* DEBUGGING: For quest vaults, do immediate verification */
    if (v_ptr->flags & VLT_QUEST) {
        int verify_y1 = y0 - v_ptr->hgt / 2;
        int verify_x1 = x0 - v_ptr->wid / 2;
        int verify_y2 = verify_y1 + v_ptr->hgt - 1;
        int verify_x2 = verify_x1 + v_ptr->wid - 1;
        
        int post_walls = 0, post_floors = 0, post_features = 0, post_monsters = 0;
        int post_icky = 0, post_room = 0;
        
        for (int vy = verify_y1; vy <= verify_y2; vy++) {
            for (int vx = verify_x1; vx <= verify_x2; vx++) {
                if (cave_feat[vy][vx] == FEAT_WALL_OUTER || cave_feat[vy][vx] == FEAT_WALL_INNER) {
                    post_walls++;
                } else if (cave_feat[vy][vx] == FEAT_FLOOR) {
                    post_floors++;
                } else if (cave_feat[vy][vx] != FEAT_WALL_EXTRA) {
                    post_features++;
                }
                
                if (cave_m_idx[vy][vx] > 0) {
                    post_monsters++;
                }
                
                if (cave_info[vy][vx] & CAVE_ICKY) {
                    post_icky++;
                }
                
                if (cave_info[vy][vx] & CAVE_ROOM) {
                    post_room++;
                }
            }
        }
        
        log_trace("build_vault: QUEST VAULT POST-BUILD VERIFICATION: Area (%d,%d) to (%d,%d)", 
                  verify_y1, verify_x1, verify_y2, verify_x2);
        log_trace("build_vault: QUEST VAULT POST-BUILD: %d walls, %d floors, %d features, %d monsters", 
                  post_walls, post_floors, post_features, post_monsters);
        log_trace("build_vault: QUEST VAULT POST-BUILD: %d CAVE_ICKY, %d CAVE_ROOM flags", 
                  post_icky, post_room);
    }

    return (true);
}

/*
 * Place a room using forced placement strategy with reduced padding for quest vaults.
 * Prefer the legacy random orientation first, but also try the alternate orientation
 * before giving up so "must place" quest content does not miss obvious fits.
 */
static bool place_room_forced(int y0, int x0, vault_type* v_ptr)
{
    bool allow_flip = !(v_ptr->flags & (VLT_NO_ROTATION));
    bool preferred_flip = allow_flip ? one_in_(3) : false;

    if (place_room_forced_internal(y0, x0, v_ptr, preferred_flip, true))
        return true;

    if (allow_flip && place_room_forced_internal(y0, x0, v_ptr, !preferred_flip, false))
    {
        log_trace("place_room_forced: Quest vault '%s' fit after trying alternate orientation at (%d,%d)",
            v_name + v_ptr->name, y0, x0);
        return true;
    }

    return false;
}

/*
 * Final fallback for "must place" quest vaults: scan the whole map for any fit.
 * This avoids regeneration loops caused by a handful of unlucky center-biased samples.
 */
static bool place_room_forced_exhaustive(
    vault_type* v_ptr, int* placed_y, int* placed_x)
{
    bool allow_flip = !(v_ptr->flags & (VLT_NO_ROTATION));

    for (int y = 3; y < p_ptr->cur_map_hgt - 3; y++)
    {
        for (int x = 3; x < p_ptr->cur_map_wid - 3; x++)
        {
            if (place_room_forced_internal(y, x, v_ptr, false, false))
            {
                if (placed_y) *placed_y = y;
                if (placed_x) *placed_x = x;
                return true;
            }

            if (allow_flip && place_room_forced_internal(y, x, v_ptr, true, false))
            {
                if (placed_y) *placed_y = y;
                if (placed_x) *placed_x = x;
                return true;
            }
        }
    }

    return false;
}

static bool place_room(int y0, int x0, vault_type* v_ptr)
{
    int y1, x1, y2, x2;
    bool flip_d;
    
    log_trace("place_room: Attempting to place vault '%s' at center (%d,%d), size %dx%d", 
             v_name + v_ptr->name, y0, x0, v_ptr->hgt, v_ptr->wid);

    // choose whether to rotate (flip diagonally)
    flip_d = one_in_(3);

    // some vaults ask not be be rotated
    if (v_ptr->flags & (VLT_NO_ROTATION))
        flip_d = false;

    if (flip_d)
    {
        /* determine the coordinates with height/width flipped */
        y1 = y0 - (v_ptr->wid / 2);
        x1 = x0 - (v_ptr->hgt / 2);
        y2 = y1 + v_ptr->wid - 1;
        x2 = x1 + v_ptr->hgt - 1;
    }

    else
    {
        /* determine the coordinates */
        y1 = y0 - (v_ptr->hgt / 2);
        x1 = x0 - (v_ptr->wid / 2);
        y2 = y1 + v_ptr->hgt - 1;
        x2 = x1 + v_ptr->wid - 1;
    }

    /* make sure that the location is within the map bounds */
    if ((y1 <= 3) || (x1 <= 3) || (y2 >= p_ptr->cur_map_hgt - 3)
        || (x2 >= p_ptr->cur_map_wid - 3))
    {
        log_trace("place_room: Vault bounds check failed - y1=%d x1=%d y2=%d x2=%d (map size %dx%d)", 
                 y1, x1, y2, x2, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
        return (false);
    }
    /* make sure that the location is empty */
    if (!solid_rock(y1 - 2, x1 - 2, y2 + 2, x2 + 2))
    {
        log_trace("place_room: solid_rock check failed - area not empty around (%d,%d)-(%d,%d)", 
                 y1 - 2, x1 - 2, y2 + 2, x2 + 2);
        return (false);
    }

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, flip_d))
    {
        log_trace("place_room: build_vault failed for vault '%s' at (%d,%d)", 
                 v_name + v_ptr->name, y0, x0);
        return (false);
    }
    
    log_trace("place_room: Successfully built vault '%s' at (%d,%d)", 
             v_name + v_ptr->name, y0, x0);

    /* save the corner locations */
    dun->corner[dun->cent_n].y1 = y1 + 1;
    dun->corner[dun->cent_n].x1 = x1 + 1;
    dun->corner[dun->cent_n].y2 = y2 - 1;
    dun->corner[dun->cent_n].x2 = x2 - 1;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = (byte)v_ptr->typ;
    dun->is_quest[dun->cent_n] = (v_ptr->flags & VLT_QUEST) ? true : false;
    dun->cent_n++;

    /* Cause a special feeling */
    good_item_flag = true;

    return (true);
}

typedef enum vault_dock_dir
{
    VAULT_DOCK_NORTH = 0,
    VAULT_DOCK_EAST = 1,
    VAULT_DOCK_SOUTH = 2,
    VAULT_DOCK_WEST = 3
} vault_dock_dir_t;

/* Ensure we have clear granite around a prospective docked vault, allowing
 * the contact edge to abut an existing vault wall. */
static bool area_clear_for_vault_dock(
    int y1, int x1, int y2, int x2, vault_dock_dir_t dir)
{
    int y_lo = y1 - 1;
    int y_hi = y2 + 1;
    int x_lo = x1 - 1;
    int x_hi = x2 + 1;

    if ((y_lo < 1) || (x_lo < 1) || (y_hi >= p_ptr->cur_map_hgt - 1)
        || (x_hi >= p_ptr->cur_map_wid - 1))
    {
        return false;
    }

    for (int y = y_lo; y <= y_hi; ++y)
    {
        for (int x = x_lo; x <= x_hi; ++x)
        {
            bool on_contact = false;
            switch (dir)
            {
            case VAULT_DOCK_EAST:
                on_contact = (x == x1 - 1);
                break;
            case VAULT_DOCK_WEST:
                on_contact = (x == x2 + 1);
                break;
            case VAULT_DOCK_NORTH:
                on_contact = (y == y2 + 1);
                break;
            case VAULT_DOCK_SOUTH:
                on_contact = (y == y1 - 1);
                break;
            }

            if (on_contact)
            {
                /* Allow touching an existing vault wall, but not overlapping
                 * known open space such as corridors. */
                if ((cave_feat[y][x] == FEAT_FLOOR)
                    && !(cave_info[y][x] & (CAVE_ICKY)))
                {
                    return false;
                }
                continue;
            }

            if (cave_info[y][x] & (CAVE_ROOM | CAVE_ICKY))
            {
                return false;
            }

            if (cave_feat[y][x] != FEAT_WALL_EXTRA)
            {
                return false;
            }
        }
    }

    return true;
}

/* Pick a contact point along one edge of an existing vault, preferring doors
 * but falling back to plain walls. */
static bool choose_vault_contact(
    int base_idx, vault_dock_dir_t dir, int* y_out, int* x_out)
{
    int y1 = dun->corner[base_idx].y1 - 1;
    int y2 = dun->corner[base_idx].y2 + 1;
    int x1 = dun->corner[base_idx].x1 - 1;
    int x2 = dun->corner[base_idx].x2 + 1;

    int door_seen = 0, wall_seen = 0;
    int door_y = 0, door_x = 0, wall_y = 0, wall_x = 0;

    if (dir == VAULT_DOCK_EAST || dir == VAULT_DOCK_WEST)
    {
        int x = (dir == VAULT_DOCK_EAST) ? x2 : x1;
        for (int y = y1 + 1; y <= y2 - 1; ++y)
        {
            if (!(cave_info[y][x] & (CAVE_ICKY)))
                continue;
            int feat = cave_feat[y][x];
            if (feature_is_any_door(feat))
            {
                door_seen++;
                if (one_in_(door_seen))
                {
                    door_y = y;
                    door_x = x;
                }
            }
            else if ((feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL))
            {
                wall_seen++;
                if (one_in_(wall_seen))
                {
                    wall_y = y;
                    wall_x = x;
                }
            }
        }
    }
    else
    {
        int y = (dir == VAULT_DOCK_NORTH) ? y1 : y2;
        for (int x = x1 + 1; x <= x2 - 1; ++x)
        {
            if (!(cave_info[y][x] & (CAVE_ICKY)))
                continue;
            int feat = cave_feat[y][x];
            if (feature_is_any_door(feat))
            {
                door_seen++;
                if (one_in_(door_seen))
                {
                    door_y = y;
                    door_x = x;
                }
            }
            else if ((feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL))
            {
                wall_seen++;
                if (one_in_(wall_seen))
                {
                    wall_y = y;
                    wall_x = x;
                }
            }
        }
    }

    if (door_seen > 0)
    {
        *y_out = door_y;
        *x_out = door_x;
        return true;
    }
    if (wall_seen > 0)
    {
        *y_out = wall_y;
        *x_out = wall_x;
        return true;
    }
    return false;
}

/* Attempt to place a vault flush against an existing vault so that a single
 * door separates them. Returns the placed centre if successful. */
static bool try_place_docked_vault(
    vault_type* v_ptr, int* out_y0, int* out_x0)
{
    if (!room_kind_is_vault((byte)v_ptr->typ))
    {
        return false;
    }

    /* Never dock Morgoth's throne room */
    if (v_ptr->typ == 9)
    {
        return false;
    }

    if (v_ptr->flags & (VLT_QUEST))
    {
        return false;
    }

    if (dun->cent_n >= room_capacity_limit())
    {
        return false;
    }

    /* Collect existing vault indices to target */
    int vault_indices[CENT_MAX];
    int vault_count = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        if (room_kind_is_vault(dun->kind[i]) && !dun->is_quest[i] && dun->kind[i] != 9)
        {
            vault_indices[vault_count++] = i;
        }
    }
    if (vault_count == 0)
    {
        return false;
    }

    /* Try a handful of random attachment attempts */
    for (int attempt = 0; attempt < 40; ++attempt)
    {
        styles_set_vault_avoid_style(-1);
        int base_idx = vault_indices[rand_int(vault_count)];
        int base_y1 = dun->corner[base_idx].y1 - 1;
        int base_y2 = dun->corner[base_idx].y2 + 1;
        int base_x1 = dun->corner[base_idx].x1 - 1;
        int base_x2 = dun->corner[base_idx].x2 + 1;

        vault_dock_dir_t dir_order[4] = {VAULT_DOCK_NORTH, VAULT_DOCK_EAST,
            VAULT_DOCK_SOUTH, VAULT_DOCK_WEST};
        for (int s = 0; s < 4; ++s)
        {
            int swap_idx = rand_int(4);
            vault_dock_dir_t tmp = dir_order[s];
            dir_order[s] = dir_order[swap_idx];
            dir_order[swap_idx] = tmp;
        }

        for (int di = 0; di < 4; ++di)
        {
            vault_dock_dir_t dir = dir_order[di];
            int contact_y = 0, contact_x = 0;
            if (!choose_vault_contact(base_idx, dir, &contact_y, &contact_x))
            {
                continue;
            }

            /* Prefer a different primary style than the contacted vault */
            int avoid_style = style_at_color(contact_y, contact_x);
            styles_set_vault_avoid_style(avoid_style);

            bool flip_d = (!(v_ptr->flags & (VLT_NO_ROTATION)) && one_in_(3));
            int h = flip_d ? v_ptr->wid : v_ptr->hgt;
            int w = flip_d ? v_ptr->hgt : v_ptr->wid;

            int y0 = 0, x0 = 0, y1 = 0, x1 = 0, y2 = 0, x2 = 0;

            switch (dir)
            {
            case VAULT_DOCK_EAST:
                x1 = base_x2 + 1;
                x2 = x1 + w - 1;
                x0 = x1 + w / 2;
                y0 = contact_y;
                y1 = y0 - h / 2;
                y2 = y1 + h - 1;
                break;
            case VAULT_DOCK_WEST:
                x2 = base_x1 - 1;
                x1 = x2 - w + 1;
                x0 = x1 + w / 2;
                y0 = contact_y;
                y1 = y0 - h / 2;
                y2 = y1 + h - 1;
                break;
            case VAULT_DOCK_NORTH:
                y2 = base_y1 - 1;
                y1 = y2 - h + 1;
                y0 = y1 + h / 2;
                x0 = contact_x;
                x1 = x0 - w / 2;
                x2 = x1 + w - 1;
                break;
            case VAULT_DOCK_SOUTH:
                y1 = base_y2 + 1;
                y2 = y1 + h - 1;
                y0 = y1 + h / 2;
                x0 = contact_x;
                x1 = x0 - w / 2;
                x2 = x1 + w - 1;
                break;
            }

            if ((y1 <= 3) || (x1 <= 3) || (y2 >= p_ptr->cur_map_hgt - 3)
                || (x2 >= p_ptr->cur_map_wid - 3))
            {
                continue;
            }

            if (!area_clear_for_vault_dock(y1, x1, y2, x2, dir))
            {
                continue;
            }

            if (!build_vault(y0, x0, v_ptr, flip_d))
            {
                styles_set_vault_avoid_style(-1);
                continue;
            }

            dun->corner[dun->cent_n].y1 = y1 + 1;
            dun->corner[dun->cent_n].x1 = x1 + 1;
            dun->corner[dun->cent_n].y2 = y2 - 1;
            dun->corner[dun->cent_n].x2 = x2 - 1;
            dun->cent[dun->cent_n].y = y0;
            dun->cent[dun->cent_n].x = x0;
            dun->kind[dun->cent_n] = (byte)v_ptr->typ;
            dun->is_quest[dun->cent_n] = false;
            int new_idx = dun->cent_n;
            dun->cent_n++;

            dun->connection[base_idx][new_idx] = true;
            dun->connection[new_idx][base_idx] = true;

            int new_y = contact_y;
            int new_x = contact_x;
            if (dir == VAULT_DOCK_EAST)
                new_x = contact_x + 1;
            else if (dir == VAULT_DOCK_WEST)
                new_x = contact_x - 1;
            else if (dir == VAULT_DOCK_SOUTH)
                new_y = contact_y + 1;
            else
                new_y = contact_y - 1;

            if (!feature_is_any_door(cave_feat[contact_y][contact_x]))
            {
                place_closed_door(contact_y, contact_x);
            }

            /* Carve through walls in BOTH vaults to ensure passability.
             * We need to carve into the docked vault AND into the base vault,
             * since either side may have thick walls at the contact point. */
            int dy = 0, dx = 0;
            if (dir == VAULT_DOCK_EAST) dx = 1;
            else if (dir == VAULT_DOCK_WEST) dx = -1;
            else if (dir == VAULT_DOCK_SOUTH) dy = 1;
            else dy = -1;

            /* Carve in both directions from the door */
            for (int side = 0; side < 2; ++side)
            {
                int carve_dy, carve_dx, start_y, start_x;
                
                if (side == 0)
                {
                    /* Carve into the docked vault */
                    carve_dy = dy;
                    carve_dx = dx;
                    start_y = new_y;
                    start_x = new_x;
                }
                else
                {
                    /* Carve into the base vault (opposite direction) */
                    carve_dy = -dy;
                    carve_dx = -dx;
                    start_y = contact_y - dy;
                    start_x = contact_x - dx;
                }
                
                int carve_y = start_y;
                int carve_x = start_x;
                int max_carve = 6;
                bool found_floor = false;
                
                for (int c = 0; c < max_carve; ++c)
                {
                    int feat = cave_feat[carve_y][carve_x];
                    if (feat == FEAT_FLOOR || feature_is_any_door(feat))
                    {
                        found_floor = true;
                        break;
                    }
                    if (!(cave_info[carve_y][carve_x] & CAVE_ICKY))
                        break;
                    cave_set_feat(carve_y, carve_x, FEAT_FLOOR);
                    carve_y += carve_dy;
                    carve_x += carve_dx;
                }
                
                /* If straight carve didn't find floor, search perpendicular */
                if (!found_floor)
                {
                    int perp_dy = (carve_dy == 0) ? 1 : 0;
                    int perp_dx = (carve_dx == 0) ? 1 : 0;
                    int search_radius = 8;
                    
                    for (int sign = -1; sign <= 1; sign += 2)
                    {
                        for (int dist = 1; dist <= search_radius; ++dist)
                        {
                            int check_y = carve_y + sign * perp_dy * dist;
                            int check_x = carve_x + sign * perp_dx * dist;
                            
                            if (!(cave_info[check_y][check_x] & CAVE_ICKY))
                                break;
                            
                            int feat = cave_feat[check_y][check_x];
                            if (feat == FEAT_FLOOR || feature_is_any_door(feat))
                            {
                                for (int d = 1; d < dist; ++d)
                                {
                                    int path_y = carve_y + sign * perp_dy * d;
                                    int path_x = carve_x + sign * perp_dx * d;
                                    cave_set_feat(path_y, path_x, FEAT_FLOOR);
                                }
                                found_floor = true;
                                break;
                            }
                        }
                        if (found_floor) break;
                    }
                }
            }

            good_item_flag = true;

            if (out_y0)
                *out_y0 = y0;
            if (out_x0)
                *out_x0 = x0;

            styles_set_vault_avoid_style(-1);
            return true;
        }
    }

    styles_set_vault_avoid_style(-1);
    return false;
}

/*
 * Type 6 -- least vaults (see "vault.txt")
 */
/* Helper: scan vault template text (from vault.txt) for Aule symbol 'L' BEFORE placement */
static bool vault_template_has_aule(vault_type *v) {
    if (!v || v->text == 0 || v->hgt == 0) return false;
    char *s = v_text + v->text;
    for (int row = 0; row < v->hgt; ++row) {
        if (strchr(s, 'L')) return true; /* 'L' designates Aule in template */
        s += strlen(s) + 1; /* advance to next stored line (null-terminated) */
    }
    return false;
}

static bool vault_template_has_mandos(vault_type *v) {
    if (!v || v->text == 0 || v->hgt == 0) return false;
    char *s = v_text + v->text;
    for (int row = 0; row < v->hgt; ++row) {
        if (strchr(s, 'N')) return true; /* 'N' designates Mandos in template */
        s += strlen(s) + 1; /* advance to next stored line (null-terminated) */
    }
    return false;
}

static bool vault_template_has_duruin(vault_type *v) {
    if (!v) return false;
    const char *name = v_name + v->name;
    return (strstr(name, "Duruin") != NULL);
}

static bool vault_template_has_shadow_bastion(vault_type *v) {
    if (!v) return false;
    return (strstr(v_name + v->name, "Shadow Bastion") != NULL);
}

static bool vault_template_is_orc_stronghold(vault_type *v) {
    if (!v) return false;
    return (strstr(v_name + v->name, "Orc Stronghold") != NULL);
}

/* Global variables to store quest vault coordinates for monitoring */
int qv_stored_y1 = -1, qv_stored_x1 = -1, qv_stored_y2 = -1, qv_stored_x2 = -1;
bool qv_placed_this_level = false;  /* Track if quest vault actually placed this level */

/* DEBUGGING: Function to check if quest vault still exists at monitored coordinates */
void check_quest_vault_integrity(const char* checkpoint_name) {
    if (!qv_placed_this_level) {
        log_trace("VAULT INTEGRITY CHECK [%s]: No quest vault placed this level - skipping check", checkpoint_name);
        return;
    }
    if (qv_stored_y1 < 0 || qv_stored_y2 < 0) {
        log_trace("VAULT INTEGRITY CHECK [%s]: No quest vault coordinates stored", checkpoint_name);
        return;
    }
    
    int check_walls = 0, check_floors = 0, check_features = 0, check_monsters = 0;
    int check_icky = 0, check_room = 0, check_extra = 0;
    
    for (int cy = qv_stored_y1; cy <= qv_stored_y2; cy++) {
        for (int cx = qv_stored_x1; cx <= qv_stored_x2; cx++) {
            if (cave_feat[cy][cx] == FEAT_WALL_OUTER || cave_feat[cy][cx] == FEAT_WALL_INNER) {
                check_walls++;
            } else if (cave_feat[cy][cx] == FEAT_FLOOR) {
                check_floors++;
            } else if (cave_feat[cy][cx] == FEAT_WALL_EXTRA) {
                check_extra++;
            } else {
                check_features++;
            }
            
            if (cave_m_idx[cy][cx] > 0) {
                check_monsters++;
            }
            
            if (cave_info[cy][cx] & CAVE_ICKY) {
                check_icky++;
            }
            
            if (cave_info[cy][cx] & CAVE_ROOM) {
                check_room++;
            }
        }
    }
    
    log_trace("VAULT INTEGRITY CHECK [%s]: Area (%d,%d) to (%d,%d)", 
              checkpoint_name, qv_stored_y1, qv_stored_x1, qv_stored_y2, qv_stored_x2);
    log_trace("VAULT INTEGRITY CHECK [%s]: %d walls, %d floors, %d features, %d monsters, %d extra_walls", 
              checkpoint_name, check_walls, check_floors, check_features, check_monsters, check_extra);
    log_trace("VAULT INTEGRITY CHECK [%s]: %d CAVE_ICKY, %d CAVE_ROOM flags", 
              checkpoint_name, check_icky, check_room);
              
    /* Alert if vault appears to be gone */
    if (check_walls < 50 && check_floors < 30) {
        log_trace("VAULT INTEGRITY WARNING [%s]: Vault appears to have been OVERWRITTEN! Very low content.", checkpoint_name);
    }
}

static void process_quest_vault_area(int y0, int x0, vault_type *qv) {
    int y1 = y0 - qv->hgt / 2;
    int x1 = x0 - qv->wid / 2;
    int y2 = y1 + qv->hgt - 1;
    int x2 = x1 + qv->wid - 1;
    bool has_forge = false;
    bool has_aule  = false;
    bool has_mandos = false;
    
    log_trace("Quest vault processing: Area (%d,%d) to (%d,%d), size %dx%d", 
              y1, x1, y2, x2, qv->wid, qv->hgt);
    
    /* Debug: Check what's actually in the vault area */
    int wall_count = 0, floor_count = 0, monster_count = 0, feature_count = 0;
    for (int dy = y1; dy <= y2; ++dy) {
        for (int dx = x1; dx <= x2; ++dx) {
            if (cave_feat[dy][dx] == FEAT_WALL_OUTER || cave_feat[dy][dx] == FEAT_WALL_INNER) {
                wall_count++;
            } else if (cave_feat[dy][dx] == FEAT_FLOOR) {
                floor_count++;
            } else if (cave_feat[dy][dx] != FEAT_WALL_EXTRA) {
                feature_count++;
            }
            
            if (cave_m_idx[dy][dx] > 0) {
                monster_count++;
            }
            
            if ((cave_feat[dy][dx] >= FEAT_FORGE_HEAD) && (cave_feat[dy][dx] <= FEAT_FORGE_TAIL)) {
                if (!has_forge) {
                    p_ptr->aule_forge_y = (byte)dy;
                    p_ptr->aule_forge_x = (byte)dx;
                    has_forge = true;
                    log_trace("Quest vault: Found forge at (%d,%d), feature=%d", dy, dx, cave_feat[dy][dx]);
                }
            }
            if (cave_m_idx[dy][dx] > 0) {
                monster_type *m_ptr = &mon_list[cave_m_idx[dy][dx]];
                if (m_ptr->r_idx == R_IDX_AULE) {
                    has_aule = true;
                    log_trace("Quest vault: Found Aule at (%d,%d)", dy, dx);
                }
                if (m_ptr->r_idx == R_IDX_MANDOS) {
                    has_mandos = true;
                    p_ptr->mandos_vault_y = (byte)dy;
                    p_ptr->mandos_vault_x = (byte)dx;
                    log_trace("Quest vault: Found Mandos at (%d,%d)", dy, dx);
                }
            }
        }
    }
    
    log_trace("Quest vault contents: %d walls, %d floors, %d features, %d monsters", 
              wall_count, floor_count, feature_count, monster_count);
              
    /* DEBUGGING: Store quest vault bounds for monitoring */
    qv_stored_y1 = y1; qv_stored_x1 = x1; qv_stored_y2 = y2; qv_stored_x2 = x2;
    qv_placed_this_level = true;  /* Mark that quest vault was actually placed */
    log_trace("QUEST VAULT MONITOR: Storing bounds (%d,%d) to (%d,%d) for tracking", 
              qv_stored_y1, qv_stored_x1, qv_stored_y2, qv_stored_x2);
              
#if DEBUG_QUEST_VAULT
    qv_y1 = y1; qv_x1 = x1; qv_y2 = y2; qv_x2 = x2; /* record bounds */
    qv_capture();
    qv_dump("initial");
    /* Force mark/reveal for debugging */
    for (int ry = y1; ry <= y2; ++ry) for (int rx = x1; rx <= x2; ++rx) cave_info[ry][rx] |= (CAVE_MARK|CAVE_SEEN|CAVE_GLOW);
#endif
    if (has_forge && has_aule && p_ptr->aule_quest == AULE_QUEST_NOT_STARTED && 
        !quest_metarun_blocked(QUEST_ID_AULE, METARUN_QUEST_AULE) && !p_ptr->quest_reserved[0]) {
        /* Immediately reserve quest slot to prevent other quests from spawning */
        p_ptr->quest_reserved[0] = 1;
        /* Record pending quest state change instead of applying immediately */
        pending_quest_states.has_aule_change = true;
        pending_quest_states.aule_level = p_ptr->depth;
        pending_quest_states.aule_forge_y = p_ptr->aule_forge_y;
        pending_quest_states.aule_forge_x = p_ptr->aule_forge_x;
        level_gen_debug_note_questgiver(QUEST_ID_AULE);
        log_trace("Aule quest: FORGE_PRESENT change DEFERRED (quest vault) at %d,%d depth=%d, quest_reserved[0] set to 1", p_ptr->aule_forge_y, p_ptr->aule_forge_x, p_ptr->depth);
    }
    if (has_mandos) {
        int mandos_target = QUEST_ID_MANDOS;
        bool nonblocking;
        byte mandos_state;
        u32b mandos_flag;

        if (is_maeglin_quest_vault(qv)) {
            mandos_target = QUEST_ID_MANDOS_BETRAYER;
        } else if (is_easterling_quest_vault(qv)) {
            mandos_target = QUEST_ID_MANDOS_TRAITOR;
        } else if (pending_quest_states.mandos_quest_id > 0) {
            mandos_target = pending_quest_states.mandos_quest_id;
        }

        pending_quest_states.mandos_quest_id = mandos_target;
        pending_quest_states.mandos_next_state = QUEST_STATE_GIVER_PRESENT;
        nonblocking = (mandos_target == QUEST_ID_MANDOS_BETRAYER);
        mandos_state = (mandos_target == QUEST_ID_MANDOS) ? p_ptr->mandos_quest : quest_get_state(mandos_target);
        mandos_flag = quest_metarun_flag(mandos_target);

        if (mandos_state == QUEST_STATE_NOT_STARTED && (!p_ptr->quest_reserved[0] || nonblocking)) {
            if (mandos_flag && quest_metarun_blocked(mandos_target, mandos_flag)) {
                log_trace("Mandos quest: blocked during vault processing (quest_id=%d)", mandos_target);
            } else {
                if (!nonblocking) {
                    p_ptr->quest_reserved[0] = 1;
                }
                pending_quest_states.has_mandos_change = true;
                pending_quest_states.mandos_level = p_ptr->depth;
                pending_quest_states.mandos_vault_y = p_ptr->mandos_vault_y;
                pending_quest_states.mandos_vault_x = p_ptr->mandos_vault_x;
                level_gen_debug_note_questgiver(mandos_target);
                log_trace("Mandos quest: GIVER_PRESENT change DEFERRED (quest vault) at %d,%d depth=%d, quest_reserved[0]=%d, quest_id=%d", 
                          p_ptr->mandos_vault_y, p_ptr->mandos_vault_x, p_ptr->depth, p_ptr->quest_reserved[0], mandos_target);
            }
        }
    }
}

bool build_type6(int y0, int x0, bool force_forge)
{
    vault_type* v_ptr;
    int tries = 0;

    /* Pick an interesting room */
    while (true)
    {
        unsigned long long rarity = 0;
        tries++;

        /* Get a random vault record */
        v_ptr = &v_info[rand_int(z_info->v_max)];

        // log_trace("Vault selection: Trying vault #%d '%s' (type=%d, depth=%d, rarity=%d, flags=0x%x)",
        //           (int)(v_ptr - v_info), v_name + v_ptr->name, v_ptr->typ, v_ptr->depth, v_ptr->rarity, v_ptr->flags);

        // if forcing a forge, then skip vaults without forges in them
        if (force_forge && !v_ptr->forge)
        {
            log_trace("Skipping vault - force_forge=true but vault has no forge");
            continue;
        }

        // unless forcing a forge, try additional times to place any vault
        // marked TEST
        if ((tries < 1000) && !(v_ptr->flags & (VLT_TEST))
            && !p_ptr->force_forge)
        {
            // log_trace("Skipping vault - tries=%d, no TEST flag", tries);
            continue;
        }

        rarity = v_ptr->rarity;
        if (p_ptr->depth < 6)
        {
            /* Surface rooms are more common at low depths */
            if (!(v_ptr->flags & (VLT_SURFACE)) && !one_in_(4))
                continue;
        }
        else if (v_ptr->flags & (VLT_SURFACE))
        {
            /* Surface rooms get very much rarer at depth */
            rarity += (1 << (p_ptr->depth));
        }

        /* Accept the first interesting room (but not quest vaults) */
        if ((v_ptr->typ == 6) && (v_ptr->depth <= p_ptr->depth)
            && (v_ptr->max_depth == 0 || p_ptr->depth <= v_ptr->max_depth)
            && (one_in_(rarity)) && !(v_ptr->flags & VLT_QUEST))
            break;

        if (tries > 20000)
        {
            if (!DEPLOYMENT || cheat_room)
                msg_format(
                    "Bug: Could not find a record for an Interesting Room in "
                    "vault.txt");
            return (false);
        }
    }

    if (!force_forge && one_in_(4))
    {
        level_gen_debug_note_room_name(v_name + v_ptr->name);
        if (try_place_docked_vault(v_ptr, NULL, NULL))
        {
            return true;
        }
    }

    level_gen_debug_note_room_name(v_name + v_ptr->name);
    return place_room(y0, x0, v_ptr);
}

/*
 * Type 7 -- lesser vaults (see "vault.txt")
 */
bool build_type7(int y0, int x0)
{
    vault_type* v_ptr;
    int tries = 0;

    /* Pick a lesser vault */
    while (true)
    {
        tries++;

        /* Get a random vault record */
        v_ptr = &v_info[rand_int(z_info->v_max)];

        // try additional times to place any vault marked TEST
        if ((tries < 1000) && !(v_ptr->flags & (VLT_TEST)))
            continue;

        /* Accept the first lesser vault (but not quest vaults) */
        if ((v_ptr->typ == 7) && (v_ptr->depth <= p_ptr->depth)
            && (v_ptr->max_depth == 0 || p_ptr->depth <= v_ptr->max_depth)
            && (one_in_(v_ptr->rarity)) && !(v_ptr->flags & VLT_QUEST))
            break;

        if (tries > 2000)
        {
            msg_format(
                "Bug: Could not find a record for a Lesser Vault in vault.txt");
            return (false);
        }
    }

    bool placed = false;
    int placed_y = y0, placed_x = x0;

    level_gen_debug_note_room_name(v_name + v_ptr->name);
    if (one_in_(4) && try_place_docked_vault(v_ptr, &placed_y, &placed_x))
    {
        placed = true;
    }
    else if (place_room(y0, x0, v_ptr))
    {
        placed = true;
    }

    if (!placed)
        return false;
    /* Message */
    if (cheat_room)
        msg_format("LV (%s).", v_name + v_ptr->name);

    return true;
}

/*
 * Mark greater vault grids with the CAVE_G_VAULT flag.
 * Returns true if it succeds.
 */
static bool mark_g_vault(int y0, int x0, int ymax, int xmax)
{
    int y1, x1, y2, x2, y, x;

    /* Get the coordinates */
    y1 = y0 - ymax / 2;
    x1 = x0 - xmax / 2;
    y2 = y1 + ymax - 1;
    x2 = x1 + xmax - 1;

    /* Step 1 - Mark all grids inside that perimeter with the new flag */
    for (y = y1 + 1; y < y2; y++)
    {
        for (x = x1 + 1; x < x2; x++)
        {
            cave_info[y][x] |= (CAVE_G_VAULT);
        }
    }

    return (true);
}

static bool vault_type8_is_repeated(s16b v_idx)
{
    for (int i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        if (p_ptr->greater_vaults[i] == v_idx)
            return true;
    }

    return false;
}

static bool vault_type8_is_eligible(s16b v_idx, bool test_only)
{
    vault_type* v_ptr = &v_info[v_idx];

    if (v_ptr->typ != 8)
        return false;
    if (v_ptr->flags & VLT_QUEST)
        return false;
    if (v_ptr->depth > p_ptr->depth)
        return false;
    if (v_ptr->max_depth != 0 && p_ptr->depth > v_ptr->max_depth)
        return false;
    if (vault_type8_is_repeated(v_idx))
        return false;
    if (test_only && !(v_ptr->flags & VLT_TEST))
        return false;

    return true;
}

static bool any_eligible_type8_test_vault(void)
{
    for (int i = 0; i < z_info->v_max; i++)
    {
        if (vault_type8_is_eligible(i, false) && (v_info[i].flags & VLT_TEST))
            return true;
    }

    return false;
}

static bool choose_reserved_type8(vault_type** out_v_ptr, s16b* out_v_idx)
{
    int tries = 0;
    bool test_only = any_eligible_type8_test_vault();

    while (tries++ < 2000)
    {
        s16b v_idx = rand_int(z_info->v_max);
        vault_type* v_ptr = &v_info[v_idx];

        if (!vault_type8_is_eligible(v_idx, test_only))
            continue;

        if (!one_in_(vault_type8_generation_rarity(v_ptr, p_ptr->depth)))
            continue;

        *out_v_ptr = v_ptr;
        *out_v_idx = v_idx;
        return true;
    }

    return false;
}

static bool place_type8_vault(int y0, int x0, vault_type* v_ptr, s16b v_idx)
{
    bool placed = false;
    int placed_y = y0;
    int placed_x = x0;

    level_gen_debug_note_greater_vault_name(v_name + v_ptr->name);
    if (one_in_(2) && try_place_docked_vault(v_ptr, &placed_y, &placed_x))
    {
        placed = true;
    }
    else if (place_room(y0, x0, v_ptr))
    {
        placed = true;
    }

    if (!placed)
        return false;

    for (int i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        if (p_ptr->greater_vaults[i] == 0)
        {
            p_ptr->greater_vaults[i] = v_idx;
            break;
        }
    }

    if (cheat_room)
        msg_format("GV (%s).", v_name + v_ptr->name);

    if (mark_g_vault(placed_y, placed_x, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }

    return true;
}

bool build_reserved_type8(int y0, int x0)
{
    vault_type* v_ptr = NULL;
    s16b v_idx = 0;

    if (g_vault_name[0] != '\0')
        return false;

    if (!choose_reserved_type8(&v_ptr, &v_idx))
        return false;

    return place_type8_vault(y0, x0, v_ptr, v_idx);
}

/*
 * Type 8 -- greater vaults (see "vault.txt")
 */
bool build_type8(int y0, int x0)
{
    vault_type* v_ptr = NULL;
    int tries = 0;
    bool found = false;
    bool repeated = false;
    int i;
    s16b v_idx;
    bool prefer_test = any_eligible_type8_test_vault();

    // Can only have one greater vault per level
    if (g_vault_name[0] != '\0')
    {
        return (false);
    }

    /* Pick a greater vault */
    while (!found)
    {
        tries++;

        /* Get a random vault record */
        v_idx = rand_int(z_info->v_max);
        v_ptr = &v_info[v_idx];

        // try additional times to place any vault marked TEST
        if (prefer_test && (tries < 1000) && !(v_ptr->flags & (VLT_TEST)))
            continue;

        /* Surface vaults get exponentially rarer at depth */
        {
            /* Accept the first greater vault (but not quest vaults) */
            if ((v_ptr->typ == 8) && (v_ptr->depth <= p_ptr->depth)
                && (v_ptr->max_depth == 0 || p_ptr->depth <= v_ptr->max_depth)
                && (one_in_(vault_type8_generation_rarity(v_ptr, p_ptr->depth))) && !(v_ptr->flags & VLT_QUEST))
        {
            repeated = false;
            for (i = 0; i < MAX_GREATER_VAULTS; i++)
            {
                if (v_idx == p_ptr->greater_vaults[i])
                {
                    repeated = true;
                }
            }

            if (!repeated)
                found = true;
            }
        }

        if (tries > 2000)
        {
            // if (!repeated) msg_debug("Bug: Could not find a record for a
            // Greater Vault in vault.txt");
            return (false);
        }
    }

    return place_type8_vault(y0, x0, v_ptr, v_idx);
}

/*
 * Type 9 -- Morgoth's vault (see "vault.txt")
 */
bool build_type9(int y0, int x0, vault_type** used_vault)
{
    vault_type* v_ptr;
    int tries = 0;

    /* Pick a version of Morgoth's vault */
    while (true)
    {
        /* Get a random vault record */
        v_ptr = &v_info[rand_int(z_info->v_max)];

        /* Accept the first morgoth vault */
        if (v_ptr->typ == 9)
            break;

        tries++;
        if (tries > 10000)
        {
            msg_format(
                "Could not find a record for Morgoth's Vault in vault.txt");
            return (false);
        }
    }

    if (used_vault)
        *used_vault = v_ptr;

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, false))
    {
        return (false);
    }

    /* Cause a special feeling */
    good_item_flag = true;

    /* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
    if (mark_g_vault(y0, x0, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }

    return (true);
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

/*
 * Type 10 -- The Gates of Angband (see "vault.txt")
 */
bool build_type10(int y0, int x0)
{
    vault_type* v_ptr;

    /* Get the first vault record */
    v_ptr = &v_info[1];

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, false))
    {
        return (false);
    }

    /* Cause a special feeling */
    good_item_flag = true;

    /* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
    if (mark_g_vault(y0, x0, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }

    return (true);
}

/*
 * Attempt to build a room of the given type at the given co-ordinates
 */
#if 0
static bool room_build(int typ)
{
    int y, x;

    if (dun->cent_n >= room_capacity_limit())
    {
        return (false);
    }

    y = rand_range(5, p_ptr->cur_map_hgt - 5);
    x = rand_range(5, p_ptr->cur_map_wid - 5);

    /* Build a room */
    switch (typ)
    {
    /* Build an appropriate room */
    // Greater Vault
    case 8:
    {
        if (!build_type8(y, x))
        {
            return (false);
        }
        break;
    }
    // Lesser Vault
    case 7:
    {
        if (!build_type7(y, x))
        {
            return (false);
        }
        break;
    }
    // Least Vault
    case 6:
    {
        if (!build_type6(y, x, false))
        {
            return (false);
        }
        break;
    }
    // Cross Room
    case 2:
    {
        if (!build_type2(y, x))
        {
            return (false);
        }
        break;
    }
    // Normal Room
    case 1:
    {
        if (!build_type1(y, x))
        {
            return (false);
        }
        break;
    }
    /* Paranoia */
    default:
        return (false);
    }

    /* Success */
    return (true);
}
#endif

/*
 * Try to place a quest vault of specified type using forced placement strategy
 * Returns true if successfully placed, false otherwise
 */
bool place_orc_stronghold(void)
{
    vault_type* qv_ptr;
    int y, x;

    log_trace("Tulkas orc quest: Attempting to force-place Orc Stronghold at depth %d", p_ptr->depth);

    for (int i = 0; i < z_info->v_max; i++)
    {
        qv_ptr = &v_info[i];
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (!vault_template_is_orc_stronghold(qv_ptr)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;
        if (!quest_vault_surface_roll_allows(qv_ptr, p_ptr->depth)) continue;

        level_gen_debug_note_quest_vault_name(v_name + qv_ptr->name);

        y = p_ptr->cur_map_hgt / 2 + rand_range(-p_ptr->cur_map_hgt / 6, p_ptr->cur_map_hgt / 6);
        x = p_ptr->cur_map_wid / 2 + rand_range(-p_ptr->cur_map_wid / 6, p_ptr->cur_map_wid / 6);
        y = MAX(qv_ptr->hgt / 2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
        x = MAX(qv_ptr->wid / 2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

        if (place_room_forced(y, x, qv_ptr)) {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_tulkas_change = true;
            pending_quest_states.tulkas_level = p_ptr->depth;
            pending_quest_states.tulkas_next_state = QUEST_STATE_GIVER_PRESENT;
            pending_quest_states.tulkas_spawn_pending = true;
            log_trace("Tulkas orc quest: Orc Stronghold placed at (%d,%d)", y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d)",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        for (int attempts = 0; attempts < 10; attempts++) {
            y = p_ptr->cur_map_hgt / 2 + rand_range(-p_ptr->cur_map_hgt / 4, p_ptr->cur_map_hgt / 4);
            x = p_ptr->cur_map_wid / 2 + rand_range(-p_ptr->cur_map_wid / 4, p_ptr->cur_map_wid / 4);
            y = MAX(qv_ptr->hgt / 2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
            x = MAX(qv_ptr->wid / 2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

            if (place_room_forced(y, x, qv_ptr)) {
                qv_placed_this_level = true;
                level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
                process_quest_vault_area(y, x, qv_ptr);
                p_ptr->quest_reserved[0] = 1;
                pending_quest_states.has_tulkas_change = true;
                pending_quest_states.tulkas_level = p_ptr->depth;
                pending_quest_states.tulkas_next_state = QUEST_STATE_GIVER_PRESENT;
                pending_quest_states.tulkas_spawn_pending = true;
                log_trace("Tulkas orc quest: Orc Stronghold placed on fallback attempt %d at (%d,%d)", attempts + 1, y, x);
                genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) on fallback %d",
                    v_name + qv_ptr->name, y, x, attempts + 1);
                return true;
            }
        }

        if (place_room_forced_exhaustive(qv_ptr, &y, &x))
        {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_tulkas_change = true;
            pending_quest_states.tulkas_level = p_ptr->depth;
            pending_quest_states.tulkas_next_state = QUEST_STATE_GIVER_PRESENT;
            pending_quest_states.tulkas_spawn_pending = true;
            log_trace("Tulkas orc quest: Orc Stronghold placed by exhaustive scan at (%d,%d)", y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) after full-map scan",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        log_trace("Tulkas orc quest: Orc Stronghold placement failed after all attempts, returning false");
        genlog_quest("QUEST VAULT FAILED: '%s' could not be placed",
            v_name + qv_ptr->name);
        return false;
    }

    log_trace("Tulkas orc quest: Failed to find Orc Stronghold vault template at depth %d", p_ptr->depth);
    return false;
}

bool place_duruin_bastion(void)
{
    vault_type* qv_ptr;
    int y, x;
    
    log_trace("Varda quest: Attempting to force-place Duruin Bastion at depth %d", p_ptr->depth);
    
    for (int i = 0; i < z_info->v_max; i++)
    {
        qv_ptr = &v_info[i];
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (!vault_template_has_duruin(qv_ptr)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;
        if (!quest_vault_surface_roll_allows(qv_ptr, p_ptr->depth)) continue;

        /* Found Duruin Bastion - attempt placement and return result */
        log_trace("Varda quest: Found Duruin Bastion vault at index %d: '%s', attempting placement", i, v_name + qv_ptr->name);
        log_trace("Varda quest: Vault details - typ=%d, hgt=%d, wid=%d, depth=%d, flags=0x%x", 
                  qv_ptr->typ, qv_ptr->hgt, qv_ptr->wid, qv_ptr->depth, qv_ptr->flags);
        level_gen_debug_note_quest_vault_name(v_name + qv_ptr->name);

        int center_y = p_ptr->cur_map_hgt / 2;
        int center_x = p_ptr->cur_map_wid / 2;
        
        /* Attempt primary placement near center */
        y = center_y + rand_range(-p_ptr->cur_map_hgt/6, p_ptr->cur_map_hgt/6);
        x = center_x + rand_range(-p_ptr->cur_map_wid/6, p_ptr->cur_map_wid/6);
        y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
        x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));
        
        if (place_room_forced(y, x, qv_ptr)) {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_varda_change = true;
            pending_quest_states.varda_level = p_ptr->depth;
            pending_quest_states.varda_vault_y = y;
            pending_quest_states.varda_vault_x = x;
            log_trace("Varda quest: Duruin Bastion placed at (%d,%d)", y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d)",
                v_name + qv_ptr->name, y, x);
            return true;
        }
        
        /* Fallback attempts with wider variance */
        for (int attempts = 0; attempts < 10; attempts++) {
            y = center_y + rand_range(-p_ptr->cur_map_hgt/4, p_ptr->cur_map_hgt/4);
            x = center_x + rand_range(-p_ptr->cur_map_wid/4, p_ptr->cur_map_wid/4);
            y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
            x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));
            
            if (place_room_forced(y, x, qv_ptr)) {
                qv_placed_this_level = true;
                level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
                process_quest_vault_area(y, x, qv_ptr);
                p_ptr->quest_reserved[0] = 1;
                pending_quest_states.has_varda_change = true;
                pending_quest_states.varda_level = p_ptr->depth;
                pending_quest_states.varda_vault_y = y;
                pending_quest_states.varda_vault_x = x;
                log_trace("Varda quest: Duruin Bastion placed on fallback attempt %d at (%d,%d)", attempts + 1, y, x);
                genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) on fallback %d",
                    v_name + qv_ptr->name, y, x, attempts + 1);
                return true;
            }
        }

        log_trace("Varda quest: Random placement failed for '%s', scanning the full map for a guaranteed fit",
            v_name + qv_ptr->name);
        if (place_room_forced_exhaustive(qv_ptr, &y, &x))
        {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_varda_change = true;
            pending_quest_states.varda_level = p_ptr->depth;
            pending_quest_states.varda_vault_y = y;
            pending_quest_states.varda_vault_x = x;
            log_trace("Varda quest: Duruin Bastion placed by exhaustive scan at (%d,%d)", y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) after full-map scan",
                v_name + qv_ptr->name, y, x);
            return true;
        }
        
        /* If we reach here, Duruin placement failed - return immediately without trying other vaults */
        log_trace("Varda quest: Duruin Bastion placement failed after all attempts, returning false");
        genlog_quest("QUEST VAULT FAILED: '%s' could not be placed",
            v_name + qv_ptr->name);
        return false;
    }
    
    log_trace("Varda quest: Failed to find Duruin Bastion vault template at depth %d", p_ptr->depth);
    return false;
}

bool place_shadow_bastion(void)
{
    vault_type* qv_ptr;
    int y, x;

    log_trace("Varda quest: Attempting to force-place Shadow Bastion at depth %d", p_ptr->depth);

    for (int i = 0; i < z_info->v_max; i++)
    {
        qv_ptr = &v_info[i];
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (!vault_template_has_shadow_bastion(qv_ptr)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;
        if (!quest_vault_surface_roll_allows(qv_ptr, p_ptr->depth)) continue;

        log_trace("Varda quest: Found Shadow Bastion vault at index %d: '%s', attempting placement", i, v_name + qv_ptr->name);
        level_gen_debug_note_quest_vault_name(v_name + qv_ptr->name);

        y = p_ptr->cur_map_hgt / 2 + rand_range(-p_ptr->cur_map_hgt / 6, p_ptr->cur_map_hgt / 6);
        x = p_ptr->cur_map_wid / 2 + rand_range(-p_ptr->cur_map_wid / 6, p_ptr->cur_map_wid / 6);
        y = MAX(qv_ptr->hgt / 2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
        x = MAX(qv_ptr->wid / 2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

        if (place_room_forced(y, x, qv_ptr)) {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_varda_shadow_change = true;
            pending_quest_states.varda_shadow_level = p_ptr->depth;
            pending_quest_states.varda_shadow_y = y;
            pending_quest_states.varda_shadow_x = x;
            log_trace("Varda quest: Shadow Bastion placed at (%d,%d)", y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d)",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        for (int attempts = 0; attempts < 10; attempts++) {
            y = p_ptr->cur_map_hgt / 2 + rand_range(-p_ptr->cur_map_hgt / 4, p_ptr->cur_map_hgt / 4);
            x = p_ptr->cur_map_wid / 2 + rand_range(-p_ptr->cur_map_wid / 4, p_ptr->cur_map_wid / 4);
            y = MAX(qv_ptr->hgt / 2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
            x = MAX(qv_ptr->wid / 2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

            if (place_room_forced(y, x, qv_ptr)) {
                qv_placed_this_level = true;
                level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
                process_quest_vault_area(y, x, qv_ptr);
                p_ptr->quest_reserved[0] = 1;
                pending_quest_states.has_varda_shadow_change = true;
                pending_quest_states.varda_shadow_level = p_ptr->depth;
                pending_quest_states.varda_shadow_y = y;
                pending_quest_states.varda_shadow_x = x;
                log_trace("Varda quest: Shadow Bastion placed on fallback attempt %d at (%d,%d)", attempts + 1, y, x);
                genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) on fallback %d",
                    v_name + qv_ptr->name, y, x, attempts + 1);
                return true;
            }
        }

        if (place_room_forced_exhaustive(qv_ptr, &y, &x))
        {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_varda_shadow_change = true;
            pending_quest_states.varda_shadow_level = p_ptr->depth;
            pending_quest_states.varda_shadow_y = y;
            pending_quest_states.varda_shadow_x = x;
            log_trace("Varda quest: Shadow Bastion placed by exhaustive scan at (%d,%d)", y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) after full-map scan",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        log_trace("Varda quest: Shadow Bastion placement failed after all attempts, returning false");
        genlog_quest("QUEST VAULT FAILED: '%s' could not be placed",
            v_name + qv_ptr->name);
        return false;
    }

    log_trace("Varda quest: Failed to find Shadow Bastion vault template at depth %d", p_ptr->depth);
    return false;
}

bool try_quest_vault_type(int v_type, bool *had_eligible_candidate)
{
    int i;
    vault_type* qv_ptr;
    int y, x;
    bool attempted_placement = false;

    if (had_eligible_candidate)
        *had_eligible_candidate = false;

    log_trace("Quest vault: Attempting type %d quest vault with forced placement strategy", v_type);
    
    for (i = 0; i < z_info->v_max; i++)
    {
        bool reserve_slot_for_this = true;
        qv_ptr = &v_info[i];
        if (qv_ptr->typ != v_type) continue;
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;
        if (!quest_vault_surface_roll_allows(qv_ptr, p_ptr->depth)) continue;
        if (vault_template_has_duruin(qv_ptr) || vault_template_has_shadow_bastion(qv_ptr) || vault_template_is_orc_stronghold(qv_ptr)) {
            log_trace("Quest vault: Skipping bespoke quest vault in generic placement path");
            continue;
        }
        
        log_trace("Quest vault: Checking vault %d '%s' (rarity=%d)", i, v_name + qv_ptr->name, qv_ptr->rarity);
        
        /* Once the quest-vault roll has committed this level to quest content,
         * still honor SURFACE weighting, but do not re-gate by template rarity. */
        
        /* Check Aule requirements */
        if (vault_template_has_aule(qv_ptr)) {
            log_trace("Quest vault: === AULE VAULT DETECTED === Checking eligibility (depth=%d)", p_ptr->depth);
            log_trace("Quest vault: CRITICAL CHECK - quest_reserved[0]=%d (MUST be 0 to proceed)", p_ptr->quest_reserved[0]);
            log_trace("  Player SMT skill_base = %d", p_ptr->skill_base[S_SMT]);
            log_trace("  Player SMT skill_use = %d", p_ptr->skill_use[S_SMT]);
            
            /* Use data-driven eligibility check from quest.txt E: field */
            if (!check_quest_eligibility(2, p_ptr->depth)) { /* Aule is quest index 2 */
                log_trace("Quest vault: Aule vault skipped (eligibility check failed)");
                continue;
            }
            log_trace("Quest vault: Aule eligibility check PASSED");
            
            if (quest_metarun_blocked(QUEST_ID_AULE, METARUN_QUEST_AULE)) {
                log_trace("Quest vault: Aule vault skipped (quest blocked by metarun)");
                continue;
            }
            if (p_ptr->quest_reserved[0]) {
                log_trace("Quest vault: === AULE BLOCKED === Another quest already spawned (quest_reserved[0]=1)");
                continue;
            }
            log_trace("Quest vault: === AULE APPROVED === All checks passed, proceeding with generation");
        }
        
        /* Check Mandos requirements */
        if (vault_template_has_mandos(qv_ptr)) {
            bool is_easterling = is_easterling_quest_vault(qv_ptr);
            bool is_maeglin = is_maeglin_quest_vault(qv_ptr);
            int mandos_stage = is_maeglin ? 3 : (is_easterling ? 2 : 1);
            int mandos_quest_id = (mandos_stage == 3) ? QUEST_ID_MANDOS_BETRAYER :
                                  (mandos_stage == 2) ? QUEST_ID_MANDOS_TRAITOR : QUEST_ID_MANDOS;
            u32b mandos_flag = quest_metarun_flag(mandos_quest_id);
            byte mandos_state = (mandos_stage == 1) ? p_ptr->mandos_quest : quest_get_state(mandos_quest_id);
            bool uses_reserve = (mandos_stage < 3);

            log_trace("Quest vault: Checking Mandos vault '%s' - quest_id=%d stage=%d state=%d, quest_reserved[0]=%d", 
                     v_name + qv_ptr->name, mandos_quest_id, mandos_stage, mandos_state, p_ptr->quest_reserved[0]);
            if (mandos_stage == 2 && (p_ptr->depth < 10 || p_ptr->depth > 13)) {
                log_trace("Quest vault: Mandos second quest '%s' skipped - depth %d outside 10-13", v_name + qv_ptr->name, p_ptr->depth);
                continue;
            }
            if (mandos_stage == 3 && (p_ptr->depth < 17 || p_ptr->depth > 19)) {
                log_trace("Quest vault: Mandos third quest '%s' skipped - depth %d outside 17-19", v_name + qv_ptr->name, p_ptr->depth);
                continue;
            }
            if (mandos_state != QUEST_STATE_NOT_STARTED) {
                log_trace("Quest vault: Mandos vault skipped (quest state %d)", mandos_state);
                continue;
            }
            if (mandos_stage == 2 && !mandos_second_stage_ready()) {
                log_trace("Quest vault: Mandos second quest skipped (requirements not met)");
                continue;
            }
            if (mandos_stage == 3 && !mandos_third_stage_ready()) {
                log_trace("Quest vault: Mandos third quest skipped (requirements not met)");
                continue;
            }
            if (mandos_stage == 1 && metarun_quest_completion_count(METARUN_QUEST_MANDOS) != 0) {
                log_trace("Quest vault: Mandos first quest skipped - later quest is pending");
                continue;
            }
            if (quest_metarun_blocked(mandos_quest_id, mandos_flag)) {
                log_trace("Quest vault: Mandos vault skipped (quest blocked by metarun)");
                continue;
            }
            if (mandos_stage == 2) {
                monster_race *ulf = &r_info[R_IDX_ULFANG];
                monster_race *uld = &r_info[R_IDX_ULDOR];
                if (ulf->max_num == 0 || uld->max_num == 0 || ulf->cur_num > 0 || uld->cur_num > 0) {
                    log_trace("Quest vault: Mandos second quest skipped - traitors unavailable (ulf max=%d cur=%d, uld max=%d cur=%d)",
                              ulf->max_num, ulf->cur_num, uld->max_num, uld->cur_num);
                    continue;
                }
            }
            if (mandos_stage == 3) {
                monster_race *mae = &r_info[R_IDX_MAEGLIN];
                if (mae->max_num == 0 || mae->cur_num > 0) {
                    log_trace("Quest vault: Mandos third quest skipped - Maeglin unavailable (max=%d cur=%d)", mae->max_num, mae->cur_num);
                    continue;
                }
            }
            if (uses_reserve && p_ptr->quest_reserved[0]) {
                log_trace("Quest vault: Mandos vault skipped (another quest already spawned this run)");
                continue;
            }
            pending_quest_states.mandos_quest_id = mandos_quest_id;
            pending_quest_states.mandos_next_state = QUEST_STATE_GIVER_PRESENT;
            reserve_slot_for_this = uses_reserve;
        }

        attempted_placement = true;
        if (had_eligible_candidate)
            *had_eligible_candidate = true;
        level_gen_debug_note_quest_vault_name(v_name + qv_ptr->name);

        /* Use forced placement strategy like forge placement:
         * Pick optimal location near center and use reduced padding */
        
        /* Calculate optimal placement position (center of map with some variation) */
        int center_y = p_ptr->cur_map_hgt / 2;
        int center_x = p_ptr->cur_map_wid / 2;
        
        /* Add some randomness but keep near center for best chance of success */
        y = center_y + rand_range(-p_ptr->cur_map_hgt/6, p_ptr->cur_map_hgt/6);
        x = center_x + rand_range(-p_ptr->cur_map_wid/6, p_ptr->cur_map_wid/6);
        
        /* Ensure within reasonable bounds */
        y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
        x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));
        
        log_trace("Quest vault: Attempting forced placement of '%s' at optimal location (%d,%d) (center: %d,%d)", 
                 v_name + qv_ptr->name, y, x, center_y, center_x);
        
        if (place_room_forced(y, x, qv_ptr)) {
            /* Mark that quest vault was placed in this attempt */
            qv_placed_this_level = true;  /* Track for integrity checks */
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
             
            /* DEBUGGING: Verify vault actually exists at coordinates immediately after placement */
            int y1 = y - qv_ptr->hgt / 2;
            int x1 = x - qv_ptr->wid / 2;
            int y2 = y1 + qv_ptr->hgt - 1;
            int x2 = x1 + qv_ptr->wid - 1;
            
            int verify_walls = 0, verify_floors = 0, verify_features = 0, verify_monsters = 0;
            int verify_icky = 0, verify_room = 0;
            
            for (int vy = y1; vy <= y2; vy++) {
                for (int vx = x1; vx <= x2; vx++) {
                    if (cave_feat[vy][vx] == FEAT_WALL_OUTER || cave_feat[vy][vx] == FEAT_WALL_INNER) {
                        verify_walls++;
                    } else if (cave_feat[vy][vx] == FEAT_FLOOR) {
                        verify_floors++;
                    } else if (cave_feat[vy][vx] != FEAT_WALL_EXTRA) {
                        verify_features++;
                    }
                    
                    if (cave_m_idx[vy][vx] > 0) {
                        verify_monsters++;
                    }
                    
                    if (cave_info[vy][vx] & CAVE_ICKY) {
                        verify_icky++;
                    }
                    
                    if (cave_info[vy][vx] & CAVE_ROOM) {
                        verify_room++;
                    }
                }
            }
            
            log_trace("VAULT VERIFICATION IMMEDIATELY AFTER PLACEMENT: Area (%d,%d) to (%d,%d)", 
                      y1, x1, y2, x2);
            log_trace("VAULT VERIFICATION: %d walls, %d floors, %d features, %d monsters", 
                      verify_walls, verify_floors, verify_features, verify_monsters);
            log_trace("VAULT VERIFICATION: %d CAVE_ICKY, %d CAVE_ROOM flags", 
                      verify_icky, verify_room);
            
            process_quest_vault_area(y, x, qv_ptr);
            if (reserve_slot_for_this) {
                p_ptr->quest_reserved[0] = 1;
            }
            log_trace("Quest vault: Type %d quest vault '%s' placed at (%d,%d) using forced strategy", 
                     v_type, v_name + qv_ptr->name, y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' type=%d at (%d,%d)",
                v_name + qv_ptr->name, v_type, y, x);
            return true;
        } else {
            log_trace("Quest vault: Failed to place vault '%s' at (%d,%d) even with forced strategy", 
                     v_name + qv_ptr->name, y, x);
            /* Try a few more strategic locations before giving up */
            for (int attempts = 0; attempts < 10; attempts++) {
                y = center_y + rand_range(-p_ptr->cur_map_hgt/4, p_ptr->cur_map_hgt/4);
                x = center_x + rand_range(-p_ptr->cur_map_wid/4, p_ptr->cur_map_wid/4);
                y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
                x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));
                
                if (place_room_forced(y, x, qv_ptr)) {
                    /* Mark that quest vault was placed in this attempt */
                    qv_placed_this_level = true;  /* Track for integrity checks */
                    level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
                     
                    /* DEBUGGING: Verify vault actually exists at coordinates immediately after placement */
                    int y1 = y - qv_ptr->hgt / 2;
                    int x1 = x - qv_ptr->wid / 2;
                    int y2 = y1 + qv_ptr->hgt - 1;
                    int x2 = x1 + qv_ptr->wid - 1;
                    
                    int verify_walls = 0, verify_floors = 0, verify_features = 0, verify_monsters = 0;
                    int verify_icky = 0, verify_room = 0;
                    
                    for (int vy = y1; vy <= y2; vy++) {
                        for (int vx = x1; vx <= x2; vx++) {
                            if (cave_feat[vy][vx] == FEAT_WALL_OUTER || cave_feat[vy][vx] == FEAT_WALL_INNER) {
                                verify_walls++;
                            } else if (cave_feat[vy][vx] == FEAT_FLOOR) {
                                verify_floors++;
                            } else if (cave_feat[vy][vx] != FEAT_WALL_EXTRA) {
                                verify_features++;
                            }
                            
                            if (cave_m_idx[vy][vx] > 0) {
                                verify_monsters++;
                            }
                            
                            if (cave_info[vy][vx] & CAVE_ICKY) {
                                verify_icky++;
                            }
                            
                            if (cave_info[vy][vx] & CAVE_ROOM) {
                                verify_room++;
                            }
                        }
                    }
                    
                    log_trace("VAULT VERIFICATION (FALLBACK) IMMEDIATELY AFTER PLACEMENT: Area (%d,%d) to (%d,%d)", 
                              y1, x1, y2, x2);
                    log_trace("VAULT VERIFICATION (FALLBACK): %d walls, %d floors, %d features, %d monsters", 
                              verify_walls, verify_floors, verify_features, verify_monsters);
                    log_trace("VAULT VERIFICATION (FALLBACK): %d CAVE_ICKY, %d CAVE_ROOM flags", 
                              verify_icky, verify_room);
                    
                    process_quest_vault_area(y, x, qv_ptr);
                    if (reserve_slot_for_this) {
                        p_ptr->quest_reserved[0] = 1;
                    }
                    log_trace("Quest vault: Type %d quest vault '%s' placed at (%d,%d) using fallback attempt %d", 
                             v_type, v_name + qv_ptr->name, y, x, attempts + 1);
                    genlog_quest("QUEST VAULT PLACED: '%s' type=%d at (%d,%d) on fallback %d",
                        v_name + qv_ptr->name, v_type, y, x, attempts + 1);
                    return true;
                }
            }

            log_trace("Quest vault: Random placement failed for '%s', scanning the full map for any valid fit",
                v_name + qv_ptr->name);
            if (place_room_forced_exhaustive(qv_ptr, &y, &x))
            {
                qv_placed_this_level = true;
                level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
                process_quest_vault_area(y, x, qv_ptr);
                if (reserve_slot_for_this) {
                    p_ptr->quest_reserved[0] = 1;
                }
                log_trace("Quest vault: Type %d quest vault '%s' placed at (%d,%d) by exhaustive scan",
                    v_type, v_name + qv_ptr->name, y, x);
                genlog_quest("QUEST VAULT PLACED: '%s' type=%d at (%d,%d) after full-map scan",
                    v_name + qv_ptr->name, v_type, y, x);
                return true;
            }

            genlog_quest("QUEST VAULT FAILED: '%s' type=%d could not be placed",
                v_name + qv_ptr->name, v_type);
        }
    }

    if (attempted_placement)
    {
        log_trace("Quest vault: Type %d had eligible templates, but none fit this attempt", v_type);
    }
    else
    {
        log_trace("Quest vault: Type %d has no eligible templates for this character/depth", v_type);
    }

    return false;
}

/*
 * Generate a new dungeon level
 */

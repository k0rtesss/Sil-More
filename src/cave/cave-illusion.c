#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "cave/cave.h"
#include "cave/cave-fixtures.h"
#include "level-generation/level-generation-terrain.h"
#include "level-generation/level-generation-terrain-access.h"
#include "level-generation/level-generation-terrain-vaults.h"

/* Session-only inspection overlay; never stored in saves or gameplay options. */
static bool illusion_debug_enabled;

bool cave_illusion_debug_enabled(void)
{
    return illusion_debug_enabled;
}

bool cave_illusion_debug_marked(int y, int x)
{
    return illusion_debug_enabled && p_ptr && cave_feat && in_bounds(y, x)
        && cave_feat[y][x] == FEAT_ILLUSORY_WALL;
}

void cave_illusion_debug_set(bool enabled)
{
    if (illusion_debug_enabled == enabled) return;
    illusion_debug_enabled = enabled;
#ifdef USE_SDL
    sdl_minimap_map_texture_cache_clear();
#endif
    if (!character_dungeon) return;
    p_ptr->redraw |= PR_MAP;
    p_ptr->window |= PW_OVERHEAD;
    /* The wall glyph is unchanged in tile mode: invalidate its cached pixels
     * and the side map on both enable and disable, even outside sight. */
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
            if (cave_feat[y][x] == FEAT_ILLUSORY_WALL)
                lite_spot(y, x);
}

/* Light only reveals the disguise while the player can actually see it. */
int cave_illusion_opacity(int y, int x)
{
    if (!in_bounds(y, x) || cave_feat[y][x] != FEAT_ILLUSORY_WALL
        || p_ptr->blind || !(cave_info[y][x] & CAVE_SEEN))
        return 255;
    return 255 - 30 * MIN(5, MAX(0, cave_light[y][x]));
}

void cave_dissolve_illusion(int y, int x)
{
    if (cave_feat[y][x] != FEAT_ILLUSORY_WALL) return;
    bool player_entered = p_ptr->py == y && p_ptr->px == x;
    if (player_entered)
    {
        msg_print(p_ptr->blind ? "The wall gives way without resistance."
                              : "The illusory wall dissolves as you pass through it.");
        disturb(0, 0);
    }
    else if (cave_info[y][x] & CAVE_SEEN)
        msg_print("An illusory wall dissolves.");
    cave_set_feat(y, x, FEAT_FLOOR);
#ifdef USE_SDL
    if (illusion_debug_enabled) sdl_minimap_map_texture_cache_clear();
#endif
    p_ptr->redraw |= PR_MAP;
    p_ptr->window |= PW_OVERHEAD;
}

static bool illusion_stone(int feat)
{
    return feat == FEAT_WALL_EXTRA || feat == FEAT_WALL_INNER
        || feat == FEAT_WALL_OUTER;
}

static bool illusion_corridor_floor(int y, int x)
{
    return in_bounds_fully(y, x) && cave_feat[y][x] == FEAT_FLOOR
        && !(cave_info[y][x] & CAVE_ROOM) && !cave_natural[y][x]
        && cave_corridor1[y][x] >= 0 && cave_corridor1[y][x] < DUN_ROOMS
        && cave_corridor2[y][x] >= 0 && cave_corridor2[y][x] < DUN_ROOMS;
}

/* Tunnel endpoints are not corridor cells themselves: they are converted to
 * doors before illusion placement runs. Record those doors against the room
 * pair whose corridor floor they touch. */
static void illusion_find_door_corridors(
    bool corridor_has_door[DUN_ROOMS][DUN_ROOMS])
{
    static const int dy[4] = { -1, 1, 0, 0 };
    static const int dx[4] = { 0, 0, -1, 1 };

    memset(corridor_has_door, 0,
        sizeof(bool) * DUN_ROOMS * DUN_ROOMS);
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            int r1, r2, low, high;
            if (!illusion_corridor_floor(y, x)) continue;
            r1 = cave_corridor1[y][x];
            r2 = cave_corridor2[y][x];
            low = MIN(r1, r2);
            high = MAX(r1, r2);
            for (int i = 0; i < 4; i++)
                if (feature_is_any_door(cave_feat[y + dy[i]][x + dx[i]]))
                {
                    corridor_has_door[low][high] = true;
                    break;
                }
        }
}

/* Leave the footprint and perimeter of authored features intact. */
static bool illusion_clearance(int y, int x, int partition)
{
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
        {
            int yy = y + dy, xx = x + dx;
            if ((cave_info[yy][xx] & (CAVE_ICKY | CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
                || terrain_vault_instance_at(yy, xx) >= 0
                || terrain_generation_reserved(yy, xx)
                || cave_fixture_at(yy, xx) || cave_m_idx[yy][xx]
                || cave_o_idx[yy][xx]
                || cave_feat[yy][xx] == FEAT_ILLUSORY_WALL
                || level_partition_index_for_point(yy, xx) != partition)
                return false;
        }
    return true;
}

#define ILLUSION_MIN_LENGTH 4 /* Two mouths with at least two hidden floor tiles. */
#define ILLUSION_MAX_LENGTH 8

/* Trace out from the side of a corridor. Every rock tile needs solid flanks
 * so the new passage can only be entered through its disguised ends. */
static int illusion_passage_length(int y, int x, int dy, int dx,
    const byte exterior[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    if (!illusion_corridor_floor(y + dx, x + dy)
        || !illusion_corridor_floor(y - dx, x - dy))
        return 0;

    int partition = level_partition_index_for_point(y, x);
    for (int length = 1; length <= ILLUSION_MAX_LENGTH; length++)
    {
        y += dy;
        x += dx;
        if (!in_bounds_fully(y, x) || (cave_info[y][x] & CAVE_ROOM)
            || !illusion_stone(cave_feat[y][x])
            || !illusion_stone(cave_feat[y + dx][x + dy])
            || !illusion_stone(cave_feat[y - dx][x - dy])
            || !illusion_clearance(y, x, partition))
            return 0;

        int far_y = y + dy, far_x = x + dx;
        if (cave_feat[far_y][far_x] == FEAT_FLOOR)
        {
            if (length < ILLUSION_MIN_LENGTH || !exterior[far_y][far_x]
                || !illusion_corridor_floor(far_y, far_x))
                return 0;
            for (int i = 0; i < 4; i++)
                if (illusion_corridor_floor(far_y + ddy_ddd[i], far_x + ddx_ddd[i]))
                    return length;
            return 0;
        }
    }
    return 0;
}

static bool vault_entrance_candidate(int y, int x, int instance)
{
    int feat = cave_feat[y][x];
    if ((feat != FEAT_FLOOR && !feature_is_any_door(feat))
        || cave_m_idx[y][x] || cave_o_idx[y][x] || cave_fixture_at(y, x)
        || terrain_generation_reserved(y, x))
        return false;

    /* Use a threshold with solid sides and a plain floor on each side of the
     * entrance. The outside landing must be separate from any authored vault. */
    static const int dy[] = { -1, 1, 0, 0 };
    static const int dx[] = { 0, 0, -1, 1 };
    for (size_t d = 0; d < N_ELEMENTS(dy); d++)
    {
        int in_y = y + dy[d], in_x = x + dx[d];
        int out_y = y - dy[d], out_x = x - dx[d];
        if (terrain_vault_instance_at(in_y, in_x) == instance
            && terrain_vault_instance_at(out_y, out_x) < 0
            && cave_feat[in_y][in_x] == FEAT_FLOOR
            && cave_feat[out_y][out_x] == FEAT_FLOOR
            && !(cave_info[out_y][out_x] & (CAVE_ICKY | CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
            && FEAT_IS_ROCK(cave_feat[y + dx[d]][x + dy[d]])
            && FEAT_IS_ROCK(cave_feat[y - dx[d]][x - dy[d]]))
            return true;
    }
    return false;
}

static bool vault_barrier(int y, int x)
{
    int feat = cave_feat[y][x];
    return FEAT_IS_GRANITE(feat) || feat == FEAT_WALL_PERM;
}

static bool vault_has_only_entrance(int entrance_y, int entrance_x, int instance)
{
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (terrain_vault_instance_at(y, x) != instance) continue;
            if ((y == p_ptr->py && x == p_ptr->px) || cave_stair_bold(y, x)
                || cave_forge_bold(y, x)
                || (cave_info[y][x] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)))
                return false;
            if ((y == entrance_y && x == entrance_x) || vault_barrier(y, x))
                continue;

            /* Every other crossing of the footprint must have solid stone
             * on one side. Treat hazards and breakable terrain as open so
             * they cannot act as a second entrance. */
            for (int d = 0; d < 8; d++)
            {
                int ny = y + ddy_ddd[d], nx = x + ddx_ddd[d];
                if (terrain_vault_instance_at(ny, nx) != instance
                    && !vault_barrier(ny, nx))
                    return false;
            }
        }
    return true;
}

static bool place_illusory_vault_entrance(
    const byte exterior[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    static byte after[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static byte blocked[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    int candidates = 0, chosen_y = 0, chosen_x = 0;

    for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++)
        for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
        {
            int id = terrain_vault_id_at(y, x);
            if (!exterior[y][x] || id < 0 || v_info[id].typ != 7
                || (v_info[id].flags & (VLT_QUEST | VLT_SURFACE)))
                continue;
            int instance = terrain_vault_instance_at(y, x);
            if (!vault_entrance_candidate(y, x, instance)
                || !vault_has_only_entrance(y, x, instance))
                continue;

            /* The entrance can touch multiple exterior routes. Block it in
             * a shadow map and make sure those routes still reach each other. */
            memcpy(blocked, cave_feat, sizeof(blocked));
            blocked[y][x] = FEAT_WALL_EXTRA;
            terrain_generation_flood(p_ptr->py, p_ptr->px, after, blocked);
            bool preserves_access = true;
            for (int yy = 1; preserves_access && yy < p_ptr->cur_map_hgt - 1; yy++)
                for (int xx = 1; xx < p_ptr->cur_map_wid - 1; xx++)
                    if (exterior[yy][xx] && !after[yy][xx]
                        && terrain_vault_instance_at(yy, xx) != instance)
                    {
                        preserves_access = false;
                        break;
                    }
            if (preserves_access && rand_int(++candidates) == 0)
            {
                chosen_y = y;
                chosen_x = x;
            }
        }
    if (!candidates) return false;
    cave_set_feat(chosen_y, chosen_x, FEAT_ILLUSORY_WALL);
    return true;
}

void place_illusory_passages(void)
{
    /* Keep the surface, throne-room, and Utumno layouts authored. */
    if (!illusory_walls || p_ptr->depth <= 0 || p_ptr->depth >= MORGOTH_DEPTH)
        return;
    static byte exterior[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    terrain_generation_flood(p_ptr->py, p_ptr->px, exterior, NULL);
    int placed = place_illusory_vault_entrance(exterior) ? 1 : 0;
    int map_size = (p_ptr->cur_map_hgt + p_ptr->cur_map_wid) / 2;
    /* Scale the count with the generated level, like other per-level terrain
     * populations: two walls on a 66x66 level, eight on a 165x165 level. */
    int target = 2 + ((map_size - 66) * 6) / 99;
    target = MAX(2, target);
    bool corridor_has_door[DUN_ROOMS][DUN_ROOMS];
    illusion_find_door_corridors(corridor_has_door);
    while (placed + 2 <= target)
    {
        int candidates = 0, chosen_y = 0, chosen_x = 0;
        int chosen_dir = 0, chosen_length = 0;
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
            for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
            {
                if (!exterior[y][x] || !illusion_corridor_floor(y, x)) continue;
                int r1 = cave_corridor1[y][x], r2 = cave_corridor2[y][x];
                if (corridor_has_door[MIN(r1, r2)][MAX(r1, r2)]) continue;

                for (int i = 0; i < 4; i++)
                {
                    int length = illusion_passage_length(y, x, ddy_ddd[i], ddx_ddd[i], exterior);
                    if (!length) continue;
                    if (rand_int(++candidates) == 0)
                    {
                        chosen_y = y;
                        chosen_x = x;
                        chosen_dir = i;
                        chosen_length = length;
                    }
                }
            }
        if (!candidates) break;
        for (int i = 1; i <= chosen_length; i++)
        {
            chosen_y += ddy_ddd[chosen_dir];
            chosen_x += ddx_ddd[chosen_dir];
            bool entrance = i == 1 || i == chosen_length;
            cave_set_feat(chosen_y, chosen_x, entrance ? FEAT_ILLUSORY_WALL : FEAT_FLOOR);
            terrain_generation_reserve(chosen_y, chosen_x);
            if (entrance) placed++;
        }
    }
    log_debug("Placed %d illusory walls at depth %d", placed, p_ptr->depth);
}

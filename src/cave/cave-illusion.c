#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "cave/cave.h"
#include "cave/cave-fixtures.h"

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
        && (cave_corridor1[y][x] >= 0 || cave_corridor2[y][x] >= 0);
}

static bool illusion_corridor_id_valid(int id)
{
    return id >= 0 && id < DUN_ROOMS;
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
            if (!illusion_corridor_id_valid(r1)
                || !illusion_corridor_id_valid(r2))
                continue;
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

/* A candidate is the near wall of a planned, doorless corridor. Walk through
 * the corridor width in the direction of its paired wall, so widened
 * corridors still contribute exactly one wall pair. */
static bool illusion_corridor_side(
    int y, int x, int side_dy, int side_dx,
    bool corridor_has_door[DUN_ROOMS][DUN_ROOMS])
{
    int source_y = y + side_dy;
    int source_x = x + side_dx;
    int axis_dy = side_dx;
    int axis_dx = -side_dy;
    int pair_y, pair_x;
    int width = 0;
    int partition = level_partition_index_for_point(y, x);
    int r1, r2, low, high;

    if (!illusion_corridor_floor(source_y, source_x)) return false;
    r1 = cave_corridor1[source_y][source_x];
    r2 = cave_corridor2[source_y][source_x];
    if (!illusion_corridor_id_valid(r1)
        || !illusion_corridor_id_valid(r2))
        return false;
    low = MIN(r1, r2);
    high = MAX(r1, r2);
    if (corridor_has_door[low][high]) return false;

    /* Do not treat a room edge or a turn as a corridor side. */
    if (!illusion_corridor_floor(source_y + axis_dy, source_x + axis_dx)
        || !illusion_corridor_floor(source_y - axis_dy, source_x - axis_dx))
        return false;

    pair_y = source_y + side_dy;
    pair_x = source_x + side_dx;
    while (width < 3 && illusion_corridor_floor(pair_y, pair_x))
    {
        pair_y += side_dy;
        pair_x += side_dx;
        width++;
    }

    if (!in_bounds_fully(pair_y, pair_x)
        || !illusion_stone(cave_feat[pair_y][pair_x])
        || cave_fixture_at(pair_y, pair_x) != CAVE_FIXTURE_NONE
        || (cave_info[pair_y][pair_x]
            & (CAVE_ICKY | CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
        || level_partition_index_for_point(pair_y, pair_x) != partition)
        return false;

    return true;
}

/* Only pierce the side of a doorless planned corridor, within one partition.
 * Protected rooms, their perimeter, fixtures, and permanent walls are
 * intact. */
static bool illusion_candidate(
    int y, int x, bool corridor_has_door[DUN_ROOMS][DUN_ROOMS])
{
    if (!illusion_stone(cave_feat[y][x])) return false;
    int partition = level_partition_index_for_point(y, x);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
        {
            int yy = y + dy, xx = x + dx;
            if ((cave_info[yy][xx] & (CAVE_ICKY | CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
                || cave_fixture_at(yy, xx) || cave_m_idx[yy][xx]
                || cave_o_idx[yy][xx]
                || cave_feat[yy][xx] == FEAT_ILLUSORY_WALL
                || level_partition_index_for_point(yy, xx) != partition)
                return false;
        }
    return illusion_corridor_side(y, x, -1, 0, corridor_has_door)
        || illusion_corridor_side(y, x, 1, 0, corridor_has_door)
        || illusion_corridor_side(y, x, 0, -1, corridor_has_door)
        || illusion_corridor_side(y, x, 0, 1, corridor_has_door);
}

void place_illusory_passages(void)
{
    /* Keep the surface, throne-room, and Utumno layouts authored. */
    if (!illusory_walls || p_ptr->depth <= 0 || p_ptr->depth >= MORGOTH_DEPTH)
        return;
    int placed = 0;
    int map_size = (p_ptr->cur_map_hgt + p_ptr->cur_map_wid) / 2;
    /* Scale the count with the generated level, like other per-level terrain
     * populations: two walls on a 66x66 level, eight on a 165x165 level. */
    int target = 2 + ((map_size - 66) * 6) / 99;
    target = MAX(2, target);
    bool corridor_has_door[DUN_ROOMS][DUN_ROOMS];
    illusion_find_door_corridors(corridor_has_door);
    for (int n = 0; n < target; n++)
    {
        int candidates = 0, chosen_y = 0, chosen_x = 0;
        for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++)
            for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
                if (illusion_candidate(y, x, corridor_has_door)
                    && rand_int(++candidates) == 0)
                {
                    chosen_y = y;
                    chosen_x = x;
                }
        if (!candidates) break;
        cave_set_feat(chosen_y, chosen_x, FEAT_ILLUSORY_WALL);
        placed++;
    }
    log_debug("Placed %d illusory passages at depth %d", placed, p_ptr->depth);
}

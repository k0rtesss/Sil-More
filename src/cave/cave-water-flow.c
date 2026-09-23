/* File: cave-water-flow.c */

#include "angband.h"
#include "externs.h"
#include "cave/cave-water-flow.h"
#include "cave/cave-bridge.h"
#include "level-generation/level-generation-landmarks.h"
#include "level-generation/level-generation-terrain-history.h"
#include "level-generation/level-generation-themes.h"

#define WATER_FLOW_LAKE_BIT 0x08
#define WATER_FLOW_DIRECTION_MASK 0x07
#define WATER_FLOW_CELLS (MAX_DUNGEON_HGT * MAX_DUNGEON_WID)

static byte flow_direction[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte flow_lake[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte topology_basin[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte topology_role[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte topology_channel[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte generation_feature[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte generation_basin[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte generation_channel[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte generation_outlet[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte component_seen[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int flow_distance[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int flow_channel_distance[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int flow_channel_owner[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int flow_queue[WATER_FLOW_CELLS];
static int flow_component[WATER_FLOW_CELLS];
static bool flow_valid;

static bool map_coord(int y, int x)
{
    return p_ptr && y >= 0 && x >= 0
        && y < p_ptr->cur_map_hgt && x < p_ptr->cur_map_wid;
}

static int flow_feature(int feat)
{
    int underlay = cave_bridge_underlay(feat);
    if (underlay == FEAT_WATER || underlay == FEAT_DEEP_WATER)
        return FEAT_WATER;
    if (underlay == FEAT_POISON)
        return FEAT_POISON;
    return FEAT_NONE;
}

static bool liquid_feature(int feat)
{
    return flow_feature(feat) != FEAT_NONE;
}

static bool liquid_cell(int y, int x)
{
    return map_coord(y, x) && liquid_feature(cave_feat[y][x]);
}

void cave_water_flow_reset(void)
{
    memset(flow_direction, 0, sizeof(flow_direction));
    memset(flow_lake, 0, sizeof(flow_lake));
    memset(topology_basin, 0, sizeof(topology_basin));
    memset(topology_role, 0, sizeof(topology_role));
    memset(topology_channel, 0, sizeof(topology_channel));
    memset(component_seen, 0, sizeof(component_seen));
    flow_valid = false;
}

void cave_water_flow_invalidate(void)
{
    /* Keep this cheap: feature edits are common during generation, while the
     * final graph is built once after all terrain edits have settled. */
    flow_valid = false;
}

void cave_water_flow_generation_plan_reset(void)
{
    memset(generation_feature, 0, sizeof(generation_feature));
    memset(generation_basin, 0, sizeof(generation_basin));
    memset(generation_channel, 0, sizeof(generation_channel));
    memset(generation_outlet, 0, sizeof(generation_outlet));
}

void cave_water_flow_generation_plan_cell(int y, int x, int feature,
    bool channel, bool basin, bool outlet)
{
    if (!map_coord(y, x)
        || (feature != FEAT_WATER && feature != FEAT_POISON)) return;
    generation_feature[y][x] = (byte)feature;
    generation_channel[y][x] |= channel;
    generation_basin[y][x] |= basin;
    generation_outlet[y][x] |= outlet;
}

void cave_water_flow_invalidate_at(int y, int x)
{
    if (!map_coord(y, x)) return;
    /* The caller invokes this when the material at this cell changes.  Clear
     * the cell even when the replacement is another flow liquid, so a stale
     * water arrow cannot survive a water/acid conversion. */
    flow_direction[y][x] = CAVE_WATER_FLOW_CALM;
    flow_lake[y][x] = 0;
    /* Clear only upstream arrows which pointed into the changed cell;
     * unrelated rivers remain readable during a live terrain edit. */
    for (int d = 0; d < 4; d++)
    {
        int ny = y + (d == 0 ? -1 : d == 2 ? 1 : 0);
        int nx = x + (d == 1 ? 1 : d == 3 ? -1 : 0);
        if (map_coord(ny, nx)) {
            int expected = ny < y ? CAVE_WATER_FLOW_SOUTH
                : ny > y ? CAVE_WATER_FLOW_NORTH
                : nx < x ? CAVE_WATER_FLOW_EAST : CAVE_WATER_FLOW_WEST;
            if (flow_direction[ny][nx] == expected) {
                flow_direction[ny][nx] = CAVE_WATER_FLOW_CALM;
                flow_lake[ny][nx] = 0;
            }
        }
    }
}

void cave_water_flow_set(int y, int x, int direction)
{
    if (!map_coord(y, x)) return;
    if (direction < CAVE_WATER_FLOW_CALM || direction > CAVE_WATER_FLOW_WEST)
        direction = CAVE_WATER_FLOW_CALM;
    flow_direction[y][x] = (byte)direction;
    flow_lake[y][x] = 0;
    flow_valid = true;
}

int cave_water_flow_direction(int y, int x)
{
    return flow_valid && map_coord(y, x) ? flow_direction[y][x] : CAVE_WATER_FLOW_CALM;
}

bool cave_water_flow_is_lake(int y, int x)
{
    return flow_valid && map_coord(y, x) && flow_lake[y][x];
}

bool cave_water_flow_is_valid(void)
{
    return flow_valid;
}

byte cave_water_flow_encoded_at(int y, int x)
{
    if (!flow_valid || !map_coord(y, x)) return 0;
    return (byte)((flow_direction[y][x] & WATER_FLOW_DIRECTION_MASK)
        | (flow_lake[y][x] ? WATER_FLOW_LAKE_BIT : 0));
}

void cave_water_flow_restore_begin(void)
{
    cave_water_flow_reset();
}

bool cave_water_flow_restore_cell(int y, int x, byte encoded)
{
    if (!map_coord(y, x) || (encoded & ~((byte)(WATER_FLOW_LAKE_BIT
        | WATER_FLOW_DIRECTION_MASK)))
        || (encoded & WATER_FLOW_DIRECTION_MASK) > CAVE_WATER_FLOW_WEST)
        return false;
    flow_direction[y][x] = encoded & WATER_FLOW_DIRECTION_MASK;
    flow_lake[y][x] = (encoded & WATER_FLOW_LAKE_BIT) != 0;
    return true;
}

void cave_water_flow_restore_finish(void)
{
    flow_valid = true;
}

static int flow_material_feature(int material)
{
    /* Terrain material IDs are shared by the landmark and history planners.
     * Only freshwater and poison have a moving surface; chasms, lava, and ice
     * retain their existing visual behaviour. */
    if (material == TERRAIN_THEME_WATER) return FEAT_WATER;
    if (material == TERRAIN_THEME_POISON) return FEAT_POISON;
    return FEAT_NONE;
}

static void gather_topology_system(int system,
    const terrain_landmark_stats* stats)
{
    int feature;
    if (!stats || !stats->accepted
        || (feature = flow_material_feature(stats->material)) == FEAT_NONE)
        return;
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (flow_feature(cave_feat[y][x]) != feature) continue;
            if (terrain_landmark_system_cell(system, y, x, 1))
                topology_basin[y][x] = 1;
            if (terrain_landmark_system_cell(system, y, x, 2))
                topology_channel[y][x] = 1;
            if (terrain_landmark_system_cell(system, y, x, 5) & 3)
                topology_role[y][x] |=
                    (byte)(terrain_landmark_system_cell(system, y, x, 5) & 3);
        }
}

/* Copy the generation-only graph into a compact, level-owned form.  Historical
 * systems retain their plans after realization, so all of them are considered;
 * the ordinary one-shot landmark uses the live arrays instead.  Water and
 * poisonous acid share the graph format, but their connected components stay
 * separate. */
static void gather_generation_topology(void)
{
    memset(topology_basin, 0, sizeof(topology_basin));
    memset(topology_role, 0, sizeof(topology_role));
    memset(topology_channel, 0, sizeof(topology_channel));

    int histories = terrain_history_count();
    if (histories > 0)
    {
        for (int s = 0; s < histories; s++)
            gather_topology_system(s, terrain_landmark_system_stats(s));
        return;
    }

    const terrain_landmark_stats* stats = terrain_landmark_last_stats();
    int feature = stats ? flow_material_feature(stats->material) : FEAT_NONE;
    if (!stats || !stats->accepted || feature == FEAT_NONE) return;
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (flow_feature(cave_feat[y][x]) != feature) continue;
            if (terrain_landmark_basin_cell(y, x)) topology_basin[y][x] = 1;
            if (terrain_landmark_channel_cell(y, x)) topology_channel[y][x] = 1;
            int role = terrain_landmark_terminal_role(y, x);
            if (role & 3) topology_role[y][x] |= (byte)(role & 3);
    }
}

static void merge_generation_plan(void)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            int feature = generation_feature[y][x];
            if (!feature || flow_feature(cave_feat[y][x]) != feature) continue;
            if (generation_basin[y][x]) topology_basin[y][x] = 1;
            if (generation_channel[y][x]) topology_channel[y][x] = 1;
            if (generation_outlet[y][x]) topology_role[y][x] |= 2;
        }
}

static int opposite_direction(int dy, int dx)
{
    if (dy < 0) return CAVE_WATER_FLOW_NORTH;
    if (dx > 0) return CAVE_WATER_FLOW_EAST;
    if (dy > 0) return CAVE_WATER_FLOW_SOUTH;
    if (dx < 0) return CAVE_WATER_FLOW_WEST;
    return CAVE_WATER_FLOW_CALM;
}

static bool flow_direction_reaches_feature(int y, int x, int direction,
    int feature)
{
    int dy = direction == CAVE_WATER_FLOW_NORTH ? -1
        : direction == CAVE_WATER_FLOW_SOUTH ? 1 : 0;
    int dx = direction == CAVE_WATER_FLOW_EAST ? 1
        : direction == CAVE_WATER_FLOW_WEST ? -1 : 0;
    return direction > CAVE_WATER_FLOW_CALM
        && direction <= CAVE_WATER_FLOW_WEST
        && map_coord(y + dy, x + dx)
        && flow_feature(cave_feat[y + dy][x + dx]) == feature;
}

void cave_water_flow_build(void)
{
    cave_water_flow_reset();
    if (!p_ptr || p_ptr->cur_map_hgt <= 0 || p_ptr->cur_map_wid <= 0) {
        flow_valid = true;
        cave_water_flow_generation_plan_reset();
        return;
    }

    gather_generation_topology();
    merge_generation_plan();
    memset(component_seen, 0, sizeof(component_seen));
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!liquid_cell(y, x) || component_seen[y][x]) continue;
            int component_feature = flow_feature(cave_feat[y][x]);

            int head = 0, tail = 1;
            flow_queue[0] = y * MAX_DUNGEON_WID + x;
            component_seen[y][x] = 1;
            while (head < tail)
            {
                int id = flow_queue[head++];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                flow_component[head - 1] = id;
                for (int d = 0; d < 4; d++)
                {
                    int ny = cy + (d == 0 ? -1 : d == 2 ? 1 : 0);
                    int nx = cx + (d == 1 ? 1 : d == 3 ? -1 : 0);
                    if (!map_coord(ny, nx)
                        || flow_feature(cave_feat[ny][nx]) != component_feature
                        || component_seen[ny][nx]) continue;
                    component_seen[ny][nx] = 1;
                    if (tail < WATER_FLOW_CELLS)
                        flow_queue[tail++] = ny * MAX_DUNGEON_WID + nx;
                }
            }

            bool has_outlet = false;
            for (int i = 0; i < tail; i++)
            {
                int id = flow_component[i];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                if (topology_role[cy][cx] & 2) {
                    flow_distance[cy][cx] = 0;
                    flow_queue[i] = id;
                    has_outlet = true;
                } else {
                    flow_distance[cy][cx] = -1;
                }
                flow_lake[cy][cx] = topology_basin[cy][cx];
                flow_channel_distance[cy][cx] = -1;
                flow_channel_owner[cy][cx] = -1;
            }
            if (!has_outlet) continue;

            int seed_count = 0;
            /* Reuse flow_queue as the multi-source BFS queue. */
            for (int i = 0; i < tail; i++)
            {
                int id = flow_queue[i];
                if (flow_distance[id / MAX_DUNGEON_WID][id % MAX_DUNGEON_WID] == 0)
                    flow_queue[seed_count++] = id;
            }
            head = 0;
            int bfs_tail = seed_count;
            while (head < bfs_tail)
            {
                int id = flow_queue[head++];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                int next_distance = flow_distance[cy][cx] + 1;
                for (int d = 0; d < 4; d++)
                {
                    int ny = cy + (d == 0 ? -1 : d == 2 ? 1 : 0);
                    int nx = cx + (d == 1 ? 1 : d == 3 ? -1 : 0);
                    if (!map_coord(ny, nx)
                        || flow_feature(cave_feat[ny][nx]) != component_feature
                        || flow_distance[ny][nx] >= 0)
                        continue;
                    flow_distance[ny][nx] = next_distance;
                    if (bfs_tail < WATER_FLOW_CELLS) flow_queue[bfs_tail++] = ny * MAX_DUNGEON_WID + nx;
                }
            }

            for (int i = 0; i < tail; i++)
            {
                int id = flow_component[i];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                if (flow_lake[cy][cx] || flow_distance[cy][cx] <= 0) continue;
                for (int d = 0; d < 4; d++)
                {
                    int ny = cy + (d == 0 ? -1 : d == 2 ? 1 : 0);
                    int nx = cx + (d == 1 ? 1 : d == 3 ? -1 : 0);
                    if (liquid_cell(ny, nx)
                        && flow_feature(cave_feat[ny][nx]) == component_feature
                        && flow_distance[ny][nx] == flow_distance[cy][cx] - 1)
                    {
                        flow_direction[cy][cx] = (byte)opposite_direction(ny - cy, nx - cx);
                        break;
                }
            }

            /* A wide channel has several equally short routes to the outlet.
             * The full liquid BFS is useful as a fallback, but its tie-break
             * can make one lane turn across the river.  The generator keeps a
             * one-cell channel spine, so first solve flow on that spine and
             * then let each wider cell inherit its nearest spine direction. */
            int channel_count = 0, channel_outlets = 0;
            for (int i = 0; i < tail; i++)
            {
                int id = flow_component[i];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                if (!topology_channel[cy][cx]) continue;
                channel_count++;
                if (topology_role[cy][cx] & 2) channel_outlets++;
            }
            if (!channel_count || !channel_outlets) continue;

            int channel_head = 0, channel_tail = 0;
            for (int i = 0; i < tail; i++)
            {
                int id = flow_component[i];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                if (!topology_channel[cy][cx]
                    || !(topology_role[cy][cx] & 2)) continue;
                flow_channel_distance[cy][cx] = 0;
                flow_queue[channel_tail++] = id;
            }
            while (channel_head < channel_tail)
            {
                int id = flow_queue[channel_head++];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                int next_distance = flow_channel_distance[cy][cx] + 1;
                for (int d = 0; d < 4; d++)
                {
                    int ny = cy + (d == 0 ? -1 : d == 2 ? 1 : 0);
                    int nx = cx + (d == 1 ? 1 : d == 3 ? -1 : 0);
                    if (!map_coord(ny, nx)
                        || flow_feature(cave_feat[ny][nx]) != component_feature
                        || !topology_channel[ny][nx]
                        || flow_channel_distance[ny][nx] >= 0) continue;
                    flow_channel_distance[ny][nx] = next_distance;
                    flow_queue[channel_tail++] = ny * MAX_DUNGEON_WID + nx;
                }
            }

            for (int i = 0; i < tail; i++)
            {
                int id = flow_component[i];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                if (!topology_channel[cy][cx]
                    || flow_lake[cy][cx] || flow_channel_distance[cy][cx] <= 0)
                    continue;
                for (int d = 0; d < 4; d++)
                {
                    int ny = cy + (d == 0 ? -1 : d == 2 ? 1 : 0);
                    int nx = cx + (d == 1 ? 1 : d == 3 ? -1 : 0);
                    if (!map_coord(ny, nx)
                        || flow_feature(cave_feat[ny][nx]) != component_feature
                        || !topology_channel[ny][nx]
                        || flow_channel_distance[ny][nx]
                            != flow_channel_distance[cy][cx] - 1) continue;
                    flow_direction[cy][cx] =
                        (byte)opposite_direction(ny - cy, nx - cx);
                    break;
                }
            }

            /* Multi-source flood-fill from the spine.  The owner is a channel
             * cell, not the first direction encountered, so a two-cell-wide
             * horizontal/vertical river inherits one coherent current across
             * its width. */
            channel_head = channel_tail = 0;
            for (int i = 0; i < tail; i++)
            {
                int id = flow_component[i];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                if (!topology_channel[cy][cx]) continue;
                flow_channel_owner[cy][cx] = id;
                flow_queue[channel_tail++] = id;
            }
            while (channel_head < channel_tail)
            {
                int id = flow_queue[channel_head++];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                int owner = flow_channel_owner[cy][cx];
                for (int d = 0; d < 4; d++)
                {
                    int ny = cy + (d == 0 ? -1 : d == 2 ? 1 : 0);
                    int nx = cx + (d == 1 ? 1 : d == 3 ? -1 : 0);
                    if (!map_coord(ny, nx)
                        || flow_feature(cave_feat[ny][nx]) != component_feature
                        || flow_channel_owner[ny][nx] >= 0) continue;
                    flow_channel_owner[ny][nx] = owner;
                    flow_queue[channel_tail++] = ny * MAX_DUNGEON_WID + nx;
                }
            }
            for (int i = 0; i < tail; i++)
            {
                int id = flow_component[i];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                if (topology_channel[cy][cx] || flow_lake[cy][cx]
                    || flow_channel_owner[cy][cx] < 0) continue;
                int owner = flow_channel_owner[cy][cx];
                int oy = owner / MAX_DUNGEON_WID, ox = owner % MAX_DUNGEON_WID;
                int inherited = flow_lake[oy][ox]
                    ? CAVE_WATER_FLOW_CALM : flow_direction[oy][ox];
                if (inherited == CAVE_WATER_FLOW_CALM
                    || flow_direction_reaches_feature(cy, cx, inherited,
                        component_feature))
                    flow_direction[cy][cx] = inherited;
            }
        }
        }
    flow_valid = true;
    cave_water_flow_generation_plan_reset();
}

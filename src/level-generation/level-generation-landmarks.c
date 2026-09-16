#include "angband.h"
#include "level-generation/level-generation-internal.h"
#include "level-generation/level-generation-landmarks.h"
#include "level-generation/level-generation-terrain-vaults.h"
#include "level-generation/level-generation-terrain-access.h"
#include "level-generation/level-generation-terrain-structures.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-bridge.h"
#include "level-generation/level-generation-terrain-history.h"
#include <limits.h>

#define LM_CELLS (MAX_DUNGEON_HGT * MAX_DUNGEON_WID)
#define LM_PATH 1024
#define LM_ATTEMPTS 24
#define LM_BRIDGES 20
#define LM_MOVES 2048
#define LM_BASINS 6

typedef struct landmark_move { coord from, to; int monster, object; } landmark_move;
typedef struct landmark_bridge { coord first; int dy, dx, length; } landmark_bridge;
static const int lm_dy[4] = { -1, 0, 1, 0 };
static const int lm_dx[4] = { 0, 1, 0, -1 };
static const int lm_features[5] = { FEAT_WATER, FEAT_CHASM, FEAT_LAVA, FEAT_POISON, FEAT_ICE };
static byte lm_hard[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_built[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_work[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_live[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_shadow[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_taken[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_basin[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_live_basin[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_channel[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_live_channel[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_cavity[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_terminal[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_terminal_role[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_live_terminal[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_live_structure[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_hydraulic[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte lm_effect_hard[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static terrain_structure_stats lm_structures;
static coord lm_basins[LM_BASINS];
static coord lm_merges[8];
static int lm_basin_count, lm_family, lm_junctions, lm_bridge_savings;
static int lm_before[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int lm_after[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int lm_distance[LM_CELLS], lm_parent[LM_CELLS], lm_heap[LM_CELLS], lm_position[LM_CELLS];
static int lm_heap_count, lm_feature;
static coord lm_goal;
static coord lm_path[LM_PATH];
static int lm_path_count;
static landmark_bridge lm_bridges[LM_BRIDGES];
static int lm_bridge_count;
static landmark_move lm_moves[LM_MOVES];
static int lm_move_count;
static bool lm_partitions[25];
static terrain_landmark_stats lm_stats;
static bool lm_preparing, lm_preserve_form;
static int lm_epoch = TERRAIN_HISTORY_DISASTER;
typedef struct landmark_plan
{
    bool ready;
    terrain_landmark_stats stats;
    byte work[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte basin[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte channel[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte cavity[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte terminal[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte role[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte cap[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte structure[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte initial[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    coord basins[LM_BASINS], merges[8];
    int basin_count, junctions;
} landmark_plan;
static landmark_plan lm_plans[TERRAIN_HISTORY_SYSTEMS];
static int lm_system = -1;

/* Shares the local planner's reversible walking/one-tile-jump graph. */
extern int terrain_generation_components(
    const byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int labels[MAX_DUNGEON_HGT][MAX_DUNGEON_WID]);

void terrain_landmark_reset(void)
{
    memset(&lm_stats, 0, sizeof(lm_stats));
    memset(lm_live, 0, sizeof(lm_live));
    memset(lm_live_basin, 0, sizeof(lm_live_basin));
    memset(lm_live_channel, 0, sizeof(lm_live_channel));
    memset(lm_live_terminal, 0, sizeof(lm_live_terminal));
    memset(lm_live_structure, 0, sizeof(lm_live_structure));
    memset(lm_partitions, 0, sizeof(lm_partitions));
}

const terrain_landmark_stats* terrain_landmark_last_stats(void) { return &lm_stats; }
int terrain_landmark_cell(int y, int x)
{ return in_bounds_fully(y, x) ? lm_live[y][x] : TERRAIN_LANDMARK_NONE; }
int terrain_landmark_basin_cell(int y, int x)
{ return in_bounds_fully(y, x) ? lm_live_basin[y][x] : 0; }
bool terrain_landmark_channel_cell(int y, int x)
{ return in_bounds_fully(y, x) && lm_live_channel[y][x]; }
int terrain_landmark_terminal_cell(int y, int x)
{ return in_bounds_fully(y, x) ? lm_live_terminal[y][x] : 0; }
int terrain_landmark_terminal_role(int y, int x)
{ return in_bounds_fully(y, x) ? lm_terminal_role[y][x] : 0; }
int terrain_landmark_structure_cell(int y, int x)
{ return in_bounds_fully(y, x) ? lm_live_structure[y][x] : 0; }
bool terrain_landmark_partition(int pi)
{ return pi >= 0 && pi < 25 && lm_partitions[pi]; }

static bool lm_rock(int feat)
{
    return feat == FEAT_WALL_EXTRA || feat == FEAT_WALL_OUTER
        || feat == FEAT_WALL_INNER || feat == FEAT_WALL_SOLID || feat == FEAT_QUARTZ;
}

static bool lm_passage(int y, int x)
{
    if (!in_bounds_fully(y, x)) return false;
    int feat = cave_feat[y][x];
    if (lm_epoch != TERRAIN_HISTORY_DISASTER && feat == lm_feature
        && (cave_corridor1[y][x] || cave_corridor2[y][x])) return true;
    return feat == FEAT_FLOOR || cave_feat_is_bridge(feat) || feat == FEAT_OPEN || feat == FEAT_BROKEN
        || feat == FEAT_SECRET || feat == FEAT_RUBBLE
        || (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)
        || FEAT_IS_TRAP(feat);
}

static bool lm_critical_object(int y, int x)
{
    int remaining = o_max;
    for (int i = cave_o_idx[y][x]; i > 0 && remaining-- > 0; i = o_list[i].next_o_idx)
    {
        if (i >= o_max || o_list[i].name1 || o_list[i].tval == TV_NOTE) return true;
    }
    return remaining <= 0;
}

static void lm_context(void)
{
    memset(lm_hard, 0, sizeof(lm_hard));
    memset(lm_built, 0, sizeof(lm_built));
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            int f = cave_feat[y][x], pi = level_partition_index_for_point(y, x);
            unsigned policy = terrain_vault_policy_at(y, x);
            bool critical = f == FEAT_GLYPH || f == FEAT_WARDED || f == FEAT_WARDED2
                || f == FEAT_WARDED3 || f == FEAT_SUNLIGHT || f == FEAT_FORGE
                || (f >= FEAT_FORGE_HEAD && f <= FEAT_FORGE_TAIL)
                || (f >= FEAT_STAIR_HEAD && f <= FEAT_STAIR_TAIL)
                || lm_critical_object(y, x);
            int m = cave_m_idx[y][x];
            if (m > 0 && m < mon_max && mon_list[m].r_idx > 0
                && (r_info[mon_list[m].r_idx].flags1 & RF1_UNIQUE)) critical = true;
            if (!in_bounds_fully(y, x) || f == FEAT_WALL_PERM || f == FEAT_NONE
                || critical || (cave_info[y][x] & (CAVE_G_VAULT | CAVE_CHASM_AREA | CAVE_MORGOTH_TUNNEL))
                || coord_in_morgoth_region(y, x, 0)
                || ((cave_info[y][x] & CAVE_ICKY) && !policy)
                || (terrain_vault_id_at(y, x) >= 0 && !policy)) lm_hard[y][x] = 1;
            /* Mixing requires an authored boundary, not last-writer-wins paint. */
            if (lm_feature != FEAT_CHASM && f != lm_feature
                && (f == FEAT_WATER || f == FEAT_ICE || f == FEAT_LAVA || f == FEAT_POISON))
                lm_hard[y][x] = 1;
            /* Independently planned systems retain their own cores. A later
             * catastrophe cannot silently repaint an earlier river. */
            if (lm_system >= 0) for (int s = 0; s < TERRAIN_HISTORY_SYSTEMS; s++)
                if (s != lm_system && lm_plans[s].ready
                    && (lm_plans[s].work[y][x] == TERRAIN_LANDMARK_TERRAIN
                        || lm_plans[s].work[y][x] == TERRAIN_LANDMARK_BRIDGE
                        || lm_plans[s].work[y][x] == TERRAIN_LANDMARK_REPAIR
                        || lm_plans[s].cap[y][x])) lm_hard[y][x] = 1;
            if (cave_feat_is_bridge(f) && cave_bridge_underlay(f) != lm_feature) lm_hard[y][x] = 1;
            if (lm_system >= 0 && !lm_preparing && lm_epoch != TERRAIN_HISTORY_DISASTER
                && lm_plans[lm_system].cap[y][x]) lm_hard[y][x] = 1;
            /* Geology crosses partition lines, but water does not become an
             * unexplained river through the middle of an elemental fire cave. */
            if (pi >= 0 && pi < current_partition_count
                && current_partition_modes[pi] == QUAD_MODE_BIG_CAVE
                && (cave_info[y][x] & CAVE_ROOM) && lm_feature != FEAT_CHASM)
            {
                big_cave_type_t type = current_partition_big_cave_types[pi];
                if ((type == BIG_CAVE_FIRE && lm_feature != FEAT_LAVA)
                    || (type == BIG_CAVE_ICE && lm_feature != FEAT_ICE)
                    || (type == BIG_CAVE_POIS && lm_feature != FEAT_POISON)) lm_hard[y][x] = 1;
            }
            /* Actual constructed corridors remain candidate bridge axes. */
            if (lm_passage(y, x) && (!(cave_info[y][x] & CAVE_ROOM)
                || cave_corridor1[y][x] || cave_corridor2[y][x]
                || terrain_vault_id_at(y, x) >= 0)) lm_built[y][x] = 1;
            if (critical && in_bounds_fully(y, x))
                for (int d = 0; d < 4; d++) lm_hard[y + lm_dy[d]][x + lm_dx[d]] = 1;
        }
    for (int i = 0; i < dun->cent_n; i++)
    {
        rectangle b = dun->corner[i];
        if (room_anchor_kind[i] != LAYOUT_ANCHOR_CA_BLOB)
        {
            for (int y = b.y1; y <= b.y2; y++) for (int x = b.x1; x <= b.x2; x++)
                if (in_bounds_fully(y, x) && lm_passage(y, x)) lm_built[y][x] = 1;
        }
    }
}

static bool lm_allowed(int y, int x)
{ return in_bounds_fully(y, x) && !lm_hard[y][x]; }

static int lm_priority(int id)
{
    return lm_distance[id] + 10 * (ABS(id / MAX_DUNGEON_WID - lm_goal.y)
        + ABS(id % MAX_DUNGEON_WID - lm_goal.x));
}

static void lm_heap_up(int pos)
{
    int id = lm_heap[pos];
    while (pos > 0)
    {
        int parent = (pos - 1) / 2;
        if (lm_priority(lm_heap[parent]) <= lm_priority(id)) break;
        lm_heap[pos] = lm_heap[parent]; lm_position[lm_heap[pos]] = pos; pos = parent;
    }
    lm_heap[pos] = id; lm_position[id] = pos;
}

static int lm_heap_pop(void)
{
    int result = lm_heap[0], pos = 0, id = lm_heap[--lm_heap_count];
    lm_position[result] = -2;
    if (!lm_heap_count) return result;
    while (pos * 2 + 1 < lm_heap_count)
    {
        int child = pos * 2 + 1;
        if (child + 1 < lm_heap_count && lm_priority(lm_heap[child + 1]) < lm_priority(lm_heap[child])) child++;
        if (lm_priority(id) <= lm_priority(lm_heap[child])) break;
        lm_heap[pos] = lm_heap[child]; lm_position[lm_heap[pos]] = pos; pos = child;
    }
    lm_heap[pos] = id; lm_position[id] = pos;
    return result;
}

/* Map-scale routing has no room bounding box and no granite excavation cap.
 * A coherent bend is the preferred course; immutable landmarks divert it. */
static int lm_route(coord start, coord end, coord* path, int capacity)
{
    if (!lm_allowed(start.y, start.x) || !lm_allowed(end.y, end.x)) return 0;
    bool horizontal = ABS(end.x - start.x) >= ABS(end.y - start.y);
    int span = horizontal ? (int)end.x - start.x : (int)end.y - start.y;
    if (!span) return 0;
    unsigned salt = (unsigned)rand_int(65536);
    int bend = rand_range(4, MAX(5, MIN(p_ptr->cur_map_hgt, p_ptr->cur_map_wid) / 8));
    if (one_in_(2)) bend = -bend;
    int source = start.y * MAX_DUNGEON_WID + start.x, target = end.y * MAX_DUNGEON_WID + end.x;
    for (int i = 0; i < LM_CELLS; i++)
    { lm_distance[i] = INT_MAX; lm_parent[i] = -1; lm_position[i] = -1; }
    lm_goal = end; lm_heap_count = 1; lm_heap[0] = source;
    lm_distance[source] = 0; lm_position[source] = 0;
    while (lm_heap_count)
    {
        int id = lm_heap_pop(), y = id / MAX_DUNGEON_WID, x = id % MAX_DUNGEON_WID;
        if (id == target) break;
        for (int d = 0; d < 4; d++)
        {
            int ny = y + lm_dy[d], nx = x + lm_dx[d], next = ny * MAX_DUNGEON_WID + nx;
            if (!lm_allowed(ny, nx) || lm_position[next] == -2) continue;
            int along = horizontal ? nx - start.x : ny - start.y;
            int linear = horizontal ? start.y + ((int)end.y - start.y) * along / span
                : start.x + ((int)end.x - start.x) * along / span;
            int t = MAX(0, MIN(ABS(span), span > 0 ? along : -along));
            int curve = lm_feature == FEAT_CHASM
                ? bend * MIN(t, ABS(span) - t) * 2 / ABS(span)
                : 4 * bend * t * (ABS(span) - t) / (span * span);
            int cross = horizontal ? p_ptr->cur_map_hgt : p_ptr->cur_map_wid;
            int margin = MAX(4, cross / 12);
            int preferred = MAX(margin, MIN(cross - margin - 1, linear + curve));
            unsigned noise = ((unsigned)(ny / 5) * 374761393u + (unsigned)(nx / 5) * 668265263u + salt) * 1274126177u;
            int cost = 10 + MIN(36, ABS((horizontal ? ny : nx) - preferred)) + (int)((noise >> 24) % 5);
            if (lm_rock(cave_feat[ny][nx]))
                cost += (terrain_vault_policy_at(ny, nx) & TERRAIN_VAULT_RUINED) ? 0 : 1;
            for (int side = 0; side < 4; side++)
                if (lm_hard[ny + lm_dy[side]][nx + lm_dx[side]]) cost += 7;
            int distance = lm_distance[id] + cost;
            if (distance >= lm_distance[next]) continue;
            lm_distance[next] = distance; lm_parent[next] = id;
            if (lm_position[next] < 0) { lm_position[next] = lm_heap_count; lm_heap[lm_heap_count++] = next; }
            lm_heap_up(lm_position[next]);
        }
    }
    if (lm_parent[target] < 0) return 0;
    int count = 0;
    for (int id = target; id >= 0; id = lm_parent[id])
    {
        if (count >= capacity) return 0;
        path[count++] = (coord){ id / MAX_DUNGEON_WID, id % MAX_DUNGEON_WID };
        if (id == source) break;
    }
    return count;
}

static bool lm_endpoints(const terrain_landmark_profile* p, coord* a, coord* b)
{
    bool horizontal = one_in_(2);
    int axis = horizontal ? p_ptr->cur_map_wid : p_ptr->cur_map_hgt;
    int cross = horizontal ? p_ptr->cur_map_hgt : p_ptr->cur_map_wid;
    int length = axis * rand_range(p->min_span_percent, p->max_span_percent) / 100;
    length = MIN(length, axis - 8);
    for (int attempt = 0; attempt < 80; attempt++)
    {
        int from = rand_range(3, MAX(3, axis - length - 4));
        /* Span the body of the dungeon. Headwaters may enter from its sides,
         * but a random bend must not turn the entire river into a border moat. */
        int margin = MAX(5, cross / 5);
        int side1 = rand_range(margin, cross - margin - 1);
        int side2 = MAX(margin, MIN(cross - margin - 1, side1 + rand_range(-cross / 4, cross / 4)));
        *a = horizontal ? (coord){ side1, from } : (coord){ from, side1 };
        *b = horizontal ? (coord){ side2, from + length } : (coord){ from + length, side2 };
        if (lm_allowed(a->y, a->x) && lm_allowed(b->y, b->x)) return true;
    }
    return false;
}

static bool lm_at_edge(coord p)
{
    return p.y == 1 || p.x == 1 || p.y == p_ptr->cur_map_hgt - 2 || p.x == p_ptr->cur_map_wid - 2;
}

static void lm_mark_terminal(coord p, int kind, int role)
{
    lm_terminal[p.y][p.x] = kind;
    lm_terminal_role[p.y][p.x] |= role; /* 1 source, 2 receiving outlet */
}

static bool lm_edge_mouth(coord* p, bool horizontal, bool far)
{
    int cross = horizontal ? p_ptr->cur_map_hgt : p_ptr->cur_map_wid;
    int wanted = horizontal ? p->y : p->x;
    for (int offset = 0; offset < cross; offset++) for (int side = -1; side <= 1; side += 2)
    {
        int v = wanted + offset * side;
        if (v < 2 || v >= cross - 2) continue;
        coord c = horizontal ? (coord){ v, far ? p_ptr->cur_map_wid - 2 : 1 }
            : (coord){ far ? p_ptr->cur_map_hgt - 2 : 1, v };
        if (lm_allowed(c.y, c.x)) { *p = c; return true; }
    }
    return false;
}

/* An acid pool or vent begins in rock, not as a cut-off stripe on an open floor. */
static bool lm_source_site(coord wanted, int search, coord* site)
{
    int best = INT_MAX;
    for (int y = MAX(3, wanted.y - search); y <= MIN(p_ptr->cur_map_hgt - 4, wanted.y + search); y++)
        for (int x = MAX(3, wanted.x - search); x <= MIN(p_ptr->cur_map_wid - 4, wanted.x + search); x++)
        {
            if (!lm_allowed(y, x) || !lm_rock(cave_feat[y][x]) || lm_work[y][x]) continue;
            if (lm_feature == FEAT_LAVA)
            {
                bool clear = true;
                for (int yy = y - 1; yy <= y + 1; yy++) for (int xx = x - 1; xx <= x + 1; xx++)
                    if (!lm_allowed(yy, xx) || lm_work[yy][xx]) clear = false;
                if (!clear) continue;
            }
            int backing = 0;
            for (int d = 0; d < 4; d++) backing += lm_rock(cave_feat[y + lm_dy[d]][x + lm_dx[d]]);
            if (backing < 2) continue;
            int score = ABS(y - wanted.y) + ABS(x - wanted.x);
            if (terrain_vault_id_at(y, x) >= 0) score += 8;
            if (score < best) { best = score; *site = (coord){ y, x }; }
        }
    return best < INT_MAX;
}

static void lm_disk(coord center, int width)
{
    int radius = (width + 1) / 2, parity = (width % 2) ? 0 : 1;
    for (int dy = -radius; dy <= radius; dy++) for (int dx = -radius; dx <= radius; dx++)
    {
        int y = center.y + dy, x = center.x + dx;
        if ((2 * dy + parity) * (2 * dy + parity) + (2 * dx + parity) * (2 * dx + parity) > width * width
            || !lm_allowed(y, x)) continue;
        unsigned policy = terrain_vault_policy_at(y, x);
        /* In an intact crossing hall, the river is channelled through a
         * controlled breach; the wider natural river resumes outside it. */
        if ((policy & TERRAIN_VAULT_CROSSING) && !(policy & TERRAIN_VAULT_FLOOD)
            && (ABS(dy) > 1 || ABS(dx) > 1)) continue;
        lm_work[y][x] = TERRAIN_LANDMARK_TERRAIN;
    }
    /* The complete cardinal spine is never lost to rounding or constriction. */
    if (lm_allowed(center.y, center.x)) lm_work[center.y][center.x] = TERRAIN_LANDMARK_TERRAIN;
}

static bool lm_vault_waypoint(coord a, coord b, bool lake, coord* point)
{
    int seen = 0;
    int y1 = MIN(a.y, b.y), y2 = MAX(a.y, b.y), x1 = MIN(a.x, b.x), x2 = MAX(a.x, b.x);
    for (int y = MAX(2, y1 - 15); y <= MIN(p_ptr->cur_map_hgt - 3, y2 + 15); y++)
        for (int x = MAX(2, x1 - 15); x <= MIN(p_ptr->cur_map_wid - 3, x2 + 15); x++)
        {
            unsigned policy = terrain_vault_policy_at(y, x);
            if (!policy || (lake && !(policy & TERRAIN_VAULT_FLOOD))
                || !lm_allowed(y, x) || cave_feat[y][x] != FEAT_FLOOR) continue;
            if (ABS(y - a.y) + ABS(x - a.x) < 10 || ABS(y - b.y) + ABS(x - b.x) < 10) continue;
            if (rand_int(++seen) == 0) *point = (coord){ y, x };
        }
    return seen > 0;
}

/* Basins are cavities with their own floor relief and fill level. Channels
 * connect them afterwards; adding a basin does not widen those connections. */
static bool lm_add_basin(coord center, int radius)
{
    if (lm_basin_count >= LM_BASINS || !lm_allowed(center.y, center.x)) return false;
    const terrain_network_profile* network = terrain_network_for_depth(p_ptr->depth);
    int rx = MIN(network->max_radius, radius + rand_int(3));
    int ry = MAX(network->min_radius, radius - rand_int(2));
    if (one_in_(2)) { int swap = rx; rx = ry; ry = swap; }
    unsigned salt = (unsigned)rand_int(65536);
    int id = lm_basin_count + 1, area = 0;
    for (int dy = -ry - 3; dy <= ry + 3; dy++) for (int dx = -rx - 3; dx <= rx + 3; dx++)
    {
        int y = center.y + dy, x = center.x + dx;
        if (!lm_allowed(y, x)) continue;
        unsigned policy = terrain_vault_policy_at(y, x);
        if ((policy & TERRAIN_VAULT_CROSSING) && !(policy & TERRAIN_VAULT_FLOOD)) continue;
        unsigned noise = ((unsigned)((y + 256) / 3) * 374761393u
            + (unsigned)((x + 256) / 3) * 668265263u + salt) * 1274126177u;
        int relief = (int)((noise >> 24) % 9) - 4;
        int floor = 100 * dx * dx / (rx * rx) + 100 * dy * dy / (ry * ry) + relief * 6;
        /* Dry chamber shelves have independently varying breadth. */
        if (floor < 130 + (int)(noise % 35)) lm_cavity[y][x] = 1;
        if (floor > 95) continue;
        if (lm_basin[y][x] && lm_basin[y][x] != id) return false;
        lm_work[y][x] = TERRAIN_LANDMARK_TERRAIN; lm_basin[y][x] = id; area++;
    }
    if (area < rx * ry * 2) return false;
    lm_basins[lm_basin_count++] = center;
    return true;
}

/* A nearby cave chamber is preferred, but the cavity may extend well into
 * granite. Separate basin nodes retain a genuine neck between their shores. */
static bool lm_basin_site(coord wanted, int radius, int search, bool require_floor, coord* site)
{
    int best = INT_MAX;
    for (int y = MAX(3, wanted.y - search); y <= MIN(p_ptr->cur_map_hgt - 4, wanted.y + search); y++)
        for (int x = MAX(3, wanted.x - search); x <= MIN(p_ptr->cur_map_wid - 4, wanted.x + search); x++)
        {
            if (!lm_allowed(y, x) || (require_floor && !lm_passage(y, x))) continue;
            unsigned policy = terrain_vault_policy_at(y, x);
            if ((policy & TERRAIN_VAULT_CROSSING) && !(policy & TERRAIN_VAULT_FLOOD)) continue;
            bool close = false;
            for (int i = 0; i < lm_basin_count; i++)
                if (distance(y, x, lm_basins[i].y, lm_basins[i].x) < radius * 2 + 8) close = true;
            if (close) continue;
            int blocked = 0;
            for (int dy = -radius; dy <= radius; dy += MAX(1, radius / 2))
                for (int dx = -radius; dx <= radius; dx += MAX(1, radius / 2))
                    if (!lm_allowed(y + dy, x + dx)) blocked++;
            int score = (ABS(y - wanted.y) + ABS(x - wanted.x)) * 3 + blocked * 8;
            if (lm_passage(y, x)) score -= 18;
            if (cave_natural[y][x]) score -= 6;
            if (score < best) { best = score; *site = (coord){ y, x }; }
        }
    return best < INT_MAX;
}

static void lm_paint_flow(const coord* path, int count,
    const terrain_network_profile* network, bool broad)
{
    const terrain_landmark_profile* profile = terrain_landmark_for_depth(p_ptr->depth);
    int width = 1, bank_side = one_in_(2) ? 1 : -1;
    unsigned salt = (unsigned)rand_int(65536);
    for (int i = 0; i < count; i++)
    {
        if (i % 9 == 0) width = broad ? rand_range(2, 4)
            : percent_chance(network->one_tile_percent) || profile->max_width < 2 ? 1
            : rand_range(MAX(2, profile->min_width), profile->max_width);
        if (lm_feature == FEAT_CHASM) width = 1;
        if ((i < 5 && lm_terminal[path[0].y][path[0].x] != TERRAIN_TERMINAL_EDGE)
            || (i >= count - 5 && lm_terminal[path[count - 1].y][path[count - 1].x] != TERRAIN_TERMINAL_EDGE)) width = 1;
        coord p = path[i];
        lm_disk(p, width); lm_channel[p.y][p.x] = 1;
        if (i + 1 < count)
        {
            int dy = (int)path[i + 1].y - p.y, dx = (int)path[i + 1].x - p.x;
            /* Some stretches fill a fissure; others expose a ledge on one
             * side. Existing floors remain usable on either bank. */
            bool ledge = i >= 4 && i < count - 4 && ((i / 7 + salt) % 4) != 0;
            int y = p.y + dx * bank_side, x = p.x - dy * bank_side;
            if (ledge && lm_allowed(y, x)) lm_cavity[y][x] = 1;
        }
    }
}

static bool lm_flow_edge(coord from, coord to, const terrain_network_profile* network, bool broad)
{
    coord path[LM_PATH];
    int count = lm_route(from, to, path, LM_PATH);
    if (!count) return false;
    lm_paint_flow(path, count, network, broad);
    if (lm_terminal[from.y][from.x] == TERRAIN_TERMINAL_VENT) lm_disk(from, 3);
    return true;
}

static bool lm_feeder(coord to, int min_length, int max_length,
    const terrain_network_profile* network, bool junction)
{
    for (int attempt = 0; attempt < 12; attempt++)
    {
        int dy = rand_range(-max_length, max_length), dx = rand_range(-max_length, max_length);
        if (ABS(dy) + ABS(dx) < min_length) continue;
        coord from = { MAX(2, MIN(p_ptr->cur_map_hgt - 3, to.y + dy)),
            MAX(2, MIN(p_ptr->cur_map_wid - 3, to.x + dx)) };
        if (!lm_source_site(from, 8, &from)) continue;
        if (!junction)
        {
            if (!lm_flow_edge(from, to, network, false)) continue;
            lm_mark_terminal(from, lm_feature == FEAT_LAVA ? TERRAIN_TERMINAL_VENT
                : lm_feature == FEAT_CHASM ? TERRAIN_TERMINAL_FISSURE : TERRAIN_TERMINAL_SPRING, 1);
            if (lm_feature == FEAT_LAVA) lm_disk(from, 3);
            return true;
        }
        coord path[LM_PATH];
        int count = lm_route(from, to, path, LM_PATH), merge = -1;
        for (int i = count - 1; i >= 0; i--)
            if (lm_work[path[i].y][path[i].x] == TERRAIN_LANDMARK_TERRAIN) { merge = i; break; }
        if (merge < 0 || count - merge < 10) continue;
        coord p = path[merge];
        if (!lm_channel[p.y][p.x] || lm_basin[p.y][p.x]) continue;
        int neighbors = 0;
        for (int d = 0; d < 4; d++) neighbors += lm_channel[p.y + lm_dy[d]][p.x + lm_dx[d]] != 0;
        if (neighbors < 2) continue;
        bool close = false;
        for (int i = 0; i < lm_junctions; i++)
            if (distance(p.y, p.x, lm_merges[i].y, lm_merges[i].x) < 6) close = true;
        if (close || lm_junctions >= 8) continue;
        /* Stop at the first real merge. Do not retrace the receiving stream
         * and silently widen it with another full-width brush. */
        lm_paint_flow(path + merge, count - merge, network, false);
        lm_mark_terminal(from, lm_feature == FEAT_LAVA ? TERRAIN_TERMINAL_VENT
            : lm_feature == FEAT_CHASM ? TERRAIN_TERMINAL_FISSURE : TERRAIN_TERMINAL_SPRING, 1);
        if (lm_feature == FEAT_LAVA) lm_disk(from, 3);
        lm_merges[lm_junctions++] = p;
        return true;
    }
    return false;
}

static bool lm_network_shape(const terrain_landmark_profile* profile,
    const terrain_network_profile* network)
{
    coord a = { 3, 3 }, b = { p_ptr->cur_map_hgt - 4, p_ptr->cur_map_wid - 4 }, site = { 0, 0 };
    bool compact = lm_family == TERRAIN_NETWORK_CHAMBER || lm_family == TERRAIN_NETWORK_POOLS;
    bool traverse = !compact && (lm_family == TERRAIN_NETWORK_RIVER || one_in_(3));
    bool lake_chain = !compact && !traverse && lm_family == TERRAIN_NETWORK_CHAIN;
    bool sink_end = !compact && !traverse && !lake_chain && lm_feature != FEAT_LAVA && one_in_(3);
    lm_stats.scenario = compact ? (lm_family == TERRAIN_NETWORK_POOLS ? TERRAIN_FLOW_POOLS : TERRAIN_FLOW_WORKS)
        : traverse ? TERRAIN_FLOW_TRAVERSE : lake_chain ? TERRAIN_FLOW_LAKE_CHAIN
        : lm_feature == FEAT_CHASM ? TERRAIN_FLOW_FRACTURE
        : lm_feature == FEAT_LAVA ? TERRAIN_FLOW_VOLCANIC : TERRAIN_FLOW_SPRING;
    lm_path_count = 0;
    coord origin = { 0, 0 };
    if (compact)
    {
        /* Regional pools need no long route through unrelated partitions. */
        for (int attempt = 0; attempt < 100; attempt++)
        {
            coord p = { rand_range(4, p_ptr->cur_map_hgt - 5), rand_range(4, p_ptr->cur_map_wid - 5) };
            if (lm_allowed(p.y, p.x) && (lm_preparing || lm_passage(p.y, p.x))) { origin = p; break; }
        }
        if (!origin.y) return false;
        /* A floodable ruin is a local opportunity for a pool field, not a
         * reason to relocate one of its basins to the far side of the map. */
        int influence = network->max_radius * 3 + 8;
        a = (coord){ MAX(3, origin.y - influence), MAX(3, origin.x - influence) };
        b = (coord){ MIN(p_ptr->cur_map_hgt - 4, origin.y + influence),
            MIN(p_ptr->cur_map_wid - 4, origin.x + influence) };
    }
    else
    {
        if (!lm_endpoints(profile, &a, &b)) return false;
        if (traverse)
        {
            bool horizontal = ABS(b.x - a.x) >= ABS(b.y - a.y);
            if (!lm_edge_mouth(&a, horizontal, false) || !lm_edge_mouth(&b, horizontal, true)) return false;
            lm_mark_terminal(a, TERRAIN_TERMINAL_EDGE, 1); lm_mark_terminal(b, TERRAIN_TERMINAL_EDGE, 2);
        }
        else if (!lake_chain)
        {
            if (!lm_source_site(a, 12, &a)) return false;
            lm_mark_terminal(a, lm_feature == FEAT_LAVA ? TERRAIN_TERMINAL_VENT
                : lm_feature == FEAT_CHASM ? TERRAIN_TERMINAL_FISSURE : TERRAIN_TERMINAL_SPRING, 1);
            if (sink_end)
            {
                if (!lm_source_site(b, 12, &b)) return false;
                lm_mark_terminal(b, lm_feature == FEAT_CHASM ? TERRAIN_TERMINAL_FISSURE : TERRAIN_TERMINAL_SINK, 2);
            }
        }
        lm_path_count = lm_route(a, b, lm_path, LM_PATH);
        if (!lm_path_count) return false;
    }
    int count = rand_range(network->min_basins, network->max_basins);
    if (lm_family == TERRAIN_NETWORK_CHAMBER) count = 1;
    if (lm_family == TERRAIN_NETWORK_TRIBUTARIES) count = MIN(2, count);
    if (lm_family == TERRAIN_NETWORK_RIVER) count = MIN(network->max_basins,
        percent_chance(profile->lake_chance) ? 2 : 1);
    if (lm_family == TERRAIN_LANDMARK_FRACTURE_FAMILY) count = rand_range(1, MIN(2, network->max_basins));
    int radius_cap = compact ? network->max_radius
        : MIN(network->max_radius, MAX(3, lm_path_count / (count * 3 + 3)));
    if (radius_cap < network->min_radius) return false;
    int small = network->min_radius;
    for (int i = 0; i < count; i++)
    {
        coord wanted = origin;
        if (!compact)
        {
            int numerator = lake_chain ? i : i + 1;
            int denominator = lake_chain ? MAX(1, count - 1) : (traverse || sink_end) ? count + 1 : count;
            int index = lm_path_count - 1 - numerator * (lm_path_count - 1) / denominator;
            wanted = lm_path[index];
        }
        else if (i)
        {
            int spread = network->max_radius * 3 + 8;
            wanted = (coord){ MAX(3, MIN(p_ptr->cur_map_hgt - 4, origin.y + rand_range(-spread, spread))),
                MAX(3, MIN(p_ptr->cur_map_wid - 4, origin.x + rand_range(-spread, spread))) };
        }
        int radius = rand_range(small, radius_cap);
        /* Unequal chambers matter more than random width noise along a line. */
        if (count > 1 && i == 0) radius = small;
        if (count > 1 && i == count - 1) radius = radius_cap;
        if (lm_family == TERRAIN_NETWORK_CHAMBER) radius = radius_cap;
        if (lm_feature == FEAT_CHASM) radius = MAX(small, MIN(radius, 4));
        if (i == count / 2 && lm_vault_waypoint(a, b, true, &site) && one_in_(2)
            && (!compact || (ABS(site.y - origin.y) <= network->max_radius * 3 + 8
                && ABS(site.x - origin.x) <= network->max_radius * 3 + 8))) wanted = site;
        if (!lm_basin_site(wanted, radius, 9, compact && !lm_preparing, &site) || !lm_add_basin(site, radius)) return false;
    }
    if (lm_family == TERRAIN_NETWORK_POOLS)
    {
        for (int i = 0; i < lm_basin_count; i++) lm_mark_terminal(lm_basins[i], TERRAIN_TERMINAL_BASIN, 3);
        /* Some pools have no visible outlet. The access check does not invent
         * a player route through any inferred below-floor connection. */
        if (count > 2 && one_in_(2))
            return lm_flow_edge(lm_basins[0], lm_basins[1], network, false);
        return true;
    }
    if (lm_family == TERRAIN_NETWORK_CHAMBER)
    {
        lm_mark_terminal(lm_basins[0], TERRAIN_TERMINAL_BASIN, 2);
        if (!lm_feeder(lm_basins[0], 12, 28, network, false)) return false;
        if (percent_chance(profile->branch_chance)) lm_feeder(lm_basins[0], 12, 22, network, false);
        return true;
    }
    if (lake_chain) { a = lm_basins[0]; lm_mark_terminal(a, TERRAIN_TERMINAL_BASIN, 1); }
    if (!traverse && !sink_end) { b = lm_basins[lm_basin_count - 1]; lm_mark_terminal(b, TERRAIN_TERMINAL_BASIN, 2); }
    coord previous = a;
    for (int i = 0; i <= lm_basin_count; i++)
    {
        coord next = i < lm_basin_count ? lm_basins[i] : b;
        if (next.y == previous.y && next.x == previous.x) continue;
        if (!lm_flow_edge(previous, next, network, lm_family == TERRAIN_NETWORK_RIVER)) return false;
        previous = next;
    }
    int feeders = lm_family == TERRAIN_NETWORK_TRIBUTARIES ? rand_range(2, 3)
        : lm_family == TERRAIN_LANDMARK_FRACTURE_FAMILY ? rand_range(1, 2)
        : percent_chance(profile->branch_chance) ? 1 : 0;
    for (int i = 0; i < feeders; i++)
    {
        coord target = { 0, 0 }; int seen = 0;
        for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++) for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
            if (lm_channel[y][x] && !lm_basin[y][x] && rand_int(++seen) == 0) target = (coord){ y, x };
        if (seen) lm_feeder(target, 14, MIN(p_ptr->cur_map_hgt, p_ptr->cur_map_wid) / 3, network, true);
    }
    return lm_family != TERRAIN_NETWORK_TRIBUTARIES || lm_junctions >= 2;
}

static void lm_prune_shapes(void)
{
    static byte seen[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static coord queue[LM_CELLS];
    memset(seen, 0, sizeof(seen));
    int head = 0, tail = 0;
    /* Multiple basins are allowed to be visibly separate. Keep their actual
     * connected shapes, discarding only fragments clipped off by hard cells. */
    for (int i = 0; i < lm_basin_count; i++)
    {
        coord p = lm_basins[i];
        if (!seen[p.y][p.x]) { seen[p.y][p.x] = 1; queue[tail++] = p; }
    }
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        if (lm_channel[y][x] && !seen[y][x]) { seen[y][x] = 1; queue[tail++] = (coord){ y, x }; }
    while (head < tail)
    {
        coord c = queue[head++];
        for (int d = 0; d < 4; d++)
        {
            int y = c.y + lm_dy[d], x = c.x + lm_dx[d];
            if (!in_bounds_fully(y, x) || seen[y][x] || !lm_work[y][x]) continue;
            seen[y][x] = 1; queue[tail++] = (coord){ y, x };
        }
    }
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        if (!seen[y][x]) { lm_work[y][x] = 0; lm_basin[y][x] = 0; }
}

static void lm_banks(void)
{
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
    {
        if (!lm_cavity[y][x] || !lm_allowed(y, x) || lm_work[y][x]) continue;
        bool mouth = false;
        for (int yy = MAX(1, y - 2); yy <= MIN(p_ptr->cur_map_hgt - 2, y + 2); yy++)
            for (int xx = MAX(1, x - 2); xx <= MIN(p_ptr->cur_map_wid - 2, x + 2); xx++)
                if (lm_terminal[yy][xx] && lm_terminal[yy][xx] != TERRAIN_TERMINAL_BASIN
                    && lm_terminal[yy][xx] != TERRAIN_TERMINAL_EDGE) mouth = true;
        if (mouth) continue;
        unsigned policy = terrain_vault_policy_at(y, x);
        if ((policy & TERRAIN_VAULT_CROSSING) && !(policy & TERRAIN_VAULT_FLOOD)
            && lm_rock(cave_feat[y][x])) continue;
        if (lm_rock(cave_feat[y][x]) || cave_feat[y][x] == FEAT_RUBBLE)
            lm_work[y][x] = TERRAIN_LANDMARK_BANK;
    }
    memcpy(lm_shadow, cave_feat, sizeof(lm_shadow));
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
    {
        if (lm_work[y][x] == TERRAIN_LANDMARK_TERRAIN) lm_shadow[y][x] = lm_feature;
        else if (lm_work[y][x] == TERRAIN_LANDMARK_BANK && (lm_rock(cave_feat[y][x]) || cave_feat[y][x] == FEAT_RUBBLE))
            lm_shadow[y][x] = FEAT_FLOOR;
    }
}

static bool lm_wet(int y, int x)
{
    return in_bounds_fully(y, x) && (lm_work[y][x] == TERRAIN_LANDMARK_TERRAIN
        || lm_work[y][x] == TERRAIN_LANDMARK_BRIDGE
        || (lm_work[y][x] == TERRAIN_LANDMARK_REPAIR && lm_hydraulic[y][x]));
}

static void lm_raise_basins(void)
{
    static byte previous[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static byte throat[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    memset(throat, 0, sizeof(throat));
    for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++) for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
        if (lm_channel[y][x] && !lm_basin[y][x])
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) throat[y + dy][x + dx] = 1;
    int waves = rand_range(2, 4);
    for (int wave = 0; wave < waves; wave++)
    {
        memcpy(previous, lm_basin, sizeof(previous));
        for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++) for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
        {
            if (!lm_allowed(y, x) || previous[y][x] || throat[y][x]
                || lm_work[y][x] == TERRAIN_LANDMARK_BRIDGE
                || lm_work[y][x] == TERRAIN_LANDMARK_REPAIR || cave_feat_is_bridge(cave_feat[y][x])) continue;
            if (!lm_rock(cave_feat[y][x]) && !lm_passage(y, x) && cave_feat[y][x] != lm_feature) continue;
            int basin = 0;
            for (int d = 0; d < 4; d++)
            {
                int id = previous[y + lm_dy[d]][x + lm_dx[d]];
                if (id > 0 && id <= MIN(lm_basin_count, 2)) { basin = id; break; }
            }
            if (!basin) continue;
            bool another = false;
            for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++)
                if (previous[y + dy][x + dx] && previous[y + dy][x + dx] != basin) another = true;
            if (another) continue;
            lm_basin[y][x] = basin; lm_work[y][x] = TERRAIN_LANDMARK_TERRAIN;
            lm_shadow[y][x] = lm_feature; lm_stats.overflow_tiles++;
        }
    }
}

static void lm_structure_proposal(void)
{
    memcpy(lm_effect_hard, lm_hard, sizeof(lm_effect_hard));
    memset(lm_hydraulic, 0, sizeof(lm_hydraulic));
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
    {
        lm_hydraulic[y][x] = lm_work[y][x] == TERRAIN_LANDMARK_TERRAIN;
        if (lm_terminal[y][x] && lm_terminal[y][x] != TERRAIN_TERMINAL_BASIN)
            for (int yy = MAX(1, y - 2); yy <= MIN(p_ptr->cur_map_hgt - 2, y + 2); yy++)
                for (int xx = MAX(1, x - 2); xx <= MIN(p_ptr->cur_map_wid - 2, x + 2); xx++)
                    lm_effect_hard[yy][xx] = 1;
        /* Keep the stream necks and junctions that made this a network.
         * Incidents spread from receiving basins, not along every thin link. */
        if (lm_preserve_form && lm_channel[y][x] && !lm_basin[y][x])
            for (int yy = MAX(1, y - 1); yy <= MIN(p_ptr->cur_map_hgt - 2, y + 1); yy++)
                for (int xx = MAX(1, x - 1); xx <= MIN(p_ptr->cur_map_wid - 2, x + 1); xx++)
                    if (!lm_basin[yy][xx] && lm_passage(yy, xx)) lm_effect_hard[yy][xx] = 1;
    }
    if (lm_epoch == TERRAIN_HISTORY_ANCIENT)
        memset(lm_effect_hard, 1, sizeof(lm_effect_hard));
    else if (lm_epoch == TERRAIN_HISTORY_OVERFLOW)
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            bool floodplain = false;
            /* A rising basin spills into its immediate neighbours. Other
             * lakes and the connecting stream remain recognizable. */
            int radius = terrain_network_for_depth(p_ptr->depth)->max_radius + 4;
            for (int i = 0; i < MIN(lm_basin_count, 2); i++)
                if (distance(y, x, lm_basins[i].y, lm_basins[i].x) <= radius) floodplain = true;
            if (!floodplain) lm_effect_hard[y][x] = 1;
        }
    terrain_structure_apply(lm_effect_hard, lm_work, lm_shadow, lm_feature, &lm_structures);
    /* A flooded room with a broad interior is a receiving chamber, not a
     * suddenly wide connector. Classify actual geometry, not scenario labels. */
    static int receiving[LM_CELLS], areas[LM_CELLS], cores[LM_CELLS];
    memset(receiving, 0, sizeof(receiving)); memset(areas, 0, sizeof(areas)); memset(cores, 0, sizeof(cores));
    for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++) for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
    {
        int owner = terrain_structure_owner(y, x);
        if (owner <= 0 || owner >= LM_CELLS || !lm_wet(y, x)) continue;
        areas[owner]++;
        if (lm_basin[y][x]) receiving[owner] = lm_basin[y][x];
        bool broad = true;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
            if (!lm_wet(y + dy, x + dx)) broad = false;
        cores[owner] += broad;
    }
    int next = lm_basin_count + 1;
    for (int id = 1; id < LM_CELLS; id++)
        if (areas[id] >= 16 && cores[id] >= 3 && !receiving[id] && next < 256) receiving[id] = next++;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
    {
        int owner = terrain_structure_owner(y, x);
        if (owner > 0 && owner < LM_CELLS && areas[owner] >= 16 && cores[owner] >= 3
            && receiving[owner] && !lm_basin[y][x] && lm_wet(y, x)) lm_basin[y][x] = receiving[owner];
    }
}

static bool lm_validate_terminals(void)
{
    /* A causeway may cover the chosen centre of a receiving pool. Keep its
     * source/outlet marker on actual liquid in that same basin. */
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
    {
        if (lm_terminal[y][x] != TERRAIN_TERMINAL_BASIN || lm_shadow[y][x] == lm_feature) continue;
        coord best = { 0, 0 }; int score = LM_CELLS;
        if (!lm_basin[y][x]) return false;
        for (int yy = 1; yy < p_ptr->cur_map_hgt - 1; yy++) for (int xx = 1; xx < p_ptr->cur_map_wid - 1; xx++)
        {
            if (lm_basin[yy][xx] != lm_basin[y][x] || lm_shadow[yy][xx] != lm_feature
                || !lm_wet(yy, xx) || (lm_terminal[yy][xx] && lm_terminal[yy][xx] != TERRAIN_TERMINAL_BASIN)) continue;
            int dist = ABS(yy - y) + ABS(xx - x);
            if (dist < score) { score = dist; best = (coord){ yy, xx }; }
        }
        if (!best.y) return false;
        lm_terminal[best.y][best.x] = TERRAIN_TERMINAL_BASIN;
        lm_terminal_role[best.y][best.x] |= lm_terminal_role[y][x];
        lm_terminal[y][x] = lm_terminal_role[y][x] = 0;
    }
    lm_stats.sources = lm_stats.outlets = lm_stats.edge_mouths = lm_stats.springs = 0;
    lm_stats.sinks = lm_stats.terminal_basins = lm_stats.vents = lm_stats.fissure_tips = 0;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
    {
        int kind = lm_terminal[y][x];
        if (!kind) continue;
        if (!lm_wet(y, x) || lm_shadow[y][x] != lm_feature) return false;
        if (kind == TERRAIN_TERMINAL_EDGE)
        { if (!lm_at_edge((coord){ y, x })) return false; lm_stats.edge_mouths++; }
        else if (kind == TERRAIN_TERMINAL_BASIN)
        { if (!lm_basin[y][x]) return false; lm_stats.terminal_basins++; }
        else if (kind == TERRAIN_TERMINAL_VENT)
        {
            if (lm_feature != FEAT_LAVA) return false;
            int backing = 0;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
                if (lm_shadow[y + dy][x + dx] != FEAT_LAVA) return false;
            for (int d = 0; d < 4; d++) backing += lm_rock(lm_shadow[y + 2 * lm_dy[d]][x + 2 * lm_dx[d]]);
            if (!backing) return false;
            lm_stats.vents++;
        }
        else
        {
            int backing = 0, horizontal = 1, vertical = 1;
            for (int d = 0; d < 4; d++)
            {
                backing += lm_rock(lm_shadow[y + lm_dy[d]][x + lm_dx[d]]);
                if (lm_wet(y + lm_dy[d], x + lm_dx[d])) { if (d % 2) horizontal++; else vertical++; }
            }
            if (!backing || MIN(horizontal, vertical) != 1 || horizontal + vertical != 3) return false;
            if (kind == TERRAIN_TERMINAL_SPRING) lm_stats.springs++;
            else if (kind == TERRAIN_TERMINAL_SINK) lm_stats.sinks++;
            else lm_stats.fissure_tips++;
        }
        lm_stats.sources += (lm_terminal_role[y][x] & 1) != 0;
        lm_stats.outlets += (lm_terminal_role[y][x] & 2) != 0;
    }
    return lm_stats.sources > 0 && lm_stats.outlets > 0;
}

static bool lm_access(void)
{
    static int match[LM_CELLS + 1];
    static byte old_component[LM_CELLS + 1];
    memset(match, 0, sizeof(match)); memset(old_component, 0, sizeof(old_component));
    terrain_generation_components(lm_shadow, lm_after);
    bool connected = true;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
    {
        int a = lm_before[y][x], b = lm_after[y][x];
        if (!a || !b) continue;
        old_component[b] = 1;
        if (match[a] && match[a] != b) connected = false;
        match[a] = b;
    }
    if (!connected) return false;
    /* A clipped shore can leave a new floor pocket against immutable rock.
     * Keep that pocket rock instead of rejecting a whole river or inventing
     * an inaccessible walkway. No original reachable floor is removed. */
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        if (lm_work[y][x] && lm_after[y][x] && !old_component[lm_after[y][x]])
        {
            if (lm_work[y][x] != TERRAIN_LANDMARK_BANK || lm_before[y][x]) return false;
            lm_work[y][x] = TERRAIN_LANDMARK_NONE; lm_shadow[y][x] = cave_feat[y][x];
        }
    return true;
}

/* Value of restoring a dry constructed route. This is an optional crossing
 * score, not a requirement that every player route avoids water or jumping. */
static int lm_crossing_distance(coord from, coord to, int limit)
{
    static short steps[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static coord queue[LM_CELLS];
    memset(steps, -1, sizeof(steps));
    int head = 0, tail = 1; queue[0] = from; steps[from.y][from.x] = 0;
    while (head < tail)
    {
        coord p = queue[head++]; int step = steps[p.y][p.x];
        if (p.y == to.y && p.x == to.x) return step;
        if (step >= limit) continue;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
        {
            if (!dy && !dx) continue;
            int y = p.y + dy, x = p.x + dx;
            if (!terrain_generation_walkable(y, x, lm_shadow)) continue;
            if (lm_work[y][x] == TERRAIN_LANDMARK_TERRAIN || steps[y][x] >= 0) continue;
            steps[y][x] = step + 1; queue[tail++] = (coord){ y, x };
        }
    }
    return limit + 1;
}

static bool lm_breached_wall(int y, int x)
{
    return in_bounds_fully(y, x) && lm_rock(cave_feat[y][x])
        && (terrain_structure_cell(y, x) || terrain_vault_policy_at(y, x));
}

static bool lm_bridge(bool required)
{
    if (lm_bridge_count >= LM_BRIDGES) return false;
    landmark_bridge best = { { 0, 0 }, 0, 0, 0 };
    int best_score = -1;
    int best_saving = 0;
    /* Evaluate a bounded shortlist; access repair is still allowed to search
     * all structural spans. Long rivers no longer receive two cosmetic tips. */
    landmark_bridge candidates[24]; int priorities[24], candidate_count = 0;
    for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++) for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
    {
        if (lm_work[y][x] != TERRAIN_LANDMARK_TERRAIN
            || (!(lm_built[y][x] && lm_passage(y, x))
                && !(required && lm_breached_wall(y, x)))) continue;
        for (int axis = 0; axis < 2; axis++)
        {
            int dy = axis, dx = 1 - axis, lo = 0, hi = 0;
            while (lo > -48 && in_bounds_fully(y + (lo - 1) * dy, x + (lo - 1) * dx)
                && lm_work[y + (lo - 1) * dy][x + (lo - 1) * dx] == TERRAIN_LANDMARK_TERRAIN) lo--;
            while (hi < 48 && in_bounds_fully(y + (hi + 1) * dy, x + (hi + 1) * dx)
                && lm_work[y + (hi + 1) * dy][x + (hi + 1) * dx] == TERRAIN_LANDMARK_TERRAIN) hi++;
            int length = hi - lo + 1;
            if (lo != 0 || length < (required ? 1 : 2) || length > 32) continue;
            bool valid = true, built_route = false;
            for (int i = lo; i <= hi; i++)
            {
                int yy = y + i * dy, xx = x + i * dx;
                if ((!lm_passage(yy, xx) && !(required && lm_breached_wall(yy, xx)))
                    || lm_hard[yy][xx]
                    || (lm_terminal[yy][xx] && lm_terminal[yy][xx] != TERRAIN_TERMINAL_BASIN)) valid = false;
                built_route |= lm_built[yy][xx] && lm_passage(yy, xx);
            }
            for (int i = 1; i <= 2; i++)
                if (!terrain_generation_walkable(y + (lo - i) * dy, x + (lo - i) * dx, lm_shadow)
                    || !terrain_generation_walkable(y + (hi + i) * dy, x + (hi + i) * dx, lm_shadow)) valid = false;
            if (!valid) continue;
            int ay = y + (lo - 1) * dy, ax = x + (lo - 1) * dx;
            int by = y + (hi + 1) * dy, bx = x + (hi + 1) * dx;
            /* Rebuild a route through breached masonry only where the span
             * serves constructed floor or has surviving approaches on both
             * sides. Unworked granite is not an architectural bridge axis. */
            if (!built_route && !(lm_built[ay][ax] && lm_built[by][bx])) continue;
            int score = 200 - length;
            if (lm_after[ay][ax] != lm_after[by][bx]) score += 10000;
            bool near = false;
            for (int i = 0; i < lm_bridge_count; i++)
                if (distance(y, x, lm_bridges[i].first.y, lm_bridges[i].first.x) < 10) near = true;
            if (near) score -= 500;
            int slot = candidate_count;
            if (slot >= 24)
            {
                slot = 0;
                for (int i = 1; i < 24; i++) if (priorities[i] < priorities[slot]) slot = i;
                if (score < priorities[slot] || (score == priorities[slot] && !one_in_(4))) continue;
            }
            else candidate_count++;
            priorities[slot] = score;
            candidates[slot] = (landmark_bridge){ { y + lo * dy, x + lo * dx }, dy, dx, length };
        }
    }
    for (int i = 0; i < candidate_count; i++)
    {
        landmark_bridge c = candidates[i];
        coord a = { c.first.y - c.dy, c.first.x - c.dx };
        coord b = { c.first.y + c.length * c.dy, c.first.x + c.length * c.dx };
        int saving = lm_crossing_distance(a, b, 64) - c.length - 1;
        if (!required && saving < 8) continue;
        int score = priorities[i] + MAX(0, saving) * 10;
        if (score > best_score) { best_score = score; best = c; best_saving = MAX(0, saving); }
    }
    if (best_score < 0) return false;
    for (int i = 0; i < best.length; i++)
    {
        int y = best.first.y + i * best.dy, x = best.first.x + i * best.dx;
        lm_work[y][x] = TERRAIN_LANDMARK_BRIDGE; lm_shadow[y][x] = FEAT_FLOOR;
    }
    lm_bridges[lm_bridge_count++] = best;
    lm_bridge_savings += best_saving;
    return true;
}

static int lm_broad_cores(void)
{
    static byte core[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static coord queue[LM_CELLS];
    memset(core, 0, sizeof(core));
    for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++) for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
    {
        bool full = true;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
            if (!lm_wet(y + dy, x + dx)) full = false;
        core[y][x] = full;
    }
    int count = 0;
    for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++) for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
    {
        if (!core[y][x]) continue;
        int head = 0, tail = 1; queue[0] = (coord){ y, x }; core[y][x] = 0;
        while (head < tail)
        {
            coord p = queue[head++];
            for (int d = 0; d < 4; d++)
            {
                int yy = p.y + lm_dy[d], xx = p.x + lm_dx[d];
                if (core[yy][xx]) { core[yy][xx] = 0; queue[tail++] = (coord){ yy, xx }; }
            }
        }
        if (tail >= 3) count++;
    }
    return count;
}

static bool lm_shape_statistics(const terrain_landmark_profile* profile)
{
    static int vault_total[LM_CELLS], vault_changed[LM_CELLS];
    static int wall_total[LM_CELLS], wall_changed[LM_CELLS];
    memset(vault_total, 0, sizeof(vault_total)); memset(vault_changed, 0, sizeof(vault_changed));
    memset(wall_total, 0, sizeof(wall_total)); memset(wall_changed, 0, sizeof(wall_changed));
    bool parts[25] = { false };
    int tiles = 0, original_floor = 0;
    int basin_area[256] = { 0 };
    lm_stats.channel_cells = lm_stats.narrow_cells = lm_stats.basin_tiles = 0;
    lm_stats.road_crossings = 0;
    lm_stats.y1 = lm_stats.x1 = 255; lm_stats.y2 = lm_stats.x2 = 0;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
    {
        unsigned policy = terrain_vault_policy_at(y, x);
        int instance = terrain_vault_instance_at(y, x);
        if (instance > 0 && instance < LM_CELLS && (policy & TERRAIN_VAULT_CROSSING)
            && !(policy & TERRAIN_VAULT_FLOOD))
        {
            vault_total[instance]++;
            vault_changed[instance] += lm_work[y][x] == TERRAIN_LANDMARK_TERRAIN;
            if (lm_rock(cave_feat[y][x]))
            {
                wall_total[instance]++;
                wall_changed[instance] += lm_shadow[y][x] != cave_feat[y][x];
            }
        }
        if (!lm_wet(y, x)) continue;
        tiles++;
        if (lm_basin[y][x]) { basin_area[lm_basin[y][x]]++; lm_stats.basin_tiles++; }
        else if (lm_channel[y][x])
        {
            lm_stats.channel_cells++;
            int horizontal = 1, vertical = 1;
            for (int d = 0; d < 4; d++)
                for (int step = 1; step < 20; step++)
                {
                    int yy = y + lm_dy[d] * step, xx = x + lm_dx[d] * step;
                    if (!in_bounds_fully(yy, xx) || (lm_shadow[yy][xx] != lm_feature
                        && !lm_wet(yy, xx))) break;
                    if (d % 2) horizontal++; else vertical++;
                }
            if (MIN(horizontal, vertical) == 1) lm_stats.narrow_cells++;
            if (lm_built[y][x] && ((horizontal <= 2 && lm_passage(y, x - 1) && lm_passage(y, x + 1))
                || (vertical <= 2 && lm_passage(y - 1, x) && lm_passage(y + 1, x)))) lm_stats.road_crossings++;
        }
        if (lm_passage(y, x)) original_floor++;
        lm_stats.y1 = MIN(lm_stats.y1, y); lm_stats.x1 = MIN(lm_stats.x1, x);
        lm_stats.y2 = MAX(lm_stats.y2, y); lm_stats.x2 = MAX(lm_stats.x2, x);
        int pi = level_partition_index_for_point(y, x); if (pi >= 0 && pi < 25) parts[pi] = true;
    }
    int height = lm_stats.y2 - lm_stats.y1 + 1, width = lm_stats.x2 - lm_stats.x1 + 1;
    int broad_cores = lm_broad_cores();
    if ((lm_family == TERRAIN_NETWORK_CHAIN || lm_family == TERRAIN_NETWORK_POOLS) && broad_cores < 2)
    {
        /* Destruction can join formerly separate basins inside a ruin. Keep
         * that useful flooded chamber, and report its actual resulting form. */
        if (!lm_preserve_form && broad_cores == 1 && (lm_structures.flooded || lm_structures.breached))
        { lm_family = TERRAIN_NETWORK_CHAMBER; lm_stats.scenario = TERRAIN_FLOW_WORKS; }
        else return false;
    }
    lm_stats.span = MAX(height * 100 / p_ptr->cur_map_hgt, width * 100 / p_ptr->cur_map_wid);
    lm_stats.partitions = 0; for (int i = 0; i < 25; i++) lm_stats.partitions += parts[i];
    bool compact = lm_family == TERRAIN_NETWORK_CHAMBER || lm_family == TERRAIN_NETWORK_POOLS;
    if ((!lm_preparing && lm_epoch == TERRAIN_HISTORY_DISASTER && !original_floor) || (!compact && (lm_stats.span < profile->min_span_percent
        || (!lm_preparing && current_partition_count > 1 && lm_stats.partitions < 2)))) return false;
    lm_stats.basin_count = broad_cores;
    if (!lm_stats.basin_count || tiles < 30) return false;
    if ((lm_family == TERRAIN_NETWORK_CHAIN || lm_family == TERRAIN_NETWORK_POOLS)
        && lm_stats.basin_count < MIN(2, lm_basin_count)) return false;
    if (lm_family != TERRAIN_NETWORK_RIVER && !compact && profile->min_width == 1
        && (lm_stats.channel_cells < 12 || lm_stats.narrow_cells * 100 < lm_stats.channel_cells * 40)) return false;
    for (int i = 1; i < LM_CELLS; i++)
        if (vault_total[i] && (vault_changed[i] * 100 > vault_total[i] * 35
            || wall_changed[i] * 100 > wall_total[i] * 25)) return false;
    return true;
}

static bool lm_relocations(void)
{
    lm_move_count = 0;
    memset(lm_taken, 0, sizeof(lm_taken));
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
    {
        int monster = cave_m_idx[y][x], object = cave_o_idx[y][x];
        /* Flooding leaves ordinary objects submerged in place. Creatures
         * still move to safe banks; important objects remain protected. */
        if (lm_feature == FEAT_WATER && !lm_critical_object(y, x)) object = 0;
        if (lm_work[y][x] != TERRAIN_LANDMARK_TERRAIN || (!monster && !object)) continue;
        if (monster < 0 || lm_move_count >= LM_MOVES || lm_critical_object(y, x)) return false;
        int owner = terrain_vault_instance_at(y, x), pi = level_partition_index_for_point(y, x);
        int best = INT_MAX; coord target = { 0, 0 };
        for (int yy = MAX(1, y - 40); yy <= MIN(p_ptr->cur_map_hgt - 2, y + 40); yy++)
            for (int xx = MAX(1, x - 40); xx <= MIN(p_ptr->cur_map_wid - 2, x + 40); xx++)
            {
                if (lm_shadow[yy][xx] != FEAT_FLOOR || lm_taken[yy][xx]
                    || cave_m_idx[yy][xx] || cave_o_idx[yy][xx]
                    || level_partition_index_for_point(yy, xx) != pi) continue;
                int target_owner = terrain_vault_instance_at(yy, xx);
                bool evacuation = owner >= 0 && target_owner < 0
                    && (terrain_vault_policy_at(y, x) & TERRAIN_VAULT_FLOOD)
                    && ABS(y - yy) + ABS(x - xx) <= 12;
                if (target_owner != owner && !evacuation) continue;
                if (lm_before[y][x] && lm_before[yy][xx] != lm_before[y][x]) continue;
                int score = ABS(y - yy) + ABS(x - xx);
                if (evacuation) score += 40; /* Keep contents inside if possible. */
                if (lm_work[yy][xx] == TERRAIN_LANDMARK_BRIDGE) score += 3;
                if (score < best) { best = score; target = (coord){ yy, xx }; }
            }
        if (best == INT_MAX) return false;
        lm_taken[target.y][target.x] = 1;
        lm_moves[lm_move_count++] = (landmark_move){ { y, x }, target, monster, object };
    }
    return true;
}

static void lm_commit(void)
{
    for (int i = 0; i < lm_move_count; i++)
    {
        landmark_move m = lm_moves[i];
        if (m.monster > 0)
        {
            cave_m_idx[m.from.y][m.from.x] = 0; cave_m_idx[m.to.y][m.to.x] = m.monster;
            mon_list[m.monster].fy = m.to.y; mon_list[m.monster].fx = m.to.x;
            lm_stats.relocated_monsters++;
        }
        if (m.object > 0)
        {
            cave_o_idx[m.from.y][m.from.x] = 0; cave_o_idx[m.to.y][m.to.x] = m.object;
            for (int j = m.object; j > 0; j = o_list[j].next_o_idx)
            { o_list[j].iy = m.to.y; o_list[j].ix = m.to.x; lm_stats.relocated_objects++; }
        }
    }
    static byte seen_vault[LM_CELLS]; memset(seen_vault, 0, sizeof(seen_vault));
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
    {
        if (!lm_work[y][x]) continue;
        int original = cave_feat[y][x];
        bool original_passage = lm_passage(y, x);
        if (lm_work[y][x] == TERRAIN_LANDMARK_BRIDGE
            || (lm_work[y][x] == TERRAIN_LANDMARK_REPAIR && lm_hydraulic[y][x]))
        {
            bool vertical = cave_feat_is_bridge(lm_shadow[y][x]) ? cave_bridge_vertical(lm_shadow[y][x])
                : cave_feat_is_bridge(original) ? cave_bridge_vertical(original)
                : lm_work[y - 1][x] == TERRAIN_LANDMARK_REPAIR || lm_work[y + 1][x] == TERRAIN_LANDMARK_REPAIR;
            for (int b = 0; b < lm_bridge_count; b++)
                for (int i = 0; i < lm_bridges[b].length; i++)
                    if (lm_bridges[b].first.y + i * lm_bridges[b].dy == y
                        && lm_bridges[b].first.x + i * lm_bridges[b].dx == x) vertical = lm_bridges[b].dy != 0;
            lm_shadow[y][x] = cave_bridge_feature(lm_feature, vertical);
        }
        if (lm_shadow[y][x] != original)
        {
            cave_set_feat(y, x, lm_shadow[y][x]);
            if (lm_rock(original)) { lm_stats.excavated++; cave_natural[y][x] = 1; }
            if (original_passage) lm_stats.original_floor++;
            /* These are real new caves/banks, available to later population. */
            cave_info[y][x] |= CAVE_ROOM;
        }
        lm_live[y][x] = lm_work[y][x] == TERRAIN_LANDMARK_REPAIR && lm_hydraulic[y][x]
            ? TERRAIN_LANDMARK_BRIDGE : lm_work[y][x];
        lm_live_terminal[y][x] = lm_terminal[y][x];
        lm_live_structure[y][x] = terrain_structure_cell(y, x);
        if (lm_wet(y, x))
        {
            lm_live_basin[y][x] = lm_basin[y][x];
            lm_live_channel[y][x] = lm_channel[y][x] && !lm_basin[y][x];
        }
        if (lm_work[y][x] == TERRAIN_LANDMARK_TERRAIN)
        {
            lm_stats.tiles++;
            int instance = terrain_vault_instance_at(y, x);
            if (instance > 0 && instance < LM_CELLS && !seen_vault[instance])
            { seen_vault[instance] = 1; lm_stats.vaults++; }
        }
        int pi = level_partition_index_for_point(y, x); if (pi >= 0 && pi < 25) lm_partitions[pi] = true;
        if (lm_work[y][x] == TERRAIN_LANDMARK_REPAIR || lm_work[y][x] == TERRAIN_LANDMARK_BRIDGE
            || lm_terminal[y][x]) terrain_generation_reserve(y, x);
        if (lm_terminal[y][x] && lm_terminal[y][x] != TERRAIN_TERMINAL_BASIN
            && lm_terminal[y][x] != TERRAIN_TERMINAL_EDGE)
            for (int yy = MAX(1, y - 2); yy <= MIN(p_ptr->cur_map_hgt - 2, y + 2); yy++)
                for (int xx = MAX(1, x - 2); xx <= MIN(p_ptr->cur_map_wid - 2, x + 2); xx++)
                    if (lm_rock(cave_feat[yy][xx])) terrain_generation_reserve(yy, xx);
    }
    for (int i = 0; i < lm_bridge_count; i++)
        for (int j = -2; j < lm_bridges[i].length + 2; j++)
        {
            int y = lm_bridges[i].first.y + j * lm_bridges[i].dy;
            int x = lm_bridges[i].first.x + j * lm_bridges[i].dx;
            terrain_generation_reserve(y, x);
        }
    lm_stats.accepted = 1; lm_stats.bridges = lm_bridge_count;
    lm_stats.family = lm_family; lm_stats.lakes = lm_stats.basin_count;
    lm_stats.tributaries = lm_stats.confluences = lm_junctions;
    lm_stats.bridge_savings = lm_bridge_savings;
    lm_stats.flooded_structures = lm_structures.flooded;
    lm_stats.breached_structures = lm_structures.breached;
    lm_stats.repaired_structures = lm_structures.repaired;
    lm_stats.rubble_tiles = lm_structures.rubble_tiles;
    lm_stats.repair_tiles = lm_structures.repair_tiles;
    lm_stats.kind = lm_feature == FEAT_CHASM ? TERRAIN_LANDMARK_RIFT
        : lm_stats.basin_count ? TERRAIN_LANDMARK_LAKE : TERRAIN_LANDMARK_RIVER;
    log_debug("Underground network: material=%d family=%d span=%d%% tiles=%d basins=%d thin=%d/%d bridges=%d vaults=%d granite=%d",
        lm_stats.material, lm_stats.family, lm_stats.span, lm_stats.tiles, lm_stats.basin_count,
        lm_stats.narrow_cells, lm_stats.channel_cells, lm_stats.bridges, lm_stats.vaults, lm_stats.excavated);
}

bool place_terrain_landmark(void)
{
    lm_preparing = lm_preserve_form = false; lm_epoch = TERRAIN_HISTORY_DISASTER; lm_system = -1;
    terrain_landmark_reset();
    const terrain_landmark_profile* profile = terrain_landmark_for_depth(p_ptr->depth);
    const terrain_theme_profile* theme = terrain_theme_for_depth(p_ptr->depth);
    const terrain_network_profile* network = terrain_network_for_depth(p_ptr->depth);
    if (!dun || current_partition_count < 1 || !percent_chance(profile->chance)) return false;
    int total = 0; for (int i = 0; i < 5; i++) total += theme->weights[i];
    if (!total) return false;
    int roll = rand_int(total), material = 0;
    for (; material < 4; material++) { if (roll < theme->weights[material]) break; roll -= theme->weights[material]; }
    lm_stats.material = material; lm_feature = lm_features[material];
    total = 0; for (int i = 0; i < TERRAIN_NETWORK_FAMILY_MAX; i++) total += network->weights[i];
    if (!total) return false;
    roll = rand_int(total); lm_family = 0;
    for (; lm_family < TERRAIN_NETWORK_FAMILY_MAX - 1; lm_family++)
    { if (roll < network->weights[lm_family]) break; roll -= network->weights[lm_family]; }
    if (lm_feature == FEAT_CHASM) lm_family = TERRAIN_LANDMARK_FRACTURE_FAMILY;
    lm_stats.family = lm_family;
    lm_context();
    terrain_generation_components((const byte (*)[MAX_DUNGEON_WID])cave_feat, lm_before);
    for (int attempt = 0; attempt < LM_ATTEMPTS; attempt++)
    {
        /* A failed large scenario does not spend all attempts repeating the
         * same failure and leave an otherwise usable level almost empty. */
        if ((attempt == 8 || attempt == 16) && lm_feature != FEAT_CHASM)
        {
            int next = attempt == 16 ? TERRAIN_NETWORK_POOLS
                : lm_family == TERRAIN_NETWORK_CHAIN ? TERRAIN_NETWORK_CHAMBER : TERRAIN_NETWORK_CHAIN;
            if (network->weights[next] && next != lm_family)
            { lm_family = next; lm_stats.scenario_retries++; }
        }
        lm_stats.attempted++;
        lm_bridge_count = lm_basin_count = lm_junctions = lm_bridge_savings = 0;
        memset(lm_work, 0, sizeof(lm_work)); memset(lm_basin, 0, sizeof(lm_basin));
        memset(lm_channel, 0, sizeof(lm_channel)); memset(lm_cavity, 0, sizeof(lm_cavity));
        memset(lm_terminal, 0, sizeof(lm_terminal)); memset(lm_terminal_role, 0, sizeof(lm_terminal_role));
        if (!lm_network_shape(profile, network)) { lm_stats.rejected_route++; continue; }
        lm_prune_shapes(); lm_banks();
        lm_structure_proposal();
        bool connected = lm_access();
        while (!connected && lm_bridge(true)) connected = lm_access();
        if (!connected) { lm_stats.rejected_access++; continue; }
        /* Optional architecture must serve a useful dry route. This is a cap,
         * not a quota: a one-cell stream normally remains a simple crossing. */
        int optional = rand_range(1, 4);
        while (optional-- > 0 && lm_bridge(false)) connected = lm_access();
        if (!connected) { lm_stats.rejected_access++; continue; }
        if (!lm_validate_terminals()) { lm_stats.rejected_route++; continue; }
        if (!lm_shape_statistics(profile)) { lm_stats.rejected_route++; continue; }
        if (!lm_relocations()) { lm_stats.rejected_contents++; continue; }
        lm_commit(); return true;
    }
    log_debug("Major terrain skipped after %d bounded proposals (route=%d access=%d contents=%d)",
        lm_stats.attempted, lm_stats.rejected_route, lm_stats.rejected_access, lm_stats.rejected_contents);
    return false;
}

static void lm_save_plan(landmark_plan* p)
{
    p->stats = lm_stats; p->stats.family = lm_family;
    p->basin_count = lm_basin_count; p->junctions = lm_junctions;
    memcpy(p->work, lm_work, sizeof(p->work));
    memcpy(p->basin, lm_basin, sizeof(p->basin));
    memcpy(p->channel, lm_channel, sizeof(p->channel));
    memcpy(p->cavity, lm_cavity, sizeof(p->cavity));
    memcpy(p->terminal, lm_terminal, sizeof(p->terminal));
    memcpy(p->role, lm_terminal_role, sizeof(p->role));
    memcpy(p->basins, lm_basins, sizeof(p->basins));
    memcpy(p->merges, lm_merges, sizeof(p->merges));
    p->ready = true;
}

static void lm_load_plan(const landmark_plan* p)
{
    terrain_landmark_reset(); lm_stats = p->stats;
    lm_family = p->stats.family; lm_feature = lm_features[p->stats.material];
    lm_basin_count = p->basin_count; lm_junctions = p->junctions;
    lm_bridge_count = lm_bridge_savings = 0;
    memcpy(lm_work, p->work, sizeof(lm_work));
    memcpy(lm_basin, p->basin, sizeof(lm_basin));
    memcpy(lm_channel, p->channel, sizeof(lm_channel));
    memcpy(lm_cavity, p->cavity, sizeof(lm_cavity));
    memcpy(lm_terminal, p->terminal, sizeof(lm_terminal));
    memcpy(lm_terminal_role, p->role, sizeof(lm_terminal_role));
    memcpy(lm_basins, p->basins, sizeof(lm_basins));
    memcpy(lm_merges, p->merges, sizeof(lm_merges));
}

void terrain_landmark_plans_reset(void)
{
    memset(lm_plans, 0, sizeof(lm_plans));
    lm_system = -1; lm_preparing = lm_preserve_form = false;
    lm_epoch = TERRAIN_HISTORY_DISASTER;
    terrain_landmark_reset();
}

bool terrain_landmark_plan_system(int system, int material)
{
    if (system < 0 || system >= TERRAIN_HISTORY_SYSTEMS || material < 0 || material >= 5) return false;
    const terrain_network_profile* network = terrain_network_for_depth(p_ptr->depth);
    const terrain_landmark_profile* profile = terrain_landmark_for_depth(p_ptr->depth);
    terrain_landmark_reset(); lm_system = system;
    lm_preparing = lm_preserve_form = true; lm_epoch = TERRAIN_HISTORY_ANCIENT;
    lm_feature = lm_features[material]; lm_stats.material = material;
    int total = 0; for (int i = 0; i < TERRAIN_NETWORK_FAMILY_MAX; i++) total += network->weights[i];
    if (!total) return false;
    int roll = rand_int(total); lm_family = 0;
    for (; lm_family < TERRAIN_NETWORK_FAMILY_MAX - 1; lm_family++)
    { if (roll < network->weights[lm_family]) break; roll -= network->weights[lm_family]; }
    if (material == TERRAIN_THEME_CHASM) lm_family = TERRAIN_LANDMARK_FRACTURE_FAMILY;
    lm_context();
    for (int attempt = 0; attempt < LM_ATTEMPTS; attempt++)
    {
        lm_stats.attempted++;
        lm_bridge_count = lm_basin_count = lm_junctions = lm_bridge_savings = 0;
        memset(lm_work, 0, sizeof(lm_work)); memset(lm_basin, 0, sizeof(lm_basin));
        memset(lm_channel, 0, sizeof(lm_channel)); memset(lm_cavity, 0, sizeof(lm_cavity));
        memset(lm_terminal, 0, sizeof(lm_terminal)); memset(lm_terminal_role, 0, sizeof(lm_terminal_role));
        if (!lm_network_shape(profile, network)) continue;
        lm_prune_shapes(); lm_banks();
        if (!lm_validate_terminals() || !lm_shape_statistics(profile)) continue;
        landmark_plan* p = &lm_plans[system];
        lm_save_plan(p);
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
            if (lm_work[y][x] == TERRAIN_LANDMARK_TERRAIN) p->initial[y][x] = lm_feature;
        for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++) for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
            if (lm_terminal[y][x] && lm_terminal[y][x] != TERRAIN_TERMINAL_BASIN && lm_terminal[y][x] != TERRAIN_TERMINAL_EDGE)
                for (int yy = y - 2; yy <= y + 2; yy++) for (int xx = x - 2; xx <= x + 2; xx++)
                    if (in_bounds_fully(yy, xx) && !lm_work[yy][xx] && lm_rock(cave_feat[yy][xx])) p->cap[yy][xx] = cave_feat[yy][xx];
        lm_preparing = false; return true;
    }
    lm_preparing = false; return false;
}

void terrain_landmark_foundation(int system)
{
    landmark_plan* p = &lm_plans[system];
    if (!p->ready) return;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        if (p->work[y][x])
        {
            cave_set_feat(y, x, p->work[y][x] == TERRAIN_LANDMARK_TERRAIN ? lm_features[p->stats.material] : FEAT_FLOOR);
            cave_natural[y][x] = 1;
        }
    /* Shore anchors let the normal corridor planner reach the old geology.
     * The anchor is dry ground, never an assumed route through the liquid. */
    for (int b = 0; b < p->basin_count && dun->cent_n < room_capacity_limit(); b++)
    {
        coord best = {0, 0}; int score = LM_CELLS;
        for (int y = MAX(2, p->basins[b].y - 14); y < MIN(p_ptr->cur_map_hgt - 2, p->basins[b].y + 15); y++)
            for (int x = MAX(2, p->basins[b].x - 14); x < MIN(p_ptr->cur_map_wid - 2, p->basins[b].x + 15); x++)
            {
                if (cave_feat[y][x] != FEAT_FLOOR) continue;
                int dist = distance(y, x, p->basins[b].y, p->basins[b].x);
                if (dist < score) { score = dist; best = (coord){y, x}; }
            }
        if (!best.y) continue;
        int i = dun->cent_n++;
        dun->cent[i] = best;
        p->cap[best.y][best.x] = FEAT_FLOOR;
        cave_info[best.y][best.x] |= CAVE_ROOM;
        dun->corner[i] = (rectangle){best.y - 1, best.x - 1, best.y + 1, best.x + 1};
        dun->kind[i] = ROOM_KIND_CLASSIC; dun->is_quest[i] = false;
        mark_room_anchor_meta(i, LAYOUT_ANCHOR_CA_BLOB, false);
    }
}

bool terrain_landmark_realize(int system, int epoch)
{
    landmark_plan* p = &lm_plans[system];
    if (!p->ready) return false;
    lm_system = system; lm_epoch = epoch; lm_preparing = false; lm_preserve_form = true;
    const terrain_landmark_profile* profile = terrain_landmark_for_depth(p_ptr->depth);
    const terrain_network_profile* network = terrain_network_for_depth(p_ptr->depth);
    for (int attempt = 0; attempt < (epoch == TERRAIN_HISTORY_DISASTER ? LM_ATTEMPTS : 2); attempt++)
    {
        lm_load_plan(p); lm_context();
        terrain_generation_components((const byte (*)[MAX_DUNGEON_WID])cave_feat, lm_before);
        if (attempt && epoch == TERRAIN_HISTORY_DISASTER)
        {
            lm_basin_count = lm_junctions = 0;
            memset(lm_work, 0, sizeof(lm_work)); memset(lm_basin, 0, sizeof(lm_basin));
            memset(lm_channel, 0, sizeof(lm_channel)); memset(lm_cavity, 0, sizeof(lm_cavity));
            memset(lm_terminal, 0, sizeof(lm_terminal)); memset(lm_terminal_role, 0, sizeof(lm_terminal_role));
            if (!lm_network_shape(profile, network)) continue;
        }
        bool clipped = false;
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
            if (lm_work[y][x] && lm_hard[y][x])
            {
                if (epoch != TERRAIN_HISTORY_DISASTER && (cave_feat[y][x] == lm_feature
                    || cave_bridge_underlay(cave_feat[y][x]) == lm_feature))
                { lm_hard[y][x] = 0; continue; }
                if (lm_work[y][x] == TERRAIN_LANDMARK_TERRAIN && cave_feat[y][x] != lm_feature
                    && cave_bridge_underlay(cave_feat[y][x]) != lm_feature) clipped = true;
                lm_work[y][x] = 0;
            }
        if (clipped) continue;
        lm_banks();
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
            if (p->work[y][x] == TERRAIN_LANDMARK_TERRAIN && cave_feat_is_bridge(cave_feat[y][x])
                && cave_bridge_underlay(cave_feat[y][x]) == lm_feature)
            { lm_work[y][x] = TERRAIN_LANDMARK_BRIDGE; lm_shadow[y][x] = cave_feat[y][x]; }
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (lm_work[y][x] != TERRAIN_LANDMARK_BRIDGE) continue;
            int dy = cave_bridge_vertical(cave_feat[y][x]), dx = 1 - dy;
            if (lm_work[y - dy][x - dx] == TERRAIN_LANDMARK_BRIDGE
                && cave_feat[y - dy][x - dx] == cave_feat[y][x]) continue;
            int length = 1;
            while (in_bounds_fully(y + length * dy, x + length * dx)
                && lm_work[y + length * dy][x + length * dx] == TERRAIN_LANDMARK_BRIDGE
                && cave_feat[y + length * dy][x + length * dx] == cave_feat[y][x]) length++;
            if (lm_bridge_count < LM_BRIDGES)
                lm_bridges[lm_bridge_count++] = (landmark_bridge){(coord){y, x}, dy, dx, length};
        }
        /* An overflow that cannot fit safely retains the preexisting river. */
        if (epoch == TERRAIN_HISTORY_OVERFLOW)
        {
            if (attempt) lm_epoch = TERRAIN_HISTORY_ANCIENT;
            else lm_raise_basins();
        }
        lm_structure_proposal();
        bool connected = lm_access();
        while (!connected && lm_bridge(true)) connected = lm_access();
        if (!connected) continue;
        int optional = rand_range(1, 4);
        while (optional-- && lm_bridge(false)) connected = lm_access();
        if (!connected || !lm_validate_terminals() || !lm_shape_statistics(profile) || !lm_relocations()) continue;
        lm_stats.attempted = attempt + 1;
        lm_commit(); lm_save_plan(p);
        memcpy(p->structure, lm_live_structure, sizeof(p->structure));
        if (epoch == TERRAIN_HISTORY_DISASTER)
        {
            memset(p->cap, 0, sizeof(p->cap));
            for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++) for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
                if (p->terminal[y][x] && p->terminal[y][x] != TERRAIN_TERMINAL_BASIN
                    && p->terminal[y][x] != TERRAIN_TERMINAL_EDGE)
                    for (int yy = y - 2; yy <= y + 2; yy++) for (int xx = x - 2; xx <= x + 2; xx++)
                        if (in_bounds_fully(yy, xx) && lm_rock(cave_feat[yy][xx])) p->cap[yy][xx] = cave_feat[yy][xx];
        }
        return true;
    }
    return false;
}

int terrain_landmark_planned_feature(int system, int y, int x)
{
    if (system < 0 || system >= TERRAIN_HISTORY_SYSTEMS || !in_bounds_fully(y, x)) return 0;
    landmark_plan* p = &lm_plans[system];
    return p->ready && p->work[y][x] == TERRAIN_LANDMARK_TERRAIN ? lm_features[p->stats.material] : 0;
}
int terrain_landmark_planned_cap(int system, int y, int x)
{ return system >= 0 && system < TERRAIN_HISTORY_SYSTEMS && in_bounds_fully(y, x) ? lm_plans[system].cap[y][x] : 0; }
const terrain_landmark_stats* terrain_landmark_system_stats(int system)
{ return system >= 0 && system < TERRAIN_HISTORY_SYSTEMS ? &lm_plans[system].stats : NULL; }
int terrain_landmark_system_cell(int system, int y, int x, int field)
{
    if (system < 0 || system >= TERRAIN_HISTORY_SYSTEMS || !in_bounds_fully(y, x)) return 0;
    landmark_plan* p = &lm_plans[system];
    switch (field) {
    case 0: return p->work[y][x] == TERRAIN_LANDMARK_REPAIR && cave_feat_is_bridge(cave_feat[y][x])
        ? TERRAIN_LANDMARK_BRIDGE : p->work[y][x];
    case 1: return p->basin[y][x];
    case 2: return p->channel[y][x]; case 3: return p->terminal[y][x];
    case 4: return p->structure[y][x]; case 5: return p->role[y][x];
    case 6: return p->cap[y][x]; case 7: return p->initial[y][x]; default: return 0; }
}

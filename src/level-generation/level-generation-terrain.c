#include "angband.h"
#include "level-generation/level-generation-terrain-history.h"
#include "level-generation/level-generation-internal.h"
#include "level-generation/level-generation-terrain.h"
#include "level-generation/level-generation-terrain-access.h"
#include "level-generation/level-generation-landmarks.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-bridge.h"
#include "cave/cave-water-flow.h"
#include <limits.h>

/* A region gets a complete channel, not a sequence of independent tile edits.
 * All scratch state is generation-only; authored terrain and partition chasms
 * keep their existing representation and behavior. */
#define TERRAIN_PATH_MAX 96
#define TERRAIN_SHAPE_MAX 384
#define TERRAIN_ATTEMPTS 24
#define TERRAIN_CELLS (MAX_DUNGEON_HGT * MAX_DUNGEON_WID)
/* Deep water is a patch within a broad water body, rather than a blanket
 * recolour of every tile away from the shore.  Keep small streams shallow. */
#define DEEP_WATER_MIN_CORE 6
#define DEEP_WATER_MIN_PERCENT 45
#define DEEP_WATER_MAX_PERCENT 65

enum terrain_role { TERRAIN_BLOCKED, TERRAIN_NATURAL, TERRAIN_ROCK, TERRAIN_LAB };
typedef struct terrain_candidate
{
    coord path[TERRAIN_PATH_MAX];
    int path_count;
    coord cells[TERRAIN_SHAPE_MAX];
    int count, feature, partition, bridges;
    bool endpoint_pool;
    coord bridge_approaches[7];
    int bridge_approach_count;
} terrain_candidate;

static const int channel_dy[4] = { -1, 0, 1, 0 };
static const int channel_dx[4] = { 0, 1, 0, -1 };
static const int terrain_features[TERRAIN_THEME_MATERIAL_MAX] = {
    FEAT_WATER, FEAT_CHASM, FEAT_LAVA, FEAT_POISON, FEAT_ICE
};
static byte roles[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte protected_cells[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte melting_ice_candidates[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte reserved[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
/* 1: terrain, 2: an intentional architectural crossing. */
static byte proposed[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte shadow[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte deep_water_core[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte deep_water_seen[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte deep_water_selected[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte deep_water_frontier[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int baseline[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int after[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static terrain_generation_stats terrain_stats;

void terrain_generation_reset(void)
{
    memset(reserved, 0, sizeof(reserved));
    memset(roles, 0, sizeof(roles));
    memset(protected_cells, 0, sizeof(protected_cells));
    memset(melting_ice_candidates, 0, sizeof(melting_ice_candidates));
    memset(proposed, 0, sizeof(proposed));
    memset(&terrain_stats, 0, sizeof(terrain_stats));
    terrain_landmark_reset();
}

bool terrain_generation_reserved(int y, int x)
{
    return in_bounds_fully(y, x) && reserved[y][x];
}

void terrain_generation_reserve(int y, int x)
{
    if (in_bounds_fully(y, x)) reserved[y][x] = 1;
}

const terrain_generation_stats* terrain_generation_last_stats(void)
{
    return &terrain_stats;
}

static bool terrain_is_rock(int feat)
{
    return feat == FEAT_WALL_EXTRA || feat == FEAT_WALL_OUTER
        || feat == FEAT_QUARTZ;
}

static bool terrain_is_gap(int feat)
{
    return feat == FEAT_CHASM || feat == FEAT_LAVA || feat == FEAT_POISON;
}

static bool terrain_architectural_partition(int pi)
{
    return pi >= 0 && pi < current_partition_count
        && current_partition_modes[pi] == QUAD_MODE_LABYRINTH;
}

static void terrain_context(void)
{
    /* Natural floor tags are useful, but an authored room built over a cave
     * must win over an old tag. Its registered envelope includes its walls. */
    for (int i = 0; i < dun->cent_n; i++)
    {
        int pi = level_partition_index_for_point(dun->cent[i].y, dun->cent[i].x);
        bool generated_cave = room_anchor_kind[i] == LAYOUT_ANCHOR_CA_BLOB;
        bool generated_lab = terrain_architectural_partition(pi)
            && room_anchor_kind[i] == LAYOUT_ANCHOR_BSP_SLICE;
        if (!dun->is_quest[i] && (generated_cave || generated_lab)) continue;
        rectangle b = dun->corner[i];
        for (int y = b.y1; y <= b.y2; y++)
            for (int x = b.x1; x <= b.x2; x++)
                if (in_bounds_fully(y, x)) protected_cells[y][x] = 1;
    }
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            int feat = cave_feat[y][x];
            int pi = level_partition_index_for_point(y, x);
            if (pi < 0 || pi >= current_partition_count
                || current_partition_modes[pi] == QUAD_MODE_CHASM
                || (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT
                    | CAVE_CHASM_AREA | CAVE_MORGOTH_TUNNEL))
                || coord_in_morgoth_region(y, x, 0)
                || cave_m_idx[y][x] || cave_fixture_at(y, x))
                protected_cells[y][x] = 1;
            /* Existing natural ice (including landmark lakes) may thaw, but
             * authored room envelopes and their occupied cells stay intact. */
            if (feat == FEAT_ICE && !protected_cells[y][x]
                && !cave_o_idx[y][x] && !generation_escape_tunnel_bold(y, x))
                melting_ice_candidates[y][x] = 1;
            if (feat != FEAT_FLOOR && !terrain_is_rock(feat))
            {
                protected_cells[y][x] = 1;
                /* Preserve the functional approach to fixed features. Existing
                 * liquids/chasm boundaries need no arbitrary exclusion halo. */
                if (!terrain_is_gap(feat) && feat != FEAT_WATER && feat != FEAT_ICE
                    && feat != FEAT_WALL_PERM && feat != FEAT_WALL_INNER
                    && feat != FEAT_WALL_SOLID)
                    for (int d = 0; d < 4; d++)
                        protected_cells[y + channel_dy[d]][x + channel_dx[d]] = 1;
            }
        }
    /* Keep actual room/tunnel attachment anchors, not a broad dry circle. */
    for (int i = 0; i < dun->cent_n; i++)
        if (in_bounds_fully(dun->cent[i].y, dun->cent[i].x))
        {
            protected_cells[dun->cent[i].y][dun->cent[i].x] = 1;
            melting_ice_candidates[dun->cent[i].y][dun->cent[i].x] = 0;
        }

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            int pi = level_partition_index_for_point(y, x);
            if (protected_cells[y][x] || pi < 0 || pi >= current_partition_count
                || generation_escape_tunnel_bold(y, x)) continue;
            if (cave_feat[y][x] != FEAT_FLOOR) continue;
            if (terrain_architectural_partition(pi) && (cave_info[y][x] & CAVE_ROOM))
                roles[y][x] = TERRAIN_LAB;
            else if (cave_natural[y][x]
                || (current_partition_modes[pi] == QUAD_MODE_BIG_CAVE
                    && (cave_info[y][x] & CAVE_ROOM)))
                roles[y][x] = TERRAIN_NATURAL;
        }
    /* Short links may excavate natural separators. Do not infer masonry from
     * a style color or let a channel punch through a labyrinth wall. */
    for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++)
        for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
        {
            int pi = level_partition_index_for_point(y, x);
            bool natural = false, masonry = false;
            if (protected_cells[y][x] || !terrain_is_rock(cave_feat[y][x])
                || pi < 0 || pi >= current_partition_count
                || current_partition_modes[pi] != QUAD_MODE_CAVEY) continue;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                {
                    if (roles[y + dy][x + dx] == TERRAIN_NATURAL) natural = true;
                    if (protected_cells[y + dy][x + dx]
                        || ((cave_info[y + dy][x + dx] & CAVE_ROOM)
                            && cave_feat[y + dy][x + dx] == FEAT_FLOOR
                            && !cave_natural[y + dy][x + dx])) masonry = true;
                }
            if (natural && !masonry) roles[y][x] = TERRAIN_ROCK;
        }
}

static bool terrain_cell(int y, int x, int pi, bool excavate)
{
    if (!in_bounds_fully(y, x) || protected_cells[y][x] || reserved[y][x]
        || level_partition_index_for_point(y, x) != pi) return false;
    if (cave_feat[y][x] == FEAT_FLOOR)
        return roles[y][x] == TERRAIN_NATURAL || roles[y][x] == TERRAIN_LAB;
    return excavate && roles[y][x] == TERRAIN_ROCK && terrain_is_rock(cave_feat[y][x]);
}

/* Component labels use reversible walking/jump edges: a landing must also
 * allow a way back. Existing separate vault/chasm components are not joined
 * by fiat, and a one-cell gap with genuine run-up room needs no bridge. */
static int terrain_components(const byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int labels[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    static int queue[TERRAIN_CELLS];
    int count = 0;
    memset(labels, 0, sizeof(int) * TERRAIN_CELLS);
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            int head = 0, tail = 0;
            if (labels[y][x] || !terrain_generation_walkable(y, x, features)) continue;
            labels[y][x] = ++count;
            queue[tail++] = y * MAX_DUNGEON_WID + x;
            while (head < tail)
            {
                int cell = queue[head++], cy = cell / MAX_DUNGEON_WID;
                int cx = cell % MAX_DUNGEON_WID;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        int ny = cy + dy, nx = cx + dx;
                        if (!dy && !dx) continue;
                        if (!terrain_generation_walkable(ny, nx, features))
                        {
                            if (!terrain_generation_jump(cy, cx, dy, dx, features)
                                || !terrain_generation_jump(cy + 2 * dy,
                                    cx + 2 * dx, -dy, -dx, features)) continue;
                            ny += dy; nx += dx;
                        }
                        if (labels[ny][nx]) continue;
                        labels[ny][nx] = count;
                        queue[tail++] = ny * MAX_DUNGEON_WID + nx;
                    }
            }
        }
    return count;
}

int terrain_generation_components(const byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int labels[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    return terrain_components(features, labels);
}

static bool terrain_preserves_access(void)
{
    static int component_match[TERRAIN_CELLS + 1];
    memset(component_match, 0, sizeof(component_match));
    terrain_components(shadow, after);
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            int original = baseline[y][x], changed = after[y][x];
            if (!original || !changed) continue;
            if (component_match[original] && component_match[original] != changed)
                return false;
            component_match[original] = changed;
        }
    return true;
}

/* Bounded A* with a coherent bent preferred course. Its cost field varies in
 * patches, not independently every step; positive costs prevent self-loops. */
static int route_dist[TERRAIN_CELLS], route_parent[TERRAIN_CELLS];
static int heap[TERRAIN_CELLS], heap_pos[TERRAIN_CELLS], heap_count;
static coord route_end;

static int route_priority(int cell)
{
    return route_dist[cell] + 10 * (ABS(cell / MAX_DUNGEON_WID - route_end.y)
        + ABS(cell % MAX_DUNGEON_WID - route_end.x));
}

static void route_heap_up(int pos)
{
    int cell = heap[pos];
    while (pos > 0)
    {
        int parent = (pos - 1) / 2;
        if (route_priority(heap[parent]) <= route_priority(cell)) break;
        heap[pos] = heap[parent]; heap_pos[heap[pos]] = pos; pos = parent;
    }
    heap[pos] = cell; heap_pos[cell] = pos;
}

static int route_heap_pop(void)
{
    int result = heap[0], pos = 0, cell = heap[--heap_count];
    heap_pos[result] = -2;
    if (!heap_count) return result;
    while (pos * 2 + 1 < heap_count)
    {
        int child = pos * 2 + 1;
        if (child + 1 < heap_count
            && route_priority(heap[child + 1]) < route_priority(heap[child])) child++;
        if (route_priority(cell) <= route_priority(heap[child])) break;
        heap[pos] = heap[child]; heap_pos[heap[pos]] = pos; pos = child;
    }
    heap[pos] = cell; heap_pos[cell] = pos;
    return result;
}

static int terrain_route(coord start, coord end, int pi, int feature,
    int max_length, coord* path)
{
    unsigned salt = (unsigned)rand_int(65536);
    bool horizontal = ABS(end.x - start.x) >= ABS(end.y - start.y);
    int span = horizontal ? (int)end.x - start.x : (int)end.y - start.y;
    int bend = rand_range(1, feature == FEAT_CHASM ? 3 : 5) * (one_in_(2) ? 1 : -1);
    int start_id = start.y * MAX_DUNGEON_WID + start.x;
    int end_id = end.y * MAX_DUNGEON_WID + end.x;
    bool excavate = !terrain_architectural_partition(pi);
    if (!span) return 0;
    for (int i = 0; i < TERRAIN_CELLS; i++)
    { route_dist[i] = INT_MAX; route_parent[i] = -1; heap_pos[i] = -1; }
    route_end = end; heap_count = 1; heap[0] = start_id;
    heap_pos[start_id] = 0; route_dist[start_id] = 0;
    while (heap_count)
    {
        int id = route_heap_pop(), y = id / MAX_DUNGEON_WID, x = id % MAX_DUNGEON_WID;
        if (id == end_id) break;
        for (int d = 0; d < 4; d++)
        {
            int ny = y + channel_dy[d], nx = x + channel_dx[d];
            int next = ny * MAX_DUNGEON_WID + nx;
            int along, linear, offset, preferred, cost, new_dist;
            unsigned noise;
            if (!terrain_cell(ny, nx, pi, excavate) || heap_pos[next] == -2) continue;
            if (ABS(ny - start.y) + ABS(nx - start.x) > max_length
                || ABS(ny - end.y) + ABS(nx - end.x) > max_length) continue;
            along = horizontal ? nx - start.x : ny - start.y;
            linear = horizontal ? start.y + ((int)end.y - start.y) * along / span
                : start.x + ((int)end.x - start.x) * along / span;
            offset = MIN(ABS(along), ABS(span - along));
            preferred = linear + bend * offset * 2 / ABS(span);
            noise = ((unsigned)(ny / 3) * 374761393u
                + (unsigned)(nx / 3) * 668265263u + salt) * 1274126177u;
            cost = 10 + MIN(16, ABS((horizontal ? ny : nx) - preferred) * 2)
                + (int)((noise >> 24) % 5);
            if (roles[ny][nx] == TERRAIN_ROCK) cost += 40;
            else if (!terrain_architectural_partition(pi)
                && ABS(ny - start.y) + ABS(nx - start.x) > 2
                && ABS(ny - end.y) + ABS(nx - end.x) > 2)
            {
                /* In a cavern, bring the channel into the chamber instead
                 * of tracing the wall merely because that route is shortest. */
                for (int side = 0; side < 4; side++)
                    if (terrain_is_rock(cave_feat[ny + channel_dy[side]][nx + channel_dx[side]]))
                        cost += 12;
            }
            new_dist = route_dist[id] + cost;
            if (new_dist >= route_dist[next]) continue;
            route_dist[next] = new_dist; route_parent[next] = id;
            if (heap_pos[next] < 0)
            { heap_pos[next] = heap_count; heap[heap_count++] = next; }
            route_heap_up(heap_pos[next]);
        }
    }
    if (route_parent[end_id] < 0) return 0;
    int count = 0, rock_run = 0, excavated = 0;
    for (int id = end_id; id >= 0; id = route_parent[id])
    {
        int y = id / MAX_DUNGEON_WID, x = id % MAX_DUNGEON_WID;
        if (count >= max_length || count >= TERRAIN_PATH_MAX) return 0;
        if (roles[y][x] == TERRAIN_ROCK)
        { if (++rock_run > 4 || ++excavated > 8) return 0; }
        else rock_run = 0;
        path[count++] = (coord){ y, x };
        if (id == start_id) break;
    }
    return count;
}

static bool terrain_mouth(int y, int x, int pi)
{
    int floor_neighbors = 0;
    bool wall = false;
    if (!terrain_cell(y, x, pi, false)) return false;
    for (int d = 0; d < 4; d++)
    {
        int ny = y + channel_dy[d], nx = x + channel_dx[d];
        if (terrain_cell(ny, nx, pi, false)) floor_neighbors++;
        if (terrain_is_rock(cave_feat[ny][nx]) || cave_feat[ny][nx] == FEAT_WALL_INNER
            || cave_feat[ny][nx] == FEAT_WALL_SOLID) wall = true;
    }
    return wall && floor_neighbors >= 2;
}

static bool terrain_inward_toward(coord mouth, coord target)
{
    int dy = (int)target.y - mouth.y, dx = (int)target.x - mouth.x;
    for (int d = 0; d < 4; d++)
        if (terrain_is_rock(cave_feat[mouth.y + channel_dy[d]][mouth.x + channel_dx[d]])
            && -dy * channel_dy[d] - dx * channel_dx[d] >= MAX(2, (ABS(dy) + ABS(dx)) / 4))
            return true;
    return false;
}

static bool terrain_endpoints(int pi, const terrain_theme_profile* profile,
    coord* start, coord* end, bool* endpoint_pool)
{
    int seen = 0;
    *endpoint_pool = percent_chance(profile->pool_chance);
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
            if (terrain_mouth(y, x, pi) && rand_int(++seen) == 0)
                *start = (coord){ y, x };
    if (!seen) return false;
    seen = 0;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            int dist = ABS(y - start->y) + ABS(x - start->x);
            coord target = { y, x };
            if (dist < profile->min_length - 1 || dist > profile->max_length - 1
                || !terrain_cell(y, x, pi, false)) continue;
            if (*endpoint_pool)
            {
                int banks = 0;
                for (int d = 0; d < 4; d++)
                    if (terrain_cell(y + channel_dy[d], x + channel_dx[d], pi, false)) banks++;
                if (banks < 3) continue;
            }
            else if (!terrain_mouth(y, x, pi)) continue;
            if (!terrain_architectural_partition(pi)
                && (!terrain_inward_toward(*start, target)
                    || (!*endpoint_pool && !terrain_inward_toward(target, *start)))) continue;
            if (rand_int(++seen) == 0) *end = (coord){ y, x };
        }
    return seen > 0;
}

static bool terrain_add_cell(terrain_candidate* c, int y, int x, bool excavate)
{
    if (!terrain_cell(y, x, c->partition, excavate)) return false;
    /* Water may cover ordinary floor items. Other materials retain the
     * existing item protection; never bury a note or an artefact. */
    if (cave_o_idx[y][x])
    {
        if (c->feature != FEAT_WATER) return false;
        for (int i = cave_o_idx[y][x], left = o_max; i > 0; i = o_list[i].next_o_idx)
            if (i >= o_max || --left <= 0 || o_list[i].name1 || o_list[i].tval == TV_NOTE)
                return false;
    }
    if (proposed[y][x]) return true;
    if (c->count >= TERRAIN_SHAPE_MAX) return false;
    proposed[y][x] = 1;
    shadow[y][x] = c->feature;
    c->cells[c->count++] = (coord){ y, x };
    return true;
}

static void terrain_shape(terrain_candidate* c, int width, bool pool)
{
    int side = one_in_(2) ? 1 : -1;
    c->count = c->bridges = c->bridge_approach_count = 0;
    memset(proposed, 0, sizeof(proposed));
    memcpy(shadow, cave_feat, sizeof(shadow));
    for (int i = 0; i < c->path_count; i++)
        terrain_add_cell(c, c->path[i].y, c->path[i].x, true);
    for (int i = 3; i < c->path_count - 3; i++)
    {
        coord p = c->path[i], prev = c->path[i - 1], next = c->path[i + 1];
        int dy = next.x != prev.x ? 1 : 0, dx = dy ? 0 : 1;
        /* Stretches, rather than white noise at each grid. Narrow throats
         * and endpoints remain legible and frequently jumpable. */
        if (width < 2 || (i / 4) % 3 == 0) continue;
        terrain_add_cell(c, p.y + dy * side, p.x + dx * side, false);
        if (width >= 3 && (i / 4) % 3 == 1)
            terrain_add_cell(c, p.y - dy * side, p.x - dx * side, false);
    }
    if ((pool || c->endpoint_pool) && c->feature != FEAT_CHASM)
    {
        coord seed = c->endpoint_pool ? c->path[0] : c->path[c->path_count - 1];
        /* An inland endpoint always retains its attached basin, including
         * when a wider candidate is narrowed for connectivity. */
        for (int radius = 1; radius <= 2; radius++)
            for (int dy = -radius; dy <= radius; dy++)
                for (int dx = -radius; dx <= radius; dx++)
                {
                    int y = seed.y + dy, x = seed.x + dx;
                    bool attached = false;
                    if (ABS(dy) + ABS(dx) > radius + (c->feature == FEAT_POISON)) continue;
                    if (!terrain_cell(y, x, c->partition, false)) continue;
                    for (int d = 0; d < 4; d++)
                        if (proposed[y + channel_dy[d]][x + channel_dx[d]]) attached = true;
                    if (attached) terrain_add_cell(c, y, x, false);
                }
    }
}

static bool terrain_architectural_bridge(terrain_candidate* c)
{
    if (!terrain_architectural_partition(c->partition) || c->bridges) return false;
    /* A short straight span follows an existing transverse maze passage.
     * It is never a random dry cut through an otherwise natural river. */
    for (int i = 3; i < c->path_count - 3; i++)
    {
        coord p = c->path[i], prev = c->path[i - 1], next = c->path[i + 1];
        int dy = next.x != prev.x ? 1 : 0, dx = dy ? 0 : 1;
        int lo = 0, hi = 0;
        if (!proposed[p.y][p.x]) continue;
        while (lo > -3 && in_bounds_fully(p.y + (lo - 1) * dy, p.x + (lo - 1) * dx)
            && proposed[p.y + (lo - 1) * dy][p.x + (lo - 1) * dx]) lo--;
        while (hi < 3 && in_bounds_fully(p.y + (hi + 1) * dy, p.x + (hi + 1) * dx)
            && proposed[p.y + (hi + 1) * dy][p.x + (hi + 1) * dx]) hi++;
        if (hi - lo + 1 > 3) continue;
        bool fits = true;
        for (int n = lo - 2; n <= hi + 2; n++)
        {
            int y = p.y + n * dy, x = p.x + n * dx;
            if (!in_bounds_fully(y, x) || roles[y][x] != TERRAIN_LAB
                || cave_feat[y][x] != FEAT_FLOOR || protected_cells[y][x]
                || ((n < lo || n > hi) && proposed[y][x])) fits = false;
        }
        if (!fits) continue;
        for (int n = lo; n <= hi; n++)
        {
            int y = p.y + n * dy, x = p.x + n * dx;
            proposed[y][x] = 2; shadow[y][x] = FEAT_FLOOR;
        }
        for (int n = lo - 2; n <= hi + 2; n++)
            c->bridge_approaches[c->bridge_approach_count++] =
                (coord){ p.y + n * dy, p.x + n * dx };
        c->bridges = 1;
        return true;
    }
    return false;
}

static bool terrain_candidate_valid(const terrain_candidate* c)
{
    /* A transverse span can separate a widening spur from its channel. Do
     * not publish isolated puddles/crack pixels as the price of a bridge. */
    for (int i = 0; i < c->count; i++)
    {
        coord p = c->cells[i];
        bool neighbor = false;
        if (proposed[p.y][p.x] != 1) continue;
        for (int d = 0; d < 4; d++)
            if (proposed[p.y + channel_dy[d]][p.x + channel_dx[d]] == 1)
                neighbor = true;
        if (!neighbor) return false;
    }
    return terrain_preserves_access();
}

static void terrain_reserve_crossings(const terrain_candidate* c)
{
    for (int i = 0; i < c->bridge_approach_count; i++)
        reserved[c->bridge_approaches[i].y][c->bridge_approaches[i].x] = 1;
    for (int i = 0; i < c->count; i++)
    {
        coord p = c->cells[i];
        if (proposed[p.y][p.x] == 2) reserved[p.y][p.x] = 1;
        if (!terrain_is_gap(cave_feat[p.y][p.x])) continue;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
            {
                int y = p.y - dy, x = p.x - dx;
                if ((!dy && !dx) || !terrain_generation_jump(y, x, dy, dx, NULL)) continue;
                reserved[y][x] = reserved[p.y + dy][p.x + dx] = 1;
                /* Reserve usable approach cells on the takeoff bank. This
                 * affects later rubble, not normal combat on the crossing. */
                for (int ay = -1; ay <= 1; ay++)
                    for (int ax = -1; ax <= 1; ax++)
                        if ((ay || ax) && terrain_generation_walkable(y + ay, x + ax, NULL))
                            reserved[y + ay][x + ax] = 1;
            }
    }
}

static void terrain_commit(terrain_candidate* c, int material)
{
    int painted = 0;
    for (int i = 0; i < c->count; i++)
    {
        coord p = c->cells[i];
        if (proposed[p.y][p.x] == 2) continue;
        bool excavated = terrain_is_rock(cave_feat[p.y][p.x]);
        cave_set_feat(p.y, p.x, c->feature); /* preserve this region's style */
        if (c->feature == FEAT_ICE) melting_ice_candidates[p.y][p.x] = 1;
        if (excavated)
        {
            cave_natural[p.y][p.x] = 1;
            cave_info[p.y][p.x] |= CAVE_ROOM;
        }
        painted++;
    }
    terrain_reserve_crossings(c);
    terrain_stats.accepted++;
    terrain_stats.tiles += painted;
    terrain_stats.bridges += c->bridges;
    terrain_stats.material_tiles[material] += painted;
    terrain_stats.material_features[material]++;
    memcpy(baseline, after, sizeof(baseline));
    log_debug("Terrain region %d: %s, length=%d tiles=%d architectural_spans=%d endpoints=(%d,%d)-(%d,%d)",
        c->partition, material == TERRAIN_THEME_CHASM ? "fracture"
            : material == TERRAIN_THEME_POISON ? "poison river"
            : material == TERRAIN_THEME_LAVA ? "lava river"
            : material == TERRAIN_THEME_ICE ? "frozen river" : "water river",
        c->path_count, painted, c->bridges, c->path[0].y, c->path[0].x,
        c->path[c->path_count - 1].y, c->path[c->path_count - 1].x);
}

static bool terrain_region(int pi, int material, const terrain_theme_profile* profile)
{
    terrain_candidate candidate;
    for (int attempt = 0; attempt < TERRAIN_ATTEMPTS; attempt++)
    {
        coord start = { 0, 0 }, end = { 0, 0 };
        terrain_stats.proposals++;
        memset(&candidate, 0, sizeof(candidate));
        candidate.partition = pi; candidate.feature = terrain_features[material];
        terrain_theme_profile shape_profile = *profile;
        if (candidate.feature == FEAT_CHASM) shape_profile.pool_chance = 0;
        if (!terrain_endpoints(pi, &shape_profile, &start, &end, &candidate.endpoint_pool))
        { terrain_stats.no_route++; terrain_stats.rejected++; continue; }
        candidate.path_count = terrain_route(start, end, pi, candidate.feature,
            profile->max_length, candidate.path);
        if (candidate.path_count < profile->min_length)
        { terrain_stats.no_route++; terrain_stats.rejected++; continue; }
        /* Endpoint selection already rolls the configured pool chance.
         * Do not roll it twice and silently increase the designer's odds. */
        bool pool = candidate.endpoint_pool;
        int width = rand_range(1, profile->max_width);
        terrain_shape(&candidate, width, pool);
        /* In constructed labyrinths, an aligned transverse passage can be
         * an architectural span. One-tile natural gaps remain jump crossings. */
        if (terrain_architectural_partition(pi) && width > 1 && one_in_(3))
            terrain_architectural_bridge(&candidate);
        if (terrain_candidate_valid(&candidate))
        { terrain_commit(&candidate, material); return true; }
        if (terrain_architectural_bridge(&candidate) && terrain_candidate_valid(&candidate))
        { terrain_commit(&candidate, material); return true; }
        if (width > 1)
        {
            terrain_shape(&candidate, 1, false);
            if (terrain_candidate_valid(&candidate)
                || (terrain_architectural_bridge(&candidate) && terrain_candidate_valid(&candidate)))
            { terrain_commit(&candidate, material); return true; }
        }
        terrain_stats.blocked_access++; terrain_stats.rejected++;
    }
    return false;
}

static int terrain_material(int pi, const terrain_theme_profile* profile)
{
    if (current_partition_modes[pi] == QUAD_MODE_BIG_CAVE)
    {
        switch (current_partition_big_cave_types[pi])
        {
            case BIG_CAVE_FIRE: return TERRAIN_THEME_LAVA;
            case BIG_CAVE_ICE: return TERRAIN_THEME_ICE;
            case BIG_CAVE_POIS: return TERRAIN_THEME_POISON;
            default: break;
        }
    }
    int total = 0;
    for (int i = 0; i < TERRAIN_THEME_MATERIAL_MAX; i++) total += profile->weights[i];
    if (!total) return -1;
    int pick = rand_int(total);
    for (int i = 0; i < TERRAIN_THEME_MATERIAL_MAX; i++)
    {
        if (pick < profile->weights[i]) return i;
        pick -= profile->weights[i];
    }
    return -1;
}

/* Erode the shoreline mask into a few connected deep-water patches.  A broad
 * lake still has a dark centre, but its whole interior does not become deep;
 * the selected patch grows from the middle and leaves shallow pockets around
 * it. Bridge decks are dry ground and keep the adjacent water shallow. Check
 * the final walking graph so an island or narrow approach is not cut off by
 * deepening a lake/channel. */
static void terrain_deepen_water(void)
{
    static int core_cells[TERRAIN_CELLS];
    static int frontier_cells[TERRAIN_CELLS];
    int deep = 0;

    memset(deep_water_core, 0, sizeof(deep_water_core));
    memset(deep_water_seen, 0, sizeof(deep_water_seen));
    memset(deep_water_selected, 0, sizeof(deep_water_selected));
    memset(deep_water_frontier, 0, sizeof(deep_water_frontier));
    memcpy(shadow, cave_feat, sizeof(shadow));
    terrain_components((const byte (*)[MAX_DUNGEON_WID])cave_feat, baseline);

    /* A complete water/ice ring puts deep water at least two tiles from dry
     * ground, including bridge decks, even along a diagonal shoreline. */
    for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++)
        for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
        {
            int feat = cave_feat[y][x];
            if (feat != FEAT_WATER
                || cave_m_idx[y][x] || cave_fixture_at(y, x)) continue;
            if (cave_deep_water_allowed(y, x)) deep_water_core[y][x] = 1;
        }

    /* Each connected core gets one irregular patch.  Growing from its centre
     * keeps the outer water readable as shallow while avoiding isolated deep
     * speckles. */
    for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++)
        for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
        {
            int head = 0, tail = 0, count = 0;
            long sum_y = 0, sum_x = 0;
            int seed = -1, seed_distance = INT_MAX;
            int target, selected = 0, frontier_count = 0;
            if (!deep_water_core[y][x] || deep_water_seen[y][x]) continue;

            deep_water_seen[y][x] = 1;
            core_cells[tail++] = y * MAX_DUNGEON_WID + x;
            while (head < tail)
            {
                int id = core_cells[head++];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                core_cells[count++] = id;
                sum_y += cy;
                sum_x += cx;
                for (int d = 0; d < 4; d++)
                {
                    int ny = cy + channel_dy[d], nx = cx + channel_dx[d];
                    if (!deep_water_core[ny][nx] || deep_water_seen[ny][nx]) continue;
                    deep_water_seen[ny][nx] = 1;
                    core_cells[tail++] = ny * MAX_DUNGEON_WID + nx;
                }
            }

            if (count < DEEP_WATER_MIN_CORE) continue;

            /* Pick the candidate nearest the component's centroid. */
            for (int i = 0; i < count; i++)
            {
                int id = core_cells[i];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                int distance_from_centre =
                    ABS(cy - (int)(sum_y / count)) + ABS(cx - (int)(sum_x / count));
                if (distance_from_centre < seed_distance)
                {
                    seed = id;
                    seed_distance = distance_from_centre;
                }
            }

            target = count * rand_range(DEEP_WATER_MIN_PERCENT,
                DEEP_WATER_MAX_PERCENT) / 100;
            target = MAX(1, MIN(count - 1, target));
            deep_water_selected[seed / MAX_DUNGEON_WID][seed % MAX_DUNGEON_WID] = 1;
            selected = 1;

            /* Maintain a de-duplicated random frontier for a compact,
             * organic-looking patch without quadratic rescans. */
            for (int d = 0; d < 4; d++)
            {
                int ny = seed / MAX_DUNGEON_WID + channel_dy[d];
                int nx = seed % MAX_DUNGEON_WID + channel_dx[d];
                if (!deep_water_core[ny][nx] || deep_water_selected[ny][nx]
                    || deep_water_frontier[ny][nx]) continue;
                deep_water_frontier[ny][nx] = 1;
                frontier_cells[frontier_count++] = ny * MAX_DUNGEON_WID + nx;
            }
            while (selected < target && frontier_count > 0)
            {
                int pick = rand_int(frontier_count);
                int id = frontier_cells[pick];
                int cy = id / MAX_DUNGEON_WID, cx = id % MAX_DUNGEON_WID;
                frontier_cells[pick] = frontier_cells[--frontier_count];
                deep_water_frontier[cy][cx] = 0;
                if (deep_water_selected[cy][cx]) continue;
                deep_water_selected[cy][cx] = 1;
                selected++;
                for (int d = 0; d < 4; d++)
                {
                    int ny = cy + channel_dy[d], nx = cx + channel_dx[d];
                    if (!deep_water_core[ny][nx] || deep_water_selected[ny][nx]
                        || deep_water_frontier[ny][nx]) continue;
                    deep_water_frontier[ny][nx] = 1;
                    frontier_cells[frontier_count++] = ny * MAX_DUNGEON_WID + nx;
                }
            }
        }

    for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++)
        for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
            if (deep_water_selected[y][x])
            {
                shadow[y][x] = FEAT_DEEP_WATER;
                deep++;
            }
    if (!deep) return;
    if (!terrain_preserves_access())
    {
        log_debug("Deep water: kept shallow water to preserve bank access");
        return;
    }
    for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++)
        for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
            if (shadow[y][x] != cave_feat[y][x]) cave_set_feat(y, x, shadow[y][x]);
    log_debug("Deep water: %d tiles in partial interior patches", deep);
}

/* Finish the ice after planning all regions, so its animated, fragile variant
 * does not change the material counts or split a candidate's frozen sheet. */
static void terrain_melt_ice(void)
{
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
            if (melting_ice_candidates[y][x] && cave_feat[y][x] == FEAT_ICE
                && !reserved[y][x] && !cave_m_idx[y][x] && !cave_o_idx[y][x]
                && !cave_fixture_at(y, x) && one_in_(4))
                cave_set_feat(y, x, FEAT_MELTING_ICE);
}

void place_dungeon_terrain(void)
{
    const terrain_theme_profile* profile = terrain_theme_for_depth(p_ptr->depth);
    if (!terrain_history_active()) terrain_generation_reset();
    if (!dun || current_partition_count < 1 || current_partition_count > 25) return;
    bool landmark = terrain_history_started() ? terrain_history_active() : place_terrain_landmark();
    if (landmark)
    {
        for (int i = 0; i < MAX(1, terrain_history_count()); i++)
        {
            const terrain_landmark_stats* s = terrain_history_active()
                ? terrain_landmark_system_stats(i) : terrain_landmark_last_stats();
            terrain_stats.accepted++; terrain_stats.tiles += s->tiles;
            terrain_stats.bridges += s->bridges;
            terrain_stats.material_tiles[s->material] += s->tiles;
            terrain_stats.material_features[s->material]++;
        }
    }
    terrain_context();
    terrain_components((const byte (*)[MAX_DUNGEON_WID])cave_feat, baseline);
    for (int pi = 0; pi < current_partition_count; pi++)
    {
        if (current_partition_modes[pi] == QUAD_MODE_CHASM) continue;
        bool elemental = current_partition_modes[pi] == QUAD_MODE_BIG_CAVE
            && current_partition_big_cave_types[pi] != BIG_CAVE_NONE;
        /* Fine features coexist with a network, even in a touched partition.
         * Existing liquids and reserved architecture are excluded by context. */
        if (landmark && !elemental && !one_in_(2)) continue;
        if (!elemental && !percent_chance(profile->chance)) continue;
        int material = terrain_material(pi, profile);
        if (material < 0) continue;
        terrain_region(pi, material, profile);
        if (elemental)
        {
            /* A cavern's defining terrain should occupy more than one small
             * accent. Add broad frozen sheets or hazardous rivers/pools until
             * roughly a tenth of its natural floor carries that material.
             * The existing planner still rejects blocked routes, authored
             * cells and reserved crossing approaches on every attempt. */
            int floor_area = 0, matching = 0;
            for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
                for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
                {
                    if (level_partition_index_for_point(y, x) != pi) continue;
                    if (roles[y][x] == TERRAIN_NATURAL) floor_area++;
                    if (cave_feat[y][x] == terrain_features[material]) matching++;
                }
            terrain_theme_profile cavern = *profile;
            cavern.min_length = MAX(cavern.min_length, 18);
            cavern.max_length = MAX(cavern.max_length, 40);
            cavern.max_width = 3;
            cavern.pool_chance = MAX(cavern.pool_chance, 80);
            int target = floor_area / 10;
            for (int feature = 0; feature < 5 && matching < target; feature++)
            {
                int before_tiles = terrain_stats.material_tiles[material];
                if (!terrain_region(pi, material, &cavern)) break;
                matching += terrain_stats.material_tiles[material] - before_tiles;
            }
        }
    }
    terrain_deepen_water();
    terrain_melt_ice();
    log_debug("Dungeon terrain theme '%s': %d accepted/%d proposals, %d tiles, %d architectural spans; rejected path=%d access=%d",
        profile->name, terrain_stats.accepted, terrain_stats.proposals, terrain_stats.tiles,
        terrain_stats.bridges, terrain_stats.no_route, terrain_stats.blocked_access);
}

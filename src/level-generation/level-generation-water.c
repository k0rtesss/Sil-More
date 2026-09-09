#include "angband.h"
#include "level-generation/level-generation-internal.h"
#include "cave/cave-fixtures.h"

/* Water follows normal cave geometry. Lakes grow from an interior seed;
 * streams use four-connected paths, so a diagonal pixel chain can never
 * masquerade as a continuous river. All edits are validated before carving. */
#define WATER_PATH_MAX 96
static const int water_dy[4] = { -1, 0, 1, 0 };
static const int water_dx[4] = { 0, 1, 0, -1 };

static bool normal_cave_area(int y, int x)
{
    int pi;
    if (!in_bounds_fully(y, x))
        return false;
    pi = level_partition_index_for_point(y, x);
    return pi >= 0 && current_partition_modes[pi] == QUAD_MODE_CAVEY
        && current_partition_big_cave_types[pi] == BIG_CAVE_NONE
        && !(cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT | CAVE_CHASM_AREA));
}

/* A nonnegative partition selects one ice cavern. Normal water keeps its
 * original cave-only scope; frozen rivers never dig into adjacent partitions. */
static bool water_area(int y, int x, int partition)
{
    if (partition < 0) return normal_cave_area(y, x);
    return in_bounds_fully(y, x)
        && level_partition_index_for_point(y, x) == partition
        && current_partition_modes[partition] == QUAD_MODE_BIG_CAVE
        && current_partition_big_cave_types[partition] == BIG_CAVE_ICE
        && !(cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT | CAVE_CHASM_AREA));
}

static bool natural_water_floor(int y, int x, int partition)
{
    int feat = partition < 0 ? FEAT_WATER : FEAT_ICE;
    return water_area(y, x, partition)
        && (partition < 0 ? cave_natural[y][x] : (cave_info[y][x] & CAVE_ROOM))
        && (cave_feat[y][x] == FEAT_FLOOR || cave_feat[y][x] == feat)
        && !cave_m_idx[y][x] && !cave_o_idx[y][x] && !cave_fixture_at(y, x);
}

/* Keep the room anchor and its immediate neighborhood dry for population
 * and stairs. Features/objects/fixtures are never overwritten by a lake. */
static bool lake_floor(int y, int x, int room, int partition)
{
    if (!natural_water_floor(y, x, partition)) return false;
    if (partition < 0)
        return distance(y, x, dun->cent[room].y, dun->cent[room].x) > 1;
    /* Other anchors can share cavern bounds after later room placement. */
    for (int i = 0; i < dun->cent_n; i++)
        if (distance(y, x, dun->cent[i].y, dun->cent[i].x) <= 1)
            return false;
    return true;
}

static int cave_lake(int room, int partition)
{
    rectangle b = dun->corner[room];
    coord cells[WATER_PATH_MAX];
    static byte visited[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    memset(visited, 0, sizeof(visited));
    int sy = -1, sx = -1, count = 0, total = 0, best = -1, best_seen = 0;
    int h = b.y2 - b.y1 + 1, w = b.x2 - b.x1 + 1;
    if (h <= 0 || w <= 0 || h > MAX_DUNGEON_HGT || w > MAX_DUNGEON_WID
        || (partition < 0 && (h > 24 || w > 24)))
        return 0;
    /* Prefer sheltered interior water to a fringe of single wet squares. */
    for (int y = b.y1; y <= b.y2; y++)
        for (int x = b.x1; x <= b.x2; x++)
        {
            int score = 0;
            if (!lake_floor(y, x, room, partition)
                || (partition >= 0 && cave_feat[y][x] == FEAT_ICE))
                continue;
            total++;
            for (int d = 0; d < 4; d++)
                if (lake_floor(y + water_dy[d], x + water_dx[d], room, partition))
                    score += 10;
            score += rand_int(10);
            if (score > best) { sy = y; sx = x; best = score; best_seen = 1; }
            /* Large caverns have many equally sheltered cells. Sample tied
             * ice seeds uniformly instead of piling pools in the first rows. */
            else if (partition >= 0 && score == best && rand_int(++best_seen) == 0)
            { sy = y; sx = x; }
        }
    if (total < 16 || best < 30)
        return 0;

    int target = MIN(24, MAX(4, total / 4));
    int head = 0, tail = 1;
    cells[0] = (coord){ sy, sx };
    visited[sy - b.y1][sx - b.x1] = 1;
    /* Breadth-first growth makes a compact pool; rotating each expansion
     * breaks symmetry while clipping naturally against the cave wall. */
    while (head < tail && count < target)
    {
        coord c = cells[head++];
        cave_set_feat(c.y, c.x, partition < 0 ? FEAT_WATER : FEAT_ICE);
        count++;
        int start = rand_int(4);
        for (int k = 0; k < 4 && tail < WATER_PATH_MAX; k++)
        {
            int d = (start + k) % 4;
            int y = c.y + water_dy[d], x = c.x + water_dx[d];
            if (y < b.y1 || y > b.y2 || x < b.x1 || x > b.x2
                || visited[y - b.y1][x - b.x1] || !lake_floor(y, x, room, partition)
                || (partition >= 0 && cave_feat[y][x] == FEAT_ICE))
                continue;
            visited[y - b.y1][x - b.x1] = 1;
            cells[tail++] = (coord){ y, x };
        }
    }
    return count;
}

static bool river_cell(int y, int x, bool carve, int partition)
{
    if (!water_area(y, x, partition) || cave_m_idx[y][x] || cave_o_idx[y][x]
        || cave_fixture_at(y, x))
        return false;
    if (natural_water_floor(y, x, partition))
    {
        if (partition >= 0)
            for (int i = 0; i < dun->cent_n; i++)
                if (distance(y, x, dun->cent[i].y, dun->cent[i].x) <= 1)
                    return false;
        return true;
    }
    if (!carve)
        return false;
    /* Rivers may join caves through ordinary rock or an existing tunnel.
     * Stay one square away from authored rooms, doors and vault masonry. */
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
        {
            int yy = y + dy, xx = x + dx;
            if (!in_bounds_fully(yy, xx)
                || (cave_info[yy][xx] & (CAVE_ICKY | CAVE_G_VAULT))
                || ((cave_info[yy][xx] & CAVE_ROOM)
                    && cave_floor_bold(yy, xx) && !cave_natural[yy][xx])
                || cave_any_closed_door_bold(yy, xx))
                return false;
        }
    return cave_feat[y][x] == FEAT_WALL_EXTRA
        || cave_feat[y][x] == FEAT_WALL_OUTER
        || cave_feat[y][x] == FEAT_QUARTZ
        || (cave_feat[y][x] == FEAT_FLOOR && !(cave_info[y][x] & CAVE_ROOM));
}

/* A bounded, randomized BFS finds a legal connected path around pillars.
 * Its dry-run result allows us to reject an entire river without leaving
 * cut-off wet fragments or half-dug passages. */
static int river_path(coord start, coord end, coord* path, bool carve, int partition)
{
    static int parent[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static coord queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    int y1 = MAX(1, MIN(start.y, end.y) - 4);
    int y2 = MIN(p_ptr->cur_map_hgt - 2, MAX(start.y, end.y) + 4);
    int x1 = MAX(1, MIN(start.x, end.x) - 4);
    int x2 = MIN(p_ptr->cur_map_wid - 2, MAX(start.x, end.x) + 4);
    int head = 0, tail = 1, length = 0;
    if (!river_cell(start.y, start.x, carve, partition) || !river_cell(end.y, end.x, carve, partition))
        return 0;
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++) parent[y][x] = -1;
    queue[0] = start;
    parent[start.y][start.x] = 0;
    while (head < tail && parent[end.y][end.x] < 0)
    {
        coord c = queue[head++];
        int first = rand_int(4);
        for (int k = 0; k < 4; k++)
        {
            int d = (first + k) % 4;
            int y = c.y + water_dy[d], x = c.x + water_dx[d];
            if (y < y1 || y > y2 || x < x1 || x > x2 || parent[y][x] >= 0
                || !river_cell(y, x, carve, partition))
                continue;
            parent[y][x] = c.y * MAX_DUNGEON_WID + c.x;
            queue[tail++] = (coord){ y, x };
        }
    }
    if (parent[end.y][end.x] < 0)
        return 0;
    coord c = end;
    while (c.y != start.y || c.x != start.x)
    {
        if (length >= WATER_PATH_MAX - 1) return 0;
        path[length++] = c;
        int prev = parent[c.y][c.x];
        c = (coord){ prev / MAX_DUNGEON_WID, prev % MAX_DUNGEON_WID };
    }
    path[length++] = start;
    return length;
}

/* A river needs a bend even when its cave mouths share a row or column.
 * Route both halves through an offset waypoint, rejecting loops and failed
 * halves before any terrain is changed. The direct route is a fallback for
 * tight geography, not the default shape. */
static int river_meander(coord start, coord end, coord* path, int partition)
{
    coord first[WATER_PATH_MAX], second[WATER_PATH_MAX];
    bool horizontal = ABS(end.x - start.x) >= ABS(end.y - start.y);
    for (int attempt = 0; attempt < 8; attempt++)
    {
        int offset = rand_range(2, 4) * (one_in_(2) ? 1 : -1);
        int y = (start.y + end.y) / 2 + (horizontal ? offset : 0);
        int x = (start.x + end.x) / 2 + (horizontal ? 0 : offset);
        if (!river_cell(y, x, partition < 0, partition)) continue;
        coord bend = { y, x };
        int n1 = river_path(start, bend, first, partition < 0, partition);
        int n2 = river_path(bend, end, second, partition < 0, partition);
        if (!n1 || !n2 || n1 + n2 - 1 > WATER_PATH_MAX) continue;
        bool loop = false;
        for (int a = 1; a < n1 && !loop; a++)
            for (int b = 0; b < n2; b++)
                if (first[a].y == second[b].y && first[a].x == second[b].x)
                { loop = true; break; }
        if (loop) continue;
        /* Both searches return end-to-start order. Share the bend once. */
        memcpy(path, second, n2 * sizeof(*path));
        memcpy(path + n2, first + 1, (n1 - 1) * sizeof(*path));
        return n1 + n2 - 1;
    }
    return river_path(start, end, path, partition < 0, partition);
}

static bool cave_water_seed(int room, coord* out, int partition)
{
    rectangle b = dun->corner[room];
    int seen = 0;
    bool wet = false;
    for (int y = b.y1; y <= b.y2; y++)
        for (int x = b.x1; x <= b.x2; x++)
        {
            if (!lake_floor(y, x, room, partition)) continue;
            bool this_wet = cave_feat[y][x] == (partition < 0 ? FEAT_WATER : FEAT_ICE);
            if (wet && !this_wet) continue;
            if (this_wet && !wet) { wet = true; seen = 0; }
            if (rand_int(++seen) == 0) *out = (coord){ y, x };
        }
    return seen > 0;
}

static void place_cave_ice(void)
{
    int pools = 0, rivers = 0, tiles = 0;
    for (int i = 0; i < dun->cent_n; i++)
    {
        int pi = level_partition_index_for_point(dun->cent[i].y, dun->cent[i].x);
        if (pi < 0 || current_partition_modes[pi] != QUAD_MODE_BIG_CAVE
            || current_partition_big_cave_types[pi] != BIG_CAVE_ICE
            || room_anchor_kind[i] != LAYOUT_ANCHOR_CA_BLOB || dun->is_quest[i])
            continue;
        rectangle b = dun->corner[i];
        /* Attempt a winding river through existing floor first. Retrying
         * seeds handles caverns whose pillars block a particular route. */
        for (int attempt = 0; attempt < 24; attempt++)
        {
            coord start, end = { 0, 0 }, path[WATER_PATH_MAX];
            int farthest = 0;
            if (!cave_water_seed(i, &start, pi)) break;
            for (int y = b.y1; y <= b.y2; y++)
                for (int x = b.x1; x <= b.x2; x++)
                {
                    int d = distance(start.y, start.x, y, x);
                    /* Stay within the bounded path capacity in large caves. */
                    if (d <= 40 && d > farthest && lake_floor(y, x, i, pi))
                    { farthest = d; end = (coord){ y, x }; }
                }
            if (farthest < 8) continue;
            int n = river_meander(start, end, path, pi);
            if (n < 8) continue;
            for (int k = 0; k < n; k++)
                cave_set_feat(path[k].y, path[k].x, FEAT_ICE);
            rivers++; tiles += n;
            break;
        }
        int pool_count = 1 + ((b.y2 - b.y1) * (b.x2 - b.x1) >= 500);
        for (int k = 0; k < pool_count; k++)
        {
            int n = cave_lake(i, pi);
            if (n) { pools++; tiles += n; }
        }
    }
    log_debug("Cave ice: %d frozen pools, %d frozen rivers, %d painted tiles",
        pools, rivers, tiles);
}

void place_cave_water(void)
{
    int rooms[CENT_MAX], room_count = 0, lakes = 0, rivers = 0, streams = 0, tiles = 0;
    bool linked[CENT_MAX] = { false };
    /* Vault water is authored separately and is never part of this pass. */
    for (int i = 0; i < dun->cent_n; i++)
    {
        coord seed;
        if (room_anchor_kind[i] != LAYOUT_ANCHOR_CA_BLOB || dun->is_quest[i]
            || !cave_water_seed(i, &seed, -1)) continue;
        rooms[room_count++] = i;
        if (one_in_(2))
        {
            int n = cave_lake(i, -1);
            if (n) { lakes++; tiles += n; }
        }
    }
    for (int a = 0; a < room_count; a++)
    {
        int i = rooms[a], nearest = -1, best = 30;
        coord start, end, path[WATER_PATH_MAX];
        if (linked[i] || !one_in_(2) || !cave_water_seed(i, &start, -1)) continue;
        for (int b = a + 1; b < room_count; b++)
        {
            int j = rooms[b];
            int d = distance(dun->cent[i].y, dun->cent[i].x,
                dun->cent[j].y, dun->cent[j].x);
            if (!linked[j] && d < best) { best = d; nearest = j; }
        }
        if (nearest < 0 || !cave_water_seed(nearest, &end, -1)) continue;
        int n = river_meander(start, end, path, -1);
        if (n < 4) continue;
        byte style = cave_color[start.y][start.x];
        for (int k = 0; k < n; k++)
        {
            int y = path[k].y, x = path[k].x;
            cave_set_feat_with_color(y, x, FEAT_WATER, style);
            cave_natural[y][x] = 1;
            cave_info[y][x] |= CAVE_ROOM;
        }
        linked[i] = linked[nearest] = true;
        rivers++; tiles += n;
    }
    /* A cave can also have a stream crossing its own chamber, including
     * levels where no second cave is close enough for a river connection. */
    for (int a = 0; a < room_count; a++)
    {
        int i = rooms[a], farthest = 0;
        coord start, end = { 0, 0 }, path[WATER_PATH_MAX];
        rectangle b = dun->corner[i];
        if (linked[i] || !one_in_(3) || !cave_water_seed(i, &start, -1)) continue;
        for (int y = b.y1; y <= b.y2; y++)
            for (int x = b.x1; x <= b.x2; x++)
            {
                int d = distance(start.y, start.x, y, x);
                if (lake_floor(y, x, i, -1) && d > farthest)
                { farthest = d; end = (coord){ y, x }; }
            }
        if (farthest < 5) continue;
        int n = river_path(start, end, path, false, -1);
        if (n < 5) continue;
        for (int k = 0; k < n; k++)
            cave_set_feat(path[k].y, path[k].x, FEAT_WATER);
        streams++; tiles += n;
    }
    log_debug("Cave water: %d lakes, %d cave-connecting rivers, %d chamber streams, %d painted tiles",
        lakes, rivers, streams, tiles);
    place_cave_ice();
}

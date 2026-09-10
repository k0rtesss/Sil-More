#include "angband.h"
#include "level-generation/level-generation-internal.h"
#include "cave/cave-fixtures.h"

/* Each hazardous liquid square must preserve a dry route around it.
 * Shapes are prepared before painting: a rejected river
 * never leaves disconnected fragments behind. */
#define LAVA_SHAPE_MAX 96
static const int lava_dy[4] = { -1, 0, 1, 0 };
static const int lava_dx[4] = { 0, 1, 0, -1 };
static byte lava_proposed[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

static bool lava_floor(int y, int x, int partition)
{
    if (!in_bounds_fully(y, x)
        || level_partition_index_for_point(y, x) != partition
        || cave_feat[y][x] != FEAT_FLOOR
        || !(cave_info[y][x] & CAVE_ROOM)
        || (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT | CAVE_CHASM_AREA))
        || cave_m_idx[y][x] || cave_o_idx[y][x] || cave_fixture_at(y, x))
        return false;
    /* Keep room anchors and their surroundings available for stairs,
     * player placement, and the existing tunnel attachment points. */
    for (int i = 0; i < dun->cent_n; i++)
        if (distance(y, x, dun->cent[i].y, dun->cent[i].x) <= 2)
            return false;
    return true;
}

/* A sufficient local connectivity test: after removing this square, all
 * of its dry neighbors must still connect inside the surrounding 3x3 ring.
 * Applying this to each addition preserves every existing dry component,
 * including entrances and routes to objects; it needs no chosen start. */
static bool lava_preserves_routes(int y, int x)
{
    bool dry[3][3] = { { false } }, seen[3][3] = { { false } };
    coord queue[8];
    int head = 0, tail = 0, count = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
        {
            int yy = y + dy, xx = x + dx;
            if ((!dy && !dx) || !in_bounds_fully(yy, xx)
                || lava_proposed[yy][xx] || cave_feat[yy][xx] == FEAT_LAVA
                || cave_feat[yy][xx] == FEAT_POISON
                || !player_passable(yy, xx, false))
                continue;
            dry[dy + 1][dx + 1] = true;
            count++;
            if (!tail)
            {
                queue[tail++] = (coord){ dy + 1, dx + 1 };
                seen[dy + 1][dx + 1] = true;
            }
        }
    if (!count) return false;
    while (head < tail)
    {
        coord c = queue[head++];
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
            {
                int yy = c.y + dy, xx = c.x + dx;
                if (yy < 0 || yy > 2 || xx < 0 || xx > 2
                    || !dry[yy][xx] || seen[yy][xx]) continue;
                seen[yy][xx] = true;
                queue[tail++] = (coord){ yy, xx };
            }
    }
    return tail == count;
}

static void liquid_paint(const coord* cells, int count, int feature)
{
    for (int i = 0; i < count; i++)
        cave_set_feat(cells[i].y, cells[i].x, feature);
}

static bool lava_seed(rectangle b, int partition, coord* seed)
{
    int seen = 0;
    /* Interior seeds leave a bank on both sides of a channel. */
    for (int y = b.y1; y <= b.y2; y++)
        for (int x = b.x1; x <= b.x2; x++)
        {
            if (!lava_floor(y, x, partition)) continue;
            int adjacent = 0;
            for (int d = 0; d < 4; d++)
                if (lava_floor(y + lava_dy[d], x + lava_dx[d], partition))
                    adjacent++;
            if (adjacent < 3) continue;
            if (rand_int(++seen) == 0) *seed = (coord){ y, x };
        }
    return seen > 0;
}

/* A narrow channel advances along a main axis, bending sideways every few
 * squares. It never jumps rock, closes a loop, or crosses a vault. */
static int lava_river(rectangle b, int partition)
{
    for (int attempt = 0; attempt < 32; attempt++)
    {
        coord cells[LAVA_SHAPE_MAX], seed;
        if (!lava_seed(b, partition, &seed)) return 0;
        memset(lava_proposed, 0, sizeof(lava_proposed));
        int main_dir = rand_int(4), dir = main_dir, count = 0, turns = 0;
        int straight = rand_range(2, 4), sideways = 0;
        int target = rand_range(16, 30);
        coord c = seed;
        while (count < target)
        {
            if (c.y < b.y1 || c.y > b.y2 || c.x < b.x1 || c.x > b.x2
                || !lava_floor(c.y, c.x, partition)
                || lava_proposed[c.y][c.x]
                || !lava_preserves_routes(c.y, c.x)) break;
            cells[count++] = c;
            lava_proposed[c.y][c.x] = 1;
            if (sideways)
            {
                if (--sideways == 0)
                { dir = main_dir; straight = rand_range(3, 6); turns++; }
            }
            else if (--straight == 0)
            {
                dir = (main_dir + (one_in_(2) ? 1 : 3)) % 4;
                sideways = rand_range(1, 3);
                turns++;
            }
            c.y += lava_dy[dir];
            c.x += lava_dx[dir];
        }
        if (count < 10 || turns < 2) continue;
        liquid_paint(cells, count, FEAT_LAVA);
        return count;
    }
    return 0;
}

static int liquid_pool(rectangle b, int partition, int feature,
    int min_size, int max_size)
{
    for (int attempt = 0; attempt < 16; attempt++)
    {
        coord seed, cells[LAVA_SHAPE_MAX];
        if (!lava_seed(b, partition, &seed)) return 0;
        memset(lava_proposed, 0, sizeof(lava_proposed));
        if (!lava_preserves_routes(seed.y, seed.x)) continue;
        cells[0] = seed;
        lava_proposed[seed.y][seed.x] = 1;
        int head = 0, count = 1, target = rand_range(min_size, max_size);
        while (head < count && count < target)
        {
            coord c = cells[head++];
            int first = rand_int(4);
            for (int k = 0; k < 4 && count < target; k++)
            {
                int d = (first + k) % 4;
                int y = c.y + lava_dy[d], x = c.x + lava_dx[d];
                if (y < b.y1 || y > b.y2 || x < b.x1 || x > b.x2
                    || !lava_floor(y, x, partition) || lava_proposed[y][x]
                    || !lava_preserves_routes(y, x)) continue;
                lava_proposed[y][x] = 1;
                cells[count++] = (coord){ y, x };
            }
        }
        if (count < MIN(5, min_size)) continue;
        liquid_paint(cells, count, feature);
        return count;
    }
    return 0;
}

static int lava_pool(rectangle b, int partition)
{
    return liquid_pool(b, partition, FEAT_LAVA, 8, 22);
}

void place_cave_lava(void)
{
    int pools = 0, rivers = 0, tiles = 0;
    for (int i = 0; i < dun->cent_n; i++)
    {
        int pi = level_partition_index_for_point(dun->cent[i].y, dun->cent[i].x);
        if (pi < 0 || current_partition_modes[pi] != QUAD_MODE_BIG_CAVE
            || current_partition_big_cave_types[pi] != BIG_CAVE_FIRE
            || room_anchor_kind[i] != LAYOUT_ANCHOR_CA_BLOB || dun->is_quest[i])
            continue;
        rectangle b = dun->corner[i];
        /* Every fire cavern attempts both shapes. Tight geography may
         * reject a shape rather than sacrificing its only dry route. */
        int n = lava_river(b, pi);
        if (n) { rivers++; tiles += n; }
        int pool_count = 1 + ((b.y2 - b.y1) * (b.x2 - b.x1) >= 500);
        for (int k = 0; k < pool_count; k++)
        {
            n = lava_pool(b, pi);
            if (n) { pools++; tiles += n; }
        }
    }
    log_debug("Cave lava: %d pools, %d meandering rivers, %d tiles; dry routes preserved",
        pools, rivers, tiles);
}

void place_cave_poison(void)
{
    int pools = 0, tiles = 0;
    for (int i = 0; i < dun->cent_n; i++)
    {
        int pi = level_partition_index_for_point(dun->cent[i].y, dun->cent[i].x);
        if (pi < 0 || current_partition_modes[pi] != QUAD_MODE_BIG_CAVE
            || current_partition_big_cave_types[pi] != BIG_CAVE_POIS
            || room_anchor_kind[i] != LAYOUT_ANCHOR_CA_BLOB || dun->is_quest[i])
            continue;
        rectangle b = dun->corner[i];
        /* Small connected seeps leave generous banks and dry escape routes.
         * Reuse the same protected-cell and connectivity checks as lava. */
        int pool_count = 2 + ((b.y2 - b.y1) * (b.x2 - b.x1) >= 500);
        for (int k = 0; k < pool_count; k++)
        {
            int n = liquid_pool(b, pi, FEAT_POISON, 4, 10);
            if (n) { pools++; tiles += n; }
        }
    }
    log_debug("Cave poison: %d seeps, %d tiles; dry routes preserved", pools, tiles);
}

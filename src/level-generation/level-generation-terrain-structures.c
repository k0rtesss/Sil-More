#include "angband.h"
#include "cave/cave-bridge.h"
#include "level-generation/level-generation-internal.h"
#include "level-generation/level-generation-landmarks.h"
#include "level-generation/level-generation-terrain-access.h"
#include "level-generation/level-generation-terrain-vaults.h"
#include "level-generation/level-generation-terrain-structures.h"

#define STRUCTURE_CELLS (MAX_DUNGEON_HGT * MAX_DUNGEON_WID)
typedef struct structure_region
{
    int y1, x1, y2, x2, exposed;
    unsigned policy;
    byte scenario;
} structure_region;

static int owners[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte marks[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte wet[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static structure_region regions[STRUCTURE_CELLS];
static const int sy[4] = { -1, 0, 1, 0 };
static const int sx[4] = { 0, 1, 0, -1 };

int terrain_structure_cell(int y, int x)
{
    return in_bounds_fully(y, x) ? marks[y][x] : 0;
}

int terrain_structure_owner(int y, int x)
{
    return in_bounds_fully(y, x) ? owners[y][x] : 0;
}

static bool structure_wall(int feat)
{
    return feat == FEAT_WALL_EXTRA || feat == FEAT_WALL_OUTER
        || feat == FEAT_WALL_INNER || feat == FEAT_WALL_SOLID
        || feat == FEAT_QUARTZ;
}

static bool structure_floor(int feat)
{
    return feat == FEAT_FLOOR || feat == FEAT_OPEN || feat == FEAT_BROKEN
        || feat == FEAT_SECRET || (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)
        || (feat >= FEAT_TRAP_HEAD && feat <= FEAT_TRAP_TAIL);
}

static bool structure_editable(int y, int x,
    const byte hard[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    return in_bounds_fully(y, x) && !hard[y][x]
        && (structure_wall(cave_feat[y][x]) || structure_floor(cave_feat[y][x])
            || cave_feat[y][x] == FEAT_RUBBLE);
}

static void structure_own(int y, int x, int id, unsigned policy)
{
    owners[y][x] = id;
    structure_region* r = &regions[id];
    if (!r->y2) { r->y1 = r->y2 = y; r->x1 = r->x2 = x; }
    r->y1 = MIN(r->y1, y); r->y2 = MAX(r->y2, y);
    r->x1 = MIN(r->x1, x); r->x2 = MAX(r->x2, x);
    r->policy |= policy;
}

/* Preserve a real straight route through the water. Both dry ends and every
 * replaced tile must be part of the old traversable construction. */
static bool structure_causeway(int id,
    const byte hard[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    byte work[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    byte shadow[MAX_DUNGEON_HGT][MAX_DUNGEON_WID], int feature)
{
    structure_region* r = &regions[id];
    int best_y = 0, best_x = 0, best_axis = 0, best_length = 99;
    for (int y = r->y1; y <= r->y2; y++) for (int x = r->x1; x <= r->x2; x++)
    {
        if (owners[y][x] != id || work[y][x] != TERRAIN_LANDMARK_TERRAIN) continue;
        for (int axis = 0; axis < 2; axis++)
        {
            int dy = axis, dx = 1 - axis;
            int ay = y - dy, ax = x - dx;
            if (!in_bounds_fully(ay, ax) || shadow[ay][ax] == feature
                || !structure_floor(cave_feat[ay][ax])
                || !terrain_generation_walkable(ay, ax, shadow)) continue;
            int n = 0;
            while (n < 16 && in_bounds_fully(y + n * dy, x + n * dx)
                && owners[y + n * dy][x + n * dx] == id
                && shadow[y + n * dy][x + n * dx] == feature
                && !hard[y + n * dy][x + n * dx]
                && structure_floor(cave_feat[y + n * dy][x + n * dx])) n++;
            int by = y + n * dy, bx = x + n * dx;
            if (n < 2 || n >= best_length || !in_bounds_fully(by, bx)
                || shadow[by][bx] == feature || !structure_floor(cave_feat[by][bx])
                || !terrain_generation_walkable(by, bx, shadow)) continue;
            best_y = y; best_x = x; best_axis = axis; best_length = n;
        }
    }
    if (!best_y) return false;
    int dy = best_axis, dx = 1 - best_axis;
    for (int i = 0; i < best_length; i++)
    {
        int y = best_y + i * dy, x = best_x + i * dx;
        work[y][x] = TERRAIN_LANDMARK_REPAIR; shadow[y][x] = cave_bridge_feature(feature, dy != 0);
        marks[y][x] = 3;
    }
    /* Two contiguous masonry footings at each end give the crossing a built
     * abutment. These widen existing wall breaches without blocking water. */
    for (int end = 0; end < 2; end++) for (int side = -1; side <= 1; side += 2)
        for (int step = 0; step < MIN(2, best_length); step++)
        {
            int i = end ? best_length - 1 - step : step;
            int y = best_y + i * dy + side * dx;
            int x = best_x + i * dx + side * dy;
            if (!structure_editable(y, x, hard) || owners[y][x] != id
                || !structure_wall(cave_feat[y][x]) || shadow[y][x] == feature) continue;
            work[y][x] = TERRAIN_LANDMARK_REPAIR; shadow[y][x] = FEAT_FLOOR;
            marks[y][x] = 3;
        }
    return true;
}

void terrain_structure_apply(
    const byte hard[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    byte work[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    byte shadow[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int feature, terrain_structure_stats* stats)
{
    memset(owners, 0, sizeof(owners)); memset(marks, 0, sizeof(marks));
    memset(regions, 0, sizeof(regions)); memset(wet, 0, sizeof(wet));
    memset(stats, 0, sizeof(*stats));
    int count = 0;
    /* Instance identity survives disconnected template pieces and rotation. */
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            int id = terrain_vault_instance_at(y, x);
            unsigned policy = terrain_vault_policy_at(y, x);
            if (id > 0 && id < STRUCTURE_CELLS && policy)
            { structure_own(y, x, id, policy); count = MAX(count, id); }
        }
    /* Generated constructed rooms have the same exposure rules. Natural
     * cavern anchors do not invent an architectural footprint. */
    if (dun) for (int i = 0; i < dun->cent_n && count + 1 < STRUCTURE_CELLS; i++)
    {
        if (room_anchor_kind[i] == LAYOUT_ANCHOR_CA_BLOB
            || room_anchor_kind[i] == LAYOUT_ANCHOR_BSP_SLICE) continue;
        rectangle b = dun->corner[i]; int id = ++count;
        for (int y = MAX(1, b.y1); y <= MIN(p_ptr->cur_map_hgt - 2, b.y2); y++)
            for (int x = MAX(1, b.x1); x <= MIN(p_ptr->cur_map_wid - 2, b.x2); x++)
                if (!owners[y][x] && terrain_vault_id_at(y, x) < 0)
                    structure_own(y, x, id, TERRAIN_VAULT_CROSSING | TERRAIN_VAULT_FLOOD);
    }
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
            wet[y][x] = work[y][x] == TERRAIN_LANDMARK_TERRAIN && shadow[y][x] == feature;
    for (int id = 1; id <= count; id++)
    {
        structure_region* r = &regions[id];
        if (!r->y2) continue;
        for (int y = r->y1; y <= r->y2; y++) for (int x = r->x1; x <= r->x2; x++)
        {
            if (owners[y][x] != id || hard[y][x]) continue;
            if (wet[y][x]) r->exposed++;
            else for (int d = 0; d < 4; d++)
                if (wet[y + sy[d]][x + sx[d]]) { r->exposed++; break; }
        }
        if (!r->exposed) continue;
        bool ruined = (r->policy & TERRAIN_VAULT_RUINED) != 0;
        bool repaired = (r->policy & TERRAIN_VAULT_REPAIRED) != 0;
        r->scenario = feature == FEAT_LAVA || feature == FEAT_CHASM ? 2 : 1;
        if (ruined || one_in_(3)) r->scenario = 2;
        if ((repaired ? !one_in_(5) : !ruined && one_in_(8))
            && structure_causeway(id, hard, work, shadow, feature)) r->scenario = 3;
        for (int y = r->y1; y <= r->y2; y++) for (int x = r->x1; x <= r->x2; x++)
            if (owners[y][x] == id && work[y][x] == TERRAIN_LANDMARK_REPAIR) wet[y][x] = 0;

        /* Dilation uses the previous wave, so each incident has one coherent
         * erosion front. Ruins admit a deeper front, including their floors. */
        int waves = feature == FEAT_CHASM ? 1 : ruined ? 3 : r->scenario == 2 ? 2 : 1;
        for (int wave = 0; wave < waves; wave++)
        {
            for (int y = r->y1; y <= r->y2; y++) for (int x = r->x1; x <= r->x2; x++)
            {
                if (owners[y][x] != id || !structure_editable(y, x, hard)
                    || work[y][x] == TERRAIN_LANDMARK_BRIDGE
                    || work[y][x] == TERRAIN_LANDMARK_REPAIR) continue;
                if (wet[y][x]) { marks[y][x] = r->scenario; continue; }
                /* A fracture can collapse masonry, but it does not flood the
                 * surviving floor. Its planned width already describes the
                 * missing ground through the room. */
                if (feature == FEAT_CHASM && !structure_wall(cave_feat[y][x])) continue;
                bool beside = false;
                for (int d = 0; d < 4; d++) beside |= wet[y + sy[d]][x + sx[d]] != 0;
                if (!beside) continue;
                if (r->scenario == 3 && structure_wall(cave_feat[y][x])) continue;
                work[y][x] = TERRAIN_LANDMARK_TERRAIN;
                shadow[y][x] = feature; marks[y][x] = r->scenario;
            }
            for (int y = r->y1; y <= r->y2; y++) for (int x = r->x1; x <= r->x2; x++)
                if (owners[y][x] == id)
                    wet[y][x] = work[y][x] == TERRAIN_LANDMARK_TERRAIN && shadow[y][x] == feature;
        }
        if (r->scenario != 3)
        {
            /* Open the dry lip where the eroded wall meets a surviving room
             * floor; these breaches become usable shore instead of dead rock. */
            for (int y = r->y1; y <= r->y2; y++) for (int x = r->x1; x <= r->x2; x++)
            {
                if (owners[y][x] != id || !structure_editable(y, x, hard)
                    || !structure_wall(cave_feat[y][x]) || work[y][x]) continue;
                bool water = false, floor = false;
                for (int d = 0; d < 4; d++)
                {
                    int yy = y + sy[d], xx = x + sx[d];
                    water |= wet[yy][xx] != 0;
                    floor |= structure_floor(cave_feat[yy][xx]) && shadow[yy][xx] != feature;
                }
                if (water && floor)
                {
                    work[y][x] = TERRAIN_LANDMARK_BANK; shadow[y][x] = FEAT_FLOOR;
                    marks[y][x] = r->scenario;
                }
            }
            /* A connected two-cell shoulder at an eroded wall survives as
             * debris. Never put rubble on an original walkable/occupied tile. */
            int shoulders = 0, limit = ruined ? 3 : 1;
            for (int y = r->y1; y <= r->y2 && shoulders < limit; y++)
                for (int x = r->x1; x <= r->x2 && shoulders < limit; x++)
                {
                    if (owners[y][x] != id || !structure_editable(y, x, hard)
                        || !structure_wall(cave_feat[y][x]) || shadow[y][x] == feature
                        || cave_o_idx[y][x] || cave_m_idx[y][x] || marks[y][x]) continue;
                    for (int d = 0; d < 4; d++)
                    {
                        if (!wet[y + sy[d]][x + sx[d]]) continue;
                        int yy = y + sy[(d + 1) % 4], xx = x + sx[(d + 1) % 4];
                        if (!structure_editable(yy, xx, hard) || owners[yy][xx] != id
                            || !structure_wall(cave_feat[yy][xx]) || shadow[yy][xx] == feature
                            || cave_o_idx[yy][xx] || cave_m_idx[yy][xx] || marks[yy][xx]) continue;
                        work[y][x] = work[yy][xx] = TERRAIN_LANDMARK_RUBBLE;
                        shadow[y][x] = shadow[yy][xx] = FEAT_RUBBLE;
                        marks[y][x] = marks[yy][xx] = r->scenario;
                        shoulders++; break;
                    }
                }
        }
        bool changed = false;
        for (int y = r->y1; y <= r->y2; y++) for (int x = r->x1; x <= r->x2; x++)
            if (owners[y][x] == id && marks[y][x] && shadow[y][x] != cave_feat[y][x]) changed = true;
        if (changed)
        {
            if (r->scenario == 3) stats->repaired++;
            else if (r->scenario == 2) stats->breached++;
            else stats->flooded++;
        }
    }
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (!marks[y][x]) continue;
            stats->ruined_walls += structure_wall(cave_feat[y][x]) && shadow[y][x] != cave_feat[y][x];
            stats->rubble_tiles += work[y][x] == TERRAIN_LANDMARK_RUBBLE;
            stats->repair_tiles += work[y][x] == TERRAIN_LANDMARK_REPAIR;
        }
}

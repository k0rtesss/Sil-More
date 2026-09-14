#include "angband.h"
#include "externs.h"
#include "level-generation/level-generation-terrain-access.h"

/* Clockwise neighbours, matching cmd-run.c's cycle[] run-up test. */
static const int terrain_dy[8] = {1, 1, 1, 0, -1, -1, -1, 0};
static const int terrain_dx[8] = {-1, 0, 1, 1, 1, 0, -1, -1};

static bool terrain_gap(byte feat)
{
    return feat == FEAT_CHASM || feat == FEAT_LAVA || feat == FEAT_POISON;
}

bool terrain_generation_walkable(int y, int x,
    const byte (*features)[MAX_DUNGEON_WID])
{
    byte feat;
    if (!in_bounds_fully(y, x)) return false;
    if (!features) features = (const byte (*)[MAX_DUNGEON_WID])cave_feat;
    feat = features[y][x];
    if (terrain_gap(feat) || feat == FEAT_NONE) return false;
    if (feat == FEAT_SECRET) return true;
    return feat < FEAT_WALL_HEAD || feat > FEAT_WALL_TAIL;
}

/* A leap must start and end on safe, unobstructed footing.  Closed doors are
 * openable during ordinary traversal, but cannot be a planned landing. */
static bool terrain_jump_footing(int y, int x,
    const byte (*features)[MAX_DUNGEON_WID])
{
    byte feat;
    if (!terrain_generation_walkable(y, x, features)) return false;
    feat = features[y][x];
    if (feat == FEAT_SECRET
        || (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)
        || FEAT_IS_TRAP(feat))
        return false;
    return !cave_m_idx || cave_m_idx[y][x] <= 0;
}

bool terrain_generation_jump(int y, int x, int dir_y, int dir_x,
    const byte (*features)[MAX_DUNGEON_WID])
{
    int direction = -1;
    int my = y + dir_y, mx = x + dir_x;
    int ly = my + dir_y, lx = mx + dir_x;

    if (!features) features = (const byte (*)[MAX_DUNGEON_WID])cave_feat;
    for (int d = 0; d < 8; d++)
        if (terrain_dy[d] == dir_y && terrain_dx[d] == dir_x) direction = d;
    if (direction < 0 || !in_bounds_fully(my, mx)) return false;
    if (!terrain_gap(features[my][mx])) return false;
    if (cave_m_idx && cave_m_idx[my][mx] > 0) return false;
    if (!terrain_jump_footing(y, x, features)
        || !terrain_jump_footing(ly, lx, features)) return false;

    /* Once the takeoff is reached, the player can step back and approach it
     * from any of the three previous-move directions accepted by Leaping.
     * No wider gap or jump-to-jump run-up is assumed.  Lava's existing heat
     * damage and the character's Leaping requirement remain gameplay rules. */
    for (int offset = -1; offset <= 1; offset++)
    {
        int approach = (direction + offset + 8) % 8;
        if (terrain_jump_footing(y - terrain_dy[approach],
                x - terrain_dx[approach], features)) return true;
    }
    return false;
}

void terrain_generation_flood(int sy, int sx,
    byte reached[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    const byte (*features)[MAX_DUNGEON_WID])
{
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    int head = 0, tail = 0;
    memset(reached, 0, sizeof(byte) * MAX_DUNGEON_HGT * MAX_DUNGEON_WID);
    if (!terrain_generation_walkable(sy, sx, features)) return;
    reached[sy][sx] = 1;
    queue[tail++] = sy * MAX_DUNGEON_WID + sx;
    while (head < tail)
    {
        int cell = queue[head++];
        int y = cell / MAX_DUNGEON_WID, x = cell % MAX_DUNGEON_WID;
        for (int d = 0; d < 8; d++)
        {
            int ny = y + terrain_dy[d], nx = x + terrain_dx[d];
            if (!terrain_generation_walkable(ny, nx, features))
            {
                if (!terrain_generation_jump(y, x, terrain_dy[d], terrain_dx[d], features)) continue;
                ny += terrain_dy[d];
                nx += terrain_dx[d];
            }
            if (reached[ny][nx]) continue;
            reached[ny][nx] = 1;
            queue[tail++] = ny * MAX_DUNGEON_WID + nx;
        }
    }
}

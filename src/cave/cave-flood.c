#include "angband.h"
#include "externs.h"
#include "cave/cave-flood.h"

/* Stage selects the eight- or sixteen-cell quota for the next player action.
 * A per-action guard keeps a newly triggered flood at its source until the
 * following action. Neither energy cost nor monster turns affect this clock. */
static byte flood_stage[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static bool flood_new[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static bool flood_surface[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte flood_surface_kind[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte flood_trap_kind[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static u16b flood_path_distance[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int flood_path_queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];

static void flood_mark_surface_area(int y, int x, int radius, byte kind);

static byte flood_kind_for_feature(int feat)
{
    if (feat == FEAT_WATER || feat == FEAT_DEEP_WATER)
        return CAVE_FLOOD_KIND_WATER;
    if (feat == FEAT_POISON)
        return CAVE_FLOOD_KIND_ACID;
    return 0;
}

static bool flood_surface_feature(byte kind, int feat)
{
    return kind && flood_kind_for_feature(feat) == kind;
}

static int flood_surface_feat(byte kind)
{
    return kind == CAVE_FLOOD_KIND_ACID ? FEAT_POISON : FEAT_WATER;
}

void cave_flood_clear(void)
{
    memset(flood_stage, 0, sizeof(flood_stage));
    memset(flood_new, 0, sizeof(flood_new));
    cave_flood_clear_surface_markers();
    memset(flood_trap_kind, 0, sizeof(flood_trap_kind));
}

void cave_flood_clear_surface_markers(void)
{
    memset(flood_surface, 0, sizeof(flood_surface));
    memset(flood_surface_kind, 0, sizeof(flood_surface_kind));
}

void cave_flood_begin_action(void)
{
    memset(flood_new, 0, sizeof(flood_new));
}

byte cave_flood_stage_at(int y, int x)
{
    return in_bounds(y, x) ? flood_stage[y][x] : 0;
}

void cave_flood_set_trap_kind(int y, int x, byte kind)
{
    if (!in_bounds(y, x) || cave_feat[y][x] != FEAT_TRAP_FLOOD)
        return;
    flood_trap_kind[y][x] = kind == CAVE_FLOOD_KIND_ACID
        ? CAVE_FLOOD_KIND_ACID : CAVE_FLOOD_KIND_WATER;
}

byte cave_flood_trap_kind_at(int y, int x)
{
    if (!in_bounds(y, x) || cave_feat[y][x] != FEAT_TRAP_FLOOD)
        return 0;
    return flood_trap_kind[y][x] == CAVE_FLOOD_KIND_ACID
        ? CAVE_FLOOD_KIND_ACID : CAVE_FLOOD_KIND_WATER;
}

bool cave_flood_trap_is_acid_at(int y, int x)
{
    return cave_flood_trap_kind_at(y, x) == CAVE_FLOOD_KIND_ACID;
}

bool cave_flood_restore_trap_kind(int y, int x, byte kind)
{
    if (!in_bounds_fully(y, x) || cave_feat[y][x] != FEAT_TRAP_FLOOD
        || kind != CAVE_FLOOD_KIND_ACID
        || flood_trap_kind[y][x])
        return false;
    flood_trap_kind[y][x] = kind;
    return true;
}

bool cave_flood_restore(int y, int x, byte stage)
{
    byte kind;

    if (!in_bounds_fully(y, x) || stage < 1
        || (stage > 2 && stage != CAVE_FLOOD_STAGE_COMPLETE)
        || flood_stage[y][x])
        return false;
    kind = flood_kind_for_feature(cave_feat[y][x]);
    if (!kind)
        return false;
    flood_stage[y][x] = stage;
    flood_mark_surface_area(y, x, (int)stage - 1, kind);
    return true;
}

bool cave_flood_surface_at(int y, int x)
{
    return in_bounds(y, x) && flood_surface[y][x]
        && cave_feat
        && flood_surface_feature(flood_surface_kind[y][x], cave_feat[y][x]);
}

byte cave_flood_surface_kind_at(int y, int x)
{
    if (!cave_flood_surface_at(y, x))
        return 0;
    return flood_surface_kind[y][x];
}

bool cave_flood_restore_surface(int y, int x, byte kind)
{
    if (!in_bounds_fully(y, x) || !flood_surface_feature(kind, cave_feat[y][x])
        || flood_surface[y][x])
        return false;
    flood_surface[y][x] = true;
    flood_surface_kind[y][x] = kind;
    return true;
}

void cave_flood_surface_changed(int y, int x, int new_feat)
{
    byte old_kind;
    byte new_kind;

    if (!in_bounds(y, x))
        return;
    old_kind = flood_surface_kind[y][x];
    new_kind = flood_kind_for_feature(new_feat);
    flood_surface[y][x] = false;
    flood_surface_kind[y][x] = 0;
    if (new_feat != FEAT_TRAP_FLOOD)
        flood_trap_kind[y][x] = 0;
    if (!new_kind || (old_kind && old_kind != new_kind))
        flood_stage[y][x] = 0;
}

static bool floodable_ground(int feat, byte kind)
{
    /* Ordinary walls remain blockers. Traps, doors, rubble and bridges are consumed
     * by the flood and become liquid in their place. cave_set_feat preserves
     * objects already lying on the flooded ground. */
    return feat == FEAT_FLOOR
        || FEAT_IS_TRAP(feat)
        || feat == FEAT_OPEN || feat == FEAT_BROKEN
        || (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)
        || feat == FEAT_SECRET || feat == FEAT_WARDED
        || feat == FEAT_WARDED2 || feat == FEAT_WARDED3
        || feat == FEAT_RUBBLE || FEAT_IS_BRIDGE(feat)
        || (kind == CAVE_FLOOD_KIND_WATER
            && (feat == FEAT_WATER || feat == FEAT_DEEP_WATER))
        || (kind == CAVE_FLOOD_KIND_ACID && feat == FEAT_POISON);
}

static bool flood_diagonal_step_allowed(int y, int x, int ny, int nx,
    byte kind)
{
    if (y == ny || x == nx)
        return true;

    /* Do not let the wave squeeze diagonally through the corner of two
     * blocking cells. */
    return floodable_ground(cave_feat[y][nx], kind)
        && floodable_ground(cave_feat[ny][x], kind);
}

static void flood_path_search_begin(int y, int x)
{
    memset(flood_path_distance, 0, sizeof(flood_path_distance));
    flood_path_distance[y][x] = 1;
    flood_path_queue[0] = y * MAX_DUNGEON_WID + x;
}

static void flood_mark_surface_area(int y, int x, int radius, byte kind)
{
    int head = 0, tail = 1;

    flood_path_search_begin(y, x);
    if (flood_surface_feature(kind, cave_feat[y][x]))
    {
        flood_surface[y][x] = true;
        flood_surface_kind[y][x] = kind;
    }

    while (head < tail)
    {
        int current = flood_path_queue[head++];
        int cy = current / MAX_DUNGEON_WID;
        int cx = current % MAX_DUNGEON_WID;
        int current_distance = flood_path_distance[cy][cx] - 1;

        if (current_distance >= radius)
            continue;

        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
            {
                int ny = cy + dy;
                int nx = cx + dx;
                int next_distance = current_distance + 1;
                if ((!dy && !dx) || !in_bounds_fully(ny, nx)
                    || flood_path_distance[ny][nx]
                    || !floodable_ground(cave_feat[ny][nx], kind)
                    || !flood_diagonal_step_allowed(cy, cx, ny, nx, kind))
                    continue;
                flood_path_distance[ny][nx] = (u16b)(next_distance + 1);
                flood_path_queue[tail++] = ny * MAX_DUNGEON_WID + nx;
                if (flood_surface_feature(kind, cave_feat[ny][nx]))
                {
                    flood_surface[ny][nx] = true;
                    flood_surface_kind[ny][nx] = kind;
                }
            }
    }
}

static void flood_fill_reachable_quota(int y, int x, int quota, byte kind)
{
    int head = 0, tail = 1;
    int filled = 0;
    int surface_feat = flood_surface_feat(kind);

    flood_path_search_begin(y, x);
    while (head < tail && filled < quota)
    {
        int current = flood_path_queue[head++];
        int cy = current / MAX_DUNGEON_WID;
        int cx = current % MAX_DUNGEON_WID;

        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
            {
                if (filled >= quota)
                    break;
                int ny = cy + dy;
                int nx = cx + dx;
                if ((!dy && !dx) || !in_bounds_fully(ny, nx)
                    || flood_path_distance[ny][nx]
                    || !floodable_ground(cave_feat[ny][nx], kind)
                    || !flood_diagonal_step_allowed(cy, cx, ny, nx, kind))
                    continue;
                flood_path_distance[ny][nx] = flood_path_distance[cy][cx] + 1;
                flood_path_queue[tail++] = ny * MAX_DUNGEON_WID + nx;
                if (!flood_surface_feature(kind, cave_feat[ny][nx]))
                {
                    /* Wash the trap away without triggering it, including
                     * another flood plate. Do not leave hidden-trap state
                     * attached to the replacement liquid. */
                    cave_info[ny][nx] &= ~CAVE_HIDDEN;
                    cave_set_feat(ny, nx, surface_feat);
                    flood_surface[ny][nx] = true;
                    flood_surface_kind[ny][nx] = kind;
                    filled++;
                    if (filled >= quota)
                        break;
                }
            }
        if (filled >= quota)
            break;
    }
}

static void cave_flood_trigger_kind(int y, int x, byte kind, cptr message)
{
    if (!in_bounds_fully(y, x) || cave_feat[y][x] != FEAT_TRAP_FLOOD)
        return;
    flood_stage[y][x] = 1;
    flood_new[y][x] = true;
    cave_info[y][x] &= ~CAVE_HIDDEN;
    cave_set_feat(y, x, flood_surface_feat(kind));
    flood_surface[y][x] = true;
    flood_surface_kind[y][x] = kind;
    msg_print(message);
}

void cave_flood_trigger(int y, int x)
{
    byte kind = cave_flood_trap_kind_at(y, x);
    if (kind == CAVE_FLOOD_KIND_ACID)
        cave_flood_trigger_kind(y, x, kind,
            "Acid bursts from beneath your feet!");
    else
        cave_flood_trigger_kind(y, x, CAVE_FLOOD_KIND_WATER,
            "Water bursts from beneath your feet!");
}

void cave_flood_end_action(void)
{
    if (!p_ptr->energy_use || p_ptr->leaving || p_ptr->is_dead)
        return;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            int radius = flood_stage[y][x];
            byte kind = flood_surface_kind[y][x];
            if (!radius || radius == CAVE_FLOOD_STAGE_COMPLETE
                || flood_new[y][x])
                continue;
            if (!kind)
                kind = flood_kind_for_feature(cave_feat[y][x]);
            if (!kind)
            {
                flood_stage[y][x] = 0;
                continue;
            }
            flood_fill_reachable_quota(y, x, radius == 1 ? 8 : 16, kind);
            if (radius == 2)
            {
                if (kind == CAVE_FLOOD_KIND_WATER
                    && cave_feat[y][x] == FEAT_WATER)
                {
                    /* A trap flood is an artificial reservoir: its origin
                     * deepens even when ordinary natural-water rules would
                     * reject deep water near dry ground or walls. */
                    cave_set_feat(y, x, FEAT_DEEP_WATER);
                    flood_surface[y][x] = true;
                    flood_surface_kind[y][x] = kind;
                }
                flood_stage[y][x] = CAVE_FLOOD_STAGE_COMPLETE;
            }
            else
                flood_stage[y][x] = 2;
        }
}

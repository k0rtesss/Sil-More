#include "angband.h"
#include "externs.h"
#include "cave/cave-flood.h"

/* Stage is the radius to apply after the next completed player action.
 * A per-action guard keeps a newly triggered flood at radius zero until the
 * following action. Neither energy cost nor monster turns affect this clock. */
static byte flood_stage[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static bool flood_new[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

void cave_flood_clear(void)
{
    memset(flood_stage, 0, sizeof(flood_stage));
    memset(flood_new, 0, sizeof(flood_new));
}

void cave_flood_begin_action(void)
{
    memset(flood_new, 0, sizeof(flood_new));
}

byte cave_flood_stage_at(int y, int x)
{
    return in_bounds(y, x) ? flood_stage[y][x] : 0;
}

bool cave_flood_restore(int y, int x, byte stage)
{
    if (!in_bounds_fully(y, x) || stage < 1 || stage > 2
        || flood_stage[y][x])
        return false;
    flood_stage[y][x] = stage;
    return true;
}

static bool floodable_ground(int feat)
{
    /* Leave structures, fixtures' support, bridges and other hazards intact.
     * cave_set_feat preserves objects already lying on the flooded ground. */
    return feat == FEAT_FLOOR || feat == FEAT_WATER;
}

void cave_flood_trigger(int y, int x)
{
    if (!in_bounds_fully(y, x) || cave_feat[y][x] != FEAT_TRAP_FLOOD)
        return;
    flood_stage[y][x] = 1;
    flood_new[y][x] = true;
    cave_info[y][x] &= ~CAVE_HIDDEN;
    cave_set_feat(y, x, FEAT_WATER);
    msg_print("Water bursts from beneath your feet!");
}

void cave_flood_end_action(void)
{
    if (!p_ptr->energy_use || p_ptr->leaving || p_ptr->is_dead)
        return;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            int radius = flood_stage[y][x];
            if (!radius || flood_new[y][x])
                continue;
            for (int yy = y - radius; yy <= y + radius; yy++)
                for (int xx = x - radius; xx <= x + radius; xx++)
                    if (in_bounds_fully(yy, xx)
                        && distance(y, x, yy, xx) <= radius
                        && floodable_ground(cave_feat[yy][xx]))
                        cave_set_feat(yy, xx, FEAT_WATER);
            if (radius == 2)
            {
                if (floodable_ground(cave_feat[y][x]))
                    cave_set_feat(y, x, FEAT_DEEP_WATER);
                flood_stage[y][x] = 0;
            }
            else
                flood_stage[y][x] = 2;
        }
}

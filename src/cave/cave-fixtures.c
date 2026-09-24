#include "angband.h"
#include "externs.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-events.h"

/* 53 KiB at the maximum level size; constant-time lookup while drawing. */
static byte fixtures[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

void cave_fixtures_clear(void)
{
    memset(fixtures, 0, sizeof(fixtures));
}

byte cave_fixture_at(int y, int x)
{
    byte kind;
    byte feat;

    if (!p_ptr || !cave_feat || y < 0 || x < 0
        || y >= p_ptr->cur_map_hgt || x >= p_ptr->cur_map_wid)
        return CAVE_FIXTURE_NONE;
    kind = fixtures[y][x];
    feat = cave_feat[y][x];
    /* All fixture textures are mounted on a wall. Keep this check here as
     * well as in generation/load paths so stale metadata can never turn a
     * walkable floor into a fixture tile. */
    if (kind >= CAVE_FIXTURE_WALL_TORCH
        && kind <= CAVE_FIXTURE_WALL_TORCH_2
        && feat >= FEAT_WALL_EXTRA && feat <= FEAT_WALL_SOLID)
        return kind;
    return CAVE_FIXTURE_NONE;
}

void cave_fixture_set(int y, int x, byte kind)
{
    if (y < 0 || x < 0 || y >= MAX_DUNGEON_HGT || x >= MAX_DUNGEON_WID)
        return;
    fixtures[y][x] = kind <= CAVE_FIXTURE_WALL_TORCH_2
        ? kind : CAVE_FIXTURE_NONE;
}

float cave_fixture_sound_gain_at(int y, int x)
{
    float best = 0.0f;

    if (!p_ptr || !in_bounds_fully(y, x))
        return 0;

    for (int yy = y - CAVE_FIXTURE_BRAZIER_SOUND_RADIUS;
         yy <= y + CAVE_FIXTURE_BRAZIER_SOUND_RADIUS; ++yy)
    {
        for (int xx = x - CAVE_FIXTURE_BRAZIER_SOUND_RADIUS;
             xx <= x + CAVE_FIXTURE_BRAZIER_SOUND_RADIUS; ++xx)
        {
            byte kind;
            int radius;
            int grid_distance;

            if (!in_bounds_fully(yy, xx)
                || (kind = cave_fixture_at(yy, xx)) == CAVE_FIXTURE_NONE)
                continue;

            radius = kind == CAVE_FIXTURE_BRAZIER
                ? CAVE_FIXTURE_BRAZIER_SOUND_RADIUS
                : CAVE_FIXTURE_TORCH_SOUND_RADIUS;
            grid_distance = cave_audio_distance(yy, xx, y, x);
            best = MAX(best, sound_distance_gain(grid_distance, radius));
        }
    }

    return best;
}

#include "angband.h"
#include "externs.h"
#include "cave/cave-fixtures.h"

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
    /* Both fixture textures are mounted on a wall. Keep this check here as
     * well as in generation/load paths so stale metadata can never turn a
     * walkable floor into a fixture tile. */
    if ((kind == CAVE_FIXTURE_WALL_TORCH || kind == CAVE_FIXTURE_BRAZIER)
        && feat >= FEAT_WALL_EXTRA && feat <= FEAT_WALL_SOLID)
        return kind;
    return CAVE_FIXTURE_NONE;
}

void cave_fixture_set(int y, int x, byte kind)
{
    if (y < 0 || x < 0 || y >= MAX_DUNGEON_HGT || x >= MAX_DUNGEON_WID)
        return;
    fixtures[y][x] = kind <= CAVE_FIXTURE_BRAZIER ? kind : CAVE_FIXTURE_NONE;
}

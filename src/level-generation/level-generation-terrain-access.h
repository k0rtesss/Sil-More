#ifndef LEVEL_GENERATION_TERRAIN_ACCESS_H
#define LEVEL_GENERATION_TERRAIN_ACCESS_H

#include "angband.h"

/* Generation capability routes, independent of the current character.  A NULL
 * feature map uses cave_feat; a supplied map supports uncommitted proposals. */
bool terrain_generation_walkable(int y, int x,
    const byte (*features)[MAX_DUNGEON_WID]);
bool terrain_generation_jump(int y, int x, int dir_y, int dir_x,
    const byte (*features)[MAX_DUNGEON_WID]);
void terrain_generation_flood(int sy, int sx,
    byte reached[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    const byte (*features)[MAX_DUNGEON_WID]);

#endif

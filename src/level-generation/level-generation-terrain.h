#ifndef INCLUDED_LEVEL_GENERATION_TERRAIN_H
#define INCLUDED_LEVEL_GENERATION_TERRAIN_H

#include "level-generation/level-generation-themes.h"

typedef struct terrain_generation_stats
{
    int proposals, accepted, rejected, tiles, bridges;
    int no_route, blocked_access;
    int material_tiles[TERRAIN_THEME_MATERIAL_MAX];
    int material_features[TERRAIN_THEME_MATERIAL_MAX];
} terrain_generation_stats;

void terrain_generation_reset(void);
void place_dungeon_terrain(void);
/* Temporary takeoffs, run-ups, landings and architectural spans. */
bool terrain_generation_reserved(int y, int x);
void terrain_generation_reserve(int y, int x);
const terrain_generation_stats* terrain_generation_last_stats(void);

#endif

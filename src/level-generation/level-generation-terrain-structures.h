#ifndef INCLUDED_LEVEL_GENERATION_TERRAIN_STRUCTURES_H
#define INCLUDED_LEVEL_GENERATION_TERRAIN_STRUCTURES_H

#include "angband.h"

typedef struct terrain_structure_stats
{
    int flooded, breached, repaired;
    int ruined_walls, rubble_tiles, repair_tiles;
} terrain_structure_stats;

/* Proposal only: never changes cave grids, actors or reservations. The caller
 * validates access/contents and commits work + shadow together. */
void terrain_structure_apply(
    const byte hard[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    byte work[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    byte shadow[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int feature, terrain_structure_stats* stats);
/* Proposal marks: 1 flooded, 2 breached, 3 repaired; reset on each apply. */
int terrain_structure_cell(int y, int x);
/* Proposal owner, including ordinary constructed rooms; zero outside. */
int terrain_structure_owner(int y, int x);

#endif

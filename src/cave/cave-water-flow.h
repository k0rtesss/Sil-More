/* File: cave-water-flow.h */

#ifndef INCLUDED_CAVE_WATER_FLOW_H
#define INCLUDED_CAVE_WATER_FLOW_H

#include "../h-basic.h"

/* Direction encoded for the liquid animation layer.  Zero is still water or
 * pooled acid; the other values point from this grid to the next grid
 * downstream. */
enum cave_water_flow_direction {
    CAVE_WATER_FLOW_CALM = 0,
    CAVE_WATER_FLOW_NORTH = 1,
    CAVE_WATER_FLOW_EAST = 2,
    CAVE_WATER_FLOW_SOUTH = 3,
    CAVE_WATER_FLOW_WEST = 4
};

#define CAVE_WATER_FLOW_SAVE_MAGIC 0xC4F0

/* Build the level's immutable visual flow graph after terrain generation.
 * Freshwater and poisonous acid use the same source-to-outlet graph. */
void cave_water_flow_build(void);

/* Clear the graph when a terrain edit invalidates it.  An invalid graph keeps
 * the visual flow calm until the next build or savefile restore; ambient
 * audio has a compatibility fallback for liquid terrain. */
void cave_water_flow_reset(void);
void cave_water_flow_invalidate(void);
void cave_water_flow_invalidate_at(int y, int x);

/* Generation-only topology for rivers built outside the landmark planner.
 * Mark their channel/basin cells and receiving outlet before the final graph
 * is built. */
void cave_water_flow_generation_plan_reset(void);
void cave_water_flow_generation_plan_cell(int y, int x, int feature,
    bool channel, bool basin, bool outlet);

/* Test/tools hook and savefile support.  set() is intentionally O(1) and
 * makes the table immediately readable by the renderer. */
void cave_water_flow_set(int y, int x, int direction);
int cave_water_flow_direction(int y, int x);
bool cave_water_flow_is_lake(int y, int x);
bool cave_water_flow_is_valid(void);

/* Packed per-cell form used by the dungeon save lane.  Bits 0..2 hold the
 * direction and bit 3 marks a basin/lake cell. */
byte cave_water_flow_encoded_at(int y, int x);
void cave_water_flow_restore_begin(void);
bool cave_water_flow_restore_cell(int y, int x, byte encoded);
void cave_water_flow_restore_finish(void);

#endif /* INCLUDED_CAVE_WATER_FLOW_H */

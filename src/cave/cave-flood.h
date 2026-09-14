#ifndef CAVE_FLOOD_H
#define CAVE_FLOOD_H

/* Pending floods belong to the current level, with fixed trigger origins. */
void cave_flood_clear(void);
void cave_flood_begin_action(void);
void cave_flood_end_action(void);
void cave_flood_trigger(int y, int x);
byte cave_flood_stage_at(int y, int x);
bool cave_flood_restore(int y, int x, byte stage);

#endif

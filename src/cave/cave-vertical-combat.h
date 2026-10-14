#ifndef INCLUDED_CAVE_VERTICAL_COMBAT_H
#define INCLUDED_CAVE_VERTICAL_COMBAT_H

#include "h-basic.h"

/* Availability is a read-only query, suitable for context-menu construction. */
bool cave_vertical_combat_available(int y, int x);
bool cave_vertical_combat_interact(int y, int x);
/* Called on the shared dungeon clock, including turns spent resting. */
void cave_vertical_combat_process(void);
void cave_vertical_combat_reset(void);

#endif

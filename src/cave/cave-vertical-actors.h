#ifndef INCLUDED_CAVE_VERTICAL_ACTORS_H
#define INCLUDED_CAVE_VERTICAL_ACTORS_H

#include "h-basic.h"

/* The scheduler brackets each real energy-consuming creature action. The
 * token makes repeated end hooks harmless without freezing fast creatures. */
void cave_vertical_monster_begin_action(int index);
void cave_vertical_monster_end_action(int index);
/* Handles a planned gravity opening, including a capacity/load deferral.
 * False leaves ordinary chasms and airborne creatures to their usual rules. */
bool cave_vertical_monster_fall(int index);

#endif

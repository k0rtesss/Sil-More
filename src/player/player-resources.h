/* File: player/player-resources.h */

#ifndef INCLUDED_PLAYER_RESOURCES_H
#define INCLUDED_PLAYER_RESOURCES_H

#include "../h-basic.h"

typedef struct object_type object_type;

void calc_torch(void);
bool weapon_glows(const object_type* o_ptr);

#endif /* INCLUDED_PLAYER_RESOURCES_H */

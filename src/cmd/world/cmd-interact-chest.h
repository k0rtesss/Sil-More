/* File: cmd-interact-chest.h */

#ifndef INCLUDED_CMD_INTERACT_CHEST_H
#define INCLUDED_CMD_INTERACT_CHEST_H

#include "h-basic.h"

typedef struct object_type object_type;

void chest_release_contents(object_type* o_ptr, int y, int x, int destroy_typ);

#endif /* INCLUDED_CMD_INTERACT_CHEST_H */

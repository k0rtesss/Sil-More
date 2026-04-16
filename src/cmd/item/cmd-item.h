/* File: cmd-item.h */

/*
 * Transitional public header for the item command split.
 */

#ifndef INCLUDED_CMD_ITEM_H
#define INCLUDED_CMD_ITEM_H

#include "h-basic.h"

typedef struct object_type object_type;

void do_cmd_wield(object_type* default_o_ptr, int default_item);
void do_cmd_takeoff(object_type* default_o_ptr, int default_item);

#endif /* INCLUDED_CMD_ITEM_H */

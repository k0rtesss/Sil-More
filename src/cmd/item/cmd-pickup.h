/* File: cmd-pickup.h */

#ifndef INCLUDED_CMD_PICKUP_H
#define INCLUDED_CMD_PICKUP_H

#include "h-basic.h"

void give_player_item(object_type* o_ptr);
void py_pickup_aux(int o_idx);
void do_cmd_pickup_from_pile(void);

#endif /* INCLUDED_CMD_PICKUP_H */

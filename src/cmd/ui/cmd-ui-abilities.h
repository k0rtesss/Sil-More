#ifndef INCLUDED_CMD_UI_ABILITIES_H
#define INCLUDED_CMD_UI_ABILITIES_H

/*
 * Transitional staging header for the Wave 7A abilities continuation lane.
 * Keeps the lane-owned command surface out of the broader cmd-ui umbrella.
 */

#include "h-basic.h"

typedef struct object_type object_type;

void do_cmd_change_song(void);
void do_cmd_ability_screen(void);
void add_random_curse(object_type* o_ptr);

#endif /* INCLUDED_CMD_UI_ABILITIES_H */

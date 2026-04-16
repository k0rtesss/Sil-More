/* File: cmd-movement.h */

/*
 * Transitional public header for the movement command split.
 */

#ifndef INCLUDED_CMD_MOVEMENT_H
#define INCLUDED_CMD_MOVEMENT_H

#include "h-basic.h"

extern const byte cycle[];
extern const byte chome[];

int min_depth(void);
void min_depth_timer_status(int* base_increment, int* additional_increment,
    int* total_increment, int* progress, int* threshold);
void run_step(int dir);

#endif /* INCLUDED_CMD_MOVEMENT_H */

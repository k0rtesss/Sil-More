/* File: cmd-movement.h */
/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

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
void note_lost_greater_vault(void);
void do_cmd_go_up(void);
void do_cmd_go_down(void);
void do_cmd_search(void);
void do_cmd_toggle_stealth(void);
void search(void);
void perceive(void);
void do_cmd_walk(void);
void do_cmd_run(void);
void do_cmd_hold(void);
void do_cmd_pickup(void);
void do_cmd_rest(void);
void run_step(int dir);

#endif /* INCLUDED_CMD_MOVEMENT_H */

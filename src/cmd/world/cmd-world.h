/* File: cmd-world.h */
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
 * Transitional internal header for the world command split.
 */

#ifndef INCLUDED_CMD_WORLD_H
#define INCLUDED_CMD_WORLD_H

#include "h-basic.h"
#include "cmd-interact-chest.h"

s16b cmd_interact_chest_check(int y, int x);
bool cmd_interact_is_open(int feat);
bool cmd_interact_is_closed(int feat);
bool cmd_interact_is_trap(int feat);
int cmd_interact_count_feats(int* y, int* x, bool (*test)(int feat),
    bool under);
int cmd_interact_count_chests(int* y, int* x, bool trapped);
int cmd_interact_coords_to_dir(int y, int x);

bool cmd_interact_close_aux(int y, int x);
bool cmd_interact_tunnel_aux(int y, int x);
bool cmd_interact_disarm_aux(int y, int x);
bool cmd_interact_bash_aux(int y, int x);
void do_cmd_exchange(void);
void do_cmd_alter(void);
bool do_cmd_open_aux(int y, int x);
void do_cmd_open(void);
void do_cmd_close(void);
void do_cmd_tunnel(void);
void do_cmd_disarm(void);
void do_cmd_bash(void);
bool break_free_of_web(void);

#endif /* INCLUDED_CMD_WORLD_H */

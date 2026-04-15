/* File: cmd-world.h */

/*
 * Transitional internal header for the world command split.
 */

#ifndef INCLUDED_CMD_WORLD_H
#define INCLUDED_CMD_WORLD_H

#include "h-basic.h"

s16b cmd_interact_chest_check(int y, int x);
bool cmd_interact_is_open(int feat);
bool cmd_interact_is_closed(int feat);
bool cmd_interact_is_trap(int feat);
int cmd_interact_count_feats(int* y, int* x, bool (*test)(int feat),
    bool under);
int cmd_interact_count_chests(int* y, int* x, bool trapped);
int cmd_interact_coords_to_dir(int y, int x);

void cmd_interact_search_skeleton(int y, int x, s16b o_idx);
bool cmd_interact_open_chest(int y, int x, s16b o_idx);
bool cmd_interact_disarm_chest(int y, int x, s16b o_idx);
bool cmd_interact_close_aux(int y, int x);
bool cmd_interact_tunnel_aux(int y, int x);
bool cmd_interact_disarm_aux(int y, int x);
bool cmd_interact_bash_aux(int y, int x);

#endif /* INCLUDED_CMD_WORLD_H */

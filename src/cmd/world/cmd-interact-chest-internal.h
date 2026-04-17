/* File: cmd-interact-chest-internal.h */

#ifndef INCLUDED_CMD_INTERACT_CHEST_INTERNAL_H
#define INCLUDED_CMD_INTERACT_CHEST_INTERNAL_H

#include "cmd-interact-chest.h"

typedef enum
{
    CHEST_ALIGNMENT_STANDARD = 0,
    CHEST_ALIGNMENT_NOBLE = 1,
    CHEST_ALIGNMENT_EVIL = 2,
    CHEST_ALIGNMENT_INVALID = 3
} chest_alignment_type;

void chest_apply_drop_alignment(chest_alignment_type alignment);
chest_alignment_type chest_item_alignment(const object_type* o_ptr);

void cmd_interact_search_skeleton_impl(int y, int x, s16b o_idx);
bool cmd_interact_open_chest_impl(int y, int x, s16b o_idx);
bool cmd_interact_disarm_chest_impl(int y, int x, s16b o_idx);
void cmd_interact_chest_maybe_show_skeleton_note(byte sval, int skel_y, int skel_x);

#endif /* INCLUDED_CMD_INTERACT_CHEST_INTERNAL_H */
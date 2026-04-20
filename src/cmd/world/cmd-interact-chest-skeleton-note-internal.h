/* File: cmd-interact-chest-skeleton-note-internal.h */
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

#ifndef INCLUDED_CMD_INTERACT_CHEST_SKELETON_NOTE_INTERNAL_H
#define INCLUDED_CMD_INTERACT_CHEST_SKELETON_NOTE_INTERNAL_H

#include "cmd-interact-chest-internal.h"

bool skeleton_note_is_quest_giver_r_idx(int r_idx);
const char* skeleton_note_quest_site_name(int r_idx);
const char* skeleton_get_unique_type_name(const monster_race* r_ptr);
const char* skeleton_note_direction_phrase(int from_y, int from_x, int to_y,
    int to_x);
const char* skeleton_note_distance_phrase(int dist,
    const level_layout_info* layout);
bool skeleton_note_find_nearest_stairs(byte sval, int from_y, int from_x,
    int* out_y, int* out_x, int* out_feat, int* out_dist);
bool skeleton_note_find_nearest_forge(int from_y, int from_x, int* out_y,
    int* out_x, int* out_feat, int* out_dist);
bool skeleton_note_find_nearest_quest_site(int from_y, int from_x, int* out_y,
    int* out_x, int* out_dist, const char** out_site);
bool skeleton_note_find_nearest_great_vault(int from_y, int from_x, int* out_y,
    int* out_x, int* out_dist);
bool skeleton_note_find_nearest_artefact(int from_y, int from_x, int* out_y,
    int* out_x, int* out_dist, const char** out_site,
    char* out_artefact_kind, size_t out_artefact_kind_sz);
bool skeleton_note_find_nearest_unique(int from_y, int from_x, int* out_r_idx,
    int* out_y, int* out_x, int* out_dist);
bool skeleton_note_find_nearest_partition_site(level_partition_kind kind,
    big_cave_type_t cave_type, int from_y, int from_x, int* out_y,
    int* out_x, int* out_dist);
const char* skeleton_note_stair_site(int feat);
const char* skeleton_note_stair_title(int feat);
void skeleton_note_partition_meta_for_hint(skeleton_hint_kind hint,
    level_partition_kind* out_kind, big_cave_type_t* out_type);
const char* skeleton_note_forge_site(int feat, char* buf, size_t buf_sz);
void hint_message_meta_init(hint_message_meta* meta, int source_y,
    int source_x);
void hint_message_meta_add_cue(hint_message_meta* meta, const char* dist,
    const char* dir);
const char* skeleton_hint_title(skeleton_hint_kind hint, int stairs_feat);

#endif /* INCLUDED_CMD_INTERACT_CHEST_SKELETON_NOTE_INTERNAL_H */

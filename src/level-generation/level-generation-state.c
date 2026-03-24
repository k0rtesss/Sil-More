/* File: level-generation-state.c */

/*
 * Shared mutable state for level generation.
 */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

static dun_data dun_body;
dun_data* dun = &dun_body;

int cave_corridor1[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
int cave_corridor2[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
bool cave_escape_tunnel[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

layout_anchor_t layout_anchors[LAYOUT_ANCHOR_MAX];
int layout_anchor_count = 0;
layout_anchor_kind_t room_anchor_kind[CENT_MAX];
bool room_anchor_requires_neighbor[CENT_MAX];

int current_partition_rows = 0;
int current_partition_cols = 0;
int current_partition_count = 0;
quadrant_mode_t current_partition_modes[25];
density_level_t current_partition_densities[25];
big_cave_type_t current_partition_big_cave_types[25];
int current_partition_bridge_styles[25];

bool g_big_cave_type_rule_set[32];
int g_big_cave_type_weight[32][BIG_CAVE_TYPE_MAX];

int current_labyrinth_partitions = 0;

bool morgoth_level_active = false;
bool morgoth_partition_reserved = false;
int morgoth_partition_index = -1;
rectangle morgoth_partition_bounds;
int morgoth_vault_center_y = 0;
int morgoth_vault_center_x = 0;

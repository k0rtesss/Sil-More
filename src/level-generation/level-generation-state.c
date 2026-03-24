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
partition_population_meta current_partition_population_meta[25];

bool morgoth_level_active = false;
bool morgoth_partition_reserved = false;
int morgoth_partition_index = -1;
rectangle morgoth_partition_bounds;
int morgoth_vault_center_y = 0;
int morgoth_vault_center_x = 0;

void init_partition_chest_recipe(partition_chest_recipe* recipe)
{
    if (!recipe)
        return;

    recipe->chest_mode = 0;
    recipe->material_wood_pct = -1;
    recipe->material_steel_pct = -1;
    recipe->material_jewel_pct = -1;
    recipe->anchor_pref = PARTITION_CHEST_ANCHOR_ANY;
}

void set_partition_chest_recipe(partition_population_meta* meta, int slot,
    int chest_mode, int wooden_pct, int steel_pct, int jewel_pct,
    partition_chest_anchor_pref anchor_pref)
{
    if (!meta || slot < 0 || slot >= PARTITION_CHEST_RECIPE_MAX)
        return;

    meta->chest_recipes[slot].chest_mode = (byte)chest_mode;
    meta->chest_recipes[slot].material_wood_pct = (s16b)wooden_pct;
    meta->chest_recipes[slot].material_steel_pct = (s16b)steel_pct;
    meta->chest_recipes[slot].material_jewel_pct = (s16b)jewel_pct;
    meta->chest_recipes[slot].anchor_pref = (byte)anchor_pref;
    if (meta->chest_count < slot + 1)
        meta->chest_count = slot + 1;
}

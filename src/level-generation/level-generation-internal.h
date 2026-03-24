/* File: level-generation-internal.h */

#ifndef INCLUDED_LEVEL_GENERATION_INTERNAL_H
#define INCLUDED_LEVEL_GENERATION_INTERNAL_H

#include "level-generation/level-generation.h"

#ifndef CENT_MAX
#define CENT_MAX DUN_ROOMS
#endif

#ifndef ROOM_MIN
#define ROOM_MIN 2
#endif

typedef struct coord coord;
struct coord
{
    byte y;
    byte x;
};

typedef struct rectangle rectangle;
struct rectangle
{
    byte y1;
    byte x1;
    byte y2;
    byte x2;
};

typedef enum room_kind
{
    ROOM_KIND_NONE = 0,
    ROOM_KIND_CLASSIC = 1,
    ROOM_KIND_CROSS = 2,
    ROOM_KIND_INTERESTING = 6,
    ROOM_KIND_LESSER_VAULT = 7,
    ROOM_KIND_GREATER_VAULT = 8
} room_kind_t;

typedef struct dun_data dun_data;
struct dun_data
{
    byte kind[CENT_MAX];
    bool is_quest[CENT_MAX];
    int cent_n;
    coord cent[CENT_MAX];
    rectangle corner[CENT_MAX];
    byte piece[CENT_MAX];
    bool connection[DUN_ROOMS][DUN_ROOMS];
};

#define LAYOUT_ANCHOR_MAX CENT_MAX

typedef enum layout_anchor_kind
{
    LAYOUT_ANCHOR_NONE = 0,
    LAYOUT_ANCHOR_ROOM,
    LAYOUT_ANCHOR_PREFAB,
    LAYOUT_ANCHOR_CA_BLOB,
    LAYOUT_ANCHOR_BSP_SLICE,
    LAYOUT_ANCHOR_SETPIECE
} layout_anchor_kind_t;

typedef struct layout_anchor
{
    layout_anchor_kind_t kind;
    rectangle bounds;
    coord center;
    byte room_kind;
    bool is_quest;
    bool requires_neighbor;
    bool neighbor_linked;
    int style_primary;
    int room_slot;
} layout_anchor_t;

static inline int room_capacity_limit(void)
{
    return MIN(CENT_MAX, DUN_ROOMS - 1);
}

typedef enum quadrant_mode
{
    QUAD_MODE_ROOMY = 0,
    QUAD_MODE_CAVEY,
    QUAD_MODE_RUINED,
    QUAD_MODE_LABYRINTH,
    QUAD_MODE_CHASM,
    QUAD_MODE_BIG_CAVE
} quadrant_mode_t;

typedef enum density_level
{
    DENSITY_SPARSE = 0,
    DENSITY_NORMAL,
    DENSITY_DENSE
} density_level_t;

typedef struct pending_quest_states {
    bool has_aule_change;
    bool has_mandos_change;
    bool has_varda_change;
    bool has_varda_shadow_change;
    bool has_tulkas_change;
    int aule_level;
    int mandos_level;
    int varda_level;
    int varda_shadow_level;
    int tulkas_level;
    int aule_forge_y, aule_forge_x;
    int mandos_vault_y, mandos_vault_x;
    int varda_vault_y, varda_vault_x;
    int varda_shadow_y, varda_shadow_x;
    int mandos_quest_id;
    int mandos_next_state;
    int tulkas_next_state;
    bool tulkas_spawn_pending;
} pending_quest_states_t;

extern dun_data* dun;
extern int cave_corridor1[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
extern int cave_corridor2[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
extern bool cave_escape_tunnel[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

extern layout_anchor_t layout_anchors[LAYOUT_ANCHOR_MAX];
extern int layout_anchor_count;
extern layout_anchor_kind_t room_anchor_kind[CENT_MAX];
extern bool room_anchor_requires_neighbor[CENT_MAX];

extern int current_partition_rows;
extern int current_partition_cols;
extern int current_partition_count;
extern quadrant_mode_t current_partition_modes[25];
extern density_level_t current_partition_densities[25];
extern big_cave_type_t current_partition_big_cave_types[25];
extern int current_partition_bridge_styles[25];
extern bool g_big_cave_type_rule_set[32];
extern int g_big_cave_type_weight[32][BIG_CAVE_TYPE_MAX];
extern int current_labyrinth_partitions;

extern bool morgoth_level_active;
extern bool morgoth_partition_reserved;
extern int morgoth_partition_index;
extern rectangle morgoth_partition_bounds;
extern int morgoth_vault_center_y;
extern int morgoth_vault_center_x;

extern bool morgoth_region_active(void);
extern bool coord_in_morgoth_region(int y, int x, int margin);
extern bool morgoth_segment_blocked(int y1, int x1, int y2, int x2, int margin);
extern void reset_morgoth_layout_state(bool active);
extern void seal_morgoth_partition(const vault_type* v_ptr, int y0, int x0);

extern void set_perm_boundry(void);
extern void basic_granite(void);
extern void make_patches_of_sunlight(void);
extern bool varda_sunlight_tile_ok(int y, int x, bool require_empty);
extern bool varda_no_rubble_path_tile_ok(int y, int x,
    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID]);
extern int pick_varda_sunlight_spawn_tile(int *out_y, int *out_x,
    int *out_total_sunlight, int *out_empty_sunlight);
extern bool force_varda_sunlight_tile(int *out_y, int *out_x);
extern void ensure_sunlight_for_varda(void);

extern void flood_access(int y, int x,
    int access_array[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    bool ignore_rubble_and_chasms);

extern pending_quest_states_t pending_quest_states;
extern int quest_lottery_winner;

extern bool quest_metarun_blocked(int quest_id, u32b metarun_flag);
extern bool mandos_second_stage_ready(void);
extern bool mandos_third_stage_ready(void);
extern bool is_easterling_quest_vault(vault_type* v);
extern bool is_maeglin_quest_vault(vault_type* v);
extern bool spawn_tulkas_near_player_with_fallback(void);
extern void run_quest_lottery(void);
extern void reset_quest_lottery_state(void);
extern void reset_pending_quest_states(void);
extern bool run_has_consumed_quest_slot(void);
extern void reset_quest_vault_states(bool preserve_run_quest_slot);
extern void apply_pending_quest_states(void);
extern void level_gen_debug_note_questgiver(int quest_id);

#endif /* INCLUDED_LEVEL_GENERATION_INTERNAL_H */

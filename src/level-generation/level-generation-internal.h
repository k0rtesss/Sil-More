/* File: level-generation-internal.h */

#ifndef INCLUDED_LEVEL_GENERATION_INTERNAL_H
#define INCLUDED_LEVEL_GENERATION_INTERNAL_H

#include "level-generation/level-generation.h"
#include "drop_system.h"

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

typedef struct partition_drop_profile
{
    bool allow_floor_drops;
    drop_profile profile;
} partition_drop_profile;

#ifndef ALLOC_SET_CORR
#define ALLOC_SET_CORR 1
#endif
#ifndef ALLOC_SET_ROOM
#define ALLOC_SET_ROOM 2
#endif
#ifndef ALLOC_SET_BOTH
#define ALLOC_SET_BOTH 3
#endif

#ifndef ALLOC_TYP_RUBBLE
#define ALLOC_TYP_RUBBLE 1
#endif
#ifndef ALLOC_TYP_OBJECT
#define ALLOC_TYP_OBJECT 5
#endif

typedef enum partition_chest_anchor_pref
{
    PARTITION_CHEST_ANCHOR_ANY = 0,
    PARTITION_CHEST_ANCHOR_BSP_SLICE
} partition_chest_anchor_pref;

typedef struct partition_chest_recipe
{
    byte chest_mode;
    s16b material_wood_pct;
    s16b material_steel_pct;
    s16b material_jewel_pct;
    byte anchor_pref;
} partition_chest_recipe;

#define PARTITION_CHEST_RECIPE_MAX 3

typedef struct partition_population_meta
{
    int chest_count;
    partition_chest_recipe chest_recipes[PARTITION_CHEST_RECIPE_MAX];
} partition_population_meta;

typedef struct partition_population_plan
{
    int pi;
    quadrant_mode_t mode;
    big_cave_type_t cave_type;
    int y1, y2, x1, x2;
    int room_centers;
    int floor_count;
    int room_floor_count;
    int corridor_floor_count;
    int floor_count_non_icky;
    int floor_count_non_vault;
    partition_population_meta meta;
    int monsters_base;
    int monsters_floor;
    int monsters_depth;
    int monsters_precurse;
    int monsters_curse_bonus;
    int monsters_total;
    int room_objects;
    int corr_objects;
} partition_population_plan;

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
extern partition_population_meta current_partition_population_meta[25];

extern layout_anchor_t layout_anchors[LAYOUT_ANCHOR_MAX];
extern int layout_anchor_count;
extern layout_anchor_kind_t room_anchor_kind[CENT_MAX];
extern bool room_anchor_requires_neighbor[CENT_MAX];

extern int style_at_color(int y, int x);
extern void layout_anchor_reset(void);
extern void mark_room_anchor_meta(int room_idx, layout_anchor_kind_t kind,
    bool requires_neighbor);
extern void layout_anchor_capture_existing_rooms(void);
extern void seed_prefab_anchors(void);
extern bool room_kind_is_vault(byte kind);
extern void record_partition_metadata(const quadrant_mode_t* modes,
    const density_level_t* densities, int count);
extern void fallback_partition_grid_from_blocks(int blocks, int *rows, int *cols);
extern bool area_is_reserved_or_dense(int y1, int y2, int x1, int x2,
    int *floor_pct_out, int *icky_pct_out);
extern bool compute_partition_bounds(int pi, int rows, int cols,
    int *y1, int *y2, int *x1, int *x2);
extern bool level_has_chasm_partition(void);
extern void apply_chasm_partition_tags(void);
extern void apply_partition_and_room_glow_rules(void);
extern void remember_partition_grid(int rows, int cols, int count);
extern int partition_index_from_point(int y, int x, int rows, int cols);
extern void connect_partition_hubs(void);
extern int scaled_attempts(int base, int area_factor);
extern quadrant_mode_t pick_weighted_mode(const int *weights, int count);
extern int mode_weight_for_depth(quadrant_mode_t mode, int depth, int blocks,
    const int* mode_counts, int partition_count);
extern bool room_build_in_bounds(int typ, int y1, int y2, int x1, int x2);
extern bool place_room_with_budget(int typ, int y1, int y2, int x1, int x2,
    int max_tries, int depth, int *budget_t6, int *budget_t7,
    int *budget_t8, int *used_t6, int *used_t7, int *used_t8);
extern void cave_set_feat_style(int y, int x, int feat, int style_idx);
extern void scatter_quartz_veins_in_bounds(int y1, int y2, int x1, int x2,
    u16b info_flag);
extern bool bounds_have_chasm_tag(int y1, int y2, int x1, int x2);
extern bool carve_ca_blob_anchor_bounds(int y_min, int y_max, int x_min,
    int x_max, int style_idx);
extern bool carve_bsp_slice_anchor_bounds(int y_min, int y_max, int x_min,
    int x_max);
extern bool carve_big_cave_bounds(int y_min, int y_max, int x_min, int x_max,
    int style_idx, big_cave_type_t cave_type);
extern bool build_type1(int y0, int x0);
extern bool build_type2(int y0, int x0);
extern bool build_type6(int y0, int x0, bool force_forge);
extern bool build_type7(int y0, int x0);
extern bool build_type8(int y0, int x0);

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
extern bool player_passable(int y, int x, bool ignore_rubble_and_chasms);
extern int calculate_nearest_down_stair_distance_from(int y0, int x0);
extern bool feature_is_any_door(int feat);
extern int squash_double_doors(void);
extern int dungeon_pieces(void);
extern quadrant_mode_t partition_mode_for_point(int y, int x);
extern quadrant_mode_t drop_mode_for_point(int y, int x);
extern partition_drop_profile partition_drop_profile_for_mode_source_cfg(
    quadrant_mode_t mode, partition_drop_source_t source);
extern void place_object_with_profile_params(
    int y, int x, int base_depth, int min_depth_penalty_depth,
    drop_quality quality, int droptype, bool allow_artefacts,
    int artefact_weight_multiplier, u32b extra_ident,
    const partition_drop_profile* prof);
extern bool connect_two_rooms(int r1, int r2, bool tentative, bool desperate);
extern int trap_placement_chance(int y, int x);
extern void place_traps(void);
extern bool place_rubble_player(void);
extern bool check_connectivity(void);
extern bool connect_rooms_stairs(void);
extern void reset_partition_population_metadata(void);
extern bool generation_escape_tunnel_bold(int y, int x);
extern void mark_generation_escape_tunnel(int y, int x);
extern bool tunnel_should_mark_escape(int r1, int r2);
extern void init_partition_chest_recipe(partition_chest_recipe* recipe);
extern int build_partition_population_plans(
    partition_population_plan* plans, int max_plans);
extern bool partition_monster_pass_skips_plan(
    const partition_population_plan* plans);
extern int run_partition_object_pass(
    const partition_population_plan* plans, int plan_count, bool rooms);
extern int run_partition_monster_pass(
    const partition_population_plan* plans, int plan_count);
extern int run_partition_special_scatter_pass(
    const partition_population_plan* plans, int plan_count);
extern int qv_stored_y1;
extern int qv_stored_x1;
extern int qv_stored_y2;
extern int qv_stored_x2;
extern bool qv_placed_this_level;
extern bool choose_chasm_sanctum_seed(
    const bool* is_cave, int h, int w, int* out_y, int* out_x);
extern void place_chasm_island_sanctum(int cy, int cx);
extern void check_quest_vault_integrity(const char* checkpoint_name);
extern bool build_reserved_type8(int y0, int x0);
extern bool build_type9(int y0, int x0, vault_type** used_vault);
extern void carve_morgoth_entry_tunnels(const vault_type* v_ptr, int y0, int x0);
extern bool connect_morgoth_entry_tunnels(void);
extern bool build_type10(int y0, int x0);
extern int vault_type8_generation_rarity(const vault_type* v_ptr, int depth);
extern bool quest_vault_surface_roll_allows(const vault_type* v_ptr, int depth);
extern bool place_duruin_bastion(void);
extern bool place_shadow_bastion(void);
extern bool try_quest_vault_type(int v_type, bool *had_eligible_candidate);
extern bool place_monster_by_flag_try(int y, int x, int flagset, u32b flag,
    bool allow_unique, int max_depth);
extern bool place_monster_by_letter_try(int y, int x, char letter,
    bool allow_unique, int max_depth);
extern bool place_big_cave_elemental_monster(int y, int x,
    big_cave_type_t cave_type, int max_depth);
extern bool place_big_cave_troll_or_giant(int y, int x, int max_depth);

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
extern void level_gen_debug_note_room_name(cptr name);
extern void level_gen_debug_note_greater_vault_name(cptr name);
extern void level_gen_debug_note_quest_vault_name(cptr name);
extern void level_gen_debug_activate_quest_vault_name(cptr name);
extern void level_gen_debug_note_questgiver(int quest_id);
extern bool place_orc_stronghold(void);

#endif /* INCLUDED_LEVEL_GENERATION_INTERNAL_H */

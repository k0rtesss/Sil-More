#ifndef INCLUDED_LEVEL_STATE_H
#define INCLUDED_LEVEL_STATE_H

#include "level-generation/level-generation-internal.h"
#include "cave/cave-environment.h"
#include "cave/cave-events.h"
#include "cave/cave-atmosphere.h"
#include "monster/monster-social.h"
#include "cave/cave-vertical.h"
#include "level-generation/level-generation-vertical-vaults.h"
#include "level-generation/level-stream.h"

/* Scalar run changes made by generation. Keep this list in one place for the
 * deferred commit and its portable, typed save representation. */
#define LEVEL_GENERATED_PLAYER_FIELDS(X) \
    X(unique_forge_made) X(forge_count) X(quest_vault_used) \
    X(tulkas_quest) X(tulkas_target_r_idx) X(tulkas_prize_a_idx) \
    X(aule_quest) X(aule_level) X(aule_forge_y) X(aule_forge_x) \
    X(mandos_quest) X(mandos_level) X(mandos_vault_y) X(mandos_vault_x) \
    X(niena_quest) X(niena_monsters_seen) X(niena_monsters_killed) \
    X(orome_quest) X(orome_target_type) X(orome_target_count) \
    X(varda_quest) X(varda_level) X(varda_vault_placed) X(varda_vault_ready)

/* No pointers into another level's monsters, objects, or mutable terrain.
 * Objects have their own allocation because o_max is a template limit. */
typedef struct level_image {
    int depth, hgt, wid, py, px;
    level_stream_frame stream;
    byte stream_site_count;
    level_stream_site_def stream_sites[LEVEL_STREAM_SITE_DEFS];
    s32b suspended_turn;
    bool visited;
    bool pending_player;
    player_type generation_before, generation_after;
    byte feeling, do_feeling, traps;
    s16b rating, mon_max, mon_cnt, o_max, o_cnt, repro;
    bool good_item, truce;
    char vault_name[80];
    u16b info[MAX_DUNGEON_HGT][256];
    byte feat[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte color[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte natural[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte rewired[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    u16b legendary[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    s16b light[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    s16b objects_at[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    s16b monsters_at[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte scent[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    int scent_when;
    byte wandering_cost[MAX_WANDERING_GROUPS][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte flow_y[MAX_FLOWS], flow_x[MAX_FLOWS];
    byte update_y[MAX_FLOWS], update_x[MAX_FLOWS];
    s16b wandering[MAX_FLOWS];
    monster_type monsters[MAX_MONSTERS];
    object_type* objects;
    void* legendary_runtime;
    int partition_rows, partition_cols, partition_count, labyrinths;
    quadrant_mode_t modes[PARTITION_META_MAX];
    density_level_t densities[PARTITION_META_MAX];
    big_cave_type_t big_caves[PARTITION_META_MAX];
    int bridge_styles[PARTITION_META_MAX];
    partition_population_meta population[PARTITION_META_MAX];
    byte atmospheres[PARTITION_META_MAX];
    byte floor_choices[64], door_choices[64];
    int primary_style;
    environment_state environment;
    environment_cell cells[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    environment_source sources[ENV_SOURCES_MAX];
    byte fixtures[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte floods[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte flood_surface[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte flood_traps[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte water_flow[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte relations[MON_GROUP_MAX][MON_GROUP_MAX];
    cave_world_event events[CAVE_EVENTS_MAX];
    u32b next_event;
    catastrophe_state catastrophe;
    u32b catastrophe_steps[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    skeleton_note_state_save skeleton;
    byte hint_count, hint_lengths[32];
    char hints[32][16][100];
    hint_message_meta hint_metadata[32];
    /* Remaining distance to the landing, including this floor. At the top a
     * shaft spans at least two floors; each additional floor is less likely. */
    byte chasm_drop[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    byte chasm_up[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    cave_vertical_state vertical;
    vertical_vault_state vertical_vaults;
} level_image;

level_image* level_image_new(void);
level_image* level_image_clone(const level_image* source);
void level_image_free(level_image* image);
void level_image_capture(level_image* image);
void level_image_restore(const level_image* image);
void level_image_bind_grids(level_image* image);
void level_image_repair_social(level_image* image);

#endif

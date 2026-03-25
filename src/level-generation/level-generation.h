/* File: level-generation.h */

/*
 * Transitional public API for the level-generation split.
 *
 * These declarations currently describe the generate.c surface on the
 * quests-and-refactor branch. Keeping them here lets later file moves land
 * without expanding externs.h again.
 */

#ifndef INCLUDED_LEVEL_GENERATION_H
#define INCLUDED_LEVEL_GENERATION_H

#include "h-basic.h"

#ifndef LEVEL_LAYOUT_INFO_DEFINED
#define LEVEL_LAYOUT_INFO_DEFINED
typedef enum
{
    LEVEL_PART_NONE = 0,
    LEVEL_PART_ROOMY,
    LEVEL_PART_CAVEY,
    LEVEL_PART_RUINED,
    LEVEL_PART_LABYRINTH,
    LEVEL_PART_CHASM,
    LEVEL_PART_BIG_CAVE,
    LEVEL_PART_MAX
} level_partition_kind;

typedef struct
{
    int map_wid;
    int map_hgt;
    int partition_rows;
    int partition_cols;
    int partition_count;
    int labyrinth_parts;
    int big_cave_parts;
    int chasm_parts;
    level_partition_kind dominant_kind;
} level_layout_info;

typedef enum
{
    BIG_CAVE_NONE = 0,
    BIG_CAVE_ICE,
    BIG_CAVE_FIRE,
    BIG_CAVE_POIS,
    BIG_CAVE_TYPE_MAX
} big_cave_type_t;

typedef enum
{
    PART_STYLE_CA_BLOB = 0,
    PART_STYLE_LABYRINTH,
    PART_STYLE_CHASM_FLOOR,
    PART_STYLE_CHASM_BRIDGE,
    PART_STYLE_BIG_CAVE_ICE,
    PART_STYLE_BIG_CAVE_FIRE,
    PART_STYLE_BIG_CAVE_POIS,
    PART_STYLE_MAX
} partition_style_kind_t;

typedef enum
{
    PARTITION_DROP_SOURCE_FLOOR = 0,
    PARTITION_DROP_SOURCE_CHEST,
    PARTITION_DROP_SOURCE_MONSTER,
    PARTITION_DROP_SOURCE_MAX
} partition_drop_source_t;
#endif

#ifndef SKELETON_NOTE_STATE_SAVE_DEFINED
#define SKELETON_NOTE_STATE_SAVE_DEFINED
#define SKELETON_NOTE_SEEN_MAX 8

typedef struct skeleton_note_state_save {
    s16b level_depth;
    s16b note_cap;
    s16b notes_shown;
    s16b map_wid;
    s16b map_hgt;
    u32b hint_used_mask;
    byte seen_count;
    s16b seen_ids[SKELETON_NOTE_SEEN_MAX];
} skeleton_note_state_save;
#endif

#ifndef HINT_MESSAGE_META_DEFINED
#define HINT_MESSAGE_META_DEFINED
#define HINT_MESSAGE_CUE_MAX 2
#define HINT_MESSAGE_CUE_TEXT_MAX 32

typedef struct hint_message_meta {
    s16b source_y;
    s16b source_x;
    byte cue_count;
    char cue_dirs[HINT_MESSAGE_CUE_MAX][HINT_MESSAGE_CUE_TEXT_MAX];
    char cue_dists[HINT_MESSAGE_CUE_MAX][HINT_MESSAGE_CUE_TEXT_MAX];
} hint_message_meta;
#endif

#ifndef PARTITION_META_SAVE_DEFINED
#define PARTITION_META_SAVE_DEFINED
#define PARTITION_META_MAX 25

typedef struct partition_meta_save {
    s16b grid_rows;
    s16b grid_cols;
    s16b partition_count;
    byte modes[PARTITION_META_MAX];
    byte big_cave_types[PARTITION_META_MAX];
} partition_meta_save;
#endif

extern void place_monster_by_flag(
    int y, int x, int flagset, u32b f, bool allow_unique, int max_depth);
extern void place_random_stairs(int y, int x);
extern byte get_nest_theme(int nestlevel);
extern byte get_pit_theme(int pitlevel);
extern byte num_trap_on_level;
extern char g_vault_name[80];
extern void level_layout_info_current(level_layout_info* out);
extern level_partition_kind level_partition_kind_for_point(int y, int x);
extern int level_partition_index_for_point(int y, int x);
extern void level_partition_meta_get(partition_meta_save* out);
extern void level_partition_meta_set(const partition_meta_save* in);
extern void big_cave_type_rules_clear(void);
extern void big_cave_type_set_rule(
    int depth, int ice_weight, int fire_weight, int pois_weight);
extern big_cave_type_t big_cave_type_pick_for_depth(int depth);
extern big_cave_type_t level_partition_big_cave_type_for_point(int y, int x);
extern big_cave_type_t level_partition_big_cave_type_for_index(int pi);
extern void log_partition_debug_for_point(const char* tag, int y, int x);
extern void skeleton_note_level_reset(void);
extern void reset_hint_skeleton_state(void);
extern void skeleton_note_get_state(skeleton_note_state_save* out);
extern void skeleton_note_set_state(const skeleton_note_state_save* in);
extern void hint_messages_level_reset(void);
extern void hint_messages_ensure_level_state(void);
extern byte hint_messages_count_for_save(void);
extern s16b hint_messages_level_depth_for_save(void);
extern s16b hint_messages_map_wid_for_save(void);
extern s16b hint_messages_map_hgt_for_save(void);
extern byte hint_messages_message_line_count(int index);
extern const char* hint_messages_message_line(int index, int line);
extern void hint_messages_message_meta(int index, hint_message_meta* out);
extern void hint_messages_clear_for_load(
    s16b level_depth, s16b map_wid, s16b map_hgt);
extern int hint_messages_add_for_load(
    const char lines[][100], int line_count, const hint_message_meta* meta);
extern int hint_messages_add_note_lines(
    const char note_lines[][100], const hint_message_meta* meta);
extern void show_hint_message_screen(int index);
extern void trigger_chasm_sanctum_ambush_if_needed(int y, int x);
extern void generate_cave(void);

#ifdef ALLOW_DEBUG
extern void debug_run_quest_roulette(void);
extern int debug_get_quest_lottery_winner(void);
#endif /* ALLOW_DEBUG */

#endif /* INCLUDED_LEVEL_GENERATION_H */

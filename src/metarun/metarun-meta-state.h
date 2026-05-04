#ifndef INCLUDED_META_STATE_H
#define INCLUDED_META_STATE_H

#include "angband.h"

#define META_STATE_DB_VERSION 0x00010000u

#define META_STATE_NAME_MAX 32
#define META_STATE_MONSTER_NAME_MAX 80
#define META_STATE_CAUSE_MAX 80
#define META_MONSTER_KILL_MEMORY_MAX 3
#define META_ARTIFACT_DESCRIPTION_MAX 512
#define META_DUNGEON_ENTRY_MESSAGE_MAX 256
#define META_DUNGEON_AFFECTED_MONSTER_MAX 8
#define META_DUNGEON_MAX_HGT 20
#define META_DUNGEON_MAX_WID 32
#define META_DUNGEON_MAX_MASK_CELLS 420

#define META_DUNGEON_TILE_ROLE_ROOM 0x01
#define META_DUNGEON_TILE_ROLE_ICKY 0x02
#define META_DUNGEON_TILE_ROLE_GLOW 0x04
#define META_DUNGEON_TILE_ROLE_CHASM 0x08

#define META_DUNGEON_LEGENDARY_AREA_ID_NONE 0
#define META_DUNGEON_LEGENDARY_AREA_ID_PRIMARY 1

typedef enum meta_state_db_kind {
    META_STATE_DB_ARTEFACT = 0,
    META_STATE_DB_MONSTER = 1,
    META_STATE_DB_DUNGEON = 2,
    META_STATE_DB_KIND_MAX = 3
} meta_state_db_kind;

typedef enum meta_state_record_flag {
    META_STATE_RECORD_ACTIVE = 0x00,
    META_STATE_RECORD_DELETED = 0x01,
    META_STATE_RECORD_DISABLED = 0x02
} meta_state_record_flag;

typedef struct meta_state_db_header {
    char magic[4];
    u32b version;
    u32b header_size;
    u32b record_header_size;
    u32b record_count;
    u32b active_count;
    u32b reserved[4];
} meta_state_db_header;

typedef struct meta_state_record_meta {
    u32b metarun_id;
    guid64 record_guid;
    s32b created_turn;
    byte creation_depth;
    byte flags;
    u16b reserved;
} meta_state_record_meta;

typedef bool (*meta_state_record_callback)(const meta_state_record_meta* meta,
                                           const void* payload,
                                           u32b payload_size,
                                           void* user);

typedef struct meta_artifact_record {
    meta_state_record_meta meta;
    guid64 artefact_guid;
    guid64 creator_character_guid;
    char creator_name[META_STATE_NAME_MAX];
    char artefact_name[MAX_LEN_ART_NAME];
    s16b saved_difficulty;
    u16b rarity_weight;
    byte used_masterpiece;
    byte used_aule_forge;
    byte spawn_num;
    byte reserved0;
    artefact_type artefact;
    u32b seen_count;
    u32b spawned_count;
    char description[META_ARTIFACT_DESCRIPTION_MAX];
} meta_artifact_record;

typedef struct meta_monster_kill_memory {
    guid64 character_guid;
    char character_name[META_STATE_NAME_MAX];
    byte depth;
    byte reserved[3];
    s32b turn;
    char cause[META_STATE_CAUSE_MAX];
} meta_monster_kill_memory;

typedef struct meta_monster_death_event {
    guid64 monster_guid;
    u16b r_idx;
    char monster_name[META_STATE_MONSTER_NAME_MAX];
    guid64 character_guid;
    char character_name[META_STATE_NAME_MAX];
    byte depth;
    byte reserved[3];
    s32b turn;
    char cause[META_STATE_CAUSE_MAX];
} meta_monster_death_event;

typedef struct meta_monster_record {
    meta_state_record_meta meta;
    guid64 monster_guid;
    u16b r_idx;
    u16b original_level;
    char monster_name[META_STATE_MONSTER_NAME_MAX];
    byte rank;
    byte kill_memory_count;
    byte global_revenge;
    byte reserved0;
    u16b stat_percent;
    u16b revenge_kill_count;
    meta_monster_kill_memory kills[META_MONSTER_KILL_MEMORY_MAX];
} meta_monster_record;

typedef enum meta_dungeon_source_kind {
    META_DUNGEON_SOURCE_UNKNOWN = 0,
    META_DUNGEON_SOURCE_GREATER_VAULT,
    META_DUNGEON_SOURCE_SPECIAL_ROOM,
    META_DUNGEON_SOURCE_NORMAL_ROOM,
    META_DUNGEON_SOURCE_PARTITION,
    META_DUNGEON_SOURCE_CORRIDOR
} meta_dungeon_source_kind;

typedef struct meta_dungeon_record {
    meta_state_record_meta meta;
    byte song_id;
    byte depth;
    byte source_kind;
    byte partition_kind;
    byte big_cave_type;
    byte hgt;
    byte wid;
    byte style_primary;
    s16b singer_y;
    s16b singer_x;
    u16b mask_cell_count;
    guid64 singer_character_guid;
    char singer_name[META_STATE_NAME_MAX];
    byte affected_monster_count;
    byte reserved0[3];
    guid64 affected_monster_guid[META_DUNGEON_AFFECTED_MONSTER_MAX];
    s16b affected_rel_y[META_DUNGEON_AFFECTED_MONSTER_MAX];
    s16b affected_rel_x[META_DUNGEON_AFFECTED_MONSTER_MAX];
    char entry_message[META_DUNGEON_ENTRY_MESSAGE_MAX];
    u32b tile_blob_size;
} meta_dungeon_record;

typedef struct meta_dungeon_area {
    meta_dungeon_record record;
    byte* tile_blob;
    u32b tile_blob_size;
} meta_dungeon_area;

bool meta_state_init(void);
void meta_state_shutdown(void);
bool meta_state_clear_current_metarun_files(void);

bool meta_state_build_db_path(meta_state_db_kind kind, char* path, size_t len);
u32b meta_state_current_metarun_id(void);
bool meta_state_is_current_metarun_id(u32b metarun_id);
bool meta_state_record_is_active(const meta_state_record_meta* meta);

bool meta_state_append_current_record(meta_state_db_kind kind,
                                      const meta_state_record_meta* meta,
                                      const void* payload,
                                      u32b payload_size);
bool meta_state_load_current_records(meta_state_db_kind kind,
                                     meta_state_record_callback callback,
                                     void* user);

bool meta_artifact_load_for_current_metarun(meta_artifact_record** out_records,
                                            u32b* out_count);
guid64 meta_artifact_current_creator_guid(void);
u16b meta_artifact_rarity_weight(int saved_difficulty,
                                 bool used_masterpiece,
                                 bool used_aule_forge);
bool meta_artifact_build_created_record(meta_artifact_record* out_record,
                                        const artefact_type* art,
                                        const object_type* source_obj,
                                        int saved_difficulty,
                                        bool used_masterpiece,
                                        bool used_aule_forge);
bool meta_artifact_register_created(const meta_artifact_record* record);
bool meta_artifact_prepare_runtime(void);
u32b meta_artifact_runtime_count(void);
bool meta_artifact_runtime_slot_is_meta(int a_idx);
const char* meta_artifact_runtime_description(int a_idx);
void meta_artifact_records_free(meta_artifact_record* records);

bool meta_monster_load_for_current_metarun(meta_monster_record** out_records,
                                           u32b* out_count);
bool meta_monster_record_player_death(const meta_monster_death_event* event);
bool meta_monster_record_current_player_death(cptr cause);
void meta_monster_invalidate_runtime_overrides(void);
bool meta_monster_apply_runtime_overrides(void);
const meta_monster_record* meta_monster_find_record_for_race(u16b r_idx);
bool meta_monster_is_revenge_marked_race(u16b r_idx);
const char* meta_monster_revenge_reason_for_race(u16b r_idx);
int meta_monster_revenge_bonus(void);
int meta_monster_revenge_kills(void);
bool meta_monster_record_revenge_kill(u16b r_idx);
void meta_monster_sync_player_state(void);
void meta_monster_records_free(meta_monster_record* records);

bool meta_dungeon_load_for_current_metarun(meta_dungeon_area** out_areas,
                                           u32b* out_count);
bool meta_dungeon_register_legendary_area(const meta_dungeon_record* record,
                                          const byte* tile_blob,
                                          u32b tile_blob_size);
void meta_dungeon_areas_free(meta_dungeon_area* areas, u32b count);

extern u16b (*legendary_area_id)[MAX_DUNGEON_WID];

bool meta_dungeon_record_is_valid(const meta_dungeon_record* record,
                                  u32b tile_blob_size);
bool meta_dungeon_area_cell_at(const meta_dungeon_area* area, int y, int x,
                               byte* feat, byte* color, byte* role);

bool legendary_area_map_ensure(void);
void legendary_area_map_reset(void);
void legendary_area_level_reset(void);
bool legendary_area_get_save_record(u16b area_id, guid64* record_guid,
                                    bool* entry_seen);
bool legendary_area_restore_after_load(u16b area_id, guid64 record_guid,
                                       bool entry_seen);
void legendary_area_discard_unresolved_loaded_records(void);
void legendary_area_note_spawned(u16b area_id, const meta_dungeon_area* area);
void legendary_area_note_player_position(void);
bool legendary_area_song_is_available(int song);
int legendary_area_song_skill_bonus(int song);
int legendary_area_current_song(void);

void legendary_song_observe_begin(int song_id, int effective_score);
void legendary_song_observe_monster(int m_idx, int effect_kind);
void legendary_song_observe_end(int song_id, int effective_score);

#endif /* INCLUDED_META_STATE_H */

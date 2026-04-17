#ifndef INCLUDED_FS_LOAD_INTERNAL_H
#define INCLUDED_FS_LOAD_INTERNAL_H

/*
 * Lane-local load helpers shared across src/fs/load*.c.
 * This is intentionally not a public subsystem header.
 */

extern bool savefile_has_runtime_overrides;
extern bool savefile_has_monster_shatter;
extern bool savefile_has_song_duels;
extern bool savefile_has_ability_timeline;
extern bool savefile_has_varda_quest;
extern bool savefile_has_artifact_seen;
extern bool savefile_has_skeleton_notes;
extern bool savefile_has_skeleton_hint_mask;
extern bool savefile_has_skeleton_hint_mask32;
extern bool savefile_has_partition_meta;
extern bool savefile_has_partition_meta_types;
extern bool savefile_has_cave_info_hi;
extern bool savefile_has_hint_messages;
extern bool savefile_has_hint_message_meta;
extern bool savefile_has_thrall_quest;
extern bool savefile_has_thrall_quest_requested;
extern bool savefile_has_randart_flags4;
extern bool savefile_has_item_bonuses;
extern bool savefile_has_randart_bonuses;

extern u32b load_byte_offset;
extern bool load_read_failed;
extern u16b objects_count_prefetch;
extern bool color_rle_pair_prefetched;
extern byte color_rle_count_prefetch;
extern byte color_rle_value_prefetch;
extern u16b new_artefacts;
extern u16b art_norm_count;
extern u32b randart_version;

errr load_expect_stream_ok(cptr context);
bool load_savefile_version_at_least(byte major, byte minor, byte patch, byte extra);
void load_note(cptr msg);
void load_rd_byte(byte* ip);
void load_rd_bool(bool* bp);
void load_rd_u16b(u16b* ip);
void load_rd_s16b(s16b* ip);
void load_rd_u32b(u32b* ip);
void load_rd_s32b(s32b* ip);
bool load_rd_string(char* str, int max);
void load_strip_bytes(int n);
errr load_rd_item(object_type* o_ptr);
void load_rd_monster(monster_type* m_ptr);
void artefact_derive_stat_skill_bonuses_from_pval(artefact_type* a_ptr);

errr load_read_extra(void);
errr load_read_randarts(void);
bool load_read_notes(void);
errr load_read_inventory(void);
errr load_read_dungeon(void);

#define note load_note
#define rd_byte load_rd_byte
#define rd_bool load_rd_bool
#define rd_u16b load_rd_u16b
#define rd_s16b load_rd_s16b
#define rd_u32b load_rd_u32b
#define rd_s32b load_rd_s32b
#define rd_string load_rd_string
#define strip_bytes load_strip_bytes
#define rd_item load_rd_item
#define rd_monster load_rd_monster
#define savefile_version_at_least load_savefile_version_at_least
#define rd_extra load_read_extra
#define rd_randarts load_read_randarts
#define rd_notes load_read_notes
#define rd_inventory load_read_inventory
#define rd_dungeon load_read_dungeon

#define LOAD_LOG(fmt, ...)                                                     \
    log_trace("[load:%06u] " fmt, (unsigned)load_byte_offset, __VA_ARGS__)
#define LOAD_LOG0(msg)                                                         \
    log_trace("[load:%06u] %s", (unsigned)load_byte_offset, msg)

#endif /* INCLUDED_FS_LOAD_INTERNAL_H */

#include "angband.h"

#include "fs/file.h"
#include "fs/path.h"
#include "level-generation/level-generation.h"
#include "log/log.h"
#include "metarun.h"
#include "metarun/metarun-meta-state.h"
#include "player/player-abilities.h"
#include "player/player-song-effects.h"
#include "score/score_guid.h"

#include <stdlib.h>
#include <string.h>

#define META_STATE_MAX_PAYLOAD_SIZE (1024u * 1024u)

typedef struct meta_state_record_disk_header {
    u32b header_size;
    u32b payload_size;
    u32b metarun_id;
    guid64 record_guid;
    s32b created_turn;
    byte creation_depth;
    byte flags;
    u16b reserved;
    u32b reserved2[2];
} meta_state_record_disk_header;

typedef struct meta_artifact_loader {
    meta_artifact_record* records;
    u32b count;
    u32b capacity;
} meta_artifact_loader;

typedef struct meta_monster_loader {
    meta_monster_record* records;
    u32b count;
    u32b capacity;
} meta_monster_loader;

typedef struct meta_dungeon_loader {
    meta_dungeon_area* areas;
    u32b count;
    u32b capacity;
} meta_dungeon_loader;

typedef struct meta_artifact_runtime_slot {
    s16b a_idx;
    meta_artifact_record record;
} meta_artifact_runtime_slot;

typedef u16b legendary_area_row[MAX_DUNGEON_WID];

typedef struct legendary_runtime_area {
    bool active;
    u16b area_id;
    bool entry_seen;
    meta_dungeon_area area;
} legendary_runtime_area;

typedef struct legendary_song_observer_state {
    bool active;
    int song_id;
    int effective_score;
    int affected_count;
    s16b m_idx[META_DUNGEON_AFFECTED_MONSTER_MAX];
    s16b y[META_DUNGEON_AFFECTED_MONSTER_MAX];
    s16b x[META_DUNGEON_AFFECTED_MONSTER_MAX];
    guid64 race_guid[META_DUNGEON_AFFECTED_MONSTER_MAX];
} legendary_song_observer_state;

typedef struct legendary_capture_rect {
    int y1;
    int x1;
    int y2;
    int x2;
} legendary_capture_rect;

extern s32b turn;

static meta_artifact_record* g_meta_artifact_cache = NULL;
static u32b g_meta_artifact_cache_count = 0;
static meta_artifact_runtime_slot g_meta_artifact_runtime_slots[64];
static u32b g_meta_artifact_runtime_slot_count = 0;
static meta_monster_record* cached_monster_records = NULL;
static u32b cached_monster_record_count = 0;
static bool cached_monster_records_loaded = false;
static u32b cached_monster_metarun_id = 0;
static u16b cached_revenge_kills = 0;
static u16b cached_revenge_bonus = 0;
static bool cached_monster_validation_logged = false;
static bool cached_monster_runtime_applied = false;
static legendary_runtime_area legendary_runtime;
static legendary_song_observer_state legendary_observer;
static u16b legendary_player_area_id = META_DUNGEON_LEGENDARY_AREA_ID_NONE;

static const char* meta_state_db_leaf(meta_state_db_kind kind)
{
    switch (kind)
    {
    case META_STATE_DB_ARTEFACT: return "artefact.db";
    case META_STATE_DB_MONSTER: return "monster.db";
    case META_STATE_DB_DUNGEON: return "dungeon.db";
    default: return NULL;
    }
}

static const char* meta_state_db_magic(meta_state_db_kind kind)
{
    switch (kind)
    {
    case META_STATE_DB_ARTEFACT: return "MSAF";
    case META_STATE_DB_MONSTER: return "MSMO";
    case META_STATE_DB_DUNGEON: return "MSDG";
    default: return NULL;
    }
}

static bool meta_state_guid_is_zero(guid64 guid)
{
    return guid.hi == 0 && guid.lo == 0;
}

static bool meta_state_guid_equal(guid64 a, guid64 b)
{
    return a.hi == b.hi && a.lo == b.lo;
}

static void meta_state_init_header(meta_state_db_kind kind,
                                   meta_state_db_header* header)
{
    const char* magic = meta_state_db_magic(kind);

    memset(header, 0, sizeof(*header));
    if (magic)
        memcpy(header->magic, magic, sizeof(header->magic));
    header->version = META_STATE_DB_VERSION;
    header->header_size = sizeof(*header);
    header->record_header_size = sizeof(meta_state_record_disk_header);
}

static bool meta_state_header_valid(meta_state_db_kind kind,
                                    const meta_state_db_header* header)
{
    const char* magic = meta_state_db_magic(kind);

    return magic && header &&
        memcmp(header->magic, magic, sizeof(header->magic)) == 0 &&
        header->version == META_STATE_DB_VERSION &&
        header->header_size == sizeof(*header) &&
        header->record_header_size == sizeof(meta_state_record_disk_header);
}

static bool meta_state_write_header(ang_file* file,
                                    const meta_state_db_header* header)
{
    if (!file || !header)
        return false;
    if (ang_file_seek(file, 0, ANG_FILE_SEEK_SET) < 0)
        return false;
    return ang_file_write(file, header, sizeof(*header)) == sizeof(*header);
}

static ang_file* meta_state_open_db(meta_state_db_kind kind, bool create,
                                    meta_state_db_header* out_header)
{
    char path[1024];
    meta_state_db_header header;
    ang_file* file;

    if (!meta_state_build_db_path(kind, path, sizeof(path)))
        return NULL;

    safe_setuid_grab();
    file = ang_file_open_path(path, "r+b");
    safe_setuid_drop();

    if (!file && !create)
        return NULL;

    if (!file)
    {
        meta_state_init_header(kind, &header);
        safe_setuid_grab();
        file = ang_file_open_path(path, "w+b");
        safe_setuid_drop();
        if (!file)
        {
            log_warn("meta_state: failed to create %s", path);
            return NULL;
        }
        if (!meta_state_write_header(file, &header))
        {
            ang_file_close_path(file);
            return NULL;
        }
    }
    else if (ang_file_read(file, &header, sizeof(header)) != sizeof(header) ||
             !meta_state_header_valid(kind, &header))
    {
        if (!create)
        {
            ang_file_close_path(file);
            return NULL;
        }
        log_warn("meta_state: resetting invalid DB header for %s", path);
        ang_file_close_path(file);
        meta_state_init_header(kind, &header);
        safe_setuid_grab();
        file = ang_file_open_path(path, "w+b");
        safe_setuid_drop();
        if (!file || !meta_state_write_header(file, &header))
        {
            if (file)
                ang_file_close_path(file);
            return NULL;
        }
    }

    if (out_header)
        *out_header = header;
    return file;
}

static void meta_state_disk_from_meta(const meta_state_record_meta* meta,
                                      u32b payload_size,
                                      meta_state_record_disk_header* out)
{
    memset(out, 0, sizeof(*out));
    out->header_size = sizeof(*out);
    out->payload_size = payload_size;
    out->metarun_id = meta ? meta->metarun_id : metar.id;
    out->record_guid = meta ? meta->record_guid : score_guid_from_u64(0);
    out->created_turn = meta ? meta->created_turn : turn;
    out->creation_depth = meta ? meta->creation_depth :
        (byte)(p_ptr ? MAX(0, p_ptr->depth) : 0);
    out->flags = meta ? meta->flags : META_STATE_RECORD_ACTIVE;
    out->reserved = meta ? meta->reserved : 0;
}

static void meta_state_meta_from_disk(
    const meta_state_record_disk_header* disk,
    meta_state_record_meta* meta)
{
    meta->metarun_id = disk->metarun_id;
    meta->record_guid = disk->record_guid;
    meta->created_turn = disk->created_turn;
    meta->creation_depth = disk->creation_depth;
    meta->flags = disk->flags;
    meta->reserved = disk->reserved;
}

static bool meta_state_skip_payload(ang_file* file, u32b payload_size)
{
    if (!payload_size)
        return true;
    return ang_file_seek(file, payload_size, ANG_FILE_SEEK_CUR) >= 0;
}

static bool meta_state_grow_array(void** array, u32b* capacity,
                                  u32b count, size_t elem_size)
{
    u32b new_capacity;
    void* grown;

    if (count < *capacity)
        return true;

    new_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
    grown = realloc(*array, (size_t)new_capacity * elem_size);
    if (!grown)
        return false;

    *array = grown;
    *capacity = new_capacity;
    return true;
}

static bool meta_state_append_artifact(meta_artifact_loader* loader,
                                       const meta_state_record_meta* meta,
                                       const meta_artifact_record* record)
{
    meta_artifact_record copy = *record;

    copy.meta = *meta;
    for (u32b i = 0; i < loader->count; i++)
    {
        if (meta_state_guid_equal(loader->records[i].meta.record_guid,
                copy.meta.record_guid))
        {
            loader->records[i] = copy;
            return true;
        }
    }

    if (!meta_state_grow_array((void**)&loader->records, &loader->capacity,
            loader->count, sizeof(*loader->records)))
        return false;
    loader->records[loader->count++] = copy;
    return true;
}

static bool meta_state_append_monster(meta_monster_loader* loader,
                                      const meta_state_record_meta* meta,
                                      const meta_monster_record* record)
{
    meta_monster_record copy = *record;

    copy.meta = *meta;
    for (u32b i = 0; i < loader->count; i++)
    {
        if (meta_state_guid_equal(loader->records[i].meta.record_guid,
                copy.meta.record_guid))
        {
            loader->records[i] = copy;
            return true;
        }
    }

    if (!meta_state_grow_array((void**)&loader->records, &loader->capacity,
            loader->count, sizeof(*loader->records)))
        return false;
    loader->records[loader->count++] = copy;
    return true;
}

static bool meta_state_append_dungeon(meta_dungeon_loader* loader,
                                      const meta_state_record_meta* meta,
                                      const meta_dungeon_record* record,
                                      const byte* tile_blob,
                                      u32b tile_blob_size)
{
    meta_dungeon_area copy;

    memset(&copy, 0, sizeof(copy));
    copy.record = *record;
    copy.record.meta = *meta;
    copy.record.tile_blob_size = tile_blob_size;
    copy.tile_blob_size = tile_blob_size;
    if (tile_blob_size)
    {
        copy.tile_blob = mem_alloc_array(tile_blob_size, byte);
        if (!copy.tile_blob)
            return false;
        memcpy(copy.tile_blob, tile_blob, tile_blob_size);
    }

    for (u32b i = 0; i < loader->count; i++)
    {
        if (meta_state_guid_equal(loader->areas[i].record.meta.record_guid,
                copy.record.meta.record_guid))
        {
            mem_free_null(loader->areas[i].tile_blob);
            loader->areas[i] = copy;
            return true;
        }
    }

    if (!meta_state_grow_array((void**)&loader->areas, &loader->capacity,
            loader->count, sizeof(*loader->areas)))
    {
        mem_free_null(copy.tile_blob);
        return false;
    }
    loader->areas[loader->count++] = copy;
    return true;
}

static bool meta_artifact_load_callback(const meta_state_record_meta* meta,
                                        const void* payload,
                                        u32b payload_size,
                                        void* user)
{
    if (payload_size != sizeof(meta_artifact_record))
    {
        log_warn("meta_state: skipped artefact record with size %u",
            (unsigned)payload_size);
        return true;
    }

    return meta_state_append_artifact(user, meta, payload);
}

static bool meta_monster_load_callback(const meta_state_record_meta* meta,
                                       const void* payload,
                                       u32b payload_size,
                                       void* user)
{
    if (payload_size != sizeof(meta_monster_record))
    {
        log_warn("meta_state: skipped monster record with size %u",
            (unsigned)payload_size);
        return true;
    }

    return meta_state_append_monster(user, meta, payload);
}

static bool meta_dungeon_load_callback(const meta_state_record_meta* meta,
                                       const void* payload,
                                       u32b payload_size,
                                       void* user)
{
    const meta_dungeon_record* record;
    const byte* tile_blob = NULL;
    u32b tile_blob_size = 0;

    if (payload_size < sizeof(meta_dungeon_record))
    {
        log_warn("meta_state: skipped dungeon record with size %u",
            (unsigned)payload_size);
        return true;
    }

    record = payload;
    if (payload_size > sizeof(meta_dungeon_record))
    {
        tile_blob = ((const byte*)payload) + sizeof(meta_dungeon_record);
        tile_blob_size = payload_size - (u32b)sizeof(meta_dungeon_record);
    }

    if (!meta_dungeon_record_is_valid(record, tile_blob_size))
    {
        log_warn("meta_state: skipped invalid dungeon record depth=%u song=%u",
            (unsigned)record->depth, (unsigned)record->song_id);
        return true;
    }

    return meta_state_append_dungeon(user, meta, record, tile_blob,
        tile_blob_size);
}

static void meta_artifact_cache_clear(void)
{
    mem_free_null(g_meta_artifact_cache);
    g_meta_artifact_cache_count = 0;
}

static bool meta_artifact_cache_replace(const meta_artifact_record* records,
                                        u32b count)
{
    meta_artifact_record* copy = NULL;

    meta_artifact_cache_clear();
    if (!records || count == 0)
        return true;

    copy = mem_alloc_array(count, meta_artifact_record);
    if (!copy)
        return false;
    memcpy(copy, records, (size_t)count * sizeof(*copy));
    g_meta_artifact_cache = copy;
    g_meta_artifact_cache_count = count;
    return true;
}

static bool meta_artifact_cache_upsert(const meta_artifact_record* record)
{
    meta_artifact_record* grown;

    if (!record)
        return false;

    for (u32b i = 0; i < g_meta_artifact_cache_count; i++)
    {
        if (meta_state_guid_equal(g_meta_artifact_cache[i].artefact_guid,
                record->artefact_guid))
        {
            g_meta_artifact_cache[i] = *record;
            return true;
        }
    }

    grown = realloc(g_meta_artifact_cache,
        (size_t)(g_meta_artifact_cache_count + 1) * sizeof(*grown));
    if (!grown)
        return false;

    g_meta_artifact_cache = grown;
    g_meta_artifact_cache[g_meta_artifact_cache_count++] = *record;
    return true;
}

static const meta_artifact_record* meta_artifact_cache_find_by_guid(guid64 guid)
{
    if (meta_state_guid_is_zero(guid))
        return NULL;

    for (u32b i = 0; i < g_meta_artifact_cache_count; i++)
    {
        if (meta_state_guid_equal(g_meta_artifact_cache[i].artefact_guid, guid))
            return &g_meta_artifact_cache[i];
    }

    return NULL;
}

static void meta_artifact_runtime_clear(void)
{
    if (a_info && z_info)
    {
        for (u32b i = 0; i < g_meta_artifact_runtime_slot_count; i++)
        {
            s16b a_idx = g_meta_artifact_runtime_slots[i].a_idx;
            if (a_idx > 0 && a_idx < z_info->art_max)
                memset(&a_info[a_idx], 0, sizeof(a_info[a_idx]));
        }
    }

    memset(g_meta_artifact_runtime_slots, 0,
        sizeof(g_meta_artifact_runtime_slots));
    g_meta_artifact_runtime_slot_count = 0;
}

static bool meta_artifact_slot_is_empty(s16b a_idx)
{
    artefact_type* a_ptr;

    if (!a_info || !z_info || a_idx <= 0 || a_idx >= z_info->art_max)
        return false;

    a_ptr = &a_info[a_idx];
    return a_ptr->tval == 0 && a_ptr->sval == 0 && !a_ptr->name[0];
}

static bool meta_artifact_record_is_eligible_for_runtime(
    const meta_artifact_record* record)
{
    guid64 current_guid;

    if (!record)
        return false;
    if (record->saved_difficulty < 15)
        return false;
    if (!meta_state_record_is_active(&record->meta))
        return false;
    if (!record->artefact.tval || !record->artefact.sval)
        return false;

    current_guid = meta_artifact_current_creator_guid();
    if (!meta_state_guid_is_zero(current_guid) &&
        meta_state_guid_equal(record->creator_character_guid, current_guid))
        return false;

    return true;
}

static void meta_artifact_sort_runtime_records(meta_artifact_record* records,
                                               u32b count)
{
    for (u32b i = 1; i < count; i++)
    {
        meta_artifact_record key = records[i];
        s32b j = (s32b)i - 1;

        while (j >= 0)
        {
            bool move = false;
            meta_artifact_record* lhs = &records[j];

            if (lhs->saved_difficulty < key.saved_difficulty)
                move = true;
            else if (lhs->saved_difficulty == key.saved_difficulty &&
                     lhs->meta.created_turn < key.meta.created_turn)
                move = true;
            else if (lhs->saved_difficulty == key.saved_difficulty &&
                     lhs->meta.created_turn == key.meta.created_turn &&
                     lhs->meta.record_guid.lo < key.meta.record_guid.lo)
                move = true;

            if (!move)
                break;

            records[j + 1] = records[j];
            j--;
        }

        records[j + 1] = key;
    }
}

static void meta_monster_cache_free(void)
{
    meta_monster_records_free(cached_monster_records);
    cached_monster_records = NULL;
    cached_monster_record_count = 0;
    cached_monster_records_loaded = false;
    cached_monster_metarun_id = 0;
    cached_revenge_kills = 0;
    cached_revenge_bonus = 0;
    cached_monster_runtime_applied = false;
}

static int meta_monster_revenge_bonus_from_kills(int kills)
{
    int bonus = 0;
    int tier = 1;
    int remaining = kills;

    while (remaining >= tier)
    {
        bonus += tier;
        remaining -= tier;
        tier++;
    }

    return bonus;
}

static void meta_monster_log_validation_once(void)
{
    if (cached_monster_validation_logged)
        return;
    cached_monster_validation_logged = true;
    log_debug("meta_monster: validation rank_cap=3 milestones 1->%d 3->%d 6->%d 10->%d",
        meta_monster_revenge_bonus_from_kills(1),
        meta_monster_revenge_bonus_from_kills(3),
        meta_monster_revenge_bonus_from_kills(6),
        meta_monster_revenge_bonus_from_kills(10));
}

static int meta_monster_find_cached_index_by_guid(guid64 monster_guid)
{
    for (u32b i = 0; i < cached_monster_record_count; i++)
    {
        if (meta_state_guid_equal(cached_monster_records[i].monster_guid,
                monster_guid))
            return (int)i;
    }
    return -1;
}

static int meta_monster_find_cached_index_by_race(u16b r_idx)
{
    for (u32b i = 0; i < cached_monster_record_count; i++)
    {
        if (cached_monster_records[i].r_idx == r_idx)
            return (int)i;
    }
    return -1;
}

static int meta_monster_find_character_mark_index(u16b r_idx)
{
    guid64 monster_guid;
    character_profile* profile;

    if (!p_ptr || !c_info || !z_info || r_idx >= z_info->r_max)
        return -1;
    if (p_ptr->pcharacter >= z_info->c_max)
        return -1;

    profile = &c_info[p_ptr->pcharacter];
    monster_guid = score_guid_from_u64(r_info[r_idx].guid);
    for (int i = 0; i < profile->revenge_mark_count &&
         i < (int)N_ELEMENTS(profile->revenge_monster_guid); i++)
    {
        if (meta_state_guid_equal(profile->revenge_monster_guid[i],
                monster_guid))
            return i;
    }

    return -1;
}

static void meta_monster_copy_base_name(u16b r_idx, char* dst, size_t len)
{
    monster_race* base_ptr;

    if (!dst || !len)
        return;
    dst[0] = '\0';
    if (!z_info || r_idx >= z_info->r_max || !r_info)
        return;

    base_ptr = r_base ? &r_base[r_idx] : &r_info[r_idx];
    if (!base_ptr->name)
        return;

    strnfmt(dst, len, "%s", r_name + base_ptr->name);
}

static int meta_monster_resolve_race_index(guid64 monster_guid,
                                           u16b fallback_r_idx)
{
    if (z_info && fallback_r_idx < z_info->r_max &&
        meta_state_guid_equal(score_guid_from_u64(r_info[fallback_r_idx].guid),
            monster_guid))
        return fallback_r_idx;

    if (!z_info)
        return -1;

    for (int i = 0; i < z_info->r_max; i++)
    {
        if (meta_state_guid_equal(score_guid_from_u64(r_info[i].guid),
                monster_guid))
            return i;
    }

    return -1;
}

static int meta_monster_scale_positive(int value, int percent)
{
    int scaled;

    if (value <= 0)
        return value;

    scaled = (value * percent + 99) / 100;
    if (scaled < 1)
        scaled = 1;

    return scaled;
}

static void meta_monster_scale_ranked_race(monster_race* r_ptr,
                                           const monster_race* base_ptr,
                                           const meta_monster_record* record)
{
    byte cur_num;
    byte max_num;
    int percent;

    if (!r_ptr || !base_ptr || !record)
        return;

    cur_num = r_ptr->cur_num;
    max_num = r_ptr->max_num;
    percent = 100 + (30 * record->rank);

    *r_ptr = *base_ptr;
    r_ptr->cur_num = cur_num;
    r_ptr->max_num = (max_num == 0) ? 0 : 1;
    r_ptr->flags1 |= RF1_UNIQUE;
    r_ptr->level = (byte)MIN(255, base_ptr->level + record->rank);
    r_ptr->hdice = (byte)MIN(255,
        meta_monster_scale_positive(base_ptr->hdice, percent));
    r_ptr->evn = (s16b)MIN(32767,
        meta_monster_scale_positive(base_ptr->evn, percent));
    r_ptr->pd = (byte)MIN(255,
        meta_monster_scale_positive(base_ptr->pd, percent));
    r_ptr->ps = (byte)MIN(255,
        meta_monster_scale_positive(base_ptr->ps, percent));
    r_ptr->per = (s16b)MIN(32767,
        meta_monster_scale_positive(base_ptr->per, percent));
    r_ptr->wil = (s16b)MIN(32767,
        meta_monster_scale_positive(base_ptr->wil, percent));
    r_ptr->spell_power = (byte)MIN(255,
        meta_monster_scale_positive(base_ptr->spell_power, percent));
    r_ptr->mon_power = (u32b)meta_monster_scale_positive(
        (int)base_ptr->mon_power, percent);

    for (int i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        if (!base_ptr->blow[i].method)
            continue;
        r_ptr->blow[i].att = (s16b)MIN(32767,
            meta_monster_scale_positive(base_ptr->blow[i].att, percent));
        r_ptr->blow[i].dd = (byte)MIN(255,
            meta_monster_scale_positive(base_ptr->blow[i].dd, percent));
    }
}

static bool meta_monster_refresh_cache(void)
{
    meta_monster_record* records = NULL;
    u32b count = 0;

    if (cached_monster_records_loaded && cached_monster_metarun_id == metar.id)
        return true;

    meta_monster_cache_free();
    if (!meta_monster_load_for_current_metarun(&records, &count))
        return false;

    cached_monster_records = records;
    cached_monster_record_count = count;
    cached_monster_records_loaded = true;
    cached_monster_metarun_id = metar.id;
    cached_monster_runtime_applied = false;
    return true;
}

static void legendary_runtime_area_free(void)
{
    mem_free_null(legendary_runtime.area.tile_blob);
    memset(&legendary_runtime, 0, sizeof(legendary_runtime));
    legendary_player_area_id = META_DUNGEON_LEGENDARY_AREA_ID_NONE;
}

static bool legendary_runtime_area_copy(u16b area_id,
                                        const meta_dungeon_area* area,
                                        bool entry_seen)
{
    meta_dungeon_area copy;

    if (!area || !area->tile_blob ||
        !meta_dungeon_record_is_valid(&area->record, area->tile_blob_size))
        return false;

    memset(&copy, 0, sizeof(copy));
    copy.record = area->record;
    copy.tile_blob_size = area->tile_blob_size;
    copy.record.tile_blob_size = area->tile_blob_size;
    copy.tile_blob = mem_alloc_array(copy.tile_blob_size, byte);
    if (!copy.tile_blob)
        return false;
    memcpy(copy.tile_blob, area->tile_blob, copy.tile_blob_size);

    legendary_runtime_area_free();
    legendary_runtime.active = true;
    legendary_runtime.area_id = area_id;
    legendary_runtime.entry_seen = entry_seen;
    legendary_runtime.area = copy;
    return true;
}

static u32b meta_dungeon_mask_bytes(const meta_dungeon_record* record)
{
    return (u32b)(((int)record->hgt * (int)record->wid + 7) / 8);
}

static u32b meta_dungeon_expected_tile_blob_size(
    const meta_dungeon_record* record)
{
    if (!record)
        return 0;
    return meta_dungeon_mask_bytes(record) +
        ((u32b)record->mask_cell_count * 3u);
}

bool meta_state_init(void)
{
    meta_monster_log_validation_once();
    return true;
}

void meta_state_shutdown(void)
{
    meta_artifact_runtime_clear();
    meta_artifact_cache_clear();
    meta_monster_cache_free();
    legendary_runtime_area_free();
    mem_free_null(legendary_area_id);
}

bool meta_state_build_db_path(meta_state_db_kind kind, char* path, size_t len)
{
    const char* leaf = meta_state_db_leaf(kind);

    if (!leaf || !path || !len)
        return false;

    return path_build(path, len, ANGBAND_DIR_APEX, leaf);
}

u32b meta_state_current_metarun_id(void)
{
    return metar.id;
}

bool meta_state_is_current_metarun_id(u32b metarun_id)
{
    return metarun_id == metar.id;
}

bool meta_state_record_is_active(const meta_state_record_meta* meta)
{
    if (!meta)
        return false;
    return (meta->flags &
        (META_STATE_RECORD_DELETED | META_STATE_RECORD_DISABLED)) == 0;
}

bool meta_state_clear_current_metarun_files(void)
{
    bool ok = true;

    for (int i = 0; i < META_STATE_DB_KIND_MAX; i++)
    {
        char path[1024];
        ang_file* probe;

        if (!meta_state_build_db_path((meta_state_db_kind)i, path, sizeof(path)))
        {
            ok = false;
            continue;
        }

        probe = ang_file_open_path(path, "rb");
        if (!probe)
            continue;
        ang_file_close_path(probe);

        safe_setuid_grab();
        if (!fd_kill(path))
        {
            log_warn("meta_state: failed to remove %s", path);
            ok = false;
        }
        safe_setuid_drop();
    }

    meta_artifact_runtime_clear();
    meta_artifact_cache_clear();
    meta_monster_cache_free();
    legendary_runtime_area_free();
    return ok;
}

bool meta_state_append_current_record(meta_state_db_kind kind,
                                      const meta_state_record_meta* meta,
                                      const void* payload,
                                      u32b payload_size)
{
    meta_state_db_header db_header;
    meta_state_record_disk_header rec_header;
    ang_file* file;
    bool ok = false;

    if (!payload || !payload_size || payload_size > META_STATE_MAX_PAYLOAD_SIZE)
        return false;

    file = meta_state_open_db(kind, true, &db_header);
    if (!file)
        return false;

    meta_state_disk_from_meta(meta, payload_size, &rec_header);
    if (ang_file_seek(file, 0, ANG_FILE_SEEK_END) >= 0 &&
        ang_file_write(file, &rec_header, sizeof(rec_header)) ==
            sizeof(rec_header) &&
        ang_file_write(file, payload, payload_size) == payload_size)
    {
        db_header.record_count++;
        if ((rec_header.flags &
             (META_STATE_RECORD_DELETED | META_STATE_RECORD_DISABLED)) == 0)
            db_header.active_count++;
        ok = meta_state_write_header(file, &db_header);
        SDL_FlushIO(file);
    }

    ang_file_close_path(file);
    return ok;
}

bool meta_state_load_current_records(meta_state_db_kind kind,
                                     meta_state_record_callback callback,
                                     void* user)
{
    meta_state_db_header db_header;
    ang_file* file;
    bool ok = true;

    if (!callback)
        return false;

    file = meta_state_open_db(kind, false, &db_header);
    if (!file)
        return true;

    if (ang_file_seek(file, sizeof(db_header), ANG_FILE_SEEK_SET) < 0)
    {
        ang_file_close_path(file);
        return false;
    }

    for (u32b i = 0; i < db_header.record_count; i++)
    {
        meta_state_record_disk_header rec_header;
        meta_state_record_meta meta;
        byte* payload = NULL;

        if (ang_file_read(file, &rec_header, sizeof(rec_header)) !=
            sizeof(rec_header))
        {
            log_warn("meta_state: truncated DB record in kind %d", kind);
            ok = false;
            break;
        }

        if (rec_header.header_size != sizeof(rec_header) ||
            rec_header.payload_size > META_STATE_MAX_PAYLOAD_SIZE)
        {
            log_warn("meta_state: invalid DB record in kind %d", kind);
            ok = false;
            break;
        }

        meta_state_meta_from_disk(&rec_header, &meta);
        if (!meta_state_is_current_metarun_id(meta.metarun_id) ||
            !meta_state_record_is_active(&meta))
        {
            if (!meta_state_skip_payload(file, rec_header.payload_size))
                ok = false;
            if (!ok)
                break;
            continue;
        }

        payload = mem_alloc_array(rec_header.payload_size, byte);
        if (!payload)
        {
            ok = false;
            break;
        }

        if (ang_file_read(file, payload, rec_header.payload_size) !=
            rec_header.payload_size)
        {
            mem_free_null(payload);
            ok = false;
            break;
        }

        if (!callback(&meta, payload, rec_header.payload_size, user))
            ok = false;

        mem_free_null(payload);
        if (!ok)
            break;
    }

    ang_file_close_path(file);
    return ok;
}

bool meta_artifact_load_for_current_metarun(meta_artifact_record** out_records,
                                            u32b* out_count)
{
    meta_artifact_loader loader;
    bool ok;

    if (!out_records || !out_count)
        return false;

    *out_records = NULL;
    *out_count = 0;
    memset(&loader, 0, sizeof(loader));

    ok = meta_state_load_current_records(META_STATE_DB_ARTEFACT,
        meta_artifact_load_callback, &loader);
    if (!ok)
    {
        meta_artifact_records_free(loader.records);
        return false;
    }

    *out_records = loader.records;
    *out_count = loader.count;
    return true;
}

guid64 meta_artifact_current_creator_guid(void)
{
    char seed_text[128];
    const char* name = "unknown-forger";
    u32b salt = 0x41525442u ^ metar.id;
    byte character_idx = p_ptr ? p_ptr->pcharacter : 0;

    if (op_ptr && op_ptr->full_name[0])
        name = op_ptr->full_name;

    salt ^= ((u32b)character_idx << 8);
    salt ^= ((u32b)metar.deaths << 16);
    strnfmt(seed_text, sizeof(seed_text), "%s:%u:%u", name,
        (unsigned)character_idx, (unsigned)metar.deaths);
    return score_guid_from_string(seed_text, salt ? salt : 1u);
}

u16b meta_artifact_rarity_weight(int saved_difficulty,
                                 bool used_masterpiece,
                                 bool used_aule_forge)
{
    int d = MAX(saved_difficulty, 15);
    int weight = 32 - ((d - 15) * 2) / 5;

    weight = MIN(MAX(weight, 8), 30);
    if (used_masterpiece || used_aule_forge)
        weight = MIN(weight + MAX(weight / 3, 4), 40);
    return (u16b)weight;
}

bool meta_artifact_build_created_record(meta_artifact_record* out_record,
                                        const artefact_type* art,
                                        const object_type* source_obj,
                                        int saved_difficulty,
                                        bool used_masterpiece,
                                        bool used_aule_forge)
{
    (void)source_obj;

    if (!out_record || !art || meta_state_guid_is_zero(art->guid))
        return false;

    memset(out_record, 0, sizeof(*out_record));
    out_record->meta.metarun_id = metar.id;
    out_record->meta.record_guid = art->guid;
    out_record->meta.created_turn = turn;
    out_record->meta.creation_depth = (byte)(p_ptr ? MAX(0, p_ptr->depth) : 0);
    out_record->artefact_guid = art->guid;
    out_record->creator_character_guid = meta_artifact_current_creator_guid();
    strnfmt(out_record->creator_name, sizeof(out_record->creator_name), "%s",
        (op_ptr && op_ptr->full_name[0]) ? op_ptr->full_name : "unknown");
    strnfmt(out_record->artefact_name, sizeof(out_record->artefact_name), "%s",
        art->name);
    out_record->saved_difficulty = (s16b)saved_difficulty;
    out_record->rarity_weight = meta_artifact_rarity_weight(saved_difficulty,
        used_masterpiece, used_aule_forge);
    out_record->used_masterpiece = used_masterpiece ? 1 : 0;
    out_record->used_aule_forge = used_aule_forge ? 1 : 0;
    out_record->spawn_num = art->spawn_num ? art->spawn_num : 1;
    out_record->artefact = *art;
    strnfmt(out_record->description, sizeof(out_record->description),
        "%s was remembered from an earlier life of this story.",
        out_record->artefact_name[0] ? out_record->artefact_name : "This artefact");
    return true;
}

bool meta_artifact_register_created(const meta_artifact_record* record)
{
    meta_artifact_record copy;
    bool ok;

    if (!record)
        return false;

    copy = *record;
    if (!copy.meta.metarun_id)
        copy.meta.metarun_id = metar.id;
    if (!copy.meta.created_turn)
        copy.meta.created_turn = turn;
    if (!copy.meta.creation_depth && p_ptr)
        copy.meta.creation_depth = (byte)MAX(0, p_ptr->depth);
    if (meta_state_guid_is_zero(copy.meta.record_guid))
        copy.meta.record_guid = copy.artefact_guid;
    if (meta_state_guid_is_zero(copy.artefact_guid))
        copy.artefact_guid = copy.meta.record_guid;

    ok = meta_state_append_current_record(META_STATE_DB_ARTEFACT,
        &copy.meta, &copy, sizeof(copy));
    if (ok)
        (void)meta_artifact_cache_upsert(&copy);
    return ok;
}

bool meta_artifact_prepare_runtime(void)
{
    meta_artifact_record* records = NULL;
    meta_artifact_record* eligible = NULL;
    u32b record_count = 0;
    u32b eligible_count = 0;
    u32b eligible_idx = 0;
    bool ok = false;

    if (!a_info || !z_info)
        return false;

    meta_artifact_runtime_clear();

    if (!meta_artifact_load_for_current_metarun(&records, &record_count))
        return false;

    if (!meta_artifact_cache_replace(records, record_count))
        goto cleanup;

    if (record_count == 0)
    {
        ok = true;
        goto cleanup;
    }

    eligible = mem_alloc_array(record_count, meta_artifact_record);
    if (!eligible)
        goto cleanup;

    for (u32b i = 0; i < record_count; i++)
    {
        if (!meta_artifact_record_is_eligible_for_runtime(&records[i]))
            continue;
        eligible[eligible_count++] = records[i];
    }

    meta_artifact_sort_runtime_records(eligible, eligible_count);

    for (s16b a_idx = (s16b)z_info->art_self_made_max - 3;
         a_idx >= (s16b)z_info->art_rand_max && eligible_idx < eligible_count;
         a_idx--)
    {
        artefact_type* slot;
        const meta_artifact_record* record;
        int k_idx;

        if (!meta_artifact_slot_is_empty(a_idx))
            continue;
        if (g_meta_artifact_runtime_slot_count >=
            N_ELEMENTS(g_meta_artifact_runtime_slots))
            break;

        record = &eligible[eligible_idx];
        k_idx = lookup_kind(record->artefact.tval, record->artefact.sval);
        if (k_idx <= 0)
        {
            eligible_idx++;
            continue;
        }

        slot = &a_info[a_idx];
        *slot = record->artefact;
        slot->guid = record->artefact_guid;
        if (record->artefact_name[0])
            strnfmt(slot->name, sizeof(slot->name), "%s",
                record->artefact_name);
        slot->level = record->meta.creation_depth ?
            record->meta.creation_depth : 1;
        slot->rarity = (byte)MIN(record->rarity_weight, 255);
        slot->cur_num = 0;
        slot->found_num = 0;
        slot->seen = 0;
        if (!slot->spawn_num)
            slot->spawn_num = record->spawn_num ? record->spawn_num : 1;

        g_meta_artifact_runtime_slots[g_meta_artifact_runtime_slot_count].a_idx =
            a_idx;
        g_meta_artifact_runtime_slots[g_meta_artifact_runtime_slot_count].record =
            *record;
        g_meta_artifact_runtime_slot_count++;
        eligible_idx++;
    }

    if (eligible_count > g_meta_artifact_runtime_slot_count)
    {
        log_warn("meta_state: injected %u of %u remembered artefacts into runtime slots",
            (unsigned)g_meta_artifact_runtime_slot_count,
            (unsigned)eligible_count);
    }
    else if (g_meta_artifact_runtime_slot_count > 0)
    {
        log_info("meta_state: injected %u remembered artefacts into runtime slots",
            (unsigned)g_meta_artifact_runtime_slot_count);
    }

    ok = true;

cleanup:
    mem_free_null(eligible);
    meta_artifact_records_free(records);
    return ok;
}

u32b meta_artifact_runtime_count(void)
{
    return g_meta_artifact_runtime_slot_count;
}

bool meta_artifact_runtime_slot_is_meta(int a_idx)
{
    for (u32b i = 0; i < g_meta_artifact_runtime_slot_count; i++)
    {
        if (g_meta_artifact_runtime_slots[i].a_idx == a_idx)
            return true;
    }

    return false;
}

const char* meta_artifact_runtime_description(int a_idx)
{
    const meta_artifact_record* cached;

    for (u32b i = 0; i < g_meta_artifact_runtime_slot_count; i++)
    {
        if (g_meta_artifact_runtime_slots[i].a_idx == a_idx)
            return g_meta_artifact_runtime_slots[i].record.description;
    }

    if (!a_info || !z_info || a_idx <= 0 || a_idx >= z_info->art_max)
        return NULL;

    cached = meta_artifact_cache_find_by_guid(a_info[a_idx].guid);
    return (cached && cached->description[0]) ? cached->description : NULL;
}

void meta_artifact_records_free(meta_artifact_record* records)
{
    mem_free_null(records);
}

bool meta_monster_load_for_current_metarun(meta_monster_record** out_records,
                                           u32b* out_count)
{
    meta_monster_loader loader;
    bool ok;

    if (!out_records || !out_count)
        return false;

    *out_records = NULL;
    *out_count = 0;
    memset(&loader, 0, sizeof(loader));

    ok = meta_state_load_current_records(META_STATE_DB_MONSTER,
        meta_monster_load_callback, &loader);
    if (!ok)
    {
        meta_monster_records_free(loader.records);
        return false;
    }

    *out_records = loader.records;
    *out_count = loader.count;
    return true;
}

bool meta_monster_record_player_death(const meta_monster_death_event* event)
{
    meta_monster_record record;
    int existing_idx;
    int kill_slot;

    if (!event || meta_state_guid_is_zero(event->monster_guid))
        return false;
    if (!meta_monster_refresh_cache())
        return false;

    existing_idx = meta_monster_find_cached_index_by_guid(event->monster_guid);
    if (existing_idx >= 0)
        record = cached_monster_records[existing_idx];
    else
        memset(&record, 0, sizeof(record));

    record.meta.metarun_id = metar.id;
    record.meta.record_guid = event->monster_guid;
    record.meta.created_turn = event->turn ? event->turn : turn;
    record.meta.creation_depth = event->depth;
    record.monster_guid = event->monster_guid;
    record.r_idx = event->r_idx;
    if (!record.original_level && z_info && event->r_idx < z_info->r_max)
        record.original_level =
            r_base ? r_base[event->r_idx].level : r_info[event->r_idx].level;
    if (event->monster_name[0])
        strnfmt(record.monster_name, sizeof(record.monster_name), "%s",
            event->monster_name);
    else
        meta_monster_copy_base_name(event->r_idx, record.monster_name,
            sizeof(record.monster_name));
    record.rank = (byte)MIN(3, MAX(1, record.rank + 1));
    record.stat_percent = 100 + (30 * record.rank);
    record.global_revenge = true;

    for (kill_slot = META_MONSTER_KILL_MEMORY_MAX - 1; kill_slot > 0;
         kill_slot--)
        record.kills[kill_slot] = record.kills[kill_slot - 1];
    record.kills[0].character_guid = event->character_guid;
    strnfmt(record.kills[0].character_name,
        sizeof(record.kills[0].character_name), "%s", event->character_name);
    record.kills[0].depth = event->depth;
    record.kills[0].turn = event->turn ? event->turn : turn;
    strnfmt(record.kills[0].cause, sizeof(record.kills[0].cause), "%s",
        event->cause);
    if (record.kill_memory_count < META_MONSTER_KILL_MEMORY_MAX)
        record.kill_memory_count++;

    if (!meta_state_append_current_record(META_STATE_DB_MONSTER,
        &record.meta, &record, sizeof(record)))
        return false;

    if (existing_idx >= 0)
        cached_monster_records[existing_idx] = record;
    else
    {
        meta_monster_cache_free();
        if (!meta_monster_refresh_cache())
            return true;
    }

    cached_monster_runtime_applied = false;
    meta_monster_sync_player_state();

    log_info("meta_monster: death record updated for %s rank=%u memories=%u",
        record.monster_name, (unsigned)record.rank,
        (unsigned)record.kill_memory_count);

    return true;
}

void meta_monster_invalidate_runtime_overrides(void)
{
    cached_monster_runtime_applied = false;
}

bool meta_monster_apply_runtime_overrides(void)
{
    if (!r_base || !z_info)
        return false;
    if (!meta_monster_refresh_cache())
        return false;

    if (cached_monster_runtime_applied)
        return true;

    for (u32b i = 0; i < z_info->r_max; i++)
    {
        byte cur_num = r_info[i].cur_num;
        byte max_num = r_info[i].max_num;

        r_info[i] = r_base[i];
        r_info[i].cur_num = cur_num;
        r_info[i].max_num = max_num;
    }

    for (u32b i = 0; i < cached_monster_record_count; i++)
    {
        meta_monster_record* record = &cached_monster_records[i];
        int resolved_r_idx;

        if (record->rank < 1)
            continue;

        resolved_r_idx = meta_monster_resolve_race_index(record->monster_guid,
            record->r_idx);
        if (resolved_r_idx < 0)
            continue;

        record->r_idx = (u16b)resolved_r_idx;
        meta_monster_scale_ranked_race(&r_info[resolved_r_idx],
            &r_base[resolved_r_idx], record);
    }

    cached_monster_runtime_applied = true;
    return true;
}

const meta_monster_record* meta_monster_find_record_for_race(u16b r_idx)
{
    int idx;

    if (!meta_monster_refresh_cache())
        return NULL;

    idx = meta_monster_find_cached_index_by_race(r_idx);
    return (idx >= 0) ? &cached_monster_records[idx] : NULL;
}

bool meta_monster_is_revenge_marked_race(u16b r_idx)
{
    const meta_monster_record* record = meta_monster_find_record_for_race(r_idx);

    return meta_monster_find_character_mark_index(r_idx) >= 0 ||
        (record && record->global_revenge);
}

const char* meta_monster_revenge_reason_for_race(u16b r_idx)
{
    int idx = meta_monster_find_character_mark_index(r_idx);

    if (idx < 0 || !p_ptr)
        return NULL;
    return c_text + c_info[p_ptr->pcharacter].revenge_reason[idx];
}

int meta_monster_revenge_bonus(void)
{
    meta_monster_sync_player_state();
    return cached_revenge_bonus;
}

int meta_monster_revenge_kills(void)
{
    meta_monster_sync_player_state();
    return cached_revenge_kills;
}

bool meta_monster_record_revenge_kill(u16b r_idx)
{
    meta_monster_record record;
    const meta_monster_record* existing;
    guid64 monster_guid;
    int old_bonus;
    int old_kills;

    if (!z_info || r_idx >= z_info->r_max)
        return false;
    if (!meta_monster_is_revenge_marked_race(r_idx))
        return false;
    if (!meta_monster_refresh_cache())
        return false;

    old_bonus = meta_monster_revenge_bonus();
    old_kills = meta_monster_revenge_kills();
    existing = meta_monster_find_record_for_race(r_idx);
    if (existing)
        record = *existing;
    else
        memset(&record, 0, sizeof(record));

    monster_guid = score_guid_from_u64(r_info[r_idx].guid);
    record.meta.metarun_id = metar.id;
    record.meta.record_guid = monster_guid;
    record.meta.created_turn = turn;
    record.meta.creation_depth = (byte)(p_ptr ? MAX(0, p_ptr->depth) : 0);
    record.monster_guid = monster_guid;
    record.r_idx = r_idx;
    if (!record.original_level)
        record.original_level = r_base ? r_base[r_idx].level : r_info[r_idx].level;
    if (!record.monster_name[0])
        meta_monster_copy_base_name(r_idx, record.monster_name,
            sizeof(record.monster_name));
    record.revenge_kill_count++;

    if (!meta_state_append_current_record(META_STATE_DB_MONSTER,
        &record.meta, &record, sizeof(record)))
        return false;

    meta_monster_cache_free();
    meta_monster_sync_player_state();
    if (p_ptr && old_bonus != p_ptr->revenge_bonus)
    {
        log_info("meta_monster: revenge bonus advanced kills=%u bonus=%u",
            (unsigned)p_ptr->revenge_kills, (unsigned)p_ptr->revenge_bonus);
        meta_monster_invalidate_runtime_overrides();
    }
    else if (p_ptr && old_kills != p_ptr->revenge_kills)
    {
        log_debug("meta_monster: revenge kill recorded kills=%u bonus=%u",
            (unsigned)p_ptr->revenge_kills, (unsigned)p_ptr->revenge_bonus);
    }

    return true;
}

void meta_monster_sync_player_state(void)
{
    int total_kills = 0;
    int total_bonus;

    if (!meta_monster_refresh_cache())
        return;

    for (u32b i = 0; i < cached_monster_record_count; i++)
        total_kills += cached_monster_records[i].revenge_kill_count;

    total_bonus = meta_monster_revenge_bonus_from_kills(total_kills);
    cached_revenge_kills = (u16b)MIN(65535, total_kills);
    cached_revenge_bonus = (u16b)MIN(65535, total_bonus);

    if (p_ptr)
    {
        p_ptr->revenge_kills = cached_revenge_kills;
        p_ptr->revenge_bonus = cached_revenge_bonus;
    }
}

void meta_monster_records_free(meta_monster_record* records)
{
    mem_free_null(records);
}

bool meta_dungeon_load_for_current_metarun(meta_dungeon_area** out_areas,
                                           u32b* out_count)
{
    meta_dungeon_loader loader;
    bool ok;

    if (!out_areas || !out_count)
        return false;

    *out_areas = NULL;
    *out_count = 0;
    memset(&loader, 0, sizeof(loader));

    ok = meta_state_load_current_records(META_STATE_DB_DUNGEON,
        meta_dungeon_load_callback, &loader);
    if (!ok)
    {
        meta_dungeon_areas_free(loader.areas, loader.count);
        return false;
    }

    *out_areas = loader.areas;
    *out_count = loader.count;
    return true;
}

bool meta_dungeon_register_legendary_area(const meta_dungeon_record* record,
                                          const byte* tile_blob,
                                          u32b tile_blob_size)
{
    meta_dungeon_record copy;
    byte* payload;
    u32b payload_size;
    bool ok;

    if (!record || (tile_blob_size && !tile_blob) ||
        tile_blob_size > META_STATE_MAX_PAYLOAD_SIZE - sizeof(copy) ||
        !meta_dungeon_record_is_valid(record, tile_blob_size))
        return false;

    copy = *record;
    if (!copy.meta.metarun_id)
        copy.meta.metarun_id = metar.id;
    if (!copy.meta.created_turn)
        copy.meta.created_turn = turn;
    if (!copy.meta.creation_depth)
        copy.meta.creation_depth = copy.depth;
    copy.tile_blob_size = tile_blob_size;

    payload_size = (u32b)sizeof(copy) + tile_blob_size;
    payload = mem_alloc_array(payload_size, byte);
    if (!payload)
        return false;

    memcpy(payload, &copy, sizeof(copy));
    if (tile_blob_size)
        memcpy(payload + sizeof(copy), tile_blob, tile_blob_size);

    ok = meta_state_append_current_record(META_STATE_DB_DUNGEON,
        &copy.meta, payload, payload_size);
    mem_free_null(payload);
    return ok;
}

void meta_dungeon_areas_free(meta_dungeon_area* areas, u32b count)
{
    if (!areas)
        return;
    for (u32b i = 0; i < count; i++)
        mem_free_null(areas[i].tile_blob);
    mem_free_null(areas);
}

bool meta_dungeon_record_is_valid(const meta_dungeon_record* record,
                                  u32b tile_blob_size)
{
    u32b expected_size;
    int cells;

    if (!record)
        return false;
    if (record->song_id <= SNG_NOTHING || record->song_id >= SNG_MAX)
        return false;
    if (record->depth <= 0 || record->depth >= MORGOTH_DEPTH)
        return false;
    if (record->hgt < 1 || record->hgt > META_DUNGEON_MAX_HGT)
        return false;
    if (record->wid < 1 || record->wid > META_DUNGEON_MAX_WID)
        return false;

    cells = (int)record->hgt * (int)record->wid;
    if (record->mask_cell_count == 0 ||
        record->mask_cell_count > META_DUNGEON_MAX_MASK_CELLS ||
        record->mask_cell_count > cells)
        return false;

    expected_size = meta_dungeon_expected_tile_blob_size(record);
    return expected_size != 0 && expected_size <= META_STATE_MAX_PAYLOAD_SIZE &&
        tile_blob_size == expected_size &&
        (record->tile_blob_size == 0 || record->tile_blob_size == tile_blob_size);
}

static bool meta_dungeon_blob_mask_has(const meta_dungeon_record* record,
                                       const byte* tile_blob,
                                       int y, int x)
{
    int cell;

    if (!record || !tile_blob)
        return false;
    if (y < 0 || y >= record->hgt || x < 0 || x >= record->wid)
        return false;

    cell = y * record->wid + x;
    return (tile_blob[cell / 8] & (1u << (cell % 8))) != 0;
}

bool meta_dungeon_area_cell_at(const meta_dungeon_area* area, int y, int x,
                               byte* feat, byte* color, byte* role)
{
    const meta_dungeon_record* record;
    const byte* tile_blob;
    const byte* feats;
    const byte* colors;
    const byte* roles;
    u32b mask_bytes;
    u32b ordinal = 0;
    int target;

    if (!area || !area->tile_blob)
        return false;
    record = &area->record;
    tile_blob = area->tile_blob;
    if (!meta_dungeon_record_is_valid(record, area->tile_blob_size) ||
        !meta_dungeon_blob_mask_has(record, tile_blob, y, x))
        return false;

    target = y * record->wid + x;
    for (int cell = 0; cell < target; cell++)
    {
        if (tile_blob[cell / 8] & (1u << (cell % 8)))
            ordinal++;
    }
    if (ordinal >= record->mask_cell_count)
        return false;

    mask_bytes = meta_dungeon_mask_bytes(record);
    feats = tile_blob + mask_bytes;
    colors = feats + record->mask_cell_count;
    roles = colors + record->mask_cell_count;

    if (feat) *feat = feats[ordinal];
    if (color) *color = colors[ordinal];
    if (role) *role = roles[ordinal];
    return true;
}

bool legendary_area_map_ensure(void)
{
    if (legendary_area_id)
        return true;

    legendary_area_id = mem_alloc_array(MAX_DUNGEON_HGT, legendary_area_row);
    if (!legendary_area_id)
        return false;

    memset(legendary_area_id, 0,
        (size_t)MAX_DUNGEON_HGT * sizeof(*legendary_area_id));
    return true;
}

void legendary_area_map_reset(void)
{
    if (legendary_area_map_ensure())
        memset(legendary_area_id, 0,
            (size_t)MAX_DUNGEON_HGT * sizeof(*legendary_area_id));
    legendary_runtime_area_free();
}

void legendary_area_level_reset(void)
{
    legendary_player_area_id = META_DUNGEON_LEGENDARY_AREA_ID_NONE;
}

bool legendary_area_get_save_record(u16b area_id, guid64* record_guid,
                                    bool* entry_seen)
{
    if (!legendary_runtime.active || legendary_runtime.area_id != area_id)
        return false;
    if (record_guid)
        *record_guid = legendary_runtime.area.record.meta.record_guid;
    if (entry_seen)
        *entry_seen = legendary_runtime.entry_seen;
    return true;
}

bool legendary_area_restore_after_load(u16b area_id, guid64 record_guid,
                                       bool entry_seen)
{
    meta_dungeon_area* areas = NULL;
    u32b count = 0;
    bool ok = false;

    if (area_id == META_DUNGEON_LEGENDARY_AREA_ID_NONE ||
        meta_state_guid_is_zero(record_guid) ||
        !meta_dungeon_load_for_current_metarun(&areas, &count))
        return false;

    for (u32b i = 0; i < count; i++)
    {
        if (meta_state_guid_equal(areas[i].record.meta.record_guid, record_guid))
        {
            ok = legendary_runtime_area_copy(area_id, &areas[i], entry_seen);
            break;
        }
    }

    meta_dungeon_areas_free(areas, count);
    return ok;
}

void legendary_area_discard_unresolved_loaded_records(void)
{
    if (legendary_runtime.active || !legendary_area_id)
        return;

    for (int y = 0; y < MAX_DUNGEON_HGT; y++)
    {
        for (int x = 0; x < MAX_DUNGEON_WID; x++)
            legendary_area_id[y][x] = META_DUNGEON_LEGENDARY_AREA_ID_NONE;
    }
}

void legendary_area_note_spawned(u16b area_id, const meta_dungeon_area* area)
{
    if (!legendary_runtime_area_copy(area_id, area, false))
        log_warn("legendary area: failed to copy spawned area runtime state");
}

int legendary_area_current_song(void)
{
    u16b area_id;

    if (!p_ptr || !legendary_area_id || !legendary_runtime.active)
        return SNG_NOTHING;
    if (!in_bounds(p_ptr->py, p_ptr->px))
        return SNG_NOTHING;

    area_id = legendary_area_id[p_ptr->py][p_ptr->px];
    if (area_id == META_DUNGEON_LEGENDARY_AREA_ID_NONE ||
        area_id != legendary_runtime.area_id)
        return SNG_NOTHING;

    return legendary_runtime.area.record.song_id;
}

bool legendary_area_song_is_available(int song)
{
    return song > SNG_NOTHING && song < SNG_MAX &&
        legendary_area_current_song() == song;
}

int legendary_area_song_skill_bonus(int song)
{
    if (!legendary_area_song_is_available(song))
        return 0;
    if (!p_ptr->active_ability[S_SNG][song])
        return 0;
    return 5;
}

static void legendary_area_stop_area_only_song(int song)
{
    if (!p_ptr || song <= SNG_NOTHING || song >= SNG_MAX)
        return;
    if (p_ptr->active_ability[S_SNG][song])
        return;

    if (p_ptr->song1 == song)
    {
        change_song(SNG_NOTHING);
        disturb(0, 0);
    }
    else if (p_ptr->song2 == song)
    {
        p_ptr->song2 = SNG_NOTHING;
        p_ptr->redraw |= PR_VOICE;
        p_ptr->update |= PU_BONUS;
        disturb(0, 0);
    }
}

void legendary_area_note_player_position(void)
{
    u16b area_id = META_DUNGEON_LEGENDARY_AREA_ID_NONE;
    int previous_song = SNG_NOTHING;

    if (!p_ptr || p_ptr->is_dead || !legendary_area_id)
        return;
    if (in_bounds(p_ptr->py, p_ptr->px))
        area_id = legendary_area_id[p_ptr->py][p_ptr->px];
    if (legendary_player_area_id == area_id)
        return;

    if (legendary_runtime.active &&
        legendary_player_area_id == legendary_runtime.area_id)
        previous_song = legendary_runtime.area.record.song_id;

    legendary_player_area_id = area_id;
    if (previous_song != SNG_NOTHING &&
        (area_id == META_DUNGEON_LEGENDARY_AREA_ID_NONE ||
         area_id != legendary_runtime.area_id))
        legendary_area_stop_area_only_song(previous_song);

    if (!legendary_runtime.active || area_id != legendary_runtime.area_id ||
        area_id == META_DUNGEON_LEGENDARY_AREA_ID_NONE)
        return;

    if (!legendary_runtime.entry_seen)
    {
        if (legendary_runtime.area.record.entry_message[0])
            msg_format("A memory of the First Age stirs here: %s",
                legendary_runtime.area.record.entry_message);
        else
            msg_print("A memory of the First Age stirs here.");
        legendary_runtime.entry_seen = true;
    }
}

void legendary_song_observe_begin(int song_id, int effective_score)
{
    memset(&legendary_observer, 0, sizeof(legendary_observer));
    if (song_id <= SNG_NOTHING || song_id >= SNG_MAX)
        return;
    legendary_observer.active = true;
    legendary_observer.song_id = song_id;
    legendary_observer.effective_score = effective_score;
}

void legendary_song_observe_monster(int m_idx, int effect_kind)
{
    monster_type* m_ptr;
    monster_race* r_ptr;

    (void)effect_kind;

    if (!legendary_observer.active)
        return;
    if (m_idx <= 0 || m_idx >= mon_max)
        return;

    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx)
        return;

    for (int i = 0; i < legendary_observer.affected_count; i++)
    {
        if (legendary_observer.m_idx[i] == m_idx)
            return;
    }

    if (legendary_observer.affected_count >= META_DUNGEON_AFFECTED_MONSTER_MAX)
        return;

    r_ptr = &r_info[m_ptr->r_idx];
    int slot = legendary_observer.affected_count++;
    legendary_observer.m_idx[slot] = (s16b)m_idx;
    legendary_observer.y[slot] = m_ptr->fy;
    legendary_observer.x[slot] = m_ptr->fx;
    legendary_observer.race_guid[slot] = score_guid_from_u64(r_ptr->guid);
}

static bool legendary_feature_walkable_or_door(int y, int x)
{
    if (!in_bounds(y, x))
        return false;
    return cave_floor_bold(y, x) || cave_any_closed_door_bold(y, x) ||
        cave_forge_bold(y, x) || cave_glyph(y, x);
}

static bool legendary_feature_border(int y, int x)
{
    if (!in_bounds(y, x))
        return false;
    return cave_wall_bold(y, x) || cave_any_closed_door_bold(y, x) ||
        cave_forge_bold(y, x) || cave_glyph(y, x) ||
        cave_feat[y][x] == FEAT_CHASM;
}

static bool legendary_capture_member(int y, int x, int source_kind,
                                     int source_partition,
                                     level_partition_kind partition_kind)
{
    u16b info;

    if (!legendary_feature_walkable_or_door(y, x))
        return false;

    info = cave_info[y][x];
    switch (source_kind)
    {
    case META_DUNGEON_SOURCE_GREATER_VAULT:
        return (info & CAVE_G_VAULT) != 0;
    case META_DUNGEON_SOURCE_SPECIAL_ROOM:
        return (info & CAVE_ROOM) && (info & CAVE_ICKY);
    case META_DUNGEON_SOURCE_NORMAL_ROOM:
        return (info & CAVE_ROOM) && !(info & CAVE_ICKY);
    case META_DUNGEON_SOURCE_PARTITION:
    case META_DUNGEON_SOURCE_CORRIDOR:
        if ((info & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0)
            return false;
        if (source_partition >= 0 &&
            level_partition_index_for_point(y, x) != source_partition)
            return false;
        if (partition_kind != LEVEL_PART_NONE &&
            level_partition_kind_for_point(y, x) != partition_kind)
            return false;
        return true;
    default:
        return true;
    }
}

static int legendary_capture_source_kind(int y, int x)
{
    u16b info = cave_info[y][x];

    if (info & CAVE_G_VAULT)
        return META_DUNGEON_SOURCE_GREATER_VAULT;
    if ((info & CAVE_ICKY) && (info & CAVE_ROOM))
        return META_DUNGEON_SOURCE_SPECIAL_ROOM;
    if (info & CAVE_ROOM)
        return META_DUNGEON_SOURCE_NORMAL_ROOM;
    if (level_partition_kind_for_point(y, x) != LEVEL_PART_NONE)
        return META_DUNGEON_SOURCE_PARTITION;
    return META_DUNGEON_SOURCE_CORRIDOR;
}

static void legendary_rect_init(legendary_capture_rect* rect, int y, int x)
{
    rect->y1 = rect->y2 = y;
    rect->x1 = rect->x2 = x;
}

static void legendary_rect_include(legendary_capture_rect* rect, int y, int x)
{
    rect->y1 = MIN(rect->y1, y);
    rect->x1 = MIN(rect->x1, x);
    rect->y2 = MAX(rect->y2, y);
    rect->x2 = MAX(rect->x2, x);
}

static void legendary_rect_clamp(legendary_capture_rect* rect)
{
    int hgt = p_ptr ? p_ptr->cur_map_hgt : MAX_DUNGEON_HGT;
    int wid = p_ptr ? p_ptr->cur_map_wid : MAX_DUNGEON_WID;

    rect->y1 = MAX(1, MIN(rect->y1, hgt - 2));
    rect->x1 = MAX(1, MIN(rect->x1, wid - 2));
    rect->y2 = MAX(1, MIN(rect->y2, hgt - 2));
    rect->x2 = MAX(1, MIN(rect->x2, wid - 2));
    if (rect->y2 < rect->y1) rect->y2 = rect->y1;
    if (rect->x2 < rect->x1) rect->x2 = rect->x1;
}

static void legendary_rect_limit(legendary_capture_rect* rect)
{
    legendary_rect_clamp(rect);
    if (rect->y2 - rect->y1 + 1 > META_DUNGEON_MAX_HGT)
    {
        int cy = (rect->y1 + rect->y2) / 2;
        rect->y1 = cy - META_DUNGEON_MAX_HGT / 2;
        rect->y2 = rect->y1 + META_DUNGEON_MAX_HGT - 1;
    }
    if (rect->x2 - rect->x1 + 1 > META_DUNGEON_MAX_WID)
    {
        int cx = (rect->x1 + rect->x2) / 2;
        rect->x1 = cx - META_DUNGEON_MAX_WID / 2;
        rect->x2 = rect->x1 + META_DUNGEON_MAX_WID - 1;
    }
    legendary_rect_clamp(rect);
}

static bool legendary_capture_cell_included(int y, int x, int source_kind,
                                            int source_partition,
                                            level_partition_kind partition_kind)
{
    if (legendary_capture_member(y, x, source_kind, source_partition,
            partition_kind))
        return true;
    if (!legendary_feature_border(y, x))
        return false;

    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            if (!dy && !dx)
                continue;
            if (legendary_capture_member(y + dy, x + dx, source_kind,
                    source_partition, partition_kind))
                return true;
        }
    }

    return false;
}

static bool legendary_capture_area(meta_dungeon_record* record,
                                   byte** out_blob,
                                   u32b* out_blob_size)
{
    legendary_capture_rect rect;
    byte* include = NULL;
    byte* blob = NULL;
    u32b mask_bytes;
    u32b cell_count = 0;
    u32b ordinal = 0;
    int hgt;
    int wid;
    int source_kind;
    int source_partition;
    level_partition_kind partition_kind;

    if (!record || !out_blob || !out_blob_size || !p_ptr)
        return false;

    *out_blob = NULL;
    *out_blob_size = 0;
    if (!in_bounds(p_ptr->py, p_ptr->px))
        return false;

    source_kind = legendary_capture_source_kind(p_ptr->py, p_ptr->px);
    source_partition = level_partition_index_for_point(p_ptr->py, p_ptr->px);
    partition_kind = level_partition_kind_for_point(p_ptr->py, p_ptr->px);

    legendary_rect_init(&rect, p_ptr->py, p_ptr->px);
    for (int i = 0; i < legendary_observer.affected_count; i++)
    {
        if (in_bounds(legendary_observer.y[i], legendary_observer.x[i]))
            legendary_rect_include(&rect, legendary_observer.y[i],
                legendary_observer.x[i]);
    }
    rect.y1 -= 2;
    rect.x1 -= 2;
    rect.y2 += 2;
    rect.x2 += 2;
    legendary_rect_limit(&rect);

    hgt = rect.y2 - rect.y1 + 1;
    wid = rect.x2 - rect.x1 + 1;
    include = mem_alloc_array((size_t)hgt * (size_t)wid, byte);
    if (!include)
        return false;

    for (int yy = 0; yy < hgt; yy++)
    {
        for (int xx = 0; xx < wid; xx++)
        {
            int y = rect.y1 + yy;
            int x = rect.x1 + xx;
            if (!legendary_capture_cell_included(y, x, source_kind,
                    source_partition, partition_kind))
                continue;
            include[yy * wid + xx] = 1;
            cell_count++;
        }
    }

    if (cell_count == 0 || cell_count > META_DUNGEON_MAX_MASK_CELLS)
        goto fail;

    mask_bytes = (u32b)((hgt * wid + 7) / 8);
    *out_blob_size = mask_bytes + cell_count * 3u;
    blob = mem_alloc_array(*out_blob_size, byte);
    if (!blob)
        goto fail;

    memset(blob, 0, *out_blob_size);
    for (int yy = 0; yy < hgt; yy++)
    {
        for (int xx = 0; xx < wid; xx++)
        {
            int cell = yy * wid + xx;
            int y = rect.y1 + yy;
            int x = rect.x1 + xx;
            byte role = 0;

            if (!include[cell])
                continue;

            blob[cell / 8] |= (byte)(1u << (cell % 8));
            blob[mask_bytes + ordinal] = cave_feat[y][x];
            blob[mask_bytes + cell_count + ordinal] = cave_color[y][x];

            if (cave_info[y][x] & CAVE_ROOM) role |= META_DUNGEON_TILE_ROLE_ROOM;
            if (cave_info[y][x] & CAVE_ICKY) role |= META_DUNGEON_TILE_ROLE_ICKY;
            if (cave_info[y][x] & CAVE_GLOW) role |= META_DUNGEON_TILE_ROLE_GLOW;
            if (cave_info[y][x] & CAVE_CHASM_AREA)
                role |= META_DUNGEON_TILE_ROLE_CHASM;
            blob[mask_bytes + cell_count * 2u + ordinal] = role;
            ordinal++;
        }
    }

    record->source_kind = (byte)source_kind;
    record->partition_kind = (byte)partition_kind;
    record->big_cave_type =
        (byte)level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px);
    record->hgt = (byte)hgt;
    record->wid = (byte)wid;
    record->style_primary = cave_color[p_ptr->py][p_ptr->px];
    record->singer_y = (s16b)(p_ptr->py - rect.y1);
    record->singer_x = (s16b)(p_ptr->px - rect.x1);
    record->mask_cell_count = (u16b)cell_count;
    record->tile_blob_size = *out_blob_size;
    *out_blob = blob;
    mem_free_null(include);
    return true;

fail:
    mem_free_null(include);
    mem_free_null(blob);
    *out_blob_size = 0;
    return false;
}

static bool legendary_song_is_starting_song(int song_id)
{
    const character_profile* cp;

    if (!p_ptr || !c_info || !z_info || !b_info || !b_name)
        return false;
    if (p_ptr->pcharacter >= z_info->c_max)
        return false;

    cp = &c_info[p_ptr->pcharacter];
    for (int i = 0; i < CHARACTER_ABILITY_MAX; i++)
    {
        if (cp->a_adj[i][0] == S_SNG && cp->a_adj[i][1] == song_id)
            return true;
    }

    return false;
}

static const char* legendary_song_name(int song_id)
{
    int idx = ability_index(S_SNG, song_id);

    if (idx >= 0 && b_info && b_info[idx].name)
        return b_name + b_info[idx].name;
    return "the song";
}

static const char* legendary_singer_name(void)
{
    if (op_ptr && op_ptr->full_name[0])
        return op_ptr->full_name;
    if (p_ptr && c_info && c_name && z_info && p_ptr->pcharacter < z_info->c_max)
        return c_name + c_info[p_ptr->pcharacter].name;
    return "a singer of old";
}

void legendary_song_observe_end(int song_id, int effective_score)
{
    meta_dungeon_record record;
    meta_dungeon_area* areas = NULL;
    u32b area_count = 0;
    byte* tile_blob = NULL;
    u32b tile_blob_size = 0;
    guid64 singer_guid = { 0, 0 };
    int chance;

    if (!legendary_observer.active || legendary_observer.song_id != song_id)
        return;

    if (effective_score <= 0)
        effective_score = legendary_observer.effective_score;

    if (legendary_observer.affected_count <= 0)
        goto cleanup;
    if (effective_score < 15)
        goto cleanup;
    if (!p_ptr || p_ptr->depth <= 0 || p_ptr->depth >= MORGOTH_DEPTH)
        goto cleanup;
    if (!legendary_song_is_starting_song(song_id))
        goto cleanup;

    singer_guid = c_info[p_ptr->pcharacter].guid;
    if (meta_dungeon_load_for_current_metarun(&areas, &area_count))
    {
        for (u32b i = 0; i < area_count; i++)
        {
            if (areas[i].record.song_id == song_id &&
                areas[i].record.depth == p_ptr->depth &&
                meta_state_guid_equal(areas[i].record.singer_character_guid,
                    singer_guid))
                goto cleanup;
        }
    }

    chance = 2 + ((effective_score - 15) / 2);
    chance = MAX(2, MIN(20, chance));
    if (!percent_chance(chance))
        goto cleanup;

    memset(&record, 0, sizeof(record));
    record.meta.metarun_id = metar.id;
    record.meta.record_guid = score_guid_random();
    record.meta.created_turn = turn;
    record.meta.creation_depth = (byte)p_ptr->depth;
    record.song_id = (byte)song_id;
    record.depth = (byte)p_ptr->depth;
    record.singer_character_guid = singer_guid;
    strnfmt(record.singer_name, sizeof(record.singer_name), "%s",
        legendary_singer_name());
    record.affected_monster_count =
        (byte)legendary_observer.affected_count;
    for (int i = 0; i < legendary_observer.affected_count; i++)
    {
        record.affected_monster_guid[i] = legendary_observer.race_guid[i];
        record.affected_rel_y[i] =
            (s16b)(legendary_observer.y[i] - p_ptr->py);
        record.affected_rel_x[i] =
            (s16b)(legendary_observer.x[i] - p_ptr->px);
    }

    strnfmt(record.entry_message, sizeof(record.entry_message),
        "%s once raised %s here, and the stone remembers.",
        record.singer_name, legendary_song_name(song_id));

    if (!legendary_capture_area(&record, &tile_blob, &tile_blob_size))
        goto cleanup;

    if (!meta_dungeon_register_legendary_area(&record, tile_blob,
            tile_blob_size))
    {
        log_warn("legendary song: failed to register area song=%d depth=%d",
            song_id, p_ptr->depth);
        goto cleanup;
    }

    log_debug("legendary song: registered song=%d depth=%d score=%d chance=%d%% size=%dx%d cells=%u",
        song_id, p_ptr->depth, effective_score, chance, record.wid,
        record.hgt, (unsigned)record.mask_cell_count);

cleanup:
    meta_dungeon_areas_free(areas, area_count);
    mem_free_null(tile_blob);
    memset(&legendary_observer, 0, sizeof(legendary_observer));
}

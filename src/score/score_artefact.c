#include "score/score_artefact.h"

#include "angband.h"
#include "fs/file.h"
#include "fs/path.h"
#include "log/log.h"
#include "score/score_format.h"
#include "score/score_guid.h"

#define SCORE_ARTEFACT_DB_MAGIC "ARDB"
#define SCORE_ARTEFACT_DB_VERSION 0x00010000u

typedef struct artefact_db_header {
    char magic[4];
    u32b version;
    u32b record_count;
    u32b reserved;
} artefact_db_header;

typedef struct artefact_db_record_v1 {
    score_guid64 guid;
    char name[MAX_LEN_ART_NAME];
    byte tval;
    byte sval;
    s16b pval;
    s16b att;
    s16b evn;
    byte dd, ds;
    byte pd, ps;
    s16b weight;
    s32b cost;
    u32b flags1, flags2, flags3;
    byte level;
    byte rarity;
    byte activation;
    u16b time;
    u16b randtime;
    byte abilities;
    byte skilltype[4];
    byte abilitynum[4];
} artefact_db_record_v1;

static void artefact_db_build_path(char* path, size_t len)
{
    path_build(path, len, ANGBAND_DIR_APEX, "artefacts.db");
}

static void artefact_db_init_header(artefact_db_header* header)
{
    memset(header, 0, sizeof(*header));
    memcpy(header->magic, SCORE_ARTEFACT_DB_MAGIC, sizeof(header->magic));
    header->version = SCORE_ARTEFACT_DB_VERSION;
}

static ang_file* artefact_db_open(const char* path,
                                  artefact_db_header* header,
                                  bool* created)
{
    if (created)
        *created = false;

    ang_file* file = ang_file_open_compat(path, "r+b");
    if (!file) {
        file = ang_file_open_compat(path, "w+b");
        if (!file)
            return NULL;
        if (created)
            *created = true;
    }

    if (created && *created) {
        artefact_db_init_header(header);
        ang_file_write_compat(file, header, sizeof(*header));
        ang_file_seek_compat(file, 0, ANG_FILE_SEEK_END);
        return file;
    }

    ang_file_seek_compat(file, 0, ANG_FILE_SEEK_SET);
    if (ang_file_read_compat(file, header, sizeof(*header)) != sizeof(*header) ||
        memcmp(header->magic, SCORE_ARTEFACT_DB_MAGIC, sizeof(header->magic)) != 0) {
        artefact_db_init_header(header);
        ang_file_seek_compat(file, 0, ANG_FILE_SEEK_SET);
        ang_file_write_compat(file, header, sizeof(*header));
    }
    ang_file_seek_compat(file, 0, ANG_FILE_SEEK_END);
    return file;
}

static bool artefact_db_find(ang_file* file, const score_guid64* guid,
                             artefact_db_record_v1* out, ang_file_off_t* offset)
{
    if (!file || !guid)
        return false;

    if (ang_file_seek_compat(file, sizeof(artefact_db_header), ANG_FILE_SEEK_SET) < 0)
        return false;

    artefact_db_record_v1 rec;
    ang_file_off_t pos = ang_file_tell_compat(file);
    while (ang_file_read_compat(file, &rec, sizeof(rec)) == sizeof(rec)) {
        if (rec.guid.hi == guid->hi && rec.guid.lo == guid->lo) {
            if (out)
                *out = rec;
            if (offset)
                *offset = pos;
            ang_file_seek_compat(file, 0, ANG_FILE_SEEK_END);
            return true;
        }
        pos = ang_file_tell_compat(file);
    }

    ang_file_seek_compat(file, 0, ANG_FILE_SEEK_END);
    return false;
}

static void artefact_db_snapshot(const artefact_type* art,
                                 artefact_db_record_v1* rec)
{
    memset(rec, 0, sizeof(*rec));
    rec->guid = art->guid;
    SDL_strlcpy(rec->name, art->name, sizeof(rec->name));
    rec->tval = art->tval;
    rec->sval = art->sval;
    rec->pval = art->pval;
    rec->att = art->att;
    rec->evn = art->evn;
    rec->dd = art->dd;
    rec->ds = art->ds;
    rec->pd = art->pd;
    rec->ps = art->ps;
    rec->weight = art->weight;
    rec->cost = art->cost;
    rec->flags1 = art->flags1;
    rec->flags2 = art->flags2;
    rec->flags3 = art->flags3;
    rec->level = art->level;
    rec->rarity = art->rarity;
    rec->activation = art->activation;
    rec->time = art->time;
    rec->randtime = art->randtime;
    rec->abilities = art->abilities;
    for (int i = 0; i < 4; i++) {
        rec->skilltype[i] = art->skilltype[i];
        rec->abilitynum[i] = art->abilitynum[i];
    }
}

bool score_artefact_register(const artefact_type* art)
{
    if (!art || score_guid_is_zero(&art->guid))
        return false;

    char path[1024];
    artefact_db_build_path(path, sizeof(path));

    artefact_db_header header;
    bool created = false;

    safe_setuid_grab();
    ang_file* file = artefact_db_open(path, &header, &created);
    safe_setuid_drop();

    if (!file) {
        log_warn("score_artefact: unable to open %s", path);
        return false;
    }

    artefact_db_record_v1 existing;
    ang_file_off_t offset = -1;
    bool found = artefact_db_find(file, &art->guid, &existing, &offset);

    artefact_db_record_v1 snapshot;
    artefact_db_snapshot(art, &snapshot);

    bool ok = false;
    if (found) {
        if (ang_file_seek_compat(file, offset, ANG_FILE_SEEK_SET) >= 0 &&
            ang_file_write_compat(file, &snapshot, sizeof(snapshot)) == sizeof(snapshot)) {
            ok = true;
        }
    } else {
        if (ang_file_write_compat(file, &snapshot, sizeof(snapshot)) == sizeof(snapshot)) {
            header.record_count++;
            ang_file_seek_compat(file, 0, ANG_FILE_SEEK_SET);
            ang_file_write_compat(file, &header, sizeof(header));
            ok = true;
        }
    }

    (void)ang_file_close_compat(file);
    return ok;
}

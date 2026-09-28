#include "angband.h"
#include <assert.h>

#ifdef TEST_MEMORY_DB
#include "metarun/metarun-artefact-memory.c"
typedef metarun_artefact_memory_header test_header;
typedef metarun_artefact_memory_record test_record;
#define init_header metarun_artefact_memory_init_header
static SDL_IOStream* open_database(const char* path, test_header* header)
{
    (void)path;
    return metarun_artefact_memory_open(header, true);
}
#else
#include "score/score_artefact.c"
typedef artefact_db_header test_header;
typedef artefact_db_record_v1 test_record;
#define init_header artefact_db_init_header
static SDL_IOStream* open_database(const char* path, test_header* header)
{
    bool created;
    return artefact_db_open(path, header, &created);
}
#endif

static void reject_unchanged(const char* path, const void* bytes, size_t size)
{
    test_header header;
    byte actual[sizeof(test_header) + sizeof(test_record) + 1];
    SDL_IOStream* file = SDL_IOFromFile(path, "wb");
    assert(file && size <= sizeof(actual));
    assert(SDL_WriteIO(file, bytes, size) == size);
    assert(SDL_CloseIO(file));
    assert(!open_database(path, &header));
    file = SDL_IOFromFile(path, "rb");
    assert(file && SDL_GetIOSize(file) == (Sint64)size);
    assert(SDL_ReadIO(file, actual, size) == size);
    assert(!memcmp(actual, bytes, size));
    assert(SDL_CloseIO(file));
}

int main(int argc, char** argv)
{
    char path[1024], meta_path[1024];
    test_header header;
    byte bytes[sizeof(test_header) + sizeof(test_record) + 1] = {0};
    assert(argc == 2);
    log_set_level(LOG_ERROR);
    ANGBAND_DIR_APEX = argv[1];
    assert(path_build(meta_path, sizeof(meta_path), argv[1], "metaruns"));
    ANGBAND_DIR_METARUN = meta_path;
#ifdef TEST_MEMORY_DB
    assert(path_build(path, sizeof(path), argv[1], "artefact_memory.db"));
#else
    assert(path_build(path, sizeof(path), argv[1], "artefacts.db"));
#endif
    SDL_IOStream* file = open_database(path, &header);
    assert(file && header.record_count == 0);
    assert(SDL_CloseIO(file));
    file = open_database(path, &header);
    assert(file && header.record_count == 0);
    assert(SDL_CloseIO(file));

    /* A valid append followed by an update remains readable and does not
     * inflate record_count under the stricter length validation. */
    artefact_type arts[2] = {{0}};
    arts[1].guid.hi = 1; arts[1].guid.lo = 2;
    arts[1].tval = TV_SWORD;
    SDL_strlcpy(arts[1].name, "Remembered blade", sizeof(arts[1].name));
#ifdef TEST_MEMORY_DB
    maxima limits = {0};
    limits.art_max = 2;
    z_info = &limits; a_info = arts; metar.id = 123;
    assert(metarun_record_artefact_revealed(1));
    assert(metarun_record_artefact_revealed(1));
#else
    assert(score_artefact_register(&arts[1]));
    arts[1].att = 7;
    assert(score_artefact_register(&arts[1]));
#endif
    file = open_database(path, &header);
    assert(file && header.record_count == 1);
    test_record record;
    assert(SDL_SeekIO(file, sizeof(header), SDL_IO_SEEK_SET) >= 0);
    assert(SDL_ReadIO(file, &record, sizeof(record)) == sizeof(record));
    assert(record.guid.hi == 1 && record.guid.lo == 2);
#ifdef TEST_MEMORY_DB
    assert(record.metarun_id == 123 && record.flags == METARUN_ARTEFACT_MEMORY_REVEALED);
#else
    assert(record.att == 7);
#endif
    assert(SDL_CloseIO(file));

    init_header(&header);
    header.version++;
    memcpy(bytes, &header, sizeof(header));
    reject_unchanged(path, bytes, sizeof(header));
    init_header(&header);
    header.record_count = 1;
    memcpy(bytes, &header, sizeof(header));
    reject_unchanged(path, bytes, sizeof(header) + sizeof(test_record) - 1);
    header.record_count = 0;
    memcpy(bytes, &header, sizeof(header));
    reject_unchanged(path, bytes, sizeof(bytes));
    reject_unchanged(path, bytes, sizeof(header) - 1);
    bytes[0] = '?';
    reject_unchanged(path, bytes, sizeof(header));

    init_header(&header);
    header.record_count = 1;
    memcpy(bytes, &header, sizeof(header));
    file = SDL_IOFromFile(path, "wb");
    assert(file);
    assert(SDL_WriteIO(file, bytes, sizeof(bytes) - 1) == sizeof(bytes) - 1);
    assert(SDL_CloseIO(file));
    file = open_database(path, &header);
    assert(file && header.record_count == 1);
    assert(SDL_CloseIO(file));
    puts("Artefact database: create/reopen/append/update, incompatible versions, truncation and invalid counts preserve existing bytes: PASS");
    return 0;
}

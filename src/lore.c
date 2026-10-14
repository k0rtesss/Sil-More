#include "angband.h"
#include "externs.h"
#include "lore.h"
#include "cJSON.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <stdarg.h>

/* The catalog contains presentation only. In particular, do not modify the
 * template pools: starting gear, randarts, sets and scores use their names. */
typedef struct lore_rule {
    cptr from;
    cptr to;
    size_t length;
    size_t replacement_length;
    bool passage;
    struct lore_rule* next;
} lore_rule;

typedef struct lore_cached_text {
    char* canonical;
    char* translated;
    struct lore_cached_text* next;
    struct lore_cached_text* older;
    struct lore_cached_text* newer;
    size_t bucket;
    size_t bytes;
} lore_cached_text;

#define LORE_CACHE_BUCKETS 1024
#define LORE_CACHE_ENTRIES 4096
#define LORE_CACHE_BYTES (16 * 1024 * 1024)
static cJSON* catalog;
static lore_rule* rules;
static lore_rule* term_buckets[256];
static lore_rule* passage_buckets[256];
static lore_cached_text* text_cache[LORE_CACHE_BUCKETS];
static lore_cached_text* oldest_text;
static lore_cached_text* newest_text;
static size_t cached_entries;
static size_t cached_bytes;

static void lore_cache_touch(lore_cached_text* entry)
{
    if (newest_text == entry)
        return;
    if (entry->older)
        entry->older->newer = entry->newer;
    else if (oldest_text == entry)
        oldest_text = entry->newer;
    if (entry->newer)
        entry->newer->older = entry->older;
    entry->older = newest_text;
    entry->newer = NULL;
    if (newest_text)
        newest_text->newer = entry;
    else
        oldest_text = entry;
    newest_text = entry;
}

static void lore_cache_evict(void)
{
    lore_cached_text* entry = oldest_text;
    lore_cached_text** position = &text_cache[entry->bucket];
    while (*position != entry)
        position = &(*position)->next;
    *position = entry->next;
    oldest_text = entry->newer;
    if (oldest_text)
        oldest_text->older = NULL;
    else
        newest_text = NULL;
    --cached_entries;
    cached_bytes -= entry->bytes;
    SDL_free(entry->canonical);
    SDL_free(entry->translated);
    SDL_free(entry);
}

static unsigned char lore_fold(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static bool lore_word(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_' || c >= 128;
}

void lore_cleanup(void)
{
    for (size_t i = 0; i < LORE_CACHE_BUCKETS; ++i) {
        lore_cached_text* entry = text_cache[i];
        while (entry) {
            lore_cached_text* next = entry->next;
            SDL_free(entry->canonical);
            SDL_free(entry->translated);
            SDL_free(entry);
            entry = next;
        }
    }
    memset(text_cache, 0, sizeof(text_cache));
    oldest_text = newest_text = NULL;
    cached_entries = cached_bytes = 0;
    memset(term_buckets, 0, sizeof(term_buckets));
    memset(passage_buckets, 0, sizeof(passage_buckets));
    SDL_free(rules);
    rules = NULL;
    cJSON_Delete(catalog);
    catalog = NULL;
}

bool lore_init(void)
{
    char path[1024];
    char* json;
    size_t size = 0;
    SDL_IOStream* stream;
    int count;
    size_t used = 0;

    if (catalog)
        return true;
    path_build(path, sizeof(path), ANGBAND_DIR_PREF, "generic_lore.json");
    stream = sdl_fopen(path, "rb");
    if (!stream)
        goto failed;
    json = SDL_LoadFile_IO(stream, &size, true);
    if (!json)
        goto failed;
    catalog = cJSON_ParseWithLength(json, size);
    SDL_free(json);
    if (!cJSON_IsObject(catalog)
        || !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(catalog, "terms"))
        || !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(catalog, "passages")))
        goto failed;
    count = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(catalog, "terms"))
        + cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(catalog, "passages"));
    if (count <= 0)
        goto failed;
    rules = SDL_calloc((size_t)count, sizeof(*rules));
    if (!rules)
        goto failed;
    for (int group = 0; group < 2; ++group) {
        cJSON* pair;
        cJSON* list = cJSON_GetObjectItemCaseSensitive(catalog,
            group ? "passages" : "terms");
        cJSON_ArrayForEach(pair, list) {
            cJSON* from = cJSON_GetArrayItem(pair, 0);
            cJSON* to = cJSON_GetArrayItem(pair, 1);
            lore_rule* rule;
            lore_rule** position;
            unsigned char bucket;
            if (!cJSON_IsArray(pair) || cJSON_GetArraySize(pair) != 2
                || !cJSON_IsString(from) || !from->valuestring[0]
                || !cJSON_IsString(to) || !to->valuestring[0])
                goto failed;
            rule = &rules[used++];
            rule->from = from->valuestring;
            rule->to = to->valuestring;
            rule->length = strlen(rule->from);
            rule->replacement_length = strlen(rule->to);
            rule->passage = group != 0;
            bucket = (unsigned char)rule->from[0];
            if (!group)
                bucket = lore_fold(bucket);
            position = group ? &passage_buckets[bucket] : &term_buckets[bucket];
            /* A full narrative always wins; within each group longest wins. */
            while (*position && (*position)->length >= rule->length)
                position = &(*position)->next;
            rule->next = *position;
            *position = rule;
        }
    }
    log_info("Generic lore loaded: %d substitutions", count);
    return true;

failed:
    log_error("Cannot load generic lore catalog '%s'", path);
    lore_cleanup();
    return false;
}

bool lore_set_generic(bool enabled)
{
    if (enabled && !lore_init())
        return false;
    op_ptr->opt[OPT_generic] = enabled;
    return true;
}

static const lore_rule* lore_match(cptr start, cptr at)
{
    const lore_rule* rule;
    for (rule = passage_buckets[(unsigned char)*at]; rule; rule = rule->next)
        if (strncmp(at, rule->from, rule->length) == 0)
            return rule;
    for (rule = term_buckets[lore_fold((unsigned char)*at)]; rule;
         rule = rule->next) {
        size_t i;
        if (at > start && lore_word((unsigned char)at[-1])
            && lore_word((unsigned char)rule->from[0]))
            continue;
        for (i = 0; i < rule->length && at[i]; ++i)
            if (lore_fold((unsigned char)at[i])
                != lore_fold((unsigned char)rule->from[i]))
                break;
        if (i == rule->length
            && !(lore_word((unsigned char)rule->from[i - 1])
                && lore_word((unsigned char)at[i])))
            return rule;
    }
    return NULL;
}

static void lore_replace(char* dest, cptr source, const lore_rule* rule)
{
    bool upper = !rule->passage;
    bool lower = !rule->passage;
    size_t letters = 0;
    for (size_t i = 0; i < rule->length; ++i) {
        unsigned char c = (unsigned char)source[i];
        if (c >= 'a' && c <= 'z') {
            upper = false;
            ++letters;
        }
        if (c >= 'A' && c <= 'Z') {
            lower = false;
            ++letters;
        }
    }
    for (size_t i = 0; i < rule->replacement_length; ++i) {
        unsigned char c = (unsigned char)rule->to[i];
        if (upper && letters > 1 && c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        else if (lower && letters && c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        dest[i] = (char)c;
    }
}

cptr lore_text(cptr canonical)
{
    unsigned long hash = 5381;
    size_t length = 0;
    bool changed = false;
    lore_cached_text* entry;
    char* out;

    if (!canonical || !op_ptr || !op_ptr->opt[OPT_generic] || !catalog)
        return canonical;
    for (cptr p = canonical; *p; ++p)
        hash = ((hash << 5) + hash) ^ (unsigned char)*p;
    hash %= LORE_CACHE_BUCKETS;
    for (entry = text_cache[hash]; entry; entry = entry->next)
        if (strcmp(entry->canonical, canonical) == 0) {
            lore_cache_touch(entry);
            return entry->translated;
        }
    for (cptr p = canonical; *p;) {
        const lore_rule* rule = lore_match(canonical, p);
        size_t add = rule ? rule->replacement_length : 1;
        if (length > SIZE_MAX - add - 1)
            return canonical;
        length += add;
        p += rule ? rule->length : 1;
        changed |= rule != NULL;
    }
    if (!changed)
        return canonical;
    entry = SDL_calloc(1, sizeof(*entry));
    if (!entry)
        return canonical;
    entry->canonical = SDL_strdup(canonical);
    entry->translated = SDL_malloc(length + 1);
    if (!entry->canonical || !entry->translated) {
        SDL_free(entry->canonical);
        SDL_free(entry->translated);
        SDL_free(entry);
        return canonical;
    }
    out = entry->translated;
    for (cptr p = canonical; *p;) {
        const lore_rule* rule = lore_match(canonical, p);
        if (rule) {
            lore_replace(out, p, rule);
            out += rule->replacement_length;
            p += rule->length;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
    entry->bucket = hash;
    entry->bytes = sizeof(*entry) + strlen(canonical) + length + 2;
    /* Numeric combat and journal messages are often unique. Keep them from
     * growing the presentation cache for the lifetime of a long metarun. */
    while (oldest_text && (cached_entries >= LORE_CACHE_ENTRIES
        || cached_bytes + entry->bytes > LORE_CACHE_BYTES))
        lore_cache_evict();
    entry->next = text_cache[hash];
    text_cache[hash] = entry;
    ++cached_entries;
    cached_bytes += entry->bytes;
    lore_cache_touch(entry);
    return entry->translated;
}

void lore_copy(char* dest, size_t size, cptr canonical)
{
    cptr translated = lore_text(canonical);
    if (dest != translated && size)
        SDL_strlcpy(dest, translated ? translated : "", size);
}

int lore_dump_printf(SDL_IOStream* stream, cptr fmt, ...)
{
    va_list args;
    char* text = NULL;
    int result;
    va_start(args, fmt);
    result = SDL_vasprintf(&text, fmt, args);
    va_end(args);
    if (result >= 0) {
        result = SDL_IOprintf(stream, "%s", lore_text(text));
        SDL_free(text);
    }
    return result;
}

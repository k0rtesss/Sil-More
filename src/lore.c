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
    bool case_sensitive;
    struct lore_rule* next;
    struct lore_rule* prose_next;
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
static lore_rule* description_buckets[LORE_CACHE_BUCKETS];
static lore_rule* prose_buckets[256];
static lore_rule* prose_wildcards;
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

static unsigned long lore_hash(cptr text)
{
    unsigned long hash = 5381;
    for (; *text; ++text)
        hash = ((hash << 5) + hash) ^ (unsigned char)*text;
    return hash % LORE_CACHE_BUCKETS;
}

static cJSON* lore_read_catalog(cptr path)
{
    size_t size = 0;
    SDL_IOStream* stream = sdl_fopen(path, "rb");
    char* json;
    cJSON* root;
    if (!stream)
        return NULL;
    json = SDL_LoadFile_IO(stream, &size, true);
    if (!json)
        return NULL;
    root = cJSON_ParseWithLength(json, size);
    SDL_free(json);
    return root;
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
    memset(description_buckets, 0, sizeof(description_buckets));
    memset(prose_buckets, 0, sizeof(prose_buckets));
    prose_wildcards = NULL;
    SDL_free(rules);
    rules = NULL;
    cJSON_Delete(catalog);
    catalog = NULL;
}

bool lore_init(void)
{
    char path[1024];
    static const char* groups[] = { "terms", "passages", "descriptions" };
    int count;
    size_t used = 0;

    if (catalog)
        return true;
    path_build(path, sizeof(path), ANGBAND_DIR_PREF, "generic_lore.json");
    catalog = lore_read_catalog(path);
    if (!cJSON_IsObject(catalog)
        || !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(catalog, "terms"))
        || !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(catalog, "passages")))
        goto failed;
    if (!cJSON_GetObjectItemCaseSensitive(catalog, "descriptions")
        && !cJSON_AddArrayToObject(catalog, "descriptions"))
        goto failed;
    if (!cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(catalog, "descriptions")))
        goto failed;
    {
        cJSON* includes = cJSON_GetObjectItemCaseSensitive(catalog, "includes");
        cJSON* file;
        if (includes && !cJSON_IsArray(includes))
            goto failed;
        cJSON_ArrayForEach(file, includes) {
            cJSON* supplement;
            bool valid = true;
            /* Catalog includes are resource basenames, never asset paths. */
            if (!cJSON_IsString(file) || !file->valuestring[0]
                || strpbrk(file->valuestring, "/\\:")
                || strstr(file->valuestring, "..")
                || !path_build(path, sizeof(path), ANGBAND_DIR_PREF,
                    file->valuestring))
                goto failed;
            supplement = lore_read_catalog(path);
            if (!cJSON_IsObject(supplement)) {
                cJSON_Delete(supplement);
                goto failed;
            }
            for (size_t group = 0; group < N_ELEMENTS(groups); ++group) {
                cJSON* source = cJSON_GetObjectItemCaseSensitive(supplement, groups[group]);
                cJSON* dest = cJSON_GetObjectItemCaseSensitive(catalog, groups[group]);
                if (!source)
                    continue;
                if (!cJSON_IsArray(source) || !cJSON_IsArray(dest)) {
                    valid = false;
                    break;
                }
                while (cJSON_GetArraySize(source)) {
                    cJSON* pair = cJSON_DetachItemFromArray(source, 0);
                    if (!cJSON_AddItemToArray(dest, pair)) {
                        cJSON_Delete(pair);
                        valid = false;
                        break;
                    }
                }
                if (!valid)
                    break;
            }
            cJSON_Delete(supplement);
            if (!valid)
                goto failed;
        }
    }
    count = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(catalog, "terms"))
        + cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(catalog, "passages"))
        + cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(catalog, "descriptions"));
    if (count <= 0)
        goto failed;
    rules = SDL_calloc((size_t)count, sizeof(*rules));
    if (!rules)
        goto failed;
    for (int group = 0; group < 3; ++group) {
        cJSON* pair;
        cJSON* list = cJSON_GetObjectItemCaseSensitive(catalog,
            groups[group]);
        cJSON_ArrayForEach(pair, list) {
            cJSON* from = cJSON_GetArrayItem(pair, 0);
            cJSON* to = cJSON_GetArrayItem(pair, 1);
            lore_rule* rule;
            lore_rule** position;
            unsigned char bucket;
            int pair_size = cJSON_GetArraySize(pair);
            if (!cJSON_IsArray(pair) || (pair_size != 2
                    && !(group == 0 && pair_size == 3
                        && cJSON_IsBool(cJSON_GetArrayItem(pair, 2))))
                || !cJSON_IsString(from) || !from->valuestring[0]
                || !cJSON_IsString(to) || !to->valuestring[0])
                goto failed;
            rule = &rules[used++];
            rule->from = from->valuestring;
            rule->to = to->valuestring;
            rule->length = strlen(rule->from);
            rule->replacement_length = strlen(rule->to);
            rule->passage = group != 0;
            rule->case_sensitive = cJSON_IsTrue(cJSON_GetArrayItem(pair, 2));
            bucket = (unsigned char)rule->from[0];
            if (!group)
                bucket = lore_fold(bucket);
            position = group == 2 ? &description_buckets[lore_hash(rule->from)]
                : group ? &passage_buckets[bucket] : &term_buckets[bucket];
            if (group == 2) {
                for (lore_rule* existing = *position; existing; existing = existing->next) {
                    if (!strcmp(existing->from, rule->from)
                        && strcmp(existing->to, rule->to)) {
                        log_error("Conflicting generic descriptions: %.100s", rule->from);
                        goto failed;
                    }
                }
            }
            /* A full narrative always wins; within each group longest wins. */
            while (*position && (*position)->length > rule->length)
                position = &(*position)->next;
            rule->next = *position;
            *position = rule;
            if (group == 2 && (rule->length >= 24 || strchr(rule->from, '{'))) {
                lore_rule** prose = rule->from[0] == '{' ? &prose_wildcards
                    : &prose_buckets[(unsigned char)rule->from[0]];
                while (*prose && (*prose)->length >= rule->length)
                    prose = &(*prose)->prose_next;
                rule->prose_next = *prose;
                *prose = rule;
            }
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

bool lore_has_description(cptr canonical)
{
    const lore_rule* rule;
    if (!canonical || !catalog || !canonical[0])
        return false;
    for (rule = description_buckets[lore_hash(canonical)]; rule; rule = rule->next)
        if (!strcmp(rule->from, canonical))
            return true;
    /* Older authored story passages also supply complete replacements. */
    for (rule = passage_buckets[(unsigned char)canonical[0]]; rule; rule = rule->next)
        if (!strcmp(rule->from, canonical))
            return true;
    return false;
}

static const lore_rule* lore_match(cptr start, cptr at)
{
    const lore_rule* rule;
    for (rule = passage_buckets[(unsigned char)*at]; rule; rule = rule->next)
        if (strncmp(at, rule->from, rule->length) == 0)
            return rule;
    if (at > start && at[-1] == '{') {
        cptr token = at;
        while ((*token >= 'A' && *token <= 'Z')
            || (*token >= '0' && *token <= '9') || *token == '_')
            ++token;
        if (token > at && *token == '}')
            return NULL;
    }
    for (rule = term_buckets[lore_fold((unsigned char)*at)]; rule;
         rule = rule->next) {
        size_t i;
        if (at > start && lore_word((unsigned char)at[-1])
            && lore_word((unsigned char)rule->from[0]))
            continue;
        for (i = 0; i < rule->length && at[i]; ++i)
            if (rule->case_sensitive ? at[i] != rule->from[i]
                : lore_fold((unsigned char)at[i])
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
    unsigned long hash;
    size_t length = 0;
    bool changed = false;
    lore_cached_text* entry;
    char* out;

    if (!canonical || !op_ptr || !op_ptr->opt[OPT_generic] || !catalog)
        return canonical;
    hash = lore_hash(canonical);
    /* Descriptions are authored whole fields, including mechanical prose.
     * Match before formatting, controller adaptation or markup selection.
     * Short effects must never replace substrings in an unrelated sentence. */
    for (const lore_rule* rule = description_buckets[hash]; rule; rule = rule->next)
        if (!strcmp(rule->from, canonical))
            return rule->to;
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

#define LORE_CAPTURE_MAX 16
#define LORE_CAPTURE_BYTES 256
typedef struct lore_capture {
    cptr name;
    size_t name_length;
    cptr text;
    size_t length;
} lore_capture;

static bool lore_distance_valid(cptr text, size_t length)
{
    char value[LORE_CAPTURE_BYTES];
    static const char* prefixes[] = {
        "a short way (less than ", "some distance (",
        "a long way (", "a very long way (more than "
    };
    memcpy(value, text, length);
    value[length] = '\0';
    if (!strcmp(value, "somewhere"))
        return true;
    for (size_t i = 0; i < N_ELEMENTS(prefixes); ++i) {
        size_t prefix_length = strlen(prefixes[i]);
        if (strncmp(value, prefixes[i], prefix_length))
            continue;
        cptr at = value + prefix_length;
        if (*at < '0' || *at > '9')
            return false;
        while (*at >= '0' && *at <= '9')
            ++at;
        if (i == 1 || i == 2) {
            if (strncmp(at, " to ", 4))
                return false;
            at += 4;
            if (*at < '0' || *at > '9')
                return false;
            while (*at >= '0' && *at <= '9')
                ++at;
        }
        return !strcmp(at, " squares)");
    }
    return false;
}

static bool lore_capture_valid(cptr name, size_t name_length, cptr text, size_t length)
{
    bool optional_metal = name_length == 7 && !strncmp(name, "MITHRIL", 7);
    if (!length && !optional_metal)
        return false;
    if (length && !optional_metal
        && (text[0] == ' ' || text[length - 1] == ' '))
        return false;
    /* A captured location never includes the preceding note introduction. */
    for (size_t i = 0; i < length; ++i)
        if (text[i] == ':' || text[i] == '\n')
            return false;
    if (name_length == 4 && !strncmp(name, "DIST", 4))
        return lore_distance_valid(text, length);
    if (name_length == 3 && !strncmp(name, "DIR", 3)) {
        static const char* directions[] = {
            "to the north", "to the north-east", "to the east", "to the south-east",
            "to the south", "to the south-west", "to the west", "to the north-west",
            "here", "on this level"
        };
        for (size_t i = 0; i < N_ELEMENTS(directions); ++i)
            if (strlen(directions[i]) == length && !strncmp(text, directions[i], length))
                return true;
        return false;
    }
    return true;
}

/* A small bounded template matcher for saved hints. Whitespace can differ
 * because old archives wrapped text into fixed-width rows. Captured values
 * are copied verbatim; no destination or gameplay information is invented. */
static bool lore_prose_match(cptr pattern, cptr text, lore_capture* captures,
    int* count, int* budget, cptr* end)
{
    while (*pattern && *pattern != '{') {
        if (*pattern == ' ' || *pattern == '\n' || *pattern == '\t') {
            if (*text != ' ' && *text != '\n' && *text != '\t')
                return false;
            while (*pattern == ' ' || *pattern == '\n' || *pattern == '\t')
                ++pattern;
            while (*text == ' ' || *text == '\n' || *text == '\t')
                ++text;
        } else {
            if (*text != *pattern)
                return false;
            ++text;
            ++pattern;
        }
    }
    if (!*pattern) {
        *end = text;
        return true;
    }
    cptr close = strchr(pattern, '}');
    if (!close || *count >= LORE_CAPTURE_MAX)
        return false;
    int slot = *count;
    size_t name_length = (size_t)(close - pattern - 1);
    cptr rest = close + 1;
    size_t remaining = strlen(text);
    size_t maximum = MIN(remaining, LORE_CAPTURE_BYTES - 1);
    for (size_t length = 0; length <= maximum; ++length) {
        if (--*budget < 0)
            return false;
        if (*rest && text[length] != *rest
            && !((*rest == ' ' || *rest == '\n')
                && (text[length] == ' ' || text[length] == '\n')))
            continue;
        if (!*rest && text[length] && text[length] != '\n'
            && text[length] != '.' && text[length] != ';')
            continue;
        if (!lore_capture_valid(pattern + 1, name_length, text, length))
            continue;
        bool repeated = false;
        for (int i = 0; i < slot; ++i) {
            if (captures[i].name_length == name_length
                && !strncmp(captures[i].name, pattern + 1, name_length)
                && (captures[i].length != length
                    || strncmp(captures[i].text, text, length))) {
                repeated = true;
                break;
            }
        }
        if (repeated)
            continue;
        captures[slot] = (lore_capture){ pattern + 1, name_length, text, length };
        *count = slot + 1;
        if (lore_prose_match(rest, text + length, captures, count, budget, end))
            return true;
        *count = slot;
    }
    return false;
}

static void lore_prose_append(char* dest, size_t size, size_t* used,
    cptr text, size_t length)
{
    size_t copy = MIN(length, size - *used - 1);
    memcpy(dest + *used, text, copy);
    *used += copy;
    dest[*used] = '\0';
}

static void lore_prose_expand(char* dest, size_t size, size_t* used,
    cptr target, const lore_capture* captures, int count)
{
    while (*target && *used + 1 < size) {
        cptr close = *target == '{' ? strchr(target, '}') : NULL;
        int slot = -1;
        if (close) {
            size_t name_length = (size_t)(close - target - 1);
            for (int i = 0; i < count; ++i)
                if (captures[i].name_length == name_length
                    && !strncmp(captures[i].name, target + 1, name_length)) {
                    slot = i;
                    break;
                }
        }
        if (slot >= 0) {
            char value[LORE_CAPTURE_BYTES];
            memcpy(value, captures[slot].text, captures[slot].length);
            value[captures[slot].length] = '\0';
            cptr display = lore_text(value);
            lore_prose_append(dest, size, used, display, strlen(display));
            target = close + 1;
        } else {
            lore_prose_append(dest, size, used, target++, 1);
        }
    }
}

void lore_prose_copy(char* dest, size_t size, cptr canonical)
{
    size_t used = 0;
    char* alias = NULL;
    if (!dest || !size)
        return;
    if (!canonical || !catalog || !op_ptr || !op_ptr->opt[OPT_generic]
        || lore_has_description(canonical)) {
        lore_copy(dest, size, canonical);
        return;
    }
    if (dest == canonical) {
        alias = SDL_strdup(canonical);
        if (!alias)
            return;
        canonical = alias;
    }
    dest[0] = '\0';
    for (cptr at = canonical; *at && used + 1 < size;) {
        const lore_rule* match = NULL;
        lore_capture captures[LORE_CAPTURE_MAX];
        int count = 0;
        cptr end = NULL;
        for (int group = 0; group < 2 && !match; ++group) {
            const lore_rule* first = group ? prose_wildcards
                : prose_buckets[(unsigned char)*at];
            if (group && at > canonical && at[-1] != ' '
                && at[-1] != '\n' && at[-1] != ':')
                continue;
            for (const lore_rule* rule = first; rule; rule = rule->prose_next) {
                int budget = 2048;
                count = 0;
                if (at > canonical && lore_word((unsigned char)at[-1])
                    && lore_word((unsigned char)rule->from[0]))
                    continue;
                if (lore_prose_match(rule->from, at, captures, &count, &budget, &end)
                    && end > at) {
                    match = rule;
                    break;
                }
            }
        }
        if (match) {
            lore_prose_expand(dest, size, &used, match->to, captures, count);
            at = end;
        } else {
            const lore_rule* rule = lore_match(canonical, at);
            if (rule) {
                size_t copy = MIN(rule->replacement_length, size - used - 1);
                char* target = SDL_malloc(rule->replacement_length + 1);
                if (!target)
                    break;
                lore_replace(target, at, rule);
                lore_prose_append(dest, size, &used, target, copy);
                SDL_free(target);
                at += rule->length;
            } else {
                lore_prose_append(dest, size, &used, at++, 1);
            }
        }
    }
    SDL_free(alias);
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

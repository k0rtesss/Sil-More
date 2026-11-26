#include "angband.h"
#include "ui/ui-language.h"
#include "cJSON.h"
#include "externs.h"
#include "fs/path.h"
#include "log/log.h"
#include "tutorial/tutorial.h"

typedef struct ui_catalogue_index {
    const cJSON **entries;
    size_t count;
} ui_catalogue_index;

typedef struct ui_catalogue {
    cJSON *document;
    ui_catalogue_index strings;
    ui_catalogue_index messages;
    bool attempted;
} ui_catalogue;

/* Share the tutorial locale list while keeping the two preferences independent.
 * Menus may retain translated labels across a switch, so keep loaded catalogues
 * until shutdown. */
static size_t language_index;
static ui_catalogue *catalogues;

static int compare_entries(const void *a, const void *b)
{
    const cJSON *const *left = a;
    const cJSON *const *right = b;
    return strcmp((*left)->string, (*right)->string);
}

/* Format entries are also passed to C's printf family and the game's
 * formatter. Preserve argument types, widths and order, including %^. */
static const char *skip_position(const char *start)
{
    const char *end = start + strspn(start, "0123456789");
    return end != start && *end == '$' ? end + 1 : start;
}

static size_t format_token_length(const char *start)
{
    const char *end = skip_position(start + 1);
    end += strspn(end, "#0- +'^I");
    if (*end == '*') end = skip_position(end + 1);
    else end += strspn(end, "0123456789");
    if (*end == '.') {
        ++end;
        if (*end == '*') end = skip_position(end + 1);
        else end += strspn(end, "0123456789");
    }
    end += strspn(end, "hljztLqZ");
    if (*end && strchr("diouxXfFeEgGaAcCsSpnrvbB%", *end))
        return (size_t)(end + 1 - start);
    return 1;
}

static bool matching_format(const char *source, const char *translation)
{
    while (true) {
        source = strchr(source, '%');
        translation = strchr(translation, '%');
        if (!source || !translation) return !source && !translation;
        size_t len = format_token_length(source);
        if (len != format_token_length(translation)
            || (len > 1 && source[len - 1] == 'n')
            || strncmp(source, translation, len) != 0)
            return false;
        source += len;
        translation += len;
    }
}

static bool index_catalogue(ui_catalogue_index *index, const cJSON *group,
    const char *code)
{
    if (!cJSON_IsObject(group)) return false;
    size_t capacity = (size_t)cJSON_GetArraySize(group);
    if (!capacity) return true;
    index->entries = calloc(capacity, sizeof(*index->entries));
    if (!index->entries) return false;

    size_t entry_count = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, group)
        index->entries[entry_count++] = entry;
    qsort(index->entries, entry_count, sizeof(*index->entries), compare_entries);
    const cJSON *previous = NULL;
    for (size_t i = 0; i < entry_count; ++i) {
        entry = index->entries[i];
        if (previous && streq(previous->string, entry->string)) return false;
        previous = entry;
        if (cJSON_IsString(entry) && entry->string[0] && entry->valuestring[0]
            && matching_format(entry->string, entry->valuestring))
            index->entries[index->count++] = entry;
        else
            log_warn("Ignoring invalid UI translation for '%s' in locale '%s'",
                entry->string, code);
    }
    return true;
}

static void clear_catalogue(ui_catalogue *catalogue)
{
    free(catalogue->strings.entries);
    free(catalogue->messages.entries);
    catalogue->strings = (ui_catalogue_index){0};
    catalogue->messages = (ui_catalogue_index){0};
    cJSON_Delete(catalogue->document);
    catalogue->document = NULL;
}

static const ui_catalogue *load_catalogue(void)
{
    if (!language_index) return NULL;
    if (catalogues && catalogues[language_index].attempted)
        return &catalogues[language_index];
    if (!ANGBAND_DIR_HELP || !*ANGBAND_DIR_HELP) return NULL;

    size_t count;
    const tutorial_locale *locales = tutorial_available_locales(&count);
    const char *code = locales[language_index].code;
    if (!catalogues) {
        catalogues = calloc(count, sizeof(*catalogues));
        if (!catalogues) return NULL;
    }

    ui_catalogue *catalogue = &catalogues[language_index];
    catalogue->attempted = true;
    char filename[32];
    char path[1024];
    size_t len = 0;
    strnfmt(filename, sizeof(filename), "ui.%s.json", code);
    if (!path_build(path, sizeof(path), ANGBAND_DIR_HELP, filename)) return NULL;
    char *json = SDL_LoadFile(path, &len);
    if (json && len <= 2 * 1024 * 1024)
        catalogue->document = cJSON_ParseWithLengthOpts(json, len + 1, NULL, true);
    SDL_free(json);

    const cJSON *document = catalogue->document;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(document, "version");
    const cJSON *language = cJSON_GetObjectItemCaseSensitive(document, "language");
    if (!cJSON_IsNumber(version) || version->valuedouble != 1
        || (language && (!cJSON_IsString(language) || !streq(language->valuestring, code)))
        || !index_catalogue(&catalogue->strings,
            cJSON_GetObjectItemCaseSensitive(document, "strings"), code)
        || !index_catalogue(&catalogue->messages,
            cJSON_GetObjectItemCaseSensitive(document, "messages"), code)) {
        clear_catalogue(catalogue);
        log_warn("Could not load UI translations for locale '%s' from '%s'", code, path);
    }
    return catalogue;
}

static const char *find_text(const ui_catalogue_index *index, const char *english)
{
    size_t first = 0;
    size_t last = index->count;
    while (first < last) {
        size_t mid = first + (last - first) / 2;
        int order = strcmp(index->entries[mid]->string, english);
        if (order == 0) return index->entries[mid]->valuestring;
        if (order < 0) first = mid + 1;
        else last = mid;
    }
    return english;
}

const char *ui_language(void)
{
    size_t count;
    return tutorial_available_locales(&count)[language_index].code;
}

void ui_set_language(const char *code)
{
    size_t count;
    const tutorial_locale *locales = tutorial_available_locales(&count);
    size_t next = 0;
    for (size_t i = 1; code && i < count; ++i) {
        if (streq(code, locales[i].code)) {
            next = i;
            break;
        }
    }
    if (next == language_index) return;
    language_index = next;
#ifdef USE_SDL
    sdl_ui_language_changed();
#endif
}

const char *ui_text(const char *english)
{
    if (!english || !*english) return english;
    const ui_catalogue *catalogue = load_catalogue();
    if (!catalogue) return english;
    const char *text = find_text(&catalogue->strings, english);
    return text == english ? find_text(&catalogue->messages, english) : text;
}

const char *ui_message_text(const char *english)
{
    if (!english || !*english) return english;
    const ui_catalogue *catalogue = load_catalogue();
    return catalogue ? find_text(&catalogue->messages, english) : english;
}

void ui_language_cleanup(void)
{
    if (!catalogues) return;
    size_t count;
    tutorial_available_locales(&count);
    for (size_t i = 1; i < count; ++i) clear_catalogue(&catalogues[i]);
    free(catalogues);
    catalogues = NULL;
}

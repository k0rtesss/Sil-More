#include "angband.h"
#include "externs.h"
#include "fs/path.h"
#include "fs/io_sdl.h"
#include "cJSON.h"
#include <assert.h>

/* The check script extracts these helpers directly from sdl-config.c. */
#include "app-options-under-test.inc"

static player_other options;
player_other* op_ptr = &options;
cptr ANGBAND_DIR_PREF;

bool sdl_touch_only_device_active(void) { return false; }
void log_log(int level, const char* file, int line, const char* fmt, ...)
{ (void)level; (void)file; (void)line; (void)fmt; }
bool path_build(char* buf, size_t max, cptr path, cptr file)
{ return SDL_snprintf(buf, max, "%s/%s", path, file) < (int)max; }
SDL_IOStream* sdl_fopen(cptr path, cptr mode)
{ return SDL_IOFromFile(path, mode); }

static void expand_test_tokens(char* out, size_t size, cptr text, bool real)
{
    out[0] = '\0';
    while (*text) {
        cptr close = *text == '{' ? strchr(text, '}') : NULL;
        if (close) {
            char name[64], value[96];
            size_t length = (size_t)(close - text - 1);
            assert(length < sizeof(name));
            memcpy(name, text + 1, length);
            name[length] = '\0';
            if (!strcmp(name, "DIR"))
                SDL_strlcpy(value, real ? "to the north-east" : "to the east", sizeof(value));
            else if (!strcmp(name, "DIST"))
                SDL_strlcpy(value, real ? "a long way (25 to 48 squares)" : "somewhere", sizeof(value));
            else if (!strcmp(name, "MITHRIL"))
                SDL_strlcpy(value, real ? " or mithril" : "", sizeof(value));
            else if (real && !strcmp(name, "ART_CAP"))
                SDL_strlcpy(value, "A suit of armour", sizeof(value));
            else if (real && (!strcmp(name, "SITE") || !strcmp(name, "SITE_CAP")))
                SDL_strlcpy(value, "a dragon-lord's hoard", sizeof(value));
            else if (real && !strcmp(name, "PART"))
                SDL_strlcpy(value, "a winding labyrinth", sizeof(value));
            else
                SDL_snprintf(value, sizeof(value), "value_%s_marker", name);
            SDL_strlcat(out, value, size);
            text = close + 1;
        } else {
            size_t length = strlen(out);
            assert(length + 2 < size);
            out[length] = *text++;
            out[length + 1] = '\0';
        }
    }
}

static void check_catalog(cptr path)
{
    size_t size;
    char* json = SDL_LoadFile(path, &size);
    assert(json);
    cJSON* root = cJSON_ParseWithLength(json, size);
    SDL_free(json);
    assert(root);
    int checked = 0;
    int templates = 0;
    const char* groups[] = { "terms", "passages", "descriptions" };
    for (int group = 0; group < 3; ++group) {
        cJSON* pair;
        cJSON_ArrayForEach(pair, cJSON_GetObjectItemCaseSensitive(root, groups[group])) {
            cptr source = cJSON_GetArrayItem(pair, 0)->valuestring;
            cptr target = cJSON_GetArrayItem(pair, 1)->valuestring;
            assert(lore_set_generic(false));
            assert(lore_text(source) == source);
            assert(lore_set_generic(true));
            cptr translated = lore_text(source);
            assert(strcmp(translated, source) != 0);
            if (group && strcmp(translated, target) != 0) {
                fprintf(stderr, "Passage mismatch: %.100s\n", source);
                abort();
            }
            cptr twice = lore_text(translated);
            if (strcmp(translated, twice) != 0) {
                fprintf(stderr, "Non-idempotent catalog: %s -> %s -> %s\n",
                    source, translated, twice);
                abort();
            }
            if (group == 2 && strchr(source, '{')) {
                for (int real = 0; real < 2; ++real) {
                    char canonical[8192], expected[8192], actual[8192];
                    char raw[4096], generic[4096];
                    expand_test_tokens(raw, sizeof(raw), source, real != 0);
                    expand_test_tokens(generic, sizeof(generic), target, real != 0);
                    SDL_snprintf(canonical, sizeof(canonical), "Archived warning: %s End of note.", raw);
                    SDL_snprintf(expected, sizeof(expected), "Archived warning: %s End of note.", generic);
                    lore_copy(expected, sizeof(expected), expected);
                    lore_prose_copy(actual, sizeof(actual), canonical);
                    if (strcmp(expected, actual)) {
                        fprintf(stderr, "Archived template mismatch:\n%s\nExpected: %s\nActual: %s\n", source, expected, actual);
                        abort();
                    }
                    assert(lore_set_generic(false));
                    lore_prose_copy(actual, sizeof(actual), canonical);
                    assert(!strcmp(actual, canonical));
                    assert(lore_set_generic(true));
                    SDL_strlcpy(actual, canonical, sizeof(actual));
                    lore_prose_copy(actual, sizeof(actual), actual);
                    assert(!strcmp(expected, actual));
                }
                ++templates;
            }
            ++checked;
        }
    }
    cJSON_Delete(root);
    printf("Catalog: %d reversible, idempotent substitutions PASS\n", checked);
    printf("Archived notes: %d templates, multiword values, aliases and off-mode roundtrip PASS\n", templates);
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    ANGBAND_DIR_PREF = argv[1];
    assert(option_norm[OPT_generic] == false);
    assert(strcmp(option_text[OPT_generic], "generic") == 0);
    assert(option_is_app_persistent(OPT_generic));

    cJSON* config = cJSON_Parse("{\"other\":{\"generic\":true}}");
    sdl_config_load_app_option_group(config, "other", app_other_options);
    assert(op_ptr->opt[OPT_generic]);
    cJSON_Delete(config);
    config = cJSON_CreateObject();
    sdl_config_save_app_option_group(config, "other", app_other_options);
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(config, "other"), "generic")));
    cJSON_Delete(config);
    config = cJSON_Parse("{\"other\":{\"generic\":\"true\"}}");
    sdl_config_load_app_option_group(config, "other", app_other_options);
    assert(!op_ptr->opt[OPT_generic]);
    cJSON_Delete(config);
    config = cJSON_CreateObject();
    op_ptr->opt[OPT_generic] = true;
    sdl_config_load_app_option_group(config, "other", app_other_options);
    assert(!op_ptr->opt[OPT_generic]);
    cJSON_Delete(config);
    puts("App setting: JSON round trip, missing/invalid defaults PASS");

    assert(lore_init());
    char canonical[] = "Morgoth guards a Silmaril in Angband.";
    char before[sizeof(canonical)];
    memcpy(before, canonical, sizeof(before));
    assert(lore_text(canonical) == canonical);
    assert(lore_set_generic(true));
    cptr themed = lore_text(canonical);
    assert(strcmp(themed, canonical) != 0);
    assert(!strstr(themed, "Morgoth") && !strstr(themed, "Silmaril")
        && !strstr(themed, "Angband"));
    assert(strcmp(lore_text("Morgothian_Angband2"), "Morgothian_Angband2") == 0);
    assert(lore_text(NULL) == NULL);
    char small[5] = "xxxx";
    lore_copy(small, sizeof(small), canonical);
    assert(small[4] == '\0');
    char alias[256];
    SDL_strlcpy(alias, canonical, sizeof(alias));
    lore_copy(alias, sizeof(alias), alias);
    assert(strcmp(alias, themed) == 0);
    assert(memcmp(canonical, before, sizeof(before)) == 0);
    assert(lore_set_generic(false));
    assert(lore_text(canonical) == canonical);
    assert(lore_set_generic(true));
    assert(strcmp(lore_text(canonical), themed) == 0);
    puts("Presentation: immediate toggle, canonical storage, bounds, aliases PASS");

    char dump[2048] = { 0 };
    SDL_IOStream* stream = SDL_IOFromMem(dump, sizeof(dump));
    assert(stream);
    lore_dump_printf(stream, "%s\n", canonical);
    SDL_CloseIO(stream);
    assert(strstr(dump, themed) == dump && strstr(dump, "Morgoth") == NULL);
    puts("Character export: generic prose, canonical input PASS");
    assert(!strcmp(lore_text("They resist fear; a sting hurts in this region."),
        "They resist fear; a sting hurts in this region."));
    assert(!strcmp(lore_text("{MITHRIL}"), "{MITHRIL}"));
    assert(!strcmp(lore_text("Sting"), "Needlewake"));
    assert(!strcmp(lore_text("A sting causes pain."), "A sting causes pain."));
    check_catalog(argv[2]);
    for (int i = 0; i < 10000; ++i) {
        char dynamic[1024];
        SDL_snprintf(dynamic, sizeof(dynamic), "Morgoth strikes for %d damage. %0900d", i, i);
        cptr rendered = lore_text(dynamic);
        assert(!strstr(rendered, "Morgoth"));
        assert(strstr(rendered, " damage."));
    }
    puts("Long metarun: 10000 distinct translated messages through bounded cache PASS");
    lore_cleanup();
    assert(lore_init());
    lore_cleanup();
    ANGBAND_DIR_PREF = "generic-lore-test-missing-directory";
    assert(lore_set_generic(false));
    assert(!lore_set_generic(true));
    assert(!op_ptr->opt[OPT_generic]);
    puts("Catalog lifecycle and missing-data rejection PASS");
    return 0;
}

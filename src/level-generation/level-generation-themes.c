#include "angband.h"
#include "externs.h"
#include "fs/path.h"
#include "log/log.h"
#include "level-generation-themes.h"

#define TERRAIN_THEME_DEPTHS 22
#define TERRAIN_THEME_LINE_MAX 512
#define TERRAIN_THEME_FILE_MAX 65536

/* Keep these fallback profiles identical to lib/edit/dungeon-themes.txt.
 * The parser harness compares every field of all shipped records. */
static const terrain_theme_profile terrain_theme_defaults[TERRAIN_THEME_DEPTHS] = {
    { "Upper halls",          {1,0,0,0,0}, 20, 6, 8,1,15 },
    { "Upper halls",          {1,0,0,0,0}, 30, 6,10,1,15 },
    { "Shallow stone",        {9,1,0,0,0}, 12, 6,10,1,10 },
    { "Shallow stone",        {8,2,0,0,0}, 20, 6,12,1,15 },
    { "Green caves",          {9,1,0,0,0}, 45, 8,18,2,35 },
    { "Green caves",          {9,1,0,0,0}, 55,10,22,2,40 },
    { "Green caves",          {8,2,0,0,0}, 55,10,24,2,40 },
    { "Shadowed stone",       {5,5,0,0,0}, 45, 8,22,2,25 },
    { "Shadowed stone",       {4,6,0,0,0}, 50,10,24,2,25 },
    { "Shadowed stone",       {4,6,0,0,0}, 50,10,26,2,25 },
    { "Deep shadow",          {3,6,0,1,0}, 50,10,26,2,25 },
    { "Rusty metallic halls", {2,2,0,6,0}, 55,10,26,2,40 },
    { "Rusty metallic halls", {2,2,1,7,0}, 60,10,28,2,40 },
    { "Rusty metallic halls", {2,2,2,8,0}, 60,12,28,2,40 },
    { "Deep forges",          {2,3,6,4,0}, 55,10,28,2,30 },
    { "Deep forges",          {2,3,8,4,0}, 60,12,30,3,30 },
    { "Deep forges",          {2,3,9,4,0}, 60,12,32,3,30 },
    { "Fiery depths",         {2,3,12,4,0},65,12,32,3,35 },
    { "Fiery depths",         {2,3,14,4,0},65,12,34,3,35 },
    { "Fiery depths",         {2,3,14,4,0},65,12,34,3,35 },
    { "Unused depth",         {0,0,0,0,0},0,6,6,1,0 },
    { "Utumno corridors",     {1,0,1,0,1},75,12,34,3,35 }
};
static const terrain_theme_profile terrain_theme_disabled = {
    "Outside dungeon", {0,0,0,0,0}, 0,6,6,1,0
};
static const terrain_landmark_profile terrain_landmark_defaults[TERRAIN_THEME_DEPTHS] = {
    {100,50,75,1,2,30,20}, {100,50,75,1,2,30,20},
    {100,50,80,1,2,35,25}, {100,50,80,1,2,35,25},
    {100,50,80,1,2,35,25}, {100,50,80,1,2,35,25},
    {100,50,80,1,2,35,25},
    {100,55,90,1,2,40,30}, {100,55,90,1,2,40,30},
    {100,55,90,1,2,40,30}, {100,55,90,1,2,40,30},
    {100,55,90,1,2,40,30}, {100,55,90,1,2,40,30},
    {100,55,90,1,2,40,30}, {100,55,90,1,2,40,30},
    {100,55,90,1,2,40,30}, {100,55,90,1,2,40,30},
    {100,55,90,1,2,40,30}, {100,55,90,1,2,40,30},
    {100,55,90,1,2,40,30},
    {0,50,50,1,1,0,0}, {100,40,70,1,2,40,30}
};
static const terrain_landmark_profile terrain_landmark_disabled = {
    0,50,50,1,1,0,0
};
static const terrain_network_profile terrain_network_defaults[TERRAIN_THEME_DEPTHS] = {
    {{40,30,15,5,10},2,4,3,6,70}, {{40,30,15,5,10},2,4,3,6,70},
    {{40,30,15,5,10},2,4,3,6,70}, {{40,30,15,5,10},2,4,3,6,70},
    {{40,30,15,5,10},2,4,3,8,70}, {{40,30,15,5,10},2,4,3,8,70},
    {{40,30,15,5,10},2,4,3,8,70}, {{40,30,15,5,10},2,4,3,8,70},
    {{40,30,15,5,10},2,4,3,8,70}, {{40,30,15,5,10},2,4,3,8,70},
    {{40,30,15,5,10},2,4,3,8,70}, {{40,30,15,5,10},2,4,3,8,70},
    {{40,30,15,5,10},2,4,3,8,70}, {{40,30,15,5,10},2,4,3,8,70},
    {{35,10,25,20,10},2,4,3,8,70}, {{35,10,25,20,10},2,4,3,8,70},
    {{35,10,25,20,10},2,4,3,8,70}, {{35,10,25,20,10},2,4,3,8,70},
    {{35,10,25,20,10},2,4,3,8,70}, {{35,10,25,20,10},2,4,3,8,70},
    {{0,0,0,0,0},1,1,2,2,0}, {{25,25,0,0,50},2,4,3,8,70}
};
static const terrain_network_profile terrain_network_disabled = {
    {0,0,0,0,0},1,1,2,2,0
};
static terrain_theme_profile terrain_theme_profiles[TERRAIN_THEME_DEPTHS];
static terrain_landmark_profile terrain_landmark_profiles[TERRAIN_THEME_DEPTHS];
static terrain_network_profile terrain_network_profiles[TERRAIN_THEME_DEPTHS];
static bool terrain_themes_attempted;
static bool terrain_themes_loaded;
static const terrain_history_profile terrain_history_default = {45, 30, 25, 35};
static const terrain_history_profile terrain_history_utumno = {100, 0, 0, 100};
static const terrain_history_profile terrain_history_disabled = {0, 0, 0, 0};
static terrain_history_profile terrain_history_profiles[TERRAIN_THEME_DEPTHS];

/* Parse unsigned decimal only: no signs, suffixes, overflow or empty fields. */
static bool terrain_theme_number(const char* text, int low, int high, int* out)
{
    int value = 0;
    if (!*text) return false;
    for (; *text; text++)
    {
        if (*text < '0' || *text > '9') return false;
        int digit = *text - '0';
        if (value > (high - digit) / 10) return false;
        value = value * 10 + digit;
        if (value > high) return false;
    }
    if (value < low) return false;
    *out = value;
    return true;
}

static bool terrain_theme_error(const char* path, int line, const char* reason)
{
    log_warn("Dungeon themes %s:%d: %s; using built-in defaults", path, line, reason);
    return false;
}

/* Commit only after the complete file has passed validation. */
static bool terrain_themes_parse(const char* text, size_t size, const char* path)
{
    terrain_theme_profile pending[TERRAIN_THEME_DEPTHS] = {0};
    memcpy(pending, terrain_theme_defaults, sizeof(pending));
    terrain_landmark_profile landmarks[TERRAIN_THEME_DEPTHS];
    memcpy(landmarks, terrain_landmark_defaults, sizeof(landmarks));
    terrain_network_profile networks[TERRAIN_THEME_DEPTHS];
    memcpy(networks, terrain_network_defaults, sizeof(networks));
    bool seen[TERRAIN_THEME_DEPTHS] = {false};
    bool landmark_seen[TERRAIN_THEME_DEPTHS] = {false};
    bool network_seen[TERRAIN_THEME_DEPTHS] = {false};
    bool history_seen[TERRAIN_THEME_DEPTHS] = {false};
    terrain_history_profile histories[TERRAIN_THEME_DEPTHS];
    for (int d = 0; d < TERRAIN_THEME_DEPTHS; d++) histories[d] = terrain_history_default;
    histories[UTUMNO_DEPTH - 1] = terrain_history_utumno;
    int version = 0;
    size_t offset = 0;
    int line_number = 0;

    if (size > TERRAIN_THEME_FILE_MAX)
        return terrain_theme_error(path, 1, "file exceeds 64 KiB");
    if (memchr(text, '\0', size))
        return terrain_theme_error(path, 1, "embedded NUL byte");
    /* Editors on Windows may write an UTF-8 BOM. */
    if (size >= 3 && (unsigned char)text[0] == 0xef
        && (unsigned char)text[1] == 0xbb && (unsigned char)text[2] == 0xbf)
        offset = 3;

    while (offset < size)
    {
        char line[TERRAIN_THEME_LINE_MAX];
        size_t start = offset;
        while (offset < size && text[offset] != '\n') offset++;
        size_t length = offset - start;
        if (offset < size) offset++;
        line_number++;
        if (length && text[start + length - 1] == '\r') length--;
        if (length >= sizeof(line))
            return terrain_theme_error(path, line_number, "line exceeds 511 bytes");
        memcpy(line, text + start, length);
        line[length] = '\0';
        char* record = line;
        while (*record == ' ' || *record == '\t') record++;
        if (!*record || *record == '#') continue;
        if ((!strcmp(record, "V:1") || !strcmp(record, "V:2")
                || !strcmp(record, "V:3")) && !version)
        {
            version = record[2] - '0';
            continue;
        }
        if (!version)
            return terrain_theme_error(path, line_number, "expected schema header V:1, V:2 or V:3");

        char* fields[13];
        int field_count = 1;
        fields[0] = record;
        for (char* cursor = record; *cursor; cursor++)
        {
            if (*cursor != ':') continue;
            if (field_count == 13)
                return terrain_theme_error(path, line_number, "too many fields");
            *cursor = '\0';
            fields[field_count++] = cursor + 1;
        }
        bool landmark_record = !strcmp(fields[0], "M");
        bool network_record = !strcmp(fields[0], "N");
        bool history_record = !strcmp(fields[0], "H");
        if (history_record ? (version != 3 || field_count != 6)
            : network_record ? (version != 3 || field_count != 12)
            : landmark_record ? (version < 2 || field_count != 9)
            : (strcmp(fields[0], "D") || field_count != 13))
            return terrain_theme_error(path, line_number,
                "expected D with 12 values, M with 8 values in V:2/V:3, or N with 11 values in V:3");
        int depth;
        if (!terrain_theme_number(fields[1], 1, TERRAIN_THEME_DEPTHS, &depth))
            return terrain_theme_error(path, line_number, "depth must be 1..22");
        if (history_record)
        {
            terrain_history_profile* history = &histories[depth - 1];
            if (history_seen[depth - 1]
                || !terrain_theme_number(fields[2], 0, 1000, &history->ancient)
                || !terrain_theme_number(fields[3], 0, 1000, &history->disaster)
                || !terrain_theme_number(fields[4], 0, 1000, &history->overflow)
                || !terrain_theme_number(fields[5], 0, 100, &history->second_system_chance)
                || !(history->ancient + history->disaster + history->overflow))
                return terrain_theme_error(path, line_number, "invalid or duplicate history profile");
            history_seen[depth - 1] = true;
            continue;
        }
        if (network_record)
        {
            if (network_seen[depth - 1])
                return terrain_theme_error(path, line_number, "duplicate network depth");
            terrain_network_profile* network = &networks[depth - 1];
            for (int family = 0; family < TERRAIN_NETWORK_FAMILY_MAX; family++)
                if (!terrain_theme_number(fields[2 + family], 0, 1000,
                        &network->weights[family]))
                    return terrain_theme_error(path, line_number, "network weights must be 0..1000");
            if (!terrain_theme_number(fields[7], 1, 6, &network->min_basins)
                || !terrain_theme_number(fields[8], 1, 6, &network->max_basins)
                || !terrain_theme_number(fields[9], 2, 12, &network->min_radius)
                || !terrain_theme_number(fields[10], 2, 12, &network->max_radius)
                || !terrain_theme_number(fields[11], 0, 100, &network->one_tile_percent))
                return terrain_theme_error(path, line_number, "invalid network basin, radius or narrow-channel bounds");
            if (network->min_basins > network->max_basins
                || network->min_radius > network->max_radius)
                return terrain_theme_error(path, line_number, "inverted network basin or radius bounds");
            network_seen[depth - 1] = true;
            continue;
        }
        if (landmark_record)
        {
            if (landmark_seen[depth - 1])
                return terrain_theme_error(path, line_number, "duplicate landmark depth");
            terrain_landmark_profile* landmark = &landmarks[depth - 1];
            if (!terrain_theme_number(fields[2], 0, 100, &landmark->chance)
                || !terrain_theme_number(fields[3], 35, 95, &landmark->min_span_percent)
                || !terrain_theme_number(fields[4], 35, 95, &landmark->max_span_percent)
                || !terrain_theme_number(fields[5], 1, 12, &landmark->min_width)
                || !terrain_theme_number(fields[6], 1, 12, &landmark->max_width)
                || !terrain_theme_number(fields[7], 0, 100, &landmark->lake_chance)
                || !terrain_theme_number(fields[8], 0, 100, &landmark->branch_chance))
                return terrain_theme_error(path, line_number, "invalid landmark chance or shape bounds");
            if (landmark->min_span_percent > landmark->max_span_percent
                || landmark->min_width > landmark->max_width)
                return terrain_theme_error(path, line_number, "inverted landmark span or width bounds");
            landmark_seen[depth - 1] = true;
            continue;
        }
        if (seen[depth - 1])
            return terrain_theme_error(path, line_number, "duplicate depth");
        terrain_theme_profile* profile = &pending[depth - 1];
        size_t name_length = strlen(fields[2]);
        if (!name_length || name_length >= sizeof(profile->name))
            return terrain_theme_error(path, line_number, "name must be 1..47 characters");
        bool name_has_word = false;
        for (const char* cursor = fields[2]; *cursor; cursor++)
        {
            bool word = (*cursor >= 'a' && *cursor <= 'z')
                || (*cursor >= 'A' && *cursor <= 'Z')
                || (*cursor >= '0' && *cursor <= '9');
            if (!word && *cursor != ' ' && *cursor != '_' && *cursor != '-')
                return terrain_theme_error(path, line_number, "invalid name character");
            name_has_word |= word;
        }
        if (!name_has_word)
            return terrain_theme_error(path, line_number, "name needs a letter or digit");
        memcpy(profile->name, fields[2], name_length + 1);
        int total = 0;
        for (int material = 0; material < TERRAIN_THEME_MATERIAL_MAX; material++)
        {
            if (!terrain_theme_number(fields[3 + material], 0, 1000,
                    &profile->weights[material]))
                return terrain_theme_error(path, line_number, "material weights must be 0..1000");
            total += profile->weights[material];
        }
        if (!terrain_theme_number(fields[8], 0, 100, &profile->chance)
            || !terrain_theme_number(fields[9], 6, 48, &profile->min_length)
            || !terrain_theme_number(fields[10], 6, 48, &profile->max_length)
            || !terrain_theme_number(fields[11], 1, 3, &profile->max_width)
            || !terrain_theme_number(fields[12], 0, 100, &profile->pool_chance))
            return terrain_theme_error(path, line_number, "invalid chance or shape bounds");
        if (profile->min_length > profile->max_length)
            return terrain_theme_error(path, line_number, "min_length exceeds max_length");
        if (profile->chance && !total)
            return terrain_theme_error(path, line_number, "enabled profile has no material weight");
        seen[depth - 1] = true;
    }
    if (!version)
        return terrain_theme_error(path, line_number + 1, "missing schema header V:1, V:2 or V:3");
    for (int depth = 0; depth < TERRAIN_THEME_DEPTHS; depth++)
    {
        if (depth < MORGOTH_DEPTH && !seen[depth])
            return terrain_theme_error(path, line_number + 1, "missing depth record (all 1..20 required)");
        if (depth < MORGOTH_DEPTH && version >= 2 && !landmark_seen[depth])
            return terrain_theme_error(path, line_number + 1, "missing landmark record (all 1..20 required)");
        if (depth < MORGOTH_DEPTH && version == 3 && !network_seen[depth])
            return terrain_theme_error(path, line_number + 1, "missing network record (all 1..20 required)");
        int total = 0;
        for (int material = 0; material < TERRAIN_THEME_MATERIAL_MAX; material++)
            total += pending[depth].weights[material];
        if (version >= 2 && landmarks[depth].chance && !total)
            return terrain_theme_error(path, line_number + 1, "enabled landmark has no material weight");
        total = 0;
        for (int family = 0; family < TERRAIN_NETWORK_FAMILY_MAX; family++)
            total += networks[depth].weights[family];
        if (landmarks[depth].chance && !total)
            return terrain_theme_error(path, line_number + 1, "enabled network has no family weight");
    }
    /* This route requires all three networks before accepting its map. Reject
     * an impossible configured recipe here instead of retrying generation
     * forever; the loader will retain the complete built-in defaults. */
    const terrain_theme_profile* utumno = &pending[UTUMNO_DEPTH - 1];
    if (utumno->weights[TERRAIN_THEME_WATER] <= 0
        || utumno->weights[TERRAIN_THEME_ICE] <= 0
        || utumno->weights[TERRAIN_THEME_LAVA] <= 0
        || utumno->weights[TERRAIN_THEME_CHASM] != 0
        || utumno->weights[TERRAIN_THEME_POISON] != 0
        || landmarks[UTUMNO_DEPTH - 1].chance <= 0)
    {
        return terrain_theme_error(path, line_number + 1,
            "Utumno depth 22 requires enabled water, ice and lava networks, with no chasm or poison weight");
    }
    memcpy(terrain_theme_profiles, pending, sizeof(pending));
    memcpy(terrain_landmark_profiles, landmarks, sizeof(landmarks));
    memcpy(terrain_network_profiles, networks, sizeof(networks));
    memcpy(terrain_history_profiles, histories, sizeof(histories));
    return true;
}

bool terrain_themes_load(void)
{
    char path[1024];
    size_t size = 0;
    if (terrain_themes_attempted) return terrain_themes_loaded;
    terrain_themes_attempted = true;
    if (!ANGBAND_DIR_EDIT || !path_build(path, sizeof(path), ANGBAND_DIR_EDIT,
            "dungeon-themes.txt"))
        return terrain_theme_error("dungeon-themes.txt", 0, "cannot resolve edit path");
    char* text = SDL_LoadFile(path, &size);
    if (!text)
        return terrain_theme_error(path, 0, "cannot read file");
    terrain_themes_loaded = terrain_themes_parse(text, size, path);
    SDL_free(text);
    if (terrain_themes_loaded)
        log_info("Dungeon themes: loaded all %d depths from %s", TERRAIN_THEME_DEPTHS, path);
    return terrain_themes_loaded;
}

const terrain_theme_profile* terrain_theme_for_depth(int depth)
{
    if (depth < 1 || depth > TERRAIN_THEME_DEPTHS) return &terrain_theme_disabled;
    return terrain_themes_loaded ? &terrain_theme_profiles[depth - 1]
        : &terrain_theme_defaults[depth - 1];
}

const terrain_landmark_profile* terrain_landmark_for_depth(int depth)
{
    if (depth < 1 || depth > TERRAIN_THEME_DEPTHS) return &terrain_landmark_disabled;
    return terrain_themes_loaded ? &terrain_landmark_profiles[depth - 1]
        : &terrain_landmark_defaults[depth - 1];
}

const terrain_network_profile* terrain_network_for_depth(int depth)
{
    if (depth < 1 || depth > TERRAIN_THEME_DEPTHS) return &terrain_network_disabled;
    return terrain_themes_loaded ? &terrain_network_profiles[depth - 1]
        : &terrain_network_defaults[depth - 1];
}

const terrain_history_profile* terrain_history_for_depth(int depth)
{
    if (depth < 1 || depth > TERRAIN_THEME_DEPTHS) return &terrain_history_disabled;
    if (depth == UTUMNO_DEPTH && !terrain_themes_loaded) return &terrain_history_utumno;
    return terrain_themes_loaded ? &terrain_history_profiles[depth - 1] : &terrain_history_default;
}

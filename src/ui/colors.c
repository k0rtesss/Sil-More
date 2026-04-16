/* ui/colors.c - Color name and attribute utilities */

#include "../angband.h"
#include "colors.h"
#include "cJSON.h"
#include "fs/file.h"
#include "fs/resource.h"
#include "log/log.h"

#include <string.h>

int use_graphics = GRAPHICS_NONE;
bool use_bigtile = false;
bool use_transparency = false;

byte angband_color_table[256][4] = {
    { 0x00, 0x00, 0x00, 0x00 }, /* TERM_DARK */
    { 0x00, 0xFF, 0xFF, 0xFF }, /* TERM_WHITE */
    { 0x00, 0x80, 0x80, 0x80 }, /* TERM_SLATE */
    { 0x00, 0xFF, 0x80, 0x00 }, /* TERM_ORANGE */
    { 0x00, 0xC0, 0x00, 0x00 }, /* TERM_RED */
    { 0x00, 0x00, 0x80, 0x40 }, /* TERM_GREEN */
    { 0x00, 0x00, 0x40, 0xFF }, /* TERM_BLUE */
    { 0x00, 0x80, 0x40, 0x00 }, /* TERM_UMBER */
    { 0x00, 0x50, 0x50, 0x50 }, /* TERM_L_DARK */
    { 0x00, 0xC0, 0xC0, 0xC0 }, /* TERM_L_WHITE */
    { 0x00, 0xA0, 0x00, 0xFF }, /* TERM_VIOLET */
    { 0x00, 0xFF, 0xFF, 0x00 }, /* TERM_YELLOW */
    { 0x00, 0xFF, 0x60, 0x60 }, /* TERM_L_RED */
    { 0x00, 0x00, 0xFF, 0x00 }, /* TERM_L_GREEN */
    { 0x00, 0x00, 0xFF, 0xFF }, /* TERM_L_BLUE */
    { 0x00, 0xC0, 0x80, 0x40 }, /* TERM_L_UMBER */

    { 0x00, 0x30, 0x30, 0x30 }, /* TERM_DARK1 */
    { 0x00, 0xC0, 0xC0, 0xC0 }, /* TERM_WHITE1 */
    { 0x00, 0xA0, 0xA0, 0xA0 }, /* TERM_SLATE1 */
    { 0x00, 0xDC, 0x64, 0x00 }, /* TERM_ORANGE1 */
    { 0x00, 0xF0, 0x00, 0x00 }, /* TERM_RED1 */
    { 0x00, 0x00, 0x70, 0x00 }, /* TERM_GREEN1 */
    { 0x00, 0x00, 0x80, 0xFF }, /* TERM_BLUE1 */
    { 0x00, 0xC8, 0x64, 0x00 }, /* TERM_UMBER1 */
    { 0x00, 0x78, 0x64, 0x64 }, /* TERM_L_DARK1 */
    { 0x00, 0xE8, 0xD0, 0xC0 }, /* TERM_L_WHITE1 */
    { 0x00, 0x60, 0x00, 0xFF }, /* TERM_VIOLET1 */
    { 0x00, 0xC8, 0xC8, 0x00 }, /* TERM_YELLOW1 */
    { 0x00, 0xB4, 0x46, 0x32 }, /* TERM_L_RED1 */
    { 0x00, 0x00, 0xDC, 0x64 }, /* TERM_L_GREEN1 */
    { 0x00, 0x64, 0xAA, 0xC8 }, /* TERM_L_BLUE1 */
    { 0x00, 0xC8, 0xAA, 0x46 }, /* TERM_L_UMBER1 */

    { 0x00, 0x18, 0x18, 0x18 }, /* TERM_DARK2 */
    { 0x00, 0x80, 0x80, 0x80 }, /* TERM_WHITE2 */
    { 0x00, 0x60, 0x60, 0x60 }, /* TERM_SLATE2 */
    { 0x00, 0xB8, 0x50, 0x00 }, /* TERM_ORANGE2 */
    { 0x00, 0x90, 0x00, 0x00 }, /* TERM_RED2 */
    { 0x00, 0x00, 0x50, 0x20 }, /* TERM_GREEN2 */
    { 0x00, 0x00, 0x30, 0xA0 }, /* TERM_BLUE2 */
    { 0x00, 0x60, 0x30, 0x00 }, /* TERM_UMBER2 */
    { 0x00, 0x38, 0x38, 0x38 }, /* TERM_L_DARK2 */
    { 0x00, 0x90, 0x90, 0x90 }, /* TERM_L_WHITE2 */
    { 0x00, 0x70, 0x00, 0xC0 }, /* TERM_VIOLET2 */
    { 0x00, 0xB8, 0xB8, 0x00 }, /* TERM_YELLOW2 */
    { 0x00, 0xC0, 0x40, 0x20 }, /* TERM_L_RED2 */
    { 0x00, 0x00, 0xB8, 0x00 }, /* TERM_L_GREEN2 */
    { 0x00, 0x00, 0xC0, 0xC0 }, /* TERM_L_BLUE2 */
    { 0x00, 0x90, 0x70, 0x30 }, /* TERM_L_UMBER2 */

    { 0x00, 0x0C, 0x0C, 0x0C }, /* TERM_DARK3 */
    { 0x00, 0x60, 0x60, 0x60 }, /* TERM_WHITE3 */
    { 0x00, 0x40, 0x40, 0x40 }, /* TERM_SLATE3 */
    { 0x00, 0x90, 0x38, 0x00 }, /* TERM_ORANGE3 */
    { 0x00, 0x60, 0x00, 0x00 }, /* TERM_RED3 */
    { 0x00, 0x00, 0x38, 0x10 }, /* TERM_GREEN3 */
    { 0x00, 0x00, 0x20, 0x70 }, /* TERM_BLUE3 */
    { 0x00, 0x40, 0x20, 0x00 }, /* TERM_UMBER3 */
    { 0x00, 0x28, 0x28, 0x28 }, /* TERM_L_DARK3 */
    { 0x00, 0x70, 0x70, 0x70 }, /* TERM_L_WHITE3 */
    { 0x00, 0x50, 0x00, 0x90 }, /* TERM_VIOLET3 */
    { 0x00, 0x90, 0x90, 0x00 }, /* TERM_YELLOW3 */
    { 0x00, 0x90, 0x30, 0x18 }, /* TERM_L_RED3 */
    { 0x00, 0x00, 0x90, 0x00 }, /* TERM_L_GREEN3 */
    { 0x00, 0x00, 0x90, 0x90 }, /* TERM_L_BLUE3 */
    { 0x00, 0x70, 0x58, 0x28 } /* TERM_L_UMBER3 */
};

static bool g_use_background_colors = false;
typedef struct ui_color_preset {
    char id[UI_COLOR_PRESET_ID_LEN];
    char label[UI_COLOR_PRESET_LABEL_LEN];
    byte base[16][3];
} ui_color_preset;

static ui_color_preset g_palette_presets[UI_COLOR_PRESET_MAX];
static int g_palette_preset_count = 0;
static char g_current_palette_preset[UI_COLOR_PRESET_ID_LEN] = "";

static const struct {
    const char* id;
    const char* label;
    byte base[16][3];
} builtin_palette_presets[] = {
    {
        "classic",
        "Classic",
        {
            { 0x00, 0x00, 0x00 }, { 0xFF, 0xFF, 0xFF },
            { 0x80, 0x80, 0x80 }, { 0xFF, 0x80, 0x00 },
            { 0xC0, 0x00, 0x00 }, { 0x00, 0x80, 0x40 },
            { 0x00, 0x40, 0xFF }, { 0x80, 0x40, 0x00 },
            { 0x50, 0x50, 0x50 }, { 0xC0, 0xC0, 0xC0 },
            { 0xA0, 0x00, 0xFF }, { 0xFF, 0xFF, 0x00 },
            { 0xFF, 0x60, 0x60 }, { 0x00, 0xFF, 0x00 },
            { 0x00, 0xFF, 0xFF }, { 0xC0, 0x80, 0x40 }
        }
    },
    {
        "embers",
        "Embers",
        {
            { 0x0A, 0x06, 0x04 }, { 0xF7, 0xEE, 0xD8 },
            { 0x8B, 0x78, 0x68 }, { 0xF2, 0x9A, 0x3A },
            { 0xCC, 0x4A, 0x2B }, { 0x4E, 0x8B, 0x57 },
            { 0x4A, 0x79, 0xC9 }, { 0x8A, 0x58, 0x36 },
            { 0x43, 0x39, 0x33 }, { 0xD7, 0xC8, 0xB4 },
            { 0x9A, 0x5F, 0xD6 }, { 0xF1, 0xD4, 0x54 },
            { 0xF2, 0x83, 0x5B }, { 0x5E, 0xC0, 0x76 },
            { 0x73, 0xC8, 0xE3 }, { 0xD1, 0x9A, 0x5A }
        }
    },
    {
        "twilight",
        "Twilight",
        {
            { 0x05, 0x08, 0x10 }, { 0xE8, 0xF0, 0xFF },
            { 0x7A, 0x83, 0x99 }, { 0xE6, 0x8A, 0x3A },
            { 0xB6, 0x44, 0x5C }, { 0x4B, 0x9A, 0x79 },
            { 0x4C, 0x76, 0xD8 }, { 0x8C, 0x63, 0x3E },
            { 0x39, 0x45, 0x56 }, { 0xC8, 0xD2, 0xE8 },
            { 0x8F, 0x62, 0xD8 }, { 0xE8, 0xD6, 0x67 },
            { 0xEB, 0x7A, 0x89 }, { 0x63, 0xD2, 0x8A },
            { 0x73, 0xC4, 0xFF }, { 0xC6, 0x97, 0x62 }
        }
    }
};

static void ui_colors_reset_preset_registry(void)
{
    memset(g_palette_presets, 0, sizeof(g_palette_presets));
    g_palette_preset_count = 0;
}

static bool ui_colors_append_preset(const char* id, const char* label,
    const byte base[16][3])
{
    ui_color_preset* preset;

    if (!id || !id[0] || !base)
        return false;
    if (g_palette_preset_count >= UI_COLOR_PRESET_MAX)
        return false;

    preset = &g_palette_presets[g_palette_preset_count++];
    memset(preset, 0, sizeof(*preset));
    SDL_strlcpy(preset->id, id, sizeof(preset->id));
    if (label && label[0])
        SDL_strlcpy(preset->label, label, sizeof(preset->label));
    else
        SDL_strlcpy(preset->label, id, sizeof(preset->label));
    memcpy(preset->base, base, sizeof(preset->base));
    return true;
}

static void ui_colors_load_builtin_presets(void)
{
    for (int i = 0; i < (int)N_ELEMENTS(builtin_palette_presets); i++)
    {
        (void)ui_colors_append_preset(builtin_palette_presets[i].id,
            builtin_palette_presets[i].label, builtin_palette_presets[i].base);
    }
}

static bool ui_colors_parse_preset_colors(cJSON* colors, byte out[16][3])
{
    int count;

    if (!cJSON_IsArray(colors) || !out)
        return false;

    count = cJSON_GetArraySize(colors);
    if (count != 16)
        return false;

    for (int i = 0; i < 16; i++)
    {
        cJSON* color = cJSON_GetArrayItem(colors, i);
        if (!cJSON_IsArray(color) || cJSON_GetArraySize(color) != 3)
            return false;

        for (int channel = 0; channel < 3; channel++)
        {
            cJSON* value = cJSON_GetArrayItem(color, channel);
            if (!cJSON_IsNumber(value) || value->valueint < 0
                || value->valueint > 255)
            {
                return false;
            }
            out[i][channel] = (byte)value->valueint;
        }
    }

    return true;
}

static bool ui_colors_load_presets_from_file(const char* path)
{
    ang_file* f = NULL;
    cJSON* root = NULL;
    cJSON* presets = NULL;
    char* buffer = NULL;
    Sint64 file_size = 0;
    bool loaded_any = false;

    if (!path || !path[0])
        return false;

    f = ang_file_open(path, "rb");
    if (!f)
        return false;

    file_size = ang_file_size(f);
    if (file_size <= 0 || file_size > 1024 * 1024)
        goto cleanup;

    buffer = mem_alloc_array((size_t)file_size + 1, char);
    if (!buffer)
        goto cleanup;

    if (ang_file_read(f, buffer, (size_t)file_size) != (size_t)file_size)
        goto cleanup;
    buffer[file_size] = '\0';

    root = cJSON_Parse(buffer);
    if (!root)
        goto cleanup;

    presets = cJSON_GetObjectItemCaseSensitive(root, "presets");
    if (!cJSON_IsArray(presets))
        goto cleanup;

    cJSON* preset = NULL;
    cJSON_ArrayForEach(preset, presets)
    {
        cJSON* id = cJSON_GetObjectItemCaseSensitive(preset, "id");
        cJSON* label = cJSON_GetObjectItemCaseSensitive(preset, "label");
        cJSON* colors = cJSON_GetObjectItemCaseSensitive(preset, "colors");
        byte base[16][3];

        if (!cJSON_IsString(id) || !id->valuestring)
            continue;
        if (!ui_colors_parse_preset_colors(colors, base))
            continue;

        if (ui_colors_append_preset(id->valuestring,
                cJSON_IsString(label) ? label->valuestring : id->valuestring,
                base))
        {
            loaded_any = true;
        }
    }

cleanup:
    if (root)
        cJSON_Delete(root);
    if (buffer)
        mem_free(buffer);
    if (f)
        ang_file_close(f);
    return loaded_any;
}

static byte ui_colors_scale_channel(byte value, int numerator, int denominator)
{
    int scaled = (value * numerator + denominator / 2) / denominator;
    if (scaled < 0)
        scaled = 0;
    if (scaled > 255)
        scaled = 255;
    return (byte)scaled;
}

static void ui_colors_write_derived_palette(const byte base[16][3])
{
    static const int numerators[4] = { 8, 6, 4, 3 };
    const int denominator = 8;

    if (!base)
        return;

    for (int shade = 0; shade < 4; shade++)
    {
        for (int color = 0; color < 16; color++)
        {
            int idx = color + shade * 16;
            angband_color_table[idx][0] = 0;
            for (int channel = 0; channel < 3; channel++)
            {
                angband_color_table[idx][channel + 1]
                    = ui_colors_scale_channel(base[color][channel],
                        numerators[shade], denominator);
            }
        }
    }
}

/* Short color names for base colors */
static char* short_color_names[MAX_BASE_COLORS] = {
    "Dark", "White", "Slate", "Orange",
    "Red", "Green", "Blue", "Umber",
    "L.Dark", "L.Slate", "Violet", "Yellow",
    "L.Red", "L.Green", "L.Blue", "L.Umber"
};

/*
 * Extract a textual representation of an attribute.
 * Returns the base color name, optionally with a shade suffix.
 */
cptr attr_to_text(byte a)
{
    char* base;

    base = short_color_names[GET_BASE_COLOR(a)];

#if DO_YOU_WANT_THIS_IN_MONSTER_SPOILERS_Q

    if (GET_SHADE(a) > 0)
    {
        static char buf[25];

        strnfmt(buf, sizeof(buf), "%s%d", base, GET_SHADE(a));

        return (buf);
    }

#endif

    return (base);
}

bool ui_colors_use_backgrounds(void)
{
    return g_use_background_colors;
}

void ui_colors_set_backgrounds(bool enabled)
{
    g_use_background_colors = enabled;
}

bool ui_colors_load_palette_presets(void)
{
    char path[1024];
    path[0] = '\0';

    ui_colors_reset_preset_registry();

    if (!resource_build_path(path, sizeof(path), RESOURCE_ROOT_PREF,
            "palette_presets.json")
        || !ui_colors_load_presets_from_file(path))
    {
        ui_colors_load_builtin_presets();
        if (path[0] != '\0')
            log_warn("palette presets: using built-in presets (file unavailable or invalid: %s)", path);
        else
            log_warn("palette presets: using built-in presets");
    }

    if (g_palette_preset_count <= 0)
        ui_colors_load_builtin_presets();

    if (g_palette_preset_count > 0)
        (void)ui_colors_apply_palette_preset(g_palette_presets[0].id);

    return g_palette_preset_count > 0;
}

int ui_colors_palette_preset_count(void)
{
    return g_palette_preset_count;
}

cptr ui_colors_palette_preset_id(int index)
{
    if (index < 0 || index >= g_palette_preset_count)
        return NULL;
    return g_palette_presets[index].id;
}

cptr ui_colors_palette_preset_label(int index)
{
    if (index < 0 || index >= g_palette_preset_count)
        return NULL;
    return g_palette_presets[index].label;
}

cptr ui_colors_current_palette_preset(void)
{
    return g_current_palette_preset;
}

bool ui_colors_apply_palette_preset(cptr id)
{
    int index = 0;

    if (g_palette_preset_count <= 0)
        ui_colors_load_builtin_presets();
    if (g_palette_preset_count <= 0)
        return false;

    if (id && id[0])
    {
        for (index = 0; index < g_palette_preset_count; index++)
        {
            if (streq(g_palette_presets[index].id, id))
                break;
        }
        if (index >= g_palette_preset_count)
            index = 0;
    }

    ui_colors_write_derived_palette(g_palette_presets[index].base);
    SDL_strlcpy(g_current_palette_preset, g_palette_presets[index].id,
        sizeof(g_current_palette_preset));
    return true;
}

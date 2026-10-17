#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "steamworks.h"

#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
typedef struct sdl_handheld_system_info {
    char vendor[128];
    char product[128];
    char version[128];
    char family[128];
} sdl_handheld_system_info;

static void sdl_handheld_trim(char* text)
{
    char* start = text;
    size_t length;

    while (isspace((unsigned char)*start))
        start++;
    length = strlen(start);
    memmove(text, start, length + 1);
    while (length > 0 && isspace((unsigned char)text[length - 1]))
        text[--length] = '\0';
}

static bool sdl_handheld_prefix(const char* text, const char* prefix)
{
    size_t length = strlen(prefix);

    if (SDL_strncasecmp(text, prefix, length) != 0)
        return false;
    return text[length] == '\0' || text[length] == ' '
        || text[length] == '_' || text[length] == '-';
}

static bool sdl_handheld_model_in(const char* product,
    const char* const* models, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (sdl_handheld_prefix(product, models[i]))
            return true;
    }
    return false;
}

static sdl_desktop_handheld_kind sdl_handheld_identify(
    const sdl_handheld_system_info* info)
{
    /* Model identifiers, not controller names or screen resolution. See the
     * source references in docs/steam-build.md before extending these lists. */
    static const char* const legion_models[] = {
        "83E1", "83N0", "83N1", "83L3", "83N6", "83Q2", "83Q3",
    };
    static const char* const gpd_models[] = {
        "G1618-03", "G1618-04", "G1618-05", "G1617-01", "G1617-02",
        "G1619-04", "G1619-05",
    };
    static const char* const aya_models[] = {
        "AIR", "GEEK", "FLIP", "NEXT", "SLIDE", "KUN",
        "AYANEO AIR", "AYANEO GEEK", "AYANEO FLIP", "AYANEO NEXT",
        "AYANEO SLIDE", "AYANEO KUN", "AYANEO 2", "AYANEO 2S",
        "AYANEO 3", "AYANEO 2021", "AYA NEO FOUNDER", "AYA NEO 2021",
    };

    if (sdl_handheld_prefix(info->vendor, "Valve")
        && (SDL_strcasecmp(info->product, "Jupiter") == 0
            || SDL_strcasecmp(info->product, "Galileo") == 0
            || sdl_handheld_prefix(info->product, "Steam Deck")))
    {
        return SDL_DESKTOP_HANDHELD_STEAM_DECK;
    }
    if ((sdl_handheld_prefix(info->vendor, "ASUSTeK")
            || sdl_handheld_prefix(info->vendor, "ASUS"))
        && (sdl_handheld_prefix(info->product, "ROG Ally")
            || sdl_handheld_prefix(info->product, "ROG Xbox Ally")))
    {
        return SDL_DESKTOP_HANDHELD_OTHER;
    }
    if (sdl_handheld_prefix(info->vendor, "LENOVO")
        && (sdl_handheld_prefix(info->product, "Legion Go")
            || sdl_handheld_prefix(info->version, "Legion Go")
            || sdl_handheld_prefix(info->family, "Legion Go")
            || sdl_handheld_model_in(info->product, legion_models,
                N_ELEMENTS(legion_models))))
    {
        return SDL_DESKTOP_HANDHELD_OTHER;
    }
    if ((sdl_handheld_prefix(info->vendor, "Micro-Star")
            || sdl_handheld_prefix(info->vendor, "MSI"))
        && sdl_handheld_prefix(info->product, "Claw"))
    {
        return SDL_DESKTOP_HANDHELD_OTHER;
    }
    if (sdl_handheld_prefix(info->vendor, "GPD")
        && sdl_handheld_model_in(info->product, gpd_models,
            N_ELEMENTS(gpd_models)))
    {
        return SDL_DESKTOP_HANDHELD_OTHER;
    }
    if ((sdl_handheld_prefix(info->vendor, "AYA")
            || sdl_handheld_prefix(info->vendor, "AYANEO"))
        && sdl_handheld_model_in(info->product, aya_models,
            N_ELEMENTS(aya_models)))
    {
        return SDL_DESKTOP_HANDHELD_OTHER;
    }
    if (sdl_handheld_prefix(info->vendor, "ONE-NETBOOK")
        && sdl_handheld_prefix(info->product, "ONEXPLAYER"))
    {
        return SDL_DESKTOP_HANDHELD_OTHER;
    }
    if (sdl_handheld_prefix(info->vendor, "ZOTAC")
        && sdl_handheld_prefix(info->product, "ZOTAC GAMING ZONE"))
    {
        return SDL_DESKTOP_HANDHELD_OTHER;
    }
    return SDL_DESKTOP_HANDHELD_NONE;
}

static void sdl_handheld_read_dmi(const char* directory,
    sdl_handheld_system_info* info)
{
    const char* names[] = {
        "sys_vendor", "product_name", "product_version", "product_family",
    };
    char* fields[] = {
        info->vendor, info->product, info->version, info->family,
    };

    for (size_t i = 0; i < N_ELEMENTS(names); i++) {
        char path[256];
        FILE* file;

        strnfmt(path, sizeof(path), "%s/%s", directory, names[i]);
        file = fopen(path, "r");
        if (!file)
            continue;
        if (fgets(fields[i], sizeof(info->vendor), file))
            sdl_handheld_trim(fields[i]);
        fclose(file);
    }
}

#if defined(SDL_PLATFORM_WINDOWS)
static void sdl_handheld_smbios_string(char* dest, size_t size,
    const Uint8* strings, const Uint8* end, Uint8 index)
{
    const Uint8* start = strings;

    dest[0] = '\0';
    if (!index)
        return;
    for (Uint8 i = 1; i < index; i++) {
        while (start < end && *start)
            start++;
        if (start >= end)
            return;
        start++;
    }
    if (start >= end)
        return;
    size_t length = 0;
    while (start + length < end && start[length])
        length++;
    if (length >= size)
        length = size - 1;
    memcpy(dest, start, length);
    dest[length] = '\0';
    sdl_handheld_trim(dest);
}

static bool sdl_handheld_read_smbios(const Uint8* raw, size_t size,
    sdl_handheld_system_info* info)
{
    /* RawSMBIOSData has an eight-byte header followed by bounded records. */
    if (size < 8)
        return false;
    Uint32 length = (Uint32)raw[4] | ((Uint32)raw[5] << 8)
        | ((Uint32)raw[6] << 16) | ((Uint32)raw[7] << 24);
    if (length > size - 8)
        return false;
    const Uint8* record = raw + 8;
    const Uint8* limit = record + length;
    while (limit - record >= 4) {
        Uint8 type = record[0];
        Uint8 formatted = record[1];
        if (formatted < 4 || formatted > limit - record)
            return false;
        const Uint8* strings = record + formatted;
        const Uint8* end = strings;
        while (limit - end >= 2 && (end[0] || end[1]))
            end++;
        if (limit - end < 2)
            return false;
        if (type == 1 && formatted >= 8) {
            sdl_handheld_smbios_string(info->vendor, sizeof(info->vendor),
                strings, end + 1, record[4]);
            sdl_handheld_smbios_string(info->product, sizeof(info->product),
                strings, end + 1, record[5]);
            sdl_handheld_smbios_string(info->version, sizeof(info->version),
                strings, end + 1, record[6]);
            if (formatted >= 27) {
                sdl_handheld_smbios_string(info->family, sizeof(info->family),
                    strings, end + 1, record[26]);
            }
            return true;
        }
        if (type == 127)
            break;
        record = end + 2;
    }
    return false;
}

static void sdl_handheld_read_windows_system(sdl_handheld_system_info* info)
{
    const DWORD provider = 0x52534D42; /* 'RSMB' */
    UINT size = GetSystemFirmwareTable(provider, 0, NULL, 0);

    if (size >= 8 && size <= 1024 * 1024) {
        Uint8* raw = SDL_malloc(size);
        if (raw) {
            UINT written = GetSystemFirmwareTable(provider, 0, raw, size);
            if (written == size)
                (void)sdl_handheld_read_smbios(raw, size, info);
            SDL_free(raw);
        }
    }
    /* Wine/Proton may expose synthetic SMBIOS. Its usual host-root mapping
     * lets non-Steam builds inspect the same public DMI fields as Linux. */
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll && GetProcAddress(ntdll, "wine_get_version")
        && sdl_handheld_identify(info) == SDL_DESKTOP_HANDHELD_NONE)
    {
        sdl_handheld_read_dmi("Z:/sys/class/dmi/id", info);
    }
}
#endif
#endif

sdl_desktop_handheld_kind sdl_detect_desktop_handheld(void)
{
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
    static bool detected = false;
    static sdl_desktop_handheld_kind kind = SDL_DESKTOP_HANDHELD_NONE;
    sdl_handheld_system_info info = { 0 };
    const char* source = "system model";
    const char* override;

    if (detected)
        return kind;
    detected = true;
    override = SDL_getenv("SIL_HANDHELD");
    if (override && SDL_strcasecmp(override, "auto") != 0 && override[0]) {
        if (SDL_strcasecmp(override, "steamdeck") == 0)
            kind = SDL_DESKTOP_HANDHELD_STEAM_DECK;
        else if (SDL_strcasecmp(override, "handheld") == 0)
            kind = SDL_DESKTOP_HANDHELD_OTHER;
        else if (SDL_strcasecmp(override, "desktop") != 0)
            log_warn("Ignoring unknown SIL_HANDHELD value; use auto, desktop, handheld or steamdeck");
        if (SDL_strcasecmp(override, "desktop") == 0
            || kind != SDL_DESKTOP_HANDHELD_NONE)
        {
            source = "SIL_HANDHELD override";
            goto finished;
        }
    }
    if (sil_steamworks_is_steam_deck()) {
        kind = SDL_DESKTOP_HANDHELD_STEAM_DECK;
        source = "Steamworks";
        goto finished;
    }
#if defined(SDL_PLATFORM_LINUX)
    sdl_handheld_read_dmi("/sys/class/dmi/id", &info);
#else
    sdl_handheld_read_windows_system(&info);
#endif
    kind = sdl_handheld_identify(&info);
finished:
    log_info("Desktop hardware: %s (source=%s, vendor=%s, model=%s)",
        kind == SDL_DESKTOP_HANDHELD_STEAM_DECK ? "steam-deck"
            : kind == SDL_DESKTOP_HANDHELD_OTHER ? "other-handheld" : "desktop",
        source, info.vendor, info.product);
    return kind;
#else
    return SDL_DESKTOP_HANDHELD_NONE;
#endif
}

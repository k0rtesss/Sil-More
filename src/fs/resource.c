#include "angband.h"

#define ANGBAND_NO_IO_COMPAT
#include "fs/io_sdl.h"
#undef ANGBAND_NO_IO_COMPAT
#include "fs/path.h"
#include "fs/resource.h"

static bool resource_copy_path(char* buf, size_t max, cptr path)
{
    if (!buf || !max || !path || !path[0])
        return false;

    return strnfmt(buf, max, "%s", path) < max;
}

static void resource_normalize_separators(char* path)
{
    if (!path)
        return;

    for (; *path; path++)
    {
        if (*path == '\\')
            *path = '/';
    }
}

static bool resource_has_separator(cptr path)
{
    if (!path)
        return false;

    for (; *path; path++)
    {
        if (*path == '/' || *path == '\\')
            return true;
    }

    return false;
}

static bool resource_is_absolute_path(cptr path)
{
    if (!path || !path[0])
        return false;

    if (path[0] == '/' || path[0] == '\\')
        return true;

#ifdef WINDOWS
    if (((path[0] >= 'A' && path[0] <= 'Z')
            || (path[0] >= 'a' && path[0] <= 'z'))
        && path[1] == ':')
    {
        return true;
    }
#endif

    return false;
}

static bool resource_prefix_is(cptr path, cptr prefix)
{
    size_t prefix_len;

    if (!path || !prefix)
        return false;

    prefix_len = strlen(prefix);
    return strncmp(path, prefix, prefix_len) == 0;
}

static bool resource_build_subpath(char* buf, size_t max, cptr base,
    cptr subdir, cptr leaf)
{
    char temp[1024];

    if (!base || !base[0])
        return false;

    if (subdir && subdir[0])
    {
        if (!path_build(temp, sizeof(temp), base, subdir))
            return false;
        if (!leaf || !leaf[0])
            return resource_copy_path(buf, max, temp);
        return path_build(buf, max, temp, leaf);
    }

    if (!leaf || !leaf[0])
        return resource_copy_path(buf, max, base);

    return path_build(buf, max, base, leaf);
}

static bool resource_build_bundle_relative(char* buf, size_t max, cptr leaf)
{
    if (!ANGBAND_DIR || !ANGBAND_DIR[0] || !leaf || !leaf[0])
        return false;

    return path_build(buf, max, ANGBAND_DIR, leaf);
}

bool resource_build_path(char* buf, size_t max, resource_root root, cptr leaf)
{
    switch (root)
    {
    case RESOURCE_ROOT_USER:
        return resource_build_subpath(buf, max, ANGBAND_DIR_USER, NULL, leaf);

    case RESOURCE_ROOT_PREF:
        return resource_build_subpath(buf, max, ANGBAND_DIR_PREF, NULL, leaf);

    case RESOURCE_ROOT_HELP:
        return resource_build_subpath(buf, max, ANGBAND_DIR_HELP, NULL, leaf);

    case RESOURCE_ROOT_XTRA:
        return resource_build_subpath(buf, max, ANGBAND_DIR_XTRA, NULL, leaf);

    case RESOURCE_ROOT_XTRA_FONT:
        return resource_build_subpath(buf, max, ANGBAND_DIR_XTRA, "font", leaf);

    case RESOURCE_ROOT_XTRA_GRAF:
        return resource_build_subpath(buf, max, ANGBAND_DIR_XTRA, "graf", leaf);

    case RESOURCE_ROOT_XTRA_SOUND:
        return resource_build_subpath(buf, max, ANGBAND_DIR_XTRA, "sound",
            leaf);

    case RESOURCE_ROOT_XTRA_MUSIC:
        return resource_build_subpath(buf, max, ANGBAND_DIR_XTRA, "music",
            leaf);
    }

    return false;
}

bool resource_resolve_path(char* buf, size_t max, resource_root default_root,
    cptr spec)
{
    char normalized[1024];

    if (!spec || !spec[0])
        return false;

    if (spec[0] == '~' || resource_is_absolute_path(spec))
        return path_parse(buf, max, spec);

    if (!resource_copy_path(normalized, sizeof(normalized), spec))
        return false;

    resource_normalize_separators(normalized);

    if (resource_prefix_is(normalized, "lib/"))
        return resource_build_bundle_relative(buf, max, normalized + 4);

    if (resource_prefix_is(normalized, "pref/"))
        return resource_build_path(buf, max, RESOURCE_ROOT_PREF,
            normalized + strlen("pref/"));

    if (resource_prefix_is(normalized, "help/"))
        return resource_build_path(buf, max, RESOURCE_ROOT_HELP,
            normalized + strlen("help/"));

    if (resource_prefix_is(normalized, "xtra/"))
        return resource_build_path(buf, max, RESOURCE_ROOT_XTRA,
            normalized + strlen("xtra/"));

    if (default_root == RESOURCE_ROOT_XTRA && resource_has_separator(normalized))
        return resource_build_path(buf, max, RESOURCE_ROOT_XTRA, normalized);

    if (default_root == RESOURCE_ROOT_XTRA_FONT && resource_has_separator(normalized))
    {
        char prefixed[1024];

        if (resource_prefix_is(normalized, "font/"))
            return resource_build_path(buf, max, RESOURCE_ROOT_XTRA, normalized);

        if (strnfmt(prefixed, sizeof(prefixed), "font/%s", normalized)
            >= (int)sizeof(prefixed))
        {
            return false;
        }
        return resource_build_path(buf, max, RESOURCE_ROOT_XTRA, prefixed);
    }

    if (default_root == RESOURCE_ROOT_XTRA_GRAF && resource_has_separator(normalized))
    {
        char prefixed[1024];

        if (resource_prefix_is(normalized, "graf/"))
            return resource_build_path(buf, max, RESOURCE_ROOT_XTRA, normalized);

        if (strnfmt(prefixed, sizeof(prefixed), "graf/%s", normalized)
            >= (int)sizeof(prefixed))
        {
            return false;
        }
        return resource_build_path(buf, max, RESOURCE_ROOT_XTRA, prefixed);
    }

    if (default_root == RESOURCE_ROOT_XTRA_SOUND && resource_has_separator(normalized))
    {
        char prefixed[1024];

        if (resource_prefix_is(normalized, "sound/"))
            return resource_build_path(buf, max, RESOURCE_ROOT_XTRA, normalized);

        if (strnfmt(prefixed, sizeof(prefixed), "sound/%s", normalized)
            >= (int)sizeof(prefixed))
        {
            return false;
        }
        return resource_build_path(buf, max, RESOURCE_ROOT_XTRA, prefixed);
    }

    if (default_root == RESOURCE_ROOT_XTRA_MUSIC && resource_has_separator(normalized))
    {
        char prefixed[1024];

        if (resource_prefix_is(normalized, "music/"))
            return resource_build_path(buf, max, RESOURCE_ROOT_XTRA, normalized);

        if (strnfmt(prefixed, sizeof(prefixed), "music/%s", normalized)
            >= (int)sizeof(prefixed))
        {
            return false;
        }
        return resource_build_path(buf, max, RESOURCE_ROOT_XTRA, prefixed);
    }

    return resource_build_path(buf, max, default_root, normalized);
}

bool resource_resolve_xtra_path(char* buf, size_t max, cptr spec,
    cptr default_relative)
{
    if (spec && spec[0])
        return resource_resolve_path(buf, max, RESOURCE_ROOT_XTRA, spec);

    if (!default_relative || !default_relative[0])
        return false;

    return resource_resolve_path(buf, max, RESOURCE_ROOT_XTRA,
        default_relative);
}

bool resource_enumerate_directory(cptr path,
    resource_enumerate_directory_callback callback, void* userdata)
{
    return sdl_enumerate_directory(path, callback, userdata);
}

bool resource_build_ui_config_path(char* buf, size_t max)
{
    return resource_build_path(buf, max, RESOURCE_ROOT_USER, "sil_sdl.json");
}

bool resource_build_active_sound_config_path(char* buf, size_t max)
{
#ifdef SIL_USE_LOCAL_DATA
    return resource_build_path(buf, max, RESOURCE_ROOT_PREF, "sound.json");
#else
    return resource_build_path(buf, max, RESOURCE_ROOT_USER, "sound.json");
#endif
}

bool resource_build_default_sound_config_path(char* buf, size_t max)
{
    return resource_build_path(buf, max, RESOURCE_ROOT_PREF, "sound.json");
}

bool resource_build_default_tileset_path(char* buf, size_t max)
{
    return resource_resolve_xtra_path(buf, max, NULL, "graf/16x16.png");
}

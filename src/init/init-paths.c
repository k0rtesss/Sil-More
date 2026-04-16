#define ANGBAND_NO_IO_COMPAT
#include "angband.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "fs/resource.h"
#include "log/log.h"
#include "metarun.h"
#include <SDL3/SDL.h>

cptr ANGBAND_SYS = "xxx";
cptr ANGBAND_GRAF = "old";
cptr ANGBAND_DIR;
cptr ANGBAND_DIR_APEX;
cptr ANGBAND_DIR_METARUN;
cptr ANGBAND_DIR_BONE;
cptr ANGBAND_DIR_DATA;
cptr ANGBAND_DIR_EDIT;
cptr ANGBAND_DIR_FILE;
cptr ANGBAND_DIR_HELP;
cptr ANGBAND_DIR_INFO;
cptr ANGBAND_DIR_SAVE;
cptr ANGBAND_DIR_PREF;
cptr ANGBAND_DIR_USER;
cptr ANGBAND_DIR_XTRA;
cptr ANGBAND_DIR_SCRIPT;

#define SIL_USER_ROOT "sil-more"
#define SIL_USER_DATA_DIR "data"
#define SIL_USER_SAVE_DIR "save"
#define SIL_USER_META_DIR "meta"
#define SIL_USER_META_RUNS "metaruns"

#ifndef SIL_USE_LOCAL_DATA
static bool is_path_separator(char ch)
{
#ifdef WINDOWS
    return (ch == '\\') || (ch == '/');
#else
    return (ch == '/');
#endif
}

static void strip_trailing_separators(char* path)
{
    size_t len = strlen(path);
    while (len > 0 && is_path_separator(path[len - 1]))
    {
        path[--len] = '\0';
    }
}

static bool build_user_root_path(char* buf, size_t len)
{
#if !defined(SIL_IOS)
    /* On desktop, prefer the system saved-games / home folder.
     * On iOS the container root returned by SDL_GetUserFolder is not
     * directly writable, so fall back to SDL_GetPrefPath().
     */
    const char* base = SDL_GetUserFolder(SDL_FOLDER_SAVEDGAMES);
    char temp[1024];

    if ((!base || !*base))
        base = SDL_GetUserFolder(SDL_FOLDER_HOME);

    if (base && *base)
    {
        if (SDL_strlcpy(temp, base, sizeof(temp)) >= sizeof(temp))
            return false;
        strip_trailing_separators(temp);
        return path_build(buf, len, temp, SIL_USER_ROOT);
    }
#endif

    char* pref = SDL_GetPrefPath("Sil-QH", SIL_USER_ROOT);
    if (!pref)
        return false;

    bool ok = false;
    if (SDL_strlcpy(buf, pref, len) < len)
    {
        strip_trailing_separators(buf);
        ok = true;
    }
    SDL_free(pref);
    return ok;
}

static void ensure_directory_exists(const char* path, const char* label)
{
    if (!path || !*path)
        return;

    if (!SDL_CreateDirectory(path))
    {
        log_warn("init_file_paths: unable to create %s directory '%s': %s",
            label ? label : "user", path, SDL_GetError());
    }
}

static void seed_user_data_from_install(const char* user_data_dir)
{
    if (!user_data_dir || !*user_data_dir || !ANGBAND_DIR || !*ANGBAND_DIR)
        return;

    char install_data_dir[1024];
    if (!path_build(
            install_data_dir, sizeof(install_data_dir), ANGBAND_DIR, "data"))
        return;

    SDL_PathInfo install_info;
    if (!SDL_GetPathInfo(install_data_dir, &install_info)
        || install_info.type != SDL_PATHTYPE_DIRECTORY)
        return;

    int entry_count = 0;
    char** entries = SDL_GlobDirectory(install_data_dir, "*.raw", 0, &entry_count);
    if (!entries)
        return;

    for (int i = 0; entries[i]; ++i)
    {
        char source[1024];
        char destination[1024];

        if (!path_build(source, sizeof(source), install_data_dir, entries[i]))
            continue;
        if (!path_build(destination, sizeof(destination), user_data_dir, entries[i]))
            continue;

        if (SDL_GetPathInfo(destination, NULL))
            continue;

        if (!SDL_CopyFile(source, destination))
        {
            log_warn("init_file_paths: unable to copy '%s' to '%s': %s",
                source, destination, SDL_GetError());
        }
    }

    SDL_free(entries);
}

#if !defined(__ANDROID__) && !defined(SIL_IOS)
typedef bool (*version_check_fn)(const char* path);

static void migrate_legacy_metarun_layout(
    const char* meta_root, const char* metarun_dir);

static bool copy_leaf_if_needed(const char* src_dir, const char* dst_dir,
    const char* leaf, const char* label, version_check_fn needs_refresh)
{
    char src[1024];
    char dst[1024];
    SDL_PathInfo info;

    if (!src_dir || !dst_dir || !leaf)
        return false;

    if (!path_build(src, sizeof(src), src_dir, leaf))
        return false;
    if (!SDL_GetPathInfo(src, &info) || info.type != SDL_PATHTYPE_FILE)
        return false;

    if (!path_build(dst, sizeof(dst), dst_dir, leaf))
        return false;

    bool dest_exists = SDL_GetPathInfo(dst, NULL);
    bool should_copy = !dest_exists;
    if (!should_copy && needs_refresh)
        should_copy = needs_refresh(dst);

    if (!should_copy)
        return false;

    if (!SDL_CopyFile(src, dst))
    {
        log_warn("init_file_paths: unable to seed %s file '%s' from '%s': %s",
            label ? label : "user", dst, src, SDL_GetError());
        return false;
    }
    else
    {
        log_info("init_file_paths: seeded %s file at '%s'",
            label ? label : leaf, dst);
    }
    return true;
}

static bool has_valid_metarun_data(const char* meta_dir)
{
    if (!meta_dir || !*meta_dir)
        return false;

    char meta_path[1024];
    if (!path_build(meta_path, sizeof(meta_path), meta_dir, META_RAW))
        return false;

    SDL_PathInfo info;
    if (!SDL_GetPathInfo(meta_path, &info) || info.type != SDL_PATHTYPE_FILE)
        return false;

    /* File exists and is readable */
    ang_file* fd = sdl_fopen(meta_path, "rb");
    if (!fd)
        return false;

    /* Check if it has valid header */
    meta_file_header meta_hdr;
    bool valid = (ang_file_read_compat(fd, &meta_hdr, sizeof(meta_hdr))
        == sizeof(meta_hdr))
        && meta_hdr.entry_count > 0;

    sdl_fclose(fd);
    return valid;
}

static void seed_user_meta_from_install(
    const char* user_meta_dir, const char* user_metarun_dir)
{
    (void)user_metarun_dir;

    if (!user_meta_dir || !*user_meta_dir || !ANGBAND_DIR || !*ANGBAND_DIR)
        return;

    /* Check if user directory already has valid metarun data (from previous migration) */
    bool has_existing_data = has_valid_metarun_data(user_meta_dir);
    if (has_existing_data)
    {
        log_info("init_file_paths: user meta directory already contains valid data, skipping migration");
        return;
    }

    log_debug("init_file_paths: ANGBAND_DIR = '%s'", ANGBAND_DIR);

    /* First, check for legacy location: lib/apex/metaruns/meta.raw (old structure) */
    char install_apex_dir[1024];
    char install_metaruns_dir[1024];
    char legacy_meta_path[1024];
    bool found_legacy = false;

    if (path_build(
            install_apex_dir, sizeof(install_apex_dir), ANGBAND_DIR, "apex"))
    {
        log_debug("init_file_paths: apex dir = '%s'", install_apex_dir);
        if (path_build(install_metaruns_dir, sizeof(install_metaruns_dir),
                install_apex_dir, "metaruns"))
        {
            log_debug("init_file_paths: metaruns dir = '%s'", install_metaruns_dir);
            if (path_build(legacy_meta_path, sizeof(legacy_meta_path),
                    install_metaruns_dir, META_RAW))
            {
                log_debug("init_file_paths: checking for legacy meta.raw at '%s'",
                    legacy_meta_path);
                SDL_PathInfo info;
                if (SDL_GetPathInfo(legacy_meta_path, &info)
                    && info.type == SDL_PATHTYPE_FILE)
                {
                    found_legacy = true;
                    log_info("init_file_paths: found legacy meta.raw in lib/apex/metaruns/");
                }
                else
                {
                    log_debug("init_file_paths: legacy meta.raw not found or not a file");
                }
            }
        }
    }

    /* Determine source directory for scores.raw and possibly meta.raw */
    char install_meta_dir[1024];
    if (!path_build(install_meta_dir, sizeof(install_meta_dir), ANGBAND_DIR, "apex"))
        return;

    log_info("init_file_paths: migrating metarun data from install directory");

    /* Copy scores.raw from lib/apex/ if present */
    char score_path[1024];
    if (path_build(score_path, sizeof(score_path), user_meta_dir, "scores.raw"))
    {
        SDL_PathInfo info;
        if (!SDL_GetPathInfo(score_path, &info) || info.type != SDL_PATHTYPE_FILE)
        {
            if (copy_leaf_if_needed(
                    install_meta_dir, user_meta_dir, "scores.raw", "scores", NULL))
            {
                log_info("init_file_paths: migrated scores.raw from lib/apex/");
            }
        }
    }

    /* Copy meta.raw from the appropriate source */
    char user_meta_path[1024];
    if (path_build(user_meta_path, sizeof(user_meta_path), user_meta_dir, META_RAW))
    {
        SDL_PathInfo info;
        if (!SDL_GetPathInfo(user_meta_path, &info) || info.type != SDL_PATHTYPE_FILE)
        {
            if (found_legacy)
            {
                /* Copy from lib/apex/metaruns/meta.raw (old location) */
                log_info("init_file_paths: attempting to copy legacy meta.raw from '%s' to '%s'",
                    legacy_meta_path, user_meta_path);
                if (SDL_CopyFile(legacy_meta_path, user_meta_path))
                {
                    log_info("init_file_paths: migrated meta.raw from lib/apex/metaruns/ (legacy location)");
                }
                else
                {
                    log_warn("init_file_paths: failed to copy legacy meta.raw: %s",
                        SDL_GetError());
                }
            }
            else
            {
                log_debug("init_file_paths: no legacy meta.raw found, trying lib/apex/meta.raw");
                /* Try lib/apex/meta.raw (if it exists) */
                if (copy_leaf_if_needed(
                        install_meta_dir, user_meta_dir, META_RAW, "meta", NULL))
                {
                    log_info("init_file_paths: migrated meta.raw from lib/apex/");
                }
                else
                {
                    log_debug("init_file_paths: no meta.raw found in lib/apex/");
                }
            }
        }
    }
}

static void seed_user_saves_from_install(const char* user_save_dir)
{
    if (!user_save_dir || !*user_save_dir || !ANGBAND_DIR || !*ANGBAND_DIR)
        return;

    char install_save_dir[1024];
    if (!path_build(install_save_dir, sizeof(install_save_dir), ANGBAND_DIR, "save"))
        return;

    SDL_PathInfo info;
    if (!SDL_GetPathInfo(install_save_dir, &info)
        || info.type != SDL_PATHTYPE_DIRECTORY)
        return;

    /* Check if user directory already has save files */
    int user_entry_count = 0;
    char** user_entries = SDL_GlobDirectory(user_save_dir, NULL, 0, &user_entry_count);
    bool has_existing_saves = false;

    if (user_entries)
    {
        for (int i = 0; user_entries[i]; ++i)
        {
            if (!streq(user_entries[i], ".") && !streq(user_entries[i], ".."))
            {
                has_existing_saves = true;
                break;
            }
        }
        SDL_free(user_entries);
    }

    if (has_existing_saves)
    {
        log_info("init_file_paths: user save directory already contains files, skipping migration");
        return;
    }

    /* Only migrate if user directory is empty */
    log_info("init_file_paths: migrating save files from install directory");

    int entry_count = 0;
    char** entries = SDL_GlobDirectory(install_save_dir, NULL, 0, &entry_count);
    if (!entries)
        return;

    for (int i = 0; entries[i]; ++i)
    {
        if (streq(entries[i], ".") || streq(entries[i], ".."))
            continue;

        /* Skip git files and backup folders */
        if (streq(entries[i], ".gitignore") || strstr(entries[i], "saves_metarun_"))
            continue;

        char src_path[1024];
        char dst_path[1024];
        if (!path_build(src_path, sizeof(src_path), install_save_dir, entries[i]))
            continue;
        if (!path_build(dst_path, sizeof(dst_path), user_save_dir, entries[i]))
            continue;

        if (!SDL_GetPathInfo(src_path, &info) || info.type != SDL_PATHTYPE_FILE)
            continue;
        if (SDL_GetPathInfo(dst_path, NULL))
            continue;

        if (!SDL_CopyFile(src_path, dst_path))
        {
            log_warn("init_file_paths: unable to seed save '%s' -> '%s': %s",
                src_path, dst_path, SDL_GetError());
        }
        else
        {
            log_info("init_file_paths: copied legacy save '%s' -> '%s'",
                src_path, dst_path);
        }
    }

    SDL_free(entries);
}
#endif

/* Copy a file using the platform IO facade (works with Android assets). */
static bool copy_file_io(const char* src, const char* dst)
{
    if (!src || !*src || !dst || !*dst)
        return false;

    ang_file* in = sdl_fopen(src, "rb");
    if (!in)
        return false;

    ang_file* out = sdl_fopen(dst, "wb");
    if (!out)
    {
        sdl_fclose(in);
        return false;
    }

    bool ok = true;
    char buf[8192];
    for (;;)
    {
        size_t r = ang_file_read_compat(in, buf, sizeof(buf));
        if (r == 0)
            break;

        size_t w = ang_file_write_compat(out, buf, r);
        if (w != r)
        {
            ok = false;
            break;
        }
    }

    if (sdl_fclose(out) != 0)
        ok = false;
    if (sdl_fclose(in) != 0)
        ok = false;

    return ok;
}

static void seed_sound_config(const char* user_dir)
{
    if (!user_dir || !*user_dir || !ANGBAND_DIR || !*ANGBAND_DIR)
        return;

    char user_sound_path[1024];
    if (!resource_build_path(user_sound_path, sizeof(user_sound_path),
            RESOURCE_ROOT_USER, "sound.json"))
        return;

    /* Check if sound.json already exists in user folder */
    if (SDL_GetPathInfo(user_sound_path, NULL))
    {
        log_debug("init_file_paths: sound.json already exists in user folder");
        return;
    }

    /* Copy the bundled default sound config into the user folder. */
    char pref_sound_path[1024];

    if (!ANGBAND_DIR_PREF || !*ANGBAND_DIR_PREF)
    {
        log_warn("init_file_paths: ANGBAND_DIR_PREF not set, cannot seed sound.json");
        return;
    }

    if (!resource_build_default_sound_config_path(pref_sound_path,
            sizeof(pref_sound_path)))
        return;

    /* Prefer streaming copy so it can work when source lives in Android assets. */
    if (copy_file_io(pref_sound_path, user_sound_path))
        log_info("init_file_paths: copied default sound.json to user folder");
    else
        log_warn("init_file_paths: failed to seed sound.json from '%s'",
            pref_sound_path);
}

static void migrate_legacy_metarun_layout(
    const char* meta_root, const char* metarun_dir)
{
    if (!meta_root || !*meta_root || !metarun_dir || !*metarun_dir)
        return;

    char legacy[1024];
    if (!path_build(legacy, sizeof(legacy), metarun_dir, META_RAW))
        return;

    SDL_PathInfo info;
    if (!SDL_GetPathInfo(legacy, &info) || info.type != SDL_PATHTYPE_FILE)
    {
        /* No legacy file to migrate - check if the metaruns/ folder is empty and remove it */
        int entry_count = 0;
        char** entries = SDL_GlobDirectory(metarun_dir, NULL, 0, &entry_count);
        bool is_empty = true;

        if (entries)
        {
            for (int i = 0; entries[i]; ++i)
            {
                if (!streq(entries[i], ".") && !streq(entries[i], ".."))
                {
                    is_empty = false;
                    break;
                }
            }
            SDL_free(entries);
        }

        if (is_empty)
        {
            if (SDL_RemovePath(metarun_dir))
            {
                log_info("init_file_paths: removed empty legacy metaruns directory '%s'",
                    metarun_dir);
            }
        }
        return;
    }

    char target[1024];
    if (!path_build(target, sizeof(target), meta_root, META_RAW))
        return;

    SDL_PathInfo target_info;
    bool target_exists = SDL_GetPathInfo(target, &target_info)
        && target_info.type == SDL_PATHTYPE_FILE;

    if (target_exists)
    {
        if (!SDL_RemovePath(legacy))
        {
            log_warn("init_file_paths: failed to remove legacy metarun file '%s': %s",
                legacy, SDL_GetError());
        }
        else
        {
            log_info("init_file_paths: removed duplicate legacy metarun file '%s'",
                legacy);

            /* Try to remove the now-empty metaruns directory */
            if (SDL_RemovePath(metarun_dir))
            {
                log_info("init_file_paths: removed empty legacy metaruns directory '%s'",
                    metarun_dir);
            }
        }
        return;
    }

    if (!SDL_RenamePath(legacy, target))
    {
        log_warn("init_file_paths: failed to migrate legacy metarun file '%s' -> '%s': %s",
            legacy, target, SDL_GetError());
    }
    else
    {
        log_info("init_file_paths: migrated legacy metarun file to '%s'", target);

        /* Try to remove the now-empty metaruns directory */
        if (SDL_RemovePath(metarun_dir))
        {
            log_info("init_file_paths: removed empty legacy metaruns directory '%s'",
                metarun_dir);
        }
    }
}
#endif /* SIL_USE_LOCAL_DATA */

/*
 * Find the default paths to all of our important sub-directories.
 */
void init_file_paths(char* path)
{
    char* tail;
    char buf[1024];

    /*** Free everything ***/

    /* Free the main path */
    str_free(ANGBAND_DIR);

    /* Free the sub-paths */
    str_free(ANGBAND_DIR_APEX);
    str_free(ANGBAND_DIR_METARUN);
    str_free(ANGBAND_DIR_BONE);
    str_free(ANGBAND_DIR_DATA);
    str_free(ANGBAND_DIR_EDIT);
    str_free(ANGBAND_DIR_FILE);
    str_free(ANGBAND_DIR_HELP);
    str_free(ANGBAND_DIR_INFO);
    str_free(ANGBAND_DIR_SAVE);
    str_free(ANGBAND_DIR_PREF);
    str_free(ANGBAND_DIR_USER);
    str_free(ANGBAND_DIR_XTRA);
    str_free(ANGBAND_DIR_SCRIPT);

    /*** Prepare the "path" ***/

    /* Hack -- save the main directory */
    ANGBAND_DIR = str_dup(path);

    /* Prepare to append to the Base Path */
    tail = path + strlen(path);

#ifdef VM

    /*** Use "flat" paths with VM/ESA ***/

    /* Use "blank" path names */
    ANGBAND_DIR_APEX = str_dup("");
    ANGBAND_DIR_BONE = str_dup("");
    ANGBAND_DIR_DATA = str_dup("");
    ANGBAND_DIR_EDIT = str_dup("");
    ANGBAND_DIR_FILE = str_dup("");
    ANGBAND_DIR_HELP = str_dup("");
    ANGBAND_DIR_INFO = str_dup("");
    ANGBAND_DIR_SAVE = str_dup("");
    ANGBAND_DIR_PREF = str_dup("");
    ANGBAND_DIR_USER = str_dup("");
    ANGBAND_DIR_XTRA = str_dup("");
    ANGBAND_DIR_SCRIPT = str_dup("");

#else /* VM */

    /*** Build the sub-directory names ***/

    strcpy(tail, "edit");
    ANGBAND_DIR_EDIT = str_dup(path);
    strcpy(tail, "pref");
    ANGBAND_DIR_PREF = str_dup(path);

    strcpy(tail, "pref");
    ANGBAND_DIR_PREF = str_dup(path);

#ifdef SIL_USE_LOCAL_DATA
    if (path_build(buf, sizeof(buf), ANGBAND_DIR, "user"))
        ANGBAND_DIR_USER = str_dup(buf);
    else
        ANGBAND_DIR_USER = str_dup(ANGBAND_DIR);

    if (path_build(buf, sizeof(buf), ANGBAND_DIR, "data"))
        ANGBAND_DIR_DATA = str_dup(buf);
    else
        ANGBAND_DIR_DATA = str_dup(ANGBAND_DIR);

    if (path_build(buf, sizeof(buf), ANGBAND_DIR, "save"))
        ANGBAND_DIR_SAVE = str_dup(buf);
    else
        ANGBAND_DIR_SAVE = str_dup(ANGBAND_DIR);

    if (path_build(buf, sizeof(buf), ANGBAND_DIR, "apex"))
        ANGBAND_DIR_APEX = str_dup(buf);
    else
        ANGBAND_DIR_APEX = str_dup(ANGBAND_DIR);

    if (path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, SIL_USER_META_RUNS))
        ANGBAND_DIR_METARUN = str_dup(buf);
    else
        ANGBAND_DIR_METARUN = str_dup(ANGBAND_DIR_APEX);
#else
    char user_root[1024];
    if (!build_user_root_path(user_root, sizeof(user_root)))
    {
        SDL_strlcpy(user_root, SIL_USER_ROOT, sizeof(user_root));
        log_warn("init_file_paths: defaulting user path to '%s' relative to the working directory",
            user_root);
    }

    ensure_directory_exists(user_root, "user root");
    ANGBAND_DIR_USER = str_dup(user_root);

    if (path_build(buf, sizeof(buf), user_root, SIL_USER_DATA_DIR))
    {
        ensure_directory_exists(buf, "data");
        ANGBAND_DIR_DATA = str_dup(buf);
    }
    else
    {
        ANGBAND_DIR_DATA = str_dup(user_root);
    }

    if (path_build(buf, sizeof(buf), user_root, SIL_USER_SAVE_DIR))
    {
        ensure_directory_exists(buf, "save");
        ANGBAND_DIR_SAVE = str_dup(buf);
    }
    else
    {
        ANGBAND_DIR_SAVE = str_dup(user_root);
    }

    /* Set ANGBAND_DIR_APEX to the actual apex directory in game folder */
    if (path_build(buf, sizeof(buf), ANGBAND_DIR, "apex"))
        ANGBAND_DIR_APEX = str_dup(buf);
    else
        ANGBAND_DIR_APEX = str_dup(ANGBAND_DIR);

    /* Set up meta directory for scores.raw and metarun data */
    char meta_root[1024];
    if (path_build(meta_root, sizeof(meta_root), user_root, SIL_USER_META_DIR))
    {
        ensure_directory_exists(meta_root, "meta");

        char metarun_dir[1024];
        if (path_build(
                metarun_dir, sizeof(metarun_dir), meta_root, SIL_USER_META_RUNS))
        {
            ensure_directory_exists(metarun_dir, "metarun");
            ANGBAND_DIR_METARUN = str_dup(metarun_dir);
        }
        else
        {
            ANGBAND_DIR_METARUN = str_dup(meta_root);
        }
    }
    else
    {
        ANGBAND_DIR_METARUN = str_dup(user_root);
    }

    migrate_legacy_metarun_layout(meta_root, ANGBAND_DIR_METARUN);
    seed_user_data_from_install(ANGBAND_DIR_DATA);
#if !defined(__ANDROID__) && !defined(SIL_IOS)
    seed_user_meta_from_install(meta_root, ANGBAND_DIR_METARUN);
    seed_user_saves_from_install(ANGBAND_DIR_SAVE);
#endif
    seed_sound_config(user_root);
#endif /* SIL_USE_LOCAL_DATA */

    strcpy(tail, "xtra");
    ANGBAND_DIR_XTRA = str_dup(path);

    strcpy(tail, "script");
    ANGBAND_DIR_SCRIPT = str_dup(path);

#endif /* VM */

#ifdef NeXT

    /* Allow "fat binary" usage with NeXT */
    if (true)
    {
        cptr next = NULL;

#if defined(m68k)
        next = "m68k";
#endif

#if defined(i386)
        next = "i386";
#endif

#if defined(sparc)
        next = "sparc";
#endif

#if defined(hppa)
        next = "hppa";
#endif

        /* Use special directory */
        if (next)
        {
            /* Forget the old path name */
            str_free(ANGBAND_DIR_DATA);

            /* Build a new path name */
            sprintf(tail, "data-%s", next);
            ANGBAND_DIR_DATA = str_dup(path);
        }
    }

#endif /* NeXT */
}

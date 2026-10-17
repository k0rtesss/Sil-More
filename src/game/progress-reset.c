#include "angband.h"
#include "externs.h"
#include "game/progress-reset.h"
#include "fs/path.h"
#include "log/log.h"
#include <SDL3/SDL.h>
#ifdef SIL_USE_LOCAL_DATA
#include "sound-defaults.h"
#endif

#if defined(WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#if !defined(__ANDROID__) && !defined(SIL_IOS)
#include <signal.h>
#endif
#endif
#ifdef __ANDROID__
#include <jni.h>
#endif

#define RESET_MARKER ".sil-more-progress-reset"
#define RESET_PARENT_ARG "--progress-reset-parent="
#define RESET_PATH_MAX 4096

static char reset_user[RESET_PATH_MAX];
static char reset_save[RESET_PATH_MAX];
static char reset_apex[RESET_PATH_MAX];
static char reset_metarun[RESET_PATH_MAX];
static char reset_marker[RESET_PATH_MAX];
static char reset_install[RESET_PATH_MAX];
static char reset_runtime_apex[RESET_PATH_MAX];
#if !defined(__ANDROID__) && !defined(SIL_IOS)
static char reset_executable[RESET_PATH_MAX];
#endif
static bool reset_paths_ready;
static bool reset_skip_import;
static bool reset_restarting;

bool sil_progress_reset_restarting(void) { return reset_restarting; }
bool sil_progress_reset_skip_legacy_import(void) { return reset_skip_import; }

/* Inspect the entry itself, never the destination of a link. In particular,
 * SDL_GetPathInfo follows symlinks and cannot guard a recursive erase. */
static int reset_path_type(const char* path)
{
#ifdef WINDOWS
    wchar_t* wide = (wchar_t*)SDL_iconv_string("WCHAR_T", "UTF-8", path,
        SDL_strlen(path) + 1);
    if (!wide) return -1;
    DWORD attr = GetFileAttributesW(wide);
    DWORD error = GetLastError();
    SDL_free(wide);
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return 0;
        SDL_SetError("Cannot inspect reset path '%s' (Windows error %lu)",
            path, (unsigned long)error);
        return -1;
    }
    if (attr & FILE_ATTRIBUTE_REPARSE_POINT)
        return SDL_SetError("Reset cannot follow a link or junction: %s", path), -1;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 2 : 1;
#else
    struct stat info;
    if (lstat(path, &info) != 0)
    {
        if (errno == ENOENT) return 0;
        return SDL_SetError("Cannot inspect reset path '%s': %s", path,
            strerror(errno)), -1;
    }
    if (S_ISLNK(info.st_mode))
        return SDL_SetError("Reset cannot follow a symbolic link: %s", path), -1;
    if (S_ISDIR(info.st_mode)) return 2;
    if (S_ISREG(info.st_mode)) return 1;
    return SDL_SetError("Reset found an unsupported file: %s", path), -1;
#endif
}

static bool reset_absolute_path(char* out, const char* path)
{
    if (!path || !path[0]) return false;
#ifdef WINDOWS
    wchar_t* wide = (wchar_t*)SDL_iconv_string("WCHAR_T", "UTF-8", path,
        SDL_strlen(path) + 1);
    wchar_t full[RESET_PATH_MAX];
    DWORD length = wide ? GetFullPathNameW(wide, RESET_PATH_MAX, full, NULL) : 0;
    SDL_free(wide);
    if (!length || length >= RESET_PATH_MAX) return false;
    char* utf8 = SDL_iconv_string("UTF-8", "WCHAR_T", (char*)full,
        (wcslen(full) + 1) * sizeof(wchar_t));
    if (!utf8) return false;
    bool ok = SDL_strlcpy(out, utf8, RESET_PATH_MAX) < RESET_PATH_MAX;
    SDL_free(utf8);
    if (!ok) return false;
    for (char* p = out; *p; ++p) if (*p == '\\') *p = '/';
#else
    char* full = realpath(path, NULL);
    if (!full) return false;
    bool ok = SDL_strlcpy(out, full, RESET_PATH_MAX) < RESET_PATH_MAX;
    free(full);
    if (!ok) return false;
#endif
    size_t len = strlen(out);
    while (len > 1 && out[len - 1] == '/') out[--len] = '\0';
    return len > 3;
}

static bool reset_same_path(const char* left, const char* right)
{
    for (; *left && *right; ++left, ++right)
    {
        char a = *left, b = *right;
#ifdef WINDOWS
        if (a == '\\') a = '/';
        if (b == '\\') b = '/';
        a = (char)SDL_tolower((unsigned char)a);
        b = (char)SDL_tolower((unsigned char)b);
#endif
        if (a != b) return false;
    }
    return *left == *right;
}

static bool reset_contains(const char* parent, const char* child)
{
    size_t len = strlen(parent);
    if (strlen(child) < len) return false;
    char part[RESET_PATH_MAX];
    SDL_strlcpy(part, child, len + 1);
    return reset_same_path(parent, part)
        && (child[len] == '/' || child[len] == '\\' || child[len] == '\0');
}

struct reset_walk { bool erase; int depth; };
static bool reset_walk_tree(const char* path, bool erase, int depth);

static SDL_EnumerationResult SDLCALL reset_walk_entry(void* data,
    const char* dir, const char* name)
{
    struct reset_walk* walk = data;
    char path[RESET_PATH_MAX];
    if (streq(name, ".") || streq(name, "..")) return SDL_ENUM_CONTINUE;
    /* This is the only retained file in the owned user directory. Its completed
     * state also prevents old install saves from being imported on later boots. */
    if (walk->depth == 0 && reset_same_path(name, RESET_MARKER)
        && reset_contains(reset_user, dir) && strlen(dir) == strlen(reset_user) + 1)
        return SDL_ENUM_CONTINUE;
    /* Mobile diagnostics are open before paths are initialized. They contain
     * no progression and must remain available if the reset needs recovery. */
    if (walk->depth == 0 && (reset_same_path(name, "log.txt")
        || reset_same_path(name, "generation.txt")
        || reset_same_path(name, "generation-summary.txt"))
        && reset_contains(reset_user, dir) && strlen(dir) == strlen(reset_user) + 1)
        return SDL_ENUM_CONTINUE;
    if (!path_build(path, sizeof(path), dir, name)
        || !reset_walk_tree(path, walk->erase, walk->depth + 1))
        return SDL_ENUM_FAILURE;
    return SDL_ENUM_CONTINUE;
}

static bool reset_walk_tree(const char* path, bool erase, int depth)
{
    if (depth > 64) return SDL_SetError("Reset directory nesting is too deep");
    int kind = reset_path_type(path);
    if (kind < 0) return false;
    if (!kind) return true;
    if (kind == 2)
    {
        struct reset_walk walk = { erase, depth };
        if (!SDL_EnumerateDirectory(path, reset_walk_entry, &walk)) return false;
    }
    return !erase || depth == 0 || SDL_RemovePath(path);
}

static bool reset_check_roots(void)
{
    if (!reset_paths_ready) return SDL_SetError("Reset paths are unavailable");
    const char* roots[] = { reset_user, reset_save, reset_apex };
    for (int i = 0; i < (int)N_ELEMENTS(roots); ++i)
    {
        if (!roots[i][0]) continue;
        int kind = reset_path_type(roots[i]);
        if (kind < 0) return false;
        if (kind != 0 && kind != 2)
            return SDL_SetError("A progress directory is not a directory: %s", roots[i]);
        if (reset_install[0] && reset_contains(roots[i], reset_install))
            return SDL_SetError("A progress directory contains the game installation");
        if (!reset_walk_tree(roots[i], false, 0)) return false;
    }
#ifdef SIL_USE_LOCAL_DATA
    char pref[RESET_PATH_MAX], sound[RESET_PATH_MAX];
    if (!path_build(pref, sizeof(pref), reset_install, "pref")
        || reset_path_type(pref) != 2
        || !path_build(sound, sizeof(sound), pref, "sound.json")
        || reset_path_type(sound) < 0)
        return false;
#endif
    /* The marker itself is excluded by the walk, so validate it explicitly. */
    int marker_kind = reset_path_type(reset_marker);
    return marker_kind >= 0 && marker_kind <= 1;
}

static bool reset_write_marker(const char* state)
{
    char temporary[RESET_PATH_MAX];
    if (!path_build(temporary, sizeof(temporary), reset_user, RESET_MARKER ".tmp")
        || reset_path_type(temporary) < 0
        || reset_path_type(reset_marker) < 0)
        return false;
    SDL_IOStream* stream = SDL_IOFromFile(temporary, "wb");
    if (!stream) return false;
    bool ok = SDL_WriteIO(stream, state, strlen(state)) == strlen(state);
    if (!SDL_FlushIO(stream)) ok = false;
    if (!SDL_CloseIO(stream)) ok = false;
    if (ok) ok = SDL_RenamePath(temporary, reset_marker);
    if (!ok) SDL_RemovePath(temporary);
    return ok;
}

bool sil_progress_reset_startup(const char* install_root, const char* user_root)
{
    char install[RESET_PATH_MAX], user[RESET_PATH_MAX], marker[RESET_PATH_MAX];
    reset_paths_ready = false;
    reset_skip_import = false;
    if (!path_build(marker, sizeof(marker), user_root, RESET_MARKER)) return false;
    int marker_kind = reset_path_type(marker);
    if (marker_kind < 0) return false;
    if (!SDL_CreateDirectory(user_root) || !reset_absolute_path(user, user_root))
        return !marker_kind; /* A requested reset must never fall through. */
#if defined(__ANDROID__)
    /* Android assets are virtual APK entries, not writable directories. */
    install[0] = '\0';
#else
    if (!reset_absolute_path(install, install_root)) return !marker_kind;
#endif
    SDL_strlcpy(reset_install, install, sizeof(reset_install));
    SDL_strlcpy(reset_user, user, sizeof(reset_user));
    if (!path_build(reset_runtime_apex, sizeof(reset_runtime_apex), install_root, "apex")
#ifdef SIL_USE_LOCAL_DATA
        || !path_build(reset_save, sizeof(reset_save), install, "save")
        || !path_build(reset_metarun, sizeof(reset_metarun), install, "apex")
#else
        || !path_build(reset_save, sizeof(reset_save), user, "save")
        || !path_build(reset_metarun, sizeof(reset_metarun), user, "meta/metaruns")
#endif
        || !path_build(reset_marker, sizeof(reset_marker), user, RESET_MARKER))
        return false;
#if defined(__ANDROID__) || defined(SIL_IOS)
    /* Mobile install assets cannot contain writable personal progress. The
     * owned user tree contains all local saves, meta data and preferences. */
    reset_apex[0] = '\0';
#else
    if (!path_build(reset_apex, sizeof(reset_apex), install, "apex")) return false;
#endif
    reset_paths_ready = true;
    marker_kind = reset_path_type(reset_marker);
    if (!marker_kind) return true;
    if (marker_kind != 1) return false;
    char state[16] = { 0 };
    SDL_IOStream* stream = SDL_IOFromFile(reset_marker, "rb");
    if (!stream) return false;
    size_t length = SDL_ReadIO(stream, state, sizeof(state) - 1);
    SDL_CloseIO(stream);
    if (length == 9 && streq(state, "complete\n"))
    {
        reset_skip_import = true;
        return true;
    }
    if (length != 8 || !streq(state, "pending\n"))
        return SDL_SetError("The progress reset request is invalid");
    /* Reject a redirected user root before using its canonical destination.
     * A normal launch through such a path is still supported. */
    if (reset_path_type(user_root) < 0) return false;
    if (!reset_check_roots()) return false;
    /* No engine, achievement provider, autosave or quit hook is alive yet.
     * Repeating this after a partial failure is safe; leave pending on failure. */
    if ((reset_apex[0] && !reset_walk_tree(reset_apex, true, 0))
        || !reset_walk_tree(reset_save, true, 0)
        || !reset_walk_tree(reset_user, true, 0))
        return false;
#ifdef SIL_USE_LOCAL_DATA
    /* Portable settings write to lib/pref/sound.json. Keep shipped pref assets
     * and replace only that mutable sound configuration with factory values. */
    char sound[RESET_PATH_MAX], temporary[RESET_PATH_MAX];
    if (!path_build(sound, sizeof(sound), install, "pref/sound.json")
        || !path_build(temporary, sizeof(temporary), user, "sound-reset.tmp"))
        return false;
    if (!SDL_SaveFile(temporary, sil_factory_sound_json,
            sizeof(sil_factory_sound_json) - 1)
        || !SDL_RenamePath(temporary, sound)) return false;
#endif
    if (!reset_write_marker("complete\n")) return false;
    reset_skip_import = true;
    log_info("All local progress and preferences cleared before startup");
    return true;
}

bool sil_progress_reset_launch_init(int* argc, char*** argv)
{
#if !defined(__ANDROID__) && !defined(SIL_IOS)
    /* Wait before opening logs or any progress file. The parent must release
     * its autosaves, workers and open file handles before deletion can begin. */
    if (*argc > 1 && prefix((*argv)[1], RESET_PARENT_ARG))
    {
        char* end;
        unsigned long parent = strtoul((*argv)[1] + strlen(RESET_PARENT_ARG), &end, 10);
        if (*end || !parent) return false;
#ifdef WINDOWS
        if (parent == GetCurrentProcessId()) return false;
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)parent);
        if (process)
        {
            DWORD result = WaitForSingleObject(process, 15000);
            CloseHandle(process);
            if (result != WAIT_OBJECT_0) return false;
        }
        else if (GetLastError() != ERROR_INVALID_PARAMETER) return false;
#else
        if (parent == (unsigned long)getpid() || parent > INT_MAX) return false;
        Uint64 until = SDL_GetTicks() + 15000;
        while (kill((pid_t)parent, 0) == 0 || errno == EPERM)
        {
            if (SDL_GetTicks() >= until) return false;
            SDL_Delay(10);
        }
        if (errno != ESRCH) return false;
#endif
        for (int i = 1; i < *argc; ++i) (*argv)[i] = (*argv)[i + 1];
        --*argc;
    }
#ifdef WINDOWS
    wchar_t full[RESET_PATH_MAX];
    DWORD length = GetModuleFileNameW(NULL, full, RESET_PATH_MAX);
    if (!length || length >= RESET_PATH_MAX) return false;
    char* utf8 = SDL_iconv_string("UTF-8", "WCHAR_T", (char*)full,
        (wcslen(full) + 1) * sizeof(wchar_t));
    if (!utf8) return false;
    bool ok = SDL_strlcpy(reset_executable, utf8, sizeof(reset_executable))
        < sizeof(reset_executable);
    SDL_free(utf8);
    return ok;
#else
    if (!reset_absolute_path(reset_executable, (*argv)[0]))
    {
        const char* base = SDL_GetBasePath();
        const char* name = strrchr((*argv)[0], '/');
        if (base) (void)path_build(reset_executable, sizeof(reset_executable),
            base, name ? name + 1 : (*argv)[0]);
    }
    return true;
#endif
#else
    (void)argc;
    (void)argv;
    return true;
#endif
}

static bool reset_launch_app(void)
{
#ifdef __ANDROID__
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    if (!env) return false;
    jobject activity = (jobject)SDL_GetAndroidActivity();
    jclass cls = activity ? (*env)->GetObjectClass(env, activity) : NULL;
    jmethodID method = cls ? (*env)->GetMethodID(env, cls,
        "restartAfterProgressReset", "()Z") : NULL;
    bool ok = method && (*env)->CallBooleanMethod(env, activity, method) == JNI_TRUE;
    if ((*env)->ExceptionCheck(env))
    {
        (*env)->ExceptionClear(env);
        ok = false;
    }
    if (cls) (*env)->DeleteLocalRef(env, cls);
    if (activity) (*env)->DeleteLocalRef(env, activity);
    return ok;
#elif defined(SIL_IOS)
    /* iOS provides no supported process relaunch. The final confirmation tells
     * the player to reopen; the durable request runs before that startup. */
    return true;
#else
    if (!reset_executable[0]) return SDL_SetError("The app executable is unavailable");
    char parent[80];
#ifdef WINDOWS
    unsigned long pid = GetCurrentProcessId();
#else
    unsigned long pid = (unsigned long)getpid();
#endif
    strnfmt(parent, sizeof(parent), RESET_PARENT_ARG "%lu", pid);
    const char* args[] = { reset_executable, parent, NULL };
    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) return false;
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    SDL_Process* process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process) return false;
    SDL_DestroyProcess(process);
    return true;
#endif
}

bool sil_progress_reset_request_restart(char* error, size_t error_size)
{
    char user[RESET_PATH_MAX], save[RESET_PATH_MAX];
#if !defined(__ANDROID__) && !defined(SIL_IOS)
    char apex[RESET_PATH_MAX];
#endif
    char metarun[RESET_PATH_MAX];
    if (!reset_paths_ready
        || reset_path_type(ANGBAND_DIR_USER) < 0
        || reset_path_type(ANGBAND_DIR_SAVE) < 0
        || reset_path_type(ANGBAND_DIR_METARUN) < 0
        || !reset_absolute_path(user, ANGBAND_DIR_USER)
        || !reset_absolute_path(save, ANGBAND_DIR_SAVE)
        || !reset_absolute_path(metarun, ANGBAND_DIR_METARUN)
        || !reset_same_path(user, reset_user) || !reset_same_path(save, reset_save)
        || !reset_same_path(metarun, reset_metarun)
#if defined(__ANDROID__) || defined(SIL_IOS)
        || !reset_same_path(ANGBAND_DIR_APEX, reset_runtime_apex)
#else
        || reset_path_type(ANGBAND_DIR_APEX) < 0
        || !reset_absolute_path(apex, ANGBAND_DIR_APEX)
        || !reset_same_path(apex, reset_apex)
#endif
        )
    {
        SDL_strlcpy(error, "Reset is unavailable with custom progress directories. "
            "Relaunch the app with its normal directories and try again.", error_size);
        return false;
    }
    if (!reset_check_roots() || !reset_write_marker("pending\n"))
    {
        strnfmt(error, error_size, "Could not prepare the reset: %s", SDL_GetError());
        return false;
    }
    reset_restarting = true;
    if (!reset_launch_app())
    {
        strnfmt(error, error_size, "Could not restart the app: %s", SDL_GetError());
        reset_restarting = false;
        /* Preserve any previous opt-out from importing legacy install saves. */
        bool cancelled = reset_skip_import ? reset_write_marker("complete\n")
            : SDL_RemovePath(reset_marker);
        if (!cancelled)
            SDL_strlcat(error, " The reset remains queued. Reopen the app to finish it.", error_size);
        return false;
    }
    quit(NULL);
    return true;
}

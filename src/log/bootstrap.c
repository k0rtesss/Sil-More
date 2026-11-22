/* log/bootstrap.c - Logger initialization */

#include "../angband.h"
#include "bootstrap.h"
#include "../fs/io_sdl.h"
#include "../fs/path.h"
#include "log.h"
#include <time.h>

#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#endif

static FILE* open_log_in(const char* directory, const char* name,
    char* path, size_t size)
{
    if (!directory || !*directory || !path_build(path, size, directory, name))
        return NULL;
    return file_open_utf8(path, "w");
}

void init_logger(bool quiet, const char* directory)
{
#if defined(__ANDROID__) || defined(SIL_IOS) || defined(NDEBUG)
    int level = LOG_INFO;
#else
    int level = LOG_DEBUG;
#endif
    const char* requested_level = getenv("SIL_LOG_LEVEL");
    if (requested_level && *requested_level)
    {
        for (level = LOG_TRACE; level <= LOG_FATAL; ++level)
            if (strcmp(requested_level, log_level_string(level)) == 0)
                break;
        if (level > LOG_FATAL)
        {
            level = LOG_INFO;
            log_warn("Unknown log level %s, using INFO", requested_level);
        }
    }

    const char* name = "log.txt";
#if LOG_MODE_TIMESTAMP && !defined(__ANDROID__) && !defined(SIL_IOS)
    char timestamped[32];
    time_t now = time(NULL);
    struct tm* when = localtime(&now);
    if (when && strftime(timestamped, sizeof(timestamped), "log_%Y%m%d_%H%M%S.txt", when))
        name = timestamped;
#endif

    char path[1024];
    const char* first = directory;
#ifdef __ANDROID__
    if (!first) first = SDL_GetAndroidInternalStoragePath();
#elif defined(SIL_IOS)
    if (!first) first = SDL_getenv("TMPDIR");
#elif defined(SIL_USE_LOCAL_DATA)
    if (!first) first = SDL_GetBasePath();
#endif
    FILE* file = open_log_in(first, name, path, sizeof(path));
#ifdef __ANDROID__
    if (!file && !directory)
        file = open_log_in(SDL_getenv("TMPDIR"), name, path, sizeof(path));
#endif
    if (!file && !directory)
    {
        char* prefs = SDL_GetPrefPath("Sil-QH", "sil-more");
        file = open_log_in(prefs, name, path, sizeof(path));
        SDL_free(prefs);
    }

    log_set_level(level);
    if (!file || log_add_fp(file, level) != 0)
    {
        if (file) fclose(file);
        log_set_quiet(false);
        log_warn("File logging is unavailable, writing diagnostics to stderr");
        return;
    }

    log_set_quiet(quiet);
    log_info("logger initialised with level %d at %s", level, path);
    atexit(log_close_files);
}

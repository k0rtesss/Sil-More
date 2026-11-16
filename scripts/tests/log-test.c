#include "../../src/log/log.h"
#include <assert.h>
#include <string.h>

static int locks, calls, last_level;
static void fixture_lock(bool locked, void* data)
{
    (void)data;
    if (locked) ++locks;
}
static void fixture_callback(log_Event* event)
{
    ++calls;
    last_level = event->level;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    log_set_lock(fixture_lock, NULL);
    log_set_quiet(true);
    log_set_level(LOG_INFO);
    assert(!log_is_enabled(LOG_DEBUG));
    log_debug("disabled");
    assert(!locks && !calls);

    FILE* file = fopen(argv[1], "w");
    assert(file && log_add_fp(file, LOG_WARN) == 0);
    assert(!log_is_enabled(LOG_INFO) && log_is_enabled(LOG_WARN));
    log_warn("warning survives filtering");
    FILE* reader = fopen(argv[1], "r");
    char line[512];
    assert(reader && fgets(line, sizeof(line), reader));
    assert(strstr(line, "warning survives filtering"));
    fclose(reader);

    /* A more verbose callback remains independent of stderr's threshold. */
    assert(log_add_callback(fixture_callback, NULL, LOG_TRACE) == 0);
    assert(log_is_enabled(LOG_TRACE));
    log_trace("callback trace");
    assert(calls == 1 && last_level == LOG_TRACE);
    log_set_level(LOG_FATAL);
    log_set_quiet(false);
    assert(log_is_enabled(LOG_DEBUG));
    log_debug("callback debug");
    assert(calls == 2 && last_level == LOG_DEBUG);
    log_close_files();
    puts("Logging filtering, callback thresholds and warning flush: PASS");
    return 0;
}

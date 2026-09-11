#ifndef SIL_LOG_PERF_H
#define SIL_LOG_PERF_H

#include <SDL3/SDL.h>

/* Optional, main-thread-only diagnostics. Labels must be string literals.
 * Nested phases are inclusive; actual event waits are subtracted. */
typedef struct {
    Uint64 wall_ns;
    Uint64 wait_ns;
} sil_perf_stamp;

#ifdef SIL_PERF_DIAGNOSTICS
bool sil_perf_enabled(void);
sil_perf_stamp sil_perf_begin(void);
void sil_perf_end(const char* label, sil_perf_stamp start);
void sil_perf_wait_end(sil_perf_stamp start);
void sil_perf_flush(void);
#else
/* Standalone engine fixtures do not need the diagnostic backend. */
static inline bool sil_perf_enabled(void) { return false; }
static inline sil_perf_stamp sil_perf_begin(void)
{ return (sil_perf_stamp){0, 0}; }
static inline void sil_perf_end(const char* label, sil_perf_stamp start)
{ (void)label; (void)start; }
static inline void sil_perf_wait_end(sil_perf_stamp start) { (void)start; }
static inline void sil_perf_flush(void) {}
#endif

#define SIL_PERF_PHASE(label, stmt) do { \
    sil_perf_stamp sil_phase_start = sil_perf_begin(); \
    stmt; \
    sil_perf_end(label, sil_phase_start); \
} while (0)

#endif

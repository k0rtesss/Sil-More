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
/* Temporary targeted diagnostics: INFO-level wall-clock landing-to-popup
 * traces, independent of SIL_PERF. Main thread only; no gameplay changes. */
void sil_popup_trace_begin(int from_y, int from_x, int to_y, int to_x);
void sil_popup_trace_stage(const char* stage);
void sil_popup_trace_end(const char* reason);
void sil_popup_trace_frame_begin(void);
void sil_popup_trace_drawn(void);
void sil_popup_trace_player_drawn(int y, int x);
void sil_popup_trace_presented(void);
Uint64 sil_popup_trace_phase_begin(void);
void sil_popup_trace_phase_end(const char* phase, Uint64 started_ns);
#else
/* Standalone engine fixtures do not need the diagnostic backend. */
static inline bool sil_perf_enabled(void) { return false; }
static inline sil_perf_stamp sil_perf_begin(void)
{ return (sil_perf_stamp){0, 0}; }
static inline void sil_perf_end(const char* label, sil_perf_stamp start)
{ (void)label; (void)start; }
static inline void sil_perf_wait_end(sil_perf_stamp start) { (void)start; }
static inline void sil_perf_flush(void) {}
static inline void sil_popup_trace_begin(int fy, int fx, int ty, int tx)
{ (void)fy; (void)fx; (void)ty; (void)tx; }
static inline void sil_popup_trace_stage(const char* stage) { (void)stage; }
static inline void sil_popup_trace_end(const char* reason) { (void)reason; }
static inline void sil_popup_trace_frame_begin(void) {}
static inline void sil_popup_trace_drawn(void) {}
static inline void sil_popup_trace_player_drawn(int y, int x)
{ (void)y; (void)x; }
static inline void sil_popup_trace_presented(void) {}
static inline Uint64 sil_popup_trace_phase_begin(void) { return 0; }
static inline void sil_popup_trace_phase_end(const char* phase, Uint64 ns)
{ (void)phase; (void)ns; }
#endif

#define SIL_PERF_PHASE(label, stmt) do { \
    sil_perf_stamp sil_phase_start = sil_perf_begin(); \
    stmt; \
    sil_perf_end(label, sil_phase_start); \
} while (0)

#endif

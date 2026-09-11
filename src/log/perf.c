#include "angband.h"
#include "externs.h"
#include "log/perf.h"
#include "log/log.h"

typedef struct {
    const char* label;
    Uint64 calls, total_ns, max_ns;
} perf_bucket;

static perf_bucket buckets[48];
static int bucket_count;
static Uint64 wait_ns, last_flush_ns;

bool sil_perf_enabled(void)
{
    static int enabled = -1;
    if (enabled < 0)
    {
        const char* value = getenv("SIL_PERF");
        enabled = value && strcmp(value, "1") == 0;
    }
    return enabled != 0;
}

sil_perf_stamp sil_perf_begin(void)
{
    if (!sil_perf_enabled()) return (sil_perf_stamp){0, 0};
    return (sil_perf_stamp){SDL_GetTicksNS(), wait_ns};
}

void sil_perf_wait_end(sil_perf_stamp start)
{
    if (start.wall_ns) wait_ns += SDL_GetTicksNS() - start.wall_ns;
}

void sil_perf_end(const char* label, sil_perf_stamp start)
{
    if (!start.wall_ns) return;
    Uint64 elapsed = SDL_GetTicksNS() - start.wall_ns;
    Uint64 idle = wait_ns - start.wait_ns;
    elapsed = elapsed > idle ? elapsed - idle : 0;
    int i;
    for (i = 0; i < bucket_count; i++)
        if (strcmp(buckets[i].label, label) == 0) break;
    if (i == bucket_count)
    {
        if (bucket_count == (int)N_ELEMENTS(buckets)) return;
        buckets[bucket_count++].label = label;
    }
    buckets[i].calls++;
    buckets[i].total_ns += elapsed;
    if (elapsed > buckets[i].max_ns) buckets[i].max_ns = elapsed;
}

/* Flush only at the input boundary, never during a path search or while the
 * sound lock is held. At most one summary/second; the logger's own I/O is
 * excluded from any enclosing phase. Counts remain useful for repeated work. */
void sil_perf_flush(void)
{
    if (!sil_perf_enabled()) return;
    Uint64 now = SDL_GetTicksNS();
    if (now - last_flush_ns < 1000000000ULL) return;
    last_flush_ns = now;
    bool any = false;
    for (int i = 0; i < bucket_count; i++) any |= buckets[i].calls != 0;
    if (!any) return;
    log_info("[PERF] turn=%ld depth=%d map=%dx%d monsters=%d (inclusive phases, input wait excluded)",
        (long)playerturn, p_ptr->depth, p_ptr->cur_map_wid,
        p_ptr->cur_map_hgt, mon_cnt);
    for (int i = 0; i < bucket_count; i++)
    {
        perf_bucket* b = &buckets[i];
        if (b->calls)
            log_info("[PERF] %s calls=%llu total_ms=%.2f max_ms=%.2f",
                b->label, (unsigned long long)b->calls,
                b->total_ns / 1000000.0, b->max_ns / 1000000.0);
        b->calls = b->total_ns = b->max_ns = 0;
    }
    wait_ns += SDL_GetTicksNS() - now;
}

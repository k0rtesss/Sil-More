#include "angband.h"
#include "externs.h"
#include "log/perf.h"
#include "log/log.h"
#include "tutorial/tutorial.h"

static struct {
    bool active, frame_drawn, first_frame_seen, player_grid_redrawn;
    unsigned int id, frames;
    Uint64 started_ns, io_ns, first_frame_ns;
    int from_y, from_x, to_y, to_x;
    const char* stages[48];
    int stage_count;
} popup_trace;

void sil_popup_trace_stage(const char* stage)
{
    Uint64 now;

    if (!popup_trace.active || !stage)
        return;
    if (!p_ptr || !cave_o_idx || !in_bounds(p_ptr->py, p_ptr->px))
    {
        popup_trace.active = false;
        return;
    }
    /* Idle events and world ticks can repeat without making progress. Log
     * each stage once per landing instead of flooding the normal log. */
    for (int i = 0; i < popup_trace.stage_count; i++)
        if (strcmp(popup_trace.stages[i], stage) == 0)
            return;
    if (popup_trace.stage_count >= (int)N_ELEMENTS(popup_trace.stages))
        return;
    popup_trace.stages[popup_trace.stage_count++] = stage;
    now = SDL_GetTicksNS();
    log_info("[POPUP_DELAY] id=%u stage=%s elapsed_ms=%.3f trace_io_ms=%.3f "
        "turn=%ld from=(%d,%d) target=(%d,%d) player=(%d,%d) floor=%d "
        "energy=%d use=%d action=%d previous=%d running=%d repeat=%d "
        "resting=%d leaping=%d skip=%d entranced=%d stun=%d leaving=%d "
        "dead=%d inkey=%d icky=%d tutorial=%d menu=%d hint=%d "
        "update=0x%lx redraw=0x%lx window=0x%lx frames=%u",
        popup_trace.id, stage, (now - popup_trace.started_ns) / 1000000.0,
        popup_trace.io_ns / 1000000.0, (long)playerturn,
        popup_trace.from_y, popup_trace.from_x,
        popup_trace.to_y, popup_trace.to_x, p_ptr->py, p_ptr->px,
        cave_o_idx[p_ptr->py][p_ptr->px], p_ptr->energy, p_ptr->energy_use,
        p_ptr->previous_action[0], p_ptr->previous_action[1],
        p_ptr->running, p_ptr->command_rep, p_ptr->resting, p_ptr->leaping,
        p_ptr->skip_next_turn, p_ptr->entranced, p_ptr->stun, p_ptr->leaving,
        p_ptr->is_dead, inkey_flag, character_icky, tutorial_is_active(),
        sdl_question_menu_is_active(), sdl_question_menu_context_hint_active(),
        (unsigned long)p_ptr->update, (unsigned long)p_ptr->redraw,
        (unsigned long)p_ptr->window, popup_trace.frames);
    popup_trace.io_ns += SDL_GetTicksNS() - now;
}

void sil_popup_trace_end(const char* reason)
{
    sil_popup_trace_stage(reason);
    popup_trace.active = false;
}

void sil_popup_trace_begin(int from_y, int from_x, int to_y, int to_x)
{
    unsigned int next_id = popup_trace.id + 1;

    if (from_y == to_y && from_x == to_x)
        return;
    sil_popup_trace_end("moved-again-before-popup");
    if (!p_ptr || !cave_o_idx || !character_dungeon
        || !p_ptr->playing || p_ptr->is_dead
        || (!cave_o_idx[to_y][to_x] && !cave_stair_bold(to_y, to_x)
            && !cave_forge_bold(to_y, to_x)))
        return;
    memset(&popup_trace, 0, sizeof(popup_trace));
    popup_trace.active = true;
    popup_trace.id = next_id;
    popup_trace.started_ns = SDL_GetTicksNS();
    popup_trace.from_y = from_y;
    popup_trace.from_x = from_x;
    popup_trace.to_y = to_y;
    popup_trace.to_x = to_x;
    sil_popup_trace_stage("arrival");
}

void sil_popup_trace_frame_begin(void)
{
    popup_trace.frame_drawn = false;
    sil_popup_trace_stage("frame-compose-begin");
}

void sil_popup_trace_drawn(void)
{
    popup_trace.frame_drawn = true;
    sil_popup_trace_stage("popup-draw-complete");
}

void sil_popup_trace_player_drawn(int y, int x)
{
    if (!popup_trace.active || popup_trace.player_grid_redrawn
        || y != popup_trace.to_y || x != popup_trace.to_x
        || p_ptr->py != y || p_ptr->px != x)
        return;
    popup_trace.player_grid_redrawn = true;
    sil_popup_trace_stage("player-cell-drawn");
}

void sil_popup_trace_presented(void)
{
    Uint64 now;
    Uint64 display_io_ns;

    if (!popup_trace.active)
        return;
    now = SDL_GetTicksNS();
    display_io_ns = popup_trace.io_ns;
    popup_trace.frames++;
    if (!popup_trace.first_frame_seen && popup_trace.player_grid_redrawn)
    {
        popup_trace.first_frame_seen = true;
        popup_trace.first_frame_ns = now;
        sil_popup_trace_stage(popup_trace.frame_drawn
            ? "first-frame-with-popup" : "first-frame-without-popup");
    }
    else if (!popup_trace.player_grid_redrawn)
        sil_popup_trace_stage("frame-before-player-grid-redraw");
    if (popup_trace.frame_drawn)
    {
        sil_popup_trace_stage("popup-presented");
        log_info("[POPUP_DELAY] id=%u summary arrival_to_popup_ms=%.3f "
            "player_frame_to_popup_ms=%.3f trace_io_ms=%.3f frames=%u",
            popup_trace.id, (now - popup_trace.started_ns) / 1000000.0,
            popup_trace.first_frame_seen
                ? (now - popup_trace.first_frame_ns) / 1000000.0 : -1.0,
            display_io_ns / 1000000.0, popup_trace.frames);
        popup_trace.active = false;
    }
}

Uint64 sil_popup_trace_phase_begin(void)
{
    return popup_trace.active ? SDL_GetTicksNS() : 0;
}

void sil_popup_trace_phase_end(const char* phase, Uint64 started_ns)
{
    Uint64 now;

    if (!popup_trace.active || !started_ns)
        return;
    now = SDL_GetTicksNS();
    /* The scheduler visits each phase on every energy tick. Retain costly
     * later passes even when the first pass did no work, without idle spam. */
    if (now - started_ns < 1000000ULL)
        return;
    log_info("[POPUP_DELAY] id=%u phase=%s duration_ms=%.3f elapsed_ms=%.3f",
        popup_trace.id, phase, (now - started_ns) / 1000000.0,
        (now - popup_trace.started_ns) / 1000000.0);
    popup_trace.io_ns += SDL_GetTicksNS() - now;
}

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

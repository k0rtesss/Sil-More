#ifndef SIL_ACHIEVEMENTS_GAME_CENTER_STATE_H
#define SIL_ACHIEVEMENTS_GAME_CENTER_STATE_H

#include "achievements.h"
#include <string.h>

/* Main-thread state, separate from GameKit so retries and stale asynchronous
 * replies can be exercised without an Apple account or SDK. The local profile
 * remains the durable queue. A provider reply alone confirms an upload. */
typedef struct sil_game_center_masks {
    uint64_t words[SIL_ACH_WORD_COUNT];
} sil_game_center_masks;

typedef struct sil_game_center_state {
    sil_game_center_masks wanted, available, confirmed, remote, pending;
    uint64_t serial, retry_at, refresh_at;
    enum { SIL_GC_IDLE, SIL_GC_LOADING, SIL_GC_REPORTING } operation;
    bool loaded;
} sil_game_center_state;

static inline uint64_t sil_gc_achievement_word_mask(unsigned word)
{
    uint64_t mask = sil_achievement_word_mask(word);
#define SIL_ACH_GAME_CENTER_EXCLUDED(key) \
    if (word == SIL_ACH_##key / 64) mask &= ~(UINT64_C(1) << (SIL_ACH_##key % 64));
#include "achievement-game-center-exclusions.h"
#undef SIL_ACH_GAME_CENTER_EXCLUDED
    return mask;
}

static inline void sil_gc_reset(sil_game_center_state* state)
{
    uint64_t serial = state->serial + 1;
    memset(state, 0, sizeof(*state));
    state->serial = serial;
}

static inline void sil_gc_want(sil_game_center_state* state,
    const sil_game_center_masks* masks)
{
    for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word)
        state->wanted.words[word] |= masks->words[word] & sil_gc_achievement_word_mask(word);
}

static inline bool sil_gc_begin_load(sil_game_center_state* state, uint64_t now)
{
    if (now < state->retry_at || (state->loaded && now < state->refresh_at)) return false;
    ++state->serial;
    state->loaded = false;
    state->operation = SIL_GC_LOADING;
    state->retry_at = now + 60000;
    return true;
}

static inline bool sil_gc_finish_load(sil_game_center_state* state, uint64_t serial,
    const sil_game_center_masks* available, const sil_game_center_masks* remote,
    bool success, uint64_t now)
{
    if (state->serial != serial || state->operation != SIL_GC_LOADING) return false;
    state->operation = SIL_GC_IDLE;
    state->retry_at = success ? now : now + 60000;
    if (!success) return true;
    for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word)
    {
        state->available.words[word] = available->words[word] & sil_gc_achievement_word_mask(word);
        state->remote.words[word] = remote->words[word] & state->available.words[word];
        state->confirmed.words[word] = state->remote.words[word];
    }
    state->loaded = true;
    state->refresh_at = now + 300000;
    return true;
}

static inline bool sil_gc_begin_report(sil_game_center_state* state, uint64_t now)
{
    if (!state->loaded || now < state->retry_at) return false;
    bool any = false;
    for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word)
    {
        state->pending.words[word] = state->wanted.words[word]
            & state->available.words[word] & ~state->confirmed.words[word];
        any |= state->pending.words[word] != 0;
    }
    if (!any) return false;
    ++state->serial;
    state->operation = SIL_GC_REPORTING;
    state->retry_at = now + 60000;
    return true;
}

static inline bool sil_gc_finish_report(sil_game_center_state* state, uint64_t serial,
    bool success, uint64_t now)
{
    if (state->serial != serial || state->operation != SIL_GC_REPORTING) return false;
    state->operation = SIL_GC_IDLE;
    state->retry_at = success ? now : now + 60000;
    for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word)
    {
        if (success)
        {
            state->confirmed.words[word] |= state->pending.words[word];
            state->remote.words[word] |= state->pending.words[word];
        }
        state->pending.words[word] = 0;
    }
    return true;
}
#endif

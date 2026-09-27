/* Quest tracking helpers split from metarun.c */
#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "quest/quest-runtime.h"
#include <string.h>

/* Local helpers */
static int popcount32(u32b value)
{
    int count = 0;
    while (value) {
        value &= (value - 1);
        count++;
    }
    return count;
}

#define METARUN_KNOWN_QUEST_MASK 0xffffUL

static int quest_slot_from_flag(u32b flag)
{
    for (int i = 0; i < METARUN_KNOWN_QUESTS; ++i)
        if (flag == (1UL << i)) return i;
    return -1;
}

static byte *quest_count_slot(metarun *m, int slot)
{
    if (!m || slot < 0 || slot >= METARUN_KNOWN_QUESTS) return NULL;
    return slot < METARUN_QUEST_SLOT_MAX ? &m->quest_completion_counts[slot]
        : &m->reserved_runtime[slot - METARUN_QUEST_SLOT_MAX];
}

static int quest_count(const metarun *m, int slot)
{
    if (!m || slot < 0 || slot >= METARUN_KNOWN_QUESTS) return 0;
    return slot < METARUN_QUEST_SLOT_MAX ? m->quest_completion_counts[slot]
        : m->reserved_runtime[slot - METARUN_QUEST_SLOT_MAX];
}

void metarun_seed_quest_counts_from_mask(metarun *m, u32b mask)
{
    if (!m) return;
    for (int i = 0; i < METARUN_KNOWN_QUESTS; ++i)
        *quest_count_slot(m, i) = (mask & (1UL << i)) ? 1 : 0;
}

void metarun_clamp_and_sync_quests(metarun *m)
{
    if (!m) return;
    u32b mask = m->completed_quests & ~((u32b)METARUN_KNOWN_QUEST_MASK);
    for (int i = 0; i < METARUN_KNOWN_QUESTS; ++i) {
        byte *count = quest_count_slot(m, i);
        /* Data/option caps control future awards, never erase earned history. */
        if (*count > METARUN_QUEST_COMPLETION_CAP) *count = METARUN_QUEST_COMPLETION_CAP;
        if (*count) mask |= 1UL << i;
    }
    m->completed_quests = mask;
}

int metarun_total_quest_completions(const metarun *m)
{
    if (!m) return 0;
    int total = 0;
    for (int i = 0; i < METARUN_KNOWN_QUESTS; ++i) total += quest_count(m, i);
    return total + popcount32(m->completed_quests & ~((u32b)METARUN_KNOWN_QUEST_MASK));
}

int metarun_quests_completed_at_least(int minimum_count)
{
    int total = 0;
    int limit = quest_rules_enabled() ? METARUN_KNOWN_QUESTS : 6;
    for (int i = 0; i < limit; ++i)
        if (quest_count(&metar, i) >= minimum_count) ++total;
    return total;
}

bool metarun_repeat_tier_unlocked(int prior_completion_count)
{
    if (prior_completion_count <= 0) return true;

    return metarun_quests_completed_at_least(prior_completion_count) >= QUEST_REPEAT_TIER_REQUIRED;
}

int quest_initiated_count_this_run(void)
{
    if (!p_ptr) return 0;

    return MIN((int)p_ptr->quest_reserved[0], QUEST_MAX_INITIATED_PER_RUN);
}

static bool quest_state_is_accepted(int quest_idx)
{
    if (!p_ptr || !quest_enabled(quest_idx)) return false;

    switch (quest_idx) {
        case QUEST_ID_TULKAS:
            return p_ptr->tulkas_quest == TULKAS_QUEST_ACTIVE;
        case QUEST_ID_AULE:
            return p_ptr->aule_quest == AULE_QUEST_ACTIVE;
        case QUEST_ID_MANDOS:
            return p_ptr->mandos_quest == MANDOS_QUEST_ACTIVE;
        case QUEST_ID_NIENA:
            return p_ptr->niena_quest == NIENA_QUEST_ACTIVE;
        case QUEST_ID_OROME:
            return p_ptr->orome_quest == OROME_QUEST_ACTIVE;
        case QUEST_ID_VARDA:
            return p_ptr->varda_quest == VARDA_QUEST_ACTIVE;
        default:
            return quest_get_state(quest_idx) == QUEST_STATE_ACTIVE
                && !(p_ptr->quest_followup_flags[quest_idx - 7] & 2);
    }
}

int quest_accepted_count_this_run(void)
{
    int total = 0;

    for (int quest_idx = QUEST_ID_TULKAS; quest_idx <= QUEST_ID_VARDA_UNGOLIANT; quest_idx++) {
        if (quest_state_is_accepted(quest_idx)) {
            total++;
        }
    }

    return total;
}

bool quest_can_initiate_more(void)
{
    return quest_debug_sandbox() || quest_initiated_count_this_run() < QUEST_MAX_INITIATED_PER_RUN;
}

bool quest_can_accept_more(void)
{
    return quest_debug_sandbox() || quest_accepted_count_this_run() < QUEST_MAX_ACCEPTED_PER_RUN;
}

void quest_note_initiated(int quest_idx)
{
    if (!p_ptr) return;

    if (p_ptr->quest_reserved[0] < QUEST_MAX_INITIATED_PER_RUN) {
        p_ptr->quest_reserved[0]++;
    } else {
        p_ptr->quest_reserved[0] = QUEST_MAX_INITIATED_PER_RUN;
    }

    log_trace("Quest %d initiated this run (%d/%d)",
              quest_idx, p_ptr->quest_reserved[0], QUEST_MAX_INITIATED_PER_RUN);
}

#define QUEST_RESERVED_RECORD_BASE 1

static bool quest_completion_recorded_for_run(u32b quest_flag)
{
    if (!p_ptr) return false;
    int slot = quest_slot_from_flag(quest_flag);
    if (slot < 0) return false;

    if (slot >= 6) return (p_ptr->quest_followup_recorded & (1U << (slot - 6))) != 0;
    int idx = QUEST_RESERVED_RECORD_BASE + slot;
    if (idx >= (int)N_ELEMENTS(p_ptr->quest_reserved)) return true; /* fail safe: assume recorded */

    return p_ptr->quest_reserved[idx] != 0;
}

static void mark_quest_completion_recorded_for_run(u32b quest_flag)
{
    if (!p_ptr) return;
    int slot = quest_slot_from_flag(quest_flag);
    if (slot < 0) return;

    if (slot >= 6) { p_ptr->quest_followup_recorded |= 1U << (slot - 6); return; }
    int idx = QUEST_RESERVED_RECORD_BASE + slot;
    if (idx >= (int)N_ELEMENTS(p_ptr->quest_reserved)) return;

    p_ptr->quest_reserved[idx] = 1;
}

int metarun_quest_completion_count(u32b quest_flag)
{
    if (run_mode_is_blitz()) return 0;
    if (metarun_current_index() < 0) return 0;

    int slot = quest_slot_from_flag(quest_flag);
    if (slot >= 0) return quest_count(&metar, slot);

    /* Unknown flags fall back to the bitmask so legacy callers still work */
    return (metar.completed_quests & quest_flag) ? 1 : 0;
}

bool metarun_is_quest_completed(u32b quest_flag)
{
    if (run_mode_is_blitz()) return false;
    /* Only check the current metarun, not all metaruns */
    const metarun *current = metarun_current();
    s16b current_idx = metarun_current_index();
    if (!current) {
        log_trace("Metarun quest check: Invalid current run (idx=%d, max=%d)", current_idx, metarun_entry_count());
        return false;
    }

    int count = metarun_quest_completion_count(quest_flag);
    if (count > 0) {
        log_trace("Metarun quest check: Quest 0x%x completed %d time(s) in metarun[%d] (id=%d)",
                  quest_flag, count, current_idx, current->id);
        return true;
    }

    log_trace("Metarun quest check: Quest 0x%x not completed in current metarun[%d] (id=%d)",
              quest_flag, current_idx, current->id);
    return false;
}

void metarun_mark_quest_completed(u32b quest_flag)
{
    if (run_mode_is_blitz() || quest_debug_sandbox() || !quest_flag) return;
    metarun *current = metarun_current_mutable();
    if (!current || quest_completion_recorded_for_run(quest_flag)) return;
    int slot = quest_slot_from_flag(quest_flag);
    if (slot >= 0) {
        if (!quest_enabled(slot + 1)) return;
        byte *count = quest_count_slot(&metar, slot);
        if (*count < quest_completion_cap(slot + 1)) ++*count;
        mark_quest_completion_recorded_for_run(quest_flag);
    } else metar.completed_quests |= quest_flag;
    metarun_clamp_and_sync_quests(&metar);
    current->completed_quests = metar.completed_quests;
    for (int i = 0; i < METARUN_KNOWN_QUESTS; ++i)
        *quest_count_slot(current, i) = (byte)quest_count(&metar, i);
    refresh_current_metar_score();
    save_metaruns();
}

void metarun_check_and_update_quests(void)
{
    if (run_mode_is_blitz()) return;
    s16b current_idx = metarun_current_index();
    log_trace("Metarun quest check: Entry - current_run=%d, metarun_max=%d", current_idx, metarun_entry_count());
    
    if (current_idx < 0 || !p_ptr) {
        log_trace("Metarun quest check: Early return - current_run=%d, metarun_max=%d", current_idx, metarun_entry_count());
        return;
    }
    
    log_trace("Metarun quest check: current_run=%d, tulkas=%d, aule=%d, mandos=%d, niena=%d, orome=%d, varda=%d", 
              current_idx, p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->niena_quest, p_ptr->orome_quest, p_ptr->varda_quest);

    for (int id = 7; id <= 16; ++id)
        if (quest_get_state(id) == QUEST_STATE_REWARDED)
            metarun_mark_quest_completed(quest_metarun_flag(id));

    /* Only record once per character; completion handlers also call metarun_mark_quest_completed */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_TULKAS)) {
        log_trace("Metarun: Marking Tulkas quest as completed (rewarded, was %d)", p_ptr->tulkas_quest);
        metarun_mark_quest_completed(METARUN_QUEST_TULKAS);
    }
    
    if (p_ptr->aule_quest == AULE_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_AULE)) {
        log_trace("Metarun: Marking Aulë quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_AULE);
    }

    if (p_ptr->mandos_quest == MANDOS_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_MANDOS)) {
        log_trace("Metarun: Marking Mandos quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_MANDOS);
    }

    if (p_ptr->niena_quest == NIENA_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_NIENA)) {
        log_trace("Metarun: Marking Nienna quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_NIENA);
    }

    if (p_ptr->orome_quest == OROME_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_OROME)) {
        log_trace("Metarun: Marking Oromë quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_OROME);
    }
    
    if (p_ptr->varda_quest == VARDA_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_VARDA)) {
        log_trace("Metarun: Marking Varda quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_VARDA);
    }
}

void metarun_restore_quest_states(void)
{
    if (!p_ptr) return;
    s16b current_idx = metarun_current_index();
    const metarun *current = metarun_current();
    if (!current) {
        log_trace("Metarun restore: Invalid current_run=%d, metarun_max=%d", current_idx, metarun_entry_count());
        return;
    }
    
    log_trace("Metarun restore: Restoring quest states from metarun[%d], completed_quests=0x%08X", 
              current_idx, current->completed_quests);
    
    /* Restore Tulkas quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_TULKAS) > 0) {
        if (p_ptr->tulkas_quest < TULKAS_QUEST_REWARDED) {
            p_ptr->tulkas_quest = TULKAS_QUEST_REWARDED;
            log_trace("Metarun restore: Tulkas quest set to REWARDED (%d)", TULKAS_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_TULKAS);
    }
    
    /* Restore Aulë quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_AULE) > 0) {
        if (p_ptr->aule_quest < AULE_QUEST_REWARDED) {
            p_ptr->aule_quest = AULE_QUEST_REWARDED;
            log_trace("Metarun restore: Aulë quest set to REWARDED (%d)", AULE_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_AULE);
    }
    
    /* Restore Mandos quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_MANDOS) > 0) {
        if (p_ptr->mandos_quest < MANDOS_QUEST_REWARDED) {
            p_ptr->mandos_quest = MANDOS_QUEST_REWARDED;
            log_trace("Metarun restore: Mandos quest set to REWARDED (%d)", MANDOS_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_MANDOS);
    }
    
    /* Restore Nienna quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_NIENA) > 0) {
        if (p_ptr->niena_quest < NIENA_QUEST_REWARDED) {
            p_ptr->niena_quest = NIENA_QUEST_REWARDED;
            p_ptr->niena_level = 0; /* Clear depth for previous run attribution */
            log_trace("Metarun restore: Nienna quest set to REWARDED (%d)", NIENA_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_NIENA);
    }
    
    /* Restore Oromë quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_OROME) > 0) {
        if (p_ptr->orome_quest < OROME_QUEST_REWARDED) {
            p_ptr->orome_quest = OROME_QUEST_REWARDED;
            log_trace("Metarun restore: Oromë quest set to REWARDED (%d)", OROME_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_OROME);
    }
    
    /* Restore Varda quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_VARDA) > 0) {
        if (p_ptr->varda_quest != VARDA_QUEST_REWARDED) {
            p_ptr->varda_quest = VARDA_QUEST_REWARDED;
            log_trace("Metarun restore: Varda quest set to REWARDED (%d)", VARDA_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_VARDA);
    }
    
    log_trace("Metarun restore: Final quest states - Tulkas: %d, Aulë: %d, Mandos: %d, Nienna: %d, Oromë: %d, Varda: %d",
              p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->niena_quest, p_ptr->orome_quest, p_ptr->varda_quest);
}

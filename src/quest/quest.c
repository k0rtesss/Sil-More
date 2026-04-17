/* Quest tracking helpers split from metarun.c */
#include "angband.h"
#include "log/log.h"
#include "metarun.h"
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

static const u32b metarun_known_quest_flags[] = {
    METARUN_QUEST_TULKAS,
    METARUN_QUEST_AULE,
    METARUN_QUEST_MANDOS,
    METARUN_QUEST_MANDOS_TRAITOR,
    METARUN_QUEST_NIENA,
    METARUN_QUEST_OROME,
    METARUN_QUEST_VARDA,
    METARUN_QUEST_MANDOS_BETRAYER,
    METARUN_QUEST_OROME_DRAGONS,
    METARUN_QUEST_OROME_GREAT_HUNT,
    METARUN_QUEST_NIENA_MORGOTH,
    METARUN_QUEST_NIENA_PACIFIST,
    METARUN_QUEST_TULKAS_ORCS,
    METARUN_QUEST_TULKAS_MORGOTH,
    METARUN_QUEST_VARDA_SHADOW,
    METARUN_QUEST_VARDA_UNGOLIANT
};

#define METARUN_KNOWN_QUEST_MASK (METARUN_QUEST_TULKAS | METARUN_QUEST_AULE | METARUN_QUEST_MANDOS | METARUN_QUEST_MANDOS_TRAITOR | METARUN_QUEST_MANDOS_BETRAYER | METARUN_QUEST_NIENA | METARUN_QUEST_OROME | METARUN_QUEST_OROME_DRAGONS | METARUN_QUEST_OROME_GREAT_HUNT | METARUN_QUEST_VARDA | METARUN_QUEST_NIENA_MORGOTH | METARUN_QUEST_NIENA_PACIFIST | METARUN_QUEST_TULKAS_ORCS | METARUN_QUEST_TULKAS_MORGOTH | METARUN_QUEST_VARDA_SHADOW | METARUN_QUEST_VARDA_UNGOLIANT)

static int quest_slot_from_flag(u32b quest_flag)
{
    for (size_t i = 0; i < N_ELEMENTS(metarun_known_quest_flags) && i < METARUN_QUEST_SLOT_MAX; i++) {
        if (quest_flag == metarun_known_quest_flags[i]) return (int)i;
    }
    return -1;
}

static int quest_id_from_slot(int slot)
{
    switch (slot) {
        case 0: return QUEST_ID_TULKAS;
        case 1: return QUEST_ID_AULE;
        case 2: return QUEST_ID_MANDOS;
        case 3: return QUEST_ID_MANDOS_TRAITOR;
        case 4: return QUEST_ID_NIENA;
        case 5: return QUEST_ID_OROME;
        case 6: return QUEST_ID_VARDA;
        case 7: return QUEST_ID_MANDOS_BETRAYER;
        case 8: return QUEST_ID_OROME_DRAGONS;
        case 9: return QUEST_ID_OROME_GREAT_HUNT;
        case 10: return QUEST_ID_NIENA_MORGOTH;
        case 11: return QUEST_ID_NIENA_PACIFIST;
        case 12: return QUEST_ID_TULKAS_ORCS;
        case 13: return QUEST_ID_TULKAS_MORGOTH;
        case 14: return QUEST_ID_VARDA_SHADOW;
        case 15: return QUEST_ID_VARDA_UNGOLIANT;
        default: return 0;
    }
}

static byte* quest_state_slot(int quest_id)
{
    if (!p_ptr || quest_id <= 0) return NULL;
    if (!z_info || quest_id >= z_info->quest_max) return NULL;
    if (!quest_info) return NULL;

    quest_type* q_ptr = &quest_info[quest_id];
    int vala_idx = (q_ptr->vala_id > 0) ? (q_ptr->vala_id - 1) : -1;
    int stage = (q_ptr->sequence > 0) ? q_ptr->sequence : 1;

    if (stage == 1) {
        switch (q_ptr->vala_id) {
            case VALA_TULKAS: return &p_ptr->tulkas_quest;
            case VALA_AULE:   return &p_ptr->aule_quest;
            case VALA_MANDOS: return &p_ptr->mandos_quest;
            case VALA_NIENNA: return &p_ptr->niena_quest;
            case VALA_OROME:  return &p_ptr->orome_quest;
            case VALA_VARDA:  return &p_ptr->varda_quest;
            default: break;
        }
    } else if (stage == 2) {
        if (vala_idx >= 0 && vala_idx < VALA_MAX) return &p_ptr->vala_quest_stage2[vala_idx];
    } else if (stage == 3) {
        if (vala_idx >= 0 && vala_idx < VALA_MAX) return &p_ptr->vala_quest_stage3[vala_idx];
    }

    return NULL;
}

byte quest_get_state(int quest_id)
{
    byte* slot = quest_state_slot(quest_id);
    return slot ? *slot : QUEST_STATE_NOT_STARTED;
}

void quest_set_state(int quest_id, byte state)
{
    byte* slot = quest_state_slot(quest_id);
    if (slot) *slot = state;
}

int quest_id_for_vala_stage(int vala_id, int stage)
{
    if (!z_info || !quest_info) return 0;

    for (int i = 1; i < z_info->quest_max; i++) {
        quest_type* q_ptr = &quest_info[i];
        int q_stage;

        if (!q_ptr->name) continue;
        if (q_ptr->vala_id != vala_id) continue;

        q_stage = q_ptr->sequence ? q_ptr->sequence : 1;
        if (q_stage == stage) return i;
    }

    return 0;
}

u32b quest_metarun_flag(int quest_id)
{
    switch (quest_id) {
        case QUEST_ID_TULKAS: return METARUN_QUEST_TULKAS;
        case QUEST_ID_AULE: return METARUN_QUEST_AULE;
        case QUEST_ID_MANDOS: return METARUN_QUEST_MANDOS;
        case QUEST_ID_MANDOS_TRAITOR: return METARUN_QUEST_MANDOS_TRAITOR;
        case QUEST_ID_MANDOS_BETRAYER: return METARUN_QUEST_MANDOS_BETRAYER;
        case QUEST_ID_NIENA: return METARUN_QUEST_NIENA;
        case QUEST_ID_OROME: return METARUN_QUEST_OROME;
        case QUEST_ID_OROME_DRAGONS: return METARUN_QUEST_OROME_DRAGONS;
        case QUEST_ID_OROME_GREAT_HUNT: return METARUN_QUEST_OROME_GREAT_HUNT;
        case QUEST_ID_NIENA_MORGOTH: return METARUN_QUEST_NIENA_MORGOTH;
        case QUEST_ID_NIENA_PACIFIST: return METARUN_QUEST_NIENA_PACIFIST;
        case QUEST_ID_TULKAS_ORCS: return METARUN_QUEST_TULKAS_ORCS;
        case QUEST_ID_TULKAS_MORGOTH: return METARUN_QUEST_TULKAS_MORGOTH;
        case QUEST_ID_VARDA: return METARUN_QUEST_VARDA;
        case QUEST_ID_VARDA_SHADOW: return METARUN_QUEST_VARDA_SHADOW;
        case QUEST_ID_VARDA_UNGOLIANT: return METARUN_QUEST_VARDA_UNGOLIANT;
        default: return 0;
    }
}

cptr quest_display_title(int quest_id)
{
    static char fallback[32];

    if (!z_info || quest_id <= 0 || quest_id >= z_info->quest_max || !quest_info)
        return "Unknown quest";

    if (quest_info[quest_id].title_text && q_text)
        return q_text + quest_info[quest_id].title_text;

    if (quest_info[quest_id].name && q_text)
        return q_text + quest_info[quest_id].name;

    strnfmt(fallback, sizeof(fallback), "Quest %d", quest_id);
    return fallback;
}

int quest_completion_cap(int quest_idx)
{
    byte cap;

    if (!z_info || quest_idx <= 0 || quest_idx >= z_info->quest_max || !quest_info)
        return METARUN_QUEST_COMPLETION_CAP;

    cap = quest_info[quest_idx].completion_cap;
    if (cap == 0) return METARUN_QUEST_COMPLETION_CAP;
    if (cap > METARUN_QUEST_COMPLETION_CAP) return METARUN_QUEST_COMPLETION_CAP;
    return cap;
}

void metarun_seed_quest_counts_from_mask(metarun *m, u32b mask)
{
    if (!m) return;
    for (size_t i = 0; i < N_ELEMENTS(metarun_known_quest_flags) && i < METARUN_QUEST_SLOT_MAX; i++) {
        m->quest_completion_counts[i] = (mask & metarun_known_quest_flags[i]) ? 1 : 0;
    }
    for (size_t i = N_ELEMENTS(metarun_known_quest_flags); i < METARUN_QUEST_SLOT_MAX; i++) {
        m->quest_completion_counts[i] = 0;
    }
}

void metarun_clamp_and_sync_quests(metarun *m)
{
    if (!m) return;

    u32b mask = m->completed_quests & ~((u32b)METARUN_KNOWN_QUEST_MASK);

    for (size_t i = 0; i < METARUN_QUEST_SLOT_MAX; i++) {
        byte count = m->quest_completion_counts[i];
        int quest_id = quest_id_from_slot((int)i);
        int cap = quest_completion_cap(quest_id);
        if (cap < 1) cap = METARUN_QUEST_COMPLETION_CAP;
        if (count > cap) count = (byte)cap;
        if (count > METARUN_QUEST_COMPLETION_CAP) count = METARUN_QUEST_COMPLETION_CAP;

        if (i < N_ELEMENTS(metarun_known_quest_flags)) {
            if (count > 0) {
                mask |= metarun_known_quest_flags[i];
            }
        } else {
            count = 0;
        }

        m->quest_completion_counts[i] = count;
    }

    m->completed_quests = mask;
}

int metarun_total_quest_completions(const metarun *m)
{
    if (!m) return 0;

    int total = 0;
    for (size_t i = 0; i < METARUN_QUEST_SLOT_MAX && i < N_ELEMENTS(metarun_known_quest_flags); i++) {
        total += m->quest_completion_counts[i];
    }

    /* Preserve completions for unknown future quests represented only by the bitmask */
    u32b unknown_mask = m->completed_quests & ~((u32b)METARUN_KNOWN_QUEST_MASK);
    total += popcount32(unknown_mask);

    return total;
}

#define QUEST_RESERVED_RECORD_BASE 1

static bool quest_completion_recorded_for_run(u32b quest_flag)
{
    if (!p_ptr) return false;
    int slot = quest_slot_from_flag(quest_flag);
    if (slot < 0) return false;

    int idx = QUEST_RESERVED_RECORD_BASE + slot;
    if (idx >= (int)N_ELEMENTS(p_ptr->quest_reserved)) return true; /* fail safe: assume recorded */

    return p_ptr->quest_reserved[idx] != 0;
}

static void mark_quest_completion_recorded_for_run(u32b quest_flag)
{
    if (!p_ptr) return;
    int slot = quest_slot_from_flag(quest_flag);
    if (slot < 0) return;

    int idx = QUEST_RESERVED_RECORD_BASE + slot;
    if (idx >= (int)N_ELEMENTS(p_ptr->quest_reserved)) return;

    p_ptr->quest_reserved[idx] = 1;
}

int metarun_quest_completion_count(u32b quest_flag)
{
    if (metarun_current_index() < 0) return 0;

    int slot = quest_slot_from_flag(quest_flag);
    if (slot >= 0 && slot < METARUN_QUEST_SLOT_MAX) {
        return metar.quest_completion_counts[slot];
    }

    /* Unknown flags fall back to the bitmask so legacy callers still work */
    return (metar.completed_quests & quest_flag) ? 1 : 0;
}

bool metarun_is_quest_completed(u32b quest_flag)
{
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
    metarun *current = metarun_current_mutable();
    if (!current) return;
    if (!quest_flag) return;

    int slot = quest_slot_from_flag(quest_flag);
    bool changed = false;

    if (slot >= 0 && slot < METARUN_QUEST_SLOT_MAX) {
        byte completion_count = metar.quest_completion_counts[slot];
        int quest_id = quest_id_from_slot(slot);
        int cap = quest_completion_cap(quest_id);
        if (cap < 1) cap = METARUN_QUEST_COMPLETION_CAP;
        if (completion_count < cap) {
            metar.quest_completion_counts[slot] = completion_count + 1;
            changed = true;
        }
        /* Always sync the per-run marker so we don't double-count this character */
        mark_quest_completion_recorded_for_run(quest_flag);
    } else if (!(metar.completed_quests & quest_flag)) {
        /* Unknown quest flag - preserve legacy behavior */
        metar.completed_quests |= quest_flag;
        changed = true;
    }

    metarun_clamp_and_sync_quests(&metar);
    current->completed_quests = metar.completed_quests;
    memcpy(current->quest_completion_counts, metar.quest_completion_counts, sizeof(metar.quest_completion_counts));

    if (changed) {
        int new_count = metarun_quest_completion_count(quest_flag);
        log_trace("Metarun: Quest 0x%x completion recorded (count=%d, mask=0x%08X)",
                  quest_flag, new_count, metar.completed_quests);
        refresh_current_metar_score();
        save_metaruns();
    } else {
        log_trace("Metarun: Quest 0x%x completion ignored (already at cap or recorded); mask=0x%08X",
                  quest_flag, metar.completed_quests);
    }
}

void metarun_check_and_update_quests(void)
{
    s16b current_idx = metarun_current_index();
    log_trace("Metarun quest check: Entry - current_run=%d, metarun_max=%d", current_idx, metarun_entry_count());
    
    if (current_idx < 0 || !p_ptr) {
        log_trace("Metarun quest check: Early return - current_run=%d, metarun_max=%d", current_idx, metarun_entry_count());
        return;
    }
    
    log_trace("Metarun quest check: current_run=%d, tulkas=%d, aule=%d, mandos=%d, niena=%d, orome=%d, varda=%d", 
              current_idx, p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->niena_quest, p_ptr->orome_quest, p_ptr->varda_quest);

    /* Only record once per character; completion handlers also call metarun_mark_quest_completed */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_TULKAS)) {
        log_trace("Metarun: Marking Tulkas quest as completed (rewarded, was %d)", p_ptr->tulkas_quest);
        metarun_mark_quest_completed(METARUN_QUEST_TULKAS);
    }
    if (quest_get_state(QUEST_ID_TULKAS_ORCS) == QUEST_STATE_REWARDED &&
        !quest_completion_recorded_for_run(METARUN_QUEST_TULKAS_ORCS)) {
        log_trace("Metarun: Marking Tulkas orc quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_TULKAS_ORCS);
    }
    if (quest_get_state(QUEST_ID_TULKAS_MORGOTH) == QUEST_STATE_REWARDED &&
        !quest_completion_recorded_for_run(METARUN_QUEST_TULKAS_MORGOTH)) {
        log_trace("Metarun: Marking Tulkas Morgoth quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_TULKAS_MORGOTH);
    }
    
    if (p_ptr->aule_quest == AULE_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_AULE)) {
        log_trace("Metarun: Marking Aule quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_AULE);
    }

    if (p_ptr->mandos_quest == MANDOS_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_MANDOS)) {
        log_trace("Metarun: Marking Mandos quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_MANDOS);
    }
    if (quest_get_state(QUEST_ID_MANDOS_TRAITOR) == QUEST_STATE_REWARDED &&
        !quest_completion_recorded_for_run(METARUN_QUEST_MANDOS_TRAITOR)) {
        log_trace("Metarun: Marking Mandos second quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_MANDOS_TRAITOR);
    }
    if (quest_get_state(QUEST_ID_MANDOS_BETRAYER) == QUEST_STATE_REWARDED &&
        !quest_completion_recorded_for_run(METARUN_QUEST_MANDOS_BETRAYER)) {
        log_trace("Metarun: Marking Mandos third quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_MANDOS_BETRAYER);
    }

    if (p_ptr->niena_quest == NIENA_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_NIENA)) {
        log_trace("Metarun: Marking Nienna quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_NIENA);
    }
    if (quest_get_state(QUEST_ID_NIENA_MORGOTH) == QUEST_STATE_REWARDED &&
        !quest_completion_recorded_for_run(METARUN_QUEST_NIENA_MORGOTH)) {
        log_trace("Metarun: Marking Nienna Morgoth quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_NIENA_MORGOTH);
    }
    if (quest_get_state(QUEST_ID_NIENA_PACIFIST) == QUEST_STATE_REWARDED &&
        !quest_completion_recorded_for_run(METARUN_QUEST_NIENA_PACIFIST)) {
        log_trace("Metarun: Marking Nienna pacifist quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_NIENA_PACIFIST);
    }

    if (p_ptr->orome_quest == OROME_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_OROME)) {
        log_trace("Metarun: Marking Orome quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_OROME);
    }
    if (quest_get_state(QUEST_ID_OROME_DRAGONS) == QUEST_STATE_REWARDED &&
        !quest_completion_recorded_for_run(METARUN_QUEST_OROME_DRAGONS)) {
        log_trace("Metarun: Marking Orome dragon quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_OROME_DRAGONS);
    }
    if (quest_get_state(QUEST_ID_OROME_GREAT_HUNT) == QUEST_STATE_REWARDED &&
        !quest_completion_recorded_for_run(METARUN_QUEST_OROME_GREAT_HUNT)) {
        log_trace("Metarun: Marking Orome great hunt quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_OROME_GREAT_HUNT);
    }
    
    if (p_ptr->varda_quest == VARDA_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_VARDA)) {
        log_trace("Metarun: Marking Varda quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_VARDA);
    }
    if (quest_get_state(QUEST_ID_VARDA_SHADOW) == QUEST_STATE_REWARDED &&
        !quest_completion_recorded_for_run(METARUN_QUEST_VARDA_SHADOW)) {
        log_trace("Metarun: Marking Varda shadow quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_VARDA_SHADOW);
    }
    if (quest_get_state(QUEST_ID_VARDA_UNGOLIANT) == QUEST_STATE_REWARDED &&
        !quest_completion_recorded_for_run(METARUN_QUEST_VARDA_UNGOLIANT)) {
        log_trace("Metarun: Marking Varda Ungoliant quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_VARDA_UNGOLIANT);
    }
}

void metarun_restore_quest_states(void)
{
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
    if (metarun_quest_completion_count(METARUN_QUEST_TULKAS_ORCS) > 0) {
        if (quest_get_state(QUEST_ID_TULKAS_ORCS) < QUEST_STATE_REWARDED) {
            quest_set_state(QUEST_ID_TULKAS_ORCS, QUEST_STATE_REWARDED);
            log_trace("Metarun restore: Tulkas orc quest set to REWARDED (%d)", QUEST_STATE_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_TULKAS_ORCS);
    }
    if (metarun_quest_completion_count(METARUN_QUEST_TULKAS_MORGOTH) > 0) {
        if (quest_get_state(QUEST_ID_TULKAS_MORGOTH) < QUEST_STATE_REWARDED) {
            quest_set_state(QUEST_ID_TULKAS_MORGOTH, QUEST_STATE_REWARDED);
            log_trace("Metarun restore: Tulkas Morgoth quest set to REWARDED (%d)", QUEST_STATE_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_TULKAS_MORGOTH);
        if (p_ptr->tulkas_morgoth_progress < 100) {
            p_ptr->tulkas_morgoth_progress = 100;
        }
    }
    
    /* Restore Aule quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_AULE) > 0) {
        if (p_ptr->aule_quest < AULE_QUEST_REWARDED) {
            p_ptr->aule_quest = AULE_QUEST_REWARDED;
            log_trace("Metarun restore: Aule quest set to REWARDED (%d)", AULE_QUEST_REWARDED);
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
    if (metarun_quest_completion_count(METARUN_QUEST_MANDOS_TRAITOR) > 0) {
        if (quest_get_state(QUEST_ID_MANDOS_TRAITOR) < QUEST_STATE_REWARDED) {
            quest_set_state(QUEST_ID_MANDOS_TRAITOR, QUEST_STATE_REWARDED);
            log_trace("Metarun restore: Mandos second quest set to REWARDED (%d)", QUEST_STATE_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_MANDOS_TRAITOR);
    }
    if (metarun_quest_completion_count(METARUN_QUEST_MANDOS_BETRAYER) > 0) {
        if (quest_get_state(QUEST_ID_MANDOS_BETRAYER) < QUEST_STATE_REWARDED) {
            quest_set_state(QUEST_ID_MANDOS_BETRAYER, QUEST_STATE_REWARDED);
            log_trace("Metarun restore: Mandos third quest set to REWARDED (%d)", QUEST_STATE_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_MANDOS_BETRAYER);
    }
    
    /* Restore Niena quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_NIENA) > 0) {
        if (p_ptr->niena_quest < NIENA_QUEST_REWARDED) {
            p_ptr->niena_quest = NIENA_QUEST_REWARDED;
            p_ptr->niena_level = 0; /* Clear depth for previous run attribution */
            log_trace("Metarun restore: Niena quest set to REWARDED (%d)", NIENA_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_NIENA);
    }
    if (metarun_quest_completion_count(METARUN_QUEST_NIENA_MORGOTH) > 0) {
        if (quest_get_state(QUEST_ID_NIENA_MORGOTH) < QUEST_STATE_REWARDED) {
            quest_set_state(QUEST_ID_NIENA_MORGOTH, QUEST_STATE_REWARDED);
            log_trace("Metarun restore: Nienna Morgoth quest set to REWARDED (%d)", QUEST_STATE_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_NIENA_MORGOTH);
    }
    if (metarun_quest_completion_count(METARUN_QUEST_NIENA_PACIFIST) > 0) {
        if (quest_get_state(QUEST_ID_NIENA_PACIFIST) < QUEST_STATE_REWARDED) {
            quest_set_state(QUEST_ID_NIENA_PACIFIST, QUEST_STATE_REWARDED);
            log_trace("Metarun restore: Nienna pacifist quest set to REWARDED (%d)", QUEST_STATE_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_NIENA_PACIFIST);
    }
    
    /* Restore Orome quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_OROME) > 0) {
        if (p_ptr->orome_quest < OROME_QUEST_REWARDED) {
            p_ptr->orome_quest = OROME_QUEST_REWARDED;
            log_trace("Metarun restore: Orome quest set to REWARDED (%d)", OROME_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_OROME);
    }
    if (metarun_quest_completion_count(METARUN_QUEST_OROME_DRAGONS) > 0) {
        if (quest_get_state(QUEST_ID_OROME_DRAGONS) < QUEST_STATE_REWARDED) {
            quest_set_state(QUEST_ID_OROME_DRAGONS, QUEST_STATE_REWARDED);
            log_trace("Metarun restore: Orome dragon quest set to REWARDED (%d)", QUEST_STATE_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_OROME_DRAGONS);
    }
    p_ptr->orome_great_hunt_mask = metarun_orome_great_hunt_mask();
    if (metarun_quest_completion_count(METARUN_QUEST_OROME_GREAT_HUNT) > 0) {
        if (quest_get_state(QUEST_ID_OROME_GREAT_HUNT) < QUEST_STATE_REWARDED) {
            quest_set_state(QUEST_ID_OROME_GREAT_HUNT, QUEST_STATE_REWARDED);
            log_trace("Metarun restore: Orome great hunt quest set to REWARDED (%d)", QUEST_STATE_REWARDED);
        }
        metarun_set_orome_great_hunt_active(false);
        mark_quest_completion_recorded_for_run(METARUN_QUEST_OROME_GREAT_HUNT);
    }
    if (metarun_orome_great_hunt_active() &&
        quest_get_state(QUEST_ID_OROME_GREAT_HUNT) < QUEST_STATE_REWARDED) {
        if (quest_get_state(QUEST_ID_OROME_GREAT_HUNT) < QUEST_STATE_ACTIVE) {
            quest_set_state(QUEST_ID_OROME_GREAT_HUNT, QUEST_STATE_ACTIVE);
            log_trace("Metarun restore: Orome great hunt quest set to ACTIVE (%d)", QUEST_STATE_ACTIVE);
        }
    } else if (quest_get_state(QUEST_ID_OROME_GREAT_HUNT) >= QUEST_STATE_ACTIVE &&
               quest_get_state(QUEST_ID_OROME_GREAT_HUNT) < QUEST_STATE_REWARDED &&
               !metarun_orome_great_hunt_active()) {
        metarun_set_orome_great_hunt_active(true);
        log_trace("Metarun restore: Backfilled Orome great hunt active flag from quest state");
    }
    
    /* Restore Varda quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_VARDA) > 0) {
        if (p_ptr->varda_quest < VARDA_QUEST_REWARDED) {
            p_ptr->varda_quest = VARDA_QUEST_REWARDED;
            log_trace("Metarun restore: Varda quest set to REWARDED (%d)", VARDA_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_VARDA);
    }
    if (metarun_quest_completion_count(METARUN_QUEST_VARDA_SHADOW) > 0) {
        if (quest_get_state(QUEST_ID_VARDA_SHADOW) < QUEST_STATE_REWARDED) {
            quest_set_state(QUEST_ID_VARDA_SHADOW, QUEST_STATE_REWARDED);
            log_trace("Metarun restore: Varda shadow quest set to REWARDED (%d)", QUEST_STATE_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_VARDA_SHADOW);
    }
    if (metarun_quest_completion_count(METARUN_QUEST_VARDA_UNGOLIANT) > 0) {
        if (quest_get_state(QUEST_ID_VARDA_UNGOLIANT) < QUEST_STATE_REWARDED) {
            quest_set_state(QUEST_ID_VARDA_UNGOLIANT, QUEST_STATE_REWARDED);
            log_trace("Metarun restore: Varda Ungoliant quest set to REWARDED (%d)", QUEST_STATE_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_VARDA_UNGOLIANT);
    }
    
    log_trace("Metarun restore: Final quest states - Tulkas: %d (orcs:%d, morgoth:%d), Aule: %d, Mandos: %d (2:%d,3:%d), Niena: %d (2:%d,3:%d), Orome: %d (dragons:%d, hunt:%d), Varda: %d (shadow:%d, ungoliant:%d)",
              p_ptr->tulkas_quest, quest_get_state(QUEST_ID_TULKAS_ORCS), quest_get_state(QUEST_ID_TULKAS_MORGOTH), p_ptr->aule_quest, p_ptr->mandos_quest,
              quest_get_state(QUEST_ID_MANDOS_TRAITOR), quest_get_state(QUEST_ID_MANDOS_BETRAYER),
              p_ptr->niena_quest, quest_get_state(QUEST_ID_NIENA_MORGOTH), quest_get_state(QUEST_ID_NIENA_PACIFIST),
              p_ptr->orome_quest, quest_get_state(QUEST_ID_OROME_DRAGONS), quest_get_state(QUEST_ID_OROME_GREAT_HUNT),
              p_ptr->varda_quest, quest_get_state(QUEST_ID_VARDA_SHADOW), quest_get_state(QUEST_ID_VARDA_UNGOLIANT));
}

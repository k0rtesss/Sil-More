/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "metarun-internal.h"
#include "ui/ui-semantic-scene.h"

/* =========================  globals  =========================== */
metarun         metar;
metarun *metaruns    = NULL;
s16b     metarun_max = 0;
s16b     current_run = 0;
bool            metarun_created = false;

static bool metarun_runtime_ready(void)
{
    return metarun_current_index() >= 0;
}

static byte* challenge_count_slot(int challenge_id)
{
    int idx;

    if (challenge_id <= CHALLENGE_NONE || challenge_id > CHALLENGE_MAX_TRACKED)
        return NULL;

    idx = METARUN_RUNTIME_CHALLENGE_COUNT_BASE + (challenge_id - 1);
    if (idx < 0 || idx >= (int)N_ELEMENTS(metar.reserved_runtime))
        return NULL;

    return &metar.reserved_runtime[idx];
}

static byte* metarun_quest_reserved_slot(int slot)
{
    if (slot < 0 || slot >= (int)N_ELEMENTS(metar.quest_reserved))
        return NULL;
    return &metar.quest_reserved[slot];
}

bool metarun_challenge_disconnected_unlocked(void)
{
    byte flags;

    if (!metarun_runtime_ready()) return false;

    flags = metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX];
    if (!(flags & METARUN_CHALLENGE_DISCON_FLAG) &&
        metarun_quest_completion_count(METARUN_QUEST_MANDOS_TRAITOR) > 0) {
        metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] |= METARUN_CHALLENGE_DISCON_FLAG;
        save_metaruns();
        flags = metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX];
    }

    return (flags & METARUN_CHALLENGE_DISCON_FLAG) != 0;
}

void metarun_unlock_challenge_disconnected(void)
{
    if (!metarun_runtime_ready()) return;
    if (metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] & METARUN_CHALLENGE_DISCON_FLAG) return;

    metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] |= METARUN_CHALLENGE_DISCON_FLAG;
    save_metaruns();
}

bool metarun_challenge_single_stair_unlocked(void)
{
    byte flags;

    if (!metarun_runtime_ready()) return false;

    flags = metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX];
    if (!(flags & METARUN_CHALLENGE_SINGLE_FLAG) &&
        metarun_quest_completion_count(METARUN_QUEST_OROME_DRAGONS) > 0) {
        metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] |= METARUN_CHALLENGE_SINGLE_FLAG;
        save_metaruns();
        flags = metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX];
    }

    return (flags & METARUN_CHALLENGE_SINGLE_FLAG) != 0;
}

void metarun_unlock_challenge_single_stair(void)
{
    if (!metarun_runtime_ready()) return;
    if (metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] & METARUN_CHALLENGE_SINGLE_FLAG) return;

    metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] |= METARUN_CHALLENGE_SINGLE_FLAG;
    save_metaruns();
}

bool metarun_challenge_fixed_exp_unlocked(void)
{
    byte flags;

    if (!metarun_runtime_ready()) return false;

    flags = metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX];
    if (!(flags & METARUN_CHALLENGE_FIXED_FLAG) &&
        (metarun_quest_completion_count(METARUN_QUEST_NIENA_MORGOTH) > 0 ||
         metarun_challenge_completion_count(CHALLENGE_FIXED_50K_XP) > 0)) {
        metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] |= METARUN_CHALLENGE_FIXED_FLAG;
        save_metaruns();
        flags = metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX];
    }

    return (flags & METARUN_CHALLENGE_FIXED_FLAG) != 0;
}

void metarun_unlock_challenge_fixed_exp(void)
{
    if (!metarun_runtime_ready()) return;
    if (metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] & METARUN_CHALLENGE_FIXED_FLAG) return;

    metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] |= METARUN_CHALLENGE_FIXED_FLAG;
    save_metaruns();
}

bool metarun_challenge_tulkas_blunt_unlocked(void)
{
    byte flags;

    if (!metarun_runtime_ready()) return false;

    flags = metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX];
    if (!(flags & METARUN_CHALLENGE_TULKAS_BLUNT_FLAG) &&
        (metarun_quest_completion_count(METARUN_QUEST_TULKAS_ORCS) > 0 ||
         metarun_challenge_completion_count(CHALLENGE_TULKAS_BLUNT) > 0)) {
        metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] |= METARUN_CHALLENGE_TULKAS_BLUNT_FLAG;
        save_metaruns();
        flags = metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX];
    }

    return (flags & METARUN_CHALLENGE_TULKAS_BLUNT_FLAG) != 0;
}

void metarun_unlock_challenge_tulkas_blunt(void)
{
    if (!metarun_runtime_ready()) return;
    if (metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] & METARUN_CHALLENGE_TULKAS_BLUNT_FLAG) return;

    metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] |= METARUN_CHALLENGE_TULKAS_BLUNT_FLAG;
    save_metaruns();
}

bool metarun_challenge_torchlight_unlocked(void)
{
    byte flags;

    if (!metarun_runtime_ready()) return false;

    flags = metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX];
    if (!(flags & METARUN_CHALLENGE_TORCHLIGHT_FLAG) &&
        metarun_challenge_completion_count(CHALLENGE_TORCHLIGHT) > 0) {
        metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] |= METARUN_CHALLENGE_TORCHLIGHT_FLAG;
        save_metaruns();
        flags = metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX];
    }

    return (flags & METARUN_CHALLENGE_TORCHLIGHT_FLAG) != 0;
}

void metarun_unlock_challenge_torchlight(void)
{
    if (!metarun_runtime_ready()) return;
    if (metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] & METARUN_CHALLENGE_TORCHLIGHT_FLAG) return;

    metar.reserved_runtime[METARUN_RUNTIME_CHALLENGE_FLAGS_IDX] |= METARUN_CHALLENGE_TORCHLIGHT_FLAG;
    save_metaruns();
}

int metarun_challenge_completion_count(int challenge_id)
{
    byte *slot = challenge_count_slot(challenge_id);
    if (!metarun_runtime_ready() || !slot) return 0;
    return *slot;
}

void metarun_mark_challenge_completed(int challenge_id)
{
    byte *slot;

    if (!metarun_runtime_ready()) return;

    slot = challenge_count_slot(challenge_id);
    if (!slot) return;
    if (*slot < 255) (*slot)++;
    save_metaruns();
}

int metarun_mandos_resurrection_charges(void)
{
    byte *slot = metarun_quest_reserved_slot(METARUN_SLOT_MANDOS_RES_CHARGES);
    if (!metarun_runtime_ready() || !slot) return 0;
    return *slot;
}

void metarun_add_mandos_resurrection_charge(void)
{
    byte *slot;

    if (!metarun_runtime_ready()) return;

    slot = metarun_quest_reserved_slot(METARUN_SLOT_MANDOS_RES_CHARGES);
    if (!slot || *slot >= 1) return;
    *slot = 1;
    save_metaruns();
}

bool metarun_consume_mandos_resurrection_charge(void)
{
    byte *slot;

    if (!metarun_runtime_ready()) return false;

    slot = metarun_quest_reserved_slot(METARUN_SLOT_MANDOS_RES_CHARGES);
    if (!slot || *slot == 0) return false;
    (*slot)--;
    save_metaruns();
    return true;
}

int metarun_niena_curse_cleanses(void)
{
    byte *slot = metarun_quest_reserved_slot(METARUN_SLOT_NIENA_CURSE_CLEANSE);
    if (!metarun_runtime_ready() || !slot) return 0;
    return *slot;
}

void metarun_add_niena_curse_cleansing_charge(void)
{
    byte *slot;

    if (!metarun_runtime_ready()) return;

    slot = metarun_quest_reserved_slot(METARUN_SLOT_NIENA_CURSE_CLEANSE);
    if (!slot || *slot >= 1) return;
    *slot = 1;
    save_metaruns();
}

bool metarun_consume_niena_curse_cleansing_charge(void)
{
    byte *slot;

    if (!metarun_runtime_ready()) return false;

    slot = metarun_quest_reserved_slot(METARUN_SLOT_NIENA_CURSE_CLEANSE);
    if (!slot || *slot == 0) return false;
    (*slot)--;
    save_metaruns();
    return true;
}

byte metarun_orome_great_hunt_mask(void)
{
    byte *slot = metarun_quest_reserved_slot(METARUN_SLOT_OROME_GREAT_HUNT_MASK);
    if (!metarun_runtime_ready() || !slot) return 0;
    return *slot;
}

void metarun_set_orome_great_hunt_mask(byte mask)
{
    byte *slot;

    if (!metarun_runtime_ready()) return;

    slot = metarun_quest_reserved_slot(METARUN_SLOT_OROME_GREAT_HUNT_MASK);
    if (!slot || *slot == mask) return;
    *slot = mask;
    save_metaruns();
}

bool metarun_orome_great_hunt_active(void)
{
    byte *slot = metarun_quest_reserved_slot(METARUN_SLOT_OROME_GREAT_HUNT_ACTIVE);
    if (!metarun_runtime_ready() || !slot) return false;
    return *slot != 0;
}

void metarun_set_orome_great_hunt_active(bool active)
{
    byte value = active ? 1 : 0;
    byte *slot;

    if (!metarun_runtime_ready()) return;

    slot = metarun_quest_reserved_slot(METARUN_SLOT_OROME_GREAT_HUNT_ACTIVE);
    if (!slot || *slot == value) return;
    *slot = value;
    save_metaruns();
}

void metarun_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    ui_semantic_prompt_label(binding, fallback, buf, buflen);
}

/* ----------------------- accessors --------------------------- */
const metarun *metarun_current(void)
{
    if (!metaruns) return NULL;
    if (current_run < 0 || current_run >= metarun_max) return NULL;
    return &metaruns[current_run];
}

metarun *metarun_current_mutable(void)
{
    if (!metaruns) return NULL;
    if (current_run < 0 || current_run >= metarun_max) return NULL;
    return &metaruns[current_run];
}

const metarun *metarun_entry_const(s16b idx)
{
    if (!metaruns) return NULL;
    if (idx < 0 || idx >= metarun_max) return NULL;
    return &metaruns[idx];
}

metarun *metarun_entry_mutable(s16b idx)
{
    if (!metaruns) return NULL;
    if (idx < 0 || idx >= metarun_max) return NULL;
    return &metaruns[idx];
}

s16b metarun_current_index(void)
{
    if (!metaruns) return -1;
    if (current_run < 0 || current_run >= metarun_max) return -1;
    return current_run;
}

s16b metarun_entry_count(void)
{
    return metarun_max;
}

int metarun_completed_count(void)
{
    int completed = 0;

    if (!metaruns)
        return 0;

    for (s16b i = 0; i < metarun_max; i++) {
        const metarun *m = &metaruns[i];
        int win_goal = WINCON_SILMARILS;

        if (runtype_info && z_info && m->type < z_info->rt_max) {
            win_goal = runtype_info[m->type].win_con
                ? runtype_info[m->type].win_con
                : WINCON_SILMARILS;
        }

        if (m->silmarils >= win_goal)
            completed++;
    }

    return completed;
}

int any_curse_flag_active(u32b flag)
{
    /* Intended for CUR flags such as CUR_NOCHOICE (curse-only, not blessings). */
    if (!z_info || !cu_info) return 0;
    int count = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        int stacks = CURSE_GET(id);
        if (stacks > 0 && (cu_info[id].flags_u & flag)) count += stacks;
    }
    return count;
}

/* ---------------------------------------------------------------
 * Simple counters used by other modules (no UI side-effects)
 * ------------------------------------------------------------- */
void metarun_increment_deaths(void)
{
    /* Clamp to byte range; defer saving/UI to caller */
    if (metar.deaths >= 255) return;

    metar.deaths++;

    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_increment_deaths: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
    refresh_current_metar_score();
}

void metarun_gain_silmarils(byte n)
{
    if (!n) return;
    int total = (int)metar.silmarils + (int)n;
    if (total > 255) total = 255;
    if (total < 0) total = 0;
    metar.silmarils = (byte)total;
    refresh_current_metar_score();

    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_gain_silmarils: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
}

/* ---------------------------------------------------------------
 * Persistent Settings Management
 * ------------------------------------------------------------- */

/*
 * Save current game options to the metarun persistent settings
 */
void metarun_save_persistent_settings(void)
{
    log_info("Saving persistent settings to metarun");

    /* Save options */
    for (int i = 0; i < 8; i++) {
        metar.persistent_options[i] = 0;
    }

    /* Pack options into the persistent storage */
    for (int i = 0; i < OPT_MAX; i++) {
        int word_idx = i / 32;
        int bit_idx = i % 32;

        if (word_idx < 8 && option_text[i] && !option_is_app_persistent(i)
            && op_ptr->opt[i]) {
            metar.persistent_options[word_idx] |= (1UL << bit_idx);
        }
    }

    /* Save window flags */
    for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
        metar.persistent_window_flags[i] = op_ptr->window_flag[i];
    }

    /* Mark as initialized */
    metar.persistent_options_initialized = 1;

    /* Save the metarun data */
    save_metaruns();

    log_info("Persistent settings saved successfully");
}

/*
 * Load metarun persistent settings to current game options
 */
void metarun_load_persistent_settings(void)
{
    /* Only load if settings have been previously saved */
    if (!metar.persistent_options_initialized) {
        log_info("No persistent settings found, using defaults");
        return;
    }

    log_info("Loading persistent settings from metarun");

    /* Load options */
    for (int i = 0; i < OPT_MAX; i++) {
        int word_idx = i / 32;
        int bit_idx = i % 32;

        if (word_idx < 8 && option_text[i] && !option_is_app_persistent(i)) {
            op_ptr->opt[i] = (metar.persistent_options[word_idx] & (1UL << bit_idx)) != 0;
        }
    }

    /* Load window flags */
    for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
        op_ptr->window_flag[i] = metar.persistent_window_flags[i];
    }

    log_info("Persistent settings loaded successfully");
}

/* ------------------------------------------------------------------ */
/*  Oath system tracking                                              */
/* ------------------------------------------------------------------ */

/*
 * Check if an oath is unlocked in the current metarun
 */
bool oath_unlocked(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) return false;
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) return false;

    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-5 to bits 1,2,4,8,16 */
    return (metaruns[current_run].unlocked_oaths & oath_bit) != 0;
}

/*
 * Check if an oath is banned in the current metarun
 */
bool oath_banned(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) return false;
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) return false;

    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-5 to bits 1,2,4,8,16 */
    return (metaruns[current_run].banned_oaths & oath_bit) != 0;
}

/*
 * Unlock an oath in the current metarun
 */
void metarun_unlock_oath(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Oath unlock: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) {
        log_trace("Oath unlock: Invalid oath_id=%d", oath_id);
        return;
    }

    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-based oath_id to bitmask */

    /* Update both the global metar and the metaruns array */
    metar.unlocked_oaths |= oath_bit;
    metaruns[current_run].unlocked_oaths |= oath_bit;

    log_trace("Oath unlock: Unlocked oath %d (bit %d) in metarun[%d], unlocked_oaths=0x%02X",
              oath_id, oath_bit, current_run, metaruns[current_run].unlocked_oaths);

    /* Save immediately to persist the change */
    save_metaruns();
}

/*
 * Ban an oath in the current metarun (when broken)
 */
void metarun_ban_oath(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Oath ban: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) {
        log_trace("Oath ban: Invalid oath_id=%d", oath_id);
        return;
    }

    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-based oath_id to bitmask */

    /* Update both the global metar and the metaruns array */
    metar.banned_oaths |= oath_bit;
    metaruns[current_run].banned_oaths |= oath_bit;

    log_trace("Oath ban: Banned oath %d (bit %d) in metarun[%d], banned_oaths=0x%02X",
              oath_id, oath_bit, current_run, metaruns[current_run].banned_oaths);

    /* Save immediately to persist the change */
    refresh_current_metar_score();
    save_metaruns();
}

/*
 * Get bitmask of oaths available for selection (unlocked but not banned)
 */
int get_available_oaths_mask(void)
{
    if (blitz_oaths_enabled()) {
        int available = 0;
        int max_oath_id;

        if (!z_info)
            return 0;
        if (z_info->oath_max <= 1)
            return 0;

        max_oath_id = MIN(OATH_LIGHT, z_info->oath_max - 1);

        for (int i = 1; i <= max_oath_id; i++)
            available |= (1 << (i - 1));

        return available;
    }

    if (current_run < 0 || current_run >= metarun_max) return 0;

    byte unlocked = metaruns[current_run].unlocked_oaths;
    byte banned = metaruns[current_run].banned_oaths;
    byte available = unlocked & ~banned;

    log_trace("Oath availability: unlocked=0x%02X, banned=0x%02X, available=0x%02X",
              unlocked, banned, available);

    return available;
}

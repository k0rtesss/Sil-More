/* --------------------------------------------------------------------
 *  src/metarun.c   (2025-07-06)   â€“ final, crash-free, warning-free
 * --------------------------------------------------------------------
 *  Tracks a â€œmeta-runâ€ that ends after 15 Silmarils (win) or
 *  15 deaths (lose).  Finished runs are appended to meta.raw so
 *  the entire history is preserved.  Includes:
 *     â€¢ list_metaruns()  â€“ compact history view
 *     â€¢ print_metarun_stats() â€“ details for current run
 * -------------------------------------------------------------------- */

#ifndef WINDOWS
#define _DEFAULT_SOURCE  /* For DT_DIR and other POSIX extensions */
#define _BSD_SOURCE      /* For setregid on older systems */
#endif

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "platform-audio.h"
#include "platform-config.h"
#include "platform-input.h"
#include "platform-story-font.h"
#include "reliability-checks.h"
#include "app/app-ui.h"
#include "metarun.h"
#include "runtime/runtime-game.h"
#include "score/score_entry.h"
#include "score/score_io.h"
#include "ui/ui-information-scene.h"
#include "h-define.h"
#include "platform.h"    /* MKDIR helper                      */
#include "supplies.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#ifdef WINDOWS
#include <windows.h>
#else
#include <dirent.h>
#include <sys/types.h>
#endif  

/* --------------------------------------------------------------- */
/*  metarun.c : quick-and-dirty logger                             */
/* --------------------------------------------------------------- */

/* Enable this to delete old save/score files on fresh metarun start.
 * Currently disabled to prevent accidental data loss during debugging. */
/* #define METARUN_CLEANUP_OLD_FILES */

/* =========================  constants  ========================= */
#define CURSE_MENU_LINES  3
#define METARUN_RUNTIME_CHALLENGE_FLAGS_IDX 0
#define METARUN_CHALLENGE_DISCON_FLAG 0x01
#define METARUN_CHALLENGE_SINGLE_FLAG 0x02
#define METARUN_CHALLENGE_FIXED_FLAG 0x04
#define METARUN_CHALLENGE_TULKAS_BLUNT_FLAG 0x08
#define METARUN_CHALLENGE_TORCHLIGHT_FLAG 0x10
#define METARUN_RUNTIME_CHALLENGE_COUNT_BASE 1
#define METARUN_KNOWN_CURSE_PAGE_SIZE 12

/* =========================  globals  =========================== */
metarun         metar;
static metarun *metaruns    = NULL;
static s16b     metarun_max = 0;
static s16b     current_run = 0;
bool            metarun_created = false;

/* ==================  tiny local helpers  ======================= */
static int rng_int(int max) { return max ? (int)(rand() % max) : 0; }

static int popcount32(u32b value)
{
    int count = 0;
    while (value) {
        value &= (value - 1);
        count++;
    }
    return count;
}

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

static void metarun_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
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

static int parse_character_file(SDL_IOStream *fp)
{
    int count = 0;
    char line[1024];

    while (sdl_fgets(fp, line, sizeof(line)) == 0) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p || *p == '#') continue;
        if ((p[0] == 'N') && (p[1] == ':')) count++;
    }

    return count;
}

static int count_character_txt_entries(void)
{
    static int cached_total = -1;
    if (cached_total >= 0) return cached_total;

    const struct {
        cptr dir;
        cptr filename;
    } candidates[] = {
        { ANGBAND_DIR_SAVE, "character.txt" },
        { ANGBAND_DIR_USER, "character.txt" },
        { ANGBAND_DIR_APEX, "character.txt" },
        { ANGBAND_DIR_DATA, "character.txt" },
        { ANGBAND_DIR_EDIT, "character.txt" },
        { NULL, NULL }
    };

    char path[1024];
    SDL_IOStream *fp = NULL;

    for (size_t i = 0; candidates[i].dir; i++) {
        if (!candidates[i].dir || !*candidates[i].dir) continue;
        if (!path_build(path, sizeof(path), candidates[i].dir, candidates[i].filename))
        {
            log_error("count_character_txt_entries: failed to build path for %s/%s",
                candidates[i].dir ? candidates[i].dir : "(null)",
                candidates[i].filename ? candidates[i].filename : "(null)");
            continue;
        }
        log_debug("count_character_txt_entries: trying %s", path);
        fp = sdl_fopen(path, "r");
        if (fp) {
            cached_total = parse_character_file(fp);
            sdl_fclose(fp);
            log_debug("count_character_txt_entries: loaded %d entries from %s", cached_total, path);
            break;
        }
    }

    if (fp == NULL) {
        log_debug("count_character_txt_entries: no character.txt found in known locations");
        cached_total = 0;
    }

    return cached_total;
}

static u32b runtype_threshold_for_mode(int runtype_id, metarun_blessing_threshold_mode mode);
static u32b metarun_threshold_value(const metarun *m);
static const char *threshold_mode_name(metarun_blessing_threshold_mode mode);

/* Clamp blessing economy values after (re)computing the ledger */
static void update_blessing_ledger(metarun *m)
{
    if (!m) return;

    /* Get blessing point threshold from runtype data */
    u32b threshold = metarun_threshold_value(m);
    if (threshold == 0) threshold = 1;

    u32b total = m->fallen_score_total;
    u32b earned = total / threshold;
    u32b remainder = total % threshold;

    if (earned > (u32b)SHRT_MAX) {
        earned = (u32b)SHRT_MAX;
    }

    m->blessing_points = (s16b)earned;
    m->fallen_score_pool = remainder;
}

/* Rebuild blessing_points from fallen_score_total.
 * Does NOT clamp blessing_points_spent - that value must be preserved from save.
 * Clamping of spent vs earned happens only at spend-time, not at load-time. */
void metarun_sanitize_blessing_economy(metarun *m)
{
    if (!m) return;

    /* Rebuild blessing_points from fallen_score_total */
    update_blessing_ledger(m);

    /* blessing_points CAN be negative - that's valid
     * blessing_points_spent is preserved exactly as loaded */
}

void metarun_clear_blessing_runtime_fields(metarun *m)
{
    if (!m) return;

    m->fallen_score_total = 0;
    m->fallen_score_pool = 0;
    m->blessing_points = 0;
    m->blessing_points_spent = 0;
    m->major_blessings = 0;
    m->alive_characters = 0;
    
    /* Clear pending blessing choices */
    m->pending_blessing_count = 0;
    for (int i = 0; i < 3; i++) {
        m->pending_blessing_choices[i] = 255;
    }
    
    metarun_set_threshold_mode(m, METARUN_BLESSING_THRESHOLD_NORMAL);
    memset(m->reserved_runtime, 0, sizeof(m->reserved_runtime));
}

static int major_blessing_capacity(void)
{
    if (!z_info) return 0;
    int cap = (int)z_info->mb_max;
    if (cap < 0) cap = 0;
    if (cap > 16) cap = 16; /* stored in u16 bitmask */
    return cap;
}

static u16b major_blessing_mask(void)
{
    int cap = major_blessing_capacity();
    if (cap <= 0) return 0;
    if (cap >= 16) return 0xFFFFu;
    return (u16b)((1u << cap) - 1u);
}

void metarun_sanitize_major_blessing_bits(metarun *m)
{
    if (!m || !z_info || !mb_info) return;

    u16b mask = major_blessing_mask();
    if (mask == 0) {
        m->major_blessings = 0;
        return;
    }

    u16b defined_mask = 0;
    int cap = major_blessing_capacity();
    for (int i = 0; i < cap; i++) {
        const major_blessing_type *def = &mb_info[i];
        if (def->name)
            defined_mask |= (1U << i);
    }

    if (defined_mask == 0) {
        m->major_blessings = 0;
        return;
    }

    m->major_blessings &= (mask & defined_mask);
}

static const major_blessing_type *major_blessing_def(int idx)
{
    if (!mb_info || !z_info) return NULL;
    if (idx < 0 || idx >= (int)z_info->mb_max) return NULL;
    const major_blessing_type *def = &mb_info[idx];
    if (!def->name) return NULL;
    return def;
}

static cptr major_blessing_name_str(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_name || !def->name) return "(unknown)";
    return mb_name + def->name;
}

static cptr major_blessing_short_desc(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_text || !def->short_desc) return NULL;
    return mb_text + def->short_desc;
}

static cptr major_blessing_detail_desc(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_text || !def->detail_desc) return NULL;
    return mb_text + def->detail_desc;
}

static cptr major_blessing_unlock_msg(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_text || !def->unlock_msg) return NULL;
    return mb_text + def->unlock_msg;
}

static int major_blessing_cost(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def) return 0;
    if (def->cost == 0) return 3;
    return def->cost;
}

static metarun_major_effect major_blessing_effect(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def) return METARUN_MAJOR_EFFECT_NONE;
    return (metarun_major_effect)def->effect;
}

static void build_symbol_bar(char *out, size_t out_len, int current, int maximum, char filled)
{
    if (!out || out_len == 0) return;
    if (maximum <= 0) {
        strnfmt(out, out_len, "[]");
        return;
    }

    const int MAX_BAR_SLOTS = 20;
    int slots = maximum;
    if (slots > MAX_BAR_SLOTS) slots = MAX_BAR_SLOTS;
    if (slots < 1) slots = 1;

    char buffer[MAX_BAR_SLOTS + 1];
    for (int i = 0; i < slots; i++) {
        buffer[i] = (i < current) ? filled : '.';
    }
    buffer[slots] = '\0';

    strnfmt(out, out_len, "[%s]", buffer);
}

static void build_death_marks(char *out, size_t out_len, int deaths)
{
    if (!out || out_len == 0) return;
    if (deaths <= 0) {
        strnfmt(out, out_len, "none");
        return;
    }

    int max_marks = (int)out_len - 1;
    if (max_marks <= 0) {
        if (out_len > 0) out[0] = '\0';
        return;
    }

    if (deaths <= max_marks) {
        for (int i = 0; i < deaths; i++) out[i] = 'x';
        out[deaths] = '\0';
    } else {
        int marks = max_marks - 1;
        if (marks < 0) marks = 0;
        for (int i = 0; i < marks; i++) out[i] = 'x';
        out[marks] = '+';
        out[marks + 1] = '\0';
    }
}

static void refresh_alive_cache(void)
{
    int alive_scores = score_count_alive_entries();
    if (alive_scores < 0) alive_scores = 0;

    int roster_total = count_character_txt_entries();
    int alive_from_roster = roster_total - (int)metar.deaths;
    if (alive_from_roster < 0) {
        log_warn("refresh_alive_cache: metar.deaths=%d exceeds roster_total=%d", metar.deaths, roster_total);
        alive_from_roster = 0;
    }

    int alive = MAX(alive_scores, alive_from_roster);

    if (character_generated && p_ptr && !p_ptr->is_dead) {
        if (alive < 1) alive = 1;
    }

    if (alive > 255) alive = 255;
    metar.alive_characters = (byte)alive;

    log_debug("refresh_alive_cache: roster=%d deaths=%d scoreboard=%d final=%d",
              roster_total, metar.deaths, alive_scores, alive);
}

static u32b runtype_threshold_for_mode(int runtype_id, metarun_blessing_threshold_mode mode)
{
    u32b fallback = METARUN_BLESSING_POINT_THRESHOLD;

    if (!runtype_info || !z_info) return fallback;
    if (runtype_id < 0 || runtype_id >= z_info->rt_max) return fallback;

    runtype_type *rt = &runtype_info[runtype_id];

    int idx = (int)mode;
    if (idx < 0 || idx >= RUNTYPE_BLESSING_MODE_COUNT) idx = RUNTYPE_BLESSING_MODE_NORMAL;

    u16b val = rt->blessing_threshold_modes[idx];
    if (!val && idx != RUNTYPE_BLESSING_MODE_NORMAL) {
        val = rt->blessing_threshold_modes[RUNTYPE_BLESSING_MODE_NORMAL];
    }
    if (!val) val = METARUN_BLESSING_POINT_THRESHOLD;

    return (u32b)val;
}

static u32b metarun_threshold_value(const metarun *m)
{
    if (!m) return METARUN_BLESSING_POINT_THRESHOLD;
    return runtype_threshold_for_mode(m->type, metarun_get_threshold_mode(m));
}

static const char *threshold_mode_name(metarun_blessing_threshold_mode mode)
{
    switch (mode) {
        case METARUN_BLESSING_THRESHOLD_EASIER: return "Easier";
        case METARUN_BLESSING_THRESHOLD_HARDER: return "Harder";
        default: return "Normal";
    }
}

static u32b get_best_run_score_from_highscores(void)
{
    #define MAX_SCORES 100
    high_score scores[MAX_SCORES];
    int count = collect_high_scores(scores, MAX_SCORES, true);
    u32b best = 0;
    
    for (int i = 0; i < count; i++) {
        int pts = score_points(&scores[i]);
        if (pts > 0 && (u32b)pts > best) {
            best = (u32b)pts;
        }
    }
    
    #undef MAX_SCORES
    return best;
}

/* Calculate progressive diminishing score across all character runs
 * Formula: best/1 + second/2 + third/4 + fourth/8 + fifth/16 + ...
 * Rewards consistency while still heavily weighting best performance.
 * Caps at top 16 runs to prevent overflow and keep calculation fast.
 * Returns aggregate score contribution from character performance. */
static u32b compute_progressive_character_score(void)
{
    #define MAX_SCORES 100
    high_score scores[MAX_SCORES];
    int count = collect_high_scores(scores, MAX_SCORES, true); /* sorted by score descending */
    
    unsigned long long total = 0;
    unsigned long long divisor = 1;
    
    /* Process top 16 runs with progressive halving */
    for (int i = 0; i < count && i < 16; i++) {
        int pts = score_points(&scores[i]);
        if (pts > 0) {
            total += (unsigned long long)pts / divisor;
            divisor *= 2;  /* Each subsequent run worth half the previous */
        }
    }
    
    /* Clamp to u32b range */
    if (total > UINT32_MAX) return UINT32_MAX;
    
    log_debug("compute_progressive_character_score: processed %d runs, total=%u", 
              (count < 16 ? count : 16), (u32b)total);
    
    #undef MAX_SCORES
    return (u32b)total;
}

static u32b compute_metarun_score(const metarun *m)
{
    if (!m) return 0;

    /* Use progressive scoring across all character runs (v0.9.0.2+) */
    u32b progressive_score = compute_progressive_character_score();
    
    int quest_count = metarun_total_quest_completions(m);
    
    s32b total = (s32b)progressive_score;
    total += (s32b)m->silmarils * 120;
    total -= (s32b)m->deaths * 60;
    total += (s32b)60 * quest_count;
    total -= (s32b)100 * popcount32(m->banned_oaths);

    log_debug("compute_metarun_score: progressive=%u, sils=%d, deaths=%d, quest_count=%d (0x%08X), banned=%d => total=%d",
              progressive_score, m->silmarils, m->deaths, quest_count, m->completed_quests, 
              popcount32(m->banned_oaths), total);

    if (total < 0) total = 0;
    return (u32b)total;
}

void refresh_current_metar_score(void)
{
    if (!metaruns) return;
    if (current_run < 0 || current_run >= metarun_max) return;

    metar.score = compute_metarun_score(&metar);
    metaruns[current_run].score = metar.score;
}

static int compare_metarun_indices(const void *a, const void *b)
{
    const s16b ia = *(const s16b *)a;
    const s16b ib = *(const s16b *)b;

    if (!metaruns) return 0;

    const metarun *ma = &metaruns[ia];
    const metarun *mb = &metaruns[ib];

    if (ma->score != mb->score)
        return (ma->score < mb->score) ? 1 : -1;
    if (ma->last_played != mb->last_played)
        return (ma->last_played < mb->last_played) ? 1 : -1;
    if (ma->id < mb->id) return -1;
    if (ma->id > mb->id) return 1;
    return 0;
}

static bool build_meta_path(char *buf, size_t len,
    const metarun *m, const char *leaf)
{
    const char* name = leaf ? leaf : "";

    if (!m)
    {
#ifdef SIL_USE_LOCAL_DATA
        /* Portable build: use ANGBAND_DIR_APEX */
        if (!path_build(buf, len, ANGBAND_DIR_APEX, name))
#else
        /* Normal build: use parent of ANGBAND_DIR_METARUN (the meta directory) */
        char meta_dir[1024];
        if (ANGBAND_DIR_METARUN && *ANGBAND_DIR_METARUN) {
            SDL_strlcpy(meta_dir, ANGBAND_DIR_METARUN, sizeof(meta_dir));
            char* last_sep = strrchr(meta_dir, PATH_SEP[0]);
            if (last_sep) *last_sep = '\0';
        } else {
            SDL_strlcpy(meta_dir, ANGBAND_DIR_APEX, sizeof(meta_dir));
        }
        if (!path_build(buf, len, meta_dir, name))
#endif
        {
            log_error("build_meta_path: failed for apex/%s", name);
            return false;
        }
        return true;
    }

    char sub[128];
    if (name[0])
        strnfmt(sub, sizeof sub, "%s/%08u/%s",
            META_SUBDIR, (unsigned)m->id, name);
    else
        strnfmt(sub, sizeof sub, "%s/%08u",
            META_SUBDIR, (unsigned)m->id);
#ifdef SIL_USE_LOCAL_DATA
    if (!path_build(buf, len, ANGBAND_DIR_APEX, sub))
#else
    /* For metarun subdirectories, use ANGBAND_DIR_METARUN */
    if (!path_build(buf, len, ANGBAND_DIR_METARUN, sub))
#endif
    {
        log_error("build_meta_path: failed for %s", sub);
        return false;
    }
    return true;
}

static bool build_meta_sidecar_path(char* buf, size_t len, const char* live_path,
    const char* suffix)
{
    if (!buf || !len || !live_path || !suffix)
        return false;

    if (SDL_strlcpy(buf, live_path, len) >= len)
        return false;

    return SDL_strlcat(buf, suffix, len) < len;
}

static void recover_staged_metarun_file(const char* live_path)
{
    char staged_new[1024];
    char staged_old[1024];
    SDL_IOStream* live_fd = NULL;
    SDL_IOStream* new_fd = NULL;
    SDL_IOStream* old_fd = NULL;

    if (!build_meta_sidecar_path(staged_new, sizeof(staged_new), live_path, ".new")
        || !build_meta_sidecar_path(staged_old, sizeof(staged_old), live_path, ".old"))
        return;

    live_fd = sdl_fopen(live_path, "rb");
    if (live_fd)
    {
        sdl_fclose(live_fd);

        new_fd = sdl_fopen(staged_new, "rb");
        if (new_fd)
        {
            sdl_fclose(new_fd);
            log_warn("Removing stale staged metarun file '%s'", staged_new);
            fd_kill(staged_new);
        }
        return;
    }

    new_fd = sdl_fopen(staged_new, "rb");
    if (new_fd)
    {
        sdl_fclose(new_fd);
        if (fd_move(staged_new, live_path))
        {
            log_warn("Recovered staged metarun file '%s' -> '%s'", staged_new,
                live_path);
            return;
        }
    }

    old_fd = sdl_fopen(staged_old, "rb");
    if (old_fd)
    {
        sdl_fclose(old_fd);
        if (fd_move(staged_old, live_path))
            log_warn("Recovered previous metarun file '%s' -> '%s'", staged_old,
                live_path);
    }
}

static void reset_defaults(metarun *m)
{
    log_info("Initializing new metarun with default values");
    memset(m, 0, sizeof(*m));
    metarun_clear_blessing_runtime_fields(m);
    m->id          = 1;
    m->last_played = (u32b)time(NULL);
    memset(m->curse_stacks, 0, sizeof(m->curse_stacks));
    m->curses_seen = 0;
    m->deaths      = 0;
    m->silmarils   = 0;
    
    /* Initialize persistent settings with defaults */
    for (int i = 0; i < 8; i++) {
        m->persistent_options[i] = 0;
    }
    for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
        m->persistent_window_flags[i] = 0;
    }
    m->persistent_delay_factor = 5;      /* Default delay factor */
    m->persistent_hitpoint_warn = 3;     /* Default hitpoint warning */
    m->persistent_options_initialized = 0; /* Mark as not initialized yet */
    
    /* Initialize quest tracking */
    m->completed_quests = 0;             /* No quests completed initially */
    for (int i = 0; i < METARUN_QUEST_SLOT_MAX; i++) {
        m->quest_completion_counts[i] = 0;
    }
    metarun_clamp_and_sync_quests(m);
    
    /* Initialize oath system tracking */
    m->unlocked_oaths = 0;               /* No oaths unlocked initially */
    m->banned_oaths = 0;                 /* No oaths banned initially */
    m->max_difficulty_reached = 0;       /* Start with easiest difficulty */
    
    /* Clear quest_reserved array */
    for (int i = 0; i < 12; i++) {
        m->quest_reserved[i] = 0;
    }

    m->score = compute_metarun_score(m);
    update_blessing_ledger(m);

    log_debug("After init: curses_seen = 0x%016llX", (unsigned long long)m->curses_seen);
}

static bool ensure_default_metarun_slot(const char *reason)
{
    if (metarun_max > 0 && metaruns) return false;

    if (metaruns) {
        mem_free_null(metaruns);
        metaruns = NULL;
    }

    if (reason && *reason)
        log_warn("Metarun recovery triggered (%s); creating default entry", reason);
    else
        log_warn("Metarun recovery triggered; creating default entry");

    metarun_max = 1;
    metaruns = mem_alloc_array(metarun_max, metarun);
    reset_defaults(&metaruns[0]);
    metarun_created = true;

    return true;
}

/* Apply initial curses based on difficulty level (runtype) */
static void apply_difficulty_curses(metarun *m)
{
    if (!runtype_info) return; /* runtype data not loaded yet */
    if (m->type >= z_info->rt_max) return; /* invalid runtype */

    runtype_type *rt = &runtype_info[m->type];
    
    log_info("Applying curses for runtype %d (%s)", m->type, rt->name);
    
    /* Apply curses based on runtype configuration */
    if (rt->start_curses)
    {
        int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
        for (int curse_id = 0; curse_id < limit; curse_id++)
        {
            if (rt->start_curses & (1ULL << curse_id))
            {
                byte stacks = rt->curse_stacks[curse_id];
                if (stacks > 0)
                {
                    CURSE_SET(curse_id, stacks);
                    CURSE_SEEN_SET(curse_id);
                    log_debug("Applied %d stacks of curse %d from runtype", stacks, curse_id);
                }
            }
        }
    }
}

/* ensure directory apex/metaruns/NNNNNNNN exists */
static void ensure_run_dir(const metarun *m)
{
    char dir[1024];
    if (!path_build(dir, sizeof dir, ANGBAND_DIR_APEX, META_SUBDIR))
    {
        log_error("ensure_run_dir: failed to build base metarun directory");
        return;
    }
    MKDIR(dir);
    strnfmt(dir, sizeof dir, "%s/%08u", META_SUBDIR, (unsigned)m->id);
    if (!path_build(dir, sizeof dir, ANGBAND_DIR_APEX, dir))
    {
        log_error("ensure_run_dir: failed to build run directory for id=%u",
            (unsigned)m->id);
        return;
    }
    MKDIR(dir);
}

static bool sync_current_metarun_slot(bool stamp_time)
{
    if (!metaruns || current_run < 0 || current_run >= metarun_max) {
        return false;
    }

    if (stamp_time) {
        metar.last_played = (u32b)time(NULL);
    }

    metarun_clamp_and_sync_quests(&metar);
    metaruns[current_run] = metar;
    return true;
}

/* forward declarations */
static void start_new_metarun(void);
static void choose_difficulty_menu(bool reopen_stats_on_exit);
static bool metarun_adjust_blessing_threshold_information_scene(
    bool steamdeck, const char* accept_label, const char* back_label);
static bool metarun_list_history_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label);
static bool metarun_ui_add_effect_row_ex(app_ui_panel* panel, int id,
    bool selected);
static app_ui_panel* metarun_ui_begin_browser_scene(app_ui_scene* scene,
    byte title_attr, const char* title, byte subtitle_attr,
    const char* subtitle);
static app_ui_panel* metarun_ui_begin_story_scene(app_ui_scene* scene,
    byte title_attr, const char* title);
static void metarun_ui_clear_pending_input(void);
static bool metarun_ui_add_wrapped_detail_lines(app_ui_panel* panel, byte attr,
    const char* text);
static bool metarun_ui_add_story_paragraphs(app_ui_scene* scene,
    app_ui_panel* panel, const char* const* paragraphs, const byte* attrs,
    int paragraph_count);
static bool metarun_ui_add_effect_detail_lines(app_ui_panel* panel, int id);
static bool metarun_ui_add_known_curse_detail_lines(app_ui_panel* panel, int id);
static bool metarun_ui_show_notice_modal(const char* title, byte title_attr,
    const char* const* lines, const byte* attrs, int line_count, bool steamdeck,
    const char* accept_label);
static bool metarun_ui_show_story_modal(const char* title, byte title_attr,
    const char* const* paragraphs, const byte* attrs, int paragraph_count,
    bool steamdeck, const char* accept_label, const char* action_label);
static bool metarun_ui_confirm_modal(const char* title, byte title_attr,
    const char* const* lines, const byte* attrs, int line_count, bool steamdeck,
    const char* accept_label, const char* back_label);
static void metarun_present_story_texts(const char* title,
    cptr texts[], int total_texts, byte title_attr,
    byte text_attr, const char* action_label);
static cptr metarun_curse_choice_label(int n);
static int metarun_ui_choose_curse_scene(int n,
    const int picks[CURSE_MENU_LINES], bool steamdeck,
    const char* accept_label);
static void metarun_log_blessing_key(const char* context, int mode, int key);
static bool metarun_show_known_curses_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label);
/* =======================  load / save  ========================= */

/*
 * Clean up old save and score files when starting fresh (no meta.raw exists)
 */
void cleanup_old_game_files(void)
{
#ifndef METARUN_CLEANUP_OLD_FILES
    log_info("*** FRESH STARTUP CLEANUP DISABLED (METARUN_CLEANUP_OLD_FILES not defined) ***");
    return;
#else
    log_info("*** FRESH STARTUP CLEANUP STARTING ***");
    
    /* Use the correct save directory - ANGBAND_DIR_SAVE points to lib/save */
    char save_dir[1024];
    strnfmt(save_dir, sizeof(save_dir), "%s", ANGBAND_DIR_SAVE);
    
    log_trace("Fresh startup: checking save directory: %s", save_dir);
    
    /* Platform-agnostic approach: scan directory for ANY files (except .gitignore and archives) */
    bool has_save_files = false;
    
    #ifdef WINDOWS
    /* Windows: Use FindFirstFile/FindNextFile for directory scanning */
    WIN32_FIND_DATA findData;
    char search_path[1024];
    if (!path_build(search_path, sizeof(search_path), save_dir, "*"))
    {
        log_error("cleanup_old_game_files: failed to build save directory search path");
        return;
    }
    
    HANDLE hFind = FindFirstFile(search_path, &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            /* Skip directories and special entries */
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            
            char* filename = findData.cFileName;
            
            /* Skip .gitignore and archive files */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;
            
            /* Found a save file! */
            has_save_files = true;
            log_trace("Fresh startup: found save file: %s", filename);
            break;
            
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }
    #else
    /* Unix/Linux/macOS: Use POSIX opendir/readdir */
    DIR *dir = opendir(save_dir);
    if (dir) {
        struct dirent *entry;
        
        while ((entry = readdir(dir)) != NULL) {
            /* Skip directories and special entries */
            if (entry->d_type == DT_DIR) continue;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            
            char* filename = entry->d_name;
            
            /* Skip .gitignore and archive files */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;
            
            /* Found a save file! */
            has_save_files = true;
            log_trace("Fresh startup: found save file: %s", filename);
            break;
        }
        closedir(dir);
    }
    #endif
    
    /* ULTRA FAST EXIT if no save files detected */
    if (!has_save_files) {
        log_info("*** NO SAVE FILES DETECTED - INSTANT FRESH START ***");
        
        /* Quick score file check and removal */
        char score_file[1024];
        if (path_build(score_file, sizeof(score_file), ANGBAND_DIR_APEX, "scores.raw"))
        {
            SDL_IOStream* score_fd = sdl_fopen(score_file, "rb");
            if (score_fd) {
                sdl_fclose(score_fd);
                log_info("*** REMOVING SCORE FILE FOR FRESH START ***");

                /* Platform-agnostic file removal using standard C */
                remove(score_file);
            } else {
                log_trace("Fresh startup: no score file found");
            }
        }
        else
        {
            log_error("cleanup_old_game_files: failed to build score file path");
        }
        
        log_info("*** INSTANT FRESH STARTUP COMPLETED ***");
        return;  /* INSTANT EXIT - no shell commands needed */
    }
    
    /* Comprehensive cleanup: delete ALL files except .gitignore and archive files using ONLY standard C */
    log_info("*** FOUND SAVE FILES - DELETING ALL NON-ARCHIVE FILES ***");
    
    /* Use ONLY standard C functions - no shell commands for better portability */
    int files_deleted = 0;
    
#ifdef WINDOWS
    /* Windows: Use FindFirstFile/FindNextFile to enumerate and delete */
    WIN32_FIND_DATA cleanupFindData;
    char cleanup_search_path[1024];
    if (path_build(cleanup_search_path, sizeof(cleanup_search_path), save_dir, "*"))
    {
        HANDLE hCleanupFind = FindFirstFile(cleanup_search_path, &cleanupFindData);
        if (hCleanupFind != INVALID_HANDLE_VALUE) {
            do {
                /* Skip directories and special entries */
                if (cleanupFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

                char* filename = cleanupFindData.cFileName;

                /* Skip .gitignore and archive files */
                if (strcmp(filename, ".gitignore") == 0) continue;
                if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;

                /* Delete this file using standard C */
                char file_path[1024];
                if (!path_build(file_path, sizeof(file_path), save_dir, filename))
                {
                    log_error("cleanup_old_game_files: failed to build deletion path for '%s'", filename);
                    continue;
                }

                if (remove(file_path) == 0) {
                    files_deleted++;
                    log_trace("Fresh startup: deleted file: %s", filename);
                } else {
                    log_trace("Fresh startup: failed to delete: %s", filename);
                }

            } while (FindNextFile(hCleanupFind, &cleanupFindData));
            FindClose(hCleanupFind);
        }
    }
    else
    {
        log_error("cleanup_old_game_files: failed to build cleanup search path");
    }
#else
    /* Unix/Linux/macOS: Use opendir/readdir to enumerate and delete */
    dir = opendir(save_dir);
    if (dir) {
        struct dirent *entry;
        
        while ((entry = readdir(dir)) != NULL) {
            /* Skip directories and special entries */
            if (entry->d_type == DT_DIR) continue;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            
            char* filename = entry->d_name;
            
            /* Skip .gitignore and archive files */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;
            
            /* Delete this file using standard C */
            char file_path[1024];
            if (!path_build(file_path, sizeof(file_path), save_dir, filename))
            {
                log_error("cleanup_old_game_files: failed to build deletion path for '%s'", filename);
                continue;
            }
            
            if (remove(file_path) == 0) {
                files_deleted++;
                log_trace("Fresh startup: deleted file: %s", filename);
            } else {
                log_trace("Fresh startup: failed to delete: %s", filename);
            }
        }
        closedir(dir);
    }
    #endif
    
    if (files_deleted > 0) {
        log_info("*** FRESH STARTUP DELETED %d FILES USING STANDARD C ***", files_deleted);
    } else {
        log_info("*** NO FILES FOUND TO DELETE ***");
    }
    
    /* Score file cleanup */
    char score_file[1024];
    if (!path_build(score_file, sizeof(score_file), ANGBAND_DIR_APEX, "scores.raw"))
    {
        log_error("cleanup_old_game_files: failed to build score file path during cleanup");
        return;
    }
    
    SDL_IOStream* score_fd = sdl_fopen(score_file, "rb");
    if (score_fd) {
        sdl_fclose(score_fd);
        log_info("*** REMOVING SCORE FILE FOR FRESH START ***");
        
        /* Platform-agnostic file removal using standard C */
        remove(score_file);
    }
    
    log_info("*** FRESH STARTUP CLEANUP COMPLETED ***");
#endif /* METARUN_CLEANUP_OLD_FILES */
}

errr load_metaruns(bool create_if_missing)
{
    char fn[1024];
    SDL_IOStream* fd;
    bool found_existing_data = false;

    if (!build_meta_path(fn, sizeof fn, NULL, META_RAW))
        return -1;
    recover_staged_metarun_file(fn);
    fd = sdl_fopen(fn, "rb");

    if (fd) {
        found_existing_data = true;
    }

    if (!fd && create_if_missing) {
        log_info("Creating new versioned metarun file: %s", fn);
        FILE_TYPE(FILE_TYPE_DATA);
        fd = sdl_fmake(fn, 0644);
        if (!fd) return -1;

        /* Write versioned header */
        meta_file_header meta_hdr;
        meta_hdr.version_major = METARUN_FILE_VERSION_MAJOR;
        meta_hdr.version_minor = METARUN_FILE_VERSION_MINOR;
        meta_hdr.version_patch = METARUN_FILE_VERSION_PATCH;
        meta_hdr.version_extra = METARUN_FILE_VERSION_EXTRA;
        meta_hdr.entry_count = 1;

        sdl_write(fd, (cptr)&meta_hdr, sizeof(meta_hdr));

        metarun seed;
        reset_defaults(&seed);
        seed.score = compute_metarun_score(&seed);
        sdl_write(fd, (cptr)&seed, sizeof seed);
        sdl_fclose(fd);
        fd = sdl_fopen(fn, "rb");
        /* Only set metarun_created if we truly created a NEW file, not migrating existing data */
        if (!found_existing_data) {
            metarun_created = true;
            log_info("Created brand new metarun - will show story intro");
        } else {
            log_info("Seeded new metarun file from existing data - skipping intro");
        }
    }
    else log_info("Loading existing metarun file: %s", fn);
    if (!fd) return -1;

    /* All metarun files are versioned (v0.9.0+) */
    Sint64 file_size_64 = sdl_size(fd);
    int file_size = (file_size_64 > 0) ? (int)file_size_64 : 0;
    size_t meta_file_size = (file_size > 0) ? (size_t)file_size : 0;
    const char *recovery_reason = NULL;

    meta_file_header meta_hdr;
    sdl_seek(fd, 0);
    if (sdl_read(fd, (char*)&meta_hdr, sizeof(meta_hdr)) != 0) {
        log_error("Failed to read metarun header");
        sdl_fclose(fd);
        return -1;
    }

    log_info("Loading versioned meta file v%d.%d.%d.%d (%u entries)",
             meta_hdr.version_major, meta_hdr.version_minor,
             meta_hdr.version_patch, meta_hdr.version_extra, meta_hdr.entry_count);

    bool header_matches_current = (meta_hdr.version_major == METARUN_FILE_VERSION_MAJOR &&
                                   meta_hdr.version_minor == METARUN_FILE_VERSION_MINOR &&
                                   meta_hdr.version_patch == METARUN_FILE_VERSION_PATCH &&
                                   meta_hdr.version_extra == METARUN_FILE_VERSION_EXTRA);
    if (!header_matches_current) {
        log_warn("metarun: file version v%d.%d.%d.%d differs from game version v%d.%d.%d.%d",
                 meta_hdr.version_major, meta_hdr.version_minor, meta_hdr.version_patch, meta_hdr.version_extra,
                 METARUN_FILE_VERSION_MAJOR, METARUN_FILE_VERSION_MINOR, METARUN_FILE_VERSION_PATCH, METARUN_FILE_VERSION_EXTRA);
    }

    metarun_max = meta_hdr.entry_count;
    size_t payload = 0;
    size_t entry_size = 0;
    reliability_metarun_layout layout = RELIABILITY_METARUN_LAYOUT_INVALID;

    if (metarun_max > 0) {
        layout = reliability_detect_metarun_layout(meta_file_size,
            sizeof(meta_file_header), meta_hdr.entry_count, sizeof(metarun),
            (size_t)-1, (size_t)-2, (size_t)-3, &payload, &entry_size);
    }

    if (metarun_max > 0 && layout != RELIABILITY_METARUN_LAYOUT_INVALID) {
        metaruns = mem_alloc_array(metarun_max, metarun);
        if (!metaruns) {
            recovery_reason = "unable to allocate metarun array";
            sdl_fclose(fd);
            return -1;
        }
        sdl_seek(fd, sizeof(meta_file_header));

        if (layout == RELIABILITY_METARUN_LAYOUT_CURRENT) {
            if (sdl_read(fd, (char*)metaruns, metarun_max * sizeof(metarun)) != 0) {
                recovery_reason = "versioned meta.raw payload was truncated";
            } else {
                for (s16b i = 0; i < metarun_max; i++) {
                    if (meta_hdr.version_major == 0 && meta_hdr.version_minor < 9) {
                        metaruns[i].blessing_points_spent = 0;
                    }
                    /* Initialize pending blessing choices for pre-0.9.0.1 saves
                     * (fields were part of reserved_runtime and may contain garbage) */
                    if (meta_hdr.version_major == 0 && meta_hdr.version_minor == 9 &&
                        meta_hdr.version_patch == 0 && meta_hdr.version_extra == 0) {
                        /* Clear pending choices - will be regenerated on first menu open */
                        metaruns[i].pending_blessing_count = 0;
                        for (int j = 0; j < 3; j++) {
                            metaruns[i].pending_blessing_choices[j] = 255;
                        }
                        log_debug("Cleared pending blessing choices for metarun %d (loaded from v0.9.0.0)", i);
                    }
                    metarun_clamp_and_sync_quests(&metaruns[i]);
                    metarun_sanitize_blessing_economy(&metaruns[i]);
                    metarun_sanitize_major_blessing_bits(&metaruns[i]);
                }
            }
        } else {
            recovery_reason = "versioned meta.raw used unsupported legacy layout";
            log_warn("Unsupported legacy metarun layout size %zu; migration support has been removed",
                entry_size);
        }

        if (recovery_reason) {
            mem_free_null(metaruns);
            metaruns = NULL;
            metarun_max = 0;
        }
    } else if (metarun_max == 0) {
        recovery_reason = "versioned meta.raw reported zero entries";
        log_warn("Versioned meta file contains zero entries");
    } else if (entry_size > 0) {
        recovery_reason = "versioned meta.raw had unsupported entry size (requires v0.9.0+)";
        log_warn("Unsupported metarun entry size %zu in versioned file; dropping pre-0.9.0 legacy support",
            entry_size);
    } else {
        recovery_reason = "versioned meta.raw had invalid payload size";
        log_warn("Versioned meta file payload %zu does not align with %d entries",
                 payload, metarun_max);
        mem_free_null(metaruns);
        metaruns = NULL;
        metarun_max = 0;
    }

    if (metaruns) {
        for (s16b i = 0; i < metarun_max; i++) {
            metaruns[i].score = compute_metarun_score(&metaruns[i]);
        }
    }

    sdl_fclose(fd);

    bool seeded_default = false;
    if (metarun_max <= 0 || !metaruns) {
        seeded_default = ensure_default_metarun_slot(recovery_reason);
    }

    /* choose current run */
    u32b latest = 0;
    current_run = -1;  /* Initialize to invalid value so any valid entry will be selected */

    if (metarun_max > 0 && metaruns) {
        for (s16b i = 0; i < metarun_max; i++) {
            log_debug("Metarun %d: id=%u, last_played=%u, deaths=%u, silmarils=%u",
                      i, metaruns[i].id, metaruns[i].last_played, metaruns[i].deaths, metaruns[i].silmarils);

            if (metaruns[i].last_played > latest ||
                (metaruns[i].last_played == latest && i > current_run))
            {
                latest      = metaruns[i].last_played;
                current_run = i;
                log_debug("Selected metarun %d as current (last_played=%u)", i, latest);
            }
        }
    }

    if (current_run < 0 || current_run >= metarun_max) {
        if (ensure_default_metarun_slot("no valid metarun could be selected")) {
            seeded_default = true;
        }
        log_info("No valid metarun found, defaulting to entry 0");
        current_run = 0;
    }

    if (metarun_max <= 0 || !metaruns) {
        if (ensure_default_metarun_slot("metarun array unavailable before final selection")) {
            seeded_default = true;
        }
    }

    if (seeded_default) {
        log_info("Metarun loader seeded a default entry to recover from a corrupt or empty meta.raw");
    }

    metar = metaruns[current_run];
    metarun_clamp_and_sync_quests(&metar);
    metaruns[current_run].completed_quests = metar.completed_quests;
    memcpy(metaruns[current_run].quest_completion_counts,
           metar.quest_completion_counts,
           sizeof(metar.quest_completion_counts));
    metarun_sanitize_blessing_economy(&metar);
    metaruns[current_run].fallen_score_pool = metar.fallen_score_pool;
    metaruns[current_run].blessing_points = metar.blessing_points;
    metaruns[current_run].blessing_points_spent = metar.blessing_points_spent;
    metar.score = compute_metarun_score(&metar);
    metaruns[current_run].score = metar.score;
    metarun_apply_runtime_effects();
    log_debug("Final current_run=%d, metar: id=%u, deaths=%u, silmarils=%u",
              current_run, metar.id, metar.deaths, metar.silmarils);

    /* ensure its per-run directory exists */
    ensure_run_dir(&metar);
    
    /* Apply difficulty curses only if this is a newly created metarun */
    if (metarun_created)
    {
        apply_difficulty_curses(&metar);
        save_metaruns(); /* persist the changes */
    }
    
    log_debug("Loaded metarun %d with %d silmarils, %d deaths", metar.id, metar.silmarils, metar.deaths);
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Safely write the meta-run array.  Bail out if the indices look     *
 *  wrong â€“ avoids dereferencing a freed/reallocated block.           *
 * ------------------------------------------------------------------ */
static errr backup_file(const char *filepath)
{
    static u32b last_backup_time = 0;
    static char last_backed_up_file[1024] = "";
    u32b current_time = (u32b)time(NULL);
    
    /* Throttle backups: only create backup if 
     * 1. This is a different file than last time, OR
     * 2. More than 300 seconds (5 minutes) have passed since last backup of this file
     */
    if (SDL_strcasecmp(last_backed_up_file, filepath) != 0) {
        /* Different file - always backup */
        log_info("backup_file: backing up different file: %s", filepath);
    } else if (current_time - last_backup_time >= 300) {
        /* Same file but enough time has passed (5 minutes instead of 1 minute) */
        log_info("backup_file: backing up %s after %u seconds", filepath, current_time - last_backup_time);
    } else {
        /* Same file, recent backup - skip */
        log_trace("backup_file: skipping backup of %s (last backup %u seconds ago)", 
                  filepath, current_time - last_backup_time);
        return 0;
    }
    
    /* Check if original file exists */
    SDL_IOStream* fd_src = sdl_fopen(filepath, "rb");
    if (!fd_src) {
        /* Original file doesn't exist, no backup needed */
        log_info("backup_file: original file %s doesn't exist, no backup needed", filepath);
        return 0;
    }
    
    /* Get file size */
    Sint64 file_size_64 = sdl_size(fd_src);
    int file_size = (file_size_64 > 0) ? (int)file_size_64 : 0;
    if (file_size <= 0) {
        log_info("backup_file: original file %s is empty, no backup needed", filepath);
        sdl_fclose(fd_src);
        return 0;
    }
    
    log_info("backup_file: creating backup for %s (size: %d bytes)", filepath, file_size);
    
    /* Read original file */
    char *buffer = mem_alloc_array(file_size, char);
    if (!buffer) {
        sdl_fclose(fd_src);
        return -1;
    }
    
    if (sdl_read(fd_src, buffer, file_size) != 0) {
        buffer = mem_free(buffer);
        sdl_fclose(fd_src);
        return -1;
    }
    sdl_fclose(fd_src);
    
    /* Optimize backup rotation: Only do full rotation once per session/day
     * For frequent saves, just overwrite .bak1 */
    char backup_path1[1024], backup_path2[1024], backup_path3[1024];
    strnfmt(backup_path1, sizeof(backup_path1), "%s.bak1", filepath);
    strnfmt(backup_path2, sizeof(backup_path2), "%s.bak2", filepath);
    strnfmt(backup_path3, sizeof(backup_path3), "%s.bak3", filepath);
    
    /* Check if this is the first backup of the day (roughly) */
    bool should_rotate = false;
    SDL_IOStream* fd_test1 = sdl_fopen(backup_path1, "rb");
    if (fd_test1) {
        /* Check if bak1 is old enough to warrant rotation (use simple time check) */
        /* If we created a backup within the last hour, don't rotate */
        if (current_time - last_backup_time >= 3600) {  /* 1 hour */
            should_rotate = true;
            log_info("backup_file: enough time passed since last backup, will rotate backups");
        }
        sdl_fclose(fd_test1);
    } else {
        /* No bak1 exists, create fresh backup */
        should_rotate = false;
        log_info("backup_file: no existing backup, creating fresh bak1");
    }
    
    if (should_rotate) {
        log_info("backup_file: rotating backups for %s", filepath);
        
        /* Rotate: bak2 -> bak3, bak1 -> bak2, current -> bak1 */
        fd_kill(backup_path3);                    /* Remove oldest */
        log_debug("backup_file: removed old bak3");
        
        /* Move bak2 to bak3 (if bak2 exists) */
        SDL_IOStream* fd_test2 = sdl_fopen(backup_path2, "rb");
        if (fd_test2) {
            sdl_fclose(fd_test2);
            log_debug("backup_file: moving bak2 to bak3");
            if (!fd_move(backup_path2, backup_path3)) {
                log_debug("backup_file: failed to move bak2 to bak3");
            }
        }
        
        /* Move bak1 to bak2 (if bak1 exists) */
        fd_test1 = sdl_fopen(backup_path1, "rb");
        if (fd_test1) {
            sdl_fclose(fd_test1);
            log_debug("backup_file: moving bak1 to bak2");
            if (!fd_move(backup_path1, backup_path2)) {
                log_debug("backup_file: failed to move bak1 to bak2");
            }
        }
    } else {
        /* Just overwrite bak1 for frequent saves */
        log_debug("backup_file: overwriting existing bak1 (frequent save)");
        fd_kill(backup_path1);
    }
    
    /* Create new bak1 from current file */
    log_info("backup_file: creating new bak1 from current file (size: %d)", file_size);
    SDL_IOStream* fd_dst = sdl_fmake(backup_path1, 0644);
    if (!fd_dst) {
        buffer = mem_free(buffer);
        return -1;
    }
    
    errr result = sdl_write(fd_dst, buffer, file_size);
    sdl_fclose(fd_dst);
    buffer = mem_free(buffer);
    
    if (result == 0) {
        log_info("backup_file: successfully created backup for %s", filepath);
        /* Update throttling variables only on successful backup */
        last_backup_time = current_time;
        SDL_strlcpy(last_backed_up_file, filepath, sizeof(last_backed_up_file));
    } else {
        log_error("backup_file: failed to write bak1 for %s", filepath);
    }
    
    return result;
}

errr save_metaruns(void)
{
    static u32b last_save_time = 0;
    u32b current_time = (u32b)time(NULL);
    
    /* Log save frequency tracking */
    if (last_save_time > 0) {
        u32b time_since_last = current_time - last_save_time;
        log_info("save_metaruns() called again after %u seconds", time_since_last);
    } else {
        log_info("save_metaruns() called for the first time this session");
    }
    last_save_time = current_time;

    refresh_current_metar_score();

    char fn[1024];
    char safe[1024];
    char previous[1024];
    SDL_IOStream* old_fd = NULL;
    if (!build_meta_path(fn, sizeof fn, NULL, META_RAW))
        return -1;
    if (!build_meta_sidecar_path(safe, sizeof(safe), fn, ".new")
        || !build_meta_sidecar_path(previous, sizeof(previous), fn, ".old"))
        return -1;

    /* Create backup before saving */
    backup_file(fn);

    log_debug("Before save: current_run=%d, metar: id=%u, deaths=%u, silmarils=%u, score=%u", 
              current_run, metar.id, metar.deaths, metar.silmarils, metar.score);
              
    metarun_clamp_and_sync_quests(&metar);
    metar.last_played      = current_time;
    metaruns[current_run] = metar;            /* safe: array is valid */
    
    log_debug("After updating array: metaruns[%d]: id=%u, deaths=%u, silmarils=%u, score=%u", 
              current_run, metaruns[current_run].id, metaruns[current_run].deaths, metaruns[current_run].silmarils,
              metaruns[current_run].score);
    
    /* Write using the new versioned format */
    fd_kill(safe);
    SDL_IOStream* fd = sdl_fmake(safe, 0644);
    if (!fd) {
        log_info("Failed to create metarun file for writing");
        return -1;
    }

    /* Write version header first */
    meta_file_header meta_hdr;
    meta_hdr.version_major = METARUN_FILE_VERSION_MAJOR;
    meta_hdr.version_minor = METARUN_FILE_VERSION_MINOR;
    meta_hdr.version_patch = METARUN_FILE_VERSION_PATCH;
    meta_hdr.version_extra = METARUN_FILE_VERSION_EXTRA;
    meta_hdr.entry_count = metarun_max;
    
    errr result = sdl_write(fd, (cptr)&meta_hdr, sizeof(meta_hdr));
    if (result != 0) {
        sdl_fclose(fd);
        log_info("Failed to write metarun header to file");
        return -1;
    }

    /* Write metarun data */
    int bytes_to_write = metarun_max * sizeof(metarun);
    result = sdl_write(fd, (cptr)metaruns, bytes_to_write);
    sdl_fclose(fd);
    
    if (result != 0) {
        log_info("Failed to write metarun data to file");
        fd_kill(safe);
        return -1;
    }

    fd_kill(previous);
    old_fd = sdl_fopen(fn, "rb");
    if (old_fd)
    {
        sdl_fclose(old_fd);
        if (!fd_move(fn, previous))
        {
            log_error("Failed to stage previous metarun file '%s' -> '%s'", fn,
                previous);
            fd_kill(safe);
            return -1;
        }
    }

    if (!fd_move(safe, fn))
    {
        SDL_IOStream* previous_fd = NULL;
        log_error("Failed to activate staged metarun file '%s' -> '%s'", safe,
            fn);
        previous_fd = sdl_fopen(previous, "rb");
        if (previous_fd)
        {
            sdl_fclose(previous_fd);
            fd_move(previous, fn);
        }
        fd_kill(safe);
        return -1;
    }

    fd_kill(previous);
    
    log_info("Metarun data saved successfully (%d bytes, %d entries)", bytes_to_write, metarun_max);

    return 0;
}

u32b compute_blessing_pool(void)
{
    u32b total = score_sum_dead_points();
    metar.fallen_score_total = total;
    update_blessing_ledger(&metar);
    metarun_sanitize_major_blessing_bits(&metar);
    refresh_alive_cache();

    if (!sync_current_metarun_slot(false)) {
        log_warn("compute_blessing_pool: unable to sync current slot");
    }
    return total;
}

int blessing_points_available(void)
{
    u32b total = compute_blessing_pool();
    (void)total; /* Up-to-date total already stored in metar */

    int available = (int)metar.blessing_points - (int)metar.blessing_points_spent;
    if (available < 0) available = 0;
    return available;
}

int metarun_alive_count_cached(void)
{
    refresh_alive_cache();
    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_alive_count_cached: unable to sync current slot");
    }
    return metar.alive_characters;
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

void metarun_apply_runtime_effects(void)
{
    metarun_sanitize_major_blessing_bits(&metar);

    int weight_cap = SUPPLIES_MAX_WEIGHT_DEFAULT;
    if (metarun_has_major_blessing_effect(METARUN_MAJOR_EFFECT_SUPPLY_LIMIT)) {
        weight_cap = SUPPLIES_MAX_WEIGHT_BLESSING;
    }
    supplies_set_max_weight_cap(weight_cap);
}

int metarun_major_blessing_count(void)
{
    return major_blessing_capacity();
}

bool metarun_has_major_blessing_index(int idx)
{
    metarun_sanitize_major_blessing_bits(&metar);
    if (idx < 0) return false;
    int cap = major_blessing_capacity();
    if (idx >= cap) return false;
    if (!major_blessing_def(idx)) return false;
    return (metar.major_blessings & (1U << idx)) != 0;
}

bool metarun_has_major_blessing_effect(metarun_major_effect effect)
{
    if (effect == METARUN_MAJOR_EFFECT_NONE) return false;
    metarun_sanitize_major_blessing_bits(&metar);
    int cap = major_blessing_capacity();
    for (int i = 0; i < cap; i++) {
        if (!metarun_has_major_blessing_index(i)) continue;
        if (major_blessing_effect(i) == effect) return true;
    }
    return false;
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

/* ---------------------------------------------------------------
 * Pick a curse at random, respecting weights, stacks, caps,
 * and the RHF_CURSE tail-lift and exclusion of most weighted curses.
 * ------------------------------------------------------------- */
static int weighted_random_curse(void)
{
    long total = 0;
    int  w_max = 1;

    /* Does the hero's lineage carry the flag? */
    bool tilt = (p_info[p_ptr->prace].flags  & RHF_CURSE) ||
                (c_info[p_ptr->pcharacter].flags & RHF_CURSE);

    /* Pass 1 â€“ find the largest weight and (later) build the total */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* â† unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        if (w > w_max) w_max = w;
    }

    /* Pass 2 â€” sum effective weights */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* â† unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        int  cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        if (cap && cnt >= cap) continue;           /* cap reached */

        /* RHF_CURSE excludes the most weighted choices */
        if (tilt && w == w_max) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)           /* lift the tail */
            : w;

        total += base / (cnt + 1);
    }

    if (!total) return rng_int(z_info->cu_max);    /* safety net */

    /* Pass 3 â€” roulette wheel */
    long pick = rng_int(total), run = 0;
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* â† unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        int  cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        if (cap && cnt >= cap) continue;

        /* RHF_CURSE excludes the most weighted choices */
        if (tilt && w == w_max) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)
            : w;

        long eff = base / (cnt + 1);
        run += eff;
        if (pick < run) return i;
    }

    return rng_int(z_info->cu_max);                /* unreachable */
}

void add_curse_stack(int idx)
{
    /* respect per-curse stack cap */
    if (CURSE_CURSE_CAP(idx) &&
        CURSE_CURSE_STACK(idx) >= CURSE_CURSE_CAP(idx))
    {
        log_debug("Curse %d (%s) already at max stacks", idx, cu_name + cu_info[idx].name);
        return;
    }

    CURSE_ADD(idx, 1);
    log_info("Added curse stack: %s (now %d stacks)", cu_name + cu_info[idx].name, CURSE_GET(idx));
    save_metaruns();
}

static cptr metarun_curse_choice_label(int n)
{
    switch (n) {
        case 1: return "the second";
        case 2: return "the third";
        case 3: return "the fourth";
        default: return "a";
    }
}

int menu_choose_one_curse(int n)
{
    /* if any active curse has the "noâ€choice" flag, skip the menu */
    if (any_curse_flag_active(CUR_NOCHOICE))
        return weighted_random_curse();

    int pick[CURSE_MENU_LINES];
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    ui_information_scene_scope info_scope;

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        bool dup;
        do {
            dup     = false;
            pick[i] = weighted_random_curse();
            for (int j = 0; j < i; j++)
                if (pick[i] == pick[j]) { dup = true; break; }
            
            byte cap = (byte)CURSE_CURSE_CAP(pick[i]);
            if (cap && CURSE_CURSE_STACK(pick[i]) >= cap) { dup = true; continue; }

        } while (dup);
    }

    if (steamdeck) {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }
    if (!ui_information_scene_acquire(&info_scope)) {
        log_error("menu_choose_one_curse: semantic scene unavailable");
        return -1;
    }

    {
        int semantic_choice = metarun_ui_choose_curse_scene(n, pick, steamdeck,
            accept_label);

        ui_information_scene_leave(&info_scope);
        if (semantic_choice >= 0)
            return semantic_choice;
    }

    log_error("menu_choose_one_curse: semantic scene failed");
    return -1;
}


/* ------------------------------------------------------------------ *
 *  Debug helper â€“ wipe every active curse for the current meta-run.  *
 * ------------------------------------------------------------------ */
void metarun_clear_all_curses(void)
{
    log_info("Clearing all curses for current metarun");
    memset(metar.curse_stacks, 0, sizeof(metar.curse_stacks));
    metar.curses_seen = 0;
    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_clear_all_curses: unable to sync current slot");
    }
    save_metaruns();
}

/* ------------------------------------------------------------------ *
 *  Main entry point used by game exits, deaths, escapes, etc.        *
 *  NOTE: save_metaruns() comes **after** check_run_end() so that     *
 *  any realloc in start_new_metarun() has already finished.          *
 * ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ *
 *  Main entry point used by game exits, deaths, escapes, etc.        *
 * ------------------------------------------------------------------ */
/*
 * Metarun narrative & exit logic - refactor **v4** (30â€¯Julâ€¯2025)
 * ------------------------------------------------------------------
 *  âœ§Â Reâ€‘orders the sequence so NOTHING is overwritten:
 *      0.Â Escapeâ€‘curse chooser (UI)  â†’ clears screen once finished.
 *      1.Â Chosenâ€‘curse line(s).
 *      2.Â Victoryâ€¯banner & Silmaril count paragraph.
 *      3.Â TemptationÂ ofÂ Treachery (escalating 1â€‘3 lines).
 *      4.Â StoryÂ Fragment (depends on Silmarils & Treachery flag).
 *      5.Â EchoesÂ ofÂ Kinslaying (escalating 1â€‘3 lines)
 *      6.Â Final pause, then deferred sideâ€‘effects.
 *
 *  âœ§Â `choose_escape_curses_ui()` now **returns** the indices chosen and
 *    does NOT leave the menu clutter on screen. We reâ€‘render the
 *    â€œThe curse of X binds your fate.â€ lines after a clean clear.
 *
 *  âœ§Â Adds `print_story_fragment()` â€“ a short narrative bridge keyed off
 *    Silmaril count (1â€‘3) and whether treachery was overcome.
 *
 *  âœ§Â Tested matrix: {treachery flagâ€¯Ã—â€¯kinslayer flagâ€¯Ã—â€¯silmarils (1â€‘3)}
 *    All show in the intended order with no garbled overlaps.
 */

static cptr curse_display_name(int idx)
{
    cptr raw = cu_name + cu_info[idx].name;
    /* Strip common prefixes for cleaner display */
    if (strncmp(raw, "Curse of ", 9) == 0) raw += 9;
    else if (strncmp(raw, "Burden of ", 10) == 0) raw += 10;
    else if (strncmp(raw, "Sorrow of ", 10) == 0) raw += 10;
    else if (strncmp(raw, "Doom of ", 8) == 0) raw += 8;
    return raw;
}

static cptr blessing_display_name(int idx)
{
    if (cu_info[idx].blessing_name) {
        cptr raw = cu_name + cu_info[idx].blessing_name;
        /* Strip "Blessing of " prefix for consistency */
        if (strncmp(raw, "Blessing of ", 12) == 0) raw += 12;
        return raw;
    }
    return curse_display_name(idx);
}

/****************  Escapeâ€‘curse chooser (clean version) ************/

/*
 * Presents the menu *n* times (or once if CUR_NOCHOICE). Returns the
 * number of curses actually chosen and fills `out` with their indices.
 * The display is cleared afterwards so we can start narrative fresh.
 */
int choose_escape_curses_ui(int n, int out[4])
{
    // int rolls = any_curse_flag_active(CUR_NOCHOICE) ? 1 : n;
    int taken = 0;
    char intro_text[512];
    const char* paragraphs[] = { intro_text };

    strnfmt(intro_text, sizeof(intro_text),
            "The Valar watch silently as Morgoth's malice reaches out from shadow-"
            "Your triumph has drawn his wrath. His dark will twists fate, "
            "forcing upon you the final choice-%s curse%s you must bear.",
            (n == 1) ? "a" : (n == 2) ? "two" : (n == 3) ? "three" : "four",
            (n == 1) ? "" : "s");

    metarun_present_story_texts("The Valar's Judgment", (cptr*)paragraphs, 1,
        TERM_L_BLUE, TERM_WHITE, "Continue");

    for (int i = 0; i < n; i++)
    {
        int idx = menu_choose_one_curse(i);   /* weighted picker, UI */
        if (idx < 0)
        {
            log_error("choose_escape_curses_ui: curse selection failed after %d of %d picks",
                taken, n);
            return taken ? taken : -1;
        }
        log_debug("Player selected curse %d: %s", idx, cu_name + cu_info[idx].name);
        add_curse_stack(idx);                /* gameplay sideâ€‘effect */
        if (taken < 4) out[taken++] = idx;
    }
    
    return taken;
}

/****************  Oath-breaking curse chooser with fade ************/

/*
 * Shows the oath-specific curse message with fade-in, waits 3 seconds,
 * then shows the permanent consequence message and curse selection menu.
 * Returns the selected curse index.
 */
int choose_oath_breaking_curse_ui(int oath_id)
{
    /* Get oath-specific permanent message (E: field from oath.txt) */
    char* perm_msg = oath_permanent_message(oath_id);
    char intro_text[256];
    strnfmt(intro_text, sizeof(intro_text),
            "The breach of your sacred vow has drawn Morgoth's attention. "
            "His malice reaches out to compound your suffering with a curse you must bear.");

    {
        cptr first_paragraphs[] = {
            (perm_msg && perm_msg[0]) ? perm_msg
                : "Your oath is forever broken in this age."
        };
        cptr second_paragraphs[] = { intro_text };

        metarun_present_story_texts("The Sundering of Sacred Vows",
            first_paragraphs, 1, TERM_L_RED, TERM_L_RED, "Continue");
        metarun_present_story_texts("The Sundering of Sacred Vows",
            second_paragraphs, 1, TERM_L_RED, TERM_RED, "Choose");
    }

    /* Let the player choose 1 curse from 3 options */
    int idx = menu_choose_one_curse(0);
    if (idx < 0)
        log_error("choose_oath_breaking_curse_ui: curse selection failed");
    log_debug("Player selected curse %d for oath breaking", idx);
    
    return idx;
}

/* ------------------------------------------------------------------ */
/*  Standard â€œPress any keyâ€¦â€ prompts â€“ use enum, not raw strings     */
/* ------------------------------------------------------------------ */
typedef enum {
    PROMPT_CONTINUE_TALE,
    PROMPT_FACE_TEMPTATION,
    PROMPT_CONTINUE_GENERIC,
    PROMPT_FACE_ECHOES,
    PROMPT_CONCLUDE_TALE,
    PROMPT_WITNESS_CONSEQUENCES,
    PROMPT_RETURN_MIDDLE_EARTH
} prompt_t;

static const char* metarun_prompt_action_label(prompt_t id)
{
    switch (id) {
        case PROMPT_FACE_TEMPTATION: return "Temptation";
        case PROMPT_FACE_ECHOES: return "Echoes";
        case PROMPT_CONCLUDE_TALE: return "Conclude";
        case PROMPT_WITNESS_CONSEQUENCES: return "Witness";
        case PROMPT_RETURN_MIDDLE_EARTH: return "Return";
        case PROMPT_CONTINUE_TALE:
        case PROMPT_CONTINUE_GENERIC:
        default:
            return "Continue";
    }
}

static const char* challenge_display_name(int challenge_id)
{
    switch (challenge_id) {
        case CHALLENGE_DISCONNECTED: return "Disconnected stairs";
        case CHALLENGE_SINGLE_STAIR: return "Single stair";
        case CHALLENGE_FIXED_50K_XP: return "Fixed 50k XP";
        case CHALLENGE_TULKAS_BLUNT: return "Tulkas' blunt arms";
        case CHALLENGE_TORCHLIGHT: return "Varda's torches-only";
        default: return "Unknown challenge";
    }
}

static bool metarun_show_completed_quests_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label)
{
    struct quest_summary_entry {
        bool challenge_entry;
        int id;
        int count;
        int unlock_count;
    } entries[96];
    int entry_count = 0;
    int completed_quests = 0;
    int selected = 0;
    const int challenge_ids[] = {
        CHALLENGE_DISCONNECTED,
        CHALLENGE_SINGLE_STAIR,
        CHALLENGE_FIXED_50K_XP,
        CHALLENGE_TULKAS_BLUNT,
        CHALLENGE_TORCHLIGHT
    };

    if (z_info && quest_info) {
        for (int i = 1; i < z_info->quest_max
            && entry_count < (int)N_ELEMENTS(entries); i++)
        {
            quest_type *q_ptr = &quest_info[i];
            u32b flag;
            int count;

            if (!q_ptr->name) continue;
            flag = quest_metarun_flag(i);
            if (!flag) continue;
            count = metarun_quest_completion_count(flag);
            if (count <= 0) continue;

            entries[entry_count].challenge_entry = false;
            entries[entry_count].id = i;
            entries[entry_count].count = count;
            entries[entry_count].unlock_count = q_ptr->challenge_unlock
                ? metarun_challenge_completion_count(q_ptr->challenge_unlock)
                : 0;
            entry_count++;
            completed_quests++;
        }
    }

    for (int i = 0; i < (int)N_ELEMENTS(challenge_ids)
        && entry_count < (int)N_ELEMENTS(entries); i++)
    {
        entries[entry_count].challenge_entry = true;
        entries[entry_count].id = challenge_ids[i];
        entries[entry_count].count =
            metarun_challenge_completion_count(challenge_ids[i]);
        entries[entry_count].unlock_count = 0;
        entry_count++;
    }

    while (true) {
        app_ui_scene scene;
        app_ui_panel *panel;
        char subtitle[APP_UI_TEXT_MAX];
        int key;

        if (selected < 0) selected = 0;
        if (selected >= entry_count) selected = entry_count - 1;

        strnfmt(subtitle, sizeof(subtitle), "%d completed quest%s",
            completed_quests, (completed_quests == 1) ? "" : "s");
        panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
            "Completed Quests", TERM_SLATE, subtitle);
        if (!panel)
            return false;

        if (completed_quests == 0) {
            (void)app_ui_panel_add_body_line(panel, TERM_L_DARK,
                "No quests completed yet in this metarun.");
        }

        for (int i = 0; i < entry_count; i++) {
            char meta[APP_UI_META_MAX];
            byte attr;

            if (entries[i].challenge_entry) {
                strnfmt(meta, sizeof(meta), "completed %d",
                    entries[i].count);
                attr = TERM_WHITE;
                if (!app_ui_panel_add_row(panel, entries[i].id, attr, true,
                        i == selected, "", challenge_display_name(
                            entries[i].id), meta))
                {
                    return false;
                }
            } else {
                strnfmt(meta, sizeof(meta), "x%d", entries[i].count);
                attr = TERM_WHITE;
                if (!app_ui_panel_add_row(panel, entries[i].id, attr, true,
                        i == selected, "",
                        quest_display_title(entries[i].id), meta))
                {
                    return false;
                }
            }
        }

        if (selected >= 0 && selected < entry_count) {
            char line[APP_UI_TEXT_MAX];

            if (entries[selected].challenge_entry) {
                app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
                    "Challenge");
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE,
                        challenge_display_name(entries[selected].id)))
                {
                    return false;
                }
                strnfmt(line, sizeof(line), "Completed %d time%s.",
                    entries[selected].count,
                    (entries[selected].count == 1) ? "" : "s");
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
                    return false;
            } else {
                quest_type *q_ptr = &quest_info[entries[selected].id];
                app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
                    "Quest");
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE,
                        quest_display_title(entries[selected].id)))
                {
                    return false;
                }
                strnfmt(line, sizeof(line), "Completed %d time%s.",
                    entries[selected].count,
                    (entries[selected].count == 1) ? "" : "s");
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
                    return false;
                if (q_ptr->challenge_unlock) {
                    strnfmt(line, sizeof(line), "Unlocks %s (completed %d)",
                        challenge_display_name(q_ptr->challenge_unlock),
                        entries[selected].unlock_count);
                    if (!app_ui_panel_add_detail_line(panel, TERM_L_BLUE,
                            line))
                    {
                        return false;
                    }
                }
            }
        }

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            steamdeck ? accept_label : "Any", "Close");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "8/2", "Move");
        if (steamdeck) {
            (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE,
                true, back_label, "Back");
        }

        if (!ui_information_scene_present_ui(&scene))
            return false;

        key = ui_information_scene_wait_key_nonrepeat();
        if (key == '8' || key == 'k' || key == '-') {
            selected = (selected + entry_count - 1) % entry_count;
            continue;
        }
        if (key == '2' || key == 'j' || key == '+') {
            selected = (selected + 1) % entry_count;
            continue;
        }

        metarun_ui_clear_pending_input();
        return true;
    }
}

static bool metarun_ui_show_story_texts(const char* title, byte title_attr,
    cptr texts[], int total_texts, byte text_attr, bool steamdeck,
    const char* accept_label, const char* action_label)
{
    byte* attrs = NULL;
    bool presented = false;

    if (total_texts > 0)
    {
        attrs = mem_alloc_array(total_texts, byte);
        if (!attrs)
            return false;
        for (int i = 0; i < total_texts; i++)
            attrs[i] = text_attr;
    }

    presented = metarun_ui_show_story_modal(title, title_attr,
        (const char* const*)texts, attrs, total_texts, steamdeck,
        accept_label, action_label);
    mem_free_null(attrs);
    return presented;
}

static void metarun_present_story_texts(const char* title,
    cptr texts[], int total_texts, byte title_attr,
    byte text_attr, const char* action_label)
{
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    ui_information_scene_scope info_scope;
    bool presented = false;

    if (steamdeck) {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }
    if (!ui_information_scene_acquire(&info_scope)) {
        log_error("metarun: semantic story scene unavailable for '%s'",
            title ? title : "(untitled)");
        return;
    }

    presented = metarun_ui_show_story_texts(title, title_attr, texts,
        total_texts, text_attr, steamdeck, accept_label,
        action_label ? action_label : "Continue");
    if (!presented) {
        log_error("metarun: failed to present story texts '%s'",
            title ? title : "(untitled)");
    }

    ui_information_scene_leave(&info_scope);
}

static void show_mandos_third_unlock_message(void)
{
    const char *lines[] = {
        "You endured the challenge of disconnected stairs.",
        "Mandos acknowledges your feat and sets a final doom:",
        "Seek Maeglin's hidden vault between delvings seventeen and nineteen and end his false life.",
        "This third quest stands apart and will not bar other callings."
    };
    metarun_present_story_texts("Mandos' Final Doom", lines,
        N_ELEMENTS(lines), TERM_L_BLUE, TERM_WHITE, "Continue");
}

static void activate_orome_great_hunt(void)
{
    byte mask = metarun_orome_great_hunt_mask();
    p_ptr->orome_great_hunt_mask = mask;
    metarun_set_orome_great_hunt_mask(mask);
    metarun_set_orome_great_hunt_active(true);
    if (quest_get_state(QUEST_ID_OROME_GREAT_HUNT) < QUEST_STATE_ACTIVE) {
        quest_set_state(QUEST_ID_OROME_GREAT_HUNT, QUEST_STATE_ACTIVE);
    }
    log_trace("Metarun: Orome great hunt activated (mask=0x%02x, state=%d)", mask, quest_get_state(QUEST_ID_OROME_GREAT_HUNT));
}

static void show_orome_great_hunt_unlock_message(void)
{
    const char *lines[] = {
        "You endured the single-stair challenge.",
        "Orome offers the Wraith for purchase and names a hunt that spans your line:",
        "Scatha, Smaug, Draugluin, Gostir, Shelob, Thuringwethil. Slay them in any delve.",
        "This hunt runs alongside all other quests until the last mark falls."
    };
    metarun_present_story_texts("Orome's Great Hunt", lines,
        N_ELEMENTS(lines), TERM_GREEN, TERM_WHITE, "Continue");
}

static void maybe_unlock_orome_great_hunt(bool challenge_single_stair_active)
{
    if (!challenge_single_stair_active) return;
    if (quest_get_state(QUEST_ID_OROME_GREAT_HUNT) >= QUEST_STATE_REWARDED) return;
    if (metarun_quest_completion_count(METARUN_QUEST_OROME_GREAT_HUNT) >= quest_completion_cap(QUEST_ID_OROME_GREAT_HUNT)) return;
    if (metarun_orome_great_hunt_active()) return;

    activate_orome_great_hunt();
    show_orome_great_hunt_unlock_message();
}

static void show_niena_mercy_unlock_message(void)
{
    const char *lines[] = {
        "You completed the fixed 50,000 XP challenge.",
        "Nienna smiles through tears: her Gift of Mercy may now be learned for 5000 experience.",
        "Find it in the Special abilities list if you would carry her pity into future delvings."
    };
    metarun_present_story_texts("Nienna's Gift of Mercy", lines,
        N_ELEMENTS(lines), TERM_L_BLUE, TERM_WHITE, "Continue");
}

static void show_tulkas_blunt_unlock_message(void)
{
    const char *lines[] = {
        "You endured the blunt-arms challenge.",
        "Tulkas roars with laughter and offers the lore of Unique Bane to all your line.",
        "Seek it among your Special abilities for 5000 experience in future delvings."
    };
    metarun_present_story_texts("Tulkas' Orc-Bane", lines,
        N_ELEMENTS(lines), TERM_YELLOW, TERM_WHITE, "Continue");
}

static void show_torchlight_unlock_message(void)
{
    const char *lines[] = {
        "You conquered Varda's torches-only challenge.",
        "The Queen of the Stars will now teach her radiance to any who pay 5000 experience.",
        "Find Queen of the Stars among your Special abilities in future delves."
    };
    metarun_present_story_texts("Queen of the Stars", lines,
        N_ELEMENTS(lines), TERM_WHITE, TERM_L_BLUE, "Continue");
}

static int total_player_kills_this_run(void)
{
    if (!z_info || !l_list) return 0;

    int total = 0;
    for (int i = 0; i < z_info->r_max; i++) {
        int kills = l_list[i].pkills;
        if (kills > 0) total += kills;
    }

    return total;
}

static void maybe_unlock_niena_mercy_purchase(bool challenge_fixed_active, int fixed_count_before)
{
    if (!challenge_fixed_active) return;
    if (fixed_count_before > 0) return; /* Already unlocked */
    int fixed_after = metarun_challenge_completion_count(CHALLENGE_FIXED_50K_XP);
    if (fixed_after <= fixed_count_before) return;
    show_niena_mercy_unlock_message();
}

static void maybe_unlock_queen_of_stars_purchase(bool challenge_torch_active, int torch_count_before)
{
    if (!challenge_torch_active) return;
    if (torch_count_before > 0) return; /* Already unlocked */
    int torch_after = metarun_challenge_completion_count(CHALLENGE_TORCHLIGHT);
    if (torch_after <= torch_count_before) return;
    show_torchlight_unlock_message();
}

static void resolve_niena_morgoth_quest_on_exit(bool escaped_with_sils)
{
    byte state = quest_get_state(QUEST_ID_NIENA_MORGOTH);
    if (state != QUEST_STATE_ACTIVE) return;

    bool violated = (p_ptr->niena_reserved & NIENA_FLAG_MORGOTH_ATTACKED) != 0;
    if (!escaped_with_sils || violated) {
        if (violated) {
            msg_print("You struck Morgoth; Nienna's mercy quest is lost.");
        }
        quest_set_state(QUEST_ID_NIENA_MORGOTH, QUEST_STATE_NOT_STARTED);
        niena_revoke_temp_mercy_gift(true);
        p_ptr->niena_reserved &= ~(NIENA_FLAG_MORGOTH_ATTACKED);
        return;
    }

    int completion_count = 0;
    cptr *completion_texts = extract_quest_completion_texts(QUEST_ID_NIENA_MORGOTH, &completion_count);
    if (completion_texts && completion_count > 0) {
        metarun_present_story_texts("Nienna's Mercy", completion_texts,
            completion_count, TERM_L_BLUE, TERM_WHITE, "Continue");
        free_quest_texts(completion_texts, completion_count);
    } else {
        const char *fallback[] = {
            "Nienna's presence returns as you escape with the Silmaril, untouched by your hand.",
            "'Your restraint has spared even the Black Foe a blow. Carry this mercy into the tales to come.'"
        };
        metarun_present_story_texts("Nienna's Mercy", fallback,
            N_ELEMENTS(fallback), TERM_L_BLUE, TERM_WHITE, "Continue");
    }

    quest_set_state(QUEST_ID_NIENA_MORGOTH, QUEST_STATE_REWARDED);
    p_ptr->niena_reserved &= ~(NIENA_FLAG_MORGOTH_ATTACKED);
    niena_revoke_temp_mercy_gift(true);
    metarun_mark_quest_completed(METARUN_QUEST_NIENA_MORGOTH);
    metarun_unlock_challenge_fixed_exp();
    p_ptr->quest_reserved[0] = 1;
    msg_print("The fixed 50k XP challenge is now unlocked.");
}

static void resolve_nienia_pacifist_quest_on_exit(bool died, bool escaped)
{
    int completion_cap = quest_completion_cap(QUEST_ID_NIENA_PACIFIST);
    if (completion_cap < 1) completion_cap = METARUN_QUEST_COMPLETION_CAP;
    int completion_count = metarun_quest_completion_count(METARUN_QUEST_NIENA_PACIFIST);

    if (completion_count >= completion_cap) return;

    byte state = quest_get_state(QUEST_ID_NIENA_PACIFIST);
    if (state >= QUEST_STATE_REWARDED) return;

    /* Only escapes can finish the quest */
    if (!escaped || died) {
        /* If kills accrued, make sure the failure flag is set for logging/UI */
        if (total_player_kills_this_run() > 0) {
            p_ptr->niena_reserved |= NIENA_FLAG_PACIFIST_FAILED;
        }
        return;
    }

    int kills = total_player_kills_this_run();
    bool failed = (kills > 0) || ((p_ptr->niena_reserved & NIENA_FLAG_PACIFIST_FAILED) != 0);
    if (failed) {
        log_trace("Niena pacifist quest not completed - kills=%d, flag=%d", kills, (p_ptr->niena_reserved & NIENA_FLAG_PACIFIST_FAILED));
        return;
    }

    int completion_count_text = 0;
    cptr *completion_texts = extract_quest_completion_texts(QUEST_ID_NIENA_PACIFIST, &completion_count_text);
    if (completion_texts && completion_count_text > 0) {
        metarun_present_story_texts("Nienna, Lady of Pity",
            completion_texts, completion_count_text, TERM_L_BLUE,
            TERM_WHITE, "Continue");
        free_quest_texts(completion_texts, completion_count_text);
    } else {
        const char *fallback[] = {
            "You return to the surface with no blood on your hands.",
            "Nienna's relief washes over you: the curses that followed you may be lifted once and for all."
        };
        metarun_present_story_texts("Nienna, Lady of Pity", fallback,
            N_ELEMENTS(fallback), TERM_L_BLUE, TERM_WHITE, "Continue");
    }

    quest_set_state(QUEST_ID_NIENA_PACIFIST, QUEST_STATE_REWARDED);
    p_ptr->niena_reserved &= ~NIENA_FLAG_PACIFIST_FAILED;
    metarun_mark_quest_completed(METARUN_QUEST_NIENA_PACIFIST);
    metarun_add_niena_curse_cleansing_charge();
    msg_print("Nienna grants you a single cleansing; use the quest menu to cast off every curse.");
}

/* ------------------------------------------------------------------
 * metarun_update_on_exit() â€“ v5, 30â€¯Julâ€¯2025
 * ------------------------------------------------------------------
 * Implements the finalised story/logic flow discussed in chat:
 *   0.  Escape check (silmarils? giftâ€‘ofâ€‘Eru?)
 *   1.  Escapeâ€‘curse chooser UI
 *   2.  Victory banner & Silmaril paragraph
 *   3.  Temptation of Treachery (3 rolls â€“ stolen Silmarils don't count)
 *   4.  Story Fragment (pure vs tainted, 1â€‘3 jewels)
 *   5.  Echoes of Kinslaying / "Kill a Kin" (stop at first kill)
 *   6.  Final pause â†’ apply deferred effects
 *   7.  Persist silmaril/death counters, check run end, save
 *
 *  All narrative helpers (print_heading(), print_paragraph(),
 *  choose_escape_curses_ui(), kinslayer_try_kill(), etc.) are reused.
 * ------------------------------------------------------------------ */
static void announce_blessing_gain(int previous_points)
{
    int current_points = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    if (current_points <= previous_points) return;
    int delta = current_points - previous_points;
    int available = current_points - metar.blessing_points_spent;
    if (available < 0) available = 0;
    msg_format("You gain %d blessing point%s. (%d available)",
               delta, (delta == 1) ? "" : "s", available);
    message_flush();
}

static bool metarun_ui_show_story_modal_auto(const char* title,
    byte title_attr, const char* const* paragraphs, const byte* attrs,
    int paragraph_count, bool steamdeck, const char* accept_label,
    const char* action_label)
{
    ui_information_scene_scope info_scope;

    if (!ui_information_scene_acquire(&info_scope))
    {
        log_error("metarun: semantic story modal unavailable for '%s'",
            title ? title : "(untitled)");
        return false;
    }

    if (!metarun_ui_show_story_modal(title, title_attr, paragraphs, attrs,
            paragraph_count, steamdeck, accept_label, action_label))
    {
        log_error("metarun: failed to present story modal '%s'",
            title ? title : "(untitled)");
        ui_information_scene_leave(&info_scope);
        return false;
    }

    ui_information_scene_leave(&info_scope);
    return true;
}

static int metarun_collect_story_indices(int* indices, int capacity)
{
    int total = 0;
    int max_st;
    byte rt = metar.type;
    int sils = metar.silmarils;

    if (!indices || capacity <= 0 || !z_info || !st_info)
        return 0;

    max_st = z_info->st_max;
    if (max_st > capacity)
        max_st = capacity;

    for (int i = 0; i < max_st; i++)
    {
        story_type* st = &st_info[i];

        if (!st->name && !st->text)
            continue;
        if (st->st_type != 0)
            continue;
        if (!(st->runtypes == 0
            || (rt < 32 && (st->runtypes & (1UL << rt)))))
        {
            continue;
        }
        if (st->order <= (byte)sils)
            indices[total++] = i;
    }

    for (int i = 1; i < total; i++)
    {
        int key = indices[i];
        byte key_ord = st_info[key].order;
        int j = i - 1;

        while (j >= 0 && st_info[indices[j]].order > key_ord)
        {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }

    return total;
}

static void metarun_show_recent_story_parts_semantic(int last_parts)
{
    int selected[1024];
    int total;
    int start;
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";

    total = metarun_collect_story_indices(selected, N_ELEMENTS(selected));
    if (total <= 0)
    {
        log_debug("metarun: no story parts available for semantic recap");
        return;
    }

    if (steamdeck)
    {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }

    start = (last_parts > 0 && last_parts < total) ? total - last_parts : 0;
    for (int idx = start; idx < total; idx++)
    {
        story_type* st = &st_info[selected[idx]];
        const char* story_title = st->name ? st_name + st->name : NULL;
        const char* story_text = st->text ? st_text + st->text : NULL;
        const char* paragraphs[2];
        byte attrs[2];
        int paragraph_count = 0;
        int page = idx - start + 1;
        int page_total = total - start;
        char title[APP_UI_TEXT_MAX];

        if (!story_text || !story_text[0])
            continue;

        if (page_total > 1)
        {
            strnfmt(title, sizeof(title), "The Tale So Far (%d/%d)", page,
                page_total);
        }
        else
        {
            SDL_strlcpy(title, "The Tale So Far", sizeof(title));
        }

        if (story_title && story_title[0])
        {
            paragraphs[paragraph_count] = story_title;
            attrs[paragraph_count++] = TERM_L_BLUE;
        }
        paragraphs[paragraph_count] = story_text;
        attrs[paragraph_count++] = TERM_WHITE;

        if (!metarun_ui_show_story_modal_auto(title, TERM_YELLOW,
                paragraphs, attrs, paragraph_count, steamdeck, accept_label,
                metarun_prompt_action_label(PROMPT_CONTINUE_TALE)))
        {
            log_warn("metarun: failed to present semantic story recap part %d/%d",
                page, page_total);
            break;
        }
    }
}

static byte metarun_play_escape_victory_semantic_sequence(int sil_count,
    bool allow_treachery, bool allow_kinslay)
{
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    int curse_count = sil_count;
    int chosen[4] = { -1, -1, -1, -1 };
    int chosen_cnt;
    byte stolen = 0;
    byte final_sils;
    bool treachery_occurred;
    bool deferred_kill[3] = { false, false, false };
    int kinslaying_victims = 0;

    if (steamdeck) {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }

    if (sil_count == 3)
        curse_count = 4;
    chosen_cnt = choose_escape_curses_ui(curse_count, chosen);
    if (chosen_cnt < 0)
    {
        log_error("metarun: escape curse selection failed");
        chosen_cnt = 0;
    }

    if (chosen_cnt > 0)
    {
        char curse_lines[4][128];
        const char* paragraphs[4];
        byte attrs[4];

        for (int i = 0; i < chosen_cnt; i++)
        {
            strnfmt(curse_lines[i], sizeof(curse_lines[i]),
                "The curse of %s binds your fate.",
                curse_display_name(chosen[i]));
            paragraphs[i] = curse_lines[i];
            attrs[i] = TERM_RED;
        }

        metarun_ui_show_story_modal_auto("The Binding of Fate",
            TERM_L_RED, paragraphs, attrs, chosen_cnt, steamdeck,
            accept_label, metarun_prompt_action_label(PROMPT_CONTINUE_TALE));
    }

    {
        const char* victory_text;
        const char* paragraphs[1];
        byte attrs[1] = { TERM_WHITE };

        switch (sil_count)
        {
            case 1:
                victory_text = "You emerge victorious from darkness, one holy jewel blazing in your grasp. Morgoth's crown is diminished, yet hope is rekindled, though shadow lingers.";
                break;
            case 2:
                victory_text = "You escape triumphant, two Silmarils blazing fiercely in your hands. Morgoth roars in wrath; his pride is wounded deeply. Your spirit exults, yet your heart begins to feel their burning weight.";
                break;
            case 3:
                victory_text = "All three stolen stars blaze now in your hands; Morgoth's crown lies darkened. Such triumph has not been known since Feanor himself dreamed it-but even as victory soars, your heart trembles beneath their burning glory.";
                break;
            default:
                victory_text = "You have achieved the impossible, claiming more Silmarils than should exist. Reality itself bends before your triumph.";
                break;
        }

        paragraphs[0] = victory_text;
        metarun_ui_show_story_modal_auto("Victory Amid Shadow",
            TERM_YELLOW, paragraphs, attrs, 1, steamdeck, accept_label,
            metarun_prompt_action_label(allow_treachery
                ? PROMPT_FACE_TEMPTATION
                : PROMPT_CONTINUE_GENERIC));
    }

    if (allow_treachery)
    {
        static const int pct[3] = { 20, 50, 95 };
        static const char *success_msgs[3] = {
            "The first jewel shines brightly, its pure light uncorrupted. You master desire, choosing honor.",
            "The second jewel blazes defiant, temptation growing strong-but once more, you cling to honor.",
            "The third Silmaril's holy flame burns fiercely. Yet against all odds, your will resists corruption."
        };
        static const char *failure_msgs[3] = {
            "Greed whispers softly, and you listen. Secretly you withhold the jewel's light, betraying even yourself.",
            "Desire gnaws deeper; you falter, concealing its brilliance in shame, light darkened by your betrayal.",
            "Consumed by lust for its beauty, you claim it secretly, sealing its radiance from all others-a betrayal of all trust."
        };
        const char* shadow_text =
            "In shadows your deeds are recorded-tainted victory shall diminish the jewel's blessing.";
        const char* paragraphs[4];
        byte attrs[4];
        int paragraph_count = 0;

        for (int i = 0; i < sil_count; ++i)
        {
            bool fail = (rand_int(100) < pct[i]);

            if (fail)
                stolen++;
            paragraphs[paragraph_count] = fail ? failure_msgs[i]
                                               : success_msgs[i];
            attrs[paragraph_count++] = fail ? TERM_RED : TERM_WHITE;
        }

        if (stolen)
        {
            paragraphs[paragraph_count] = shadow_text;
            attrs[paragraph_count++] = TERM_L_DARK;
        }

        metarun_ui_show_story_modal_auto("Temptation of Treachery",
            TERM_L_UMBER, paragraphs, attrs, paragraph_count, steamdeck,
            accept_label, metarun_prompt_action_label(PROMPT_CONTINUE_GENERIC));
    }

    final_sils = sil_count - stolen;
    treachery_occurred = (stolen > 0);

    {
        const char* pure_frag[3] = {
            "A single star reclaimed, hope rekindled faintly in Middle-earth. Yet Morgoth laughs still, for two remain bound in shadow.",
            "Two jewels shine again beneath sky; Morgoth's power falters greatly. Yet you feel their brilliance burning; temptation ever near.",
            "All three jewels, radiant and pure, blaze again beneath stars. Morgoth's power breaks. Triumph is absolute, your soul soaring."
        };
        const char* tainted_frag[3] = {
            "Though victory is yours, its memory darkens. Trust is fragile, and your spirit heavy beneath secret betrayal.",
            "Your heart trembles: Morgoth sees clearly your treachery-he smiles grimly, knowing darkness still dwells in you.",
            "Greatest triumph now mingled with darkest shame. Morgoth's laughter echoes bitterly-he senses your fall."
        };
        const char* paragraphs[1];
        byte attrs[1];

        paragraphs[0] = treachery_occurred
            ? tainted_frag[sil_count - 1]
            : pure_frag[final_sils - 1];
        attrs[0] = treachery_occurred ? TERM_RED : TERM_L_WHITE;
        metarun_ui_show_story_modal_auto("The Weight of Victory",
            TERM_L_BLUE, paragraphs, attrs, 1, steamdeck, accept_label,
            metarun_prompt_action_label(allow_kinslay
                ? PROMPT_FACE_ECHOES
                : PROMPT_CONCLUDE_TALE));
    }

    if (allow_kinslay)
    {
        static const int kin_pct[3] = { 20, 50, 95 };
        const char* doom_text =
            "Blood now stains your triumph, your fate forever woven with grief and shame.";
        const char* paragraphs[4];
        byte attrs[4];
        int paragraph_count = 0;

        for (int k = 0; k < sil_count; ++k)
        {
            bool fail = (rand_int(100) < kin_pct[k]);
            const char* echo_text = NULL;

            deferred_kill[k] = fail;
            if (fail)
                kinslaying_victims++;

            switch (k)
            {
                case 0:
                    echo_text = fail ?
                        "\"Alqualonde's Grief\"\nBlood stains starlit waves. Your hand remembers the swords at Alqualonde-first grief, first guilt." :
                        "The sorrow of Alqualonde passes over you-your spirit holds fast, blood unstained.";
                    break;
                case 1:
                    echo_text = fail ?
                        "\"Ruin of Doriath\"\nAgain your hand recalls tragedy-fallen halls of Menegroth, Dior's blood shed beneath stolen starlight." :
                        "Memory of Doriath rises briefly, but your blade remains clean, honour upheld.";
                    break;
                case 2:
                    echo_text = fail ?
                        "\"Tragedy at Sirion\"\nEchoes rise from Sirion-Elwing's flight, blood and betrayal. Once more your blade draws innocent blood, sealing doom anew." :
                        "You resist dark whispers recalling Sirion-your sword is stayed, mercy unbroken.";
                    break;
            }

            paragraphs[paragraph_count] = echo_text;
            attrs[paragraph_count++] = fail ? TERM_RED : TERM_L_WHITE;

            if (fail)
                break;
        }

        if (kinslaying_victims > 0)
        {
            paragraphs[paragraph_count] = doom_text;
            attrs[paragraph_count++] = TERM_L_DARK;
        }

        metarun_ui_show_story_modal_auto("Echoes of Kinslaying",
            TERM_L_RED, paragraphs, attrs, paragraph_count, steamdeck,
            accept_label, metarun_prompt_action_label(PROMPT_CONCLUDE_TALE));
    }

    {
        const char* paragraphs[1];
        byte attrs[1] = { TERM_L_GREEN };
        char summary[256];

        strnfmt(summary, sizeof(summary),
            "Your legend is written: %d Silmaril%s claimed, %s, %s.",
            final_sils, (final_sils == 1) ? "" : "s",
            treachery_occurred ? "tainted by treachery" : "pure of heart",
            (kinslaying_victims > 0) ? "stained by kinslaying"
                                     : "with honour intact");
        paragraphs[0] = summary;
        metarun_ui_show_story_modal_auto("The Tale Concludes",
            TERM_YELLOW, paragraphs, attrs, 1, steamdeck, accept_label,
            metarun_prompt_action_label((allow_kinslay && kinslaying_victims > 0)
                ? PROMPT_CONTINUE_GENERIC
                : PROMPT_RETURN_MIDDLE_EARTH));
    }

    if (allow_kinslay && kinslaying_victims > 0)
    {
        const char* paragraphs[1];
        byte attrs[1] = { TERM_RED };
        char kill_msg[128];

        strnfmt(kill_msg, sizeof(kill_msg),
            "Your kinslaying echoes through time. %d innocent%s will fall by your hand...",
            kinslaying_victims, (kinslaying_victims == 1) ? "" : "s");
        paragraphs[0] = kill_msg;
        metarun_ui_show_story_modal_auto("The Price of Blood",
            TERM_RED, paragraphs, attrs, 1, steamdeck, accept_label,
            metarun_prompt_action_label(PROMPT_WITNESS_CONSEQUENCES));

        {
            char kill_lines[3][96];
            const char* kill_paragraphs[3];
            byte kill_attrs[3];
            int kill_count = 0;

            for (int k = 0; k < 3; k++)
            {
                const char* character;

                if (!deferred_kill[k])
                    continue;

                character = kinslayer_try_kill(k + 1, false);
                if (!character)
                    continue;

                metarun_increment_deaths();
                log_info("Metarun: kinslaying victim counted as death (%u total)",
                    (unsigned)metar.deaths);

                strnfmt(kill_lines[kill_count], sizeof(kill_lines[kill_count]),
                    "A hero %s has fallen beneath your blade.", character);
                kill_paragraphs[kill_count] = kill_lines[kill_count];
                kill_attrs[kill_count++] = TERM_RED;
            }

            if (kill_count > 0)
            {
                metarun_ui_show_story_modal_auto("Blood Is Demanded",
                    TERM_RED, kill_paragraphs, kill_attrs, kill_count,
                    steamdeck, accept_label,
                    metarun_prompt_action_label(PROMPT_RETURN_MIDDLE_EARTH));
            }
        }
    }

    return final_sils;
}

void metarun_update_on_exit(bool died, bool escaped, byte sil_count, s32b final_score)
{
    if (run_mode_is_blitz())
    {
        log_info("Suppressing metarun end-of-run processing for Blitz");
        if (escaped || (p_ptr && p_ptr->morgoth_slain && !died))
        {
            byte summary_sils = sil_count;
            if (p_ptr && p_ptr->morgoth_slain && summary_sils < 3)
                summary_sils = 3;
            blitz_show_end_summary(summary_sils);
        }
        return;
    }

    log_info("Metarun update: died=%s, escaped=%s, sil_count=%d, final_score=%ld", 
             died ? "true" : "false", escaped ? "true" : "false", sil_count, (long)final_score);
    int blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    if (escaped)
    {
        sdl_music_play_main();
    }
    bool challenge_disconnected = (op_ptr && op_ptr->opt[OPT_adult_discon_stair]);
    bool challenge_single_stair = (op_ptr && op_ptr->opt[OPT_adult_single_stair]);
    bool challenge_fixed_exp = (op_ptr && op_ptr->opt[OPT_birth_fixed_exp] && metarun_challenge_fixed_exp_unlocked());
    bool challenge_tulkas_blunt = (op_ptr && op_ptr->opt[OPT_adult_tulkas_blunt]);
    bool challenge_torchlight = (op_ptr && op_ptr->opt[OPT_adult_torchlight] && metarun_challenge_torchlight_unlocked());
    int fixed_challenge_before = metarun_challenge_completion_count(CHALLENGE_FIXED_50K_XP);
    int blunt_challenge_before = metarun_challenge_completion_count(CHALLENGE_TULKAS_BLUNT);
    int torchlight_challenge_before = metarun_challenge_completion_count(CHALLENGE_TORCHLIGHT);
    bool challenge_disconnected_success = false;
             
    /* -------- Lineage flags -------------------------------------- */
    u32b character_flags = c_info[p_ptr->pcharacter].flags;
    u32b f_race  = p_info[p_ptr->prace].flags;

    bool has_gift_eru   = (character_flags | f_race) & RHF_GIFTERU;
    bool allow_treachery = (character_flags | f_race) & RHF_TREACHERY;
    bool allow_kinslay   = (character_flags | f_race) & RHF_KINSLAYER;

    bool escaped_with_sils = escaped && (sil_count > 0);
    bool morgoth_victory = (p_ptr->morgoth_slain && !escaped && !died);

    resolve_niena_morgoth_quest_on_exit(escaped_with_sils);
    resolve_nienia_pacifist_quest_on_exit(died, escaped);

    /* Treat as a death unless Eru intervenes */
    if (died && !has_gift_eru)
        metarun_increment_deaths();

    /* ------------------------------------------------------------- */
    /* 0. Branch: did we return with Silmarils?                      */
    /*    â€“ any path that reaches here counts as a "run end" event  */
    /* ------------------------------------------------------------- */
    if (morgoth_victory)
    {
        bool steamdeck = steamdeck_controls_active();
        char accept_label[16] = "";
        const char* paragraphs[] = {
            "The illusion of Morgoth lies shattered at your feet.",
            "From Valinor, the Valar proclaim your impossible triumph and pour out their blessing.",
            "Though the true Dark Enemy waits beyond this trial, three Silmarils are counted to your name."
        };
        const byte attrs[] = { TERM_WHITE, TERM_L_BLUE, TERM_L_BLUE };

        log_info("Metarun: Morgoth victory branch (sil_count=%d)", sil_count);
        if (steamdeck) {
            metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
                sizeof(accept_label));
        }
        (void)metarun_ui_show_story_modal_auto("Beyond Fate",
            TERM_YELLOW, paragraphs, attrs, (int)N_ELEMENTS(paragraphs),
            steamdeck, accept_label, "Continue");

        if (challenge_disconnected) {
            metarun_mark_challenge_completed(CHALLENGE_DISCONNECTED);
            challenge_disconnected_success = true;
        }
        if (challenge_single_stair) {
            metarun_mark_challenge_completed(CHALLENGE_SINGLE_STAIR);
        }
        if (challenge_fixed_exp) {
            metarun_mark_challenge_completed(CHALLENGE_FIXED_50K_XP);
        }
        if (challenge_tulkas_blunt) {
            metarun_mark_challenge_completed(CHALLENGE_TULKAS_BLUNT);
        }
        if (challenge_torchlight) {
            metarun_mark_challenge_completed(CHALLENGE_TORCHLIGHT);
        }

        byte awarded = (sil_count < 3) ? 3 : sil_count;
        metarun_gain_silmarils(awarded);
        log_info("Metarun: Morgoth victory awarded %d Silmarils (total now %d)",
                 awarded, (int)metar.silmarils);
        refresh_current_metar_score();
        compute_blessing_pool();
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        if (challenge_disconnected_success &&
            quest_get_state(QUEST_ID_MANDOS_BETRAYER) < QUEST_STATE_REWARDED &&
            metarun_quest_completion_count(METARUN_QUEST_MANDOS_BETRAYER) < quest_completion_cap(QUEST_ID_MANDOS_BETRAYER)) {
            show_mandos_third_unlock_message();
        }
        maybe_unlock_orome_great_hunt(challenge_single_stair);
        maybe_unlock_niena_mercy_purchase(challenge_fixed_exp, fixed_challenge_before);
        if (challenge_tulkas_blunt &&
            metarun_challenge_completion_count(CHALLENGE_TULKAS_BLUNT) > blunt_challenge_before) {
            show_tulkas_blunt_unlock_message();
        }
        if (challenge_torchlight) {
            maybe_unlock_queen_of_stars_purchase(challenge_torchlight, torchlight_challenge_before);
        }
        check_run_end();
        metarun_save_persistent_settings();
        save_metaruns();
        return;
    }
    else if (died)
    {
        bool steamdeck = steamdeck_controls_active();
        char accept_label[16] = "";

        log_info("Player died - displaying death narrative");
        /*****  NEW DEATH-NARRATIVE *****/
        if (steamdeck) {
            metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
                sizeof(accept_label));
        }

        /* Pick correct sequence number: 0 when Gift-of-Eru fires,
         * otherwise 1-based death counter that was just incremented. */
        byte target_order = has_gift_eru ? 0 : metar.deaths;

        /* Build a pool of candidate story entries.                    */
        int *pool = mem_alloc_array(z_info->st_max, int);
        int pool_sz = 0;
        if (!pool) {
            u32b pool_before = metar.fallen_score_total;
            refresh_current_metar_score();
            compute_blessing_pool();
            if (final_score > 0 && metar.fallen_score_total == pool_before) {
                metar.fallen_score_total += (u32b)final_score;
                update_blessing_ledger(&metar);
                (void)sync_current_metarun_slot(false);
            }
            announce_blessing_gain(blessing_points_before);
            blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
            check_run_end();
            save_metaruns();
            return;
        }
        for (int i = 0; i < z_info->st_max && pool; i++) {
            story_type *st = &st_info[i];
            if (!st->name)            continue;                /* unused slot   */
            if (st->st_type != 1)     continue;                /* not â€œdeathâ€   */
            if (st->order != target_order) continue;           /* wrong order   */
            if (st->runtypes &&
               !(st->runtypes & (1u << metar.type))) continue; /* wrong run-type*/
            pool[pool_sz++] = i;
        }

        /* Fallback â€“ allow any order-0 message if nothing matched.   */
        if (!pool_sz && target_order) {
            for (int i = 0; i < z_info->st_max && pool; i++) {
                story_type *st = &st_info[i];
                if (!st->name || st->st_type != 1) continue;
                if (st->order != 0)   continue;
                if (st->runtypes &&
                   !(st->runtypes & (1u << metar.type))) continue;
                pool[pool_sz++] = i;
            }
        }

        /* Display the chosen fragment with the usual fade-in style.  */
        if (pool_sz) {
            story_type *pick = &st_info[ pool[rng_int(pool_sz)] ];
            cptr title = st_name + pick->name;
            cptr text  = st_text + pick->text;
            char transition_text[256];

            strnfmt(transition_text, sizeof(transition_text),
                    "The hero whose mantle you took has fallen, their tale ends in shadow. "
                    "Yet your spirit returns, for the Valar's trial is not yet complete.");

            {
                const char* paragraphs[] = { text, transition_text };
                const byte attrs[] = { TERM_WHITE, TERM_L_BLUE };

                (void)metarun_ui_show_story_modal_auto(title, TERM_RED,
                    paragraphs, attrs, 2, steamdeck, accept_label, "Return");
            }
        }

        pool = mem_free(pool);
        u32b pool_before = metar.fallen_score_total;
        refresh_current_metar_score();
        compute_blessing_pool();
        if (final_score > 0 && metar.fallen_score_total == pool_before) {
            metar.fallen_score_total += (u32b)final_score;
            update_blessing_ledger(&metar);
            (void)sync_current_metarun_slot(false);
        }
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        check_run_end();
        save_metaruns();
        return;
    }
    else if (!escaped_with_sils) {
        log_debug("Player escaped without Silmarils - no narrative needed");
        refresh_current_metar_score();
        compute_blessing_pool();
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        save_metaruns();
        return;                        /* no further narrative needed  */
    }

    /* ------------------------------------------------------------- */
    /*        Enhanced Narrative Path â€“ escaped with â‰¥1 Silmaril     */
    /* ------------------------------------------------------------- */
    log_info("Player escaped with %d Silmarils - displaying victory narrative",
        sil_count);
    byte final_sils = metarun_play_escape_victory_semantic_sequence(
        sil_count, allow_treachery, allow_kinslay);

    metarun_gain_silmarils(final_sils);
    log_info("Added %d Silmarils to metarun total (now %d)", final_sils,
        metar.silmarils);
    refresh_current_metar_score();
    metarun_show_recent_story_parts_semantic(3);

    if (challenge_disconnected) {
        metarun_mark_challenge_completed(CHALLENGE_DISCONNECTED);
        challenge_disconnected_success = true;
    }
    if (challenge_single_stair) {
        metarun_mark_challenge_completed(CHALLENGE_SINGLE_STAIR);
    }
    if (challenge_fixed_exp) {
        metarun_mark_challenge_completed(CHALLENGE_FIXED_50K_XP);
    }
    if (challenge_tulkas_blunt) {
        metarun_mark_challenge_completed(CHALLENGE_TULKAS_BLUNT);
    }
    if (challenge_torchlight) {
        metarun_mark_challenge_completed(CHALLENGE_TORCHLIGHT);
    }
    if (challenge_disconnected_success &&
        quest_get_state(QUEST_ID_MANDOS_BETRAYER) < QUEST_STATE_REWARDED &&
        metarun_quest_completion_count(METARUN_QUEST_MANDOS_BETRAYER) < quest_completion_cap(QUEST_ID_MANDOS_BETRAYER)) {
        show_mandos_third_unlock_message();
    }
    maybe_unlock_orome_great_hunt(challenge_single_stair);
    maybe_unlock_niena_mercy_purchase(challenge_fixed_exp, fixed_challenge_before);
    if (challenge_tulkas_blunt &&
        metarun_challenge_completion_count(CHALLENGE_TULKAS_BLUNT) > blunt_challenge_before) {
        show_tulkas_blunt_unlock_message();
    }
    if (challenge_torchlight) {
        maybe_unlock_queen_of_stars_purchase(challenge_torchlight, torchlight_challenge_before);
    }

    compute_blessing_pool();
    announce_blessing_gain(blessing_points_before);
    blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    check_run_end();
    /* Save persistent settings when exiting */
    metarun_save_persistent_settings();
    
    /* Save metarun data (deaths, silmarils, etc.) */
    save_metaruns();
}


static int required_survivor_target(int win_goal)
{
    int remaining_silmarils = win_goal - metar.silmarils;
    if (remaining_silmarils < 0) remaining_silmarils = 0;

    int required = 0;
    if (remaining_silmarils > 0) {
        required = (remaining_silmarils + 2) / 3;
        required += CURSE_GET(CUR_DEATH);
        if (required < 1) required = 1;
    }

    if (required < 0) required = 0;
    return required;
}


/* ======================  run-state logic  ====================== */
/* ------------------------------------------------------------------ *
 *  Decide whether the current run just ended, and react accordingly. *
 *  Message text adapts automatically if you set LOSECON_DEATHS = 1.  *
 *  Loss condition takes precedence over win condition.               *
 * ------------------------------------------------------------------ */
void check_run_end(void)
{
    int win_goal = WINCON_SILMARILS;   /* fallback */

    if (runtype_info && metar.type < z_info->rt_max)
    {
        win_goal = runtype_info[metar.type].win_con ? runtype_info[metar.type].win_con : WINCON_SILMARILS;
    }

    /* Keep blessing and survivor data aligned with the score file */
    compute_blessing_pool();
    int alive = metar.alive_characters;

    int remaining_silmarils = win_goal - metar.silmarils;
    if (remaining_silmarils < 0) remaining_silmarils = 0;

    int required_survivors = required_survivor_target(win_goal);

    /* Loss takes precedence over victory */
    if (alive < required_survivors) {
        bool steamdeck = steamdeck_controls_active();
        char accept_label[16] = "";
        char defeat_text[256];

        log_info("Metarun DEFEAT: alive=%d required=%d (remaining silmarils=%d)",
                 alive, required_survivors, remaining_silmarils);
        strnfmt(defeat_text, sizeof defeat_text,
                "Only %d hero%s remain, yet %d must endure to reclaim the remaining Silmarils. "
                "This tale falls into shadow; begin anew to kindle hope once more.",
                alive, (alive == 1) ? "" : "es",
                required_survivors);
        if (steamdeck) {
            metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
                sizeof(accept_label));
        }
        {
            const char* paragraphs[] = { defeat_text };
            const byte attrs[] = { TERM_WHITE };

            (void)metarun_ui_show_story_modal_auto("The Trial's End",
                TERM_RED, paragraphs, attrs, 1, steamdeck, accept_label,
                "Begin Anew");
        }

        start_new_metarun();
        return;
    }

    if (metar.silmarils >= win_goal) {
        bool steamdeck = steamdeck_controls_active();
        char accept_label[16] = "";
        char victory_text[256];
        const char *implementation_note = "(This final trial is yet to be implemented.)";

        log_info("Metarun VICTORY: %d Silmarils collected (goal: %d)", metar.silmarils, win_goal);
        strnfmt(victory_text, sizeof victory_text,
                "%d Silmarils reclaimed from Morgoth's crown! "
                "Hope kindles anew; your long trial approaches its end. "
                "Yet one final ordeal awaits: your ultimate destiny, "
                "as your true self faces the Last Trial.",
                win_goal);
        if (steamdeck) {
            metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
                sizeof(accept_label));
        }
        {
            const char* paragraphs[] = { victory_text, implementation_note };
            const byte attrs[] = { TERM_L_GREEN, TERM_L_DARK };

            (void)metarun_ui_show_story_modal_auto("The Trial's End",
                TERM_YELLOW, paragraphs, attrs, 2, steamdeck, accept_label,
                "Begin Anew");
        }

        start_new_metarun();
    }
}





/* ------------------------------------------------------------------
 *  Start a brand-new meta-run.
 *  We snapshot the finished run **after** the array has been grown,
 *  so we only write once and always with the final pointer.
 * ------------------------------------------------------------------ */
static void start_new_metarun(void)
{
    log_info("Starting new metarun (previous run ID: %d)", metar.id);
    log_debug("metarun: pre-finalize state (wizard=%d, noscore=0x%04X, savefile='%s')",
              p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
              p_ptr ? (unsigned)p_ptr->noscore : 0,
              savefile);

    u32b previous_id = metar.id;
    if (!sync_current_metarun_slot(true)) {
        log_warn("metarun: unable to snapshot current run before rollover (idx=%d, max=%d)",
                 current_run, metarun_max);
    }

     /* Before wiping scores for the next run, backup and clear save files */
     backup_and_clear_saves();
     
     /* Before wiping scores for the next run, finalize current ones:
         - mark all alive entries as dead by their own hand
         - save any corresponding savefiles as dead
         Then archive/clear the score file so the next run starts clean. */
     metarun_finalize_scores_and_saves();
     clear_scorefile();

    /* Hard purge the current savefile if this was a noscore wizard/debug run */
    if (p_ptr && (p_ptr->wizard || (p_ptr->noscore & 0x0008)) && (p_ptr->noscore & 0x000F)) {
        if (savefile[0]) {
            bool deleted;
            safe_setuid_grab();
            deleted = fd_kill(savefile);
            safe_setuid_drop();
            if (deleted) {
                log_info("metarun: deleted noscore savefile '%s'", savefile);
            } else {
                log_warn("metarun: failed to delete noscore savefile '%s'", savefile);
            }
        }
    } else {
        log_info("metarun: purge skipped (wizard=%d, noscore=0x%04X, savefile='%s')",
                 p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
                 p_ptr ? (unsigned)p_ptr->noscore : 0,
                 savefile);
    }
    /* Save old state */
    s16b old_max   = metarun_max;
    metarun *old   = metaruns;

    /* Try to allocate a new array for one more run */
    metarun *tmp = mem_alloc_array(old_max + 1, metarun);
    if (!tmp) {
        /* Allocation failed - keep everything as is */
        return;
    }

    /* Copy over the previous runs (if any) */
    if (old) {
        memcpy(tmp, old, sizeof(metarun) * old_max);
    }

    /* Free the old array just once */
    old = mem_free(old);

    /* Commit the new array and size */
    metaruns    = tmp;
    metarun_max = old_max + 1;

    /* Initialize the brand-new slot */
    reset_defaults(&metaruns[metarun_max - 1]);
    metaruns[metarun_max - 1].id = previous_id + 1;
    metaruns[metarun_max - 1].type = 0; /* Default to type 0 (Normal) for new metaruns */

    /* Update globals */
    current_run      = metarun_max - 1;
    metar             = metaruns[current_run];
    metarun_created  = true;  /* Set flag to show story intro for new metarun */

    /* Apply difficulty curses based on the runtype */
    apply_difficulty_curses(&metar);

    /* Persist and prepare */
    save_metaruns();      /* safe now that metarunsâ‰ NULL */ 
    ensure_run_dir(&metar);
    log_info("New metarun %d created and initialized", metar.id);
}

static bool metarun_show_active_effects_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label)
{
    int active_count = 0;
    int active_ids[64];
    int selected = 0;

    for (int id = 0; id < z_info->cu_max && active_count < 64; id++)
    {
        if (CURSE_GET(id) != 0)
            active_ids[active_count++] = id;
    }

    if (active_count == 0)
    {
        const char* lines[] = {
            "No active curses or blessings remain on this saga."
        };
        const byte attrs[] = { TERM_L_DARK };

        return metarun_ui_show_notice_modal("All Active Effects", TERM_YELLOW,
            lines, attrs, (int)N_ELEMENTS(lines), steamdeck, accept_label);
    }

    while (true)
    {
        app_ui_scene scene;
        app_ui_panel* panel;
        char subtitle[APP_UI_TEXT_MAX];
        int key;

        strnfmt(subtitle, sizeof(subtitle), "%d active effect%s",
            active_count, (active_count == 1) ? "" : "s");
        panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
            "All Active Effects", TERM_SLATE, subtitle);
        if (!panel)
            return false;

        for (int i = 0; i < active_count; i++)
        {
            if (!metarun_ui_add_effect_row_ex(panel, active_ids[i],
                    i == selected))
            {
                return false;
            }
        }

        if (!metarun_ui_add_effect_detail_lines(panel, active_ids[selected]))
            return false;

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            steamdeck ? accept_label : "Any", "Close");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "8/2", "Move");
        if (steamdeck)
        {
            (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                back_label, "Back");
        }

        if (!ui_information_scene_present_ui(&scene))
            return false;

        key = ui_information_scene_wait_key_nonrepeat();
        if (key == '8' || key == 'k' || key == '-')
        {
            selected = (selected + active_count - 1) % active_count;
            continue;
        }
        if (key == '2' || key == 'j' || key == '+')
        {
            selected = (selected + 1) % active_count;
            continue;
        }
        if (steamdeck)
        {
            if (key == steamdeck_back_key()
                || key == steamdeck_confirm_key()
                || key == '\r' || key == '\n' || key == ESCAPE)
            {
                metarun_ui_clear_pending_input();
                return true;
            }
            continue;
        }

        metarun_ui_clear_pending_input();
        return true;
    }
}

static bool metarun_show_known_curses_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label)
{
    int known_ids[METAR_CURSE_SLOTS];
    int known_count = 0;
    int selected = 0;
    int row_offset = 0;
    int limit;

    if (!z_info || !cu_info)
        return false;

    limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
    for (int id = 0; id < limit; id++)
    {
        if (!cu_info[id].name || !CURSE_SEEN(id))
            continue;
        known_ids[known_count++] = id;
    }

    if (known_count == 0)
    {
        const char* lines[] = {
            "No curse lore has been uncovered in this story run yet."
        };
        const byte attrs[] = { TERM_L_DARK };

        return metarun_ui_show_notice_modal("Known Curses", TERM_L_RED,
            lines, attrs, (int)N_ELEMENTS(lines), steamdeck, accept_label);
    }

    while (true)
    {
        app_ui_scene scene;
        app_ui_panel* panel;
        char subtitle[APP_UI_TEXT_MAX];
        int key;

        if (selected < 0)
            selected = 0;
        if (selected >= known_count)
            selected = known_count - 1;
        if (row_offset > selected)
            row_offset = selected;
        if (selected >= row_offset + METARUN_KNOWN_CURSE_PAGE_SIZE)
            row_offset = selected - METARUN_KNOWN_CURSE_PAGE_SIZE + 1;
        if (row_offset < 0)
            row_offset = 0;

        strnfmt(subtitle, sizeof(subtitle), "%d known curse%s",
            known_count, (known_count == 1) ? "" : "s");
        panel = metarun_ui_begin_browser_scene(&scene, TERM_L_RED,
            "Known Curses", TERM_SLATE, subtitle);
        if (!panel)
            return false;

        for (int i = 0; i < known_count; i++)
        {
            const char* blessing_name = blessing_display_name(known_ids[i]);
            const char* curse_name = curse_display_name(known_ids[i]);
            char meta[APP_UI_META_MAX];
            byte meta_attr = TERM_SLATE;

            meta[0] = '\0';
            if (blessing_name && blessing_name[0]
                && strcmp(blessing_name, curse_name) != 0)
            {
                SDL_strlcpy(meta, blessing_name, sizeof(meta));
                meta_attr = TERM_L_GREEN;
            }

            if (!app_ui_panel_add_row_ex(panel, known_ids[i], TERM_L_RED,
                    meta_attr, 0, '\0', true, i == selected, "", curse_name,
                    meta))
            {
                return false;
            }
        }

        app_ui_panel_set_row_offset(panel, (s16b)row_offset);
        if (!metarun_ui_add_known_curse_detail_lines(panel, known_ids[selected]))
            return false;

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            steamdeck ? accept_label : "Enter", "Close");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "8/2", "Move");
        if (known_count > METARUN_KNOWN_CURSE_PAGE_SIZE)
        {
            (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "4/6", "Page");
        }
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            steamdeck ? back_label : "Esc", "Back");

        if (!ui_information_scene_present_ui(&scene))
            return false;

        key = ui_information_scene_wait_key_nonrepeat();
        if (key == '8' || key == 'k' || key == '-')
        {
            selected = (selected + known_count - 1) % known_count;
            continue;
        }
        if (key == '2' || key == 'j' || key == '+')
        {
            selected = (selected + 1) % known_count;
            continue;
        }
        if (known_count > METARUN_KNOWN_CURSE_PAGE_SIZE
            && (key == '4' || key == 'h' || key == 'H'))
        {
            selected -= METARUN_KNOWN_CURSE_PAGE_SIZE;
            if (selected < 0)
                selected = 0;
            continue;
        }
        if (known_count > METARUN_KNOWN_CURSE_PAGE_SIZE && key == '6')
        {
            selected += METARUN_KNOWN_CURSE_PAGE_SIZE;
            if (selected >= known_count)
                selected = known_count - 1;
            continue;
        }
        if (key == ESCAPE || key == '\r' || key == '\n'
            || (steamdeck
                && (key == steamdeck_confirm_key() || key == steamdeck_back_key())))
        {
            metarun_ui_clear_pending_input();
            break;
        }

        metarun_ui_clear_pending_input();
        break;
    }

    return true;
}

static int blessing_points_remaining(void)
{
    int earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    int spent = metar.blessing_points_spent;
    if (spent > earned) spent = earned;
    int available = earned - spent;
    if (available < 0) available = 0;
    return available;
}

static void blessing_spend_points(int cost)
{
    if (cost <= 0) return;
    int earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    int spent = metar.blessing_points_spent + cost;
    if (spent > earned) spent = earned;
    if (spent < 0) spent = 0;
    metar.blessing_points_spent = (u16b)spent;
}

static void blessing_commit_changes(bool apply_runtime)
{
    if (!sync_current_metarun_slot(false)) {
        log_warn("blessing_commit_changes: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
    refresh_current_metar_score();
    if (apply_runtime) {
        metarun_apply_runtime_effects();
    }
    save_metaruns();
}

typedef struct metarun_major_blessing_choice {
    int idx;
    char key;
    int cost;
} metarun_major_blessing_choice;

typedef enum metarun_blessing_scene_mode {
    METARUN_BLESSING_SCENE_MAIN = 0,
    METARUN_BLESSING_SCENE_REMOVE = 1,
    METARUN_BLESSING_SCENE_MINOR = 2,
    METARUN_BLESSING_SCENE_MAJOR = 3
} metarun_blessing_scene_mode;

static const char* metarun_blessing_scene_mode_name(int mode)
{
    switch (mode) {
    case METARUN_BLESSING_SCENE_MAIN:
        return "main";
    case METARUN_BLESSING_SCENE_REMOVE:
        return "remove";
    case METARUN_BLESSING_SCENE_MINOR:
        return "minor";
    case METARUN_BLESSING_SCENE_MAJOR:
        return "major";
    default:
        return "unknown";
    }
}

static void metarun_log_blessing_key(const char* context, int mode, int key)
{
    char printable = ((key >= 32) && (key <= 126)) ? (char)key : '?';

    log_debug("[metarun-esc-trace] %s mode=%s key=%d char=%c esc=%d active=%d",
        context ? context : "blessing", metarun_blessing_scene_mode_name(mode),
        key, printable, key == ESCAPE ? 1 : 0,
        ui_information_scene_is_active() ? 1 : 0);
}

static int blessing_collect_removable_curses(int *ids, int capacity)
{
    int count = 0;

    if (!ids || capacity <= 0)
        return 0;

    for (int id = 0; id < z_info->cu_max && count < capacity; id++) {
        if (CURSE_CURSE_STACK(id) > 0)
            ids[count++] = id;
    }

    return count;
}

static bool blessing_apply_remove_curse_choice(int curse_id, char *result_msg,
    size_t msg_size, byte *result_attr)
{
    int current_stacks;

    if (curse_id < 0 || curse_id >= z_info->cu_max
        || CURSE_CURSE_STACK(curse_id) <= 0)
    {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "No curses cling to this saga.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    current_stacks = CURSE_CURSE_STACK(curse_id);
    if (current_stacks > 1)
        CURSE_SET(curse_id, current_stacks - 1);
    else
        CURSE_SET(curse_id, 0);
    CURSE_SEEN_SET(curse_id);

    blessing_spend_points(1);

    metar.pending_blessing_count = 0;
    for (int i = 0; i < 3; i++)
        metar.pending_blessing_choices[i] = 255;

    blessing_commit_changes(true);

    if (result_msg && msg_size > 0) {
        if (current_stacks > 1) {
            snprintf(result_msg, msg_size,
                "One stack of %s is lifted. (%d remain%s)",
                curse_display_name(curse_id), current_stacks - 1,
                (current_stacks - 1 == 1) ? "s" : "");
        } else {
            snprintf(result_msg, msg_size, "The curse of %s is lifted.",
                curse_display_name(curse_id));
        }
        if (result_attr) *result_attr = TERM_L_BLUE;
    }

    return true;
}

static bool blessing_prepare_minor_choices(int *options, int *out_picks,
    char *result_msg, size_t msg_size, byte *result_attr)
{
    int picks = 0;
    bool have_valid_pending = false;

    if (!options || !out_picks)
        return false;

    if (metar.pending_blessing_count > 0) {
        for (int i = 0; i < metar.pending_blessing_count && i < 3; i++) {
            int id = metar.pending_blessing_choices[i];
            curse_type *c;
            int stacks;
            int blessing_stacks;

            if (id == 255) continue;
            c = &cu_info[id];
            if (!c->blessing_name) continue;

            stacks = CURSE_GET(id);
            if (stacks > 0) continue;

            blessing_stacks = (stacks < 0) ? -stacks : 0;
            if (CURSE_BLESSING_CAP(id) > 0
                && blessing_stacks >= CURSE_BLESSING_CAP(id))
            {
                continue;
            }

            options[picks++] = id;
        }

        if (picks > 0)
            have_valid_pending = true;
    }

    if (!have_valid_pending) {
        int eligible[METAR_CURSE_SLOTS];
        int weights[METAR_CURSE_SLOTS];
        int count = 0;
        int total_weight = 0;

        for (int id = 0; id < z_info->cu_max; id++) {
            curse_type *c = &cu_info[id];
            int stacks;
            int blessing_stacks;
            int base_weight;
            int effective_weight;

            if (!c->blessing_name) continue;

            stacks = CURSE_GET(id);
            if (stacks > 0) continue;

            blessing_stacks = (stacks < 0) ? -stacks : 0;
            if (CURSE_BLESSING_CAP(id) > 0
                && blessing_stacks >= CURSE_BLESSING_CAP(id))
            {
                continue;
            }

            if (count >= METAR_CURSE_SLOTS)
                continue;

            eligible[count] = id;
            base_weight = c->weight > 0 ? c->weight : 1;
            effective_weight = base_weight / (blessing_stacks + 1);
            weights[count] = (effective_weight > 0) ? effective_weight : 1;
            total_weight += weights[count];
            count++;
        }

        if (count == 0) {
            if (result_msg && msg_size > 0) {
                SDL_strlcpy(result_msg,
                    "No blessings are presently available.", msg_size);
                if (result_attr) *result_attr = TERM_L_DARK;
            }
            *out_picks = 0;
            return false;
        }

        picks = MIN(3, count);
        for (int i = 0; i < picks; i++) {
            int roll = rand_int(total_weight);
            int sum = 0;
            int selected = 0;

            for (int j = 0; j < count; j++) {
                sum += weights[j];
                if (roll < sum) {
                    selected = j;
                    break;
                }
            }

            options[i] = eligible[selected];
            total_weight -= weights[selected];
            eligible[selected] = eligible[count - 1];
            weights[selected] = weights[count - 1];
            count--;
        }

        metar.pending_blessing_count = picks;
        for (int i = 0; i < 3; i++) {
            metar.pending_blessing_choices[i] = (i < picks)
                ? options[i] : 255;
        }
        save_metaruns();
    }

    *out_picks = picks;
    return picks > 0;
}

static bool blessing_apply_minor_choice(int blessing_id, char *result_msg,
    size_t msg_size, byte *result_attr)
{
    int stacks = CURSE_GET(blessing_id);
    int blessing_stacks = (stacks < 0) ? -stacks : 0;

    if (blessing_id < 0 || blessing_id >= z_info->cu_max
        || !cu_info[blessing_id].blessing_name)
    {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "That blessing is no longer available.",
                msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    if (CURSE_BLESSING_CAP(blessing_id) > 0
        && blessing_stacks >= CURSE_BLESSING_CAP(blessing_id))
    {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg,
                "That blessing cannot grow any stronger.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    CURSE_ADD(blessing_id, -1);
    CURSE_SEEN_SET(blessing_id);
    blessing_spend_points(1);

    metar.pending_blessing_count = 0;
    for (int i = 0; i < 3; i++)
        metar.pending_blessing_choices[i] = 255;

    blessing_commit_changes(true);

    if (result_msg && msg_size > 0) {
        snprintf(result_msg, msg_size, "You receive the %s.",
            blessing_display_name(blessing_id));
        if (result_attr) *result_attr = TERM_L_GREEN;
    }

    return true;
}

static int blessing_collect_major_choices(
    metarun_major_blessing_choice *options, int capacity, int *out_min_cost)
{
    int option_count = 0;
    int min_cost = INT_MAX;
    int cap;

    if (out_min_cost)
        *out_min_cost = 0;

    metarun_sanitize_major_blessing_bits(&metar);
    cap = major_blessing_capacity();
    if (cap <= 0 || !mb_info || !options || capacity <= 0)
        return 0;

    for (int i = 0; i < cap && option_count < capacity; i++) {
        int cost;

        if (metarun_has_major_blessing_index(i)) continue;
        if (!major_blessing_def(i)) continue;

        cost = major_blessing_cost(i);
        if (cost < 0) cost = 0;
        options[option_count].idx = i;
        options[option_count].key = (char)('a' + option_count);
        options[option_count].cost = cost;
        if (cost < min_cost)
            min_cost = cost;
        option_count++;
    }

    if (out_min_cost) {
        *out_min_cost = (min_cost == INT_MAX) ? 0 : min_cost;
    }

    return option_count;
}

static bool blessing_apply_major_choice(int choice_idx, char *result_msg,
    size_t msg_size, byte *result_attr)
{
    metar.major_blessings |= (1U << choice_idx);
    blessing_spend_points(major_blessing_cost(choice_idx));
    blessing_commit_changes(true);

    if (result_msg && msg_size > 0) {
        const char *msg = major_blessing_unlock_msg(choice_idx);

        if (msg && *msg) {
            SDL_strlcpy(result_msg, msg, msg_size);
        } else {
            snprintf(result_msg, msg_size, "You seal the %s.",
                major_blessing_name_str(choice_idx));
        }
        if (result_attr) *result_attr = TERM_YELLOW;
    }

    return true;
}

static bool open_blessing_exchange_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label)
{
    metarun_blessing_scene_mode mode = METARUN_BLESSING_SCENE_MAIN;
    int selected_main = 0;
    int selected_remove = 0;
    int selected_minor = 0;
    int selected_major = 0;
    char status_msg[256] = "";
    byte status_attr = TERM_WHITE;
    bool clear_status_on_next_key = false;

    log_debug("[metarun-esc-trace] blessing exchange semantic enter");

    while (true) {
        int available;
        int earned;
        int spent;
        int main_option_count = 3;
        int min_major_cost = 0;
        metarun_major_blessing_choice major_options[16];
        int major_option_count;
        bool major_available;
        bool major_affordable;

        compute_blessing_pool();
        available = blessing_points_remaining();
        earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        spent = metar.blessing_points_spent;
        major_option_count = blessing_collect_major_choices(major_options,
            (int)N_ELEMENTS(major_options), &min_major_cost);
        major_available = major_option_count > 0;
        major_affordable = major_available && (min_major_cost <= available);

        if (mode == METARUN_BLESSING_SCENE_MAIN) {
            app_ui_scene scene;
            app_ui_panel *panel;
            char subtitle[APP_UI_TEXT_MAX];
            char buf[APP_UI_TEXT_MAX];
            int key;
            u32b threshold = metarun_threshold_value(&metar);

            if (threshold == 0) threshold = 1;
            if (selected_main < 0) selected_main = 0;
            if (selected_main >= main_option_count)
                selected_main = main_option_count - 1;

            strnfmt(subtitle, sizeof(subtitle),
                "%d available (%d spent / %d earned)", available, spent,
                earned);
            panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
                "Blessing Exchange", TERM_SLATE, subtitle);
            if (!panel)
                return false;

            strnfmt(buf, sizeof(buf),
                "Fallen score pool: %lu total, %lu / %lu to next blessing",
                (unsigned long)metar.fallen_score_total,
                (unsigned long)metar.fallen_score_pool,
                (unsigned long)threshold);
            (void)app_ui_panel_add_body_line(panel, TERM_WHITE, buf);
            if (status_msg[0] != '\0')
                (void)app_ui_panel_add_body_line(panel, status_attr, status_msg);

            if (!app_ui_panel_add_row(panel, 0, TERM_WHITE, true,
                    selected_main == 0, "r", "Remove a curse", "cost 1"))
            {
                return false;
            }
            if (!app_ui_panel_add_row(panel, 1, TERM_WHITE, true,
                    selected_main == 1, "m", "Gain a minor blessing",
                    "cost 1"))
            {
                return false;
            }
            if (major_available) {
                strnfmt(buf, sizeof(buf), "cost %d", min_major_cost);
                if (!app_ui_panel_add_row(panel, 2,
                        major_affordable ? TERM_WHITE : TERM_L_DARK, true,
                        selected_main == 2, "u",
                        "Unlock a major blessing", buf))
                {
                    return false;
                }
            } else {
                if (!app_ui_panel_add_row(panel, 2, TERM_L_DARK, true,
                        selected_main == 2, "u",
                        "Unlock a major blessing", "none available"))
                {
                    return false;
                }
            }

            app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Option");
            if (selected_main == 0) {
                (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
                    "Lift one stack from an active curse.");
                (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
                    "Available immediately if you have a blessing point.");
            } else if (selected_main == 1) {
                (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN,
                    "Accept one of three offered minor blessings.");
                (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
                    "Offered gifts persist until chosen or invalidated.");
            } else {
                if (major_available) {
                    strnfmt(buf, sizeof(buf), "Lowest available cost: %d",
                        min_major_cost);
                    (void)app_ui_panel_add_detail_line(panel,
                        major_affordable ? TERM_YELLOW : TERM_L_DARK,
                        "Seal a permanent major blessing for this metarun.");
                    (void)app_ui_panel_add_detail_line(panel,
                        major_affordable ? TERM_WHITE : TERM_L_DARK, buf);
                    if (!major_affordable) {
                        strnfmt(buf, sizeof(buf),
                            "You need %d blessing points to unlock one.",
                            min_major_cost);
                        (void)app_ui_panel_add_detail_line(panel, TERM_ORANGE,
                            buf);
                    }
                } else {
                    (void)app_ui_panel_add_detail_line(panel, TERM_L_DARK,
                        "All major blessings have already been sealed.");
                }
            }

            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                steamdeck ? accept_label : "Enter", "Choose");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "8/2", "Move");
            (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                steamdeck ? back_label : "Esc", "Leave");

            if (!ui_information_scene_present_ui(&scene))
                return false;

            key = ui_information_scene_wait_key_nonrepeat();
            metarun_log_blessing_key("blessing-scene-read", mode, key);
            if (clear_status_on_next_key || key == '8' || key == 'k'
                || key == '-' || key == '2' || key == 'j' || key == '+')
            {
                status_msg[0] = '\0';
                clear_status_on_next_key = false;
            }

            if (key == ESCAPE || key == '4'
                || (steamdeck && key == steamdeck_back_key())
                || (!steamdeck && (key == 'h' || key == 'H')))
            {
                metarun_ui_clear_pending_input();
                return true;
            }
            if (key == '8' || key == 'k' || key == '-') {
                selected_main = (selected_main + main_option_count - 1)
                    % main_option_count;
                continue;
            }
            if (key == '2' || key == 'j' || key == '+') {
                selected_main = (selected_main + 1) % main_option_count;
                continue;
            }
            if (key == '\r' || key == '\n'
                || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
            {
                key = (selected_main == 0) ? 'r'
                    : (selected_main == 1) ? 'm' : 'u';
            }

            if (key == 'r' || key == 'R') {
                if (available < 1) {
                    SDL_strlcpy(status_msg,
                        "You need at least one blessing point to lift a curse.",
                        sizeof(status_msg));
                    status_attr = TERM_ORANGE;
                    clear_status_on_next_key = true;
                } else {
                    log_debug("[metarun-esc-trace] blessing mode main->remove");
                    mode = METARUN_BLESSING_SCENE_REMOVE;
                }
                continue;
            }
            if (key == 'm' || key == 'M') {
                if (available < 1) {
                    SDL_strlcpy(status_msg,
                        "You need at least one blessing point to receive a gift.",
                        sizeof(status_msg));
                    status_attr = TERM_ORANGE;
                    clear_status_on_next_key = true;
                } else {
                    log_debug("[metarun-esc-trace] blessing mode main->minor");
                    mode = METARUN_BLESSING_SCENE_MINOR;
                }
                continue;
            }
            if (key == 'u' || key == 'U') {
                if (!major_available) {
                    SDL_strlcpy(status_msg,
                        "All major blessings have already been sealed.",
                        sizeof(status_msg));
                    status_attr = TERM_L_DARK;
                    clear_status_on_next_key = true;
                } else if (!major_affordable) {
                    snprintf(status_msg, sizeof(status_msg),
                        "You need %d blessing points to unlock a major blessing.",
                        min_major_cost);
                    status_attr = TERM_ORANGE;
                    clear_status_on_next_key = true;
                } else {
                    log_debug("[metarun-esc-trace] blessing mode main->major");
                    mode = METARUN_BLESSING_SCENE_MAJOR;
                }
                continue;
            }

            bell("Unrecognised option.");
            continue;
        }

        if (mode == METARUN_BLESSING_SCENE_REMOVE) {
            int ids[METAR_CURSE_SLOTS];
            int count = blessing_collect_removable_curses(ids,
                (int)N_ELEMENTS(ids));
            app_ui_scene scene;
            app_ui_panel *panel;
            int key;

            if (count <= 0) {
                SDL_strlcpy(status_msg, "No curses cling to this saga.",
                    sizeof(status_msg));
                status_attr = TERM_L_DARK;
                clear_status_on_next_key = true;
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            if (selected_remove < 0) selected_remove = 0;
            if (selected_remove >= count) selected_remove = count - 1;

            panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
                "Remove a Curse", TERM_SLATE, "Cost: 1 blessing point");
            if (!panel)
                return false;

            (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
                "Choose which curse to lift.");

            for (int i = 0; i < count; i++) {
                char key_buf[APP_UI_KEY_MAX];
                char meta[APP_UI_META_MAX];

                strnfmt(key_buf, sizeof(key_buf), "%c", (char)('a' + i));
                strnfmt(meta, sizeof(meta), "stacks: %d",
                    CURSE_CURSE_STACK(ids[i]));
                if (!app_ui_panel_add_row(panel, ids[i], TERM_RED, true,
                        i == selected_remove, key_buf,
                        curse_display_name(ids[i]), meta))
                {
                    return false;
                }
            }

            if (!metarun_ui_add_effect_detail_lines(panel, ids[selected_remove]))
                return false;

            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                steamdeck ? accept_label : "Enter", "Lift");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "8/2", "Move");
            (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                steamdeck ? back_label : "Esc", "Back");

            if (!ui_information_scene_present_ui(&scene))
                return false;

            key = ui_information_scene_wait_key_nonrepeat();
            metarun_log_blessing_key("blessing-scene-read", mode, key);
            if (key == ESCAPE || key == '4'
                || (steamdeck && key == steamdeck_back_key())
                || (!steamdeck && (key == 'h' || key == 'H')))
            {
                log_debug("[metarun-esc-trace] blessing mode remove->main via back");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }
            if (key == '8' || key == 'k' || key == '-') {
                selected_remove = (selected_remove + count - 1) % count;
                continue;
            }
            if (key == '2' || key == 'j' || key == '+') {
                selected_remove = (selected_remove + 1) % count;
                continue;
            }
            if (key == '\r' || key == '\n'
                || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
            {
                (void)blessing_apply_remove_curse_choice(ids[selected_remove],
                    status_msg, sizeof(status_msg), &status_attr);
                clear_status_on_next_key = true;
                log_debug("[metarun-esc-trace] blessing mode remove->main via apply");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            {
                int idx = (key >= 'A' && key <= 'Z') ? key - 'A' : key - 'a';
                if (idx >= 0 && idx < count) {
                    selected_remove = idx;
                    (void)blessing_apply_remove_curse_choice(ids[idx],
                        status_msg, sizeof(status_msg), &status_attr);
                    clear_status_on_next_key = true;
                    log_debug("[metarun-esc-trace] blessing mode remove->main via letter");
                    mode = METARUN_BLESSING_SCENE_MAIN;
                    continue;
                }
            }

            bell("Invalid selection.");
            continue;
        }

        if (mode == METARUN_BLESSING_SCENE_MINOR) {
            int options[3];
            int picks = 0;
            app_ui_scene scene;
            app_ui_panel *panel;
            int key;

            if (!blessing_prepare_minor_choices(options, &picks, status_msg,
                    sizeof(status_msg), &status_attr))
            {
                clear_status_on_next_key = true;
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            if (selected_minor < 0) selected_minor = 0;
            if (selected_minor >= picks) selected_minor = picks - 1;

            panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
                "Receive a Blessing", TERM_SLATE, "Cost: 1 blessing point");
            if (!panel)
                return false;

            (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
                "Select a gift to accept.");

            for (int i = 0; i < picks; i++) {
                int blessing_stacks = 0;
                char key_buf[APP_UI_KEY_MAX];
                char meta[APP_UI_META_MAX];
                int stacks = CURSE_GET(options[i]);

                if (stacks < 0)
                    blessing_stacks = -stacks;
                strnfmt(key_buf, sizeof(key_buf), "%c", (char)('a' + i));
                strnfmt(meta, sizeof(meta), "current: %d", blessing_stacks);
                if (!app_ui_panel_add_row(panel, options[i], TERM_L_GREEN, true,
                        i == selected_minor, key_buf,
                        blessing_display_name(options[i]), meta))
                {
                    return false;
                }
            }

            {
                curse_type *c = &cu_info[options[selected_minor]];
                app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
                    "Selected Blessing");
                (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN,
                    blessing_display_name(options[selected_minor]));
                if (c->blessing_text) {
                    if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_WHITE,
                            cu_text + c->blessing_text))
                    {
                        return false;
                    }
                }
                if (c->blessing_power) {
                    if (!metarun_ui_add_wrapped_detail_lines(panel,
                            TERM_L_GREEN, cu_text + c->blessing_power))
                    {
                        return false;
                    }
                }
            }

            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                steamdeck ? accept_label : "Enter", "Accept");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "8/2", "Move");
            (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                steamdeck ? back_label : "Esc", "Back");

            if (!ui_information_scene_present_ui(&scene))
                return false;

            key = ui_information_scene_wait_key_nonrepeat();
            metarun_log_blessing_key("blessing-scene-read", mode, key);
            if (key == ESCAPE || key == '4'
                || (steamdeck && key == steamdeck_back_key())
                || (!steamdeck && (key == 'h' || key == 'H')))
            {
                log_debug("[metarun-esc-trace] blessing mode minor->main via back");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }
            if (key == '8' || key == 'k' || key == '-') {
                selected_minor = (selected_minor + picks - 1) % picks;
                continue;
            }
            if (key == '2' || key == 'j' || key == '+') {
                selected_minor = (selected_minor + 1) % picks;
                continue;
            }
            if (key == '\r' || key == '\n'
                || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
            {
                (void)blessing_apply_minor_choice(options[selected_minor],
                    status_msg, sizeof(status_msg), &status_attr);
                clear_status_on_next_key = true;
                log_debug("[metarun-esc-trace] blessing mode minor->main via apply");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            {
                int idx = (key >= 'A' && key <= 'Z') ? key - 'A' : key - 'a';
                if (idx >= 0 && idx < picks) {
                    selected_minor = idx;
                    (void)blessing_apply_minor_choice(options[idx], status_msg,
                        sizeof(status_msg), &status_attr);
                    clear_status_on_next_key = true;
                    log_debug("[metarun-esc-trace] blessing mode minor->main via letter");
                    mode = METARUN_BLESSING_SCENE_MAIN;
                    continue;
                }
            }

            bell("Invalid selection.");
            continue;
        }

        if (mode == METARUN_BLESSING_SCENE_MAJOR) {
            app_ui_scene scene;
            app_ui_panel *panel;
            int key;
            int first_affordable = -1;

            if (!major_available) {
                SDL_strlcpy(status_msg,
                    "All major blessings have already been sealed.",
                    sizeof(status_msg));
                status_attr = TERM_L_DARK;
                clear_status_on_next_key = true;
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            for (int i = 0; i < major_option_count; i++) {
                if (major_options[i].cost <= available) {
                    first_affordable = i;
                    break;
                }
            }
            if (first_affordable < 0) {
                snprintf(status_msg, sizeof(status_msg),
                    "You need %d blessing points to unlock a major blessing.",
                    min_major_cost);
                status_attr = TERM_ORANGE;
                clear_status_on_next_key = true;
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }
            if (selected_major < 0 || selected_major >= major_option_count
                || major_options[selected_major].cost > available)
            {
                selected_major = first_affordable;
            }

            panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
                "Unlock a Major Blessing", TERM_SLATE,
                "Forge a covenant for this saga.");
            if (!panel)
                return false;

            for (int i = 0; i < major_option_count; i++) {
                bool affordable = (major_options[i].cost <= available);
                char key_buf[APP_UI_KEY_MAX];
                char meta[APP_UI_META_MAX];

                strnfmt(key_buf, sizeof(key_buf), "%c", major_options[i].key);
                strnfmt(meta, sizeof(meta), "cost %d", major_options[i].cost);
                if (!app_ui_panel_add_row(panel, major_options[i].idx,
                        affordable ? TERM_L_GREEN : TERM_L_DARK, true,
                        i == selected_major, key_buf,
                        major_blessing_name_str(major_options[i].idx), meta))
                {
                    return false;
                }
            }

            {
                int idx = major_options[selected_major].idx;
                const char *detail = major_blessing_detail_desc(idx);
                char line[APP_UI_TEXT_MAX];

                app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
                    "Selected Covenant");
                (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN,
                    major_blessing_name_str(idx));
                strnfmt(line, sizeof(line), "Cost: %d blessing point%s",
                    major_options[selected_major].cost,
                    (major_options[selected_major].cost == 1) ? "" : "s");
                (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, line);
                strnfmt(line, sizeof(line), "Available: %d", available);
                (void)app_ui_panel_add_detail_line(panel, TERM_L_BLUE, line);
                if (detail && *detail) {
                    if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_WHITE,
                            detail))
                    {
                        return false;
                    }
                }
            }

            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                steamdeck ? accept_label : "Enter", "Unlock");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "8/2", "Move");
            (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                steamdeck ? back_label : "Esc", "Back");

            if (!ui_information_scene_present_ui(&scene))
                return false;

            key = ui_information_scene_wait_key_nonrepeat();
            metarun_log_blessing_key("blessing-scene-read", mode, key);
            if (key == ESCAPE || key == '4'
                || (steamdeck && key == steamdeck_back_key())
                || (!steamdeck && (key == 'h' || key == 'H')))
            {
                log_debug("[metarun-esc-trace] blessing mode major->main via back");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }
            if (key == '8' || key == 'k' || key == '-') {
                int start = selected_major;
                do {
                    selected_major = (selected_major + major_option_count - 1)
                        % major_option_count;
                    if (major_options[selected_major].cost <= available)
                        break;
                } while (selected_major != start);
                continue;
            }
            if (key == '2' || key == 'j' || key == '+') {
                int start = selected_major;
                do {
                    selected_major = (selected_major + 1) % major_option_count;
                    if (major_options[selected_major].cost <= available)
                        break;
                } while (selected_major != start);
                continue;
            }
            if (key == '\r' || key == '\n'
                || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
            {
                (void)blessing_apply_major_choice(
                    major_options[selected_major].idx, status_msg,
                    sizeof(status_msg), &status_attr);
                clear_status_on_next_key = true;
                log_debug("[metarun-esc-trace] blessing mode major->main via apply");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            {
                int choice_idx = -1;
                char lowered = tolower((unsigned char)key);

                if (lowered >= 'a' && lowered <= 'z') {
                    for (int i = 0; i < major_option_count; i++) {
                        if (lowered != major_options[i].key)
                            continue;
                        if (major_options[i].cost > available) {
                            bell("Not enough blessing points for that covenant.");
                            choice_idx = -2;
                            break;
                        }
                        choice_idx = i;
                        break;
                    }
                }

                if (choice_idx == -2)
                    continue;
                if (choice_idx >= 0) {
                    selected_major = choice_idx;
                    (void)blessing_apply_major_choice(
                        major_options[choice_idx].idx, status_msg,
                        sizeof(status_msg), &status_attr);
                    clear_status_on_next_key = true;
                    log_debug("[metarun-esc-trace] blessing mode major->main via letter");
                    mode = METARUN_BLESSING_SCENE_MAIN;
                    continue;
                }
            }

            bell("Invalid selection.");
            continue;
        }
    }
}

typedef struct metarun_stats_view_model {
    const char* diff_name;
    const char* threshold_mode;
    int win_goal;
    int remaining_silmarils;
    int required_survivors;
    int alive;
    u32b best_run;
    u32b total_pool;
    u32b remainder;
    u32b threshold;
    int earned_points;
    int spent_points;
    int available_points;
    int unlocked_major;
    int major_total;
    bool steamdeck;
    bool blitz_enabled;
    char sil_bar[32];
    char death_marks[32];
    char spend_label[16];
    char threshold_label[16];
    char diff_label[16];
    char full_label[16];
    char history_label[16];
    char blitz_label[16];
    char back_label[16];
    char continue_label[16];
} metarun_stats_view_model;

static void metarun_stats_prepare_view_model(metarun_stats_view_model* view)
{
    if (!view)
        return;

    memset(view, 0, sizeof(*view));
    view->diff_name = "Unknown";
    view->win_goal = WINCON_SILMARILS;

    if (runtype_info && metar.type < z_info->rt_max
        && runtype_info[metar.type].name[0])
    {
        view->diff_name = runtype_info[metar.type].name;
        view->win_goal = runtype_info[metar.type].win_con
            ? runtype_info[metar.type].win_con
            : WINCON_SILMARILS;
    }

    if (view->win_goal <= 0)
        view->win_goal = WINCON_SILMARILS;

    view->remaining_silmarils = view->win_goal - metar.silmarils;
    if (view->remaining_silmarils < 0)
        view->remaining_silmarils = 0;

    build_symbol_bar(view->sil_bar, sizeof(view->sil_bar), metar.silmarils,
        view->win_goal, '*');
    build_death_marks(view->death_marks, sizeof(view->death_marks),
        metar.deaths);

    view->required_survivors = required_survivor_target(view->win_goal);
    view->alive = metar.alive_characters;
    view->best_run = get_best_run_score_from_highscores();
    view->total_pool = metar.fallen_score_total;
    view->remainder = metar.fallen_score_pool;
    view->threshold = metarun_threshold_value(&metar);
    if (view->threshold == 0)
        view->threshold = 1;
    view->threshold_mode = threshold_mode_name(
        metarun_get_threshold_mode(&metar));
    view->earned_points = metar.blessing_points;
    view->spent_points = metar.blessing_points_spent;
    view->available_points = view->earned_points - view->spent_points;
    view->steamdeck = steamdeck_controls_active();
    view->blitz_enabled = (op_ptr && op_ptr->opt[OPT_unlock_blitz_mode]);
    view->major_total = metarun_major_blessing_count();

    for (int i = 0; i < view->major_total; i++)
    {
        if (metarun_has_major_blessing_index(i))
            view->unlocked_major++;
    }

    if (view->steamdeck)
    {
        int confirm_key = steamdeck_confirm_key();
        int back_key = steamdeck_back_key();
        int alt_key = steamdeck_alt_action_key();
        int secondary_key = steamdeck_secondary_key();

        metarun_prompt_label(confirm_key, "A", view->continue_label,
            sizeof(view->continue_label));
        metarun_prompt_label(back_key, "B", view->back_label,
            sizeof(view->back_label));
        metarun_prompt_label(alt_key, "X", view->spend_label,
            sizeof(view->spend_label));
        metarun_prompt_label(secondary_key, "Y", view->history_label,
            sizeof(view->history_label));
        metarun_prompt_label('c', "L1", view->diff_label,
            sizeof(view->diff_label));
        metarun_prompt_label('f', "R1", view->threshold_label,
            sizeof(view->threshold_label));
        metarun_prompt_label('u', "Start", view->full_label,
            sizeof(view->full_label));
        metarun_prompt_label('x', "RS Right", view->blitz_label,
            sizeof(view->blitz_label));
    }
    else
    {
        SDL_strlcpy(view->continue_label, "Enter",
            sizeof(view->continue_label));
        SDL_strlcpy(view->back_label, "Esc", sizeof(view->back_label));
        SDL_strlcpy(view->spend_label, "b", sizeof(view->spend_label));
        SDL_strlcpy(view->threshold_label, "f",
            sizeof(view->threshold_label));
        SDL_strlcpy(view->diff_label, "c", sizeof(view->diff_label));
        SDL_strlcpy(view->full_label, "u", sizeof(view->full_label));
        SDL_strlcpy(view->history_label, "s", sizeof(view->history_label));
        SDL_strlcpy(view->blitz_label, "x", sizeof(view->blitz_label));
    }
}

static bool metarun_ui_add_section_row(app_ui_panel* panel, byte attr,
    const char* text)
{
    if (!panel || !text || !text[0]
        || !app_ui_panel_add_row(panel, 0, attr, true, false, "", text, ""))
    {
        return false;
    }

    panel->rows[panel->row_count - 1].flags |= APP_UI_ITEM_FLAG_SECTION;
    return true;
}

static bool metarun_ui_add_value_row(app_ui_panel* panel, byte label_attr,
    const char* label, byte value_attr, const char* value)
{
    return app_ui_panel_add_row_ex(panel, 0, label_attr, value_attr, 0, '\0',
        true, false, "", label ? label : "", value ? value : "");
}

static bool metarun_ui_add_effect_row_ex(app_ui_panel* panel, int id,
    bool selected)
{
    const curse_type* cu;
    const char* effect = NULL;
    const char* name;
    char label[APP_UI_LABEL_MAX];
    char meta[APP_UI_META_MAX];
    byte attr;
    char icon;
    int stacks;
    int magnitude;
    bool is_blessing;

    if (!panel || id < 0 || id >= z_info->cu_max)
        return false;

    stacks = CURSE_GET(id);
    if (!stacks)
        return true;

    is_blessing = (stacks < 0);
    magnitude = is_blessing ? -stacks : stacks;
    name = is_blessing ? blessing_display_name(id) : curse_display_name(id);
    attr = is_blessing ? TERM_L_GREEN : TERM_RED;
    icon = is_blessing ? '+' : '-';
    cu = &cu_info[id];

    if (CURSE_SEEN(id))
    {
        effect = is_blessing
            ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
            : (cu->power ? cu_text + cu->power : NULL);
    }

    strnfmt(label, sizeof(label), "%s", name);
    if (effect && *effect)
        strnfmt(meta, sizeof(meta), "%c%d  %s", icon, magnitude, effect);
    else
        strnfmt(meta, sizeof(meta), "%c%d", icon, magnitude);

    return app_ui_panel_add_row_ex(panel, id, attr, attr, attr, icon, true,
        selected, "", label, meta);
}

static bool metarun_ui_add_effect_row(app_ui_panel* panel, int id)
{
    return metarun_ui_add_effect_row_ex(panel, id, false);
}

static void metarun_trim_first_line(char* dst, size_t dst_size,
    const char* source)
{
    char* newline;

    if (!dst || dst_size == 0)
        return;

    SDL_strlcpy(dst, source ? source : "", dst_size);
    newline = strchr(dst, '\n');
    if (newline)
        *newline = '\0';
}

#define METARUN_UI_WRAP_WIDTH 68
#define METARUN_HISTORY_PAGE_SIZE 48

static app_ui_panel* metarun_ui_begin_browser_scene(app_ui_scene* scene,
    byte title_attr, const char* title, byte subtitle_attr,
    const char* subtitle)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    app_ui_panel_set_widths(panel, 980, 2048);
    app_ui_panel_set_title(panel, title_attr, title ? title : "");
    if (subtitle && subtitle[0])
        app_ui_panel_set_subtitle(panel, subtitle_attr, subtitle);

    return panel;
}

static app_ui_panel* metarun_ui_begin_modal_scene(app_ui_scene* scene,
    byte title_attr, const char* title)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 340, 620);
    app_ui_panel_set_title(panel, title_attr, title ? title : "");
    return panel;
}

static app_ui_panel* metarun_ui_begin_story_scene(app_ui_scene* scene,
    byte title_attr, const char* title)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 720, 1180);
    app_ui_panel_set_title(panel, title_attr, title ? title : "");
    return panel;
}

static bool metarun_ui_add_wrapped_text_lines(app_ui_panel* panel, byte attr,
    const char* text, bool detail)
{
    const char* cursor = text;

    if (!panel || !text || !text[0])
        return true;

    while (*cursor)
    {
        const char* start;
        const char* split = NULL;
        size_t len = 0;
        char line[APP_UI_TEXT_MAX];

        while (*cursor == ' ' || *cursor == '\t')
            cursor++;

        if (*cursor == '\n')
        {
            cursor++;
            if (detail)
            {
                if (!app_ui_panel_add_detail_line(panel, attr, " "))
                    return false;
            }
            else if (!app_ui_panel_add_body_line(panel, attr, " "))
            {
                return false;
            }
            continue;
        }

        if (detail)
        {
            if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
                break;
        }
        else if (panel->body_line_count >= APP_UI_BODY_LINE_MAX)
        {
            break;
        }

        start = cursor;
        while (cursor[len] && cursor[len] != '\n')
        {
            if (len < METARUN_UI_WRAP_WIDTH)
            {
                if (cursor[len] == ' ')
                    split = cursor + len;
            }
            else if (split)
            {
                break;
            }
            else if (len >= (sizeof(line) - 1))
            {
                break;
            }
            len++;
        }

        if (cursor[len] && cursor[len] != '\n' && split && split > start)
            len = (size_t)(split - start);
        else if (len == 0 && cursor[len] && cursor[len] != '\n')
            len = 1;

        if (len >= sizeof(line))
            len = sizeof(line) - 1;

        memcpy(line, start, len);
        line[len] = '\0';
        while (len > 0 && isspace((unsigned char)line[len - 1]))
            line[--len] = '\0';

        if (line[0])
        {
            if (detail)
            {
                if (!app_ui_panel_add_detail_line(panel, attr, line))
                    return false;
            }
            else if (!app_ui_panel_add_body_line(panel, attr, line))
            {
                return false;
            }
        }

        cursor = start + len;
        while (*cursor == ' ' || *cursor == '\t')
            cursor++;
        if (*cursor == '\n')
            cursor++;
    }

    return true;
}

static bool metarun_ui_add_wrapped_body_lines(app_ui_panel* panel, byte attr,
    const char* text)
{
    return metarun_ui_add_wrapped_text_lines(panel, attr, text, false);
}

static bool metarun_ui_add_wrapped_detail_lines(app_ui_panel* panel, byte attr,
    const char* text)
{
    return metarun_ui_add_wrapped_text_lines(panel, attr, text, true);
}

static bool metarun_ui_add_story_paragraphs(app_ui_scene* scene,
    app_ui_panel* panel, const char* const* paragraphs, const byte* attrs,
    int paragraph_count)
{
    bool wrote_any = false;

    if (!scene || !panel)
        return false;
    if (!app_ui_panel_begin_rich_paragraph(scene, panel))
        return false;

    for (int i = 0; i < paragraph_count; i++)
    {
        const char* text = paragraphs ? paragraphs[i] : NULL;
        byte attr = attrs ? attrs[i] : TERM_WHITE;

        if (!text || !text[0])
            continue;
        if (wrote_any
            && !app_ui_panel_add_rich_text_ex(scene, panel, TERM_WHITE,
                STORY_FLAG_USE, "\n\n"))
        {
            return false;
        }
        if (!app_ui_panel_add_rich_text_ex(scene, panel, attr, STORY_FLAG_USE,
                text))
        {
            return false;
        }
        wrote_any = true;
    }

    if (!wrote_any
        && !app_ui_panel_add_rich_text_ex(scene, panel, TERM_WHITE,
            STORY_FLAG_USE, " "))
    {
        return false;
    }

    return true;
}

static void metarun_ui_clear_pending_input(void)
{
    app_session* session = app_session_current();

    log_debug("[metarun-esc-trace] clear_pending_input active=%d",
        ui_information_scene_is_active() ? 1 : 0);
    if (session)
        app_session_clear_inputs(session);
    input_clear_pending();
}

static bool metarun_ui_add_effect_detail_lines(app_ui_panel* panel, int id)
{
    const curse_type* cu;
    const char* desc = NULL;
    const char* power = NULL;
    const char* name;
    char line[APP_UI_TEXT_MAX];
    int stacks;
    int magnitude;
    bool is_blessing;
    byte attr;

    if (!panel || id < 0 || id >= z_info->cu_max)
        return false;

    stacks = CURSE_GET(id);
    if (!stacks)
        return true;

    is_blessing = (stacks < 0);
    magnitude = is_blessing ? -stacks : stacks;
    name = is_blessing ? blessing_display_name(id) : curse_display_name(id);
    attr = is_blessing ? TERM_L_GREEN : TERM_RED;
    cu = &cu_info[id];

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Effect");
    strnfmt(line, sizeof(line), "%s", name);
    if (!app_ui_panel_add_detail_line(panel, attr, line))
        return false;
    strnfmt(line, sizeof(line), "%s stacks: %d",
        is_blessing ? "Blessing" : "Curse", magnitude);
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
        return false;

    desc = is_blessing
        ? (cu->blessing_text ? cu_text + cu->blessing_text : NULL)
        : (cu->text ? cu_text + cu->text : NULL);
    if (desc && *desc)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
            return false;
        if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_WHITE, desc))
            return false;
    }

    if (CURSE_SEEN(id))
    {
        power = is_blessing
            ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
            : (cu->power ? cu_text + cu->power : NULL);
    }

    if (power && *power)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
            return false;
        strnfmt(line, sizeof(line), "Effect: %s", power);
        if (!metarun_ui_add_wrapped_detail_lines(panel, attr, line))
            return false;
    }
    else if (!CURSE_SEEN(id))
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_L_DARK,
                "(Effect not yet identified)"))
        {
            return false;
        }
    }

    return true;
}

static bool metarun_ui_add_known_curse_detail_lines(app_ui_panel* panel, int id)
{
    curse_type* cu;
    const char* curse_name;
    const char* blessing_name;
    const char* curse_desc = NULL;
    const char* curse_power = NULL;
    const char* blessing_desc = NULL;
    const char* blessing_power = NULL;
    bool show_blessing_name = false;
    char line[APP_UI_TEXT_MAX];

    if (!panel || !z_info || !cu_info || id < 0 || id >= z_info->cu_max
        || !CURSE_SEEN(id) || !cu_info[id].name)
    {
        return false;
    }

    cu = &cu_info[id];
    curse_name = curse_display_name(id);
    blessing_name = blessing_display_name(id);
    show_blessing_name = blessing_name && blessing_name[0]
        && strcmp(blessing_name, curse_name) != 0;

    app_ui_panel_set_detail_title(panel, TERM_L_RED, curse_name);

    if (show_blessing_name)
    {
        strnfmt(line, sizeof(line), "Blessing: %s", blessing_name);
        if (!app_ui_panel_add_detail_line(panel, TERM_L_GREEN, line))
            return false;
    }

    curse_desc = cu->text ? cu_text + cu->text : NULL;
    curse_power = cu->power ? cu_text + cu->power : NULL;
    blessing_desc = cu->blessing_text ? cu_text + cu->blessing_text : NULL;
    blessing_power = cu->blessing_power ? cu_text + cu->blessing_power : NULL;

    if (curse_desc && *curse_desc)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
            return false;
        if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_WHITE, curse_desc))
            return false;
    }

    if (curse_power && *curse_power)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
            return false;
        strnfmt(line, sizeof(line), "Effect: %s", curse_power);
        if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_RED, line))
            return false;
    }

    if ((blessing_desc && *blessing_desc) || (blessing_power && *blessing_power))
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
            return false;
        if (show_blessing_name)
        {
            if (!app_ui_panel_add_detail_line(panel, TERM_L_GREEN,
                    "Blessing Aspect"))
            {
                return false;
            }
        }

        if (blessing_desc && *blessing_desc
            && !metarun_ui_add_wrapped_detail_lines(panel, TERM_L_GREEN,
                blessing_desc))
        {
            return false;
        }

        if (blessing_power && *blessing_power)
        {
            strnfmt(line, sizeof(line), "Blessing effect: %s", blessing_power);
            if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_L_GREEN, line))
                return false;
        }
    }

    if ((!curse_desc || !*curse_desc) && (!curse_power || !*curse_power)
        && (!blessing_desc || !*blessing_desc)
        && (!blessing_power || !*blessing_power))
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_L_DARK,
                "(No lore text recorded)"))
        {
            return false;
        }
    }

    return true;
}

static bool metarun_ui_show_story_modal(const char* title, byte title_attr,
    const char* const* paragraphs, const byte* attrs, int paragraph_count,
    bool steamdeck, const char* accept_label, const char* action_label)
{
    app_ui_scene scene;
    app_ui_panel* panel;

    panel = metarun_ui_begin_story_scene(&scene, title_attr, title);
    if (!panel)
        return false;
    if (!metarun_ui_add_story_paragraphs(&scene, panel, paragraphs, attrs,
            paragraph_count))
    {
        return false;
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        steamdeck ? accept_label : "Any",
        (action_label && action_label[0]) ? action_label : "Continue");

    if (!ui_information_scene_present_ui(&scene))
        return false;

    (void)ui_information_scene_wait_key_nonrepeat();
    metarun_ui_clear_pending_input();
    return true;
}

static bool metarun_ui_show_notice_modal(const char* title, byte title_attr,
    const char* const* lines, const byte* attrs, int line_count, bool steamdeck,
    const char* accept_label)
{
    app_ui_scene scene;
    app_ui_panel* panel;

    panel = metarun_ui_begin_modal_scene(&scene, title_attr, title);
    if (!panel)
        return false;

    for (int i = 0; i < line_count; i++)
    {
        byte attr = attrs ? attrs[i] : TERM_WHITE;

        if (!lines || !lines[i] || !lines[i][0])
            continue;
        if (!metarun_ui_add_wrapped_body_lines(panel, attr, lines[i]))
            return false;
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        steamdeck ? accept_label : "Any", "Continue");

    if (!ui_information_scene_present_ui(&scene))
        return false;

    (void)ui_information_scene_wait_key_nonrepeat();
    metarun_ui_clear_pending_input();
    return true;
}

static bool metarun_ui_confirm_modal(const char* title, byte title_attr,
    const char* const* lines, const byte* attrs, int line_count, bool steamdeck,
    const char* accept_label, const char* back_label)
{
    while (true)
    {
        app_ui_scene scene;
        app_ui_panel* panel;
        int key;

        panel = metarun_ui_begin_modal_scene(&scene, title_attr, title);
        if (!panel)
            return false;

        for (int i = 0; i < line_count; i++)
        {
            byte attr = attrs ? attrs[i] : TERM_WHITE;

            if (!lines || !lines[i] || !lines[i][0])
                continue;
            if (!metarun_ui_add_wrapped_body_lines(panel, attr, lines[i]))
                return false;
        }

        if (steamdeck)
        {
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                accept_label, "Confirm");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Cancel");
        }
        else
        {
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                "Y/Enter", "Confirm");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "N/Esc", "Cancel");
        }

        if (!ui_information_scene_present_ui(&scene))
            return false;

        key = ui_information_scene_wait_key_nonrepeat();
        if (steamdeck)
        {
            if (key == steamdeck_confirm_key() || key == '\r' || key == '\n'
                || key == '6')
            {
                metarun_ui_clear_pending_input();
                return true;
            }
            if (key == steamdeck_back_key() || key == ESCAPE || key == '4')
            {
                metarun_ui_clear_pending_input();
                return false;
            }
        }
        else
        {
            if (key == 'y' || key == 'Y' || key == '\r' || key == '\n'
                || key == '6')
            {
                metarun_ui_clear_pending_input();
                return true;
            }
            if (key == 'n' || key == 'N' || key == ESCAPE || key == '4'
                || key == 'h' || key == 'H')
            {
                metarun_ui_clear_pending_input();
                return false;
            }
        }
    }
}

static int metarun_ui_choose_curse_scene(int n,
    const int picks[CURSE_MENU_LINES], bool steamdeck,
    const char* accept_label)
{
    int selected = 0;
    char subtitle[APP_UI_TEXT_MAX];

    strnfmt(subtitle, sizeof(subtitle),
        "Dark powers demand their price. Choose %s curse.",
        metarun_curse_choice_label(n));

    while (true)
    {
        app_ui_scene scene;
        app_ui_panel* panel;
        const curse_type* cu;
        int key;

        panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
            "The Valar's Judgment", TERM_SLATE, subtitle);
        if (!panel)
            return -1;

        (void)app_ui_panel_add_body_line(panel, TERM_L_DARK,
            "Choose the curse you must bear.");

        for (int i = 0; i < CURSE_MENU_LINES; i++)
        {
            char key_buf[APP_UI_KEY_MAX];

            strnfmt(key_buf, sizeof(key_buf), "%c", (char)('a' + i));
            if (!app_ui_panel_add_row(panel, picks[i], TERM_L_RED, true,
                    i == selected, key_buf,
                    cu_name + cu_info[picks[i]].name, ""))
            {
                return -1;
            }
        }

        cu = &cu_info[picks[selected]];
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Curse");
        if (!app_ui_panel_add_detail_line(panel, TERM_L_RED,
                cu_name + cu->name))
        {
            return -1;
        }
        if (cu->text
            && !metarun_ui_add_wrapped_detail_lines(panel, TERM_SLATE,
                cu_text + cu->text))
        {
            return -1;
        }

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            steamdeck ? accept_label : "Enter", "Accept");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "8/2", "Move");
        if (!steamdeck) {
            (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "A-C", "Select");
        }

        if (!ui_information_scene_present_ui(&scene))
            return -1;

        key = ui_information_scene_wait_key_nonrepeat();
        if (key == '\r' || key == '\n' || key == ' '
            || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
        {
            metarun_ui_clear_pending_input();
            return picks[selected];
        }
        if (key == '8' || key == 'k' || key == '-') {
            selected = (selected + CURSE_MENU_LINES - 1) % CURSE_MENU_LINES;
            continue;
        }
        if (key == '2' || key == 'j' || key == '+') {
            selected = (selected + 1) % CURSE_MENU_LINES;
            continue;
        }
        if (key == ESCAPE
            || (steamdeck && key == steamdeck_back_key())
            || (!steamdeck && (key == 'h' || key == 'H')))
        {
            metarun_ui_clear_pending_input();
            return picks[0];
        }
        if (key >= 'a' && key < 'a' + CURSE_MENU_LINES) {
            metarun_ui_clear_pending_input();
            return picks[key - 'a'];
        }
        if (key >= 'A' && key < 'A' + CURSE_MENU_LINES) {
            metarun_ui_clear_pending_input();
            return picks[key - 'A'];
        }
    }
}

static bool metarun_build_stats_browser_scene(app_ui_scene* scene,
    const metarun_stats_view_model* view)
{
    app_ui_panel* panel;
    char subtitle[APP_UI_TEXT_MAX];
    char line[APP_UI_TEXT_MAX];
    char meta[APP_UI_META_MAX];
    byte alive_attr;
    byte blessing_attr;
    int active_count = 0;

    if (!scene || !view)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    app_ui_panel_set_title(panel, TERM_YELLOW, "Current Story Statistics");
    strnfmt(subtitle, sizeof(subtitle), "Run-ID %u", metar.id);
    app_ui_panel_set_subtitle(panel, TERM_L_BLUE, subtitle);
    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Blessing Pool");

    alive_attr = (view->alive < view->required_survivors)
        ? TERM_RED
        : TERM_L_GREEN;
    blessing_attr = (view->available_points > 0) ? TERM_L_GREEN : TERM_WHITE;

    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Difficulty",
            TERM_L_BLUE, view->diff_name))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%lu", (unsigned long)metar.score);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Meta Score", TERM_WHITE,
            meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%lu", (unsigned long)view->best_run);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Best Run Score",
            TERM_WHITE, meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%s  %d / %d (remaining %d)", view->sil_bar,
        metar.silmarils, view->win_goal, view->remaining_silmarils);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Silmarils", TERM_WHITE,
            meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%d (need >= %d)", view->alive,
        view->required_survivors);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Living Heroes",
            alive_attr, meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%s (%d total)", view->death_marks,
        metar.deaths);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Deaths", TERM_WHITE,
            meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%d available (%d spent / %d earned)",
        view->available_points, view->spent_points, view->earned_points);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Blessing Points",
            blessing_attr, meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%lu total, %lu / %lu to next",
        (unsigned long)view->total_pool, (unsigned long)view->remainder,
        (unsigned long)view->threshold);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Blessing Pool",
            TERM_WHITE, meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%d unlocked", view->unlocked_major);
    if (!metarun_ui_add_value_row(panel, TERM_YELLOW, "Major Blessings",
            TERM_YELLOW, meta))
    {
        return false;
    }

    if (!metarun_ui_add_section_row(panel, TERM_YELLOW,
            "Active Curses & Blessings"))
    {
        return false;
    }

    for (int id = 0; id < z_info->cu_max; id++)
    {
        if (CURSE_GET(id) == 0)
            continue;

        active_count++;
        if (!metarun_ui_add_effect_row(panel, id))
            return false;
    }

    if (active_count == 0
        && !metarun_ui_add_value_row(panel, TERM_L_DARK, "None active",
            TERM_L_DARK, ""))
    {
        return false;
    }

    strnfmt(line, sizeof(line), "Total score: %lu",
        (unsigned long)view->total_pool);
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
        return false;
    strnfmt(line, sizeof(line), "Next blessing: %lu / %lu",
        (unsigned long)view->remainder, (unsigned long)view->threshold);
    if (!app_ui_panel_add_detail_line(panel, TERM_L_BLUE, line))
        return false;
    strnfmt(line, sizeof(line), "Threshold mode: %s", view->threshold_mode);
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
        return false;
    strnfmt(line, sizeof(line), "Available points: %d", view->available_points);
    if (!app_ui_panel_add_detail_line(panel, blessing_attr, line))
        return false;
    strnfmt(line, sizeof(line), "Earned / spent: %d / %d", view->earned_points,
        view->spent_points);
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
        return false;
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
        return false;
    if (!app_ui_panel_add_detail_line(panel, TERM_YELLOW, "Major Blessings"))
        return false;

    if (view->unlocked_major == 0)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_L_DARK,
                "None unlocked yet"))
        {
            return false;
        }
    }
    else
    {
        for (int i = 0; i < view->major_total; i++)
        {
            const char* desc;
            const char* name;
            char desc_buf[96];

            if (!metarun_has_major_blessing_index(i))
                continue;

            name = major_blessing_name_str(i);
            desc = major_blessing_short_desc(i);
            metarun_trim_first_line(desc_buf, sizeof(desc_buf), desc);
            if (desc_buf[0])
                strnfmt(line, sizeof(line), "%s: %s", name, desc_buf);
            else
                strnfmt(line, sizeof(line), "%s", name);

            if (!app_ui_panel_add_detail_line(panel, TERM_L_GREEN, line))
                return false;
        }
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        view->continue_label, "Continue");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_L_GREEN, true,
        view->spend_label, "Spend");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        view->threshold_label, "Threshold");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        view->diff_label, "Difficulty");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        view->full_label, "Effects");
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
        view->history_label, "History");
    if (view->blitz_enabled)
    {
        (void)app_ui_panel_add_footer_action(panel, 7, TERM_WHITE, true,
            view->blitz_label, "Blitz");
    }
    (void)app_ui_panel_add_footer_action(panel, 8, TERM_WHITE, true,
        view->back_label, "Back");

    return true;
}

static bool metarun_adjust_blessing_threshold_information_scene(
    bool steamdeck, const char* accept_label, const char* back_label)
{
    const metarun_blessing_threshold_mode order[] = {
        METARUN_BLESSING_THRESHOLD_EASIER,
        METARUN_BLESSING_THRESHOLD_NORMAL,
        METARUN_BLESSING_THRESHOLD_HARDER
    };
    const char *labels[] = { "Easier", "Normal", "Harder" };
    const char *descs[] = {
        "If the game feels too hard, use this to earn blessings sooner.",
        "Default level.",
        "Pick this if you want fewer blessings by raising the threshold."
    };
    const int option_count = (int)N_ELEMENTS(order);

    metarun_blessing_threshold_mode current_mode = metarun_get_threshold_mode(&metar);
    int selection = 0;
    for (int i = 0; i < option_count; i++) {
        if (order[i] == current_mode) {
            selection = i;
            break;
        }
    }

    bool accepted = false;
    metarun_blessing_threshold_mode chosen_mode = current_mode;
    bool semantic_ok = true;

    while (true) {
        app_ui_scene scene;
        app_ui_panel *panel;
        char subtitle[APP_UI_TEXT_MAX];
        int key;

        strnfmt(subtitle, sizeof(subtitle), "Current: %s",
            threshold_mode_name(current_mode));
        panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
            "Blessing Threshold", TERM_SLATE, subtitle);
        if (!panel) {
            semantic_ok = false;
            break;
        }

        for (int i = 0; i < option_count; i++) {
            metarun_blessing_threshold_mode mode = order[i];
            u32b mode_threshold = runtype_threshold_for_mode(metar.type, mode);
            byte attr = (mode == METARUN_BLESSING_THRESHOLD_EASIER)
                ? TERM_L_GREEN
                : (mode == METARUN_BLESSING_THRESHOLD_HARDER)
                    ? TERM_ORANGE : TERM_WHITE;
            char key_buf[APP_UI_KEY_MAX];
            char meta[APP_UI_META_MAX];

            strnfmt(key_buf, sizeof(key_buf), "%c", (char)('a' + i));
            strnfmt(meta, sizeof(meta), "%lu pts",
                (unsigned long)mode_threshold);
            if (!app_ui_panel_add_row(panel, i, attr, true,
                    i == selection, key_buf, labels[i], meta))
            {
                semantic_ok = false;
                break;
            }
        }
        if (!semantic_ok)
            break;

        {
            metarun_blessing_threshold_mode mode = order[selection];
            u32b mode_threshold = runtype_threshold_for_mode(metar.type, mode);
            char buf[APP_UI_TEXT_MAX];

            app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
                "Selected Mode");
            if (!app_ui_panel_add_detail_line(panel, TERM_WHITE,
                    labels[selection]))
            {
                semantic_ok = false;
                break;
            }
            strnfmt(buf, sizeof(buf), "Requires %lu points per blessing.",
                (unsigned long)mode_threshold);
            if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, buf))
            {
                semantic_ok = false;
                break;
            }
            if (mode == current_mode
                && !app_ui_panel_add_detail_line(panel, TERM_L_BLUE,
                    "Current setting."))
            {
                semantic_ok = false;
                break;
            }
            if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_SLATE,
                    descs[selection]))
            {
                semantic_ok = false;
                break;
            }
        }

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            steamdeck ? accept_label : "Enter", "Apply");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            steamdeck ? back_label : "Esc", "Cancel");

        if (!ui_information_scene_present_ui(&scene)) {
            semantic_ok = false;
            break;
        }

        key = ui_information_scene_wait_key_nonrepeat();
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key())
            || (!steamdeck && (key == 'h' || key == 'H')))
        {
            metarun_ui_clear_pending_input();
            break;
        }
        if (key == '\r' || key == '\n'
            || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
        {
            accepted = true;
            chosen_mode = order[selection];
            metarun_ui_clear_pending_input();
            break;
        }
        if (key == '8' || key == 'k' || key == '-') {
            selection = (selection + option_count - 1) % option_count;
            continue;
        }
        if (key == '2' || key == 'j' || key == '+') {
            selection = (selection + 1) % option_count;
            continue;
        }
        if (key >= 'a' && key < 'a' + option_count) {
            selection = key - 'a';
            continue;
        }
        if (key >= 'A' && key < 'A' + option_count) {
            selection = key - 'A';
            continue;
        }
    }

    if (accepted && chosen_mode != current_mode) {
        metarun_set_threshold_mode(&metar, chosen_mode);
        update_blessing_ledger(&metar);
        if (!sync_current_metarun_slot(false)) {
            log_warn("Threshold change: unable to sync metarun slot (idx=%d, max=%d)", current_run, metarun_max);
        }
        save_metaruns();
    }

    if (accepted) {
        char line1[APP_UI_TEXT_MAX];
        char line2[APP_UI_TEXT_MAX];
        const char *lines[2];
        byte attrs[2];
        int line_count = 0;

        if (chosen_mode != current_mode) {
            u32b new_threshold = metarun_threshold_value(&metar);
            strnfmt(line1, sizeof(line1), "Blessing threshold set to %s.",
                threshold_mode_name(chosen_mode));
            strnfmt(line2, sizeof(line2),
                "New requirement: %lu points per blessing.",
                (unsigned long)new_threshold);
            lines[line_count] = line1;
            attrs[line_count++] = TERM_L_GREEN;
            lines[line_count] = line2;
            attrs[line_count++] = TERM_WHITE;
        } else {
            lines[line_count] = "Blessing threshold remains unchanged.";
            attrs[line_count++] = TERM_L_DARK;
        }
        if (!metarun_ui_show_notice_modal("Blessing Threshold",
                TERM_YELLOW, lines, attrs, line_count, steamdeck,
                accept_label))
        {
            semantic_ok = false;
        }
    }

    return semantic_ok;
}

void print_metarun_stats(void)
{
    ui_information_scene_scope info_scope;
    app_ui_scene metarun_scene;
    metarun_stats_view_model view;
    bool use_information_scene = ui_information_scene_enter(&info_scope);

    if (!use_information_scene) {
        log_error("print_metarun_stats: semantic scene unavailable");
        return;
    }

metarun_redraw:
    refresh_current_metar_score();

    if (current_run < 0 || current_run >= metarun_max) {
        const char *lines[] = {
            "Error: No metarun data available.",
            "Please start a new game first."
        };
        const byte attrs[] = { TERM_RED, TERM_WHITE };

        (void)metarun_ui_show_notice_modal("Current Story Statistics",
            TERM_YELLOW, lines, attrs, (int)N_ELEMENTS(lines),
            steamdeck_controls_active(), "A");
        ui_information_scene_leave(&info_scope);
        return;
    }

    compute_blessing_pool();
    metarun_sanitize_major_blessing_bits(&metar);
    metarun_stats_prepare_view_model(&view);

    if (!metarun_build_stats_browser_scene(&metarun_scene, &view)
        || !ui_information_scene_present_ui(&metarun_scene))
    {
        log_error("print_metarun_stats: failed to publish semantic scene");
        ui_information_scene_leave(&info_scope);
        return;
    }

    {
        int key = ui_information_scene_wait_key_nonrepeat();
        bool steamdeck = view.steamdeck;

        if (steamdeck) {
            int back_key = steamdeck_back_key();
            int confirm_key = steamdeck_confirm_key();
            int alt_key = steamdeck_alt_action_key();
            int secondary_key = steamdeck_secondary_key();

            if (key == back_key || key == confirm_key || key == '\r'
                || key == '\n')
            {
                ui_information_scene_leave(&info_scope);
                return;
            } else if (key == alt_key) {
                key = 'b';
            } else if (key == secondary_key) {
                key = 's';
            }
        }

        if (key == 'b' || key == 'B') {
            if (!open_blessing_exchange_information_scene(view.steamdeck,
                    view.continue_label, view.back_label))
            {
                log_error("print_metarun_stats: blessing exchange scene failed");
            }
            goto metarun_redraw;
        } else if (key == 'c' || key == 'C') {
            choose_difficulty_menu(false);
            goto metarun_redraw;
        } else if (key == 'f' || key == 'F') {
            if (!metarun_adjust_blessing_threshold_information_scene(
                    view.steamdeck, view.continue_label, view.back_label))
            {
                log_error("print_metarun_stats: threshold scene failed");
            }
            goto metarun_redraw;
        } else if (key == 'u' || key == 'U') {
            if (!metarun_show_active_effects_information_scene(
                    view.steamdeck, view.continue_label, view.back_label))
            {
                log_error("print_metarun_stats: active effects scene failed");
            }
            goto metarun_redraw;
        } else if (key == 's' || key == 'S') {
            if (!metarun_list_history_information_scene(view.steamdeck,
                    view.continue_label, view.back_label))
            {
                log_error("print_metarun_stats: history scene failed");
            }
            goto metarun_redraw;
        } else if (key == 't' || key == 'T') {
            if (!metarun_show_completed_quests_information_scene(
                    view.steamdeck, view.continue_label, view.back_label))
            {
                log_error("print_metarun_stats: completed quests scene failed");
            }
            goto metarun_redraw;
        } else if ((key == 'x' || key == 'X') && view.blitz_enabled) {
            ui_information_scene_leave(&info_scope);
            run_mode_set_pending(RUN_MODE_BLITZ);
            run_mode_set_current(RUN_MODE_BLITZ);
            return;
        }
    }

    ui_information_scene_leave(&info_scope);
}

/* Generate curse description for a runtype */
static void get_curse_description(int runtype_id, char *buf, size_t buf_size)
{
    if (!runtype_info || runtype_id >= z_info->rt_max || buf_size < 64)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }
    
    runtype_type *rt = &runtype_info[runtype_id];
    
    if (!rt->start_curses)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }
    
    /* Count curses and determine stack ranges */
    int curse_count = 0;
    int min_stacks = 255, max_stacks = 0;
    
    int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
    for (int curse_id = 0; curse_id < limit; curse_id++)
    {
        if (rt->start_curses & (1ULL << curse_id))
        {
            curse_count++;
            int stacks = rt->curse_stacks[curse_id];
            if (stacks < min_stacks) min_stacks = stacks;
            if (stacks > max_stacks) max_stacks = stacks;
        }
    }
    
    if (curse_count == 0)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }
    
    /* Format the description */
    if (min_stacks == max_stacks)
    {
        if (min_stacks == 1)
            snprintf(buf, buf_size, "Curses: %d x %d stack", curse_count, min_stacks);
        else
            snprintf(buf, buf_size, "Curses: %d x %d stacks", curse_count, min_stacks);
    }
    else
    {
        snprintf(buf, buf_size, "Curses: %d (%d-%d stacks)", curse_count, min_stacks, max_stacks);
    }
}

/* Difficulty selection menu */
static void choose_difficulty_menu(bool reopen_stats_on_exit)
{
    int choice = metar.type;
    int max_difficulty = (runtype_info && z_info->rt_max > 0)
        ? z_info->rt_max - 1 : 0;
    ui_information_scene_scope info_scope;
    bool use_information_scene = ui_information_scene_enter(&info_scope);
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    char back_label[16] = "";
    char status_msg[APP_UI_TEXT_MAX] = "";
    byte status_attr = TERM_WHITE;

    if (steamdeck) {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
    }
    if (!use_information_scene) {
        log_error("choose_difficulty_menu: semantic scene unavailable");
        if (reopen_stats_on_exit)
            print_metarun_stats();
        return;
    }

    while (true)
    {
        app_ui_scene scene;
        app_ui_panel *panel;
        char subtitle[APP_UI_TEXT_MAX];
        int key;

        strnfmt(subtitle, sizeof(subtitle), "Current difficulty: %s",
            (runtype_info && metar.type < z_info->rt_max
                && runtype_info[metar.type].name[0])
                ? runtype_info[metar.type].name : "Unknown");
        panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
            "Select Difficulty Level", TERM_SLATE, subtitle);
        if (!panel) {
            log_error("choose_difficulty_menu: failed to build semantic scene");
            break;
        }

        for (int i = 0; i <= max_difficulty; i++)
        {
            bool is_locked = (i < metar.max_difficulty_reached);
            byte attr = TERM_WHITE;
            char key_buf[APP_UI_KEY_MAX];
            char meta[APP_UI_META_MAX];
            int win_goal = WINCON_SILMARILS;
            u32b blessing_thresh = runtype_threshold_for_mode(i,
                METARUN_BLESSING_THRESHOLD_NORMAL);
            char curse_buf[64];
            const char *rt_name = "Unknown";

            if (runtype_info && i < z_info->rt_max && runtype_info[i].name[0]) {
                rt_name = runtype_info[i].name;
                win_goal = runtype_info[i].win_con
                    ? runtype_info[i].win_con : WINCON_SILMARILS;
                attr = runtype_info[i].colour;
            }
            if (is_locked)
                attr = TERM_L_DARK;

            get_curse_description(i, curse_buf, sizeof(curse_buf));
            strnfmt(key_buf, sizeof(key_buf), "%c", (char)('a' + i));
            strnfmt(meta, sizeof(meta), "Win %d  Threshold %lu  %s",
                win_goal, (unsigned long)blessing_thresh, curse_buf);
            if (!app_ui_panel_add_row(panel, i, attr, !is_locked,
                    i == choice, key_buf, rt_name, meta))
            {
                log_error("choose_difficulty_menu: failed to append row");
                ui_information_scene_leave(&info_scope);
                if (reopen_stats_on_exit)
                    print_metarun_stats();
                return;
            }
        }

        {
            bool is_locked = (choice < metar.max_difficulty_reached);
            int win_goal = WINCON_SILMARILS;
            u32b blessing_thresh = runtype_threshold_for_mode(choice,
                METARUN_BLESSING_THRESHOLD_NORMAL);
            char curse_buf[64];
            char line[APP_UI_TEXT_MAX];
            const char *rt_name = "Unknown";

            if (runtype_info && choice < z_info->rt_max
                && runtype_info[choice].name[0])
            {
                rt_name = runtype_info[choice].name;
                win_goal = runtype_info[choice].win_con
                    ? runtype_info[choice].win_con : WINCON_SILMARILS;
            }

            get_curse_description(choice, curse_buf, sizeof(curse_buf));
            app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
                "Selected Difficulty");
            (void)app_ui_panel_add_detail_line(panel,
                is_locked ? TERM_L_DARK : TERM_WHITE, rt_name);
            strnfmt(line, sizeof(line), "Win condition: %d Silmarils", win_goal);
            (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, line);
            strnfmt(line, sizeof(line), "Threshold: %lu points",
                (unsigned long)blessing_thresh);
            (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, line);
            (void)app_ui_panel_add_detail_line(panel,
                is_locked ? TERM_L_DARK : TERM_SLATE, curse_buf);
            if (status_msg[0] != '\0')
                (void)app_ui_panel_add_detail_line(panel, status_attr,
                    status_msg);
        }

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            steamdeck ? accept_label : "Enter", "Accept");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            steamdeck ? back_label : "Esc", "Cancel");

        if (!ui_information_scene_present_ui(&scene)) {
            log_error("choose_difficulty_menu: failed to publish semantic scene");
            break;
        }

        key = ui_information_scene_wait_key_nonrepeat();
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key())
            || (!steamdeck && (key == 'h' || key == 'H')))
        {
            ui_information_scene_leave(&info_scope);
            if (reopen_stats_on_exit)
                print_metarun_stats();
            return;
        }
        if (key == '\r' || key == '\n'
            || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
        {
            if (choice < metar.max_difficulty_reached) {
                SDL_strlcpy(status_msg,
                    "Cannot select easier difficulty - locked for this story run!",
                    sizeof(status_msg));
                status_attr = TERM_RED;
                continue;
            }
            break;
        }
        if (key == '8' || key == 'k' || key == '-') {
            int new_choice = choice - 1;
            while (new_choice >= 0 && new_choice < metar.max_difficulty_reached)
                new_choice--;
            if (new_choice >= 0)
                choice = new_choice;
            status_msg[0] = '\0';
            continue;
        }
        if (key == '2' || key == 'j' || key == '+') {
            if (choice < max_difficulty)
                choice++;
            status_msg[0] = '\0';
            continue;
        }
        if (key >= 'a' && key <= 'z') {
            int new_choice = key - 'a';
            if (new_choice <= max_difficulty) {
                if (new_choice < metar.max_difficulty_reached) {
                    SDL_strlcpy(status_msg,
                        "Cannot select easier difficulty - locked for this story run!",
                        sizeof(status_msg));
                    status_attr = TERM_RED;
                } else {
                    choice = new_choice;
                    status_msg[0] = '\0';
                }
            }
            continue;
        }
        if (key >= 'A' && key <= 'Z') {
            int new_choice = key - 'A';
            if (new_choice <= max_difficulty) {
                if (new_choice < metar.max_difficulty_reached) {
                    SDL_strlcpy(status_msg,
                        "Cannot select easier difficulty - locked for this story run!",
                        sizeof(status_msg));
                    status_attr = TERM_RED;
                } else {
                    choice = new_choice;
                    status_msg[0] = '\0';
                }
            }
        }
    }

    if (choice != metar.type)
    {
        if (choice > metar.type) {
            const char *lines[] = {
                "If you increase the difficulty level, you will not be able to return to an easier level for the rest of this story run.",
                "This change is permanent for this meta-run."
            };
            const byte attrs[] = { TERM_WHITE, TERM_L_RED };
            bool confirm = metarun_ui_confirm_modal("Increase Difficulty",
                TERM_YELLOW, lines, attrs, (int)N_ELEMENTS(lines), steamdeck,
                accept_label, back_label);

            if (!confirm) {
                ui_information_scene_leave(&info_scope);
                if (reopen_stats_on_exit)
                    print_metarun_stats();
                return;
            }
        }

        log_info("Changing difficulty from %d to %d", metar.type, choice);

        {
            int8_t preserved_stacks[METAR_CURSE_SLOTS];
            u64b preserved_seen = metar.curses_seen;

            memcpy(preserved_stacks, metar.curse_stacks,
                sizeof(preserved_stacks));
            memset(metar.curse_stacks, 0, sizeof(metar.curse_stacks));
            metar.curses_seen = 0;
            metar.type = (byte)choice;
            apply_difficulty_curses(&metar);

            {
                int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
                for (int curse_id = 0; curse_id < limit; curse_id++) {
                    int preserved = preserved_stacks[curse_id];
                    int combined;
                    int curse_cap;
                    int blessing_cap;

                    if (!preserved) continue;
                    combined = preserved + CURSE_GET(curse_id);
                    curse_cap = CURSE_CURSE_CAP(curse_id);
                    blessing_cap = CURSE_BLESSING_CAP(curse_id);
                    if (curse_cap > 0 && combined > curse_cap)
                        combined = curse_cap;
                    if (blessing_cap > 0 && combined < -blessing_cap)
                        combined = -blessing_cap;
                    CURSE_SET(curse_id, combined);
                }
            }

            if (choice > metar.max_difficulty_reached)
                metar.max_difficulty_reached = (byte)choice;
            metar.curses_seen |= preserved_seen;
        }

        if (!sync_current_metarun_slot(false))
            log_warn("Difficulty change failed to sync metarun slot");
        save_metaruns();

        {
            const char *new_name = "Unknown";
            if (runtype_info && choice < z_info->rt_max
                && runtype_info[choice].name[0])
            {
                new_name = runtype_info[choice].name;
            }
            msg_print(format("Difficulty changed to: %s", new_name));
        }
    }

    ui_information_scene_leave(&info_scope);
    if (reopen_stats_on_exit)
        print_metarun_stats();
}

/* compact table of all meta-runs */
static bool metarun_list_history_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label)
{
    refresh_current_metar_score();

    if (metarun_max > 0 && metaruns) {
        for (s16b i = 0; i < metarun_max; i++) {
            metaruns[i].score = compute_metarun_score(&metaruns[i]);
        }
    }

    s16b *order = NULL;
    if (metarun_max > 0 && metaruns) {
        order = mem_alloc_array(metarun_max, s16b);
        for (s16b i = 0; i < metarun_max; i++) order[i] = i;
        qsort(order, metarun_max, sizeof(s16b), compare_metarun_indices);
    }

    {
        bool semantic_ok = true;
        int highlight = 0;

        for (s16b i = 0; i < metarun_max; i++) {
            s16b idx = order ? order[i] : i;
            if (idx == current_run) {
                highlight = i;
                break;
            }
        }

        if (metarun_max <= 0 || !metaruns) {
            const char *lines[] = { "No metaruns have been recorded yet." };
            const byte attrs[] = { TERM_L_DARK };
            semantic_ok = metarun_ui_show_notice_modal("Meta-run History",
                TERM_L_GREEN, lines, attrs, (int)N_ELEMENTS(lines), steamdeck,
                accept_label);
            order = mem_free(order);
            return semantic_ok;
        }

        while (true) {
            app_ui_scene scene;
            app_ui_panel *panel;
            int page_start;
            int page_end;
            char subtitle[APP_UI_TEXT_MAX];
            int key;

            if (highlight < 0)
                highlight = 0;
            if (highlight >= metarun_max)
                highlight = metarun_max - 1;

            page_start = (highlight / METARUN_HISTORY_PAGE_SIZE)
                * METARUN_HISTORY_PAGE_SIZE;
            page_end = MIN(metarun_max, page_start + METARUN_HISTORY_PAGE_SIZE);

            strnfmt(subtitle, sizeof(subtitle), "%d-%d of %d",
                page_start + 1, page_end, metarun_max);
            panel = metarun_ui_begin_browser_scene(&scene, TERM_L_GREEN,
                "Meta-run History", TERM_SLATE, subtitle);
            if (!panel) {
                semantic_ok = false;
                break;
            }

            for (int i = page_start; i < page_end; i++) {
                s16b idx = order ? order[i] : i;
                const metarun *m = &metaruns[idx];
                int win_goal = WINCON_SILMARILS;
                int death_limit = LOSECON_DEATHS;
                char label[APP_UI_LABEL_MAX];
                char meta[APP_UI_META_MAX];
                char date[16];
                char res;
                byte attr;

                if (runtype_info && m->type < z_info->rt_max)
                {
                    win_goal = runtype_info[m->type].win_con
                        ? runtype_info[m->type].win_con : WINCON_SILMARILS;
                }

                res = (m->silmarils >= win_goal) ? 'W'
                    : (m->deaths >= death_limit) ? 'L' : ' ';
                strftime(date, sizeof date, "%Y-%m-%d",
                    localtime((time_t*)&m->last_played));
                attr = (idx == current_run) ? TERM_YELLOW : TERM_WHITE;
                strnfmt(label, sizeof(label), "%c%08u",
                    (idx == current_run) ? '*' : ' ', (unsigned)m->id);
                strnfmt(meta, sizeof(meta), "%lu  S:%d D:%d %c  %s",
                    (unsigned long)m->score, m->silmarils, m->deaths, res,
                    date);
                if (!app_ui_panel_add_row(panel, idx, attr, true,
                        i == highlight, "", label, meta))
                {
                    semantic_ok = false;
                    break;
                }
            }
            if (!semantic_ok)
                break;

            if (highlight >= page_start && highlight < page_end) {
                s16b idx = order ? order[highlight] : highlight;
                const metarun *m = &metaruns[idx];
                const char *diff_name = "Unknown";
                int win_goal = WINCON_SILMARILS;
                int death_limit = LOSECON_DEATHS;
                char line[APP_UI_TEXT_MAX];
                char date[16];
                char res;

                if (runtype_info && m->type < z_info->rt_max) {
                    if (runtype_info[m->type].name[0])
                        diff_name = runtype_info[m->type].name;
                    win_goal = runtype_info[m->type].win_con
                        ? runtype_info[m->type].win_con : WINCON_SILMARILS;
                }
                res = (m->silmarils >= win_goal) ? 'W'
                    : (m->deaths >= death_limit) ? 'L' : ' ';
                strftime(date, sizeof date, "%Y-%m-%d",
                    localtime((time_t*)&m->last_played));

                app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Run");
                strnfmt(line, sizeof(line), "Run %08u", (unsigned)m->id);
                if (!app_ui_panel_add_detail_line(panel,
                        (idx == current_run) ? TERM_YELLOW : TERM_WHITE, line))
                {
                    semantic_ok = false;
                    break;
                }
                strnfmt(line, sizeof(line), "Difficulty: %s", diff_name);
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
                {
                    semantic_ok = false;
                    break;
                }
                strnfmt(line, sizeof(line), "Score: %lu",
                    (unsigned long)m->score);
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
                {
                    semantic_ok = false;
                    break;
                }
                strnfmt(line, sizeof(line), "Silmarils / Deaths: %d / %d",
                    m->silmarils, m->deaths);
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
                {
                    semantic_ok = false;
                    break;
                }
                strnfmt(line, sizeof(line), "Result: %c", res ? res : ' ');
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
                {
                    semantic_ok = false;
                    break;
                }
                strnfmt(line, sizeof(line), "Last played: %s", date);
                if (!app_ui_panel_add_detail_line(panel, TERM_SLATE, line))
                {
                    semantic_ok = false;
                    break;
                }
            }

            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                steamdeck ? accept_label : "Enter", "Close");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "8/2", "Move");
            if (metarun_max > METARUN_HISTORY_PAGE_SIZE) {
                (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                    "4/6", "Page");
            }
            if (steamdeck) {
                (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                    back_label, "Back");
            }

            if (!ui_information_scene_present_ui(&scene)) {
                semantic_ok = false;
                break;
            }

            key = ui_information_scene_wait_key_nonrepeat();
            if (key == '8' || key == 'k' || key == '-') {
                highlight = (highlight + metarun_max - 1) % metarun_max;
                continue;
            }
            if (key == '2' || key == 'j' || key == '+') {
                highlight = (highlight + 1) % metarun_max;
                continue;
            }
            if (metarun_max > METARUN_HISTORY_PAGE_SIZE && key == '4') {
                highlight -= METARUN_HISTORY_PAGE_SIZE;
                if (highlight < 0)
                    highlight = 0;
                continue;
            }
            if (metarun_max > METARUN_HISTORY_PAGE_SIZE && key == '6') {
                highlight += METARUN_HISTORY_PAGE_SIZE;
                if (highlight >= metarun_max)
                    highlight = metarun_max - 1;
                continue;
            }
            metarun_ui_clear_pending_input();
            break;
        }

        order = mem_free(order);
        return semantic_ok;
    }
}

void list_metaruns(void)
{
    ui_information_scene_scope info_scope;
    bool use_information_scene = ui_information_scene_enter(&info_scope);
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=ok */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
    }
    if (!use_information_scene) {
        log_error("list_metaruns: semantic scene unavailable");
        return;
    }

    if (!metarun_list_history_information_scene(steamdeck, accept_label,
            back_label))
    {
        log_error("list_metaruns: failed to publish semantic scene");
    }

    ui_information_scene_leave(&info_scope);
}

void show_known_curses_menu(void)
{
    ui_information_scene_scope info_scope;
    bool use_information_scene;
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck)
    {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
    }

    use_information_scene = ui_information_scene_enter(&info_scope);
    if (!use_information_scene)
    {
        log_error("show_known_curses_menu: semantic scene unavailable");
        return;
    }

    if (!metarun_show_known_curses_information_scene(steamdeck, accept_label,
            back_label))
    {
        log_error("show_known_curses_menu: failed to publish semantic scene");
    }

    ui_information_scene_leave(&info_scope);
}

/* Public wrapper for difficulty selection menu */
void choose_difficulty_level(void)
{
    choose_difficulty_menu(false);
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

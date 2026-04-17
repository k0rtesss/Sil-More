/* File: level-generation-quests.c */

/*
 * Level-generation quest integration:
 * - quest roulette
 * - deferred quest state application during regeneration attempts
 */

#include "angband.h"
#include "log/log.h"
#include "level-generation/gen-log.h"
#include "metarun.h"
#include "level-generation/level-generation-internal.h"
#include <string.h>

/* Quest vault debug instrumentation */
#define DEBUG_QUEST_VAULT 0
#if DEBUG_QUEST_VAULT
static int qv_y1 = -1, qv_x1 = -1, qv_y2 = -1, qv_x2 = -1;
static int qv_h = 0, qv_w = 0;
static unsigned short *qv_feat_snapshot = NULL;

static void qv_capture(void)
{
    int y, x;

    if (qv_y1 < 0)
        return;

    qv_h = qv_y2 - qv_y1 + 1;
    qv_w = qv_x2 - qv_x1 + 1;
    mem_free_null(qv_feat_snapshot);
    qv_feat_snapshot = mem_alloc_array(qv_h * qv_w, unsigned short);
    for (y = qv_y1; y <= qv_y2; ++y)
    {
        for (x = qv_x1; x <= qv_x2; ++x)
        {
            qv_feat_snapshot[(y - qv_y1) * qv_w + (x - qv_x1)] =
                cave_feat[y][x];
        }
    }
    log_trace("Quest vault DEBUG: snapshot captured (%d x %d) bounds (%d,%d)-(%d,%d)",
        qv_h, qv_w, qv_y1, qv_x1, qv_y2, qv_x2);
}

static char qv_glyph(int feat)
{
    switch (feat)
    {
    case FEAT_FLOOR:
        return '.';
    case FEAT_WALL_OUTER:
        return '#';
    case FEAT_WALL_INNER:
        return '+';
    case FEAT_WALL_EXTRA:
        return 'X';
#ifdef FEAT_DOOR_CLOSED
    case FEAT_DOOR_CLOSED:
        return 'D';
#endif
    case FEAT_FORGE_HEAD:
    case FEAT_FORGE_TAIL:
        return 'F';
    default:
        return '?';
    }
}

static void qv_dump(const char *phase)
{
    int y, x;
    char row[256];

    if (qv_y1 < 0)
        return;

    log_trace("Quest vault DEBUG: layout (%s) bounds (%d,%d)-(%d,%d)", phase,
        qv_y1, qv_x1, qv_y2, qv_x2);
    for (y = qv_y1; y <= qv_y2; ++y)
    {
        int idx = 0;

        for (x = qv_x1; x <= qv_x2 && idx < (int)sizeof(row) - 2; ++x)
            row[idx++] = qv_glyph(cave_feat[y][x]);
        row[idx] = '\0';
        log_trace("Quest vault DEBUG ROW %2d: %s", y, row);
    }
}

static void qv_compare(void)
{
    int diffs = 0;
    int y, x;

    if (!qv_feat_snapshot)
        return;

    for (y = qv_y1; y <= qv_y2; ++y)
    {
        for (x = qv_x1; x <= qv_x2; ++x)
        {
            unsigned short before =
                qv_feat_snapshot[(y - qv_y1) * qv_w + (x - qv_x1)];
            unsigned short now = cave_feat[y][x];

            if (before != now)
            {
                log_trace("Quest vault DEBUG: tile changed (%d,%d) %d->%d",
                    y, x, before, now);
                if (++diffs >= 50)
                    return;
            }
        }
    }

    log_trace("Quest vault DEBUG: no tile changes detected since snapshot");
}
#endif

/* Global variable to track pending quest state changes */
pending_quest_states_t pending_quest_states = {0};

/* Quest lottery system: determines which quest (if any) "wins" this level */
int quest_lottery_winner = 0; /* 0=none, quest_id=winner (1=Tulkas, 4=Niena, etc.) */
static bool quest_lottery_resolved = false; /* true once lottery is run for this level */

/* Function to run the quest lottery once per level - determines which quest (if any) gets this level */
/* Roulette quest registry entry */
typedef struct {
    int quest_id;           /* Quest index from quest.txt */
    byte* quest_state_ptr;  /* Pointer to quest state variable */
    u32b metarun_quest_id;  /* Metarun completion tracking ID */
    bool (*eligibility_check)(int depth, int quest_id); /* Custom eligibility function */
    bool (*probability_roll)(int depth, int quest_id);  /* Custom probability function */
} roulette_quest_entry;

/* Forward declarations for quest-specific functions */
static bool data_driven_eligibility_check(int depth, int quest_id);
static bool tulkas_probability_roll(int depth, int quest_id);
static bool niena_probability_roll(int depth, int quest_id);


/* Generic parametric formula-based functions */
static bool generic_eligibility_check(int depth, int quest_id);
static bool generic_probability_roll(int depth, int quest_id);

/* Parametric formula calculation */
static float calculate_parametric_probability(quest_type* q_ptr, int depth);

/* Determine if metarun history blocks a quest (unless oath override applies) */
bool quest_metarun_blocked(int quest_id, u32b metarun_flag)
{
    if (!metarun_flag) return false;

    int completion_count = metarun_quest_completion_count(metarun_flag);
    int completion_cap = quest_completion_cap(quest_id);
    if (completion_cap < 1) completion_cap = METARUN_QUEST_COMPLETION_CAP;
    quest_type* q_ptr = (quest_id > 0 && quest_id < z_info->quest_max) ? &quest_info[quest_id] : NULL;
    byte oath_id = q_ptr ? q_ptr->oath_id : 0;
    bool oath_override = false;

    if (oath_id > 0 && p_ptr && p_ptr->oath_type == oath_id && !oath_invalid(oath_id)) {
        oath_override = true;
    }

    if (completion_count >= completion_cap) {
        log_trace("Quest %d blocked by metarun cap (%d/%d)", quest_id, completion_count, completion_cap);
        return true;
    }

    if (completion_count > 0 && !oath_override) {
        log_trace("Quest %d blocked by prior completion (%d) without oath override", quest_id, completion_count);
        return true;
    }
    if (completion_count > 0 && oath_override) {
        log_trace("Quest %d eligible again: %d prior completion(s) but oath %d is active", quest_id, completion_count, oath_id);
    }

    return false;
}

bool mandos_second_stage_ready(void)
{
    int mandos_cap = quest_completion_cap(QUEST_ID_MANDOS);
    int completion_count = metarun_quest_completion_count(METARUN_QUEST_MANDOS);
    int traitor_cap = quest_completion_cap(QUEST_ID_MANDOS_TRAITOR);
    int traitor_count = metarun_quest_completion_count(METARUN_QUEST_MANDOS_TRAITOR);
    quest_type* mandos_q = (QUEST_ID_MANDOS > 0 && QUEST_ID_MANDOS < z_info->quest_max) ? &quest_info[QUEST_ID_MANDOS] : NULL;
    byte mandos_oath = mandos_q ? mandos_q->oath_id : 0;
    bool oath_active = (mandos_oath > 0 && p_ptr && p_ptr->oath_type == mandos_oath && !oath_invalid(mandos_oath));

    if (mandos_cap < 1) mandos_cap = METARUN_QUEST_COMPLETION_CAP;
    if (traitor_cap < 1) traitor_cap = METARUN_QUEST_COMPLETION_CAP;

    if (quest_get_state(QUEST_ID_MANDOS_TRAITOR) >= QUEST_STATE_REWARDED) return false;
    if (!oath_active) return false;
    if (completion_count < 1) return false;
    if (completion_count >= mandos_cap) return false;
    if (traitor_count >= traitor_cap) return false;

    return true;
}

bool mandos_third_stage_ready(void)
{
    int traitor_count = metarun_quest_completion_count(METARUN_QUEST_MANDOS_TRAITOR);
    int third_cap = quest_completion_cap(QUEST_ID_MANDOS_BETRAYER);
    int third_count = metarun_quest_completion_count(METARUN_QUEST_MANDOS_BETRAYER);
    quest_type* third_q = (QUEST_ID_MANDOS_BETRAYER > 0 && QUEST_ID_MANDOS_BETRAYER < z_info->quest_max) ? &quest_info[QUEST_ID_MANDOS_BETRAYER] : NULL;
    byte mandos_oath = third_q ? third_q->oath_id : 0;
    bool oath_active = (mandos_oath > 0 && p_ptr && p_ptr->oath_type == mandos_oath && !oath_invalid(mandos_oath));

    if (third_cap < 1) third_cap = METARUN_QUEST_COMPLETION_CAP;

    if (quest_get_state(QUEST_ID_MANDOS_BETRAYER) >= QUEST_STATE_REWARDED) return false;
    if (!oath_active) return false;
    if (traitor_count < 1) return false;
    if (third_count >= third_cap) return false;
    if (metarun_challenge_completion_count(CHALLENGE_DISCONNECTED) <= 0) return false;

    return true;
}

bool is_easterling_quest_vault(vault_type *v)
{
    if (!v) return false;
    return (strstr(v_name + v->name, "Easterling Fortress") != NULL);
}

bool is_maeglin_quest_vault(vault_type *v)
{
    if (!v) return false;
    return (strstr(v_name + v->name, "Maeglin") != NULL);
}

static bool vault_template_has_aule(vault_type *v)
{
    char *s;

    if (!v || v->text == 0 || v->hgt == 0)
        return false;

    s = v_text + v->text;
    for (int row = 0; row < v->hgt; ++row)
    {
        if (strchr(s, 'L'))
            return true;
        s += strlen(s) + 1;
    }

    return false;
}

static bool vault_template_has_mandos(vault_type *v)
{
    char *s;

    if (!v || v->text == 0 || v->hgt == 0)
        return false;

    s = v_text + v->text;
    for (int row = 0; row < v->hgt; ++row)
    {
        if (strchr(s, 'N'))
            return true;
        s += strlen(s) + 1;
    }

    return false;
}

static bool vault_template_has_duruin(vault_type *v)
{
    const char *name;

    if (!v)
        return false;

    name = v_name + v->name;
    return strstr(name, "Duruin") != NULL;
}

static bool vault_template_has_shadow_bastion(vault_type *v)
{
    if (!v)
        return false;

    return strstr(v_name + v->name, "Shadow Bastion") != NULL;
}

static bool vault_template_is_orc_stronghold(vault_type *v)
{
    if (!v)
        return false;

    return strstr(v_name + v->name, "Orc Stronghold") != NULL;
}

int qv_stored_y1 = -1;
int qv_stored_x1 = -1;
int qv_stored_y2 = -1;
int qv_stored_x2 = -1;
bool qv_placed_this_level = false;

void check_quest_vault_integrity(const char* checkpoint_name)
{
    int check_walls = 0;
    int check_floors = 0;
    int check_features = 0;
    int check_monsters = 0;
    int check_icky = 0;
    int check_room = 0;
    int check_extra = 0;

    if (!qv_placed_this_level)
    {
        log_trace("VAULT INTEGRITY CHECK [%s]: No quest vault placed this level - skipping check",
            checkpoint_name);
        return;
    }
    if (qv_stored_y1 < 0 || qv_stored_y2 < 0)
    {
        log_trace("VAULT INTEGRITY CHECK [%s]: No quest vault coordinates stored",
            checkpoint_name);
        return;
    }

    for (int cy = qv_stored_y1; cy <= qv_stored_y2; cy++)
    {
        for (int cx = qv_stored_x1; cx <= qv_stored_x2; cx++)
        {
            if (cave_feat[cy][cx] == FEAT_WALL_OUTER
                || cave_feat[cy][cx] == FEAT_WALL_INNER)
            {
                check_walls++;
            }
            else if (cave_feat[cy][cx] == FEAT_FLOOR)
            {
                check_floors++;
            }
            else if (cave_feat[cy][cx] == FEAT_WALL_EXTRA)
            {
                check_extra++;
            }
            else
            {
                check_features++;
            }

            if (cave_m_idx[cy][cx] > 0)
                check_monsters++;
            if (cave_info[cy][cx] & CAVE_ICKY)
                check_icky++;
            if (cave_info[cy][cx] & CAVE_ROOM)
                check_room++;
        }
    }

    log_trace("VAULT INTEGRITY CHECK [%s]: Area (%d,%d) to (%d,%d)",
        checkpoint_name, qv_stored_y1, qv_stored_x1, qv_stored_y2,
        qv_stored_x2);
    log_trace("VAULT INTEGRITY CHECK [%s]: %d walls, %d floors, %d features, %d monsters, %d extra_walls",
        checkpoint_name, check_walls, check_floors, check_features,
        check_monsters, check_extra);
    log_trace("VAULT INTEGRITY CHECK [%s]: %d CAVE_ICKY, %d CAVE_ROOM flags",
        checkpoint_name, check_icky, check_room);

    if (check_walls < 50 && check_floors < 30)
    {
        log_trace("VAULT INTEGRITY WARNING [%s]: Vault appears to have been OVERWRITTEN! Very low content.",
            checkpoint_name);
    }
}

static void process_quest_vault_area(int y0, int x0, vault_type *qv)
{
    int y1 = y0 - qv->hgt / 2;
    int x1 = x0 - qv->wid / 2;
    int y2 = y1 + qv->hgt - 1;
    int x2 = x1 + qv->wid - 1;
    bool has_forge = false;
    bool has_aule = false;
    bool has_mandos = false;
    int wall_count = 0;
    int floor_count = 0;
    int monster_count = 0;
    int feature_count = 0;

    log_trace("Quest vault processing: Area (%d,%d) to (%d,%d), size %dx%d",
        y1, x1, y2, x2, qv->wid, qv->hgt);

    for (int dy = y1; dy <= y2; ++dy)
    {
        for (int dx = x1; dx <= x2; ++dx)
        {
            if (cave_feat[dy][dx] == FEAT_WALL_OUTER
                || cave_feat[dy][dx] == FEAT_WALL_INNER)
            {
                wall_count++;
            }
            else if (cave_feat[dy][dx] == FEAT_FLOOR)
            {
                floor_count++;
            }
            else if (cave_feat[dy][dx] != FEAT_WALL_EXTRA)
            {
                feature_count++;
            }

            if (cave_m_idx[dy][dx] > 0)
                monster_count++;

            if ((cave_feat[dy][dx] >= FEAT_FORGE_HEAD)
                && (cave_feat[dy][dx] <= FEAT_FORGE_TAIL))
            {
                if (!has_forge)
                {
                    p_ptr->aule_forge_y = (byte)dy;
                    p_ptr->aule_forge_x = (byte)dx;
                    has_forge = true;
                    log_trace("Quest vault: Found forge at (%d,%d), feature=%d",
                        dy, dx, cave_feat[dy][dx]);
                }
            }
            if (cave_m_idx[dy][dx] > 0)
            {
                monster_type *m_ptr = &mon_list[cave_m_idx[dy][dx]];

                if (m_ptr->r_idx == R_IDX_AULE)
                {
                    has_aule = true;
                    log_trace("Quest vault: Found Aule at (%d,%d)", dy, dx);
                }
                if (m_ptr->r_idx == R_IDX_MANDOS)
                {
                    has_mandos = true;
                    p_ptr->mandos_vault_y = (byte)dy;
                    p_ptr->mandos_vault_x = (byte)dx;
                    log_trace("Quest vault: Found Mandos at (%d,%d)", dy, dx);
                }
            }
        }
    }

    log_trace("Quest vault contents: %d walls, %d floors, %d features, %d monsters",
        wall_count, floor_count, feature_count, monster_count);

    qv_stored_y1 = y1;
    qv_stored_x1 = x1;
    qv_stored_y2 = y2;
    qv_stored_x2 = x2;
    qv_placed_this_level = true;
    log_trace("QUEST VAULT MONITOR: Storing bounds (%d,%d) to (%d,%d) for tracking",
        qv_stored_y1, qv_stored_x1, qv_stored_y2, qv_stored_x2);

#if DEBUG_QUEST_VAULT
    qv_y1 = y1;
    qv_x1 = x1;
    qv_y2 = y2;
    qv_x2 = x2;
    qv_capture();
    qv_dump("initial");
    for (int ry = y1; ry <= y2; ++ry)
    {
        for (int rx = x1; rx <= x2; ++rx)
            cave_info[ry][rx] |= (CAVE_MARK | CAVE_SEEN | CAVE_GLOW);
    }
#endif

    if (has_forge && has_aule
        && p_ptr->aule_quest == AULE_QUEST_NOT_STARTED
        && !quest_metarun_blocked(QUEST_ID_AULE, METARUN_QUEST_AULE)
        && !p_ptr->quest_reserved[0])
    {
        p_ptr->quest_reserved[0] = 1;
        pending_quest_states.has_aule_change = true;
        pending_quest_states.aule_level = p_ptr->depth;
        pending_quest_states.aule_forge_y = p_ptr->aule_forge_y;
        pending_quest_states.aule_forge_x = p_ptr->aule_forge_x;
        level_gen_debug_note_questgiver(QUEST_ID_AULE);
        log_trace("Aule quest: FORGE_PRESENT change DEFERRED (quest vault) at %d,%d depth=%d, quest_reserved[0] set to 1",
            p_ptr->aule_forge_y, p_ptr->aule_forge_x, p_ptr->depth);
    }
    if (has_mandos)
    {
        int mandos_target = QUEST_ID_MANDOS;
        bool nonblocking;
        byte mandos_state;
        u32b mandos_flag;

        if (is_maeglin_quest_vault(qv))
            mandos_target = QUEST_ID_MANDOS_BETRAYER;
        else if (is_easterling_quest_vault(qv))
            mandos_target = QUEST_ID_MANDOS_TRAITOR;
        else if (pending_quest_states.mandos_quest_id > 0)
            mandos_target = pending_quest_states.mandos_quest_id;

        pending_quest_states.mandos_quest_id = mandos_target;
        pending_quest_states.mandos_next_state = QUEST_STATE_GIVER_PRESENT;
        nonblocking = (mandos_target == QUEST_ID_MANDOS_BETRAYER);
        mandos_state = (mandos_target == QUEST_ID_MANDOS)
            ? p_ptr->mandos_quest
            : quest_get_state(mandos_target);
        mandos_flag = quest_metarun_flag(mandos_target);

        if (mandos_state == QUEST_STATE_NOT_STARTED
            && (!p_ptr->quest_reserved[0] || nonblocking))
        {
            if (mandos_flag
                && quest_metarun_blocked(mandos_target, mandos_flag))
            {
                log_trace("Mandos quest: blocked during vault processing (quest_id=%d)",
                    mandos_target);
            }
            else
            {
                if (!nonblocking)
                    p_ptr->quest_reserved[0] = 1;

                pending_quest_states.has_mandos_change = true;
                pending_quest_states.mandos_level = p_ptr->depth;
                pending_quest_states.mandos_vault_y = p_ptr->mandos_vault_y;
                pending_quest_states.mandos_vault_x = p_ptr->mandos_vault_x;
                level_gen_debug_note_questgiver(mandos_target);
                log_trace("Mandos quest: GIVER_PRESENT change DEFERRED (quest vault) at %d,%d depth=%d, quest_reserved[0]=%d, quest_id=%d",
                    p_ptr->mandos_vault_y, p_ptr->mandos_vault_x, p_ptr->depth,
                    p_ptr->quest_reserved[0], mandos_target);
            }
        }
    }
}

bool place_orc_stronghold(void)
{
    vault_type* qv_ptr;
    int y, x;

    log_trace("Tulkas orc quest: Attempting to force-place Orc Stronghold at depth %d",
        p_ptr->depth);

    for (int i = 0; i < z_info->v_max; i++)
    {
        qv_ptr = &v_info[i];
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (!vault_template_is_orc_stronghold(qv_ptr)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;
        if (!quest_vault_surface_roll_allows(qv_ptr, p_ptr->depth)) continue;

        level_gen_debug_note_quest_vault_name(v_name + qv_ptr->name);

        y = p_ptr->cur_map_hgt / 2
            + rand_range(-p_ptr->cur_map_hgt / 6, p_ptr->cur_map_hgt / 6);
        x = p_ptr->cur_map_wid / 2
            + rand_range(-p_ptr->cur_map_wid / 6, p_ptr->cur_map_wid / 6);
        y = MAX(qv_ptr->hgt / 2 + 3,
            MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
        x = MAX(qv_ptr->wid / 2 + 3,
            MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

        if (place_room_forced(y, x, qv_ptr))
        {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_tulkas_change = true;
            pending_quest_states.tulkas_level = p_ptr->depth;
            pending_quest_states.tulkas_next_state = QUEST_STATE_GIVER_PRESENT;
            pending_quest_states.tulkas_spawn_pending = true;
            log_trace("Tulkas orc quest: Orc Stronghold placed at (%d,%d)", y,
                x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d)",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        for (int attempts = 0; attempts < 10; attempts++)
        {
            y = p_ptr->cur_map_hgt / 2
                + rand_range(-p_ptr->cur_map_hgt / 4, p_ptr->cur_map_hgt / 4);
            x = p_ptr->cur_map_wid / 2
                + rand_range(-p_ptr->cur_map_wid / 4, p_ptr->cur_map_wid / 4);
            y = MAX(qv_ptr->hgt / 2 + 3,
                MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
            x = MAX(qv_ptr->wid / 2 + 3,
                MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

            if (place_room_forced(y, x, qv_ptr))
            {
                qv_placed_this_level = true;
                level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
                process_quest_vault_area(y, x, qv_ptr);
                p_ptr->quest_reserved[0] = 1;
                pending_quest_states.has_tulkas_change = true;
                pending_quest_states.tulkas_level = p_ptr->depth;
                pending_quest_states.tulkas_next_state = QUEST_STATE_GIVER_PRESENT;
                pending_quest_states.tulkas_spawn_pending = true;
                log_trace("Tulkas orc quest: Orc Stronghold placed on fallback attempt %d at (%d,%d)",
                    attempts + 1, y, x);
                genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) on fallback %d",
                    v_name + qv_ptr->name, y, x, attempts + 1);
                return true;
            }
        }

        if (place_room_forced_exhaustive(qv_ptr, &y, &x))
        {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_tulkas_change = true;
            pending_quest_states.tulkas_level = p_ptr->depth;
            pending_quest_states.tulkas_next_state = QUEST_STATE_GIVER_PRESENT;
            pending_quest_states.tulkas_spawn_pending = true;
            log_trace("Tulkas orc quest: Orc Stronghold placed by exhaustive scan at (%d,%d)",
                y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) after full-map scan",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        log_trace("Tulkas orc quest: Orc Stronghold placement failed after all attempts, returning false");
        genlog_quest("QUEST VAULT FAILED: '%s' could not be placed",
            v_name + qv_ptr->name);
        return false;
    }

    log_trace("Tulkas orc quest: Failed to find Orc Stronghold vault template at depth %d",
        p_ptr->depth);
    return false;
}

bool place_duruin_bastion(void)
{
    vault_type* qv_ptr;
    int y, x;

    log_trace("Varda quest: Attempting to force-place Duruin Bastion at depth %d",
        p_ptr->depth);

    for (int i = 0; i < z_info->v_max; i++)
    {
        int center_y;
        int center_x;

        qv_ptr = &v_info[i];
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (!vault_template_has_duruin(qv_ptr)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;
        if (!quest_vault_surface_roll_allows(qv_ptr, p_ptr->depth)) continue;

        log_trace("Varda quest: Found Duruin Bastion vault at index %d: '%s', attempting placement",
            i, v_name + qv_ptr->name);
        log_trace("Varda quest: Vault details - typ=%d, hgt=%d, wid=%d, depth=%d, flags=0x%x",
            qv_ptr->typ, qv_ptr->hgt, qv_ptr->wid, qv_ptr->depth, qv_ptr->flags);
        level_gen_debug_note_quest_vault_name(v_name + qv_ptr->name);

        center_y = p_ptr->cur_map_hgt / 2;
        center_x = p_ptr->cur_map_wid / 2;
        y = center_y + rand_range(-p_ptr->cur_map_hgt / 6, p_ptr->cur_map_hgt / 6);
        x = center_x + rand_range(-p_ptr->cur_map_wid / 6, p_ptr->cur_map_wid / 6);
        y = MAX(qv_ptr->hgt / 2 + 3,
            MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
        x = MAX(qv_ptr->wid / 2 + 3,
            MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

        if (place_room_forced(y, x, qv_ptr))
        {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_varda_change = true;
            pending_quest_states.varda_level = p_ptr->depth;
            pending_quest_states.varda_vault_y = y;
            pending_quest_states.varda_vault_x = x;
            log_trace("Varda quest: Duruin Bastion placed at (%d,%d)", y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d)",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        for (int attempts = 0; attempts < 10; attempts++)
        {
            y = center_y + rand_range(-p_ptr->cur_map_hgt / 4, p_ptr->cur_map_hgt / 4);
            x = center_x + rand_range(-p_ptr->cur_map_wid / 4, p_ptr->cur_map_wid / 4);
            y = MAX(qv_ptr->hgt / 2 + 3,
                MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
            x = MAX(qv_ptr->wid / 2 + 3,
                MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

            if (place_room_forced(y, x, qv_ptr))
            {
                qv_placed_this_level = true;
                level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
                process_quest_vault_area(y, x, qv_ptr);
                p_ptr->quest_reserved[0] = 1;
                pending_quest_states.has_varda_change = true;
                pending_quest_states.varda_level = p_ptr->depth;
                pending_quest_states.varda_vault_y = y;
                pending_quest_states.varda_vault_x = x;
                log_trace("Varda quest: Duruin Bastion placed on fallback attempt %d at (%d,%d)",
                    attempts + 1, y, x);
                genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) on fallback %d",
                    v_name + qv_ptr->name, y, x, attempts + 1);
                return true;
            }
        }

        log_trace("Varda quest: Random placement failed for '%s', scanning the full map for a guaranteed fit",
            v_name + qv_ptr->name);
        if (place_room_forced_exhaustive(qv_ptr, &y, &x))
        {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_varda_change = true;
            pending_quest_states.varda_level = p_ptr->depth;
            pending_quest_states.varda_vault_y = y;
            pending_quest_states.varda_vault_x = x;
            log_trace("Varda quest: Duruin Bastion placed by exhaustive scan at (%d,%d)",
                y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) after full-map scan",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        log_trace("Varda quest: Duruin Bastion placement failed after all attempts, returning false");
        genlog_quest("QUEST VAULT FAILED: '%s' could not be placed",
            v_name + qv_ptr->name);
        return false;
    }

    log_trace("Varda quest: Failed to find Duruin Bastion vault template at depth %d",
        p_ptr->depth);
    return false;
}

bool place_shadow_bastion(void)
{
    vault_type* qv_ptr;
    int y, x;

    log_trace("Varda quest: Attempting to force-place Shadow Bastion at depth %d",
        p_ptr->depth);

    for (int i = 0; i < z_info->v_max; i++)
    {
        qv_ptr = &v_info[i];
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (!vault_template_has_shadow_bastion(qv_ptr)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;
        if (!quest_vault_surface_roll_allows(qv_ptr, p_ptr->depth)) continue;

        log_trace("Varda quest: Found Shadow Bastion vault at index %d: '%s', attempting placement",
            i, v_name + qv_ptr->name);
        level_gen_debug_note_quest_vault_name(v_name + qv_ptr->name);

        y = p_ptr->cur_map_hgt / 2
            + rand_range(-p_ptr->cur_map_hgt / 6, p_ptr->cur_map_hgt / 6);
        x = p_ptr->cur_map_wid / 2
            + rand_range(-p_ptr->cur_map_wid / 6, p_ptr->cur_map_wid / 6);
        y = MAX(qv_ptr->hgt / 2 + 3,
            MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
        x = MAX(qv_ptr->wid / 2 + 3,
            MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

        if (place_room_forced(y, x, qv_ptr))
        {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_varda_shadow_change = true;
            pending_quest_states.varda_shadow_level = p_ptr->depth;
            pending_quest_states.varda_shadow_y = y;
            pending_quest_states.varda_shadow_x = x;
            log_trace("Varda quest: Shadow Bastion placed at (%d,%d)", y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d)",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        for (int attempts = 0; attempts < 10; attempts++)
        {
            y = p_ptr->cur_map_hgt / 2
                + rand_range(-p_ptr->cur_map_hgt / 4, p_ptr->cur_map_hgt / 4);
            x = p_ptr->cur_map_wid / 2
                + rand_range(-p_ptr->cur_map_wid / 4, p_ptr->cur_map_wid / 4);
            y = MAX(qv_ptr->hgt / 2 + 3,
                MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
            x = MAX(qv_ptr->wid / 2 + 3,
                MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

            if (place_room_forced(y, x, qv_ptr))
            {
                qv_placed_this_level = true;
                level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
                process_quest_vault_area(y, x, qv_ptr);
                p_ptr->quest_reserved[0] = 1;
                pending_quest_states.has_varda_shadow_change = true;
                pending_quest_states.varda_shadow_level = p_ptr->depth;
                pending_quest_states.varda_shadow_y = y;
                pending_quest_states.varda_shadow_x = x;
                log_trace("Varda quest: Shadow Bastion placed on fallback attempt %d at (%d,%d)",
                    attempts + 1, y, x);
                genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) on fallback %d",
                    v_name + qv_ptr->name, y, x, attempts + 1);
                return true;
            }
        }

        if (place_room_forced_exhaustive(qv_ptr, &y, &x))
        {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            p_ptr->quest_reserved[0] = 1;
            pending_quest_states.has_varda_shadow_change = true;
            pending_quest_states.varda_shadow_level = p_ptr->depth;
            pending_quest_states.varda_shadow_y = y;
            pending_quest_states.varda_shadow_x = x;
            log_trace("Varda quest: Shadow Bastion placed by exhaustive scan at (%d,%d)",
                y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) after full-map scan",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        log_trace("Varda quest: Shadow Bastion placement failed after all attempts, returning false");
        genlog_quest("QUEST VAULT FAILED: '%s' could not be placed",
            v_name + qv_ptr->name);
        return false;
    }

    log_trace("Varda quest: Failed to find Shadow Bastion vault template at depth %d",
        p_ptr->depth);
    return false;
}

bool try_quest_vault_type(int v_type, bool *had_eligible_candidate)
{
    int i;
    vault_type* qv_ptr;
    int y, x;
    bool attempted_placement = false;

    if (had_eligible_candidate)
        *had_eligible_candidate = false;

    log_trace("Quest vault: Attempting type %d quest vault with forced placement strategy",
        v_type);

    for (i = 0; i < z_info->v_max; i++)
    {
        bool reserve_slot_for_this = true;

        qv_ptr = &v_info[i];
        if (qv_ptr->typ != v_type) continue;
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;
        if (!quest_vault_surface_roll_allows(qv_ptr, p_ptr->depth)) continue;
        if (vault_template_has_duruin(qv_ptr)
            || vault_template_has_shadow_bastion(qv_ptr)
            || vault_template_is_orc_stronghold(qv_ptr))
        {
            log_trace("Quest vault: Skipping bespoke quest vault in generic placement path");
            continue;
        }

        log_trace("Quest vault: Checking vault %d '%s' (rarity=%d)", i,
            v_name + qv_ptr->name, qv_ptr->rarity);

        if (vault_template_has_aule(qv_ptr))
        {
            log_trace("Quest vault: === AULE VAULT DETECTED === Checking eligibility (depth=%d)",
                p_ptr->depth);
            log_trace("Quest vault: CRITICAL CHECK - quest_reserved[0]=%d (MUST be 0 to proceed)",
                p_ptr->quest_reserved[0]);
            log_trace("  Player SMT skill_base = %d", p_ptr->skill_base[S_SMT]);
            log_trace("  Player SMT skill_use = %d", p_ptr->skill_use[S_SMT]);

            if (!check_quest_eligibility(2, p_ptr->depth))
            {
                log_trace("Quest vault: Aule vault skipped (eligibility check failed)");
                continue;
            }
            log_trace("Quest vault: Aule eligibility check PASSED");

            if (quest_metarun_blocked(QUEST_ID_AULE, METARUN_QUEST_AULE))
            {
                log_trace("Quest vault: Aule vault skipped (quest blocked by metarun)");
                continue;
            }
            if (p_ptr->quest_reserved[0])
            {
                log_trace("Quest vault: === AULE BLOCKED === Another quest already spawned (quest_reserved[0]=1)");
                continue;
            }
            log_trace("Quest vault: === AULE APPROVED === All checks passed, proceeding with generation");
        }

        if (vault_template_has_mandos(qv_ptr))
        {
            bool is_easterling = is_easterling_quest_vault(qv_ptr);
            bool is_maeglin = is_maeglin_quest_vault(qv_ptr);
            int mandos_stage = is_maeglin ? 3 : (is_easterling ? 2 : 1);
            int mandos_quest_id = (mandos_stage == 3)
                ? QUEST_ID_MANDOS_BETRAYER
                : (mandos_stage == 2) ? QUEST_ID_MANDOS_TRAITOR
                                      : QUEST_ID_MANDOS;
            u32b mandos_flag = quest_metarun_flag(mandos_quest_id);
            byte mandos_state = (mandos_stage == 1)
                ? p_ptr->mandos_quest
                : quest_get_state(mandos_quest_id);
            bool uses_reserve = (mandos_stage < 3);

            log_trace("Quest vault: Checking Mandos vault '%s' - quest_id=%d stage=%d state=%d, quest_reserved[0]=%d",
                v_name + qv_ptr->name, mandos_quest_id, mandos_stage,
                mandos_state, p_ptr->quest_reserved[0]);
            if (mandos_stage == 2 && (p_ptr->depth < 10 || p_ptr->depth > 13))
            {
                log_trace("Quest vault: Mandos second quest '%s' skipped - depth %d outside 10-13",
                    v_name + qv_ptr->name, p_ptr->depth);
                continue;
            }
            if (mandos_stage == 3 && (p_ptr->depth < 17 || p_ptr->depth > 19))
            {
                log_trace("Quest vault: Mandos third quest '%s' skipped - depth %d outside 17-19",
                    v_name + qv_ptr->name, p_ptr->depth);
                continue;
            }
            if (mandos_state != QUEST_STATE_NOT_STARTED)
            {
                log_trace("Quest vault: Mandos vault skipped (quest state %d)",
                    mandos_state);
                continue;
            }
            if (mandos_stage == 2 && !mandos_second_stage_ready())
            {
                log_trace("Quest vault: Mandos second quest skipped (requirements not met)");
                continue;
            }
            if (mandos_stage == 3 && !mandos_third_stage_ready())
            {
                log_trace("Quest vault: Mandos third quest skipped (requirements not met)");
                continue;
            }
            if (mandos_stage == 1
                && metarun_quest_completion_count(METARUN_QUEST_MANDOS) != 0)
            {
                log_trace("Quest vault: Mandos first quest skipped - later quest is pending");
                continue;
            }
            if (quest_metarun_blocked(mandos_quest_id, mandos_flag))
            {
                log_trace("Quest vault: Mandos vault skipped (quest blocked by metarun)");
                continue;
            }
            if (mandos_stage == 2)
            {
                monster_race *ulf = &r_info[R_IDX_ULFANG];
                monster_race *uld = &r_info[R_IDX_ULDOR];

                if (ulf->max_num == 0 || uld->max_num == 0
                    || ulf->cur_num > 0 || uld->cur_num > 0)
                {
                    log_trace("Quest vault: Mandos second quest skipped - traitors unavailable (ulf max=%d cur=%d, uld max=%d cur=%d)",
                        ulf->max_num, ulf->cur_num, uld->max_num,
                        uld->cur_num);
                    continue;
                }
            }
            if (mandos_stage == 3)
            {
                monster_race *mae = &r_info[R_IDX_MAEGLIN];

                if (mae->max_num == 0 || mae->cur_num > 0)
                {
                    log_trace("Quest vault: Mandos third quest skipped - Maeglin unavailable (max=%d cur=%d)",
                        mae->max_num, mae->cur_num);
                    continue;
                }
            }
            if (uses_reserve && p_ptr->quest_reserved[0])
            {
                log_trace("Quest vault: Mandos vault skipped (another quest already spawned this run)");
                continue;
            }
            pending_quest_states.mandos_quest_id = mandos_quest_id;
            pending_quest_states.mandos_next_state = QUEST_STATE_GIVER_PRESENT;
            reserve_slot_for_this = uses_reserve;
        }

        attempted_placement = true;
        if (had_eligible_candidate)
            *had_eligible_candidate = true;
        level_gen_debug_note_quest_vault_name(v_name + qv_ptr->name);

        {
            int center_y = p_ptr->cur_map_hgt / 2;
            int center_x = p_ptr->cur_map_wid / 2;

            y = center_y + rand_range(-p_ptr->cur_map_hgt / 6,
                p_ptr->cur_map_hgt / 6);
            x = center_x + rand_range(-p_ptr->cur_map_wid / 6,
                p_ptr->cur_map_wid / 6);
            y = MAX(qv_ptr->hgt / 2 + 3,
                MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
            x = MAX(qv_ptr->wid / 2 + 3,
                MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

            log_trace("Quest vault: Attempting forced placement of '%s' at optimal location (%d,%d) (center: %d,%d)",
                v_name + qv_ptr->name, y, x, center_y, center_x);
        }

        if (place_room_forced(y, x, qv_ptr))
        {
            int y1 = y - qv_ptr->hgt / 2;
            int x1 = x - qv_ptr->wid / 2;
            int y2 = y1 + qv_ptr->hgt - 1;
            int x2 = x1 + qv_ptr->wid - 1;
            int verify_walls = 0;
            int verify_floors = 0;
            int verify_features = 0;
            int verify_monsters = 0;
            int verify_icky = 0;
            int verify_room = 0;

            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);

            for (int vy = y1; vy <= y2; vy++)
            {
                for (int vx = x1; vx <= x2; vx++)
                {
                    if (cave_feat[vy][vx] == FEAT_WALL_OUTER
                        || cave_feat[vy][vx] == FEAT_WALL_INNER)
                    {
                        verify_walls++;
                    }
                    else if (cave_feat[vy][vx] == FEAT_FLOOR)
                    {
                        verify_floors++;
                    }
                    else if (cave_feat[vy][vx] != FEAT_WALL_EXTRA)
                    {
                        verify_features++;
                    }

                    if (cave_m_idx[vy][vx] > 0)
                        verify_monsters++;
                    if (cave_info[vy][vx] & CAVE_ICKY)
                        verify_icky++;
                    if (cave_info[vy][vx] & CAVE_ROOM)
                        verify_room++;
                }
            }

            log_trace("VAULT VERIFICATION IMMEDIATELY AFTER PLACEMENT: Area (%d,%d) to (%d,%d)",
                y1, x1, y2, x2);
            log_trace("VAULT VERIFICATION: %d walls, %d floors, %d features, %d monsters",
                verify_walls, verify_floors, verify_features, verify_monsters);
            log_trace("VAULT VERIFICATION: %d CAVE_ICKY, %d CAVE_ROOM flags",
                verify_icky, verify_room);

            process_quest_vault_area(y, x, qv_ptr);
            if (reserve_slot_for_this)
                p_ptr->quest_reserved[0] = 1;

            log_trace("Quest vault: Type %d quest vault '%s' placed at (%d,%d) using forced strategy",
                v_type, v_name + qv_ptr->name, y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' type=%d at (%d,%d)",
                v_name + qv_ptr->name, v_type, y, x);
            return true;
        }
        else
        {
            log_trace("Quest vault: Failed to place vault '%s' at (%d,%d) even with forced strategy",
                v_name + qv_ptr->name, y, x);
            for (int attempts = 0; attempts < 10; attempts++)
            {
                int center_y = p_ptr->cur_map_hgt / 2;
                int center_x = p_ptr->cur_map_wid / 2;

                y = center_y + rand_range(-p_ptr->cur_map_hgt / 4,
                    p_ptr->cur_map_hgt / 4);
                x = center_x + rand_range(-p_ptr->cur_map_wid / 4,
                    p_ptr->cur_map_wid / 4);
                y = MAX(qv_ptr->hgt / 2 + 3,
                    MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt / 2 - 3));
                x = MAX(qv_ptr->wid / 2 + 3,
                    MIN(x, p_ptr->cur_map_wid - qv_ptr->wid / 2 - 3));

                if (place_room_forced(y, x, qv_ptr))
                {
                    int y1 = y - qv_ptr->hgt / 2;
                    int x1 = x - qv_ptr->wid / 2;
                    int y2 = y1 + qv_ptr->hgt - 1;
                    int x2 = x1 + qv_ptr->wid - 1;
                    int verify_walls = 0;
                    int verify_floors = 0;
                    int verify_features = 0;
                    int verify_monsters = 0;
                    int verify_icky = 0;
                    int verify_room = 0;

                    qv_placed_this_level = true;
                    level_gen_debug_activate_quest_vault_name(
                        v_name + qv_ptr->name);

                    for (int vy = y1; vy <= y2; vy++)
                    {
                        for (int vx = x1; vx <= x2; vx++)
                        {
                            if (cave_feat[vy][vx] == FEAT_WALL_OUTER
                                || cave_feat[vy][vx] == FEAT_WALL_INNER)
                            {
                                verify_walls++;
                            }
                            else if (cave_feat[vy][vx] == FEAT_FLOOR)
                            {
                                verify_floors++;
                            }
                            else if (cave_feat[vy][vx] != FEAT_WALL_EXTRA)
                            {
                                verify_features++;
                            }

                            if (cave_m_idx[vy][vx] > 0)
                                verify_monsters++;
                            if (cave_info[vy][vx] & CAVE_ICKY)
                                verify_icky++;
                            if (cave_info[vy][vx] & CAVE_ROOM)
                                verify_room++;
                        }
                    }

                    log_trace("VAULT VERIFICATION (FALLBACK) IMMEDIATELY AFTER PLACEMENT: Area (%d,%d) to (%d,%d)",
                        y1, x1, y2, x2);
                    log_trace("VAULT VERIFICATION (FALLBACK): %d walls, %d floors, %d features, %d monsters",
                        verify_walls, verify_floors, verify_features,
                        verify_monsters);
                    log_trace("VAULT VERIFICATION (FALLBACK): %d CAVE_ICKY, %d CAVE_ROOM flags",
                        verify_icky, verify_room);

                    process_quest_vault_area(y, x, qv_ptr);
                    if (reserve_slot_for_this)
                        p_ptr->quest_reserved[0] = 1;

                    log_trace("Quest vault: Type %d quest vault '%s' placed at (%d,%d) using fallback attempt %d",
                        v_type, v_name + qv_ptr->name, y, x, attempts + 1);
                    genlog_quest("QUEST VAULT PLACED: '%s' type=%d at (%d,%d) on fallback %d",
                        v_name + qv_ptr->name, v_type, y, x, attempts + 1);
                    return true;
                }
            }

            log_trace("Quest vault: Random placement failed for '%s', scanning the full map for any valid fit",
                v_name + qv_ptr->name);
            if (place_room_forced_exhaustive(qv_ptr, &y, &x))
            {
                qv_placed_this_level = true;
                level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
                process_quest_vault_area(y, x, qv_ptr);
                if (reserve_slot_for_this)
                    p_ptr->quest_reserved[0] = 1;

                log_trace("Quest vault: Type %d quest vault '%s' placed at (%d,%d) by exhaustive scan",
                    v_type, v_name + qv_ptr->name, y, x);
                genlog_quest("QUEST VAULT PLACED: '%s' type=%d at (%d,%d) after full-map scan",
                    v_name + qv_ptr->name, v_type, y, x);
                return true;
            }

            genlog_quest("QUEST VAULT FAILED: '%s' type=%d could not be placed",
                v_name + qv_ptr->name, v_type);
        }
    }

    if (attempted_placement)
        log_trace("Quest vault: Type %d had eligible templates, but none fit this attempt",
            v_type);
    else
        log_trace("Quest vault: Type %d has no eligible templates for this character/depth",
            v_type);

    return false;
}

static void schedule_tulkas_orc_stronghold(void)
{
    int target_depth = rand_range(8, 10);

    if (p_ptr->tulkas_stronghold_level > 0 && p_ptr->tulkas_stronghold_level <= 10) {
        log_trace("Tulkas orc quest: schedule already set to depth %d, leaving unchanged", p_ptr->tulkas_stronghold_level);
        return;
    }

    p_ptr->tulkas_stronghold_level = target_depth;
    p_ptr->tulkas_stronghold_placed = 0;
    p_ptr->tulkas_orc_mask = 0;
    p_ptr->tulkas_orc_restricted = 1;
    p_ptr->tulkas_second_spawn_pending = 0;
    p_ptr->quest_reserved[0] = 1;

    log_trace("Tulkas orc quest: Scheduled Orc Stronghold at depth %d (current depth=%d)", target_depth, p_ptr->depth);
}

bool spawn_tulkas_near_player_with_fallback(void)
{
    int j;

    for (j = 1; j < mon_max; j++)
    {
        monster_type *m_ptr = &mon_list[j];
        if (m_ptr->r_idx == R_IDX_TULKAS) {
            log_trace("Tulkas spawn helper: Tulkas already present on level");
            return true;
        }
    }

    {
        int player_y = p_ptr->py;
        int player_x = p_ptr->px;

        for (int attempts = 0; attempts < 50; attempts++)
        {
            int dy = rand_range(-2, 2);
            int dx = rand_range(-2, 2);
            int try_y = player_y + dy;
            int try_x = player_x + dx;

            if (try_y <= 0 || try_y >= p_ptr->cur_map_hgt - 1 ||
                try_x <= 0 || try_x >= p_ptr->cur_map_wid - 1) continue;
            if (!cave_floor_bold(try_y, try_x)) continue;
            if (!(cave_info[try_y][try_x] & CAVE_ROOM)) continue;
            if (cave_info[try_y][try_x] & CAVE_ICKY) continue;
            if (cave_m_idx[try_y][try_x] != 0) continue;

            if (place_monster_one(try_y, try_x, R_IDX_TULKAS, true, true, NULL))
            {
                log_trace("Tulkas spawn helper: spawned near player at (%d,%d)", try_y, try_x);
                return true;
            }
        }

        for (int attempts = 0; attempts < 100; attempts++)
        {
            int room_y = rand_int(p_ptr->cur_map_hgt);
            int room_x = rand_int(p_ptr->cur_map_wid);

            if (!cave_floor_bold(room_y, room_x)) continue;
            if (!(cave_info[room_y][room_x] & CAVE_ROOM)) continue;
            if (cave_info[room_y][room_x] & CAVE_ICKY) continue;
            if (cave_m_idx[room_y][room_x] != 0) continue;

            if (place_monster_one(room_y, room_x, R_IDX_TULKAS, true, true, NULL))
            {
                log_trace("Tulkas spawn helper: spawned in fallback room at (%d,%d)", room_y, room_x);
                return true;
            }
        }
    }

    log_trace("Tulkas spawn helper: failed to place Tulkas on this level");
    return false;
}

/* Roulette quest registry - initialized dynamically based on Y:1 field */
static roulette_quest_entry roulette_quests[8];  /* Max 8 quests from limits.txt */
static int roulette_quest_count = 0;

/* Parametric formula calculation */
static float calculate_parametric_probability(quest_type* q_ptr, int depth) {
    float probability = 0.0f;
    
    /* Check depth bounds */
    if (depth < q_ptr->depth_min || depth > q_ptr->depth_max) {
        log_trace("Quest %d: depth %d outside valid range [%d-%d], probability = 0", 
                  q_ptr->quest_num, depth, q_ptr->depth_min, q_ptr->depth_max);
        return 0.0f;
    }
    
    log_debug("Quest %d formula calculation: type=%d, depth=%d, params=[%.3f,%.3f,%.3f,%.3f]", 
              q_ptr->quest_num, q_ptr->formula_type, depth, 
              q_ptr->formula_params[0], q_ptr->formula_params[1], 
              q_ptr->formula_params[2], q_ptr->formula_params[3]);
    
    switch (q_ptr->formula_type) {
        case FORMULA_LINEAR_DECAY:
            /* Formula: 1/(base - depth) */
            /* Params: [0]=base, others unused */
            {
                float base = q_ptr->formula_params[0];
                float denominator = base - (float)depth;
                if (denominator > 0.0f) {
                    probability = 1.0f / denominator;
                    log_debug("Quest %d LINEAR_DECAY: base=%.1f, depth=%d, denominator=%.1f, probability=%.4f", 
                              q_ptr->quest_num, base, depth, denominator, probability);
                } else {
                    log_debug("Quest %d LINEAR_DECAY: base=%.1f, depth=%d, denominator=%.1f <= 0, probability=0", 
                              q_ptr->quest_num, base, depth, denominator);
                }
            }
            break;
            
        case FORMULA_SCALED_RANGE:
            /* Formula: max_prob * max(0, min(1, (depth-start_depth)/range)) */
            /* Params: [0]=max_prob, [1]=start_depth, [2]=range, [3]=unused */
            {
                float max_prob = q_ptr->formula_params[0];
                float start_depth = q_ptr->formula_params[1];
                float range = q_ptr->formula_params[2];
                
                if (range > 0.0f) {
                    float factor = ((float)depth - start_depth) / range;
                    if (factor < 0.0f) factor = 0.0f;
                    if (factor > 1.0f) factor = 1.0f;
                    probability = max_prob * factor;
                    log_debug("Quest %d SCALED_RANGE: max_prob=%.3f, start_depth=%.1f, range=%.1f, depth=%d, factor=%.3f, probability=%.4f", 
                              q_ptr->quest_num, max_prob, start_depth, range, depth, factor, probability);
                } else {
                    log_debug("Quest %d SCALED_RANGE: range=%.1f <= 0, probability=0", q_ptr->quest_num, range);
                }
            }
            break;
            
        case FORMULA_FIXED_PERCENT:
            /* Formula: constant percentage */
            /* Params: [0]=percentage (0.0-1.0), others unused */
            probability = q_ptr->formula_params[0];
            log_debug("Quest %d FIXED_PERCENT: constant probability=%.4f", q_ptr->quest_num, probability);
            break;
            
        case FORMULA_LINEAR_INTERPOLATE:
            /* Formula: linear interpolation between min_prob and max_prob over depth range */
            /* Params: [0]=min_prob, [1]=max_prob, [2]=unused, [3]=unused */
            {
                float min_prob = q_ptr->formula_params[0];
                float max_prob = q_ptr->formula_params[1];
                int depth_range = q_ptr->depth_max - q_ptr->depth_min;
                
                if (depth_range > 0) {
                    float factor = (float)(depth - q_ptr->depth_min) / (float)depth_range;
                    probability = min_prob + (max_prob - min_prob) * factor;
                    log_debug("Quest %d LINEAR_INTERPOLATE: min_prob=%.3f, max_prob=%.3f, depth=%d, range=%d, factor=%.3f, probability=%.4f", 
                              q_ptr->quest_num, min_prob, max_prob, depth, depth_range, factor, probability);
                } else {
                    /* Single depth case - use min_prob */
                    probability = min_prob;
                    log_debug("Quest %d LINEAR_INTERPOLATE: depth_range=0, using min_prob=%.3f", q_ptr->quest_num, min_prob);
                }
            }
            break;
            
        case FORMULA_HARDCODED:
        default:
            /* Use hardcoded functions - should not reach here for parametric calls */
            probability = 0.0f;
            log_debug("Quest %d: HARDCODED or unknown formula type %d, probability=0", 
                      q_ptr->quest_num, q_ptr->formula_type);
            break;
    }
    
    /* Clamp probability to valid range */
    if (probability < 0.0f) probability = 0.0f;
    if (probability > 1.0f) probability = 1.0f;
    
    log_info("Quest %d final probability at depth %d: %.4f (%.2f%%)", 
             q_ptr->quest_num, depth, probability, probability * 100.0f);
    
    return probability;
}

/* Generic eligibility check for parametric quests */
static bool generic_eligibility_check(int depth, int quest_id) {
    /* Use the comprehensive eligibility check that handles E: field data */
    return check_quest_eligibility(quest_id, depth);
}

/* Generic probability roll for parametric quests */
static bool generic_probability_roll(int depth, int quest_id) {
    quest_type* q_ptr = &quest_info[quest_id];
    float probability = calculate_parametric_probability(q_ptr, depth);
    
    if (probability <= 0.0f) {
        log_trace("Quest lottery: Quest %d probability is 0%% at depth %d", quest_id, depth);
        return false;
    }
    
    /* Roll using a 0-9999 scale to preserve fractional probabilities */
    int threshold = (int)(probability * 10000.0f + 0.5f);
    if (threshold < 1) threshold = 1;
    if (threshold > 10000) threshold = 10000;
    int dice_roll = rand_int(10000);
    bool won = (dice_roll < threshold);
    
    if (won) {
        log_trace("Quest lottery: Quest %d WINS! (rolled %d < %d, chance was %.2f%%, formula_type=%d)", 
                 quest_id, dice_roll, threshold, probability * 100.0f, q_ptr->formula_type);
    } else {
        log_trace("Quest lottery: Quest %d roll failed (rolled %d >= %d, chance was %.2f%%, formula_type=%d)", 
                 quest_id, dice_roll, threshold, probability * 100.0f, q_ptr->formula_type);
    }
    
    return won;
}

/* Helper function to map quest state variable name to pointer */
static byte* get_quest_state_ptr(u32b var_name_offset) {
    if (!var_name_offset || !quest_name_text) return NULL;
    
    /* Get the actual variable name string */
    cptr actual_name = quest_name_text + var_name_offset;
    
    if (SDL_strcasecmp(actual_name, "tulkas_quest") == 0) {
        return &p_ptr->tulkas_quest;
    } else if (SDL_strcasecmp(actual_name, "tulkas_second_quest") == 0) {
        return &p_ptr->vala_quest_stage2[VALA_TULKAS - 1];
    } else if (SDL_strcasecmp(actual_name, "aule_quest") == 0) {
        return &p_ptr->aule_quest;
    } else if (SDL_strcasecmp(actual_name, "mandos_quest") == 0) {
        return &p_ptr->mandos_quest;
    } else if (SDL_strcasecmp(actual_name, "mandos_second_quest") == 0) {
        return &p_ptr->vala_quest_stage2[VALA_MANDOS - 1];
    } else if (SDL_strcasecmp(actual_name, "mandos_third_quest") == 0) {
        return &p_ptr->vala_quest_stage3[VALA_MANDOS - 1];
    } else if (SDL_strcasecmp(actual_name, "varda_second_quest") == 0) {
        return &p_ptr->vala_quest_stage2[VALA_VARDA - 1];
    } else if (SDL_strcasecmp(actual_name, "varda_third_quest") == 0) {
        return &p_ptr->vala_quest_stage3[VALA_VARDA - 1];
    } else if (SDL_strcasecmp(actual_name, "niena_quest") == 0) {
        return &p_ptr->niena_quest;
    } else if (SDL_strcasecmp(actual_name, "niena_second_quest") == 0) {
        return &p_ptr->vala_quest_stage2[VALA_NIENNA - 1];
    } else if (SDL_strcasecmp(actual_name, "niena_third_quest") == 0) {
        return &p_ptr->vala_quest_stage3[VALA_NIENNA - 1];
    } else if (SDL_strcasecmp(actual_name, "orome_quest") == 0) {
        return &p_ptr->orome_quest;
    } else if (SDL_strcasecmp(actual_name, "orome_second_quest") == 0) {
        return &p_ptr->vala_quest_stage2[VALA_OROME - 1];
    } else if (SDL_strcasecmp(actual_name, "orome_third_quest") == 0) {
        return &p_ptr->vala_quest_stage3[VALA_OROME - 1];
    } else if (SDL_strcasecmp(actual_name, "varda_quest") == 0) {
        return &p_ptr->varda_quest;
    }
    
    return NULL; /* Unknown quest state variable */
}

/* Helper function to map metarun quest ID string to constant */
static int get_metarun_quest_id(u32b id_name_offset) {
    if (!id_name_offset || !quest_name_text) return 0;
    
    /* Get the actual ID string */
    cptr actual_id = quest_name_text + id_name_offset;
    
    if (SDL_strcasecmp(actual_id, "METARUN_QUEST_TULKAS") == 0) {
        return METARUN_QUEST_TULKAS;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_AULE") == 0) {
        return METARUN_QUEST_AULE;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_MANDOS") == 0) {
        return METARUN_QUEST_MANDOS;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_MANDOS_TRAITOR") == 0) {
        return METARUN_QUEST_MANDOS_TRAITOR;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_MANDOS_THIRD") == 0 ||
               SDL_strcasecmp(actual_id, "METARUN_QUEST_MANDOS_BETRAYER") == 0) {
        return METARUN_QUEST_MANDOS_BETRAYER;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_NIENA") == 0) {
        return METARUN_QUEST_NIENA;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_NIENA_MORGOTH") == 0) {
        return METARUN_QUEST_NIENA_MORGOTH;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_NIENA_PACIFIST") == 0) {
        return METARUN_QUEST_NIENA_PACIFIST;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_OROME") == 0) {
        return METARUN_QUEST_OROME;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_OROME_DRAGONS") == 0) {
        return METARUN_QUEST_OROME_DRAGONS;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_OROME_GREAT_HUNT") == 0) {
        return METARUN_QUEST_OROME_GREAT_HUNT;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_VARDA") == 0) {
        return METARUN_QUEST_VARDA;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_TULKAS_ORCS") == 0) {
        return METARUN_QUEST_TULKAS_ORCS;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_TULKAS_MORGOTH") == 0) {
        return METARUN_QUEST_TULKAS_MORGOTH;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_VARDA_SHADOW") == 0) {
        return METARUN_QUEST_VARDA_SHADOW;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_VARDA_UNGOLIANT") == 0) {
        return METARUN_QUEST_VARDA_UNGOLIANT;
    }
    
    return 0; /* Unknown metarun quest ID */
}

/* Initialize the roulette quest registry based on quest.txt Y: field */
static void init_roulette_quest_registry(void) {
    roulette_quest_count = 0;
    
    /* Scan all quests to find Y:1 (roulette-based) quests */
    for (int i = 1; i < z_info->quest_max; i++) {
        quest_type* q_ptr = &quest_info[i];
        
        /* Skip if not a roulette quest (Y:1) */
        if (q_ptr->quest_type != 1) continue;
        
        /* Add to registry */
        roulette_quest_entry* entry = &roulette_quests[roulette_quest_count];
        entry->quest_id = i;
        
        /* Use data-driven mapping for quest state and metarun ID */
        entry->quest_state_ptr = get_quest_state_ptr(q_ptr->quest_state_var);
        entry->metarun_quest_id = get_metarun_quest_id(q_ptr->metarun_quest_id);
        
        /* Log mapping results */
        if (entry->quest_state_ptr && entry->metarun_quest_id) {
            log_trace("Quest lottery: Quest %d mapped successfully (state_ptr=%p, metarun_id=%d)", 
                      i, entry->quest_state_ptr, entry->metarun_quest_id);
        } else {
            log_trace("Quest lottery: Quest %d mapping failed (state_ptr=%p, metarun_id=%d)", 
                      i, entry->quest_state_ptr, entry->metarun_quest_id);
            /* Still register the quest but mark as unsupported for state tracking */
        }
        
        /* Use parametric formulas if available, otherwise skip unsupported quests */
        if (q_ptr->formula_type != FORMULA_HARDCODED) {
            entry->eligibility_check = generic_eligibility_check;
            entry->probability_roll = generic_probability_roll;
            log_trace("Quest lottery: Quest %d using parametric formula (type=%d)", i, q_ptr->formula_type);
        } else {
            /* For legacy compatibility, map hardcoded functions for known quests */
            if (i == QUEST_ID_TULKAS || i == QUEST_ID_TULKAS_ORCS) { /* Tulkas quests */
                entry->eligibility_check = data_driven_eligibility_check; /* Use data-driven eligibility */
                entry->probability_roll = tulkas_probability_roll;
                log_trace("Quest lottery: Quest %d using mixed formula (data-driven eligibility + hardcoded probability)", i);
            } else if (i == 4) { /* Niena */
                entry->eligibility_check = data_driven_eligibility_check; /* Use data-driven eligibility */
                entry->probability_roll = niena_probability_roll;
                log_trace("Quest lottery: Quest %d using mixed formula (data-driven eligibility + hardcoded probability)", i);
            } else {
                entry->eligibility_check = NULL;
                entry->probability_roll = NULL;
                log_trace("Quest lottery: Quest %d has no formula implementation, skipping", i);
                continue; /* Skip unsupported quests */
            }
        }
        
        roulette_quest_count++;
        log_trace("Quest lottery: Registered roulette quest %d (index %d)", 
                  entry->quest_id, roulette_quest_count - 1);
    }
    
    log_trace("Quest lottery: Initialized with %d roulette quests (data-driven mapping)", roulette_quest_count);
}

/* Data-driven eligibility check using quest.txt E: field */
static bool data_driven_eligibility_check(int depth, int quest_id) {
    return check_quest_eligibility(quest_id, depth);
}

/* Tulkas-specific probability roll */
static bool tulkas_probability_roll(int depth, int quest_id) {
    int tulkas_chance = 27 - depth;
    int dice_roll = rand_int(tulkas_chance);  /* Get the actual roll value */
    bool won = (dice_roll == 0);  /* one_in_(N) succeeds when rand_int(N) == 0 */
    
    if (won) {
        log_trace("Quest lottery: quest %d (Tulkas) WINS! (rolled %d, needed 0, chance was 1/%d = %.1f%%)",
            quest_id, dice_roll, tulkas_chance, 100.0f / tulkas_chance);
    } else {
        log_trace("Quest lottery: quest %d (Tulkas) roll failed (rolled %d, needed 0, chance was 1/%d = %.1f%%)",
            quest_id, dice_roll, tulkas_chance, 100.0f / tulkas_chance);
    }
    
    return won;
}

/* Niena-specific probability roll */
static bool niena_probability_roll(int depth, int quest_id) {
    /* Niena probability: p_Nienna(lvl) = 0.125 * max(0, min(1, (lvl - 14) / 5)) */
    /* This gives: 0% before lvl 14, scales from 0% to 12.5% between lvl 14-19, then 12.5% at 19+ */
    
    float depth_factor = (float)(depth - 14) / 5.0f;
    if (depth_factor > 1.0f) depth_factor = 1.0f;  /* cap at 1.0 */
    if (depth_factor < 0.0f) depth_factor = 0.0f;  /* shouldn't happen due to depth check above */
    
    float niena_probability = 0.125f * depth_factor;
    
    if (niena_probability > 0.0f) {
        /* Convert probability to one_in_() parameter: if P = 1/N, then one_in_(N) gives probability P */
        int niena_chance = (int)(1.0f / niena_probability + 0.5f);  /* round to nearest int */
        
        int dice_roll = rand_int(niena_chance);  /* Get the actual roll value */
        bool won = (dice_roll == 0);  /* one_in_(N) succeeds when rand_int(N) == 0 */
        
        if (won) {
            log_trace("Quest lottery: quest %d (Niena) WINS! (rolled %d, needed 0, chance was 1/%d = %.1f%%)",
                quest_id, dice_roll, niena_chance, niena_probability * 100.0f);
        } else {
            log_trace("Quest lottery: quest %d (Niena) roll failed (rolled %d, needed 0, chance was 1/%d = %.1f%%)",
                quest_id, dice_roll, niena_chance, niena_probability * 100.0f);
        }
        
        return won;
    } else {
        log_trace("Quest lottery: quest %d (Niena) probability is 0%% at depth %d", quest_id, depth);
        return false;
    }
}

/* Debug function: manually trigger quest roulette */
void debug_run_quest_roulette(void) {
    reset_quest_lottery_state();

    run_quest_lottery();
}

/* Debug function: get quest lottery winner */
int debug_get_quest_lottery_winner(void) {
    return quest_lottery_winner;
}

void reset_quest_lottery_state(void)
{
    quest_lottery_winner = 0;
    quest_lottery_resolved = false;
}

void run_quest_lottery(void) {
    log_trace("Quest lottery: === LOTTERY START === (depth=%d, quest_reserved[0]=%d)", p_ptr->depth, p_ptr->quest_reserved[0]);
    bool mark_tulkas_second_roll = false;
    byte varda_shadow_state;
    bool varda_oath_active;
    bool varda_oath_unlocked;
    bool varda_followup_ready;

    if (!p_ptr->tulkas_second_roll_done &&
        p_ptr->depth > 4 &&
        quest_get_state(QUEST_ID_TULKAS_ORCS) == QUEST_STATE_NOT_STARTED &&
        p_ptr->oath_type == OATH_VALOROUS && !oath_invalid(OATH_VALOROUS) &&
        tulkas_orc_targets_alive(true)) {
        mark_tulkas_second_roll = true;
    }

    varda_shadow_state = quest_get_state(QUEST_ID_VARDA_SHADOW);
    varda_oath_active = (p_ptr->oath_type == OATH_LIGHT && !oath_invalid(OATH_LIGHT));
    varda_oath_unlocked = (p_ptr->varda_quest >= VARDA_QUEST_REWARDED) ||
                          (metarun_quest_completion_count(METARUN_QUEST_VARDA) > 0);
    varda_followup_ready = (varda_shadow_state == QUEST_STATE_NOT_STARTED &&
                            varda_oath_active && varda_oath_unlocked);

    if (quest_lottery_resolved) {
        log_trace("Quest lottery: Already resolved for this level (winner=%d)", quest_lottery_winner);
        if (mark_tulkas_second_roll) p_ptr->tulkas_second_roll_done = 1;
        return;
    }
    
    /* If Varda quest is active/successful, suppress other roulette quests entirely */
    if (p_ptr->varda_quest >= VARDA_QUEST_ACTIVE || varda_shadow_state >= QUEST_STATE_ACTIVE) {
        log_trace("Quest lottery: SKIPPED - Varda quest in progress (state=%d shadow_state=%d)", p_ptr->varda_quest, varda_shadow_state);
        quest_lottery_winner = 0;
        quest_lottery_resolved = true;
        if (mark_tulkas_second_roll) p_ptr->tulkas_second_roll_done = 1;
        return;
    }
    
    /* Initialize registry if not done yet */
    if (roulette_quest_count == 0) {
        init_roulette_quest_registry();
    }
    
    /* CRITICAL: Do not run lottery if player is actively escaping (on the run) */
    if (p_ptr->on_the_run) {
        log_trace("Quest lottery: SKIPPED - player is on the run (no quests spawn during escape)");
        quest_lottery_winner = 0;
        quest_lottery_resolved = true;
        if (mark_tulkas_second_roll) p_ptr->tulkas_second_roll_done = 1;
        return;
    }
    
    /* CRITICAL: Do not run lottery if any quest is already started on this character */
    log_trace("Quest lottery: Checking current quest states before lottery");
    log_trace("Quest lottery: tulkas=%d, niena=%d, orome=%d, aule=%d, mandos=%d, varda=%d shadow=%d", 
              p_ptr->tulkas_quest, p_ptr->niena_quest, p_ptr->orome_quest, p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->varda_quest, varda_shadow_state);
    log_trace("Quest lottery: quest_reserved[0]=%d (any quest spawned flag - should block all quests if set)", p_ptr->quest_reserved[0]);

    bool varda_slot_reuse = varda_followup_ready && p_ptr->quest_reserved[0] &&
        p_ptr->varda_quest == VARDA_QUEST_REWARDED &&
        p_ptr->tulkas_quest == TULKAS_QUEST_NOT_STARTED &&
        quest_get_state(QUEST_ID_TULKAS_ORCS) == QUEST_STATE_NOT_STARTED &&
        p_ptr->tulkas_stronghold_level == 0 &&
        p_ptr->niena_quest == NIENA_QUEST_NOT_STARTED &&
        p_ptr->orome_quest == OROME_QUEST_NOT_STARTED &&
        p_ptr->aule_quest == AULE_QUEST_NOT_STARTED &&
        p_ptr->mandos_quest == MANDOS_QUEST_NOT_STARTED;

    /* Check if any quest slot is already reserved */
    if (p_ptr->quest_reserved[0] && !varda_slot_reuse) {
        log_trace("Quest lottery: BLOCKED - quest slot already reserved (quest_reserved[0]=1), one-quest-per-run enforced");
        quest_lottery_winner = 0;
        quest_lottery_resolved = true;
        if (mark_tulkas_second_roll) p_ptr->tulkas_second_roll_done = 1;
        return;
    }

    bool varda_blocks = (p_ptr->varda_quest > VARDA_QUEST_NOT_STARTED) && !varda_slot_reuse;
    if (p_ptr->tulkas_quest > TULKAS_QUEST_NOT_STARTED ||
        quest_get_state(QUEST_ID_TULKAS_ORCS) > QUEST_STATE_NOT_STARTED ||
        p_ptr->tulkas_stronghold_level > 0 ||
        p_ptr->niena_quest > NIENA_QUEST_NOT_STARTED ||
        p_ptr->orome_quest > OROME_QUEST_NOT_STARTED ||
        p_ptr->aule_quest > AULE_QUEST_NOT_STARTED ||
        p_ptr->mandos_quest > MANDOS_QUEST_NOT_STARTED ||
        varda_blocks ||
        varda_shadow_state > QUEST_STATE_NOT_STARTED) {
        
        log_trace("Quest lottery: SKIPPED - quest already started on this character (tulkas=%d, tulkas_orcs_state=%d, stronghold_level=%d, niena=%d, orome=%d, aule=%d, mandos=%d, varda=%d, shadow=%d)", 
                  p_ptr->tulkas_quest, quest_get_state(QUEST_ID_TULKAS_ORCS), p_ptr->tulkas_stronghold_level,
                  p_ptr->niena_quest, p_ptr->orome_quest, p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->varda_quest, varda_shadow_state);
        quest_lottery_winner = 0;
        quest_lottery_resolved = true;
        if (mark_tulkas_second_roll) p_ptr->tulkas_second_roll_done = 1;
        return;
    }
    
    log_trace("Quest lottery: Running for depth %d with %d registered roulette quests", 
              p_ptr->depth, roulette_quest_count);
    
    /* Reset state */
    quest_lottery_winner = 0;
    quest_lottery_resolved = true;
    
    /* Create a randomized order for quest evaluation */
    int quest_order[8];  /* Max 8 quests */
    for (int i = 0; i < roulette_quest_count; i++) {
        quest_order[i] = i;
    }
    
    /* Shuffle the quest order using Fisher-Yates algorithm */
    for (int i = roulette_quest_count - 1; i > 0; i--) {
        int j = rand_int(i + 1);
        int temp = quest_order[i];
        quest_order[i] = quest_order[j];
        quest_order[j] = temp;
    }
    
    /* Log the randomized quest evaluation order */
    log_trace("Quest lottery: Random evaluation order generated for %d quests", roulette_quest_count);
    for (int i = 0; i < roulette_quest_count; i++) {
        roulette_quest_entry* entry = &roulette_quests[quest_order[i]];
        const char* quest_name = "Unknown";
        if (entry->quest_id > 0 && entry->quest_id < z_info->quest_max) {
            quest_type* q_ptr = &quest_info[entry->quest_id];
            if (q_ptr->name && quest_name_text) {
                quest_name = quest_name_text + q_ptr->name;
            }
        }
        log_trace("Quest lottery: Order position %d -> Quest %d (%s)", 
                  i, entry->quest_id, quest_name);
    }
    
    /* Evaluate quests in random order */
    for (int order_idx = 0; order_idx < roulette_quest_count; order_idx++) {
        int quest_idx = quest_order[order_idx];
        roulette_quest_entry* entry = &roulette_quests[quest_idx];
        
        /* Skip unsupported quests */
        if (!entry->quest_state_ptr || !entry->eligibility_check || !entry->probability_roll) {
            log_trace("Quest lottery: Skipping unsupported quest %d", entry->quest_id);
            continue;
        }
        
        /* Check quest state - must be NOT_STARTED */
        if (*entry->quest_state_ptr != 0) { /* 0 = NOT_STARTED for all quest types */
            log_trace("Quest lottery: Quest %d not eligible (state=%d)", 
                      entry->quest_id, *entry->quest_state_ptr);
            continue;
        }
        
        /* Check metarun history (respect oath overrides and completion cap) */
        if (quest_metarun_blocked(entry->quest_id, entry->metarun_quest_id)) {
            log_trace("Quest lottery: Quest %d not eligible due to metarun history", entry->quest_id);
            continue;
        }
        
        /* Check quest-specific eligibility */
        log_trace("Quest lottery: Checking eligibility for Quest %d at depth %d", entry->quest_id, p_ptr->depth);
        bool eligible = entry->eligibility_check(p_ptr->depth, entry->quest_id);
        log_trace("Quest lottery: Quest %d eligibility result: %s", entry->quest_id, eligible ? "PASS" : "FAIL");
        if (!eligible) {
            log_trace("Quest lottery: Quest %d failed eligibility check", entry->quest_id);
            continue;
        }
        
        /* Roll for quest probability */
        log_trace("Quest lottery: Evaluating Quest %d for probability roll at depth %d", entry->quest_id, p_ptr->depth);
        bool won_probability = entry->probability_roll(p_ptr->depth, entry->quest_id);
        log_trace("Quest lottery: Quest %d probability result: %s", entry->quest_id, won_probability ? "WON" : "LOST");
        if (won_probability) {
            quest_lottery_winner = entry->quest_id;
            if (entry->quest_id == QUEST_ID_TULKAS_ORCS) {
                log_trace("Quest lottery: Quest %d (Tulkas orc quest) WON - scheduling stronghold placement", entry->quest_id);
                schedule_tulkas_orc_stronghold();
            }
            log_trace("Quest lottery: Quest %d WINS the lottery!", entry->quest_id);
            if (mark_tulkas_second_roll) p_ptr->tulkas_second_roll_done = 1;
            return;
        } else {
            log_trace("Quest lottery: Quest %d failed probability roll, continuing to next quest", entry->quest_id);
        }
    }
    
    /* No quest won the lottery */
    log_trace("Quest lottery: === LOTTERY END === No quest won - level remains quest-free");
    if (mark_tulkas_second_roll) p_ptr->tulkas_second_roll_done = 1;
}


/* Function to reset pending quest state changes */
void reset_pending_quest_states(void) {
    pending_quest_states.has_aule_change = false;
    pending_quest_states.has_mandos_change = false;
    pending_quest_states.has_varda_change = false;
    pending_quest_states.has_varda_shadow_change = false;
    pending_quest_states.has_tulkas_change = false;
    pending_quest_states.aule_level = 0;
    pending_quest_states.mandos_level = 0;
    pending_quest_states.varda_level = 0;
    pending_quest_states.varda_shadow_level = 0;
    pending_quest_states.tulkas_level = 0;
    pending_quest_states.aule_forge_y = 0;
    pending_quest_states.aule_forge_x = 0;
    pending_quest_states.mandos_vault_y = 0;
    pending_quest_states.mandos_vault_x = 0;
    pending_quest_states.varda_vault_y = 0;
    pending_quest_states.varda_vault_x = 0;
    pending_quest_states.varda_shadow_y = 0;
    pending_quest_states.varda_shadow_x = 0;
    pending_quest_states.mandos_quest_id = QUEST_ID_MANDOS;
    pending_quest_states.mandos_next_state = QUEST_STATE_GIVER_PRESENT;
    pending_quest_states.tulkas_next_state = QUEST_STATE_NOT_STARTED;
    pending_quest_states.tulkas_spawn_pending = false;
}

bool run_has_consumed_quest_slot(void)
{
    if (!p_ptr) return false;

    return p_ptr->quest_reserved[0]
        || p_ptr->quest_vault_used
        || (p_ptr->tulkas_quest > TULKAS_QUEST_NOT_STARTED)
        || (quest_get_state(QUEST_ID_TULKAS_ORCS) > QUEST_STATE_NOT_STARTED)
        || (p_ptr->tulkas_stronghold_level > 0)
        || (p_ptr->niena_quest > NIENA_QUEST_NOT_STARTED)
        || (p_ptr->orome_quest > OROME_QUEST_NOT_STARTED)
        || (p_ptr->aule_quest > AULE_QUEST_NOT_STARTED)
        || (p_ptr->mandos_quest > MANDOS_QUEST_NOT_STARTED)
        || (p_ptr->varda_quest > VARDA_QUEST_NOT_STARTED)
        || (quest_get_state(QUEST_ID_VARDA_SHADOW) > QUEST_STATE_NOT_STARTED);
}

/* Function to reset quest states that were set by quest vaults during regeneration */
void reset_quest_vault_states(bool preserve_run_quest_slot) {
    /* Only reset quest states if they were set at the current level during quest vault placement */
    /* This prevents interfering with quests that were legitimately started on other levels */
    
    log_trace("Quest vault regeneration: START - depth=%d, quest_reserved[0]=%d", 
              p_ptr->depth, p_ptr->quest_reserved[0]);
    byte mandos_second = quest_get_state(QUEST_ID_MANDOS_TRAITOR);
    byte mandos_third = quest_get_state(QUEST_ID_MANDOS_BETRAYER);
    byte varda_shadow_state = quest_get_state(QUEST_ID_VARDA_SHADOW);
    log_trace("Quest vault regeneration: Aule state=%d level=%d, Mandos state=%d level=%d (second=%d third=%d), Tulkas state=%d", 
              p_ptr->aule_quest, p_ptr->aule_level, p_ptr->mandos_quest, p_ptr->mandos_level, mandos_second, mandos_third, p_ptr->tulkas_quest);
    log_trace("Quest vault regeneration: Pending changes - aule=%s mandos=%s", 
              pending_quest_states.has_aule_change ? "yes" : "no", 
              pending_quest_states.has_mandos_change ? "yes" : "no");
    
    /* Reset vault-based quests (Aule, Mandos) */
    if (p_ptr->aule_quest == AULE_QUEST_FORGE_PRESENT && p_ptr->aule_level == p_ptr->depth) {
        log_trace("Quest vault regeneration: Resetting Aule quest from FORGE_PRESENT to NOT_STARTED (level %d)", p_ptr->depth);
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
        p_ptr->aule_level = 0;
    }
    
    if (p_ptr->mandos_quest == MANDOS_QUEST_GIVER_PRESENT && p_ptr->mandos_level == p_ptr->depth) {
        log_trace("Quest vault regeneration: Resetting Mandos quest from GIVER_PRESENT to NOT_STARTED (level %d)", p_ptr->depth);
        p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
        p_ptr->mandos_level = 0;
    }
    if (mandos_second == QUEST_STATE_GIVER_PRESENT && p_ptr->mandos_level == p_ptr->depth) {
        log_trace("Quest vault regeneration: Resetting Mandos second quest from GIVER_PRESENT to NOT_STARTED (level %d)", p_ptr->depth);
        quest_set_state(QUEST_ID_MANDOS_TRAITOR, QUEST_STATE_NOT_STARTED);
        p_ptr->mandos_level = 0;
    }
    if (mandos_third == QUEST_STATE_GIVER_PRESENT && p_ptr->mandos_level == p_ptr->depth) {
        log_trace("Quest vault regeneration: Resetting Mandos third quest from GIVER_PRESENT to NOT_STARTED (level %d)", p_ptr->depth);
        quest_set_state(QUEST_ID_MANDOS_BETRAYER, QUEST_STATE_NOT_STARTED);
        p_ptr->mandos_level = 0;
    }
    
    /* Reset entrance-based quests (Tulkas) - these don't store level but spawn during this generation */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_GIVER_PRESENT) {
        log_trace("Quest vault regeneration: Resetting Tulkas quest from GIVER_PRESENT to NOT_STARTED");
        p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
        /* Clear any Tulkas-related quest data */
        p_ptr->tulkas_target_r_idx = 0;
        p_ptr->tulkas_prize_a_idx = 0;
    }
    
    /* Reset entrance-based quests (Niena) - similar to Tulkas, spawns during generation */
    if (p_ptr->niena_quest == NIENA_QUEST_GIVER_PRESENT) {
        log_trace("Quest vault regeneration: Resetting Niena quest from GIVER_PRESENT to NOT_STARTED");
        p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
        /* Clear Niena-related quest data */
        p_ptr->niena_monsters_seen = 0;
        p_ptr->niena_monsters_killed = 0;
    }

    /* Reset entrance-based quests (Varda) - spawns during generation on early depths */
    if (p_ptr->varda_quest == VARDA_QUEST_GIVER_PRESENT && p_ptr->varda_level == p_ptr->depth) {
        log_trace("Quest vault regeneration: Resetting Varda quest from GIVER_PRESENT to NOT_STARTED (level %d)", p_ptr->depth);
        p_ptr->varda_quest = VARDA_QUEST_NOT_STARTED;
        p_ptr->varda_level = 0;
    }
    if (varda_shadow_state == QUEST_STATE_GIVER_PRESENT && p_ptr->varda_shadow_level == p_ptr->depth) {
        log_trace("Quest vault regeneration: Resetting Varda shadow quest from GIVER_PRESENT to NOT_STARTED (level %d)", p_ptr->depth);
        quest_set_state(QUEST_ID_VARDA_SHADOW, QUEST_STATE_NOT_STARTED);
        p_ptr->varda_shadow_level = 0;
    }
    
    /* CRITICAL: Preserve quest lottery result during regeneration */
    /* The lottery determines which quest (if any) owns this level and should persist */
    /* across all regeneration attempts until the quest succeeds or we abandon the level */
    
    /* Reset quest states to allow fresh placement attempts, but preserve reservation */
    bool quest_active = (quest_lottery_winner > 0) ||
                        (p_ptr->tulkas_quest > TULKAS_QUEST_NOT_STARTED) ||
                        (quest_get_state(QUEST_ID_TULKAS_ORCS) > QUEST_STATE_NOT_STARTED) ||
                        (p_ptr->tulkas_stronghold_level > 0) ||
                        (p_ptr->niena_quest > NIENA_QUEST_NOT_STARTED) ||
                        (p_ptr->orome_quest > OROME_QUEST_NOT_STARTED) ||
                        (p_ptr->aule_quest > AULE_QUEST_NOT_STARTED) ||
                        (p_ptr->mandos_quest > MANDOS_QUEST_NOT_STARTED) ||
                        (p_ptr->varda_quest > VARDA_QUEST_NOT_STARTED) ||
                        (varda_shadow_state > QUEST_STATE_NOT_STARTED);
    
    if (quest_active) {
        /* Keep quest_reserved[0] = 1 since a quest owns this level/run */
        if (!p_ptr->quest_reserved[0]) {
            p_ptr->quest_reserved[0] = 1;
            log_trace("Quest vault regeneration: Quest context active (lottery=%d) - ensuring quest_reserved[0] = 1", quest_lottery_winner);
        }
    } else if (preserve_run_quest_slot) {
        if (!p_ptr->quest_reserved[0]) {
            p_ptr->quest_reserved[0] = 1;
        }
        log_trace("Quest vault regeneration: Preserving run-wide quest lock across level regeneration");
    } else if (p_ptr->quest_reserved[0]) {
        log_trace("Quest vault regeneration: No quest owns this level - resetting quest_reserved[0] from 1 to 0");
        p_ptr->quest_reserved[0] = 0;
    }
    
    log_trace("Quest vault regeneration: END - quest_reserved[0]=%d, lottery_winner=%d", 
              p_ptr->quest_reserved[0], quest_lottery_winner);
}

/* Function to apply pending quest state changes when level generation is successful */
void apply_pending_quest_states(void) {
    if (pending_quest_states.has_aule_change) {
        p_ptr->aule_level = pending_quest_states.aule_level;
        p_ptr->aule_quest = AULE_QUEST_FORGE_PRESENT;
        p_ptr->quest_reserved[0] = 1; /* Mark that a quest has spawned this run */
        level_gen_debug_note_questgiver(QUEST_ID_AULE);
        log_trace("Aule quest: FORGE_PRESENT APPLIED (deferred from quest vault) at %d,%d depth=%d", 
                  pending_quest_states.aule_forge_y, pending_quest_states.aule_forge_x, pending_quest_states.aule_level);
    }
    if (pending_quest_states.has_mandos_change) {
        p_ptr->mandos_level = pending_quest_states.mandos_level;
        {
            int next_state = pending_quest_states.mandos_next_state;
            bool nonblocking = (pending_quest_states.mandos_quest_id == QUEST_ID_MANDOS_BETRAYER);
            if (next_state == 0) next_state = QUEST_STATE_GIVER_PRESENT;
            if (pending_quest_states.mandos_quest_id == QUEST_ID_MANDOS) {
                p_ptr->mandos_quest = next_state;
            } else {
                quest_set_state(pending_quest_states.mandos_quest_id, next_state);
            }
            if (!nonblocking) {
                p_ptr->quest_reserved[0] = 1;
            }
            level_gen_debug_note_questgiver(pending_quest_states.mandos_quest_id);
            log_trace("Mandos quest: state %d APPLIED (deferred from quest vault) for quest_id=%d at %d,%d depth=%d", 
                      next_state, pending_quest_states.mandos_quest_id,
                      pending_quest_states.mandos_vault_y, pending_quest_states.mandos_vault_x, pending_quest_states.mandos_level);
        }
    }
    if (pending_quest_states.has_varda_change) {
        p_ptr->varda_level = pending_quest_states.varda_level;
        p_ptr->varda_vault_placed = 1;
        p_ptr->varda_vault_ready = 0;
        p_ptr->quest_reserved[0] = 1; /* Mark that a quest has spawned this run */
        log_trace("Varda quest: Bastion placement APPLIED (deferred) at %d,%d depth=%d", 
                  pending_quest_states.varda_vault_y, pending_quest_states.varda_vault_x, pending_quest_states.varda_level);
    }
    if (pending_quest_states.has_varda_shadow_change) {
        p_ptr->varda_shadow_level = pending_quest_states.varda_shadow_level;
        p_ptr->varda_shadow_placed = 1;
        p_ptr->varda_shadow_ready = 0;
        p_ptr->quest_reserved[0] = 1;
        log_trace("Varda quest: Shadow Bastion placement APPLIED (deferred) at %d,%d depth=%d",
                  pending_quest_states.varda_shadow_y, pending_quest_states.varda_shadow_x, pending_quest_states.varda_shadow_level);
    }
    if (pending_quest_states.has_tulkas_change) {
        p_ptr->tulkas_stronghold_level = pending_quest_states.tulkas_level;
        quest_set_state(QUEST_ID_TULKAS_ORCS,
                        pending_quest_states.tulkas_next_state ? pending_quest_states.tulkas_next_state : QUEST_STATE_GIVER_PRESENT);
        p_ptr->tulkas_stronghold_placed = 1;
        p_ptr->tulkas_second_spawn_pending = pending_quest_states.tulkas_spawn_pending ? 1 : p_ptr->tulkas_second_spawn_pending;
        p_ptr->tulkas_orc_mask = 0;
        p_ptr->tulkas_orc_restricted = 1;
        p_ptr->quest_reserved[0] = 1;
        log_trace("Tulkas orc quest: Stronghold placement APPLIED (deferred) at depth=%d", pending_quest_states.tulkas_level);
    }
    
    /* Reset pending changes after applying them */
    reset_pending_quest_states();
}


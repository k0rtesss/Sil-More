#include "angband.h"
#include "externs.h"
#include "metarun.h"
#include "quest/quest-runtime.h"
#include "quest/quest-internal.h"
#include "quest/quest-rewards-beta.h"
#include "quest/quest-challenges.h"
#include "log/log.h"

/* Stable quest IDs are shared by templates, save data and Tale counters. */
#define Q_FIRST 7
#define Q_LAST 16
#define Q_INDEX(id) ((id) - Q_FIRST)
#define Q_PLACED 1
#define Q_FAILED 2
#define Q_ATTACKED 4
#define Q_TEMP_MERCY 8
extern float calculate_parametric_probability(quest_type *q, int depth);

static u16b pending_vaults;
static int debug_vault;
static const int orc_targets[] = {54, 76, 84, 85, 95, 105};
static const int hunt_targets[] = {R_IDX_SCATHA, R_IDX_SMAUG,
    R_IDX_DRAUGLUIN, R_IDX_GOSTIR, R_IDX_SHELOB, R_IDX_THURINGWETHIL};

static bool followup_id(int id) { return id >= Q_FIRST && id <= Q_LAST; }
int quest_debug_vault_requested(void) { return quest_debug_sandbox() ? debug_vault : 0; }
void quest_debug_request_vault(int id) { debug_vault = followup_id(id) ? id : 0; }

byte quest_get_state(int id)
{
    if (!p_ptr) return QUEST_STATE_NOT_STARTED;
    switch (id) {
    case 1: return p_ptr->tulkas_quest;
    case 2: return p_ptr->aule_quest == AULE_QUEST_REWARDED ? QUEST_STATE_REWARDED
        : p_ptr->aule_quest == AULE_QUEST_FAILED ? 5 : p_ptr->aule_quest;
    case 3: return p_ptr->mandos_quest;
    case 4: return p_ptr->niena_quest;
    case 5: return p_ptr->orome_quest;
    case 6: return p_ptr->varda_quest;
    default: return followup_id(id) ? p_ptr->quest_followup_state[Q_INDEX(id)] : 0;
    }
}

void quest_set_state(int id, byte state)
{
    if (!p_ptr || state > QUEST_STATE_REWARDED) return;
    switch (id) {
    case 1: p_ptr->tulkas_quest = state; break;
    case 2: p_ptr->aule_quest = state == QUEST_STATE_REWARDED ? AULE_QUEST_REWARDED : state; break;
    case 3: p_ptr->mandos_quest = state; break;
    case 4: p_ptr->niena_quest = state; break;
    case 5: p_ptr->orome_quest = state; break;
    case 6: p_ptr->varda_quest = state; break;
    default: if (followup_id(id)) p_ptr->quest_followup_state[Q_INDEX(id)] = state; break;
    }
}

u32b quest_metarun_flag(int id)
{
    return id >= 1 && id <= Q_LAST ? 1UL << (id - 1) : 0;
}

int quest_id_for_vala_stage(int vala, int stage)
{
    if (!quest_info || !z_info) return 0;
    for (int id = 1; id < z_info->quest_max; ++id)
        if (quest_info[id].name && quest_info[id].vala_id == vala
            && quest_info[id].sequence == stage) return id;
    return 0;
}

cptr quest_display_title(int id)
{
    if (!quest_info || !z_info || id <= 0 || id >= z_info->quest_max)
        return "Unknown quest";
    if (quest_info[id].name && quest_name_text)
        return quest_name_text + quest_info[id].name;
    return "Unknown quest";
}

int quest_completion_cap(int id)
{
    if (id <= 6 && !quest_rules_enabled()) return METARUN_QUEST_COMPLETION_CAP;
    if (!quest_info || !z_info || id <= 0 || id >= z_info->quest_max)
        return METARUN_QUEST_COMPLETION_CAP;
    int cap = quest_info[id].completion_cap;
    return cap > 0 && cap <= METARUN_QUEST_COMPLETION_CAP ? cap : METARUN_QUEST_COMPLETION_CAP;
}

bool quest_debug_sandbox(void) { return p_ptr && p_ptr->quest_test_sandbox; }

static int giver_for(int id)
{
    static const int givers[] = {R_IDX_TULKAS, R_IDX_AULE, R_IDX_MANDOS,
        R_IDX_NIENA, R_IDX_OROME, R_IDX_VARDA};
    if (!quest_info || !z_info || id <= 0 || id >= z_info->quest_max) return 0;
    int vala = quest_info[id].vala_id;
    return vala > 0 && vala <= (int)N_ELEMENTS(givers) ? givers[vala - 1] : 0;
}

static bool done(int id)
{
    return quest_get_state(id) == QUEST_STATE_REWARDED
        || metarun_quest_completion_count(quest_metarun_flag(id)) > 0;
}

static bool oath_active(int oath)
{
    return p_ptr->oath_type == oath && !oath_invalid(oath);
}

static bool prerequisites(int id)
{
    if (quest_debug_sandbox()) return true;
    quest_type *q = &quest_info[id];
    int prior = quest_id_for_vala_stage(q->vala_id, q->sequence - 1);
    if (prior && !done(prior)) return false;
    /* Rules can be tested separately from the quest objectives. */
    if (!quest_rules_enabled()) return true;
    switch (id) {
    case QUEST_ID_MANDOS_TRAITOR: return oath_active(OATH_IRON);
    case QUEST_ID_MANDOS_BETRAYER:
        return oath_active(OATH_IRON) && quest_challenge_completion_count(CHALLENGE_DISCONNECTED) > 0;
    case QUEST_ID_TULKAS_MORGOTH: return oath_active(OATH_VALOROUS);
    case QUEST_ID_VARDA_UNGOLIANT: return oath_active(OATH_LIGHT);
    default: return true;
    }
}

static bool race_available(int race)
{
    return z_info && r_info && race > 0 && race < z_info->r_max
        && r_info[race].max_num > 0;
}

bool quest_followup_eligible(int id, int depth)
{
    if (!p_ptr || !followup_id(id) || !quest_enabled(id) || !quest_info
        || !z_info || id >= z_info->quest_max || !quest_info[id].name) return false;
    if (quest_get_state(id) != QUEST_STATE_NOT_STARTED || !prerequisites(id)) return false;
    if (p_ptr->quest_followup_flags[Q_INDEX(id)] & Q_FAILED) return false;
    if (id == QUEST_ID_NIENA_MORGOTH && (p_ptr->quest_lifetime_flags & 2)) return false;
    if (!quest_debug_sandbox() && metarun_quest_completion_count(quest_metarun_flag(id)) >= quest_completion_cap(id)) return false;
    if (id == QUEST_ID_NIENA_PACIFIST && (p_ptr->quest_lifetime_flags & 1)) return false;
    quest_type *q = &quest_info[id];
    if (q->eligibility_type == 3 && (depth < q->eligibility_depth_min || depth > q->eligibility_depth_max)) return false;
    switch (id) {
    case QUEST_ID_MANDOS_TRAITOR: return race_available(R_IDX_ULFANG) && race_available(R_IDX_ULDOR);
    case QUEST_ID_MANDOS_BETRAYER: return race_available(R_IDX_MAEGLIN);
    case QUEST_ID_TULKAS_ORCS:
        for (int i = 0; i < (int)N_ELEMENTS(orc_targets); ++i)
            if (!race_available(orc_targets[i]) || r_info[orc_targets[i]].cur_num) return false;
        break;
    case QUEST_ID_VARDA_SHADOW: return race_available(R_IDX_BELEGWATH);
    case QUEST_ID_VARDA_UNGOLIANT: return race_available(R_IDX_UNGOLIANT);
    }
    return true;
}

static void show_text(int id, bool completion)
{
    int count = 0;
    cptr *texts = completion ? extract_quest_completion_texts(id, &count)
        : extract_quest_init_texts(id, &count);
    if (texts && count) quest_typewriter_menu(quest_display_title(id), texts, count,
        completion ? TERM_L_GREEN : TERM_YELLOW, TERM_WHITE);
    free_quest_texts(texts, count);
}

static void meta_hunt_save(byte mask, bool active)
{
    if (quest_debug_sandbox()) return;
    metarun *current = metarun_current_mutable();
    if (!current) return;
    mask |= metar.quest_reserved[1];
    if (metar.quest_reserved[1] == mask && metar.quest_reserved[2] == active) return;
    metar.quest_reserved[1] = current->quest_reserved[1] = mask;
    metar.quest_reserved[2] = current->quest_reserved[2] = active;
    save_metaruns();
}

static void revoke_mercy(void)
{
    byte *flags = &p_ptr->quest_followup_flags[Q_INDEX(QUEST_ID_NIENA_MORGOTH)];
    if (!(*flags & Q_TEMP_MERCY)) return;
    *flags &= ~Q_TEMP_MERCY;
    /* Do not take away a permanent reward earned while the loan was active. */
    if (!p_ptr->innate_ability[S_SPC][SPC_NIENA_MERCY]) {
        p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY] = false;
        p_ptr->active_ability[S_SPC][SPC_NIENA_MERCY] = false;
    }
    p_ptr->update |= PU_BONUS;
}

static bool complete(int id, bool dialogue)
{
    if (!quest_enabled(id) || quest_get_state(id) != QUEST_STATE_SUCCESS
        || (p_ptr->quest_followup_flags[Q_INDEX(id)] & Q_FAILED)) return false;
    /* The choice may be cancelled. Keep the reward pending in that case. */
    if (id == QUEST_ID_VARDA_SHADOW && quest_rewards_enabled()
        && !quest_varda_radiant_gift()) return false;
    if (dialogue) show_text(id, true);
    quest_set_state(id, QUEST_STATE_REWARDED);
    apply_quest_rewards(id);
    quest_beta_apply_reward(id);
    quest_challenge_unlock_for_quest(id);
    metarun_mark_quest_completed(quest_metarun_flag(id));
    if (id == QUEST_ID_OROME_GREAT_HUNT)
        meta_hunt_save((byte)p_ptr->quest_followup_progress[Q_INDEX(id)], false);
    if (id == QUEST_ID_NIENA_MORGOTH) revoke_mercy();
    do_cmd_note(format("Completed %s%s.", quest_display_title(id),
        quest_debug_sandbox() ? " (debug test)" : ""), p_ptr->depth);
    return true;
}

static void accept(int id, bool dialogue)
{
    if (!followup_id(id) || !quest_enabled(id)
        || (p_ptr->quest_followup_flags[Q_INDEX(id)] & Q_FAILED)
        || (!quest_debug_sandbox() && !quest_can_accept_more())) return;
    if (dialogue) show_text(id, false);
    quest_set_state(id, QUEST_STATE_ACTIVE);
    int progress = p_ptr->quest_followup_progress[Q_INDEX(id)];
    if ((id == QUEST_ID_MANDOS_TRAITOR && progress == 3)
        || (id == QUEST_ID_MANDOS_BETRAYER && progress)
        || (id == QUEST_ID_TULKAS_ORCS && progress == 63)
        || (id == QUEST_ID_VARDA_SHADOW && progress))
        quest_set_state(id, QUEST_STATE_SUCCESS);
    if (id == QUEST_ID_OROME_GREAT_HUNT) {
        p_ptr->quest_followup_progress[Q_INDEX(id)] = quest_debug_sandbox() ? 0 : metar.quest_reserved[1];
        meta_hunt_save((byte)p_ptr->quest_followup_progress[Q_INDEX(id)], true);
    }
    if (id == QUEST_ID_NIENA_MORGOTH && quest_rewards_enabled()
        && !p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY]) {
        p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY] = true;
        p_ptr->active_ability[S_SPC][SPC_NIENA_MERCY] = true;
        p_ptr->quest_followup_flags[Q_INDEX(id)] |= Q_TEMP_MERCY;
        p_ptr->update |= PU_BONUS;
    }
    do_cmd_note(format("Accepted %s.", quest_display_title(id)), p_ptr->depth);
}

bool quest_followup_interaction(int giver)
{
    for (int id = Q_FIRST; id <= Q_LAST; ++id) {
        if (!quest_enabled(id) || giver_for(id) != giver
            || (p_ptr->quest_followup_flags[Q_INDEX(id)] & Q_FAILED)) continue;
        byte state = quest_get_state(id);
        if (state == QUEST_STATE_GIVER_PRESENT) {
            if (!quest_debug_sandbox() && !quest_can_accept_more()) { msg_print("Finish an active quest before accepting another."); return true; }
            accept(id, true);
            remove_quest_giver_silent(giver);
            return true;
        }
        if (state == QUEST_STATE_SUCCESS) {
            if (complete(id, true)) remove_quest_giver_silent(giver);
            return true;
        }
    }
    return false;
}

void quest_followup_update(void)
{
    if (!p_ptr || !quest_info) return;
    quest_challenge_validate();
    if (!quest_enabled(QUEST_ID_NIENA_MORGOTH) || !quest_rewards_enabled()) revoke_mercy();
    for (int id = Q_FIRST; id <= Q_LAST; ++id) {
        if (!quest_enabled(id) || (p_ptr->quest_followup_flags[Q_INDEX(id)] & Q_FAILED)) continue;
        byte state = quest_get_state(id);
        if (state == QUEST_STATE_ACTIVE &&
            ((id == QUEST_ID_NIENA_MORGOTH && (p_ptr->quest_lifetime_flags & 2))
             || (id == QUEST_ID_NIENA_PACIFIST && (p_ptr->quest_lifetime_flags & 1)))) {
            p_ptr->quest_followup_flags[Q_INDEX(id)] |= Q_FAILED;
            if (id == QUEST_ID_NIENA_MORGOTH) revoke_mercy();
            continue;
        }
        bool global = (quest_info[id].quest_flags & QUEST_FLAG_GLOBAL) != 0;
        if (id == QUEST_ID_OROME_GREAT_HUNT && state == QUEST_STATE_ACTIVE && !quest_debug_sandbox())
            p_ptr->quest_followup_progress[Q_INDEX(id)] |= metar.quest_reserved[1];
        if (global && state == QUEST_STATE_NOT_STARTED && quest_followup_eligible(id, p_ptr->depth)
            && quest_can_accept_more()) { accept(id, true); state = quest_get_state(id); }
        if (state == QUEST_STATE_ACTIVE && id == QUEST_ID_OROME_GREAT_HUNT
            && p_ptr->quest_followup_progress[Q_INDEX(id)] == 63) {
            quest_set_state(id, QUEST_STATE_SUCCESS); state = QUEST_STATE_SUCCESS;
        }
        if (state == QUEST_STATE_SUCCESS && global) { complete(id, true); continue; }
        if (state != QUEST_STATE_GIVER_PRESENT && state != QUEST_STATE_SUCCESS) continue;
        int giver = giver_for(id);
        if (!is_quest_giver_present(giver))
            ensure_reward_quest_giver_near_player(giver, 3, quest_display_title(id), NULL, NULL, NULL);
        for (int y = p_ptr->py - 1; y <= p_ptr->py + 1; ++y)
            for (int x = p_ptr->px - 1; x <= p_ptr->px + 1; ++x)
                if (in_bounds(y, x) && cave_m_idx[y][x] > 0
                    && mon_list[cave_m_idx[y][x]].r_idx == giver) {
                    quest_followup_interaction(giver); return;
                }
    }
}

void quest_followup_kill(int race)
{
    if (!p_ptr || !r_info || !z_info || race <= 0 || race >= z_info->r_max) return;
    /* Pacifism covers this entire life, including kills before the offer. */
    p_ptr->quest_lifetime_flags |= 1;
    /* Called only for a player-attributed death. Counters never advance on a paused quest. */
    for (int id = Q_FIRST; id <= Q_LAST; ++id) {
        if (!quest_enabled(id) || (quest_get_state(id) != QUEST_STATE_ACTIVE
            && quest_get_state(id) != QUEST_STATE_GIVER_PRESENT)
            || (p_ptr->quest_followup_flags[Q_INDEX(id)] & Q_FAILED)) continue;
        int idx = Q_INDEX(id);
        u16b *progress = &p_ptr->quest_followup_progress[idx];
        bool success = false;
        switch (id) {
        case QUEST_ID_MANDOS_TRAITOR:
            if (race == R_IDX_ULFANG) *progress |= 1;
            if (race == R_IDX_ULDOR) *progress |= 2;
            success = *progress == 3; break;
        case QUEST_ID_MANDOS_BETRAYER: if (race == R_IDX_MAEGLIN) *progress = 1; success = *progress != 0; break;
        case QUEST_ID_OROME_DRAGONS:
            if ((r_info[race].flags3 & RF3_DRAGON) && !strstr(r_name + r_info[race].name, "hatchling"))
                if (*progress < 10) ++*progress;
            success = *progress >= 10; break;
        case QUEST_ID_OROME_GREAT_HUNT:
            for (int i = 0; i < (int)N_ELEMENTS(hunt_targets); ++i)
                if (race == hunt_targets[i]) *progress |= 1U << i;
            meta_hunt_save((byte)*progress, true);
            success = *progress == 63; break;
        case QUEST_ID_NIENA_PACIFIST:
            p_ptr->quest_followup_flags[idx] |= Q_FAILED; break;
        case QUEST_ID_TULKAS_ORCS:
            for (int i = 0; i < (int)N_ELEMENTS(orc_targets); ++i)
                if (race == orc_targets[i]) *progress |= 1U << i;
            success = *progress == 63; break;
        case QUEST_ID_VARDA_SHADOW: if (race == R_IDX_BELEGWATH) *progress = 1; success = *progress != 0; break;
        case QUEST_ID_VARDA_UNGOLIANT: success = race == R_IDX_UNGOLIANT; break;
        }
        if (success && quest_get_state(id) == QUEST_STATE_ACTIVE) { quest_set_state(id, QUEST_STATE_SUCCESS); msg_format("Quest complete: %s.", quest_display_title(id)); }
    }
}

void quest_followup_damage(monster_type *monster, int damage, int who)
{
    if (!p_ptr || !monster || monster->r_idx != R_IDX_MORGOTH || who >= 0) return;
    p_ptr->quest_lifetime_flags |= 2;
    if (quest_enabled(QUEST_ID_NIENA_MORGOTH) && quest_get_state(QUEST_ID_NIENA_MORGOTH) == QUEST_STATE_ACTIVE) {
        p_ptr->quest_followup_flags[Q_INDEX(QUEST_ID_NIENA_MORGOTH)] |= Q_ATTACKED;
        revoke_mercy();
    }
    if (damage <= 0 || monster->maxhp <= 0 || !quest_enabled(QUEST_ID_TULKAS_MORGOTH)
        || quest_get_state(QUEST_ID_TULKAS_MORGOTH) != QUEST_STATE_ACTIVE
        || (p_ptr->quest_followup_flags[Q_INDEX(QUEST_ID_TULKAS_MORGOTH)] & Q_FAILED)) return;
    /* Accumulate actual player damage, excluding damage already dealt by monsters. */
    int dealt = MIN(damage, MAX(0, monster->hp));
    u16b *progress = &p_ptr->quest_followup_progress[Q_INDEX(QUEST_ID_TULKAS_MORGOTH)];
    *progress = (u16b)MIN(65535, (int)*progress + dealt);
    if (*progress >= (monster->maxhp + 1) / 2 && monster->hp - damage <= monster->maxhp / 2)
        quest_set_state(QUEST_ID_TULKAS_MORGOTH, QUEST_STATE_SUCCESS);
}

void quest_followup_escape(void)
{
    for (int id = QUEST_ID_NIENA_MORGOTH; id <= QUEST_ID_NIENA_PACIFIST; ++id) {
        if (!quest_enabled(id) || quest_get_state(id) != QUEST_STATE_ACTIVE
            || (p_ptr->quest_followup_flags[Q_INDEX(id)] & Q_FAILED)) continue;
        byte flags = p_ptr->quest_followup_flags[Q_INDEX(id)];
        bool success = !(flags & (Q_ATTACKED | Q_FAILED));
        if (id == QUEST_ID_NIENA_MORGOTH) success = success && !(p_ptr->quest_lifetime_flags & 2);
        if (id == QUEST_ID_NIENA_PACIFIST) success = success && !(p_ptr->quest_lifetime_flags & 1);
        if (success && silmarils_possessed() > 0) { quest_set_state(id, QUEST_STATE_SUCCESS); complete(id, true); }
    }
    quest_challenge_record_escape();
}

void quest_followup_leave(int new_depth)
{
    if (p_ptr->depth == MORGOTH_DEPTH && new_depth < MORGOTH_DEPTH
        && silmarils_possessed() > 0 && quest_enabled(QUEST_ID_NIENA_MORGOTH)
        && quest_get_state(QUEST_ID_NIENA_MORGOTH) == QUEST_STATE_ACTIVE
        && !(p_ptr->quest_lifetime_flags & 2)
        && !(p_ptr->quest_followup_flags[Q_INDEX(QUEST_ID_NIENA_MORGOTH)] & (Q_ATTACKED | Q_FAILED))) {
        quest_set_state(QUEST_ID_NIENA_MORGOTH, QUEST_STATE_SUCCESS);
        complete(QUEST_ID_NIENA_MORGOTH, true);
    }
    quest_followup_discard_level();
}

/* The map is going away even when a quest is paused. The player's depth may
 * already name the destination (falls, teleportation), or remain unchanged
 * during regeneration. Only committed, unfinished vault objectives are lost. */
void quest_followup_discard_level(void)
{
    for (int id = Q_FIRST; id <= Q_LAST; ++id) {
        byte *flags = &p_ptr->quest_followup_flags[Q_INDEX(id)];
        if (!(*flags & Q_PLACED) || (*flags & Q_FAILED)) continue;
        byte state = quest_get_state(id);
        if (state == QUEST_STATE_ACTIVE || state == QUEST_STATE_GIVER_PRESENT) {
            *flags |= Q_FAILED;
            if (quest_enabled(id))
                msg_format("You leave %s unfinished.", quest_display_title(id));
        }
    }
}

void quest_followup_reset(void)
{
    memset(p_ptr->quest_followup_state, 0, sizeof(p_ptr->quest_followup_state));
    memset(p_ptr->quest_followup_flags, 0, sizeof(p_ptr->quest_followup_flags));
    memset(p_ptr->quest_followup_depth, 0, sizeof(p_ptr->quest_followup_depth));
    memset(p_ptr->quest_followup_progress, 0, sizeof(p_ptr->quest_followup_progress));
    p_ptr->quest_followup_recorded = 0;
    p_ptr->quest_test_sandbox = 0;
    p_ptr->quest_lifetime_flags = 0;
    pending_vaults = 0; debug_vault = 0;
}

int quest_followup_vault(int vault)
{
    switch (vault) {
    case 464: return QUEST_ID_MANDOS_BETRAYER;
    case 465: return QUEST_ID_MANDOS_TRAITOR;
    case 466: return QUEST_ID_TULKAS_ORCS;
    case 467: return QUEST_ID_VARDA_SHADOW;
    default: return 0;
    }
}

bool quest_followup_vault_allowed(int vault, int depth)
{
    int id = quest_followup_vault(vault);
    if (!id) return true;
    if (!quest_enabled(id)) return false;
    if (debug_vault == id && quest_debug_sandbox()) return true;
    if (id == QUEST_ID_VARDA_SHADOW)
        return quest_get_state(id) == QUEST_STATE_ACTIVE && depth > 15
            && !(p_ptr->quest_followup_flags[Q_INDEX(id)] & (Q_PLACED | Q_FAILED));
    return quest_followup_eligible(id, depth) && quest_can_initiate_more();
}

void quest_followup_vault_placed(int id, int depth)
{
    (void)depth;
    if (followup_id(id)) pending_vaults |= 1U << Q_INDEX(id);
}

void quest_followup_generation_begin(void) { pending_vaults = 0; }

void quest_followup_generation_commit(void)
{
    for (int id = Q_FIRST; id <= Q_LAST; ++id) {
        if (!(pending_vaults & (1U << Q_INDEX(id)))) continue;
        p_ptr->quest_followup_flags[Q_INDEX(id)] |= Q_PLACED;
        p_ptr->quest_followup_depth[Q_INDEX(id)] = p_ptr->depth;
        if (quest_get_state(id) == QUEST_STATE_NOT_STARTED) {
            quest_set_state(id, QUEST_STATE_GIVER_PRESENT);
            quest_note_initiated(id);
        }
        if (debug_vault == id) debug_vault = 0;
    }
    pending_vaults = 0;
}

void quest_followup_generate_giver(void)
{
    if (p_ptr->on_the_run || !quest_can_initiate_more()) return;
    /* Called after successful generation. Disabled entries consume no RNG. */
    const int ids[] = {QUEST_ID_OROME_DRAGONS, QUEST_ID_VARDA_SHADOW, QUEST_ID_NIENA_MORGOTH};
    for (int i = 0; i < (int)N_ELEMENTS(ids); ++i) {
        int id = ids[i];
        if (!quest_followup_eligible(id, p_ptr->depth)) continue;
        if (id != QUEST_ID_NIENA_MORGOTH) {
            float chance = calculate_parametric_probability(&quest_info[id], p_ptr->depth);
            if (rand_int(10000) >= (int)(chance * 10000)) continue;
        }
        if (spawn_quest_giver_near_player(giver_for(id))) {
            quest_set_state(id, QUEST_STATE_GIVER_PRESENT); quest_note_initiated(id); return;
        }
    }
}

bool quest_followup_reserve_race(int race)
{
    if (p_ptr && (p_ptr->depth <= 6 || quest_get_state(QUEST_ID_TULKAS_ORCS) > 0)
        && quest_enabled(QUEST_ID_TULKAS_ORCS) && prerequisites(QUEST_ID_TULKAS_ORCS)
        && quest_get_state(QUEST_ID_TULKAS_ORCS) < QUEST_STATE_REWARDED
        && !(p_ptr->quest_followup_flags[Q_INDEX(QUEST_ID_TULKAS_ORCS)] & Q_FAILED))
        for (int i = 0; i < (int)N_ELEMENTS(orc_targets); ++i)
            if (race == orc_targets[i]) return true;
    if (p_ptr && (p_ptr->depth <= 3 || quest_get_state(QUEST_ID_VARDA_SHADOW) > 0)
        && quest_enabled(QUEST_ID_VARDA_SHADOW) && prerequisites(QUEST_ID_VARDA_SHADOW)
        && quest_get_state(QUEST_ID_VARDA_SHADOW) < QUEST_STATE_REWARDED
        && !(p_ptr->quest_followup_flags[Q_INDEX(QUEST_ID_VARDA_SHADOW)] & Q_FAILED)
        && race == R_IDX_BELEGWATH) return true;
    return false;
}

void quest_debug_status(int id, char *buf, size_t size)
{
    static cptr states[] = {"Not started", "Offered", "Active", "Reward pending", "Rewarded"};
    byte state = quest_get_state(id);
    strnfmt(buf, size, "%s%s | %s | progress %u | Tale %d/%d%s",
        quest_enabled(id) ? "Enabled" : "Disabled", quest_debug_sandbox() ? " (sandbox)" : "",
        state <= QUEST_STATE_REWARDED ? states[state] : "Failed",
        followup_id(id) ? p_ptr->quest_followup_progress[Q_INDEX(id)] : 0,
        metarun_quest_completion_count(quest_metarun_flag(id)), quest_completion_cap(id),
        followup_id(id) && (p_ptr->quest_followup_flags[Q_INDEX(id)] & Q_FAILED) ? " | Failed" : "");
}

void quest_debug_reset(int id)
{
    if (id < 1 || id > Q_LAST) return;
    p_ptr->quest_test_sandbox = 1;
    if (id <= 6) {
        if (id == QUEST_ID_TULKAS && valar_reserved_artifacts
            && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max)
            valar_reserved_artifacts[p_ptr->tulkas_prize_a_idx] = false;
        quest_set_state(id, 0);
        return;
    }
    if (id == QUEST_ID_NIENA_MORGOTH) { revoke_mercy(); p_ptr->quest_lifetime_flags &= ~2; }
    if (id == QUEST_ID_NIENA_PACIFIST) p_ptr->quest_lifetime_flags &= ~1;
    p_ptr->quest_followup_state[Q_INDEX(id)] = 0;
    p_ptr->quest_followup_flags[Q_INDEX(id)] = 0;
    p_ptr->quest_followup_depth[Q_INDEX(id)] = 0;
    p_ptr->quest_followup_progress[Q_INDEX(id)] = 0;
    /* Retain reward marker: reset is an objective replay, never a Tale rollback. */
}

void quest_debug_start(int id)
{
    if (id < 1 || id > Q_LAST || !quest_enabled(id)) { msg_print("Enable this quest in Quest Options first."); return; }
    quest_debug_reset(id);
    if (id <= 6) {
        quest_set_state(id, 1);
        if (id == QUEST_ID_AULE) {
            cave_set_feat(p_ptr->py, p_ptr->px, FEAT_FORGE_UNIQUE_HEAD + 3);
            p_ptr->aule_forge_y = p_ptr->py;
            p_ptr->aule_forge_x = p_ptr->px;
            p_ptr->aule_level = p_ptr->depth;
        }
        spawn_quest_giver_near_player(giver_for(id));
        return;
    }
    if (quest_info[id].quest_flags & QUEST_FLAG_GLOBAL) accept(id, true);
    else { quest_set_state(id, QUEST_STATE_GIVER_PRESENT); spawn_quest_giver_near_player(giver_for(id)); }
}

void quest_debug_complete(int id)
{
    if (id < 1 || id > Q_LAST || !quest_enabled(id)) return;
    p_ptr->quest_test_sandbox = 1;
    if (id <= 6) {
        if (id == QUEST_ID_TULKAS && p_ptr->tulkas_prize_a_idx <= 0) {
            msg_print("Accept Tulkas's quest to choose a valid target and prize first."); return;
        }
        quest_set_state(id, 3);
        spawn_quest_giver_near_player(giver_for(id));
        switch (id) {
        case 1: tulkas_quest_interaction(); break;
        case 2: aule_quest_interaction(); break;
        case 3: mandos_quest_interaction(); break;
        case 4: niena_quest_interaction(); break;
        case 5: orome_quest_interaction(); break;
        case 6: varda_quest_interaction(); break;
        }
        return;
    }
    quest_set_state(id, QUEST_STATE_SUCCESS);
    quest_followup_update();
}

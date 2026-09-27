#include "angband.h"
#include "externs.h"
#include "quest/quest-challenges.h"
#include "quest/quest-runtime.h"
#include "metarun.h"
#include "blitz.h"

#define CHALLENGE_COUNT_BASE 8
#define CHALLENGE_UNLOCK_SLOT 15

static int challenge_quest(int challenge)
{
    static const int quests[] = { 0, QUEST_ID_MANDOS_TRAITOR,
        QUEST_ID_OROME_DRAGONS, QUEST_ID_NIENA_MORGOTH,
        QUEST_ID_TULKAS_ORCS, QUEST_ID_VARDA_SHADOW };
    return challenge > 0 && challenge < (int)N_ELEMENTS(quests)
        ? quests[challenge] : 0;
}

void quest_challenge_validate(void)
{
    if (!p_ptr || !p_ptr->quest_challenge || p_ptr->quest_challenge_failed) return;
    if (!quest_challenges_enabled() || quest_debug_sandbox() || p_ptr->noscore) {
        p_ptr->quest_challenge_failed = 1;
        msg_print("This journey no longer qualifies for a quest challenge victory.");
    }
}

bool quest_challenge_active(int id)
{
    quest_challenge_validate();
    return p_ptr && id > 0 && id <= CHALLENGE_TORCHLIGHT
        && p_ptr->quest_challenge == id && quest_challenges_enabled();
}

int quest_challenge_completion_count(int id)
{
    if (!metarun_current() || id <= 0 || id > CHALLENGE_MAX_TRACKED) return 0;
    return metar.reserved_runtime[CHALLENGE_COUNT_BASE + id - 1];
}

bool quest_challenge_unlocked(int id)
{
    int quest = challenge_quest(id);
    if (!quest || !quest_challenges_enabled() || !metarun_current()) return false;
    return (metar.reserved_runtime[CHALLENGE_UNLOCK_SLOT] & (1U << (id - 1)))
        || metarun_quest_completion_count(quest_metarun_flag(quest)) > 0;
}

void quest_challenge_unlock_for_quest(int quest)
{
    metarun *current = metarun_current_mutable();
    if (!current || !quest_challenges_enabled() || !quest_enabled(quest)
        || quest_debug_sandbox() || run_mode_is_blitz() || p_ptr->noscore) return;
    for (int id = 1; id <= CHALLENGE_TORCHLIGHT; id++) {
        if (challenge_quest(id) != quest) continue;
        byte mask = 1U << (id - 1);
        if (metar.reserved_runtime[CHALLENGE_UNLOCK_SLOT] & mask) return;
        metar.reserved_runtime[CHALLENGE_UNLOCK_SLOT] |= mask;
        current->reserved_runtime[CHALLENGE_UNLOCK_SLOT]
            = metar.reserved_runtime[CHALLENGE_UNLOCK_SLOT];
        save_metaruns();
        msg_print("Your lineage has unlocked a new challenge for future journeys.");
    }
}

void quest_challenge_choose(void)
{
    int choices[6] = { 0 }, count = 1;
    cptr labels[6] = { "Ordinary journey" };
    static cptr names[] = { "Ordinary journey", "Disconnected stairs",
        "One staircase in each direction", "Fixed 50,000 experience",
        "Blunt weapons only", "Torches and Mallorn torches" };
    p_ptr->quest_challenge = p_ptr->quest_challenge_failed
        = p_ptr->quest_challenge_recorded = 0;
    if (!quest_challenges_enabled() || quest_debug_sandbox() || p_ptr->noscore) return;
    for (int id = 1; id <= CHALLENGE_TORCHLIGHT; id++) {
        if (!quest_challenge_unlocked(id)) continue;
        choices[count] = id;
        labels[count++] = names[id];
    }
    if (count == 1) return;
    cptr words[] = { "Choose an optional challenge unlocked by your lineage.",
        "Escape with a Silmaril to complete it. Disabling quest challenges forfeits the attempt.",
        "Blunt weapons forbids edged melee weapons, archery, and throwing. Torchlight allows torches, Mallorn torches, and artefact lights." };
    int chosen = quest_reward_book_choice("A New Trial", words, 3,
        "Choose your journey:", choices, labels, NULL, count, 0, NULL);
    if (chosen > 0 && chosen <= CHALLENGE_TORCHLIGHT && quest_challenge_unlocked(chosen))
        p_ptr->quest_challenge = chosen;
}

void quest_challenge_record_escape(void)
{
    quest_challenge_validate();
    int id = p_ptr->quest_challenge;
    metarun *current = metarun_current_mutable();
    if (!current || !quest_challenge_active(id) || p_ptr->quest_challenge_failed
        || p_ptr->quest_challenge_recorded
        || quest_debug_sandbox() || p_ptr->noscore || silmarils_possessed() <= 0) return;
    int slot = CHALLENGE_COUNT_BASE + id - 1;
    if (metar.reserved_runtime[slot] < 255) metar.reserved_runtime[slot]++;
    current->reserved_runtime[slot] = metar.reserved_runtime[slot];
    p_ptr->quest_challenge_recorded = 1;
    save_metaruns();
    msg_print("Your lineage remembers this challenge victory.");
}

void quest_debug_choose_challenge(void)
{
    const int choices[] = { 0, 1, 2, 3, 4, 5 };
    cptr labels[] = { "No challenge", "Disconnected stairs", "Single stair",
        "Fixed 50,000 XP", "Blunt weapons only", "Torchlight" };
    cptr words[] = { "Debug challenge fixtures never record lineage victories.",
        "Generate a fresh level to test stairs. Fixed XP resets available experience to 50,000." };
    int chosen = quest_reward_book_choice("Test a Challenge", words, 2,
        "Choose a fixture:", choices, labels, NULL, 6, 0, NULL);
    if (chosen < 0 || chosen > CHALLENGE_TORCHLIGHT) return;
    p_ptr->quest_test_sandbox = 1;
    op_ptr->opt[OPT_quest_challenges_beta] = true;
    p_ptr->quest_challenge = chosen;
    p_ptr->quest_challenge_failed = 1;
    p_ptr->quest_challenge_recorded = 0;
    if (chosen == CHALLENGE_FIXED_50K_XP) {
        p_ptr->exp = p_ptr->new_exp = PY_FIXED_EXP;
        check_experience();
    }
    quest_challenge_prune_stairs();
    p_ptr->update |= PU_BONUS | PU_UPDATE_VIEW;
    p_ptr->redraw |= PR_MAP;
}

bool quest_challenge_object_allowed(const object_type *object)
{
    if (!object || !object->k_idx) return true;
    if (quest_challenge_active(CHALLENGE_TULKAS_BLUNT)
        && (object->tval == TV_SWORD || object->tval == TV_POLEARM
            || object->tval == TV_BOW || object->tval == TV_ARROW)) return false;
    if (quest_challenge_active(CHALLENGE_TORCHLIGHT) && object->tval == TV_LIGHT) {
        if (k_info[object->k_idx].flags3 & TR3_INSTA_ART) return true;
        return object->sval == SV_LIGHT_TORCH || object->sval == SV_LIGHT_MALLORN;
    }
    return true;
}

bool quest_challenge_forbid_ranged(void)
{
    if (!quest_challenge_active(CHALLENGE_TULKAS_BLUNT)) return false;
    msg_print("This challenge permits only blunt melee weapons and unarmed blows.");
    return true;
}

bool quest_challenge_weapon_allowed(const object_type *weapon)
{
    return !quest_challenge_active(CHALLENGE_TULKAS_BLUNT)
        || !weapon || !weapon->k_idx || weapon->tval == TV_HAFTED;
}

int quest_challenge_melee_damage(const object_type *weapon, int damage)
{
    if (!quest_challenge_weapon_allowed(weapon)) {
        msg_print("Your chosen challenge forbids damage with this weapon.");
        return 0;
    }
    return damage;
}

/* Generation calls this after the entry stair and authored vaults exist.
 * Keep the arrival stair first, then the first authored stair in each direction. */
void quest_challenge_prune_stairs(void)
{
    int keep_y[2] = { -1, -1 }, keep_x[2] = { -1, -1 };
    if (!quest_challenge_active(CHALLENGE_SINGLE_STAIR) || p_ptr->depth == UTUMNO_DEPTH) return;
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++) {
            for (int x = 1; x < p_ptr->cur_map_wid - 1; x++) {
                int feat = cave_feat[y][x], direction;
                if (feat == FEAT_LESS || feat == FEAT_LESS_SHAFT) direction = 0;
                else if (feat == FEAT_MORE || feat == FEAT_MORE_SHAFT) direction = 1;
                else continue;
                bool arrival = y == p_ptr->py && x == p_ptr->px;
                if ((pass == 0) != arrival) continue;
                if (keep_y[direction] < 0) {
                    keep_y[direction] = y; keep_x[direction] = x;
                } else if (keep_y[direction] != y || keep_x[direction] != x) {
                    cave_set_feat(y, x, FEAT_FLOOR);
                }
            }
        }
    }
}

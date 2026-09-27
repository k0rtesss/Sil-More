#!/usr/bin/env python3
"""Compile production quest challenge/reward helpers and run focused C contracts.

Uses the existing Windows CMake include response file. No game launch or saved
player/Tale mutation. Outputs live in ignored scripts/output/quest-recovery.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/quest-recovery"
CHALLENGE_HARNESS = r"""
#include "angband.h"
#include "externs.h"
#include "metarun.h"
#include "quest/quest-runtime.h"
#include "quest/quest-challenges.h"
#include <assert.h>
#include <stdio.h>
player_type player_body;
player_type *p_ptr = &player_body;
metarun metar;
player_other options;
player_other *op_ptr = &options;
void check_experience(void) {}
static bool enabled = true, sandbox = false;
static int sils = 1, saves = 0;
bool quest_challenges_enabled(void) { return enabled; }
bool quest_debug_sandbox(void) { return sandbox; }
bool quest_enabled(int id) { (void)id; return true; }
bool run_mode_is_blitz(void) { return false; }
const metarun *metarun_current(void) { return &metar; }
metarun *metarun_current_mutable(void) { return &metar; }
int metarun_quest_completion_count(u32b flag) { (void)flag; return 0; }
u32b quest_metarun_flag(int id) { return 1U << id; }
errr save_metaruns(void) { saves++; return 0; }
int silmarils_possessed(void) { return sils; }
void msg_print(cptr text) { (void)text; }
static byte test_cave[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
byte (*cave_feat)[MAX_DUNGEON_WID] = test_cave;
object_kind *k_info;
void cave_set_feat(int y, int x, int feat) { cave_feat[y][x] = feat; }
int quest_reward_book_choice(cptr title, cptr words[], int n, cptr prompt,
    const int values[], cptr labels[], const bool allowed[], int count,
    int initial, void (*inspect)(int)) {
    (void)title; (void)words; (void)n; (void)prompt; (void)values; (void)labels;
    (void)allowed; (void)count; (void)initial; (void)inspect; return 0;
}
int main(void) {
    assert(!quest_challenge_unlocked(CHALLENGE_DISCONNECTED));
    enabled = false;
    quest_challenge_unlock_for_quest(QUEST_ID_MANDOS_TRAITOR);
    assert(metar.reserved_runtime[15] == 0);
    enabled = true;
    quest_challenge_unlock_for_quest(QUEST_ID_MANDOS_TRAITOR);
    assert(quest_challenge_unlocked(CHALLENGE_DISCONNECTED));
    assert(!quest_challenge_unlocked(CHALLENGE_SINGLE_STAIR));
    assert(saves == 1);
    p_ptr->quest_challenge = CHALLENGE_DISCONNECTED;
    sils = 0; quest_challenge_record_escape();
    assert(quest_challenge_completion_count(CHALLENGE_DISCONNECTED) == 0);
    sils = 1; quest_challenge_record_escape(); quest_challenge_record_escape();
    assert(quest_challenge_completion_count(CHALLENGE_DISCONNECTED) == 1);
    p_ptr->quest_challenge_recorded = 0;
    enabled = false; quest_challenge_validate(); enabled = true;
    assert(p_ptr->quest_challenge_failed);
    assert(quest_challenge_active(CHALLENGE_DISCONNECTED));
    quest_challenge_record_escape();
    assert(quest_challenge_completion_count(CHALLENGE_DISCONNECTED) == 1);
    p_ptr->quest_challenge_failed = 0;
    sandbox = true; quest_challenge_record_escape();
    assert(quest_challenge_completion_count(CHALLENGE_DISCONNECTED) == 1);
    sandbox = false; p_ptr->quest_challenge_failed = 0;
    p_ptr->quest_challenge = CHALLENGE_SINGLE_STAIR;
    p_ptr->depth = 2; p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 6;
    p_ptr->py = p_ptr->px = 2;
    cave_feat[1][1] = FEAT_LESS; cave_feat[2][2] = FEAT_LESS;
    cave_feat[3][3] = FEAT_MORE; cave_feat[4][4] = FEAT_MORE_SHAFT;
    quest_challenge_prune_stairs();
    assert(cave_feat[1][1] == FEAT_FLOOR && cave_feat[2][2] == FEAT_LESS);
    assert(cave_feat[3][3] == FEAT_MORE && cave_feat[4][4] == FEAT_FLOOR);
    puts("challenge unlock, Silmaril, once-only, disable/resume, debug isolation and arrival-stair preservation checks passed");
    return 0;
}
"""

def extract_function(source, name):
    import re
    match = re.search(r"(?m)^(?:static )?(?:bool|void|int) " + name + r"\([^;]*?\)\s*\{", source)
    if not match:
        raise AssertionError(f"Missing production helper {name}")
    start = match.start()
    depth = 1
    pos = match.end()
    while depth:
        if source[pos] == "{": depth += 1
        elif source[pos] == "}": depth -= 1
        pos += 1
    return source[start:pos]

def run_contract(name, sources):
    cc = Path(os.environ.get("CC", "C:/msys64/mingw64/bin/cc.exe"))
    env = os.environ.copy()
    env["PATH"] = str(cc.parent) + os.pathsep + "C:/msys64/usr/bin" + os.pathsep + env.get("PATH", "")
    target = OUT / (name + ".exe")
    subprocess.run([str(cc), "-std=c17", "-Wall", "-Wextra", "-DSIL_PERF_DIAGNOSTICS", "-DUSE_SDL",
        "@build-standard/CMakeFiles/sil-more.dir/includes_C.rsp", *map(str, sources), "-o", str(target)],
        cwd=ROOT, env=env, check=True)
    subprocess.run([str(target)], cwd=ROOT, env=env, check=True)

REWARD_SETUP = r"""
#include "angband.h"
#include "externs.h"
#include "metarun.h"
#include <assert.h>
#include <stdio.h>
player_type player_body;
player_type *p_ptr = &player_body;
metarun metar;
static bool rewards = true, lineage = true, sandbox = false, blitz = false;
static bool enabled_quests[17];
bool quest_rewards_enabled(void) { return rewards; }
bool quest_lineage_enabled(void) { return lineage; }
bool quest_debug_sandbox(void) { return sandbox; }
bool run_mode_is_blitz(void) { return blitz; }
bool quest_enabled(int id) { return enabled_quests[id]; }
const metarun *metarun_current(void) { return &metar; }
metarun *metarun_current_mutable(void) { return &metar; }
errr save_metaruns(void) { return 0; }
void msg_print(cptr text) { (void)text; }
#define STATUS_STUB(name) bool name(int value) { (void)value; return true; }
STATUS_STUB(set_blind) STATUS_STUB(set_confused) STATUS_STUB(set_poisoned)
STATUS_STUB(set_afraid) STATUS_STUB(set_entranced) STATUS_STUB(set_image)
STATUS_STUB(set_stun) STATUS_STUB(set_cut) STATUS_STUB(set_slow)
"""
REWARD_TEST = r"""
int main(void) {
    object_type spear = {0}; spear.tval = TV_POLEARM; spear.sval = SV_SPEAR;
    enabled_quests[QUEST_ID_OROME] = true;
    p_ptr->active_ability[S_SPC][SPC_OROME_WRAITH] = true;
    assert(quest_special_ability_active(SPC_OROME_WRAITH));
    enabled_quests[QUEST_ID_OROME] = false;
    assert(!quest_special_ability_active(SPC_OROME_WRAITH));
    enabled_quests[QUEST_ID_OROME_DRAGONS] = true;
    assert(quest_special_ability_active(SPC_OROME_WRAITH));
    rewards = false;
    assert(!quest_special_ability_active(SPC_OROME_WRAITH));
    rewards = true;
    enabled_quests[QUEST_ID_OROME_GREAT_HUNT] = true;
    p_ptr->active_ability[S_SPC][SPC_HUNTSMAN_RHYTHM] = true;
    quest_beta_bow_hit(5); quest_beta_bow_hit(5);
    assert(p_ptr->orome_spear_ready);
    assert(quest_beta_melee_damage(&spear, 9) == 18);
    assert(quest_beta_melee_damage(&spear, 9) == 9);
    quest_beta_bow_hit(5); quest_beta_bow_miss(); quest_beta_bow_hit(5);
    assert(!p_ptr->orome_spear_ready);
    quest_beta_bow_hit(5); rewards = false;
    assert(quest_beta_melee_damage(&spear, 9) == 9);
    quest_beta_bow_miss(); quest_beta_melee_miss();
    assert(p_ptr->orome_spear_ready && p_ptr->orome_bow_hit_streak == 2);
    rewards = true;
    assert(quest_beta_melee_damage(&spear, 9) == 18);
    quest_beta_bow_hit(5); quest_beta_bow_hit(0); quest_beta_bow_hit(5);
    assert(!p_ptr->orome_spear_ready);
    enabled_quests[QUEST_ID_MANDOS_BETRAYER] = true;
    enabled_quests[QUEST_ID_NIENA_PACIFIST] = true;
    enabled_quests[QUEST_ID_VARDA_UNGOLIANT] = true;
    lineage = false; quest_beta_apply_reward(QUEST_ID_MANDOS_BETRAYER);
    assert(metar.quest_reserved[GIFT_MANDOS] == 0);
    lineage = true; sandbox = true; quest_beta_apply_reward(QUEST_ID_MANDOS_BETRAYER);
    assert(metar.quest_reserved[GIFT_MANDOS] == 0);
    sandbox = false; blitz = true; quest_beta_apply_reward(QUEST_ID_MANDOS_BETRAYER);
    assert(metar.quest_reserved[GIFT_MANDOS] == 0);
    blitz = false; quest_beta_apply_reward(QUEST_ID_MANDOS_BETRAYER);
    p_ptr->mhp = 40; p_ptr->chp = -5;
    lineage = false; assert(!quest_beta_resurrect());
    assert(metar.quest_reserved[GIFT_MANDOS] == 1 && p_ptr->chp == -5);
    lineage = true; assert(quest_beta_resurrect());
    assert(p_ptr->chp == 40 && metar.quest_reserved[GIFT_MANDOS] == 0);
    p_ptr->chp = -1; assert(!quest_beta_resurrect());
    quest_beta_apply_reward(QUEST_ID_NIENA_PACIFIST);
    assert(quest_beta_cleanse_curse()); assert(!quest_beta_cleanse_curse());
    quest_beta_apply_reward(QUEST_ID_VARDA_UNGOLIANT);
    assert(metar.quest_reserved[GIFT_VARDA] == 1);
    puts("reward gates, Orome base/followup, rhythm, disabled-state preservation, lineage isolation and one-use gifts passed");
    return 0;
}
"""

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    challenge = OUT / "challenge-contract-test.c"
    challenge.write_text(CHALLENGE_HARNESS)
    run_contract("challenge-contract-test", [ROOT / "src/quest/quest-challenges.c", challenge])
    source = (ROOT / "src/quest/quest-rewards-beta.c").read_text()
    helpers = ["quest_special_ability_active", "lineage_active", "store_gift",
        "quest_beta_apply_reward", "quest_beta_resurrect", "quest_beta_cleanse_curse",
        "rhythm_active", "quest_beta_bow_hit", "quest_beta_bow_miss",
        "quest_beta_melee_damage", "quest_beta_melee_miss"]
    enum_start = source.index("enum { GIFT_MANDOS")
    enum_end = source.index(";", enum_start) + 1
    reward = OUT / "reward-contract-test.c"
    reward.write_text(REWARD_SETUP + source[enum_start:enum_end] + "\n"
        + "\n\n".join(extract_function(source, name) for name in helpers) + REWARD_TEST)
    run_contract("reward-contract-test", [reward])

if __name__ == "__main__":
    main()

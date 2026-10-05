#!/usr/bin/env python3
"""Check melee cancellation and reaction accounting with production combat code."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

import check_new_monsters as engine

CHECKS = r'''
#include "melee/melee-movement.h"
#include "melee/melee-attack.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr, "assertion failed: %s (line %d)\n", #expr, __LINE__); exit(1); } } while (0)
static int riposte_messages;
static bool accept_attack_prompt;
static int attack_prompts;
void __real_msg_print(cptr msg);
void __wrap_msg_print(cptr msg)
{
    if (msg && !strcmp(msg, "You riposte!")) riposte_messages++;
    __real_msg_print(msg);
}

bool __wrap_get_check_near(int y, int x, cptr prompt)
{
    (void)y; (void)x; (void)prompt;
    attack_prompts++;
    return accept_attack_prompt;
}

static monster_type* blunt_fixture(void)
{
    monster_type* m = combat_fixture(403, 0);
    p_ptr->quest_challenge = CHALLENGE_TULKAS_BLUNT;
    p_ptr->skill_use[S_MEL] = 1000;
    p_ptr->stat_use[A_STR] = 100;
    p_ptr->mdd = 2; p_ptr->mds = 20;
    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = 4;
    player_attacked = false;
    accept_attack_prompt = false;
    attack_prompts = 0;
    m->hp = m->maxhp = 20000;
    m->alertness = ALERTNESS_UNWARY;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    return m;
}

static void check_blunt_challenge(void)
{
    bool old_option = op_ptr->opt[OPT_quest_challenges_beta];
    op_ptr->opt[OPT_quest_challenges_beta] = true;
    monster_type* m = blunt_fixture();
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    move_player(4);
    assert(p_ptr->energy_use == 0 && !player_attacked && !combat_number);
    assert(m->hp == 20000 && m->alertness == ALERTNESS_UNWARY);
    assert(p_ptr->py == 10 && p_ptr->px == 11 && !attack_prompts);

    /* No permitted weapon must not trigger Smite, oath prompts, or attacks. */
    m = blunt_fixture();
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_POLEARM, SV_GREAT_SPEAR));
    p_ptr->active_ability[S_MEL][MEL_SMITE] = true;
    p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK] = true;
    p_ptr->truce = true;
    py_attack(m->fy, m->fx, ATT_MAIN);
    assert(!p_ptr->energy_use && !player_attacked && !p_ptr->skip_next_turn);
    assert(p_ptr->truce && !attack_prompts && !combat_number && m->hp == 20000);

    /* Denied reactions and later strikes retain an already paid action. */
    m = blunt_fixture();
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    p_ptr->energy_use = 150; p_ptr->previous_action[0] = 2;
    py_attack_aux(m->fy, m->fx, ATT_OPPORTUNITY);
    assert(p_ptr->energy_use == 150 && p_ptr->previous_action[0] == 2);
    assert(!player_attacked && !combat_number && m->hp == 20000);
    player_attacked = true;
    py_attack(m->fy, m->fx, ATT_MAIN);
    assert(p_ptr->energy_use == 150 && player_attacked && !combat_number);

    /* A permitted offhand is a real attack, even when the primary is denied. */
    m = blunt_fixture();
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    object_prep(&inventory[INVEN_ARM], lookup_kind(TV_HAFTED, SV_WAR_HAMMER));
    p_ptr->mdd2 = 2; p_ptr->mds2 = 20;
    py_attack(m->fy, m->fx, ATT_MAIN);
    assert(p_ptr->energy_use == 100 && player_attacked && combat_number == 1);
    assert(m->hp < 20000 && !p_ptr->skip_next_turn);

    /* One-blow reactions cannot borrow the normal offhand allowance. */
    m = blunt_fixture();
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    object_prep(&inventory[INVEN_ARM], lookup_kind(TV_HAFTED, SV_WAR_HAMMER));
    p_ptr->mdd2 = 2; p_ptr->mds2 = 20;
    p_ptr->energy_use = 150;
    py_attack_aux(m->fy, m->fx, ATT_OPPORTUNITY);
    assert(p_ptr->energy_use == 150 && !player_attacked && !combat_number);

    /* An illegal spear cannot force Impale and suppress a legal offhand. */
    m = blunt_fixture();
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_POLEARM, SV_SPEAR));
    object_prep(&inventory[INVEN_ARM], lookup_kind(TV_HAFTED, SV_WAR_HAMMER));
    p_ptr->mdd2 = 2; p_ptr->mds2 = 20;
    p_ptr->active_ability[S_MEL][MEL_IMPALE] = true;
    current_build_vault_exact_token = true;
    assert(place_monster_one(10, 9, 404, false, true, NULL));
    current_build_vault_exact_token = false;
    monster_type* rear = &mon_list[cave_m_idx[10][9]];
    rear->hp = rear->maxhp = 20000;
    py_attack(m->fy, m->fx, ATT_MAIN);
    assert(p_ptr->energy_use == 100 && player_attacked && combat_number == 1);
    assert(m->hp < 20000 && rear->hp == 20000);

    m = blunt_fixture();
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_HAFTED, SV_WAR_HAMMER));
    py_attack(m->fy, m->fx, ATT_MAIN);
    assert(p_ptr->energy_use == 100 && player_attacked && combat_number == 1);

    m = blunt_fixture();
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_HAFTED, SV_QUARTERSTAFF));
    p_ptr->active_ability[S_MEL][MEL_SMITE] = true;
    assert(two_handed_melee());
    py_attack(m->fy, m->fx, ATT_MAIN);
    assert(p_ptr->energy_use == 100 && player_attacked && combat_number == 1);
    assert(p_ptr->skip_next_turn && m->hp < 20000);

    for (int offhand = 0; offhand < 2; offhand++)
    {
        m = blunt_fixture();
        m->alertness = ALERTNESS_ALERT;
        object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
        if (offhand)
        {
            object_prep(&inventory[INVEN_ARM], lookup_kind(TV_HAFTED, SV_WAR_HAMMER));
            p_ptr->mdd2 = 2; p_ptr->mds2 = 20;
        }
        p_ptr->active_ability[S_EVN][EVN_RIPOSTE] = true;
        p_ptr->skill_use[S_EVN] = 1000;
        p_ptr->energy_use = 400; p_ptr->previous_action[0] = 2;
        int messages_before = riposte_messages;
        assert(make_attack_normal(m));
        assert(!p_ptr->ripostes && riposte_messages == messages_before);
        assert(!player_attacked && m->hp == 20000);
        assert(p_ptr->energy_use == 400 && p_ptr->previous_action[0] == 2);
    }
    m = blunt_fixture();
    m->alertness = ALERTNESS_ALERT;
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_HAFTED, SV_WAR_HAMMER));
    p_ptr->active_ability[S_EVN][EVN_RIPOSTE] = true;
    p_ptr->skill_use[S_EVN] = 1000;
    p_ptr->energy_use = 400; p_ptr->previous_action[0] = 2;
    int messages_before = riposte_messages;
    assert(make_attack_normal(m));
    assert(p_ptr->ripostes == 1 && riposte_messages == messages_before + 1);
    assert(player_attacked && m->hp < 20000 && p_ptr->energy_use == 400);

    m = blunt_fixture();
    py_attack(m->fy, m->fx, ATT_MAIN);
    assert(p_ptr->energy_use == 0 && !player_attacked && attack_prompts == 1);
    m = blunt_fixture();
    accept_attack_prompt = true;
    py_attack(m->fy, m->fx, ATT_MAIN);
    assert(p_ptr->energy_use == 100 && player_attacked && combat_number == 1);
    accept_attack_prompt = false;
    op_ptr->opt[OPT_quest_challenges_beta] = old_option;
    puts("Blunt trial: free armed refusal; no attack side effects; paid reactions retained; legal offhand/blunt/unarmed attacks preserved: PASS.");
}

static void check_interrupted_monster_move(void)
{
    for (int interrupted = 0; interrupted < 2; interrupted++)
    for (int take = 0; take < 2; take++)
    {
    monster_type* m = combat_fixture(403, 0);
    u32b old_flags = r_info[403].flags2;
    r_info[403].flags2 = take ? RF2_TAKE_ITEM : RF2_KILL_ITEM;
    m->hp = m->maxhp = 20000;
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    inventory[INVEN_WIELD].weight = 1000;
    p_ptr->stat_use[A_STR] = 100;
    p_ptr->skill_use[S_MEL] = 1000;
    p_ptr->mdd = p_ptr->mds = 1;
    p_ptr->active_ability[S_MEL][MEL_ZONE_OF_CONTROL] = interrupted;
    p_ptr->active_ability[S_MEL][MEL_KNOCK_BACK] = true;
    p_ptr->previous_action[1] = ACTION_MISC;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    object_type object;
    object_prep(&object, lookup_kind(TV_SWORD, SV_LONG_SWORD));
    int floor_idx = floor_carry(9, 10, &object);
    assert(floor_idx > 0);
    process_move(m, 9, 10, false);
    if (interrupted)
    {
        assert(m->fy == 10 && m->fx == 9); /* Zone of Control knocked it away. */
        assert(cave_o_idx[9][10] == floor_idx && !m->hold_o_idx);
    }
    else
    {
        assert(m->fy == 9 && m->fx == 10);
        assert(!cave_o_idx[9][10]);
        assert((m->hold_o_idx != 0) == take);
    }
    r_info[403].flags2 = old_flags;
    }
    puts("Interrupted monster move: Knock Back prevents remote pickup/destruction; completed moves retain both item behaviors: PASS.");
}

static void check_attack_energy(void)
{
    monster_type* m = combat_fixture(403, 0);
    u32b flags = r_info[403].flags1;
    r_info[403].flags1 |= RF1_PEACEFUL;
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    p_ptr->active_ability[S_EVN][EVN_FLANKING] = true;
    player_attacked = false;
    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = 2;
    flanking_or_retreat(11, 11);
    monster_swap(10, 11, 11, 11);
    assert(p_ptr->py == 11 && p_ptr->px == 11);
    assert(!player_attacked && p_ptr->energy_use == 100);

    /* A direct peaceful bump still cancels the command for free. */
    p_ptr->energy_use = 100;
    py_attack(10, 10, ATT_MAIN);
    assert(!player_attacked && p_ptr->energy_use == 0);

    m = combat_fixture(403, 0);
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    current_build_vault_exact_token = true;
    assert(place_monster_one(9, 11, 404, false, true, NULL));
    assert(place_monster_one(11, 11, 405, false, true, NULL));
    current_build_vault_exact_token = false;
    monster_type* foe = &mon_list[cave_m_idx[9][11]];
    foe->hp = foe->maxhp = 20000;
    foe->ml = true;
    foe->alertness = ALERTNESS_ALERT;
    mon_list[cave_m_idx[11][11]].hp = 20000;
    p_ptr->rage = 10;
    p_ptr->skill_use[S_MEL] = 1000;
    p_ptr->mdd = p_ptr->mds = 1;
    p_ptr->energy_use = 100;
    player_attacked = false;
    py_attack(10, 10, ATT_MAIN);
    assert(player_attacked && combat_number > 0);
    assert(p_ptr->energy_use == 100);

    /* Refusing every prompted strike cancels a requested multi-target attack. */
    pacifist_attack_warning = true;
    mon_list[cave_m_idx[11][11]].ml = true;
    p_ptr->energy_use = 100;
    player_attacked = false;
    py_attack(10, 10, ATT_MAIN);
    assert(!player_attacked && p_ptr->energy_use == 0);

    /* The same refusal on an automatic strike retains its triggering action. */
    p_ptr->energy_use = 150;
    p_ptr->previous_action[0] = 2;
    py_attack_aux(9, 11, ATT_FLANKING);
    assert(!player_attacked && p_ptr->energy_use == 150);
    assert(p_ptr->previous_action[0] == 2);
    pacifist_attack_warning = false;

    /* If every target is peaceful, the requested attack is also cancelled. */
    u32b other_flags = r_info[404].flags1;
    u32b third_flags = r_info[405].flags1;
    r_info[404].flags1 |= RF1_PEACEFUL;
    r_info[405].flags1 |= RF1_PEACEFUL;
    p_ptr->energy_use = 100;
    player_attacked = false;
    py_attack(10, 10, ATT_MAIN);
    assert(!player_attacked && p_ptr->energy_use == 0);
    r_info[403].flags1 = flags;
    r_info[404].flags1 = other_flags;
    r_info[405].flags1 = third_flags;
    puts("Combat energy: peaceful/refused automatic attacks retain action cost; direct bump and fully refused Rage refund; Rage with a later attack spends its turn: PASS.");
}

static void check_riposte_terrain(void)
{
    const int terrain[] = {FEAT_DEEP_WATER, FEAT_FLOOR, FEAT_WATER,
        FEAT_BRIDGE_DEEP_WATER_H, FEAT_BRIDGE_DEEP_WATER_V, FEAT_DEEP_WATER};
    for (int i = 0; i < (int)N_ELEMENTS(terrain); i++)
    {
        monster_type* m = combat_fixture(403, 1000);
        object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
        p_ptr->active_ability[S_EVN][EVN_RIPOSTE] = true;
        p_ptr->skill_use[S_MEL] = 1000;
        p_ptr->mdd = p_ptr->mds = 1;
        p_ptr->leaping = (i == 5);
        cave_feat[p_ptr->py][p_ptr->px] = terrain[i];
        m->hp = m->maxhp = 20000;
        int hp_before = m->hp, messages_before = riposte_messages;
        p_ptr->energy_use = 400;
        p_ptr->previous_action[0] = 2;
        player_attacked = false;
        assert(make_attack_normal(m));
        bool allowed = i != 0;
        assert(p_ptr->ripostes == (allowed ? 1 : 0));
        assert(riposte_messages - messages_before == (allowed ? 1 : 0));
        assert((m->hp < hp_before) == allowed && player_attacked == allowed);
        assert(p_ptr->energy_use == 400 && p_ptr->previous_action[0] == 2);
        /* A second miss in the same action must not allow another riposte. */
        assert(make_attack_normal(m));
        assert(p_ptr->ripostes == (allowed ? 1 : 0));
        assert(riposte_messages - messages_before == (allowed ? 1 : 0));
    }
    puts("Riposte terrain: submerged misses stay silent and retain the allowance/action cost; floor, shallow water, both bridges and airborne reactions work once: PASS.");
}

static void check_assassination_bump(void)
{
    monster_type* m;
    int assassination_attack;
    int moving_attack;

    /* A stationary contact with a visible, unaware monster gets the full
     * stealth contribution in the recorded attack score. */
    m = combat_fixture(403, 0);
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    p_ptr->skill_use[S_MEL] = 100;
    p_ptr->skill_use[S_STL] = 7;
    p_ptr->mdd = p_ptr->mds = 1;
    p_ptr->active_ability[S_STL][STL_ASSASSINATION] = true;
    p_ptr->previous_action[0] = ACTION_MISC;
    m->alertness = ALERTNESS_UNWARY;
    m->ml = true;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    process_move(m, p_ptr->py, p_ptr->px, false);
    assert(combat_number == 1);
    assassination_attack = combat_rolls[0][0].att;
    assert(m->alertness >= ALERTNESS_ALERT);

    /* The same opportunity attack without Assassination loses exactly the
     * stealth contribution. */
    m = combat_fixture(403, 0);
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    p_ptr->skill_use[S_MEL] = 100;
    p_ptr->skill_use[S_STL] = 7;
    p_ptr->mdd = p_ptr->mds = 1;
    p_ptr->previous_action[0] = ACTION_MISC;
    m->alertness = ALERTNESS_UNWARY;
    m->ml = true;
    py_attack_aux(m->fy, m->fx, ATT_OPPORTUNITY);
    assert(combat_number == 1);
    assert(assassination_attack == combat_rolls[0][0].att + 7);

    /* Contact with an unseen monster still triggers the reaction and uses
     * the exact contact square for the stealth calculation. */
    m = combat_fixture(403, 0);
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    p_ptr->skill_use[S_MEL] = 100;
    p_ptr->skill_use[S_STL] = 7;
    p_ptr->mdd = p_ptr->mds = 1;
    p_ptr->active_ability[S_STL][STL_ASSASSINATION] = true;
    p_ptr->previous_action[0] = ACTION_MISC;
    m->alertness = ALERTNESS_UNWARY;
    m->ml = false;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    process_move(m, p_ptr->py, p_ptr->px, false);
    assert(combat_number == 1);

    /* Moving on the previous turn still permits the contact attack, but it
     * does not prepare the Assassination bonus. */
    m = combat_fixture(403, 0);
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    p_ptr->skill_use[S_MEL] = 100;
    p_ptr->skill_use[S_STL] = 7;
    p_ptr->mdd = p_ptr->mds = 1;
    p_ptr->active_ability[S_STL][STL_ASSASSINATION] = true;
    p_ptr->previous_action[0] = 6;
    m->alertness = ALERTNESS_UNWARY;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    process_move(m, p_ptr->py, p_ptr->px, false);
    assert(combat_number == 1);
    moving_attack = combat_rolls[0][0].att;
    assert(m->alertness >= ALERTNESS_ALERT);

    m = combat_fixture(403, 0);
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    p_ptr->skill_use[S_MEL] = 100;
    p_ptr->skill_use[S_STL] = 7;
    p_ptr->mdd = p_ptr->mds = 1;
    p_ptr->previous_action[0] = 6;
    m->alertness = ALERTNESS_UNWARY;
    py_attack_aux(m->fy, m->fx, ATT_OPPORTUNITY);
    assert(combat_number == 1);
    assert(moving_attack == combat_rolls[0][0].att);

    puts("Assassination: unaware bumps make one free opportunity attack; standing still adds the stealth bonus: PASS.");
}
'''


def main():
    out = engine.ROOT / "scripts/output/combat-action-energy"
    out.mkdir(parents=True, exist_ok=True)
    source = out / "check.c"
    harness = engine.HARNESS.replace(
        "int main(int argc,char** argv)", CHECKS + "\nint main(int argc,char** argv)")
    harness = harness.replace("    check_combat();", "    check_blunt_challenge();\n    check_attack_energy();\n    check_assassination_bump();\n    check_interrupted_monster_move();\n    check_riposte_terrain();")
    source.write_text(harness, encoding="utf-8")
    cmake = engine.BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
        if not p.endswith(("/src/main.c.obj", "/src/cmd/combat/cmd-combat.c.obj",
            "/src/melee/melee-movement-resolution.c.obj",
            "/src/melee/melee-attack.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(engine.BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        env["PATH"]])
    exe = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(engine.ROOT / "src/cmd/combat/cmd-combat.c"),
        str(engine.ROOT / "src/melee/melee-movement-resolution.c"), "@" + str(response),
        str(engine.ROOT / "src/melee/melee-attack.c"),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=get_check_near",
        "-Wl,--wrap=msg_print", "-o", str(exe)],
        cwd=engine.BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=out) as data:
        subprocess.run([str(exe), str(engine.ROOT / "lib/edit"), data],
            cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()

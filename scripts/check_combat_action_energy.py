#!/usr/bin/env python3
"""Check combat cancellation, reactions and attack effects with the production engine."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

import check_new_monsters as engine

CHECKS = r'''
#include "melee/melee-movement.h"
#include "melee/melee-attack.h"
#include "object/object-place.h"
#include "player/player-upkeep-internal.h"
static int riposte_messages;
void __real_msg_print(cptr msg);
void __wrap_msg_print(cptr msg)
{
    if (msg && !strcmp(msg, "You riposte!")) riposte_messages++;
    __real_msg_print(msg);
}

bool __wrap_get_check_near(int y, int x, cptr prompt)
{
    (void)y; (void)x; (void)prompt;
    return false;
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
    py_attack(9, 11, ATT_MAIN);
    assert(player_attacked && combat_number > 0);
    assert(p_ptr->energy_use == 100);

    /* Refusing every prompted strike cancels a requested multi-target attack. */
    pacifist_attack_warning = true;
    mon_list[cave_m_idx[11][11]].ml = true;
    p_ptr->energy_use = 100;
    player_attacked = false;
    py_attack(9, 11, ATT_MAIN);
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

static void check_disarm_liquids(void)
{
    const int liquids[] = {FEAT_WATER, FEAT_DEEP_WATER, FEAT_POISON, FEAT_LAVA};
    bool was_dungeon = character_dungeon;
    character_dungeon = true;
    for (int t = 0; t < (int)N_ELEMENTS(liquids); t++)
    {
        /* A liquid landing square must win over surrounding dry ground. */
        combat_fixture(105, -100);
        cave_set_feat(10, 12, liquids[t]);
        object_type weapon;
        object_prep(&weapon, lookup_kind(TV_SWORD, SV_LONG_SWORD));
        weapon.att = 3;
        int idx = drop_near(&weapon, 0, 10, 12);
        assert(idx > 0 && cave_feat[o_list[idx].iy][o_list[idx].ix] == FEAT_FLOOR);
        delete_object_idx(idx);
        idx = drop_disarmed_weapon(&weapon, 10, 12);
        assert(idx > 0 && o_list[idx].iy == 10 && o_list[idx].ix == 12);
        assert(o_list[idx].number == 1 && o_list[idx].att == 3);

        /* Real enemy attacks must unequip and drop exactly one weapon locally. */
        int liquid_landings = 0;
        for (int seed = 1; seed <= 8; seed++)
        {
            monster_type* m = combat_fixture(105, -100);
            /* Keep the defences stable when damage refreshes player bonuses. */
            p_ptr->stat_base[A_STR] = p_ptr->stat_use[A_STR] = -4;
            p_ptr->skill_base[S_EVN] = -100;
            cave_m_idx[p_ptr->py][p_ptr->px] = -1;
            object_prep(&inventory[INVEN_WIELD], weapon.k_idx);
            for (int y = 9; y <= 11; y++)
                for (int x = 10; x <= 12; x++)
                    if (y != m->fy || x != m->fx)
                        cave_set_feat(y, x, liquids[t]);
            Rand_state_init(seed);
            for (int attempt = 0; attempt < 64 && inventory[INVEN_WIELD].k_idx; attempt++)
                assert(make_attack_normal(m));
            assert(!inventory[INVEN_WIELD].k_idx);
            int found = 0;
            for (int i = 1; i < o_max; i++)
                if (o_list[i].k_idx == weapon.k_idx)
                {
                    found++;
                    assert(o_list[i].number == 1);
                    assert(ABS(o_list[i].iy - p_ptr->py) <= 1);
                    assert(ABS(o_list[i].ix - p_ptr->px) <= 1);
                    liquid_landings += cave_feat[o_list[i].iy][o_list[i].ix] == liquids[t];
                }
            assert(found == 1);
            for (int i = 0; i < INVEN_TOTAL; i++)
                assert(inventory[i].k_idx != weapon.k_idx);
        }
        assert(liquid_landings > 0);
    }
    character_dungeon = was_dungeon;
    puts("Disarmed weapons: shallow/deep water, poisonous acid and lava accept local landings; 32 real enemy disarms preserve exactly one weapon: PASS.");
}

static void check_scare_resistance(void)
{
    const int scare = 96 + 12;
    for (int fear = 0; fear <= 9; fear += 9)
    {
        for (int guard = 0; guard < 3; guard++)
        {
            monster_type* m = combat_fixture(409, 0);
            cave_m_idx[p_ptr->py][p_ptr->px] = -1;
            cave_info[m->fy][m->fx] |= CAVE_FIRE;
            cave_info[p_ptr->py][p_ptr->px] |= CAVE_FIRE;
            m->mana = MON_MANA_MAX;
            p_ptr->afraid = fear;
            p_ptr->skill_use[S_WIL] = guard == 1 ? 100 : -100;
            p_ptr->rage = guard == 2 ? 10 : 0;
            assert(make_attack_ranged(m, scare));
            if (guard)
                assert(p_ptr->afraid == fear);
            else
                assert(p_ptr->afraid >= fear + 3 && p_ptr->afraid <= fear + 12);
        }
    }
    puts("Scare: Will saves and Rage prevent application and extension; failed saves add 3d4 turns: PASS.");
}

static void check_disarm_floor_capacity(void)
{
    monster_type* m = combat_fixture(105, -100);
    /* Start with an empty object pool before limiting it to the sentinel. */
    wipe_o_list();
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    p_ptr->stat_base[A_STR] = p_ptr->stat_use[A_STR] = -20;
    p_ptr->skill_base[S_EVN] = -100;
    object_type* wielded = &inventory[INVEN_WIELD];
    object_prep(wielded, lookup_kind(TV_SWORD, SV_LONG_SWORD));
    wielded->att = 3;
    p_ptr->equip_cnt = 1;

    int old_limit = z_info->o_max;
    z_info->o_max = 1; /* Only the sentinel remains, so no drop can allocate. */
    assert(make_attack_normal(m));
    assert(wielded->k_idx && wielded->number == 1 && wielded->att == 3);
    assert(p_ptr->inven_cnt == 0 && p_ptr->equip_cnt == 1 && o_cnt == 0);
    z_info->o_max = old_limit;

    assert(make_attack_normal(m));
    assert(!wielded->k_idx && p_ptr->inven_cnt == 0 && p_ptr->equip_cnt == 0);
    assert(o_cnt == 1 && o_list[1].number == 1 && o_list[1].att == 3);
    puts("Disarm with no object slots: weapon stays equipped; retry drops exactly one intact weapon: PASS.");
}

static void check_disarm_weapon_modes(void)
{
    for (int throwing = 0; throwing <= 1; throwing++)
    {
        monster_type* m = combat_fixture(105, -100);
        int initial_objects = o_cnt;
        int next_object = o_max;
        cave_m_idx[p_ptr->py][p_ptr->px] = -1;
        object_type* spear = &inventory[INVEN_WIELD];
        int spear_kind = lookup_kind(TV_POLEARM, SV_SPEAR);
        object_prep(spear, spear_kind);
        player_active_weapon_assign_harness_color(spear);
        assert(object_runtime_harness_color_id(spear));
        p_ptr->equip_cnt = 1;

        if (throwing)
        {
            assert(player_ready_throwing_weapon(spear, INVEN_WIELD));
            assert(player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_THROWING);
            assert(spear->pickup_slot == PICKUP_SLOT_ACTIVE_THROWING);
            spear->pickup = true;
        }
        else
        {
            object_prep(&inventory[INVEN_BOW], lookup_kind(TV_BOW, SV_SHORT_BOW));
            p_ptr->equip_cnt++;
            assert(player_ready_bow_with_arrow(-1));
            assert(player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_BOW);
        }
        assert(player_equipment_slot_counts_as_equipped(INVEN_WIELD) == throwing);

        p_ptr->stat_base[A_STR] = p_ptr->stat_use[A_STR] = -20;
        p_ptr->skill_base[S_EVN] = p_ptr->skill_use[S_EVN] = -100;
        int utility = monster_melee_utility(m, 0, true, false);
        assert(make_attack_normal(m));
        if (throwing)
        {
            assert(utility > 0 && !spear->k_idx && o_cnt == initial_objects + 1);
            object_type* dropped = &o_list[next_object];
            assert(dropped->k_idx == spear_kind && dropped->number == 1);
            assert(!dropped->held_m_idx && !dropped->pickup && dropped->pickup_slot == -1);
            assert(!object_runtime_harness_color_id(dropped));
        }
        else
        {
            assert(spear->k_idx == spear_kind && o_cnt == initial_objects);
            assert(inventory[INVEN_BOW].k_idx && utility == 0);
        }
    }
    puts("Disarm: readying a bow protects the stored spear; an active throwing spear drops without auto-pickup or Harness colour: PASS.");
}

static void check_disarm(void)
{
    /* Use Othrod's real whip blow without letting the AI pick his sword. */
    monster_race* race = &r_info[105];
    monster_blow first = race->blow[0], second = race->blow[1];
    assert(second.effect == RBE_DISARM);
    race->blow[0] = second;
    memset(&race->blow[1], 0, sizeof(race->blow[1]));
    check_disarm_weapon_modes();
    check_disarm_floor_capacity();
    check_disarm_liquids();
    race->blow[0] = first;
    race->blow[1] = second;
}

static void check_sprinting_history(void)
{
    /* The scheduler records completed actions starting at index 1, newest first. */
    static const struct {
        byte history[ACTION_MAX - 1];
        bool light, heavy;
    } cases[] = {
        {{6, 6, 6, 6, ACTION_MISC}, true, true},
        {{6, 6, 6, ACTION_MISC, 6}, true, false},
        {{6, 9, 6, 9, ACTION_MISC}, true, true},
        {{8, 9, 8, 9, ACTION_MISC}, true, true},
        {{6, 6, ACTION_MISC, 6, 6}, false, false},
        {{5, 6, 6, 6, 6}, false, false},
        {{2, 1, 1, 2, ACTION_MISC}, true, true},
        {{1, 9, 9, 9, 9}, false, false},
        {{3, 2, 4, 4, 4}, false, false},
    };
    for (int heavy = 0; heavy <= 1; heavy++)
    {
        combat_fixture(403, 0);
        if (heavy)
            object_prep(&inventory[INVEN_BODY], lookup_kind(TV_MAIL, SV_MAIL_CORSLET));
        assert(wearing_only_light_armour() == !heavy);
        p_ptr->innate_ability[S_EVN][EVN_SPRINTING] = true;
        calc_bonuses();
        int normal_speed = p_ptr->pspeed;
        assert(normal_speed < 3);
        p_ptr->active_ability[S_EVN][EVN_SPRINTING] = true;
        for (size_t i = 0; i < N_ELEMENTS(cases); ++i)
        {
            memcpy(&p_ptr->previous_action[1], cases[i].history, sizeof(cases[i].history));
            bool expected = heavy ? cases[i].heavy : cases[i].light;
            calc_bonuses();
            assert(sprinting() == expected);
            assert(p_ptr->pspeed == normal_speed + expected);
        }
    }
    puts("Sprinting: only continuous movement counts, compass bends work, stops and sharp turns reset, both armour thresholds set speed: PASS.");
}
'''


def main():
    out = engine.ROOT / "scripts/output/combat-action-energy"
    out.mkdir(parents=True, exist_ok=True)
    source = out / "check.c"
    harness = engine.HARNESS.replace(
        "int main(int argc,char** argv)", CHECKS + "\nint main(int argc,char** argv)")
    harness = harness.replace("    check_combat();", "\n".join([
        "    check_attack_energy();",
        "    check_interrupted_monster_move();",
        "    check_riposte_terrain();",
        "    check_scare_resistance();",
        "    check_disarm();",
        "    check_sprinting_history();",
    ]))
    source.write_text(harness, encoding="utf-8")
    cmake = engine.BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
        if not p.endswith(("/src/main.c.obj", "/src/cmd/combat/cmd-combat.c.obj",
            "/src/melee/melee-movement-resolution.c.obj",
            "/src/melee/melee-attack.c.obj", "/src/melee/melee-attack-ranged.c.obj",
            "/src/object/object-place.c.obj", "/src/player/player-skills.c.obj"))), encoding="utf-8")
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
        str(engine.ROOT / "src/melee/melee-attack-ranged.c"),
        str(engine.ROOT / "src/object/object-place.c"),
        str(engine.ROOT / "src/player/player-skills.c"),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=get_check_near",
        "-Wl,--wrap=msg_print", "-o", str(exe)],
        cwd=engine.BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=out) as data:
        subprocess.run([str(exe), str(engine.ROOT / "lib/edit"), data],
            cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check melee cancellation accounting against freshly compiled combat code."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

import check_new_monsters as engine

CHECKS = r'''
#include "melee/melee-movement.h"
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
'''


def main():
    out = engine.ROOT / "scripts/output/combat-action-energy"
    out.mkdir(parents=True, exist_ok=True)
    source = out / "check.c"
    harness = engine.HARNESS.replace(
        "int main(int argc,char** argv)", CHECKS + "\nint main(int argc,char** argv)")
    harness = harness.replace("    check_combat();", "    check_attack_energy();\n    check_interrupted_monster_move();")
    source.write_text(harness, encoding="utf-8")
    cmake = engine.BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
        if not p.endswith(("/src/main.c.obj", "/src/cmd/combat/cmd-combat.c.obj",
            "/src/melee/melee-movement-resolution.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        *(str(engine.BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        env["PATH"]])
    exe = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(engine.ROOT / "src/cmd/combat/cmd-combat.c"),
        str(engine.ROOT / "src/melee/melee-movement-resolution.c"), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=get_check_near", "-o", str(exe)],
        cwd=engine.BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=out) as data:
        subprocess.run([str(exe), str(engine.ROOT / "lib/edit"), data],
            cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()

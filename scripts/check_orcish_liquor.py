#!/usr/bin/env python3
"""Exercise orc bottle inventory and combat using the standard Windows build."""

import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_new_monsters import BUILD, ROOT

OUT = ROOT / "scripts/output/orcish-liquor"

CHECKS = r'''
#include "cave/cave-environment.h"
#include "cave/cave-events.h"
#include "melee/melee-movement.h"
#include "monster/monster-ai.h"
#include "monster/monster-social.h"
#include "monster/monster-tactics.h"

enum { SOLDIER = 41, EASTERLING_ARCHER = 103, PLAYER_Y = 10, PLAYER_X = 10 };
static int liquor_kind;

static void arena(unsigned seed)
{
    character_dungeon = false;
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    reset_map(6);
    monster_social_reset();
    cave_environment_reset();
    cave_events_reset();
    feeling = 0;
    Rand_state_init(seed);
    playerturn = 10;
    turn = 100;
    p_ptr->py = PLAYER_Y;
    p_ptr->px = PLAYER_X;
    p_ptr->chp = p_ptr->mhp = 1000;
    p_ptr->resist_fire = 1;
    p_ptr->food = PY_FOOD_FULL;
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    cave_m_idx[PLAYER_Y][PLAYER_X] = -1;
    for (int y = 1; y < 25; ++y)
        for (int x = 1; x < 25; ++x)
            cave_info[y][x] = CAVE_VIEW | CAVE_SEEN | CAVE_FIRE;
    character_generated = character_dungeon = true;
}

static monster_type* place(int race, int y, int x)
{
    current_build_vault_exact_token = true;
    assert(place_monster_one(y, x, race, false, true, NULL));
    current_build_vault_exact_token = false;
    monster_type* m = &mon_list[cave_m_idx[y][x]];
    m->alertness = ALERTNESS_ALERT;
    m->stance = STANCE_CONFIDENT;
    m->tmp_morale = 40;
    m->skip_next_turn = false;
    m->energy = 0;
    return m;
}

static int bottles(const monster_type* m)
{
    int count = 0;
    for (int i = m->hold_o_idx; i; i = o_list[i].next_o_idx)
    {
        assert(o_list[i].held_m_idx == m - mon_list);
        if (o_list[i].k_idx == liquor_kind)
            count += o_list[i].number;
    }
    return count;
}

static int floor_bottles(void)
{
    int count = 0;
    for (int i = 1; i < o_max; ++i)
        if (!o_list[i].held_m_idx && o_list[i].k_idx == liquor_kind)
            count += o_list[i].number;
    return count;
}

static void remove_bottles(monster_type* m)
{
    for (int i = m->hold_o_idx, next; i; i = next)
    {
        next = o_list[i].next_o_idx;
        if (o_list[i].k_idx == liquor_kind)
            delete_object_idx(i);
    }
}

static void stock(monster_type* m, int count)
{
    object_type bottle;
    remove_bottles(m);
    if (!count) return;
    object_prep(&bottle, liquor_kind);
    bottle.number = count;
    assert(monster_carry((int)(m - mon_list), &bottle));
    assert(bottles(m) == count);
}

static void act(monster_type* m)
{
    ++playerturn;
    turn += 10;
    m->energy = 100;
    process_monsters(100);
    assert(m->energy == 0);
}

static void check_spawn_stock(void)
{
    int stocked = 0;
    for (unsigned seed = 1; seed <= 80; ++seed)
    {
        arena(seed);
        assert(bottles(place(R_IDX_LUG, 10, 14)) == 1);
        assert(bottles(place(R_IDX_OTHROD, 10, 15)) == 1);
        int ordinary_stock = bottles(place(SOLDIER, 10, 16));
        assert(ordinary_stock <= 1);
        stocked += ordinary_stock;
        assert(bottles(place(EASTERLING_ARCHER, 10, 17)) == 0);
        assert(!floor_bottles());
    }
    assert(stocked > 0 && stocked < 80);
    puts("Spawn stock: Lug and Othrod carry exactly one, ordinary orcs sometimes carry one, and non-orcs carry none.");
}

static void check_full_object_pool(void)
{
    arena(113);
    object_type artefact;
    assert(make_fake_artefact(&artefact, ART_GROND));
    assert(floor_carry(9, 9, &artefact) == 1);
    int original_limit = z_info->o_max;
    z_info->o_max = 2;
    const int races[] = { R_IDX_LUG, R_IDX_OTHROD };
    for (size_t i = 0; i < N_ELEMENTS(races); ++i)
    {
        int race = races[i];
        assert(!place_monster_one(10, 14, race, false, true, NULL));
        assert(!cave_m_idx[10][14] && !r_info[race].cur_num && r_info[race].max_num == 1);
        assert(!mon_cnt && mon_max == 1);
        assert(o_max == 2 && o_cnt == 1 && cave_o_idx[9][9] == 1);
        assert(o_list[1].name1 == ART_GROND);
    }
    z_info->o_max = original_limit;
    assert(bottles(place(R_IDX_LUG, 10, 14)) == 1);
    assert(bottles(place(R_IDX_OTHROD, 10, 15)) == 1);
    puts("A full artefact-only object pool leaves monsters, counters, and objects intact, and both uniques can spawn once space is available.");
}

static void assert_fire_damage(int old_hp)
{
    int damage = old_hp - p_ptr->chp;
    if (damage < 3 || damage > 12)
        fprintf(stderr, "  fire did %d damage across %d combat rolls\n", damage, combat_number);
    assert(damage >= 3 && damage <= 12);
    assert(combat_number > 0);
    assert(combat_rolls[0][combat_number - 1].dam_type == GF_FIRE);
    assert(combat_rolls[0][combat_number - 1].dd == 3);
    assert(combat_rolls[0][combat_number - 1].ds == 4);
}

static void check_autonomous(void)
{
    const int races[] = { R_IDX_ORC_CAPTAIN, R_IDX_ORC_CHAMPION,
        R_IDX_LUG, R_IDX_OTHROD };
    for (size_t i = 0; i < N_ELEMENTS(races); ++i)
    {
        arena(107);
        monster_type* m = place(races[i], 10, 14);
        fprintf(stdout, "Autonomous throw: %s\n", r_name + r_info[races[i]].name);
        int initial_stock = bottles(m);
        if (races[i] != R_IDX_LUG && races[i] != R_IDX_OTHROD)
        {
            initial_stock = 2;
            stock(m, initial_stock);
        }
        int hp = p_ptr->chp;
        object_type ration;
        object_prep(&ration, lookup_kind(TV_FOOD, SV_FOOD_BREAD));
        assert(ration.k_idx);
        int food = monster_carry((int)(m - mon_list), &ration);
        assert(food > 0);
        assert(monster_ai_can_see_player(m));
        combat_number = 0;
        act(m);
        if (bottles(m) == initial_stock)
            fprintf(stderr, "  action=%d position=(%d,%d), hp=%d\n",
                m->previous_action[0], m->fy, m->fx, p_ptr->chp);
        assert(m->fy == 10 && m->fx == 14);
        assert(bottles(m) == initial_stock - 1);
        assert(m->hold_o_idx == food && o_list[food].k_idx == ration.k_idx);
        assert(!floor_bottles());
        assert_fire_damage(hp);
    }
}

static void check_order(int race)
{
    arena(109);
    monster_type* issuer = place(race, 8, 14);
    monster_type* soldier = place(SOLDIER, 10, 14);
    stock(issuer, 0);
    stock(soldier, 1);
    fprintf(stdout, "Liquor order: %s\n", r_name + r_info[race].name);
    int hp = p_ptr->chp;
    combat_number = 0;
    act(soldier);
    assert(soldier->squad.commander == issuer - mon_list);
    assert(!issuer->energy && !bottles(issuer));
    assert(!bottles(soldier) && !floor_bottles());
    assert_fire_damage(hp);
}

static void check_commands(void)
{
    check_order(R_IDX_ORC_CAPTAIN);
    check_order(R_IDX_ORC_CHAMPION);
    for (int race = 1; race < z_info->r_max; ++race)
        if ((r_info[race].flags1 & RF1_UNIQUE) && (r_info[race].flags3 & RF3_ORC))
            check_order(race);

    arena(109);
    monster_type* issuer = place(R_IDX_ORC_CHAMPION, 8, 14);
    monster_type* soldier = place(SOLDIER, 10, 14);
    stock(issuer, 1);
    stock(soldier, 0);
    int hp = p_ptr->chp;
    act(soldier);
    assert(bottles(issuer) == 1 && !bottles(soldier) && p_ptr->chp == hp);

    arena(109);
    soldier = place(SOLDIER, 10, 14);
    stock(soldier, 1);
    hp = p_ptr->chp;
    act(soldier);
    assert(bottles(soldier) == 1 && p_ptr->chp == hp);
    puts("An order needs a stocked recipient, and an isolated soldier saves its bottle.");

    arena(109);
    issuer = place(R_IDX_ORC_CHAMPION, 8, 14);
    soldier = place(SOLDIER, 10, 14);
    stock(soldier, 1);
    monster_squad_prepare();
    assert(soldier->squad.job == MON_JOB_THROW_LIQUOR);
    assert(monster_liquor_throw_utility(soldier) > 0);
    issuer->confused = 5;
    assert(!monster_liquor_throw_utility(soldier));
    hp = p_ptr->chp;
    act(soldier);
    assert(bottles(soldier) == 1 && p_ptr->chp == hp);
    puts("A soldier ignores a throw order whose commander is no longer able to direct it.");
}

static void check_bottle_drop(void)
{
    /* Suppress ordinary loot creation to isolate the carried bottle. */
    u32b flags = r_info[SOLDIER].flags1;
    r_info[SOLDIER].flags1 &= ~RF1_DROP_33;
    for (unsigned seed = 1; seed <= 24; ++seed)
    {
        arena(seed);
        monster_type* soldier = place(SOLDIER, 10, 14);
        int count = bottles(soldier);
        assert(mon_take_hit((int)(soldier - mon_list), soldier->hp, "", -1));
        assert(!soldier->r_idx && floor_bottles() == count);
    }
    r_info[SOLDIER].flags1 = flags;
    puts("The loot roll follows the actual carried bottle through death.");
}

static void check_safe_throw(void)
{
    for (int scenario = 0; scenario < 6; ++scenario)
    {
        arena(103);
        int race = scenario == 4 ? R_IDX_ORC_THRALLMASTER : R_IDX_ORC_CHAMPION;
        monster_type* shooter = place(race, 10, scenario == 5 ? 16 : 14);
        stock(shooter, 1);
        if (scenario == 0)
            cave_set_feat(10, 12, FEAT_WALL_SOLID);
        else if (scenario == 1)
            place(SOLDIER, 10, 12); /* In flight, outside the splash. */
        else if (scenario == 2)
            place(SOLDIER, 11, 10); /* In the splash, outside the flight path. */
        else if (scenario == 3)
            shooter->alertness = ALERTNESS_UNWARY;
        int hp = p_ptr->chp;
        act(shooter);
        assert(bottles(shooter) == 1 && p_ptr->chp == hp);
    }
    puts("A wall, ally on the flight path, ally in the splash, lack of detection, and excess range each prevent the throw.");
    puts("A thrallmaster doesn't throw on its own.");
}

static void check_pickup(void)
{
    object_type bottle;
    arena(111);
    monster_type* lug = place(R_IDX_LUG, 10, 15);
    stock(lug, 0);
    compact_objects(0);
    object_prep(&bottle, liquor_kind);
    int floor_slot = floor_carry(10, 14, &bottle);
    assert(floor_slot == 1 && o_max == 2 && o_cnt == 1);
    int original_limit = z_info->o_max;
    z_info->o_max = o_max;
    process_move(lug, 10, 14, false);
    z_info->o_max = original_limit;
    assert(lug->fx == 14 && bottles(lug) == 1 && !floor_bottles());
    assert(lug->hold_o_idx == floor_slot && o_cnt == 1);
    int hp = p_ptr->chp;
    combat_number = 0;
    act(lug);
    assert(!bottles(lug));
    assert_fire_damage(hp);

    arena(111);
    monster_type* captain = place(R_IDX_ORC_CAPTAIN, 10, 14);
    stock(captain, 0);
    object_prep(&bottle, liquor_kind);
    assert(floor_carry(10, 14, &bottle) > 0);
    hp = p_ptr->chp;
    act(captain);
    assert(bottles(captain) == 1 && !floor_bottles() && p_ptr->chp == hp);
    act(captain);
    assert(!bottles(captain) && p_ptr->chp < hp);

    arena(111);
    monster_type* master = place(R_IDX_ORC_THRALLMASTER, 10, 15);
    stock(master, 0);
    object_prep(&bottle, liquor_kind);
    assert(floor_carry(10, 14, &bottle) > 0);
    process_move(master, 10, 14, false);
    assert(master->fx == 14 && !bottles(master) && floor_bottles() == 1);
    puts("Lug picks up a bottle while moving at the object limit. A captain picks up one on its tile. Both can throw it later. A thrallmaster leaves it alone.");
}
'''


def main():
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    calls = r'''
    assert(init_flavor_info() == 0);
    flavor_init();
    drop_system_init();
    liquor_kind = lookup_kind(TV_POTION, SV_POTION_ORCISH_LIQUOR);
    assert(liquor_kind);
    check_spawn_stock();
    check_full_object_pool();
    check_autonomous();
    check_commands();
    check_bottle_drop();
    check_safe_throw();
    check_pickup();
    SDL_Quit();
    return 0;
}
'''
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(prefix + fixture_function("terminal_extra") + "\n"
                      + fixture_function("reset_map") + "\n" + CHECKS + init + calls,
                      encoding="utf-8")
    cmake_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join(f'"{obj}"' for obj in objects
                                  if not obj.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data],
                       cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()

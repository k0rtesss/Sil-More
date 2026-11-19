#!/usr/bin/env python3
"""Check thrall interactions, mining and gem collection in the real engine."""
import os
import shlex
import subprocess
import tempfile

import check_new_monsters as engine
from check_monster_scent_save import fixture_function


CHECKS = r'''
#include "cave/cave-environment.h"
#include "player/player-song-internal.h"
#include "spell/spell-projection-internal.h"
#ifdef NDEBUG
#error Thrall checks require active assertions
#endif
static int prompts, attacks, conversations, fear_messages;
static bool accept_exchange;

bool __wrap_get_check_near(int y, int x, cptr prompt)
{
    assert(y == 10 && x == 10);
    assert(strstr(prompt, "Change places with ") == prompt);
    prompts++;
    return accept_exchange;
}

void __real_py_attack(int y, int x, int type);
void __wrap_py_attack(int y, int x, int type)
{
    attacks++;
    __real_py_attack(y, x, type);
}

bool __wrap_handle_thrall_interaction(monster_type* m)
{
    assert(is_alert_thrall(m));
    conversations++;
    return true;
}

void __real_msg_print(cptr msg);
void __wrap_msg_print(cptr msg)
{
    if (msg && (strstr(msg, "flees in terror")
            || strstr(msg, "regains") || strstr(msg, "turns to fight")))
        fear_messages++;
    __real_msg_print(msg);
}

void __wrap_msg_format(cptr fmt, ...)
{
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vstrnfmt(msg, sizeof(msg), fmt, args);
    va_end(args);
    __wrap_msg_print(msg);
}

static monster_type* prepare_thrall(int race, bool visible)
{
    monster_type* m = combat_fixture(race, 0);
    cave_m_idx[10][11] = -1;
    for (int y = 0; y < 20; y++) for (int x = 0; x < 20; x++)
        cave_info[y][x] = CAVE_MARK | CAVE_SEEN;
    m->ml = visible;
    m->alertness = ALERTNESS_UNWARY;
    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = ACTION_MISC;
    p_ptr->command_rep = 5;
    p_ptr->running = 5;
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    pacifist_attack_warning = false;
    prompts = attacks = conversations = fear_messages = 0;
    player_attacked = false;
    repeat_clear();
    return m;
}

static void check_exchanges(void)
{
    const int races[] = {R_IDX_HUMAN_THRALL, R_IDX_ELF_THRALL};
    for (int r = 0; r < 2; r++)
    for (int seen = 0; seen < 2; seen++)
    for (int accept = 0; accept < 2; accept++)
    {
        monster_type* m = prepare_thrall(races[r], seen);
        int idx = cave_m_idx[10][10], hp = m->hp;
        accept_exchange = accept;
        move_player(4);
        assert(prompts == 1 && attacks == 0 && conversations == 0);
        assert(!p_ptr->running && !p_ptr->command_rep);
        assert(p_ptr->py == 10 && p_ptr->px == (accept ? 10 : 11));
        assert(m->fy == 10 && m->fx == (accept ? 11 : 10));
        assert(cave_m_idx[10][accept ? 10 : 11] == -1);
        assert(cave_m_idx[10][accept ? 11 : 10] == idx);
        assert(p_ptr->energy_use == (accept ? 100 : 0));
        assert(m->hp == hp && m->alertness == ALERTNESS_UNWARY);
        assert(!player_attacked && !combat_number);
        if (accept) assert(p_ptr->previous_action[0] == 4);
    }
    puts("Dejected human/elf bumps: visible/unseen, accept/refuse, positions, one-turn/free cancellation, no attack or alert PASS.");

    for (int leaving = 0; leaving < 2; leaving++)
    {
        monster_type* m = prepare_thrall(R_IDX_HUMAN_THRALL, true);
        cave_feat[10][leaving ? 11 : 10] = FEAT_WATER;
        p_ptr->active_ability[S_EVN][EVN_LEAPING] = true;
        p_ptr->previous_action[1] = 4;
        accept_exchange = true;
        move_player(4);
        assert(prompts == 1 && !attacks && !p_ptr->leaping);
        assert(p_ptr->px == 10 && m->fx == 11 && p_ptr->energy_use == 150);
    }
    puts("Shallow-water exchanges retain the 150-energy movement cost and do not leap past the thrall PASS.");

    const int hazards[] = {FEAT_LAVA, FEAT_POISON, FEAT_CHASM,
        FEAT_DEEP_WATER, FEAT_TRAP_PIT, FEAT_GLYPH};
    for (int i = 0; i < (int)N_ELEMENTS(hazards); i++)
    {
        monster_type* m = prepare_thrall(R_IDX_HUMAN_THRALL, true);
        cave_feat[10][11] = hazards[i];
        accept_exchange = true;
        move_player(4);
        assert(!prompts && !attacks && p_ptr->energy_use == 0);
        assert(p_ptr->px == 11 && m->fx == 10);
    }
    monster_type* m = prepare_thrall(R_IDX_ELF_THRALL, true);
    p_ptr->total_weight = weight_limit() * 2;
    move_player(4);
    assert(!prompts && !attacks && !p_ptr->energy_use);
    assert(p_ptr->px == 11 && m->fx == 10);
    puts("Exchange movement guards: unsafe vacated terrain and overburdening cannot displace a thrall PASS.");

    const int quest_races[] = {R_IDX_ALERT_HUMAN_THRALL, R_IDX_ALERT_ELF_THRALL};
    for (int r = 0; r < 2; r++)
    {
        m = prepare_thrall(quest_races[r], true);
        move_player(4);
        assert(!prompts && conversations == 1 && attacks == 1);
        assert(p_ptr->px == 11 && m->fx == 10 && !p_ptr->energy_use);
    }
    m = prepare_thrall(R_IDX_ORC_THRALLMASTER, true);
    m->hp = m->maxhp = 20000;
    move_player(4);
    assert(!prompts && !conversations && attacks == 1 && player_attacked);
    assert(p_ptr->px == 11 && m->fx == 10 && p_ptr->energy_use == 100);
    puts("Alert thralls retain quest conversations; hostile overseers retain normal attacks PASS.");
}

static void check_thrall_fear(void)
{
    const int races[] = {R_IDX_HUMAN_THRALL, R_IDX_ELF_THRALL,
        R_IDX_ALERT_HUMAN_THRALL, R_IDX_ALERT_ELF_THRALL};
    for (int r = 0; r < 4; r++)
    {
        monster_type* m = prepare_thrall(races[r], true);
        assert(monster_is_thrall(m));
        m->tmp_morale = 0;
        u32b flags = m->mflag;
        sing_song_of_elbereth(1000);
        assert(!m->tmp_morale && !m->achievement_elbereth_turn);
        assert(!project_m(-1, 10, 10, 0, 0, 1000, GF_FEAR, 0));
        assert(!m->tmp_morale && m->mflag == flags);
        assert(m->alertness == ALERTNESS_UNWARY && !fear_messages);
    }
    puts("Fear horns/projections and Elbereth leave every thrall unaffected, without alerting or recording a song effect PASS.");

    for (int r = 0; r < 4; r++)
    for (int state = 0; state < 4; state++)
    {
        monster_type* m = prepare_thrall(races[r], true);
        m->alertness = ALERTNESS_ALERT;
        m->stance = state == 3 ? STANCE_FLEEING : STANCE_CONFIDENT;
        m->hp = 1;
        m->tmp_morale = state == 2 ? 1000 : -1000;
        m->min_range = 20;
        p_ptr->active_ability[S_WIL][WIL_MAJESTY] = true;
        p_ptr->skill_use[S_WIL] = 1000;
        p_ptr->aggravate = state == 1;
        p_ptr->song1 = state == 2 ? SNG_CHALLENGE : SNG_ELBERETH;
        if (state == 0) sing_song_of_elbereth(1000);
        calc_morale(m);
        calc_stance(m);
        assert(m->stance == STANCE_CONFIDENT);
        assert(m->tmp_morale >= 0 && m->morale > 0 && !fear_messages);
        if (state == 3) assert(m->min_range == 0);
    }
    monster_type* m = prepare_thrall(R_IDX_ORC_THRALLMASTER, true);
    m->alertness = ALERTNESS_ALERT;
    m->stance = STANCE_CONFIDENT;
    m->tmp_morale = 0;
    assert(!monster_is_thrall(m));
    assert(project_m(-1, 10, 10, 0, 0, 1000, GF_FEAR, 0));
    assert(m->tmp_morale < 0);
    calc_morale(m); calc_stance(m);
    assert(m->stance == STANCE_FLEEING);
    puts("All four thralls stay calm under Majesty, Elbereth, low health, Wrath and Challenge; saved fear clears silently; overseer fear remains PASS.");
}
static int floor_gems(void)
{
    int count = 0;
    for (int idx = 1; idx < o_max; idx++)
        if (o_list[idx].k_idx && o_list[idx].tval == TV_GEM && !o_list[idx].held_m_idx)
            count += o_list[idx].number;
    return count;
}

static monster_type* work_fixture(int race)
{
    character_dungeon = false;
    cave_environment_reset();
    monster_type* m = combat_fixture(race, 0);
    /* Ambient work cannot depend on seeing the player's position. */
    p_ptr->py = p_ptr->px = 80;
    cave_m_idx[80][80] = -1;
    m->alertness = ALERTNESS_UNWARY;
    m->stance = STANCE_CONFIDENT;
    character_dungeon = true;
    return m;
}

static void check_mining(void)
{
    bool dungeon_before = character_dungeon;
    const int races[] = {R_IDX_HUMAN_THRALL, R_IDX_ELF_THRALL};
    const int veins[] = {FEAT_QUARTZ, FEAT_CRACKED_QUARTZ};
    for (int r = 0; r < 2; r++) for (int v = 0; v < 2; v++)
    {
        monster_type* m = work_fixture(races[r]);
        character_dungeon = false;
        for (int y = 7; y <= 13; y++) for (int x = 7; x <= 13; x++)
            cave_set_feat(y, x, FEAT_WALL_EXTRA);
        cave_set_feat(10, 10, FEAT_FLOOR);
        cave_set_feat(10, 11, veins[v]);
        cave_set_feat(10, 12, FEAT_FLOOR);
        character_dungeon = true;
        for (int i = 0; i < 3000 && cave_feat[10][11] != FEAT_RUBBLE; i++)
            assert(monster_thrall_turn(m));
        assert(cave_feat[10][11] == FEAT_RUBBLE && m->fx == 10);
        assert(floor_gems() == 1);
        for (int i = 0; i < 3000 && cave_feat[10][11] == FEAT_RUBBLE; i++)
            assert(monster_thrall_turn(m));
        assert(cave_feat[10][11] == FEAT_FLOOR && m->fx == 10);
        for (int i = 0; i < 3000 && m->fx == 10; i++)
            assert(monster_thrall_turn(m));
        assert(m->fy == 10 && m->fx == 11 && floor_gems() == 1);
        assert(p_ptr->kill_exp == 0 && p_ptr->encounter_exp == 0);
    }
    character_dungeon = dungeon_before;
    puts("Human/elven miners: intact/cracked quartz yields one gem, rubble clearing costs its own action, then the cleared step is walkable PASS.");
}

static void check_gem_collection(void)
{
    bool dungeon_before = character_dungeon;
    /* Preserve the territorial flag in the primary case, and exercise the
     * ordinary carried-loot route as a control for the shared death helper. */
    for (int territorial = 0; territorial <= 1; territorial++)
    {
        monster_type* m = work_fixture(R_IDX_ORC_THRALLMASTER);
        monster_race* r = &r_info[m->r_idx];
        u32b original_flags = r->flags2;
        assert(original_flags & RF2_TERRITORIAL);
        if (!territorial) r->flags2 &= ~RF2_TERRITORIAL;
        int m_idx = cave_m_idx[m->fy][m->fx];
        object_type gem, other;
        object_prep(&gem, lookup_kind(TV_GEM, SV_GEM_LIGHT));
        gem.number = 3; gem.pval = 7; gem.marked = true;
        int first = floor_carry(10, 13, &gem); assert(first > 0);
        object_prep(&gem, lookup_kind(TV_GEM, SV_GEM_FREEDOM));
        gem.number = 2; gem.pval = 4;
        int second = floor_carry(10, 12, &gem); assert(second > 0);
        object_prep(&other, lookup_kind(TV_POTION, SV_POTION_HEALING));
        int ignored = floor_carry(10, 11, &other); assert(ignored > 0);
        int objects_before = o_cnt;
        for (int i = 0; i < 8 && floor_gems(); i++)
            assert(monster_thrall_turn(m));
        assert(m->fy == 10 && m->fx == 13 && floor_gems() == 0);
        assert(o_cnt == objects_before && m->hold_o_idx);
        assert(o_list[first].held_m_idx == m_idx && o_list[first].number == 3
            && o_list[first].pval == 7 && !o_list[first].marked);
        assert(o_list[second].held_m_idx == m_idx && o_list[second].number == 2
            && o_list[second].pval == 4);
        assert(!o_list[ignored].held_m_idx && cave_o_idx[10][11] == ignored);
        monster_death(m_idx);
        assert(!m->hold_o_idx && floor_gems() == 5);
        delete_monster_idx(m_idx);
        assert(floor_gems() == 5 && !o_list[ignored].held_m_idx);
        r->flags2 = original_flags;
    }
    character_dungeon = dungeon_before;
    puts("Real movement/pickup/death: idle overseers collect gem stacks with the player far away, leave other items, and return all gems on territorial/ordinary death PASS.");
}
'''


def main():
    out = engine.ROOT / "scripts/output/thrall-interaction"
    out.mkdir(parents=True, exist_ok=True)
    source = out / "check.c"
    initialization = engine.HARNESS.split("int main(int argc,char** argv)\n{", 1)[1]
    initialization = initialization.split("    check_templates();", 1)[0]
    source.write_text(
        '#include "angband.h"\n#include "init/init2-internal.h"\n'
        '#include "level-generation/level-generation-internal.h"\n'
        '#include <assert.h>\nstatic term test_term;\n'
        + "\n".join(fixture_function(name) for name in
            ("terminal_extra", "reset_map", "combat_fixture"))
        + CHECKS + "\nint main(int argc, char** argv)\n{"
        + initialization + "    check_exchanges(); check_thrall_fear();\n"
        + "    assert(init_flavor_info() == 0); flavor_init();\n"
        + "    check_mining(); check_gem_collection();\n"
        + '    puts("Thrall interaction integration: PASS.");\n'
        + "    SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    changed = ("src/cmd/movement/cmd-run.c", "src/melee/melee-process.c",
        "src/monster/monster-thrall.c", "src/player/player-song-effects.c",
        "src/spell/spell-projection-effects.c", "src/world/monster-death.c")
    cmake = engine.BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = tuple("/" + path + ".obj" for path in ("src/main.c", *changed))
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + path + '"' for path in objects
        if not path.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(engine.BUILD / "_deps" / name) for name in
            ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
        "-O0", "-g", "-UNDEBUG", "@CMakeFiles/sil-more.dir/includes_C.rsp",
        str(source), *(str(engine.ROOT / path) for path in changed),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        *("-Wl,--wrap=" + name for name in ("get_check_near", "py_attack",
            "handle_thrall_interaction", "msg_print", "msg_format")),
        "-o", str(exe)],
        cwd=engine.BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=out) as data:
        subprocess.run([str(exe), str(engine.ROOT / "lib/edit"), data],
            cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()

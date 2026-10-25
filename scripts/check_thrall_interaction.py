#!/usr/bin/env python3
"""Check thrall bump exchanges and fear through the production engine."""
import os
import shlex
import subprocess
import tempfile

import check_new_monsters as engine
from check_monster_scent_save import fixture_function


CHECKS = r'''
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
        + '    puts("Thrall interaction integration: PASS.");\n'
        + "    SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    changed = ("src/cmd/movement/cmd-run.c", "src/melee/melee-process.c",
        "src/monster/monster-thrall.c", "src/player/player-song-effects.c",
        "src/spell/spell-projection-effects.c")
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

#!/usr/bin/env python3
"""Check quest state across canceled, forced, and restored level travel."""

import os
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import BUILD, HARNESS, ROOT


OUT = ROOT / "scripts/output/level-departure-quests"
SOURCES = ("src/cmd/movement/cmd-movement.c", "src/quest/quest-core.c")

CHECKS = r'''
#include "cave/cave-environment.h"

static const char* decline;
static int confirmations;

bool __wrap_get_check_near(int y, int x, cptr prompt)
{
    assert(y == p_ptr->py && x == p_ptr->px);
    assert(strstr(prompt, "abandon the forge") || strstr(prompt, "abandon the tomb"));
    ++confirmations;
    return !decline || !strstr(prompt, decline);
}

void __wrap_message_flush(void)
{
    message_discard_pending();
}

static void prepare_level(int depth, int feat)
{
    wipe_o_list();
    wipe_mon_list();
    player_wipe();
    cave_environment_reset();
    rp_ptr = &p_info[0];
    current_character_profile = &c_info[0];
    p_ptr->playing = true;
    p_ptr->tutorial_deferred = true;
    p_ptr->depth = p_ptr->max_depth = depth;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 30;
    p_ptr->py = p_ptr->px = 10;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->food = PY_FOOD_FULL;
    p_ptr->aule_quest = AULE_QUEST_ACTIVE;
    p_ptr->mandos_quest = MANDOS_QUEST_SUCCESS;
    p_ptr->aule_level = p_ptr->mandos_level = p_ptr->depth;
    p_ptr->tulkas_quest = TULKAS_QUEST_COMPLETE;
    p_ptr->tulkas_target_r_idx = R_IDX_GOTHMOG;
    p_ptr->tulkas_prize_a_idx = ART_ORCRIST;
    p_ptr->quest_reserved[0] = QUEST_MAX_INITIATED_PER_RUN;
    character_generated = character_dungeon = true;
    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 30; ++x) {
            cave_o_idx[y][x] = cave_m_idx[y][x] = 0;
            cave_info[y][x] = 0;
            bool edge = !y || !x || y == 29 || x == 29;
            cave_set_feat(y, x, edge ? FEAT_WALL_PERM : FEAT_FLOOR);
        }
    }
    cave_m_idx[10][10] = -1;
    cave_set_feat(10, 10, feat);
    turn = 800;
    playerturn = 80;
    min_depth_counter = 0;
    decline = NULL;
    confirmations = 0;
    cave_environment_seed();
}

static void assert_live_quests(void)
{
    assert(p_ptr->aule_quest == AULE_QUEST_ACTIVE);
    assert(p_ptr->mandos_quest == MANDOS_QUEST_SUCCESS);
}

static void assert_departed_quests(void)
{
    assert(p_ptr->aule_quest == AULE_QUEST_NOT_STARTED);
    assert(p_ptr->mandos_quest == MANDOS_QUEST_NOT_STARTED);
    assert(!p_ptr->aule_level && !p_ptr->mandos_level);
    assert(p_ptr->tulkas_quest == TULKAS_QUEST_COMPLETE);
    assert(p_ptr->tulkas_prize_a_idx == ART_ORCRIST);
    assert(p_ptr->quest_reserved[0] == QUEST_MAX_INITIATED_PER_RUN);
}

static void round_trip(void)
{
    SDL_strlcpy(op_ptr->full_name, "TravelFixture", sizeof(op_ptr->full_name));
    process_player_name(true);
    assert(save_player());
    p_ptr->aule_quest = p_ptr->mandos_quest = 0xFF;
    p_ptr->tulkas_quest = 0xFF;
    assert(load_player() && character_loaded && character_dungeon);
}

static void check_varda_departure(void)
{
    const int states[] = {
        VARDA_QUEST_GIVER_PRESENT, VARDA_QUEST_ACTIVE, VARDA_QUEST_SUCCESS
    };
    for (int i = 0; i < (int)N_ELEMENTS(states); ++i) {
        prepare_level(3, FEAT_CHASM);
        p_ptr->aule_quest = AULE_QUEST_REWARDED;
        p_ptr->mandos_quest = MANDOS_QUEST_REWARDED;
        p_ptr->varda_quest = states[i];
        p_ptr->varda_level = 3;
        hit_trap(10, 10);
        assert(!p_ptr->is_dead && p_ptr->leaving && p_ptr->depth == 5);
        if (states[i] == VARDA_QUEST_GIVER_PRESENT) {
            assert(p_ptr->varda_quest == VARDA_QUEST_NOT_STARTED && !p_ptr->varda_level);
        } else {
            assert(p_ptr->varda_quest == states[i] && p_ptr->varda_level == 3);
        }
        assert(p_ptr->aule_quest == AULE_QUEST_REWARDED);
        assert(p_ptr->mandos_quest == MANDOS_QUEST_REWARDED);
        assert(p_ptr->quest_reserved[0] == QUEST_MAX_INITIATED_PER_RUN);
    }
    puts("Forced departure clears an unaccepted Varda encounter and keeps portable quest progress.");
}

static void checks(void)
{
    prepare_level(8, FEAT_MORE);
    decline = "tomb";
    do_cmd_go_down();
    assert(confirmations == 2 && !p_ptr->leaving && !p_ptr->energy_use);
    assert(p_ptr->depth == 8);
    assert_live_quests();
    round_trip();
    assert_live_quests();
    assert(p_ptr->depth == 8 && cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE);
    puts("Canceled stairs and save/reload in place preserve both quests.");

    decline = NULL;
    do_cmd_go_down();
    assert(confirmations == 4 && p_ptr->leaving && p_ptr->depth == 9);
    assert_departed_quests();
    prepare_level(8, FEAT_LESS);
    do_cmd_go_up();
    assert(confirmations == 2 && p_ptr->leaving && p_ptr->depth == 7);
    assert_departed_quests();
    puts("Confirmed stairs still abandon floor-bound quests in either direction.");

    check_varda_departure();

    prepare_level(8, FEAT_TRAP_false_FLOOR);
    hit_trap(10, 10);
    assert(!p_ptr->is_dead && p_ptr->leaving && p_ptr->depth == 9);
    assert_departed_quests();
    puts("A false-floor fall clears the old forge and tomb quests.");

    prepare_level(8, FEAT_CHASM);
    hit_trap(10, 10);
    assert(!p_ptr->is_dead && p_ptr->leaving && p_ptr->depth == 10);
    assert_departed_quests();
    wipe_o_list();
    wipe_mon_list();
    flavor_init();
    drop_system_init();
    Rand_state_init(97);
    generate_cave();
    p_ptr->leaving = false;
    round_trip();
    assert(p_ptr->depth == 10);
    assert_departed_quests();
    puts("Chasm departure clears the old quests through generation and reload.");

    prepare_level(8, FEAT_TRAP_false_FLOOR);
    p_ptr->chp = 1;
    hit_trap(10, 10);
    assert(p_ptr->is_dead && p_ptr->depth == 8);
    assert_live_quests();
    puts("A fatal fall preserves the quests on the level where the hero died.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
        SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    prefix = HARNESS[:HARNESS.index("static cptr make_path")]
    init = HARNESS[HARNESS.index("static cptr make_path"):HARNESS.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init
        + "    checks(); sdl_quit_hook(NULL); return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", *("/" + path + ".obj" for path in SOURCES))
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith(excluded)), encoding="utf-8")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        *(str(ROOT / path) for path in SOURCES),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=get_check_near", "-Wl,--wrap=message_flush",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        env["CONTROL_TEST_PROFILE"] = profile
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
            env=env, check=True, timeout=100)


if __name__ == "__main__":
    main()

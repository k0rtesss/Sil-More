#!/usr/bin/env python3
"""Exercise real loot generation, a quest reward and the stair command."""

import argparse
import os
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import BUILD, ROOT, HARNESS


OUT = ROOT / "scripts/output/quest-progression"
GAME_SOURCES = (
    "src/cmd/movement/cmd-movement.c",
    "src/drop_system.c",
    "src/object/object-make.c",
    "src/quest/quest-core.c",
    "src/quest/valar/tulkas-varda.c",
    "src/world/monster-death.c",
)

CHECKS = r'''
#include "metarun.h"

static bool confirm_ascent;
static int ascent_prompts;
static int rewards;
static bool saw_relic_story;
static bool fail_prize_drop;

s16b __real_drop_near(object_type* object, int chance, int y, int x);
s16b __wrap_drop_near(object_type* object, int chance, int y, int x)
{
    if (fail_prize_drop && object->name1 == p_ptr->tulkas_prize_a_idx)
        return 0;
    return __real_drop_near(object, chance, y, x);
}

bool __wrap_get_check_near(int y, int x, cptr prompt)
{
    assert(y == p_ptr->py && x == p_ptr->px);
    assert(strstr(prompt, "abandon the quest and ascend"));
    ++ascent_prompts;
    return confirm_ascent;
}

void __wrap_quest_typewriter_menu(cptr title, cptr texts[], int count,
    byte title_color, byte text_color)
{
    (void)title; (void)title_color; (void)text_color;
    for (int i = 0; i < count; ++i)
        if (strstr(texts[i], "Bear it well"))
            saw_relic_story = true;
}

void __wrap_metarun_mark_quest_completed(u32b flag)
{
    assert(flag == METARUN_QUEST_TULKAS);
    ++rewards;
}

void __wrap_metarun_unlock_oath(int oath)
{
    assert(oath == OATH_VALOROUS);
}

static void setup_level(int depth, int feat)
{
    p_ptr->depth = p_ptr->max_depth = depth;
    p_ptr->playing = true;
    p_ptr->is_dead = false;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 30;
    p_ptr->py = p_ptr->px = 10;
    p_ptr->chp = p_ptr->mhp = 100;
    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 30; ++x) {
            cave_feat[y][x] = FEAT_FLOOR;
            cave_m_idx[y][x] = cave_o_idx[y][x] = 0;
        }
    }
    cave_m_idx[10][10] = -1;
    cave_feat[10][10] = feat;
    turn = 2345;
    playerturn = 234;
}

static void check_tulkas(void)
{
    const int prize = ART_ORCRIST;
    const int replacement = ART_GLAMDRING;
    object_type generated;

    player_wipe();
    setup_level(19, FEAT_FLOOR);
    flavor_init();
    drop_system_init();
    for (int i = 1; i < z_info->art_max; ++i)
        a_info[i].cur_num = i != prize;
    Rand_state_init(37);
    assert(drop_generate_guaranteed_artefact(19, 19, DROP_QUALITY_SUPERB,
        DROP_TYPE_UNTHEMED, NULL, &generated));
    assert(generated.name1 == prize);

    a_info[prize].cur_num = 0;
    p_ptr->depth = 8;
    p_ptr->tulkas_quest = TULKAS_QUEST_GIVER_PRESENT;
    tulkas_quest_interaction();
    assert(p_ptr->tulkas_quest == TULKAS_QUEST_ACTIVE);
    assert(p_ptr->tulkas_prize_a_idx == prize && p_ptr->tulkas_target_r_idx > 0);
    p_ptr->depth = 19;
    Rand_state_init(37);
    assert(!drop_generate_guaranteed_artefact(19, 19, DROP_QUALITY_SUPERB,
        DROP_TYPE_UNTHEMED, NULL, &generated));

    /* The saved quest fields are available after resume, regardless of the
     * lifecycle of the in-memory reservation table. */
    assert(path_build(savefile, sizeof(savefile), ANGBAND_DIR_SAVE, "tulkas"));
    SDL_strlcpy(op_ptr->full_name, "TulkasFixture", sizeof(op_ptr->full_name));
    assert(save_player());
    p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
    p_ptr->tulkas_prize_a_idx = 0;
    if (valar_reserved_artifacts)
        valar_reserved_artifacts[prize] = false;
    assert(load_player() && character_loaded);
    assert(p_ptr->tulkas_quest == TULKAS_QUEST_ACTIVE);
    assert(p_ptr->tulkas_prize_a_idx == prize);
    Rand_state_init(37);
    assert(!drop_generate_guaranteed_artefact(19, 19, DROP_QUALITY_SUPERB,
        DROP_TYPE_UNTHEMED, NULL, &generated));

    check_tulkas_quest_completion(p_ptr->tulkas_target_r_idx);
    assert(p_ptr->tulkas_quest == TULKAS_QUEST_COMPLETE);
    Rand_state_init(37);
    assert(!drop_generate_guaranteed_artefact(19, 19, DROP_QUALITY_SUPERB,
        DROP_TYPE_UNTHEMED, NULL, &generated));
    create_chosen_artefact(prize, 10, 11, false);
    assert(!a_info[prize].cur_num && !cave_o_idx[10][11]);

    s32b xp_before = p_ptr->new_exp;
    fail_prize_drop = true;
    ++turn;
    tulkas_quest_interaction();
    assert(p_ptr->tulkas_quest == TULKAS_QUEST_COMPLETE && !rewards);
    assert(!a_info[prize].cur_num && !cave_o_idx[10][10]);
    assert(!saw_relic_story && p_ptr->new_exp == xp_before);

    fail_prize_drop = false;
    ++turn;
    tulkas_quest_interaction();
    assert(p_ptr->tulkas_quest == TULKAS_QUEST_REWARDED && rewards == 1);
    int item = cave_o_idx[10][10];
    assert(item > 0 && o_list[item].name1 == prize && o_list[item].number == 1);
    assert(a_info[prize].cur_num == 1 && saw_relic_story);
    ++turn;
    tulkas_quest_interaction();
    assert(rewards == 1 && cave_o_idx[10][10] == item);
    puts("Tulkas: catalog respects active and unclaimed prize before and after load; claim grants one promised artefact.");

    /* Old saves may already have generated the promised artefact. */
    const int existing_count = o_cnt;
    a_info[replacement].cur_num = 0;
    p_ptr->tulkas_quest = TULKAS_QUEST_COMPLETE;
    saw_relic_story = false;
    xp_before = p_ptr->new_exp;
    fail_prize_drop = true;
    ++turn;
    tulkas_quest_interaction();
    assert(p_ptr->tulkas_quest == TULKAS_QUEST_COMPLETE && rewards == 1);
    assert(p_ptr->tulkas_prize_a_idx == replacement);
    assert(!a_info[replacement].cur_num && o_cnt == existing_count);
    assert(!saw_relic_story && p_ptr->new_exp == xp_before);

    fail_prize_drop = false;
    ++turn;
    tulkas_quest_interaction();
    assert(p_ptr->tulkas_quest == TULKAS_QUEST_REWARDED && rewards == 2);
    assert(p_ptr->tulkas_prize_a_idx == replacement);
    assert(o_cnt == existing_count + 1 && a_info[replacement].cur_num == 1);
    assert(o_list[item].name1 == prize);

    p_ptr->tulkas_quest = TULKAS_QUEST_COMPLETE;
    saw_relic_story = false;
    xp_before = p_ptr->new_exp;
    ++turn;
    tulkas_quest_interaction();
    assert(p_ptr->tulkas_quest == TULKAS_QUEST_REWARDED && rewards == 3);
    assert(!p_ptr->tulkas_prize_a_idx && o_cnt == existing_count + 1);
    assert(!saw_relic_story && p_ptr->new_exp == xp_before + QUEST_COMPLETION_EXP);

    birth_no_artefacts = true;
    p_ptr->tulkas_quest = TULKAS_QUEST_COMPLETE;
    p_ptr->tulkas_prize_a_idx = ART_DURIN;
    a_info[ART_DURIN].cur_num = 0;
    xp_before = p_ptr->new_exp;
    ++turn;
    tulkas_quest_interaction();
    assert(p_ptr->tulkas_quest == TULKAS_QUEST_REWARDED && rewards == 4);
    assert(!a_info[ART_DURIN].cur_num && !p_ptr->tulkas_prize_a_idx);
    assert(!saw_relic_story && p_ptr->new_exp == xp_before + QUEST_COMPLETION_EXP);
    puts("Tulkas: original and replacement placement can be retried; exhausted pool and no-artefacts mode claim no relic.");
}

static void check_nienna(void)
{
    setup_level(15, FEAT_LESS);
    assert(min_depth() < 15);
    p_ptr->niena_quest = NIENA_QUEST_ACTIVE;
    p_ptr->niena_level = 15;
    p_ptr->niena_monsters_seen = 3;

    do_cmd_go_up();
    assert(ascent_prompts == 1 && p_ptr->depth == 15 && !p_ptr->leaving);
    assert(p_ptr->niena_quest == NIENA_QUEST_ACTIVE && !p_ptr->energy_use);

    confirm_ascent = true;
    do_cmd_go_up();
    assert(ascent_prompts == 2 && p_ptr->leaving && p_ptr->depth == 14);
    assert(p_ptr->niena_quest == NIENA_QUEST_FAILED && !p_ptr->niena_level);
    cave_feat[10][10] = FEAT_MORE;
    check_niena_quest_completion();
    assert(p_ptr->niena_quest == NIENA_QUEST_FAILED);
    puts("Nienna: declining ascent keeps the quest active; confirmed ascent records failure.");

    setup_level(15, FEAT_MORE_SHAFT);
    p_ptr->leaving = false;
    p_ptr->niena_level = 15;
    p_ptr->niena_quest = NIENA_QUEST_ACTIVE;
    p_ptr->leaping = true;
    check_niena_quest_interaction();
    assert(p_ptr->niena_quest == NIENA_QUEST_ACTIVE);
    p_ptr->leaping = false;
    check_niena_quest_interaction();
    assert(p_ptr->niena_quest == NIENA_QUEST_SUCCESS);
    do_cmd_go_down();
    assert(p_ptr->leaving && p_ptr->niena_quest == NIENA_QUEST_SUCCESS);

    setup_level(15, FEAT_MORE);
    p_ptr->leaving = false;
    p_ptr->niena_level = 15;
    p_ptr->niena_quest = NIENA_QUEST_ACTIVE;
    do_cmd_go_down(); /* A second action can run before the world tick. */
    assert(p_ptr->leaving && p_ptr->niena_quest == NIENA_QUEST_SUCCESS);

    setup_level(15, FEAT_TRAP_false_FLOOR);
    p_ptr->leaving = false;
    p_ptr->niena_level = 15;
    p_ptr->niena_quest = NIENA_QUEST_ACTIVE;
    hit_trap(10, 10);
    assert(p_ptr->depth == 16 && p_ptr->niena_quest == NIENA_QUEST_FAILED);
    p_ptr->leaving = false;
    cave_set_feat(10, 10, FEAT_MORE);
    check_niena_quest_completion();
    assert(p_ptr->niena_quest == NIENA_QUEST_FAILED);

    /* An active trial carried over by an older save belongs to its original floor. */
    p_ptr->niena_level = 15;
    p_ptr->niena_quest = NIENA_QUEST_ACTIVE;
    check_niena_quest_completion();
    assert(p_ptr->niena_quest == NIENA_QUEST_FAILED);
    puts("Nienna: earned reward survives descent, forced departure ends an unfinished trial, and a stale assignment cannot complete elsewhere.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenario", choices=("tulkas", "nienna"))
    args = parser.parse_args()
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
        + f"    check_{args.scenario}(); sdl_quit_hook(NULL); return 0;\n}}\n",
        encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", *("/" + name + ".obj" for name in GAME_SOURCES))
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
        if not obj.endswith(excluded)), encoding="utf-8")
    exe = OUT / "check.exe"
    wraps = ("get_check_near", "drop_near", "quest_typewriter_menu",
        "metarun_mark_quest_completed", "metarun_unlock_oath")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        *(str(ROOT / name) for name in GAME_SOURCES), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        env["CONTROL_TEST_PROFILE"] = profile
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
            env=env, check=True, timeout=80)


if __name__ == "__main__":
    main()

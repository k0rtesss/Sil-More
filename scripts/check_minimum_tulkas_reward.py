#!/usr/bin/env python3
"""Check Tulkas reward keeps its existing saved story fields after completion."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from qa9_sdl_fixture import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/minimum-tulkas-reward"

CHECKS = r'''
#include "achievements.h"
#include "metarun.h"
static int created, rewarded, stories, marks, unlocks, removed;
void __wrap_create_chosen_artefact(byte id, int y, int x, bool identified)
{ assert(id == 1 && y == p_ptr->py && x == p_ptr->px && identified); ++created; }
void __wrap_apply_quest_rewards(int id) { assert(id == QUEST_ID_TULKAS); ++rewarded; }
void __wrap_metarun_mark_quest_completed(u32b flag)
{ assert(flag == METARUN_QUEST_TULKAS); ++marks; }
void __wrap_metarun_unlock_oath(int oath) { assert(oath == OATH_VALOROUS); ++unlocks; }
void __wrap_remove_quest_giver(int race) { assert(race == R_IDX_TULKAS); ++removed; }
void __wrap_sil_achievements_on_event(sil_achievement_id id) { (void)id; }
void __wrap_quest_minimum_dialogue(int id, bool complete, cptr title,
    cptr texts[], int count, byte title_color, byte text_color)
{
    (void)title_color; (void)text_color;
    assert(id == QUEST_ID_TULKAS && complete && title && texts && count > 0);
    assert(p_ptr->tulkas_target_r_idx == 51 && p_ptr->tulkas_prize_a_idx == 1);
    ++stories;
}
static void tests(void)
{
    op_ptr->opt[OPT_minimum_text_mode] = true;
    p_ptr->playing = true; p_ptr->py = p_ptr->px = 10;
    p_ptr->tulkas_quest = TULKAS_QUEST_COMPLETE;
    p_ptr->tulkas_target_r_idx = 51; p_ptr->tulkas_prize_a_idx = 1;
    p_ptr->tulkas_quest_complete = 1; turn = 1234;
    if (!valar_reserved_artifacts)
        valar_reserved_artifacts = mem_alloc_array(z_info->art_max, bool);
    assert(valar_reserved_artifacts);
    valar_reserved_artifacts[1] = true;
    tulkas_quest_interaction();
    assert(p_ptr->tulkas_quest == TULKAS_QUEST_REWARDED);
    assert(p_ptr->tulkas_target_r_idx == 51 && p_ptr->tulkas_prize_a_idx == 1);
    assert(!p_ptr->tulkas_quest_complete && !valar_reserved_artifacts[1]);
    assert(created == 1 && rewarded == 1 && stories == 1 && marks == 1 && unlocks == 1 && removed == 1);
    ++turn; tulkas_quest_interaction();
    assert(created == 1 && rewarded == 1 && stories == 1 && marks == 1 && unlocks == 1 && removed == 1);
    puts("Tulkas reward: exact-once grant, reservation release, saved story identity retained PASS");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = HARNESS[:HARNESS.index("static cptr make_path")]
    init = HARNESS[HARNESS.index("static cptr make_path"):HARNESS.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "    tests(); sdl_quit_hook(NULL); return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(
        ("/src/main.c.obj", "/src/quest/valar/tulkas-varda.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    wraps = ("create_chosen_artefact", "apply_quest_rewards", "metarun_mark_quest_completed",
             "metarun_unlock_oath", "remove_quest_giver", "sil_achievements_on_event", "quest_minimum_dialogue")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/quest/valar/tulkas-varda.c"), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        env["CONTROL_TEST_PROFILE"] = profile
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT, env=env,
                       check=True, timeout=30)


if __name__ == "__main__":
    main()

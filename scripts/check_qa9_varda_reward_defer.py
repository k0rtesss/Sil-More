#!/usr/bin/env python3
"""Check Varda reward deferral, failed placement, and completion without a relic.

Uses the real quest interaction, reward creation and initialized SDL engine.
Controls the book's answer, placement failure, and metarun side effects. --baseline
compiles the committed Varda source and reproduces repeated reward prompts.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/qa9-varda-reward-defer"
SUPPORT_SOURCES = ("src/quest/quest-status.c", "src/world/monster-death.c")

CHECKS = r'''
#include "metarun.h"
#include "quest/quest-internal.h"
static int book_calls, reward_marks, oath_unlocks, chosen_artifact, completion_books;
static int relic_notes, oath_only_notes;
static int anticipated_gifts[3];
static int anticipated_gift_count;
static u32b expected_completion_flag;
static bool take_reward;
static bool fail_placement;
s16b __real_drop_near(object_type* object, int chance, int y, int x);
s16b __wrap_drop_near(object_type* object, int chance, int y, int x)
{
    if (fail_placement && object->name1 == chosen_artifact)
        return 0;
    return __real_drop_near(object, chance, y, x);
}
#ifdef QA_BASELINE
void varda_quest_begin_player_turn(void) {}
#endif
int __wrap_quest_reward_book_choice(cptr title, cptr words[], int text_count,
    cptr prompt, const int values[], cptr labels[], const bool allowed[],
    int count, int initial, void (*inspect)(int))
{
    (void)words; (void)text_count; (void)prompt; (void)labels;
    (void)allowed; (void)initial; (void)inspect;
    assert(!strcmp(title,"Starlight Triumph") && count == anticipated_gift_count);
    for (int i = 0; i < count; ++i) {
        bool offered_gift = false;
        for (int j = 0; j < count; ++j)
            if (values[i] == anticipated_gifts[j])
                offered_gift = true;
        assert(offered_gift);
        for (int j = 0; j < i; ++j)
            assert(values[i] != values[j]);
    }
    ++book_calls;
    if (!take_reward) return -1;
    chosen_artifact=values[0];
    return chosen_artifact;
}
void __wrap_metarun_mark_quest_completed(u32b flag)
{ assert(flag==expected_completion_flag); ++reward_marks; }
void __wrap_metarun_unlock_oath(int oath)
{ assert(oath==OATH_LIGHT); ++oath_unlocks; }
void __wrap_do_cmd_note(char* note, int depth)
{
    assert(depth == 11 && strstr(note, "Oath of Light"));
    if (strstr(note, "radiant artefact")) {
        assert(chosen_artifact > 0 && strstr(note, a_info[chosen_artifact].name));
        ++relic_notes;
    } else {
        ++oath_only_notes;
    }
}
void __wrap_quest_typewriter_menu(cptr title, cptr words[], int count,
    byte title_color, byte text_color)
{
    (void)title; (void)words; (void)count; (void)title_color; (void)text_color;
    ++completion_books;
}
static void begin_action(void)
{
    ++playerturn;
#ifndef QA_BASELINE
    varda_quest_begin_player_turn();
#endif
}
static void world_ticks(int count)
{
    for(int i=0;i<count;++i) {
        check_varda_quest_interaction();
        ++turn;
    }
}
static void place_varda(void)
{
    p_ptr->varda_quest = VARDA_QUEST_SUCCESS;
    mon_max = 2;
    mon_cnt = r_info[R_IDX_VARDA].cur_num = 1;
    mon_list[1] = (monster_type){.r_idx=R_IDX_VARDA,.fy=9,.fx=10};
    cave_m_idx[9][10] = 1;
}
static void check_no_relic(bool no_artefacts)
{
    for (int i = 0; i < anticipated_gift_count; ++i)
        a_info[anticipated_gifts[i]].cur_num = 1;
    birth_no_artefacts = no_artefacts;
    if (no_artefacts)
        a_info[anticipated_gifts[0]].cur_num = 0;

    int original_oath_notes = oath_only_notes;
    int original_marks = reward_marks;
    int original_books = book_calls;
    int original_objects = o_cnt;
    s32b original_xp = p_ptr->new_exp;
    place_varda();
    begin_action();
    world_ticks(10);
    assert(p_ptr->varda_quest == VARDA_QUEST_REWARDED);
    assert(!mon_list[1].r_idx && !cave_m_idx[9][10]);
    assert(book_calls == original_books && o_cnt == original_objects);
    assert(reward_marks == original_marks + 1 && oath_unlocks == reward_marks);
    assert(relic_notes == 1 && oath_only_notes == original_oath_notes + 1);
    assert(p_ptr->new_exp == original_xp + QUEST_COMPLETION_EXP);

    char summary[512];
    quest_minimum_summary(QUEST_ID_VARDA, true, summary, sizeof(summary));
    assert(strstr(summary, "Oath of Light") && !strstr(summary, "radiant artefact"));
    int count = 0;
    cptr* story = extract_quest_completion_texts(QUEST_ID_VARDA, &count);
    assert(story && count > 0);
    for (int i = 0; i < count; ++i)
        assert(!strstr(story[i], "Choose now"));
    free_quest_texts(story, count);

    begin_action();
    world_ticks(10);
    assert(reward_marks == original_marks + 1 && oath_only_notes == original_oath_notes + 1);
}
static void checks(void)
{
    int gifts = 0;
    for (int i = 1; i < z_info->art_max; ++i) {
        artefact_type* artefact = &a_info[i];
        if (!artefact->tval || !artefact->name[0]
            || !(artefact->flags2 & (TR2_LIGHT | TR2_RADIANCE))
            || (artefact->flags2 & TR2_DARKNESS)
            || (artefact->flags3 & TR3_LIGHT_CURSE)
            || (artefact->flags4 & TR4_UNLIGHT))
            continue;
        /* Leave one shallow and two deep gifts to force the second pass. */
        if (gifts < 3) {
            anticipated_gifts[gifts] = i;
            artefact->level = gifts++ ? 18 : 15;
            artefact->cur_num = 0;
        } else {
            artefact->cur_num = 1;
        }
    }
    assert(gifts == 3);
    anticipated_gift_count = 2;
    a_info[anticipated_gifts[2]].cur_num = 1;
    expected_completion_flag=METARUN_QUEST_VARDA;
    p_ptr->playing=true; p_ptr->is_dead=false;
    p_ptr->py=p_ptr->px=10; p_ptr->depth=11;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=30;
    p_ptr->chp=p_ptr->mhp=100;
    p_ptr->varda_level=11;
    o_max=1; o_cnt=0;
    for(int y=0;y<30;++y) for(int x=0;x<30;++x) {
        cave_feat[y][x]=FEAT_FLOOR;
        cave_m_idx[y][x]=cave_o_idx[y][x]=0;
    }
    place_varda();
    turn=2301; playerturn=230;
    s32b xp=p_ptr->new_exp;
    begin_action();
    world_ticks(10);
    assert(book_calls==1);
    assert(p_ptr->varda_quest==VARDA_QUEST_SUCCESS);
    assert(mon_list[1].r_idx==R_IDX_VARDA && !cave_o_idx[10][10]);
    assert(p_ptr->new_exp==xp);
    assert(!reward_marks && !oath_unlocks && !p_ptr->energy_use);
    /* Free inspection/observations do not start another player action. */
    world_ticks(5);
    assert(book_calls==1 && playerturn==231);
    /* A later paid action can offer the pending reward once again. */
    anticipated_gift_count = 3;
    a_info[anticipated_gifts[2]].cur_num = 0;
    begin_action(); world_ticks(20);
    assert(book_calls==2 && p_ptr->varda_quest==VARDA_QUEST_SUCCESS);
    assert(!cave_o_idx[10][10] && p_ptr->new_exp==xp);
    /* Failed placement is deferred for the rest of this action. */
    begin_action(); take_reward=true; fail_placement=true; world_ticks(10);
    assert(book_calls==3 && chosen_artifact>0);
    assert(p_ptr->varda_quest==VARDA_QUEST_SUCCESS);
    assert(mon_list[1].r_idx==R_IDX_VARDA && !cave_o_idx[10][10]);
    assert(!a_info[chosen_artifact].cur_num && p_ptr->new_exp==xp);
    assert(!reward_marks && !oath_unlocks && !relic_notes && !oath_only_notes);

    /* A later turn can successfully place the chosen relic exactly once. */
    begin_action(); fail_placement=false; world_ticks(10);
    assert(book_calls==4 && chosen_artifact>0);
    assert(p_ptr->varda_quest==VARDA_QUEST_REWARDED);
    assert(!mon_list[1].r_idx && !cave_m_idx[9][10]);
    int object=cave_o_idx[10][10];
    assert(object>0 && o_list[object].name1==chosen_artifact);
    assert(o_list[object].number==1 && !o_list[object].next_o_idx);
    assert(a_info[chosen_artifact].cur_num==1);
    assert(reward_marks==1 && oath_unlocks==1 && relic_notes==1);
    assert(p_ptr->new_exp>xp);
    begin_action(); world_ticks(20);
    assert(book_calls==4 && reward_marks==1 && oath_unlocks==1);
    assert(cave_o_idx[10][10]==object && o_list[object].number==1);
    check_no_relic(false);
    check_no_relic(true);
    puts("Varda: distinct gifts, delayed and retryable placement, and truthful completion with an empty pool or no-artefacts mode PASS.");

}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    controls = parser.add_mutually_exclusive_group()
    controls.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    vala = ROOT / "src/quest/valar/tulkas-varda.c"
    if args.baseline:
        vala = OUT / "varda-baseline.c"
        vala.write_text(subprocess.check_output([
            "git", "show", "HEAD:src/quest/valar/tulkas-varda.c"],
            cwd=ROOT, text=True, encoding="utf-8"), encoding="utf-8")
    prefix = HARNESS[:HARNESS.index("static cptr make_path")]
    init = HARNESS[HARNESS.index("static cptr make_path"):HARNESS.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init
                      + "    checks(); sdl_quit_hook(NULL); return 0;\n}\n",
                      encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    excluded = ("/src/main.c.obj", "/src/quest/valar/tulkas-varda.c.obj",
        *("/" + path + ".obj" for path in SUPPORT_SOURCES))
    response.write_text("\n".join('"' + p + '"' for p in objects
        if not p.endswith(excluded)),
        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / p) for p in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
        SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        *(["-DQA_BASELINE"] if args.baseline else []),
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(vala),
        *(str(ROOT / path) for path in SUPPORT_SOURCES),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        *("-Wl,--wrap=" + name for name in ("quest_reward_book_choice",
            "metarun_mark_quest_completed", "metarun_unlock_oath", "do_cmd_note",
            "quest_typewriter_menu", "drop_near")),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as profile:
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
            env=dict(env, CONTROL_TEST_PROFILE=profile), check=True, timeout=30)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Load real templates, roundtrip settings, and build all recovered vaults in engine memory."""
from pathlib import Path
import argparse
import os
import re
import shlex
import subprocess
import tempfile
from collections import deque
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/quest-integration"
BUILD = ROOT / "build-standard"

TESTS = r'''
#include "quest/quest-runtime.h"
#include "sdl-config.h"
#include "fs/path.h"
#include "blitz.h"
static int checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static int dialogue_lines, dialogue_nonempty, dialogue_calls;
void __wrap_quest_typewriter_menu(cptr title,cptr texts[],int count,byte a,byte b)
{
    (void)title;(void)a;(void)b;
    ++dialogue_calls; dialogue_lines=count; dialogue_nonempty=0;
    for(int i=0;i<count;++i) if(texts[i] && texts[i][0]) ++dialogue_nonempty;
}
static void nienna_reward_dialogue(void)
{
    /* Only the presentation boundary is adapted. Real templates, interaction,
     * rewards and state transitions run for both empty and populated routes. */
    int expected=0; cptr *texts=extract_quest_completion_texts(QUEST_ID_NIENA,&expected);
    CHECK(texts && expected>1); free_quest_texts(texts,expected);
    for(int seen=0;seen<=2;++seen) {
        memset(p_ptr,0,sizeof(*p_ptr)); p_ptr->quest_test_sandbox=1;
        p_ptr->depth=14; p_ptr->niena_quest=NIENA_QUEST_SUCCESS;
        p_ptr->niena_monsters_seen=seen*10;
        quest_set_enabled(QUEST_ID_NIENA,true);
        turn=2000+10*seen; dialogue_calls=dialogue_lines=dialogue_nonempty=0;
        niena_quest_interaction();
        CHECK(dialogue_calls==1 && dialogue_lines==expected && dialogue_nonempty>1);
        CHECK(p_ptr->niena_quest==NIENA_QUEST_REWARDED);
        CHECK(p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY]);
        niena_quest_interaction(); CHECK(dialogue_calls==1);
    }
    puts("Nienna reward: zero/10/20 creatures, full authored dialogue, reward and once-only completion PASS.");
}
static void orome_hunt_depths(void)
{
    static const int depths[]={2,5,6,10,11,15,16,20};
    static const int targets[]={OROME_TARGET_WOLF,OROME_TARGET_WOLF,
        OROME_TARGET_SPIDER,OROME_TARGET_SPIDER,OROME_TARGET_SERPENT,
        OROME_TARGET_SERPENT,OROME_TARGET_VAMPIRE,OROME_TARGET_VAMPIRE};
    static const int counts[]={100,100,80,80,60,60,30,30};
    for(int i=0;i<(int)N_ELEMENTS(depths);i++) {
        memset(p_ptr,0,sizeof(*p_ptr));p_ptr->quest_test_sandbox=1;
        p_ptr->depth=depths[i];p_ptr->orome_quest=OROME_QUEST_GIVER_PRESENT;
        quest_set_enabled(QUEST_ID_OROME,true);turn=3000+10*i;
        orome_quest_interaction();
        CHECK(p_ptr->orome_quest==OROME_QUEST_ACTIVE);
        CHECK(p_ptr->orome_target_type==targets[i]);
        CHECK(p_ptr->orome_target_count==counts[i]);
    }
    puts("Orome actual offers: level-to-feet conversion and both sides of250/500/750ft boundaries PASS.");
}
static const int opts[] = { OPT_quest_1,OPT_quest_2,OPT_quest_3,OPT_quest_4,
    OPT_quest_5,OPT_quest_6,OPT_quest_7,OPT_quest_8,OPT_quest_9,OPT_quest_10,
    OPT_quest_11,OPT_quest_12,OPT_quest_13,OPT_quest_14,OPT_quest_15,OPT_quest_16,
    OPT_quest_rules_beta,OPT_quest_rewards_beta,OPT_quest_challenges_beta,OPT_quest_lineage_beta };
static void settings(void)
{
    static struct sdl_config cfg;
    char path[1024]; path_build(path,sizeof(path),ANGBAND_DIR_USER,"quests.json");
    sdl_config_set_defaults(&cfg);
    sdl_config_reset_app_options_to_defaults();
    for(int i=0;i<20;++i) {
        CHECK(option_norm[opts[i]] == (i<6)); CHECK(op_ptr->opt[opts[i]] == (i<6));
        CHECK(option_is_app_persistent(opts[i]));
        int count=0; for(int j=0;j<OPT_PAGE_PER;++j) count += option_page[QUEST_PAGE][j]==opts[i];
        CHECK(count==1);
    }
    /* One-hot/one-cold patterns exercise every independent key, plus all on/off. */
    for(int pattern=0;pattern<42;++pattern) {
        for(int i=0;i<20;++i) op_ptr->opt[opts[i]] = pattern<20 ? i==pattern
            : pattern<40 ? i!=pattern-20 : pattern==40;
        CHECK(sdl_config_save(path,&cfg,NULL,0));
        for(int i=0;i<20;++i) op_ptr->opt[opts[i]] = false;
        CHECK(sdl_config_load(path,&cfg,NULL,0,NULL)==SDL_CONFIG_LOAD_OK);
        sdl_config_load_app_options(path);
        for(int i=0;i<20;++i) CHECK(op_ptr->opt[opts[i]] == (pattern<20 ? i==pattern
            : pattern<40 ? i!=pattern-20 : pattern==40));
    }
    SDL_IOStream *f=SDL_IOFromFile(path,"wb"); CHECK(f);
    CHECK(SDL_WriteIO(f,"{}",2)==2); SDL_CloseIO(f);
    CHECK(sdl_config_load(path,&cfg,NULL,0,NULL)==SDL_CONFIG_LOAD_OK);
    sdl_config_load_app_options(path);
    for(int i=0;i<20;++i) CHECK(op_ptr->opt[opts[i]] == (i<6));
    for(int i=0;i<20;++i) op_ptr->opt[opts[i]]=true;
    run_mode_set_current(RUN_MODE_BLITZ);
    for(int id=1;id<=16;++id) CHECK(quest_enabled(id)==(id<=6));
    CHECK(!quest_rules_enabled() && !quest_rewards_enabled() && !quest_challenges_enabled() && !quest_lineage_enabled());
    run_mode_set_current(RUN_MODE_STORY); sdl_config_reset_app_options_to_defaults();
}
static void vaults(void)
{
    const int ids[]={8,7,13,15}, depths[]={17,10,5,16};
    for(int i=0;i<4;++i) {
        reset_map(depths[i]); quest_followup_reset();
        CHECK(!vault_is_valid_for_depth(&v_info[464+i],depths[i]));
        quest_set_enabled(ids[i],true); p_ptr->quest_test_sandbox=1;
        quest_debug_request_vault(ids[i]);
        CHECK(vault_is_valid_for_depth(&v_info[464+i],depths[i]));
        CHECK(build_vault(44,44,&v_info[464+i],false));
        int targets=0;
        for(int m=1;m<mon_max;++m) {
            int r=mon_list[m].r_idx;
            if (!r || !(r_info[r].flags1 & RF1_UNIQUE) || (r_info[r].flags1 & RF1_PEACEFUL)) continue;
            ++targets;
        }
        CHECK(targets==(i==0?1:i==1?2:i==2?6:1));
        /* Production vault construction must leave all authored target races on the map. */
        for(int c=32;c<127;++c) {
            int race=quest_vault_token_race(464+i,(char)c);
            if(race<=0 || !(r_info[race].flags1 & RF1_UNIQUE)) continue;
            int present=0; for(int m=1;m<mon_max;++m) present += mon_list[m].r_idx==race;
            CHECK(present==1);
        }
        quest_set_enabled(ids[i],false);
    }
}

static void level_exits(void)
{
    for(int route=0;route<4;++route) {
        wipe_o_list(); wipe_mon_list();
        memset(p_ptr,0,sizeof(*p_ptr)); reset_map(16); quest_followup_reset();
        for(int id=1;id<=16;++id) quest_set_enabled(id,false);
        quest_set_state(QUEST_ID_VARDA,QUEST_STATE_REWARDED);
        quest_set_enabled(QUEST_ID_VARDA_SHADOW,true);
        quest_set_state(QUEST_ID_VARDA_SHADOW,QUEST_STATE_ACTIVE);
        quest_followup_vault_placed(QUEST_ID_VARDA_SHADOW,16);
        quest_followup_generation_commit();
        CHECK(quest_accepted_count_this_run()==1);
        CHECK(quest_followup_reserve_race(R_IDX_BELEGWATH));
        p_ptr->chp=p_ptr->mhp=30000; p_ptr->playing=true;
        p_ptr->tutorial_deferred=true; p_ptr->fixed_forge_count=3;
        p_ptr->song1=p_ptr->song2=SNG_NOTHING;
        turn=1000;playerturn=100;character_generated=true;character_dungeon=true;
        Rand_state_init(901+route);
        if(route<2) {
            cave_set_feat(p_ptr->py,p_ptr->px,route==0?FEAT_CHASM:FEAT_TRAP_false_FLOOR);
            hit_trap(p_ptr->py,p_ptr->px);
            CHECK(p_ptr->leaving && p_ptr->depth==(route==0?18:17));
        } else if(route==2) {
            teleport_player_level();
            CHECK(p_ptr->leaving && (p_ptr->depth==15 || p_ptr->depth==17));
        } else {
            /* Replacing a paused quest's map at the same depth still loses it. */
            quest_set_enabled(QUEST_ID_VARDA_SHADOW,false);
        }
        generate_cave();
        CHECK(character_dungeon && !p_ptr->is_dead);
        quest_set_enabled(QUEST_ID_VARDA_SHADOW,true);
        CHECK(!quest_accepted_count_this_run());
        CHECK(!quest_followup_reserve_race(R_IDX_BELEGWATH));
        quest_followup_kill(R_IDX_BELEGWATH);
        CHECK(quest_get_state(QUEST_ID_VARDA_SHADOW)==QUEST_STATE_ACTIVE);
        CHECK(!p_ptr->quest_followup_progress[QUEST_ID_VARDA_SHADOW-7]);
    }
    puts("Quest level exits: real chasm, false floor, teleport and paused same-depth regeneration PASS.");
}
'''


def topology():
    content = (ROOT / "lib/edit/vault.txt").read_text(encoding="utf-8")
    symbols = {464: "mN", 465: "{}N", 466: "bgclpt", 467: "_"}
    for block in re.split(r"(?=^N:)", content, flags=re.M):
        match = re.match(r"N:(\d+):", block)
        if not match or int(match[1]) not in symbols:
            continue
        serial = int(match[1])
        rows = re.findall(r"^D:(.*)$", block, re.M)
        h, w = len(rows), len(rows[0])
        assert all(len(row) == w for row in rows)
        entrances = [(y, x) for y in range(h) for x in range(w)
                     if (y in (0, h-1) or x in (0, w-1)) and rows[y][x] == "$" ]
        assert entrances, f"Vault {serial} has no tunnelable boundary"
        seen = set(entrances)
        queue = deque(entrances)
        while queue:
            y, x = queue.popleft()
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    ny, nx = y+dy, x+dx
                    if 0 <= ny < h and 0 <= nx < w and (ny, nx) not in seen and rows[ny][nx] not in "#%":
                        seen.add((ny, nx)); queue.append((ny, nx))
        for y, row in enumerate(rows):
            for x, char in enumerate(row):
                if char in symbols[serial]:
                    assert (y, x) in seen, f"Vault {serial} isolates target {char}"


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-nienna',action='store_true',help='Prove the Nienna dialogue regression against HEAD')
    parser.add_argument('--baseline-orome',action='store_true',help='Prove the Orome depth regression against HEAD')
    args=parser.parse_args()
    topology()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    harness = prefix + fixture_function("terminal_extra") + "\n" + fixture_function("reset_map")
    harness += "\n" + TESTS + "\n" + init
    harness += '''
    CHECK(init_quest_info()==0);
    for(int id=1;id<=16;++id) CHECK(quest_info[id].name && quest_info[id].vala_id);
    settings(); /* QUEST_CHECKS */
    printf("Quest engine integration: %d checks PASS (real templates/vault builds, 42 JSON patterns, defaults, Blitz).\\n",checks);
    SDL_Quit(); return 0;
}
'''
    harness=harness.replace('/* QUEST_CHECKS */','orome_hunt_depths();' if args.baseline_orome else 'nienna_reward_dialogue(); orome_hunt_depths(); vaults(); level_exits();')
    source = OUT / "check.c"
    source.write_text(harness, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"'+o+'"' for o in objects if not o.endswith(("/src/main.c.obj","/src/quest/valar/other-valar.c.obj"))))
    valar=ROOT/'src/quest/valar/other-valar.c'
    if args.baseline_nienna or args.baseline_orome:
        valar=OUT/'baseline-other-valar.c'
        valar.write_bytes(subprocess.check_output(['git','show','HEAD:src/quest/valar/other-valar.c'],cwd=ROOT))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / n) for n in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(valar), "@"+str(response),
        "-Wl,--wrap=quest_typewriter_menu",
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        result = subprocess.run([str(exe), str(ROOT / "lib/edit"), data], cwd=data,
            env=env, capture_output=True, text=True, timeout=90)
        (OUT / "validation.log").write_text(result.stdout+result.stderr, encoding="utf-8")
        print(result.stdout, end="")
        if result.returncode:
            print(result.stderr[-6000:]); result.check_returncode()


if __name__ == "__main__":
    main()

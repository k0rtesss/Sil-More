#!/usr/bin/env python3
"""Load real templates, roundtrip settings, and build all recovered vaults in engine memory."""
from pathlib import Path
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
    settings(); vaults(); level_exits();
    printf("Quest engine integration: %d checks PASS (real templates/vault builds, 42 JSON patterns, defaults, Blitz).\\n",checks);
    SDL_Quit(); return 0;
}
'''
    source = OUT / "check.c"
    source.write_text(harness, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"'+o+'"' for o in objects if not o.endswith("/src/main.c.obj")))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / n) for n in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@"+str(response),
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

#!/usr/bin/env python3
"""Execute production Insight/birth functions and version-selected save lanes.

The fixture compiles current source fragments, never alters real saves/config,
and places a sentinel directly after each historical player lane. Requires the
configured build-standard headers; it does not link stale gameplay objects.
"""
from pathlib import Path
import json
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/insight-birth-save"


def function(source, name):
    start = re.search(r"^[^\n]*\b" + name + r"\([^;]*?\)\s*\{", source, re.M).start()
    return source[start:source.index("\n}", start) + 2] + "\n"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    read = lambda name: (ROOT / name).read_text(encoding="utf-8-sig")
    save = read("src/fs/save-player.c")
    load = read("src/fs/load-player.c")
    setup = read("src/birth/birth-setup.c")
    experience = read("src/player/experience.c")
    traits = read("src/birth/birth-traits.c")
    allocation = read("src/birth/birth-allocation.c")
    blitz = read("src/birth/birth-blitz.c")
    parser = read("src/init/init-parse-player.c")
    save = save[save.index("    // Compatibility block:"):save.index("    /* Reserved: legacy item-quality")]
    load = load[load.index("    // Compatibility block:"):load.index("    /* Reserved: legacy item-quality")]
    parser = parser[parser.rindex("    /* Process 'C' for character ability entries */"):]
    parser = parser[parser.index("    {\n"):parser.index("    /* Process 'P'")]

    # Explicit proposal ledger, independent from the character C: records.
    origins = [
        [21], [125,122], [48,109], [147,151], [108], [64], [25], [65,153],
        [86,87], [152,153,151], [49,13], [7], [109,156], [82,84], [11],
        [154,100], [80], [27], [123,120], [124], [62], [9],
        [144,60,61,62,63,64,65], [8,61], [10], [147,104], [46,41],
        [11,5,2], [102,11,6], [101], [103,105,149], [87], [4], [42],
        [148], [156,146], [88,3], [12,128], [3,4], [24], [159], [103],
        [5], [45], [87], [102], [122], [13], [10], [63], [11,122,124,125],
    ]
    identities = {}
    for entry in re.split(r"(?m)^N:", read("lib/edit/ability.txt"))[1:]:
        serial = int(entry.split(":", 1)[0])
        info = re.search(r"(?m)^I:(\d+):(\d+)", entry)
        if info:
            identities[serial] = tuple(map(int, info.groups()))
    records = {}
    for entry in re.split(r"(?m)^N:", read("lib/edit/character.txt"))[1:]:
        serial = int(entry.split(":", 1)[0])
        records[serial] = re.search(r"(?m)^C:.*", entry).group(0)
    assert len(origins) == len(records) == 51
    cases = ""
    for origin, expected in enumerate(origins):
        pairs = [identities[serial] for serial in expected]
        cases += "{ char line[] = " + json.dumps(records[origin]) + ";\n"
        cases += f"assert(parse_grants(line, &profiles[{origin}]) == 0); body.pcharacter={origin}; get_extra();\n"
        cases += "int count=0; for(int s=0;s<S_MAX;s++) for(int a=0;a<ABILITIES_MAX;a++) count+=body.innate_ability[s][a]!=0;\n"
        cases += f"assert(count=={len(pairs)});\n"
        for skill, ability in pairs:
            cases += f"assert(body.innate_ability[{skill}][{ability}] && body.active_ability[{skill}][{ability}]);\n"
        cases += "}\n"

    harness = r'''
#include "angband.h"
#include "birth/birth-internal.h"
#include "log/log.h"
#include "init.h"
#include <assert.h>
#include <ctype.h>
static player_type body;
static player_other options;
static maxima limits;
static character_profile profiles[51];
static player_race race;
static ego_item_type ego;
static ability_type abilities[4];
player_type* p_ptr=&body;
player_other* op_ptr=&options;
maxima* z_info=&limits;
character_profile* c_info=profiles;
const player_race* rp_ptr=&race;
player_race* p_info=&race;
ego_item_type* e_info=&ego;
char* p_name="fixture race";
char* c_name="fixture hero";
character_profile* current_character_profile=&profiles[0];
ability_type* b_info=abilities;
char* b_name=NULL;
static bool spectator;
static byte stream[1024];
static int pos, size, warnings;
static byte version[4];
void log_log(int level,const char* file,int line,const char* fmt,...) { warnings++; }
errr Term_clear(void) { return 0; }
bool quest_challenge_active(int challenge) { return false; }
bool death_spectator_active(void) { return spectator; }
int ability_index(int skill,int ability) { return 1; }
cptr ability_display_name(const ability_type* a) { return "policy name"; }
static u32b random_state;
u32b Rand_div(u32b m) { random_state=random_state*1664525U+1013904223U;return random_state%m; }
void wr_byte(byte v) { assert(pos<sizeof(stream)); stream[pos++]=v; }
void wr_u16b(u16b v) { wr_byte(v&255); wr_byte(v>>8); }
void wr_s16b(s16b v) { wr_u16b(v); }
void wr_u32b(u32b v) { wr_u16b(v&65535); wr_u16b(v>>16); }
void wr_s32b(s32b v) { wr_u32b(v); }
void rd_byte(byte* v) { assert(pos<size); *v=stream[pos++]; }
void rd_u16b(u16b* v) { byte lo,hi; rd_byte(&lo); rd_byte(&hi); *v=lo+(hi<<8); }
void rd_s16b(s16b* v) { u16b n; rd_u16b(&n); *v=n; }
void rd_u32b(u32b* v) { u16b lo,hi; rd_u16b(&lo); rd_u16b(&hi); *v=lo+((u32b)hi<<16); }
void rd_s32b(s32b* v) { u32b n; rd_u32b(&n); *v=n; }
void strip_bytes(int n) { byte value; while(n--) rd_byte(&value); }
bool savefile_version_at_least(byte a,byte b,byte c,byte d)
{ byte wanted[]={a,b,c,d}; for(int i=0;i<4;i++) if(version[i]!=wanted[i]) return version[i]>wanted[i]; return true; }
int player_active_weapon_mode(void) { return PLAYER_ACTIVE_WEAPON_MELEE; }
static bool savefile_has_morgoth_call_state=true;
'''
    # Use the actual arithmetic and policy implementation, not copies of it.
    for name in ("insight_system_enabled", "insight_reworked_enabled", "insight_stat_increase_cost",
                 "insight_increase_stat", "insight_ability_upgrade_cost", "insight_upgrade_ability"):
        harness += function(experience, name)
    harness += setup[setup.index("const int birth_stat_costs[11]"):]
    # birth_stat_current_cost ends this production file.
    harness += function(setup, "get_start_xp") + function(setup, "get_extra")
    harness += function(setup, "finalize_character_creation_selection")
    harness += function(allocation, "birth_skill_specialty_score") + function(allocation, "birth_recommended_stats")
    harness += function(blitz, "blitz_auto_assign_stats")
    harness += traits[traits.index("static const char *character_ability_names"):traits.index("int collect_character_starting_abilities")]
    harness += function(traits, "collect_character_starting_abilities")
    harness += "static errr parse_grants(char* buf,character_profile* ph_ptr) { header head_body={0}; header* head=&head_body; head->name_ptr=\"fixture\";\n" + parser + "return 0;}\n"
    harness += "static void save_lane(void) { int i;\n" + save + "}\n"
    harness += "static void load_lane(void) { int i;\n" + load + "}\n"
    harness += r'''
static void policy(void)
{
    limits.b_max=4;
    /* An option changed on the hero screen must override the initial wipe's
     * rules, in both directions, before allocation and origin grants. */
    profiles[0].a_adj[0][0]=-1;
    for(int mode=0;mode<2;mode++) {
        options.opt[OPT_insight_beta]=mode;
        body.insight_ruleset=mode?INSIGHT_RULESET_CLASSIC:INSIGHT_RULESET_REWORKED;
        finalize_character_creation_selection();
        assert(body.insight_ruleset==(mode?INSIGHT_RULESET_REWORKED:INSIGHT_RULESET_CLASSIC));
        assert(insight_system_enabled()==mode);
    }
    for(int mode=0;mode<2;mode++) {
        options.opt[OPT_insight_beta]=mode;
        body.insight_ruleset=INSIGHT_RULESET_UNSET;
        assert(insight_system_enabled()==mode && !insight_reworked_enabled());
        body.insight_ruleset=INSIGHT_RULESET_CLASSIC;
        assert(!insight_system_enabled());
        body.insight_ruleset=INSIGHT_RULESET_LEGACY;
        assert(insight_system_enabled() && !insight_reworked_enabled());
        body.insight_ruleset=INSIGHT_RULESET_REWORKED;
        assert(insight_system_enabled() && insight_reworked_enabled());
    }
    for(int rank=0;rank<=6;rank++) {
        body.insight_stat_invested[0]=rank; body.stat_base[0]=rank+3;
        body.insight_ruleset=INSIGHT_RULESET_LEGACY;
        assert(insight_stat_increase_cost(0)==birth_stat_increase_cost(rank));
        body.insight_ruleset=INSIGHT_RULESET_REWORKED;
        int cost=birth_stat_increase_cost(rank);
        assert(insight_stat_increase_cost(0)==(cost?MAX(2,cost):0));
    }
    body.stat_base[0]=BASE_STAT_MAX; assert(!insight_stat_increase_cost(0));
    body.stat_base[0]=3;body.insight_stat_invested[0]=0;body.insight_points=10;
    spectator=true;assert(!insight_increase_stat(0));spectator=false;
    assert(insight_increase_stat(0)&&body.insight_points==8&&body.insight_stat_invested[0]==1&&body.stat_base[0]==4);
    abilities[1].name=1;abilities[1].skilltype=S_SMT;abilities[1].abilitynum=3;abilities[1].insight_upgrade_cost=2;
    body.innate_ability[S_SMT][3]=true;
    assert(!insight_ability_upgrade_cost(S_SMT,3)&&!insight_upgrade_ability(S_SMT,3));
    body.insight_ruleset=INSIGHT_RULESET_LEGACY;
    assert(insight_ability_upgrade_cost(S_SMT,3)==2&&insight_upgrade_ability(S_SMT,3));
    assert(body.insight_points==6&&!insight_upgrade_ability(S_SMT,3));
    assert(!insight_stat_increase_cost(-1)&&!insight_stat_increase_cost(A_MAX));
    assert(!insight_ability_upgrade_cost(-1,0)&&!insight_ability_upgrade_cost(0,-1));
    birth_fixed_exp=false;assert(get_start_xp()==5000);
    birth_fixed_exp=true;assert(get_start_xp()==PY_FIXED_EXP);birth_fixed_exp=false;
}
static void saves(void)
{
    byte expected[1024],fixture[1024];
    body.insight_ruleset=INSIGHT_RULESET_REWORKED;body.light_dimmed=1;
    body.insight_points=17;body.insight_stat_invested[0]=4;
    body.insight_monster_types=RF3_ORC;body.insight_milestones=3;
    body.insight_ability_upgraded[S_SMT][3]=true;
    pos=0;save_lane();int current=pos;memcpy(expected,stream,current);
    const byte versions[][4]={{0,9,8,26},{0,9,8,27},{0,9,8,28},{0,9,8,29},{0,9,8,30},{0,9,8,31},{0,9,9,0},{0,9,9,1}};
    for(int v=0;v<8;v++) for(int option=0;option<2;option++) {
        memcpy(version,versions[v],4);int n=0;
        memcpy(fixture,expected,15);n=15;
        if(savefile_version_at_least(0,9,8,30)){memcpy(fixture+n,expected+15,4);n+=4;}
        if(savefile_version_at_least(0,9,8,27)){memcpy(fixture+n,expected+19,A_MAX);n+=A_MAX;}
        if(savefile_version_at_least(0,9,8,29)){memcpy(fixture+n,expected+19+A_MAX,S_MAX*ABILITIES_MAX);n+=S_MAX*ABILITIES_MAX;}
        if(savefile_version_at_least(0,9,9,1)){memcpy(fixture+n,expected+current-2,2);n+=2;}
        fixture[n++]=0xA7;memcpy(stream,fixture,n);size=n;pos=0;
        memset(&body,255,sizeof(body));options.opt[OPT_insight_beta]=option;load_lane();
        byte sentinel;rd_byte(&sentinel);assert(sentinel==0xA7&&pos==size);
        assert(body.insight_ruleset==(v==7?INSIGHT_RULESET_REWORKED:option?INSIGHT_RULESET_LEGACY:INSIGHT_RULESET_CLASSIC));
        assert(body.light_dimmed==(v==7));
        assert(body.insight_points==(v?17:0));
        assert(body.insight_stat_invested[0]==(v?4:0));
        assert(body.insight_ability_upgraded[S_SMT][3]==(v>=3));
        assert(body.insight_monster_types==(v>=4?RF3_ORC:0));
    }
    for(int bad=0;bad<2;bad++) {
        memcpy(version,versions[7],4);memcpy(stream,expected,current);stream[current-2]=bad?255:0;
        size=current;pos=0;options.opt[OPT_insight_beta]=bad;load_lane();
        assert(body.insight_ruleset==(bad?INSIGHT_RULESET_LEGACY:INSIGHT_RULESET_CLASSIC)&&pos==size);
    }
    for(int rules=1;rules<=3;rules++) {
        body.insight_ruleset=rules;body.light_dimmed=0;pos=0;save_lane();size=pos;pos=0;
        options.opt[OPT_insight_beta]=rules==1;load_lane();
        assert(body.insight_ruleset==rules&&!body.light_dimmed&&pos==size);
    }
}
static void origins(void) {
    memset(&body,0,sizeof(body));
    limits.c_max=51;
'''
    harness += cases
    harness += r'''
    profiles[22].flags_u=UNQ_MIM;
    body.insight_ruleset=INSIGHT_RULESET_REWORKED;
    assert(collect_character_starting_abilities(22,NULL,0,NULL,NULL)==7);
    assert(collect_character_starting_abilities(0,NULL,0,NULL,NULL)==1);
    assert(!collect_character_starting_abilities(-1,NULL,0,NULL,NULL));
    assert(!collect_character_starting_abilities(51,NULL,0,NULL,NULL));
    body.insight_ruleset=INSIGHT_RULESET_LEGACY;
    assert(!collect_character_starting_abilities(22,NULL,0,NULL,NULL));
    int stats[BIRTH_STAT_MAX];
    body.insight_ruleset=INSIGHT_RULESET_REWORKED;
    for(int seed=0;seed<1000;seed++) {
        random_state=seed;blitz_auto_assign_stats(stats);int cost=0;
        for(int s=0;s<A_MAX;s++) cost+=birth_stat_current_cost(stats[s]);
        assert(cost==13);
    }
    char comments[]="C:5:1 # ignored 1:-1:4:1:";
    assert(!parse_grants(comments,&profiles[29])&&profiles[29].a_adj[1][0]==-1);
    const char* invalid[]={"C:-1:0","C:9:0","C:0:-1","C:0:20","C:0:0:","C:x:1","C:1:x","C:999999999999999999999999:1"};
    for(int i=0;i<8;i++) {char line[100];snprintf(line,sizeof(line),"%s",invalid[i]);assert(parse_grants(line,&profiles[0])==PARSE_ERROR_GENERIC);}
    profiles[0].a_adj[0][0]=0;profiles[0].a_adj[0][1]=-1;
    profiles[0].a_adj[1][0]=S_MAX;profiles[0].a_adj[1][1]=0;
    profiles[0].a_adj[2][0]=-1;body.pcharacter=0;get_extra();
    for(int s=0;s<S_MAX;s++)for(int a=0;a<ABILITIES_MAX;a++)assert(!body.innate_ability[s][a]);
}
int main(void) { policy();saves();origins();puts("Insight birth/save: fixed rulesets, stat/upgrade fees, all 51 origin packages, bounds, 0.9.8.26-31/0.9.9.0-1 sentinel consumption PASS");return 0;}
'''
    source = OUT / "check.c"
    source.write_text(harness, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-O0", "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "-o", str(exe)], cwd=ROOT / "build-standard", env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

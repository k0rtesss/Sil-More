#!/usr/bin/env python3
"""Execute production disease rules in an isolated C fixture using real headers.

Requires the configured Windows build's include response file and MinGW compiler.
No game save, config, executable, or player data is opened. Unrelated UI, healing,
loot generation, and status effects are stubbed. Save tests execute the contiguous
production stat/disease/skill record section, not the complete save pipeline.
"""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/disease-check"


def function(path, name):
    source = (ROOT / path).read_text(encoding="utf-8-sig")
    match = re.search(r"^(?:static\s+)?(?:void|bool|int|errr)\s+" + re.escape(name)
                      + r"\s*\([^;{]*\)\s*\{", source, re.M)
    assert match, f"Missing function: {name}"
    return source[match.start():source.index("\n}", match.end()) + 2]


def caller_source_guards():
    """Check integration placement explicitly, without claiming loop execution."""
    def compact(source):
        source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
        return re.sub(r"\s+", " ", source)

    loop = compact(function("src/dungeon/dungeon-player.c", "process_player"))
    capture = loop.index("bool disease_was_active = p_ptr->diseased != 0;")
    command_loop = loop.index("p_ptr->energy_use = 0;")
    completed = loop.index("while (!p_ptr->energy_use && !p_ptr->leaving);")
    leaving = loop.index("if (p_ptr->leaving) return;", completed)
    tick = loop.index("if (disease_was_active) process_disease();")
    assert capture < command_loop < completed < leaving < tick < loop.index("playerturn++;")
    assert loop.count("process_disease();") == 1

    move = compact(function("src/cmd/movement/cmd-run.c", "move_player"))
    takeoff = "player_water_movement(cave_feat[py][px], FEAT_FLOOR);"
    assert takeoff in move
    start = move.index(takeoff)
    end = move.index("p_ptr->leaping = true; return;", start)
    assert "player_water_movement" not in move[start + len(takeoff):end]
    ground = "if (p_ptr->py == y && p_ptr->px == x) player_water_movement(cave_feat[py][px], cave_feat[y][x]);"
    assert move.index(ground) > end
    land = compact(function("src/dungeon/dungeon-player.c", "land"))
    assert "player_water_movement(FEAT_FLOOR, cave_feat[p_ptr->py][p_ptr->px]);" in land
    continued = compact(function("src/dungeon/dungeon-player.c", "continue_leap"))
    assert "player_water_movement" not in continued and continued.count("land();") == 1
    return ("Source guards only: pre-action infection snapshot, completed-action loop and leaving guard before one disease tick; "
            "leap takeoff skips midpoint, land checks destination once: PASS\n")


PRELUDE = r'''
#include "angband.h"
#include <assert.h>
#include <stdio.h>
static player_type player;
player_type *p_ptr=&player;
static object_type objects[2];
object_type *o_list=objects;
byte object_generation_mode;
int stealth_score;
static int random_value, random_calls, expected_bound, disturbances;
u32b Rand_div(u32b bound) {
    ++random_calls;
    if(bound==100) return 99; /* A skeleton can infect without yielding loot. */
    assert(bound==(u32b)expected_bound);
    assert(random_value>=0 && random_value<(int)bound);
    return random_value;
}
void msg_print(cptr s) {(void)s;}
void msg_format(cptr s,...) {(void)s;}
void disturb(int a,int b) {(void)a;(void)b;++disturbances;}
void handle_stuff(void) {}
void update_flow(int y,int x,int which) {(void)y;(void)x;(void)which;}
void monster_perception(bool a,bool b,int n) {(void)a;(void)b;(void)n;}
bool easter_time(void) {return false;}
int damroll(int n,int sides) {(void)sides;return n;}
int consumable_healing_points(const object_type *o) {(void)o;return 10;}
bool hp_player(int n,bool percent,bool message) {(void)n;(void)percent;(void)message;return true;}
bool do_dec_stat(int stat,monster_type *m) {(void)stat;(void)m;return false;}
void update_combat_rolls1b(const monster_type *a,const monster_type *b,bool c) {(void)a;(void)b;(void)c;}
void pois_dam_pure(int a,int b,bool c) {(void)a;(void)b;(void)c;}
#define ALLOW(name) bool allow_player_##name(monster_type *m) {(void)m;return true;}
ALLOW(fear) ALLOW(image) ALLOW(entrancement) ALLOW(stun) ALLOW(slow) ALLOW(blind) ALLOW(confusion)
#define SET(name) bool set_##name(int n) {p_ptr->name=n;return true;}
SET(afraid) SET(rage) SET(food) SET(fast) SET(cut) SET(blind) SET(image)
SET(entranced) SET(stun) SET(confused) SET(poisoned) SET(tmp_per)
SET(tim_invis) SET(oppose_fire) SET(oppose_cold) SET(tmp_str) SET(tmp_dex)
SET(tmp_con) SET(tmp_gra) SET(slow)
static cptr desc_stat_neg[]={"weak","awkward","sickly","drained"};
static void skeleton_note_maybe_show(byte s,int y,int x) {(void)s;(void)y;(void)x;}
static void prep_skeleton_food(object_type *o,byte s) {(void)o;(void)s;assert(false);}
static bool prep_skeleton_light(object_type *o) {(void)o;assert(false);return false;}
static bool generate_skeleton_damaged_item(object_type *o,byte s,bool *b) {(void)o;(void)s;(void)b;assert(false);return false;}
void object_desc(char *s,size_t n,const object_type *o,int p,int m) {(void)s;(void)n;(void)o;(void)p;(void)m;assert(false);}
s16b inven_carry(object_type *o,bool b) {(void)o;(void)b;assert(false);return -1;}
char supplies_label_char(void) {assert(false);return 'a';}
char player_inventory_label(int n) {(void)n;assert(false);return 'a';}
s16b drop_near(object_type *o,int c,int y,int x) {(void)o;(void)c;(void)y;(void)x;assert(false);return 0;}
void break_truce(bool b) {(void)b;assert(false);}
static void reset(void) {
    memset(p_ptr,0,sizeof(*p_ptr)); memset(objects,0,sizeof(objects));
    random_calls=disturbances=0; expected_bound=A_MAX; random_value=0;
    p_ptr->sustain_str=p_ptr->sustain_dex=p_ptr->sustain_con=p_ptr->sustain_gra=1;
}
static byte bytes[1024];
static int write_pos,read_pos,extra_version;
static void wr_s16b(s16b n) {assert(write_pos+2<=1024);bytes[write_pos++]=(byte)n;bytes[write_pos++]=(byte)((u16b)n>>8);}
static void rd_s16b(s16b *n) {assert(read_pos+2<=write_pos);*n=(s16b)(bytes[read_pos]|((u16b)bytes[read_pos+1]<<8));read_pos+=2;}
static bool savefile_version_at_least(byte a,byte b,byte c,byte d) {assert(a==0&&b==9&&c==8);return extra_version>=d;}
static void note(cptr s) {(void)s;}
'''

TESTS = r'''
static void progression(void) {
    reset(); process_disease(); assert(!random_calls&&!disturbances);
    p_ptr->stat_base[A_CON]=5;p_ptr->stat_drain[A_CON]=-2;
    assert(infect_disease()); assert(p_ptr->diseased==50&&p_ptr->stat_disease[A_CON]==-1);
    assert(p_ptr->stat_drain[A_CON]==-2); calc_stats();assert(p_ptr->stat_use[A_CON]==2);
    assert(p_ptr->update&PU_BONUS);assert(p_ptr->redraw&PR_STATS);assert(p_ptr->redraw&PR_EXTRA);
    for(int n=0;n<49;n++)process_disease();
    assert(p_ptr->diseased==1&&random_calls==0&&p_ptr->stat_disease[A_STR]==0);
    assert(!infect_disease()&&p_ptr->diseased==1);
    process_disease(); assert(p_ptr->diseased==50&&p_ptr->stat_disease[A_STR]==-1&&random_calls==1);
    random_value=A_GRA;for(int n=0;n<50;n++)process_disease();
    assert(p_ptr->diseased==50&&p_ptr->stat_disease[A_GRA]==-1&&random_calls==2);
    assert(res_stat(A_CON,20));assert(p_ptr->stat_drain[A_CON]==0&&p_ptr->stat_disease[A_CON]==-1);
    assert(!res_stat(A_CON,20));p_ptr->stat_drain[A_DEX]=-4;
    for(int s=0;s<A_MAX;s++) {expected_bound=A_MAX;random_value=s;p_ptr->diseased=1;process_disease();assert(p_ptr->stat_disease[s]<0);}
    p_ptr->stat_base[A_CON]=20;p_ptr->stat_equip_mod[A_CON]=100;calc_stats();assert(p_ptr->stat_use[A_CON]==BASE_STAT_MAX);
    p_ptr->stat_disease[A_CON]=-32767;p_ptr->diseased=1;random_value=A_CON;process_disease();
    assert(p_ptr->stat_disease[A_CON]==-32767);calc_stats();assert(p_ptr->stat_use[A_CON]==BASE_STAT_MIN);
    assert(cure_disease());assert(!p_ptr->diseased&&p_ptr->stat_drain[A_DEX]==-4);
    for(int s=0;s<A_MAX;s++)assert(!p_ptr->stat_disease[s]);
    assert(!cure_disease());int calls=random_calls;process_disease();assert(calls==random_calls);
    assert(infect_disease()&&p_ptr->stat_disease[A_CON]==-1&&p_ptr->diseased==50);
    puts("Disease: immediate CON despite sustain; 49/50/100 ticks; repeated exposure; all four stats; restore/cure isolation; bounds and saturation: PASS");
}
static void consumables(void) {
    object_type item={0};bool ident;
    reset();item.sval=SV_FOOD_SICKNESS;ident=false;
    assert(eat_food(&item,&ident)&&ident&&p_ptr->diseased==50&&!random_calls);
    for(int roll=0;roll<5;roll++) {
        reset();expected_bound=5;random_value=roll;item.sval=SV_FOOD_MEAT;ident=false;
        assert(eat_food(&item,&ident)&&ident);assert(random_calls==1);assert((p_ptr->diseased>0)==(roll==0));
    }
    for(int sval=0;sval<256;sval++) {
        reset();infect_disease();p_ptr->diseased=17;for(int s=0;s<A_MAX;s++)p_ptr->stat_drain[s]=-3;
        item.sval=sval;ident=false;eat_food(&item,&ident);
        assert(p_ptr->diseased==17&&p_ptr->stat_disease[A_CON]==-1);
        if(sval==SV_FOOD_RESTORATION)for(int s=0;s<A_MAX;s++)assert(p_ptr->stat_drain[s]==0);
        reset();infect_disease();p_ptr->diseased=17;p_ptr->stat_drain[A_CON]=-2;
        item.sval=sval;ident=false;quaff_potion(&item,&ident);
        bool cures=sval==SV_POTION_HEALING||sval==SV_POTION_MIRUVOR;
        assert(p_ptr->diseased==(cures?0:17));assert(p_ptr->stat_disease[A_CON]==(cures?0:-1));
        assert(p_ptr->stat_drain[A_CON]==-2);
    }
    puts("Consumables: sickness certain, meat exactly 1/5; every food/potion subtype; only Healing/Miruvor cure; restoration stays independent: PASS");
}
static void exposure(void) {
    for(int mode=0;mode<2;mode++)for(int roll=0;roll<200;roll++) {
        reset();expected_bound=200;random_value=roll;p_ptr->energy_use=100;
        if(mode)player_water_displaced(FEAT_FLOOR,FEAT_WATER);else player_water_movement(FEAT_FLOOR,FEAT_WATER);
        assert(random_calls==1);assert((p_ptr->diseased>0)==(roll==0));
        reset();expected_bound=200;random_value=roll;
        if(mode)player_water_displaced(FEAT_WATER,FEAT_FLOOR);else player_water_movement(FEAT_WATER,FEAT_FLOOR);
        assert(!random_calls&&!p_ptr->diseased);
    }
    reset();expected_bound=200;random_value=0;
    player_water_movement(FEAT_FLOOR,FEAT_FLOOR);player_water_displaced(FEAT_FLOOR,FEAT_FLOOR);
    assert(!random_calls&&!p_ptr->diseased);
    player_water_movement(FEAT_WATER,FEAT_WATER);assert(random_calls==1&&p_ptr->diseased==50);
    reset();infect_disease();p_ptr->diseased=7;player_water_movement(FEAT_WATER,FEAT_WATER);player_water_displaced(FEAT_FLOOR,FEAT_WATER);
    assert(!random_calls&&p_ptr->diseased==7);
    for(int roll=0;roll<20;roll++) {
        reset();expected_bound=20;random_value=roll;objects[1].sval=SV_SKELETON_ORC;objects[1].pval=1;
        do_cmd_search_skeleton(1,1,1);assert((p_ptr->diseased>0)==(roll==0));assert(random_calls==2&&!objects[1].pval);
        cure_disease();int calls=random_calls;do_cmd_search_skeleton(1,1,1);assert(random_calls==calls&&!p_ptr->diseased);
    }
    reset();objects[1].sval=SV_SKELETON_ELF;objects[1].pval=1;do_cmd_search_skeleton(1,1,1);assert(!p_ptr->diseased&&random_calls==1);
    puts("Exposure: movement/displacement exactly 1/200, bank exit excluded, repeats preserve clock; first orc search exactly 1/20 even without loot, searched/elf excluded: PASS");
}
static void save_records(void) {
    for(int version=2;version<=3;version++)for(int countdown=0;countdown<=50;countdown++) {
        reset();p_ptr->diseased=countdown;
        for(int s=0;s<A_MAX;s++){p_ptr->stat_base[s]=s+2;p_ptr->stat_drain[s]=-s;p_ptr->stat_disease[s]=countdown?-s-1:0;}
        for(int s=0;s<S_MAX;s++)p_ptr->skill_base[s]=101+s;
        write_pos=read_pos=0;extra_version=version;write_stats();
        if(version==2){memmove(bytes+4*A_MAX,bytes+4*A_MAX+2*(A_MAX+1),write_pos-4*A_MAX-2*(A_MAX+1));write_pos-=2*(A_MAX+1);}
        memset(p_ptr,0x55,sizeof(*p_ptr));assert(read_stats()==0);assert(read_pos==write_pos);
        assert(p_ptr->diseased==(version==2?0:countdown));
        for(int s=0;s<A_MAX;s++){assert(p_ptr->stat_base[s]==s+2&&p_ptr->stat_drain[s]==-s);assert(p_ptr->stat_disease[s]==(version==2||!countdown?0:-s-1));}
        for(int s=0;s<S_MAX;s++)assert(p_ptr->skill_base[s]==101+s);
    }
    for(int bad=0;bad<4;bad++) {
        reset();extra_version=3;p_ptr->diseased=bad==0?-1:bad==1?51:bad==2?1:0;
        p_ptr->stat_disease[A_CON]=bad==2?1:bad==3?-1:0;
        write_pos=read_pos=0;write_stats();assert(read_stats()==-1);
    }
    puts("Stat save record: all 51 countdowns roundtrip, 0.9.8.2 defaults and following skill alignment, four corrupt states rejected: PASS");
}
int main(void) {progression();consumables();exposure();save_records();return 0;}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    caller_result = caller_source_guards()
    implementation = "\n".join(function(path, name) for path, names in [
        ("src/player/effects.c", ["disease_changed", "infect_disease", "cure_disease", "process_disease"]),
        ("src/player/player-skills.c", ["calc_stats"]),
        ("src/spell/spell-damage.c", ["res_stat"]),
        ("src/spell/spell-utility.c", ["do_res_stat"]),
        ("src/use-obj.c", ["eat_food", "quaff_potion"]),
        ("src/cave/cave-water.c", ["water_movement_energy", "player_water_movement", "player_water_displaced"]),
        ("src/cmd/world/cmd-interact-chest.c", ["do_cmd_search_skeleton"]),
    ] for name in names)
    writer = (ROOT / "src/fs/save-player.c").read_text()
    reader = (ROOT / "src/fs/load-player.c").read_text()
    writer = writer[writer.index("    /* Dump the stats"):writer.index("    for (i = 0; i < S_MAX; ++i)\n    {")]
    reader = reader[reader.index("    /* Read the stat info"):reader.index("    /* Read the abilities info")]
    save = "static void write_stats(void) {int i;\n" + writer + "\n}\n"
    save += "static int read_stats(void) {int i;\n" + reader + "\nreturn 0;\n}\n"
    source = OUT / "check.c"
    source.write_text(PRELUDE + implementation + save + TESTS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O2",
                    "-Wall", "-Wextra", "-Werror", "@CMakeFiles/sil-more.dir/includes_C.rsp",
                    str(source), "-o", str(exe)], cwd=BUILD, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30,
                            capture_output=True, text=True)
    (OUT / "result.txt").write_text(result.stdout + caller_result, encoding="utf-8")
    print(result.stdout + caller_result, end="")


if __name__ == "__main__":
    main()

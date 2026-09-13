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
    match = re.search(r"^(?:static\s+)?(?:void|bool|int|errr|cptr)\s+" + re.escape(name)
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
    assert move.index("p_ptr->leaping = true;") < start
    end = move.index("return;", start)
    assert "player_water_movement" not in move[start + len(takeoff):end]
    ground = "if ((py != y || px != x) && p_ptr->py == y && p_ptr->px == x) player_water_movement(cave_feat[py][px], cave_feat[y][x]);"
    assert move.index(ground) > end
    land = compact(function("src/dungeon/dungeon-player.c", "land"))
    assert "player_water_movement(FEAT_FLOOR, cave_feat[p_ptr->py][p_ptr->px]);" in land
    continued = compact(function("src/dungeon/dungeon-player.c", "continue_leap"))
    assert "player_water_movement" not in continued and continued.count("land();") == 1
    knowledge = compact(function("src/spell/spell-utility.c", "self_knowledge"))
    assert knowledge.count("disease_identify();") == 1
    assert "disease_name()" in knowledge and "disease_cure_name()" in knowledge
    assert "DISEASE_KNOWN_NAME" in knowledge and "DISEASE_KNOWN_CURE" in knowledge
    gem = compact(function("src/use-obj.c", "use_staff_effects"))
    assert "case SV_STAFF_SELF_KNOWLEDGE:" in gem and "self_knowledge();" in gem
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
static int identity_calls, name_roll, herb_roll, diagnosis_calls, diagnosis_rolls[2];
static int healing_calls, stat_loss_calls;
u32b Rand_div(u32b bound) {
    if(bound==DISEASE_NAME_COUNT) {++identity_calls;return name_roll;}
    if(bound==DISEASE_HERB_COUNT) {++identity_calls;return herb_roll;}
    if(bound==2) {assert(diagnosis_calls<2);return diagnosis_rolls[diagnosis_calls++];}
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
bool hp_player(int n,bool percent,bool message) {(void)n;(void)percent;(void)message;++healing_calls;return true;}
bool do_dec_stat(int stat,monster_type *m) {(void)stat;(void)m;++stat_loss_calls;return false;}
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
    identity_calls=name_roll=herb_roll=diagnosis_calls=healing_calls=stat_loss_calls=0;
    diagnosis_rolls[0]=diagnosis_rolls[1]=0;
    p_ptr->sustain_str=p_ptr->sustain_dex=p_ptr->sustain_con=p_ptr->sustain_gra=1;
}
static byte bytes[1024];
static int write_pos,read_pos,extra_version;
static void wr_byte(byte n) {assert(write_pos<1024);bytes[write_pos++]=n;}
static void rd_byte(byte *n) {assert(read_pos<write_pos);*n=bytes[read_pos++];}
static void wr_s16b(s16b n) {assert(write_pos+2<=1024);bytes[write_pos++]=(byte)n;bytes[write_pos++]=(byte)((u16b)n>>8);}
static void rd_s16b(s16b *n) {assert(read_pos+2<=write_pos);*n=(s16b)(bytes[read_pos]|((u16b)bytes[read_pos+1]<<8));read_pos+=2;}
static bool savefile_version_at_least(byte a,byte b,byte c,byte d) {assert(a==0&&b==9&&c==8);return extra_version>=d;}
static void note(cptr s) {(void)s;}
'''

TESTS = r'''
static void identity_and_diagnosis(void) {
    reset();disease_identify();assert(!diagnosis_calls&&!identity_calls);
    for(int name=0;name<DISEASE_NAME_COUNT;name++)for(int herb=0;herb<DISEASE_HERB_COUNT;herb++) {
        reset();name_roll=name;herb_roll=herb;assert(infect_disease());
        assert(identity_calls==2&&p_ptr->disease_name==name+1&&p_ptr->disease_cure==herb);
        assert(!p_ptr->disease_knowledge);
        assert(disease_name()&&*disease_name()&&disease_cure_name()&&*disease_cure_name());
        assert(!infect_disease()&&identity_calls==2);
        assert(p_ptr->disease_name==name+1&&p_ptr->disease_cure==herb);
        assert(cure_disease());assert(!p_ptr->disease_name&&!p_ptr->disease_cure&&!p_ptr->disease_knowledge);
    }
    for(int name=0;name<2;name++)for(int cure=0;cure<2;cure++) {
        reset();infect_disease();diagnosis_rolls[0]=name;diagnosis_rolls[1]=cure;
        disease_identify();
        assert(diagnosis_calls==(name?1:2));
        assert(p_ptr->disease_knowledge==(name?0:DISEASE_KNOWN_NAME|(cure?0:DISEASE_KNOWN_CURE)));
    }
    reset();infect_disease();diagnosis_rolls[0]=0;diagnosis_rolls[1]=1;
    disease_identify();assert(p_ptr->disease_knowledge==DISEASE_KNOWN_NAME);
    diagnosis_calls=0;diagnosis_rolls[0]=1;disease_identify();
    assert(diagnosis_calls==1&&p_ptr->disease_knowledge==DISEASE_KNOWN_NAME);
    diagnosis_calls=0;diagnosis_rolls[0]=0;disease_identify();
    assert(diagnosis_calls==1&&p_ptr->disease_knowledge==(DISEASE_KNOWN_NAME|DISEASE_KNOWN_CURE));
    diagnosis_calls=0;disease_identify();assert(!diagnosis_calls);
    for(int known=0;known<=3;known++) {
        if(known==DISEASE_KNOWN_CURE)continue;
        reset();infect_disease();p_ptr->disease_knowledge=known;
        p_ptr->active_ability[S_PER][PER_ALCHEMY]=true;disease_identify();
        assert(!diagnosis_calls&&p_ptr->disease_knowledge==(DISEASE_KNOWN_NAME|DISEASE_KNOWN_CURE));
    }
    puts("Identity/diagnosis: every disease/herb assignment; unchanged on exposure; cure reset; all 50% branches; retained partial knowledge; Alchemist certainty: PASS");
}
static void progression(void) {
    reset(); process_disease(); assert(!random_calls&&!disturbances);
    p_ptr->stat_base[A_CON]=5;p_ptr->stat_drain[A_CON]=-2;
    assert(infect_disease()); assert(p_ptr->diseased==100&&p_ptr->stat_disease[A_CON]==-1);
    assert(p_ptr->stat_drain[A_CON]==-2); calc_stats();assert(p_ptr->stat_use[A_CON]==2);
    assert(p_ptr->update&PU_BONUS);assert(p_ptr->redraw&PR_STATS);assert(p_ptr->redraw&PR_EXTRA);
    for(int n=0;n<99;n++)process_disease();
    assert(p_ptr->diseased==1&&random_calls==0&&p_ptr->stat_disease[A_STR]==0);
    assert(!infect_disease()&&p_ptr->diseased==1);
    process_disease(); assert(p_ptr->diseased==100&&p_ptr->stat_disease[A_STR]==-1&&random_calls==1);
    random_value=A_GRA;for(int n=0;n<100;n++)process_disease();
    assert(p_ptr->diseased==100&&p_ptr->stat_disease[A_GRA]==-1&&random_calls==2);
    assert(res_stat(A_CON,20));assert(p_ptr->stat_drain[A_CON]==0&&p_ptr->stat_disease[A_CON]==-1);
    assert(!res_stat(A_CON,20));p_ptr->stat_drain[A_DEX]=-4;
    for(int s=0;s<A_MAX;s++) {expected_bound=A_MAX;random_value=s;p_ptr->diseased=1;process_disease();assert(p_ptr->stat_disease[s]<0);}
    p_ptr->stat_base[A_CON]=20;p_ptr->stat_equip_mod[A_CON]=100;calc_stats();assert(p_ptr->stat_use[A_CON]==BASE_STAT_MAX);
    p_ptr->stat_disease[A_CON]=-32767;p_ptr->diseased=1;random_value=A_CON;process_disease();
    assert(p_ptr->stat_disease[A_CON]==-32767);calc_stats();assert(p_ptr->stat_use[A_CON]==BASE_STAT_MIN);
    assert(cure_disease());assert(!p_ptr->diseased&&p_ptr->stat_drain[A_DEX]==-4);
    for(int s=0;s<A_MAX;s++)assert(!p_ptr->stat_disease[s]);
    assert(!cure_disease());int calls=random_calls;process_disease();assert(calls==random_calls);
    assert(infect_disease()&&p_ptr->stat_disease[A_CON]==-1&&p_ptr->diseased==100);
    puts("Disease: immediate CON despite sustain; 99/100/200 ticks; repeated exposure; all four stats; restore/cure isolation; bounds and saturation: PASS");
}
static void consumables(void) {
    object_type item={0};bool ident;
    reset();item.tval=TV_FOOD;item.sval=SV_FOOD_SICKNESS;ident=false;
    assert(eat_food(&item,&ident)&&ident&&p_ptr->diseased==100&&!random_calls);
    for(int roll=0;roll<5;roll++) {
        reset();expected_bound=5;random_value=roll;item.sval=SV_FOOD_MEAT;ident=false;
        assert(eat_food(&item,&ident)&&ident);assert(random_calls==1);assert((p_ptr->diseased>0)==(roll==0));
    }
    for(int sval=0;sval<256;sval++) {
        reset();infect_disease();p_ptr->diseased=17;for(int s=0;s<A_MAX;s++)p_ptr->stat_drain[s]=-3;
        p_ptr->disease_cure=(sval+1)%DISEASE_HERB_COUNT;
        item.tval=TV_FOOD;item.sval=sval;ident=false;eat_food(&item,&ident);
        assert(p_ptr->diseased==17&&p_ptr->stat_disease[A_CON]==-1);
        if(sval==SV_FOOD_RESTORATION)for(int s=0;s<A_MAX;s++)assert(p_ptr->stat_drain[s]==0);
        reset();infect_disease();p_ptr->diseased=17;p_ptr->stat_drain[A_CON]=-2;
        item.tval=TV_POTION;item.sval=sval;ident=false;quaff_potion(&item,&ident);
        bool cures=sval==SV_POTION_HEALING||sval==SV_POTION_MIRUVOR;
        assert(p_ptr->diseased==(cures?0:17));assert(p_ptr->stat_disease[A_CON]==(cures?0:-1));
        assert(p_ptr->stat_drain[A_CON]==-2);
        if(cures)assert(!p_ptr->disease_name&&!p_ptr->disease_cure&&!p_ptr->disease_knowledge);
    }
    for(int herb=0;herb<DISEASE_HERB_COUNT;herb++)for(int known=0;known<2;known++) {
        reset();herb_roll=herb;infect_disease();p_ptr->diseased=17;
        p_ptr->disease_knowledge=known?(DISEASE_KNOWN_NAME|DISEASE_KNOWN_CURE):0;
        p_ptr->food=2000;p_ptr->afraid=17;p_ptr->rage=7;p_ptr->cut=20;p_ptr->blind=13;
        for(int s=0;s<A_MAX;s++)p_ptr->stat_drain[s]=-3;
        player_type before=*p_ptr;assert(cure_disease());player_type expected=*p_ptr;*p_ptr=before;
        item.tval=TV_FOOD;item.sval=herb;item.pval=250;ident=false;
        assert(disease_herb_matches(&item));assert(eat_food(&item,&ident)&&ident);
        assert(memcmp(p_ptr,&expected,sizeof(expected))==0);
        assert(!random_calls&&!healing_calls&&!stat_loss_calls);
        assert(!disease_herb_matches(&item));
    }
    reset();infect_disease();assert(!disease_herb_matches(NULL));
    item.tval=TV_POTION;item.sval=p_ptr->disease_cure;assert(!disease_herb_matches(&item));
    item.tval=TV_FOOD;item.sval=SV_FOOD_BREAD;assert(!disease_herb_matches(&item));
    item.pval=125;p_ptr->food=2000;eat_food(&item,&ident);assert(p_ptr->food==2125&&p_ptr->diseased);
    puts("Consumables: exposure odds and every food/potion subtype; all ten matching herbs suppress normal effects and nourishment even with unknown cure; wrong herbs remain normal; Healing/Miruvor clear identity: PASS");
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
    player_water_movement(FEAT_WATER,FEAT_WATER);assert(random_calls==1&&p_ptr->diseased==100);
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
    for(int version=2;version<=9;version++)for(int countdown=0;countdown<=100;countdown++) {
        reset();p_ptr->diseased=countdown;
        if(countdown) {p_ptr->disease_name=DISEASE_NAME_COUNT;p_ptr->disease_cure=DISEASE_HERB_COUNT-1;p_ptr->disease_knowledge=DISEASE_KNOWN_NAME|DISEASE_KNOWN_CURE;}
        for(int s=0;s<A_MAX;s++){p_ptr->stat_base[s]=s+2;p_ptr->stat_drain[s]=-s;p_ptr->stat_disease[s]=countdown?-s-1:0;}
        for(int s=0;s<S_MAX;s++)p_ptr->skill_base[s]=101+s;
        write_pos=read_pos=0;extra_version=version;write_stats();
        if(version<9){int offset=4*A_MAX+2*(A_MAX+1);memmove(bytes+offset,bytes+offset+3,write_pos-offset-3);write_pos-=3;}
        if(version==2){memmove(bytes+4*A_MAX,bytes+4*A_MAX+2*(A_MAX+1),write_pos-4*A_MAX-2*(A_MAX+1));write_pos-=2*(A_MAX+1);}
        memset(p_ptr,0x55,sizeof(*p_ptr));assert(read_stats()==0);assert(read_pos==write_pos);
        assert(p_ptr->diseased==(version==2?0:countdown));
        if(version==2||!countdown)assert(!p_ptr->disease_name&&!p_ptr->disease_cure&&!p_ptr->disease_knowledge&&!identity_calls);
        else if(version<9)assert(identity_calls==2&&p_ptr->disease_name==1&&!p_ptr->disease_cure&&!p_ptr->disease_knowledge);
        else assert(!identity_calls&&p_ptr->disease_name==DISEASE_NAME_COUNT&&p_ptr->disease_cure==DISEASE_HERB_COUNT-1&&p_ptr->disease_knowledge==(DISEASE_KNOWN_NAME|DISEASE_KNOWN_CURE));
        for(int s=0;s<A_MAX;s++){assert(p_ptr->stat_base[s]==s+2&&p_ptr->stat_drain[s]==-s);assert(p_ptr->stat_disease[s]==(version==2||!countdown?0:-s-1));}
        for(int s=0;s<S_MAX;s++)assert(p_ptr->skill_base[s]==101+s);
    }
    for(int bad=0;bad<4;bad++) {
        reset();extra_version=9;p_ptr->diseased=bad==0?-1:bad==1?101:bad==2?1:0;
        p_ptr->stat_disease[A_CON]=bad==2?1:bad==3?-1:0;
        write_pos=read_pos=0;write_stats();assert(read_stats()==-1);
    }
    for(int name=1;name<=DISEASE_NAME_COUNT;name++)for(int herb=0;herb<DISEASE_HERB_COUNT;herb++)for(int known=0;known<=3;known++) {
        if(known==DISEASE_KNOWN_CURE)continue;
        reset();extra_version=9;p_ptr->diseased=17;p_ptr->disease_name=name;p_ptr->disease_cure=herb;p_ptr->disease_knowledge=known;
        write_pos=read_pos=0;write_stats();memset(p_ptr,0,sizeof(*p_ptr));assert(read_stats()==0);
        assert(p_ptr->disease_name==name&&p_ptr->disease_cure==herb&&p_ptr->disease_knowledge==known&&!identity_calls);
    }
    for(int bad=0;bad<8;bad++) {
        reset();extra_version=9;p_ptr->diseased=17;p_ptr->disease_name=1;
        if(bad==0)p_ptr->disease_name=0;
        if(bad==1)p_ptr->disease_name=DISEASE_NAME_COUNT+1;
        if(bad==2)p_ptr->disease_cure=DISEASE_HERB_COUNT;
        if(bad==3)p_ptr->disease_knowledge=4;
        if(bad==4)p_ptr->disease_knowledge=DISEASE_KNOWN_CURE;
        if(bad>=5){p_ptr->diseased=0;p_ptr->disease_name=bad==5?1:0;p_ptr->disease_cure=bad==6?1:0;p_ptr->disease_knowledge=bad==7?DISEASE_KNOWN_NAME:0;}
        write_pos=read_pos=0;write_stats();assert(read_stats()==-1);
    }
    puts("Stat save record: 101 countdowns for every .2-.9 version; .8 legacy identity migration preserves penalties/clock and skill alignment; all identities/knowledge roundtrip; 12 corrupt states rejected: PASS");
}
int main(void) {identity_and_diagnosis();progression();consumables();exposure();save_records();return 0;}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    caller_result = caller_source_guards()
    implementation = "\n".join(function(path, name) for path, names in [
        ("src/player/effects.c", ["disease_name", "disease_cure_name", "disease_assign_identity", "disease_identify", "disease_herb_matches", "disease_changed", "infect_disease", "cure_disease", "process_disease"]),
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

#!/usr/bin/env python3
"""Execute production monster poison and save-record code in a C fixture.

Uses real game headers and the configured Windows build includes. Damage/death
UI is stubbed; this does not claim gameplay, rendering, or full-save validation.
"""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/monster-poison-check"


def function(path, name):
    source = (ROOT / path).read_text(encoding="utf-8-sig")
    match = re.search(r"^(?:static )?(?:void|int) " + name + r"\([^;{]*\)\s*\{", source, re.M)
    assert match, name
    return source[match.start():source.index("\n}", match.end()) + 2]


PRELUDE = r'''
#include "angband.h"
#include "monster/monster-ai.h"
#include "monster/monster-senses.h"
#include <assert.h>
#include <stdio.h>
static player_type player;
player_type *p_ptr=&player;
static monster_type monsters[3];
monster_type *mon_list=monsters;
static monster_race races[3];
monster_race *r_info=races;
static monster_lore lore[3];
monster_lore *l_list=lore;
s16b mon_max=3;
static int dealt, hits;
bool mon_take_hit(int idx,int damage,cptr note,int who) {
    (void)note;assert(who==0);hits++;dealt+=damage;
    monsters[idx].hp-=damage;
    if(monsters[idx].hp<=0){monsters[idx].r_idx=0;return true;}
    return false;
}
void display_hit(int y,int x,int d,int type,bool dead) {
    (void)y;(void)x;(void)d;(void)dead;assert(type==GF_POIS);
}
void object_flags(const object_type *o,u32b *a,u32b *b,u32b *c) {
    *a=o->xtra1?TR1_BRAND_POIS:0;*b=*c=0;
}
static byte bytes[1024];
static int wp,rp,extra_version;
static int poison_offset, abilities_offset, ai_offset;
static bool savefile_has_song_duels=true,savefile_has_monster_shatter=true;
static bool savefile_has_thrall_quest=true,savefile_has_thrall_quest_requested=true;
static void wr_byte(byte n){assert(wp<1024);bytes[wp++]=n;}
static void rd_byte(byte *n){assert(rp<wp);*n=bytes[rp++];}
static void wr_s16b(s16b n){wr_byte((byte)n);wr_byte((byte)((u16b)n>>8));}
static void rd_s16b(s16b *n){byte a,b;rd_byte(&a);rd_byte(&b);*n=(s16b)(a|((u16b)b<<8));}
static void wr_u32b(u32b n){for(int i=0;i<4;i++)wr_byte((byte)(n>>(8*i)));}
static void wr_s32b(s32b n){wr_u32b((u32b)n);}
static void rd_u32b(u32b *n){*n=0;for(int i=0;i<4;i++){byte b;rd_byte(&b);*n|=(u32b)b<<(8*i);}}
static void rd_s32b(s32b *n){u32b u;rd_u32b(&u);*n=(s32b)u;}
static void strip_bytes(int n){assert(rp+n<=wp);rp+=n;}
static bool savefile_version_at_least(byte a,byte b,byte c,byte d){assert(a==0&&b==9&&c==8);return extra_version>=d;}
static void reset(void){
    memset(monsters,0,sizeof(monsters));memset(races,0,sizeof(races));memset(lore,0,sizeof(lore));
    memset(&player,0,sizeof(player));monsters[1].r_idx=1;monsters[1].hp=1000;
    monsters[1].ml=true;player.health_who=1;dealt=hits=0;
}
'''

TESTS = r'''
int main(void){
    reset();monster_poison_add(1,11);monster_poison_add(1,8);
    assert(monsters[1].poisoned==19&&monsters[1].hp==1000);
    assert(player.redraw&PR_HEALTHBAR);assert(monsters[1].mflag&MFLAG_ACTV);
    int expected[]={4,3,3,2,2,1,1,1,1,1};
    for(int i=0;i<10;i++){int before=dealt;assert(!monster_poison_tick(1));assert(dealt-before==expected[i]);}
    assert(monsters[1].poisoned==0&&dealt==19&&hits==10);
    assert(!monster_poison_tick(1)&&hits==10);
    for(int dose=1;dose<=100;dose++){
        reset();monster_poison_add(1,dose);
        while(monsters[1].poisoned){int old=monsters[1].poisoned;assert(!monster_poison_tick(1));assert(monsters[1].poisoned==old-(old+4)/5);}
        assert(dealt==dose);
    }
    reset();monster_poison_add(1,2147483647);monster_poison_add(1,20);assert(monsters[1].poisoned==100);
    monster_poison_add(1,-3);monster_poison_add(0,20);monster_poison_add(3,20);assert(monsters[1].poisoned==100);
    reset();races[1].flags3=RF3_RES_POIS;monster_poison_add(1,20);assert(!monsters[1].poisoned);assert(lore[1].flags3&RF3_RES_POIS);
    reset();monsters[1].hp=1;monster_poison_add(1,1);assert(monster_poison_tick(1));assert(!monsters[1].r_idx);
    assert(!monster_poison_tick(1));
    puts("Poison: all doses 1-100 conserve pending damage; ceil(/5), stacking, cap, immunity, lethal tick: PASS");
    object_type plain={0},venom={0};venom.xtra1=1;
    for(int damage=0;damage<=99;damage++){
        reset();monster_poison_brand(1,&plain,NULL,damage);assert(!monsters[1].poisoned);
        monster_poison_brand(1,&venom,NULL,damage);assert(monsters[1].poisoned==(damage+1)/2);
        reset();monster_poison_brand(1,&plain,&venom,damage);assert(monsters[1].poisoned==(damage+1)/2);
        reset();monster_poison_brand(1,&venom,&venom,damage);assert(monsters[1].poisoned==(damage+1)/2);
    }
    puts("Brands: positive post-armor damage adds ceil(half), zero blocked hit, bow/ammo either or both apply once: PASS");
    for(int version=2;version<=6;version++)for(int poison=0;poison<=100;poison++){
        monster_type src={0},dst;src.r_idx=2;src.hp=123;src.energy=-50;src.poisoned=poison;src.thrall_quest_requested=1;
        src.song_will_penalty=7;src.blow_ds_reduction[0]=3;
        src.vengeance=1;src.smite_recovery=2;
        src.ai.observations[MON_AI_FIRE].value=2;
        src.ai.observations[MON_AI_FIRE].ttl=40;
        src.ai.observations[MON_AI_FIRE].turn=1234;
        src.ai.sense.kind=MON_SENSE_SHARED_TRACE;
        src.ai.sense.y=7;src.ai.sense.x=9;src.ai.sense.observed_turn=1234;
        src.ai.cast_reserve=2;src.ai.goal_age=3;
        wp=rp=0;extra_version=version;wr_monster(&src);
        if(version<4)wp=poison_offset;
        else if(version<5)wp=abilities_offset;
        else if(version<6)wp=ai_offset;
        wr_byte(0xA5);memset(&dst,0x55,sizeof(dst));rd_monster(&dst);
        assert(dst.poisoned==(version<4?0:poison));assert(dst.r_idx==2&&dst.hp==123&&dst.energy==-50);
        assert(dst.thrall_quest_requested==1&&dst.song_will_penalty==7&&dst.blow_ds_reduction[0]==3);
        assert(dst.vengeance==(version<5?0:1));
        assert(dst.smite_recovery==(version<5?0:2));
        assert(dst.ai.observations[MON_AI_FIRE].value==(version<6?0:2));
        assert(dst.ai.cast_reserve==(version<6?0:2));
        assert(dst.ai.goal_age==(version<6?0:3));
        assert(dst.ai.sense.kind==(version<6?0:MON_SENSE_SHARED_TRACE));
        assert(!dst.ai.cast_checked&&!dst.ai.cast_available);
        byte sentinel;rd_byte(&sentinel);assert(sentinel==0xA5&&rp==wp);
    }
    puts("Complete monster record: versions 0.9.8.2-6, poison/ability/AI defaults and roundtrip; following record alignment: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    save_source = (ROOT / "src/fs/save.c").read_text(encoding="utf-8-sig")
    start = save_source.index("#define SAVE_MON_FLAGS")
    flags = save_source[start:save_source.index("\n\n", start)]
    source = PRELUDE + (ROOT / "src/monster/monster-poison.c").read_text()
    writer = function("src/fs/save.c", "wr_monster")
    writer = writer.replace("wr_s16b(m_ptr->poisoned);", "poison_offset=wp;wr_s16b(m_ptr->poisoned);")
    writer = writer.replace("wr_byte(m_ptr->vengeance);", "abilities_offset=wp;wr_byte(m_ptr->vengeance);")
    writer = writer.replace("/* 0.9.8.6:", "ai_offset=wp;/* 0.9.8.6:")
    source += function("src/monster/monster-ai.c", "observation_lifetime")
    source += "\n" + function("src/monster/monster-ai.c", "monster_ai_sanitize")
    source += "\n" + flags + "\n" + writer
    source += "\n" + function("src/fs/load.c", "rd_monster") + TESTS
    fixture = OUT / "check.c"
    fixture.write_text(source, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O2",
                    "-Wall", "-Wextra", "-Werror", "@CMakeFiles/sil-more.dir/includes_C.rsp",
                    str(fixture), "-o", str(exe)], cwd=ROOT / "build-standard", env=env, check=True)
    result = subprocess.run([str(exe)], env=env, check=True, capture_output=True, text=True, timeout=30)
    (OUT / "result.txt").write_text(result.stdout, encoding="utf-8")
    print(result.stdout, end="")


if __name__ == "__main__":
    main()

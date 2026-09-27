#!/usr/bin/env python3
"""Exercise production poison provenance and verify its integration hooks."""
from pathlib import Path
import os
import re
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'scripts/output/quest-attribution'
HARNESS=r'''
#include "angband.h"
#include "externs.h"
#include "quest/quest-runtime.h"
#include <assert.h>
static player_type player;player_type *p_ptr=&player;
static monster_type monsters[4];monster_type *mon_list=monsters;s16b mon_max=4;
static monster_race races[8];monster_race *r_info=races;
static monster_lore lore[8];monster_lore *l_list=lore;
static int credited_damage,credited_kills,damage_hooks,ambient_hits,hit_displays,last_race;
void quest_followup_damage(monster_type *m,int damage,int who){
 ++damage_hooks;
 if(who<0)credited_damage+=MIN(damage,MAX(0,m->hp));
}
void quest_followup_kill(int race){++credited_kills;last_race=race;}
bool mon_take_hit(int idx,int damage,cptr note,int who){
 (void)note;assert(who==0);++ambient_hits;
 /* Production mon_take_hit forwards damage to quests. The zero source must
  * not credit the player a second time after poison's explicit attribution. */
 quest_followup_damage(&monsters[idx],damage,who);
 monsters[idx].hp-=damage;
 if(monsters[idx].hp<=0){monsters[idx].r_idx=0;return true;}
 return false;
}
void object_flags(const object_type *object,u32b *f1,u32b *f2,u32b *f3){*f1=object->sval?TR1_BRAND_POIS:0;*f2=*f3=0;}
void display_hit(int y,int x,int damage,int type,bool fatal){(void)y;(void)x;(void)damage;(void)type;(void)fatal;++hit_displays;}
static void reset(void){
 memset(&player,0,sizeof(player));memset(monsters,0,sizeof(monsters));memset(races,0,sizeof(races));memset(lore,0,sizeof(lore));
 credited_damage=credited_kills=damage_hooks=ambient_hits=hit_displays=last_race=0;
 monsters[1].r_idx=3;monsters[1].hp=100;monsters[1].maxhp=100;monsters[1].ml=true;
}
int main(void){
 object_type branded={.sval=1},plain={0};
 reset();monster_poison_brand(1,&branded,&branded,10);
 assert(monsters[1].poisoned==5);assert(monsters[1].mflag&MFLAG_PLAYER_POISON);
 assert(!monster_poison_tick(1));assert(credited_damage==1 && credited_kills==0 && ambient_hits==1 && damage_hooks==2);
 while(monsters[1].poisoned)assert(!monster_poison_tick(1));
 assert(credited_damage==5 && monsters[1].hp==95 && !(monsters[1].mflag&MFLAG_PLAYER_POISON));
 assert(damage_hooks==2*ambient_hits);
 reset();monsters[1].hp=1;monster_poison_brand(1,&branded,NULL,10);
 assert(monster_poison_tick(1));assert(credited_kills==1 && last_race==3 && credited_damage==1);
 assert(!monster_poison_tick(1));assert(credited_kills==1);
 reset();monster_poison_add(1,10);assert(!(monsters[1].mflag&MFLAG_PLAYER_POISON));monsters[1].hp=1;
 assert(monster_poison_tick(1));assert(!credited_damage && !credited_kills && damage_hooks==1);
 reset();monsters[1].mflag|=MFLAG_PLAYER_POISON;monster_poison_add(1,5);
 assert(!(monsters[1].mflag&MFLAG_PLAYER_POISON));
 reset();monster_poison_brand(1,&plain,&plain,10);assert(!monsters[1].poisoned);
 monster_poison_brand(1,&branded,NULL,0);assert(!monsters[1].poisoned);
 races[3].flags3=RF3_RES_POIS;monster_poison_brand(1,&branded,NULL,10);
 assert(!monsters[1].poisoned && !(monsters[1].mflag&MFLAG_PLAYER_POISON));assert(lore[3].flags3&RF3_RES_POIS);
 reset();monster_poison_brand(1,NULL,&branded,1000);assert(monsters[1].poisoned==100);
 monsters[1].hp=1000;assert(!monster_poison_tick(1));assert(monsters[1].poisoned==80 && credited_damage==20);
 assert(!monster_poison_tick(0));assert(!monster_poison_tick(mon_max));
 puts("Quest attribution: production poison brand/dose/decay/player and ambient death, no duplicate credit, stale provenance clearing: PASS");
}
'''
def integration_checks():
    save=(ROOT/'src/fs/save.c').read_text(encoding='utf-8')
    load=(ROOT/'src/fs/load.c').read_text(encoding='utf-8')
    combat=(ROOT/'src/cmd/combat/cmd-combat.c').read_text(encoding='utf-8')
    death=(ROOT/'src/world/monster-death.c').read_text(encoding='utf-8')
    saved_flags = re.search(r'#define SAVE_MON_FLAGS[\s\S]*?\)', save)[0]
    assert 'MFLAG_PLAYER_POISON' in saved_flags, 'Player poison must survive save'
    assert 'MFLAG_PLAYER_PUSH' not in saved_flags, 'Transient shove attribution must not persist'
    assert 'm_ptr->mflag & (SAVE_MON_FLAGS)' in save or 'm_ptr->mflag & SAVE_MON_FLAGS' in save
    assert re.search(r'!savefile_version_at_least\(0, 9, 8, 26\)[\s\S]{0,120}~MFLAG_PLAYER_POISON',load)
    assert 'm_ptr->mflag &= ~MFLAG_PLAYER_PUSH;' in load
    assert 'quest_followup_damage(m_ptr, dam, who);' in death
    assert re.search(r'if \(who < 0\) quest_followup_kill\(m_ptr->r_idx\)',death)
    assert re.search(r'quest_followup_damage\(m_ptr, song_dam, -1\);[\s\S]{0,250}morgoth_enter_final_stage',combat)
    assert re.search(r'quest_followup_kill\(m_ptr->r_idx\);\s*monster_death\(m_idx\)',combat)
    assert re.search(r'if \(player_push\) m_ptr->mflag \|= MFLAG_PLAYER_PUSH;[\s\S]{0,200}if \(player_push\) m_ptr->mflag &= ~MFLAG_PLAYER_PUSH;',combat)
    for name in ['src/monster/monster-move.c','src/cave/cave-lava.c']:
        text=(ROOT/name).read_text(encoding='utf-8')
        assert re.search(r'if \(m_ptr->mflag & MFLAG_PLAYER_PUSH\) quest_followup_kill\(m_ptr->r_idx\);\s*monster_death',text),name

def main():
    integration_checks();OUT.mkdir(parents=True,exist_ok=True)
    source=OUT/'check.c';source.write_text(HARNESS,encoding='utf-8')
    exe=OUT/'check.exe';env=os.environ.copy();env['PATH']='C:/msys64/mingw64/bin;C:/msys64/usr/bin;'+env.get('PATH','')
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-std=c17','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(ROOT/'src/monster/monster-poison.c'),'-o',str(exe)],cwd=ROOT/'build-standard',env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True,timeout=15)
    print('Integration guards: serialized poison, legacy clear, direct song, scoped player shove and terrain deaths: PASS')
if __name__=='__main__':main()

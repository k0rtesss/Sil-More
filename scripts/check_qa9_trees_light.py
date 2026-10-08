#!/usr/bin/env python3
"""Exercise Trees role changes through production song/ability updates and vision.

Uses initialized engine objects, real change_song/update_stuff and the native
ability callback. --song-baseline and --ui-baseline isolate missing refreshes.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/qa9-trees-light"
CHECKS = r'''
#include "player/player-upkeep-internal.h"
#include "supplies.h"
static const char* scenario="initialization";
#undef assert
#define assert(expr) do{if(!(expr)){fprintf(stderr,"%s: %s line%d\n",scenario,#expr,__LINE__);exit(1);}}while(0)
extern bool fixture_woven_toggle(void);
void __wrap_handle_stuff(void){update_stuff();}
void __wrap_message_flush(void){}
static void setup(bool item_woven)
{
    memset(p_ptr,0,sizeof(*p_ptr));memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));reset_map(3);
    for(int y=0;y<MAX_DUNGEON_HGT;y++)for(int x=0;x<MAX_DUNGEON_WID;x++)cave_light[y][x]=0;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=60;p_ptr->py=p_ptr->px=30;
    p_ptr->playing=true;p_ptr->chp=p_ptr->mhp=100;p_ptr->csp=20;p_ptr->msp=34;
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    for(int i=0;i<A_MAX;i++)p_ptr->stat_base[i]=3;
    p_ptr->skill_base[S_SNG]=19;p_ptr->new_exp=100000;
    const int songs[]={SNG_TREES,SNG_SILENCE,SNG_ELBERETH,SNG_LORIEN};
    for(int i=0;i<4;i++)p_ptr->innate_ability[S_SNG][songs[i]]=p_ptr->active_ability[S_SNG][songs[i]]=true;
    p_ptr->innate_ability[S_SNG][SNG_WOVEN_THEMES]=!item_woven;
    p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES]=true;
    if(item_woven){object_type* a=&inventory[INVEN_NECK];object_prep(a,lookup_kind(TV_AMULET,SV_AMULET_SELF_MADE));a->abilities=1;a->skilltype[0]=S_SNG;a->abilitynum[0]=SNG_WOVEN_THEMES;}
    object_type* torch=&inventory[INVEN_LITE];object_prep(torch,lookup_kind(TV_LIGHT,SV_LIGHT_TORCH));torch->number=1;player_light_set_fuel(torch,1000);
    rp_ptr=&p_info[0];current_character_profile=&c_info[0];character_generated=character_dungeon=true;character_icky=0;
    cave_feat[30][34]=cave_feat[32][30]=FEAT_RUBBLE;
    cave_m_idx[30][30]=-1;turn=1001;playerturn=100;
    calc_bonuses();calc_torch();update_stuff();assert(p_ptr->cur_light==1);p_ptr->update=0;
}
static void choose(int song,bool light_change,int energy,int light)
{
    int voice=p_ptr->csp,fuel=player_light_fuel(&inventory[INVEN_LITE]);
    p_ptr->energy_use=0;p_ptr->update=0;
    change_song(song);assert(!!(p_ptr->update&PU_TORCH)==light_change);
    assert(p_ptr->energy_use==energy);update_stuff();
    if(p_ptr->cur_light!=light||p_ptr->csp!=voice||player_light_fuel(&inventory[INVEN_LITE])!=fuel)fprintf(stderr,"light=%d/%d voice=%d/%d fuel=%d/%d score=%d songs=%d/%d\n",p_ptr->cur_light,light,p_ptr->csp,voice,player_light_fuel(&inventory[INVEN_LITE]),fuel,p_ptr->skill_use[S_SNG],p_ptr->song1,p_ptr->song2);
    assert(p_ptr->cur_light==light&&p_ptr->csp==voice&&player_light_fuel(&inventory[INVEN_LITE])==fuel);
    assert(turn==1001&&playerturn==100);
}
static void checks(void)
{
    scenario="main start updates real vision";setup(false);choose(SNG_TREES,true,100,5);
    assert(cave_info[30][34]&CAVE_SEEN);assert(cave_info[30][34]&CAVE_MARK);
    scenario="non-light minor does not reroll torch";choose(SNG_LORIEN,false,100,5);
    scenario="main to minor exchange";choose(SNG_EXCHANGE_THEMES,true,100,3);
    assert(!(cave_info[30][34]&CAVE_SEEN)&&(cave_info[30][34]&CAVE_MARK));assert(cave_info[32][30]&CAVE_SEEN);
    scenario="free stop minor updates visibility but keeps memory";choose(SNG_TREES,true,0,1);
    assert(p_ptr->song1==SNG_LORIEN&&p_ptr->song2==SNG_NOTHING);
    assert(!(cave_info[32][30]&CAVE_SEEN)&&(cave_info[32][30]&CAVE_MARK));
    scenario="minor to main exchange";choose(SNG_TREES,true,100,3);choose(SNG_EXCHANGE_THEMES,true,100,5);
    scenario="free stop main";choose(SNG_TREES,true,0,1);assert(p_ptr->song1==SNG_NOTHING&&p_ptr->song2==SNG_NOTHING);
    scenario="ordinary song start and stop keep torch unchanged";choose(SNG_LORIEN,false,100,1);choose(SNG_LORIEN,false,0,1);
    scenario="Silence damping changes Trees bonus without changing its role";
    choose(SNG_TREES,true,100,5);choose(SNG_SILENCE,true,100,4);choose(SNG_SILENCE,true,0,5);choose(SNG_TREES,true,0,1);
    puts("Real change_song/update_stuff main/minor/exchange/free stop, visibility/memory, non-Trees control and action resources PASS.");
    scenario="Voice exhaustion clears Trees light";choose(SNG_TREES,true,100,5);p_ptr->csp=0;p_ptr->update=0;sing();assert(p_ptr->update&PU_TORCH);update_stuff();assert(p_ptr->cur_light==1&&p_ptr->song1==SNG_NOTHING);
    scenario="item Woven loss keeps learned main and removes Trees light";setup(true);choose(SNG_LORIEN,false,100,1);choose(SNG_TREES,true,100,3);
    object_wipe(&inventory[INVEN_NECK]);calc_bonuses();assert(!p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES]);p_ptr->update=0;sing();
    assert(p_ptr->song1==SNG_LORIEN&&p_ptr->song2==SNG_NOTHING&&(p_ptr->update&PU_TORCH));update_stuff();assert(p_ptr->cur_light==1);
    object_type* a=&inventory[INVEN_NECK];object_prep(a,lookup_kind(TV_AMULET,SV_AMULET_SELF_MADE));a->abilities=1;a->skilltype[0]=S_SNG;a->abilitynum[0]=SNG_WOVEN_THEMES;p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES]=true;calc_bonuses();sing();update_stuff();assert(p_ptr->song2==SNG_NOTHING&&p_ptr->cur_light==1);
    puts("Real sing Voice/source cleanup retains valid main, no light/song resurrection PASS.");
    scenario="native Woven ability callback clears minor light";setup(false);choose(SNG_LORIEN,false,100,1);choose(SNG_TREES,true,100,3);
    int voice=p_ptr->csp,fuel=player_light_fuel(&inventory[INVEN_LITE]);p_ptr->energy_use=0;
    assert(fixture_woven_toggle());assert(!p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES]&&p_ptr->song2==SNG_NOTHING);
    assert(p_ptr->song1==SNG_LORIEN&&p_ptr->cur_light==1&&p_ptr->csp==voice&&!p_ptr->energy_use);
    assert(player_light_fuel(&inventory[INVEN_LITE])==fuel&&turn==1001&&playerturn==100);
    scenario="Woven-off removes Silence damping from Trees main";setup(false);choose(SNG_TREES,true,100,5);choose(SNG_SILENCE,true,100,4);
    voice=p_ptr->csp;fuel=player_light_fuel(&inventory[INVEN_LITE]);p_ptr->energy_use=0;
    assert(fixture_woven_toggle());assert(p_ptr->song1==SNG_TREES&&p_ptr->song2==SNG_NOTHING&&p_ptr->cur_light==5);
    assert(p_ptr->csp==voice&&!p_ptr->energy_use&&player_light_fuel(&inventory[INVEN_LITE])==fuel&&turn==1001&&playerturn==100);
    puts("Production ability callback + update_stuff Woven-off free light refresh PASS.");
}
'''



def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--song-baseline',action='store_true');parser.add_argument('--ui-baseline',action='store_true');args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):];init=init[:init.index('    check_templates();')]
    check=OUT/'check.c';check.write_text(prefix+CHECKS+init+'    checks();SDL_Quit();return 0;\n}\n',encoding='utf-8')
    songs=ROOT/'src/player/player-songs.c';abilities=ROOT/'src/cmd/ui/cmd-ui-abilities.c'
    if args.song_baseline:
        current=songs.read_text(encoding='utf-8');assert current.count('update_trees_song_light(previous_trees_bonus);')==3
        songs=OUT/'songs-baseline.c';songs.write_text(current.replace('update_trees_song_light(previous_trees_bonus);',''),encoding='utf-8')
    if args.ui_baseline:
        assert not args.song_baseline
        current=abilities.read_text(encoding='utf-8');needle="                if (trees_bonus != previous_trees_bonus)\n                    p_ptr->update |= PU_TORCH;\n";assert needle in current
        abilities=OUT/'abilities-baseline.c';abilities.write_text(current.replace(needle,''),encoding='utf-8')
    ability_wrapper=OUT/'abilities.c';ability_wrapper.write_text('#include "'+abilities.as_posix()+'"\nbool fixture_woven_toggle(void){return ability_browser_activate_choice(S_SNG,SNG_WOVEN_THEMES); }\n',encoding='utf-8')
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text());rsp=OUT/'objects.rsp';rsp.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/player/player-songs.c.obj','/src/cmd/ui/cmd-ui-abilities.c.obj'))),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe';subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(check),str(songs),str(ability_wrapper),'@'+str(rsp),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-Wl,--wrap=handle_stuff','-Wl,--wrap=message_flush','-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=30)

if __name__=='__main__':main()

#!/usr/bin/env python3
"""Traverse and fight all four recovered quest vaults through real engine commands.

Player stats and a digging tool are boosted deterministically; monster templates,
authored maps, movement, doors, melee/death callbacks and quest objectives remain
production code. Monsters do not receive scheduled turns. Native dialogue/reward
flows are outside this objective-and-topology fixture. All state is isolated.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/quest-vault-gameplay'
CHECKS=r'''
#include "quest/quest-runtime.h"
static int movements,attacks,tunnels,doors;
static int direction(int y,int x)
{
    for(int d=1;d<=9;d++) if(ddy[d]==y && ddx[d]==x) return d;
    assert(false); return 0;
}
static bool route_cell(int y,int x)
{
    if(!in_bounds_fully(y,x)) return false;
    int m=cave_m_idx[y][x], f=cave_feat[y][x];
    if(m>0 && (r_info[mon_list[m].r_idx].flags1&RF1_PEACEFUL)) return false;
    return cave_floor_bold(y,x) || cave_known_closed_door_bold(y,x)
        || f==FEAT_SECRET || f==FEAT_WALL_OUTER || f==FEAT_RUBBLE;
}
/* Return one physical step toward the target; inner vault walls are never dug. */
static int next_step(int ty,int tx)
{
    static int parent[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int queue[MAX_DUNGEON_HGT*MAX_DUNGEON_WID];
    memset(parent,-1,sizeof(parent));
    int start=p_ptr->py*MAX_DUNGEON_WID+p_ptr->px, target=ty*MAX_DUNGEON_WID+tx;
    int head=0,tail=0; queue[tail++]=start; parent[p_ptr->py][p_ptr->px]=start;
    while(head<tail && parent[ty][tx]<0) {
        int cell=queue[head++], y=cell/MAX_DUNGEON_WID,x=cell%MAX_DUNGEON_WID;
        for(int d=1;d<=9;d++) if(d!=5) {
            int yy=y+ddy[d],xx=x+ddx[d];
            if(!route_cell(yy,xx) || parent[yy][xx]>=0) continue;
            parent[yy][xx]=cell;queue[tail++]=yy*MAX_DUNGEON_WID+xx;
        }
    }
    if(parent[ty][tx]<0) {fprintf(stderr,"Unreachable target %d,%d from %d,%d\n",ty,tx,p_ptr->py,p_ptr->px);exit(2);}
    int cell=target;
    while(parent[cell/MAX_DUNGEON_WID][cell%MAX_DUNGEON_WID]!=start)
        cell=parent[cell/MAX_DUNGEON_WID][cell%MAX_DUNGEON_WID];
    return direction(cell/MAX_DUNGEON_WID-p_ptr->py,cell%MAX_DUNGEON_WID-p_ptr->px);
}
static void boost(void)
{
    for(int i=0;i<A_MAX;i++) p_ptr->stat_base[i]=p_ptr->stat_use[i]=30;
    for(int i=0;i<S_MAX;i++) p_ptr->skill_base[i]=p_ptr->skill_use[i]=200;
    p_ptr->mdd=20;p_ptr->mds=20;p_ptr->mdd2=p_ptr->mds2=0;
    p_ptr->chp=p_ptr->mhp=30000;p_ptr->csp=p_ptr->msp=30000;
    p_ptr->food=PY_FOOD_FULL;p_ptr->cur_light=20;
}
static void run_vault(int vault,int id,int depth,int seed,int expected)
{
    wipe_o_list();wipe_mon_list(); memset(p_ptr,0,sizeof(*p_ptr));
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));reset_map(depth);
    quest_followup_reset();
    for(int q=1;q<=16;q++) quest_set_enabled(q,false);
    quest_set_enabled(id,true);p_ptr->quest_test_sandbox=1;
    quest_debug_request_vault(id);quest_followup_generation_begin();
    quest_set_state(id,QUEST_STATE_ACTIVE);
    for(int c=32;c<127;c++) {
        int race=quest_vault_token_race(vault,(char)c);
        if(race<=0 || (r_info[race].flags1&RF1_PEACEFUL)) continue;
        if(id==13 || id==15) {
            assert(quest_followup_reserve_race(race));
            assert(!place_monster_one(4,4,race,false,true,NULL));
        }
    }
    op_ptr->opt[OPT_quest_rules_beta]=op_ptr->opt[OPT_quest_rewards_beta]=false;
    op_ptr->opt[OPT_lockpick_minigame]=false;
    op_ptr->opt[OPT_pacifist_attack_warning]=false;
    op_ptr->vault_drop_frequency=VDF_MEAGER;
    Rand_state_init(seed);int fy=one_in_(2),fx=one_in_(2);
    Rand_state_init(seed); assert(build_vault(44,44,&v_info[vault],false));
    vault_type* v=&v_info[vault]; cptr layout=v_text+v->text;
    for(int dy=0;dy<v->hgt;dy++) for(int dx=0;dx<v->wid;dx++) {
        int race=quest_vault_token_race(vault,layout[dy*v->wid+dx]);
        if(race<=0) continue;
        int y=44-v->hgt/2+(fy?v->hgt-1-dy:dy);
        int x=44-v->wid/2+(fx?v->wid-1-dx:dx);
        assert(cave_m_idx[y][x]>0 && mon_list[cave_m_idx[y][x]].r_idx==race);
    }
    quest_followup_vault_placed(id,depth);quest_followup_generation_commit();
    assert(p_ptr->quest_followup_depth[id-7]==depth);
    quest_set_state(id,QUEST_STATE_ACTIVE);

    assert(quest_accepted_count_this_run()==1);
    object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_LONG_SWORD));
    object_prep(&inventory[0],lookup_kind(TV_DIGGING,SV_MATTOCK)); inventory[0].pval=20;
    p_ptr->inven_cnt=1;p_ptr->playing=true;p_ptr->tutorial_deferred=true;
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    turn=1000;playerturn=100; character_generated=true;character_dungeon=true;
    player_place(44,44-v_info[vault].wid/2-2); boost();
    for(int y=0;y<88;y++) for(int x=0;x<88;x++) cave_info[y][x]|=CAVE_MARK|CAVE_SEEN|CAVE_GLOW;
    for(int m=1;m<mon_max;m++) {mon_list[m].ml=true;mon_list[m].alertness=ALERTNESS_ALERT;}
    int targets[16],n=0;
    for(int c=32;c<127;c++) {
        int race=quest_vault_token_race(vault,(char)c);
        if(race<=0 || !(r_info[race].flags1&RF1_UNIQUE) || (r_info[race].flags1&RF1_PEACEFUL)) continue;
        assert(n<16);targets[n++]=race;
        int present=0;for(int m=1;m<mon_max;m++) present+=mon_list[m].r_idx==race;
        assert(present==1);
    }
    assert(n==expected);
    movements=attacks=tunnels=doors=0;
    for(int t=0;t<n;t++) {
        int race=targets[t],m=0;
        for(int i=1;i<mon_max;i++) if(mon_list[i].r_idx==race) m=i;
        assert(m>0); int steps=0;
        while(mon_list[m].r_idx) {
            assert(++steps<3000);boost();
            int d=next_step(mon_list[m].fy,mon_list[m].fx), y=p_ptr->py+ddy[d],x=p_ptr->px+ddx[d];
            int f=cave_feat[y][x];
            p_ptr->command_dir=d;p_ptr->energy_use=100;
            if(f==FEAT_SECRET) {search();assert(cave_feat[y][x]!=FEAT_SECRET);continue;}
            if(f==FEAT_WALL_OUTER || f==FEAT_RUBBLE) {do_cmd_tunnel();tunnels++;}
            else if(cave_known_closed_door_bold(y,x)) {if(f>=FEAT_DOOR_HEAD+8) do_cmd_bash(); else do_cmd_open_aux(y,x);doors++;}
            else {if(cave_m_idx[y][x]>0) attacks++;else movements++;move_player(d);}
            turn++;playerturn++;new_combat_round();
            assert(!p_ptr->is_dead && !p_ptr->leaving);
        }
        assert(r_info[race].max_num==0);
        if(t<n-1) assert(quest_get_state(id)==QUEST_STATE_ACTIVE);
        printf("Vault %d quest %d seed %d: physically defeated %s; progress=0x%x state=%d\n",
            vault,id,seed,r_name+r_info[race].name,p_ptr->quest_followup_progress[id-7],quest_get_state(id));
    }
    assert(quest_get_state(id)==QUEST_STATE_SUCCESS);
    assert(p_ptr->quest_followup_progress[id-7]==(id==7?3:id==13?63:1));
    printf("Vault %d seed %d PASS: %d targets, %d movement steps, %d attack actions, %d tunnel actions, %d door actions\n",vault,seed,n,movements,attacks,tunnels,doors);
}
'''
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix+=fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    source=OUT/'check.c'
    source.write_text(prefix+CHECKS+init+'''
    assert(init_flavor_info()==0); assert(init_quest_info()==0);
    const int ids[]={8,7,13,15}, depths[]={17,10,5,16},counts[]={1,2,6,1};
    for(int v=0;v<4;v++) {
        unsigned orientations=0;
        for(int seed=1;seed<=64 && orientations!=15;seed++) {
            Rand_state_init(seed); int fy=one_in_(2),fx=one_in_(2);
            unsigned flag=1U<<(fy*2+fx);
            if(orientations&flag) continue;
            run_vault(464+v,ids[v],depths[v],seed,counts[v]);orientations|=flag;
        }
        assert(orientations==15);
    }
    puts("All recovered authored vault movement/combat/objective paths PASS.");SDL_Quit();return 0;
}
''',encoding='utf-8')
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    rsp=OUT/'objects.rsp';rsp.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith('/src/main.c.obj')))
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),'@'+str(rsp),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='data-',dir=OUT) as data:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),data],cwd=data,env=env,check=True,timeout=90)
if __name__=='__main__':main()

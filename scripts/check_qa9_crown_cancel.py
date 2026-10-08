#!/usr/bin/env python3
"""Production Crown cancellations, paid rolls, shatters and final-jewel events.
--baseline reproduces paid inner No; --final-event-baseline independently
restores only the stale Crown-pointer guard. No player saves are opened.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/qa9-crown-cancel'
CHECKS=r'''
#include "angband.h"
#include "object/object-inventory-limits.h"
#include "cmd/item/cmd-item-core.c"
#include <assert.h>
extern void fixture_reset_map(void);
static int answers[2], questions, hit_calls, damage_calls, truce_calls;
static int final_cries, final_dooms, final_wakes;
static bool force_drop;
bool __wrap_get_check(cptr prompt) {
    assert(questions<2);
    if(questions==0) assert(strstr(prompt,"Will you try to prise"));
    else assert(strstr(prompt,"Will you dare") || strstr(prompt,"sure you wish"));
    return answers[questions++]!=0;
}
extern int __real_hit_roll(int,int,const monster_type*,const monster_type*,bool);
int __wrap_hit_roll(int att,int evn,const monster_type* a,const monster_type* b,bool display)
{ ++hit_calls;return __real_hit_roll(att,evn,a,b,display); }
extern void __real_update_combat_rolls2(int,int,int,int,int,int,int,int,bool);
void __wrap_update_combat_rolls2(int dd,int ds,int dam,int pd,int ps,int prt,int percent,int type,bool melee)
{ ++damage_calls;__real_update_combat_rolls2(dd,ds,dam,pd,ps,prt,percent,type,melee); }
extern void __real_break_truce(bool);
void __wrap_break_truce(bool obvious) {++truce_calls;__real_break_truce(obvious);}
void __wrap_handle_stuff(void) {}
void __wrap_update_stuff(void) {}
void __wrap_msg_print(cptr text) {
    if(!text)return;
    if(strstr(text,"cry of vengeance"))++final_cries;
    if(strstr(text,"doom awaiting"))++final_dooms;
}
extern void __real_wake_all_monsters(int);
void __wrap_wake_all_monsters(int who) {
    assert(who==0);++final_wakes;__real_wake_all_monsters(who);
}
extern int __real_inven_carry(object_type*,bool);
int __wrap_inven_carry(object_type* o,bool combine) {
    if(force_drop && o->tval==TV_LIGHT && o->sval==SV_LIGHT_SILMARIL)return -1;
    return __real_inven_carry(o,combine);
}
bool __wrap_tutorial_game_action_allowed(const char* action,const object_type* o)
{(void)action;(void)o;return true;}
void __wrap_tutorial_game_action_done(const char* action,const object_type* o)
{(void)action;(void)o;}
static void prepare(int crown,int seed)
{
    memset(p_ptr,0,sizeof(*p_ptr));fixture_reset_map();
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();
    p_ptr->py=p_ptr->px=5;p_ptr->depth=20;p_ptr->playing=true;
    p_ptr->chp=p_ptr->mhp=100;p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    p_ptr->stat_base[A_STR]=20;p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->skill_use[S_MEL]=100;p_ptr->mdd=100;p_ptr->mds=1;
    p_ptr->previous_action[0]=ACTION_READY_MELEE;p_ptr->truce=1;
    p_ptr->morgoth_state=crown==ART_MORGOTH_3?1:(crown==ART_MORGOTH_2?2:3);
    object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_LONG_SWORD));p_ptr->equip_cnt=1;
    int silmarils=ART_MORGOTH_3-crown;
    if(silmarils) {
        object_prep(&inventory[0],lookup_kind(TV_LIGHT,SV_LIGHT_SILMARIL));
        inventory[0].number=silmarils;p_ptr->inven_cnt=1;
    }
    object_prep(&o_list[1],lookup_kind(a_info[crown].tval,a_info[crown].sval));
    o_list[1].name1=crown;o_list[1].iy=o_list[1].ix=5;
    cave_o_idx[5][5]=1;cave_m_idx[5][5]=-1;o_max=2;o_cnt=1;
    mon_list[1]=(monster_type){.r_idx=R_IDX_MORGOTH,.fy=4,.fx=5,.cdis=1,
        .alertness=ALERTNESS_VERY_ALERT,.hp=100,.maxhp=100};
    cave_m_idx[4][5]=1;
    mon_list[2]=(monster_type){.r_idx=11,.fy=7,.fx=7,
        .alertness=ALERTNESS_UNWARY,.hp=40,.maxhp=40};
    cave_m_idx[7][7]=2;mon_max=3;mon_cnt=2;
    questions=hit_calls=damage_calls=truce_calls=0;
    final_cries=final_dooms=final_wakes=0;force_drop=false;
    answers[0]=answers[1]=1;stealth_score=55;
    combat_history_head=combat_history_count=0;
    memset(combat_history,0,sizeof(combat_history));
    Rand_state_init(seed);
}
static void assert_attempt_paid(void)
{
    assert(p_ptr->energy_use==100 && p_ptr->previous_action[0]==ACTION_MISC);
    assert(hit_calls==1 && stealth_score<55);
    if(o_list[1].name1!=ART_MORGOTH_0)
        assert(!final_cries && !final_dooms && !final_wakes);
}
void checks(void)
{
    const int crowns[]={ART_MORGOTH_3,ART_MORGOTH_2,ART_MORGOTH_1};
    for(int stage=0;stage<3;++stage) for(int incoming=0;incoming<2;++incoming) {
        prepare(crowns[stage],123);answers[0]=0;
        p_ptr->energy_use=incoming?50:0;
        int old_sils=silmarils_possessed();u64b old_rng=Rand_state_export();
        assert(do_cmd_delete_item_by_index(-1));
        assert(questions==1 && !hit_calls && !damage_calls && !truce_calls);
        assert(p_ptr->energy_use==(incoming?50:0) && p_ptr->previous_action[0]==ACTION_READY_MELEE);
        assert(o_list[1].name1==crowns[stage] && silmarils_possessed()==old_sils);
        assert(stealth_score==55 && p_ptr->truce==1 && Rand_state_export()==old_rng);
        if(stage==0)continue;
        prepare(crowns[stage],123);answers[1]=0;
        p_ptr->energy_use=incoming?50:0;old_rng=Rand_state_export();
        int old_state=p_ptr->morgoth_state;
        assert(do_cmd_delete_item_by_index(-1));
        assert(questions==2 && !hit_calls && !damage_calls && !truce_calls);
        assert(p_ptr->energy_use==(incoming?50:0) && p_ptr->previous_action[0]==ACTION_READY_MELEE);
        assert(o_list[1].name1==crowns[stage] && silmarils_possessed()==old_sils);
        assert(p_ptr->morgoth_state==old_state && !p_ptr->crown_shatter_sil2 && !p_ptr->crown_shatter_sil3);
        assert(stealth_score==55 && p_ptr->truce==1 && Rand_state_export()==old_rng);
        assert(!combat_history_count && inventory[INVEN_WIELD].k_idx);
        assert(!final_cries && !final_dooms && !final_wakes);
    }
    puts("PASS: outerNo and both innerNo preserve incoming0/50energy, action, RNG, rolls, noise, truce, Crown, jewels and anger.");
    for(int crown=ART_MORGOTH_1;crown<=ART_MORGOTH_3;++crown) {
        prepare(crown,123);p_ptr->skill_use[S_MEL]=-1000;
        assert(do_cmd_delete_item_by_index(-1));assert_attempt_paid();
        assert(!damage_calls && o_list[1].name1==crown);
        assert(silmarils_possessed()==ART_MORGOTH_3-crown);
        prepare(crown,123);p_ptr->mdd=p_ptr->mds=1;inventory[INVEN_WIELD].weight=30000;
        assert(do_cmd_delete_item_by_index(-1));assert_attempt_paid();
        assert(damage_calls==1 && o_list[1].name1==crown);
        assert(silmarils_possessed()==ART_MORGOTH_3-crown);
    }
    prepare(ART_MORGOTH_3,123);
    assert(do_cmd_delete_item_by_index(-1));assert_attempt_paid();
    assert(o_list[1].name1==ART_MORGOTH_2 && silmarils_possessed()==1);
    bool second_shattered=false;
    for(int seed=1;seed<=40 && !second_shattered;++seed) {
        prepare(ART_MORGOTH_2,seed);
        assert(do_cmd_delete_item_by_index(-1));assert_attempt_paid();
        second_shattered=p_ptr->crown_shatter_sil2;
        if(second_shattered) {
            assert(!inventory[INVEN_WIELD].k_idx && o_list[1].name1==ART_MORGOTH_2);
            assert(silmarils_possessed()==1 && p_ptr->morgoth_state==3);
        }
    }
    assert(second_shattered);
    prepare(ART_MORGOTH_2,123);p_ptr->crown_shatter_sil2=1;
    assert(do_cmd_delete_item_by_index(-1));assert_attempt_paid();
    assert(o_list[1].name1==ART_MORGOTH_1 && silmarils_possessed()==2 && inventory[INVEN_WIELD].k_idx);
    prepare(ART_MORGOTH_1,123);
    assert(do_cmd_delete_item_by_index(-1));assert_attempt_paid();
    assert(p_ptr->crown_shatter_sil3 && !inventory[INVEN_WIELD].k_idx);
    assert(o_list[1].name1==ART_MORGOTH_1 && silmarils_possessed()==2 && p_ptr->morgoth_state==4);
    prepare(ART_MORGOTH_1,123);p_ptr->crown_shatter_sil3=1;
    assert(do_cmd_delete_item_by_index(-1));assert_attempt_paid();
    assert(o_list[1].name1==ART_MORGOTH_0 && silmarils_possessed()==3 && inventory[INVEN_WIELD].k_idx);
    assert(p_ptr->cursed && p_ptr->morgoth_state==4);
    assert(final_cries==1 && final_dooms==1 && final_wakes==1);
    assert(mon_list[2].alertness>=ALERTNESS_VERY_ALERT);
    prepare(ART_MORGOTH_1,123);p_ptr->crown_shatter_sil3=1;force_drop=true;
    assert(do_cmd_delete_item_by_index(-1));assert_attempt_paid();
    assert(o_list[1].name1==ART_MORGOTH_0 && silmarils_possessed()==2);
    int floor_silmarils=0;
    for(int i=2;i<o_max;++i)
        if(o_list[i].k_idx && o_list[i].tval==TV_LIGHT && o_list[i].sval==SV_LIGHT_SILMARIL)
            floor_silmarils+=o_list[i].number;
    assert(floor_silmarils==1 && p_ptr->cursed && p_ptr->morgoth_state==4);
    assert(final_cries==1 && final_dooms==1 && final_wakes==1);
    assert(mon_list[2].alertness>=ALERTNESS_VERY_ALERT);
    puts("PASS: real miss/protection failures stay paid, 2nd/3rd shatters and replacement retries paid, all3 jewel successes paid; final carry/drop each emit one cry/doom/globalwake and force anger3to4.");
}
'''
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    group=parser.add_mutually_exclusive_group()
    group.add_argument('--baseline',action='store_true')
    group.add_argument('--final-event-baseline',action='store_true')
    args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix+=fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    (OUT/'check.c').write_text(prefix+'\nvoid fixture_reset_map(void){reset_map(20);}\nextern void checks(void);\n'+init+'    checks();SDL_Quit();return 0;\n}\n')
    checks=CHECKS
    if args.baseline:
        baseline=OUT/'item-baseline.c'
        baseline.write_bytes(subprocess.check_output(['git','-c',f'safe.directory={ROOT.as_posix()}','show','HEAD:src/cmd/item/cmd-item-core.c'],cwd=ROOT))
        checks=checks.replace('"cmd/item/cmd-item-core.c"','"'+baseline.as_posix()+'"')
    if args.final_event_baseline:
        baseline=OUT/'final-event-baseline.c'
        current=(ROOT/'src/cmd/item/cmd-item-core.c').read_text()
        assert 'if (final_jewel_freed)' in current
        baseline.write_text(current.replace('if (final_jewel_freed)',
            'if (o_ptr->name1 == ART_MORGOTH_0)',1))
        checks=checks.replace('"cmd/item/cmd-item-core.c"','"'+baseline.as_posix()+'"')
    (OUT/'crown.c').write_text(checks)
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response=OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/cmd/item/cmd-item-core.c.obj'))))
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    wrapped=('wake_all_monsters','inven_carry','get_check','hit_roll','update_combat_rolls2','break_truce','handle_stuff','update_stuff','msg_print','tutorial_game_action_allowed','tutorial_game_action_done')
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g','@CMakeFiles/sil-more.dir/includes_C.rsp',str(OUT/'check.c'),str(OUT/'crown.c'),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*('-Wl,--wrap='+p for p in wrapped),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=30)
if __name__=='__main__':main()

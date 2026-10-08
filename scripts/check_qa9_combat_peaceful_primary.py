#!/usr/bin/env python3
"""Real melee dispatch: peaceful targets cannot start or stop automatic attacks.
--baseline removes only the primary-target guard; --follow-baseline removes
only the peaceful Follow-Through filter. No player saves opened.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/qa9-combat-peaceful-primary'
GUARD = '''    /* A peaceful primary target is an interaction, not the start of a sweep. */
    if (attack_type == ATT_MAIN
        && handle_peaceful_attack_target(y, x, attack_type))
        return;

'''
FOLLOW_GUARD = '                    && !(r_info[m_ptr->r_idx].flags1 & RF1_PEACEFUL)\n'
CHECKS = r'''
#include "angband.h"
#include "cmd/combat/cmd-combat.c"
#include <assert.h>
extern void fixture_reset_map(void);
static int rolls, sweeps, questions, continuations, interactions;
extern void __real_update_combat_rolls1(const monster_type*,const monster_type*,bool,int,int,int,int);
void __wrap_update_combat_rolls1(const monster_type* x,const monster_type* y,bool v,int a,int ar,int e,int er)
{ ++rolls; __real_update_combat_rolls1(x,y,v,a,ar,e,er); }
void __wrap_msg_print(cptr text)
{
    if(text && strstr(text,"whirl around")) ++sweeps;
    if(text && strstr(text,"continue your attack")) ++continuations;
}
bool __wrap_get_check(cptr text) { (void)text; ++questions; return false; }
bool __wrap_handle_thrall_interaction(monster_type* npc)
{ assert(is_alert_thrall(npc)); ++interactions; return true; }
void __wrap_handle_stuff(void) {}
void __wrap_update_stuff(void) {}
bool __wrap_tutorial_game_action_allowed(cptr action,const object_type* object)
{ (void)action; (void)object; return true; }
void __wrap_tutorial_game_action_done(cptr action,const object_type* object)
{ (void)action; (void)object; }
static int place(int y,int x,int race)
{
    assert(place_monster_one(y,x,race,false,true,NULL));
    int id=cave_m_idx[y][x]; assert(id>0);
    mon_list[id].ml=true; mon_list[id].alertness=ALERTNESS_VERY_ALERT;
    return id;
}
static void prepare(void)
{
    memset(p_ptr,0,sizeof(*p_ptr)); fixture_reset_map();
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store();
    supplies_reset_store();
    p_ptr->py=p_ptr->px=20; cave_m_idx[20][20]=-1;
    p_ptr->playing=true; p_ptr->mhp=p_ptr->chp=1000;
    p_ptr->pspeed=2; p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->active_ability[S_MEL][MEL_WHIRLWIND_ATTACK]=1;
    p_ptr->skill_base[S_MEL]=20; p_ptr->skill_use[S_MEL]=40;
    p_ptr->skill_use[S_EVN]=40;
    for(int i=0;i<A_MAX;++i) p_ptr->stat_base[i]=p_ptr->stat_use[i]=20;
    object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_POLEARM,SV_SPEAR));
    inventory[INVEN_WIELD].number=1;
    p_ptr->mdd=1; p_ptr->mds=12;
    p_ptr->energy_use=100; p_ptr->previous_action[0]=ACTION_MISC;
    forgo_attacking_unwary=true; player_attacked=false;
    Rand_state_init(1729);
    rolls=sweeps=questions=continuations=interactions=0; turn=999; playerturn=99;
    assert(count_open_adjacent_squares(20,20)==8);
}
void checks(void)
{
    const int primary_races[]={14,16,17,18};
    for(int race=0;race<4;++race) {
        int peaceful=primary_races[race];
        for(int enemies=0;enemies<=2;++enemies) {
            prepare(); int npc=place(20,19,peaceful);
            int old_hp=mon_list[npc].hp;
            if(enemies>0) place(19,20,32);
            if(enemies>1) place(20,21,32);
            u64b rng=Rand_state_export();
            py_attack(20,19,ATT_MAIN);
            assert(!rolls && !sweeps && !questions && !player_attacked);
            /* Actual quest assignment can consume RNG; this suite verifies
             * the public interaction dispatch, not that quest's UI. */
            assert(interactions==((peaceful==16 || peaceful==17)?1:0));
            if(!interactions) assert(Rand_state_export()==rng);
            assert(p_ptr->energy_use==0);
            assert(mon_list[npc].hp==old_hp && cave_m_idx[20][19]==npc);
            assert(!enemies || cave_m_idx[19][20]>0);
            assert(enemies<2 || cave_m_idx[20][21]>0);
            assert(turn==999 && playerturn==99);
        }
    }
    prepare(); int npc=place(20,19,14), hp=mon_list[npc].hp;
    place(19,20,32); place(20,21,32);
    py_attack(19,20,ATT_MAIN);
    assert(rolls==2 && sweeps==1 && !questions && player_attacked);
    assert(p_ptr->energy_use==100 && inventory[INVEN_WIELD].number==1);
    assert(!cave_m_idx[19][20] && !cave_m_idx[20][21]);
    assert(cave_m_idx[20][19]==npc && mon_list[npc].hp==hp);
    puts("PASS: twelve authored peaceful-primary cases with zero/one/two awake hostiles stay free/no combat; alert16/17 dispatch interaction once, non-quest cases preserve RNG; hostile-primary Whirlwind attacks both foes once, preserves NPC and remains paid.");
    const int peaceful_races[]={0,14,18};
    for(int i=0;i<3;++i) {
        prepare();
        p_ptr->active_ability[S_MEL][MEL_WHIRLWIND_ATTACK]=0;
        p_ptr->active_ability[S_MEL][MEL_FOLLOW_THROUGH]=1;
        npc=0; hp=0;
        if(peaceful_races[i]) {
            npc=place(21,20,peaceful_races[i]); hp=mon_list[npc].hp;
        }
        /* SW primary, south peaceful, east hostile: the peaceful square is
         * encountered first by the anticlockwise follow-through scan. */
        place(21,19,32); place(20,21,32);
        py_attack(21,19,ATT_MAIN);
        assert(rolls==2 && continuations==1 && !sweeps && !questions);
        assert(player_attacked && p_ptr->energy_use==100);
        assert(!cave_m_idx[21][19] && !cave_m_idx[20][21]);
        assert(!npc || (cave_m_idx[21][20]==npc && mon_list[npc].hp==hp));
        assert(inventory[INVEN_WIELD].number==1);
    }
    puts("PASS: real two-hostile Follow-Through stays paid and rolls once per foe, both ordinarily and past authored south thrall/Tulkas; peaceful HP unchanged, default ForGo with awake hostiles.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--baseline', action='store_true')
    group.add_argument('--follow-baseline', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n' + fixture_function('reset_map') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    (OUT/'check.c').write_text(prefix+'\nvoid fixture_reset_map(void){reset_map(1);}\nextern void checks(void);\n'+init+'    checks();SDL_Quit();return 0;\n}\n')
    source = CHECKS
    if args.baseline or args.follow_baseline:
        combat = (ROOT/'src/cmd/combat/cmd-combat.c').read_text()
        removed = FOLLOW_GUARD if args.follow_baseline else GUARD
        assert combat.count(removed) == 1
        baseline = OUT/('follow-baseline.c' if args.follow_baseline else 'combat-baseline.c')
        baseline.write_text(combat.replace(removed, '', 1))
        source = source.replace('"cmd/combat/cmd-combat.c"', '"'+baseline.as_posix()+'"')
    (OUT/'combat.c').write_text(source)
    objects = shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/cmd/combat/cmd-combat.c.obj'))))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    wrapped = ('update_combat_rolls1','msg_print','get_check','handle_thrall_interaction','handle_stuff','update_stuff','tutorial_game_action_allowed','tutorial_game_action_done')
    exe = OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g','@CMakeFiles/sil-more.dir/includes_C.rsp',str(OUT/'check.c'),str(OUT/'combat.c'),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*('-Wl,--wrap='+p for p in wrapped),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=30)


if __name__ == '__main__':
    main()

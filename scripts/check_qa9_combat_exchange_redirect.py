#!/usr/bin/env python3
"""Real Exchange command: revalidate creatures selected by confusion.
--baseline removes only the redirected immovable-target guard.
Direction inputs are controlled; placement, reaction, swap and water cost are real.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT/'build-standard'
OUT = ROOT/'scripts/output/qa9-combat-exchange-redirect'
GUARD = '''    /* Confusion can select a different creature with different restrictions. */
    if ((r_ptr->flags1 & (RF1_NEVER_MOVE))
        || (r_ptr->flags1 & (RF1_HIDDEN_MOVE)))
    {
        msg_format("You cannot get past %s.", m_name);
        return;
    }

'''
CHECKS = r'''
#include "angband.h"
#include "cmd/item/cmd-item-utility.c"
#include "melee/melee-movement.h"
#include <assert.h>
extern void fixture_reset_map(void);
extern bool current_build_vault_exact_token;
static int intended, redirected, prompts, confusion_calls, reactions, rolls;
bool __wrap_get_rep_dir(int* dir)
{ ++prompts; *dir=intended; return intended!=0; }
bool __wrap_confuse_dir(int* dir)
{ ++confusion_calls; if(!redirected)return false; bool changed=*dir!=redirected; *dir=redirected; return changed; }
extern bool __real_make_attack_reaction(monster_type*);
bool __wrap_make_attack_reaction(monster_type* m)
{ ++reactions; return __real_make_attack_reaction(m); }
extern void __real_update_combat_rolls1(const monster_type*,const monster_type*,bool,int,int,int,int);
void __wrap_update_combat_rolls1(const monster_type* a,const monster_type* b,bool v,int x,int xr,int y,int yr)
{ ++rolls; __real_update_combat_rolls1(a,b,v,x,xr,y,yr); }
void __wrap_sdl_player_exchange_begin_direction_prompt(void) {}
void __wrap_sdl_player_exchange_cancel_direction_prompt(void) {}
void __wrap_handle_stuff(void) {}
void __wrap_update_stuff(void) {}
void __wrap_msg_print(cptr text) { (void)text; }
static int place(int y,int x,int race,int alertness)
{
    assert(place_monster_one(y,x,race,false,true,NULL));
    int id=cave_m_idx[y][x]; assert(id>0);
    mon_list[id].ml=true; mon_list[id].alertness=alertness;
    return id;
}
static void prepare(void)
{
    memset(p_ptr,0,sizeof(*p_ptr)); fixture_reset_map();
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store(); supplies_reset_store();
    p_ptr->py=p_ptr->px=20; cave_m_idx[20][20]=-1;
    p_ptr->playing=true; p_ptr->mhp=p_ptr->chp=1000;
    p_ptr->pspeed=2; p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES]=1;
    p_ptr->skill_use[S_EVN]=40;
    for(int i=0;i<A_MAX;++i)p_ptr->stat_base[i]=p_ptr->stat_use[i]=20;
    object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_POLEARM,SV_SPEAR));
    inventory[INVEN_WIELD].number=1;
    object_prep(&inventory[0],lookup_kind(TV_ARROW,SV_NORMAL_ARROW)); inventory[0].number=7;
    p_ptr->previous_action[0]=5; p_ptr->energy_use=0;
    intended=4; redirected=prompts=confusion_calls=reactions=rolls=0;
    Rand_state_init(1729); turn=999; playerturn=99;
}
static void conserved(void)
{
    assert(inventory[INVEN_WIELD].number==1 && inventory[0].number==7);
    assert(turn==999 && playerturn==99);
}
static void stationary(int actor,int y,int x,int energy)
{
    assert(p_ptr->py==20 && p_ptr->px==20 && cave_m_idx[20][20]==-1);
    assert(cave_m_idx[y][x]==actor && mon_list[actor].fy==y && mon_list[actor].fx==x);
    assert(p_ptr->energy_use==energy && !reactions && !rolls); conserved();
}

static void check_interrupted_exchanges(void)
{
    prepare();
    int troll = place(20,19,74,ALERTNESS_VERY_ALERT);
    p_ptr->skill_use[S_EVN] = -100;
    p_ptr->stat_use[A_CON] = -5;
    do_cmd_exchange();
    printf("Troll response: player x=%d, troll x=%d, knockback=%d.\n",
        p_ptr->px, mon_list[troll].fx, p_ptr->knocked_back);
    assert(p_ptr->knocked_back && p_ptr->skip_next_turn && p_ptr->chp > 0);
    assert(p_ptr->px == 21 && cave_m_idx[20][21] == -1);
    assert(mon_list[troll].fx == 19 && cave_m_idx[20][19] == troll);
    assert(!cave_m_idx[20][20] && p_ptr->energy_use == 100);

    prepare();
    int scout = place(20,19,31,ALERTNESS_VERY_ALERT);
    mon_list[scout].hp = 1;
    p_ptr->active_ability[S_EVN][EVN_RIPOSTE] = true;
    p_ptr->skill_use[S_EVN] = p_ptr->skill_use[S_MEL] = 100;
    p_ptr->mdd = 10; p_ptr->mds = 5;
    do_cmd_exchange();
    assert(p_ptr->ripostes == 1 && !mon_list[scout].r_idx);
    assert(p_ptr->px == 20 && cave_m_idx[20][20] == -1 && !cave_m_idx[20][19]);
    assert(!mon_list[scout].alertness && p_ptr->energy_use == 100);

    prepare();
    int bird = place(20,19,22,ALERTNESS_ALERT);
    cave_set_feat(20,19,FEAT_CHASM);
    cave_info[20][19] |= CAVE_SEEN;
    do_cmd_exchange();
    stationary(bird,20,19,0);

    prepare();
    bird = place(20,19,22,ALERTNESS_ALERT);
    cave_set_feat(20,19,FEAT_CHASM);
    cave_info[20][19] &= ~(CAVE_MARK | CAVE_SEEN);
    do_cmd_exchange();
    stationary(bird,20,19,100);

    prepare();
    place(20,19,31,ALERTNESS_ALERT);
    bird = place(21,19,22,ALERTNESS_ALERT);
    cave_set_feat(21,19,FEAT_CHASM);
    redirected=1;
    do_cmd_exchange();
    stationary(bird,21,19,100);
    puts("Interrupted Exchange: knockback and counter-kills retain real positions; deliberate chasm refusal is free.");
}

static void check_monster_exchange_landing(void)
{
    prepare();
    int cat = place(20,19,154,ALERTNESS_VERY_ALERT);
    p_ptr->leaping = true;
    cave_set_feat(20,20,FEAT_CHASM);
    cave_set_feat(20,19,FEAT_TRAP_PIT);
    monster_exchange_places(&mon_list[cat]);
    assert(!mon_list[cat].r_idx && !cave_m_idx[20][20]);
    assert(!p_ptr->leaping && p_ptr->px == 19 && cave_m_idx[20][19] == -1);
    printf("Cat falls in chasm: player reaches pit with %d of 1000 HP.\n", p_ptr->chp);
    assert(p_ptr->chp < 1000);
}

static void check_fatal_passing_attack(void)
{
    prepare();
    current_build_vault_exact_token = true;
    int ulfang = place(19,20,125,ALERTNESS_VERY_ALERT);
    int reaper = place(20,19,406,ALERTNESS_VERY_ALERT);
    current_build_vault_exact_token = false;
    int old_deaths = l_list[406].deaths;
    p_ptr->chp = 1;
    p_ptr->skill_use[S_EVN] = -100;
    monster_swap(20,20,21,20);
    assert(p_ptr->is_dead && cave_m_idx[20][20] == -1 && !cave_m_idx[21][20]);
    printf("Lethal passing attack: %d reactions, %d combat rolls.\n", reactions, rolls);
    assert(reactions == 1 && rolls == 1 && l_list[406].deaths == old_deaths);
    assert(cave_m_idx[19][20] == ulfang && cave_m_idx[20][19] == reaper);
}
void checks(void)
{
    prepare(); int scout=place(20,19,31,ALERTNESS_VERY_ALERT); intended=0;
    do_cmd_exchange(); stationary(scout,20,19,0); assert(prompts==1 && !confusion_calls);
    prepare(); do_cmd_exchange(); assert(!p_ptr->energy_use && !confusion_calls);
    const int immovable[]={12,163,17};
    for(int i=0;i<3;++i) {
        prepare(); int actor=place(20,19,immovable[i],ALERTNESS_VERY_ALERT);
        do_cmd_exchange(); stationary(actor,20,19,0); assert(!confusion_calls);
        prepare(); scout=place(20,19,31,ALERTNESS_VERY_ALERT);
        actor=place(21,19,immovable[i],ALERTNESS_VERY_ALERT);
        p_ptr->confused=10; redirected=1;
        do_cmd_exchange(); stationary(actor,21,19,100);
        assert(cave_m_idx[20][19]==scout && confusion_calls==1);
    }
    prepare(); scout=place(20,19,31,ALERTNESS_VERY_ALERT); redirected=8;
    do_cmd_exchange(); stationary(scout,20,19,100); assert(confusion_calls==1);
    prepare(); cave_feat[20][19]=FEAT_WALL_EXTRA;
    int spectre=place(20,19,167,ALERTNESS_VERY_ALERT);
    do_cmd_exchange(); stationary(spectre,20,19,0);
    prepare(); scout=place(20,19,31,ALERTNESS_VERY_ALERT);
    cave_feat[21][19]=FEAT_WALL_EXTRA; spectre=place(21,19,167,ALERTNESS_VERY_ALERT);
    redirected=1; do_cmd_exchange(); stationary(spectre,21,19,100);
    assert(cave_m_idx[20][19]==scout);
    for(int water=0;water<3;++water) {
        prepare(); scout=place(20,19,31,ALERTNESS_VERY_ALERT);
        if(water==1)cave_feat[20][20]=FEAT_WATER;
        if(water==2)cave_feat[20][19]=FEAT_WATER;
        do_cmd_exchange();
        assert(p_ptr->py==20 && p_ptr->px==19 && cave_m_idx[20][19]==-1);
        assert(cave_m_idx[20][20]==scout && mon_list[scout].fy==20 && mon_list[scout].fx==20);
        assert(p_ptr->energy_use==(water?150:100) && reactions==1 && rolls==1);
        conserved();
    }
    /* Preserve the existing movable-peaceful policy; confusion must not
     * impose new restrictions beyond the normal Exchange predicate. */
    prepare(); scout=place(20,19,31,ALERTNESS_VERY_ALERT);
    int thrall=place(21,19,14,ALERTNESS_UNWARY); redirected=1;
    do_cmd_exchange();
    assert(p_ptr->py==21 && p_ptr->px==19 && cave_m_idx[21][19]==-1);
    assert(cave_m_idx[20][20]==thrall && cave_m_idx[20][19]==scout);
    assert(p_ptr->energy_use==100 && !reactions && !rolls); conserved();
    prepare();
    int peaceful = place(20,19,14,ALERTNESS_VERY_ALERT);
    do_cmd_exchange();
    assert(p_ptr->px == 19 && cave_m_idx[20][20] == peaceful && !reactions && !rolls);
    check_interrupted_exchanges();
    check_monster_exchange_landing();
    check_fatal_passing_attack();
    puts("PASS: real Exchange cancellation/initial immovable+wall refusals free; redirected thorn/Grotesque/alert-thrall/empty/wall refusals paid without reactions or displacement; normal floor and both water swaps have one real passing attack/correct100or150energy, movable-friendly policy and gear conserved.");
}
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',action='store_true')
    args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix+=fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    (OUT/'check.c').write_text(prefix+'\nvoid fixture_reset_map(void){reset_map(1);}\nextern void checks(void);\n'+init+'    checks();SDL_Quit();return 0;\n}\n')
    source=CHECKS
    if args.baseline:
        utility=(ROOT/'src/cmd/item/cmd-item-utility.c').read_text()
        assert utility.count(GUARD)==1
        baseline=OUT/'exchange-baseline.c'; baseline.write_text(utility.replace(GUARD,'',1))
        source=source.replace('"cmd/item/cmd-item-utility.c"','"'+baseline.as_posix()+'"')
    (OUT/'exchange.c').write_text(source)
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response=OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/cmd/item/cmd-item-utility.c.obj'))))
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    wrapped=('get_rep_dir','confuse_dir','make_attack_reaction','update_combat_rolls1','sdl_player_exchange_begin_direction_prompt','sdl_player_exchange_cancel_direction_prompt','handle_stuff','update_stuff','msg_print')
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g','@CMakeFiles/sil-more.dir/includes_C.rsp',str(OUT/'check.c'),str(OUT/'exchange.c'),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*('-Wl,--wrap='+p for p in wrapped),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=30)


if __name__=='__main__':main()

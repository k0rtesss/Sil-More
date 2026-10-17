#!/usr/bin/env python3
"""Exercise all six production oaths with isolated engine data and real prompts.

Requires a current standard CMake build. No user save or configuration is opened.
The core fixture adapts only the mandatory native curse chooser with a seeded
choice. Curse stacks, prompts, commands, rewards and persistence are production
code. --native also plays unadapted SDL oath selection, prompts and curse menus.
"""
from pathlib import Path
import argparse
import json
import time
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/oath-gameplay"
CHECKS = r'''
#include "player/player-upkeep-internal.h"
#include "metarun.h"
#include "fs/path.h"
#include "fs/io_sdl.h"
#include "quest/quest-runtime.h"
extern metarun* metaruns;
extern s16b metarun_max, current_run;
void reset_defaults(metarun*);
/* Keep production curse stacks and bans; only replace the mandatory native
 * choice screen in this terminal fixture with a deterministic first curse. */
int choose_oath_breaking_curse_ui(int oath) { return (CAMPAIGN_SEED+oath+CAMPAIGN_VARIANT)%z_info->cu_max; }
static const char* keys;
static int events;
static errr oath_input(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT) {
        assert(++events < 120);
        Term_keypress(keys && *keys ? *keys++ : '\r');
    }
    return 0;
}
static void input(cptr sequence)
{
    Term_flush(); keys=sequence; events=0;
}
static void setup(int oath)
{
    memset(p_ptr,0,sizeof(*p_ptr));
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    reset_map(10);
    player_carried_extra_reset_store();player_pack_action_reset();
    Rand_state_init(CAMPAIGN_SEED+oath*97);
    for(int i=0;i<A_MAX;i++) p_ptr->stat_base[i]=3;
    p_ptr->mhp=p_ptr->chp=p_ptr->msp=p_ptr->csp=100;
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    p_ptr->oath_type=oath;
    if (oath) {
        int ability=oath_special_ability_from_oath_num(oath);
        p_ptr->have_ability[S_SPC][ability]=true;
        p_ptr->innate_ability[S_SPC][ability]=true;
        p_ptr->active_ability[S_SPC][ability]=true;
    }
    metar.banned_oaths=metaruns[0].banned_oaths=0;
    memset(metar.curse_stacks,0,sizeof(metar.curse_stacks));
    memset(metaruns[0].curse_stacks,0,sizeof(metaruns[0].curse_stacks));
    min_depth_counter=0; birth_ironman=false;
    p_ptr->energy_use=0; input("");
}
static void check_rewards(void)
{
    int base[A_MAX], base_smith, base_light;
    const int stats[7][A_MAX]={{0},{0,0,0,1},{0,1,0,0},{0,0,1,0},
        {0,0,0,0},{1,0,0,0},{0,0,0,0}};
    setup(0); calc_bonuses(); calc_torch();
    for(int j=0;j<A_MAX;j++) base[j]=p_ptr->stat_misc_mod[j];
    base_smith=p_ptr->skill_misc_mod[S_SMT]; base_light=p_ptr->cur_light;
    for(int oath=1;oath<=OATH_LIGHT;oath++) {
        setup(oath); calc_bonuses(); calc_torch();
        for(int j=0;j<A_MAX;j++)
            assert(p_ptr->stat_misc_mod[j]==base[j]+stats[oath][j]);
        assert(p_ptr->skill_misc_mod[S_SMT]==base_smith+(oath==OATH_SMITH?5:0));
        assert(p_ptr->cur_light==base_light+(oath==OATH_LIGHT?1:0));
        /* Actual equipped boost doubles only the active oath's reward. */
        int kind=lookup_kind(TV_RING,1);
        int boost_slot=CAMPAIGN_VARIANT==1?INVEN_RIGHT:INVEN_LEFT;
        u32b flags=k_info[kind].flags3;
        k_info[kind].flags3 |= TR3_OATH_BOOST;
        object_prep(&inventory[boost_slot],kind);
        calc_bonuses(); calc_torch();
        for(int j=0;j<A_MAX;j++) assert(p_ptr->stat_misc_mod[j]==base[j]+2*stats[oath][j]);
        assert(p_ptr->skill_misc_mod[S_SMT]==base_smith+(oath==OATH_SMITH?10:0));
        assert(p_ptr->cur_light==base_light+(oath==OATH_LIGHT?2:0));
        /* A carried negator suppresses even a boosted oath. */
        int sword=lookup_kind(TV_SWORD,SV_LONG_SWORD);
        u32b sword_flags=k_info[sword].flags3;
        k_info[sword].flags3 |= TR3_OATH_NEGATE;
        int negate_slot=CAMPAIGN_VARIANT==2?INVEN_WIELD:0;
        object_type negator;object_prep(&negator,sword);
        if(CAMPAIGN_VARIANT==1)assert(player_carried_extra_load(&negator));
        else {object_copy(&inventory[negate_slot],&negator);p_ptr->inven_cnt=1;}
        calc_bonuses(); calc_torch();
        for(int j=0;j<A_MAX;j++) assert(p_ptr->stat_misc_mod[j]==base[j]);
        assert(p_ptr->skill_misc_mod[S_SMT]==base_smith);
        assert(p_ptr->cur_light==base_light);
        k_info[sword].flags3=sword_flags; object_wipe(&inventory[negate_slot]);player_carried_extra_reset_store();
        k_info[kind].flags3=flags; object_wipe(&inventory[boost_slot]);
        p_ptr->oaths_broken=1U<<(oath-1);
        calc_bonuses(); calc_torch();
        for(int j=0;j<A_MAX;j++) assert(p_ptr->stat_misc_mod[j]==base[j]);
        assert(p_ptr->skill_misc_mod[S_SMT]==base_smith);
        assert(p_ptr->cur_light==base_light);
    }
    puts("All six actual bonuses, equipped boost, carried negation and loss after breaking PASS.");
}
static void check_attack_oaths(void)
{
    monster_type monster={0}; monster.r_idx=1; monster.ml=true;
    u32b saved=r_info[1].flags3;
    for(int oath=OATH_MERCY;oath<=OATH_VALOROUS;oath+=4) {
        setup(oath); r_info[1].flags3=CAMPAIGN_VARIANT==1?RF3_ELF:RF3_MAN; monster.stance=STANCE_FLEEING;
        assert(oath==OATH_MERCY?merciless_attack(&monster):cowardly_attack(&monster));
        for(int i=0;i<2;i++) {
            input((i+CAMPAIGN_VARIANT)%2?"\033":"n");
            assert(oath==OATH_MERCY?abort_for_mercy(&monster):abort_for_valorous(&monster));
            assert(!p_ptr->oaths_broken && !metar.banned_oaths);
        }
        input("y");
        assert(!(oath==OATH_MERCY?abort_for_mercy(&monster):abort_for_valorous(&monster)));
        if(oath==OATH_MERCY) break_mercy_oath(&monster,0);
        else {break_valorous_oath(&monster,0,ATT_MAIN,-1); break_valorous_oath(&monster,1,ATT_MAIN,1);}
        assert(!p_ptr->oaths_broken);
        input("");
        if(oath==OATH_MERCY) break_mercy_oath(&monster,1);
        else break_valorous_oath(&monster,1,ATT_MAIN,-1);
        assert(oath_invalid(oath));
        assert(!p_ptr->active_ability[S_SPC][oath_special_ability_from_oath_num(oath)]);
        assert(oath_banned(oath));
        /* An already broken oath does not apply another curse. */
        int before=events;
        if(oath==OATH_MERCY) break_mercy_oath(&monster,1);
        else break_valorous_oath(&monster,1,ATT_MAIN,-1);
        assert(events==before);
    }
    r_info[1].flags3=saved;
    puts("Mercy/Valour real attack classification, No/Escape/Yes, no damage, monster attribution, break and no duplicate curse PASS.");
}
static void check_smith(void)
{
    setup(OATH_SMITH);
    object_type found; const int tv[]={TV_SWORD,TV_HELM,TV_BOOTS};
    const int sv[]={SV_LONG_SWORD,SV_HELM,SV_PAIR_OF_LEATHER_BOOTS};
    object_prep(&found,lookup_kind(tv[CAMPAIGN_VARIANT],sv[CAMPAIGN_VARIANT]));
    assert(smith_oath_forbids_object(&found));
    found.unused1=1; assert(!smith_oath_forbids_object(&found)); found.unused1=0;
    for(int i=0;i<2;i++) {input((i+CAMPAIGN_VARIANT)%2?"\033":"n"); assert(!smith_oath_confirm_break()); assert(!p_ptr->oaths_broken);}
    input("y"); assert(smith_oath_confirm_break()); assert(oath_invalid(OATH_SMITH)); assert(oath_banned(OATH_SMITH));
    puts("Smith found/self-made classification, No/Escape/Yes and persistent ban PASS.");
}
static void check_smith_equipping(void)
{
    const int tv[]={TV_SWORD,TV_HELM,TV_BOOTS};
    const int sv[]={SV_LONG_SWORD,SV_HELM,SV_PAIR_OF_LEATHER_BOOTS};
    const int slots[]={INVEN_WIELD,INVEN_HEAD,INVEN_FEET};
    int kind=lookup_kind(tv[CAMPAIGN_VARIANT],sv[CAMPAIGN_VARIANT]);
    for(int answer=0;answer<3;answer++) {
        setup(OATH_SMITH);object_prep(&inventory[0],kind);p_ptr->inven_cnt=1;
        assert(smith_oath_forbids_object(&inventory[0]));
        input(answer==0?"n":answer==1?"\033":"y");
        do_cmd_wield(&inventory[0],0);
        bool paid_pack=player_pack_action_pending();int stages=0;
        while(player_pack_action_pending()) {
            assert(++stages<4);p_ptr->energy_use=0;player_pack_action_process();
        }
        assert((inventory[slots[CAMPAIGN_VARIANT]].k_idx==kind)==(answer==2));
        assert(oath_invalid(OATH_SMITH)==(answer==2));
        assert(oath_banned(OATH_SMITH)==(answer==2));
        if(answer<2)assert(p_ptr->energy_use==(paid_pack?100:0) && inventory[0].k_idx==kind);
    }
    puts("Smith actual foreign sword/helm/boots equip No/Escape/Yes, ownership, paid Pack retrieval and cancellation PASS.");
}
static void check_iron(void)
{
    for(int i=0;i<2;i++) {
        setup(OATH_IRON); cave_feat[2][2]=CAMPAIGN_VARIANT==1?FEAT_LESS_SHAFT:FEAT_LESS; input((i+CAMPAIGN_VARIANT)%2?"\033":"n");
        do_cmd_go_up(); assert(p_ptr->depth==10 && !p_ptr->energy_use && !p_ptr->leaving);
        assert(!p_ptr->oaths_broken && !metar.banned_oaths);
    }
    setup(OATH_IRON); cave_feat[2][2]=CAMPAIGN_VARIANT==1?FEAT_LESS_SHAFT:FEAT_LESS; input("y");
    do_cmd_go_up(); assert(p_ptr->depth==(CAMPAIGN_VARIANT==1?8:9) && p_ptr->leaving); assert(oath_invalid(OATH_IRON));
    puts("Iron actual stairs No/Escape/Yes and break PASS.");
    setup(OATH_IRON);cave_feat[2][2]=FEAT_LESS;
    object_type silmaril;object_prep(&silmaril,lookup_kind(TV_LIGHT,SV_LIGHT_SILMARIL));
    if(CAMPAIGN_VARIANT==1)assert(player_carried_extra_load(&silmaril));
    else object_copy(&inventory[CAMPAIGN_VARIANT==2?INVEN_LITE:0],&silmaril);
    assert(silmarils_possessed()==1);input("");do_cmd_go_up();
    assert(p_ptr->depth==9 && p_ptr->leaving && !p_ptr->oaths_broken && events==0);
    puts("Iron Silmaril in pack/extra/equipment permits actual ascent without curse or prompt PASS.");
}
static void check_silence(void)
{
    for(int i=0;i<3;i++) {
        setup(OATH_SILENCE);
        p_ptr->have_ability[S_SNG][SNG_ELBERETH]=true;
        p_ptr->innate_ability[S_SNG][SNG_ELBERETH]=true;
        p_ptr->active_ability[S_SNG][SNG_ELBERETH]=true;
        input(i==0?"an":i==1?"a\033":"ay");
        do_cmd_change_song();
        assert((p_ptr->song1==SNG_ELBERETH)==(i==2));
        assert(oath_invalid(OATH_SILENCE)==(i==2));
        assert(oath_banned(OATH_SILENCE)==(i==2));
    }
    puts("Silence actual song menu No/Escape/Yes, song activation and ban PASS.");
}
static void check_light(void)
{
    int kind=lookup_kind(TV_SWORD,SV_LONG_SWORD);
    u32b flags=k_info[kind].flags2, flags4=k_info[kind].flags4;
    for(int shadow=0;shadow<2;shadow++) {
    k_info[kind].flags2=flags | (shadow==0?TR2_DARKNESS:0);
    k_info[kind].flags4=flags4 | (shadow==1?TR4_UNLIGHT:0);
    for(int i=0;i<3;i++) {
        setup(OATH_LIGHT); object_prep(&inventory[0],kind); p_ptr->inven_cnt=1;
        input(i==0?"n":i==1?"\033":"y");
        do_cmd_wield(&inventory[0],0);
        assert((inventory[INVEN_WIELD].k_idx==kind)==(i==2));
        assert(oath_invalid(OATH_LIGHT)==(i==2));
        assert(oath_banned(OATH_LIGHT)==(i==2));
        if(i<2) assert(!p_ptr->energy_use && inventory[0].k_idx==kind);
    }
        }
    k_info[kind].flags2=flags; k_info[kind].flags4=flags4;
    puts("Light Darkness/Unlight actual equip No/Escape/Yes, cancelled item/time preservation and ban PASS.");
}
static void check_iron_cancelled_quest(void)
{
    setup(OATH_IRON); cave_feat[2][2]=FEAT_LESS;
    p_ptr->niena_quest=NIENA_QUEST_ACTIVE;
    assert(quest_enabled(QUEST_ID_NIENA));
    input("yn"); do_cmd_go_up();
    assert(p_ptr->depth==10 && !p_ptr->leaving && !p_ptr->energy_use);
    assert(!p_ptr->oaths_broken && !metar.banned_oaths);
    puts("Iron later quest abandonment cancellation preserves oath PASS.");
    setup(OATH_IRON); reset_map(MORGOTH_DEPTH); cave_feat[2][2]=FEAT_LESS;
    p_ptr->morgoth_hall_entered=true;
    input("y"); do_cmd_go_up();
    assert(p_ptr->depth==MORGOTH_DEPTH && !p_ptr->leaving);
    assert(!p_ptr->oaths_broken && !metar.banned_oaths && events==0);
    assert(p_ptr->energy_use==100);
    puts("Iron locked throne staircase preserves oath without warning PASS.");
}
static void check_persistence(void)
{
    setup(0);
    for(int n=0;n<OATH_LIGHT;n++) {
        int oath=(n+CAMPAIGN_VARIANT)%OATH_LIGHT+1;
        metarun_unlock_oath(oath);assert(oath_unlocked(oath));
        byte unlocked=metar.unlocked_oaths;assert(save_metaruns()==0);
        assert(load_metaruns(false)==0);current_run=0;metar=metaruns[0];
        assert(metar.unlocked_oaths==unlocked && !metar.banned_oaths);
    }
    assert(get_available_oaths_mask()==63);
    for(int n=0;n<OATH_LIGHT;n++) {
        int oath=(OATH_LIGHT-n+CAMPAIGN_VARIANT-1)%OATH_LIGHT+1;
        metarun_ban_oath(oath);assert(oath_banned(oath));
        byte banned=metar.banned_oaths;assert(save_metaruns()==0);
        assert(load_metaruns(false)==0);current_run=0;metar=metaruns[0];
        assert(metar.banned_oaths==banned && !(get_available_oaths_mask()&(1<<(oath-1))));
    }
    assert(metar.unlocked_oaths==63 && metar.banned_oaths==63);
    assert(save_metaruns()==0);
    metar.unlocked_oaths=metar.banned_oaths=0;
    memset(metaruns,0,sizeof(*metaruns));
    assert(load_metaruns(false)==0);
    current_run=0; metar=metaruns[0];
    assert(metar.unlocked_oaths==63 && metar.banned_oaths==63);
    assert(get_available_oaths_mask()==0);
    puts("All six unlock/ban bits and real metarun disk save/load roundtrip PASS.");
}
'''


def native_oaths(args, env):
    """Exercise unadapted SDL birth menus, confirmation screens and curse UI."""
    from check_game_control import HARNESS, GameControl
    native = OUT / 'native'
    native.mkdir(parents=True, exist_ok=True)
    prefix=HARNESS[:HARNESS.index('    int last = 0,')]
    prefix=prefix.replace('#include <assert.h>', '#include <assert.h>\n#include "metarun.h"\n#include "player/player-upkeep-internal.h"\nextern metarun* metaruns;\nextern s16b metarun_max,current_run;\nvoid reset_defaults(metarun*);\nextern NavResult select_oath(void);')
    code=prefix+r'''
    setbuf(stdout,NULL);
    metaruns=mem_alloc_array(1,metarun);metarun_max=1;current_run=0;
    rp_ptr=&p_info[0];current_character_profile=&c_info[0];
    for(int n=0;n<6;n++) {
        int expected=(n+CAMPAIGN_VARIANT)%6+1;
        reset_defaults(&metaruns[0]);metar=metaruns[0];
        metar.unlocked_oaths=metaruns[0].unlocked_oaths=63;
        memset(p_ptr,0,sizeof(*p_ptr));turn=playerturn=0;
        Rand_state_init(CAMPAIGN_SEED+n*131);
        printf("NATIVE SELECT %d\n",expected);
        assert(select_oath()==NAV_OK && p_ptr->oath_type==expected);
        int ability=oath_special_ability_from_oath_num(expected);
        assert(p_ptr->active_ability[S_SPC][ability]);
        printf("NATIVE CONFIRM %d\n",expected);
        assert(!get_check_oath_multiline(oath_confirmation_prompt(expected)));
        assert(!get_check_oath_multiline(oath_confirmation_prompt(expected)));
        assert(get_check_oath_multiline(oath_confirmation_prompt(expected)));
        assert(!p_ptr->oaths_broken && !metar.banned_oaths);
        p_ptr->oaths_broken|=1U<<(expected-1);
        apply_oath_breaking_curse(expected);
        assert(!p_ptr->active_ability[S_SPC][ability]);
        assert(oath_banned(expected));
        int curses=0;for(int i=0;i<z_info->cu_max;i++)curses+=metar.curse_stacks[i];
        assert(curses==1);
        printf("NATIVE OATH %d selected, No/Escape/Yes and real curse/ban PASS\n",expected);
    }
    puts("All6 unadapted SDL oath selection, confirmations and curse chooser PASS.");
    sdl_control_shutdown();sdl_quit_hook(NULL);return 0;
}
'''
    source=native/'check-native.c'
    source.write_text(f'#define CAMPAIGN_SEED {args.seed}\n#define CAMPAIGN_VARIANT {args.variant}\n'+code,encoding='utf-8')
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    rsp=native/'objects.rsp';rsp.write_text('\n'.join('"'+obj+'"' for obj in objects if not obj.endswith('/src/main.c.obj')))
    exe=native/'native.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),'@'+str(rsp),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-o',str(exe)],cwd=BUILD,env=env,check=True)
    profile=Path(tempfile.mkdtemp(prefix='profile-',dir=native))
    control=GameControl(profile/'control',timeout=30)
    env=dict(env,SDL_VIDEO_DRIVER='dummy',SDL_RENDER_DRIVER='software',SDL_AUDIO_DRIVER='dummy',CONTROL_TEST_ASSETS=str(ROOT/'lib'),CONTROL_TEST_PROFILE=str(profile),CONTROL_TEST_LOG_BASE=str(profile/'native.exe'))
    actions=[]
    with (native/'native.log').open('w',encoding='utf-8') as log:
        process=subprocess.Popen([str(exe),'--windowed','--tiles','--control-dir',str(control.directory)],cwd=ROOT,env=env,stdout=log,stderr=log)
        try:
            deadline=time.monotonic()+30
            while not (control.directory/'session.json').exists():
                assert process.poll() is None,'Native harness exited during startup'
                assert time.monotonic()<deadline,'Native startup deadline'
                time.sleep(.05)
            def observe(label):
                response=control.observe(output=profile/(label+'.png'))
                assert response['ok'],response
                (profile/(label+'.json')).write_text(json.dumps(response,indent=2))
                return response
            def key(value,label):
                response=control.key(value,output=profile/(label+'.png'))
                assert response['ok'],response
                actions.append({'key':value,'stage':label,'state':response.get('state'),'closed':response.get('closed',False)})
                (profile/(label+'.json')).write_text(json.dumps(response,indent=2))
                return response
            deadline=time.monotonic()+30
            while True:
                assert process.poll() is None,'Native harness exited before selection'
                ready=control.observe(capture=False)
                if ready['state']['waiting_for_input'] and 'NATIVE SELECT' in (native/'native.log').read_text(encoding='utf-8'):break
                assert time.monotonic()<deadline,'Native oath menu never became ready'
                time.sleep(.05)
            observe('initial-selection')
            for n in range(6):
                oath=(n+args.variant)%6+1
                observe(f'{oath}-oath-menu')
                key(chr(ord('a')+oath),f'{oath}-selected-confirm-no')
                key('n',f'{oath}-confirm-escape')
                key('Escape',f'{oath}-confirm-yes')
                key('y',f'{oath}-sundering')
                key('Space',f'{oath}-curse-choices')
                for step in range(args.variant):key('Down',f'{oath}-curse-row-{step+1}')
                key('Enter',f'{oath}-curse-chosen')
                print(f'Native oath{oath} seed{args.seed} completed',flush=True)
            process.wait(timeout=15)
            assert process.returncode==0,(native/'native.log').read_text()
            assert 'All6 unadapted SDL oath' in (native/'native.log').read_text()
        finally:
            if process.poll() is None:process.terminate();process.wait(timeout=10)
            (native/'actions.json').write_text(json.dumps(actions,indent=2))
    print(f'Native SDL evidence: {profile}',flush=True)


def main():
    global OUT
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native',action='store_true',help='Also play unadapted native SDL oath menus and curse choices')
    parser.add_argument('--seed',type=int,default=101)
    parser.add_argument('--variant',type=int,choices=(0,1,2),default=0)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.output: OUT=args.output.resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    init = init.replace("assert(init_oath_info()==0);", "assert(init_quest_info()==0); assert(init_oath_info()==0);")
    source = OUT / "check.c"
    prefix+=f"\n#define CAMPAIGN_SEED {args.seed}\n#define CAMPAIGN_VARIANT {args.variant}\n"
    print(f"Oath campaign seed={args.seed} variant={args.variant}",flush=True)
    source.write_text(prefix + CHECKS + init + r'''
    ANGBAND_DIR_APEX=argv[2];
    static char metadir[1024];
    assert(path_build(metadir,sizeof(metadir),argv[2],"metaruns"));
    assert(SDL_CreateDirectory(metadir)); ANGBAND_DIR_METARUN=metadir;
    metaruns=mem_alloc_array(1,metarun); metarun_max=1; current_run=0;
    reset_defaults(&metaruns[0]); metar=metaruns[0];
    test_term.xtra_hook=oath_input;
    check_rewards(); check_attack_oaths(); check_smith(); check_smith_equipping(); check_iron(); check_silence(); check_light(); check_iron_cancelled_quest(); check_persistence();
    SDL_Quit(); return 0;
}
''', encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    rsp=OUT / "objects.rsp"
    rsp.write_text("\n".join('"'+p+'"' for p in objects if not p.endswith(("/src/main.c.obj", "/src/metarun/metarun-curses.c.obj", "/src/cmd/movement/cmd-movement.c.obj"))))
    env=os.environ.copy()
    env["PATH"]=os.pathsep.join([*(str(BUILD/"_deps"/p) for p in ("SDL","SDL_ttf","SDL_image","SDL_mixer")),"C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    exe=OUT/"check.exe"
    movement=OUT/"movement.obj"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","@CMakeFiles/sil-more.dir/includes_C.rsp","-c",str(ROOT/"src/cmd/movement/cmd-movement.c"),"-o",str(movement)],cwd=BUILD,env=env,check=True)
    curses=OUT/"curses.obj"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","-Dchoose_oath_breaking_curse_ui=native_oath_curse_ui","@CMakeFiles/sil-more.dir/includes_C.rsp","-c",str(ROOT/"src/metarun/metarun-curses.c"),"-o",str(curses)],cwd=BUILD,env=env,check=True)
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","-g","@CMakeFiles/sil-more.dir/includes_C.rsp",str(source),str(curses),str(movement),"@"+str(rsp),"@CMakeFiles/sil-more.dir/linkLibs.rsp","-o",str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix="data-",dir=OUT) as data:
        subprocess.run([str(exe),str(ROOT/"lib/edit"),data],cwd=data,env=env,check=True,timeout=60)
    if args.native:native_oaths(args,env)

if __name__ == "__main__":
    main()



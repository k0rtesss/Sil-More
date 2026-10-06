#!/usr/bin/env python3
"""Run actual death knowledge/reveal and identification without player files.

Production ending helpers are source-included to expose their static entry
points. The minimal term omits Final Look pane repaint but runs production lore;
message wrappers record reward announcements without displaying modal prompts.
Use --baseline, optionally --case reveal or lore, to demonstrate old failures.
"""
from pathlib import Path
import argparse, os, shlex, subprocess, tempfile
import check_new_monsters as engine
ROOT, BUILD = engine.ROOT, engine.BUILD
OUT=ROOT/'scripts/output/postmortem-identification'
CHECKS=r'''
#undef assert
#define assert(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); } } while(0)
static int reward_messages;
static bool skip_dead_probe;
void __wrap_msg_print(cptr message) { (void)message; }
void __wrap_msg_format(cptr format, ...) {
    if (strstr(format,"experience is won")) reward_messages++;
}
extern bool death_spectator_mode;
extern void update_lore_aux(object_type* o_ptr);
extern void update_lore(u32b flags);
extern void __real_handle_stuff(void);
/* The bare term has no SDL pane runtime; keep the real lore update, omit repaint. */
void __wrap_handle_stuff(void) {
    if (death_spectator_active()) {
        p_ptr->update=0; p_ptr->redraw=0; p_ptr->window=0;
        update_lore(PU_UPDATE_VIEW);
    } else __real_handle_stuff();
}
extern void fixture_death_knowledge(void);
extern void fixture_final_reveal(void);
static void seed(int mode) {
    reset_map(3); memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store();
    character_generated=true; character_xtra=false; character_icky=0;
    p_ptr->is_dead=mode==1; p_ptr->leaving=mode==1 || mode==3;
    death_spectator_mode=mode==2; p_ptr->playing=mode!=1;
    p_ptr->exp=5899; p_ptr->new_exp=499; p_ptr->ident_exp=0;
    p_ptr->insight_points=p_ptr->insight_milestones=0;
    reward_messages=0; p_ptr->energy_use=0; playerturn=817; turn=8591;
    int kind=lookup_kind(TV_POTION,SV_POTION_HEALING); assert(kind);
    object_prep(&inventory[0],kind); k_info[kind].aware=false;
    object_prep(&o_list[1],lookup_kind(TV_POTION,SV_POTION_VOICE));
    k_info[o_list[1].k_idx].aware=false; o_list[1].iy=o_list[1].ix=3;
    cave_o_idx[3][3]=1; o_max=2; o_cnt=1;
}
static void unchanged(void) {
    if(p_ptr->exp!=5899) fprintf(stderr,"observed XP %ld/%ld, identification %ld\n",(long)p_ptr->new_exp,(long)p_ptr->exp,(long)p_ptr->ident_exp);
    assert(p_ptr->exp==5899 && p_ptr->new_exp==499 && p_ptr->ident_exp==0);
    assert(reward_messages==0);
    assert(p_ptr->insight_points==0 && p_ptr->insight_milestones==0);
    assert(playerturn==817 && turn==8591 && p_ptr->energy_use==0);
}
static void check_death_reveal(void) {
    seed(1); fixture_death_knowledge();
    assert(object_known_p(&inventory[0]) && object_aware_p(&inventory[0])); unchanged();
    if (!skip_dead_probe) { p_ptr->leaving=false; object_prep(&inventory[1],lookup_kind(TV_POTION,SV_POTION_QUICKNESS));
    k_info[inventory[1].k_idx].aware=false; ident(&inventory[1]); unchanged(); }
    seed(1); fixture_final_reveal();
    assert(object_known_p(&o_list[1]) && object_aware_p(&o_list[1])); unchanged();
    object_aware(&inventory[0]); unchanged();
    puts("Dead carried reveal and live-flag Final Look floor/awareness: no XP or actions PASS");
}
static void check_lore_rewards(void) {
    for(int mode=0;mode<4;mode++) {
        seed(mode); p_ptr->leaving=mode==3;
        object_type item; object_prep(&item,lookup_kind(TV_ARROW,SV_NORMAL_ARROW));
        item.name2=169; object_known(&item); e_info[169].aware=false;
        update_lore_aux(&item);
        if(mode) unchanged(); else assert(p_ptr->exp==5974 && p_ptr->ident_exp==75);
        object_prep(&item,lookup_kind(a_info[119].tval,a_info[119].sval));
        item.name1=119; apply_magic(&item,-1,true,true,true,true); object_known(&item);
        a_info[119].found_num=0; op_ptr->opt[OPT_insight_beta]=true;
        update_lore_aux(&item);
        if(mode) { unchanged(); assert(a_info[119].found_num==0); }
        else { assert(p_ptr->ident_exp==175 && a_info[119].found_num==1); assert(p_ptr->insight_points>0); }
        op_ptr->opt[OPT_insight_beta]=false;
    }
    seed(0); character_icky=2; object_aware(&inventory[0]);
    assert(p_ptr->exp==5974 && p_ptr->new_exp==574 && p_ptr->ident_exp==75);
    puts("Living overlay awards preserved; dead/spectator ego and artefact rewards suppressed PASS");
}
static void check_awareness_controls(void) {
    for(int reason=0;reason<3;reason++) {
        seed(0);
        if(reason==0) p_ptr->leaving=true;
        if(reason==1) character_generated=false;
        if(reason==2) character_xtra=true;
        object_aware(&inventory[0]);
        assert(object_aware_p(&inventory[0])); unchanged();
    }
    character_generated=true; character_xtra=false;
    puts("Existing leaving, birth and temporary projection awareness controls preserved PASS");
}

'''

def main():
    args=argparse.ArgumentParser(); args.add_argument("--baseline",action="store_true"); args.add_argument("--case",choices=["all","reveal","lore"],default="all"); options=args.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    prefix,body=engine.HARNESS.split('int main(int argc,char** argv)',1)
    setup=body.split('    check_templates();',1)[0]
    setup=setup.replace('assert(init_a_info()==0);', 'assert(init_flavor_info()==0); flavor_init(); assert(init_a_info()==0);')
    calls={'all':'check_death_reveal(); check_lore_rewards(); check_awareness_controls();','reveal':'skip_dead_probe=true; check_death_reveal();','lore':'check_lore_rewards();'}[options.case]
    source=OUT/'check.c'; source.write_text(prefix+CHECKS+'int main(int argc,char** argv)'+setup+calls+' SDL_Quit(); return 0; }\n')
    changed=['src/object/object-knowledge.c','src/player/player-lore.c','src/dungeon/dungeon-startup.c','src/dungeon/dungeon-spectator.c']
    wrappers=[]
    for path,call in [(changed[2],'death_knowledge'),(changed[3],'death_spectator_prepare_display')]:
        wrap=OUT/(Path(path).stem+'-fixture.c')
        name='fixture_death_knowledge' if call=='death_knowledge' else 'fixture_final_reveal'
        statement=call+'();'
        if name=='fixture_final_reveal':
            statement='death_spectator_mode=true; death_spectator_game_state old=death_spectator_enter_game_ui(); death_spectator_prepare_display(); death_spectator_leave_game_ui(&old); death_spectator_mode=false;'
        wrap.write_text('#include "'+(ROOT/path).as_posix()+'"\nvoid '+name+'(void) { '+statement+' }\n'); wrappers.append(str(wrap))
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response=OUT/'objects.rsp'; response.write_text('\n'.join('"'+o+'"' for o in objects if not any(o.endswith('/'+p+'.obj') for p in ['src/main.c',*changed])))
    env=os.environ.copy(); env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/n) for n in ['SDL','SDL_ttf','SDL_image','SDL_mixer']),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    production=[]
    for path in changed[:2]:
        if options.baseline:
            dst=OUT/(Path(path).stem+'-baseline.c')
            dst.write_bytes(subprocess.check_output(['git','-c','safe.directory='+ROOT.as_posix(),'show','HEAD:'+path],cwd=ROOT))
            production.append(str(dst))
        else: production.append(str(ROOT/path))
    exe=OUT/('baseline.exe' if options.baseline else 'check.exe')
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),*production,*wrappers,'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-Wl,--wrap=handle_stuff','-Wl,--wrap=msg_print','-Wl,--wrap=msg_format','-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='data-',dir=OUT) as data: subprocess.run([str(exe),str(ROOT/'lib/edit'),data],cwd=data,env=env,check=True,timeout=45)
if __name__=='__main__': main()

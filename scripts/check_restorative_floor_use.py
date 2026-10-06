#!/usr/bin/env python3
"""Drive production supply menus and restorative floor consumption.

Only input, messages, tutorials and frontend redraw are isolated; real request
routing, confirmation, item use, effects, awareness and stack mutation execute.
World clocks remain unchanged until the caller spends the returned action energy.
--baseline demonstrates the old floor Pick up confirmation in a USE context.
"""
import argparse, os, shlex, subprocess, tempfile
from pathlib import Path
import check_new_monsters as engine
ROOT, BUILD = engine.ROOT, engine.BUILD
OUT = ROOT / "scripts/output/restorative-floor-use"
CHECKS = r'''
#include "supplies.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"FAIL %s at %d\n",#expr,__LINE__); exit(1); } } while(0)
static int input_index, confirmations, quantity_calls;
static bool accept;
static cptr expected_action;
static const char* inputs;
char __wrap_inkey(void) {
    assert(input_index<4);
    char key=inputs[input_index++];
    return key ? key : ESCAPE;
}
void __wrap_message_flush(void) {}
void __wrap_msg_print(cptr text) { (void)text; }
void __wrap_msg_format(cptr format, ...) { (void)format; }
void __wrap_handle_stuff(void) {}
void __wrap_redraw_stuff(void) {}
void __wrap_tutorial_game_menu(const char* id, const char* detail) { (void)id;(void)detail; }
bool __wrap_tutorial_game_action_allowed(const char* action,const object_type* item) { (void)action;(void)item;return true; }
bool __wrap_get_check(cptr prompt) {
    confirmations++;
    if(strncmp(prompt,expected_action,strlen(expected_action))) {
        fprintf(stderr,"expected %s, got %s\n",expected_action,prompt);exit(1);
    }
    return accept;
}
s16b __wrap_get_quantity(cptr prompt,int max) { (void)prompt;assert(max>=1);quantity_calls++;return 1; }
static void prepare(int tval,int sval) {
    reset_map(1); memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();
    memset(p_ptr->active_ability,0,sizeof(p_ptr->active_ability));
    p_ptr->playing=true;p_ptr->leaving=false;p_ptr->is_dead=false;
    character_generated=true;character_xtra=false;character_icky=0;
    p_ptr->exp=5000;p_ptr->new_exp=800;p_ptr->ident_exp=0;
    p_ptr->chp=10;p_ptr->mhp=50;p_ptr->csp=10;p_ptr->msp=59;
    p_ptr->stat_base[A_DEX]=5;p_ptr->stat_drain[A_DEX]=-1;
    p_ptr->food=5000;p_ptr->energy_use=0;p_ptr->update=0;p_ptr->redraw=0;p_ptr->window=0;
    turn=101;playerturn=10;
    int kind=lookup_kind(tval,sval);assert(kind);
    object_prep(&o_list[1],kind);o_list[1].number=2;o_list[1].iy=p_ptr->py;o_list[1].ix=p_ptr->px;o_list[1].marked=true;
    cave_o_idx[p_ptr->py][p_ptr->px]=1;o_max=2;o_cnt=1;k_info[kind].aware=false;
    input_index=confirmations=quantity_calls=0;
}
static void menu(supply_menu_action action,int group,bool yes,cptr label) {
    accept=yes;expected_action=label;inputs="-\r\033";input_index=confirmations=0;
    (void)open_supplies_menu_with_context(action,group,true,true);
    assert(turn==101 && playerturn==10 && confirmations==1);
    assert(item_tester_tval==0);
}
static void check_floor_restoratives(void) {
    int kinds[]={SV_POTION_HEALING,SV_POTION_VOICE};
    for(int i=0;i<2;i++) {
        prepare(TV_POTION,kinds[i]);
        menu(SUPPLY_MENU_ACTION_USE,SUPPLY_GROUP_POTIONS,false,"Quaff ");
        assert(o_list[1].number==2 && p_ptr->energy_use==0);
        assert(!k_info[o_list[1].k_idx].aware);
        assert(p_ptr->chp==10 && p_ptr->csp==10 && p_ptr->exp==5000);
        menu(SUPPLY_MENU_ACTION_USE,SUPPLY_GROUP_POTIONS,true,"Quaff ");
        assert(o_list[1].number==1 && supplies_entry_count()==0 && p_ptr->energy_use==100);
        assert(k_info[o_list[1].k_idx].aware && p_ptr->exp==5075 && p_ptr->ident_exp==75);
        if(i==0) assert(p_ptr->chp==33 && p_ptr->csp==10);
        else assert(p_ptr->csp==59 && p_ptr->chp==10);
    }
    prepare(TV_FOOD,SV_FOOD_RESTORATION);
    menu(SUPPLY_MENU_ACTION_USE,SUPPLY_GROUP_HERBS,true,"Eat ");
    assert(o_list[1].number==1 && p_ptr->stat_drain[A_DEX]==0);
    assert(p_ptr->exp==5075 && p_ptr->energy_use==100 && supplies_entry_count()==0);
    puts("Floor Healing/Voice/Restoration: real USE, free cancellation, one consumed, effects and75XP,100energy PASS");
}
static void check_browse_and_drop(void) {
    prepare(TV_POTION,SV_POTION_HEALING);
    menu(SUPPLY_MENU_ACTION_NONE,SUPPLY_GROUP_POTIONS,false,"Pick up ");
    assert(o_list[1].number==2 && p_ptr->energy_use==0 && p_ptr->chp==10);
    menu(SUPPLY_MENU_ACTION_NONE,SUPPLY_GROUP_POTIONS,true,"Pick up ");
    assert(!o_list[1].k_idx && supplies_entry_count()==1 && supplies_entry_at(0)->number==2);
    assert(p_ptr->chp==10 && p_ptr->exp==5000);
    p_ptr->energy_use=0;inputs="\r\r\033";input_index=confirmations=0;expected_action="Drop ";accept=false;
    (void)open_supplies_menu_with_context(SUPPLY_MENU_ACTION_DROP,SUPPLY_GROUP_POTIONS,true,true);
    assert(confirmations==1 && quantity_calls==0);
    assert(supplies_entry_at(0)->number==2 && p_ptr->energy_use==0 && p_ptr->exp==5000);
    assert(turn==101 && playerturn==10);
    puts("Ordinary browser remains Pick up; Drop cancellation and quantity preserved PASS");
}
'''

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--baseline",action="store_true");args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    prefix,body=engine.HARNESS.split("int main(int argc,char** argv)",1)
    setup=body.split("    check_templates();",1)[0].replace("assert(init_a_info()==0);","assert(init_flavor_info()==0); flavor_init(); assert(init_a_info()==0);")
    source=OUT/"check.c";source.write_text(prefix+CHECKS+"int main(int argc,char** argv)"+setup+"check_floor_restoratives();check_browse_and_drop();SDL_Quit();return 0;}\n")
    changed="src/cmd/item/cmd-item-core.c";production=ROOT/changed
    if args.baseline:
        production=OUT/"core-baseline.c"
        original=subprocess.check_output(["git","-c","safe.directory="+ROOT.as_posix(),"show","HEAD:"+changed],cwd=ROOT).decode("utf-8")
        current=(ROOT/changed).read_text(encoding="utf-8")
        start="bool open_supplies_menu_with_context("
        end="\nbool open_inventory_menu_page("
        old_function=original[original.index(start):original.index(end,original.index(start))]
        begin=current.index(start);finish=current.index(end,begin)
        # Restore only the defective request initialization, retaining current
        # unrelated production helpers required by the rebuilt menu objects.
        production.write_text(current[:begin]+old_function+current[finish:],encoding="utf-8")
    objects=shlex.split((BUILD/"CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response=OUT/"objects.rsp";response.write_text("\n".join('"'+o+'"' for o in objects if not any(o.endswith('/'+p+'.obj') for p in ['src/main.c',changed,'src/player/player-active-weapon.c'])))
    env=os.environ.copy();env["PATH"]=os.pathsep.join([*(str(BUILD/"_deps"/n) for n in ['SDL','SDL_ttf','SDL_image','SDL_mixer']),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env["PATH"]])
    exe=OUT/("baseline.exe" if args.baseline else "check.exe")
    wraps=['inkey','message_flush','msg_print','msg_format','handle_stuff','redraw_stuff','tutorial_game_menu','tutorial_game_action_allowed','get_check','get_quantity']
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(production),str(ROOT/'src/player/player-active-weapon.c'),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*("-Wl,--wrap="+n for n in wraps),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix="data-",dir=OUT) as data: subprocess.run([str(exe),str(ROOT/'lib/edit'),data],cwd=data,env=env,check=True,timeout=20)
if __name__=="__main__":main()

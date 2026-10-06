#!/usr/bin/env python3
"""Real Pack equip dispatch, deferred transfer and readiness; isolated frontend.

Start and both production advances must each request100 energy. Confirmation,
tutorials and redraw are test callbacks; wield/item/mode/Pack policies are real.
Baseline removes only pending readiness/completion behavior, retaining offhand.
"""
import argparse, os, shlex, subprocess, tempfile
import check_new_monsters as engine
ROOT, BUILD = engine.ROOT, engine.BUILD
OUT = ROOT / 'scripts/output/pack-equipment-readiness'
CHECKS = r'''
#include "supplies.h"
#undef assert
#define assert(c) do { if (!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);exit(1); } } while(0)
extern bool fixture_equip(int item,int slot);
extern void player_pack_action_process(void);
void __wrap_handle_stuff(void) {}
void __wrap_update_stuff(void) {}
void __wrap_redraw_stuff(void) {}
void __wrap_message_flush(void) {}
void __wrap_msg_print(cptr t) {(void)t;}
void __wrap_msg_format(cptr f, ...) {(void)f;}
bool __wrap_get_check(cptr p) {(void)p;return true;}
bool __wrap_tutorial_game_action_allowed(const char* a,const object_type* o) {(void)a;(void)o;return true;}
void __wrap_tutorial_game_explain(const char* i,const char* t,const char* d) {(void)i;(void)t;(void)d;}
static object_type initial;
static int old_mode;
static void prepare(int slot,bool shield) {
 reset_map(1);memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
 player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();player_pack_action_reset();player_active_weapon_begin_player_turn();
 memset(p_ptr->active_ability,0,sizeof(p_ptr->active_ability));memset(p_ptr->have_ability,0,sizeof(p_ptr->have_ability));
 int t=slot==INVEN_BOW?TV_BOW:shield?TV_SHIELD:TV_SWORD;
 int s=slot==INVEN_BOW?SV_LONG_BOW:shield?SV_ROUND_SHIELD:SV_DAGGER;
 object_prep(&inventory[0],lookup_kind(t,s));inventory[0].number=2;inventory[0].storage=OBJECT_STORAGE_PACK;
 object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_SHORT_SWORD));inventory[INVEN_WIELD].number=1;inventory[INVEN_WIELD].storage=OBJECT_STORAGE_HARNESS;
 object_prep(&inventory[INVEN_BOW],lookup_kind(TV_BOW,SV_SHORT_BOW));inventory[INVEN_BOW].number=1;inventory[INVEN_BOW].storage=OBJECT_STORAGE_HARNESS;
 object_type arrows;object_prep(&arrows,lookup_kind(TV_ARROW,SV_NORMAL_ARROW));arrows.number=5;assert(player_quiver_absorb_arrow(&arrows)==5);
 p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]=true;
 p_ptr->active_ability[S_ARC][ARC_POINT_BLANK]=shield;
 p_ptr->active_ability[S_ARC][ARC_VERSATILITY]=true;
 p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK]=true;
 old_mode=slot==INVEN_BOW?PLAYER_ACTIVE_WEAPON_MELEE:PLAYER_ACTIVE_WEAPON_RANGED_1;
 p_ptr->active_weapon_mode=old_mode;character_generated=true;character_xtra=false;character_icky=0;
 p_ptr->playing=true;p_ptr->leaving=false;p_ptr->is_dead=false;p_ptr->energy_use=0;
 p_ptr->total_weight=inventory[0].weight*2+inventory[INVEN_WIELD].weight+inventory[INVEN_BOW].weight;
 object_copy(&initial,&inventory[0]);
}
static void pending(int slot) {
 assert(player_pack_action_pending() && player_pack_action_turns_left()==2 && p_ptr->energy_use==100);
 assert(p_ptr->active_weapon_mode==old_mode && !memcmp(&initial,&inventory[0],sizeof(initial)));
 if(slot==INVEN_ARM) assert(!inventory[INVEN_ARM].k_idx);
}
static void finish(void) {
 player_pack_action_process();assert(player_pack_action_pending() && player_pack_action_turns_left()==1 && p_ptr->energy_use==100);
 assert(p_ptr->active_weapon_mode==old_mode && inventory[0].number==2);
 player_pack_action_process();assert(!player_pack_action_pending() && p_ptr->energy_use==100);
}
static void checks(void) {
 for(int i=0;i<2;i++) { prepare(INVEN_ARM,false);assert(fixture_equip(0,INVEN_ARM));pending(INVEN_ARM);
  if(i)player_pack_action_interrupt();else player_pack_action_cancel();
  assert(!player_pack_action_pending() && p_ptr->active_weapon_mode==old_mode && !inventory[INVEN_ARM].k_idx);
  assert(!memcmp(&initial,&inventory[0],sizeof(initial)));
 }
 puts("Pending/canceled/interrupted Off-hand keeps Bow and exact Pack stack PASS");
 int slots[]={INVEN_ARM,INVEN_WIELD,INVEN_BOW};
 for(int i=0;i<3;i++) { int slot=slots[i];prepare(slot,false);assert(fixture_equip(0,slot));pending(slot);int kind=inventory[0].k_idx;finish();
  assert(inventory[slot].k_idx==kind && inventory[slot].number==1);
  int n=0;for(int j=0;j<player_pack_entry_count();j++) {object_type* o=player_pack_entry_at(j);if(o->k_idx==kind)n+=o->number;}assert(n==1);
  assert(p_ptr->active_weapon_mode==(slot==INVEN_BOW?PLAYER_ACTIVE_WEAPON_RANGED_1:PLAYER_ACTIVE_WEAPON_MELEE));
  if(slot==INVEN_ARM) assert(player_active_weapon_change_is_free(PLAYER_ACTIVE_WEAPON_KIND_BOW,PLAYER_ACTIVE_WEAPON_KIND_MELEE));
 }
 prepare(INVEN_ARM,true);assert(fixture_equip(0,INVEN_ARM));pending(INVEN_ARM);finish();
 assert(inventory[INVEN_ARM].tval==TV_SHIELD && inventory[INVEN_ARM].number==1 && p_ptr->active_weapon_mode==old_mode && player_equipment_slot_is_active(INVEN_ARM));
 puts("ARM/WIELD/BOW: three100energy payments and one transfer; active Shortbow shield keeps Bow PASS");
 prepare(INVEN_ARM,false);assert(fixture_equip(0,INVEN_ARM));pending(INVEN_ARM);
 p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]=false;finish();
 assert(!inventory[INVEN_ARM].k_idx && inventory[0].number==2 && p_ptr->active_weapon_mode==old_mode);
 prepare(INVEN_WIELD,false);assert(do_cmd_wield_stack_to_slot(&inventory[0],0,INVEN_WIELD));pending(INVEN_WIELD);int kind=inventory[0].k_idx;finish();
 assert(inventory[INVEN_WIELD].k_idx==kind && inventory[INVEN_WIELD].number==2 && p_ptr->active_weapon_mode==old_mode);
 int n=0;for(int i=0;i<player_pack_entry_count();i++) {object_type* o=player_pack_entry_at(i);if(o->k_idx==kind)n+=o->number;}assert(n==0);
 puts("Failed completion preserves mode/stock; full Throwing stack remains ranged PASS");
}
'''

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--baseline',action='store_true');args=parser.parse_args()
 OUT.mkdir(parents=True,exist_ok=True)
 ui=(ROOT/'src/cmd/ui/cmd-ui-knowledge.c').read_text();pack=(ROOT/'src/player/player-pack-action.c').read_text()
 if args.baseline:
  pos=ui.index('static bool equipment_ready_after_wield(');end=ui.index('static cptr equipment_menu_use_action_text',pos)
  function=ui[pos:end];guard='    if (player_pack_action_pending())\n        return true;\n';assert guard in function
  ui=ui[:pos]+function.replace(guard,'',1)+ui[end:]
  original=subprocess.check_output(['git','-c','safe.directory='+ROOT.as_posix(),'show','HEAD:src/player/player-pack-action.c'],cwd=ROOT).decode()
  start='    case PLAYER_PACK_ACTION_WIELD:';end='    case PLAYER_PACK_ACTION_TAKEOFF:';pos=pack.index(start);stop=pack.index(end,pos)
  pack=pack[:pos]+original[original.index(start):original.index(end,original.index(start))]+pack[stop:]
 ui_file=OUT/'ui-source.c';ui_file.write_text(ui);pack_file=OUT/'pack.c';pack_file.write_text(pack)
 wrapper=OUT/'ui.c';wrapper.write_text('#include "'+ui_file.as_posix()+'"\nbool fixture_equip(int item,int slot) {equipment_list_entry e;equipment_entry_clear(&e);e.item_idx=item;return equipment_menu_use_entry(&e,slot,SUPPLY_FLOOR_ACTION_DEFAULT);}\n')
 prefix,body=engine.HARNESS.split('int main(int argc,char** argv)',1)
 setup=body.split('    check_templates();',1)[0].replace('assert(init_a_info()==0);','assert(init_flavor_info()==0);flavor_init();assert(init_a_info()==0);')
 source=OUT/'check.c';source.write_text(prefix+CHECKS+'int main(int argc,char** argv)'+setup+'checks();SDL_Quit();return 0;}\n')
 objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text());response=OUT/'objects.rsp'
 response.write_text('\n'.join('"'+o+'"' for o in objects if not any(o.endswith('/'+p+'.obj') for p in ['src/main.c','src/cmd/ui/cmd-ui-knowledge.c','src/player/player-pack-action.c'])))
 env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/n) for n in ['SDL','SDL_ttf','SDL_image','SDL_mixer']),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
 wraps=['handle_stuff','update_stuff','redraw_stuff','message_flush','msg_print','msg_format','get_check','tutorial_game_action_allowed','tutorial_game_explain']
 exe=OUT/('baseline.exe' if args.baseline else 'check.exe')
 subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(wrapper),str(pack_file),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*('-Wl,--wrap='+w for w in wraps),'-o',str(exe)],cwd=BUILD,env=env,check=True)
 with tempfile.TemporaryDirectory(prefix='data-',dir=OUT) as data:subprocess.run([str(exe),str(ROOT/'lib/edit'),data],env=env,cwd=data,check=True,timeout=20)
if __name__=='__main__':main()

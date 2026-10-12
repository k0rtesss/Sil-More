#!/usr/bin/env python3
"""Exercise real Inventory focus through terminal layout changes.

Only input, confirmation and preview output boundaries are injected. Real
item collections, registered pointer rows and browser dispatch run.
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
OUT=ROOT/'scripts/output/inventory-layout-focus'

CHECKS=r'''
#include "supplies.h"
#include "sdl-config.h"
#include "cmd/ui/cmd-ui-internal.h"
#include "ui/menu-click.h"
static int stage,previews,expected_supplies;static object_type* wanted;static cptr wanted_text;
static bool small_first;
void __wrap_message_flush(void) {}
void __wrap_handle_stuff(void) {}
bool __wrap_get_check(cptr prompt) {(void)prompt;return false;}
bool __wrap_object_info_overlay_show_multi(const object_type** objects,const char** headings,int count)
{(void)headings;assert(count>0);if(objects[0]!=wanted)fprintf(stderr,"Wrong focused object stage%d: wanted%d got%d\n",stage,wanted->k_idx,objects[0]->k_idx);assert(objects[0]==wanted);previews++;return true;}
static void resize_term(int h) {if(Term->wid!=96||Term->hgt!=h)assert(!Term_resize(96,h));}
static void select_row(void)
{
 for(int y=0;y<Term->hgt;y++) {char row[256];memcpy(row,Term->scr->c[y],Term->wid);row[Term->wid]=0;
  if(!strstr(row,wanted_text))continue;
  for(int x=(int)(strstr(row,wanted_text)-row);x<Term->wid;x++)if(ui_menu_click_has_cell(x,y)) {
   assert(ui_menu_click_handle_cell_action(x,y,UI_MENU_CLICK_SECONDARY));return;
  }
 }
 fprintf(stderr,"Missing item row %s stage%d\n",wanted_text,stage);assert(false);
}
char __wrap_inkey(void)
{
 assert(supplies_entry_count()==expected_supplies);
 for(int y=0;y<Term->hgt;y++){char row[256];memcpy(row,Term->scr->c[y],Term->wid);row[Term->wid]=0;assert(!strstr(row,"Potion"));}
 switch(stage++) {
 case 0:select_row();return UI_MENU_CLICK_WAKE_KEY;
 case 1:assert(previews>0);resize_term(small_first?36:18);return UI_MENU_CLICK_WAKE_KEY;
 case 2:assert(previews>1);resize_term(small_first?18:36);return UI_MENU_CLICK_WAKE_KEY;
 case 3:assert(previews>2);return ESCAPE;
 default:assert(false);return ESCAPE;
 }
}
static void prep(object_type* o,int tv,int sv)
{object_prep(o,lookup_kind(tv,sv));assert(o->k_idx);o->number=1;object_known(o);object_aware(o);}
static void setup(bool pack)
{
 memset(p_ptr,0,sizeof(*p_ptr));memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
 player_quiver_reset_store();player_carried_extra_reset_store();supplies_reset_store();player_pack_action_reset();
 p_ptr->py=p_ptr->px=2;p_ptr->playing=true;p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;p_ptr->chp=p_ptr->mhp=100;p_ptr->csp=42;
 rp_ptr=&p_info[0];current_character_profile=&c_info[0];mon_max=1;o_max=2;
 for(int y=0;y<20;y++)for(int x=0;x<20;x++){cave_feat[y][x]=FEAT_FLOOR;cave_o_idx[y][x]=cave_m_idx[y][x]=0;}
 cave_m_idx[2][2]=-1;prep(&inventory[INVEN_WIELD],TV_SWORD,SV_SHORT_SWORD);
 prep(&inventory[0],TV_DIGGING,SV_MATTOCK);inventory[0].storage=OBJECT_STORAGE_HARNESS;
 prep(&o_list[1],TV_STAFF,SV_STAFF_IMPRISONMENT);o_list[1].marked=true;o_list[1].iy=o_list[1].ix=2;o_list[1].next_o_idx=0;cave_o_idx[2][2]=1;
 object_type arrows;prep(&arrows,TV_ARROW,SV_NORMAL_ARROW);assert(player_quiver_absorb_arrow(&arrows)>=0);
 if(pack){prep(&inventory[1],TV_CLOAK,SV_CLOAK);inventory[1].storage=OBJECT_STORAGE_PACK;object_type potion;prep(&potion,TV_POTION,SV_POTION_HEALING);assert(supplies_absorb_object(&potion));}
 expected_supplies=pack;turn=1001;playerturn=100;p_ptr->energy_use=0;stage=previews=0;set_sdl_input_ui_mode(SDL_INPUT_UI_MODE_PLATFORM);
}
static void checks(void)
{
 for(int pack=0;pack<2;pack++)for(int direction=0;direction<2;direction++)for(int kind=0;kind<(pack?5:4);kind++) {
  setup(pack);small_first=direction;resize_term(small_first?18:36);
  supply_menu_request r={0};r.focus_page=true;r.page=SUPPLY_MENU_PAGE_INVENTORY;
  r.focus_inventory_group=true;
  if(kind==0){wanted=&o_list[1];wanted_text="Staff";r.inventory_group=INVENTORY_MENU_GROUP_HARNESS;}
  else if(kind==1){wanted=&inventory[INVEN_WIELD];wanted_text="Shortsword";r.inventory_group=INVENTORY_MENU_GROUP_HARNESS;}
  else if(kind==2){wanted=&inventory[0];wanted_text="Mattock";r.inventory_group=INVENTORY_MENU_GROUP_HARNESS;}
  else if(kind==3){int slots[10];assert(player_quiver_arrow_slots(slots,10)>0);wanted=player_quiver_arrow_object(slots[0]);wanted_text="Arrow";r.inventory_group=INVENTORY_MENU_GROUP_QUIVER;}
  else {wanted=&inventory[1];wanted_text="Cloak";r.inventory_group=INVENTORY_MENU_GROUP_PACK;}
  (void)do_cmd_knowledge_supplies(&r);assert(stage==4&&previews>=3);assert(supplies_entry_count()==pack);
  assert(turn==1001&&playerturn==100&&!p_ptr->energy_use&&p_ptr->chp==100&&p_ptr->csp==42&&!player_pack_action_pending());
 }
 puts("Inventory real browser one-page/category transitions: floor/equipped/carried/quiver identities, both directions, empty/nonempty Pack and free clocks PASS");
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--baseline',action='store_true')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):];init=init[:init.index('    check_templates();')]
    init=init.replace('term_init(&test_term,80,24,256)','term_init(&test_term,96,36,256)')
    (OUT/'check.c').write_text(prefix+CHECKS+init+'assert(!init_flavor_info());flavor_init();checks();SDL_Quit();return 0;\n}\n',encoding='utf-8')
    browser=(ROOT/'src/cmd/ui/cmd-ui-knowledge.c').read_text()
    if args.baseline:
        old='bool preserve_layout_focus = ordinary_inventory';assert browser.count(old)==1
        browser=browser.replace(old,'bool preserve_layout_focus = false && ordinary_inventory')
    (OUT/'browser.c').write_text(browser,encoding='utf-8')
    cmake=BUILD/'CMakeFiles/sil-more.dir';objects=shlex.split((cmake/'objects1.rsp').read_text())
    (OUT/'objects.rsp').write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/cmd/ui/cmd-ui-knowledge.c.obj'))),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe';subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(OUT/'check.c'),str(OUT/'browser.c'),'@'+str(OUT/'objects.rsp'),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*(f'-Wl,--wrap={name}' for name in ('inkey','get_check','message_flush','handle_stuff','object_info_overlay_show_multi')),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=20)

if __name__=='__main__':main()

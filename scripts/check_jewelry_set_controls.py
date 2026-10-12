#!/usr/bin/env python3
"""Exercise the real Jewelry Sets browser keys and registered footer cells.

Only inkey/name/confirmation boundaries are injected. Real preset storage,
browser dispatch, footer hit registration and pointer choice handling run.
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
OUT=ROOT/'scripts/output/jewelry-set-controls'

CHECKS=r'''
#include "supplies.h"
#include "sdl-config.h"
#include "cmd/ui/cmd-ui-internal.h"
#include "ui/menu-click.h"
static int keys[20],key_count,key_pos,confirmations,names,previews,preview_kind;
static int group=SUPPLY_GROUP_JEWELRY_PRESETS;
static bool answer;
static char prompts[8][160];
char __wrap_inkey(void)
{
    assert(key_pos<key_count);int key=keys[key_pos++];
    if(key>=1000) {
        cptr tokens[]={"s save","n name","c clear"};
        char footer[256];int row=Term->hgt-1;
        memcpy(footer,Term->scr->c[row],Term->wid);footer[Term->wid]=0;
        cptr token=strstr(footer,tokens[key-1000]);assert(token);
        int col=(int)(token-footer)+2;
        if(!ui_menu_click_has_cell(col,row))fprintf(stderr,"Missing footer hit target: %s\n",tokens[key-1000]);
        assert(ui_menu_click_has_cell(col,row));
        assert(ui_menu_click_handle_cell_action(col,row,UI_MENU_CLICK_PRIMARY));
        return UI_MENU_CLICK_WAKE_KEY;
    }
    return (char)key;
}
bool __wrap_get_check(cptr prompt)
{
    assert(confirmations<8);SDL_strlcpy(prompts[confirmations++],prompt,sizeof(prompts[0]));return answer;
}
bool __wrap_term_get_string(cptr prompt,char* text,size_t len)
{
    (void)len;assert(strstr(prompt,"Jewelry set name"));assert(!strcmp(text,"Amber QA"));names++;
    SDL_strlcpy(text,"Canceled",len);return false;
}
void __wrap_message_flush(void) {}
void __wrap_handle_stuff(void) {}
bool __wrap_object_info_overlay_show_multi(const object_type** objects,const char** headings,int count)
{
    (void)headings;assert(count>0&&objects[0]);previews++;preview_kind=objects[0]->k_idx;return true;
}
static void setup(void)
{
    memset(p_ptr,0,sizeof(*p_ptr));memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_quiver_reset_store();player_carried_extra_reset_store();supplies_reset_store();player_pack_action_reset();jewelry_presets_reset();
    p_ptr->py=p_ptr->px=2;p_ptr->playing=true;p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    p_ptr->chp=p_ptr->mhp=100;p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    rp_ptr=&p_info[0];current_character_profile=&c_info[0];mon_max=o_max=1;
    for(int y=0;y<20;y++)for(int x=0;x<20;x++){cave_feat[y][x]=FEAT_FLOOR;cave_o_idx[y][x]=cave_m_idx[y][x]=0;}
    cave_m_idx[2][2]=-1;
    object_prep(&inventory[INVEN_LEFT],lookup_kind(TV_RING,SV_RING_ACCURACY));
    object_prep(&inventory[INVEN_RIGHT],lookup_kind(TV_RING,SV_RING_EVASION));
    object_prep(&inventory[INVEN_NECK],lookup_kind(TV_AMULET,SV_AMULET_CON));
    for(int slot=INVEN_LEFT;slot<=INVEN_NECK;slot++){inventory[slot].number=1;object_known(&inventory[slot]);object_aware(&inventory[slot]);}
    assert(jewelry_preset_store_current(0));assert(jewelry_preset_set_name(0,"Amber QA"));
    turn=1001;playerturn=100;key_count=key_pos=confirmations=names=previews=preview_kind=0;memset(prompts,0,sizeof(prompts));
    set_sdl_input_ui_mode(SDL_INPUT_UI_MODE_PLATFORM);
}
static void run(const int* actions,int count,bool accept,int expect_checks,int expect_names,bool cleared)
{
    setup();answer=accept;
    object_type original[3];for(int i=0;i<3;i++)object_copy(&original[i],jewelry_preset_object(0,i));
    keys[key_count++]='6';for(int i=0;i<count;i++)keys[key_count++]=actions[i];keys[key_count++]=ESCAPE;
    supply_menu_request request={0};request.focus_group=true;request.group=group;
    (void)do_cmd_knowledge_supplies(&request);
    assert(key_pos==key_count&&confirmations==expect_checks&&names==expect_names);
    assert(jewelry_preset_is_set(0)==!cleared);
    if(!cleared) {
        assert(!strcmp(jewelry_preset_name(0),"Amber QA"));
        for(int i=0;i<3;i++)assert(!memcmp(&original[i],jewelry_preset_object(0,i),sizeof(original[i])));
    }
    assert(turn==1001&&playerturn==100&&!p_ptr->energy_use&&!player_pack_action_pending());
}
static void checks(void)
{
    const int clear[]={'c'},upper[]={'C'},pointer[]={1000,1001,1002},name[]={'n'},equip[]={'u'},tab[]={'e'};
    run(clear,1,false,1,0,false);
    if(!strstr(prompts[0],"Clear")||!strstr(prompts[0],"Amber QA"))fprintf(stderr,"c clear reached wrong prompt: %s\n",prompts[0]);
    assert(strstr(prompts[0],"Clear")&&strstr(prompts[0],"Amber QA"));
    run(upper,1,false,1,0,false);assert(strstr(prompts[0],"Clear"));
    run(name,1,false,0,1,false);
    run(pointer,3,false,2,1,false);assert(strstr(prompts[0],"Replace")&&strstr(prompts[1],"Clear"));
    run(clear,1,true,1,0,true);
    run(equip,1,false,1,0,false);assert(strstr(prompts[0],"Equip"));
    run(tab,1,false,0,0,false); /* e must remain the Equipped page command. */
    const int ordinary[]={'c','x'};group=SUPPLY_GROUP_JEWELRY;
    run(ordinary,2,false,0,0,false);
    assert(previews==1&&preview_kind==lookup_kind(TV_RING,SV_RING_EVASION));
    puts("Real Jewelry Sets browser: c/C clear, pointer Save/Name/Clear IDs, cancellation/membership, accepted clear, equip/tab commands and free clocks PASS");
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--alias-baseline',action='store_true');parser.add_argument('--pointer-baseline',action='store_true')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):];init=init[:init.index('    check_templates();')]
    init=init.replace('term_init(&test_term,80,24,256)','term_init(&test_term,96,36,256)')
    (OUT/'check.c').write_text(prefix+CHECKS+init+'assert(!init_flavor_info());flavor_init();checks();SDL_Quit();return 0;\n}\n',encoding='utf-8')
    browser=(ROOT/'src/cmd/ui/cmd-ui-knowledge.c').read_text()
    if args.alias_baseline:
        old='if (!click_generated_command\n            && grp_idx[grp_cur] != SUPPLY_GROUP_JEWELRY_PRESETS)';assert browser.count(old)==1
        browser=browser.replace(old,'if (!click_generated_command)')
    if args.pointer_baseline:
        for action,token in [('SAVE','s save'),('NAME','n name'),('CLEAR','c clear')]:
            old=f'    ui_menu_click_add_text_token(SUPPLY_CLICK_PRESET_{action}, 0, row, prompt,\n        "{token}");\n';assert browser.count(old)==1;browser=browser.replace(old,'')
    (OUT/'browser.c').write_text(browser,encoding='utf-8')
    cmake=BUILD/'CMakeFiles/sil-more.dir';objects=shlex.split((cmake/'objects1.rsp').read_text())
    (OUT/'objects.rsp').write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/cmd/ui/cmd-ui-knowledge.c.obj'))),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe';subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(OUT/'check.c'),str(OUT/'browser.c'),'@'+str(OUT/'objects.rsp'),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*(f'-Wl,--wrap={name}' for name in ('inkey','get_check','term_get_string','message_flush','handle_stuff','object_info_overlay_show_multi')),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=20)

if __name__=='__main__':main()

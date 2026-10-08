#!/usr/bin/env python3
"""Exercise Inventory/Supplies row-label conflicts with the real browser.

--alias-baseline removes only the new Ctrl aliases, retaining other repairs.
--footer-baseline restores long verbs in compact Inventory footer variants.
"""
from pathlib import Path
import os
import argparse
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/qa9-browser-shortcuts'

CHECKS = r'''
#include "supplies.h"
#include "sdl-config.h"
#include "cmd/ui/cmd-ui-internal.h"
#include "ui/menu-click.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__); exit(1); } } while (0)
static int keys[80], count, pos, confirmations, previews, preview_note;
static int stack_count;
static int browser_width=120;
static bool controller, pointer_pending;
static bool supply_page,picker,mixed_harness;
static char footer[256];
extern errr __real_Term_putstr(int x,int y,int n,byte a,cptr s);
errr __wrap_Term_putstr(int x,int y,int n,byte a,cptr s)
{
    if (x==0 && y==Term->hgt-1) SDL_strlcpy(footer,s,sizeof(footer));
    return __real_Term_putstr(x,y,n,a,s);
}
char __wrap_inkey(void)
{
    assert(pos<count);
    if(controller&&footer[0]){assert(!strstr(footer,"Ctrl+x")&&!strstr(footer,"Ctrl+y"));}
    if (!controller && footer[0]) {
        assert(strlen(footer)<=(size_t)Term->wid&&strstr(footer,"Esc"));
        if(!picker)assert(strstr(footer,stack_count>=21 ? "Space " : "u "));
        assert(strstr(footer,stack_count>=24 ? "Ctrl+x preview" : "x preview"));
        if (!supply_page&&!picker) assert(strstr(footer,stack_count>=25 ? "Ctrl+y " : "y "));
    }
    int key=keys[pos++];
    if (key==127) { pointer_pending=true; return UI_MENU_CLICK_WAKE_KEY; }
    return (char)key;
}
bool __wrap_ui_menu_click_take_action(int* choice,int* action)
{
    if (!pointer_pending) return false;
    pointer_pending=false; *choice=SUPPLY_CLICK_PREVIEW;
    *action=UI_MENU_CLICK_PRIMARY; return true;
}
bool __wrap_get_check(cptr prompt)
{
    (void)prompt; confirmations++; return false;
}
bool __wrap_object_info_overlay_show_multi(const object_type** objects,
    const char** headings,int n)
{
    (void)headings; assert(n>0 && objects[0]);
    previews++; preview_note=atoi(quark_str(objects[0]->obj_note));
    return true;
}
void __wrap_message_flush(void) {}
void __wrap_handle_stuff(void) {}
bool __wrap_sdl_gamepad_control_available(int type,int id)
{ (void)type; (void)id; return true; }
int __wrap_steamdeck_info_key(void) { return '!'; }
static void run(int stacks,bool pad,const int* actions,int actions_count,
    int expected_confirmations,int expected_note)
{
    object_type item; supply_menu_request request={0};
    if(Term->wid!=browser_width || Term->hgt!=40)assert(!Term_resize(browser_width,40));
    memset(p_ptr,0,sizeof(*p_ptr));memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_quiver_reset_store();player_carried_extra_reset_store();supplies_reset_store();
    player_pack_action_reset();jewelry_presets_reset();
    p_ptr->py=p_ptr->px=2;p_ptr->playing=true;p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    p_ptr->chp=p_ptr->mhp=100;p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    rp_ptr=&p_info[0];current_character_profile=&c_info[0];mon_max=o_max=1;
    for(int y=0;y<20;y++)for(int x=0;x<20;x++){cave_feat[y][x]=FEAT_FLOOR;cave_o_idx[y][x]=cave_m_idx[y][x]=0;}
    cave_m_idx[2][2]=-1;
    if(supply_page){
        int count=0;
        for(int k=0;k<stacks;k++){
            object_prep(&item,lookup_kind(TV_POTION,SV_POTION_MIRUVOR));item.number=1;object_aware(&item);object_known(&item);
            char note[16];strnfmt(note,sizeof(note),"%d",++count);item.obj_note=quark_add(note);
            assert(supplies_absorb_object(&item));
        }
        assert(count==stacks&&supplies_entry_count()==stacks);
    }else if(mixed_harness){
        object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_LONG_SWORD));inventory[INVEN_WIELD].number=1;
        object_prep(&item,lookup_kind(TV_SWORD,SV_DAGGER));item.number=1;item.storage=OBJECT_STORAGE_HARNESS;
        for(int i=1;i<stacks;i++){
            char note[16];strnfmt(note,sizeof(note),"%d",i);item.obj_note=quark_add(note);
            assert(player_carried_extra_load(&item));
        }
    }else{
        object_prep(&item,lookup_kind(TV_ARROW,1));item.number=1;
        for(int i=1;i<=stacks;i++){
            char note[16];strnfmt(note,sizeof(note),"%d",i);item.obj_note=quark_add(note);
            assert(player_quiver_absorb_arrow(&item)==1);item.number=1;
        }
    }
    controller=pad;stack_count=stacks;set_sdl_input_ui_mode(pad?SDL_INPUT_UI_MODE_CONTROLLER:SDL_INPUT_UI_MODE_PLATFORM);
    assert(indexed_menu_letters_enabled()==!pad);
    count=pos=confirmations=previews=preview_note=0;pointer_pending=false;footer[0]=0;
    if(supply_page||browser_width<55)keys[count++]='6';
    for(int i=0;i<actions_count;i++)keys[count++]=actions[i];keys[count++]=ESCAPE;
    request.focus_page=true;request.page=supply_page?SUPPLY_MENU_PAGE_SUPPLIES:SUPPLY_MENU_PAGE_INVENTORY;
    request.focus_inventory_group=!supply_page;request.inventory_group=mixed_harness?INVENTORY_MENU_GROUP_HARNESS:INVENTORY_MENU_GROUP_QUIVER;
    int chosen=-999;if(picker){request.item_select_mode=true;request.item_select_flags=USE_INVEN;request.item_select_item_out=&chosen;}
    turn=1001;playerturn=100;
    (void)do_cmd_knowledge_supplies(&request);
    if(pos!=count||confirmations!=expected_confirmations)fprintf(stderr,"page=%d width=%d stacks=%d key=%d pos=%d/%d confirms=%d/%d\n",supply_page,browser_width,stacks,actions[0],pos,count,confirmations,expected_confirmations);
    assert(pos==count&&confirmations==expected_confirmations);
    if(picker)assert(chosen==-1);
    assert(previews==1&&preview_note==expected_note);
    assert(!player_pack_action_pending()&&!p_ptr->energy_use&&turn==1001&&playerturn==100);
    if(supply_page){assert(supplies_entry_count()==stacks);for(int i=0;i<stacks;i++)assert(supplies_entry_units(i)==1);}
    else if(mixed_harness){assert(player_pack_entry_count()==stacks-1&&inventory[INVEN_WIELD].number==1);}
    else assert(player_quiver_store_entry_count()==stacks&&player_quiver_arrow_count()==stacks);
}
static void check(void)
{
    const int harness[]={'2',KTRL('X'),KTRL('Y')};
    mixed_harness=true;browser_width=50;run(25,false,harness,3,1,1);mixed_harness=false;
    puts("Inventory 50-column mixed Harness change-setup footer retains Ctrl+x/Ctrl+y/Space/Esc PASS.");
    const int widths[]={50,60,80,120};
    const int x[]={ 'x',KTRL('X') },y[]={ 'y',KTRL('X'),KTRL('Y') };
    const int upper[]={ 'X',KTRL('X') },upper_y[]={ 'Y',KTRL('X'),KTRL('Y') };
    const int ordinary[]={ 'u','x' },ordinary_y[]={ 'y','x' };
    const int space[]={ ' ',KTRL('X') }, enter[]={ '\r',KTRL('X') };
    const int pointer[]={127},pad[]={ '!' };
    for(int page=0;page<2;page++){
        supply_page=page;
        for(int w=0;w<4;w++){
            browser_width=widths[w];
            run(24,false,x,2,1,24);run(24,false,upper,2,1,24);
            run(25,false,page?x:y,page?2:3,page?1:2,page?24:25);
            if(!page)run(25,false,upper_y,3,2,25);
            run(20,false,ordinary,2,1,1);
            if(!page)run(20,false,ordinary_y,2,1,1);
            run(24,false,space,2,1,1);run(24,false,enter,2,1,1);
            run(24,false,pointer,1,0,1);run(24,true,pad,1,0,1);
            if(!page){const int preview[]={KTRL('Y'),KTRL('X')};picker=true;run(25,false,preview,2,0,1);picker=false;}
            printf("%s real footer/aliases/row labels/No/stock clocks at %d columns PASS.\n",page?"Supplies":"Inventory",browser_width);
        }
    }
}
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--alias-baseline",action="store_true");parser.add_argument("--footer-baseline",action="store_true");args=parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    init = init.replace('term_init(&test_term,80,24,256)', 'term_init(&test_term,120,40,256)')
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init +
                      '    assert(!init_flavor_info());flavor_init();check(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    browser=OUT/'browser.obj'
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects
                                 if not p.endswith(('/src/main.c.obj','/src/cmd/ui/cmd-ui-knowledge.c.obj'))), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    browser_source=ROOT/'src/cmd/ui/cmd-ui-knowledge.c'
    if args.alias_baseline:
        current=browser_source.read_text(encoding='utf-8')
        split=current.index('            cptr delete_action = "delete";')
        head,tail=current[:split],current[split:]
        assert tail.count("case KTRL('X'):")==2 and tail.count("case KTRL('Y'):")==1
        tail=tail.replace("            case KTRL('X'):\n","").replace("        case KTRL('X'):\n","").replace("            case KTRL('Y'):\n","")
        browser_source=OUT/'browser-alias-baseline.c';browser_source.write_text(head+tail,encoding='utf-8')
    if args.footer_baseline:
        assert not args.alias_baseline
        current=browser_source.read_text(encoding='utf-8')
        old='use_key, "use", preview_key, delete_key, short_delete);'
        assert current.count(old)==2
        browser_source=OUT/'browser-footer-baseline.c'
        browser_source.write_text(current.replace(old,'use_key, primary_action, preview_key, delete_key, short_delete);'),encoding='utf-8')
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp','-c',str(browser_source),'-o',str(browser)],cwd=BUILD,env=env,check=True)
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source),str(browser), '@' + str(response),
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp',
                    *(f'-Wl,--wrap={name}' for name in ('inkey','message_flush','handle_stuff',
                        'Term_putstr','get_check','object_info_overlay_show_multi',
                        'ui_menu_click_take_action','sdl_gamepad_control_available',
                        'steamdeck_info_key')), '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Exercise Equipment's real browser with attainable distinct Quiver stacks."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/qa9-equipment-shortcuts'

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
    if (pos>=13 && !controller) {
        assert(strlen(footer)<=(size_t)Term->wid);
        assert(strstr(footer,stack_count>=21 ? "Space " : "u "));
        assert(strstr(footer,stack_count>=24 ? "Ctrl+x preview" : "x preview"));
        if (stack_count>=21) assert(!strstr(footer," u "));
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
    assert(strstr(prompt,"Ready Arrow")); confirmations++; return false;
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
    object_type arrow;
    supply_menu_request request={0};
    if (Term->wid!=browser_width || Term->hgt!=40)
        assert(Term_resize(browser_width,40)==0);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_quiver_reset_store(); player_carried_extra_reset_store();
    supplies_reset_store(); player_pack_action_reset();
    object_prep(&arrow,lookup_kind(TV_ARROW,1)); arrow.number=1;
    for (int i=1;i<=stacks;i++) {
        char note[16]; strnfmt(note,sizeof(note),"%d",i);
        arrow.obj_note=quark_add(note);
        assert(player_quiver_absorb_arrow(&arrow)==1);
        arrow.number=1;
    }
    assert(player_quiver_store_entry_count()==stacks);
    p_ptr->py=p_ptr->px=2;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    p_ptr->playing=true; p_ptr->energy_use=0;
    controller=pad; stack_count=stacks;
    set_sdl_input_ui_mode(pad ? SDL_INPUT_UI_MODE_CONTROLLER
        : SDL_INPUT_UI_MODE_PLATFORM);
    assert(indexed_menu_letters_enabled()==!pad);
    /* All-equipped is bounded by real slots; Quiver is category 12 and
     * can show 48 independently inscribed single-arrow stacks. */
    count=pos=confirmations=previews=preview_note=0;
    pointer_pending=false; footer[0]=0;
    for (int i=0;i<12;i++) keys[count++]='2';
    keys[count++]='6';
    for (int i=0;i<actions_count;i++) keys[count++]=actions[i];
    keys[count++]=ESCAPE;
    request.focus_page=true; request.page=SUPPLY_MENU_PAGE_EQUIPPED;
    (void)do_cmd_knowledge_supplies(&request);
    assert(pos==count && confirmations==expected_confirmations);
    assert(previews==1 && preview_note==expected_note);
    assert(player_quiver_store_entry_count()==stacks);
    assert(player_quiver_arrow_count()==stacks);
    assert(!player_pack_action_pending() && p_ptr->energy_use==0);
}
static void check(void)
{
    const int u[]={ 'u',KTRL('X') }, upper_u[]={ 'U',KTRL('X') };
    const int x[]={ 'x',KTRL('X') }, upper_x[]={ 'X',KTRL('X') };
    const int space[]={ '2','2',' ',KTRL('X') };
    const int pointer[]={ '2','2',127 };
    const int ordinary[]={ 'u','x' }, ordinary_upper[]={ 'u','X' };
    const int pad[]={ '!' };
    assert(QUIVER_ARROW_CAPACITY==48);
    assert(browser_entry_label_for_index(24)=='y');
    assert(browser_entry_label_for_index(25)==0);
    run(24,false,u,2,1,21); run(24,false,upper_u,2,1,21);
    run(24,false,x,2,1,24); run(24,false,upper_x,2,1,24);
    run(24,false,space,4,1,3); run(24,false,pointer,3,0,3);
    run(20,false,ordinary,2,1,1); run(20,false,ordinary_upper,2,1,1);
    run(21,false,u,2,1,21); run(23,false,u,2,1,21);
    for (int i=0;i<25;i++) {
        const int label[]={ 'a'+i,KTRL('X') };
        run(25,false,label,2,1,i+1);
    }
    run(24,true,pad,1,0,1);
    /* At the real Quiver capacity, labels remain capped at a..y. */
    run(48,false,u,2,1,21); run(48,false,x,2,1,24);
    const int widths[]={50,60,80,120};
    for (int i=0;i<4;i++) {
        browser_width=widths[i];
        run(24,false,space,4,1,3);
        printf("Equipment %d-column keyboard footer: [%s]\n",browser_width,footer);
        run(24,false,pointer,3,0,3);
        run(24,false,x,2,1,24);
        run(24,true,pad,1,0,1);
        printf("Equipment real footer/preview dispatch at %d columns PASS.\n",browser_width);
    }
    puts("Equipment shortcuts: effective hints, item u/x, Space, Ctrl+x, pointer and controller preview; stock/energy conserved PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    init = init.replace('term_init(&test_term,80,24,256)', 'term_init(&test_term,120,40,256)')
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init +
                      '    check(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects
                                 if not p.endswith('/src/main.c.obj')), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), '@' + str(response),
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

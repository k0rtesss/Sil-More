#!/usr/bin/env python3
"""Exercise real object overlay shortcut, preview and navigation dispatch."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/qa9-items-selector-shortcuts'

CHECKS = r'''
#include "object/object-ui-select.h"
#include "ui/menu-click.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__); exit(1); } } while (0)
static int keys[8], navs[8], key_count, key_pos, pending_nav;
static int preview_count, preview_item, pending_click;
static char displayed[40][OBJECT_CHOICE_LABEL_LEN];
static object_choice_entry entries[37], original[37];
static object_type token={.k_idx=1};
char __wrap_inkey(void)
{
    assert(key_pos<key_count);
    pending_nav=navs[key_pos];
    if (keys[key_pos]==127) pending_click=23;
    return keys[key_pos++]==127 ? UI_MENU_CLICK_WAKE_KEY : keys[key_pos-1];
}
int __wrap_sdl_question_menu_take_navigation(void)
{ int result=pending_nav; pending_nav=0; return result; }
bool __wrap_ui_menu_click_take_action(int* choice,int* action)
{
    if (pending_click<0) return false;
    *choice=pending_click; *action=UI_MENU_CLICK_PRIMARY; pending_click=-1;
    return true;
}
void __wrap_sdl_question_menu_add_object_entry(int choice,cptr letter,
    cptr label,byte attr,const object_type* object)
{
    (void)label; (void)attr; (void)object;
    assert(choice>=0 && choice<40);
    SDL_strlcpy(displayed[choice],letter,sizeof(displayed[choice]));
}
void __wrap_message_flush(void) {}
void __wrap_handle_stuff(void) {} /* No live dungeon panes in this fixture. */
void __wrap_describe_item_with_comparisons(int item,bool comparisons)
{ assert(comparisons); preview_count++; preview_item=item; }
static void build(int count)
{
    memset(entries,0,sizeof(entries));
    for (int i=0;i<count;i++) {
        entries[i].item=i; entries[i].o_ptr=&token;
        entries[i].attr=TERM_WHITE;
        strnfmt(entries[i].text,sizeof(entries[i].text),"Item %d",i);
        strnfmt(entries[i].label,sizeof(entries[i].label),"%c)",i<26?'a'+i:'0'+i-26);
    }
    memcpy(original,entries,sizeof(entries));
}
static int run(int count,int initial,int first,int second)
{
    int selected=-1;
    memset(keys,0,sizeof(keys)); memset(navs,0,sizeof(navs));
    memset(displayed,0,sizeof(displayed));
    keys[0]=first; keys[1]=second; keys[2]=ESCAPE;
    key_count=3; key_pos=0; pending_nav=0; pending_click=-1; preview_count=0;
    bool result=object_choice_overlay("Fixture items",NULL,entries,count,initial,&selected);
    assert(!memcmp(entries,original,sizeof(entries)));
    return result?selected:-1;
}
static void check_shortcuts(void)
{
    for (int vi=0;vi<2;vi++) {
        hjkl_movement=vi;
        build(26);
        assert(run(26,0,'j',ESCAPE)==9);
        assert(run(26,0,'K',ESCAPE)==10);
        assert(run(26,0,'u',ESCAPE)==20);
        assert(run(26,0,'x','0')==23 && preview_count==1 && preview_item==0);
        assert(displayed[23][0]=='0');
        char assigned[40][OBJECT_CHOICE_LABEL_LEN];
        assert(run(26,0,ESCAPE,ESCAPE)==-1);
        memcpy(assigned,displayed,sizeof(assigned));
        for (int i=0;i<26;i++) {
            assert(assigned[i][0] && assigned[i][0]!='x');
            for (int j=0;j<i;j++) assert(assigned[i][0]!=assigned[j][0]);
            assert(run(26,0,assigned[i][0],ESCAPE)==i);
        }
        build(5);
        assert(run(5,0,'j','\r')==1);
        assert(run(5,1,'k','\r')==0);
        assert(run(5,2,'X','\r')==2 && preview_count==1 && preview_item==2);
        build(36);
        assert(run(36,0,ESCAPE,ESCAPE)==-1);
        assert(!displayed[23][0]); /* All 35 safe shortcuts are already used. */
        memcpy(assigned,displayed,sizeof(assigned));
        for (int i=0;i<36;i++) if (assigned[i][0]) {
            for (int j=0;j<i;j++) if (assigned[j][0]) assert(assigned[i][0]!=assigned[j][0]);
            assert(run(36,0,assigned[i][0],ESCAPE)==i);
        }
        /* Physical semantic navigation wins over a displayed numeric key. */
        keys[0]='2'; keys[1]='\r'; key_pos=0; key_count=2;
        navs[0]=1; navs[1]=0; pending_nav=0; pending_click=-1;
        int selected=-1;
        assert(object_choice_overlay("Arrow",NULL,entries,36,22,&selected) && selected==23);
        assert(!memcmp(entries,original,sizeof(entries)));
        assert(run(36,0,127,127)==23); /* Unlabelled row remains clickable. */
        entries[36]=entries[0]; entries[36].item=-1;
        SDL_strlcpy(entries[36].label,"-)",sizeof(entries[36].label));
        memcpy(original,entries,sizeof(entries));
        assert(run(37,0,'-',ESCAPE)==36);
    }
    puts("Object selector: classic/Vi j/k and all assigned 26/36-row shortcuts, unique local preview remap, x/X preview, u selection, fallback navigation, semantic arrows, click and floor '-' PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init +
                      '    check_shortcuts(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
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
                        'sdl_question_menu_add_object_entry','sdl_question_menu_take_navigation',
                        'ui_menu_click_take_action','describe_item_with_comparisons')),
                    '-o', str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()

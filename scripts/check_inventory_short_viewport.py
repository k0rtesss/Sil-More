#!/usr/bin/env python3
"""Run the production Inventory browser at short touch-terminal heights."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_qa9_browser_shortcuts import CHECKS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/inventory-short-viewport'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true')
    parser.add_argument('--portrait-baseline', action='store_true')
    parser.add_argument('--resize-baseline', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    checks = CHECKS[:CHECKS.index('static void check(void)')]
    event_source = (ROOT / 'src/sdl/core/sdl-events.c').read_text()
    wake_start = event_source.index('    /* Inventory draws into the terminal')
    wake_end = event_source.index('    // Don\'t strictly need this', wake_start)
    resize_wake = '' if args.resize_baseline else event_source[wake_start:wake_end]
    resize_policy = r'''
static int resize_target_hgt, resize_wake_count;
static bool resize_click_active=true, resize_question;
static enum sdl_menu_font resize_terminal=SDL_MENU_FONT_INVENTORY;
static enum sdl_menu_font resize_current=SDL_MENU_FONT_INVENTORY;
static enum sdl_menu_font resize_terminal_font(void) {return resize_terminal;}
static enum sdl_menu_font resize_current_font(void) {return resize_current;}
static bool resize_clicks(void) {return resize_click_active;}
static bool resize_question_active(void) {return resize_question;}
static int resize_keypress(int key) {
    assert(key==UI_MENU_CLICK_WAKE_KEY);resize_wake_count++;return 0;
}
#define sdl_terminal_menu_font resize_terminal_font
#define sdl_menu_font_current resize_current_font
#define ui_menu_click_is_active resize_clicks
#define sdl_question_menu_is_active resize_question_active
#define Term_keypress resize_keypress
static void emit_resize_wake(void) {
RESIZE_WAKE
}
#undef sdl_terminal_menu_font
#undef sdl_menu_font_current
#undef ui_menu_click_is_active
#undef sdl_question_menu_is_active
#undef Term_keypress
static void check_resize_policy(void) {
    resize_wake_count=0;emit_resize_wake();assert(resize_wake_count==1);
    resize_question=true;emit_resize_wake();assert(resize_wake_count==1);
    resize_question=false;resize_current=SDL_MENU_FONT_QUESTIONS;
    emit_resize_wake();assert(resize_wake_count==1);
    resize_current=SDL_MENU_FONT_INVENTORY;resize_click_active=false;
    emit_resize_wake();assert(resize_wake_count==1);
    resize_click_active=true;resize_terminal=SDL_MENU_FONT_NONE;
    emit_resize_wake();assert(resize_wake_count==1);
    resize_terminal=SDL_MENU_FONT_INVENTORY;
}
'''.replace('RESIZE_WAKE', resize_wake)
    checks = checks.replace('static int browser_width=120;',
        resize_policy + '\nstatic int browser_width=120;')
    checks = checks.replace('static int browser_width=120;', '''
static int browser_width=120, browser_height=40, reserved_rows;
static bool bigger, touch, pack_fixture, portrait;
bool __real_ui_menu_click_take_action(int* choice,int* action);
bool __wrap_get_sdl_menu_bigger_font(void) { return bigger; }
bool __wrap_get_sdl_compact_inventory_menus(void) { return true; }
bool __wrap_sdl_touch_only_device_active(void) { return touch; }
bool __wrap_sdl_mobile_portrait_layout_active(void) { return portrait; }
int __wrap_sdl_touch_menu_button_reserved_rows(void) { return reserved_rows; }
''')
    checks = checks.replace('assert(player_quiver_absorb_arrow(&item)==1);item.number=1;',
        'if(pack_fixture){item.storage=OBJECT_STORAGE_PACK;assert(player_carried_extra_load(&item));}'
        'else assert(player_quiver_absorb_arrow(&item)==1);item.number=1;')
    checks = checks.replace('mixed_harness?INVENTORY_MENU_GROUP_HARNESS:INVENTORY_MENU_GROUP_QUIVER',
        'mixed_harness?INVENTORY_MENU_GROUP_HARNESS:pack_fixture?INVENTORY_MENU_GROUP_PACK:INVENTORY_MENU_GROUP_QUIVER')
    checks = checks.replace('else assert(player_quiver_store_entry_count()==stacks',
        'else if(pack_fixture)assert(player_pack_entry_count()==stacks);'
        'else assert(player_quiver_store_entry_count()==stacks')
    checks = checks.replace('Term->hgt!=40', 'Term->hgt!=browser_height').replace(
        'Term_resize(browser_width,40)', 'Term_resize(browser_width,browser_height)')
    checks = checks.replace('    assert(pos<count);', '''
    if(resize_target_hgt && pos==2) {
        browser_height=resize_target_hgt;resize_target_hgt=0;
        assert(!Term_resize(browser_width,browser_height));
        resize_wake_count=0;emit_resize_wake();assert(resize_wake_count==1);
        return UI_MENU_CLICK_WAKE_KEY;
    }
    assert(pos<count);
    bool arrow_visible=false;
    int visible_index=-1;
    for(int y=0;y<Term->hgt-1-reserved_rows;y++) {
        char row[256]; memcpy(row,Term->scr->c[y],Term->wid); row[Term->wid]=0;
        if(strstr(row,"an Arrow")) {
            arrow_visible=true;
            char* name=strstr(row,"an Arrow");
            assert(ui_menu_click_has_cell((int)(name-row),y));
            assert(ui_menu_click_handle_cell_action((int)(name-row),y,UI_MENU_CLICK_PRIMARY));
            int choice,action;
            assert(__real_ui_menu_click_take_action(&choice,&action));
            assert(choice>=SUPPLY_CLICK_ENTRY_BASE && choice<SUPPLY_CLICK_ENTRY_BASE+3);
            if(visible_index<0) visible_index=choice-SUPPLY_CLICK_ENTRY_BASE;
            assert(choice==SUPPLY_CLICK_ENTRY_BASE+visible_index++ && action==UI_MENU_CLICK_PRIMARY);
        }
    }
    if(!arrow_visible) {
        fprintf(stderr,"No item row at %dx%d reserved=%d big=%d\\n",browser_width,browser_height,reserved_rows,bigger);
        for(int y=0;y<Term->hgt;y++) fprintf(stderr,"%d: %.*s\\n",y,Term->wid,Term->scr->c[y]);
    }
    assert(arrow_visible);
''')
    checks += r'''
static void check(void)
{
    check_resize_policy();
    const int preview[]={'!'};
    const int next_preview[]={'2','!'};
    const int sizes[][3]={{51,10,4},{51,11,4},{51,12,4},{51,16,4},
        {51,24,4},{80,24,0},{120,40,0}};
    for(int orient=0;orient<2;orient++)for(int pack=0;pack<2;pack++)for(int big=1;big>=0;big--)for(unsigned i=0;i<N_ELEMENTS(sizes);i++) {
        portrait=orient;
        pack_fixture=pack;
        bigger=big; browser_width=sizes[i][0]; browser_height=sizes[i][1];
        reserved_rows=sizes[i][2]; touch=reserved_rows>0;
        run(3,true,preview,1,0,1);
        run(3,true,next_preview,2,0,2);
        printf("Inventory %s visible row/click IDs/preview/clocks %dx%d reserved=%d big=%d portrait=%d PASS.\n",
            pack_fixture?"Pack":"Quiver",browser_width,browser_height,reserved_rows,bigger,portrait);
    }
    /* Resize while entry 2 is selected, then preview through the rebuilt
     * browser. Its identity, registered click ID, stacks and clocks survive. */
    portrait=false;reserved_rows=4;touch=true;bigger=true;browser_width=51;
    for(int pack=0;pack<2;pack++)for(int shrink=0;shrink<2;shrink++) {
        pack_fixture=pack;browser_height=shrink?13:10;
        resize_target_hgt=shrink?10:13;
        run(3,true,next_preview,2,0,2);
        assert(!resize_target_hgt);
        puts("Inventory live resize retains selected stack, row IDs and clocks PASS.");
    }
}
'''
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    init = init.replace('term_init(&test_term,80,24,256)', 'term_init(&test_term,120,40,256)')
    source = OUT / 'check.c'
    source.write_text(prefix + checks + init +
        '    assert(!init_flavor_info());flavor_init();check(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    browser_source = ROOT / 'src/cmd/ui/cmd-ui-knowledge.c'
    if args.baseline or args.portrait_baseline:
        current = browser_source.read_text(encoding='utf-8')
        if args.portrait_baseline:
            start = current.index('static void knowledge_init_inventory_portrait_layout_normal(')
            before, tail = current[:start], current[start:]
            tail = tail.replace('layout->status_row - layout->list_row >= 3',
                'layout->status_row > layout->list_row', 1)
            current = before + tail
        else:
            current = current.replace('    knowledge_inventory_reclaim_empty_body(layout);\n', '')
        browser_source = OUT / 'browser-baseline.c'
        browser_source.write_text(current, encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects if not p.endswith(
        ('/src/main.c.obj','/src/cmd/ui/cmd-ui-knowledge.c.obj'))), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    cc = 'C:/msys64/mingw64/bin/cc.exe'
    flags = ['-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp']
    browser = OUT / 'browser.obj'
    subprocess.run([cc,*flags,'-c',str(browser_source),'-o',str(browser)],cwd=BUILD,env=env,check=True)
    wraps = ('inkey','message_flush','handle_stuff','Term_putstr','get_check',
        'object_info_overlay_show_multi','ui_menu_click_take_action',
        'sdl_gamepad_control_available','steamdeck_info_key',
        'get_sdl_menu_bigger_font','get_sdl_compact_inventory_menus',
        'sdl_touch_only_device_active','sdl_touch_menu_button_reserved_rows',
        'sdl_mobile_portrait_layout_active')
    exe = OUT / 'check.exe'
    subprocess.run([cc,*flags,str(source),str(browser),'@'+str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp',*(f'-Wl,--wrap={name}' for name in wraps),
        '-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=20)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Run the actual Hints/Quests/Thralls page loops for shoulder and Tab routing.

Normalized controller keys are injected at inkey(); native rendering and the
production page parsers run. This is not a virtual-device integration test.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess

from check_quest_status_scroll import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT/'build-standard'
OUT = ROOT/'scripts/output/hint-quest-shoulders'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',action='store_true')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    harness=HARNESS
    start=harness.index('char __wrap_inkey(void)')
    end=harness.index('\nint main(',start)
    harness=harness[:start]+r'''
static char input_key;
static hint_quest_page opened_page;
hint_quest_page fixture_sibling_page(hint_quest_page page);
char __wrap_inkey(void)
{
    assert(++pos==1);
    assert(sdl_hint_quest_menu_active()&&g_hint_quest.page==opened_page);
    sdl_hint_quest_menu_render();
    char name[64];strnfmt(name,sizeof(name),"page-%d-key-%d",opened_page,input_key);
    capture(name);
    return input_key;
}
''' + harness[end:]
    start=harness.index('    for(ending=0;ending<3;ending++)')
    end=harness.index('    sdl_story_font_cache_clear();',start)
    harness=harness[:start]+r'''
    const hint_quest_page pages[]={HINT_QUEST_PAGE_THRALLS,HINT_QUEST_PAGE_HINTS,HINT_QUEST_PAGE_QUESTS};
    const char keys[]={'[',']','\t'};
    for(unsigned p=0;p<N_ELEMENTS(pages);p++)for(unsigned k=0;k<N_ELEMENTS(keys);k++) {
        opened_page=pages[p];input_key=keys[k];pos=0;
        hint_quest_page wanted;
        if(k==0)wanted=opened_page==HINT_QUEST_PAGE_HINTS?HINT_QUEST_PAGE_THRALLS:(hint_quest_page)(opened_page-1);
        else wanted=opened_page==HINT_QUEST_PAGE_THRALLS?HINT_QUEST_PAGE_HINTS:(hint_quest_page)(opened_page+1);
        hint_quest_page actual=opened_page==HINT_QUEST_PAGE_QUESTS?do_cmd_quest_status_page():fixture_sibling_page(opened_page);
        fprintf(stderr,"Book page %d key %d -> %d (expected %d)\n",opened_page,input_key,actual,wanted);
        assert(pos==1&&actual==wanted);
        assert(turn==1001&&playerturn==100&&items[INVEN_LITE].timeout==281&&p_ptr->chp==100&&p_ptr->new_exp==123);
        sdl_hint_quest_menu_hide();
    }
    puts("Production Hints/Quests/Thralls: all six shoulder directions, all three forward Tabs, native rendering and unchanged clocks/resources PASS");
''' + harness[end:]
    quest=(ROOT/'src/quest/quest-status.c').read_text().replace('#include "externs.h"','')
    hint=(ROOT/'src/sdl/ui/sdl-hint-quest-menu.c').read_text()
    source=OUT/'check.c';source.write_text(harness.replace('/* HINT_SOURCE */',hint).replace('/* QUEST_SOURCE */',quest))
    menu=(ROOT/'src/cmd/ui/cmd-ui-main-menu.c').read_text()
    if args.baseline:
        old="steamdeck_menu_key(ch, '[', ']')"
        assert menu.count(old)==2
        menu=menu.replace(old,"steamdeck_menu_key(ch, '\\t', '\\t')")
    wrapper=r'''
hint_quest_page fixture_sibling_page(hint_quest_page page)
{
    bool pending_look=false,pending_map=false;int y=-1,x=-1,map_y=-1,map_x=-1;
    hint_quest_page result=page==HINT_QUEST_PAGE_HINTS
        ?do_cmd_hint_messages(&pending_look,&y,&x,&pending_map,&map_y,&map_x)
        :do_cmd_thrall_quests(&pending_look,&y,&x,&pending_map,&map_y,&map_x);
    assert(!pending_look&&!pending_map);return result;
}
'''
    menu_source=OUT/'menu.c';menu_source.write_text('#include <assert.h>\n'+menu+wrapper)
    cmake=BUILD/'CMakeFiles/sil-more.dir';objects=shlex.split((cmake/'objects1.rsp').read_text())
    response=OUT/'objects.rsp';response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(
        ('/src/main.c.obj','/src/quest/quest-status.c.obj','/src/sdl/ui/sdl-hint-quest-menu.c.obj','/src/cmd/ui/cmd-ui-main-menu.c.obj'))))
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(menu_source),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',
        '-Wl,--wrap=inkey,--wrap=handle_stuff,--wrap=sdl_present_if_needed,--wrap=sdl_refresh_supporting_panes_layout,--wrap=sdl_suspend_main_view_zoom_for_saved_screen,--wrap=sdl_resume_main_view_zoom_for_saved_screen',
        '-o',str(exe)],cwd=BUILD,env=env,check=True)
    subprocess.run([str(exe),str(ROOT/'lib/xtra/font/EBGaramond-Regular.ttf'),str(OUT)],cwd=ROOT,env=env,check=True,timeout=30)


if __name__=='__main__':main()

#!/usr/bin/env python3
"""Check Log cleanup repairs clipped gameplay cells and publishes the result.

Uses the real screen stack and Term resize/load; a redraw boundary models the
engine's repaired map cells, while real terminal hooks track published pixels.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
CHECKS = r'''
static char canvas[90][180];
static int repairs, expected_width, expected_height;
static errr publish_text(int x,int y,int n,byte attr,cptr text)
{(void)attr;assert(x>=0&&y>=0&&y<90&&x+n<=180);memcpy(canvas[y]+x,text,n);return 0;}
static errr publish_wipe(int x,int y,int n)
{assert(x>=0&&y>=0&&y<90&&x+n<=180);memset(canvas[y]+x,' ',n);return 0;}
static errr publish_extra(int action,int value)
{(void)value;if(action==TERM_XTRA_CLEAR)memset(canvas,' ',sizeof(canvas));return 0;}
void __wrap_handle_stuff(void)
{
    assert(Term->wid==expected_width&&Term->hgt==expected_height&&character_icky==0);
    assert(p_ptr->redraw & PR_MAP);repairs++;
    Term_putch(expected_width-3,expected_height-3,TERM_WHITE,'R');
    p_ptr->update=p_ptr->redraw=p_ptr->window=0;
}
void __wrap_sdl_refresh_supporting_panes_layout(void) {}
void __wrap_sdl_suspend_main_view_zoom_for_saved_screen(void) {}
bool __wrap_sdl_resume_main_view_zoom_for_saved_screen(void) {return false;}
void fixture_log_return(void);
static void resize(int w,int h)
{if(Term->wid!=w||Term->hgt!=h)assert(!Term_resize(w,h));}
static void checks(void)
{
    const int sizes[][2]={{96,36},{160,56}};
    Term->text_hook=publish_text;Term->wipe_hook=publish_wipe;Term->xtra_hook=publish_extra;
    Term->higher_pict=Term->always_pict=false;
    character_generated=character_dungeon=true;turn=1616;playerturn=161;
    p_ptr->chp=53;p_ptr->csp=42;p_ptr->new_exp=40000;p_ptr->energy_use=0;
    inventory[INVEN_LITE].timeout=281;
    for(int size=0;size<2;size++)for(int direction=0;direction<2;direction++)
    for(int dead=0;dead<2;dead++)for(int nested=0;nested<2;nested++) {
        expected_width=sizes[size][0];expected_height=sizes[size][1];
        resize(expected_width,expected_height);assert(character_icky==0);
        p_ptr->playing=!dead;p_ptr->is_dead=dead;
        p_ptr->update=p_ptr->redraw=p_ptr->window=0;repairs=0;
        int x=expected_width-3,y=expected_height-3;
        Term_clear();Term_putch(x,y,TERM_WHITE,'.');Term_fresh();
        assert(canvas[y][x]=='.');
        if(nested)screen_save();
        screen_save();screen_push_supporting_panes_hidden();
        /* Normal -> Big clips all saved Term buffers. Big -> normal returns
         * to the larger grid while the reader is still open. */
        resize(expected_width*2/3,expected_height*2/3);
        if(direction)resize(expected_width,expected_height);
        resize(expected_width,expected_height); /* Restored scale/panes. */
        if(nested)Term_putch(x,y,TERM_WHITE,'N');
        memset(canvas,' ',sizeof(canvas)); /* Recreated SDL canvas. */
        fixture_log_return();
        assert(character_icky==nested);
        if(nested) {
            assert(!repairs&&Term->scr->c[y][x]=='N'&&canvas[y][x]=='N');
            screen_load();
        } else {
            assert(repairs>=1&&Term->scr->c[y][x]=='R');
            if(canvas[y][x]!='R')fprintf(stderr,"Log repair was not published size%d direction%d dead%d\n",size,direction,dead);
            assert(canvas[y][x]=='R');
        }
        assert(turn==1616&&playerturn==161&&p_ptr->energy_use==0);
        assert(p_ptr->chp==53&&p_ptr->csp==42&&p_ptr->new_exp==40000&&inventory[INVEN_LITE].timeout==281);
    }
    puts("Log cleanup real screen stack/resize: both font directions, normal/large geometry, living/postmortem/nested return, final pixels and free resources PASS");
}
'''


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT/'build-standard'
OUT = ROOT/'scripts/output/log-return-publication'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true', help='Restore the old Log cleanup')
    parser.add_argument('--no-publication', action='store_true', help='Remove only final publication')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    viewer = (ROOT/'src/cmd/ui/cmd-ui-main-menu.c').read_text()
    function_start = viewer.index('void do_cmd_messages_with_filter(')
    function_end = viewer.index('\nvoid do_cmd_messages(void)', function_start)
    start = viewer.rindex('    ui_scroll_area_clear();', function_start, function_end)
    end = viewer.rindex('\n}', start, function_end)
    cleanup = viewer[start:end]
    assert cleanup.count('    Term_fresh();') == 1
    if args.baseline:
        cleanup = cleanup[:cleanup.index('    /* Resizing for the log font')]
    elif args.no_publication:
        cleanup = cleanup.replace('    Term_fresh();', '')
    cleanup_source = OUT/'cleanup.c'
    cleanup_source.write_text('#include "angband.h"\n#include "externs.h"\n#include "ui/menu-click.h"\n'
                             'void fixture_log_return(void)\n{\n'+cleanup+'\n}\n')
    checks = CHECKS
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT/'check.c'
    source.write_text(prefix+checks+init+'checks(); SDL_Quit(); return 0;\n}\n')
    cmake = BUILD/'CMakeFiles/sil-more.dir'
    objects = shlex.split((cmake/'objects1.rsp').read_text())
    response = OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith('/src/main.c.obj')))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe = OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(cleanup_source),'@'+str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp','-Wl,--wrap=handle_stuff,--wrap=sdl_refresh_supporting_panes_layout,--wrap=sdl_suspend_main_view_zoom_for_saved_screen,--wrap=sdl_resume_main_view_zoom_for_saved_screen','-o',str(exe)],
        cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=20)


if __name__ == '__main__':
    main()

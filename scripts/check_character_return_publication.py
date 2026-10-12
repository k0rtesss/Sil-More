#!/usr/bin/env python3
"""Check Character's production cleanup publishes repaired cells after restore.

Uses the real screen stack and Term save/load/resize. A handle_stuff boundary
queues a repaired map cell after restoration; terminal hooks model the visible
canvas. This distinguishes desired cells from already published pixels without
opening a player save or depending on the SDL compositor.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/character-return-publication'

CHECKS = r'''
static char canvas[50][180];
static int repairs;
static errr publish_text(int x,int y,int n,byte attr,cptr text)
{
    (void)attr;assert(x>=0&&y>=0&&y<50&&x+n<=180);
    memcpy(canvas[y]+x,text,n);return 0;
}
static errr publish_wipe(int x,int y,int n)
{
    assert(x>=0&&y>=0&&y<50&&x+n<=180);
    memset(canvas[y]+x,' ',n);return 0;
}
static errr publish_extra(int action,int value)
{
    (void)value;if(action==TERM_XTRA_CLEAR)memset(canvas,' ',sizeof(canvas));return 0;
}
void __wrap_handle_stuff(void)
{
    /* The restored gameplay geometry must own this repair, not a menu. */
    assert(Term->wid==180&&Term->hgt==50&&character_icky==0);
    repairs++;
    Term_putch(150,35,TERM_WHITE,'A'+repairs);
    p_ptr->update=p_ptr->redraw=p_ptr->window=0;
}
void fixture_character_return(void);
static void checks(void)
{
    assert(!Term_resize(180,50));
    Term->text_hook=publish_text;Term->wipe_hook=publish_wipe;
    Term->xtra_hook=publish_extra;Term->higher_pict=false;Term->always_pict=false;
    character_generated=true;character_dungeon=true;p_ptr->playing=true;
    p_ptr->update=p_ptr->redraw=p_ptr->window=0;
    long old_turn=turn,old_player_turn=playerturn;int old_energy=p_ptr->energy_use;
    /* Cover a canvas recreation at unchanged geometry and a nested browser's
     * smaller terminal, which clips the ordinary saved screen buffer. */
    for(int nested=0;nested<2;nested++) {
        repairs=0;Term_clear();Term_putch(150,35,TERM_WHITE,'.');Term_fresh();
        assert(canvas[35][150]=='.');
        screen_save();screen_push_supporting_panes_hidden();screen_push_touch_pane_hidden();
        if(nested) {screen_save();assert(!Term_resize(90,25));screen_load();}
        (void)Term_resize(180,50);assert(Term->wid==180&&Term->hgt==50);
        memset(canvas,' ',sizeof(canvas)); /* Newly created SDL canvas. */
        fixture_character_return();
        assert(character_icky==0&&repairs>=1);
        char desired='A'+repairs;
        assert(Term->scr->c[35][150]==desired); /* Latest repaired cell. */
        if(canvas[35][150]!=desired)fprintf(stderr,"Character return queued map repair but did not publish it (nested=%d)\n",nested);
        assert(canvas[35][150]==desired);
        assert(turn==old_turn&&playerturn==old_player_turn&&p_ptr->energy_use==old_energy);
    }
    puts("Character cleanup real screen stack/resize: final repaired cell published, both geometry paths and clocks unchanged PASS");
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',action='store_true',help='Remove only final Character Term_fresh')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    character=(ROOT/'src/cmd/ui/cmd-ui-character.c').read_text()
    start=character.index('    /* Load screen */',character.index('void do_cmd_character_sheet(void)'))
    end=character.index('\n}',start)
    cleanup=character[start:end]
    assert cleanup.count('    Term_fresh();')==1
    if args.baseline:cleanup=cleanup.replace('    Term_fresh();','')
    cleanup_source=OUT/'cleanup.c'
    cleanup_source.write_text('#include "angband.h"\n#include "externs.h"\n#include "ui/menu-click.h"\nvoid fixture_character_return(void)\n{\n'+cleanup+'\n}\n',encoding='utf-8')
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    source=OUT/'check.c';source.write_text(prefix+CHECKS+init+'checks(); SDL_Quit(); return 0;\n}\n',encoding='utf-8')
    cmake=BUILD/'CMakeFiles/sil-more.dir';objects=shlex.split((cmake/'objects1.rsp').read_text())
    (OUT/'objects.rsp').write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith('/src/main.c.obj')),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([
        *(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(cleanup_source),'@'+str(OUT/'objects.rsp'),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp','-Wl,--wrap=handle_stuff','-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=20)

if __name__=='__main__':main()

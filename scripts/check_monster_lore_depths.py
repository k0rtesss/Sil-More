#!/usr/bin/env python3
"""Render production monster recall and verify authored/generic depth captions."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE,fixture_function

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/monster-lore-depths'
CHECKS=r'''
#include <ctype.h>
#undef assert
#define assert(expr) do { if(!(expr)){fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__);exit(1);} }while(0)
static void check_caption(int race,cptr expected)
{
    assert(race>0);Term_clear();display_roff(race,NULL);Term_fresh();
    char text[20000];int n=0;
    for(int y=0;y<Term->hgt;y++)for(int x=0;x<Term->wid;x++) {
        char c=Term->scr->c[y][x];if(!isspace((unsigned char)c))text[n++]=c;
    }
    text[n]=0;
    if(!strstr(text,expected))fprintf(stderr,"%s expected %s\nRecall: %s\n",r_name+r_info[race].name,expected,text);
    assert(strstr(text,expected));
}
static void checks(void)
{
    assert(!Term_resize(120,80));cheat_know=true;
    check_caption(monster_lookup_guid_text("7222d6d7c82f6571"),"atdepthsof1100feet");
    check_caption(monster_lookup_guid_text("23eb4b3c8f754c0a"),"atdepthsof1100feet");
    check_caption(monster_lookup_guid_text("f1f38a4b25dfcc33"),"atdepthsof1150feet");
    assert(r_info[31].level==3);check_caption(31,"atdepthsof150feet");
    assert(r_info[405].level>MORGOTH_DEPTH);check_caption(405,"atdepthsof1000feet");
    check_caption(R_IDX_MORGOTH,"atdepthsof1000feet");
    check_caption(R_IDX_CARCHAROTH,"guardingthegatesofAngband");
    /* Exercise the generic level-zero branch on a real race; current shipped
     * non-Carcharoth creatures all have positive native levels. */
    r_info[31].level=0;check_caption(31,"dwellsatthegatesofAngband");r_info[31].level=3;
    puts("Production recall depths: three authored Utumno guardians, ordinary depth, out-of-depth/Morgoth cap, Carcharoth and gates PASS.");
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--baseline',action='store_true');args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):];init=init[:init.index('    check_templates();')]
    source=OUT/'check.c';source.write_text(prefix+CHECKS+init+'    checks();SDL_Quit();return 0;\n}\n',encoding='utf-8')
    lore=ROOT/'src/monster/monster-lore.c'
    if args.baseline:
        text=lore.read_text(encoding='utf-8');assert text.count('monster_lore_encounter_depth(r_ptr)')==1
        lore=OUT/'lore-baseline.c';lore.write_text(text.replace('monster_lore_encounter_depth(r_ptr)','MIN(r_ptr->level, MORGOTH_DEPTH)'),encoding='utf-8')
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response=OUT/'objects.rsp';response.write_text('\n'.join('"'+p+'"'for p in objects
        if not p.endswith(('/src/main.c.obj','/src/monster/monster-lore.c.obj'))),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p)for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp','-I'+str(ROOT/'src/monster'),
        str(source),str(lore),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT)as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=15)

if __name__=='__main__':main()

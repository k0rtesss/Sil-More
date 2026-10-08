#!/usr/bin/env python3
"""Exercise translated Delvings discovery using the production song effect.

Uses initialized engine objects and isolated template caches. The optional
--index-baseline, --bounds-baseline and --map-baseline flags restore one defect
at a time without rebuilding shared objects.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/qa9-delvings-discovery"
CHECKS = r'''
#include "player/player-song-internal.h"
static const char* scenario="initialization";
#undef assert
#define assert(expr) do{if(!(expr)){fprintf(stderr,"%s: %s line%d\n",scenario,#expr,__LINE__);exit(1);}}while(0)
static void setup(int y,int x,int height,int width)
{
    memset(p_ptr,0,sizeof(*p_ptr));reset_map(1);
    p_ptr->cur_map_hgt=height;p_ptr->cur_map_wid=width;p_ptr->py=y;p_ptr->px=x;
    character_dungeon=character_generated=true;
    cave_m_idx[y][x]=-1;cave_info[y][x]=CAVE_MARK;
    turn=1001;playerturn=100;
}
static void discover(int py,int px,int dy,int dx,int feature,int height,int width)
{
    setup(py,px,height,width);int y=py+dy,x=px+dx;
    assert(in_bounds_fully(y,x));
    cave_feat[y][x]=feature;cave_info[y][x]=CAVE_HIDDEN;
    sing_song_of_delvings(23);
    assert(cave_info[y][x]&CAVE_MARK);
    if(cave_trap_bold(y,x))assert(!(cave_info[y][x]&CAVE_HIDDEN));
    assert(turn==1001&&playerturn==100);
}
static void checks(void)
{
    const int directions[][2]={{-1,0},{1,0},{0,-1},{0,1}};
    const int features[]={FEAT_TRAP_HEAD+1,FEAT_LESS,FEAT_FORGE_NORMAL_HEAD+1};
    for(int f=0;f<3;f++)for(int d=0;d<4;d++){
        scenario="translated four-axis special feature discovery";
        discover(80,80,directions[d][0]*5,directions[d][1]*5,features[f],160,160);
    }
    puts("Translated north/south/east/west trap/stair/forge discovery PASS.");
    for(int d=0;d<4;d++){
        scenario="inclusive five from newly discovered mask endpoint";
        discover(80,80,directions[d][0]*6,directions[d][1]*6,FEAT_TRAP_HEAD+1,160,160);
        scenario="outside five from fresh discovery mask";
        setup(80,80,160,160);int y=80+directions[d][0]*7,x=80+directions[d][1]*7;
        cave_feat[y][x]=FEAT_TRAP_HEAD+1;cave_info[y][x]=CAVE_HIDDEN;
        sing_song_of_delvings(23);assert(!(cave_info[y][x]&CAVE_MARK)&&(cave_info[y][x]&CAVE_HIDDEN));
    }
    puts("Inclusive +/-5 mask endpoints and +/-6 exclusions PASS.");
    scenario="small bounded map near upper left";
    discover(3,3,5,0,FEAT_TRAP_HEAD+1,40,40);discover(3,3,0,5,FEAT_LESS,40,40);
    scenario="small bounded map near lower right";
    discover(36,36,-5,0,FEAT_TRAP_HEAD+1,40,40);discover(36,36,0,-5,FEAT_LESS,40,40);
    scenario="outside bounded map remains unchanged";
    setup(36,36,40,40);cave_feat[41][36]=FEAT_TRAP_HEAD+1;cave_info[41][36]=CAVE_HIDDEN;
    sing_song_of_delvings(23);assert(cave_info[41][36]==CAVE_HIDDEN);
    scenario="rectangular local matrix near top edge";
    discover(3,80,5,0,FEAT_TRAP_HEAD+1,40,160);discover(3,80,0,5,FEAT_LESS,40,160);
    scenario="rectangular local matrix near left edge";
    discover(80,3,5,0,FEAT_TRAP_HEAD+1,160,40);discover(80,3,0,5,FEAT_LESS,160,40);
    scenario="last interior physical row and column";
    discover(MAX_DUNGEON_HGT-2,MAX_DUNGEON_WID-2,-3,0,FEAT_TRAP_HEAD+1,MAX_DUNGEON_HGT,MAX_DUNGEON_WID);
    discover(MAX_DUNGEON_HGT-2,MAX_DUNGEON_WID-2,0,-3,FEAT_LESS,MAX_DUNGEON_HGT,MAX_DUNGEON_WID);
    scenario="allocated map edge";
    discover(MAX_DUNGEON_HGT-4,MAX_DUNGEON_WID-4,-5,0,FEAT_TRAP_HEAD+1,MAX_DUNGEON_HGT,MAX_DUNGEON_WID);
    scenario="outside authored score+8 range";
    setup(80,80,160,160);cave_feat[112][80]=FEAT_LESS;sing_song_of_delvings(23);assert(!(cave_info[112][80]&CAVE_MARK));
    scenario="adjacent secret door";
    setup(80,80,160,160);cave_feat[80][79]=FEAT_SECRET;sing_song_of_delvings(23);
    assert(cave_any_closed_door_bold(80,79)&&(cave_info[80][79]&CAVE_MARK));
    puts("Bounded/rectangular maps, last-interior/allocated edges, score-range exclusion and secret door PASS.");
}
'''



def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--index-baseline',action='store_true')
    parser.add_argument('--bounds-baseline',action='store_true')
    parser.add_argument('--map-baseline',action='store_true')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix+=fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    check=OUT/'check.c';check.write_text(prefix+CHECKS+init+'    checks();SDL_Quit();return 0;\n}\n',encoding='utf-8')
    source=ROOT/'src/player/player-song-effects.c'
    if args.index_baseline:
        current=source.read_text(encoding='utf-8')
        current=current.replace('delvings[((j - min_y) * x_range) + dx]','delvings[(j * x_range) + dx]').replace('delvings[(dy * x_range) + (i - min_x)]','delvings[(dy * x_range) + i]')
        source=OUT/'effects-index-baseline.c';source.write_text(current,encoding='utf-8')
    if args.bounds_baseline:
        assert not args.index_baseline
        current=source.read_text(encoding='utf-8').replace('MIN(max_y, y + 6)','MIN(max_y, y + 5)').replace('MIN(max_x, x + 6)','MIN(max_x, x + 5)')
        source=OUT/'effects-bounds-baseline.c';source.write_text(current,encoding='utf-8')
    if args.map_baseline:
        assert not args.index_baseline and not args.bounds_baseline
        current=source.read_text(encoding='utf-8').replace('MIN(p_ptr->cur_map_hgt, py + range + 1)','MIN(MAX_DUNGEON_HGT, py + range + 1)').replace('MIN(p_ptr->cur_map_wid, px + range + 1)','MIN(MAX_DUNGEON_WID, px + range + 1)')
        source=OUT/'effects-map-baseline.c';source.write_text(current,encoding='utf-8')
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text());response=OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/player/player-song-effects.c.obj'))),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(check),str(source),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=30)

if __name__=='__main__':main()

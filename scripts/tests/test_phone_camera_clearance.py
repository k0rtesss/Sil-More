"""Production camera fallback checks for real phone HUD blockers; no game data."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / 'src/world/panel.c').read_text()
SOURCE = SOURCE[SOURCE.index('/* Screen-cell rectangle'):]
PREFIX = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#define USE_SDL 1
#define MAX_PANE_CONFIGS 32
#define ABS(x) ((x)<0?-(x):(x))
#define SGN(x) ((x)<0?-1:((x)>0?1:0))
#define mem_alloc_array(n,t) ((t*)calloc((n),sizeof(t)))
#define mem_free_null(x) do {free(x);(x)=NULL;} while(0)
struct player {int py,px,wy,wx,depth,cur_map_hgt,cur_map_wid; unsigned redraw,window;bool running,smithing_starting;};
static struct player player,*p_ptr=&player;
struct term {int wid,hgt;};static struct term terminal,*Term=&terminal;
#define SCREEN_HGT (Term->hgt)
#define SCREEN_WID ((Term->wid+1)/2)
#define PR_MAP 1
#define PW_OVERHEAD 1
#define PANEL_HGT 11
#define PANEL_WID 16
static int ddy[10],ddx[10];
static bool use_bigtile=true,center_player=false,run_avoid_center=false,path_following=true;
static int disturbed,requested_vertical=3,requested_horizontal=3;
static int game_turn=96,player_turn=10;
static int get_sdl_camera_center_clearance_vertical(void){return requested_vertical;}
static int get_sdl_camera_center_clearance_horizontal(void){return requested_horizontal;}
static bool sdl_mouse_path_is_following(void){return path_following;}
static void disturb(int a,int b){disturbed++;}
static bool panel_contains(int y,int x){return y>=player.wy&&y<player.wy+SCREEN_HGT&&x>=player.wx&&x<player.wx+SCREEN_WID;}
struct rect {int x,y,w,h;};
static struct rect blockers[12];static int blocker_count,grid_x,grid_y,tile;
static int floor_div(int n,int d){return n>=0?n/d:-((-n+d-1)/d);}
static int ceil_div(int n,int d){return -floor_div(-n,d);}
static int sdl_map_overlay_map_coverages(int max,int *xs,int *ws,int *ys,int *hs){
    int n=0;
    for(int i=0;i<blocker_count&&n<max;i++){
        struct rect r=blockers[i];int x1=floor_div(r.x-grid_x,tile),y1=floor_div(r.y-grid_y,tile);
        int x2=ceil_div(r.x+r.w-grid_x,tile),y2=ceil_div(r.y+r.h-grid_y,tile);
        if(x1<0)x1=0;if(y1<0)y1=0;if(x2>SCREEN_WID)x2=SCREEN_WID;if(y2>SCREEN_HGT)y2=SCREEN_HGT;
        if(x2<=x1||y2<=y1)continue;
        xs[n]=x1;ws[n]=x2-x1;ys[n]=y1;hs[n]=y2-y1;n++;
    }
    return n;
}
'''
CHECKS = r'''
static bool intersects(struct rect a,struct rect b){return a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y;}
static bool whole_player_tile_clear(void){
    struct rect hero={grid_x+(player.px-player.wx)*tile,grid_y+(player.py-player.wy)*tile,tile,tile};
    for(int i=0;i<blocker_count;i++)if(intersects(hero,blockers[i]))return false;
    return true;
}
static void initialize(int term_w,int term_h,int cell,int origin_x,int origin_y){
    terminal=(struct term){term_w,term_h};tile=cell;grid_x=origin_x;grid_y=origin_y;
    player=(struct player){.py=64,.px=16,.wy=61,.wx=9,.depth=1,.cur_map_hgt=150,.cur_map_wid=80};
    player.wy=player.py-SCREEN_HGT/2;player.wx=player.px-SCREEN_WID/2;
    blocker_count=0;requested_vertical=requested_horizontal=3;game_turn=96;player_turn=10;disturbed=0;
}
static int check_hd(void){
    /* final12 HD: terminal30x7,96px tiles; rendered log x849–1600,y96–507. */
    initialize(30,7,96,148,0);
    blockers[blocker_count++]=(struct rect){188,0,336,222};
    blockers[blocker_count++]=(struct rect){849,96,751,411};
    blockers[blocker_count++]=(struct rect){188,373,336,142};
    blockers[blocker_count++]=(struct rect){532,584,824,96};
    blockers[blocker_count++]=(struct rect){1252,506,308,52};
    blockers[blocker_count++]=(struct rect){1344,0,216,96};
    assert(!whole_player_tile_clear());verify_panel();
    if(!whole_player_tile_clear()){fputs("HD player tile still intersects painted blocker\n",stderr);return 2;}
    assert(requested_vertical==3&&requested_horizontal==3);
    int wy=player.wy,wx=player.wx;
    for(int i=0;i<4;i++){verify_panel();assert(player.wy==wy&&player.wx==wx);}
    assert(game_turn==96&&player_turn==10&&disturbed==0);
    /* A paid step remains paid once; camera maintenance spends no extra turn
       and does not cancel a following path or run. */
    for(int direction=-1;direction<=1;direction+=2){
        player.running=true;player.px+=direction;game_turn++;player_turn++;
        int before_turn=game_turn,before_player_turn=player_turn;
        verify_panel();assert(whole_player_tile_clear());
        assert(game_turn==before_turn&&player_turn==before_player_turn&&player.running&&path_following&&disturbed==0);
    }
    return 0;
}
static void other_grids(void){
    /* Actual cold 4a terminal45x11, plus portrait phones at the same tile size. */
    int grids[][2]={{45,11},{14,16},{20,24},{21,25},{50,11},{48,11}};
    for(int i=0;i<6;i++){
        initialize(grids[i][0],grids[i][1],96,0,0);
        int w=SCREEN_WID*tile,h=SCREEN_HGT*tile;
        blockers[blocker_count++]=(struct rect){0,0,w/4,h/4};
        blockers[blocker_count++]=(struct rect){w*55/100,h/10,w*45/100,h*45/100};
        blockers[blocker_count++]=(struct rect){0,h/2,w/4,h/5};
        blockers[blocker_count++]=(struct rect){w/4,h*4/5,w*3/4,h/5};
        verify_panel();assert(whole_player_tile_clear());
        int wy=player.wy,wx=player.wx;verify_panel();assert(player.wy==wy&&player.wx==wx);
        assert(requested_vertical==3&&requested_horizontal==3);
    }
}
static void normal_and_best_effort(void){
    initialize(45,24,96,0,0);struct map_pane_span spans[2];int cy,cx;
    map_safe_center(&cy,&cx,spans,0,12,11,0,0);
    assert(cy==(SCREEN_HGT-1)/2&&cx==(SCREEN_WID-1)/2);
    /* The normal lead calculation still selects from the requested safe zone. */
    map_safe_center(&cy,&cx,spans,0,12,11,1,1);
    assert(cy==(SCREEN_HGT-1)/2-3&&cx==(SCREEN_WID-1)/2-3);
    initialize(30,7,96,0,0);
    spans[0]=(struct map_pane_span){0,0,SCREEN_WID,SCREEN_HGT};
    map_safe_center(&cy,&cx,spans,1,2,3,0,0);assert(cy==2&&cx==3);
    spans[1]=(struct map_pane_span){2,1,5,4};
    map_safe_center(&cy,&cx,spans,2,2,3,0,0);
    assert(!(cx>=2&&cx<5&&cy>=1&&cy<4));
}
int main(void){int result=check_hd();if(result)return result;other_grids();normal_and_best_effort();puts("Phone camera: HD blockers, whole tile safety, cold idle stability, paid move invariants, other grids PASS");return 0;}
'''


def main():
    msys = Path(r'C:\msys64\mingw64\bin')
    cc = os.environ.get('CC') or shutil.which('gcc') or str(msys / 'gcc.exe')
    env = os.environ.copy()
    env['PATH'] = str(msys) + os.pathsep + str(msys.parent / 'usr/bin') + os.pathsep + env.get('PATH', '')
    old = SOURCE[:SOURCE.index('/* Find the closest whole map cell')] + SOURCE[SOURCE.index('/* Select the visible zone nearest'):]
    old = old.replace('    if (!have_zone_center)\n        map_fallback_safe_center(screen_h, screen_w, spans, span_count,\n            &clearance, anchor_y, anchor_x, &cy, &cx);\n', '')
    with tempfile.TemporaryDirectory(prefix='sil-phone-camera-') as temporary:
        temporary = Path(temporary)
        for name, source in [('fixed', SOURCE), ('before', old)]:
            cfile = temporary / (name + '.c')
            exe = temporary / (name + '.exe')
            cfile.write_text(PREFIX + source + CHECKS)
            subprocess.run([cc, '-std=c17', '-O0', '-Wall', '-Wextra', str(cfile), '-o', str(exe)], check=True, env=env, capture_output=True)
            result = subprocess.run([str(exe)], env=env, capture_output=True, text=True)
            if name == 'fixed':
                if result.returncode:
                    raise RuntimeError(result.stdout + result.stderr)
                print(result.stdout.strip())
            else:
                assert result.returncode == 2, result.stdout + result.stderr
                assert 'HD player tile still intersects' in result.stderr
                print('Before-change negative: reproduced HD player tile occlusion')


if __name__ == '__main__':
    main()

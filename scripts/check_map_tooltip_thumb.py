#!/usr/bin/env python3
"""Compile the real map-tooltip placement against observed mobile geometry."""
from pathlib import Path
import os, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'scripts/output/map-tooltip-thumb'
def function(source, marker):
    start=source.index(marker); brace=source.index('{',start); depth=1; end=brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}'); end+=1
    return source[start:end]
def main():
    source=(ROOT/'src/sdl/input/sdl-tooltips.c').read_text(encoding='utf-8-sig')
    helper=function(source,'static bool sdl_object_tooltip_box_clear')+'\n'+function(source,'static bool sdl_object_tooltip_avoid_controls')
    padding=function(source,'static bool sdl_object_tooltip_place_map_box')
    render=function(source,'void sdl_object_tooltip_render(void)')
    assert 'if (!g_object_tooltip.term_cell && !g_object_tooltip.screen_rect)' in render
    assert 'sdl_touch_thumb_current_bounds(&controls[count])' in render
    for api in ('sdl_touch_top_panel_compute_layout','sdl_touch_pane_current_rect','sdl_touch_round_compute_layout'):assert api in render
    assert 'have_player ? &player_rect : NULL' in render
    assert render.count('box.y + vertical_pad + (float)(run->line * line_h)')==2
    if '--baseline' in sys.argv: helper=function(source,'static bool sdl_object_tooltip_box_clear')+'\n'+function(source,'static bool sdl_object_tooltip_avoid_controls').split('{',1)[0]+'{(void)box;(void)controls;(void)count;(void)player;(void)screen;(void)margin;(void)gap;return false;}'
    if '--full-padding' in sys.argv: padding=padding.split('{',1)[0]+'{(void)vertical_pad;return sdl_object_tooltip_avoid_controls(box,controls,count,player,screen,margin,gap)&&sdl_object_tooltip_box_clear(box,controls,count,player,screen,margin);}'
    if '--skip-minimum' in sys.argv: padding=padding.replace('trial_pad = trial_pad > 8.0f ? trial_pad - 1.0f : 7.0f;', 'trial_pad -= 1.0f; if (trial_pad < 7.0f) break;')
    code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
typedef struct {float x,y,w,h;} SDL_FRect;
typedef struct {int x,y,w,h;} SDL_Rect;
static bool sdl_description_overlay_rects_intersect(const SDL_FRect*a,const SDL_FRect*b)
{return a->x<b->x+b->w&&a->x+a->w>b->x&&a->y<b->y+b->h&&a->y+a->h>b->y;}
__HELPER__
static void check(SDL_FRect initial,SDL_FRect thumb,SDL_FRect*player,SDL_Rect screen,bool must_move)
{SDL_FRect b=initial;sdl_object_tooltip_avoid_controls(&b,&thumb,1,player,&screen,10,7);
 assert(b.w==initial.w&&b.h==initial.h);
 if(must_move){assert(!sdl_description_overlay_rects_intersect(&b,&thumb));if(player)assert(!sdl_description_overlay_rects_intersect(&b,player));
 assert(b.x>=screen.x+10&&b.y>=screen.y+10&&b.x+b.w<=screen.x+screen.w-10&&b.y+b.h<=screen.y+screen.h-10);}
 else assert(b.x==initial.x&&b.y==initial.y);}
int main(void){
 SDL_Rect landscape={0,0,1600,720},portrait={0,0,720,1600};
 SDL_FRect thumb={50,161,182,279},player={640,230,64,64};
 /* Prefer lateral clearance: moving upward here would hit the center Menu. */
 SDL_FRect exact={154,170,1164,52};sdl_object_tooltip_avoid_controls(&exact,&thumb,1,&player,&landscape,10,7);assert(exact.x==239&&exact.y==170);
 /* Exact normal landscape alarm/Echoing tooltip; enlarged height and width. */
 check((SDL_FRect){154,170,1164,52},thumb,&player,landscape,true);
 check((SDL_FRect){90,70,1200,150},thumb,&player,landscape,true);
 /* Portrait unchanged when popup is clear of the actual bottom thumb zone. */
 check((SDL_FRect){70,500,640,90},(SDL_FRect){509,987,160,443},NULL,portrait,false);
 /* Sideways move blocked by player: above/below remains available. */
 SDL_FRect blocking={239,170,64,64};check((SDL_FRect){154,170,1164,52},thumb,&blocking,landscape,true);
 /* A right-hand thumb uses the left-hand candidate. */
 check((SDL_FRect){200,200,1200,55},(SDL_FRect){1300,160,200,300},NULL,landscape,true);
 /* Offset viewport, absent/empty control, and no-room fallback. */
 check((SDL_FRect){254,270,1164,52},(SDL_FRect){150,261,182,279},NULL,(SDL_Rect){100,100,1600,720},true);
 check((SDL_FRect){154,170,1164,52},(SDL_FRect){0},NULL,landscape,false);
 check((SDL_FRect){10,10,1580,700},thumb,NULL,landscape,false);
 SDL_FRect b={154,170,1164,52},old=b;sdl_object_tooltip_avoid_controls(&b,NULL,0,NULL,&landscape,10,7);assert(b.x==old.x&&b.y==old.y);
 /* Exact global-enlarged runtime controls: viewport-edge band is necessary. */
 SDL_FRect big_controls[]={{444,323,735,288},{1192,303,406,406}},big_player={400,230,96,96},big_box={10,338,1547,194};
 sdl_object_tooltip_avoid_controls(&big_box,big_controls,2,&big_player,&landscape,10,12);
 assert(big_box.y==10);assert(sdl_object_tooltip_box_clear(&big_box,big_controls,2,&big_player,&landscape,10));
 SDL_FRect portrait_controls[]={{526,638,177,500},{16,1147,688,269},{20,647,490,490}},portrait_player={380,468,96,96},portrait_box={20,577,690,442};
 sdl_object_tooltip_avoid_controls(&portrait_box,portrait_controls,3,&portrait_player,&portrait,10,12);
 assert(portrait_box.y==10);assert(sdl_object_tooltip_box_clear(&portrait_box,portrait_controls,3,&portrait_player,&portrait,10));
 /* A safe-area inset that removes the free band retains the original fallback. */
 SDL_FRect impossible={10,338,1547,194};sdl_object_tooltip_avoid_controls(&impossible,big_controls,2,&big_player,&(SDL_Rect){0,40,1600,680},10,12);assert(impossible.y==338);
 /* Measured Android safe-area/line metrics: full padding cannot fit,
  * six pixels per side reclaimed without changing two 83px text lines. */
 SDL_FRect measured_controls[]={{444.5f,323,735.2f,287.8f},{1190,302,410,410}},measured_player={384,230,96,96};
 SDL_Rect safe={0,38,1600,682};SDL_FRect measured={10,338,1548,194};float vpad=14;
 sdl_object_tooltip_place_map_box(&measured,&vpad,measured_controls,2,&measured_player,&safe,10,12);
 assert(vpad==8&&measured.y==48&&measured.h==182&&measured.w==1548);
 assert(measured.h==2*83+2*vpad);assert(measured.y+vpad+2*83<=measured.y+measured.h-vpad);
 assert(sdl_object_tooltip_box_clear(&measured,measured_controls,2,&measured_player,&safe,10));
 SDL_FRect never={10,338,1548,194};vpad=14;SDL_FRect full={0,0,1600,720};
 sdl_object_tooltip_place_map_box(&never,&vpad,&full,1,&measured_player,&safe,10,12);assert(vpad==14&&never.h==194&&never.y==338);
 SDL_FRect untouched={300,80,400,52};vpad=14;sdl_object_tooltip_place_map_box(&untouched,&vpad,measured_controls,2,NULL,&safe,10,12);assert(vpad==14&&untouched.h==52&&untouched.y==80);
 /* Fractional role-font padding must still try the exact minimum7. */
 for(int i=0;i<2;i++) {
  float fractional=i?8.64f:7.92f;SDL_FRect narrow={10,70,380,44+2*fractional},lower={0,68,400,32},occupant={0,68,400,20};SDL_Rect viewport={0,0,400,100};
  sdl_object_tooltip_place_map_box(&narrow,&fractional,&lower,1,&occupant,&viewport,10,7);
  assert(fractional==7&&narrow.y==10&&narrow.h==58);
 }
 puts("Map tooltip: observed normal/big landscape, portrait, player avoidance, clipping and impossible-space fallback PASS");}
'''.replace('__HELPER__',helper+'\n'+padding)
    OUT.mkdir(parents=True,exist_ok=True); c=OUT/'check.c'; c.write_text(code)
    env=os.environ.copy();env['PATH']=os.pathsep.join(['C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe';subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-std=c17','-Wall','-Wextra',str(c),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True,timeout=30)
if __name__=='__main__':main()

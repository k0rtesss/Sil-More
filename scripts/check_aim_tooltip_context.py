#!/usr/bin/env python3
"""Check native aim/Unified Look tooltip owners, nesting and visible wheels."""
from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'scripts/output/aim-tooltip-context'
def main():
 s=(ROOT/'src/sdl/input/sdl-tooltips.c').read_text();a=s.index('static bool sdl_object_tooltip_map_context_active');b=s.index('\n}\n',a)+2;guard=s[a:b]
 a=s.index('        if (sdl_touch_round_layer_config_enabled()',s.index('void sdl_object_tooltip_render'));b=s.index('&& sdl_touch_round_compute_layout',a);wheel=s[a:b].strip()[4:].strip()
 look_source=(ROOT/'src/sdl/input/sdl-map-geometry.c').read_text();a=look_source.index('bool sdl_unified_look_pointer_input_active(void)');b=look_source.index('\n}\n',a)+2;look=look_source[a:b]
 if '--look-negative' in sys.argv:guard=guard.replace('(sdl_unified_look_pointer_input_active() && character_icky == 1)', 'false')
 if '--look-pause-negative' in sys.argv:look=look.replace(' && g_unified_look_map_hover_enabled','')
 if '--baseline' in sys.argv:guard=guard.split('{',1)[0]+'{return sdl_mouse_gameplay_context_active();}'
 if '--wheel-negative' in sys.argv:wheel='sdl_touch_round_layer_config_enabled() && sdl_mouse_gameplay_context_active()'
 code=r'''
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#define PANE_MAIN 0
static bool gameplay,layer=true,character_generated=true,character_dungeon=true;static int character_icky;
static struct {bool active,select_mode,select_location;}g_pointer_aim;
static bool g_unified_look_active,g_unified_look_map_hover_enabled;
__LOOK_OWNER__
static struct {bool term_ready;}g_views[1]={{true}};
static struct {bool playing,leaving,is_dead;}player={true,false,false},*p_ptr=&player;
static bool sdl_mouse_gameplay_context_active(void){return gameplay;}
static bool sdl_touch_round_layer_config_enabled(void){return layer;}
__GUARD__
static bool wheel_context(void){return __WHEEL__;}
int main(void){
 /* Real owner: footer click UI is active, ordinary mouse gameplay is false. */
 gameplay=false;g_pointer_aim.active=g_pointer_aim.select_mode=true;
 assert(sdl_object_tooltip_map_context_active());assert(wheel_context());
 layer=false;assert(!wheel_context());layer=true;
 g_pointer_aim.select_location=true;assert(sdl_object_tooltip_map_context_active());assert(!wheel_context());g_pointer_aim.select_location=false;
 g_pointer_aim.select_mode=false;assert(!sdl_object_tooltip_map_context_active());assert(!wheel_context());g_pointer_aim.select_mode=true;
 g_pointer_aim.active=false;assert(!sdl_object_tooltip_map_context_active());assert(!wheel_context());
 gameplay=true;assert(sdl_object_tooltip_map_context_active()&&wheel_context());gameplay=false;g_pointer_aim.active=true;
 player.playing=false;assert(!sdl_object_tooltip_map_context_active());player.playing=true;
 player.leaving=true;assert(!sdl_object_tooltip_map_context_active());player.leaving=false;
 player.is_dead=true;assert(!sdl_object_tooltip_map_context_active());player.is_dead=false;
 character_generated=false;assert(!sdl_object_tooltip_map_context_active());character_generated=true;
 character_dungeon=false;assert(!sdl_object_tooltip_map_context_active());character_dungeon=true;
 g_views[0].term_ready=false;assert(!sdl_object_tooltip_map_context_active());g_views[0].term_ready=true;
 character_icky=1;assert(!sdl_object_tooltip_map_context_active());character_icky=0;
 /* Look paints exactly one saved clean-map screen; nested Info pauses hover. */
 g_pointer_aim.active=false;g_unified_look_active=g_unified_look_map_hover_enabled=true;
 assert(!sdl_object_tooltip_map_context_active());character_icky=1;
 assert(sdl_object_tooltip_map_context_active());assert(!wheel_context());
 character_icky=2;assert(!sdl_object_tooltip_map_context_active());character_icky=1;
 g_unified_look_map_hover_enabled=false;assert(!sdl_object_tooltip_map_context_active());
 character_icky=2;assert(!sdl_object_tooltip_map_context_active());character_icky=1;
 g_unified_look_map_hover_enabled=true;assert(sdl_object_tooltip_map_context_active());
 g_unified_look_active=false;assert(!sdl_object_tooltip_map_context_active());g_unified_look_active=true;
 player.playing=false;assert(!sdl_object_tooltip_map_context_active());player.playing=true;
 player.leaving=true;assert(!sdl_object_tooltip_map_context_active());player.leaving=false;
 player.is_dead=true;assert(!sdl_object_tooltip_map_context_active());player.is_dead=false;
 character_generated=false;assert(!sdl_object_tooltip_map_context_active());character_generated=true;
 character_dungeon=false;assert(!sdl_object_tooltip_map_context_active());character_dungeon=true;
 g_views[0].term_ready=false;assert(!sdl_object_tooltip_map_context_active());g_views[0].term_ready=true;
 p_ptr=NULL;assert(!sdl_object_tooltip_map_context_active());
 puts("Native aim and Unified Look tooltip owners, paused/nested modal exclusion, lifecycle and visible-wheel gates PASS");}
'''.replace('__LOOK_OWNER__',look).replace('__GUARD__',guard).replace('__WHEEL__',wheel)
 OUT.mkdir(parents=True,exist_ok=True);c=OUT/'check.c';c.write_text(code);env=os.environ.copy();env['PATH']=os.pathsep.join(['C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']]);exe=OUT/'check.exe';subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-std=c17','-Wall','-Wextra',str(c),'-o',str(exe)],env=env,check=True);subprocess.run([str(exe)],env=env,check=True,timeout=30)
if __name__=='__main__':main()

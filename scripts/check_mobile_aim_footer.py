#!/usr/bin/env python3
"""Render the real native aim footer and verify control clearance and action IDs."""
from pathlib import Path
import os,shlex,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build-standard';OUT=ROOT/'scripts/output/mobile-aim-footer'
HARNESS=r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "ui/targeting/targeting-internal.h"
#include <assert.h>
#undef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD FIXTURE_MOBILE
#include "sdl/render/sdl-fonts.c"
static cptr tooltip_payload="a Wolf [HP: ******-- Morale: Confident 8]; an Arrow";static bool gauntlets_payload;
__PRODUCTION__
__TOOLTIPS__
static int fixture_weapon_kind=PLAYER_ACTIVE_WEAPON_KIND_BOW;
int __wrap_player_active_weapon_kind(void){return fixture_weapon_kind;}
static bool fixture_controller,throw_payload,controller_five;
bool __wrap_steamdeck_controls_active(void){return fixture_controller;}
void __wrap_target_prompt_label(int binding,cptr fallback,char*out,size_t n){(void)binding;SDL_strlcpy(out,fallback,n);}
__THUMB_LABEL__
__AIM_PROMPT__
static void fixture_prompt_checks(void){
 for(int controller=0;controller<2;controller++){fixture_controller=controller;temp_n=2;
  const int commands[]={'t',KTRL('T'),'f','F','f','p'};
  for(unsigned i=0;i<N_ELEMENTS(commands);i++){p_ptr->command_cmd=commands[i];fixture_weapon_kind=i==4?PLAYER_ACTIVE_WEAPON_KIND_BOW:PLAYER_ACTIVE_WEAPON_KIND_THROWING;
   for(int vertical=0;vertical<2;vertical++){aim_select_show_prompt(false,vertical,false,NULL);assert(g_unified_look_prompt.count==(controller?5:3)+2*vertical);char expected[40];strnfmt(expected,sizeof(expected),"%s %s",controller?"A":"f",i<4?"Throw":"Fire");assert(!strcmp(g_unified_look_prompt.buttons[0].labels[0],expected));assert(g_unified_look_prompt.buttons[0].choice=='f');char thumb_label[64];fixture_thumb_selected_label(thumb_label,sizeof(thumb_label));bool short_label=FIXTURE_MOBILE&&(config.bigger_font||get_sdl_menu_bigger_font_for(SDL_MENU_FONT_LOOK));assert(!strcmp(thumb_label,i<4?(short_label?"Throw":"Throw at target"):(short_label?"Fire":"Fire at target")));}
  }
  p_ptr->command_cmd='t';aim_select_show_prompt(false,false,true,"Teleport");assert(g_unified_look_prompt.count==2);assert(!strcmp(g_unified_look_prompt.buttons[0].labels[0],controller?"A Teleport":"Space Teleport"));assert(g_unified_look_prompt.buttons[0].choice=='t');
  aim_select_show_prompt(false,false,true,NULL);assert(strstr(g_unified_look_prompt.buttons[0].labels[0],"Select"));
  temp_n=1;aim_select_show_prompt(false,false,false,NULL);assert(g_unified_look_prompt.count==3);temp_n=2;aim_select_show_prompt(true,false,false,NULL);assert(g_unified_look_prompt.count==3);assert(strstr(g_unified_look_prompt.buttons[1].labels[0],"Targets"));
 }
 char label[64];g_pointer_aim.select_location=true;fixture_thumb_selected_label(label,sizeof(label));assert(!strcmp(label,"Ranged Attack"));g_pointer_aim.select_location=false;g_pointer_aim.active=false;fixture_thumb_selected_label(label,sizeof(label));assert(!strcmp(label,"Ranged Attack"));g_pointer_aim.active=true;
}
static SDL_FRect fixture_center_rect;
static cptr fixture_wheel_center_label(void){float cy=100,inner_radius=90;SDL_FRect center_arrow={0,0,166,166};__CENTER_LABEL__ fixture_center_rect=center_arrow;return center_label;}
static int width,height;static float density;static SDL_FRect thumb,toolbar,wheel,player_rect,menu_button,font_button;static bool controls_on=true,far_target;static bool capturing_tooltip;static int captured_fills;static SDL_FRect tooltip_box;
bool __real_SDL_RenderFillRect(SDL_Renderer*r,const SDL_FRect*rect);
bool __real_SDL_RenderTexture(SDL_Renderer*r,SDL_Texture*t,const SDL_FRect*src,const SDL_FRect*dst);
bool __wrap_SDL_RenderTexture(SDL_Renderer*r,SDL_Texture*t,const SDL_FRect*src,const SDL_FRect*dst){if(capturing_tooltip&&dst){assert(dst->x>=tooltip_box.x-1&&dst->y>=tooltip_box.y-1&&dst->x+dst->w<=tooltip_box.x+tooltip_box.w+1&&dst->y+dst->h<=tooltip_box.y+tooltip_box.h+1);}return __real_SDL_RenderTexture(r,t,src,dst);}
bool __wrap_SDL_RenderFillRect(SDL_Renderer*r,const SDL_FRect*rect){if(capturing_tooltip&&captured_fills++==0){assert(rect);tooltip_box=*rect;}return __real_SDL_RenderFillRect(r,rect);}
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID id){(void)id;return density;}
SDL_Rect __wrap_sdl_get_layout_screen_rect(void){return (SDL_Rect){0,height>width?85:38,width,height-(height>width?85:38)};}
SDL_Rect __wrap_sdl_menu_content_rect(SDL_Rect r){return r;}
bool __wrap_sdl_touch_thumb_current_bounds(SDL_FRect*r){*r=thumb;return controls_on&&thumb.w>0;}
bool __wrap_sdl_touch_top_panel_compute_layout(SDL_FRect*b,SDL_FRect*r){(void)b;if(r)*r=toolbar;return false;}
bool __wrap_sdl_main_menu_pane_button_rect(SDL_FRect*r){*r=menu_button;return menu_button.w>0;}
SDL_FRect __wrap_sdl_menu_font_button_bounds(void){return font_button;}
bool __wrap_sdl_touch_pane_current_rect(SDL_Rect*r){(void)r;return false;}
bool __wrap_sdl_touch_round_layer_config_enabled(void){return controls_on&&wheel.w>0;}
bool __wrap_sdl_mouse_gameplay_context_active(void){return false;}
bool __wrap_sdl_touch_round_compute_layout(float*x,float*y,float*r,float*i,SDL_Rect*c){(void)i;(void)c;if(x)*x=wheel.x+wheel.w/2;if(y)*y=wheel.y+wheel.h/2;if(r)*r=wheel.w/2;return controls_on&&wheel.w>0;}
bool __wrap_sdl_player_map_rect(int y,int x,SDL_FRect*r){*r=(y==p_ptr->py&&x==p_ptr->px)?player_rect:(SDL_FRect){player_rect.x,far_target?height-120:player_rect.y-64,64,64};return true;}
static bool overlap(SDL_FRect a,SDL_FRect b){return a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y;}
static void run(int w,int h,bool big,int count,cptr out,bool look_only){
 width=w;height=h;far_target=look_only;density=FIXTURE_MOBILE?2.625f:1;config.bigger_font=big&&!look_only;for(int i=0;i<SDL_MENU_FONT_COUNT;i++)config.menu_bigger_font[i]=look_only?i==SDL_MENU_FONT_LOOK:big;
 g_state.system_scale=density;g_state.window=SDL_CreateWindow("Aim fixture",w,h,SDL_WINDOW_HIDDEN);assert(g_state.window);g_state.renderer=SDL_CreateRenderer(g_state.window,"software");assert(g_state.renderer);
 sdl_view*v=&g_views[PANE_MAIN];memset(v,0,sizeof(*v));v->cell_w=16;v->cell_h=32;v->rect=(SDL_Rect){0,h>w?85:38,w,h-(h>w?85:38)};v->cols=w/16;v->rows=v->rect.h/32;term_init(&v->t,v->cols,v->rows,256);Term_activate(&v->t);v->term_ready=true;v->canvas=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,w,h);assert(v->canvas);
 g_views[PANE_LOG].term_ready=true;g_views[PANE_LOG].cell_h=24;
 if(h>w){thumb=(SDL_FRect){509,987,160,443};toolbar=(SDL_FRect){0};wheel=(SDL_FRect){59,995,426,426};player_rect=(SDL_FRect){328,598,64,64};}
 else {thumb=big?(SDL_FRect){0}:(SDL_FRect){50,161,182,279};toolbar=big?(SDL_FRect){444.5f,323,735.2f,287.8f}:(SDL_FRect){740,38,118,126};wheel=big?(SDL_FRect){1190,302,410,410}:(SDL_FRect){1219,238,380,380};player_rect=big?(SDL_FRect){384,230,96,96}:(SDL_FRect){640,230,64,64};}
 menu_button=(h<w&&!config.bigger_font)?(SDL_FRect){740,38,118,126}:(SDL_FRect){0};font_button=(SDL_FRect){w-116,h>w?96:50,104,104};
 g_pointer_aim.active=true;g_pointer_aim.select_mode=true;g_pointer_aim.select_location=false;
 fixture_prompt_checks();fixture_controller=count==7||controller_five;temp_n=2;p_ptr->command_cmd='f';fixture_weapon_kind=throw_payload?PLAYER_ACTIVE_WEAPON_KIND_THROWING:PLAYER_ACTIVE_WEAPON_KIND_BOW;
 aim_select_show_prompt(false,count!=3&&!controller_five,false,NULL);sdl_menu_font_set_render_context(SDL_MENU_FONT_LOOK);
 int keys[7];for(int i=0;i<count;i++)keys[i]=g_unified_look_prompt.buttons[i].choice;
 assert(!strcmp(g_unified_look_prompt.buttons[0].labels[0],throw_payload?(fixture_controller?"A Throw":"f Throw"):(fixture_controller?"A Fire":"f Fire")));
 p_ptr->command_cmd='f';fixture_weapon_kind=PLAYER_ACTIVE_WEAPON_KIND_BOW;assert(!strcmp(fixture_wheel_center_label(),FIXTURE_MOBILE&&big?"Fire":"Fire direction"));
 p_ptr->command_cmd='t';assert(!strcmp(fixture_wheel_center_label(),FIXTURE_MOBILE&&big?"Throw":"Throw direction"));
 p_ptr->command_cmd='f';fixture_weapon_kind=PLAYER_ACTIVE_WEAPON_KIND_THROWING;assert(!strcmp(fixture_wheel_center_label(),FIXTURE_MOBILE&&big?"Throw":"Throw direction"));
 p_ptr->command_cmd='p';assert(!strcmp(fixture_wheel_center_label(),FIXTURE_MOBILE&&big?"Fire":"Fire direction"));
 p_ptr->command_cmd='f';fixture_weapon_kind=PLAYER_ACTIVE_WEAPON_KIND_BOW;
 if(FIXTURE_MOBILE&&big){TTF_Font*f=sdl_story_font_for_height_slot(sdl_menu_role_font_px(SDL_UI_FONT_CONTROL),SDL_STORY_FONT_SLOT_MENU);for(int k=0;k<2;k++){int tw,th;p_ptr->command_cmd=k?'t':'f';cptr center=fixture_wheel_center_label();assert(sdl_ui_wrapped_text_texture(f,center,(int)fixture_center_rect.w-12,g_state.palette[TERM_WHITE],&tw,&th));assert(th<=fixture_center_rect.h-8);}}

 sdl_unified_look_prompt_layout_info layout;assert(sdl_unified_look_prompt_layout(&layout));assert(layout.count==count);
 if(FIXTURE_MOBILE){
  SDL_FRect blocks[]={thumb,(SDL_FRect){0},wheel};for(unsigned k=0;k<N_ELEMENTS(blocks);k++)if(blocks[k].w>0)assert(!overlap(layout.panel,blocks[k]));assert(!overlap(layout.panel,player_rect));
  if(big){assert(layout.font_px==sdl_menu_role_font_px(SDL_UI_FONT_BODY));TTF_Font*f=sdl_story_font_for_height_slot(layout.font_px,SDL_STORY_FONT_SLOT_LOG);for(int i=0;i<count;i++){int tw=0,th=0;cptr label=sdl_unified_look_prompt_label(&g_unified_look_prompt.buttons[i],layout.variant);assert(TTF_GetStringSize(f,label,strlen(label),&tw,&th));assert(tw<=layout.buttons[i].w&&th<=layout.buttons[i].h);}}
 }
 for(int i=0;i<count;i++){int id=-1;bool in=false;SDL_FRect r=layout.buttons[i];assert(r.w>0&&r.h>0);if(FIXTURE_MOBILE&&big)assert(r.h>=sdl_ui_min_tap_px()&&r.w>=sdl_ui_min_tap_px());assert(sdl_unified_look_prompt_choice_at(r.x+r.w/2,r.y+r.h/2,&id,&in)&&in&&id==keys[i]);for(int j=0;j<i;j++)assert(!overlap(r,layout.buttons[j]));}
 SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
 if(FIXTURE_MOBILE){
  sdl_menu_font_set_render_context(SDL_MENU_FONT_NONE);SDL_FRect queried;assert(sdl_unified_look_prompt_current_rect(&queried));assert(!memcmp(&queried,&layout.panel,sizeof(queried)));
  assert(sdl_menu_font_set_render_context(SDL_MENU_FONT_LOOK)==SDL_MENU_FONT_NONE);
  g_object_tooltip.active=true;g_object_tooltip.map_y=p_ptr->py-1;g_object_tooltip.map_x=p_ptr->px;g_object_tooltip.term_cell=g_object_tooltip.screen_rect=false;g_object_tooltip.touch=false;g_object_tooltip_motion_owned=false;SDL_strlcpy(g_object_tooltip.text,tooltip_payload,sizeof(g_object_tooltip.text));
  /* Actual earlier paint path uses its own font context, then queries LOOK. */
  sdl_menu_font_set_render_context(SDL_MENU_FONT_NONE);capturing_tooltip=true;captured_fills=0;sdl_object_tooltip_render();capturing_tooltip=false;assert(captured_fills>0);
  if(overlap(tooltip_box,layout.panel)||overlap(tooltip_box,player_rect)||overlap(tooltip_box,wheel)||overlap(tooltip_box,font_button)||overlap(tooltip_box,menu_button)||(thumb.w>0&&overlap(tooltip_box,thumb)))fprintf(stderr,"PAYLOAD%d %dx%d Big%d LookOnly%d count%d tooltip%.1f,%.1f,%.1f,%.1f bar%.1f,%.1f,%.1f,%.1f player%.1f,%.1f,%.1f,%.1f\n",gauntlets_payload,w,h,big,look_only,count,tooltip_box.x,tooltip_box.y,tooltip_box.w,tooltip_box.h,layout.panel.x,layout.panel.y,layout.panel.w,layout.panel.h,player_rect.x,player_rect.y,player_rect.w,player_rect.h);
  assert(!overlap(tooltip_box,menu_button));assert(!overlap(tooltip_box,font_button));assert(!overlap(tooltip_box,layout.panel));assert(!overlap(tooltip_box,player_rect));assert(!overlap(tooltip_box,wheel));if(thumb.w>0)assert(!overlap(tooltip_box,thumb));
  sdl_menu_font_set_render_context(SDL_MENU_FONT_LOOK);
 }
 sdl_unified_look_prompt_render();char path[1024];strnfmt(path,sizeof(path),"%s/%s-%dx%d-%s-%d-%s.png",out,FIXTURE_MOBILE?(throw_payload?(gauntlets_payload?"phone-throw-gauntlets":"phone-throw"):(gauntlets_payload?"phone-gauntlets":"phone")):(throw_payload?(gauntlets_payload?"desktop-throw-gauntlets":"desktop-throw"):(gauntlets_payload?"desktop-gauntlets":"desktop")),w,h,look_only?"look-only":big?"big":"normal",count,controller_five?"controller":"default");SDL_Surface*p=SDL_RenderReadPixels(g_state.renderer,NULL);assert(p&&IMG_SavePNG(p,path));for(int i=0;i<count;i++){SDL_FRect r=layout.buttons[i];int ink=0;for(int y=(int)r.y+3;y<(int)(r.y+r.h)-3;y++)for(int x=(int)r.x+3;x<(int)(r.x+r.w)-3;x++){Uint8 rr,gg,bb,aa;assert(SDL_ReadSurfacePixel(p,x,y,&rr,&gg,&bb,&aa));if(rr>150&&gg>150&&bb>150)ink++;}assert(ink>10);}SDL_DestroySurface(p);
 /* Hidden controls/non-aim must preserve the ordinary layout exactly. */
 controls_on=false;sdl_unified_look_prompt_layout_info original;assert(sdl_unified_look_prompt_layout(&original));controls_on=true;g_pointer_aim.active=false;sdl_unified_look_prompt_layout_info nonaim;assert(sdl_unified_look_prompt_layout(&nonaim));assert(!memcmp(&original,&nonaim,sizeof(original)));
 sdl_unified_look_prompt_clear();sdl_ui_text_cache_clear();sdl_story_font_cache_clear();SDL_DestroyTexture(v->canvas);term_nuke(&v->t);SDL_DestroyRenderer(g_state.renderer);SDL_DestroyWindow(g_state.window);g_state.renderer=NULL;g_state.window=NULL;
}
int main(int argc,char**argv){assert(argc==4);player_type player={0};p_ptr=&player;player.playing=true;player.py=10;player.px=9;character_generated=character_dungeon=true;player_other options={0};op_ptr=&options;log_set_quiet(true);SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)&&TTF_Init());sdl_config_set_defaults(&config);sdl_sync_palette();SDL_strlcpy(config.story_font,argv[1],sizeof(config.story_font));SDL_strlcpy(config.story_font2,argv[2],sizeof(config.story_font2));
 for(int throwing=0;throwing<2;throwing++){throw_payload=throwing;for(int payload=0;payload<2;payload++){gauntlets_payload=payload;tooltip_payload=payload?"a Wolf [HP: ******** Awareness: Sleeping]; a Set of Gauntlets of the Torturer":"a Wolf [HP: ******-- Morale: Confident 8]; an Arrow";
 for(int big=0;big<2;big++)for(int count=3;count<=7;count+=2){if(FIXTURE_MOBILE){run(720,1600,big,count,argv[3],false);run(1600,720,big,count,argv[3],false);if(big){run(720,1600,big,count,argv[3],true);run(1600,720,big,count,argv[3],true);}}else{run(1280,900,big,count,argv[3],false);run(768,576,big,count,argv[3],false);}}
 }
 }
 if(FIXTURE_MOBILE){controller_five=true;for(int throwing=0;throwing<2;throwing++){throw_payload=throwing;for(int payload=0;payload<2;payload++){gauntlets_payload=payload;tooltip_payload=payload?"a Wolf [HP: ******** Awareness: Sleeping]; a Set of Gauntlets of the Torturer":"a Wolf [HP: ******-- Morale: Confident 8]; an Arrow";for(int big=0;big<2;big++){run(720,1600,big,5,argv[3],false);run(1600,720,big,5,argv[3],false);}}}controller_five=false;}
 puts("Aim footer and tooltip real fonts/combined clearance/render/hit IDs; LOOK-only context and desktop/non-aim preservation PASS");return 0;}
'''
def main():
 global OUT
 if "--baseline" in sys.argv:OUT=OUT/"baseline"
 elif "--prompt-negative" in sys.argv:OUT=OUT/"prompt-negative"
 elif "--context-negative" in sys.argv:OUT=OUT/"context-negative"
 elif "--chrome-negative" in sys.argv:OUT=OUT/"chrome-negative"
 elif "--throw-height-negative" in sys.argv:OUT=OUT/"throw-height-negative"
 elif "--no-reflow" in sys.argv:OUT=OUT/"no-reflow"
 elif "--bounds-only-negative" in sys.argv:OUT=OUT/"bounds-only-negative"
 if "--fire-label-negative" in sys.argv:OUT=OUT/"fire-label-negative"
 if "--thumb-label-negative" in sys.argv:OUT=OUT/"thumb-label-negative"
 if "--verbose-label-negative" in sys.argv:OUT=OUT/"verbose-label-negative"
 OUT.mkdir(parents=True,exist_ok=True);source=(ROOT/'src/sdl/ui/sdl-menus.c').read_text(encoding='utf-8-sig')
 if '--baseline' in sys.argv:source=source.replace('sdl_unified_look_prompt_place_aim(out, screen_margin);','/* original overlap */').replace('&& sdl_unified_look_prompt_place_aim(&candidate, screen_margin)', '')
 if '--context-negative' in sys.argv:source=source.replace('int previous = sdl_menu_font_set_render_context(SDL_MENU_FONT_LOOK);','int previous = SDL_MENU_FONT_NONE;')
 tooltip=(ROOT/'src/sdl/input/sdl-tooltips.c').read_text()
 a=tooltip.index('bool sdl_object_tooltip_format_grid(');b=tooltip.index('\nbool sdl_object_tooltip_show_grid(',a)
 tooltip=tooltip[:a]+"bool sdl_object_tooltip_format_grid(int y,int x,char*out,size_t n,byte*attrs){(void)y;(void)x;SDL_strlcpy(out,tooltip_payload,n);if(attrs)memset(attrs,TERM_WHITE,strlen(out));return true;}\n"+tooltip[b:]
 if '--no-reflow' in sys.argv:tooltip=tooltip.replace('if (!placed && g_pointer_aim.active && g_pointer_aim.select_mode','if (false && g_pointer_aim.active && g_pointer_aim.select_mode')
 if '--bounds-only-negative' in sys.argv:
  a=tooltip.index('                    SDL_memcpy(runs, candidate_runs,')
  b=tooltip.index('                    box = candidate;',a)
  tooltip=tooltip[:a]+tooltip[b:]
 if '--chrome-negative' in sys.argv:
  a=tooltip.index('        /* Menu is separate from Quick Access')
  b=tooltip.index('        if (((g_pointer_aim.active && g_pointer_aim.select_mode',a)
  tooltip=tooltip[:a]+tooltip[b:]
 if '--prompt-negative' in sys.argv:
  a=tooltip.index('        if (((g_pointer_aim.active && g_pointer_aim.select_mode',tooltip.index('void sdl_object_tooltip_render'))
  b=tooltip.index('        bool placed =',a)
  tooltip=tooltip[:a]+tooltip[b:]
 controls=(ROOT/'src/sdl/input/sdl-touch-controls.c').read_text()
 a=controls.index('        cptr center_label = sdl_touch_aim_is_throwing()');b=controls.index('        sdl_touch_pane_draw_button_text_scaled(&center_arrow',a)
 label=controls[a:b]
 if "--verbose-label-negative" in sys.argv:label=label.replace('sdl_touch_aim_is_throwing()\n            ? "Throw direction" : "Fire direction"', '"Fire direction"')
 if "--throw-height-negative" in sys.argv:label=label.replace("center_arrow.h = inner_radius * 2.0f;", "/* original clipped height */")
 helper_start=controls.index('static bool sdl_touch_aim_is_throwing(');helper_end=controls.index('\nstatic ',helper_start+1)
 active_start=controls.index('static bool sdl_touch_thumb_fire_targeting_active(');active_end=controls.index('\n/*',active_start+1)
 branch_start=controls.index('    if (binding == SDL_TOUCH_THUMB_BIND_FIRE_SELECTED) {',controls.index('static void sdl_touch_context_label_for_binding('));branch_end=controls.index('    if (binding == ESCAPE',branch_start)
 thumb=controls[active_start:active_end]+controls[helper_start:helper_end]+'\n#define SDL_TOUCH_THUMB_BIND_FIRE_SELECTED -102\nstatic void fixture_thumb_selected_label(char*buf,size_t buflen){int binding=SDL_TOUCH_THUMB_BIND_FIRE_SELECTED;'+controls[branch_start:branch_end]+'}\n'
 if '--thumb-label-negative' in sys.argv:thumb=thumb.replace('sdl_touch_aim_is_throwing()\n                    ? "Throw" : "Fire"','false\n                    ? "Throw" : "Fire"').replace('sdl_touch_aim_is_throwing()\n                    ? "Throw at target" : "Fire at target"','false\n                    ? "Throw at target" : "Fire at target"')
 aim=(ROOT/'src/ui/targeting/aim-select.c').read_text();a=aim.index('static char aim_select_up_key(');b=aim.index('/*',aim.index('    sdl_unified_look_prompt_finish();',aim.index('    /* Gamepad binding labels')))
 # Extract exactly the prompt helpers, stopping before the next helper comment.
 aim=aim[a:b]
 if '--fire-label-negative' in sys.argv:aim=aim.replace('action_verb = "Throw";', 'action_verb = "Fire";')
 code=OUT/'check.c';code.write_text(HARNESS.replace('__PRODUCTION__',source).replace('__TOOLTIPS__',tooltip).replace('__CENTER_LABEL__',label).replace('__AIM_PROMPT__',aim).replace('__THUMB_LABEL__',thumb),encoding='utf-8')
 objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text());response=OUT/'objects.rsp';response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/sdl/ui/sdl-menus.c.obj','/src/sdl/render/sdl-fonts.c.obj','/src/sdl/input/sdl-tooltips.c.obj'))))
 env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/n) for n in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
 wraps='steamdeck_controls_active,target_prompt_label,SDL_RenderTexture,player_active_weapon_kind,sdl_main_menu_pane_button_rect,sdl_menu_font_button_bounds,SDL_RenderFillRect,sdl_get_layout_screen_rect,SDL_GetDisplayContentScale,sdl_menu_content_rect,sdl_touch_thumb_current_bounds,sdl_touch_top_panel_compute_layout,sdl_touch_pane_current_rect,sdl_touch_round_layer_config_enabled,sdl_mouse_gameplay_context_active,sdl_touch_round_compute_layout,sdl_player_map_rect'
 for mobile in ([1] if '--baseline' in sys.argv else [0,1]):
  exe=OUT/f'check-{mobile}.exe';subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL',f'-DFIXTURE_MOBILE={mobile}','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(code),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-Wl,'+','.join('--wrap='+x for x in wraps.split(',')),'-o',str(exe)],cwd=BUILD,env=env,check=True)
  subprocess.run([str(exe),str(ROOT/'lib/xtra/font/Cinzel-Medium.ttf'),str(ROOT/'lib/xtra/font/EBGaramond-Regular.ttf'),str(OUT)],cwd=ROOT,env=env,check=True,timeout=60)
if __name__=='__main__':main()

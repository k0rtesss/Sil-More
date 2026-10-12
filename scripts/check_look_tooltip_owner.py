#!/usr/bin/env python3
"""Exercise selected known terrain through the real saved-screen Look tooltip owner."""
from pathlib import Path
import argparse,os,shlex,subprocess,tempfile
from check_monster_scent_save import ENGINE_FIXTURE,fixture_function
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build-standard';OUT=ROOT/'scripts/output/look-tooltip-owner'
LAYOUT_CHECK=r'''
#include <assert.h>
void fixture_mutual_layout_check(void)
{
 int previous=sdl_menu_font_set_render_context(SDL_MENU_FONT_LOOK);
 sdl_unified_look_prompt_layout_info p;sdl_unified_look_sidebar_layout_info s;
 assert(sdl_unified_look_prompt_layout(&p)&&sdl_unified_look_sidebar_layout(&s));
 if(sdl_description_overlay_rects_intersect(&p.panel,&s.panel))fprintf(stderr,"Mutual Look overlap mobile%d Big%d sidebar%.0f,%.0f,%.0f,%.0f footer%.0f,%.0f,%.0f,%.0f\n",FIXTURE_MOBILE,get_sdl_menu_bigger_font(),s.panel.x,s.panel.y,s.panel.w,s.panel.h,p.panel.x,p.panel.y,p.panel.w,p.panel.h);
 assert(!sdl_description_overlay_rects_intersect(&p.panel,&s.panel));
 assert(p.count==9&&s.visible_count>0);
 TTF_Font*f=sdl_story_font_for_height_slot(p.font_px,SDL_STORY_FONT_SLOT_LOG);assert(f);
 if(FIXTURE_MOBILE&&get_sdl_menu_bigger_font())assert(p.font_px==sdl_unified_look_log_text_font_px());
 for(int i=0;i<p.count;i++){
  SDL_FRect r=p.buttons[i];int id;bool in=false;
  assert(sdl_unified_look_prompt_choice_at(r.x+r.w/2,r.y+r.h/2,&id,&in)&&in&&id==g_unified_look_prompt.buttons[i].choice);
  cptr text=sdl_unified_look_prompt_label(&g_unified_look_prompt.buttons[i],p.variant);int w,h;
  assert(TTF_GetStringSize(f,text,strlen(text),&w,&h));assert(w<=r.w&&h<=r.h);
  if(FIXTURE_MOBILE&&get_sdl_menu_bigger_font())assert(r.w>=sdl_ui_min_tap_px()&&r.h>=sdl_ui_min_tap_px());
 }
 bool selected=false;
 for(int i=0;i<s.visible_count;i++){
  const sdl_unified_look_sidebar_item_state*item=&g_unified_look_sidebar.items[s.item_indices[i]];
  SDL_FRect r=s.rows[i];assert(r.x>=s.panel.x&&r.y>=s.panel.y&&r.x+r.w<=s.panel.x+s.panel.w&&r.y+r.h<=s.panel.y+s.panel.h);
  if(item->kind!=SDL_UNIFIED_LOOK_SIDEBAR_ITEM_ENTRY)continue;
  int id;bool in=false;assert(sdl_unified_look_sidebar_choice_at(r.x+r.w/2,r.y+r.h/2,&id,&in)&&in&&id==item->choice);
  if(id==3)selected=true;
 }
 assert(selected);sdl_unified_look_sidebar_render();sdl_unified_look_prompt_render();
 SDL_Surface*pixels=SDL_RenderReadPixels(g_state.renderer,NULL);assert(pixels);
 for(int i=0;i<p.count;i++){
  SDL_FRect r=p.buttons[i];int ink=0;
  for(int y=(int)r.y+4;y<(int)(r.y+r.h)-4;y++)for(int x=(int)r.x+4;x<(int)(r.x+r.w)-4;x++){
   Uint8 rr,gg,bb,aa;assert(SDL_ReadSurfacePixel(pixels,x,y,&rr,&gg,&bb,&aa));if(rr>150&&gg>150&&bb>150)ink++;
  }assert(ink>10);
 }
 SDL_DestroySurface(pixels);sdl_menu_font_set_render_context(previous);
}
'''
CHECKS=r'''
#include "sdl/main-sdl-private.h"
#undef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD FIXTURE_MOBILE
#include "sdl/render/sdl-fonts.c"
static int width=768,height=576;static float density=1;
static SDL_FRect player_box;
void fixture_mutual_layout_check(void);
static int fills;
static SDL_FRect tooltip_box;
bool __real_SDL_RenderFillRect(SDL_Renderer*r,const SDL_FRect*b);
bool __wrap_SDL_RenderFillRect(SDL_Renderer*r,const SDL_FRect*b){if(fills++==0)tooltip_box=*b;return __real_SDL_RenderFillRect(r,b);}
bool __wrap_sdl_player_map_rect(int y,int x,SDL_FRect*r){*r=(y==p_ptr->py&&x==p_ptr->px)?player_box:(SDL_FRect){width*.25f,FIXTURE_MOBILE?(height>width?700:400):100,24,24};return true;}
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID id){(void)id;return density;}
SDL_Rect __wrap_sdl_get_layout_screen_rect(void){return (SDL_Rect){0,FIXTURE_MOBILE?38:0,width,height-(FIXTURE_MOBILE?38:0)};}
SDL_Rect __wrap_sdl_menu_content_rect(SDL_Rect r){return r;}
bool __wrap_sdl_touch_thumb_current_bounds(SDL_FRect*r){(void)r;return false;}
bool __wrap_sdl_touch_top_panel_compute_layout(SDL_FRect*b,SDL_FRect*r){(void)b;(void)r;return false;}
bool __wrap_sdl_touch_pane_current_rect(SDL_Rect*r){(void)r;return false;}
bool __wrap_sdl_touch_round_layer_config_enabled(void){return false;}
bool __wrap_sdl_main_menu_pane_button_rect(SDL_FRect*r){(void)r;return false;}
SDL_FRect __wrap_sdl_menu_font_button_bounds(void){assert(sdl_menu_font_set_render_context(SDL_MENU_FONT_LOOK)==SDL_MENU_FONT_LOOK);return (SDL_FRect){width-110,48,100,100};}
void __wrap_sdl_refresh_supporting_panes_layout(void) {}
void __wrap_sdl_suspend_main_view_zoom_for_saved_screen(void) {}
bool __wrap_sdl_resume_main_view_zoom_for_saved_screen(void){return false;}
static void request(bool visible)
{
 if(visible){
  sdl_unified_look_prompt_begin(Term->hgt-1);
  const char* labels[]={"L1 Prev","R1 Next","A Exam","X Target","View Objects","Y Pan","Zoom +","Zoom -","B Back"};const char* compact[]={"L1 Prv","R1 Nxt","A Ex","X Tgt","View Obj","Y Pan","Z+","Z-","B Bk"};const char* tiny[]={"L1","R1","A","X","View","Y","Z+","Z-","B"};const int keys[]={'e','i',' ','f','u','s','+','-',ESCAPE};
  for(unsigned i=0;i<N_ELEMENTS(labels);i++)sdl_unified_look_prompt_add(-1000-keys[i],labels[i],labels[i],compact[i],tiny[i]);sdl_unified_look_prompt_finish();
  sdl_unified_look_sidebar_begin(false,true,3);sdl_unified_look_sidebar_add_header("Monsters:");
  sdl_unified_look_sidebar_add_entry(1,1,4,4,TERM_WHITE,TERM_WHITE,"C","a Wolf [HP: ******** Morale: Confident]");
  sdl_unified_look_sidebar_add_entry(2,1,4,4,TERM_WHITE,TERM_WHITE,"o","an Orc scout");sdl_unified_look_sidebar_add_header("Objects:");
  sdl_unified_look_sidebar_add_entry(3,2,4,4,TERM_WHITE,TERM_WHITE,"[","a Set of Gauntlets of the Torturer");sdl_unified_look_sidebar_finish();
 }

 fills=0;assert(sdl_object_tooltip_show_grid(5,6,false));assert(g_object_tooltip.active);
 sdl_object_tooltip_render();
 if(visible)fixture_mutual_layout_check();
 if(visible){if(!g_object_tooltip.active||!fills)fprintf(stderr,"Selected known terrain request cleared in real saved Look owner, icky%d\n",character_icky);assert(g_object_tooltip.active&&fills>0);assert(strstr(g_object_tooltip.text,"staircase"));SDL_FRect footer,sidebar;int previous=sdl_menu_font_set_render_context(SDL_MENU_FONT_NONE);assert(sdl_unified_look_prompt_current_rect(&footer));assert(sdl_unified_look_sidebar_current_rect(&sidebar));assert(!sdl_description_overlay_rects_intersect(&footer,&sidebar));assert(sdl_menu_font_set_render_context(SDL_MENU_FONT_LOOK)==SDL_MENU_FONT_NONE);SDL_FRect b=__wrap_sdl_menu_font_button_bounds();sdl_menu_font_set_render_context(previous);if(sdl_description_overlay_rects_intersect(&tooltip_box,&sidebar))fprintf(stderr,"Sidebar overlap mobile%d %dx%d big%d box%.0f,%.0f,%.0f,%.0f sidebar%.0f,%.0f,%.0f,%.0f\n",FIXTURE_MOBILE,width,height,config.bigger_font,tooltip_box.x,tooltip_box.y,tooltip_box.w,tooltip_box.h,sidebar.x,sidebar.y,sidebar.w,sidebar.h);assert(!sdl_description_overlay_rects_intersect(&tooltip_box,&footer));assert(!sdl_description_overlay_rects_intersect(&tooltip_box,&sidebar));assert(!sdl_description_overlay_rects_intersect(&tooltip_box,&b));assert(!sdl_description_overlay_rects_intersect(&tooltip_box,&player_box));}
 else assert(!g_object_tooltip.active&&!fills);
}
static void checks(void)
{
 assert(SDL_InitSubSystem(SDL_INIT_VIDEO));assert(TTF_Init());sdl_config_set_defaults(&config);
 SDL_strlcpy(config.story_font,"C:/Users/efrem/Documents/GitHub/Sil-More/lib/xtra/font/Cinzel-Medium.ttf",sizeof(config.story_font));
 SDL_strlcpy(config.story_font2,"C:/Users/efrem/Documents/GitHub/Sil-More/lib/xtra/font/EBGaramond-Regular.ttf",sizeof(config.story_font2));sdl_sync_palette();
 sdl_view*v=&g_views[PANE_MAIN];
 p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;p_ptr->py=5;p_ptr->px=5;p_ptr->playing=true;p_ptr->leaving=p_ptr->is_dead=false;character_generated=character_dungeon=true;
 cave_feat[5][6]=FEAT_MORE;cave_info[5][6]=CAVE_MARK|CAVE_SEEN;
 turn=4441;playerturn=444;p_ptr->energy_use=0;p_ptr->chp=48;p_ptr->csp=59;p_ptr->new_exp=47464;
 for(int size=0;size<2;size++)for(int big=0;big<2;big++){
  width=FIXTURE_MOBILE?(size?720:1600):(size?1280:768);height=FIXTURE_MOBILE?(size?1600:720):(size?900:576);density=FIXTURE_MOBILE?2.625f:1;
  g_state.system_scale=density;g_state.window=SDL_CreateWindow("Look ownership",width,height,SDL_WINDOW_HIDDEN);assert(g_state.window);g_state.renderer=SDL_CreateRenderer(g_state.window,"software");assert(g_state.renderer);
  assert(!term_init(&v->t,width/16,height/32,256));Term_activate(&v->t);v->term_ready=true;v->cell_w=16;v->cell_h=32;v->cols=width/16;v->rows=height/32;v->rect=(SDL_Rect){0,FIXTURE_MOBILE?38:0,width,height-(FIXTURE_MOBILE?38:0)};v->canvas=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,width,height);assert(v->canvas);player_box=(SDL_FRect){width*.75f,height*.65f,24,24};
  config.bigger_font=big;config.menu_bigger_font[SDL_MENU_FONT_LOOK]=big;
  screen_save();assert(character_icky==1);sdl_unified_look_set_active(true);sdl_unified_look_set_map_hover_enabled(true);ui_menu_click_begin();
  assert(!sdl_mouse_gameplay_context_active());request(true);
  sdl_unified_look_set_map_hover_enabled(false);screen_save();assert(character_icky==2);request(false);screen_load();sdl_unified_look_set_map_hover_enabled(true);request(true);
  sdl_unified_look_set_map_hover_enabled(false);request(false);sdl_unified_look_set_map_hover_enabled(true);
  /* Even a stale active flag must not authorize a different nested owner. */
  screen_save();request(false);screen_load();
  p_ptr->playing=false;request(false);p_ptr->playing=true;p_ptr->leaving=true;request(false);p_ptr->leaving=false;p_ptr->is_dead=true;request(false);p_ptr->is_dead=false;
  character_generated=false;request(false);character_generated=true;character_dungeon=false;request(false);character_dungeon=true;v->term_ready=false;request(false);v->term_ready=true;
  sdl_unified_look_set_active(false);request(false);SDL_FRect unavailable={1,2,3,4};int old=sdl_menu_font_set_render_context(SDL_MENU_FONT_NONE);assert(!sdl_unified_look_sidebar_current_rect(&unavailable));assert(!unavailable.x&&!unavailable.y&&!unavailable.w&&!unavailable.h);assert(sdl_menu_font_set_render_context(old)==SDL_MENU_FONT_NONE);ui_menu_click_clear();screen_load();assert(!character_icky);
  assert(turn==4441&&playerturn==444&&!p_ptr->energy_use&&p_ptr->chp==48&&p_ptr->csp==59&&p_ptr->new_exp==47464);
 sdl_ui_text_cache_clear();sdl_story_font_cache_clear();SDL_DestroyTexture(v->canvas);term_nuke(&v->t);SDL_DestroyRenderer(g_state.renderer);SDL_DestroyWindow(g_state.window);
 }
 puts("Look real saved owner/terrain renderer + populated sidebar/footer/B/player clearance, normal/Big, context restore/nesting/lifecycle/free state PASS");
}
'''
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--baseline',action='store_true');parser.add_argument('--sidebar-negative',action='store_true');args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
 tooltip=(ROOT/'src/sdl/input/sdl-tooltips.c').read_text()
 if args.baseline:
  a=tooltip.index('static bool sdl_object_tooltip_map_context_active');b=tooltip.index('\n}\n',a)+2
  tooltip=tooltip[:a]+'''static bool sdl_object_tooltip_map_context_active(void){if(sdl_mouse_gameplay_context_active())return true;return g_pointer_aim.active&&g_pointer_aim.select_mode&&character_generated&&character_dungeon&&p_ptr&&p_ptr->playing&&!p_ptr->leaving&&!p_ptr->is_dead&&character_icky==0&&g_views[PANE_MAIN].term_ready;}'''+tooltip[b:]
 if args.sidebar_negative:
  a=tooltip.index('        if (look_owner\n            && sdl_unified_look_sidebar_current_rect')
  b=tooltip.index('        bool placed =',a);tooltip=tooltip[:a]+tooltip[b:]
 override='#include "sdl/main-sdl-private.h"\n#undef SIL_SDL_MOBILE_BUILD\n#define SIL_SDL_MOBILE_BUILD FIXTURE_MOBILE'
 tooltip=tooltip.replace('#include "sdl/main-sdl-private.h"',override)
 production=OUT/'tooltips.c';production.write_text(tooltip)
 menus=OUT/'menus.c';menus.write_text((ROOT/'src/sdl/ui/sdl-menus.c').read_text(encoding='utf-8-sig').replace('#include "sdl/main-sdl-private.h"',override)+LAYOUT_CHECK)
 prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'
 prefix=prefix.replace('#include "level-generation/level-generation-internal.h"','')
 init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):];init=init[:init.index('    check_templates();')]
 source=OUT/'check.c';source.write_text(prefix+CHECKS+init+'checks(); SDL_Quit(); return 0;\n}\n')
 cmake=BUILD/'CMakeFiles/sil-more.dir';objects=shlex.split((cmake/'objects1.rsp').read_text());rsp=OUT/'objects.rsp';rsp.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/sdl/input/sdl-tooltips.c.obj','/src/sdl/ui/sdl-menus.c.obj','/src/sdl/render/sdl-fonts.c.obj'))))
 env=os.environ.copy();env['SDL_VIDEODRIVER']='dummy';env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/n) for n in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
 wraps='SDL_GetDisplayContentScale,sdl_get_layout_screen_rect,sdl_menu_content_rect,SDL_RenderFillRect,sdl_player_map_rect,sdl_touch_thumb_current_bounds,sdl_touch_top_panel_compute_layout,sdl_touch_pane_current_rect,sdl_touch_round_layer_config_enabled,sdl_main_menu_pane_button_rect,sdl_menu_font_button_bounds,sdl_refresh_supporting_panes_layout,sdl_suspend_main_view_zoom_for_saved_screen,sdl_resume_main_view_zoom_for_saved_screen'
 for mobile in (0,1):
  exe=OUT/f'check-{mobile}.exe';subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL',f'-DFIXTURE_MOBILE={mobile}','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(production),str(menus),'@'+str(rsp),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-Wl,'+','.join('--wrap='+x for x in wraps.split(',')),'-o',str(exe)],cwd=BUILD,env=env,check=True)
  with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=ROOT,env=env,check=True,timeout=30)
if __name__=='__main__':main()

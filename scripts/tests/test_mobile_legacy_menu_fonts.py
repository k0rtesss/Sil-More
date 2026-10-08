"""Exercise production mobile menu font scopes and responsive legacy layouts.

No game build, display, save, emulator, or profile is opened. Uses the actual C
functions with a Term/input fixture; geometry assertions cover phone-size grids.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
def function(path, marker):
    source=(ROOT/path).read_text();start=source.rindex(marker)
    opening=source.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
def declaration(path, pattern):
    return re.search(pattern,(ROOT/path).read_text(),re.S).group(0)
settings='src/sdl/config/sdl-settings.c'
knowledge='src/cmd/ui/cmd-ui-knowledge.c'
abilities='src/cmd/ui/cmd-ui-abilities.c'
smithing='src/cmd/ui/cmd-ui-smithing.c'
parts=[function('src/sdl/render/sdl-fonts.c','int sdl_ui_font_px(')]
parts += [function(settings,x) for x in ['int sdl_terminal_menu_font_px(',
    'static void sdl_push_terminal_menu_scale_value(',
    'void sdl_push_terminal_menu_scale(', 'void sdl_pop_terminal_menu_scale(']]
parts += [function(knowledge,x) for x in ['static int equipment_entry_wrap_take(',
    'static bool equipment_entry_wrap_next(', 'static int supply_put_wrapped(',
    'static void knowledge_draw_status(']]
parts += [function(knowledge,x) for x in ['static void knowledge_init_layout(',
    'static void knowledge_expand_active_column(', 'static void knowledge_expand_entry_view(',
    'static void knowledge_init_inventory_portrait_layout_normal(',
    'static void knowledge_init_inventory_portrait_layout(',
    'static void knowledge_touch_scroll_region(', 'static void knowledge_begin_split_touch_scroll_areas(',
    'static void inventory_browser_group_status(',
    'static bool supply_take_category_return(', 'static void supply_draw_category_return(',
    'static bool supply_touch_preview_entry_select_only(',
    'static cptr supply_browser_page_text(', 'static supply_menu_page supply_browser_turn_page(',
    'static int supply_browser_page_tab_col(', 'static int supply_browser_page_token_width(',
    'static byte supply_browser_page_tab_attr(', 'static bool supply_page_header_uses_wrapped_title(',
    'static bool supply_page_tabs_need_paging(', 'static void supply_draw_page_header(',
    'static int supply_browser_page_click_choice(', 'static void supply_register_page_tabs(']]
parts += [function(abilities,x) for x in ['static void ability_browser_init_layout_normal(',
    'static void ability_browser_init_layout(',
    'static int ability_browser_skill_tab_split(', 'static int ability_browser_skill_tab_width(',
    'static void ability_browser_draw_skill_summary(']]
parts += [function(smithing,x) for x in ['static int smith_ui_configure_list_view(',
    'static int smith_root_draw(']]
types=declaration('src/cmd/ui/cmd-ui-internal.h',r'struct knowledge_browser_layout\s*\{.*?\};')
types+='\ntypedef struct knowledge_browser_layout knowledge_browser_layout;\n'
types+=declaration(abilities,r'typedef struct ability_browser_layout\s*\{.*?\} ability_browser_layout;')
harness=r"""
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
typedef const char *cptr;typedef unsigned char byte;
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define N_ELEMENTS(a) (sizeof(a)/sizeof((a)[0]))
#define SDL_ceilf ceilf
#define log_warn(...) ((void)0)
#define TERM_L_BLUE 1
#define TERM_SLATE 2
#define TERM_L_DARK 3
#define TERM_L_WHITE 4
#define TERM_SHADE 16
#define S_MAX 8
#define SMT_MENU_MAX 7
#define SMITH_SCROLL_ROOT 10
typedef int smith_ui_scroll_id;
#define SMITH_SCROLL_MAX 11
#define ABILITY_MENU_CLICK_SKILL_BASE 200
#define SUPPLY_CLICK_CATEGORY_RETURN (-16)
#define SUPPLY_CLICK_ENTRY_BASE 10000
#define UI_MENU_CLICK_PRIMARY 1
#define UI_MENU_CLICK_SECONDARY 2
#define UI_MENU_CLICK_HOVER 3
#define SUPPLY_CLICK_PAGE_EQUIPPED 100
#define SUPPLY_CLICK_PAGE_INVENTORY 101
#define SUPPLY_CLICK_PAGE_JEWELRY 102
#define SUPPLY_CLICK_PAGE_SUPPLIES 103
typedef enum {SUPPLY_MENU_PAGE_EQUIPPED,SUPPLY_MENU_PAGE_INVENTORY,
 SUPPLY_MENU_PAGE_JEWELRY,SUPPLY_MENU_PAGE_SUPPLIES} supply_menu_page;
struct {bool bigger_font;int terminal_menu_scale_offset;} config;
static bool get_sdl_bigger_font(void) {return config.bigger_font;}
static int g_terminal_menu_scale_override,g_terminal_menu_scale_depth;
static int g_terminal_menu_scale_stack[16];
static unsigned int g_terminal_menu_scale_overflow_depth;
static float density=2.0f;
static int redraws;
static float sdl_ui_density_scale(void) {return density;}
static void sdl_apply_config_no_redraw(void) {++redraws;}
static int get_sdl_terminal_menu_scale(void) {return 2;}
static struct {int wid,hgt;} fixture={30,12},*Term=&fixture;
static bool portrait=true,touch=true,insight=false;
static int reserved_rows=2;
static char screen[80][256];
static int ids[80],clicks;
static struct {int x,y,w;} hit_rects[80];
static int inv_entry_cur,inv_entry_top,inv_grp_cur,inv_grp_top,inv_column;
static int smith_ui_scroll_top[SMITH_SCROLL_MAX];
static int *drag_offset,drag_max;
static int strnfmt(char *s,size_t n,cptr f,...) {va_list a;va_start(a,f);int r=vsnprintf(s,n,f,a);va_end(a);return r;}
static void strnfcat(char *s,size_t n,size_t *used,cptr f,...) {va_list a;va_start(a,f);vsnprintf(s+*used,n-*used,f,a);va_end(a);*used=strlen(s);}
static void Term_get_size(int *w,int *h) {if(w)*w=Term->wid;if(h)*h=Term->hgt;}
static void Term_clear(void) {memset(screen,0,sizeof(screen));clicks=0;}
static void Term_erase(int x,int y,int n) {assert(y>=0&&y<Term->hgt);}
static void Term_putstr(int x,int y,int n,int attr,cptr s) {
 assert(x>=0&&x<Term->wid&&y>=0&&y<Term->hgt);
 int count=MIN((int)strlen(s),n);assert(count<=Term->wid-x);
 memcpy(screen[y]+x,s,count);
}
static void supply_put_fitted(int x,int y,int w,byte a,cptr text) {Term_putstr(x,y,MIN(w,(int)strlen(text)),a,text);}
static int supply_put_wrapped(int x,int y,int w,int rows,byte a,cptr text);
static void ui_menu_click_add(int id,int x,int y,int w) {assert(x>=0&&y>=0&&y<Term->hgt&&x+w<=Term->wid);ids[clicks]=id;hit_rects[clicks].x=x;hit_rects[clicks].y=y;hit_rects[clicks].w=w;++clicks;}
static bool sdl_mobile_portrait_layout_active(void) {return portrait;}
static int sdl_touch_menu_button_reserved_rows(void) {return reserved_rows;}
static int sdl_main_view_visible_col0(void) {return 0;}
static int sdl_main_view_visible_cols(void) {return Term->wid;}
static bool insight_system_enabled(void) {return insight;}
static int ability_browser_wrapped_rows(cptr s,int w) {return MAX(1,((int)strlen(s)+w-1)/w);}
static byte ability_browser_skill_attr(int skill,bool hover) {return 1;}
static int ability_browser_tab_skill(int skill) {return skill;}
static cptr skills[]={"Melee","Archery","Evasion","Stealth","Perception","Will","Smithing","Song","Insight"};
static void ability_browser_build_skill_tokens(int count,int current,bool full,char tokens[][40],int widths[]) {
 for(int i=0;i<count;i++){strnfmt(tokens[i],40,i==current?"[%s]":" %s ",skills[i]);widths[i]=(int)strlen(tokens[i])+1;}
}
static void ability_browser_put_fitted(int x,int y,int w,byte a,cptr s) {supply_put_fitted(x,y,w,a,s);}
static bool sdl_touch_only_device_active(void) {return touch;}
static bool ui_scroll_area_take_touch_scrolled(void) {return false;}
static struct {int left,right,top,bottom;int *offset,max;} regions[2];
static int region_count;
static bool sdl_touch_tutorial_device_available(void) {return true;}
static void ui_scroll_area_begin_cols(int l,int r,int t,int b,int category) {
 region_count=1;regions[0].left=l;regions[0].right=r;regions[0].top=t;regions[0].bottom=b;
}
static bool ui_scroll_area_add_cols(int l,int r,int t,int b,int category) {
 assert(region_count<2);int i=region_count++;regions[i].left=l;regions[i].right=r;regions[i].top=t;regions[i].bottom=b;return true;
}
static void ui_scroll_area_set_keys(int a,int b,int c,int d) {}
static void ui_scroll_area_set_offset_target(int *top,int max) {
 drag_offset=top;drag_max=max;
 if(region_count){regions[region_count-1].offset=top;regions[region_count-1].max=max;}
}
#define SDL_memcpy memcpy
#define ABS(x) abs(x)
#define streq(a,b) (!strcmp(a,b))
static int utf8_sequence_len_n(cptr text,int len) {return len>0&&*text ? 1:0;}
static int utf8_display_width_n(cptr text,int len) {return len;}
static int utf8_safe_prefix_len(cptr text,int len) {return len;}
typedef enum {INVENTORY_MENU_GROUP_ALL,INVENTORY_MENU_GROUP_PACK,
 INVENTORY_MENU_GROUP_HARNESS,INVENTORY_MENU_GROUP_QUIVER,INVENTORY_MENU_GROUP_JEWELRY} inventory_menu_group;
enum inventory_limit_group {INV_LIMIT_PACK,INV_LIMIT_HARNESS};
#define QUIVER_ARROW_CAPACITY 48
static int inventory_limit_limit_for_group(int group) {return group==INV_LIMIT_PACK?260:210;}
static int inventory_limit_usage_for_group(int group) {return group==INV_LIMIT_PACK?0:13;}
static int inventory_limit_carriage_savings_for_group(int group) {return 0;}
static int player_quiver_arrow_count(void) {return 23;}
static int player_quiver_arrow_space(void) {return 25;}
static void inventory_browser_weight_status(char *s,size_t n) {strnfmt(s,n,"8.3/172.8 lb");}
static void inventory_browser_group_limit_status(int group,char *s,size_t n) {strnfmt(s,n,"0.0/26.0 qt");}
static cptr inventory_browser_group_text(int group) {return "Pack";}
static int smith_ui_content_bottom_row(void) {return Term->hgt-1-reserved_rows;}
static int smith_root_detail_col(void) {return 0;}
static int smith_root_list_width(int col) {return Term->wid;}
static void smith_ui_reset_description_state(void) {}
/* Root's compact chrome is two summary rows plus heading/divider. */
static int smith_root_draw_header(void) {return 2;}
static int smith_root_draw_chrome(int col,int w,int row) {return row+2;}
static void smith_root_draw_action(int choice,int row,int w,bool selected,byte a,cptr label) {ui_menu_click_add(choice,0,row,w);}
static void smith_root_draw_detail(int choice,int col,int w,int row,bool valid,cptr label) {}
static void smith_ui_draw_navigation_prompt(bool root) {}
"""
source=(ROOT/knowledge).read_text()
start=source.index('else if (clicked_choice >= SUPPLY_CLICK_ENTRY_BASE)',source.index('bool inventory_one_page ='))
opening=source.index('{',start);depth=1;end=opening+1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}');end+=1
entry_route=source[start:end].replace('else if','if',1).replace('continue;','return ch;')
input_fixture=r"""
static int fixture_click_at(int x,int y) {
 for(int i=clicks-1;i>=0;i--)
  if(y==hit_rects[i].y&&x>=hit_rects[i].x&&x<hit_rects[i].x+hit_rects[i].w)return ids[i];
 return 0;
}
static int fixture_inventory_entry_event(int clicked_choice,int click_action) {
 int inventory_entry_cnt=5;char ch=0;
 bool desc_overlay_on=false,storage_exchange_mode=false,replacement_mode=false,
  slot_pick_mode=false,item_select_mode=false,drop_click_mode=false,delete_click_mode=false,
  click_generated_command=false;
"""+entry_route+"\nreturn ch;}\n"
main=r"""
int main(void) {
 float densities[]={2.0f,2.625f,2.75f};
 for(int d=0;d<3;d++){
  density=densities[d];config.terminal_menu_scale_offset=0;config.bigger_font=false;
  assert(sdl_terminal_menu_font_px()==0);sdl_push_terminal_menu_scale();
#if SIL_SDL_MOBILE_BUILD
  assert(sdl_terminal_menu_font_px()==0); /* Normal uses the original grid. */
  config.bigger_font=true;assert(sdl_terminal_menu_font_px()==(int)ceilf(24*density));
  config.terminal_menu_scale_offset=-3;assert(sdl_terminal_menu_font_px()==(int)ceilf(15*density));
  config.terminal_menu_scale_offset=2;assert(sdl_terminal_menu_font_px()==(int)ceilf(30*density));
  config.terminal_menu_scale_offset=-100;assert(sdl_terminal_menu_font_px()==(int)ceilf(12*density));
#else
  assert(sdl_terminal_menu_font_px()==0);
#endif
  sdl_pop_terminal_menu_scale();assert(!g_terminal_menu_scale_depth&&sdl_terminal_menu_font_px()==0);
 }
 config.bigger_font=true;config.terminal_menu_scale_offset=0;
 /* Same integer gameplay scale still needs pixel-font scope transitions. */
 g_terminal_menu_scale_override=2;int before=redraws;
 sdl_push_terminal_menu_scale();sdl_push_terminal_menu_scale();
#if SIL_SDL_MOBILE_BUILD
 assert(redraws==before+1);
#endif
 sdl_pop_terminal_menu_scale();assert(g_terminal_menu_scale_depth==1);
 sdl_pop_terminal_menu_scale();assert(!g_terminal_menu_scale_depth&&g_terminal_menu_scale_override==2);
 for(int i=0;i<24;i++)sdl_push_terminal_menu_scale();
 assert(g_terminal_menu_scale_depth==16);
 for(int i=0;i<24;i++)sdl_pop_terminal_menu_scale();
 assert(!g_terminal_menu_scale_depth&&!g_terminal_menu_scale_overflow_depth);
 sdl_pop_terminal_menu_scale();assert(g_terminal_menu_scale_override==2);

 int widths[]={30,30,34,70,74,94},heights[]={12,26,33,12,17,22};
 for(int c=0;c<6;c++){
  Term->wid=widths[c];Term->hgt=heights[c];portrait=c<3;
  knowledge_browser_layout k;knowledge_init_inventory_portrait_layout(&k,20,true,0);
  assert(k.list_rows>=1&&k.entry_rows>=1&&k.entry_row+k.entry_rows<=k.status_row);
  assert(k.status_row<k.prompt_row&&k.prompt_row<Term->hgt-reserved_rows);
  if(portrait)assert(k.group_rows<=2||Term->wid>=55);
  if(portrait&&k.stacked){
   int group_top=0,entry_top=0;region_count=0;
   knowledge_begin_split_touch_scroll_areas(&k,0,&group_top,4-k.group_rows,&entry_top,20-k.entry_rows);
   assert(region_count==2&&regions[0].offset==&group_top&&regions[1].offset==&entry_top);
   assert(regions[0].bottom<regions[1].top&&regions[0].max>=2);
   *regions[0].offset=regions[0].max;
   assert(group_top+k.group_rows==4&&entry_top==0);
  }
  if(Term->wid<55){
   assert(k.status_rows==2);char status[384];Term_clear();
   inventory_browser_group_status(INVENTORY_MENU_GROUP_PACK,0,status,sizeof(status));
   knowledge_draw_status(&k,1,status);
   assert(!strcmp(screen[k.status_row],"Volume 0.0/26.0 qt; 26.0 left"));
   assert(!strcmp(screen[k.status_row+1],"Weight 8.3/172.8 lb"));
   inventory_browser_group_status(INVENTORY_MENU_GROUP_HARNESS,1,status,sizeof(status));
   assert(strstr(status,"1.3/21.0 qt")&&strstr(status,"8.3/172.8 lb"));
   inventory_browser_group_status(INVENTORY_MENU_GROUP_QUIVER,1,status,sizeof(status));
   assert(strstr(status,"Arrows 23/48 (25 free)")&&strstr(status,"Weight 8.3/172.8 lb"));
  }
  knowledge_expand_entry_view(&k);assert(k.list_w==Term->wid&&k.entry_rows>=3);
  for(int page=0;page<4;page++){
   Term_clear();supply_draw_page_header(&k,page,-1,"Inventory");supply_register_page_tabs(&k,page);
   if(Term->wid<45){
    assert(clicks==2&&strstr(screen[0],supply_browser_page_text(page)));
    assert(ids[0]==100+supply_browser_turn_page(page,-1));
    assert(ids[1]==100+supply_browser_turn_page(page,1));
   }else assert(clicks==4);
  }
  ability_browser_layout a;ability_browser_init_layout(&a,19,"A long skill summary that must not consume the body viewport");
  assert(a.ability_rows>=1&&a.desc_rows>=1);
  assert(a.ability_col+a.ability_w<=Term->wid&&a.desc_col+a.desc_w<=Term->wid);
  assert(a.desc_row+a.desc_rows<=a.status_row&&a.prompt_row<Term->hgt-reserved_rows);
  if(Term->hgt<=18)assert(a.summary_rows==1&&a.skill_rows==1);
  if(Term->wid<60)assert(a.stacked);
  Term_clear();ability_browser_draw_skill_summary(&a,8,7,-1);
  if(Term->wid<60)assert(clicks==2&&strstr(screen[a.skill_row],"Song"));
 }
 /* Pointer input uses production entry routing, then the real two-row
  * Categories target restores the strip without resetting its selection/scroll.
  * A cancelled item confirmation leaves the entry routing state untouched. */
 Term->wid=30;Term->hgt=26;portrait=true;Term_clear();
 knowledge_browser_layout nav;
 knowledge_init_inventory_portrait_layout(&nav,20,true,0);
 inv_grp_cur=2;inv_grp_top=1;inv_entry_cur=1;inv_entry_top=1;inv_column=0;
 ui_menu_click_add(SUPPLY_CLICK_ENTRY_BASE+2,0,nav.entry_row,nav.list_w);
 int command=fixture_inventory_entry_event(fixture_click_at(3,nav.entry_row),UI_MENU_CLICK_PRIMARY);
 assert(command=='u'&&inv_column==1&&inv_entry_cur==2&&inv_entry_top==1);
 /* Confirmation is cancelled at the external action boundary: no state reset. */
 assert(inv_grp_cur==2&&inv_grp_top==1&&inv_entry_cur==2&&inv_entry_top==1);
 knowledge_expand_entry_view(&nav);assert(nav.group_rows==0&&nav.list_w==30);
 Term_clear();supply_draw_category_return(&nav,nav.tabs_row,false,"Quiver");
 assert(clicks==2&&hit_rects[0].y+1==hit_rects[1].y);
 assert(!strcmp(screen[nav.tabs_row],"< Categories"));
 int choice=fixture_click_at(15,nav.tabs_row+1);
 assert(supply_take_category_return(choice,UI_MENU_CLICK_PRIMARY,&inv_column));
 assert(inv_column==0&&inv_grp_cur==2&&inv_grp_top==1&&inv_entry_cur==2&&inv_entry_top==1);
 knowledge_init_inventory_portrait_layout(&nav,20,true,0);assert(nav.group_rows==2);
 inv_column=1;assert(supply_take_category_return(choice,UI_MENU_CLICK_HOVER,&inv_column)&&inv_column==1);
 assert(!supply_take_category_return('u',UI_MENU_CLICK_PRIMARY,&inv_column));
 assert(!supply_take_category_return('x',UI_MENU_CLICK_PRIMARY,&inv_column));
 assert(!supply_take_category_return('-',UI_MENU_CLICK_PRIMARY,&inv_column));
 Term_clear();knowledge_expand_entry_view(&nav);
 supply_draw_category_return(&nav,nav.tabs_row,true,"All equipped");
 assert(!strcmp(screen[nav.tabs_row],"< Slots")&&clicks==2);
 Term_clear();supply_draw_category_return(&nav,nav.header_row,false,"Potions");
 assert(!strcmp(screen[nav.header_row],"< Categories"));
 assert(!strcmp(screen[nav.divider_row],"Potions")&&clicks==2);

 /* The last Smithing action remains reachable in a 30x12 phone viewport. */
 Term->wid=30;Term->hgt=12;portrait=true;Term_clear();
 bool valid[7]={true,true,true,true,true,true,true};byte attrs[7]={0};char labels[7][32]={{0}};
 smith_root_draw(1,valid,attrs,labels);assert(clicks==6&&ids[5]==6&&drag_max==1);
 *drag_offset=drag_max;smith_root_draw(7,valid,attrs,labels);
 assert(clicks==6&&ids[0]==2&&ids[5]==7);
 puts("Mobile legacy menu font/layout checks passed");
}
"""
msys=Path(r'C:\msys64\mingw64\bin');cc=os.environ.get('CC') or shutil.which('gcc') or str(msys/'gcc.exe')
env=os.environ.copy()
if msys.exists():env['PATH']=str(msys)+os.pathsep+str(msys.parent/'usr/bin')+os.pathsep+env.get('PATH','')
with tempfile.TemporaryDirectory(prefix='sil-mobile-menu-') as directory:
 directory=Path(directory);source=directory/'test.c';source.write_text(harness+types+'\n'.join(parts)+input_fixture+main)
 for mobile in [1,0]:
  exe=directory/('check'+str(mobile)+('.exe' if os.name=='nt' else ''))
  subprocess.run([cc,'-std=c17','-DSIL_SDL_MOBILE_BUILD='+str(mobile),str(source),'-lm','-o',str(exe)],check=True,env=env)
  subprocess.run([str(exe)],check=True,env=env)

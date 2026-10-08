"""Render populated 30-column phone inventory cards with production C functions.

Only object-description/storage APIs and SDL/Term input are fixtures. Column
selection, metadata, wrapping, row counting, rendering and action dispatch use
production code. No game save, SDL display, emulator, or shared build is used.
"""
from pathlib import Path
import os,re,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
SOURCE=(ROOT/'src/cmd/ui/cmd-ui-knowledge.c').read_text()
def function(marker):
 start=SOURCE.rindex(marker);opening=SOURCE.index('{',start);depth=1;end=opening+1
 while depth:
  depth+=(SOURCE[end]=='{')-(SOURCE[end]=='}');end+=1
 return SOURCE[start:end]
header=(ROOT/'src/cmd/ui/cmd-ui-internal.h').read_text()
def struct(name):return re.search(r'struct '+name+r'\s*\{.*?\};',header,re.S).group(0)+'\ntypedef struct '+name+' '+name+';\n'
types=struct('knowledge_browser_layout')+struct('supply_list_columns')
types+=re.search(r'typedef struct equipment_entry_columns\s*\{.*?\} equipment_entry_columns;',SOURCE,re.S).group(0)
parts=[function(x) for x in [
 'static int equipment_entry_wrap_take(', 'static bool equipment_entry_wrap_next(',
 'static int equipment_entry_wrapped_rows(', 'static int supply_entry_wrapped_row_count(',
 'static void equipment_entry_init_columns(', 'static void supply_init_columns(',
 'static bool equipment_entry_volume_text(', 'static bool equipment_entry_display_values(',
 'static bool equipment_entry_wrapped_values(', 'static int equipment_entry_display_rows(',
 'static int display_equipment_slot_entries_wrapped(', 'static void supply_entry_append_metadata(',
 'static bool supply_entry_wrapped_values(', 'static int display_supply_list_wrapped(',
 'static bool inventory_page_use_entry(', 'static bool supply_take_category_return(',
 'static void inventory_browser_group_limit_status(', 'static void inventory_browser_weight_status(',
 'static void inventory_browser_group_status(', 'static void inventory_browser_reserve_status(',
 'static int supply_put_wrapped(', 'static void knowledge_draw_status(']]
harness=r"""
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
typedef const char *cptr;typedef unsigned char byte;
static bool get_sdl_bigger_font(void) {return true;}
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ABS(x) abs(x)
#define N_ELEMENTS(a) (sizeof(a)/sizeof((a)[0]))
#define TERM_L_DARK 3
#define TERM_L_BLUE 4
#define TERM_UI_SELECTED 128
#define SDL_memcpy memcpy
#define SDL_strlcat cat
#define SUPPLY_CLICK_ENTRY_BASE 10000
#define SUPPLY_CLICK_CATEGORY_RETURN (-16)
#define UI_MENU_CLICK_PRIMARY 1
#define UI_MENU_CLICK_HOVER 3
#define INVEN_WIELD 0
#define INVEN_ARM 1
#define INVEN_TOTAL 12
#define TV_RING 1
#define TV_AMULET 2
#define TR3_TWO_HANDED 1
#define QUIVER_INDEX 100
#define SUPPLIES_INDEX 200
#define OBJECT_STORAGE_PACK 1
#define OBJECT_STORAGE_HARNESS 2
#define EQUIPMENT_ENTRY_PLACEHOLDER_NONE 0
#define EQUIPMENT_ENTRY_PLACEHOLDER_RESERVED 1
#define SUPPLY_GROUP_FOOD 1
#define SUPPLY_GROUP_LIGHTS 2
#define SUPPLY_GROUP_SUPPLY 3
#define SUPPLY_GROUP_JEWELRY_PRESETS 4
#define SUPPLY_GROUP_JEWELRY 5
#define SUPPLY_GROUP_HERBS 6
enum inventory_limit_group {INV_LIMIT_PACK,INV_LIMIT_HARNESS};
typedef int supply_floor_action;
typedef int inventory_menu_group;
#define INVENTORY_MENU_GROUP_ALL 0
#define INVENTORY_MENU_GROUP_PACK 1
#define INVENTORY_MENU_GROUP_HARNESS 2
#define INVENTORY_MENU_GROUP_QUIVER 3
#define INVENTORY_MENU_GROUP_JEWELRY 4
#define QUIVER_ARROW_CAPACITY 99
typedef struct {int k_idx,tval,weight,number,volume,turns; cptr name;} object_type;
typedef struct {int item_idx,supply_idx,equip_idx,floor_idx,placeholder;bool equipped,show_empty_slot,active;object_type obj;cptr source;} equipment_list_entry;
typedef struct {int item_idx,k_idx,total,supply_idx,equip_idx,preset_idx,floor_idx;bool equipped,single_item_display;} supply_list_entry;
typedef struct {bool aware;unsigned flags3;} object_kind;
static struct {int k_max;} sizes={10},*z_info=&sizes;
static object_kind k_info[10];static object_type inventory[12],supply_objects[10];
static byte tval_to_attr[256];static bool use_bigtile;
static int o_max=100,entry_dialogs,world_actions,draw_w=30;
static struct {int wid;} term_state={30},*Term=&term_state;
static struct {int total_weight;} player={83},*p_ptr=&player;
static int limit_used=13,limit_capacity=100,carriage_saved=9;
static bool sdl_touch_only_device_active(void){return true;}
static int inventory_limit_usage_for_group(enum inventory_limit_group g){return limit_used;}
static int inventory_limit_limit_for_group(enum inventory_limit_group g){return limit_capacity;}
static int inventory_limit_carriage_savings_for_group(enum inventory_limit_group g){return carriage_saved;}
static int weight_limit(void){return 1250;}
static cptr inventory_browser_group_text(inventory_menu_group g){return "Pack";}
static int player_quiver_arrow_count(void){return 23;}
static int player_quiver_arrow_space(void){return 76;}
static char screen[48][96];static int hits[48],hit_count;
static int strnfmt(char *s,size_t n,cptr f,...) {va_list a;va_start(a,f);int r=vsnprintf(s,n,f,a);va_end(a);return r;}
static void strnfcat(char *s,size_t n,size_t *used,cptr f,...) {va_list a;va_start(a,f);vsnprintf(s+*used,n-*used,f,a);va_end(a);*used=strlen(s);}
static size_t cat(char *s,cptr a,size_t n) {size_t len=strlen(s);if(len<n)snprintf(s+len,n-len,"%s",a);return strlen(s);}
#define SDL_strlcpy(d,s,n) snprintf(d,n,"%s",s)
static int utf8_sequence_len_n(cptr s,int n){return n&&*s?1:0;}
static int utf8_display_width_n(cptr s,int n){return n;}
static int utf8_safe_prefix_len(cptr s,int n){return n;}
static void Term_erase(int x,int y,int n){assert(x>=0&&x<draw_w&&y>=0&&y<40);memset(screen[y]+x,' ',MIN(n,draw_w-x));screen[y][draw_w]=0;}
static void Term_putstr(int x,int y,int n,byte a,cptr s){assert(x>=0&&x<draw_w&&y>=0&&y<40);int len=MIN(n,(int)strlen(s));assert(x+len<=draw_w);memcpy(screen[y]+x,s,len);}
static void ui_menu_click_add(int id,int x,int y,int w){assert(x>=0&&x+w<=30&&y>=0&&y<40);hits[hit_count++]=id;}
static void supply_put_fitted(int x,int y,int w,byte a,cptr s){Term_putstr(x,y,MIN(w,(int)strlen(s)),a,s);}
static void supply_browser_fill_row(int x,int y,int w,byte a){Term_erase(x,y,w);}
static byte supply_browser_selected_attr(byte a){return a+TERM_UI_SELECTED;}
static int supply_browser_selection_width(int l,int n,int w,cptr s,int available){return available;}
static void draw_supply_icon(int x,int y,const object_type *o){Term_putstr(x,y,1,1,"*");}
static cptr equipment_entry_source_text(equipment_list_entry *e,char *s,size_t n){return e->source?e->source:"";}
static object_type *equipment_entry_object(equipment_list_entry *e){return &e->obj;}
static bool equipment_entry_is_active_combat(equipment_list_entry *e){return e->active;}
static void browser_entry_label_prefix(char *s,size_t n,int idx){strnfmt(s,n,"%c ",'a'+idx);}
static void object_desc(char *s,size_t n,const object_type *o,bool full,int mode){strnfmt(s,n,"%s",o->name);}
static bool player_equipment_slot_counts_as_equipped(int slot){return false;}
static bool hand_and_a_half_bonus(const object_type *o){return false;}
static bool weapon_glows(const object_type *o){return false;}
static byte object_display_color(const object_type *o,byte a){return a;}
static byte get_supply_item_color(int idx,bool aware){return 1;}
static cptr equipment_slot_text(int slot){return "Hands";}
static bool object_is_quivered_arrow(const object_type *o){return false;}
static enum inventory_limit_group inventory_limit_group_for_object(const object_type *o){return INV_LIMIT_HARNESS;}
static int inventory_limit_space_for_object(const object_type *o){return o->volume;}
static int supply_entry_turns(const supply_list_entry *e,const object_type *o){return o->turns;}
static object_type *supply_entry_display_object(supply_list_entry *e,bool aware,object_type *fake){return &supply_objects[e->k_idx];}
static void supply_entry_display_name(char *s,size_t n,supply_list_entry *e,const object_type *o,int group,bool compact){strnfmt(s,n,"%s",o->name);}
static bool confirm_equipment_entry_action(cptr action,equipment_list_entry *e){return false;}
static cptr inventory_page_use_action_text(equipment_list_entry *e,int action){return "Equip";}
static bool floor_entry_perform_action(int idx,int action,int slot){++world_actions;return true;}
static bool equipment_entry_has_active_item_menu(equipment_list_entry *e){return e->active;}
static int equipment_entry_item_handle(equipment_list_entry *e){return e->item_idx;}
/* The real Choose Item Setup dialog's cancellation boundary. */
static bool do_cmd_active_item(int idx){++entry_dialogs;return false;}
static void do_cmd_use_item_by_index(int idx){++world_actions;}
static bool equipment_entry_is_quiver_arrow(equipment_list_entry *e){return false;}
static bool do_cmd_move_item_to_storage(int idx,int storage){++world_actions;return true;}
static bool player_inventory_handle_is_carried(int idx){return false;}
static object_type *player_inventory_object(int idx){return &inventory[0];}
static bool object_can_choose_pack_or_harness(const object_type *o){return false;}
static void inventory_storage_move_failure_text(const object_type *o,int storage,char *s,size_t n){}
static bool supplies_menu_use_entry(supply_list_entry *e,int action){++world_actions;return true;}
"""
main=r"""
static void clear(void){memset(screen,' ',sizeof(screen));for(int r=0;r<48;r++)screen[r][draw_w]=0;hit_count=0;}
static void all_text(char *s,size_t n,int rows){s[0]=0;for(int r=0;r<rows;r++){cat(s,screen[r],n);cat(s," ",n);}}
int main(void){
 knowledge_browser_layout k={.term_wid=30,.term_hgt=32,.list_col=0,.list_w=30,.entry_rows=20};
 equipment_list_entry sword={.item_idx=7,.equip_idx=0,.supply_idx=-1,.equipped=true,.active=true,.source="Active",
  .obj={.k_idx=1,.tval=3,.weight=20,.number=1,.volume=13,.name="Shortsword (+0,1d7) [+1]"}};
 equipment_entry_columns e;equipment_entry_init_columns(&k,&sword,1,false,&e);
 assert(e.name_w==28&&!e.show_volume&&!e.show_weight&&!e.show_source&&e.inline_metadata);
 clear();int rows=0;assert(display_equipment_slot_entries_wrapped(&k,0,20,&sword,1,0,0,0,&e,&rows)==1);
 assert(rows==3);char text[4096];all_text(text,sizeof(text),rows);
 assert(strstr(text,"Shortsword")&&strstr(text,"(+0,1d7)")&&strstr(text,"[+1]")&&strstr(text,"[active]"));
 assert(strstr(text,"1.3 qt")&&strstr(text,"2.0 lb")&&hit_count==3);
 for(int i=0;i<hit_count;i++)assert(hits[i]==SUPPLY_CLICK_ENTRY_BASE);

 /* Full authored prefix/name/suffix/curse plus every floor and metadata token. */
 equipment_list_entry long_item={.item_idx=-7,.equip_idx=-1,.supply_idx=-1,.floor_idx=7,.source="Floor",
  .obj={.k_idx=2,.tval=3,.weight=40,.number=1,.volume=20,
   .name="Clawed Set of Gauntlets of the Torturer [+0,1d2] <-2> {cursed}"}};
 equipment_entry_init_columns(&k,&long_item,1,false,&e);clear();
 display_equipment_slot_entries_wrapped(&k,0,20,&long_item,1,0,0,1,&e,&rows);all_text(text,sizeof(text),rows);
 assert(rows<=5&&strstr(text,"-) a Clawed Set of")&&strstr(text,"Gauntlets of the Torturer"));
 assert(strstr(text,"[+0,1d2]")&&strstr(text,"<-2>")&&strstr(text,"{cursed}")&&strstr(text,"[floor]"));
 assert(strstr(text,"2.0 qt")&&strstr(text,"4.0 lb"));
 /* Equipped mode carries its location in the metadata, never another side column. */
 sword.obj.number=7;sword.obj.volume=91;equipment_entry_init_columns(&k,&sword,1,true,&e);clear();
 display_equipment_slot_entries_wrapped(&k,0,20,&sword,1,0,0,1,&e,&rows);all_text(text,sizeof(text),rows);
 assert(strstr(text,"x7")&&strstr(text,"9.1 qt")&&strstr(text,"14.0 lb")&&strstr(text,"Active"));

 equipment_list_entry empty={.equip_idx=2,.show_empty_slot=true,.source="L ring"};
 equipment_entry_init_columns(&k,&empty,1,true,&e);clear();
 display_equipment_slot_entries_wrapped(&k,0,20,&empty,1,0,0,0,&e,&rows);
 all_text(text,sizeof(text),rows);assert(rows==1&&strstr(text,"(empty) L ring"));

 supply_list_columns s;supply_init_columns(&k,SUPPLY_GROUP_LIGHTS,&s);
 assert(s.name_w==29&&!s.show_qty&&!s.show_turns&&!s.show_weight);
 supply_objects[1]=(object_type){.k_idx=1,.weight=20,.number=2,.turns=1000,.name="Wooden Torches"};
 supply_list_entry torch={.k_idx=1,.total=2,.supply_idx=0,.equip_idx=-1,.preset_idx=-1};
 object_type fake,*display;byte attr;char built[384];
 assert(supply_entry_wrapped_values(&k,&s,&torch,0,SUPPLY_GROUP_LIGHTS,false,&fake,built,sizeof(built),&display,&attr,&rows));
 clear();display_supply_list_wrapped(&k,0,20,&torch,1,0,0,SUPPLY_GROUP_LIGHTS,1,&s,false);
 all_text(text,sizeof(text),rows);assert(rows==2&&strstr(text,"Wooden Torches"));
 assert(strstr(text,"x2")&&strstr(text,"2.0 lb")&&strstr(text,"1000 turns"));
 supply_objects[2]=(object_type){.k_idx=2,.weight=1,.number=1,.name="Ring of Secrets <+1>"};
 supply_list_entry ring={.k_idx=2,.total=1,.supply_idx=1,.equip_idx=-1,.preset_idx=-1,.floor_idx=9};
 supply_init_columns(&k,SUPPLY_GROUP_JEWELRY,&s);clear();
 assert(supply_entry_wrapped_values(&k,&s,&ring,0,SUPPLY_GROUP_JEWELRY,false,&fake,built,sizeof(built),&display,&attr,&rows));
 display_supply_list_wrapped(&k,0,20,&ring,1,0,0,SUPPLY_GROUP_JEWELRY,1,&s,false);
 all_text(text,sizeof(text),rows);assert(strstr(text,"-) a Ring of Secrets <+1>"));
 assert(strstr(text,"[floor]")&&strstr(text,"x1")&&strstr(text,"0.1 lb"));

 /* Actual production dispatcher opens Choose Item Setup. Cancel returns false,
  * so focus/selection/scroll remain available for the explicit Categories input. */
 int column=1,group_cur=2,group_top=1,entry_cur=3,entry_top=1;char failure[80]={0};
 sword.obj.number=1;assert(!inventory_page_use_entry(&sword,0,failure,sizeof(failure)));
 assert(entry_dialogs==1&&world_actions==0&&column==1);
 assert(supply_take_category_return(SUPPLY_CLICK_CATEGORY_RETURN,UI_MENU_CLICK_PRIMARY,&column));
 assert(column==0&&group_cur==2&&group_top==1&&entry_cur==3&&entry_top==1&&world_actions==0);
 /* Complete capacity summary at the actual intermediate landscape widths,
  * plus narrow portrait/short landscape. Production formatter, measured row
  * reservation and status renderer must expose every value without ellipsis. */
 const int grids[][2]={{30,12},{30,30},{66,15},{77,17}};
 for(size_t i=0;i<N_ELEMENTS(grids);i++)for(int actual=0;actual<2;actual++)for(int over=0;over<2;over++){
  draw_w=Term->wid=grids[i][0];int h=grids[i][1];limit_capacity=actual?320:100;
  limit_used=actual?(over?343:133):(over?123:13);carriage_saved=actual?14:9;
  knowledge_browser_layout status={.term_wid=draw_w,.term_hgt=h,.header_row=2,.divider_row=3,.list_row=4,
   .prompt_row=h-3,.status_row=h-5,.status_rows=2,.list_rows=h-9,.group_row=4,.group_rows=1,
   .entry_header_row=5,.entry_row=6,.entry_rows=h-11,.stacked=true};
  if(draw_w>=55){status.stacked=false;status.status_rows=1;status.status_row=h-4;
   status.list_rows=status.entry_rows=status.group_rows=h-8;status.entry_row=status.group_row=4;status.entry_header_row=2;}
  inventory_browser_reserve_status(&status,INVENTORY_MENU_GROUP_PACK);
  char summary[384];inventory_browser_group_status(INVENTORY_MENU_GROUP_PACK,7,summary,sizeof(summary));
  assert(status.status_rows>=equipment_entry_wrapped_rows(summary,draw_w));
  assert(status.entry_rows>=1&&status.entry_row+status.entry_rows<=status.status_row);
  assert(status.status_row+status.status_rows==status.prompt_row);
  clear();knowledge_draw_status(&status,1,summary);text[0]=0;
  for(int row=status.status_row;row<status.prompt_row;row++)cat(text,screen[row],sizeof(text));
  assert(!strstr(text,"...")&&strstr(text,actual?(over?"34.3/32.0":"13.3/32.0"):(over?"12.3/10.0":"1.3/10.0")));
  assert(strstr(text,over?"2.3 over":(actual?"18.7 left":"8.7 left")));assert(strstr(text,actual?"Carriage saves 1.4 qt":"Carriage saves 0.9 qt"));
  assert(strstr(text,"Weight 8.3/125.0 lb"));
 }
 puts("Populated 30-column inventory/equipment/supply/jewelry render and dispatch checks passed");
}
"""
msys=Path(r'C:\msys64\mingw64\bin');cc=os.environ.get('CC') or shutil.which('gcc') or str(msys/'gcc.exe')
env=os.environ.copy()
if msys.exists():env['PATH']=str(msys)+os.pathsep+str(msys.parent/'usr/bin')+os.pathsep+env.get('PATH','')
with tempfile.TemporaryDirectory(prefix='sil-mobile-cards-') as directory:
 directory=Path(directory);source=directory/'test.c';exe=directory/('check.exe' if os.name=='nt' else 'check')
 source.write_text(harness+types+'\n'.join(parts)+main)
 subprocess.run([cc,'-std=c17',str(source),'-o',str(exe)],check=True,env=env)
 subprocess.run([str(exe)],check=True,env=env)

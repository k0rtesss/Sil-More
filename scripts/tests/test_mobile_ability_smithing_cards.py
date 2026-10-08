"""Populated production ability cards and complete Smithing costs at 30 columns.

Compiles the actual layout, measured card stride, renderer, wrapping and cost
report functions. Only engine queries and the Term display are fixtures.
"""
from pathlib import Path
import os,re,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
A=(ROOT/'src/cmd/ui/cmd-ui-abilities.c').read_text()
S=(ROOT/'src/cmd/ui/cmd-ui-smithing.c').read_text()
def function(source, marker):
 start=source.rindex(marker);opening=source.index('{',start);depth=1;end=opening+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
def typedef(source,name):
 return re.search(r'typedef struct '+name+r'\s*\{.*?\} '+name+r';',source,re.S).group(0)
types=typedef(A,'ability_browser_layout')+typedef(A,'ability_browser_entry')
types+=typedef(S,'smithing_cost_type')+typedef(S,'smith_calculation_report')
parts=[function(A,m) for m in ['static int ability_browser_wrap_take(',
 'static bool ability_browser_wrap_next(', 'static int ability_browser_wrapped_rows(',
 'static void ability_browser_init_layout_normal(',
 'static void ability_browser_init_layout(', 'static char ability_browser_entry_letter(',
 'static void ability_browser_size_entry_cards(', 'static void ability_browser_draw_ability_list(']]
parts += [function(S,m) for m in ['static void smith_report_add(',
 'static void smith_report_costs(', 'static void smith_ui_fit_text(',
 'static void smith_ui_draw_fitted(', 'static void smith_ui_put_fitted(',
 'static int smith_ui_menu_row_width(', 'static void smith_ui_put_menu_row(',
 'static void smith_ui_draw_cost_heading(']]
harness=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
typedef const char *cptr;typedef unsigned char byte;
static bool get_sdl_bigger_font(void) {return true;}
typedef struct {int level;cptr state;} ability_type;
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ABS(a) abs(a)
#define N_ELEMENTS(a) (sizeof(a)/sizeof((a)[0]))
#define SDL_memcpy memcpy
#define SDL_strlcpy(d,s,n) snprintf(d,n,"%s",s)
#define SDL_strlcat(d,s,n) strncat(d,s,n-strlen(d)-1)
#define TERM_SLATE 1
#define TERM_L_BLUE 2
#define TERM_L_DARK 3
#define TERM_DARK 4
#define TERM_WHITE 5
#define TERM_YELLOW 6
#define TERM_RED 7
#define ABILITY_BROWSER_INSIGHT 100
#define ABILITY_MENU_CLICK_ABILITY_BASE 1000
#define SMITH_CLICK_CALC 99
#define SMITH_REPORT_MAX_LINES 256
static int height=30,reserved=2,click_id[64],click_rows[64],click_count;
static bool portrait=true;
static char screen[40][31];
static int strnfmt(char*s,size_t n,cptr f,...) {va_list a;va_start(a,f);int r=vsnprintf(s,n,f,a);va_end(a);return r;}
static cptr format(cptr f,...) {static char s[1024];va_list a;va_start(a,f);vsnprintf(s,sizeof(s),f,a);va_end(a);return s;}
static int utf8_sequence_len_n(cptr s,int n){return n&&*s?1:0;}
static int utf8_safe_prefix_len(cptr s,int n){return n;}
static int utf8_display_width_n(cptr s,int n){return n;}
static int ability_browser_cell_width_n(cptr s,int n){return n;}
static int smith_ui_utf8_prefix_len(cptr s,int n){return MIN(n,(int)strlen(s));}
static void Term_get_size(int*w,int*h){*w=30;*h=height;}
static int sdl_main_view_visible_col0(void){return 0;}
static int sdl_main_view_visible_cols(void){return 30;}
static bool sdl_mobile_portrait_layout_active(void){return portrait;}
static int sdl_touch_menu_button_reserved_rows(void){return reserved;}
static bool insight_system_enabled(void){return false;}
static bool indexed_menu_letters_enabled(void){return true;}
static void indexed_menu_focus_prefix(char*s,size_t n,int i){strnfmt(s,n,"> ");}
static void indexed_menu_normal_prefix(char*s,size_t n,int i){strnfmt(s,n,"  ");}
static byte ability_browser_selected_attr(byte a){return a+128;}
static int ability_learning_skill(ability_type*b){return 0;}
static int ability_required_skill(ability_type*b,int s){return b->level;}
static void ability_browser_entry_state(char*s,size_t n,int skill,const void*e);
static void Term_putstr(int x,int y,int n,byte a,cptr s){assert(x>=0&&y>=0&&y<height);int len=MIN(n,(int)strlen(s));assert(x+len<=30);memcpy(screen[y]+x,s,len);}
static void ability_browser_put_fitted(int x,int y,int n,byte a,cptr s){Term_putstr(x,y,MIN(n,(int)strlen(s)),a,s);}
static void ability_browser_fill_row(int x,int y,int n,byte a){assert(y<height&&x+n<=30);memset(screen[y]+x,' ',n);}
static void ui_menu_click_add(int id,int x,int y,int w){assert(x>=0&&x+w<=30&&y<height);click_id[click_count]=id;click_rows[click_count++]=y;}
static int smith_ui_content_bottom_row(void){return height-reserved-1;}
static void Term_erase(int x,int y,int w){assert(x>=0&&x+w<=30&&y>=0&&y<height);memset(screen[y]+x,' ',w);}
static int smith_ui_safe_width(int x,int w){return MIN(w,30-x);}
static int indexed_menu_prefix_col(int col){return MAX(0,col-2);}
static int smith_ui_next_column_after(int col){return 30;}
static int smith_ui_term_wid(void){return 30;}
static byte smith_ui_selected_attr(byte attr){return attr+128;}
static void smith_ui_fill_row(int x,int y,int w,byte attr){Term_erase(x,y,w);}
static int smith_ui_line_width(int col){return 30-col;}
static void ui_menu_click_add_text_token(int id,int x,int y,cptr text,cptr token){ui_menu_click_add(id,x,y,strlen(text));}
static void clear(void){for(int y=0;y<40;y++){memset(screen[y],' ',30);screen[y][30]=0;}click_count=0;}
'''
state=r'''
static void ability_browser_entry_state(char*s,size_t n,int skill,const void*e){const ability_browser_entry*entry=e;strnfmt(s,n,"%s",entry->b_ptr->state);}
'''
main=r'''
static void collect(char*s,size_t n,int first,int rows){s[0]=0;for(int y=first;y<first+rows;y++){strncat(s,screen[y],n-strlen(s)-1);strncat(s,"\n",n-strlen(s)-1);}}
int main(void){
 ability_type facts[]={{10,"500 XP"},{5,"0 XP"},{12,"locked"}};
 ability_browser_entry entries[]={
  {.b_ptr=&facts[0],.name="Strength in Adversity",.attr=5},
  {.b_ptr=&facts[1],.name="Polearm Mastery",.attr=5},
  {.b_ptr=&facts[2],.name="Oath of the Valorous Heart forever",.attr=5}};
 for(int h=12;h<=30;h+=18) for(int p=0;p<2;p++){
  height=h;portrait=p;ability_browser_layout l;
  ability_browser_init_layout(&l,3,"Choose an ability");
  ability_browser_size_entry_cards(&l,entries,3);
  assert(l.ability_entry_rows==3);assert(l.ability_rows%3==0);
  assert(l.desc_header_row>=l.ability_row+l.ability_rows);
  assert(l.desc_row+l.desc_rows<=l.status_row);
  int visible=l.ability_rows/l.ability_entry_rows;
  for(int i=0;i<3;i++){
   clear();ability_browser_draw_ability_list(&l,0,entries,3,i,i,true);
   char text[2048];collect(text,sizeof(text),l.ability_row,3);
   if(i==0)assert(strstr(text,"Strength in Adversity")&&strstr(text,"Level 10 | 500 XP"));
   if(i==1)assert(strstr(text,"Polearm Mastery")&&strstr(text,"Level 5 | 0 XP"));
   if(i==2)assert(strstr(text,"Oath of the Valorous")&&strstr(text,"Heart")&&strstr(text,"forever")&&strstr(text,"Level 12 | locked"));
   assert(click_count==MIN(3-i,visible)*3);
   for(int r=0;r<3;r++){assert(click_id[r]==1000+i);assert(click_rows[r]==l.ability_row+r);}
  }
 }
 smithing_cost_type cost={.uses=3,.drain=2,.mithril=13,.star_iron=24,
  .str=1,.dex=2,.con=3,.gra=4,.exp=12345,.weaponsmith=1,.armoursmith=1,
  .jeweller=1,.enchantment=1,.artifice=1,.alloy_mastery=1};
 smith_calculation_report report={0};smith_report_costs(&report,26,&cost,170,true);
 char all[8192]="";
 for(int i=0;i<report.count;i++){assert(strlen(report.text[i])<=26);strcat(all,report.text[i]);strcat(all,"\n");}
 const char*sentinels[]={"COMPLETE COSTS","Requires Reforging","Requires Weaponsmith","Requires Armoursmith","Requires Jeweller","Requires Enchantment","Requires Artifice","Requires Alloy Mastery","Forge uses: 3","Smithing ranks: 2","Mithril: 1.3 lb","Star iron: 2.4 lb","Strength: 1","Dexterity: 2","Constitution: 3","Grace: 4","Experience: 12345","Turns: 170"};
 for(size_t i=0;i<N_ELEMENTS(sentinels);i++)assert(strstr(all,sentinels[i]));
 for(int h=12;h<=30;h+=18){height=h;int page=height-reserved-4,seen=0;for(int top=0;top<report.count;top+=page)for(int row=0;row<page&&top+row<report.count;row++)seen++;assert(seen==report.count);}
 height=12;clear();smith_ui_draw_cost_heading(2,6,4,TERM_WHITE);assert(strstr(screen[6],"Cost: more in Details"));assert(click_count==1&&click_id[0]==SMITH_CLICK_CALC);
 height=30;clear();smith_ui_draw_cost_heading(2,6,4,TERM_WHITE);assert(strstr(screen[6],"Cost:")&&!strstr(screen[6],"more"));
 /* Actual populated Smithing list renderer: the longest shipped property and
  * granted ability labels remain complete, as does the compact alloy action. */
 const char*labels[]={"Oath of the Valorous Heart","Resistance to Stunning","Cycle alloy metal"};
 for(int i=0;i<3;i++){clear();smith_ui_put_menu_row(i+1,2,4,TERM_WHITE,labels[i],true);assert(strstr(screen[4],labels[i]));assert(click_count==1&&click_id[0]==i+1);}
 puts("Populated 30x12/30x30 ability names, metadata, touch rows and full Smithing costs passed");
}
'''
msys=Path(r'C:\msys64\mingw64\bin');cc=os.environ.get('CC') or shutil.which('gcc') or str(msys/'gcc.exe')
env=os.environ.copy()
if msys.exists():env['PATH']=str(msys)+os.pathsep+str(msys.parent/'usr/bin')+os.pathsep+env.get('PATH','')
with tempfile.TemporaryDirectory(prefix='sil-ability-cards-') as directory:
 directory=Path(directory);source=directory/'test.c';exe=directory/('check.exe' if os.name=='nt' else 'check')
 source.write_text(harness+types+state+'\n'.join(parts)+main)
 subprocess.run([cc,'-std=c17',str(source),'-o',str(exe)],check=True,env=env)
 subprocess.run([str(exe)],check=True,env=env)

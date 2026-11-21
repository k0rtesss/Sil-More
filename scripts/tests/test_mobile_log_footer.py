"""Actual populated Log caller: reserved footer, wrapped continuations and Exit."""
from pathlib import Path
import ast,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
SOURCE=(ROOT/'src/cmd/ui/cmd-ui-main-menu.c').read_text()
def function(marker):
 start=SOURCE.rindex(marker);end=SOURCE.index('{',start)+1;depth=1
 while depth:
  depth+=(SOURCE[end]=='{')-(SOURCE[end]=='}');end+=1
 return SOURCE[start:end]
# Reuse only the small standalone engine/input fixture (not its tests or main).
tree=ast.parse((ROOT/'scripts/tests/test_mobile_about_blitz.py').read_text())
harness=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='harness' for t in n.targets))
harness=harness.replace('static void Term_get_size(int*w,int*h){*w=30;*h=12;}','static int test_w=30,test_h=12,reserved=3; static void Term_get_size(int*w,int*h){*w=test_w;*h=test_h;}')
harness=harness.replace('static bool sdl_touch_only_device_active(void){return true;}','static bool touch_only=true,bigger=true,exit_registered,measuring_menu; static bool sdl_touch_only_device_active(void){return touch_only;}')
harness=harness.replace('static bool get_sdl_menu_bigger_font(void){return true;}','static bool get_sdl_menu_bigger_font(void){return bigger;}')
harness=harness.replace('static void ui_menu_click_begin(void){}','static void ui_menu_click_begin(void){exit_registered=false;}')
harness=harness.replace('static void text_out_to_screen(byte a,cptr t){}','static void text_out_to_screen(byte a,cptr t);')
harness=harness.replace('bool playing,is_dead,quit_to_menu,leaving;','bool playing,is_dead,quit_to_menu,leaving;unsigned redraw;')
fixtures=r'''
#define USE_SDL
#define SDL_strlcpy(d,s,n) snprintf(d,n,"%s",s)
#define KTRL(c) ((c)&31)
#define SDL_MENU_FONT_MESSAGES SDL_MENU_FONT_HELP
#define LOG_HISTORY_FILTER_ALL 0
#define LOG_HISTORY_FILTER_MESSAGES 1
#define LOG_HISTORY_FILTER_COMBAT 2
#define LOG_HISTORY_FILTER_NOTES 3
#define LOG_HISTORY_CLICK_FILTER_ALL (-20001)
#define LOG_HISTORY_CLICK_FILTER_MESSAGES (-20002)
#define LOG_HISTORY_CLICK_FILTER_COMBAT (-20003)
#define LOG_HISTORY_CLICK_FILTER_NOTES (-20004)
#define LOG_HISTORY_PREV_FILTER_KEY '['
#define LOG_HISTORY_NEXT_FILTER_KEY ']'
#define LOG_HISTORY_MAX_ENTRIES 3
#define LOG_HISTORY_ENTRY_MESSAGE 0
#define LOG_HISTORY_ENTRY_COMBAT 1
#define SDL_TOUCH_MENU_CATEGORY_OTHER 0
#define TERM_L_DARK 8
#define TERM_SHADE 0
#define PR_MAP 1
#define PR_BASIC 2
#define PR_EXTRA 4
static bool character_generated,character_dungeon,character_icky;
static void handle_stuff(void){}
typedef short s16b;
typedef struct {byte attacker_attr,defender_attr;char attacker_char,defender_char;} combat_roll;
static combat_roll roll={0x81,0x82,(char)0x8a,(char)0x8b};
typedef struct {int kind,message_age;combat_roll*roll;} log_history_entry;
typedef struct {cptr text;int len;} log_history_note_line;
static log_history_entry log_history_entries[3];static log_history_note_line log_history_note_lines[3];
static cptr text[]={"FIRST A very long recorded message with all words and numbers 12345 retained. A very long recorded message with all words and numbers 12345 retained. A very long recorded message with all words and numbers 12345 retained. LASTTAIL",
 "Combat ?? to ?? att +14 hit 7 evn +8 damage (2d7+1d5) net 12 protection 3 COMBATTAIL", "Oldest message OLDESTTAIL"};
static int cursor_y,cursor_x,body_bottom_seen,prompt_seen;static char reached[8192];
static void Term_gotoxy(int x,int y){cursor_x=x;cursor_y=y;}
static void text_out_to_screen(byte a,cptr t){assert(cursor_y>=0&&cursor_y<=body_bottom_seen);assert(cursor_x+(int)strlen(t)<=test_w);strncat(reached,t,sizeof(reached)-strlen(reached)-1);cursor_x+=strlen(t);}
static int utf8_sequence_len(cptr s){return *s?1:0;}
static int utf8_display_width_n(cptr s,int n){return n;}
static int utf8_safe_prefix_len(cptr s,int n){return n;}
static int sdl_touch_menu_button_reserved_rows(void){assert(!measuring_menu||exit_registered);return touch_only?reserved:0;}
static int sdl_menu_font_button_available_columns(int row,int columns){return row==1?MIN(columns,18):columns;}
static void tutorial_game_menu(cptr a,cptr b){}
static bool dismiss_active_narrative_banner(void){return false;}
static void do_cmd_redraw(void){}
static int log_history_clamp_filter(int i){return i;}
static int log_history_collect_notes(void){for(int i=0;i<3;i++)log_history_note_lines[i]=(log_history_note_line){text[i],strlen(text[i])};return 3;}
static int log_history_collect_entries(log_history_entry*e,int n,int f){for(int i=0;i<3;i++)e[i]=(log_history_entry){i==1?1:0,i,&roll};return 3;}
static cptr message_str(s16b age){return text[age];}
static byte message_color(s16b age){return TERM_WHITE;}
static void log_history_entry_search_text(const log_history_entry*e,char*s,size_t n){strnfmt(s,n,"%s",text[e->message_age]);}
static void log_history_draw_combat_entry(const log_history_entry*e,int y,int offset){assert(y<=body_bottom_seen);strncat(reached,text[e->message_age],sizeof(reached)-strlen(reached)-1);}
static bool use_bigtile=true;
static bool graphics_are_ascii(void){return false;}
static int icon_count;
static void Term_queue_char(int x,int y,byte a,char c,byte ta,char tc){assert(x>=0&&x<test_w&&y>=0&&y<=body_bottom_seen);if(a==roll.attacker_attr||a==roll.defender_attr)icon_count++;else if(a==255)assert(c==-1);else {char glyph[2]={c,0};strncat(reached,glyph,sizeof(reached)-strlen(reached)-1);}}
/* The SDL fixture checks actual overlay content. Here retain a deliberately
 * oversized synthetic entry to exercise the caller's continuation handling. */
static void log_history_build_combat_line(const combat_roll*r,log_history_combat_line*line){memset(line,0,sizeof(*line));strnfmt(line->text,sizeof(line->text),"%s",text[1]);memset(line->attrs,TERM_WHITE,strlen(line->text));line->is_tile[7]=line->is_tile[13]=true;line->icons[7]=r->attacker_char;line->icons[13]=r->defender_char;line->attrs[7]=r->attacker_attr;line->attrs[13]=r->defender_attr;line->attrs[8]=line->attrs[14]=255;}
static void ui_scroll_area_begin(int top,int bottom,int cat){body_bottom_seen=bottom;assert(bottom<test_h-1-(touch_only?reserved:0));}
static void ui_scroll_area_set_indicator(int current,int maximum){assert(current>=0&&current<=maximum);}
static bool ui_scroll_area_add_cols(int a,int b,int c,int d,int e){return true;}
static void ui_scroll_area_set_keys(int a,int b,int c,int d){}
static void ui_scroll_area_set_horizontal_page_mode(bool b){}
static void ui_scroll_area_enable_horizontal_page_swipe(int a,int b){}
static void ui_scroll_area_clear(void){}
static void ui_menu_click_set_touch_exit_button(bool b){assert(b);exit_registered=b;}
static void log_history_draw_filters(int a,int b,int w){}
static cptr log_history_filter_label(int f){return "All";}
static cptr format(cptr f,...){static char b[256];va_list a;va_start(a,f);vsnprintf(b,sizeof(b),f,a);va_end(a);return b;}
static void prt(cptr s,int y,int x){if(strstr(s,"Swipe")||strstr(s,"Tap a tab")||strstr(s,"e/i filter")||!strcmp(s,"Esc")){prompt_seen=y;assert(y==test_h-1-(touch_only?reserved:0));}}
static void ui_menu_click_add_text_token(int a,int x,int y,cptr p,cptr token){assert(y==test_h-1-(touch_only?reserved:0));}
static int steamdeck_prev_page_key(void){return 0;}
static int steamdeck_next_page_key(void){return 0;}
static void controller_prompt_label(int k,cptr f,char*b,size_t n){strnfmt(b,n,"%s",f);}
static void terminal_prompt_pick_variant(char*b,size_t n,int w,bool v,const char*const*p,size_t count){for(size_t i=0;i<count;i++)if(strlen(p[i])<=w){strnfmt(b,n,"%s",p[i]);return;}}
static void Term_set_cursor(bool b){}
static int log_history_click_to_filter(int c){return -c-20001;}
static int log_history_previous_filter(int f){return (f+3)%4;}
static int log_history_next_filter(int f){return (f+1)%4;}
static bool log_history_filter_key(char ch){return ch=='i';}
static bool get_string_panel(cptr title,char*b,size_t n){return false;}
static void bell(cptr t){}
static void queue(int key,int click){events[event_count]=key;clicks[event_count]=click;actions[event_count++]=UI_MENU_CLICK_PRIMARY;}
'''
filter_type=SOURCE[SOURCE.index('typedef struct log_history_filter_control {'):SOURCE.index('static int log_history_filter_layout(')]
combat_line_type=SOURCE[SOURCE.index('typedef struct log_history_combat_line'):SOURCE.index('static void log_history_build_combat_line(')]
parts=[filter_type,function('static int log_history_filter_layout(')]+[function(x) for x in ['static void log_history_draw_combat_span(', 'static void log_history_wrapped_entry_text(', 'static bool log_history_wrap_next(',
 'static int log_history_wrapped_entry_rows(', 'static void log_history_draw_wrapped_text(',
 'static void log_history_draw_wrapped_combat(',
 'static int log_history_wrapped_last_page_start(', 'static void log_history_draw_wrapped_slice(',
 'static void log_history_layout(', 'void do_cmd_messages_with_filter(']]
main=r'''
int main(void){
 const int sizes[][3]={{30,8,3},{30,9,3},{30,10,3},{30,12,3},{30,30,3},{77,17,3},{80,24,0}};
 for(size_t size=0;size<N_ELEMENTS(sizes);size++)for(int filter=0;filter<4;filter++){
  test_w=sizes[size][0];test_h=sizes[size][1];reserved=sizes[size][2];reached[0]=0;event_count=event_pos=0;icon_count=0;
  int top,bottom,prompt;log_history_layout(test_h,&top,&bottom,&prompt);assert(top<=bottom&&bottom<prompt&&prompt<test_h-reserved);
  log_history_filter_control controls[4];int last=log_history_filter_layout(test_w,controls);
  assert(top>last+1);
  int padding=controls[0].row==0?0:2;
  for(int f=0;f<4;f++) {
   assert(controls[f].col+(int)strlen(controls[f].label)+padding<=sdl_menu_font_button_available_columns(controls[f].row,test_w));
   if(f && controls[f].row==controls[f-1].row)
    assert(controls[f].col>controls[f-1].col+(int)strlen(controls[f-1].label)+padding);
  }
  for(int i=0;i<40;i++)queue(filter==3?'2':'8',-999);
  queue(filter==3?'1':'7',-999);queue(0,ESCAPE);
  measuring_menu=true;do_cmd_messages_with_filter(filter);measuring_menu=false;
  assert(strstr(reached,"FIRST")&&strstr(reached,"LASTTAIL")&&strstr(reached,"COMBATTAIL")&&strstr(reached,"OLDESTTAIL"));
  assert(prompt_seen==prompt&&!restores&&!scale_depth&&!pane_depth&&!saves&&!launches);
  if(filter!=3 && test_w==30)assert(icon_count>=2);
 }
 /* Regular-size touch menus reserve the same footer and must still allow
  * scrolling through oversized entries; desktop menus do not reserve it. */
 bigger=false;
 cptr long_text[3]={text[0],text[1],text[2]};
 for(int touch=0;touch<2;touch++)for(int filter=0;filter<4;filter++){
  text[0]=touch?long_text[0]:"FIRST LASTTAIL";
  text[1]=touch?long_text[1]:"Combat ?? to ?? COMBATTAIL";
  text[2]=touch?long_text[2]:"Oldest OLDESTTAIL";
  touch_only=touch;test_w=30;test_h=12;reserved=3;reached[0]=0;event_count=event_pos=0;
  int top,bottom,prompt;log_history_layout(test_h,&top,&bottom,&prompt);
  assert(top<=bottom&&bottom<prompt&&prompt==test_h-1-(touch?reserved:0));
  for(int i=0;i<40;i++)queue(filter==3?'2':'8',-999);
  queue(filter==3?'1':'7',-999);queue(0,ESCAPE);
  measuring_menu=true;do_cmd_messages_with_filter(filter);measuring_menu=false;
  assert(strstr(reached,"FIRST")&&strstr(reached,"LASTTAIL")&&strstr(reached,"COMBATTAIL")&&strstr(reached,"OLDESTTAIL"));
  assert(prompt_seen==prompt&&!restores&&!scale_depth&&!pane_depth);
 }
 puts("Actual Log/Combat/Notes caller: populated wrapping, oversized tails, reserved hint/body and Exit passed");
}
'''
msys=Path(r'C:\msys64\mingw64\bin');cc=os.environ.get('CC') or shutil.which('gcc') or str(msys/'gcc.exe')
env=os.environ.copy();env['PATH']=str(msys)+os.pathsep+str(msys.parent/'usr/bin')+os.pathsep+env.get('PATH','')
with tempfile.TemporaryDirectory(prefix='sil-log-footer-') as tmp:
 src=Path(tmp)/'test.c';exe=Path(tmp)/'check.exe';src.write_text(harness+combat_line_type+fixtures+'\n'.join(parts)+main)
 subprocess.run([cc,'-std=c17',str(src),'-o',str(exe)],check=True,env=env)
 subprocess.run([str(exe)],check=True,env=env)

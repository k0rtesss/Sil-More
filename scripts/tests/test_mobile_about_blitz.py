"""Exercise actual mobile About/Blitz callers, fields and input dispatch in C.

Native rendering is checked separately by check_phone_about_blitz_layout.py.
This isolated test never opens a save, SDL window, emulator or shared build.
"""
from pathlib import Path
import os,re,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
M=(ROOT/'src/cmd/ui/cmd-ui-main-menu.c').read_text()
B=(ROOT/'src/birth/birth-blitz.c').read_text()
def function(source,marker):
 start=source.rindex(marker);end=source.index('{',start)+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
types=re.search(r'typedef struct main_menu_about_line.*?main_menu_about_line;',M,re.S).group()
types+=re.search(r'typedef struct blitz_setup.*?blitz_setup;', (ROOT/'src/blitz.h').read_text(),re.S).group()
types+=re.search(r'typedef struct ui_question_option.*?ui_question_option;', (ROOT/'src/ui/question.h').read_text(),re.S).group()
credits=re.search(r'static const main_menu_about_line about_lines\[\] = \{.*?\n    \};',M,re.S).group()
parts=[function(B,x) for x in ['static cptr blitz_character_mode_name(',
 'static cptr blitz_effect_mode_name(', 'static void blitz_setup_clamp(',
 'static void blitz_setup_draw_mobile(', 'static NavResult blitz_setup_menu(']]
parts += [function(M,x) for x in ['static void main_menu_about_mobile_build(', 'static bool main_menu_mobile_book_wait(', 'static void main_menu_about_mobile(',
 'static void main_menu_blitz_intro_mobile_build(',
 'static bool main_menu_blitz_confirm_mobile(', 'static void do_cmd_start_blitz(']]
harness=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
typedef const char*cptr;typedef unsigned char byte;typedef int NavResult;
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define N_ELEMENTS(a) (sizeof(a)/sizeof((a)[0]))
#define TERM_WHITE 1
#define TERM_L_BLUE 2
#define TERM_L_RED 3
#define TERM_YELLOW 4
#define TERM_SLATE 5
#define TERM_L_GREEN 6
#define TERM_L_WHITE 7
#define UI_QUESTION_GLOBAL (-1)
#define BLITZ_CHARACTER_RANDOM 0
#define BLITZ_CHARACTER_RANDOM_STATS 1
#define BLITZ_CHARACTER_SELECTED 2
#define BLITZ_EFFECT_RANDOM 0
#define BLITZ_EFFECT_SELECTED 1
#define BLITZ_EFFECT_SELECTED_DESCR 2
#define BLITZ_MAX_EFFECT_COUNT 9
#define NAV_TO_MAIN 0
#define NAV_OK 1
#define ESCAPE 27
#define UI_MENU_CLICK_PRIMARY 1
#define UI_MENU_CLICK_HOVER 2
#define UI_MENU_CLICK_WAKE_KEY 30
#define SDL_SELECT_CLICK_CLOSE (-24)
#define SDL_SELECT_CLICK_PAGE_NEXT (-21)
#define SDL_SELECT_CLICK_PAGE_PREV (-20)
static bool hide_cursor,save_game_quietly;
static struct {bool playing,is_dead,quit_to_menu,leaving;} player,*p_ptr=&player;
static int saves,launches,restores,hidden,scale_depth,pane_depth,book_count,page,page_count=3,question_result;
static char book[32][256],rows[5][160],confirm[40],description[2048];static byte colors[32];
static int selected_seen,row_count,events[128],clicks[128],actions[128],event_count,event_pos,last;
static int strnfmt(char*s,size_t n,cptr f,...){va_list a;va_start(a,f);int r=vsnprintf(s,n,f,a);va_end(a);return r;}
static bool steamdeck_controls_active(void){return false;}
static int steamdeck_confirm_key(void){return 13;}
static int steamdeck_back_key(void){return ESCAPE;}
static int steamdeck_menu_key(int k,int l,int r){return k;}
static bool sdl_touch_only_device_active(void){return true;}
static bool get_sdl_menu_bigger_font(void){return true;}
static void screen_save(void){restores++;}
static void screen_load(void){restores--;}
static void screen_push_supporting_panes_hidden(void){pane_depth++;}
static void screen_pop_supporting_panes_hidden(void){pane_depth--;}
static void screen_push_touch_pane_hidden(void){pane_depth++;}
static void screen_pop_touch_pane_hidden(void){pane_depth--;}
enum {SDL_MENU_FONT_BIRTH,SDL_MENU_FONT_HELP};
static void sdl_push_terminal_menu_scale_for(int menu){scale_depth++;}
static void sdl_pop_terminal_menu_scale(void){scale_depth--;}
static void ui_menu_click_begin(void){}
static void ui_menu_click_clear(void){}
static void ui_menu_click_set_hover_enabled(bool b){}
static int inkey(void){assert(event_pos<event_count);last=event_pos++;return events[last];}
static bool ui_menu_click_take_action(int*c,int*a){if(clicks[last]==-999)return false;*c=clicks[last];*a=actions[last];return true;}
static void sdl_character_sheet_screen_hide(void){hidden++;}
static void sdl_character_sheet_screen_begin_book(cptr title){assert(!strcmp(title,"About Sil-More")||!strcmp(title,"Blitz Mode"));book_count=0;page=0;}
static void sdl_character_sheet_screen_set_book_close_button(bool b){assert(b);}
static void sdl_character_sheet_screen_set_book_close_label(cptr label){assert(!strcmp(label,"Back")||!strcmp(label,"Continue"));}
static void sdl_character_sheet_screen_add_book_paragraph_colored(cptr text,int attr){if(!text[0])return;strnfmt(book[book_count],256,"%s",text);colors[book_count++]=attr;}
static void sdl_character_sheet_screen_commit_book(void){}
static bool sdl_character_sheet_screen_page_turning(void){return false;}
static int sdl_character_sheet_screen_select_page(void){return page;}
static int sdl_character_sheet_screen_select_page_count(void){return page_count;}
static void sdl_character_sheet_screen_begin_page_turn(int d){page+=d;assert(page>=0&&page<page_count);}
static bool sdl_character_sheet_screen_scroll_book(int d){return true;}
static void sdl_character_sheet_screen_begin_select(int selected,cptr title){assert(!strcmp(title,"Blitz Setup"));selected_seen=selected;row_count=0;}
static void sdl_character_sheet_screen_set_select_menu_style(bool b){assert(b);}
static void sdl_character_sheet_screen_set_select_confirm_label(cptr label){strnfmt(confirm,40,"%s",label);}
static void sdl_character_sheet_screen_add_select_heading(cptr intro){assert(strstr(intro,"Left/right")&&strstr(intro,"Begin"));}
static void sdl_character_sheet_screen_add_select_row(int choice,cptr label,int attr,cptr desc){assert(choice==row_count);strnfmt(rows[row_count++],160,"%s",label);}
static bool sdl_character_sheet_screen_commit_select(int i){assert(i==selected_seen&&row_count==5);return true;}
static void Term_clear(void){}
static void Term_fresh(void){}
static void Term_get_size(int*w,int*h){*w=30;*h=12;}
static void c_put_str(int a,cptr t,int y,int x){}
static int count_wrapped_lines(cptr t,int w,int i){return 1;}
static void main_menu_blitz_text_line(int*r,int w,byte a,cptr t){}
static void text_out_to_screen(byte a,cptr t){}
static void(*text_out_hook)(byte,cptr);static int text_out_wrap,text_out_indent;
static bool run_mode_is_blitz(void){return false;}
static bool death_spectator_active(void){return false;}
static void msg_print(cptr t){}
static bool get_check_lower(cptr t){assert(false);return false;}
static void do_cmd_save_game(void){assert(player.playing);saves++;}
static void blitz_request_launch(void){assert(saves);launches++;}
static int Term_flush(void){return 0;}
'''
glue=r'''
static blitz_setup setup;
static blitz_setup*blitz_current_setup_mutable(void){return &setup;}
static void blitz_setup_draw_mobile(const blitz_setup*s,int selected);
static void blitz_setup_draw(const blitz_setup*s,int selected){blitz_setup_draw_mobile(s,selected);}
static int ui_question_ask_overlay(cptr title,cptr desc,const ui_question_option*o,int n,int y,int x,int def){
 assert(!strcmp(title,"Switch to Blitz?"));assert(n==2&&o[0].key=='y'&&o[1].key=='n'&&def==1);
 assert(y==UI_QUESTION_GLOBAL&&x==UI_QUESTION_GLOBAL);strnfmt(description,sizeof(description),"%s",desc);return question_result;
}
static void queue(int key,int click,int action){events[event_count]=key;clicks[event_count]=click;actions[event_count++]=action;}
static void reset(void){event_count=event_pos=0;}
'''
main=r'''
int main(void){
 reset();queue('6',-999,1);queue('6',-999,1);queue(0,SDL_SELECT_CLICK_CLOSE,1);
 main_menu_about_mobile(about_lines);assert(page==2&&book_count==16);
 assert(strstr(book[0],"evolution of SilQ"));assert(strstr(book[15],"Tolkien and his timeless creations."));assert(colors[15]==TERM_L_RED);
 int source=0;for(int i=0;about_lines[i].text;i++)if(about_lines[i].text[0])assert(!strcmp(book[source++],about_lines[i].text));
 assert(source==book_count&&!hide_cursor);
 /* First touch selects a different field; repeated touch cycles. No input
  * can start a run except the explicit Begin control or confirm key. */
 setup=(blitz_setup){0};reset();queue(0,1,1);queue(0,1,1);queue(0,4,1);queue(0,4,1);queue(0,-2,1);
 assert(blitz_setup_menu()==NAV_OK&&setup.oaths_enabled&&setup.effect_mode==1);
 assert(!strcmp(confirm,"Begin")&&strstr(rows[4],"Selected"));assert(!restores&&!pane_depth&&!scale_depth);
 setup=(blitz_setup){.blessing_count=9,.curse_count=9,.character_mode=2,.effect_mode=2};
 reset();queue('6',-999,1);queue('4',-999,1);queue('2',-999,1);queue('2',-999,1);queue('6',-999,1);queue('2',-999,1);queue('4',-999,1);queue(0,-1,1);
 assert(blitz_setup_menu()==NAV_TO_MAIN);assert(setup.character_mode==2&&setup.blessing_count==9&&setup.curse_count==9);
 setup=(blitz_setup){.blessing_count=250,.curse_count=0,.character_mode=250,.effect_mode=250};blitz_setup_clamp(&setup);
 assert(setup.blessing_count==9&&setup.curse_count==9&&setup.character_mode==0&&setup.effect_mode==0);
 /* Actual command caller owns saving and flags. No/cancel must never save. */
 for(int answer=-1;answer<=1;answer++){
  player=(typeof(player)){.playing=true};question_result=answer;saves=launches=0;
  reset();queue(0,SDL_SELECT_CLICK_CLOSE,1);do_cmd_start_blitz();assert(saves==(answer==0)&&launches==(answer==0));
  assert(player.playing!=(answer==0));assert(player.quit_to_menu==(answer==0)&&player.leaving==(answer==0));
  assert(strstr(description,"switch to Blitz now?"));assert(book_count==5&&strstr(book[1],"normal tale, saves or score")&&strstr(book[3],"living Blitz character")&&strstr(book[4],"save your current tale game first"));
  assert(!restores&&!pane_depth&&!scale_depth);
 }
 memset(&player,0,sizeof(player));player.playing=true;saves=launches=0;question_result=0;
 reset();queue(ESCAPE,-999,1);do_cmd_start_blitz();assert(!saves&&!launches&&player.playing);
 puts("Actual mobile About credits/Back, Blitz fields/touch/cycle/limits/Begin, save/cancel passed");
}
'''.replace('player=(typeof(player)){.playing=true};','memset(&player,0,sizeof(player));player.playing=true;')
msys=Path(r'C:\msys64\mingw64\bin');cc=os.environ.get('CC') or shutil.which('gcc') or str(msys/'gcc.exe')
env=os.environ.copy()
if msys.exists():env['PATH']=str(msys)+os.pathsep+str(msys.parent/'usr/bin')+os.pathsep+env.get('PATH','')
with tempfile.TemporaryDirectory(prefix='sil-about-blitz-') as directory:
 directory=Path(directory);source=directory/'test.c';exe=directory/'check.exe'
 source.write_text(harness+types+credits+glue+'\n'.join(parts)+main)
 subprocess.run([cc,'-std=c17',str(source),'-o',str(exe)],check=True,env=env)
 subprocess.run([str(exe)],check=True,env=env)

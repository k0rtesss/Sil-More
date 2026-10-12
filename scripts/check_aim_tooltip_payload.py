#!/usr/bin/env python3
"""Check actual tooltip formatting retains combat facts and ordinary context."""
from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'scripts/output/aim-tooltip-payload'
def function(s,mark):
 a=s.index(mark);b=s.index('{',a);n=1;e=b+1
 while n:n+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
def main():
 s=(ROOT/'src/sdl/input/sdl-tooltips.c').read_text();parts='\n'.join(function(s,m)for m in ('static void sdl_object_tooltip_append_text','void sdl_object_tooltip_append_part','bool sdl_object_tooltip_format_grid'))
 if '--baseline' in sys.argv:parts=parts.replace('if (aim_selection)', 'if (false)')
 if '--detailed-floor-negative' in sys.argv:parts=parts.replace('aim_selection ? 0 : 3','3')
 code=r'''
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
typedef const char*cptr;typedef unsigned char byte;
#define TERM_WHITE 1
#define TERM_YELLOW 2
#define TERM_L_BLUE 3
#define RF2_MINDLESS 1
#define ALERTNESS_ALERT 10
#define CAVE_SEEN 1
#define CAVE_ATMOSPHERE_NONE 0
typedef int cave_atmosphere_kind;
typedef struct{int k_idx;}object_type;
typedef struct{int alertness,r_idx,maxhp,hp;}monster_type;
static monster_type mon_list[2]={{0},{20,1,10,7}};static struct{int flags2;}r_info[2];static bool styled_monster_health_bars;
static struct{char text[2048];}g_object_tooltip;static struct{bool active,select_mode,select_location;}g_pointer_aim;
static struct{int py,px;}player={1,1},*p_ptr=&player;static byte cave_info[4][4]={{0},{0,0,1}};static object_type objects[3]={{1},{1},{1}};static int object_count=1;static bool gauntlets;static bool visible=true,seen=true;
static size_t SDL_strlcpy(char*d,cptr s,size_t n){size_t len=strlen(s);if(n){size_t k=len<n-1?len:n-1;memcpy(d,s,k);d[k]=0;}return len;}
static size_t SDL_strlcat(char*d,cptr s,size_t n){size_t k=strlen(d);return k+SDL_strlcpy(d+k,s,n>k?n-k:0);}
static void strnfmt(char*d,size_t n,cptr f,...){va_list a;va_start(a,f);vsnprintf(d,n,f,a);va_end(a);}
static bool in_bounds(int y,int x){return y>=0&&y<4&&x>=0&&x<4;}
static bool grid_info_is_available(int y,int x){(void)y;(void)x;return seen;}
static bool sdl_mouse_grid_has_visible_monster(int y,int x,int*i){(void)y;(void)x;*i=1;return visible;}
static void monster_desc(char*b,size_t n,monster_type*m,int flags){(void)m;(void)flags;SDL_strlcpy(b,"a Wolf",n);}
static void sdl_object_tooltip_monster_morale_text(monster_type*m,char*b,size_t n,byte*a){(void)m;*a=TERM_YELLOW;SDL_strlcpy(b,"Confident 8",n);}
static bool monster_health_bar_allowed(monster_type*m){(void)m;return true;}
static void monster_health_bar_text(monster_type*m,char*b,size_t n,int len){(void)m;(void)len;SDL_strlcpy(b,"******--",n);}
static byte health_attr(int h,int max){(void)h;(void)max;return TERM_YELLOW;}
static bool sdl_mouse_grid_can_show_objects(int y,int x){(void)y;(void)x;return true;}
static object_type*get_first_object(int y,int x){(void)y;(void)x;return object_count?objects:NULL;}
static object_type*get_next_object(object_type*o){return o+1<objects+object_count?o+1:NULL;}
static bool object_is_visible(object_type*o){return o->k_idx>0;}
static void object_desc_floor(char*b,size_t n,object_type*o,bool full,int mode){(void)o;(void)full;SDL_strlcpy(b,gauntlets?(mode?"2 Sets of Gauntlets of the Torturer (-1) [+0,1d3] <1> {cursed, QA}":"2 Sets of Gauntlets of the Torturer"):(mode?"an Arrow (+3)":"an Arrow"),n);}
static bool sdl_object_tooltip_feature_name(int y,int x,cptr*out){(void)y;(void)x;*out="dart trap";return true;}
static bool cave_environment_describe(int y,int x,char*b,size_t n){(void)y;(void)x;SDL_strlcpy(b,"Unstable: stone is crumbling",n);return true;}
static cave_atmosphere_kind cave_atmosphere_at(int y,int x){(void)y;(void)x;return 1;}
static cptr cave_atmosphere_name(int k){(void)k;return "Echoing";}
static cptr cave_atmosphere_description(int k){(void)k;return "The stone carries every footstep. Song +3; Stealth -3 while here.";}
__PRODUCTION__
int main(void){char ordinary[2048],aim[2048],again[2048];byte attrs[2048];
 cptr core="a Wolf [HP: ******-- Morale: Confident 8]; an Arrow; dart trap";
 cptr full="a Wolf [HP: ******-- Morale: Confident 8]; an Arrow (+3); dart trap; Unstable: stone is crumbling; Echoing; The stone carries every footstep. Song +3; Stealth -3 while here.";
 assert(sdl_object_tooltip_format_grid(1,2,ordinary,sizeof(ordinary),attrs)&&!strcmp(ordinary,full));
 g_pointer_aim.active=g_pointer_aim.select_mode=true;assert(sdl_object_tooltip_format_grid(1,2,aim,sizeof(aim),attrs)&&!strcmp(aim,core));
 assert(attrs[strstr(aim,"******--")-aim]==TERM_YELLOW);
 gauntlets=true;assert(sdl_object_tooltip_format_grid(1,2,aim,sizeof(aim),NULL));assert(strstr(aim,"2 Sets of Gauntlets of the Torturer")&&strstr(aim,"HP:")&&strstr(aim,"Morale:")&&strstr(aim,"dart trap"));assert(!strstr(aim,"(-1)")&&!strstr(aim,"cursed")&&!strstr(aim,"QA"));
 g_pointer_aim.select_location=true;assert(sdl_object_tooltip_format_grid(1,2,again,sizeof(again),NULL));assert(strstr(again,"(-1) [+0,1d3] <1> {cursed, QA}")&&strstr(again,"Echoing"));g_pointer_aim.select_location=false;g_pointer_aim.active=false;assert(sdl_object_tooltip_format_grid(1,2,again,sizeof(again),NULL));assert(strstr(again,"{cursed, QA}")&&strstr(again,"Echoing"));g_pointer_aim.active=true;gauntlets=false;

 object_count=3;assert(sdl_object_tooltip_format_grid(1,2,aim,sizeof(aim),NULL));assert(strstr(aim,"+1 more")&&strstr(aim,"dart trap")&&strstr(aim,"HP:"));object_count=1;
 mon_list[1].alertness=0;assert(sdl_object_tooltip_format_grid(1,2,aim,sizeof(aim),NULL)&&strstr(aim,"Awareness:"));mon_list[1].alertness=20;r_info[1].flags2=RF2_MINDLESS;assert(sdl_object_tooltip_format_grid(1,2,aim,sizeof(aim),NULL)&&strstr(aim,"State:"));r_info[1].flags2=0;
 g_pointer_aim.select_location=true;assert(sdl_object_tooltip_format_grid(1,2,again,sizeof(again),NULL)&&!strcmp(again,ordinary));g_pointer_aim.select_location=false;g_pointer_aim.active=false;assert(sdl_object_tooltip_format_grid(1,2,again,sizeof(again),NULL)&&!strcmp(again,ordinary));
 seen=false;assert(!sdl_object_tooltip_format_grid(1,2,again,sizeof(again),NULL));
 puts("Aim tooltip combat facts, HP colors/status/more objects/terrain retained; ordinary and location context unchanged PASS");}
'''.replace('__PRODUCTION__',parts)
 OUT.mkdir(parents=True,exist_ok=True);c=OUT/'check.c';c.write_text(code);env=os.environ.copy();env['PATH']=os.pathsep.join(['C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']]);exe=OUT/'check.exe';subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-std=c17','-Wall','-Wextra',str(c),'-o',str(exe)],env=env,check=True);subprocess.run([str(exe)],env=env,check=True,timeout=30)
if __name__=='__main__':main()

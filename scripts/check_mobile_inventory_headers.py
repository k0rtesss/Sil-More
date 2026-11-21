"""Check mobile browser headings with the production wrapping/layout code.

This compiles the header and summary functions against a small terminal
fixture; it does not use a shared build, device, or player save.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/cmd/ui/cmd-ui-knowledge.c").read_text(encoding="utf-8")


def function(marker):
    start = SOURCE.rindex(marker)
    opening = SOURCE.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (SOURCE[end] == "{") - (SOURCE[end] == "}")
        end += 1
    return SOURCE[start:end]


markers = [
    "static int equipment_entry_wrap_take(",
    "static bool equipment_entry_wrap_next(",
    "static void supply_put_fitted(",
    "static int supply_put_wrapped(",
    "static cptr supply_browser_page_text(",
    "static supply_menu_page supply_browser_turn_page(",
    "static int supply_browser_page_tab_col(",
    "static int supply_browser_page_token_width(",
    "static byte supply_browser_page_tab_attr(",
    "static bool supply_page_header_uses_wrapped_title(",
    "static bool supply_page_tabs_need_paging(",
    "static int supply_page_summary_row(",
    "static bool supply_page_summary_fits(",
    "static void supply_draw_page_summary(",
    "static void supply_draw_page_header(",
    "static int supply_browser_page_click_choice(",
    "static void supply_register_page_tabs(",
]
production = "\n".join(function(marker) for marker in markers)

HARNESS = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef const char* cptr;
typedef unsigned char byte;
typedef enum { SUPPLY_MENU_PAGE_EQUIPPED, SUPPLY_MENU_PAGE_INVENTORY,
    SUPPLY_MENU_PAGE_JEWELRY, SUPPLY_MENU_PAGE_SUPPLIES } supply_menu_page;
typedef struct {int title_row,tabs_row,header_row,term_wid;} knowledge_browser_layout;
static struct {int wid,hgt;} state={77,24},*Term=&state;
static bool handheld,touch_only,bigger,portrait;
#define TERM_L_BLUE 14
#define TERM_SLATE 2
#define TERM_L_DARK 8
#define TERM_L_WHITE 9
#define TERM_SHADE 16
#define SUPPLY_CLICK_PAGE_EQUIPPED 201
#define SUPPLY_CLICK_PAGE_INVENTORY 202
#define SUPPLY_CLICK_PAGE_JEWELRY 203
#define SUPPLY_CLICK_PAGE_SUPPLIES 204
#define SDL_memcpy memcpy
#define streq(a,b) (strcmp((a),(b))==0)
static char screen[24][160];
static int click_choice[10],click_col[10],click_width[10],click_count;
static bool sdl_handheld_ui_active(void){return handheld;}
static bool sdl_touch_only_device_active(void){return touch_only;}
static bool get_sdl_menu_bigger_font(void){return bigger;}
static bool sdl_mobile_portrait_layout_active(void){return portrait;}
static int utf8_sequence_len_n(cptr s,int n){return n&&*s?1:0;}
static int utf8_display_width_n(cptr s,int n){return n;}
static int utf8_safe_prefix_len(cptr s,int n){return n;}
static void strnfmt(char* s,size_t n,cptr fmt,...){va_list a;va_start(a,fmt);vsnprintf(s,n,fmt,a);va_end(a);}
static void settings_ui_fit_text(char* out,size_t size,cptr text,int width){
 int n=(int)strlen(text); if(n<=width){snprintf(out,size,"%s",text);return;}
 if(width<4){snprintf(out,size,"%.*s",width,"...");return;}
 snprintf(out,size,"%.*s...",width-3,text);
}
static void Term_erase(int col,int row,int n){
 assert(row>=0&&row<Term->hgt);if(col<0)col=0;
 for(int i=col;i<Term->wid&&i<col+n;i++)screen[row][i]=' ';
}
static void Term_putstr(int col,int row,int n,byte attr,cptr text){
 (void)attr;int len=(int)strlen(text);if(n>=0&&len>n)len=n;
 assert(col>=0&&row>=0&&row<Term->hgt);
 for(int i=0;i<len&&col+i<Term->wid;i++)screen[row][col+i]=text[i];
}
static void ui_menu_click_add(int choice,int col,int row,int width){
 assert(row>=0&&col>=0&&col+width<=Term->wid&&click_count<10);
 click_choice[click_count]=choice;click_col[click_count]=col;
 click_width[click_count++]=width;
}
"""

MAIN = r"""
static void clear(void){memset(screen,' ',sizeof(screen));
 for(int i=0;i<24;i++)screen[i][Term->wid]='\0';click_count=0;}
int main(void){
 cptr title="Supplies - Supply, Herbs, Food, Potions, Gems, Lights/Oil";
 cptr inventory="Pack, Harness, Quiver, and Jewelry Pouch items, equipment, and floor finds";
 cptr equipped="All equipped shows active gear; slots include available items";
 cptr jewelry="Browse jewelry and save or name equipment sets";
 const int widths[]={25,30,44,55,77,120};
 for(int controller=0;controller<2;controller++)
 for(int big=0;big<2;big++)for(int p=0;p<2;p++)
 for(unsigned n=0;n<sizeof(widths)/sizeof(widths[0]);n++){
  handheld=true;touch_only=!controller;bigger=big;portrait=p;Term->wid=widths[n];
  assert(sdl_handheld_ui_active());
  assert(sdl_touch_only_device_active()==!controller);
  knowledge_browser_layout l={.title_row=0,.tabs_row=1,
   .header_row=p?4:2,.term_wid=Term->wid};
  clear();supply_draw_page_header(&l,SUPPLY_MENU_PAGE_SUPPLIES,-1,title);
  supply_register_page_tabs(&l,SUPPLY_MENU_PAGE_SUPPLIES);
  assert(!strstr(screen[0],"..."));
  assert(strstr(screen[0],"Supplies"));
  assert(!strstr(screen[0],"Supply, Herbs"));
  if(supply_page_tabs_need_paging(&l)){
   assert(strstr(screen[0],"< Supplies >")&&click_count==2);
   assert(click_choice[0]==SUPPLY_CLICK_PAGE_JEWELRY);
   assert(click_choice[1]==SUPPLY_CLICK_PAGE_EQUIPPED);
  }else assert(click_count==4);
  for(int k=0;k<3;k++){
   clear();supply_draw_page_summary(&l,TERM_SLATE,k==0?inventory:k==1?equipped:jewelry);
   int summary_row=supply_page_summary_row(&l);
   for(int row=summary_row;row<l.header_row;row++)assert(!strstr(screen[row],"..."));
   if(k==0&&Term->wid>=30){
    assert(strstr(screen[summary_row],"Pack"));
    assert(strstr(screen[summary_row],"Harness"));
    assert(strstr(screen[summary_row],"Quiver"));
    assert(strstr(screen[summary_row],"Jewelry"));
   }
  }
 }
 /* Desktop retains the original title and summary, even if they do not fit. */
 handheld=false;touch_only=false;bigger=false;portrait=false;Term->wid=77;
 knowledge_browser_layout l={.title_row=0,.tabs_row=1,.header_row=2,.term_wid=77};
 clear();supply_draw_page_header(&l,SUPPLY_MENU_PAGE_SUPPLIES,-1,title);
 assert(strstr(screen[0],"Supplies - Supply, Herbs")&&strstr(screen[0],"..."));
 clear();supply_draw_page_summary(&l,TERM_SLATE,inventory);
 assert(strstr(screen[1],inventory)&&!strstr(screen[1],"..."));
 Term->wid=l.term_wid=60;clear();
 supply_draw_page_summary(&l,TERM_SLATE,inventory);
 assert(strstr(screen[1],"Jewelry Pouch items")&&strstr(screen[1],"..."));
 puts("Mobile browser headers fit, category names and tab navigation preserved PASS");
}
"""

msys = Path("C:/msys64/mingw64/bin")
cc = os.environ.get("CC") or shutil.which("gcc") or str(msys / "gcc.exe")
env = os.environ.copy()
if msys.exists():
    env["PATH"] = os.pathsep.join((str(msys), str(msys.parent / "usr/bin"), env.get("PATH", "")))
with tempfile.TemporaryDirectory(prefix="sil-mobile-headers-") as directory:
    source = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    source.write_text(HARNESS + production + MAIN, encoding="utf-8")
    subprocess.run([cc, "-std=c17", str(source), "-o", str(exe)], check=True, env=env)
    subprocess.run([str(exe)], check=True, env=env)

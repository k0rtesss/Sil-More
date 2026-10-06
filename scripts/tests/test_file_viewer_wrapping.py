"""Compile the real legacy viewer with mocked Term/input and test narrow pages.

Run with Python; uses gcc on PATH or the repository's MSYS2 toolchain.
No game build, save data, SDL display, or Android device is needed.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
viewer = (ROOT / "src/ui/file-viewer.c").read_text()
utf8 = (ROOT / "src/support/utf8.c").read_text()
utf8 = "\n".join(line for line in utf8.splitlines() if not line.startswith("#include"))
helpers = viewer[viewer.index("static void string_lower"):viewer.index("static void file_viewer_prompt_label")]
functions = viewer[viewer.index("bool show_buffer("):]
harness = r"""
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef const char *cptr;
typedef unsigned char byte;
typedef uint32_t u32b;
typedef FILE SDL_IOStream;
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define N_ELEMENTS(a) (sizeof(a)/sizeof((a)[0]))
#define TERM_WHITE 1
#define TERM_YELLOW 2
#define TERM_SLATE 3
#define SDL_TOUCH_MENU_CATEGORY_OTHER 0
#define ESCAPE 27
#define I2D(x) ((x)+'0')
#define A2I(x) ((x)-'a')
#define log_debug(...) ((void)0)
#define log_warn(...) ((void)0)
#define SDL_strlcpy(d,s,n) snprintf(d,n,"%s",s)
#define sdl_fopen fopen
#define sdl_fclose fclose
#define streq(a,b) (!strcmp(a,b))
#define prefix(a,b) (!strncmp(a,b,strlen(b)))
static int width=12,height=8,frame=-1,errors=0;
static char screens[1024][32][128];
static byte colors[1024][32][128];
static const char *keys,*answer="";
static void Term_get_size(int *w,int *h) {*w=width;*h=height;}
static void Term_clear(void) {assert(++frame<1024);}
static void Term_putstr(int x,int y,int n,int attr,cptr text) {
    assert(y>=0 && y<height && x>=0 && x<width);
    if(n<0) n=(int)strlen(text);
    assert(n<=width-x);
    for(int j=0;j<n && text[j];j++) {
        screens[frame][y][x+j]=text[j]; colors[frame][y][x+j]=(byte)attr;
    }
}
static void file_viewer_draw_prompt(int r,int w,bool large) {(void)r;(void)w;(void)large;}
static void terminal_prompt_put_variant(int x,int y,int w,int a,bool b,cptr *v,int n) {}
static void ui_scroll_area_begin(int a,int b,int c) {}
static void ui_scroll_area_set_keys(int a,int b,int c,int d) {}
static void ui_scroll_area_set_tap_key(int a) {}
static void ui_scroll_area_clear(void) {}
static int inkey(void) {assert(*keys);return (unsigned char)*keys++;}
static int steamdeck_menu_key(int k,int a,int b) {return k;}
static bool steamdeck_controls_active(void) {return false;}
static int target_dir(int k) {return k>='1' && k<='9' ? k-'0' : 0;}
static void bell(cptr s) {++errors;}
static void msg_format(cptr f,cptr s) {}
static void message_flush(void) {}
static bool get_string_panel(cptr p,char *out,int n) {snprintf(out,n,"%s",answer);return true;}
static int sdl_fgets(FILE *f,char *out,size_t n) {
    if(!fgets(out,(int)n,f)) return 1;
    out[strcspn(out,"\r\n")]=0; return 0;
}
static void reset(cptr input,int w,int h) {
    memset(screens,0,sizeof(screens));memset(colors,0,sizeof(colors));
    frame=-1;errors=0;keys=input;width=w;height=h;
}
"""
main = r"""
static void write_file(cptr content) {
    FILE *f=fopen("viewer.txt","wb");assert(f);fputs(content,f);fclose(f);
}
int main(void) {
    /* A single source line spans several pages; every byte remains reachable. */
    char long_text[2401],input[1024];
    memset(long_text,'x',2400);long_text[2399]='Z';long_text[2400]=0;
    memset(input,'3',199);input[199]=ESCAPE;input[200]=0;
    reset(input,12,6);assert(show_buffer(long_text,0));
    assert(frame==199 && screens[199][2][11]=='Z');
    for(int f=0;f<200;f++) assert(strlen(screens[f][2])==12);

    /* Word wraps preserve indentation/spaces, blank lines and Unicode bytes. */
    cptr text="  alpha beta gamma \xc3\xa9\xe2\x80\x99tail";
    char restored[256]={0};int pos=0,length=(int)strlen(text),rows=0;
    while(pos<length) {
        int n=file_viewer_wrap_length(text+pos,length-pos,12);
        assert(n>0 && n<=12 && utf8_safe_prefix_len(text+pos,n)==n);
        memcpy(restored+pos,text+pos,n);pos+=n;rows++;
    }
    assert(!strcmp(text,restored));
    assert(file_viewer_wrapped_rows(text,length,12)==rows);
    reset("3\033",12,6);show_buffer("\nlast",0);
    assert(!screens[0][2][0] && !strcmp(screens[1][2],"last"));

    /* File tails and a match beyond the old visible width are reachable. */
    write_file("abcdefghijklmnopqrstuvwxTAIL\n");
    reset("33\033",12,6);show_file("viewer.txt","Test",0);
    assert(!strcmp(screens[2][2],"TAIL"));
    answer="TAIL";reset("/\033",12,6);show_file("viewer.txt","Test",0);
    assert(!errors && !strcmp(screens[1][2],"TAIL"));
    assert(colors[1][2][0]==TERM_YELLOW);

    /* Highlighting spans both sides of a wrap; tags still address source lines. */
    write_file("abcdefghijk\n");answer="defgh";
    reset("&\033",4,8);show_file("viewer.txt","Test",0);
    assert(colors[1][2][3]==TERM_YELLOW);
    assert(colors[1][3][0]==TERM_YELLOW && colors[1][3][3]==TERM_YELLOW && colors[1][4][0]==TERM_WHITE);
    write_file("abcdefghijklmnopqrstuvwx\n***** <tail>\nTAIL\n");
    reset("\033",12,6);show_file("viewer.txt#tail","Test",0);
    assert(!strcmp(screens[0][2],"TAIL"));
    reset("\033",12,6);show_file("viewer.txt","Test",1);
    assert(!strcmp(screens[0][2],"TAIL"));
    answer="1";reset("#\033",12,6);show_file("viewer.txt","Test",0);
    assert(!strcmp(screens[1][2],"TAIL"));
    /* Searching again does not rediscover a match on the preceding wrap. */
    write_file("TAILabcdefghijklmnopTAIL\n");answer="TAIL";
    reset("//\033",4,6);show_file("viewer.txt","Test",0);
    assert(errors==1 && !strcmp(screens[1][2],"TAIL"));
    remove("viewer.txt");puts("file viewer narrow-width regression checks passed");
    return 0;
}
"""
cc = os.environ.get("CC") or shutil.which("gcc")
msys = Path(r"C:\msys64\mingw64\bin")
if not cc and (msys / "gcc.exe").exists():
    cc = str(msys / "gcc.exe")
if not cc:
    raise SystemExit("gcc is required (set CC or install the repository's MSYS2 prerequisites)")
env = os.environ.copy()
if msys.exists():
    env["PATH"] = str(msys) + os.pathsep + str(msys.parent / "usr/bin") + os.pathsep + env.get("PATH", "")
with tempfile.TemporaryDirectory(prefix="sil-file-viewer-test-") as folder:
    folder = Path(folder)
    source = folder / "test.c"
    executable = folder / ("test.exe" if os.name == "nt" else "test")
    source.write_text(harness + utf8 + helpers + functions + main)
    subprocess.run([cc, "-std=c17", str(source), "-o", str(executable)], check=True, env=env)
    subprocess.run([str(executable)], cwd=folder, check=True, env=env)


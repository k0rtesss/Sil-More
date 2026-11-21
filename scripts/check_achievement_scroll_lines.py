"""Exercise achievement-book line clipping and scroll reachability.

The production line renderer and scroll-target functions are compiled with a
small text fixture. No shared game build, window, player save, or emulator is
needed. Paragraphs contain several wrapped lines and a non-integral gap.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/sdl/ui/sdl-screens.c").read_text(encoding="utf-8")


def function(marker):
    start = SOURCE.index(marker)
    opening = SOURCE.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (SOURCE[end] == "{") - (SOURCE[end] == "}")
        end += 1
    return SOURCE[start:end]


renderer = function("static void sdl_char_sheet_draw_visible_icon_book_text(")
target = function("static int sdl_char_sheet_icon_book_scroll_target(")
assert "sdl_char_sheet_draw_visible_icon_book_text(body_font," in SOURCE
assert "sdl_character_sheet_icon_book_scroll_target(" in function(
    "bool sdl_character_sheet_screen_scroll_book(")
assert "sdl_char_sheet_icon_book_scroll_target(" in function(
    "static int sdl_character_sheet_icon_book_scroll_target(")

HARNESS = r"""
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char byte;
typedef const char* cptr;
typedef int TTF_Font;
#define SDL_CHAR_SHEET_MAX_LINES 80
#define SDL_CHAR_SHEET_TEXT_LEN 768
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ABS(a) ((a)<0?-(a):(a))
#define SDL_floorf floorf
#define SDL_roundf roundf
static int seen, painted, fixture_text_h;
static int expected_top, expected_bottom;
static bool TTF_GetStringSize(TTF_Font* font, cptr text, size_t length,
    int* width, int* height)
{
    (void)font; (void)text; (void)length; *width=1; *height=fixture_text_h;
    return true;
}
static int sdl_char_sheet_wrap_text(TTF_Font* font, cptr text, float width,
    char out[][SDL_CHAR_SHEET_TEXT_LEN], int cap)
{
    int count = 0;
    (void)font; (void)width;
    for (const char* p=text; *p; p++)
        if (*p!='|') {assert(count<cap);out[count][0]=*p;out[count++][1]='\0';}
    return count;
}
static int sdl_char_sheet_draw_text(TTF_Font* font, cptr text, byte attr,
    float x, float y, float width, float h, bool centered)
{
    (void)font; (void)attr; (void)x; (void)width; (void)centered;
    assert(roundf(y) >= expected_top && roundf(y)+MIN(h,(float)fixture_text_h) <= expected_bottom);
    assert(*text >= 'A' && *text <= 'G');
    seen |= 1 << (*text-'A');
    painted++;
    return 0;
}
"""

MAIN = r"""
int main(void)
{
    TTF_Font font=1;
    const int counts[]={3,4};
    const float line_heights[]={9.2f,18.7f,87.2f,112.75f};
    const float top=200.0f;
    for (unsigned lh_index=0;lh_index<sizeof(line_heights)/sizeof(line_heights[0]);lh_index++) {
    float lh=line_heights[lh_index], gap=lh*0.6f;
    fixture_text_h=(int)(lh/1.28f);
    const float heights[]={lh,lh*1.2f,lh*2.5f,lh*3.8f};
    for (unsigned h=0;h<sizeof(heights)/sizeof(heights[0]);h++) {
        float viewport=heights[h], content=7*lh+gap;
        int max_scroll=(int)floorf(content-viewport), scroll=0, steps=0;
        seen=0; painted=0;
        expected_top=(int)top;
        expected_bottom=expected_top+(int)(viewport+0.5f);
        for (;;) {
            int before=painted;
            sdl_char_sheet_draw_visible_icon_book_text(&font,"A|B|C",1,0,
                top-scroll,200,lh,expected_top,expected_bottom);
            sdl_char_sheet_draw_visible_icon_book_text(&font,"D|E|F|G",1,0,
                top+3*lh+gap-scroll,200,lh,expected_top,expected_bottom);
            if (painted==before)
                fprintf(stderr,"empty viewport=%.2f scroll=%d max=%d\n",viewport,scroll,max_scroll);
            assert(painted>before);
            if (scroll==max_scroll) break;
            int next=sdl_char_sheet_icon_book_scroll_target(counts,0,2,lh,gap,
                scroll,max_scroll,1);
            assert(next>scroll && next<=max_scroll);
            scroll=next; assert(++steps<30);
        }
        assert(seen==0x7f && painted>=7);
        steps=0;
        while (scroll>0) {
            int next=sdl_char_sheet_icon_book_scroll_target(counts,0,2,lh,gap,
                scroll,max_scroll,-1);
            assert(next<scroll && next>=0);
            scroll=next; assert(++steps<30);
        }
        /* An in-between drag cannot paint a clipped line at either edge. */
        seen=painted=0;
        sdl_char_sheet_draw_visible_icon_book_text(&font,"A|B|C",1,0,
            top-lh/2,200,lh,expected_top,expected_bottom);
        int midway=(int)(lh/2);
        int snapped=sdl_char_sheet_icon_book_scroll_target(counts,0,2,lh,gap,
            midway,max_scroll,0);
        assert(snapped==0 || snapped==(int)lh);
        seen=painted=0;
        sdl_char_sheet_draw_visible_icon_book_text(&font,"A|B|C",1,0,
            top-snapped,200,lh,expected_top,expected_bottom);
        assert(painted>0);
        assert(sdl_char_sheet_icon_book_scroll_target(counts,0,2,lh,gap,
            snapped,max_scroll,0)==snapped);
    }
    }
    puts("Achievement book keeps rows whole and reaches every line PASS");
}
"""

msys = Path("C:/msys64/mingw64/bin")
cc = os.environ.get("CC") or shutil.which("gcc") or str(msys / "gcc.exe")
env = os.environ.copy()
if msys.exists():
    env["PATH"] = os.pathsep.join((str(msys), str(msys.parent / "usr/bin"), env.get("PATH", "")))
with tempfile.TemporaryDirectory(prefix="sil-achievement-scroll-") as directory:
    source = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    source.write_text(HARNESS + target + renderer + MAIN, encoding="utf-8")
    subprocess.run([cc, "-std=c17", str(source), "-o", str(exe), "-lm"], check=True, env=env)
    subprocess.run([str(exe)], check=True, env=env)

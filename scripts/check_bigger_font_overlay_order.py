#!/usr/bin/env python3
"""Exercise production overlay blocker/order with measured geometry fixtures.

No renderer, save, config, or shared build is opened. Production blocker and
ordering functions are compiled verbatim; mocks supply measured pane bounds.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/bigger-font-overlay-order"


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


FIXTURE = r'''
#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifndef SIL_SDL_MOBILE_BUILD
#define SIL_SDL_MOBILE_BUILD 0
#endif
#define MAX(a,b) ((a)>(b)?(a):(b))
enum pane_type { PANE_MAIN,PANE_LEFT_PANEL,PANE_COMBAT,PANE_ROLLS,PANE_MESSAGES,PANE_MAX };
enum pane_placement { PLACE_TOP_LEFT,PLACE_TOP_CENTER,PLACE_TOP_RIGHT,PLACE_BOTTOM_LEFT };
typedef struct { int x,y,w,h; } SDL_Rect;
typedef struct { float x,y,w,h; } SDL_FRect;
struct pane_config { enum pane_type pane; enum pane_placement where; bool enabled; };
struct pane_config pane_config[5];
int pane_config_count;
#define MAX_TERM_DATA PANE_MAX
SDL_Rect g_pane_rects[PANE_MAX],base[PANE_MAX];
struct { SDL_Rect rect; } g_views[PANE_MAX];
int g_top_right_overlay_offset;
bool g_top_right_overlay_shifted_panes[PANE_MAX];
int g_overlay_stack_shift_x[PANE_MAX],g_overlay_stack_shift_y[PANE_MAX];
static bool bigger, menu_active, supporting=true;
static int calls[8],call_count;
bool get_sdl_bigger_font(void) { return bigger; }
bool sdl_should_show_supporting_panes(void) { return supporting; }
bool sdl_rect_has_area(const SDL_Rect *r) { return r->w>0 && r->h>0; }
enum pane_placement sdl_left_panel_pane_placement(void) { return pane_config[0].where; }
bool sdl_left_panel_pane_presentation_active(void) { return true; }
bool sdl_left_panel_pane_collapsed(void) { return true; }
bool sdl_left_panel_compact_row_mode(void) { return true; }
bool sdl_main_menu_pane_button_rect(SDL_FRect *out)
{ *out=(SDL_FRect){300,0,60,40}; return menu_active; }
bool sdl_overlay_log_pane_current_rect(SDL_Rect *out)
{ *out=g_pane_rects[PANE_ROLLS]; return true; }
bool sdl_overlay_stack_visible_rect(enum pane_type pane,SDL_Rect *out)
{ calls[call_count++]=4; *out=g_pane_rects[pane]; return sdl_rect_has_area(out); }
/* ACTUAL OFFSET HELPERS */
static void sdl_remove_overlay_stack_offsets(void)
{ calls[call_count++]=1; actual_remove_stack(); }
static void sdl_remove_top_right_overlay_offset(void)
{ calls[call_count++]=2; actual_remove_top_right(); }
static void sdl_apply_overlay_stack_layout(void)
{
    calls[call_count++]=3;
    /* Character and separately measured weapon/armour pane share this stack. */
    int target=g_pane_rects[PANE_LEFT_PANEL].y+g_pane_rects[PANE_LEFT_PANEL].h;
    sdl_overlay_stack_shift_pane(PANE_COMBAT,0,target-g_pane_rects[PANE_COMBAT].y);
}
/* ACTUAL FUNCTIONS */
static bool intersects(SDL_Rect a,SDL_Rect b)
{ return a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h; }
static void check(enum pane_placement where,bool enabled,bool horizontal_overlap,bool large)
{
    bigger=large; menu_active=false; pane_config_count=4;
    pane_config[0]=(struct pane_config){PANE_LEFT_PANEL,where,true};
    pane_config[1]=(struct pane_config){PANE_COMBAT,where,enabled};
    pane_config[2]=(struct pane_config){PANE_ROLLS,PLACE_TOP_RIGHT,true};
    pane_config[3]=(struct pane_config){PANE_MESSAGES,PLACE_TOP_RIGHT,true};
    memset(base,0,sizeof(base));
    base[PANE_LEFT_PANEL]=(SDL_Rect){0,0,230,100};
    base[PANE_COMBAT]=(SDL_Rect){0,40,horizontal_overlap?300:130,60};
    base[PANE_ROLLS]=(SDL_Rect){160,40,200,70};
    base[PANE_MESSAGES]=(SDL_Rect){160,110,200,30};
    sdl_reset_top_right_overlay_offset();
    memcpy(g_pane_rects,base,sizeof(base));
    for(int pane=0;pane<PANE_MAX;pane++) g_views[pane].rect=base[pane];
    int previous=-1;
    for(int frame=0;frame<25;frame++) {
        call_count=0;
        sdl_apply_top_right_overlay_offset();
        assert(calls[0]==1 && calls[1]==2);
        if(large && enabled) assert(call_count==4 && calls[2]==3 && calls[3]==4);
        else assert(call_count==3 && calls[2]==3);
        assert(g_pane_rects[PANE_ROLLS].y==(large && enabled && horizontal_overlap?160:100));
        assert(g_pane_rects[PANE_MESSAGES].y-g_pane_rects[PANE_ROLLS].y==70);
        assert(!memcmp(&g_views[PANE_ROLLS].rect,&g_pane_rects[PANE_ROLLS],sizeof(SDL_Rect)));
        if(previous>=0) assert(previous==g_pane_rects[PANE_ROLLS].y);
        previous=g_pane_rects[PANE_ROLLS].y;
        if(large && enabled && horizontal_overlap)
            assert(!intersects(g_pane_rects[PANE_COMBAT],g_pane_rects[PANE_ROLLS]));
    }
    actual_remove_stack(); actual_remove_top_right();
    assert(!memcmp(g_pane_rects,base,sizeof(base)));
    for(int pane=0;pane<PANE_MAX;pane++) assert(!memcmp(&g_views[pane].rect,&base[pane],sizeof(SDL_Rect)));
}
int main(void)
{
    for(int where=PLACE_TOP_LEFT;where<=PLACE_TOP_CENTER;where++) {
        for(int enabled=0;enabled<2;enabled++) for(int overlap=0;overlap<2;overlap++)
            check(where,enabled,overlap,true);
        check(where,true,true,false);
    }
    puts("production Bigger-font stack-before-blocker order, TopLeft/TopCenter measured weapon avoidance, disabled/noncrossing bounds, repeated frames, Normal-off behavior: PASS");
    return 0;
}
'''


def main():
    source = (ROOT / "src/sdl/core/sdl-layout.c").read_text()
    actual = function(source, "static void sdl_apply_top_right_overlay_blocker_offset(void)")
    actual += "\n" + function(source, "void sdl_apply_top_right_overlay_offset(void)")
    helpers = "\n".join(function(source, signature) for signature in [
        "void sdl_reset_top_right_overlay_offset(void)",
        "static void sdl_remove_overlay_stack_offsets(void)",
        "static void sdl_remove_top_right_overlay_offset(void)",
        "static void sdl_overlay_stack_shift_pane(enum pane_type pane,"])
    helpers = helpers.replace("sdl_remove_overlay_stack_offsets", "actual_remove_stack")
    helpers = helpers.replace("sdl_remove_top_right_overlay_offset", "actual_remove_top_right")
    OUT.mkdir(parents=True, exist_ok=True)
    fixture = OUT / "check.c"
    fixture.write_text(FIXTURE.replace("/* ACTUAL FUNCTIONS */", actual)
                       .replace("/* ACTUAL OFFSET HELPERS */", helpers), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    for mobile in (0, 1):
        exe = OUT / f"check-{mobile}.exe"
        subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-O0",
                        f"-DSIL_SDL_MOBILE_BUILD={mobile}", str(fixture), "-o", str(exe)],
                       env=env, check=True)
        subprocess.run([str(exe)], env=env, check=True)


if __name__ == "__main__":
    main()

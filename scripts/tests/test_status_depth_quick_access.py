"""Regression geometry checks for full-size Status & Depth beside Quick Access.

Runs production geometry and public cached-layout entry points in isolation,
including a mocked Quick Access dependency that re-enters the status rectangle.
The Android smoke test additionally checks the painted result and touch targets.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
def function(path, marker):
    source = (ROOT / path).read_text()
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    return source[start:cursor]

avoid = function("src/sdl/ui/sdl-panes.c", "static void sdl_status_depth_pane_avoid_quick(")
wrapper = function("src/sdl/input/sdl-touch-controls.c", "bool sdl_touch_top_panel_compute_layout_for_anchor(")
public_layout = function("src/sdl/ui/sdl-panes.c", "bool sdl_status_depth_pane_layout(")
public_rect = function("src/sdl/ui/sdl-panes.c", "bool sdl_status_depth_pane_current_rect(")
harness = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
typedef struct {float x,y,w,h;} SDL_FRect;
typedef struct {int x,y,w,h;} SDL_Rect;
static struct {bool bigger_font;} config = {true};
enum pane_placement {PLACE_TOP_LEFT,PLACE_TOP_CENTER,PLACE_TOP_RIGHT,
    PLACE_BOTTOM_LEFT,PLACE_BOTTOM_CENTER,PLACE_BOTTOM_RIGHT};
static int sdl_overlay_inner_gap_px(void) {return 8;}
static bool sdl_left_panel_pane_placement_is_bottom(enum pane_placement w) {
    return w>=PLACE_BOTTOM_LEFT;
}
static bool g_touch_top_panel_layout_computing;
static bool implementation_result=true;
static bool sdl_touch_top_panel_compute_layout_for_anchor_impl(const SDL_Rect *s,
    const SDL_Rect *a,enum pane_placement w,SDL_FRect *b,SDL_FRect *p) {
    /* Wheel obstacle measurement must see this scope on success and failure. */
    assert(g_touch_top_panel_layout_computing);
    return implementation_result;
}
"""
dependency_harness = r"""
typedef uint64_t Uint64;
typedef struct { SDL_FRect panel; } status_depth_pane_layout;
/* This fixture contains Status/Depth and Quick Access; there is no Combat pane. */
static void sdl_status_depth_pane_avoid_combat(SDL_FRect *r,
    enum pane_placement where,const SDL_Rect *screen) {}
struct pane_config { bool enabled; enum pane_placement where; };
#define PANE_STATUS_DEPTH 1
static bool g_status_depth_pane_layout_computing;
static Uint64 g_sdl_present_generation=1;
static struct pane_config test_pc={true,PLACE_BOTTOM_RIGHT};
static bool have_pc=true,compute_ok=true,anchor_ok=true,quick_ok=true;
static int compute_calls,anchor_calls,quick_calls,reentries;
bool sdl_status_depth_pane_layout(status_depth_pane_layout *out);
bool sdl_status_depth_pane_current_rect(SDL_FRect *out);
static const struct pane_config *sdl_status_depth_pane_config(void) {
    return have_pc ? &test_pc : NULL;
}
static void dependency_reenter_status_rect(void) {
    SDL_FRect ignored={1,2,3,4};
    ++reentries;
    /* The same public path seen in the Android stack overflow. It must stop
     * before a second anchor query, including on a warmed generation cache. */
    assert(g_status_depth_pane_layout_computing);
    assert(!sdl_status_depth_pane_current_rect(&ignored));
    assert(ignored.w==0 && ignored.h==0);
    assert(g_status_depth_pane_layout_computing);
}
static bool sdl_status_depth_pane_layout_compute(status_depth_pane_layout *out) {
    ++compute_calls; dependency_reenter_status_rect();
    out->panel=(SDL_FRect){990,600,270,78}; return compute_ok;
}
static bool sdl_overlay_pane_anchor_rect(int pane,SDL_Rect *out) {
    ++anchor_calls; dependency_reenter_status_rect();
    *out=(SDL_Rect){0,0,1280,720}; return anchor_ok;
}
static SDL_Rect sdl_get_layout_screen_rect(void) {return (SDL_Rect){0,0,1280,720};}
static SDL_FRect sdl_overlay_panel_rect(const SDL_Rect *anchor,
    enum pane_placement where,int w,int h,const SDL_Rect *screen) {
    return (SDL_FRect){990,600,(float)w,(float)h};
}
static bool sdl_touch_top_panel_current_anchor(SDL_Rect *screen,
    SDL_Rect *anchor,enum pane_placement *where) {
    ++quick_calls; dependency_reenter_status_rect();
    *where=PLACE_BOTTOM_CENTER; return quick_ok;
}
static bool sdl_status_depth_pane_same_horizontal_edge(enum pane_placement a,
    enum pane_placement b) {return true;}
static bool sdl_touch_top_panel_compute_layout(SDL_FRect *buttons,SDL_FRect *out) {
    dependency_reenter_status_rect();
    *out=(SDL_FRect){398,592,676,78};return true;
}
"""
main = r"""
static bool overlap(SDL_FRect a,SDL_FRect b) {
    return a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h;
}
int main(void) {
    /* Reproduced phone: the free side span is narrower than the full caption. */
    SDL_Rect screen={0,0,1280,720};
    SDL_FRect quick={398,592,676,78},caption={990,600,270,78};
    assert(overlap(caption,quick));
    sdl_status_depth_pane_avoid_quick(&caption,&quick,PLACE_BOTTOM_RIGHT,&screen);
    assert(!overlap(caption,quick) && caption.w==270 && caption.h==78);
    assert(caption.y+caption.h==quick.y-8 && caption.y>=screen.y);
    /* Re-anchoring a cached layout applies exactly the same avoidance. */
    SDL_FRect cached={990,600,270,78};
    sdl_status_depth_pane_avoid_quick(&cached,&quick,PLACE_BOTTOM_RIGHT,&screen);
    assert(cached.x==caption.x && cached.y==caption.y);

    /* Larger landscape and ordinary portrait side spans need no relocation. */
    screen=(SDL_Rect){0,0,2400,1080};quick=(SDL_FRect){760,919,990,116};
    caption=(SDL_FRect){2100,953,280,70};
    sdl_status_depth_pane_avoid_quick(&caption,&quick,PLACE_BOTTOM_RIGHT,&screen);
    assert(caption.x==2100 && caption.y==953 && caption.w==280);
    screen=(SDL_Rect){0,0,720,1280};quick=(SDL_FRect){15,1130,500,85};
    caption=(SDL_FRect){530,1180,175,65};
    sdl_status_depth_pane_avoid_quick(&caption,&quick,PLACE_BOTTOM_RIGHT,&screen);
    assert(caption.y==1180);

    /* Top-edge collision uses the inward band, respecting safe-area offsets. */
    screen=(SDL_Rect){30,40,720,1280};quick=(SDL_FRect){80,45,550,90};
    caption=(SDL_FRect){520,55,200,75};
    sdl_status_depth_pane_avoid_quick(&caption,&quick,PLACE_TOP_RIGHT,&screen);
    assert(!overlap(caption,quick) && caption.y==143 && caption.w==200);
    /* Measurement guard must restore its caller's state on every return. */
    assert(sdl_touch_top_panel_compute_layout_for_anchor(&screen,&screen,
        PLACE_BOTTOM_CENTER,NULL,NULL));
    assert(!g_touch_top_panel_layout_computing);
    implementation_result=false;
    assert(!sdl_touch_top_panel_compute_layout_for_anchor(&screen,&screen,
        PLACE_BOTTOM_CENTER,NULL,NULL));
    assert(!g_touch_top_panel_layout_computing);
    g_touch_top_panel_layout_computing=true;
    assert(!sdl_touch_top_panel_compute_layout_for_anchor(&screen,&screen,
        PLACE_BOTTOM_CENTER,NULL,NULL));
    assert(g_touch_top_panel_layout_computing);
    /* Cold compute, warmed cache re-anchor, and public rectangle all terminate
     * through the real wrapper when the dependency queries status again. */
    status_depth_pane_layout layout;
    SDL_FRect public_caption;
    assert(sdl_status_depth_pane_layout(&layout));
    assert(compute_calls==1 && anchor_calls==1 && quick_calls==1 && reentries==4);
    assert(!g_status_depth_pane_layout_computing);
    assert(sdl_status_depth_pane_layout(&layout));
    assert(compute_calls==1 && anchor_calls==2 && quick_calls==2 && reentries==7);
    assert(!g_status_depth_pane_layout_computing && layout.panel.y==506);
    assert(sdl_status_depth_pane_current_rect(&public_caption));
    assert(public_caption.y==506 && compute_calls==1);

    /* Every public early exit restores the guard and permits a later layout. */
    assert(!sdl_status_depth_pane_layout(NULL));
    test_pc.enabled=false;
    assert(!sdl_status_depth_pane_layout(&layout));
    assert(!g_status_depth_pane_layout_computing);
    test_pc.enabled=true;have_pc=false;
    assert(!sdl_status_depth_pane_layout(&layout));
    assert(!g_status_depth_pane_layout_computing);
    have_pc=true;anchor_ok=false;
    assert(!sdl_status_depth_pane_layout(&layout));
    assert(!g_status_depth_pane_layout_computing);
    anchor_ok=true;compute_ok=false;++g_sdl_present_generation;
    assert(!sdl_status_depth_pane_layout(&layout));
    assert(!g_status_depth_pane_layout_computing);
    compute_ok=true;++g_sdl_present_generation;quick_ok=false;
    assert(sdl_status_depth_pane_layout(&layout));
    assert(!g_status_depth_pane_layout_computing);
    quick_ok=true;
    assert(sdl_status_depth_pane_layout(&layout));
    assert(!g_status_depth_pane_layout_computing);
    g_status_depth_pane_layout_computing=true;
    assert(!sdl_status_depth_pane_layout(&layout));
    assert(g_status_depth_pane_layout_computing);
    g_status_depth_pane_layout_computing=false;
    assert(sdl_status_depth_pane_layout(&layout));
    puts("Status & Depth / Quick Access regression checks passed");
}
"""
msys = Path(r"C:\msys64\mingw64\bin")
cc = os.environ.get("CC") or shutil.which("gcc") or str(msys / "gcc.exe")
env = os.environ.copy()
if msys.exists():
    env["PATH"] = str(msys) + os.pathsep + str(msys.parent / "usr/bin") + os.pathsep + env.get("PATH", "")
with tempfile.TemporaryDirectory(prefix="sil-depth-layout-") as directory:
    directory = Path(directory)
    source = directory / "check.c"
    target = directory / ("check.exe" if os.name == "nt" else "check")
    source.write_text(harness + avoid + wrapper + dependency_harness + public_layout + public_rect + main)
    subprocess.run([cc,"-std=c17",str(source),"-o",str(target)],check=True,env=env)
    subprocess.run([str(target)],check=True,env=env)

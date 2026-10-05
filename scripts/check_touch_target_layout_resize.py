#!/usr/bin/env python3
"""Check production aim-control snapshots across window and safe-area changes."""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/touch-target-layout-resize-check"


def function(source, name):
    match = re.search(r"^(?:static\s+)?\w+\s+" + re.escape(name)
                      + r"\s*\([^;{}]*\)\s*\{", source, re.M)
    assert match, name
    opening = source.index("{", match.start())
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


HARNESS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
typedef struct { int x, y, w, h; } SDL_Rect;
typedef struct { float x, y, w, h; } SDL_FRect;
#define SDL_TOUCH_THUMB_RUNTIME_CAPACITY 6
__STATE__
static touch_target_layout_state g_touch_target_layout;
static SDL_Rect screen = { 0, 0, 1600, 2560 };
static int measurements;
static SDL_Rect sdl_get_layout_screen_rect(void) { return screen; }
static bool sdl_touch_thumb_compute_runtime_rects(SDL_FRect *rects, int count)
{
    assert(!g_touch_target_layout.locked);
    ++measurements;
    for (int i=0; i<count; ++i)
        rects[i]=(SDL_FRect){screen.x,screen.y+screen.h-100,100,100};
    return true;
}
static bool sdl_touch_round_compute_layout(float *cx,float *cy,float *r,
    float *inner,SDL_Rect *clip)
{
    assert(!g_touch_target_layout.locked);
    ++measurements;
    *r=100; *inner=45; *cx=screen.x+screen.w-100;
    *cy=screen.y+screen.h-100; *clip=screen;
    return true;
}
void sdl_touch_target_layout_begin(void);
__FUNCTIONS__
static void check_snapshot(void)
{
    assert(g_touch_target_layout.locked);
    assert(g_touch_target_layout.thumb_valid && g_touch_target_layout.wheel_valid);
    assert(g_touch_target_layout.wheel_cy+g_touch_target_layout.wheel_radius
        <=screen.y+screen.h);
    assert(g_touch_target_layout.wheel_cx+g_touch_target_layout.wheel_radius
        <=screen.x+screen.w);
    assert(g_touch_target_layout.thumb_rects[0].y
        +g_touch_target_layout.thumb_rects[0].h<=screen.y+screen.h);
}
int main(void)
{
    sdl_touch_target_layout_begin(); check_snapshot(); assert(measurements==2);
    /* Normal ownership/layout changes must not cause fresh measurements. */
    sdl_touch_target_layout_refresh_viewport(); assert(measurements==2);
    screen.h=2000; sdl_touch_target_layout_refresh_viewport();
    check_snapshot(); assert(measurements==4);
    sdl_touch_target_layout_refresh_viewport(); assert(measurements==4);
    screen.w=720; screen.h=1280; sdl_touch_target_layout_refresh_viewport();
    check_snapshot(); assert(measurements==6);
    /* Insets can change usable geometry without changing display size. */
    screen.x=12; sdl_touch_target_layout_refresh_viewport(); assert(measurements==8);
    screen.y=24; sdl_touch_target_layout_refresh_viewport(); assert(measurements==10);
    screen.w-=24; sdl_touch_target_layout_refresh_viewport(); assert(measurements==12);
    screen.h-=48; sdl_touch_target_layout_refresh_viewport(); assert(measurements==14);
    check_snapshot();
    sdl_touch_target_layout_end(); assert(!g_touch_target_layout.locked);
    screen.h=2560; sdl_touch_target_layout_refresh_viewport(); assert(measurements==14);
    puts("Aim controls: stable ownership, window resize, safe-area resize, unlocked measurement PASS");
    return 0;
}
'''


def main():
    controls = (ROOT / "src/sdl/input/sdl-touch-controls.c").read_text(encoding="utf-8")
    state = re.search(r"typedef struct touch_target_layout_state\s*\{.*?\}\s*touch_target_layout_state;", controls, re.S).group()
    # Both actual consumers must refresh before taking their cached geometry.
    for name in ("sdl_touch_thumb_compute_runtime_rects", "sdl_touch_round_compute_layout"):
        body = function(controls, name)
        assert body.index("sdl_touch_target_layout_refresh_viewport();") < body.index("if (g_touch_target_layout.locked)")
    names = ("sdl_touch_target_layout_refresh_viewport", "sdl_touch_target_layout_begin", "sdl_touch_target_layout_end")
    code = HARNESS.replace("__STATE__", state).replace("__FUNCTIONS__", "\n".join(function(controls, n) for n in names))
    OUT.mkdir(parents=True, exist_ok=True)
    source, exe = OUT / "check.c", OUT / "check.exe"
    source.write_text(code, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = "C:/msys64/mingw64/bin;C:/msys64/usr/bin;" + env["PATH"]
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra", str(source), "-o", str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=20)


if __name__ == "__main__":
    main()

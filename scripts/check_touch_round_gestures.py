#!/usr/bin/env python3
"""Replay production direction-wheel gestures without opening player state.

The Android playtest exposed a quick cancelled drag becoming a repeated move.
Compile the actual wheel reducer with a deterministic clock and record the
commands it emits, including ordinary taps, movement, run, aim and cancellation.
"""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/touch-round-gestures-check"


def function(source, name):
    match = re.search(r"^(?:static\s+)?\w+\s+" + re.escape(name) + r"\s*\(", source, re.M)
    assert match, name
    opening = source.index("{", match.start())
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


HARNESS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
typedef uint64_t Uint64;
typedef int64_t SDL_FingerID;
typedef struct { int x, y, w, h; } SDL_Rect;
#define SDL_sqrtf sqrtf
__CONSTANTS__
__PRESS_STATE__
static touch_round_press_state g_touch_round_press;
static struct { bool need_present; } g_state;
static Uint64 now;
static int g_touch_round_last_dir;
static bool suppressed, aiming, overlay;
static float radius, inner;
static int sends, sent_dir;
static bool sent_ctrl, sent_run;
static Uint64 SDL_GetTicksNS(void) { return now; }
static bool sdl_touch_gameplay_controls_suppressed(void) { return suppressed; }
static bool sdl_touch_round_layer_controls_active(void) { return !suppressed; }
static bool sdl_touch_round_aim_targeting_active(void) { return aiming; }
static bool sdl_unified_look_prompt_contains_point(float x, float y)
{ (void)x; (void)y; return overlay; }
static bool sdl_touch_movement_point_blocked_by_overlay(float x, float y)
{ (void)x; (void)y; return overlay; }
static bool sdl_touch_round_point_in_bounds(float x, float y)
{ return x*x + y*y <= radius*radius; }
static float sdl_touch_round_radius_px(void) { return radius; }
static bool sdl_touch_round_compute_layout(float *cx, float *cy, float *r,
    float *ir, SDL_Rect *clip)
{
    if (cx) *cx = 0;
    if (cy) *cy = 0;
    if (r) *r = radius;
    if (ir) *ir = inner;
    (void)clip; return true;
}
static void sdl_gamepad_send_direction_mods(int dir, bool run, bool ctrl, bool alt)
{ ++sends; sent_dir=dir; sent_run=run; sent_ctrl=ctrl; assert(!alt); }
#define CANCEL(name) static void name(void) {}
CANCEL(sdl_menu_touch_cancel)
CANCEL(sdl_menu_scroll_cancel)
CANCEL(sdl_map_touch_cancel_press)
CANCEL(sdl_pointer_attack_cancel_touch_press)
CANCEL(sdl_touch_zone_cancel_press)
CANCEL(sdl_touch_top_panel_cancel_press)
CANCEL(sdl_touch_swipe_cancel)
__FUNCTIONS__

static void begin(float x, float y)
{
    sdl_touch_round_cancel_press(); now=1000000000ULL;
    sends=0; sent_dir=0; sent_ctrl=false; sent_run=false;
    g_touch_round_last_dir=4; suppressed=false; aiming=false; overlay=false;
    assert(sdl_touch_round_handle_pointer_down(x,y,7));
}
static void motion(float x, float y)
{ now+=50000000ULL; assert(sdl_touch_round_handle_pointer_motion(x,y,7)); }
static void release(float x, float y, int ms)
{
    now=1000000000ULL+(Uint64)ms*1000000ULL;
    assert(sdl_touch_round_handle_pointer_up(x,y,7));
    assert(!g_touch_round_press.active);
}
static void check(float r)
{
    radius=r; inner=r*SDL_TOUCH_ROUND_INNER_RADIUS_FRAC;

    /* A real centre tap, including a small wobble, still repeats once. */
    begin(0,0); motion(inner*0.15f,0); motion(0,0); release(0,0,150);
    assert(sends==1 && sent_dir==4 && !sent_ctrl && !sent_run);
    begin(inner*0.5f,0); release(inner*0.5f,0,150);
    assert(sends==1 && sent_dir==4);

    /* Returning to neutral cancels a direction drag at any gesture speed. */
    begin(0,0); motion(inner*0.8f,0); motion(0,0); release(0,0,250);
    assert(sends==0);
    begin(0,0); motion(inner*0.8f,0); motion(0,0); release(0,0,650);
    assert(sends==0);
    begin(inner*0.8f,0); motion(0,0); release(0,0,150);
    assert(sends==0);
    begin(0,0); motion(radius*1.5f,0); motion(0,0); release(0,0,250);
    assert(sends==0);

    /* Leaving and then reselecting a direction remains ordinary movement. */
    begin(0,0); motion(inner*0.8f,0); motion(0,0);
    motion(0,-inner*0.8f); release(0,-inner*0.8f,250);
    assert(sends==1 && sent_dir==8 && !sent_ctrl && !sent_run);
    begin(0,0); motion(inner*0.8f,0); release(inner*0.8f,0,650);
    assert(sends==1 && sent_dir==6 && !sent_ctrl && !sent_run);
    begin(0,0); motion(radius,0); release(radius,0,250);
    assert(sends==1 && sent_dir==6 && sent_run && !sent_ctrl);

    /* Direct arrow taps and holds retain move and Ctrl/alter semantics. */
    begin(radius*0.8f,0); release(radius*0.8f,0,150);
    assert(sends==1 && sent_dir==6 && !sent_ctrl && !sent_run);
    begin(radius*0.8f,0); release(radius*0.8f,0,650);
    assert(sends==1 && sent_dir==6 && sent_ctrl && !sent_run);
    begin(radius*0.8f,0); motion(0,0); release(0,0,150);
    assert(sends==0);

    /* Aim never repeats a centre tap or applies run/Ctrl modifiers. */
    begin(0,0); aiming=true; release(0,0,150); assert(sends==0);
    begin(0,0); aiming=true; motion(radius,0); release(radius,0,650);
    assert(sends==1 && sent_dir==6 && !sent_run && !sent_ctrl);
    begin(radius*0.8f,0); aiming=true; release(radius*0.8f,0,650);
    assert(sends==1 && sent_dir==6 && !sent_run && !sent_ctrl);

    /* Another finger cannot finish the owner's press; cancellation cannot fire. */
    begin(0,0);
    assert(!sdl_touch_round_handle_pointer_motion(inner,0,8));
    assert(!sdl_touch_round_handle_pointer_up(inner,0,8));
    assert(sends==0 && g_touch_round_press.active);
    sdl_touch_round_cancel_press();
    assert(!sdl_touch_round_handle_pointer_up(inner,0,7)); assert(sends==0);
    begin(0,0); suppressed=true;
    assert(!sdl_touch_round_handle_pointer_up(inner,0,7));
    assert(sends==0 && !g_touch_round_press.active);
    suppressed=false; overlay=true;
    assert(!sdl_touch_round_handle_pointer_down(0,0,7));
    assert(!g_touch_round_press.active);
    printf("Direction wheel radius %.0f: repeat, cancelled drags, move/run/alter, aim and ownership PASS\n",r);
}
int main(void) { check(72); check(150); check(345); return 0; }
'''


def main():
    controls = (ROOT / "src/sdl/input/sdl-touch-controls.c").read_text(encoding="utf-8")
    header = (ROOT / "src/sdl/main-sdl-private.h").read_text(encoding="utf-8")
    state = re.search(r"typedef struct touch_round_press_state\s*\{.*?\}\s*touch_round_press_state;", header, re.S).group()
    constants = "\n".join(re.findall(r"^#define SDL_TOUCH_ROUND_\w+ .*$", controls, re.M))
    press = re.search(r"TOUCH_PANE_LONG_PRESS_MS\s*=\s*(\d+)", header)
    assert press, "long-press threshold"
    constants += "\n#define TOUCH_PANE_LONG_PRESS_MS " + press.group(1)
    names = ["sdl_touch_round_dir_for_delta", "sdl_touch_round_point_to_wheel",
             "sdl_touch_round_drag_is_run", "sdl_touch_round_send_dir",
             "sdl_touch_round_cancel_press", "sdl_touch_round_handle_pointer_down",
             "sdl_touch_round_handle_pointer_motion", "sdl_touch_round_handle_pointer_up"]
    code = HARNESS.replace("__PRESS_STATE__", state).replace("__CONSTANTS__", constants)
    code = code.replace("__FUNCTIONS__", "\n\n".join(function(controls, name) for name in names))
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    exe = OUT / "check.exe"
    source.write_text(code, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = "C:/msys64/mingw64/bin;C:/msys64/usr/bin;" + env["PATH"]
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra",
                    str(source), "-lm", "-o", str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=20)


if __name__ == "__main__":
    main()

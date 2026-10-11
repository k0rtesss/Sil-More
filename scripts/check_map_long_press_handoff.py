#!/usr/bin/env python3
"""Replay map long presses followed by wheel input using production reducers."""
from pathlib import Path
import os
import re
import subprocess

from check_touch_round_gestures import HARNESS as WHEEL, function

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/map-long-press-handoff-check"

EXTRA = r'''
__MAP_STATES__
static map_touch_press_state g_map_touch_press;
static main_map_drag_state g_main_map_drag;
static bool g_map_touch_selected;
static struct { int py, px; } player = {10, 10}, *p_ptr = &player;
static int route, dispatches;
static void dispatched(void)
{
    /* A popup can consume the old release. Its map gesture must already end. */
    assert(!g_main_map_drag.active && !g_map_touch_press.active);
    ++dispatches;
}
static bool sdl_player_action_menu_open(void) { dispatched(); return true; }
static bool sdl_mouse_feature_action_queue_grid(int y, int x)
{ (void)y; (void)x; if (route != 1) return false; dispatched(); return true; }
static bool sdl_mouse_grid_has_describable_content(int y, int x)
{ (void)y; (void)x; return route == 2; }
static bool sdl_mouse_recall_handle_right_click(float x, float y)
{ (void)x; (void)y; dispatched(); return true; }
static bool sdl_mouse_path_start_follow_grid(int y, int x)
{ (void)y; (void)x; dispatched(); return true; }
static void sdl_mouse_path_cancel(void) {}
static bool sdl_main_map_handle_mouse_release_click(float x, float y)
{ (void)x; (void)y; return true; }
#define SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY 2
static struct { int touch_movement_mode; } config;
__MAP_FUNCTIONS__
'''

CHECK = r'''
static void handoff(void)
{
    radius=150; inner=radius*SDL_TOUCH_ROUND_INNER_RADIUS_FRAC;
    for (route=0; route<5; route++) {
        for (int drag=0; drag<2; drag++) {
            sdl_touch_round_cancel_press();
            now=1000000000ULL; sends=0; dispatches=0;
            suppressed=aiming=overlay=false;
            g_main_map_drag=(main_map_drag_state){.active=true,.finger_id=7};
            g_map_touch_press=(map_touch_press_state){.active=true,.finger_id=7,
                .map_y=route ? 11 : 10,.map_x=10,.start_time=now};
            config.touch_movement_mode=route==3 ? SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY : 0;
            assert(!sdl_map_touch_flush_pending_press(now));
            assert(g_map_touch_press.active && g_main_map_drag.active);
            now+=(Uint64)TOUCH_PANE_LONG_PRESS_MS*1000000ULL;
            assert(sdl_map_touch_flush_pending_press(now));
            assert(dispatches==1 && sends==0 && !g_map_touch_selected);

            /* The old finger-up was taken by the contextual menu. The same
             * Android finger ID is reused by the next tap or run gesture. */
            assert(sdl_touch_round_handle_pointer_down(drag?0:radius*.8f,0,7));
            if (drag) {
                now+=50000000ULL;
                assert(sdl_touch_round_handle_pointer_motion(radius,0,7));
            }
            now+=50000000ULL;
            assert(!sdl_main_map_handle_drag_up(radius*.8f,0,false,7));
            assert(sdl_touch_round_handle_pointer_up(drag?radius:radius*.8f,0,7));
            assert(sends==1 && sent_dir==6 && sent_run==(bool)drag && !sent_ctrl);
            assert(!g_touch_round_press.active);
        }
    }
    puts("Map long-press handoff: player, feature, recall, path and fallback; next wheel tap/run PASS");
}
'''


def main():
    controls = (ROOT / "src/sdl/input/sdl-touch-controls.c").read_text(encoding="utf-8")
    tooltips = (ROOT / "src/sdl/input/sdl-tooltips.c").read_text(encoding="utf-8")
    geometry = (ROOT / "src/sdl/input/sdl-map-geometry.c").read_text(encoding="utf-8")
    header = (ROOT / "src/sdl/main-sdl-private.h").read_text(encoding="utf-8")

    def state(name):
        return re.search(r"typedef struct " + name + r"\s*\{.*?\}\s*" + name + ";", header, re.S).group()

    constants = "\n".join(re.findall(r"^#define SDL_TOUCH_ROUND_\w+ .*$", controls, re.M))
    constants += "\n#define TOUCH_PANE_LONG_PRESS_MS " + re.search(r"TOUCH_PANE_LONG_PRESS_MS\s*=\s*(\d+)", header).group(1)
    names = ["sdl_touch_round_dir_for_delta", "sdl_touch_round_point_to_wheel",
             "sdl_touch_round_drag_is_run", "sdl_touch_round_send_dir",
             "sdl_touch_round_cancel_press", "sdl_touch_round_handle_pointer_down",
             "sdl_touch_round_handle_pointer_motion", "sdl_touch_round_handle_pointer_up"]
    extra = EXTRA.replace("__MAP_STATES__", state("map_touch_press_state") + "\n" + state("main_map_drag_state"))
    extra = extra.replace("__MAP_FUNCTIONS__", "\n".join([
        function(geometry, "sdl_main_map_cancel_drag"),
        function(tooltips, "sdl_map_touch_cancel_press"),
        function(tooltips, "sdl_map_touch_flush_pending_press"),
        function(geometry, "sdl_main_map_handle_drag_up")]))
    code = WHEEL.replace("CANCEL(sdl_map_touch_cancel_press)", extra)
    code = code.replace("__PRESS_STATE__", state("touch_round_press_state")).replace("__CONSTANTS__", constants)
    code = code.replace("__FUNCTIONS__", "\n".join(function(controls, name) for name in names))
    code = code.replace("int main(void) {", CHECK + "\nint main(void) { handoff();")
    OUT.mkdir(parents=True, exist_ok=True)
    source, exe = OUT / "check.c", OUT / "check.exe"
    source.write_text(code, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = "C:/msys64/mingw64/bin;C:/msys64/usr/bin;" + env["PATH"]
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra",
                    str(source), "-lm", "-o", str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=20)


if __name__ == "__main__":
    main()

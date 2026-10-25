#!/usr/bin/env python3
"""Check desktop handheld typography/touch layouts and shared wheel geometry.

Uses production renderers with disposable SDL software windows. The input and
display fixtures do not establish physical Windows/Linux handheld coverage.
"""
from pathlib import Path
import os
import subprocess

import check_phone_aux_layout as aux
import check_phone_overlays_layout as overlays

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/handheld-ui-check"
CASES = [(1280, 800, 1.0), (1280, 800, 2.0),
         (1920, 1080, 1.5), (1080, 2400, 2.625)]


def handheld(harness):
    # The touch-only profile chooser is a separate mobile feature. Its private
    # list state is not built on desktop; shared menu type/targets are below.
    signature = "static void check_touch_panels("
    if signature in harness:
        chooser = function(harness, signature)
        harness = harness.replace(chooser, "").replace("check_touch_panels();", "")
    # Android and PC controller handhelds suppress the touch-only thumb dock.
    # Keep its fixed phone-only test rectangles in the original phone suite.
    harness = harness.replace("check_context_buttons();", "")
    harness = harness.replace("/context-labels-hit-dispatch PASS", " PASS")
    # A tall, low-density PC display can fit this entire tutorial at its fixed
    # readable font size. Only the compact logical viewport must overflow.
    harness = harness.replace("if(fixture_width<1080 && fixture_height<1600)",
                              "if(fixture_width/fixture_density<540 && fixture_height/fixture_density<800)")
    return harness.replace("#define SIL_SDL_MOBILE_BUILD 1",
                           "#define SIL_SDL_MOBILE_BUILD 0").replace(
        "sdl_config_set_defaults(&config);",
        "sdl_config_set_defaults(&config); "
        "g_startup_device_class=SDL_STARTUP_DEVICE_DESKTOP_HANDHELD;")


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]


def check_wheel():
    # Compile the production geometry without a dungeon or writable profile.
    actions = (ROOT / "src/sdl/input/sdl-player-actions.c").read_text()
    fonts = (ROOT / "src/sdl/render/sdl-fonts.c").read_text()
    source = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>
sdl_view g_views[MAX_TERM_DATA];
_Thread_local player_type* p_ptr;
sdl_startup_device_class g_startup_device_class;
int sdl_main_view_visual_cols(const sdl_view* v) { return v->cols; }
int sdl_main_view_visual_rows(const sdl_view* v) { return v->rows; }
float sdl_touch_pane_clampf(float v, float lo, float hi) {
    return MAX(lo, MIN(v, hi));
}
bool sdl_player_map_rect(int y, int x, SDL_FRect* r) {
    *r=(SDL_FRect){x,y,16,16}; return true;
}
int sdl_player_action_menu_collect(player_action_menu_entry* e) {
    for(int i=0;i<6;i++) e[i]=(player_action_menu_entry){.kind=i+1};
    return 6;
}
int sdl_player_action_menu_collect_secondary(int kind, player_action_menu_entry* e) {
    e[0]=(player_action_menu_entry){.kind=101};
    e[1]=(player_action_menu_entry){.kind=102}; return 2;
}
#define SDL_PLAYER_ACTION_MENU_PI 3.14159265f
'''
    source += function(fonts, "bool sdl_handheld_ui_active(void)")
    source += function(actions, "bool sdl_player_action_menu_layout(")
    source += function(actions, "static bool sdl_player_action_menu_layout_secondary(")
    source += r'''
int main(void) {
    player_type player={0}; p_ptr=&player;
    sdl_view* view=&g_views[PANE_MAIN];
    for(int h=16;h<=48;h+=8) for(int edge=0;edge<3;edge++) {
        view->cell_w=h/2; view->cell_h=h; view->cols=80; view->rows=25;
        view->rect=(SDL_Rect){0,0,view->cols*view->cell_w,view->rows*h};
        player.px=edge==0 ? 0 : edge==1 ? view->rect.w/2 : view->rect.w-16;
        player.py=edge==0 ? 0 : edge==1 ? view->rect.h/2 : view->rect.h-16;
        player_action_menu_entry reference[SDL_PLAYER_ACTION_MAX]; int n;
        assert(sdl_player_action_menu_layout(reference,&n) && n==6);
        for(int device=SDL_STARTUP_DEVICE_DESKTOP;device<=SDL_STARTUP_DEVICE_MOBILE_TOUCH;device++) {
            g_startup_device_class=device;
            assert(sdl_handheld_ui_active()==(device>=SDL_STARTUP_DEVICE_DESKTOP_HANDHELD));
            player_action_menu_entry primary[SDL_PLAYER_ACTION_MAX], secondary[SDL_PLAYER_ACTION_MAX]; int count;
            assert(sdl_player_action_menu_layout(primary,&count) && count==n);
            for(int i=0;i<n;i++) {
                assert(reference[i].center_x==primary[i].center_x && reference[i].center_y==primary[i].center_y);
                assert(reference[i].inner_radius==primary[i].inner_radius && reference[i].outer_radius==primary[i].outer_radius);
                assert(reference[i].rect.x==primary[i].rect.x && reference[i].rect.y==primary[i].rect.y);
                assert(reference[i].rect.w==primary[i].rect.w && reference[i].rect.h==primary[i].rect.h);
            }
            float radius=primary[0].outer_radius;
            assert(radius<=196 && radius-primary[0].inner_radius>=44);
            assert(primary[0].center_x-radius>=0 && primary[0].center_x+radius<=view->rect.w);
            assert(primary[0].center_y-radius>=0 && primary[0].center_y+radius<=view->rect.h);
            assert(sdl_player_action_menu_layout_secondary(1,secondary,&count) && count==2);
            float width=secondary[0].outer_radius-secondary[0].inner_radius;
            assert(width>=42 && width<=72 && secondary[0].rect.w>=38);
            /* At the normal 32px map cell, the Android wheel loses at least
             * 15 percent of its previous radius without shrinking hit sectors. */
            if(h==32) assert(radius<32*6.7f*.85f);
        }
    }
    puts("Shared wheel: 75 platform/cell/edge cases; smaller Android radius and secondary targets PASS");
    return 0;
}
'''
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "wheel.c"
    path.write_text(source, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "wheel.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-UNDEBUG",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(path),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=30)


def main():
    check_wheel()
    aux.HARNESS = handheld(aux.HARNESS)
    aux.CASES = CASES
    aux.OUT = OUT / "menus"
    aux.main()
    overlays.HARNESS = handheld(overlays.HARNESS)
    overlays.OUT = OUT / "overlays"
    overlays.runner.CASES = CASES
    overlays.main()


if __name__ == "__main__":
    main()

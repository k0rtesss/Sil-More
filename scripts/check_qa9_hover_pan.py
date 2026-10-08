#!/usr/bin/env python3
"""Check motion-owned tooltip invalidation with real SDL camera geometry.

--baseline compiles the old tooltip source as a negative control.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
from check_game_control import HARNESS
ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/qa9-hover-pan"
CHECKS = r'''
static void frame(void)
{
    g_state.need_present=true;
    assert(sdl_render_current_window_frame());
}
static void checks(void)
{
    assert(SDL_SetWindowSize(g_state.window,800,576));
    assert(SDL_SyncWindow(g_state.window));
    config.bigger_font=false;
    set_sdl_min_terminal_mode(SDL_MIN_TERMINAL_COMPACT);
    p_ptr->playing=true;p_ptr->is_dead=false;
    p_ptr->py=p_ptr->px=40;p_ptr->cur_map_hgt=p_ptr->cur_map_wid=100;
    p_ptr->chp=p_ptr->mhp=100;
    rp_ptr=&p_info[0];current_character_profile=&c_info[0];
    p_ptr->depth=3;p_ptr->pspeed=2;p_ptr->food=PY_FOOD_FULL;
    character_generated=character_dungeon=true;character_icky=0;inkey_flag=true;
    set_sdl_main_view_scale(2);sdl_apply_config_no_redraw();
    Term_activate(term_screen);Term_clear();Term_fresh();
    for(int y=0;y<100;y++)for(int x=0;x<100;x++){
        cave_feat[y][x]=FEAT_FLOOR;cave_info[y][x]=CAVE_MARK|CAVE_SEEN|CAVE_VIEW;
    }
    for(int x=42;x<56;x++)cave_feat[40][x]=FEAT_OPEN;
    (void)modify_panel(35,35);
    SDL_FRect rect;assert(sdl_player_map_rect(40,42,&rect));
    float x=rect.x+rect.w/2,y=rect.y+rect.h/2;
    int my,mx;assert(sdl_main_view_point_to_map(x,y,&my,&mx));
    assert(my==40 && mx==42);
    sdl_object_tooltip_handle_mouse_motion(x,y);
    assert(g_object_tooltip.active && !g_object_tooltip.touch);
    frame();assert(g_object_tooltip.active); /* Stable pointer/grid keeps hover. */
    assert(modify_panel(35,36));
    assert(sdl_main_view_point_to_map(x,y,&my,&mx) && mx==43);
    frame();assert(!g_object_tooltip.active); /* Old source fails here. */
    sdl_object_tooltip_handle_mouse_motion(x,y);
    assert(g_object_tooltip.active && g_object_tooltip.map_x==43);
    sdl_object_tooltip_begin_persistent();
    assert(sdl_object_tooltip_show_grid(40,42,true));
    sdl_object_tooltip_end_persistent();
    assert(modify_panel(35,37));frame();
    assert(g_object_tooltip.active && g_object_tooltip.touch && g_object_tooltip.persistent);
    /* Direct game-owned grid tips release stale motion ownership even when
     * show_grid takes its unchanged-text fast path. */
    sdl_object_tooltip_handle_mouse_motion(x,y);
    assert(g_object_tooltip.active);
    assert(sdl_object_tooltip_show_grid(g_object_tooltip.map_y,g_object_tooltip.map_x,false));
    assert(modify_panel(35,38));frame();assert(g_object_tooltip.active);
    sdl_object_tooltip_handle_mouse_motion(x,y);
    assert(g_object_tooltip.active);
    g_unified_look_active=true;
    assert(modify_panel(35,39));frame();assert(g_object_tooltip.active);
    g_unified_look_active=false;sdl_object_tooltip_clear();
    sdl_object_tooltip_handle_mouse_motion(x,y);
    g_pointer_aim.active=true;g_pointer_aim.select_mode=true;
    assert(modify_panel(35,40));frame();assert(g_object_tooltip.active);
    g_pointer_aim.active=false;g_pointer_aim.select_mode=false;
    sdl_object_tooltip_clear();
    sdl_object_tooltip_handle_mouse_motion(x,y);assert(g_object_tooltip.active);
    character_icky=1;frame();assert(!g_object_tooltip.active);character_icky=0;
    sdl_object_tooltip_handle_mouse_motion(x,y);assert(g_object_tooltip.active);
    assert(sdl_object_tooltip_show_text_at_cell(COL_MAP,ROW_MAP,1,"Cell tip",false));
    assert(modify_panel(35,41));frame();
    assert(g_object_tooltip.active && g_object_tooltip.term_cell);
    assert(sdl_object_tooltip_show_text_at_rect(&rect,"Screen tip",false));
    assert(modify_panel(35,42));frame();
    assert(g_object_tooltip.active && g_object_tooltip.screen_rect);
    puts("Desktop hover: real camera projection, stable hover, motion refresh, touch pin, direct Look/aim, term/screen tips and context loss PASS.");
}
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    sources = []
    for relative in ("src/sdl/input/sdl-tooltips.c",):
        path = ROOT / relative
        if args.baseline:
            path = OUT / (Path(relative).stem + "-baseline.c")
            path.write_text(subprocess.check_output(["git", "show", "HEAD:" + relative],
                cwd=ROOT, text=True, encoding="utf-8"), encoding="utf-8")
        sources.append(str(path))
    prefix = HARNESS[:HARNESS.index("static cptr make_path")]
    init = HARNESS[HARNESS.index("static cptr make_path"):HARNESS.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "    checks();sdl_quit_hook(NULL);return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith((
        "/src/main.c.obj", "/src/sdl/input/sdl-tooltips.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin",
        "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
        SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *sources,
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as profile:
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
            env=dict(env, CONTROL_TEST_PROFILE=profile), check=True, timeout=30)


if __name__ == "__main__":
    main()

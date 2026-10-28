#!/usr/bin/env python3
"""Keep accepted SDL map scales usable when returning to character creation.

Exercises real pane layout, settings setters, terminal resize and temporary
zoom on both sides of the 50-column compact boundary. Writable state is private.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

import qa9_sdl_fixture as control

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/qa9-startup-scale"

WRAPPERS = r'''
#include <setjmp.h>
static jmp_buf startup_checkpoint;
static bool startup_test_active;
static bool startup_test_portrait;
static int startup_observed_cols, startup_observed_rows;
void __real_quit(cptr message);
void __real_run_mode_activate_pending(void);
bool __wrap_get_sdl_mobile_portrait_mode(void) { return startup_test_portrait; }
void __wrap_quit(cptr message)
{
    if (startup_test_active) {
        assert(message && strstr(message, "main window is too small"));
        longjmp(startup_checkpoint, 2);
    }
    __real_quit(message);
}
void __wrap_run_mode_activate_pending(void)
{
    if (startup_test_active) {
        startup_observed_cols = Term->wid;
        startup_observed_rows = Term->hgt;
        longjmp(startup_checkpoint, 1);
    }
    __real_run_mode_activate_pending();
}
static int check_play_game_start(void)
{
    int result;
    startup_test_active = true;
    result = setjmp(startup_checkpoint);
    if (!result) {
        (void)play_game();
        assert(!"play_game returned before the startup checkpoint");
    }
    startup_test_active = false;
    character_icky = 0;
    return result;
}
'''

CHECKS = r'''
    int width, height, bigger, mode;
    const char *scenario = SDL_getenv("SCALE_CASE");
    assert(scenario && sscanf(scenario, "%dx%d:%d:%d", &width, &height,
        &bigger, &mode) == 4);
    assert(SDL_SetWindowSize(g_state.window, width, height));
    assert(SDL_SyncWindow(g_state.window));
    sdl_store_platform_max_main_view_scales(width, height);
    assert(sdl_min_terminal_rows_for_mode(SDL_MIN_TERMINAL_COMPACT) == 16);
    assert(sdl_min_terminal_rows_for_mode(SDL_MIN_TERMINAL_NORMAL) == 24);
    assert(sdl_default_main_scale_for_screen_size(2268, 1080,
        SDL_MIN_TERMINAL_COMPACT) == 4);
    assert(sdl_default_main_scale_for_screen_size(2268, 1023,
        SDL_MIN_TERMINAL_COMPACT) == 3);
    assert(sdl_default_main_scale_for_screen_size(2268, 1024,
        SDL_MIN_TERMINAL_COMPACT) == 4);
    config.bigger_font = bigger;
    set_sdl_min_terminal_mode(mode);
    config.main_view_scale = 1;
    g_main_view_zoom_scale = 0;
    g_hide_left_panel = false;
    p_ptr->playing = true;
    p_ptr->chp = p_ptr->mhp = 30;
    p_ptr->py = p_ptr->px = 12;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 40;
    cave_m_idx[12][12] = -1;
    character_generated = character_dungeon = false;
    character_icky = 0;
    op_ptr->opt[OPT_hide_supporting_panes_fullscreen] = false;
    turn = 331; playerturn = 33;
    sdl_apply_config_no_redraw();
    assert(sdl_left_panel_pane_layout_enabled());
    int maximum = MIN(get_sdl_max_scale(), get_sdl_platform_max_main_view_scale());
    set_sdl_main_view_scale(maximum);
    sdl_apply_config_no_redraw();
    assert(config.main_view_scale == maximum);
    /* Gameplay may zoom more closely without persisting that smaller grid. */
    int zoom_max = get_sdl_max_main_view_zoom_scale();
    assert(zoom_max >= maximum);
    set_sdl_main_view_zoom_scale(zoom_max);
    sdl_apply_runtime_zoom();
    assert(config.main_view_scale == maximum);
    sdl_reset_main_view_zoom();
    /* This is the title/new-character state after a death or save-and-quit.
     * It has no logical sidebar columns to add to the actual terminal. */
    screen_set_startup_supporting_panes_hidden(true);
    character_generated = character_dungeon = false;
    character_icky = 1;
    sdl_resize_for_current_layout();
    Term_activate(term_screen);
    printf("%s accepted scale %d -> startup %dx%d, minimum %dx%d\n",
        scenario, maximum, Term->wid, Term->hgt,
        sdl_current_min_terminal_cols(), sdl_current_min_terminal_rows());
    fflush(stdout);
    assert(Term->wid >= sdl_current_min_terminal_cols());
    assert(Term->hgt >= sdl_current_min_terminal_rows());
    assert(turn == 331 && playerturn == 33 && p_ptr->chp == 30
        && p_ptr->py == 12 && p_ptr->px == 12);
    if (width == 800 && height == 576 && !bigger
        && mode == SDL_MIN_TERMINAL_COMPACT)
        assert(maximum == 2); /* The exact 50-column boundary stays available. */
    if (width == 768 && !bigger && mode == SDL_MIN_TERMINAL_COMPACT) {
        /* Recover a value saved by the old settings menu as well. */
        sdl_layout_recovery_result recovery;
        config.main_view_scale = 2;
        assert(sdl_recover_layout_for_current_window("QA saved scale", false,
            &recovery));
        assert(recovery.scale_changed && config.main_view_scale == 1);
        sdl_resize_for_current_layout();
        assert(Term->wid >= 50 && Term->hgt >= 16);
    }
    if (width == 2268 && height == 1080 && !bigger
        && mode == SDL_MIN_TERMINAL_COMPACT) {
        sdl_layout_recovery_result recovery;
        assert(maximum == 4); /* Reset may retain the 16-row scale. */
        config.main_view_scale = 4;
        assert(!sdl_recover_layout_for_current_window("QA saved scale", false,
            &recovery));
        assert(!recovery.scale_changed && config.main_view_scale == 4);
        sdl_resize_for_current_layout();
        assert(Term->wid >= 50 && Term->hgt == 16);

        /* Test the actual play_game check, after the same layout transition as
         * a reset/restart, rather than just comparing two copies of a limit. */
        assert(check_play_game_start() == 1);
        assert(startup_observed_cols >= 50 && startup_observed_rows == 16);

        screen_set_startup_supporting_panes_hidden(false);
        ui_reset_transient_state_for_new_session();
        g_main_view_zoom_scale = 0;
        Term_activate(term_screen);
        /* The map is 16 rows high, but the expanded, detached HUD needs the
         * monster's health and alertness on source rows 15 and 16. */
        character_generated = character_dungeon = true;
        p_ptr->playing = true;
        rp_ptr = &p_info[0];
        current_character_profile = &c_info[0];
        p_ptr->health_who = 1;
        mon_list[1] = (monster_type){.r_idx = 1, .ml = true,
            .hp = 5, .maxhp = 10, .alertness = ALERTNESS_UNWARY - 1};
        g_left_panel_pane_expanded = true;
        sdl_left_panel_source_invalidate();
        sdl_view* view = &g_views[PANE_MAIN];
        assert(Term->hgt == 16 && view->rows == 16);
        assert(SCREEN_HGT == 16 && sdl_left_panel_pane_rows_for_view(view) >= 17);
        const term* source = sdl_left_panel_source_term_for_view(view, 20);
        assert(source && source->hgt >= 17);
        assert(sdl_left_panel_source_row_width_for_term(source, 15) > 0);
        assert(sdl_left_panel_source_row_width_for_term(source, 16) > 0);
        sdl_left_panel_metrics panel;
        assert(sdl_left_panel_metrics_for_view(view, &panel));
        assert(panel.panel_rows >= 17 && panel.panel_render_h <= panel.source_h);
        SDL_FRect panel_rect;
        assert(sdl_left_panel_pane_rect_for_metrics(view, &panel, &panel_rect));
        assert(sdl_render_left_panel_pane_from_cells(view, &panel_rect));
        character_generated = character_dungeon = false;

        config.min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;
        assert(Term_resize(50, 16) == 0);
        assert(check_play_game_start() == 1);
        assert(startup_observed_cols == 50 && startup_observed_rows == 16);
        assert(Term_resize(49, 16) == 0);
        assert(check_play_game_start() == 2);
        assert(Term_resize(50, 15) == 0);
        assert(check_play_game_start() == 2);
        config.min_terminal_mode = SDL_MIN_TERMINAL_NORMAL;
        assert(Term_resize(80, 24) == 0);
        assert(check_play_game_start() == 1);
        assert(Term_resize(80, 23) == 0);
        assert(check_play_game_start() == 2);
        assert(Term_resize(79, 24) == 0);
        assert(check_play_game_start() == 2);
        if (SDL_getenv("SCALE_MOBILE_CHECK")) {
            /* The same compact grid rotated has twice as many columns as
             * landscape rows because cells are half as wide as tall. */
            startup_test_portrait = true;
            config.min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;
            assert(Term_resize(32, 16) == 0);
            assert(check_play_game_start() == 1);
            assert(Term_resize(31, 16) == 0);
            assert(check_play_game_start() == 2);
            assert(Term_resize(32, 15) == 0);
            assert(check_play_game_start() == 2);
            startup_test_portrait = false;
        }
    }
    sdl_quit_hook(NULL);
    return 0;
}
'''


def main():
    # Compile the changed state source into this fixture so it tests the
    # working tree even before the shared Windows build has been refreshed.
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    before_main, main = control.HARNESS[:control.HARNESS.index("    int last = ")].split(
        "int main(int argc, char** argv)", 1)
    source.write_text(before_main + WRAPPERS + "\nint main(int argc, char** argv)"
                      + main + CHECKS, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith((
        "/src/main.c.obj", "/src/sdl/core/sdl-state.c.obj",
        "/src/dungeon/dungeon-startup.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / dep) for dep in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
        SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL",
        "-std=c17", "-O0", "-g", "@CMakeFiles/sil-more.dir/includes_C.rsp",
        str(source), str(ROOT / "src/sdl/core/sdl-state.c"),
        str(ROOT / "src/dungeon/dungeon-startup.c"), "@" + str(response),
        "-Wl,--wrap=quit,--wrap=run_mode_activate_pending,--wrap=get_sdl_mobile_portrait_mode",
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    cases = ("768x576:0:1", "800x576:0:1", "1280x768:0:0",
             "1280x768:1:0", "1920x1152:1:0", "2268x1080:0:1")
    for case in cases:
        with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as profile:
            run_env = dict(env, CONTROL_TEST_PROFILE=profile, SCALE_CASE=case)
            subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
                           env=run_env, check=True, timeout=25)
    # Compile the same startup source with the mobile conditional enabled.
    # No mobile runtime is emulated; this exercises its rotated size policy.
    mobile_obj = OUT / "startup-mobile.obj"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-DSIL_IOS",
        "-std=c17", "-O0", "@CMakeFiles/sil-more.dir/includes_C.rsp", "-c",
        str(ROOT / "src/dungeon/dungeon-startup.c"), "-o", str(mobile_obj)],
        cwd=BUILD, env=env, check=True)
    mobile_exe = OUT / "check-mobile-policy.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/sdl/core/sdl-state.c"), str(mobile_obj), "@" + str(response),
        "-Wl,--wrap=quit,--wrap=run_mode_activate_pending,--wrap=get_sdl_mobile_portrait_mode",
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(mobile_exe)],
        cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="mobile-fixture-", dir=OUT) as profile:
        run_env = dict(env, CONTROL_TEST_PROFILE=profile,
                       SCALE_CASE="2268x1080:0:1", SCALE_MOBILE_CHECK="1")
        subprocess.run([str(mobile_exe), "--windowed", "--tiles"], cwd=ROOT,
                       env=run_env, check=True, timeout=25)
    print("Accepted main scales: post-death startup, 16-row HUD, portrait policy, temporary zoom and boundaries PASS.")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Keep accepted SDL map scales usable when returning to character creation.

Exercises real pane layout, settings setters, terminal resize and temporary
zoom on both sides of the 50-column compact boundary. Writable state is private.
"""
from pathlib import Path
import subprocess
import tempfile

import qa9_sdl_fixture as control

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/qa9-startup-scale"

CHECKS = r'''
    int width, height, bigger, mode;
    const char *scenario = SDL_getenv("SCALE_CASE");
    assert(scenario && sscanf(scenario, "%dx%d:%d:%d", &width, &height,
        &bigger, &mode) == 4);
    assert(SDL_SetWindowSize(g_state.window, width, height));
    assert(SDL_SyncWindow(g_state.window));
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
        assert(Term->wid >= 50 && Term->hgt >= 18);
    }
    sdl_quit_hook(NULL);
    return 0;
}
'''


def main():
    control.OUT = OUT
    control.HARNESS = control.HARNESS[:control.HARNESS.index("    int last = ")] + CHECKS
    exe, env = control.build_harness()
    cases = ("768x576:0:1", "800x576:0:1", "1280x768:0:0",
             "1280x768:1:0", "1920x1152:1:0")
    for case in cases:
        with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as profile:
            run_env = dict(env, CONTROL_TEST_PROFILE=profile, SCALE_CASE=case)
            subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
                           env=run_env, check=True, timeout=25)
    print("Accepted main scales: post-death startup, temporary zoom, font sizes and boundary PASS.")


if __name__ == "__main__":
    main()

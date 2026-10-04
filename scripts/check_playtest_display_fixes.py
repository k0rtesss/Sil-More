#!/usr/bin/env python3
"""Check prompt ownership, zoom visibility and Options using CMake objects.

The terminal fixture supplies input and viewport sizes without a desktop window.
All outputs stay in scripts/output; no saves or preferences are opened.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/playtest-display-check"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "support/input.h"
#include "support/screen.h"
#include <assert.h>

/* Report a failed fixture directly instead of opening a Windows CRT dialog. */
#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(1); } } while (0)

static term test_term;
static const char* input;
static int input_pos, prompt_checks;
static int zoom_scale = 2;
static int settings_saves;

void __wrap_metarun_save_persistent_settings(void) { settings_saves++; }
bool __wrap_save_pane_config_to_json(void) { assert(false); return false; }

/* Only the SDL resize/config boundary is simulated. The production zoom
 * action, panel geometry, command parser and prompt scopes are linked below. */
int __wrap_get_sdl_effective_main_view_scale(void) { return zoom_scale; }
int __wrap_get_sdl_min_main_view_zoom_scale(void) { return 2; }
int __wrap_get_sdl_max_main_view_zoom_scale(void) { return 6; }
bool __wrap_set_sdl_main_view_zoom_scale(int value) {
    zoom_scale = value;
    return true;
}
void __wrap_sdl_apply_runtime_zoom(void) {
    assert(Term_resize(400 / zoom_scale, 140 / zoom_scale) == 0);
}

static errr input_hook(int action, int value) {
    (void)value;
    if (action != TERM_XTRA_EVENT) return 0;
    assert(input && input[input_pos]);
    if (memcmp(Term->scr->c[0], "Repeat how", 10) == 0
        || memcmp(Term->scr->c[0], "Command:", 8) == 0) {
        /* Input ownership and foreground rendering must precede the wait. */
        assert(inkey_prompt_input_active());
        assert(screen_command_prompt_active());
        assert(!screen_saved_fullscreen_active());
        assert(!screen_touch_pane_hidden_active());
        prompt_checks++;
    } else assert(!inkey_prompt_input_active());
    Term_keypress(input[input_pos++]);
    return 0;
}

static void keys(const char* value) {
    Term_flush();
    input = value;
    input_pos = 0;
}
static void clean_prompt(void) {
    assert(!inkey_prompt_input_active());
    assert(!screen_command_prompt_active());
    assert(!screen_saved_fullscreen_active());
    assert(!screen_touch_pane_hidden_active());
    assert(character_icky == 0 && turn == 100 && playerturn == 10);
    assert(zoom_scale == 2);
}
static void check_prompts(void) {
    hjkl_movement = angband_keyset = false;
    keys("12\b3\r5");
    Term_keypress('R');
    request_command();
    assert(p_ptr->command_cmd == '5' && p_ptr->command_arg == 13);
    clean_prompt();

    keys("\rQ");
    Term_keypress('R');
    request_command();
    assert(p_ptr->command_cmd == 'Q' && p_ptr->command_arg == 99);
    clean_prompt();

    keys("4\r\033Q");
    Term_keypress('R');
    request_command();
    assert(p_ptr->command_cmd == 'Q' && p_ptr->command_arg == 0);
    clean_prompt();

    angband_keyset = true;
    keys("5\r5");
    Term_keypress('0');
    request_command();
    assert(p_ptr->command_cmd == '5' && p_ptr->command_arg == 5);
    clean_prompt();
    angband_keyset = false;

    char command;
    keys("\033");
    assert(!get_com("Command: ", &command) && command == ESCAPE);
    clean_prompt();
    keys("a");
    assert(get_com("Command: ", &command) && command == 'a');
    clean_prompt();
    assert(prompt_checks >= 12);
    puts("PASS: repeat editing/default/cancel, foreground prompt, balanced scopes and retained layout/zoom.");
}

static void check_zoom(void) {
    assert(modify_panel(20, 20));
    p_ptr->py = p_ptr->wy + SCREEN_HGT - 2;
    p_ptr->px = p_ptr->wx + SCREEN_WID - 2;
    assert(panel_contains(p_ptr->py, p_ptr->px));
    int py = p_ptr->py, px = p_ptr->px;
    assert(sdl_main_screen_set_main_view_scale_target(4));
    assert(panel_contains(py, px));
    assert(p_ptr->py == py && p_ptr->px == px && turn == 100);
    Term_flush();

    /* An already off-player view was deliberately panned. */
    assert(sdl_main_screen_set_main_view_scale_target(2));
    assert(modify_panel(0, 0));
    p_ptr->py = p_ptr->px = 210;
    assert(!panel_contains(p_ptr->py, p_ptr->px));
    int old_h = SCREEN_HGT, old_w = SCREEN_WID;
    assert(sdl_main_screen_set_main_view_scale_target(4));
    assert(p_ptr->wy == old_h / 2 - SCREEN_HGT / 2);
    assert(p_ptr->wx == old_w / 2 - SCREEN_WID / 2);
    assert(!panel_contains(p_ptr->py, p_ptr->px));
    Term_flush();

    /* A Look selection retains its viewport even with a visible player. */
    assert(sdl_main_screen_set_main_view_scale_target(2));
    assert(modify_panel(20, 20));
    p_ptr->py = p_ptr->wy + SCREEN_HGT - 2;
    p_ptr->px = p_ptr->wx + SCREEN_WID - 2;
    old_h = SCREEN_HGT; old_w = SCREEN_WID;
    g_unified_look_active = true;
    character_icky = 1;
    assert(sdl_main_screen_set_main_view_scale_target(4));
    assert(p_ptr->wy == 20 + old_h / 2 - SCREEN_HGT / 2);
    assert(p_ptr->wx == 20 + old_w / 2 - SCREEN_WID / 2);
    g_unified_look_active = false;
    character_icky = 0;
    puts("PASS: zoom keeps the player visible and preserves panned/Look views.");
}

static void check_options(void) {
    int option = option_page[GAMEPLAY_PAGE][0];
    assert(option != OPT_NONE && !option_is_app_persistent(option));
    op_ptr->opt[option] = false;
    keys("\r\033");
    do_cmd_options_aux(GAMEPLAY_PAGE, "Gameplay Options");
    assert(op_ptr->opt[option] && input_pos == 2 && settings_saves == 1);
    clean_prompt();

    keys("\n\033");
    do_cmd_options_aux(GAMEPLAY_PAGE, "Gameplay Options");
    assert(!op_ptr->opt[option] && input_pos == 2 && settings_saves == 2);
    clean_prompt();

    keys("\033");
    do_cmd_options_aux(GAMEPLAY_PAGE, "Gameplay Options");
    assert(!op_ptr->opt[option] && input_pos == 1 && settings_saves == 2);
    clean_prompt();
    puts("PASS: Enter toggles the focused option, Escape exits and only changed values are saved.");
}

int main(int argc, char** argv) {
    assert(argc == 2);
    setbuf(stdout, NULL);
    log_set_level(LOG_ERROR);
    sdl_config_set_defaults(&config);
    assert(term_init(&test_term, 200, 70, 256) == 0);
    test_term.xtra_hook = input_hook;
    Term_activate(&test_term);
    angband_term[0] = &test_term;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 220;
    p_ptr->py = p_ptr->px = 50;
    p_ptr->playing = true;
    character_dungeon = true;
    character_generated = false;
    character_icky = 0;
    turn = 100; playerturn = 10;
    if (streq(argv[1], "prompt")) check_prompts();
    else if (streq(argv[1], "zoom")) check_zoom();
    else if (streq(argv[1], "options")) check_options();
    else assert(false);
    return 0;
}
'''


def main():
    global BUILD, OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=("all", "prompt", "zoom", "options"), default="all")
    parser.add_argument("--portable", action="store_true")
    args = parser.parse_args()
    BUILD = ROOT / ("build-portable" if args.portable else "build-standard")
    OUT = OUT / ("portable" if args.portable else "standard")
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((objects_dir / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
                                  if not p.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *[str(BUILD / "_deps" / d) for d in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")],
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wrapped = ("get_sdl_effective_main_view_scale", "get_sdl_min_main_view_zoom_scale",
               "get_sdl_max_main_view_zoom_scale", "set_sdl_main_view_zoom_scale",
               "sdl_apply_runtime_zoom", "metarun_save_persistent_settings",
               "save_pane_config_to_json")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    *["-Wl,--wrap=" + name for name in wrapped], "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    for case in (("prompt", "zoom", "options") if args.case == "all" else (args.case,)):
        subprocess.run([str(exe), case], cwd=ROOT, env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

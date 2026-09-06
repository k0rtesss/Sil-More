#!/usr/bin/env python3
"""Exercise the real C help paginator and SDL controller dispatch without saves.

Windows: build-incremental.ps1 first. Reuses that CMake build's objects, include
paths and libraries; generates an isolated console harness under scripts/output.
No game installation, user configuration or saved character is opened.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/help-controller-check"

HARNESS = r'''
#include "angband.h"
#include <assert.h>
#include "ui/help.c"
#include "sdl/input/sdl-gamepad.c"

static void expect_key(int expected)
{
    char actual = 0;
    int result = Term_inkey(&actual, false, true);
    if (expected) {
        assert(result == 0);
        assert(actual == expected);
    } else assert(result != 0);
}

static void button(SDL_GamepadButton button, bool down)
{
    SDL_GamepadButtonEvent event = {0};
    event.button = button;
    event.down = down;
    sdl_gamepad_handle_button(&event);
}

static void dpad_event(SDL_GamepadButton button, bool down)
{
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
    event.gbutton.button = button;
    event.gbutton.down = down;
    sdl_handle_event(&g_state, &event);
}

static void expect_move(int expected)
{
    int actual = 0;
    expect_key(UI_MENU_CLICK_WAKE_KEY);
    assert(movement_input_take_legacy_direction(MOVEMENT_INPUT_CONTEXT_DUNGEON, &actual));
    if (actual != expected) {
        fprintf(stderr, "Expected one D-pad move %d, received %d\n", expected, actual);
        abort();
    }
    expect_key(0);
}

static void check_dpad_diagonals(void)
{
    const struct {
        SDL_GamepadButton first, second;
        int diagonal;
    } pairs[] = {
        {SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 9},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_UP, 9},
        {SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_LEFT, 7},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_UP, 7},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 3},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_DOWN, 3},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, 1},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_DOWN, 1},
    };
    const int delays[] = {50, 300};
    character_icky = 0;
    inkey_flag = true;
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
    config.gamepad_enabled = true;
    config.gamepad_use_dpad = true;
    config.gamepad_dpad_diagonal_delay_ms = 300;
    sdl_gamepad_reset_modifiers();
    movement_input_clear_commands();

    /* Cover all diagonals, both press orders and both driver release orders.
     * Age the pending press directly so tests need no real-time sleeps. */
    for (unsigned d = 0; d < N_ELEMENTS(delays); d++) {
        config.gamepad_dpad_diagonal_delay_ms = delays[d];
        for (unsigned p = 0; p < N_ELEMENTS(pairs); p++) {
            for (int release_first = 0; release_first <= 1; release_first++) {
                dpad_event(pairs[p].first, true);
                expect_key(0);
                assert(g_gamepad_state.dpad_pending);
                assert(!sdl_gamepad_flush_pending_dpad(
                    g_gamepad_state.dpad_pending_time + (delays[d] - 1) * 1000000ULL, false));
                if (release_first) dpad_event(pairs[p].first, false);
                g_gamepad_state.dpad_pending_time = SDL_GetTicksNS()
                    - (delays[d] / 2) * 1000000ULL;
                dpad_event(pairs[p].second, true);
                expect_move(pairs[p].diagonal);
                if (!release_first) dpad_event(pairs[p].first, false);
                dpad_event(pairs[p].second, false);
                assert(!sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true));
                expect_key(0);
            }
        }
    }

    /* Repeated and opposing cardinal taps still mean two separate moves. */
    for (int opposite = 0; opposite <= 1; opposite++) {
        SDL_GamepadButton second = opposite ? SDL_GAMEPAD_BUTTON_DPAD_DOWN
                                           : SDL_GAMEPAD_BUTTON_DPAD_UP;
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_UP, true);
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_UP, false);
        dpad_event(second, true);
        expect_move(8);
        dpad_event(second, false);
        assert(sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true));
        expect_move(opposite ? 2 : 8);
    }

    /* A lone tap resolves at the configured deadline, including after release. */
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_UP, true);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_UP, false);
    assert(sdl_gamepad_flush_pending_dpad(
        g_gamepad_state.dpad_pending_time + 300000000ULL, false));
    expect_move(8);

    /* A late perpendicular press must not combine with an expired first tap. */
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_UP, true);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_UP, false);
    g_gamepad_state.dpad_pending_time = SDL_GetTicksNS() - 301000000ULL;
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true);
    expect_move(8);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false);
    assert(sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true));
    expect_move(6);

    /* Modifier changes prevent combining differently modified actions. */
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_UP, true);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_UP, false);
    g_gamepad_state.shift_held = 1;
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true);
    expect_move(8);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false);
    assert(sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true));
    expect_move(6);
    sdl_gamepad_reset_modifiers();
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_NONE);
    inkey_flag = false;
    puts("Dungeon D-pad diagonal regression checks: PASS");
}

static void check_controller(void)
{
    const SDL_GamepadButton buttons[] = {
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH
    };
    const int expected[] = {'\r', ESCAPE, '[', ']', 'h', 'x', 's'};
    const int bindings[] = {'f', '\r', '8', GAMEPAD_BIND_SHIFT, GAMEPAD_BIND_NONE};
    character_icky = 1;
    config.gamepad_enabled = true;
    config.input_ui_mode = SDL_INPUT_UI_MODE_CONTROLLER;
    config.gamepad_use_dpad = false;
    for (unsigned b = 0; b < N_ELEMENTS(bindings); b++) {
        for (int j = 0; j < SDL_GAMEPAD_BUTTON_COUNT; j++)
            config.gamepad_button_bindings[j] = bindings[b];
        for (unsigned j = 0; j < N_ELEMENTS(buttons); j++) {
            button(buttons[j], true);
            expect_key(expected[j]);
            button(buttons[j], false);
            expect_key(0);
        }
        button(SDL_GAMEPAD_BUTTON_DPAD_UP, true);
        expect_key('8');
        button(SDL_GAMEPAD_BUTTON_DPAD_UP, false);
        expect_key(0);
        assert(steamdeck_menu_key('\r', 'a', 'b') == '\r');
        assert(steamdeck_menu_key('[', 'a', 'b') == 'a');
        assert(steamdeck_menu_key(']', 'a', 'b') == 'b');
    }
    /* B must exit the current modal even if an old gameplay row was focused. */
    g_gamepad_context_focus_kind = SDL_CONTROLLER_FOCUS_LEFT_PANEL;
    g_gamepad_context_focus_id = SDL_PANEL_CLICK_INVENTORY;
    button(SDL_GAMEPAD_BUTTON_EAST, true);
    expect_key(ESCAPE);
    assert(g_gamepad_context_focus_kind == SDL_CONTROLLER_FOCUS_NONE);

    /* Trigger remaps cannot submit commands through a native action wheel. */
    character_icky = 0;
    g_player_action_menu.active = true;
    config.gamepad_trigger_bindings[0] = 'q';
    SDL_GamepadAxisEvent axis = {0};
    axis.axis = SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
    axis.value = 32767;
    sdl_gamepad_handle_axis(&axis);
    expect_key(0);
    axis.value = 0;
    sdl_gamepad_handle_axis(&axis);
    expect_key(0);
    g_player_action_menu.active = false;

    /* One stick deflection navigates once and must recenter after the modal. */
    character_icky = 1;
    g_gamepad_state.left_ui_dir = -1;
    axis.axis = SDL_GAMEPAD_AXIS_LEFTY;
    axis.value = -32767;
    sdl_gamepad_handle_axis(&axis);
    expect_key('8');
    sdl_gamepad_handle_axis(&axis);
    expect_key(0);
    character_icky = 0;
    axis.value = -32000;
    sdl_gamepad_handle_axis(&axis);
    expect_key(0);
    axis.value = 0;
    sdl_gamepad_handle_axis(&axis);
    expect_key(0);
    assert(g_gamepad_state.left_ui_dir == -1);

    /* A View chord never moves the player, even if the layout has no targets. */
    sdl_config_set_default_gamepad_bindings(&config);
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
    button(SDL_GAMEPAD_BUTTON_BACK, true);
    button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true);
    button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false);
    button(SDL_GAMEPAD_BUTTON_BACK, false);
    expect_key(0);
    button(SDL_GAMEPAD_BUTTON_BACK, true);
    button(SDL_GAMEPAD_BUTTON_BACK, false);
    expect_key('h');
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_NONE);

    /* Releasing a UI button remapped to Shift must not release another input. */
    character_icky = 1;
    config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_SOUTH] = GAMEPAD_BIND_SHIFT;
    g_gamepad_state.shift_held = 1;
    button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    expect_key('\r');
    button(SDL_GAMEPAD_BUTTON_SOUTH, false);
    assert(g_gamepad_state.shift_held == 1);
    sdl_gamepad_reset_modifiers();
    character_icky = 0;

    /* A held stick is latched when an overlay opens, even without more motion. */
    g_gamepad_state.left_y = -32767;
    sdl_gamepad_prepare_ui_navigation();
    assert(g_gamepad_state.left_ui_dir == GAMEPAD_STICK_DIR_UP);
    axis.axis = SDL_GAMEPAD_AXIS_LEFTY;
    axis.value = -31000;
    sdl_gamepad_handle_axis(&axis);
    expect_key(0);
    axis.value = 0;
    sdl_gamepad_handle_axis(&axis);
    expect_key(0);

    /* The optional right stick moves the aim cursor, not a configured action. */
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_TARGETING);
    g_gamepad_state.right_ui_dir = -1;
    config.gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_RIGHT] = 'q';
    axis.axis = SDL_GAMEPAD_AXIS_RIGHTX;
    axis.value = 32767;
    sdl_gamepad_handle_axis(&axis);
    expect_key('6');
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_NONE);
    puts("Controller event regression checks: PASS");
}

static void check_help(void)
{
    const int widths[] = {32, 50, 79, 80, 81, 120};
    int failures = 0;
    for (int page = 1; page <= HELP_SOURCE_PAGE_COUNT; page++) {
        static char cells[HELP_DOC_MAX_ROWS][HELP_DOC_MAX_COLS];
        int starts[HELP_DOC_MAX_PAGES], ends[HELP_DOC_MAX_PAGES];
        memset(cells, 0, sizeof(cells));
        g_help_doc_ops_n = 0;
        g_help_doc_string_pool_used = 0;
        g_help_record_base_y = 0;
        g_help_record_page_min_y = INT_MAX;
        g_help_record_page_max_y = INT_MIN;
        g_help_record_ops = true;
        show_help_screen_legacy(page, page, HELP_SOURCE_PAGE_COUNT, false);
        g_help_record_ops = false;
        for (int i = 0; i < g_help_doc_ops_n; i++) {
            const help_draw_op_t *op = &g_help_doc_ops[i];
            for (int c = 0; op->text[c]; c++) {
                int x = op->x + c, y = op->y;
                assert(y >= 0 && y < HELP_DOC_MAX_ROWS);
                assert(x >= 0 && x < HELP_DOC_MAX_COLS);
                if (cells[y][x] && cells[y][x] != ' ' && op->text[c] != cells[y][x]) {
                    fprintf(stderr, "Help page %d row %d col %d overwrites '%c' with '%c' in %s\n",
                        page, y, x, cells[y][x], op->text[c], op->text);
                    failures++;
                }
                cells[y][x] = op->text[c];
            }
        }
        for (unsigned w = 0; w < N_ELEMENTS(widths); w++) {
            int rows = help_build_compact_display_rows(widths[w],
                g_help_record_page_max_y + 1);
            int pages = help_dynamic_build_display_pages(24, rows, starts, ends);
            int covered[HELP_DOC_DISPLAY_MAX_ROWS] = {0};
            for (int p = 0; p < pages; p++) {
                assert(ends[p] - starts[p] + 1 <= 21);
                for (int r = starts[p]; r <= ends[p]; r++) covered[r]++;
            }
            for (int r = 0; r < rows; r++) {
                const help_display_row_t *row = &g_help_display_rows[r];
                if (row->has_content) assert(covered[r] == 1);
                for (int s = row->span_start; s < row->span_start + row->span_count; s++)
                    assert(g_help_display_spans[s].x + (int)strlen(g_help_display_spans[s].text) < widths[w]);
            }
        }
    }
    assert(failures == 0);
    {
        bool content[HELP_DOC_MAX_ROWS], headings[HELP_DOC_MAX_ROWS];
        int height = 0;
        help_build_document_ops(&height, content, headings);
        assert(height > 0 && height < HELP_DOC_MAX_ROWS);
        assert(g_help_doc_ops_n < HELP_DOC_MAX_OPS);
        assert(g_help_doc_string_pool_used < HELP_DOC_STRING_POOL_SIZE);
    }
    puts("Help source overlap and pagination checks: PASS (15 topics, 6 widths)");
}

int main(void)
{
    term t;
    maxima limits = {0};
    z_info = &limits;
    setbuf(stdout, NULL);
    assert(SDL_Init(SDL_INIT_EVENTS));
    sdl_config_set_defaults(&config);
    term_init(&t, 80, 24, 256);
    Term_activate(&t);
    term_screen = &t;
    check_dpad_diagonals();
    check_controller();
    check_help();
    term_nuke(&t);
    SDL_Quit();
    return 0;
}
'''

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    # externs.h contains legacy typedefs without an include guard, so compile
    # each source under test in its own translation unit, as the game does.
    help_start = HARNESS.index("static void check_help(void)")
    main_start = HARNESS.index("int main(void)")
    help_source = OUT / "help-check.c"
    help_source.write_text('#include "angband.h"\n#include <assert.h>\n'
        '#include "ui/help.c"\n' + HARNESS[help_start:main_start].replace(
            "static void check_help", "void check_help"), encoding="utf-8")
    source.write_text(HARNESS[:help_start].replace('#include "ui/help.c"', '')
        + "void check_help(void);\n" + HARNESS[main_start:], encoding="utf-8")
    cmake_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    exclude = ("/src/main.c.obj", "/src/ui/help.c.obj", "/src/sdl/input/sdl-gamepad.c.obj")
    objects = [p for p in objects if not p.endswith(exclude)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        str(BUILD / "_deps/SDL"), str(BUILD / "_deps/SDL_ttf"),
        str(BUILD / "_deps/SDL_image"), str(BUILD / "_deps/SDL_mixer"), env["PATH"]])
    compiler = "C:/msys64/mingw64/bin/cc.exe"
    exe = OUT / "check.exe"
    subprocess.run([compiler, "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(help_source),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)

if __name__ == "__main__":
    main()

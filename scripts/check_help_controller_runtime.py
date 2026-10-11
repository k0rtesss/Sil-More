#!/usr/bin/env python3
"""Exercise the real C help paginator and SDL controller dispatch without saves.

Windows: build-incremental.ps1 first. Reuses that CMake build's objects, include
paths and libraries; generates an isolated console harness under scripts/output.
No game installation, user configuration or saved character is opened.
"""
from pathlib import Path
import os
import argparse
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
        if ((unsigned char)actual != (unsigned char)expected)
            fprintf(stderr, "Expected key %d, received %d\n", expected, (unsigned char)actual);
        assert(actual == expected);
    } else {
        if (result == 0)
            fprintf(stderr, "Expected no key, received %d\n", (unsigned char)actual);
        assert(result != 0);
    }
}

bool fixture_capture_is_cancel(bool controller_ui, int modifier, int type, int id);
void fixture_capture_assign_combo(int binding, int modifier, int type, int id);
static void check_capture_cancel(void)
{
    sdl_config_set_default_gamepad_bindings(&config);
    const int east_default = config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_EAST];
    const int bindings[] = { east_default, GAMEPAD_BIND_NONE, GAMEPAD_BIND_SHIFT,
        GAMEPAD_BIND_CTRL, GAMEPAD_BIND_ALT, 'x', ESCAPE };
    config.input_ui_mode = SDL_INPUT_UI_MODE_CONTROLLER;
    g_gamepad_state.pad_count = 1;
    for (unsigned i = 0; i < N_ELEMENTS(bindings); i++) {
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_EAST] = bindings[i];
        assert(sdl_gamepad_capture_begin(true));
        g_gamepad_capture_arm_time = 0;
        SDL_GamepadButtonEvent ev = {0};
        ev.button = SDL_GAMEPAD_BUTTON_EAST; ev.down = true;
        sdl_gamepad_handle_button(&ev);
        int type, id, modifier;
        assert(sdl_gamepad_capture_poll(&type, &id, &modifier));
        assert(type == GAMEPAD_CAPTURE_BUTTON && id == SDL_GAMEPAD_BUTTON_EAST);
        assert(modifier == GAMEPAD_BIND_NONE);
        assert(fixture_capture_is_cancel(true, modifier, type, id));
        sdl_gamepad_capture_cancel();
        assert(!g_gamepad_capture_active && !g_gamepad_capture_ready
            && g_gamepad_capture_modifier == GAMEPAD_BIND_NONE);
        assert(!sdl_gamepad_shift_active() && !sdl_gamepad_ctrl_active()
            && !sdl_gamepad_alt_active());
        assert(config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_EAST] == bindings[i]);
        ev.down = false; sdl_gamepad_handle_button(&ev);
    }
    int type, id, modifier;
    const int modifiers[] = { GAMEPAD_BIND_SHIFT, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_ALT };
    const int east_bases[] = { GAMEPAD_BIND_NONE, GAMEPAD_BIND_SHIFT,
        GAMEPAD_BIND_CTRL, GAMEPAD_BIND_ALT };
    for (unsigned m = 0; m < N_ELEMENTS(modifiers); m++)
    for (unsigned b = 0; b < N_ELEMENTS(east_bases); b++)
    for (int release_first = 0; release_first < 2; release_first++) {
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_EAST] = east_bases[b];
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = modifiers[m];
        assert(sdl_gamepad_capture_begin(true));
        g_gamepad_capture_arm_time = 0;
        SDL_GamepadButtonEvent first = {0}, second = {0};
        first.button = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER; first.down = true;
        second.button = SDL_GAMEPAD_BUTTON_EAST; second.down = true;
        sdl_gamepad_handle_button(&first);
        assert(g_gamepad_capture_modifier == modifiers[m]);
        sdl_gamepad_handle_button(&second);
        assert(sdl_gamepad_capture_poll(&type, &id, &modifier));
        assert(type == GAMEPAD_CAPTURE_BUTTON && id == SDL_GAMEPAD_BUTTON_EAST);
        assert(modifier == modifiers[m]);
        assert(!fixture_capture_is_cancel(true, modifier, type, id));
        fixture_capture_assign_combo('v', modifier, type, id);
        assert(get_sdl_gamepad_combo_binding(modifier, type, id) == 'v');
        first.down = second.down = false;
        sdl_gamepad_handle_button(release_first ? &first : &second);
        sdl_gamepad_handle_button(release_first ? &second : &first);
        assert(!g_gamepad_capture_active && !g_gamepad_capture_ready
            && g_gamepad_capture_modifier == GAMEPAD_BIND_NONE);
        assert(!g_gamepad_state.left_shoulder_down && !g_gamepad_state.shoulder_pending);
        assert(!sdl_gamepad_shift_active() && !sdl_gamepad_ctrl_active()
            && !sdl_gamepad_alt_active());
    }
    config.input_ui_mode = SDL_INPUT_UI_MODE_PLATFORM;
    config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_EAST] = GAMEPAD_BIND_NONE;
    assert(sdl_gamepad_capture_begin(true));
    assert(sdl_gamepad_capture_queue_input(GAMEPAD_CAPTURE_BUTTON, SDL_GAMEPAD_BUTTON_EAST));
    assert(sdl_gamepad_capture_poll(&type, &id, &modifier));
    assert(!fixture_capture_is_cancel(false, modifier, type, id));
    for (unsigned m = 0; m < N_ELEMENTS(modifiers); m++) {
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_EAST] = modifiers[m];
        assert(sdl_gamepad_capture_begin(true));
        assert(!sdl_gamepad_capture_queue_input(GAMEPAD_CAPTURE_BUTTON, SDL_GAMEPAD_BUTTON_EAST));
        assert(g_gamepad_capture_modifier == modifiers[m]);
        sdl_gamepad_capture_cancel();
        assert(g_gamepad_capture_modifier == GAMEPAD_BIND_NONE);
    }
    sdl_config_set_default_gamepad_bindings(&config);
    config.input_ui_mode = SDL_INPUT_UI_MODE_CONTROLLER;
    puts("Controller capture physical B cancel and 24 modified-East capture/release cases: PASS (base modifiers, clean state, combo assignment, keyboard capture retained)");
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

    /* Overlapping presses form a diagonal; two released taps remain separate
     * cardinal moves. Age the pending press so tests need no real-time sleeps. */
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
                if (release_first) {
                    int first = pairs[p].first == SDL_GAMEPAD_BUTTON_DPAD_UP ? 8
                        : pairs[p].first == SDL_GAMEPAD_BUTTON_DPAD_DOWN ? 2
                        : pairs[p].first == SDL_GAMEPAD_BUTTON_DPAD_LEFT ? 4 : 6;
                    expect_move(first);
                } else expect_move(pairs[p].diagonal);
                if (!release_first) dpad_event(pairs[p].first, false);
                dpad_event(pairs[p].second, false);
                if (release_first) {
                    int second = pairs[p].second == SDL_GAMEPAD_BUTTON_DPAD_UP ? 8
                        : pairs[p].second == SDL_GAMEPAD_BUTTON_DPAD_DOWN ? 2
                        : pairs[p].second == SDL_GAMEPAD_BUTTON_DPAD_LEFT ? 4 : 6;
                    assert(sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true));
                    expect_move(second);
                } else assert(!sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true));
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
    log_set_level(LOG_WARN);
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

    /* Enabled movement D-pad input must remain navigation in quantity and
     * numbered-choice overlays. Two Down taps must not type the number 22. */
    config.gamepad_use_dpad = true;
    sdl_question_menu_begin("Drop how many?");
    sdl_question_menu_add_entry(1002, "", "Drop 3", TERM_WHITE);
    sdl_question_menu_finish();
    for (int tap = 0; tap < 2; ++tap) {
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN, true);
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN, false);
        (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true);
        expect_key(UI_MENU_CLICK_WAKE_KEY);
        assert(sdl_question_menu_take_navigation() == 1);
        expect_key(0);
    }
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_UP, true);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_UP, false);
    (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true);
    expect_key(UI_MENU_CLICK_WAKE_KEY);
    assert(sdl_question_menu_take_navigation() == -1);
    expect_key(0);
    /* Horizontal arrows have the same semantics as physical arrows, and an
     * overlapping Down+Right chord resolves to one next navigation action. */
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false);
    (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true);
    expect_key(UI_MENU_CLICK_WAKE_KEY);
    assert(sdl_question_menu_take_navigation() == 1);
    expect_key(0);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN, true);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true);
    expect_key(UI_MENU_CLICK_WAKE_KEY);
    assert(sdl_question_menu_take_navigation() == 1);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN, false);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false);
    assert(!sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(), true));
    expect_key(0);
    SDL_GamepadAxisEvent question_axis = {0};
    g_gamepad_state.left_x = g_gamepad_state.left_y = 0;
    g_gamepad_state.left_ui_dir = -1;
    question_axis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    question_axis.value = 32767;
    sdl_gamepad_handle_axis(&question_axis);
    expect_key(UI_MENU_CLICK_WAKE_KEY);
    assert(sdl_question_menu_take_navigation() == 1);
    question_axis.value = 0;
    sdl_gamepad_handle_axis(&question_axis);
    expect_key(0);
    SDL_Event typed = {0};
    typed.type = SDL_EVENT_KEY_DOWN;
    typed.key.down = true;
    typed.key.key = SDLK_2;
    typed.key.scancode = SDL_SCANCODE_2;
    typed.key.mod = SDL_KMOD_NONE;
    sdl_handle_event(&g_state, &typed);
    expect_key('2');
    assert(sdl_question_menu_take_navigation() == 0);
    expect_key(0);
    sdl_question_menu_clear();
    config.gamepad_use_dpad = false;
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

    /* Zero deadzone still has a neutral center. Releasing the stick must not
     * send Right (which spends attribute points in character creation). Keep
     * cardinal navigation consistent with movement at the deadzone boundary. */
    assert(sdl_gamepad_axis_to_cardinal_dir(0, 0, 0) == -1);
    assert(sdl_gamepad_axis_to_cardinal_dir(12000, 0, 12000) == -1);
    assert(sdl_gamepad_axis_to_cardinal_dir(0, -12000, 12000) == -1);
    assert(sdl_gamepad_axis_to_cardinal_dir(1, 0, 0) == GAMEPAD_STICK_DIR_RIGHT);
    character_icky = 1;
    config.gamepad_deadzone = 0;
    g_gamepad_state.left_x = g_gamepad_state.left_y = 0;
    g_gamepad_state.left_ui_dir = -1;
    axis.axis = SDL_GAMEPAD_AXIS_LEFTY;
    axis.value = 32767;
    sdl_gamepad_handle_axis(&axis);
    expect_key('2');
    axis.value = 0;
    sdl_gamepad_handle_axis(&axis);
    expect_key(0);
    assert(g_gamepad_state.left_ui_dir == -1);
    axis.value = 32767;
    sdl_gamepad_handle_axis(&axis);
    expect_key('2');
    axis.value = 0;
    sdl_gamepad_handle_axis(&axis);
    expect_key(0);
    config.gamepad_deadzone = 12000;

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

static void check_song_controller_owner(void)
{
    sdl_config_set_defaults(&config);
    config.gamepad_enabled=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    character_icky=0;inkey_flag=true;
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
    sdl_gamepad_reset_movement_controls();
    g_gamepad_state.left_trigger_down=false;

    /* A gameplay modifier held before opening Songs must retire normally. */
    SDL_GamepadAxisEvent axis={0};
    axis.axis=SDL_GAMEPAD_AXIS_LEFT_TRIGGER;axis.value=32767;
    sdl_gamepad_handle_axis(&axis);
    assert(sdl_gamepad_shift_active());
    sdl_song_menu_begin("Songs");
    sdl_song_menu_add_entry(0,"","Stop Singing",TERM_WHITE);
    sdl_song_menu_add_entry(1,"","Song of Elbereth",TERM_WHITE);
    sdl_song_menu_set_highlight(0);sdl_song_menu_finish();
    axis.value=0;sdl_gamepad_handle_axis(&axis);
    assert(!sdl_gamepad_shift_active());expect_key(0);
    axis.value=32767;sdl_gamepad_handle_axis(&axis);
    assert(!sdl_gamepad_shift_active());expect_key(0);
    axis.value=0;sdl_gamepad_handle_axis(&axis);expect_key(0);

    const int bindings[]={'f',GAMEPAD_BIND_SHIFT,GAMEPAD_BIND_NONE};
    for(unsigned i=0;i<N_ELEMENTS(bindings);i++) {
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_SOUTH]=bindings[i];
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_EAST]=bindings[i];
        button(SDL_GAMEPAD_BUTTON_SOUTH,true);expect_key('\r');
        button(SDL_GAMEPAD_BUTTON_SOUTH,false);expect_key(0);
        button(SDL_GAMEPAD_BUTTON_EAST,true);expect_key(ESCAPE);
        button(SDL_GAMEPAD_BUTTON_EAST,false);expect_key(0);
        assert(!sdl_gamepad_shift_active());
    }
    const SDL_GamepadButton directions[]={SDL_GAMEPAD_BUTTON_DPAD_DOWN,
        SDL_GAMEPAD_BUTTON_DPAD_UP};
    for(unsigned i=0;i<N_ELEMENTS(directions);i++) {
        dpad_event(directions[i],true);dpad_event(directions[i],false);
        (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true);
        expect_key(i?'8':'2');expect_key(0);
        int direction=0;
        assert(!movement_input_take_legacy_direction(MOVEMENT_INPUT_CONTEXT_DUNGEON,&direction));
    }
    g_gamepad_state.left_ui_dir=-1;
    g_gamepad_state.left_x=g_gamepad_state.left_y=0;
    axis.axis=SDL_GAMEPAD_AXIS_LEFTY;axis.value=32767;
    sdl_gamepad_handle_axis(&axis);expect_key('2');
    sdl_gamepad_handle_axis(&axis);expect_key(0);
    axis.value=0;sdl_gamepad_handle_axis(&axis);expect_key(0);
    g_gamepad_state.right_ui_dir=-1;
    g_gamepad_state.right_x=g_gamepad_state.right_y=0;
    config.gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_RIGHT]='q';
    axis.axis=SDL_GAMEPAD_AXIS_RIGHTX;axis.value=32767;
    sdl_gamepad_handle_axis(&axis);expect_key('6');
    axis.value=0;sdl_gamepad_handle_axis(&axis);expect_key(0);
    sdl_song_menu_clear();sdl_gamepad_reset_movement_controls();
    sdl_config_set_defaults(&config);
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_NONE);
    puts("Native Songs modal owner: remap-safe A/B, D-pad/sticks, held-trigger cleanup and no dungeon movement PASS");
}



static void blocked_axis(int axis,int value)
{
    SDL_Event event={0};event.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.axis=axis;event.gaxis.value=value;
    sdl_handle_event(&g_state,&event);
}

static void check_blocked_controller_state(void)
{
    sdl_config_set_defaults(&config);config.gamepad_enabled=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    character_icky=0;inkey_flag=true;
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
    sdl_gamepad_reset_movement_controls();
    g_gamepad_state.right_trigger_down=false;
    blocked_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);
    assert(sdl_gamepad_ctrl_active());expect_key(0);
    sdl_question_menu_begin("Bash roll");
    sdl_question_menu_add_entry(0,"","Rolling...",TERM_WHITE);
    sdl_question_menu_set_blocking_input(true);sdl_question_menu_finish();
    assert(sdl_question_menu_blocks_input());
    /* Already-held trigger retains its modifier until its physical release. */
    blocked_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,30000);
    assert(sdl_gamepad_ctrl_active());expect_key(0);
    g_gamepad_state.dpad_down=g_gamepad_state.dpad_right=true;
    g_gamepad_state.dpad_dir=3;sdl_gamepad_set_pending_dpad(3);
    g_gamepad_state.stick_movement[0].pending=true;
    g_gamepad_state.stick_movement[0].pending_dir=6;
    sdl_gamepad_set_pending_shoulder(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    g_gamepad_state.confirm_pending=true;
    g_gamepad_state.confirm_pending_button=SDL_GAMEPAD_BUTTON_SOUTH;
    g_gamepad_state.confirm_pending_binding=' ';
    g_gamepad_focus_chord_pending=true;g_gamepad_focus_chord_used=false;
    blocked_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,0);
    expect_key(0);
    assert(!sdl_gamepad_ctrl_active()&&!g_gamepad_state.right_trigger_down);
    assert(!g_gamepad_state.dpad_up&&!g_gamepad_state.dpad_down
        &&!g_gamepad_state.dpad_left&&!g_gamepad_state.dpad_right
        &&!g_gamepad_state.dpad_pending&&!g_gamepad_state.dpad_dir);
    assert(!g_gamepad_state.stick_movement[0].pending
        &&!g_gamepad_state.stick_movement[1].pending);
    assert(!g_gamepad_state.shoulder_pending&&!g_gamepad_state.confirm_pending);
    assert(!g_gamepad_focus_chord_pending&&!g_gamepad_focus_chord_used);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN,false);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,false);
    dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,false);expect_key(0);
    /* Button-bound modifiers retire, but neither release nor a swallowed
     * fresh button/trigger press dispatches its ordinary binding. */
    config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_WEST]=GAMEPAD_BIND_SHIFT;
    g_gamepad_button_modifiers[SDL_GAMEPAD_BUTTON_WEST]=GAMEPAD_BIND_SHIFT;
    sdl_gamepad_apply_modifier(GAMEPAD_BIND_SHIFT,true);
    dpad_event(SDL_GAMEPAD_BUTTON_WEST,false);assert(!sdl_gamepad_shift_active());
    g_gamepad_state.left_shoulder_down=g_gamepad_state.right_shoulder_down=true;
    dpad_event(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,false);
    dpad_event(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,false);
    assert(!g_gamepad_state.left_shoulder_down&&!g_gamepad_state.right_shoulder_down);
    config.gamepad_trigger_bindings[0]='e';g_gamepad_state.left_trigger_down=false;
    blocked_axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER,32767);
    assert(g_gamepad_state.left_trigger_down);expect_key(0);
    dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,true);
    dpad_event(SDL_GAMEPAD_BUTTON_START,true);expect_key(0);
    assert(!g_main_menu_overlay_active&&!g_gamepad_state.confirm_pending);
    blocked_axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER,0);
    dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,false);
    dpad_event(SDL_GAMEPAD_BUTTON_START,false);expect_key(0);
    sdl_question_menu_clear();
    assert(!sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true));
    assert(!sdl_gamepad_flush_pending_sticks(SDL_GetTicksNS(),true));
    assert(!sdl_gamepad_flush_pending_shoulder(SDL_GetTicksNS(),true));
    assert(!sdl_gamepad_flush_pending_confirm(SDL_GetTicksNS()+1000000000ULL));expect_key(0);
    int direction=0;assert(!movement_input_take_legacy_direction(MOVEMENT_INPUT_CONTEXT_DUNGEON,&direction));
    dpad_event(SDL_GAMEPAD_BUTTON_START,true);expect_key(ESCAPE);
    dpad_event(SDL_GAMEPAD_BUTTON_START,false);expect_key(0);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,true);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,false);
    (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true);expect_move(6);
    for(int stick=0;stick<2;stick++) {
        sdl_gamepad_reset_movement_controls();
        g_gamepad_state.left_x=g_gamepad_state.left_y=0;
        g_gamepad_state.right_x=g_gamepad_state.right_y=0;
        g_gamepad_state.left_ui_dir=g_gamepad_state.right_ui_dir=-1;
        config.gamepad_use_left_stick=config.gamepad_use_right_stick=true;
        int axis=stick?SDL_GAMEPAD_AXIS_RIGHTX:SDL_GAMEPAD_AXIS_LEFTX;
        int* bindings=stick?config.gamepad_right_stick_bindings:config.gamepad_left_stick_bindings;
        bindings[GAMEPAD_STICK_DIR_RIGHT]=GAMEPAD_BIND_ALT;
        if(stick)g_gamepad_state.right_dir=GAMEPAD_STICK_DIR_RIGHT;
        else g_gamepad_state.left_bind_dir=GAMEPAD_STICK_DIR_RIGHT;
        g_gamepad_stick_binding_modifier_active[stick]=true;
        sdl_gamepad_apply_modifier(GAMEPAD_BIND_ALT,true);
        sdl_question_menu_begin("Bash roll");
        sdl_question_menu_add_entry(0,"","Rolling...",TERM_WHITE);
        sdl_question_menu_set_blocking_input(true);sdl_question_menu_finish();
        blocked_axis(axis,32767);expect_key(0);
        assert(!sdl_gamepad_alt_active()&&!g_gamepad_stick_binding_modifier_active[stick]);
        assert((stick?g_gamepad_state.right_x:g_gamepad_state.left_x)==32767);
        assert((stick?g_gamepad_state.right_ui_dir:g_gamepad_state.left_ui_dir)==GAMEPAD_STICK_DIR_RIGHT);
        sdl_question_menu_clear();
        blocked_axis(axis,32000);expect_key(0); /* held jitter cannot walk */
        blocked_axis(axis,0);expect_key(0);
        blocked_axis(axis,32767);
        (void)sdl_gamepad_flush_pending_sticks(SDL_GetTicksNS(),true);expect_move(6);
        blocked_axis(axis,0);expect_key(0);
    }
    sdl_gamepad_reset_movement_controls();sdl_config_set_defaults(&config);
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_NONE);
    puts("Blocking roll dispatcher: neutral trigger/button cleanup, pending action suppression, both stick neutral gates and restored Start PASS");
}


static void check_terminal_held_modifiers(void)
{
    sdl_config_set_defaults(&config);config.gamepad_enabled=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;inkey_flag=true;
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
    const int modifiers[]={GAMEPAD_BIND_SHIFT,GAMEPAD_BIND_CTRL,GAMEPAD_BIND_ALT};
    const SDL_GamepadButton directions[]={SDL_GAMEPAD_BUTTON_DPAD_UP,
        SDL_GAMEPAD_BUTTON_DPAD_DOWN,SDL_GAMEPAD_BUTTON_DPAD_LEFT,
        SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
    const int keys[]={'8','2','4','6'};
    int old_font=sdl_menu_font_set_render_context(SDL_MENU_FONT_INVENTORY);
    character_icky=1;
    for(int mask=0;mask<8;mask++) {
        sdl_gamepad_reset_movement_controls();
        for(int i=0;i<3;i++)if(mask&(1<<i))sdl_gamepad_apply_modifier(modifiers[i],true);
        for(unsigned d=0;d<N_ELEMENTS(directions);d++) {
            dpad_event(directions[d],true);dpad_event(directions[d],false);
            (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true);
            expect_key(keys[d]);expect_key(0);
        }
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN,true);
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,true);expect_key('3');
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN,false);
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,false);
        assert(!sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true));expect_key(0);
        SDL_GamepadAxisEvent axis={0};
        for(int stick=0;stick<2;stick++) {
            axis.axis=stick?SDL_GAMEPAD_AXIS_RIGHTY:SDL_GAMEPAD_AXIS_LEFTY;
            axis.value=32767;sdl_gamepad_handle_axis(&axis);expect_key('2');
            sdl_gamepad_handle_axis(&axis);expect_key(0);
            axis.value=0;sdl_gamepad_handle_axis(&axis);expect_key(0);
        }
        dpad_event(SDL_GAMEPAD_BUTTON_EAST,true);expect_key(ESCAPE);
        dpad_event(SDL_GAMEPAD_BUTTON_EAST,false);expect_key(0);
        for(int i=0;i<3;i++)if(mask&(1<<i))sdl_gamepad_apply_modifier(modifiers[i],false);
        movement_input_command command;
        assert(!movement_input_take_command(MOVEMENT_INPUT_CONTEXT_DUNGEON,&command));
    }
    /* Look consumes plain directions in either mode; physical Y still
     * toggles Cursor/Pan through the existing 's' parser action. */
    sdl_menu_font_set_render_context(SDL_MENU_FONT_LOOK);
    sdl_gamepad_send_direction_mods(2,false,true,false);expect_key('2');
    sdl_gamepad_send_direction_mods(2,true,false,false);expect_key('2');expect_key(0);
    button(SDL_GAMEPAD_BUTTON_NORTH,true);expect_key('s');
    button(SDL_GAMEPAD_BUTTON_NORTH,false);expect_key(0);
    sdl_menu_font_set_render_context(old_font);character_icky=0;
    /* Outside saved menus, Shift still runs and Ctrl still interacts; Alt
     * retains its explicit fire macro, including in a direction context. */
    const int contexts[]={MOVEMENT_INPUT_CONTEXT_DUNGEON,
        MOVEMENT_INPUT_CONTEXT_DIRECTION_PROMPT,MOVEMENT_INPUT_CONTEXT_TARGETING};
    for(unsigned c=0;c<N_ELEMENTS(contexts);c++) {
        movement_input_set_active_context(contexts[c]);
        for(int shift=0;shift<2;shift++) {
            sdl_gamepad_send_direction_mods(2,shift,!shift,false);
            expect_key(UI_MENU_CLICK_WAKE_KEY);
            movement_input_command command;
            assert(movement_input_take_command(contexts[c],&command));
            assert(command.action==(shift?MOVEMENT_INPUT_ACTION_RUN_DIR:MOVEMENT_INPUT_ACTION_INTERACT_DIR));
            expect_key(0);
        }
        sdl_gamepad_send_direction_mods(2,false,false,true);
        expect_key('\\');expect_key('f');expect_key('2');expect_key(0);
    }
    sdl_gamepad_reset_movement_controls();sdl_config_set_defaults(&config);
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_NONE);
    puts("Saved terminal pad navigation: all 8 modifier states, D-pad/diagonal/sticks/B, no world queue, Look plain direction/Y mode and live modified routes PASS");
}

static void check_question_held_modifiers(void)
{
    sdl_config_set_defaults(&config);config.gamepad_enabled=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    character_icky=0;inkey_flag=true;
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
    const SDL_GamepadButton directions[]={SDL_GAMEPAD_BUTTON_DPAD_UP,
        SDL_GAMEPAD_BUTTON_DPAD_DOWN,SDL_GAMEPAD_BUTTON_DPAD_LEFT,
        SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
    const int modifiers[]={GAMEPAD_BIND_SHIFT,GAMEPAD_BIND_CTRL,GAMEPAD_BIND_ALT};
    for(int mask=0;mask<8;mask++) {
        sdl_gamepad_reset_movement_controls();
        for(int i=0;i<3;i++)if(mask&(1<<i))sdl_gamepad_apply_modifier(modifiers[i],true);
        sdl_question_menu_begin("Use which digger?");
        sdl_question_menu_add_entry(0,"a)","Shovel",TERM_WHITE);
        sdl_question_menu_add_entry(1,"b)","Mattock",TERM_WHITE);
        sdl_question_menu_set_highlight(0);sdl_question_menu_finish();
        for(unsigned d=0;d<N_ELEMENTS(directions);d++) {
            dpad_event(directions[d],true);dpad_event(directions[d],false);
            (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true);
            expect_key(UI_MENU_CLICK_WAKE_KEY);
            assert(sdl_question_menu_take_navigation()==(d%2?1:-1));expect_key(0);
        }
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN,true);
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,true);
        expect_key(UI_MENU_CLICK_WAKE_KEY);assert(sdl_question_menu_take_navigation()==1);
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN,false);
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,false);
        assert(!sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true));expect_key(0);
        dpad_event(SDL_GAMEPAD_BUTTON_EAST,true);expect_key(ESCAPE);
        dpad_event(SDL_GAMEPAD_BUTTON_EAST,false);expect_key(0);
        sdl_question_menu_clear();
        for(int i=0;i<3;i++)if(mask&(1<<i))sdl_gamepad_apply_modifier(modifiers[i],false);
        (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true);expect_key(0);
        int dir=0;
        assert(!movement_input_take_legacy_direction(MOVEMENT_INPUT_CONTEXT_DUNGEON,&dir));
    }
    /* Actual default RT held across opening the Staff picker uses the same
     * direction snapshot; releasing it later cannot manufacture a command. */
    sdl_gamepad_reset_movement_controls();g_gamepad_state.right_trigger_down=false;
    SDL_GamepadAxisEvent axis={0};axis.axis=SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;axis.value=32767;
    sdl_gamepad_handle_axis(&axis);assert(sdl_gamepad_ctrl_active());
    sdl_question_menu_begin("Activate which staff?");
    sdl_question_menu_add_entry(0,"a)","Imprisonment",TERM_WHITE);
    sdl_question_menu_add_entry(1,"b)","Slumber",TERM_WHITE);
    sdl_question_menu_finish();
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN,true);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN,false);
    (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true);
    expect_key(UI_MENU_CLICK_WAKE_KEY);assert(sdl_question_menu_take_navigation()==1);expect_key(0);
    dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,true);expect_key('\r');
    dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,false);expect_key(0);
    axis.value=0;sdl_gamepad_handle_axis(&axis);assert(!sdl_gamepad_ctrl_active());expect_key(0);
    sdl_question_menu_clear();sdl_gamepad_reset_movement_controls();
    sdl_config_set_defaults(&config);movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_NONE);
    puts("Question picker held modifiers: all 8 states, D-pad/diagonal semantics, actual RT snapshot, A/B and no world queue PASS");
}

static void check_yes_no_controller_owner(void)
{
    sdl_config_set_defaults(&config);
    config.gamepad_enabled=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
    character_icky=0;inkey_flag=true;
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
    sdl_gamepad_reset_movement_controls();
    g_gamepad_state.left_trigger_down=false;
    SDL_Event event={0};
    SDL_GamepadAxisEvent axis={0};

    axis.axis=SDL_GAMEPAD_AXIS_LEFT_TRIGGER;axis.value=32767;
    sdl_gamepad_handle_axis(&axis);assert(sdl_gamepad_shift_active());
    sdl_touch_pane_begin_yes_no_prompt("Travel?");
    axis.value=0;sdl_gamepad_handle_axis(&axis);expect_key(0);
    assert(!sdl_gamepad_shift_active());
    sdl_touch_pane_end_yes_no_prompt();

    const int bindings[]={'f',GAMEPAD_BIND_SHIFT,GAMEPAD_BIND_CTRL,
        GAMEPAD_BIND_ALT,GAMEPAD_BIND_NONE};
    for(unsigned i=0;i<N_ELEMENTS(bindings);i++) {
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_SOUTH]=bindings[i];
        config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_EAST]=bindings[i];
        sdl_touch_pane_begin_yes_no_prompt("Travel?");
        assert(g_touch_pane_yes_no_prompt_hover==SDL_TOUCH_YES_NO_HOVER_NONE);
        dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,true);expect_key('y');
        dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,false);expect_key(0);
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,true);
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,true);expect_key(0);
        assert(g_touch_pane_yes_no_prompt_hover==SDL_TOUCH_YES_NO_HOVER_NO);
        dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,false);expect_key(0);
        dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,true);expect_key('n');
        dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,false);expect_key(0);
        dpad_event(SDL_GAMEPAD_BUTTON_EAST,true);expect_key(ESCAPE);
        dpad_event(SDL_GAMEPAD_BUTTON_EAST,false);expect_key(0);
        assert(!sdl_gamepad_shift_active()&&!sdl_gamepad_ctrl_active()
            &&!sdl_gamepad_alt_active());
        sdl_touch_pane_end_yes_no_prompt();
    }
    sdl_touch_pane_begin_yes_no_prompt("Travel?");
    const SDL_GamepadButton directions[]={SDL_GAMEPAD_BUTTON_DPAD_DOWN,
        SDL_GAMEPAD_BUTTON_DPAD_UP,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
        SDL_GAMEPAD_BUTTON_DPAD_LEFT};
    for(unsigned i=0;i<N_ELEMENTS(directions);i++) {
        dpad_event(directions[i],true);expect_key(0);
        assert(g_touch_pane_yes_no_prompt_hover==(i%2
            ? SDL_TOUCH_YES_NO_HOVER_YES:SDL_TOUCH_YES_NO_HOVER_NO));
        dpad_event(directions[i],false);expect_key(0);
    }
    /* A held gameplay modifier cannot alter native choice navigation. */
    g_gamepad_state.left_trigger_down=false;
    axis.axis=SDL_GAMEPAD_AXIS_LEFT_TRIGGER;axis.value=32767;
    sdl_gamepad_handle_axis(&axis);expect_key(0);
    assert(!sdl_gamepad_shift_active());
    for(int stick=0;stick<2;stick++) {
        for(int dimension=0;dimension<2;dimension++) {
            event.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;
            event.gaxis.axis=(stick?SDL_GAMEPAD_AXIS_RIGHTX:SDL_GAMEPAD_AXIS_LEFTX)+dimension;
            event.gaxis.value=32767;sdl_handle_event(&g_state,&event);expect_key(0);
            assert(g_touch_pane_yes_no_prompt_hover==SDL_TOUCH_YES_NO_HOVER_NO);
            sdl_handle_event(&g_state,&event);expect_key(0);
            event.gaxis.value=-32768;sdl_handle_event(&g_state,&event);expect_key(0);
            assert(g_touch_pane_yes_no_prompt_hover==SDL_TOUCH_YES_NO_HOVER_YES);
            event.gaxis.value=0;sdl_handle_event(&g_state,&event);expect_key(0);
        }
    }
    /* The movement press that opened the popup may release inside it. */
    g_gamepad_state.dpad_down=true;g_gamepad_state.dpad_dir=2;
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_DOWN,false);expect_key(0);
    assert(!g_gamepad_state.dpad_up&&!g_gamepad_state.dpad_down
        &&!g_gamepad_state.dpad_left&&!g_gamepad_state.dpad_right
        &&!g_gamepad_state.dpad_pending);
    /* Closing with a stick still held must require neutral before walking. */
    event.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.axis=SDL_GAMEPAD_AXIS_LEFTX;event.gaxis.value=32767;
    sdl_handle_event(&g_state,&event);expect_key(0);
    dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,true);expect_key('n');
    sdl_touch_pane_end_yes_no_prompt();
    sdl_handle_event(&g_state,&event);expect_key(0);
    event.gaxis.value=32000;sdl_handle_event(&g_state,&event);expect_key(0);
    event.gaxis.value=0;sdl_handle_event(&g_state,&event);expect_key(0);
    dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,false);expect_key(0);
    sdl_touch_pane_begin_yes_no_prompt("Travel?");
    /* An underlying native menu must not steal controller events. */
    g_main_menu_overlay_active=true;
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,true);expect_key(0);
    dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,true);expect_key('n');
    g_main_menu_overlay_active=false;
    sdl_touch_pane_end_yes_no_prompt();
    dpad_event(SDL_GAMEPAD_BUTTON_SOUTH,false);
    dpad_event(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,false);
    axis.value=0;sdl_gamepad_handle_axis(&axis);expect_key(0);
    (void)sdl_gamepad_flush_pending_dpad(SDL_GetTicksNS(),true);
    (void)sdl_gamepad_flush_pending_sticks(SDL_GetTicksNS(),true);expect_key(0);
    int direction=0;
    assert(!movement_input_take_legacy_direction(MOVEMENT_INPUT_CONTEXT_DUNGEON,&direction));
    assert(!sdl_gamepad_shift_active());
    sdl_touch_pane_begin_yes_no_prompt("New question?");
    assert(g_touch_pane_yes_no_prompt_hover==SDL_TOUCH_YES_NO_HOVER_NONE);
    const SDL_Keycode keys[]={SDLK_Y,SDLK_N};
    for(unsigned i=0;i<N_ELEMENTS(keys);i++) {
        memset(&event,0,sizeof(event));event.type=SDL_EVENT_KEY_DOWN;
        event.key.key=keys[i];event.key.mod=SDL_KMOD_NONE;
        sdl_handle_event(&g_state,&event);expect_key(i?'n':'y');
    }
    sdl_touch_pane_end_yes_no_prompt();sdl_gamepad_reset_movement_controls();
    sdl_config_set_defaults(&config);
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_NONE);
    puts("Native Yes/No controller: selected answer, remap-safe A/B, D-pad/both sticks, keyboard y/n, close/release and modal priority PASS");
}


static void check_trigger_calibration_boundaries(void)
{
    sdl_config_set_defaults(&config);config.gamepad_enabled=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;character_icky=0;inkey_flag=true;
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
    g_gamepad_state.pad_count=1;
    const int thresholds[]={0,1,16000,32767};
    for(unsigned t=0;t<N_ELEMENTS(thresholds);t++)for(int trigger=0;trigger<2;trigger++) {
        config.gamepad_trigger_threshold=thresholds[t];
        int effective=MAX(thresholds[t],1);
        sdl_gamepad_reset_movement_controls();
        g_gamepad_state.left_trigger_down=g_gamepad_state.right_trigger_down=false;
        SDL_GamepadAxisEvent axis={0};axis.axis=trigger?SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        axis.value=0;sdl_gamepad_handle_axis(&axis);expect_key(0);
        assert(!sdl_gamepad_shift_active()&&!sdl_gamepad_ctrl_active());
        axis.value=effective-1;sdl_gamepad_handle_axis(&axis);expect_key(0);
        assert(!sdl_gamepad_shift_active()&&!sdl_gamepad_ctrl_active());
        axis.value=effective;sdl_gamepad_handle_axis(&axis);expect_key(0);
        assert(trigger?sdl_gamepad_ctrl_active():sdl_gamepad_shift_active());
        axis.value=32767;sdl_gamepad_handle_axis(&axis);expect_key(0);
        axis.value=0;sdl_gamepad_handle_axis(&axis);expect_key(0);
        assert(!sdl_gamepad_shift_active()&&!sdl_gamepad_ctrl_active());
        assert(!g_gamepad_state.left_trigger_down&&!g_gamepad_state.right_trigger_down);

        /* Capture must not bind a released trigger at accepted threshold 0. */
        assert(sdl_gamepad_capture_begin(false));g_gamepad_capture_arm_time=0;
        int type,id,modifier;
        sdl_gamepad_handle_axis(&axis);assert(!sdl_gamepad_capture_poll(&type,&id,&modifier));
        axis.value=effective-1;sdl_gamepad_handle_axis(&axis);
        assert(!sdl_gamepad_capture_poll(&type,&id,&modifier));
        axis.value=effective;sdl_gamepad_handle_axis(&axis);
        assert(sdl_gamepad_capture_poll(&type,&id,&modifier));
        assert(type==GAMEPAD_CAPTURE_TRIGGER&&id==trigger);
        sdl_gamepad_capture_cancel();axis.value=0;sdl_gamepad_handle_axis(&axis);expect_key(0);

        /* Real blocked-overlay dispatch retires an already-held modifier. */
        axis.value=32767;sdl_gamepad_handle_axis(&axis);
        assert(trigger?sdl_gamepad_ctrl_active():sdl_gamepad_shift_active());
        sdl_question_menu_begin("Calibration roll");
        sdl_question_menu_add_entry(0,"","Rolling...",TERM_WHITE);
        sdl_question_menu_set_blocking_input(true);sdl_question_menu_finish();
        blocked_axis(axis.axis,0);expect_key(0);
        assert(!sdl_gamepad_shift_active()&&!sdl_gamepad_ctrl_active());
        assert(!g_gamepad_state.left_trigger_down&&!g_gamepad_state.right_trigger_down);
        blocked_axis(axis.axis,effective);expect_key(0);
        assert(!sdl_gamepad_shift_active()&&!sdl_gamepad_ctrl_active());
        blocked_axis(axis.axis,0);expect_key(0);
        assert(!g_gamepad_state.left_trigger_down&&!g_gamepad_state.right_trigger_down);
        sdl_question_menu_clear();

        /* At a zoom bound no rendering is required, but the actual minimap
         * handler must agree about neutral, activation and release state. */
        g_minimap.active=true;g_minimap.zoom_step=trigger?MINIMAP_MAX_ZOOM_STEP:0;
        axis.value=0;assert(sdl_minimap_handle_gamepad_axis(&axis));
        assert(!(trigger?g_gamepad_state.right_trigger_down:g_gamepad_state.left_trigger_down));
        axis.value=effective;assert(sdl_minimap_handle_gamepad_axis(&axis));
        assert(trigger?g_gamepad_state.right_trigger_down:g_gamepad_state.left_trigger_down);
        axis.value=0;assert(sdl_minimap_handle_gamepad_axis(&axis));
        assert(!(trigger?g_gamepad_state.right_trigger_down:g_gamepad_state.left_trigger_down));
        g_minimap.active=false;expect_key(0);
        assert(config.gamepad_trigger_threshold==thresholds[t]);
    }
    sdl_gamepad_reset_movement_controls();sdl_config_set_defaults(&config);
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_NONE);
    puts("Trigger accepted calibration boundaries: stored 0/1/16000/32767, both triggers normal/capture/blocked/minimap neutral/press/release PASS");
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
    check_song_controller_owner();
    check_yes_no_controller_owner();
    check_question_held_modifiers();
    check_terminal_held_modifiers();
    check_blocked_controller_state();
    check_trigger_calibration_boundaries();
    check_main_menu_shortcuts();
    check_capture_cancel();
    fixture_check_binding_labels();
    fixture_check_wizard_debug_routing();
    check_help();
    term_nuke(&t);
    SDL_Quit();
    return 0;
}
'''

MENU_CHECK = r'''
#include "angband.h"
#include <assert.h>
bool fixture_execute_choice(int choice);
#define do_cmd_main_menu_execute_choice fixture_execute_choice
#include "sdl/ui/sdl-main-menu.c"
#undef do_cmd_main_menu_execute_choice
static int executed_choice;
bool fixture_execute_choice(int choice) { executed_choice = choice; return false; }
void check_main_menu_shortcuts(void)
{
    const SDL_GamepadButton buttons[] = {
        SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
        SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    };
    const int choices[] = { MAIN_MENU_SAVE_QUIT, MAIN_MENU_HINTS_QUESTS,
        MAIN_MENU_OPTIONS, MAIN_MENU_HALLS_OF_MANDOS, MAIN_MENU_LOG_HISTORY };
    const char *labels[] = { "X", "Y", "Back", "L1", "R1" };
    const int remaps[] = { 'x', 'q', GAMEPAD_BIND_SHIFT, GAMEPAD_BIND_NONE };
    config.gamepad_enabled = true;
    config.input_ui_mode = SDL_INPUT_UI_MODE_CONTROLLER;
    for (unsigned r = 0; r < N_ELEMENTS(remaps); ++r) {
        for (unsigned b = 0; b < N_ELEMENTS(buttons); ++b) {
            config.gamepad_button_bindings[buttons[b]] = remaps[r];
            char label[32];
            main_menu_shortcut_label(choices[b], label, sizeof(label));
            assert(strcmp(label, labels[b]) == 0);
            g_main_menu_overlay_active = true;
            g_main_menu_overlay_highlight = MAIN_MENU_CHARACTER;
            executed_choice = 0;
            SDL_Event event = {0};
            event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            event.gbutton.button = buttons[b];
            event.gbutton.down = true;
            sdl_handle_event(&g_state, &event);
            assert(executed_choice == choices[b]);
            assert(!g_main_menu_overlay_active);
        }
    }
    puts("Native main menu physical shortcut routing: PASS (5 buttons, 4 remaps)");
}
'''

SETTINGS_CHECK = r'''
#include <assert.h>
#include "cmd/ui/cmd-ui-settings.c"
bool fixture_capture_is_cancel(bool controller_ui, int modifier, int type, int id)
{
    return controller_capture_is_cancel(controller_ui, modifier, type, id);
}
void fixture_capture_assign_combo(int binding, int modifier, int type, int id)
{
    controller_assign_combo_binding(binding, modifier, type, id);
}
void fixture_check_binding_labels(void)
{
    char label[128];
    sdl_config_set_default_gamepad_bindings(&config);
    controller_clear_effective_action_bindings('\r');
    set_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_EAST, GAMEPAD_BIND_CTRL);
    set_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_NORTH, '\r');
    set_sdl_gamepad_combo_binding(GAMEPAD_BIND_SHIFT, GAMEPAD_CAPTURE_BUTTON,
        SDL_GAMEPAD_BUTTON_EAST, '\r');
    controller_action_binding_label('\r', label, sizeof(label));
    assert(strcmp(label, "Y (North) +1") == 0);
    set_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_NORTH, GAMEPAD_BIND_NONE);
    controller_action_binding_label('\r', label, sizeof(label));
    assert(strcmp(label, "L2 (Left Trigger) + B (East)") == 0);
    set_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_NORTH, '\r');
    set_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_BACK, '\r');
    controller_action_binding_label('\r', label, sizeof(label));
    assert(strcmp(label, "Y (North) +2") == 0);
    set_sdl_gamepad_combo_binding(GAMEPAD_BIND_SHIFT, GAMEPAD_CAPTURE_BUTTON,
        SDL_GAMEPAD_BUTTON_EAST, GAMEPAD_BIND_NONE);
    controller_action_binding_label('\r', label, sizeof(label));
    assert(strcmp(label, "Y (North) +1") == 0);
    controller_clear_effective_action_bindings('\r');
    controller_action_binding_label('\r', label, sizeof(label));
    assert(strcmp(label, "(unbound)") == 0);
    sdl_config_set_default_gamepad_bindings(&config);
    puts("Controller binding summaries: PASS (direct Y plus L2+B, combo-only, multiple direct, mixed and unbound)");
}
'''

WIZARD_CHECK = r'''
#include "angband.h"
#include <assert.h>
void fixture_debug_options_page(int page, cptr title);
#define do_cmd_options_aux fixture_debug_options_page
#include "wizard2.c"
#undef do_cmd_options_aux
static int captured_options_page = -1;
void fixture_debug_options_page(int page, cptr title)
{
    captured_options_page = page;
    assert(strcmp(title, "Debug Options") == 0);
}
void fixture_check_wizard_debug_routing(void)
{
    do_cmd_debug_execute('O');
    assert(captured_options_page == DEBUG_PAGE);
    assert(captured_options_page != SOUND_PAGE);
    puts("Wizard Debug Options routes to named debug page, not Sound: PASS");
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--yes-no-baseline", action="store_true",
        help="Disable native Yes/No navigation to reproduce keypad-digit leakage")
    parser.add_argument("--question-modifier-baseline", action="store_true",
        help="Restore unmodified-only question navigation to reproduce held-trigger bypass")
    parser.add_argument("--blocked-state-baseline", action="store_true",
        help="Drop blocked gamepad events without retiring their runtime state")
    parser.add_argument("--terminal-modifier-baseline", action="store_true",
        help="Restore modified keypad filtering inside saved terminal menus")
    parser.add_argument("--trigger-zero-baseline", action="store_true",
        help="Restore threshold-zero neutral-as-pressed comparison")
    args = parser.parse_args()
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
    harness = HARNESS[:help_start]
    if args.yes_no_baseline or args.question_modifier_baseline or args.terminal_modifier_baseline or args.trigger_zero_baseline:
        gamepad = (ROOT / "src/sdl/input/sdl-gamepad.c").read_text()
        if args.yes_no_baseline:
            start = gamepad.index("static bool sdl_gamepad_yes_no_navigate(int navigation)")
            end = gamepad.index("\nvoid sdl_gamepad_send_direction_mods", start)
            gamepad = gamepad[:start] + "static bool sdl_gamepad_yes_no_navigate(int navigation) { return false; }\n" + gamepad[end:]
        if args.question_modifier_baseline:
            gamepad = gamepad.replace(
                "if (navigation && sdl_question_menu_queue_navigation(navigation))",
                "if (!shift && !ctrl && !alt && navigation && sdl_question_menu_queue_navigation(navigation))", 1)
        if args.terminal_modifier_baseline:
            start = gamepad.index("    /* Saved terminal menus own plain directional navigation")
            end = gamepad.index("    if (!shift && !ctrl && !alt)", start)
            gamepad = gamepad[:start] + gamepad[end:]
        if args.trigger_zero_baseline:
            gamepad = gamepad.replace("MAX(config.gamepad_trigger_threshold, 1)",
                "MAX(config.gamepad_trigger_threshold, 0)")
        (OUT / "yes-no-baseline-gamepad.c").write_text(gamepad, encoding="utf-8")
        harness = harness.replace('#include "sdl/input/sdl-gamepad.c"',
            '#include "yes-no-baseline-gamepad.c"')
    source.write_text(harness.replace('#include "ui/help.c"', '')
        + "void check_help(void);\nvoid check_main_menu_shortcuts(void);\nvoid fixture_check_binding_labels(void);\nvoid fixture_check_wizard_debug_routing(void);\n" + HARNESS[main_start:], encoding="utf-8")
    menu_source = OUT / "menu-check.c"
    menu_source.write_text(MENU_CHECK, encoding="utf-8")
    settings_source = OUT / "settings-check.c"
    settings_source.write_text(SETTINGS_CHECK, encoding="utf-8")
    wizard_source = OUT / "wizard-check.c"
    wizard_source.write_text(WIZARD_CHECK, encoding="utf-8")
    events_source = ROOT / "src/sdl/core/sdl-events.c"
    if args.blocked_state_baseline:
        events = events_source.read_text()
        assert events.count("sdl_gamepad_handle_blocked_event(ev);") == 1
        events_source = OUT / "blocked-state-baseline-events.c"
        events_source.write_text(events.replace("sdl_gamepad_handle_blocked_event(ev);", ""))
    cmake_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    exclude = ("/src/main.c.obj", "/src/ui/help.c.obj", "/src/sdl/input/sdl-gamepad.c.obj",
               "/src/sdl/ui/sdl-main-menu.c.obj", "/src/cmd/ui/cmd-ui-settings.c.obj",
               "/src/wizard2.c.obj", "/src/sdl/core/sdl-events.c.obj", "/src/sdl/input/sdl-screen-pointer.c.obj")
    objects = [p for p in objects if not p.endswith(exclude)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        str(BUILD / "_deps/SDL"), str(BUILD / "_deps/SDL_ttf"),
        str(BUILD / "_deps/SDL_image"), str(BUILD / "_deps/SDL_mixer"),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    compiler = "C:/msys64/mingw64/bin/cc.exe"
    exe = OUT / "check.exe"
    subprocess.run([compiler, "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(help_source), str(menu_source), str(settings_source), str(wizard_source), str(events_source), str(ROOT / "src/sdl/input/sdl-screen-pointer.c"),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)

if __name__ == "__main__":
    main()

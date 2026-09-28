#!/usr/bin/env python3
"""Exercise popup mouse dispatch and command parsing using the built SDL objects.

Run after build-incremental.ps1. Uses a dummy SDL window and isolated outputs;
does not load player saves or configuration.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/context-popup-check"

HARNESS = r'''
#include "angband.h"
#include <assert.h>
#include "sdl/ui/sdl-question-menu.c"

static term test_term;

static void popup(int choice, const char* label)
{
    Term_flush();
    inkey_next_set(NULL);
    sdl_question_menu_begin("Context action");
    sdl_question_menu_add_button(choice, label, TERM_L_WHITE);
    sdl_question_menu_finish();
    sdl_question_menu_set_context_hint();
}

static void mouse_button(int button)
{
    sdl_question_menu_layout_info layout;
    SDL_Event event = {0};
    assert(sdl_question_menu_layout(&layout));
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID(g_state.window);
    event.button.which = 1;
    event.button.button = button;
    event.button.x = layout.buttons[0].x + layout.buttons[0].w / 2;
    event.button.y = layout.buttons[0].y + layout.buttons[0].h / 2;
    sdl_handle_event(&g_state, &event);
}

static void expect_command(int command, bool direction)
{
    char key;
    request_command();
    assert(p_ptr->command_cmd == command);
    if (direction)
    {
        assert(Term_inkey(&key, false, true) == 0);
        assert(key == '5');
    }
    assert(Term_inkey(&key, false, true) != 0);
    assert(!sdl_question_menu_context_hint_active());
}

int main(void)
{
    const char* labels[] = {"Go Down", "Go Up", "Smith", "Harness"};
    const int direct[] = {'x', 'g', CMD_CONTEXT_FLOOR_ACTION};
    setbuf(stdout, NULL);
    log_set_level(LOG_WARN);
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS));
    assert(TTF_Init());
    sdl_config_set_defaults(&config);
    config.mouse_enabled = true;
    g_state.window = SDL_CreateWindow("Popup input check", 800, 600,
        SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, NULL);
    assert(g_state.renderer);
    g_pane_rects[PANE_DESCRIPTION] = (SDL_Rect){0, 0, 800, 600};
    assert(term_init(&test_term, 80, 24, 256) == 0);
    Term_activate(&test_term);
    angband_term[0] = &test_term;
    character_dungeon = true;
    p_ptr->playing = true;

    /* Selected actions must work even with hostile keyboard remaps. */
    for (int mode = 0; mode < KEYMAP_MODES; mode++)
    {
        keymap_act[mode][' '] = "Q";
        keymap_act[mode]['/'] = "Q";
        keymap_act[mode]['x'] = "Q";
        keymap_act[mode]['g'] = "Q";
    }
    for (int i = 0; i < 4; i++)
    {
        hjkl_movement = (i & 1) != 0;
        angband_keyset = (i & 2) != 0;
        for (int j = 0; j < 4; j++)
        {
            popup(' ', labels[j]);
            inkey_flag = true;
            mouse_button(SDL_BUTTON_LEFT);
            expect_command('/', true);
        }
        for (int j = 0; j < 3; j++)
        {
            popup(direct[j], "Item action");
            inkey_flag = true;
            mouse_button(SDL_BUTTON_LEFT);
            expect_command(direct[j], false);
        }
    }
    popup(' ', "Go Down");
    inkey_flag = true;
    mouse_button(SDL_BUTTON_RIGHT);
    assert(sdl_question_menu_context_hint_active());
    char key;
    assert(Term_inkey(&key, false, true) != 0);
    assert(!sdl_question_menu_activate_context_choice('Q'));
    assert(sdl_question_menu_activate_context_choice(' '));
    expect_command('/', true);
    popup('g', "Pack");
    inkey_flag = false;
    mouse_button(SDL_BUTTON_LEFT);
    assert(sdl_question_menu_context_hint_active());
    assert(!sdl_question_menu_activate_context_choice('g'));
    assert(Term_inkey(&key, false, true) != 0);
    sdl_question_menu_layout_info preview_layout;
    assert(sdl_question_menu_layout(&preview_layout));
    assert(sdl_question_menu_handle_pointer(
        preview_layout.suppress_rect.x + preview_layout.suppress_rect.w / 2,
        preview_layout.suppress_rect.y + preview_layout.suppress_rect.h / 2,
        UI_MENU_CLICK_PRIMARY));
    assert(Term_inkey(&key, false, true) != 0);
    assert(sdl_question_menu_context_hint_active());
    inkey_flag = true;
    character_icky = 1;
    assert(!sdl_question_menu_activate_context_choice('g'));
    character_icky = 0;
    assert(sdl_question_menu_activate_context_choice('g'));
    expect_command('g', false);
    popup('g', "Pack");
    sdl_question_menu_set_anchor_grid(p_ptr->py, p_ptr->px);
    p_ptr->px++;
    inkey_flag = true;
    assert(!sdl_question_menu_activate_context_choice('g'));
    assert(!sdl_question_menu_layout(&preview_layout));
    assert(!sdl_question_menu_context_hint_active());
    assert(Term_inkey(&key, false, true) != 0);
    puts("PASS: early hint rejects action/suppression input until command wait;");
    puts("      modal input and forced movement cannot activate stale hints.");
    puts("PASS: SDL mouse events and command parser, four keysets, remap isolation,");
    puts("      interact-here direction, item actions, secondary click and controller activation.");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    exclude = ("/src/main.c.obj", "/src/sdl/ui/sdl-question-menu.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
                                  if not p.endswith(exclude)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        *[str(BUILD / "_deps" / name) for name in
          ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")], env["PATH"]])
    env["SDL_VIDEO_DRIVER"] = "dummy"
    env["SDL_RENDER_DRIVER"] = "software"
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
        "-O0", "-g", "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

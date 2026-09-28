#!/usr/bin/env python3
"""Test desktop startup decisions, real SDL prompt events and config persistence.

Build first with build-incremental.ps1. Uses the CMake objects/toolchain and an
isolated output folder; does not read or change player config or saves.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/desktop-input-check"

HARNESS = r'''
#include "angband.h"
#include <assert.h>
#define sdl_prompt_desktop_startup_input_device test_prompt
#include "sdl/core/sdl-init.c"
#undef sdl_prompt_desktop_startup_input_device
sdl_startup_device_class sdl_prompt_desktop_startup_input_device(bool* remember);
static int prompts;
static bool answer_controller, answer_remember;
sdl_startup_device_class test_prompt(bool* remember)
{
    prompts++;
    *remember = answer_remember;
    return answer_controller ? SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER
                             : SDL_STARTUP_DEVICE_DESKTOP;
}
static void setup(int mode, int pads)
{
    sdl_config_set_defaults(&config);
    config.input_ui_mode = mode;
    g_gamepad_state.pad_count = pads;
    g_gamepad_auto_ui = pads > 0;
    prompts = 0;
    answer_controller = answer_remember = false;
}
static void expect_mode(int mode, int expected_prompts)
{
    sdl_check_desktop_startup_input();
    assert(config.input_ui_mode == mode);
    assert(prompts == expected_prompts);
    assert(steamdeck_controls_active() == (mode == SDL_INPUT_UI_MODE_CONTROLLER));
    assert(g_startup_device_class == (mode == SDL_INPUT_UI_MODE_CONTROLLER
        ? SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER : SDL_STARTUP_DEVICE_DESKTOP));
}
static void roundtrip(void)
{
    const char* path = "scripts/output/desktop-input-check/config.json";
    struct sdl_config_load_info info = {0};
    assert(sdl_config_save(path, &config, NULL, 0));
    sdl_config_set_defaults(&config);
    assert(sdl_config_load(path, &config, NULL, 0, &info) == SDL_CONFIG_LOAD_OK);
}
static void decisions(void)
{
    const int keyboard = SDL_INPUT_UI_MODE_PLATFORM;
    const int controller = SDL_INPUT_UI_MODE_CONTROLLER;
    setup(keyboard, 0); config.mouse_enabled = false;
    expect_mode(keyboard, 0); assert(!config.mouse_enabled);
    setup(controller, 0); config.mouse_enabled = false;
    expect_mode(keyboard, 0); assert(config.mouse_enabled);
    setup(controller, 1); expect_mode(controller, 0);
    setup(keyboard, 1); expect_mode(keyboard, 1);
    roundtrip(); expect_mode(keyboard, 2); /* Unchecked: ask on next launch. */
    setup(SDL_INPUT_UI_MODE_AUTO, 1); expect_mode(keyboard, 1);
    setup(keyboard, 1); answer_controller = true;
    expect_mode(controller, 1); roundtrip(); expect_mode(controller, 1);
    setup(controller, 1); config.gamepad_enabled = false;
    answer_controller = true; expect_mode(controller, 1);
    assert(config.gamepad_enabled);
    setup(keyboard, 1); answer_remember = true;
    expect_mode(keyboard, 1); roundtrip(); expect_mode(keyboard, 1);
    assert(config.desktop_input_choice == keyboard);
    setup(keyboard, 1); answer_remember = answer_controller = true;
    expect_mode(controller, 1); roundtrip(); expect_mode(controller, 1);
    g_gamepad_state.pad_count = 0; expect_mode(keyboard, 1);
    roundtrip(); assert(config.desktop_input_choice == controller);
    g_gamepad_state.pad_count = 1; expect_mode(controller, 1);
    set_sdl_input_ui_mode(keyboard);
    assert(config.desktop_input_choice == SDL_INPUT_UI_MODE_AUTO);
    answer_controller = answer_remember = false; expect_mode(keyboard, 2);
    puts("Startup cases, repeated launches, remembered choices and JSON roundtrip: PASS");
}
static void key(SDL_Keycode keycode)
{
    SDL_Event event = {0}; event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = keycode; assert(SDL_PushEvent(&event));
}
static void button(Uint8 which)
{
    SDL_Event event = {0}; event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    event.gbutton.button = which; assert(SDL_PushEvent(&event));
}
static void click(float x, float y)
{
    SDL_Event event = {0}; event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x; event.button.y = y; assert(SDL_PushEvent(&event));
}
static void dialog(void)
{
    bool remember;
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.name = "Startup test controller";
    g_gamepad_state.pad_count = 0;
    SDL_JoystickID pad = SDL_AttachVirtualJoystick(&desc);
    assert(pad);
    sdl_gamepad_init(); assert(g_gamepad_state.pad_count > 0);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    key(SDLK_DOWN); key(SDLK_SPACE); key(SDLK_DOWN); key(SDLK_RETURN);
    assert(sdl_prompt_desktop_startup_input_device(&remember)
        == SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER);
    assert(remember);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    button(SDL_GAMEPAD_BUTTON_DPAD_UP); button(SDL_GAMEPAD_BUTTON_SOUTH);
    assert(sdl_prompt_desktop_startup_input_device(&remember)
        == SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER);
    assert(!remember);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    click(50, 220); click(50, 160);
    assert(sdl_prompt_desktop_startup_input_device(&remember)
        == SDL_STARTUP_DEVICE_DESKTOP);
    assert(remember);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    key(SDLK_DOWN); key(SDLK_SPACE); key(SDLK_ESCAPE);
    assert(sdl_prompt_desktop_startup_input_device(&remember)
        == SDL_STARTUP_DEVICE_DESKTOP);
    assert(!remember);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    assert(SDL_DetachVirtualJoystick(pad));
    assert(sdl_prompt_desktop_startup_input_device(&remember)
        == SDL_STARTUP_DEVICE_DESKTOP);
    assert(!remember); assert(g_gamepad_state.pad_count == 0);
    puts("Real SDL dialog: keyboard, mouse, controller, checkbox, cancel and unplug: PASS");
}
int main(void)
{
    setbuf(stdout, NULL);
    log_set_level(LOG_WARN);
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD));
    assert(TTF_Init());
    decisions();
    sdl_config_set_defaults(&config);
    dialog();
    TTF_Quit(); SDL_Quit();
    return 0;
}
'''

LAYOUT = r'''#include <assert.h>

#include "sdl/main-sdl-private.h"
static bool test_present(SDL_Renderer* renderer);
#define SDL_RenderPresent test_present
#include "sdl/core/sdl-layout.c"
#undef SDL_RenderPresent
static bool test_present(SDL_Renderer* renderer)
{
    static int frames;
    if (frames == 0 || frames == 3) {
        SDL_Surface* image = SDL_RenderReadPixels(renderer, NULL);
        assert(image);
        assert(SDL_SaveBMP(image, frames == 0
            ? "scripts/output/desktop-input-check/prompt.bmp"
            : "scripts/output/desktop-input-check/remembered.bmp"));
        SDL_DestroySurface(image);
    }
    frames++;
    return SDL_RenderPresent(renderer);
}
'''

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    layout_source = OUT / "layout-check.c"
    layout_source.write_text(LAYOUT, encoding="utf-8")
    cmake_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    exclude = ("/src/main.c.obj", "/src/sdl/core/sdl-init.c.obj", "/src/sdl/core/sdl-layout.c.obj")
    objects = [p for p in objects if not p.endswith(exclude)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        str(BUILD / "_deps/SDL"), str(BUILD / "_deps/SDL_ttf"),
        str(BUILD / "_deps/SDL_image"), str(BUILD / "_deps/SDL_mixer"), env["PATH"]])
    env["SDL_VIDEO_DRIVER"] = "dummy"
    env["SDL_RENDER_DRIVER"] = "software"
    compiler = "C:/msys64/mingw64/bin/cc.exe"
    exe = OUT / "check.exe"
    subprocess.run([compiler, "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(layout_source),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=30)

if __name__ == "__main__":
    main()

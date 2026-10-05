#!/usr/bin/env python3
"""Check mobile text-session ownership, including tapping a visible keyboard.

Compile the production session helpers against an instrumented SDL keyboard
interface. Android IME appearance and focus ordering still require device replay.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/mobile-text-input-check"

HARNESS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define SDL_PLATFORM_ANDROID 1
#define SDL_HINT_ENABLE_SCREEN_KEYBOARD "screen-keyboard"
typedef struct SDL_Window { int unused; } SDL_Window;
static struct { SDL_Window *window; } g_state;
static bool active, visible;
static int starts, stops;
static bool SDL_SetHint(const char *key, const char *value)
{ (void)key; (void)value; return true; }
static bool SDL_TextInputActive(SDL_Window *window)
{ assert(window); return active; }
static bool SDL_ScreenKeyboardShown(SDL_Window *window)
{ assert(window); return visible; }
static bool SDL_StartTextInput(SDL_Window *window)
{ assert(window); active = true; starts++; return true; }
static bool SDL_StopTextInput(SDL_Window *window)
{ assert(window); active = visible = false; stops++; return true; }

__IMPLEMENTATION__

int main(void)
{
    SDL_Window window = {0};
    g_state.window = &window;
    sdl_text_input_reopen();
    assert(starts == 0 && stops == 0);

    sdl_text_input_begin();
    sdl_text_input_begin();
    assert(active && starts == 1 && stops == 0);

    /* A visible editor must retain its connection/focus on repeated taps. */
    visible = true;
    sdl_text_input_reopen();
    sdl_text_input_reopen();
    assert(active && visible && starts == 1 && stops == 0);

    /* The IME may be dismissed while SDL still owns the text session. */
    visible = false;
    sdl_text_input_reopen();
    assert(active && starts == 2 && stops == 1);

    /* Android Back can stop SDL input independently of the prompt depth. */
    active = false;
    sdl_text_input_reopen();
    assert(active && starts == 3 && stops == 1);

    sdl_text_input_end();
    assert(active && stops == 1);
    sdl_text_input_end();
    assert(!active && stops == 2);
    sdl_text_input_end();
    assert(stops == 2);
    sdl_text_input_reopen();
    assert(starts == 3 && stops == 2);

    puts("Mobile text input: visible IME tap, dismissed/Back reopen, nested ownership and final cleanup: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    events = (ROOT / "src/sdl/core/sdl-events.c").read_text(encoding="utf-8-sig")
    start = events.index("static unsigned int g_sdl_text_input_depth")
    implementation = events[start:]
    source = OUT / "check.c"
    source.write_text(HARNESS.replace("__IMPLEMENTATION__", implementation),
                      encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra", "-O0",
                    str(source), "-o", str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

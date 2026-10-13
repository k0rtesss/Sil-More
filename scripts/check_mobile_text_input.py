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
#include <stddef.h>

#define SDL_PLATFORM_ANDROID 1
#define SDL_HINT_ENABLE_SCREEN_KEYBOARD "screen-keyboard"
typedef struct SDL_Window { int unused; } SDL_Window;
static struct { SDL_Window *window; bool need_present; } g_state;
static bool active, visible;
static int starts, stops;
static bool controller_keyboard;
static int keyboard_begins, keyboard_ends;
static void sdl_controller_keyboard_begin(void) { keyboard_begins++; }
static void sdl_controller_keyboard_end(void) { keyboard_ends++; }
static bool sdl_controller_keyboard_active(void) { return controller_keyboard; }
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

__FRAME_IMPLEMENTATION__

int main(void)
{
    SDL_Window window = {0};
    g_state.window = &window;
    sdl_text_input_reopen();
    assert(starts == 0 && stops == 0);

    sdl_text_input_begin();
    sdl_text_input_begin();
    assert(active && starts == 1 && stops == 0);
    assert(keyboard_begins == 1 && keyboard_ends == 0);

    /* Tapping the in-game controller keyboard must not reopen the mobile IME. */
    controller_keyboard = true;
    sdl_text_input_reopen();
    assert(starts == 1 && stops == 0);
    controller_keyboard = false;

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
    assert(keyboard_ends == 1);
    sdl_text_input_end();
    assert(stops == 2);
    sdl_text_input_reopen();
    assert(starts == 3 && stops == 2);

    check_frame_notifications();

    puts("Mobile text input: IME tap/Back, nested ownership, queued frame redraw, notification-only events: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    events = (ROOT / "src/sdl/core/sdl-events.c").read_text(encoding="utf-8-sig")
    start = events.index("static unsigned int g_sdl_text_input_depth")
    implementation = events[start:]
    layout = (ROOT / "src/sdl/core/sdl-layout.c").read_text(encoding="utf-8-sig")
    frame_start = layout.index("static void SDLCALL sdl_text_input_frame_redraw")
    frame = layout[frame_start:layout.index("#endif", frame_start)]
    event_start = events.index("    if (ev->type == SDL_EVENT_SCREEN_KEYBOARD_SHOWN")
    event_end = events.index("    sdl_normalize_event_to_render_coords", event_start)
    frame_test = r'''
#define SDLCALL
#define JNIEXPORT
#define JNICALL
#define SDL_INIT_EVENTS 1
typedef void JNIEnv;
typedef void* jclass;
static bool initialized;
static void (*queued_frame_callback)(void*);
static unsigned SDL_WasInit(unsigned flags) { return initialized?flags:0; }
static bool SDL_RunOnMainThread(void (*callback)(void*),void *data,bool wait)
{ assert(!data&&!wait&&!queued_frame_callback);queued_frame_callback=callback;return true; }
''' + frame + r'''
enum {SDL_EVENT_SCREEN_KEYBOARD_SHOWN=1,SDL_EVENT_SCREEN_KEYBOARD_HIDDEN=2,ESC_KEY=3};
typedef struct {int type;} SDL_Event;
static int commands;
static void frame_event(const SDL_Event *ev)
{
''' + events[event_start:event_end] + r'''
    commands++;
}
static void check_frame_notifications(void)
{
    g_state.need_present=false;
    Java_com_silqh_silmore_SilMoreActivity_onTextInputVisibleFrameChanged(NULL,NULL);
    assert(!queued_frame_callback&&!g_state.need_present);
    initialized=true;active=true;
    Java_com_silqh_silmore_SilMoreActivity_onTextInputVisibleFrameChanged(NULL,NULL);
    assert(queued_frame_callback&&!g_state.need_present);
    queued_frame_callback(NULL);queued_frame_callback=NULL;
    assert(g_state.need_present);
    active=false;g_state.need_present=false;
    sdl_text_input_frame_redraw(NULL);assert(!g_state.need_present);
    for(int type=1;type<=2;type++) {
        SDL_Event ev={type};g_state.need_present=false;frame_event(&ev);
        assert(g_state.need_present&&!commands);
    }
    SDL_Event back={ESC_KEY};frame_event(&back);assert(commands==1);
}
'''
    source = OUT / "check.c"
    source.write_text(HARNESS.replace("__IMPLEMENTATION__", implementation)
                      .replace("__FRAME_IMPLEMENTATION__",frame_test),
                      encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra", "-O0",
                    str(source), "-o", str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Exercise Android autosave/audio timing and thread ownership with real SDL events.

Compiles the production lifecycle watch/dispatcher against the configured
Windows SDL build. No game saves, profiles, or Android installations are opened.
Replay Home followed by Android process reclamation separately on an emulator.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/mobile-lifecycle-check"

HARNESS = r'''
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/* Enable only the production Android dispatch branch, after SDL's platform
 * headers have selected the host ABI. */
#define __ANDROID__ 1
#define log_info(...) ((void)0)
#define log_warn(...) ((void)0)

static bool g_mobile_lifecycle_watch_registered;
static bool g_mobile_lifecycle_autosaved;
static int attempts;
static bool save_ok = true;
static char last_reason[64];
static bool audio_suspended;

bool sdl_sound_initialize(void) { return true; }
void sdl_sound_set_suspended(bool suspended)
{
    assert(SDL_IsMainThread());
    audio_suspended = suspended;
}

bool mobile_autosave_game(const char *reason)
{
    assert(SDL_IsMainThread());
    assert(audio_suspended);
    ++attempts;
    SDL_strlcpy(last_reason, reason, sizeof(last_reason));
    return save_ok;
}

__LIFECYCLE_IMPLEMENTATION__

static void push(Uint32 type)
{
    SDL_Event event = {0};
    event.type = type;
    assert(SDL_PushEvent(&event));
}

static void drain(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
        sdl_mobile_lifecycle_handle_event(&event);
}

static int SDLCALL worker(void *userdata)
{
    assert(!SDL_IsMainThread());
    push((Uint32)(uintptr_t)userdata);
    return 0;
}

static void push_worker(Uint32 type)
{
    int status = -1;
    SDL_Thread *thread = SDL_CreateThread(worker, "lifecycle producer",
        (void *)(uintptr_t)type);
    assert(thread);
    SDL_WaitThread(thread, &status);
    assert(status == 0);
}

static bool SDLCALL reject_dispatch(void *userdata, SDL_Event *event)
{
    (void)userdata;
    return event->type != g_mobile_lifecycle_dispatch_event;
}

int main(void)
{
    SDL_SetMainReady();
    assert(SDL_Init(SDL_INIT_EVENTS));
    assert(SDL_IsMainThread());
    sdl_mobile_lifecycle_register();
    assert(g_mobile_lifecycle_watch_registered);

    /* Android can block before queued events are consumed. The save must
     * already be complete when the main-thread background watch returns. */
    push(SDL_EVENT_WILL_ENTER_BACKGROUND);
    assert(attempts == 1 && g_mobile_lifecycle_autosaved && audio_suspended);
    assert(!SDL_strcmp(last_reason, "will enter background"));
    push(SDL_EVENT_DID_ENTER_BACKGROUND);
    push(SDL_EVENT_TERMINATING);
    drain();
    assert(attempts == 1);

    push(SDL_EVENT_WILL_ENTER_FOREGROUND);
    assert(!g_mobile_lifecycle_autosaved);
    assert(audio_suspended); /* Wait until foreground entry is complete. */
    push(SDL_EVENT_DID_ENTER_FOREGROUND);
    assert(!audio_suspended);
    push(SDL_EVENT_WILL_ENTER_BACKGROUND);
    assert(attempts == 2);
    drain();
    assert(attempts == 2 && g_mobile_lifecycle_autosaved);

    /* A failed first attempt is retried by the next background notification. */
    push(SDL_EVENT_DID_ENTER_FOREGROUND);
    save_ok = false;
    push(SDL_EVENT_WILL_ENTER_BACKGROUND);
    assert(attempts == 3 && !g_mobile_lifecycle_autosaved);
    save_ok = true;
    push(SDL_EVENT_DID_ENTER_BACKGROUND);
    assert(attempts == 4 && g_mobile_lifecycle_autosaved);
    drain();
    assert(attempts == 4);

    /* Callbacks on worker threads may only queue work. The actual save and
     * state changes still belong to the SDL/game thread. */
    push(SDL_EVENT_DID_ENTER_FOREGROUND);
    drain();
    push_worker(SDL_EVENT_WILL_ENTER_BACKGROUND);
    assert(attempts == 4 && !g_mobile_lifecycle_autosaved);
    assert(!audio_suspended);
    drain();
    assert(attempts == 5 && g_mobile_lifecycle_autosaved && audio_suspended);
    push_worker(SDL_EVENT_DID_ENTER_BACKGROUND);
    drain();
    assert(attempts == 5);

    /* A deferred event from an earlier lifecycle transition cannot undo
     * the foreground state already dispatched on the main thread. */
    push(SDL_EVENT_DID_ENTER_FOREGROUND);
    drain();
    push_worker(SDL_EVENT_WILL_ENTER_BACKGROUND);
    push(SDL_EVENT_DID_ENTER_FOREGROUND);
    drain();
    assert(attempts == 5 && !g_mobile_lifecycle_autosaved);
    assert(!audio_suspended);

    /* If posting the private event fails, the original queued event is a
     * safe main-thread fallback rather than an off-thread save. */
    SDL_SetEventFilter(reject_dispatch, NULL);
    push_worker(SDL_EVENT_WILL_ENTER_BACKGROUND);
    assert(attempts == 5 && !g_mobile_lifecycle_autosaved);
    drain();
    assert(attempts == 6 && g_mobile_lifecycle_autosaved && audio_suspended);
    SDL_SetEventFilter(NULL, NULL);

    sdl_mobile_lifecycle_unregister();
    assert(!g_mobile_lifecycle_watch_registered);
    push(SDL_EVENT_DID_ENTER_FOREGROUND);
    drain();
    push(SDL_EVENT_WILL_ENTER_BACKGROUND);
    assert(attempts == 6);
    drain();
    assert(attempts == 7);
    SDL_Quit();
    puts("Mobile lifecycle: pause audio/save before Android blocks, resume only on completed foreground entry, duplicate suppression, retry, worker dispatch, stale transitions and queue-failure fallback: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    events = (ROOT / "src/sdl/core/sdl-events.c").read_text(encoding="utf-8-sig")
    start = events.index("enum {\n    SDL_MOBILE_LIFECYCLE_EVENT_COUNT")
    end = events.index("\n#endif", events.index("void sdl_mobile_lifecycle_unregister", start))
    source = OUT / "check.c"
    source.write_text(HARNESS.replace("__LIFECYCLE_IMPLEMENTATION__", events[start:end]),
                      encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([str(BUILD / "_deps/SDL"),
                                   "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
                                   env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "_deps/SDL/libSDL3.dll.a", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

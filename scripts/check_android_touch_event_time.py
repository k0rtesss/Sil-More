#!/usr/bin/env python3
"""Exercise the Android timestamp filter in the real SDL event queue."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SDL_BUILD = ROOT / "build-standard/_deps/SDL"
OUT = ROOT / "scripts/output/android-touch-event-time"

HARNESS = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <jni.h>
#include <SDL3/SDL.h>

#define N_ELEMENTS SDL_arraysize

static int request_count;
static jint request_device, request_pointer, request_token;
static jlong request_uptime;
static int object_marker, method_marker;

static jclass JNICALL fixture_class(JNIEnv* env, jobject obj)
{
    (void)env;
    assert(obj == (jobject)&object_marker);
    return (jclass)obj;
}

static jmethodID JNICALL fixture_method(JNIEnv* env, jclass cls,
    const char* method, const char* signature)
{
    (void)env;
    assert(cls && !strcmp(method, "requestThumbHoldConfirmation"));
    assert(!strcmp(signature, "(IIJI)V"));
    return (jmethodID)&method_marker;
}

static void JNICALL fixture_request(JNIEnv* env, jobject obj, jmethodID method, ...)
{
    (void)env;
    assert(obj && method == (jmethodID)&method_marker);
    va_list args;
    va_start(args, method);
    request_device = va_arg(args, jint);
    request_pointer = va_arg(args, jint);
    request_uptime = va_arg(args, jlong);
    request_token = va_arg(args, jint);
    va_end(args);
    request_count++;
}

static jboolean JNICALL fixture_exception(JNIEnv* env)
{ (void)env; return JNI_FALSE; }
static void JNICALL fixture_release(JNIEnv* env, jobject obj)
{ (void)env; assert(obj); }

static const struct JNINativeInterface_ fixture_jni = {
    .GetObjectClass = fixture_class, .GetMethodID = fixture_method,
    .CallVoidMethod = fixture_request, .ExceptionCheck = fixture_exception,
    .DeleteLocalRef = fixture_release
};
static JNIEnv jni_env = &fixture_jni;
static void* fixture_android_env(void) { return &jni_env; }
static void* fixture_android_activity(void) { return &object_marker; }
#define SDL_GetAndroidJNIEnv fixture_android_env
#define SDL_GetAndroidActivity fixture_android_activity

static Uint64 fixture_ticks(void) { return SDL_SECONDS_TO_NS(1); }
static int fixture_clock(int id, struct timespec* time)
{
    (void)id;
    time->tv_sec = 11;
    time->tv_nsec = 0;
    return 0;
}
#define SDL_GetTicksNS fixture_ticks
#define clock_gettime fixture_clock
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

__PRODUCTION__

static Uint64 watched_time;
static bool SDLCALL watcher(void* unused, SDL_Event* event)
{
    (void)unused;
    if (event->type == SDL_EVENT_FINGER_UP)
        watched_time = event->tfinger.timestamp;
    return true;
}
static bool SDLCALL external_filter(void* unused, SDL_Event* event)
{
    (void)unused;
    (void)event;
    return true;
}

static void send(Uint32 type, Uint64 queued_ms, SDL_TouchID device,
    SDL_FingerID finger, jlong event_ms)
{
    SDL_Event event = {0};
    event.tfinger.type = type;
    event.tfinger.timestamp = SDL_MS_TO_NS(queued_ms);
    event.tfinger.touchID = device;
    event.tfinger.fingerID = finger;
    Java_com_silqh_silmore_SilMoreActivity_setTouchEventUptime(NULL, NULL, event_ms);
    assert(SDL_PushEvent(&event));
    Java_com_silqh_silmore_SilMoreActivity_setTouchEventUptime(NULL, NULL, 0);
}

static void expect(Uint32 type, Uint64 elapsed_ms, SDL_FingerID finger,
    bool physical_down)
{
    SDL_Event event;
    assert(SDL_PeepEvents(&event, 1, SDL_GETEVENT,
        SDL_EVENT_FINGER_DOWN, SDL_EVENT_FINGER_CANCELED) == 1);
    assert(event.type == type && event.tfinger.fingerID == finger);
    assert(event.tfinger.timestamp == SDL_MS_TO_NS(elapsed_ms));
    assert(sdl_android_take_touch_down(&event) == physical_down);
    assert(!sdl_android_take_touch_down(&event));
}

static int SDLCALL other_thread(void* unused)
{
    (void)unused;
    /* Another producer must not see the Java/UI thread's timestamp. */
    SDL_Event event = {0};
    event.tfinger.type = SDL_EVENT_FINGER_UP;
    event.tfinger.timestamp = SDL_MS_TO_NS(1950);
    event.tfinger.touchID = 1;
    event.tfinger.fingerID = 3;
    assert(SDL_PushEvent(&event));
    return 0;
}

int main(void)
{
    assert(SDL_Init(SDL_INIT_EVENTS));
    SDL_EventFilter filter;
    void* data;
    int external_context;
    SDL_SetEventFilter(external_filter, &external_context);
    sdl_android_touch_time_register();
    sdl_android_touch_time_unregister();
    assert(SDL_GetEventFilter(&filter, &data)
        && filter == external_filter && data == &external_context);
    SDL_SetEventFilter(NULL, NULL);
    assert(SDL_AddEventWatch(watcher, NULL));
    sdl_android_touch_time_register();

    /* SDL's ID type is unsigned: an emulator's physical Android device -1
     * becomes SDL -3, while physical Android device 9 becomes SDL 10. */
    sdl_android_request_thumb_hold((SDL_TouchID)(Sint64)-3, 1,
        SDL_MS_TO_NS(1200), 3);
    assert(request_count == 1 && request_device == -1
        && request_pointer == 0 && request_uptime == 11200 && request_token == 3);
    sdl_android_request_thumb_hold(10, 2, SDL_MS_TO_NS(1250), 4);
    assert(request_count == 2 && request_device == 9
        && request_pointer == 1 && request_uptime == 11250 && request_token == 4);

    /* Same original tap time; SDL's arrival times differ by 384 ms. */
    send(SDL_EVENT_FINGER_DOWN, 1210, 1, 1, 11200);
    send(SDL_EVENT_FINGER_UP, 1594, 1, 1, 11200);
    assert(watched_time == SDL_MS_TO_NS(1200));
    expect(SDL_EVENT_FINGER_DOWN, 1200, 1, true);
    expect(SDL_EVENT_FINGER_UP, 1200, 1, false);

    /* Interleaved fingers and cancellation keep their queue positions.
     * A real 400 ms hold stays longer than the gameplay threshold. */
    send(SDL_EVENT_FINGER_DOWN, 1600, 1, 1, 11200);
    send(SDL_EVENT_FINGER_DOWN, 1650, 1, 2, 11400);
    send(SDL_EVENT_FINGER_UP, 1800, 1, 1, 11600);
    send(SDL_EVENT_FINGER_CANCELED, 1850, 1, 2, 11625);
    expect(SDL_EVENT_FINGER_DOWN, 1200, 1, true);
    expect(SDL_EVENT_FINGER_DOWN, 1400, 2, true);
    expect(SDL_EVENT_FINGER_UP, 1600, 1, false);
    expect(SDL_EVENT_FINGER_CANCELED, 1625, 2, false);

    send(SDL_EVENT_FINGER_MOTION, 1900, 1, 1, 11700);
    send(SDL_EVENT_FINGER_DOWN, 1901, SDL_MOUSE_TOUCHID, 1, 11700);
    send(SDL_EVENT_FINGER_DOWN, 1902, SDL_PEN_TOUCHID, 1, 11700);
    android_touch_event_uptime_ns = SDL_MS_TO_NS(11700);
    SDL_Thread* thread = SDL_CreateThread(other_thread, "different source", NULL);
    assert(thread);
    SDL_WaitThread(thread, NULL);
    send(SDL_EVENT_FINGER_DOWN, 1960, 1, 1, 9999);
    send(SDL_EVENT_FINGER_UP, 1961, 1, 1, 13000);
    send(SDL_EVENT_FINGER_UP, 1962, 1, 1, 0);
    expect(SDL_EVENT_FINGER_MOTION, 1900, 1, false);
    expect(SDL_EVENT_FINGER_DOWN, 1901, 1, false);
    expect(SDL_EVENT_FINGER_DOWN, 1902, 1, false);
    expect(SDL_EVENT_FINGER_UP, 1950, 3, false);
    expect(SDL_EVENT_FINGER_DOWN, 1960, 1, false);
    expect(SDL_EVENT_FINGER_UP, 1961, 1, false);
    expect(SDL_EVENT_FINGER_UP, 1962, 1, false);

    /* An acknowledgement uses the same SDL queue and cannot overtake UP. */
    send(SDL_EVENT_FINGER_UP, 1990, 1, 1, 11770);
    Java_com_silqh_silmore_SilMoreActivity_confirmThumbHold(NULL, NULL, 42);
    SDL_Event queued;
    Uint32 confirmation = (Uint32)SDL_GetAtomicInt(&android_thumb_confirmation_event);
    assert(SDL_PeepEvents(&queued, 1, SDL_GETEVENT, SDL_EVENT_FINGER_UP, confirmation) == 1);
    assert(queued.type == SDL_EVENT_FINGER_UP);
    assert(SDL_PeepEvents(&queued, 1, SDL_GETEVENT, SDL_EVENT_FINGER_UP, confirmation) == 1);
    assert(queued.type == confirmation && queued.user.code == 42);

    sdl_android_touch_time_unregister();
    assert(!SDL_GetEventFilter(&filter, &data));
    SDL_RemoveEventWatch(watcher, NULL);
    SDL_Quit();
    puts("Android touch epoch, delayed tap, hold, cancellation, ordering and fallback PASS");
    return 0;
}
'''


def main():
    events = (ROOT / "src/sdl/core/sdl-events.c").read_text(encoding="utf-8")
    start = events.index("static _Thread_local Uint64 android_touch_event_uptime_ns;")
    end = events.index("\n#endif", start)
    OUT.mkdir(parents=True, exist_ok=True)
    source, exe = OUT / "check.c", OUT / "check.exe"
    source.write_text(HARNESS.replace("__PRODUCTION__", events[start:end]), encoding="utf-8")
    jdk = Path(os.environ.get("JAVA_HOME", "C:/Program Files/BellSoft/LibericaJDK-17"))
    if not (jdk / "include/jni.h").is_file():
        raise SystemExit("Set JAVA_HOME to a JDK containing include/jni.h")
    env = dict(os.environ, PATH=os.pathsep.join(
        [str(SDL_BUILD), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", os.environ["PATH"]]))
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(jdk / "include"), "-I" + str(jdk / "include/win32"),
                    "-I" + str(ROOT / "external/SDL/include"), str(source),
                    str(SDL_BUILD / "libSDL3.dll.a"), "-o", str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

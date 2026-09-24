#!/usr/bin/env python3
"""Exercise the production Android detector and UI gates with a fake JNI device list.

Runs on Windows without a phone. SDL open/close results and Android InputDevice
metadata are injected at the platform boundary; production decision functions
are compiled unchanged. This does not validate physical Android hardware.
"""
from pathlib import Path
import os
import subprocess

from check_controller_contract import function_body

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/android-input-check"

PREAMBLE = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <jni.h>
#define SDL_PLATFORM_ANDROID 1
#define __ANDROID__ 1
#define SIL_SDL_MOBILE_BUILD 1
#define SDL_strncmp strncmp
#define log_info(...) ((void)0)
enum { SDL_INPUT_UI_MODE_AUTO, SDL_INPUT_UI_MODE_PLATFORM,
       SDL_INPUT_UI_MODE_CONTROLLER, SDL_INPUT_UI_MODE_COUNT };
typedef enum {
    SDL_STARTUP_DEVICE_DESKTOP, SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER,
    SDL_STARTUP_DEVICE_DESKTOP_HANDHELD, SDL_STARTUP_DEVICE_ANDROID_HANDHELD,
    SDL_STARTUP_DEVICE_MOBILE_TOUCH
} sdl_startup_device_class;
enum { SDL_EVENT_GAMEPAD_ADDED, SDL_EVENT_GAMEPAD_REMOVED,
       SDL_EVENT_GAMEPAD_REMAPPED };
typedef struct { int type, which; } SDL_GamepadDeviceEvent;
static struct { int input_ui_mode; } config;
static struct { int pad_count; } g_gamepad_state;
static bool g_android_controller_present, g_gamepad_auto_ui;
static bool g_direct_touch_present = true;
static sdl_startup_device_class g_startup_device_class;

typedef struct {
    const char* name;
    int sources, keyboard_type;
    bool virtual_device, name_error;
} device_fixture;
static device_fixture devices[4];
static jint device_ids[] = {0, 1, 2, 3};
static int device_count, refs, strings, arrays;
static bool pending_exception, open_succeeds = true;

static jclass JNICALL fake_find_class(JNIEnv* env, const char* name)
{ refs++; return (jclass)devices; }
static jmethodID JNICALL fake_method(JNIEnv* env, jclass cls,
    const char* name, const char* signature)
{ return (jmethodID)name; }
static jobject JNICALL fake_static_object(JNIEnv* env, jclass cls,
    jmethodID method, ...)
{
    refs++;
    if (!strcmp((const char*)method, "getDeviceIds"))
        return (jobject)device_ids;
    va_list args; va_start(args, method);
    int id = va_arg(args, int); va_end(args);
    assert(id >= 0 && id < device_count);
    return (jobject)&devices[id];
}
static jobject JNICALL fake_object(JNIEnv* env, jobject object,
    jmethodID method, ...)
{
    device_fixture* device = (device_fixture*)object;
    assert(!strcmp((const char*)method, "getName"));
    if (device->name_error) { pending_exception = true; return NULL; }
    if (device->name) refs++;
    return (jobject)device->name;
}
static jint JNICALL fake_int(JNIEnv* env, jobject object,
    jmethodID method, ...)
{
    device_fixture* device = (device_fixture*)object;
    if (!strcmp((const char*)method, "getSources")) return device->sources;
    assert(!strcmp((const char*)method, "getKeyboardType"));
    return device->keyboard_type;
}
static jboolean JNICALL fake_bool(JNIEnv* env, jobject object,
    jmethodID method, ...)
{ return ((device_fixture*)object)->virtual_device; }
static jsize JNICALL fake_length(JNIEnv* env, jarray array)
{ return device_count; }
static jint* JNICALL fake_elements(JNIEnv* env, jintArray array, jboolean* copy)
{ arrays++; return device_ids; }
static void JNICALL fake_release_elements(JNIEnv* env, jintArray array,
    jint* values, jint mode)
{ arrays--; }
static const char* JNICALL fake_chars(JNIEnv* env, jstring string, jboolean* copy)
{ strings++; return (const char*)string; }
static void JNICALL fake_release_chars(JNIEnv* env, jstring string, const char* chars)
{ strings--; }
static void JNICALL fake_delete(JNIEnv* env, jobject object)
{ assert(object); refs--; }
static jboolean JNICALL fake_exception(JNIEnv* env)
{ return pending_exception; }
static void JNICALL fake_clear(JNIEnv* env)
{ pending_exception = false; }
static const struct JNINativeInterface_ fake_jni = {
    .FindClass = fake_find_class,
    .GetStaticMethodID = fake_method, .GetMethodID = fake_method,
    .CallStaticObjectMethod = fake_static_object,
    .CallObjectMethod = fake_object, .CallIntMethod = fake_int,
    .CallBooleanMethod = fake_bool, .GetArrayLength = fake_length,
    .GetIntArrayElements = fake_elements,
    .ReleaseIntArrayElements = fake_release_elements,
    .GetStringUTFChars = fake_chars, .ReleaseStringUTFChars = fake_release_chars,
    .DeleteLocalRef = fake_delete, .ExceptionCheck = fake_exception,
    .ExceptionClear = fake_clear
};
static JNIEnv fake_env = &fake_jni;
static void* SDL_GetAndroidJNIEnv(void) { return &fake_env; }
static void sdl_gamepad_open(int id)
{ if (open_succeeds) g_gamepad_state.pad_count++; }
static void sdl_gamepad_close(int id) { g_gamepad_state.pad_count--; }
'''

CASES = r'''
static int cases;
static void expect_ui(const char* label, bool controller)
{
    assert(steamdeck_controls_active() == controller);
    assert(sdl_touch_only_mobile_device_active() == !controller);
    assert(refs == 0 && strings == 0 && arrays == 0 && !pending_exception);
    cases++;
    printf("PASS: %s\n", label);
}
static void startup(const char* label, int opened, bool controller)
{
    config.input_ui_mode = SDL_INPUT_UI_MODE_AUTO;
    g_gamepad_state.pad_count = opened;
    g_gamepad_auto_ui = false;
    g_startup_device_class = sdl_detect_startup_device_class(1280, 600);
    assert(g_startup_device_class == (controller
        ? SDL_STARTUP_DEVICE_ANDROID_HANDHELD : SDL_STARTUP_DEVICE_MOBILE_TOUCH));
    sdl_gamepad_mark_auto_ui();
    assert(g_gamepad_auto_ui == controller);
    expect_ui(label, controller);
}
int main(void)
{
    device_fixture touch = {"touchscreen", 0x1002, 0, false, false};
    device_fixture keyboard = {"Tablet keyboard", 0x501, 2, false, false};
    device_fixture controller = {"Xbox Wireless Controller", 0x1000511, 1, false, false};
    const char* sensor_names[] = {"uinput-fpc", "uinput-goodix", "uinput-fortsense"};
    devices[0] = touch; device_count = 1;
    startup("fresh touch phone", 0, false);
    for (int i = 0; i < 3; i++) {
        /* Positive Android ID, non-alphabetic keyboard and controller flags. */
        device_fixture sensor = {sensor_names[i], i == 0 ? 0x1000111 : 0x501,
                                 1, false, false};
        devices[1] = sensor; device_count = 2;
        startup(sensor_names[i], 0, false);
        /* A keyboard opened by SDL must not combine with the excluded sensor. */
        devices[2] = keyboard; device_count = 3;
        startup("fingerprint sensor plus tablet keyboard", 1, false);
        devices[2] = controller;
        startup("fingerprint sensor plus real controller", 1, true);
    }
    devices[1] = keyboard; device_count = 2;
    startup("alphabetic keyboard alone", 1, false);
    devices[1] = controller;
    startup("real handheld controller", 1, true);
    startup("controller not yet opened", 0, false);
    devices[1].name = "Unknown internal joystick";
    startup("unknown raw device rejected by SDL", 0, false);
    devices[1] = controller; devices[1].virtual_device = true;
    startup("Android virtual controller", 1, false);
    devices[1] = touch;
    startup("touch source bits do not match controller", 1, false);
    devices[1] = controller; devices[1].name_error = true;
    startup("failed device metadata remains touch", 1, false);
    devices[1].name_error = false; devices[1].name = NULL;
    startup("missing device name remains touch", 1, false);

    devices[1] = controller;
    startup("before hotplug", 0, false);
    SDL_GamepadDeviceEvent event = {SDL_EVENT_GAMEPAD_ADDED, 1};
    sdl_gamepad_handle_device(&event);
    expect_ui("real controller connected", true);
    event.type = SDL_EVENT_GAMEPAD_REMOVED;
    sdl_gamepad_handle_device(&event);
    expect_ui("controller disconnected restores touch", false);
    assert(g_startup_device_class == SDL_STARTUP_DEVICE_MOBILE_TOUCH);
    open_succeeds = false; event.type = SDL_EVENT_GAMEPAD_ADDED;
    sdl_gamepad_handle_device(&event);
    expect_ui("failed controller open preserves touch", false);
    config.input_ui_mode = SDL_INPUT_UI_MODE_CONTROLLER;
    expect_ui("explicit Controller override still works", true);
    config.input_ui_mode = SDL_INPUT_UI_MODE_PLATFORM;
    expect_ui("explicit Touch after false detection", false);
    printf("Android input detection: %d scenarios passed\n", cases);
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    jdk = Path(os.environ.get("JAVA_HOME", "C:/Program Files/BellSoft/LibericaJDK-17"))
    if not (jdk / "include/jni.h").exists():
        raise SystemExit("Set JAVA_HOME to a JDK containing include/jni.h")
    selections = [
        ("src/sdl/core/sdl-layout.c", [
            "void sdl_android_clear_pending_exception(JNIEnv* env)",
            "bool sdl_android_has_controller_device(void)",
            "sdl_startup_device_class sdl_detect_startup_device_class(\n    int screen_width, int screen_height)"]),
        ("src/sdl/config/sdl-settings.c", [
            "int get_sdl_input_ui_mode(void)", "bool steamdeck_controls_active(void)"]),
        ("src/sdl/core/sdl-layout.c", ["bool sdl_touch_only_mobile_device_active(void)"]),
        ("src/sdl/input/sdl-gamepad.c", [
            "void sdl_gamepad_mark_auto_ui(void)",
            "void sdl_gamepad_handle_device(const SDL_GamepadDeviceEvent* ev)"]),
    ]
    production = []
    for path, signatures in selections:
        source = (ROOT / path).read_text(encoding="utf-8")
        for signature in signatures:
            production.append(signature + "\n" + function_body(source, signature))
    generated = OUT / "check.c"
    generated.write_text(PREAMBLE + "\n".join(production) + CASES, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = "C:/msys64/mingw64/bin;C:/msys64/usr/bin;" + env["PATH"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra",
                    "-Werror", "-Wno-unused-parameter", "-I" + str(jdk / "include"),
                    "-I" + str(jdk / "include/win32"), str(generated), "-o", str(exe)],
                   env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

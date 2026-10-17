#!/usr/bin/env python3
"""Check real handheld detection, fresh SDL defaults and saved-profile reloads.

Uses Windows CMake objects and private profiles with dummy video/audio. Hardware
identifiers, firmware tables and the Steam boundary are fixtures, not device QA.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

import qa9_sdl_fixture as control

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/handheld-startup-check"

PREFIX = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "steamworks.h"
#include <assert.h>
#if defined(TEST_LINUX_DETECTOR)
/* Compile the production Linux branch on the Windows fixture host; only
 * sysfs file access is redirected. This is not a native Linux build. */
#undef SDL_PLATFORM_WINDOWS
#define SDL_PLATFORM_LINUX 1
static FILE* test_dmi_open(const char* path, const char* mode)
{
    const char* root = "/sys/class/dmi/id/";
    if (!strncmp(path, root, strlen(root))) {
        char redirected[1024];
        strnfmt(redirected, sizeof(redirected), "%s/%s",
            SDL_getenv("TEST_LINUX_DMI_PATH"), path + strlen(root));
        return fopen(redirected, mode);
    }
    return fopen(path, mode);
}
#define fopen test_dmi_open
#endif
#if defined(SDL_PLATFORM_WINDOWS)
static UINT WINAPI test_firmware_table(DWORD provider, DWORD table,
    PVOID buffer, DWORD capacity)
{
    if (!SDL_getenv("TEST_WINDOWS_FIRMWARE"))
        return GetSystemFirmwareTable(provider, table, buffer, capacity);
    assert(provider == 0x52534D42 && table == 0);
    const char strings[] = "Valve\0Jupiter\0";
    Uint8 raw[64] = {0};
    UINT size = 16 + sizeof(strings);
    raw[4] = (Uint8)(size - 8);
    raw[8] = 1; raw[9] = 8; raw[12] = 1; raw[13] = 2;
    memcpy(raw + 16, strings, sizeof(strings));
    if (buffer && capacity >= size)
        memcpy(buffer, raw, size);
    return size;
}
#define GetSystemFirmwareTable test_firmware_table
#endif
static bool test_steam_deck(void)
{
    return SDL_getenv("TEST_STEAM_DECK") != NULL;
}
#define sil_steamworks_is_steam_deck test_steam_deck
#include "sdl/core/sdl-handheld.c"
#undef sil_steamworks_is_steam_deck
#if defined(TEST_LINUX_DETECTOR)
#undef fopen
#endif
#if defined(SDL_PLATFORM_WINDOWS)
#undef GetSystemFirmwareTable
#endif

static void identifiers(void)
{
    static const struct {
        const char* vendor;
        const char* product;
        const char* family;
        sdl_desktop_handheld_kind expected;
    } cases[] = {
        {"Valve", "Jupiter", "", SDL_DESKTOP_HANDHELD_STEAM_DECK},
        {"Valve", "Galileo", "", SDL_DESKTOP_HANDHELD_STEAM_DECK},
        {"Valve", "Steam Deck", "", SDL_DESKTOP_HANDHELD_STEAM_DECK},
        {"Valve", "Steam Machine", "", SDL_DESKTOP_HANDHELD_NONE},
        {"Other vendor", "Jupiter", "", SDL_DESKTOP_HANDHELD_NONE},
        {"ASUSTeK COMPUTER INC.", "ROG Ally RC71L_RC71L", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"ASUSTeK COMPUTER INC.", "ROG Ally X RC72LA_RC72LA", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"ASUSTeK COMPUTER INC.", "ROG Xbox Ally X RC73XA_RC73XA", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"ASUSTeK COMPUTER INC.", "ROG Zephyrus G14", "", SDL_DESKTOP_HANDHELD_NONE},
        {"LENOVO", "83E1", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"LENOVO", "83N6", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"LENOVO", "83N0", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"LENOVO", "new product code", "Legion Go S", SDL_DESKTOP_HANDHELD_OTHER},
        {"LENOVO", "Legion 7", "Legion", SDL_DESKTOP_HANDHELD_NONE},
        {"LENOVO", "83E10", "", SDL_DESKTOP_HANDHELD_NONE},
        {"Other vendor", "83E1", "", SDL_DESKTOP_HANDHELD_NONE},
        {"Micro-Star International Co., Ltd.", "Claw A1M", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"MSI", "Claw 8 AI+ A2VM", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"MSI", "Raider", "", SDL_DESKTOP_HANDHELD_NONE},
        {"GPD", "G1617-01", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"GPD", "G1618-04", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"GPD", "Pocket 4", "", SDL_DESKTOP_HANDHELD_NONE},
        {"AYANEO", "AYANEO 2S", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"AYA", "AIR Plus", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"AYANEO", "AYANEO AM01", "", SDL_DESKTOP_HANDHELD_NONE},
        {"ONE-NETBOOK", "ONEXPLAYER mini", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"ZOTAC", "ZOTAC GAMING ZONE", "", SDL_DESKTOP_HANDHELD_OTHER},
        {"ASUS", "ROG AllyNotAHandheld", "", SDL_DESKTOP_HANDHELD_NONE},
        {"", "", "", SDL_DESKTOP_HANDHELD_NONE},
        {"valve", "galileo", "", SDL_DESKTOP_HANDHELD_STEAM_DECK},
    };
    for (size_t i = 0; i < N_ELEMENTS(cases); i++) {
        sdl_handheld_system_info info = {0};
        SDL_strlcpy(info.vendor, cases[i].vendor, sizeof(info.vendor));
        SDL_strlcpy(info.product, cases[i].product, sizeof(info.product));
        SDL_strlcpy(info.family, cases[i].family, sizeof(info.family));
        assert(sdl_handheld_identify(&info) == cases[i].expected);
    }
    sdl_handheld_system_info info = {0};
    const char* dmi = SDL_getenv("TEST_DMI_PATH");
    assert(dmi);
    sdl_handheld_read_dmi(dmi, &info);
    assert(!strcmp(info.vendor, "Valve") && !strcmp(info.product, "Galileo"));
    assert(sdl_handheld_identify(&info) == SDL_DESKTOP_HANDHELD_STEAM_DECK);

#if defined(SDL_PLATFORM_WINDOWS)
    Uint8 raw[128] = {0};
    const char strings[] = "Valve\0Galileo\0OLED\0Steam Deck\0";
    raw[8] = 1; raw[9] = 27;
    raw[12] = 1; raw[13] = 2; raw[14] = 3; raw[34] = 4;
    memcpy(raw + 35, strings, sizeof(strings));
    size_t size = 35 + sizeof(strings);
    raw[4] = (Uint8)(size - 8);
    memset(&info, 0, sizeof(info));
    assert(sdl_handheld_read_smbios(raw, size, &info));
    assert(sdl_handheld_identify(&info) == SDL_DESKTOP_HANDHELD_STEAM_DECK);
    assert(!strcmp(info.version, "OLED") && !strcmp(info.family, "Steam Deck"));
    for (size_t cut = 0; cut < size; cut++)
        assert(!sdl_handheld_read_smbios(raw, cut, &info));
    raw[13] = 255;
    assert(sdl_handheld_read_smbios(raw, size, &info));
    assert(info.product[0] == '\0');
    raw[9] = 3;
    assert(!sdl_handheld_read_smbios(raw, size, &info));
    raw[9] = 127;
    assert(!sdl_handheld_read_smbios(raw, size, &info));
    raw[9] = 27; raw[size - 1] = 'x';
    assert(!sdl_handheld_read_smbios(raw, size, &info));
#endif
    puts("Handheld model matching, desktop exclusions, DMI and bounded SMBIOS: PASS");
}
'''

CHECKS = r'''
    const char* mode = SDL_getenv("TEST_PROFILE_MODE");
    bool handheld = strcmp(SDL_getenv("SIL_HANDHELD"), "desktop") != 0;
    assert(g_startup_device_class == (handheld
        ? SDL_STARTUP_DEVICE_DESKTOP_HANDHELD : SDL_STARTUP_DEVICE_DESKTOP));
    assert(config.min_terminal_mode == (handheld && strcmp(mode, "reload")
        ? SDL_MIN_TERMINAL_COMPACT : SDL_MIN_TERMINAL_NORMAL));
    if (!strcmp(mode, "reload")) {
        assert(config.enable_bottom_panes && config.left_panel_expanded_on_launch);
        assert(config.input_ui_mode == SDL_INPUT_UI_MODE_PLATFORM);
        assert(!config.gamepad_enabled && config.mouse_enabled);
        assert(!config.touch_top_panel_default_open && config.touch_top_panel_arrows_visible);
    } else {
        assert(config.enable_bottom_panes == !handheld);
        assert(config.left_panel_expanded_on_launch == !handheld);
        assert(config.mouse_enabled == !handheld);
        assert(config.gamepad_enabled);
        assert(config.input_ui_mode == (handheld ? SDL_INPUT_UI_MODE_AUTO
            : SDL_INPUT_UI_MODE_PLATFORM));
        if (handheld) {
            assert(config.touch_top_panel_default_open);
            assert(!config.touch_top_panel_arrows_visible);
            assert(steamdeck_controls_active());
        }
        for (int i = 0; i < SDL_PANE_ORIENTATION_PROFILE_COUNT; i++) {
            if (handheld || i < SDL_MIN_TERMINAL_MODE_COUNT) {
                assert(g_pane_profiles[i].enable_bottom_panes == !handheld);
                assert(g_pane_profiles[i].left_panel_expanded_on_launch == !handheld);
            }
        }
    }
    if (handheld && !strcmp(mode, "fresh")) {
        /* An explicit saved desktop layout/input choice must win on relaunch. */
        set_sdl_min_terminal_mode(SDL_MIN_TERMINAL_NORMAL);
        config.enable_bottom_panes = true;
        config.left_panel_expanded_on_launch = true;
        config.touch_top_panel_default_open = false;
        config.touch_top_panel_arrows_visible = true;
        config.input_ui_mode = SDL_INPUT_UI_MODE_PLATFORM;
        config.gamepad_enabled = false;
        config.mouse_enabled = true;
    }
    sdl_quit_hook(NULL);
    sdl_gamepad_shutdown();
    TTF_Quit(); SDL_Quit();
    puts("Isolated handheld/desktop startup and profile persistence: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    dmi = OUT / "dmi"
    dmi.mkdir(exist_ok=True)
    (dmi / "sys_vendor").write_text("  Valve \n")
    (dmi / "product_name").write_text(" Galileo\t\n")
    harness = control.HARNESS
    harness = harness.replace("    const char* assets", r'''
    identifiers();
    sdl_desktop_handheld_kind detected = sdl_detect_desktop_handheld();
    const char* expected = SDL_getenv("TEST_EXPECTED_KIND");
    assert(expected && detected == atoi(expected));
    if (SDL_getenv("TEST_DETECT_ONLY")) return 0;
    if (strcmp(SDL_getenv("SIL_HANDHELD"), "desktop")) {
        assert(SDL_Init(SDL_INIT_GAMEPAD));
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.name = "Handheld test controls";
        assert(SDL_AttachVirtualJoystick(&desc));
    }
    const char* assets''', 1)
    harness = PREFIX + harness[:harness.index("    int last = ")] + CHECKS
    source = OUT / "check.c"
    source.write_text(harness, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"'+p+'"' for p in objects
        if not p.endswith(("/src/main.c.obj", "/src/sdl/core/sdl-handheld.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software", SDL_AUDIO_DRIVER="dummy",
               CONTROL_TEST_ASSETS=str(ROOT / "lib"), TEST_DMI_PATH=str(dmi))
    env.pop("TEST_STEAM_DECK", None)
    env.pop("TEST_DETECT_ONLY", None)
    env.pop("TEST_WINDOWS_FIRMWARE", None)
    exe = OUT / "check.exe"
    command = ["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@"+str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp"]
    subprocess.run([*command, "-o", str(exe)], cwd=BUILD, env=env, check=True)
    for override, expected, steam in (("auto", "1", True), ("desktop", "0", True),
                                       ("handheld", "2", True), ("steamdeck", "1", False)):
        run_env = dict(env, SIL_HANDHELD=override, TEST_EXPECTED_KIND=expected, TEST_DETECT_ONLY="1")
        if steam:
            run_env["TEST_STEAM_DECK"] = "1"
        subprocess.run([str(exe)], cwd=ROOT, env=run_env, check=True, timeout=20)
    run_env = dict(env, SIL_HANDHELD="auto", TEST_EXPECTED_KIND="1",
                   TEST_DETECT_ONLY="1", TEST_WINDOWS_FIRMWARE="1")
    subprocess.run([str(exe)], cwd=ROOT, env=run_env, check=True, timeout=20)
    linux_exe = OUT / "check-linux-branch.exe"
    subprocess.run([*command, "-DTEST_LINUX_DETECTOR", "-o", str(linux_exe)],
                   cwd=BUILD, env=env, check=True)
    linux_dmi = OUT / "linux-dmi"
    linux_dmi.mkdir(exist_ok=True)
    for vendor, product, expected in (("Valve", "Jupiter", "1"), ("Valve", "Galileo", "1"),
                                       ("LENOVO", "83N6", "2"), ("LENOVO", "ThinkPad", "0")):
        (linux_dmi / "sys_vendor").write_text(vendor + "\n")
        (linux_dmi / "product_name").write_text(product + "\n")
        run_env = dict(env, SIL_HANDHELD="auto", TEST_EXPECTED_KIND=expected,
                       TEST_DETECT_ONLY="1", TEST_LINUX_DMI_PATH=str(linux_dmi))
        subprocess.run([str(linux_exe)], cwd=ROOT, env=run_env, check=True, timeout=20)
    print("Production Linux detector branch with sysfs fixtures on Windows: PASS", flush=True)
    for override, expected in (("steamdeck", "1"), ("handheld", "2"), ("desktop", "0")):
        with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
            log_dir = OUT / (override + "-fresh")
            log_dir.mkdir(exist_ok=True)
            run_env = dict(env, SIL_HANDHELD=override, TEST_EXPECTED_KIND=expected,
                           CONTROL_TEST_PROFILE=profile, TEST_PROFILE_MODE="fresh",
                           CONTROL_TEST_LOG_BASE=str(log_dir / "check.exe"))
            subprocess.run([str(exe), "--windowed"], cwd=ROOT, env=run_env, check=True, timeout=40)
            if override != "desktop":
                run_env["TEST_PROFILE_MODE"] = "reload"
                log_dir = OUT / (override + "-reload")
                log_dir.mkdir(exist_ok=True)
                run_env["CONTROL_TEST_LOG_BASE"] = str(log_dir / "check.exe")
                subprocess.run([str(exe), "--windowed"], cwd=ROOT, env=run_env, check=True, timeout=40)
    print("Handheld detection, first-launch defaults and saved settings: PASS")


if __name__ == "__main__":
    main()

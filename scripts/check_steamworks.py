#!/usr/bin/env python3
"""Check the production Steamworks adapter with SDK-boundary test doubles.

Uses the installed SDK headers; this does not log in to Steam or launch a game.
"""
from pathlib import Path
import os
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/steamworks-check"
SDK = Path(os.environ.get("STEAMWORKS_SDK_ROOT", ROOT / "external/steamworks_sdk/sdk"))

HARNESS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <steam/steam_api.h>
#include <steam/steam_api_flat.h>
#include "steamworks.h"

static bool restart;
static ESteamAPIInitResult init_result = k_ESteamAPIInitResult_OK;
static int restarts, inits, callbacks, shutdowns;
static int utils_handle;
static bool have_utils = true;
static ESteamHardwareType hardware = k_ESteamHardwareTypeNone;

extern "C" ISteamUtils* SteamAPI_SteamUtils_v011()
{
    assert(inits == 1 && init_result == k_ESteamAPIInitResult_OK && !shutdowns);
    return have_utils ? reinterpret_cast<ISteamUtils*>(&utils_handle) : nullptr;
}

extern "C" ESteamHardwareType SteamAPI_ISteamUtils_IsRunningOnSteamHardware(
    ISteamUtils* utils)
{
    assert(utils == reinterpret_cast<ISteamUtils*>(&utils_handle));
    return hardware;
}

extern "C" bool S_CALLTYPE SteamAPI_RestartAppIfNecessary(uint32 app_id)
{
    assert(app_id == 5442160 && inits == 0);
    ++restarts;
    return restart;
}

extern "C" ESteamAPIInitResult S_CALLTYPE SteamInternal_SteamAPI_Init(
    const char* versions, SteamErrMsg* error)
{
    assert(restarts == 1 && versions && versions[0]);
    ++inits;
    if (init_result != k_ESteamAPIInitResult_OK)
        std::snprintf(*error, sizeof(*error), "test Steam client unavailable");
    return init_result;
}

extern "C" void S_CALLTYPE SteamAPI_RunCallbacks()
{
    assert(inits == 1 && init_result == k_ESteamAPIInitResult_OK && shutdowns == 0);
    ++callbacks;
}

extern "C" void S_CALLTYPE SteamAPI_Shutdown()
{
    assert(inits == 1 && callbacks == 2 && shutdowns == 0);
    ++shutdowns;
}

extern "C" void log_log(int, const char*, int, const char*, ...) {}
extern "C" void sil_steamworks_achievements_shutdown() {}

static void verify_exit()
{
    assert(shutdowns == (!restart && init_result == k_ESteamAPIInitResult_OK ? 1 : 0));
    assert(sil_steamworks_callback_timeout_ms() == -1);
    assert(!sil_steamworks_is_steam_deck());
    sil_steamworks_run_callbacks();
    std::puts("Steamworks adapter case: PASS");
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    restart = std::strcmp(argv[1], "restart") == 0;
    if (std::strcmp(argv[1], "failure") == 0)
        init_result = k_ESteamAPIInitResult_NoSteamClient;
    assert(std::atexit(verify_exit) == 0);
    assert(sil_steamworks_callback_timeout_ms() == -1);
    sil_steamworks_run_callbacks();
    assert(callbacks == 0);
    assert(!sil_steamworks_is_steam_deck());
    assert(sil_steamworks_init() == !restart);

    if (restart || init_result != k_ESteamAPIInitResult_OK)
    {
        assert(inits == (restart ? 0 : 1));
        assert(sil_steamworks_callback_timeout_ms() == -1);
        assert(!sil_steamworks_is_steam_deck());
        sil_steamworks_run_callbacks();
        assert(callbacks == 0);
    }
    else
    {
        assert(sil_steamworks_init());
        assert(restarts == 1 && inits == 1);
        assert(!sil_steamworks_is_steam_deck());
        hardware = k_ESteamHardwareTypeSteamDeck;
        assert(sil_steamworks_is_steam_deck());
        hardware = k_ESteamHardwareTypeSteamMachine;
        assert(!sil_steamworks_is_steam_deck());
        hardware = k_ESteamHardwareTypeSteamFrame;
        assert(!sil_steamworks_is_steam_deck());
        have_utils = false;
        assert(!sil_steamworks_is_steam_deck());
        int timeout = sil_steamworks_callback_timeout_ms();
        assert(timeout > 0 && timeout <= 1000);
        sil_steamworks_run_callbacks();
        sil_steamworks_run_callbacks();
        assert(callbacks == 2);
    }
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "adapter-check.cpp"
    source.write_text(HARNESS, encoding="utf-8")
    exe = OUT / ("adapter-check.exe" if os.name == "nt" else "adapter-check")
    env = os.environ.copy()
    if os.name == "nt":
        env["PATH"] = "C:/msys64/mingw64/bin;C:/msys64/usr/bin;" + env["PATH"]
    compiler = shlex.split(env.get("CXX", "c++"))
    compiler_executable = shutil.which(compiler[0], path=env["PATH"])
    if not compiler_executable:
        raise SystemExit("No C++ compiler found; install GCC/Clang or set CXX.")
    compiler[0] = compiler_executable
    subprocess.run([
        *compiler, "-std=c++11", "-Wall", "-Wextra",
        "-DSIL_STEAMWORKS", "-DSIL_STEAM_APP_ID=5442160", "-DSTEAM_API_NODLL",
        "-I", str(ROOT / "src"), "-isystem", str(SDK / "public"),
        str(source), str(ROOT / "src/steamworks.cpp"), "-o", str(exe),
    ], cwd=ROOT, env=env, check=True)
    for case in ("success", "failure", "restart"):
        print(case, flush=True)
        subprocess.run([str(exe), case], cwd=ROOT, env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

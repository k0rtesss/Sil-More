#!/usr/bin/env python3
"""Check production Steam achievement synchronization against SDK-boundary doubles."""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/achievement-steam-check"
SDK = Path(os.environ.get("STEAMWORKS_SDK_ROOT", ROOT / "external/steamworks_sdk/sdk"))

HARNESS = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <steam/steam_api.h>
#include <steam/steam_api_flat.h>
#include "achievements.h"
#include "steamworks.h"

static bool active, ready = true, have_stats = true, store_ok = true, overlay = true;
static unsigned int cached, sets, stores, overlay_calls, registrations;
static uint64 identity = 42;
static CCallbackBase *stored_callback;
static int user_handle, stats_handle, utils_handle, friends_handle;
static const sil_achievement_definition catalogue[] = {
    CATALOGUE_ROWS
};
extern "C" const sil_achievement_definition *sil_achievement_get(int id)
{ assert(id >= 0 && id < SIL_ACH_COUNT); return &catalogue[id]; }
extern "C" bool sil_steamworks_active() { return active; }
extern "C" void log_log(int, const char*, int, const char*, ...) {}
extern "C" void S_CALLTYPE SteamAPI_RegisterCallback(CCallbackBase *callback, int type)
{
    assert(active && type == UserStatsStored_t::k_iCallback && !stored_callback);
    stored_callback = callback; ++registrations;
}
extern "C" void S_CALLTYPE SteamAPI_UnregisterCallback(CCallbackBase *callback)
{ assert(active && callback == stored_callback); stored_callback = nullptr; }
extern "C" ISteamUser *SteamAPI_SteamUser_v023()
{ assert(active); return reinterpret_cast<ISteamUser*>(&user_handle); }
extern "C" uint64_steamid SteamAPI_ISteamUser_GetSteamID(ISteamUser *user)
{ assert(user == reinterpret_cast<ISteamUser*>(&user_handle)); return identity; }
extern "C" ISteamUserStats *SteamAPI_SteamUserStats_v013()
{ return have_stats ? reinterpret_cast<ISteamUserStats*>(&stats_handle) : nullptr; }
static unsigned int bit_for(const char *key)
{
    for (int i = 0; i < SIL_ACH_COUNT; ++i) if (!std::strcmp(key, catalogue[i].key)) return 1u << i;
    assert(false); return 0;
}
extern "C" bool SteamAPI_ISteamUserStats_GetAchievement(ISteamUserStats *stats, const char *key, bool *earned)
{
    assert(stats == reinterpret_cast<ISteamUserStats*>(&stats_handle));
    *earned = (cached & bit_for(key)) != 0; return ready;
}
extern "C" bool SteamAPI_ISteamUserStats_SetAchievement(ISteamUserStats *stats, const char *key)
{
    assert(stats == reinterpret_cast<ISteamUserStats*>(&stats_handle));
    ++sets; cached |= bit_for(key); return true;
}
extern "C" bool SteamAPI_ISteamUserStats_StoreStats(ISteamUserStats *stats)
{ assert(stats == reinterpret_cast<ISteamUserStats*>(&stats_handle)); ++stores; return store_ok; }
extern "C" ISteamUtils *SteamAPI_SteamUtils_v011()
{ return reinterpret_cast<ISteamUtils*>(&utils_handle); }
extern "C" ISteamFriends *SteamAPI_SteamFriends_v018()
{ return reinterpret_cast<ISteamFriends*>(&friends_handle); }
extern "C" bool SteamAPI_ISteamUtils_IsOverlayEnabled(ISteamUtils *utils)
{ assert(utils == reinterpret_cast<ISteamUtils*>(&utils_handle)); return overlay; }
extern "C" void SteamAPI_ISteamFriends_ActivateGameOverlay(ISteamFriends *friends, const char *dialog)
{ assert(friends == reinterpret_cast<ISteamFriends*>(&friends_handle)); assert(!std::strcmp(dialog,"achievements")); ++overlay_calls; }

/* Keep the production state machine intact; move its deadline to simulate a
 * minute passing without sleeping or changing the system clock. */
#include "achievements-steam.cpp"
static void deadline() { achievements.next_attempt = Clock::time_point::min(); }
static void reply(EResult result, uint64 app = 5442160)
{ assert(stored_callback); UserStatsStored_t value = { app, result }; stored_callback->Run(&value); }

int main()
{
    sil_steamworks_sync_achievements(1);
    assert(!sets && !stores && !registrations && !sil_steamworks_achievements_unlocked());
    assert(!sil_steamworks_show_achievements());
    active = true; ready = false;
    sil_steamworks_sync_achievements(1);
    assert(registrations == 1 && !sets && !stores);
    ready = true; store_ok = false; deadline();
    sil_steamworks_sync_achievements(1);
    assert(sets == 1 && stores == 1 && achievements.staged == 1 && !achievements.pending);
    sil_steamworks_sync_achievements(1); assert(stores == 1);
    store_ok = true; deadline(); sil_steamworks_sync_achievements(1);
    assert(sets == 2 && stores == 2 && achievements.pending == 1 && !achievements.confirmed);
    sil_steamworks_sync_achievements(3); assert(stores == 2);
    reply(k_EResultOK, 1); assert(achievements.pending == 1);
    reply(k_EResultFail); assert(!achievements.pending && achievements.staged == 1);
    sil_steamworks_sync_achievements(3); assert(stores == 2);
    deadline(); sil_steamworks_sync_achievements(3);
    assert(stores == 3 && achievements.pending == 3 && !achievements.confirmed);
    reply(k_EResultOK); assert(achievements.confirmed == 3 && !achievements.staged);
    deadline(); sil_steamworks_sync_achievements(3); assert(stores == 3);
    sil_steamworks_sync_achievements(7);
    assert(stores == 4 && achievements.pending == 4);
    // Lost callback: retry only at the timeout, without dropping the local bit.
    sil_steamworks_sync_achievements(7); assert(stores == 4);
    deadline(); sil_steamworks_sync_achievements(7); assert(stores == 5);
    reply(k_EResultOK); assert(achievements.confirmed == 7);
    deadline(); sil_steamworks_sync_achievements(7); assert(stores == 5);
    assert(sil_steamworks_achievements_unlocked() == 7);
    // A new platform account starts a fresh provider reconciliation.
    identity = 43; cached = 4; sil_steamworks_sync_achievements(0);
    assert(sil_steamworks_achievements_unlocked() == 4 && stores == 5);
    have_stats = false; deadline(); sil_steamworks_sync_achievements(7); assert(stores == 5);
    have_stats = true; deadline(); sil_steamworks_sync_achievements(SIL_ACH_ALL_MASK);
    assert(achievements.pending == (SIL_ACH_ALL_MASK & ~4u));
    reply(k_EResultOK); deadline(); sil_steamworks_sync_achievements(SIL_ACH_ALL_MASK);
    assert(achievements.confirmed == SIL_ACH_ALL_MASK);
    assert(sil_steamworks_achievements_unlocked() == SIL_ACH_ALL_MASK);
    overlay = false; assert(!sil_steamworks_show_achievements() && !overlay_calls);
    overlay = true; assert(sil_steamworks_show_achievements() && overlay_calls == 1);
    sil_steamworks_achievements_shutdown(); assert(!stored_callback);
    active = false; assert(!sil_steamworks_achievements_unlocked());
    puts("Steam achievements: readiness, offline retry, cached-vs-confirmed state, callbacks, batching, timeout, import and overlay PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.cpp"
    header = (ROOT / "src/achievements.h").read_text()
    enum = header.split("typedef enum sil_achievement_id {", 1)[1].split("} sil_achievement_id;", 1)[0]
    keys = re.findall(r"\bSIL_ACH_([A-Z][A-Z0-9_]*)(?:\s*=\s*\d+)?\s*,", enum)
    rows = ",\n".join('    { "' + key + '", "", "" }' for key in keys)
    source.write_text(HARNESS.replace("CATALOGUE_ROWS", rows), encoding="utf-8")
    exe = OUT / "check.exe"
    env = os.environ.copy()
    env["PATH"] = "C:/msys64/mingw64/bin;C:/msys64/usr/bin;" + env["PATH"]
    subprocess.run([
        "C:/msys64/mingw64/bin/c++.exe", "-std=c++11", "-Wall", "-Wextra",
        "-DSIL_STEAMWORKS", "-DSIL_STEAM_APP_ID=5442160", "-DSTEAM_API_NODLL",
        "-I", str(ROOT / "src"), "-isystem", str(SDK / "public"),
        str(source), "-o", str(exe)], cwd=ROOT, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

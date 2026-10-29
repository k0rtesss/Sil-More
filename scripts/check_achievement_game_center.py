#!/usr/bin/env python3
"""Compile the production Game Center reconciliation state without Apple services.

Exercises actual retry/acknowledgement/mask code. Apple SDK compilation and device
sign-in/presentation still require Xcode on a Mac and an enabled developer app.
"""
from pathlib import Path
import os
import plistlib
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/achievement-game-center-check"

HARNESS = r'''
#include "achievements-game-center-state.h"
#include "achievements-game-center.h"
#include <assert.h>
#include <stdio.h>

static void bit(sil_game_center_masks* masks, int id)
{ masks->words[id / 64] |= UINT64_C(1) << (id % 64); }
static bool has(const sil_game_center_masks* masks, int id)
{ return (masks->words[id / 64] & (UINT64_C(1) << (id % 64))) != 0; }

int main(void)
{
    sil_game_center_state state = {0};
    sil_game_center_masks all, available = {0}, remote = {0}, extra = {0};
    for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word) all.words[word] = UINT64_MAX;
    const int excluded[] = {SIL_ACH_ANGBAND_MAINTENANCE, SIL_ACH_FOR_IDRIL_AND_EARENDIL};
    for (unsigned i = 0; i < sizeof(excluded)/sizeof(excluded[0]); ++i) {
        int id = excluded[i];
        assert(sil_achievement_word_mask(id / 64) & (UINT64_C(1) << (id % 64)));
        assert(!(sil_gc_achievement_word_mask(id / 64) & (UINT64_C(1) << (id % 64))));
    }
    sil_gc_reset(&state);
    sil_gc_want(&state, &all);
    assert(state.wanted.words[1] == sil_gc_achievement_word_mask(1));
    for (unsigned i = 0; i < sizeof(excluded)/sizeof(excluded[0]); ++i)
        assert(!has(&state.wanted, excluded[i]));
    assert(!sil_gc_begin_report(&state, 0));
    assert(sil_gc_begin_load(&state, 0));
    uint64_t stale_load = state.serial;
    assert(!sil_gc_begin_load(&state, 59999));
    assert(sil_gc_begin_load(&state, 60000));
    assert(!sil_gc_finish_load(&state, stale_load, &all, &all, true, 60000));
    bit(&available, 0); bit(&available, 1); bit(&available, 63); bit(&available, 64);
    for (unsigned i = 0; i < sizeof(excluded)/sizeof(excluded[0]); ++i)
        bit(&available, excluded[i]);
    bit(&available, SIL_ACH_SWIFT_AS_THE_WIND); bit(&available, SIL_ACH_TENFOLD_DOOM);
    bit(&remote, 0); bit(&remote, 2); /* Unknown/unregistered IDs cannot import. */
    for (unsigned i = 0; i < sizeof(excluded)/sizeof(excluded[0]); ++i)
        bit(&remote, excluded[i]);
    assert(sil_gc_finish_load(&state, state.serial, &available, &remote, true, 60000));
    assert(has(&state.remote, 0) && !has(&state.remote, 2));
    for (unsigned i = 0; i < sizeof(excluded)/sizeof(excluded[0]); ++i) {
        assert(!has(&state.available, excluded[i]));
        assert(!has(&state.remote, excluded[i]));
    }
    assert(sil_gc_begin_report(&state, 60000));
    uint64_t first_report = state.serial;
    assert(!has(&state.pending, 0) && !has(&state.pending, 2));
    assert(has(&state.pending, 63) && has(&state.pending, 64));
    assert(has(&state.pending, SIL_ACH_SWIFT_AS_THE_WIND));
    assert(has(&state.pending, SIL_ACH_TENFOLD_DOOM));
    assert(!has(&state.remote, SIL_ACH_TENFOLD_DOOM));
    assert(!sil_gc_begin_report(&state, 60001));
    assert(sil_gc_finish_report(&state, first_report, false, 60001));
    assert(!has(&state.confirmed, 63) && !has(&state.remote, 63));
    assert(!sil_gc_begin_report(&state, 120000));
    assert(sil_gc_begin_report(&state, 120001));
    uint64_t lost_report = state.serial;
    assert(!sil_gc_begin_report(&state, 180000));
    assert(sil_gc_begin_report(&state, 180001));
    assert(!sil_gc_finish_report(&state, lost_report, true, 180001));
    assert(sil_gc_finish_report(&state, state.serial, true, 180001));
    assert(has(&state.confirmed, 63) && has(&state.remote, SIL_ACH_TENFOLD_DOOM));
    assert(!sil_gc_begin_report(&state, 180002));
    assert(!sil_gc_begin_load(&state, 359999));
    assert(sil_gc_begin_load(&state, 360000));
    uint64_t old_account = state.serial;
    sil_gc_reset(&state);
    assert(!state.remote.words[0] && !state.remote.words[1] && !state.loaded);
    assert(!sil_gc_finish_load(&state, old_account, &all, &all, true, 360000));

    /* Empty server catalogues preserve wanted bits until registration exists. */
    sil_game_center_masks empty = {0};
    sil_gc_want(&state, &all);
    assert(sil_gc_begin_load(&state, 0));
    assert(sil_gc_finish_load(&state, state.serial, &empty, &empty, true, 0));
    assert(!sil_gc_begin_report(&state, 1));
    assert(sil_gc_begin_load(&state, 300000));
    assert(sil_gc_finish_load(&state, state.serial, &available, &empty, true, 300000));
    assert(sil_gc_begin_report(&state, 300000));
    uint64_t previous_account_report = state.serial;
    sil_gc_reset(&state);
    assert(!sil_gc_finish_report(&state, previous_account_report, true, 300001));

    /* A fresh unlock during an active report is never acknowledged with it. */
    bit(&extra, 63);
    sil_gc_want(&state, &extra);
    assert(sil_gc_begin_load(&state, 0));
    assert(sil_gc_finish_load(&state, state.serial, &available, &empty, true, 0));
    assert(sil_gc_begin_report(&state, 0));
    bit(&extra, 64);
    sil_gc_want(&state, &extra);
    assert(sil_gc_finish_report(&state, state.serial, true, 1));
    assert(has(&state.remote, 63) && !has(&state.remote, 64));
    assert(sil_gc_begin_report(&state, 1));
    assert(!has(&state.pending, 63) && has(&state.pending, 64));
    assert(sil_gc_finish_report(&state, state.serial, true, 2));
    assert(sil_gc_begin_load(&state, 300000));
    assert(sil_gc_finish_load(&state, state.serial, NULL, NULL, false, 300000));
    assert(!sil_gc_begin_load(&state, 359999));
    assert(!sil_gc_begin_report(&state, 360000));
    assert(sil_gc_begin_load(&state, 360000));

    /* Desktop builds link the real public header as safe no-ops. */
    sil_game_center_init(); sil_game_center_sync(all.words);
    assert(!sil_game_center_unlocked(0) && !sil_game_center_available() && !sil_game_center_show());
    puts("PASS Game Center production state: registered IDs, multiword masks, download/upload failures, acknowledgement, retries, lost replies, account reset and offline queue");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    compiler = shutil.which("cc") or "C:/msys64/mingw64/bin/cc.exe"
    env = os.environ.copy()
    if os.name == "nt":
        env["PATH"] = "C:/msys64/mingw64/bin;C:/msys64/usr/bin;" + env["PATH"]
    exe = OUT / ("check.exe" if os.name == "nt" else "check")
    subprocess.run([compiler, "-std=c17", "-Wall", "-Wextra", "-Werror", "-UNDEBUG",
                    "-I", str(ROOT / "src"), str(source), "-o", str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=15)
    entitlements = plistlib.loads((ROOT / "ios/Sil-More.entitlements").read_bytes())
    assert entitlements["com.apple.developer.game-center"] is True
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    assert "src/achievements-game-center.m" in cmake and '"-framework GameKit"' in cmake
    assert "ios/Sil-More.entitlements" in cmake and "-fobjc-arc" in cmake
    main_source = (ROOT / "src/main.c").read_text(encoding="utf-8")
    assert main_source.index("sil_achievements_init();") < main_source.index("sil_game_center_init();")
    core = (ROOT / "src/achievements.c").read_text(encoding="utf-8")
    assert "sil_game_center_sync(masks);" in core and "| sil_game_center_unlocked(word)" in core
    ui = (ROOT / "src/achievements-ui.c").read_text(encoding="utf-8")
    assert "sil_game_center_show()" in ui and '"Game Center"' in ui
    print("PASS Game Center build, entitlement, startup, sync and viewer wiring (Apple SDK/device test remains separate)")


if __name__ == "__main__":
    main()

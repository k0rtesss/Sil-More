#!/usr/bin/env python3
"""Run native logger filtering tests independently of the game frontend."""
from pathlib import Path
import os
import subprocess
import uuid

import qa9_sdl_fixture as runtime

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/logging-check"
OUT.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
exe = OUT / "check.exe"
subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-O2", "-Wall", "-Wextra",
    str(ROOT / "scripts/tests/log-test.c"), str(ROOT / "src/log/log.c"), "-o", str(exe)],
    cwd=ROOT, env=env, check=True)
subprocess.run([str(exe), str(OUT / "test.log")], env=env, check=True)

# Exercise the actual Release bootstrap and popup diagnostics with disposable
# paths. The standalone fixture keeps assertions while linking Release objects.
runtime.OUT = OUT / "runtime"
runtime.HARNESS = runtime.HARNESS.replace('#include "angband.h"',
    '#define SIL_PERF_DIAGNOSTICS\n#include "angband.h"\n#include "log/log.h"\n#include "log/perf.h"')
runtime.HARNESS = runtime.HARNESS.replace("    int last = 0;", r'''
    log_info("LOGGER_INFO_SENTINEL");
    log_debug("LOGGER_DEBUG_SENTINEL");
    log_trace("LOGGER_TRACE_SENTINEL");
    character_dungeon = true;
    p_ptr->playing = true; p_ptr->is_dead = false;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 20;
    p_ptr->py = p_ptr->px = 10;
    cave_o_idx[10][11] = 1;
    sil_popup_trace_begin(10, 10, 10, 11);
    sil_popup_trace_stage("logger-test");
    sil_popup_trace_end("logger-test-end");
    int last = 0;''')
exe, env = runtime.build_harness()
for level, perf in ((None, False), ("DEBUG", False), ("TRACE", False), (None, True)):
    profile = OUT / ("profile-森-" + uuid.uuid4().hex[:8])
    profile.mkdir()
    run_env = dict(env, CONTROL_TEST_PROFILE=str(profile))
    run_env.pop("SIL_LOG_LEVEL", None)
    run_env.pop("SIL_PERF", None)
    if level: run_env["SIL_LOG_LEVEL"] = level
    if perf: run_env["SIL_PERF"] = "1"
    subprocess.run([str(exe), "--windowed"], cwd=ROOT, env=run_env, check=True, timeout=30,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    log = (profile / "log.txt").read_text(encoding="utf-8")
    assert "LOGGER_INFO_SENTINEL" in log
    assert ("LOGGER_DEBUG_SENTINEL" in log) == (level in ("DEBUG", "TRACE"))
    assert ("LOGGER_TRACE_SENTINEL" in log) == (level == "TRACE")
    assert ("[POPUP_DELAY]" in log) == perf
print("Release INFO default, DEBUG/TRACE overrides and opt-in popup traces: PASS")

# A directory occupying the log filename denies the open even for an
# administrator. Keep the rest of this fresh profile writable.
profile = OUT / ("blocked-log-" + uuid.uuid4().hex[:8])
(profile / "log.txt").mkdir(parents=True)
run_env = dict(env, CONTROL_TEST_PROFILE=str(profile), SIL_LOG_LEVEL="INFO")
completed = subprocess.run([str(exe), "--windowed"], cwd=ROOT, env=run_env,
    check=True, timeout=30, capture_output=True)
assert b"File logging is unavailable" in completed.stderr
assert b"LOGGER_INFO_SENTINEL" in completed.stderr
assert (profile / "log.txt").is_dir()
print("Unicode log paths and startup with stderr as the only sink: PASS")

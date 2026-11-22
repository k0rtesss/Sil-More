#!/usr/bin/env python3
"""Exercise installed profile startup using a disposable copy of a desktop package."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


WINDOWS_FOLDER_OVERRIDES = r"""
#include <stdio.h>
#include <SDL3/SDL.h>

const char *SDLCALL __wrap_SDL_GetUserFolder(SDL_Folder folder)
{
    if (folder == SDL_FOLDER_HOME || folder == SDL_FOLDER_SAVEDGAMES)
        return SDL_getenv("SIL_TEST_HOME");
    return NULL;
}

char *SDLCALL __wrap_SDL_GetPrefPath(const char *organization, const char *application)
{
    (void)organization;
    (void)application;
    const char *path = SDL_getenv("SIL_TEST_PREF_PATH");
    return path ? SDL_strdup(path) : NULL;
}

bool SDLCALL __wrap_SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags flags,
    const char *title, const char *message, SDL_Window *window)
{
    (void)flags;
    (void)window;
    fprintf(stderr, "%s\n%s\n", title, message);
    return true;
}
"""


def link_windows_test_exe(exe, build, gcc):
    build = build.resolve()
    env = dict(os.environ, PATH=os.pathsep.join((
        str(gcc.parent), str(gcc.parents[2] / "usr" / "bin"), os.environ["PATH"]
    )))
    source = exe.parent.parent / "test-startup-folders.c"
    source.write_text(WINDOWS_FOLDER_OVERRIDES, encoding="ascii")
    sdl_headers = Path(__file__).resolve().parents[1] / "external" / "SDL" / "include"
    subprocess.run(
        [gcc, "-std=c17", "-O2", "-mwindows", "-I", sdl_headers, source,
         "@CMakeFiles/sil-more.dir/objects1.rsp", "@CMakeFiles/sil-more.dir/linkLibs.rsp",
         "-Wl,--wrap=SDL_GetUserFolder", "-Wl,--wrap=SDL_GetPrefPath",
         "-Wl,--wrap=SDL_ShowSimpleMessageBox", "-o", exe],
        cwd=build, env=env, check=True,
    )
    exe_deps = subprocess.check_output([gcc.with_name("objdump.exe"), "-p", exe], text=True, env=env)
    if "steam_api" in exe_deps.lower():
        raise ValueError("Use a standard non-Steam build for this check")


def run_startup(game, exe, folders, error=None, notice=None, prefs_available=True):
    home, prefs = folders
    home.mkdir(parents=True, exist_ok=True)
    prefs.mkdir(parents=True, exist_ok=True)
    clean_path = r"C:\Windows\System32;C:\Windows" if os.name == "nt" else os.defpath
    env = dict(
        os.environ,
        PATH=clean_path,
        HOME=str(home),
        XDG_DATA_HOME=str(prefs.parents[1]),
        SIL_TEST_HOME=str(home),
        SDL_VIDEO_DRIVER="dummy",
        SDL_RENDER_DRIVER="software",
        SDL_AUDIO_DRIVER="dummy",
        SIL_HANDHELD="desktop",
        SIL_LOG_LEVEL="INFO",
    )
    for name in ("ANGBAND_PATH", "SIL_MORE_CONTROL_DIR", "SIL_TEST_PREF_PATH", "DISPLAY", "WAYLAND_DISPLAY"):
        env.pop(name, None)
    if prefs_available:
        env["SIL_TEST_PREF_PATH"] = str(prefs)

    log = prefs / "log.txt"
    log.unlink(missing_ok=True)
    stderr = game.parent / "startup-stderr.txt"
    with stderr.open("w") as output, subprocess.Popen(
        [exe, "--", "--windowed"], cwd=game, env=env,
        stdout=subprocess.DEVNULL, stderr=output,
    ) as process:
        def read_output():
            messages = stderr.read_text(encoding="utf-8", errors="replace")
            details = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else ""
            return messages, details

        try:
            deadline = time.monotonic() + 30
            while process.poll() is None and time.monotonic() < deadline:
                if error is None:
                    messages, details = read_output()
                    if "initial_menu: ENTERED" in messages + details:
                        if prefs_available:
                            assert "initial_menu: ENTERED" in details
                        elif os.name == "nt":
                            assert "File logging is unavailable" in messages
                        if notice and os.name == "nt":
                            assert notice in messages, messages
                        elif os.name == "nt":
                            assert "Existing character files are still in your save folder." not in messages
                        return messages + details
                time.sleep(0.1)
            messages, details = read_output()
            displayed = messages if os.name == "nt" else messages + details
            assert error is not None and process.returncode == 1 and error in displayed, (
                process.returncode, error, details[-2200:], messages
            )
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()


def check_profiles(case_root, game, exe):
    def fixture(name):
        case = case_root / name
        home = case / "Saved Games"
        prefs = case / "prefs" / "Sil-QH" / "sil-more"
        return (home, prefs), home / "sil-more", prefs / "alternate-profile"

    folders, primary, alternate = fixture("healthy")
    stale = folders[1] / "save" / "unmarked.sav"
    stale.parent.mkdir(parents=True)
    stale.write_bytes(b"old unmarked profile")
    misc = primary / "save" / "desktop.ini"
    misc.parent.mkdir(parents=True)
    misc.write_text("not a character")
    score = primary / "meta" / "scores.raw"
    score.parent.mkdir(parents=True)
    score.touch()
    run_startup(game, exe, folders)
    assert score.stat().st_size > 0 and not alternate.exists()
    assert stale.read_bytes() == b"old unmarked profile"
    expected_score = score.read_bytes()
    score.chmod(0o444)
    try:
        run_startup(game, exe, folders)
        assert score.read_bytes() == expected_score and not alternate.exists()
    finally:
        score.chmod(0o644)
    if os.name == "nt":
        run_startup(game, exe, folders, prefs_available=False)

    for name in ("root", "meta", "scores.raw"):
        folders, primary, alternate = fixture(name)
        if name == "scores.raw":
            blocked = primary / "meta" / name
            blocked.mkdir(parents=True)
        else:
            blocked = primary if name == "root" else primary / name
            blocked.parent.mkdir(parents=True)
            blocked.write_bytes(b"preserve obstruction")
        run_startup(game, exe, folders, notice="Your usual save location is unavailable.")
        if name != "scores.raw":
            assert blocked.read_bytes() == b"preserve obstruction"
        alternate_score = alternate / "meta" / "scores.raw"
        assert alternate_score.stat().st_size > 0
        obtained = alternate_score.read_bytes()
        blocked.rename(folders[0].parent / "original_obstruction")
        trace = run_startup(game, exe, folders)
        assert f"Using alternate save location: {alternate}" in trace
        assert alternate_score.read_bytes() == obtained
        assert not (primary / "meta" / "scores.raw").exists()
        if name == "meta":
            saved = primary / "save" / "synced-character"
            saved.parent.mkdir(parents=True, exist_ok=True)
            saved.write_bytes(b"canonical character synchronized after fallback")
            trace = run_startup(game, exe, folders)
            assert f"Using alternate save location: {alternate}" in trace
            assert "also contains progress" in trace
            assert saved.read_bytes() == b"canonical character synchronized after fallback"
            assert alternate_score.read_bytes() == obtained

    folders, primary, alternate = fixture("existing")
    saved = primary / "save" / "existing-character"
    saved.parent.mkdir(parents=True)
    saved.write_bytes(b"preserve character")
    blocked = primary / "meta"
    blocked.write_bytes(b"preserve obstruction")
    run_startup(game, exe, folders, error="Could not open the existing save location.")
    assert saved.read_bytes() == b"preserve character"
    assert blocked.read_bytes() == b"preserve obstruction"
    assert not alternate.exists()

    folders, primary, alternate = fixture("missing-ledger")
    saved = primary / "save" / "existing-character"
    saved.parent.mkdir(parents=True)
    saved.write_bytes(b"preserve character without a ledger")
    run_startup(game, exe, folders, notice="Existing character files are still in your save folder.")
    assert (primary / "meta" / "scores.raw").stat().st_size > 0
    assert saved.read_bytes() == b"preserve character without a ledger"
    assert not alternate.exists()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="Staged non-Steam standard desktop package")
    parser.add_argument("--build", type=Path, default=Path("build-standard"),
                        help="Configured Windows non-Steam build directory")
    parser.add_argument("--gcc", type=Path, default=Path("C:/msys64/mingw64/bin/gcc.exe"))
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="sil-startup-profiles-") as directory:
        work = Path(directory)
        game = work / "game"
        shutil.copytree(args.package.resolve(), game)
        exe = game / ("sil-more.exe" if os.name == "nt" else "sil-more")
        if os.name == "nt":
            link_windows_test_exe(exe, args.build, args.gcc.resolve())
        elif sys.platform.startswith("linux"):
            deps = subprocess.check_output(["readelf", "-d", exe], text=True)
            if "steam_api" in deps.lower():
                raise ValueError("Use a standard non-Steam build for this check")
        else:
            raise ValueError("This check supports Windows and Linux")
        old_log = game / "log.txt"
        if old_log.is_file():
            old_log.chmod(0o644)
        old_log.write_bytes(b"previous install-side log")
        old_log.chmod(0o444)
        try:
            check_profiles(work / "profiles-森", game, exe)
            assert old_log.read_bytes() == b"previous install-side log"
        finally:
            old_log.chmod(0o644)
    print("Unicode profiles, read-only install log, alternate saves, relaunch and existing save protection passed")


if __name__ == "__main__":
    main()

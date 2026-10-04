#!/usr/bin/env python3
"""Test the real SDL control adapter in an isolated, offscreen game harness.

Run build-cmake.bat standard first. Reuses CMake's compiled objects and link
settings, like check_help_controller_runtime.py. All writable game paths are
inside a unique scripts/output directory; user saves/config/scores are untouched.
"""
from pathlib import Path
import json
import os
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts" / "output" / "game-control-check"
sys.path.insert(0, str(ROOT / "tools"))
from game_control import ControlError, GameControl, parse_key, publish_json, read_json

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "log/bootstrap.h"
#include "ui/question.h"
#include <assert.h>

static cptr make_path(const char* root, const char* tail, bool writable)
{
    char path[1200];
    assert(path_build(path, sizeof(path), root, tail));
    if (writable) assert(SDL_CreateDirectory(path));
    return str_dup(path);
}

int main(int argc, char** argv)
{
    const char* assets = SDL_getenv("CONTROL_TEST_ASSETS");
    const char* profile = SDL_getenv("CONTROL_TEST_PROFILE");
    assert(assets && profile);
    const char* log_base = SDL_getenv("CONTROL_TEST_LOG_BASE");
    init_logger(true, log_base ? log_base : argv[0]);
    ANGBAND_DIR = make_path(assets, "", false);
    ANGBAND_DIR_EDIT = make_path(assets, "edit", false);
    ANGBAND_DIR_FILE = make_path(assets, "file", false);
    ANGBAND_DIR_HELP = make_path(assets, "help", false);
    ANGBAND_DIR_INFO = make_path(assets, "info", false);
    ANGBAND_DIR_PREF = make_path(assets, "pref", false);
    ANGBAND_DIR_XTRA = make_path(assets, "xtra", false);
    ANGBAND_DIR_SCRIPT = make_path(assets, "script", false);
    ANGBAND_DIR_USER = make_path(profile, "user", true);
    ANGBAND_DIR_SAVE = make_path(profile, "save", true);
    ANGBAND_DIR_DATA = make_path(profile, "data", true);
    ANGBAND_DIR_BONE = make_path(profile, "bone", true);
    ANGBAND_DIR_APEX = make_path(profile, "meta", true);
    ANGBAND_DIR_METARUN = make_path(profile, "meta/metaruns", true);
    assert(init_sdl(argc, argv) == 0);
    if (SDL_getenv("CONTROL_TEST_DISABLED")) {
        assert(sdl_control_wait_timeout(-1) == -1);
        assert(!sdl_control_poll(true));
        sdl_quit_hook(NULL);
        return 0;
    }
    init_angband();
    if (SDL_getenv("CONTROL_TEST_INTERACTIVE")) {
        bool start_new;
        if (initial_menu(&start_new) == NAV_OK) (void)play_game();
        sdl_control_shutdown();
        return 0;
    }
    sdl_welcome_screen_hide();
    screen_set_startup_supporting_panes_hidden(false);
    screen_set_startup_touch_pane_hidden(false);
    int last = 0, choice = -1, confirm = -1, custom = -1;
    char name[128] = "";
    while (true) {
        char label[256];
        Term_clear();
        Term_putstr(0, 0, -1, TERM_WHITE, "CONTROL READY");
        strnfmt(label, sizeof(label), "LAST=%d MENU=%d CONFIRM=%d CUSTOM=%d NAME=%s",
            last, choice, confirm, custom, name);
        Term_putstr(0, 2, -1, TERM_WHITE, label);
        Term_fresh();
        last = (unsigned char)inkey();
        if (last == 'q') {
            (void)sdl_control_poll(true);
            sdl_control_shutdown();
            return 0;
        } else if (last == 't') {
            assert(term_get_string("Control name", name, sizeof(name)));
        } else if (last == 'm') {
            const ui_question_option options[] = {
                {'a', "First answer", TERM_WHITE, false},
                {'b', "Second answer", TERM_WHITE, false}
            };
            choice = ui_question_ask_overlay("Control menu", NULL, options, 2,
                UI_QUESTION_GLOBAL, UI_QUESTION_GLOBAL, 0);
        } else if (last == 'y') {
            confirm = get_check("Confirm control test?");
        } else if (last == 'i') {
            do_cmd_inven_direct();
        } else if (last == 'e') {
            do_cmd_equip_direct();
        } else if (last == 'c') {
            sdl_view* d = sdl_view_from_term(Term);
            SDL_SetRenderTarget(g_state.renderer, NULL);
            SDL_SetRenderDrawColor(g_state.renderer, 100, 20, 150, 255);
            SDL_RenderClear(g_state.renderer);
            sdl_control_present_modal(g_state.renderer);
            sdl_restore_render_target(d);
            custom = sdl_touch_tutorial_wait_action(SDL_GetTicksNS() + 90000000ULL);
        } else if (last == 'd') {
            Term_xtra(TERM_XTRA_DELAY, 800);
        }
    }
}
'''


def build_harness() -> tuple[Path, dict]:
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake_dir = BUILD / "CMakeFiles" / "sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith("/src/main.c.obj")]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env.pop("SIL_MORE_CONTROL_DIR", None)
    env["PATH"] = os.pathsep.join([
        str(BUILD / "_deps/SDL"), str(BUILD / "_deps/SDL_ttf"),
        str(BUILD / "_deps/SDL_image"), str(BUILD / "_deps/SDL_mixer"),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    return exe, env


def terminal(response: dict) -> str:
    return "\n".join(response["state"]["terminal"])


def expect_ok(response: dict) -> dict:
    assert response["ok"], response
    assert response["state"]["waiting_for_input"], response
    assert response["state"]["video_driver"] == "dummy", "Test must not open a desktop window"
    return response


def wait_ready(control: GameControl, process: subprocess.Popen) -> dict:
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        assert process.poll() is None, f"Harness exited {process.returncode}; check {OUT / 'log.txt'}"
        try:
            response = control.observe(capture=False)
            if "CONTROL READY" in terminal(response) and response["state"]["waiting_for_input"]:
                return expect_ok(response)
        except ControlError:
            pass
        time.sleep(0.05)
    raise AssertionError("Harness never reached the control prompt")


def raw_request(control: GameControl, request: dict) -> dict:
    publish_json(control.directory / "request.json", request)
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        response_path = control.directory / "response.json"
        if response_path.exists():
            response = read_json(response_path)
            if response.get("id") == request["id"]:
                return response
        time.sleep(0.025)
    raise AssertionError("Raw request did not receive a reply")


def run_checks(exe: Path, env: dict) -> None:
    profile = Path(tempfile.mkdtemp(prefix="session-", dir=OUT))
    control = GameControl(profile / "control")
    env = dict(env, CONTROL_TEST_PROFILE=str(profile))
    process = subprocess.Popen([str(exe), "--windowed", "--tiles", "--control-dir",
                                str(control.directory)], cwd=ROOT, env=env)
    try:
        initial = wait_ready(control, process)
        output = profile / "observation.png"
        response = expect_ok(control.observe(output=output))
        assert output.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
        assert struct.unpack(">II", output.read_bytes()[16:24]) == (
            response["state"]["width"], response["state"]["height"])
        assert response["state"]["turn"] == initial["state"]["turn"]
        assert not response["input_applied"]
        cli = subprocess.run([sys.executable, str(ROOT / "tools/game_control.py"),
                              "--dir", str(control.directory), "status"],
                             cwd=ROOT, env=env, capture_output=True, text=True,
                             encoding="utf-8", check=True, timeout=20)
        assert json.loads(cli.stdout)["state"]["video_driver"] == "dummy"

        for chord, value in [("a", 97), ("A", 65), ("Up", 56), ("KP3", 51),
                             ("Ctrl+a", 1), ("+", 43), (">", 62), ("!", 33)]:
            result = expect_ok(control.key(chord))
            assert result["input_applied"] and f"LAST={value} " in terminal(result), result

        expect_ok(control.key("m", output=profile / "menu.png"))
        expect_ok(control.key("Down"))
        result = expect_ok(control.key("Enter"))
        assert "MENU=1" in terminal(result), result
        result = expect_ok(control.key("m"))
        result = expect_ok(control.click(result["state"]["width"] / 2,
                                         result["state"]["height"] / 2))
        assert "MENU=0" in terminal(result), result
        expect_ok(control.key("y"))
        result = expect_ok(control.key("y"))
        assert "CONFIRM=1" in terminal(result), result

        result = expect_ok(control.key("t"))
        assert result["state"]["text_input"]
        expect_ok(control.text("Efrém"))
        expect_ok(control.key("A"))
        result = expect_ok(control.key("Enter"))
        # The terminal is a legacy byte display; UTF-8 bytes are intentionally
        # represented as spaces in the text snapshot. The prompt keeps UTF-8.
        assert "NAME=Efr  mA" in terminal(result), result
        assert not result["state"]["text_input"]

        result = expect_ok(control.key("c", output=profile / "custom-modal.png"))
        assert result["state"]["input_context"] == "modal", result
        result = expect_ok(control.key("Space"))
        assert result["state"]["input_context"] == "terminal", result
        assert "CUSTOM=1" in terminal(result), result
        quick = GameControl(control.directory, timeout=0.6)
        result = quick.key("d", capture=False)
        assert result["ok"] and result["input_applied"]
        assert not result["state"]["waiting_for_input"], result
        result = expect_ok(control.key("a", capture=False))
        assert "LAST=97 " in terminal(result), result

        # Force Windows' read-vs-rename conflict. The completed reply must be
        # retried after this handle closes, without sending the key again.
        import ctypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32,
                                      ctypes.c_uint32, ctypes.c_void_p,
                                      ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.CreateFileW(str(control.directory / "response.json"),
                                    0x80000000, 1, None, 3, 0, None)
        assert handle not in (None, ctypes.c_void_p(-1).value)
        release = threading.Timer(0.15, kernel.CloseHandle, args=[handle])
        release.start()
        try:
            expect_ok(control.key("a", capture=False))
        finally:
            release.join()

        # A valid atomically published request can temporarily deny readers
        # while allowing deletion (e.g. a Windows file-sharing conflict). The
        # game must leave it unclaimed instead of reporting malformed JSON.
        kernel.WriteFile.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32,
                                     ctypes.POINTER(ctypes.c_uint32), ctypes.c_void_p]
        kernel.FlushFileBuffers.argtypes = [ctypes.c_void_p]
        request = dict(protocol=1, session=control.session()["session"],
                       id=uuid.uuid4().hex, expires_at_ms=int((time.time() + 5) * 1000),
                       op="observe", capture=False)
        data = json.dumps(request).encode("utf-8")
        temp = control.directory / "read-busy.tmp"
        handle = kernel.CreateFileW(str(temp), 0x40000000, 4, None, 2, 0x80, None)
        assert handle not in (None, ctypes.c_void_p(-1).value)
        written = ctypes.c_uint32()
        try:
            assert kernel.WriteFile(handle, data, len(data), ctypes.byref(written), None)
            assert written.value == len(data) and kernel.FlushFileBuffers(handle)
            os.replace(temp, control.directory / "request.json")
            time.sleep(0.15)
            assert (control.directory / "request.json").exists(), "Unreadable request was lost"
            assert read_json(control.directory / "response.json").get("id") != request["id"]
        finally:
            kernel.CloseHandle(handle)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            response = read_json(control.directory / "response.json")
            if response.get("id") == request["id"]:
                break
            time.sleep(0.025)
        else:
            raise AssertionError("Readable request was not retried")
        expect_ok(response)
        assert not response["input_applied"]

        for op, fields in [("text", {"text": "not a prompt"}),
                           ("key", {"key": "NoSuchKey"}),
                           ("key", {"key": "a", "modifiers": ["gui"]}),
                           ("click", {"x": -1, "y": 0}),
                           ("click", {"x": float("inf"), "y": 0})]:
            if op == "click" and fields["x"] == float("inf"):
                try:
                    control.request(op, **fields)
                    raise AssertionError("Nonfinite JSON should be rejected by client")
                except ValueError:
                    continue
            result = control.request(op, **fields)
            assert not result["ok"] and not result["input_applied"], result

        base = dict(protocol=1, session=control.session()["session"], op="key", key="a",
                    expires_at_ms=int((time.time() + 5) * 1000), capture=False)
        for overrides in [{"session": "previous-game"}, {"expires_at_ms": 1},
                          {"protocol": 2}, {"op": "shell"}, {"capture": "yes"}]:
            result = raw_request(control, dict(base, id=uuid.uuid4().hex, **overrides))
            assert not result["ok"] and not result["input_applied"], result

        # Replay the last request id; it must fail before dispatch, including
        # when a writer republishes a request after losing its response.
        request = dict(base, id=uuid.uuid4().hex)
        result = raw_request(control, request)
        expect_ok(result)
        (control.directory / "response.json").unlink()
        result = raw_request(control, request)
        assert not result["ok"] and "Duplicate" in result["error"], result

        for payload in (b"{broken", b" " * 16385):
            response_path = control.directory / "response.json"
            response_path.unlink(missing_ok=True)
            temp = control.directory / "raw.tmp"
            temp.write_bytes(payload)
            os.replace(temp, control.directory / "request.json")
            deadline = time.monotonic() + 5
            while not response_path.exists() and time.monotonic() < deadline:
                time.sleep(0.025)
            result = read_json(response_path)
            assert not result["ok"] and not result["input_applied"], result

        for key in ("i", "e"):
            expect_ok(control.key(key, output=profile / f"{key}-overlay.png"))
            expect_ok(control.key("Escape"))
        result = control.key("q", capture=False)
        assert result["ok"] and result["input_applied"] and result["closed"], result
        process.wait(timeout=5)
        assert process.returncode == 0
        assert not read_json(control.directory / "session.json")["running"]
        disabled_profile = profile / "disabled"
        subprocess.run([str(exe), "--windowed"], cwd=ROOT,
                       env=dict(env, CONTROL_TEST_PROFILE=str(disabled_profile),
                                CONTROL_TEST_DISABLED="1"), check=True, timeout=15)
        assert not (disabled_profile / "control").exists()

        # The launcher uses the executable's directory for relative lib assets.
        # Place a unique temporary harness there, with every writable path and
        # its log still redirected to the isolated fixture profile.
        launch_exe = ROOT / ("control-check-" + uuid.uuid4().hex + ".exe")
        launch_profile = profile / "launcher"
        launch_control = GameControl(launch_profile / "control")
        shutil.copyfile(exe, launch_exe)
        process_handle = None
        kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
        kernel.OpenProcess.restype = ctypes.c_void_p
        kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        kernel.TerminateProcess.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        try:
            cli = subprocess.run([sys.executable, str(ROOT / "tools/game_control.py"),
                                  "--dir", str(launch_control.directory), "launch", "--headless",
                                  "--exe", str(launch_exe)], cwd=ROOT,
                                 env=dict(env, CONTROL_TEST_PROFILE=str(launch_profile),
                                          CONTROL_TEST_LOG_BASE=str(launch_profile / "check.exe")),
                                 capture_output=True, text=True, encoding="utf-8", check=True, timeout=30)
            launched = json.loads(cli.stdout)
            assert launched["headless"] and launched["running"]
            process_handle = kernel.OpenProcess(0x100001, False, launched["pid"])
            assert process_handle
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                response = launch_control.observe(capture=False)
                if "CONTROL READY" in terminal(response) and response["state"]["waiting_for_input"]:
                    break
            else:
                raise AssertionError("CLI-launched game never reached its prompt")
            expect_ok(response)
            expect_ok(launch_control.key("a", output=launch_profile / "launch.png"))
            result = launch_control.key("q", capture=False)
            assert result["ok"] and result["closed"]
            assert kernel.WaitForSingleObject(process_handle, 5000) == 0
        finally:
            if process_handle:
                if kernel.WaitForSingleObject(process_handle, 0) == 258:
                    kernel.TerminateProcess(process_handle, 1)
                    kernel.WaitForSingleObject(process_handle, 5000)
                kernel.CloseHandle(process_handle)
            launch_exe.unlink(missing_ok=True)
        print(f"Game control checks: PASS (CLI launch, PNGs, input, menus, prompts, guards)\nArtifacts: {profile}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)


def main() -> None:
    assert parse_key("Ctrl+Shift+s") == ("s", ["ctrl", "shift"])
    assert parse_key("Ctrl++") == ("+", ["ctrl"])
    exe, env = build_harness()
    if "--interactive" in sys.argv:
        profile = Path(tempfile.mkdtemp(prefix="interactive-", dir=OUT))
        env.update(CONTROL_TEST_PROFILE=str(profile), CONTROL_TEST_INTERACTIVE="1")
        process = subprocess.Popen([str(exe), "--windowed", "--control-dir", str(profile / "control")],
                                   cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(json.dumps({"pid": process.pid, "control": str(profile / "control")}))
    else:
        run_checks(exe, env)


if __name__ == "__main__":
    main()

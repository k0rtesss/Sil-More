#!/usr/bin/env python3
"""Continue past the shipped Linux welcome screen on a 53x16 Deck grid.

Run under xvfb-run -a -s '-screen 0 1280x800x24 -nolisten tcp'. The package
is copied and profiles/logs stay private. This tests the shipped executable,
not a harness relinked from current sources; it is not physical-device QA.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from game_control import ControlError, GameControl


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def read_log(log_paths):
    for path in log_paths:
        if path.is_file():
            return path.read_text(errors="replace")
    return ""


def check_startup(game, output, env, label):
    control = GameControl(output / f"control-{label}", timeout=30)
    control.directory.mkdir()
    run_env = dict(env, SIL_MORE_CONTROL_DIR=str(control.directory))
    # Installed builds use SDL's per-user data directory. Older packages
    # wrote the log beside the executable.
    log_paths = (Path(env["XDG_DATA_HOME"]) / "Sil-QH" / "sil-more" / "log.txt",
                 game / "log.txt")
    for path in log_paths:
        path.unlink(missing_ok=True)
    with (output / f"{label}.stdout").open("w") as stream:
        process = subprocess.Popen([str(game / "sil-more.sh"), "--", "--fullscreen"],
                                   cwd=game, env=run_env, stdout=stream, stderr=stream)
        try:
            deadline = time.monotonic() + 60
            while "initial_menu: ENTERED" not in read_log(log_paths):
                require(process.poll() is None,
                        f"{label}: exited before welcome ({process.returncode})")
                require(time.monotonic() < deadline, f"{label}: welcome timed out")
                time.sleep(0.1)
            deadline = time.monotonic() + 30
            while True:
                ready = control.observe(capture=False)
                if ready.get("state", {}).get("waiting_for_input"):
                    break
                require(time.monotonic() < deadline, f"{label}: welcome is not ready")
                time.sleep(0.1)
            before = control.observe(output=output / f"{label}-welcome.png")
            (output / f"{label}-welcome.json").write_text(json.dumps(before, indent=2))
            state = before["state"]
            grid = state["terminal"]
            require((state["width"], state["height"]) == (1280, 800),
                    "Use an isolated 1280x800 X11 display")
            require(len(grid) == 16 and all(len(row) == 53 for row in grid),
                    f"{label}: expected 53x16, got "
                    f"{len(grid[0]) if grid else 0}x{len(grid)}")
            require(state["waiting_for_input"], f"{label}: welcome is not ready")
            # The native welcome controller handler maps physical A to Enter.
            # Wait for the next prompt, including the real play_game size check.
            after = control.key("Enter", output=output / f"{label}-continued.png")
            (output / f"{label}-continued.json").write_text(json.dumps(after, indent=2))
            require(after.get("ok") and not after.get("closed"),
                    f"{label}: Continue closed the game")
            state = after["state"]
            require(state["waiting_for_input"] and (state["modal"] or state["dungeon"]),
                    f"{label}: Continue did not reach the next story/creation/game prompt")
            log = read_log(log_paths)
            require("play_game: FUNCTION ENTERED" in log, f"{label}: play_game not entered")
            require("main window too small" not in log and "main window is too small" not in log,
                    f"{label}: startup rejected the compact grid")
            require(process.poll() is None, f"{label}: exited after Continue")
            print(f"{label}: 53x16 welcome -> Continue -> next input prompt PASS", flush=True)
            return {"profile": label, "columns": 53, "rows": 16, "continued": True}
        finally:
            for _ in range(8):
                if process.poll() is not None:
                    break
                try:
                    if control.key("Escape", capture=False).get("closed"):
                        break
                except ControlError:
                    break
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            for path in log_paths:
                if path.is_file():
                    shutil.copy2(path, output / f"{label}.log")
                    break


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if sys.platform != "linux" or not os.environ.get("DISPLAY"):
        parser.error("Run on native Linux inside an isolated 1280x800 X11 display")
    package, output = args.package.resolve(), args.output.resolve()
    for name in ("sil-more", "sil-more.sh", "lib"):
        require((package / name).exists(), f"Missing package entry: {name}")
    output.mkdir(parents=True)  # Never reuse dirty test state.
    game = output / "game"
    shutil.copytree(package, game)
    (game / "steam_appid.txt").write_text("5442160\n")
    profile = output / "profile"
    profile.mkdir()
    env = dict(os.environ, HOME=str(profile), XDG_DATA_HOME=str(profile / "data"),
               XDG_CONFIG_HOME=str(profile / "config"), SIL_HANDHELD="steamdeck",
               SDL_VIDEO_DRIVER="x11", SDL_RENDER_DRIVER="software", SDL_AUDIO_DRIVER="dummy")
    results = []
    try:
        for label in ("fresh", "reload"):
            results.append(check_startup(game, output, env, label))
    finally:
        receipt = {"executable_sha256": hashlib.sha256((game / "sil-more").read_bytes()).hexdigest(),
                   "package": str(package), "checks": results, "physical_device_tested": False}
        (output / "startup-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()

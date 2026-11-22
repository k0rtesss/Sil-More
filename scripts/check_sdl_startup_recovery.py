"""Exercise window restoration and unavailable audio with an isolated Windows portable build."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from game_control import ControlError, GameControl


def launch(game, audio_driver, case, usable_bounds=None):
    control = GameControl(game / case, timeout=10)
    control.directory.mkdir()
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIO_DRIVER=audio_driver, SIL_HANDHELD="desktop", SIL_LOG_LEVEL="DEBUG",
               SIL_MORE_CONTROL_DIR=str(control.directory))
    env.pop("ANGBAND_PATH", None)
    if usable_bounds:
        env["SDL_DISPLAY_USABLE_BOUNDS"] = usable_bounds
    log = game / "log.txt"
    with subprocess.Popen([str(game / "sil-more.exe")], cwd=game, env=env) as process:
        try:
            deadline = time.monotonic() + 30
            while not (control.directory / "session.json").exists():
                if process.poll() is not None or time.monotonic() >= deadline:
                    raise AssertionError(f"{case} failed to reach Welcome\n{log.read_text()[-1500:]}")
                time.sleep(.05)
            while not control.observe(capture=False)["state"]["waiting_for_input"]:
                if time.monotonic() >= deadline:
                    raise AssertionError(f"{case} never reached an input prompt")
                time.sleep(.05)
            for _ in range(5):
                if "play_game: FUNCTION ENTERED" in log.read_text(errors="replace"):
                    break
                control.key("Enter", capture=False)
            text = log.read_text(errors="replace")
            assert "initial_menu: ENTERED" in text and "play_game: FUNCTION ENTERED" in text, case
            for _ in range(6):
                if process.poll() is not None:
                    break
                try:
                    if control.key("Escape", capture=False).get("closed"):
                        break
                except ControlError:
                    break
            assert process.wait(timeout=10) == 0, case
        finally:
            if process.poll() is None:
                process.kill()
    return log.read_text(errors="replace")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, default=ROOT / "sil-more-windows-sdl3-portable",
                        help="portable package supplying the executable and runtime DLLs")
    parser.add_argument("--exe", type=Path, help="override the portable executable")
    args = parser.parse_args()
    executable = args.exe or args.package / "sil-more.exe"
    image = executable.read_bytes()
    if b"Cannot resolve the portable user directory" not in image or b"steam_api" in image.lower():
        parser.error("a non-Steam portable executable is required to isolate the profile")

    with tempfile.TemporaryDirectory(prefix="sil-more-sdl-startup-") as scratch:
        game = Path(scratch)
        shutil.copy2(executable, game / "sil-more.exe")
        for dll in args.package.glob("*.dll"):
            shutil.copy2(dll, game / dll.name)
        for folder in ("edit", "help", "pref", "xtra"):
            shutil.copytree(ROOT / "lib" / folder, game / "lib" / folder)
        for folder in ("apex", "data", "save", "user"):
            (game / "lib" / folder).mkdir()

        sound = game / "lib" / "pref" / "sound.json"
        options = json.loads(sound.read_text())
        options.update(enabled=True, volumeMaster=0.41)
        sound.write_text(json.dumps(options))
        log = launch(game, "unavailable-test-backend", "missing-audio")
        assert "Failed to create SDL_mixer device" in log and "FATAL" not in log
        assert json.loads(sound.read_text()) == options

        settings = game / "lib" / "user" / "sil_sdl.json"
        config = json.loads(settings.read_text())
        config["sdl"].update(fullscreen=False, windowX=5000, windowY=100,
                             windowWidth=896, windowHeight=650)
        settings.write_text(json.dumps(config))
        log = launch(game, "dummy", "disconnected-monitor")
        assert "Saved window position (5000, 100) is offscreen" in log
        assert "SDL_mixer opened audio device" in log
        assert json.loads(sound.read_text()) == options
        restored = json.loads(settings.read_text())["sdl"]
        assert 0 <= restored["windowX"] < 1024 and 0 <= restored["windowY"] < 768, restored

        config = json.loads(settings.read_text())
        config["sdl"].update(windowX=20, windowY=30)
        settings.write_text(json.dumps(config))
        log = launch(game, "dummy", "visible-window")
        assert "is offscreen" not in log
        restored = json.loads(settings.read_text())["sdl"]
        assert (restored["windowX"], restored["windowY"]) == (20, 30), restored

        config["sdl"].update(windowX=-900, windowY=-100)
        settings.write_text(json.dumps(config))
        log = launch(game, "dummy", "negative-position", "-1024,-200,1024,768")
        assert "is offscreen" not in log
        restored = json.loads(settings.read_text())["sdl"]
        assert (restored["windowX"], restored["windowY"]) == (-900, -100), restored
    print("Audio failure and recovery, offscreen and visible windows, and clean menu exits passed.")


if __name__ == "__main__":
    main()

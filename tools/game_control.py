#!/usr/bin/env python3
"""Control Sil-More directly, without desktop mouse/keyboard automation.

Only Python's standard library is required. Also importable as GameControl.
"""
from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DIR = ROOT / "scripts" / "output" / "game-control"


class ControlError(RuntimeError):
    pass


def read_json(path: Path) -> dict:
    for attempt in range(4):
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
            break
        except (PermissionError, FileNotFoundError) as exc:
            # A Windows atomic replace can briefly make the filename busy.
            if attempt == 3:
                raise ControlError(f"Cannot read {path}: {exc}") from exc
            time.sleep(0.005)
        except (OSError, ValueError) as exc:
            raise ControlError(f"Cannot read {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ControlError(f"Expected a JSON object in {path}")
    return value


def publish_json(path: Path, value: dict) -> None:
    data = json.dumps(value, ensure_ascii=False, allow_nan=False).encode("utf-8")
    if len(data) > 16384:
        raise ControlError("Request exceeds 16384 bytes")
    temp = path.with_name(path.name + ".client.tmp")
    try:
        temp.write_bytes(data)
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def parse_key(chord: str) -> tuple[str, list[str]]:
    """Ctrl+Shift+s, Enter, or a literal printable character (including +)."""
    modifiers = []
    key = chord
    while "+" in key and len(key) > 1:
        prefix, rest = key.split("+", 1)
        modifier = {"ctrl": "ctrl", "control": "ctrl", "shift": "shift",
                    "alt": "alt"}.get(prefix.lower())
        if modifier is None:
            break
        modifiers.append(modifier)
        key = rest
    if not key or len(modifiers) > 3:
        raise ControlError(f"Invalid key chord: {chord}")
    aliases = {"esc": "Escape", "enter": "Enter", "return": "Return",
               "space": "Space", "pageup": "PageUp", "pagedown": "PageDown"}
    key = aliases.get(key.lower(), key)
    if len(key) == 3 and key[:2].lower() == "kp" and key[2].isdigit():
        key = "Keypad " + key[2]
    return key, modifiers


class GameControl:
    def __init__(self, directory: str | Path = DEFAULT_DIR, timeout: float = 15):
        self.directory = Path(directory).expanduser().resolve()
        if not math.isfinite(timeout) or not 0.1 <= timeout <= 60:
            raise ControlError("Timeout must be between 0.1 and 60 seconds")
        self.timeout = timeout

    def session(self) -> dict:
        session = read_json(self.directory / "session.json")
        if session.get("protocol") != 1 or not session.get("session"):
            raise ControlError("Unsupported or invalid game control session")
        if session.get("running") is not True:
            raise ControlError("This game control session has stopped")
        return session

    def request(self, op: str, *, output: str | Path | None = None,
                capture: bool = True, **fields) -> dict:
        """Send one action, wait for its resulting input prompt, and observe.

        output copies the PNG before releasing the client lock. Otherwise the
        mailbox PNG lasts until the next request. A timeout never retries input.
        """
        lock = self.directory / "client.lock"
        try:
            lock.mkdir()
        except FileExistsError as exc:
            raise ControlError("Another client owns this session. If it crashed, "
                               "remove client.lock after checking it has stopped.") from exc
        except OSError as exc:
            raise ControlError(f"Cannot access control directory: {exc}") from exc
        request_path = self.directory / "request.json"
        request_id = uuid.uuid4().hex
        submitted = False
        try:
            session = self.session()
            if request_path.exists():
                raise ControlError("A request is already waiting for the game; "
                                   "wait for it to expire before sending another")
            request = dict(fields)
            reply_margin = min(0.5, self.timeout / 4)
            request.update(protocol=1, session=session["session"], id=request_id,
                           op=op, capture=capture,
                           expires_at_ms=int((time.time() + self.timeout - reply_margin) * 1000))
            deadline = time.monotonic() + self.timeout
            publish_json(request_path, request)
            submitted = True
            while time.monotonic() < deadline:
                response_path = self.directory / "response.json"
                if response_path.exists():
                    response = read_json(response_path)
                    if (response.get("session") == session["session"]
                            and response.get("id") == request_id):
                        if response.get("screenshot"):
                            if response["screenshot"] != "screenshot.png":
                                raise ControlError("Unexpected screenshot filename")
                            source = self.directory / "screenshot.png"
                            if output is not None:
                                target = Path(output).expanduser().resolve()
                                target.parent.mkdir(parents=True, exist_ok=True)
                                if target != source:
                                    shutil.copyfile(source, target)
                                source = target
                            response["screenshot"] = str(source)
                        return response
                current = self.session()
                if current["session"] != session["session"]:
                    raise ControlError("The game restarted while this request was pending")
                time.sleep(0.025)
            raise ControlError("Game control timed out. Input may already have been "
                               "applied; inspect the game before sending it again.")
        finally:
            # Cancel only an unclaimed request belonging to this client. Claimed
            # actions are never resent and their later replies carry their id.
            if submitted and request_path.exists():
                try:
                    if read_json(request_path).get("id") == request_id:
                        request_path.unlink(missing_ok=True)
                except ControlError:
                    pass
            lock.rmdir()

    def key(self, chord: str, **options) -> dict:
        key, modifiers = parse_key(chord)
        return self.request("key", key=key, modifiers=modifiers, **options)

    def text(self, text: str, **options) -> dict:
        if not 1 <= len(text.encode("utf-8")) <= 128:
            raise ControlError("Text must contain 1 to 128 UTF-8 bytes")
        return self.request("text", text=text, **options)

    def click(self, x: float, y: float, button: str = "left", **options) -> dict:
        return self.request("click", x=x, y=y, button=button, **options)

    def observe(self, **options) -> dict:
        return self.request("observe", **options)


def launch(control: GameControl, executable: Path, headless: bool, wizard: bool = False) -> dict:
    executable = executable.expanduser().resolve()
    if not executable.is_file():
        raise ControlError(f"Game executable not found: {executable}")
    if (control.directory / "session.json").exists():
        previous = read_json(control.directory / "session.json")
        if previous.get("running"):
            raise ControlError("Control directory is in use. Choose a fresh --dir "
                               "if the previous game crashed.")
    control.directory.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    if headless:
        env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
                   SDL_AUDIO_DRIVER="dummy")
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    core_args = ["-w"] if wizard else []
    process = subprocess.Popen([str(executable), *core_args, "--", "--windowed",
                                "--control-dir", str(control.directory)],
                               cwd=executable.parent, env=env, creationflags=flags,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    deadline = time.monotonic() + control.timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise ControlError(f"Game exited with code {process.returncode}; "
                               f"check {executable.parent / 'log.txt'}")
        try:
            session = control.session()
        except ControlError:
            time.sleep(0.05)
            continue
        session.update(pid=process.pid, headless=headless)
        return session
    raise ControlError(f"Game is still starting (PID {process.pid}); check "
                       f"{executable.parent / 'log.txt'} and retry observe")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dir", type=Path,
                        default=Path(os.environ.get("SIL_MORE_CONTROL_DIR", DEFAULT_DIR)),
                        help="Control directory shared with the game")
    parser.add_argument("--timeout", type=float, default=15)
    commands = parser.add_subparsers(dest="command", required=True)
    start = commands.add_parser("launch", help="Launch an ordinary or offscreen game")
    start.add_argument("--exe", type=Path,
                       default=ROOT / "sil-more-windows-sdl3" / "sil-more.exe")
    start.add_argument("--headless", action="store_true", help="Use SDL's offscreen software driver")
    start.add_argument("--wizard", action="store_true", help="Request wizard mode for this game")
    for name in ("observe", "status", "key", "text", "click"):
        command = commands.add_parser(name)
        if name != "status":
            command.add_argument("--output", type=Path, help="Keep a copy of the resulting PNG")
            command.add_argument("--no-capture", action="store_true")
        if name == "key":
            command.add_argument("keys", nargs="+", help="SDL key names or chords; sent one at a time")
        elif name == "text":
            command.add_argument("text", help="Text for the current word-entry prompt")
        elif name == "click":
            command.add_argument("x", type=float)
            command.add_argument("y", type=float)
            command.add_argument("--button", choices=("left", "right"), default="left")
    args = parser.parse_args()
    try:
        control = GameControl(args.dir, args.timeout)
        if args.command == "launch":
            result = launch(control, args.exe, args.headless, args.wizard)
        elif args.command == "status":
            result = control.request("status", capture=False)
        else:
            options = dict(output=args.output, capture=not args.no_capture)
            if args.command == "key":
                for key in args.keys:
                    result = control.key(key, **options)
                    if not result.get("ok") or result.get("closed"):
                        break
            elif args.command == "text":
                result = control.text(args.text, **options)
            elif args.command == "click":
                result = control.click(args.x, args.y, args.button, **options)
            else:
                result = control.observe(**options)
        print(json.dumps(result, ensure_ascii=True, indent=2))
        return 0 if result.get("ok", True) else 1
    except (ControlError, OSError, ValueError) as exc:
        print(json.dumps({"ok": False, "error": str(exc)}, ensure_ascii=True), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

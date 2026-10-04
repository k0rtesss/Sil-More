# Direct game control

Sil-More has an optional local interface for an agent or script to send keys,
enter text, click game controls, and receive PNG screenshots. It uses the game's
SDL event queue and renderer. Your desktop mouse and keyboard remain available
while the game is controlled. No VM, network server, or desktop automation is
required.

The interface is off by default. Enable it for a specific launch with
`-- --control-dir <absolute-path>` or the `SIL_MORE_CONTROL_DIR` environment
variable. One game and one client use a directory at a time. The interface
allows anyone who can write to that directory to send game inputs, so keep it
in your own local folders.

## Quick start on Windows

Build and deploy with `build-cmake.bat standard`. From the repository root:

```powershell
python tools/game_control.py launch --headless
python tools/game_control.py observe --output scripts/output/current-screen.png
python tools/game_control.py key Space
python tools/game_control.py key Escape
python tools/game_control.py key Up Enter
python tools/game_control.py text "Efrem"
python tools/game_control.py click 384 288
```

Choose inputs based on the returned screen. `text` is accepted only while a
word-entry prompt is open. `key Up Enter` sends those two keys sequentially,
observing each result. Click coordinates are pixels in the returned screenshot,
with `(0, 0)` at its top left. `--output` keeps a copy of the PNG for any action.
`--no-capture` returns the JSON state without making a PNG.

`launch --headless` uses SDL's dummy video driver and software renderer. It
creates no desktop game window and uses dummy audio. Omit `--headless` for a
normal window that you can leave unfocused. Both modes use the executable's
normal saves, scores, and settings. Quit and save through the ordinary game
commands. A headless launch is windowed; the normal SDL configuration saving
behavior applies when it exits.

`launch --wizard` passes the game's `-w` option. The normal debug confirmation
and password prompts still apply. Keyboard and mouse settings remain available
when the control interface provides those inputs without physical devices.

The default mailbox is `scripts/output/game-control`. Select another directory
with a global option before the subcommand:

```powershell
python tools/game_control.py --dir C:\Temp\sil-agent launch --headless
python tools/game_control.py --dir C:\Temp\sil-agent observe
```

Alternatively, launch the executable yourself:

```powershell
.\sil-more-windows-sdl3\sil-more.exe -- --windowed --control-dir C:\Temp\sil-agent
python tools/game_control.py --dir C:\Temp\sil-agent key Ctrl+s
```

An already running game must be restarted with the interface enabled. Native
startup dialogs that precede the game's main window must be answered before the
control session becomes available. After a crash, use a fresh control directory.
The client reports a stale `client.lock` if a previous client crashed; remove
that empty directory only after checking the client has stopped.

## Python client

`tools/game_control.py` uses only the standard library. It can also be imported:

```python
from tools.game_control import GameControl

game = GameControl(r"C:\Temp\sil-agent", timeout=15)
result = game.observe(output="current-screen.png")
result = game.key("Ctrl+s")
result = game.text("Efrem")  # only in a text-entry prompt
result = game.click(384, 288)
```

Keys are SDL names (`Up`, `Enter`, `Escape`, `F1`, `Keypad 8`) or printable ASCII
characters. The client also accepts `KP8` and modifier chords such as
`Ctrl+Shift+s`. Uppercase letters imply Shift. Modifiers are handled by the same
game input routing as physical keys, including movement presets and menus.
Text accepts up to 128 UTF-8 bytes and excludes control characters; send Enter
separately to confirm. Clicking respects the game's mouse-enabled setting.

Each action usually replies when the game reaches its next input prompt, which
may ask for a direction, confirmation, or menu selection. Page turns and short
input guards finish before another action is delivered. Tutorial and coaching
screens are included. Long commands may reach the request deadline first and
return `input_applied: true` with `state.waiting_for_input: false`; observe again
to inspect their progress. Screenshots and status observations consume no game
turns.

The state includes renderer dimensions, turn counters, current player position
and health when in the dungeon, recent messages, and terminal text. The PNG is
the authoritative visual observation: terminal text does not contain native
SDL overlays, and graphical tiles and non-ASCII bytes appear as spaces there.
`input_context` distinguishes ordinary terminal waits from standalone modal
loops. `focused` reflects SDL's window flags, including the virtual flags of
the dummy driver.

Quit actions reply with `closed: true` and no PNG. Inspect `ok` and `error` on
every response. A timeout never resends input, because the game may already
have applied it. An unclaimed timed-out request is removed by the client.

## Mailbox protocol, version 1

The game creates `session.json` with `protocol`, `session`, `directory`,
`version`, and `running`. Each launch has a different session ID. The game
marks `running: false` during orderly shutdown.

The client serializes requests using the exclusive `client.lock` directory.
Write a temporary file, close it, then atomically rename it to `request.json`:

```json
{
  "protocol": 1,
  "session": "copy from session.json",
  "id": "unique request ID, at most 64 bytes",
  "expires_at_ms": 1791135000000,
  "op": "key",
  "key": "s",
  "modifiers": ["ctrl"],
  "capture": true
}
```

`expires_at_ms` is a Unix timestamp in milliseconds, in the future and no more
than 60 seconds away. The timestamp above is illustrative; generate a current
deadline. A complete request is at most 16384 bytes.

Operations:

| Operation | Fields | Result |
| --- | --- | --- |
| `observe` | Optional `capture` | Current PNG and state |
| `status` | None | State without a PNG |
| `key` | `key`, optional `modifiers` array of `ctrl`, `shift`, `alt` | Key down/up through SDL |
| `text` | `text` | UTF-8 text in the current word-entry prompt |
| `click` | `x`, `y`, optional `button` (`left` or `right`) | Pointer motion and button down/up |

The game claims the request before dispatch and publishes `response.json`
atomically. Match both its `id` and `session`; older replies may remain in the
directory. Replies contain `protocol`, `session`, `id`, `ok`, `input_applied`,
`closed`, `state`, and optionally `error` or `screenshot: "screenshot.png"`.
The PNG is published before its reply. Copy it before releasing the client
lock if it must survive the next observation. The supplied Python client does
this for `--output`.

Wrong sessions, expired requests, unsupported operations, invalid keys, and
out-of-bounds clicks are rejected before input delivery. The most recent ID is
also checked for duplicates. IDs must be unique; the protocol is not an input
retry mechanism. Malformed JSON replies have an empty ID. Ordinary game input
and flush semantics still apply, and physical input can interleave with agent
input in a visible window.

Transient file-sharing or read failures leave requests queued for a later poll;
they are not claimed or treated as malformed JSON.

## Isolated gameplay testing

```powershell
python tools/game_playtest.py combat
```

This copies the portable executable, DLLs and shipped assets into a unique
folder under `scripts/output/control-playtests`, creates empty writable game
directories, and launches a headless wizard session. It imports no saved games,
scores, or user preferences. Use `--normal` for ordinary play. The returned
JSON identifies the profile, PID, and control directory. Use that exact control
directory with `game_control.py --dir ...` or import `connect` and `capture`
from `tools/game_playtest.py` to save observation PNGs and JSON in the profile.
Keep one game process per profile, including when reusing a saved fixture.

## Validation

```powershell
.\build-cmake.bat standard
python scripts/check_game_control.py
```

The Windows harness reuses the actual CMake objects and SDL dependencies. It
assigns every writable game path to a fresh folder under `scripts/output`,
uses offscreen rendering, and tests PNG dimensions, physical key routing,
modifiers, native menu clicks, UTF-8 text entry, inventory/equipment overlays,
standalone modal loops, invalid and expired requests, duplicate rejection,
Windows publication conflicts, clean shutdown, and the disabled interface.
The file-sharing checks include a valid request that temporarily denies reads
while permitting deletion, ensuring it remains queued and is retried safely.
It also exercises the Python CLI and headless launcher against the real harness.
It does not load or modify your saved characters or configuration.

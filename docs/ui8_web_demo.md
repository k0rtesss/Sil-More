# UI8 Web Demo

This document describes the static browser proof of concept that exercises the
UI8 wire boundary.

## What It Covers
- versioned `app-wire` packets for snapshots, events, session state, and
  legacy-key input
- a host-neutral browser bridge for timing, resource fetches, persistence, and
  logging
- a minimal dungeon scene renderer, message log, and one gameplay interaction
  flow

The demo is packet-driven. It does not run the game loop in the browser. The
browser consumes packets emitted by the same C boundary that SDL and later web
hosts are expected to use.

## Files
- `web/ui8-demo/index.html`
- `web/ui8-demo/app.css`
- `web/ui8-demo/app.js`
- `web/ui8-demo/host-bridge.js`
- `web/ui8-demo/wire.js`
- `web/ui8-demo/input.js`
- `web/ui8-demo/render-dungeon.js`
- `web/ui8-demo/render-messages.js`
- `web/ui8-demo/render-overlay.js`
- `tools/ui8_emit_demo_packets.c`

## Generate Sample Packets
Build the standard tree first:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1 -Target standard
```

Emit the sample bundle into `web/ui8-demo/samples`:

```powershell
$env:PATH='C:\msys64\mingw64\bin;' + $env:PATH
.\build-standard\sil-ui8-demo-packets.exe web\ui8-demo\samples
```

The emitter writes:
- `choice-open.snapshot.bin`
- `choice-open.events.bin`
- `choice-open.session.bin`
- `choice-picked.snapshot.bin`
- `choice-picked.events.bin`
- `choice-picked.session.bin`
- `layout.json`

`layout.json` records the blob field offsets used by the JavaScript decoder so
the browser view matches the current C struct layout.

## Serve The Demo
Use any static file server. The repo venv is enough:

```powershell
src\.venv\Scripts\python.exe -m http.server 8000 -d web\ui8-demo
```

Then open `http://localhost:8000/`.

## Demo Flow
- The initial state shows a dungeon snapshot plus a list-selection overlay.
- Arrow keys map to roguelike movement keys for packet encoding.
- `b` or `Enter` advances from the open selection state to the picked state.
- `Reset State` restores the initial packet set.

The browser host stores the last eight encoded input packets in `localStorage`
and logs host activity in the page.

## Validation
- `ctest -R "sil_ui1_scaffolding|sil_ui8_wire|sil_ui8_demo_packets" --output-on-failure`
- `src\.venv\Scripts\python.exe tools\ui_debt_audit.py --check tests\ui_debt_audit_baseline.json`

UI8 validation is currently based on the host-neutral `sil-core` static
library plus the packet-driven browser demo. A live Emscripten build is still a
future delivery path, not a prerequisite for this stage.

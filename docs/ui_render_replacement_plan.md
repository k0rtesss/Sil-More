# UI Terminal Extermination Plan

Status: active on April 15, 2026.

Current checkpoint on April 15, 2026:
- SDL renderer replacement is complete.
- Gameplay and UI `flush()` ownership is gone from the active source tree.
- Public SDL term-host naming is gone from the active source tree.
- The current `terminal_model` debt audit is at zero.
- Remaining work is the internal terminal kernel still embedded in the SDL
  view implementation, plus the export and screenshot compatibility path.

## Mission
- Purge all remaining terminal-era UI logic from the codebase, not just the
  public names that exposed it.
- Preserve gameplay-space grids and cells: dungeon topology, LOS math,
  projectile paths, minimap data, and map snapshots are not the target here.
- Delete UI compatibility layers that still behave like a terminal:
  hook tables, `TERM_XTRA_*`, shadow row buffers, dirty spans, and char or
  attr screen mirrors.
- Do not add new compatibility wrappers, raw-cell fallback renderers, or new
  APIs whose ownership is still hook-driven row or column replay.

## Audit Reality
- `py -3 tools/ui_debt_audit.py --details` is currently green, but that audit
  now measures only the public debt family that has already been removed.
- Zero debt in the current report does not mean zero terminal logic in the
  repo.
- The next slice must expand the audit before landing the final purge, so the
  remaining internal kernel becomes measurable and regressions are blocked.

## Completed Foundation
- gameplay-facing pending-input clearing now flows through
  `input_clear_pending()` instead of `flush()`
- SDL view readiness and redraw naming no longer pass through public
  `term_*` APIs
- story-font activation and cell-alignment state no longer live on per-host
  SDL view objects
- the tracked public `terminal_model` family is at zero and must remain there

## Live Remnant Families
### 1. Internal SDL terminal kernel
Primary files:
- `src/sdl-main-internal.h`
- `src/sdl-render.c`
- `src/platform-frame.c`
- `src/sdl-layout.c`

What remains:
- `sdl_view_state` still stores terminal-era compatibility state such as:
  - `key_queue`
  - `wid` / `hgt`
  - dirty spans `x1` / `x2`
  - shadow buffers `old` / `scr`
- hook-driven render or control plumbing still exists:
  - `xtra_hook`
  - `curs_hook`
  - `bigcurs_hook`
  - `wipe_hook`
  - `text_hook`
  - `pict_hook`
- `TERM_XTRA_*` still owns event pumping, flush, clear, fresh, delay, and
  react behavior inside the SDL renderer
- redraw still replays row or cell state from compatibility buffers instead of
  consuming semantic scene or view data directly

Exit when:
- SDL view state contains only direct view and render state
- no `TERM_XTRA_*` or hook-table dispatch remains
- main redraw no longer depends on row or cell shadow buffers

### 2. Residual export and screenshot compatibility path
Primary files:
- `src/files.c`

What remains:
- `mini_screenshot_char`
- `mini_screenshot_attr`
- the character dump screenshot still serializes a faux screen-cell slice

Exit when:
- dump and export helpers consume semantic snapshot data directly
- any intentionally retained ASCII output is explicit formatting over semantic
  data, not UI compatibility capture

### 3. Audit blind spot
Primary files:
- `tools/ui_debt_audit.py`
- `tests/ui_debt_audit_baseline.json`
- this file

What remains:
- the current audit no longer measures the true remaining terminal kernel
- the baseline still reflects the previous public debt family instead of the
  actual terminal logic that survives in SDL internals and export code

Exit when:
- the audit tracks the internal SDL kernel and export mirror debt directly
- the baseline locks the new zero point after that code is deleted

## Next Slice
### Slice 6. Internal terminal-kernel purge
Goal:
- remove the remaining terminal compatibility engine from the SDL frontend
  instead of merely renaming its surface

Scope:
- `src/sdl-main-internal.h`
- `src/sdl-render.c`
- `src/platform-frame.c`
- `src/sdl-layout.c`
- `src/files.c`
- `tools/ui_debt_audit.py`
- `tests/ui_debt_audit_baseline.json`

Tasks:
1. Expand the audit first
- add metrics for `TERM_XTRA_`
- add metrics for hook fields:
  - `xtra_hook`
  - `curs_hook`
  - `bigcurs_hook`
  - `wipe_hook`
  - `text_hook`
  - `pict_hook`
- add metrics for SDL shadow buffers and dirty-span state:
  - `old`
  - `scr`
  - `x1`
  - `x2`
  - `wid`
  - `hgt`
  - `key_queue`
- add metrics for `mini_screenshot_char` and `mini_screenshot_attr`

2. Split event and control flow away from terminal hooks
- replace `TERM_XTRA_EVENT`, `TERM_XTRA_FLUSH`, `TERM_XTRA_CLEAR`,
  `TERM_XTRA_FRESH`, `TERM_XTRA_DELAY`, and `TERM_XTRA_REACT` with direct
  SDL or platform-frame functions
- make `platform_frame_*` and direct SDL helpers own event pumping, clear,
  delay, present, and react behavior without indirection through view-state
  hooks

3. Replace buffer-driven redraw with direct SDL view rendering
- remove `old` / `scr` shadow buffers and dirty spans
- stop redrawing by replaying row or cell writes from compatibility state
- route any remaining legacy presentation through direct view or canvas helpers
  until the last buffer-based caller is gone

4. Delete compatibility export mirrors
- rewrite miniature screenshot and character dump helpers against semantic
  snapshots
- remove `mini_screenshot_char`
- remove `mini_screenshot_attr`

5. Refresh baseline and docs
- land the expanded audit baseline only after the remaining internal kernel and
  export mirrors are gone
- update migration docs to treat the repo as terminal-free rather than merely
  host-renamed

Exit when:
- the internal SDL terminal kernel is gone
- the export mirror path is gone
- the expanded audit reports zero live debt for both

## Execution Order
### A. Expand the audit
Goal:
- make the true remaining debt measurable before touching the final kernel

Exit when:
- CI can fail on regressions in internal terminal-kernel and export-mirror
  debt, not just the already-finished public debt family

### B. Remove `TERM_XTRA_*` and hook-driven control flow
Goal:
- stop routing SDL event, clear, delay, and react behavior through terminal
  callback semantics

Exit when:
- the SDL renderer and platform frame use direct control helpers only

### C. Remove shadow buffers and dirty-row redraw
Goal:
- stop replaying UI state from compatibility row or cell buffers

Exit when:
- redraw consumes direct render state or semantic scene data

### D. Rewrite exports
Goal:
- remove char or attr compatibility capture from file output

Exit when:
- character dump and screenshot paths no longer mirror screen cells

### E. Refresh baseline and docs
Goal:
- lock the new zero point and make the repo guidance accurate

Exit when:
- audit, baseline, and docs all describe the same terminal-free end state

## Parallel Lanes
- Lane 1: audit expansion for the real remaining debt
- Lane 2: SDL control-flow and `TERM_XTRA_*` teardown
- Lane 3: SDL shadow-buffer and dirty-row redraw teardown
- Lane 4: export and screenshot rewrite
- Lane 5: docs and baseline refresh

Dependencies:
- Lane 1 starts first and stays active until the end.
- Lane 2 should land before the buffer teardown.
- Lane 3 depends on the direct control-flow path from Lane 2.
- Lane 4 can proceed once the semantic data source for exports is chosen.
- Lane 5 lands last.

## Validation
- Run `py -3 tools/ui_debt_audit.py --details`.
- Run targeted searches for:
  - `TERM_XTRA_`
  - `xtra_hook`
  - `curs_hook`
  - `bigcurs_hook`
  - `wipe_hook`
  - `text_hook`
  - `pict_hook`
  - `key_queue`
  - `old`
  - `scr`
  - `x1`
  - `x2`
  - `mini_screenshot_char`
  - `mini_screenshot_attr`
- Build with `.\build-cmake.bat` or `cmake --build build-standard --parallel`.
- Smoke-test:
  - command input, including repeat counts and keymaps
  - redraw and resize behavior across main and supporting panes
  - story pages, help pages, and browser or document views
  - inventory, equipment, monster recall, and monster list panes
  - character dump generation

## Done When
- no `TERM_XTRA_*` remains in active SDL code
- no SDL view-state hook table remains
- no SDL shadow row or cell buffers remain
- no dirty-span replay remains
- no compatibility key queue remains in SDL view state
- `src/files.c` no longer owns char or attr screenshot mirrors
- the expanded debt audit reports zero live terminal logic
- the already-complete public `terminal_model` audit remains at zero
- any remaining uses of grid, row, column, or cell terminology are clearly
  gameplay-space or scene-layout data, not terminal UI compatibility state

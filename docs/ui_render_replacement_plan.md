# UI Terminal Extermination Plan

Status: active on April 15, 2026. This replaces the completed
render-replacement plan.

## Mission
- The SDL render replacement is done. The remaining work is removal of the
  terminal-era UI model from the active source tree.
- Preserve gameplay-space grids and cells: dungeon topology, LOS math,
  projectile paths, minimap data, and map snapshots are not the target here.
- Remove terminal-space concepts from the normal SDL UI path: blocking byte
  input, delayed flush ownership, row or column text writes, document cell ops,
  SDL `term` hosts, and char or attr screen captures.
- Do not add new compatibility wrappers, raw-cell fallback renderers, or new
  UI APIs whose ownership is still `term_*`, `prt()`, or `*_wid` / `*_hgt`.

## Audit Snapshot
- `py -3 tools/ui_debt_audit.py --details` currently reports:
  - `ui0`: `inkey()`, `screen_save()/screen_load()`, direct `Term_*`, and
    `get_sdl_*`/`set_sdl_*` outside platform code all remain at 0 files /
    0 matches
  - `movement_input`: all tracked metrics are now at 0 files / 0 matches; the
    movement slice is complete and the baseline now preserves that state
  - `terminal_model`: `inkey()` 0 files / 0 matches, `request_command()` 0
    files / 0 matches, `flush()` 14 files / 24 matches, compat text wrappers
    12 files / 142 matches, terminal-size queries 7 files / 34 matches,
    document-op cell grid 0 files / 0 matches, SDL term-host symbols 6 files
    / 49 matches, and term-host story-font state 6 files / 48 matches
- `tests/ui_debt_audit_baseline.json` now carries `terminal_model` alongside
  `ui0` and `movement_input`, so `ctest -R sil_ui0_audit --output-on-failure`
  can fail on regressions in the real remaining debt families.

## Live Remnant Families
### 1. Legacy input and command loop
Primary files:
- `src/util-input.c`
- `src/dungeon.c`
- `src/externs.h`
- `src/targeting.c`

What remains:
- movement now enters through `request_player_command()` and the semantic
  movement service, but the legacy byte queue still lives in `inkey()`
- repeat-count prompting, keymap expansion, and fallback non-movement command
  acquisition still live in `request_legacy_command()` inside
  `src/util-input.c`
- targeting and other prompt flows still own direct `inkey()` waits or delayed
  `flush()` behavior

Exit when:
- input waits come from `src/app/*` interaction primitives instead of
  `inkey()`
- command acquisition has a named semantic API instead of byte queues and
  global flags
- gameplay code no longer calls `flush()` as part of normal UI flow

### 2. Compat text surface and cursor-state writers
Primary files:
- `src/util-text.c`
- `src/util-editing.c`
- `src/externs.h`
- callers in `src/monster1.c`, `src/monster2.c`,
  `src/cmd/ui/cmd-ui-object-display.c`, `src/squelch.c`, `src/wizard1.c`,
  `src/wizard2.c`, `src/score/score_ui.c`, `src/ui/ui-help.c`, and others

What remains:
- `ui_text_surface` is still a global row or column text buffer.
- `prt()`, `c_prt()`, `put_str()`, `c_put_str()`, `clear_from()`, and
  `text_out_to_screen()` remain a major authoring API.
- several screens still depend on implicit cursor state and line erasure
  behavior rather than scene widgets

Exit when:
- normal SDL UI authors text through scene widgets or dedicated semantic
  builders
- no user-facing flow depends on global text-surface cursor state
- the compat writer family is deleted or isolated outside the normal SDL path

### 3. Document-op cell grid
Primary files:
- `src/obj-info.c`
- `src/ui/ui-story.c`
- `src/ui/ui-character-screen.c`
- `src/ui/ui-file-viewer.c`
- `src/ui/ui-help.c`
- `src/object/object-ui-display.c`

What remains:
- the shared `APP_UI_PANEL_STYLE_DOCUMENT` and `APP_UI_DOCUMENT_OP_*` runtime
  surface has been deleted; remaining debt is now local to individual flows
- `src/obj-info.c` now captures semantic lines and runs directly, but still
  stages local line data before browser presentation
- `src/ui/ui-story.c` and the character tutorial in
  `src/ui/ui-character-screen.c` still mirror row-oriented content before
  browser presentation
- `src/ui/ui-file-viewer.c`, `src/ui/ui-help.c`, and related browser flows
  still paginate or size content against terminal-era widths and heights

Exit when:
- document and browser screens use paragraph, list, table, glyph-row, and
  scroll widgets with renderer-owned layout
- local row or column document mirrors are deleted from the remaining runtime
  UI flows
- terminal-sized document pagination is replaced by explicit widget layout

### 4. SDL term host layer that survived the renderer migration
Primary files:
- `src/sdl-main-internal.h`
- `src/sdl-render.c`
- `src/sdl-layout.c`
- `src/platform-frame.c`
- `src/sdl-scene.c`
- `src/sdl-story-font.c`

What remains:
- `struct term_win` and `struct term`
- `term_ready`, `term_init()`, `term_nuke()`
- term-linked view lifetime and redraw APIs
- story-font and grid-alignment state stored on the active term host

Exit when:
- SDL views own canvases, focus, and text caches directly
- view lifetime no longer passes through `term_*`
- story-font state lives on scene or view state, not term host state

### 5. Residual char or attr screen capture and export logic
Primary files:
- `src/files.c`

What remains:
- `mini_screenshot_char`
- `mini_screenshot_attr`
- the character dump screenshot still serializes a faux screen-cell slice

Exit when:
- dump and export code consumes semantic snapshot data directly
- any intentionally retained ASCII export is explicit data formatting, not a UI
  compatibility dependency

### 6. Audit and documentation blind spots
Primary files:
- `tools/ui_debt_audit.py`
- `docs/ui_migration_inventory.md`
- `docs/ui_architecture_migration_plan.md`
- this file
- legacy roadmap docs that still talk about `z-term` as live debt

What remains:
- the audit now covers `ui0`, the completed `movement_input` slice, and the
  broader `terminal_model` family
- several docs still describe the old render-replacement finish line instead of
  the current cleanup target

Exit when:
- the audit tracks the actual remaining terminal model
- docs clearly distinguish completed renderer replacement from unfinished
  terminal-logic removal

## Execution Order
### A. Expand the audit first
Status:
- completed on April 15, 2026 via the `terminal_model` audit family in
  `tools/ui_debt_audit.py` and the refreshed
  `tests/ui_debt_audit_baseline.json`

Goal:
- make the remaining work measurable before touching code

Tasks:
- extend `tools/ui_debt_audit.py` or add a second audit for:
  - `inkey`, `request_command`, `flush`
  - compat text wrappers
  - `platform_frame_*_view_cols/rows`
  - `APP_UI_DOCUMENT_OP_*`
  - SDL term-host symbols
  - story-font grid mode
- check in a baseline so new debt cannot be introduced during the cleanup

Exit when:
- CI can fail on regressions in the real remaining debt families

### B. Replace the live input stack
Goal:
- remove terminal-era command acquisition from gameplay flow

Tasks:
- introduce a semantic command-input service behind `src/app/*`
- move macro or keymap expansion into that service or into a narrow reusable
  translator
- convert the remaining repeat-count prompting and non-movement fallback path
  off `inkey()` / `request_legacy_command()` / `flush()`

Exit when:
- `src/util-input.c` is gone or reduced to platform-only translation with no
  gameplay callers

### C. Kill the compat text surface
Goal:
- remove row or column screen-authoring from normal SDL UI code

Tasks:
- replace `prt()` / `c_put_str()` / `clear_from()` family by screen family:
  - monster recall and monster list
  - object recall fallback
  - squelch, wizard, and score utility screens
  - help diagrams and editing helpers
- delete `ui_text_surface` screen-authoring APIs once callers are drained

Exit when:
- user-facing UI flows no longer depend on the compat writer family

### D. Replace document row or column ops with real widgets
Status:
- the shared `APP_UI_DOCUMENT_OP_*` runtime surface and SDL document-panel
  renderer were removed on April 15, 2026; remaining work is local row/capture
  builders and terminal-sized pagination in the surviving tutorial/browser
  flows

Goal:
- stop calling semantic documents "semantic" when they are still cell-addressed

Tasks:
- add renderer-facing widgets for paragraphs, glyph-plus-label rows, tables,
  scroll prompts, and tutorial or help callouts
- migrate `src/obj-info.c`, `src/ui/ui-story.c`, `src/ui/ui-file-viewer.c`,
  `src/ui/ui-help.c`, `src/thrall_quest.c`, and remaining tutorial or browser
  flows
- remove the remaining local row or column text mirrors and terminal-sized
  pagination helpers after the shared document-op path is gone

Exit when:
- document screens are no longer built from row or column operations

### E. Remove SDL term hosts
Goal:
- delete the SDL-side terminal container that still anchors view ownership

Tasks:
- rename view readiness and lifetime APIs away from `term`
- replace `struct term` and `struct term_win` storage with direct SDL view
  state
- move story-font activation and grid flags off the host object
- remove `term_init`, `term_nuke`, `sdl_view_link_term`,
  `sdl_term_host_redraw`, and `sdl_redraw_all_term_hosts`

Exit when:
- active SDL code no longer defines or references `struct term`

### F. Finish exports and docs
Goal:
- remove leftover screen-cell mirrors and bring repo guidance back in sync

Tasks:
- rewrite mini screenshot and dump helpers against semantic snapshot data
- update docs and audit baselines to the post-render-replacement reality

Exit when:
- repo guidance no longer points engineers at already-finished work

## Parallel Lanes
- Lane 1: audit and docs
- Lane 2: input stack (`src/util-input.c`, `src/dungeon.c`, prompt and command
  plumbing)
- Lane 3: compat text surface and utility screens
- Lane 4: document and widget migration
- Lane 5: SDL term-host removal

Dependencies:
- Lane 1 starts first and stays active until the end.
- Lane 2 should land before the SDL host teardown.
- Lane 3 and Lane 4 can run in parallel after the audit baseline exists.
- Lane 5 starts only after no gameplay or UI authoring path still depends on
  compat text or document cell ops.

## Validation
- Run `py -3 tools/ui_debt_audit.py --details`.
- Run the expanded terminal-debt audit from Workstream A.
- Run targeted searches for:
  - `inkey`
  - `request_command`
  - `flush(`
  - `struct term`
  - `term_ready`
  - `term_init(`
  - `term_nuke(`
  - `sdl_view_link_term`
  - `sdl_term_host_redraw`
  - `sdl_redraw_all_term_hosts`
  - `APP_UI_DOCUMENT_OP_`
  - `app_ui_panel_add_document_text`
  - `app_ui_panel_add_document_cell_ex`
  - `app_ui_panel_add_document_cursor`
  - `c_put_str`
  - `put_str(`
  - `c_prt`
  - `prt(`
  - `clear_from`
  - `text_out_to_screen`
- Build with `.\build-cmake.bat` or `cmake --build build-standard --parallel`.
- Smoke-test:
  - command input, including repeat counts and keymaps
  - object info, note info, and file viewer
  - story pages, help pages, and thrall reward selection
  - monster recall, monster list, inventory, and equipment panes
  - squelch or wizard utility screens if touched
  - resize and pane-layout changes
  - character dump generation

## Done When
- the expanded debt audit reports zero live terminal-model ownership on the
  normal SDL path
- `src/util-input.c` no longer exposes gameplay-facing `inkey()` or
  `request_command()`
- `src/util-text.c` no longer exports row or column screen-authoring APIs to
  gameplay modules
- `src/app/app-ui.*` no longer defines document row, column, cell, or cursor
  ops for normal runtime UI
- `src/sdl-main-internal.h` no longer defines `struct term` or `struct term_win`
- `src/files.c` no longer owns char or attr screen mirrors
- any remaining uses of grid or cell terminology are clearly gameplay-space
  data, not terminal UI compatibility

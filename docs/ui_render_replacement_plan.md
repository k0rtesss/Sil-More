# UI Render Replacement Plan

Status: active on April 14, 2026. This is the current finish-line plan for
SDL runtime UI replacement. It replaces historical rollout tracking.

## End State
- Ship one SDL runtime UI renderer driven by snapshots and `app_ui_scene`.
- Preserve the Sil visual language without keeping terminal-grid layout
  ownership on the normal SDL path.
- Keep dungeon scale and overlay or menu scale independent.
- Do not keep or reintroduce fallback runtime renderers, captured-term bodies,
  or no-op compatibility switches.

## Current State
- Core renderer work is already landed. Dungeon chrome is built as semantic
  chrome scenes in `src/app/app-scene-dungeon.c` and rendered in
  `src/sdl-scene-dungeon.c`, so chrome is no longer the top migration lane.
- The normal SDL path is already semantic for the main settings, help, file
  viewer, score, quest status, welcome, metarun story-statistics, inventory or
  equipment overlay, and pile-pickup surfaces. Those families should stay out
  of the active plan unless a regression appears.
- Current UI debt audit on April 14, 2026
  (`py -3 tools/ui_debt_audit.py --check`):
  - `inkey()`: 3 files / 3 matches
  - `screen_save()/screen_load()`: 0 files / 0 matches
  - direct `Term_*` render or control calls: 29 files / 159 matches
  - `get_sdl_*` / `set_sdl_*` outside platform code: 0 files / 0 matches
- Slice 1 is complete in the working tree on April 14, 2026:
  `src/util-prompt.c`, `src/object/object-ui-select.c`,
  `src/object/object-ui-enhanced.c`, and `src/cmd/ui/cmd-ui-main-menu.c`
  now wait through the shared semantic scene or session input path instead of
  owning file-local `inkey()` or `Term_xtra(TERM_XTRA_EVENT/FRESH)` loops.
- The remaining work is not architecture substrate and not settings cleanup.
  It is finish-line cleanup of a small set of SDL-path interaction loops,
  term-sized layout budgeting, item-display helpers, bespoke workflow tails,
  and stale fallback wording.

## Active Workstreams
### 1. Replace legacy blocking loops in semantic SDL scenes
Status:
- completed in the working tree on April 14, 2026

Goal:
- stop semantic overlay and modal scenes from owning `inkey()` loops or
  `Term_xtra(TERM_XTRA_EVENT/FRESH)` polling on the normal SDL path

Primary targets:
- `src/util-prompt.c`
- `src/object/object-ui-select.c`
- `src/object/object-ui-enhanced.c`
- `src/cmd/ui/cmd-ui-main-menu.c`

Exit when:
- these flows wait through the shared scene or session input path instead of
  direct blocking terminal loops
- opening and closing them does not require manual refresh polling to keep the
  scene current

### 2. Remove term-sized layout budgeting from otherwise-semantic screens
Goal:
- stop using `Term->wid`, `Term->hgt`, or `Term_get_size()` to size semantic
  scenes on the SDL path

Primary targets:
- `src/cmd/ui/cmd-ui-main-menu.c` for the About scene
- `src/ui/ui-character-screen.c`
- `src/ui/ui-death.c`
- `src/spell/spell-utility.c`

Exit when:
- scene sizing and wrapping come from semantic or frontend layout policy
  rather than terminal dimensions
- remaining terminal-size queries are legacy-only and explicitly outside the
  active SDL runtime path

### 3. Finish the item-display and subwindow tail
Goal:
- either migrate the remaining item-display helpers off term-sized row
  rendering or clearly quarantine them as legacy subwindow code

Primary targets:
- `src/object/object-ui-display.c`
- `src/ui/ui-death.c` inventory or equipment pages that still reuse
  `menu_term_width()`
- any remaining item-family call sites that depend on those helpers

Exit when:
- normal SDL item surfaces no longer depend on `menu_term_width()`,
  `Term->wid`, or row or column story-font helpers
- any remaining term-grid item renderer is clearly limited to legacy
  subwindows or debug-only paths

### 4. Close bespoke workflow tails
Goal:
- finish the remaining file-local snapshot loops in late custom workflows

Primary targets:
- `src/ui/smithing/ui-smithing-screen.c`
- `src/birth.c`
- `src/metarun.c`

Exit when:
- these flows reuse the same semantic scene and input conventions as the rest
  of SDL UI
- no bespoke workflow keeps its own refresh loop unless it is intentionally
  documented as legacy-only

### 5. Remove stale fallback surface area
Goal:
- delete stale switches, usage text, and doc wording that still imply the
  removed fallback renderer exists

Primary targets:
- `src/main.c`
- `src/runtime-cli.c`
- `src/runtime-cli.h`
- this document and any linked UI migration docs that still describe the old
  rollout state

Exit when:
- the tree no longer advertises `-X` or any other removed snapshot-renderer
  toggle
- docs describe current finish-line work only, not rollout history

## Parallel Execution
- Lane A: Workstream 1. Write set: prompt and selector or main-menu
  interaction files.
- Lane B: Workstreams 2 and 3. Write set: layout budgeting and item-display
  helpers.
- Lane C: Workstreams 4 and 5. Write set: bespoke workflow files plus CLI and
  doc cleanup.

Rules:
- Do not add or widen fallback renderers.
- Do not reintroduce `screen_save()` / `screen_load()` or captured-term
  presentation on the SDL path.
- If a slice cannot delete a bridge cleanly, stop and document the blocker
  instead of preserving the bridge.

## Validation
- Run `py -3 tools/ui_debt_audit.py --check`.
- Smoke-test every touched surface in SDL:
  - main menu
  - prompt or confirm flows
  - inventory, equipment, and floor selection
  - one item side flow such as pile pickup or activation
  - one bespoke flow if touched
- Confirm there is no stale-screen flash during menu or prompt transitions.

## Done When
- Normal SDL play uses semantic scenes for visible UI and no longer depends on
  terminal-grid input or layout ownership.
- Remaining `Term_*` code is either platform, legacy subwindow, debug, or
  intentionally retained compatibility code, not part of the active SDL
  runtime UI path.
- This plan can stay short because the remaining work is a finite cleanup
  list, not another staged migration.

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
  - direct `Term_*` render or control calls: 21 files / 128 matches
  - `get_sdl_*` / `set_sdl_*` outside platform code: 0 files / 0 matches
- Slice 1 is complete in the working tree on April 14, 2026:
  `src/util-prompt.c`, `src/object/object-ui-select.c`,
  `src/object/object-ui-enhanced.c`, and `src/cmd/ui/cmd-ui-main-menu.c`
  now wait through the shared semantic scene or session input path instead of
  owning file-local `inkey()` or `Term_xtra(TERM_XTRA_EVENT/FRESH)` loops.
- Workstream 4 is complete in the working tree on April 14, 2026:
  `src/ui/smithing/ui-smithing-screen.c`, `src/birth.c`, and
  `src/metarun.c` now reuse the shared semantic scene or wait-input helpers
  instead of bespoke workflow-scoped snapshot ownership.
- Dead snapshot-renderer fallback branches have now been deleted from the
  semantic UI entry surfaces. Remaining repo-wide audit hits are no longer
  interpreted as active SDL runtime debt by default.
- The remaining finish-line build blocker is still explicit: `src/z-term.c`
  appears in `CMakeLists.txt` under `SIL_MORE_SOURCES_LEGACY_COMPAT`, and
  `sil-legacy-compat` is still linked into `sil-more`,
  `sil-ui1-tests`, `sil-ui8-tests`, and `sil-ui8-demo-packets`.
- The real transitive pulls are now easy to name: `src/angband.h` still
  includes `src/z-term.h`, `src/sdl-main-internal.h` still includes
  `src/z-term.h`, and `src/dungeon.c` plus `src/files.c` still include
  `src/z-term.h` directly.
- The dungeon snapshot path already owns semantic map, chrome, and pane
  metadata through `src/app/app-scene-dungeon.c`, but supporting-pane content
  still refreshes through `window_stuff()` in `src/ui/ui-status.c` and the
  legacy `display_*()` term-grid renderers.
- The remaining `Term_*` concentration is mostly in legacy side-pane and
  subwindow surfaces, especially `src/ui/ui-status.c`,
  `src/object/object-ui-display.c`, `src/cmd/ui/cmd-ui-object-display.c`,
  `src/monster1.c`, `src/monster2.c`, `src/cave.c`,
  `src/ui/ui-character-screen.c`, and `src/ui/ui-look-sidebar.c`.
- Remaining non-zero audit counts are acceptable only in documented
  legacy/platform/debug families such as:
  - `src/main-sdl.c`, `src/sdl-*.c`, `src/platform-*.h`
  - legacy side-pane and subwindow renderers in `src/ui/ui-status.c`,
    `src/object/object-ui-display.c`, `src/cmd/ui/cmd-ui-object-display.c`,
    `src/monster1.c`, `src/monster2.c`, `src/cave.c`,
    `src/ui/ui-character-screen.c`, and `src/ui/ui-look-sidebar.c`
  - debug or utility paths such as `src/wizard*.c`, `src/squelch.c`,
    `src/util-text.c`, `src/obj-info.c`, `src/ui/story_font.c`, and
    `src/spell/spell-utility.c`

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
- `src/ui/ui-death.c`
- `src/spell/spell-utility.c`
- `src/obj-info.c`

Exit when:
- scene sizing and wrapping come from semantic or frontend layout policy
  rather than terminal dimensions
- remaining terminal-size queries are legacy-only and explicitly outside the
  active SDL runtime path

### 3. Finish the terminal text and story helper tail
Goal:
- migrate the remaining text helper and story-font routines off terminal-sized
  layout ownership or clearly quarantine them as legacy compatibility code

Primary targets:
- `src/ui/ui-story.c`
- `src/ui/story_font.c`
- `src/util-text.c`
- any remaining helper call sites that depend on those routines

Exit when:
- normal SDL text helpers no longer depend on `Term->wid`, `Term->hgt`, or
  `Term_get_size()`
- any remaining story-font or text helper is clearly limited to legacy or
  debug-only paths

### 4. Close bespoke workflow tails
Status:
- completed in the working tree on April 14, 2026

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
Status:
- completed in the working tree on April 14, 2026

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

### 6. Drain The Remaining Side-Pane And Subwindow Debt
Goal:
- remove the last side-pane and subwindow dependencies that still need the
  terminal compatibility surface on the normal SDL path
- move supporting-pane content ownership onto snapshot or scene data instead
  of `p_ptr->window` fan-out plus `Term_activate()` refresh loops
- split the remaining work into independent renderer lanes so agents can work
  in parallel without overlapping edits

Primary targets:
- `src/ui/ui-status.c`
- `src/object/object-ui-display.c`
- `src/cmd/ui/cmd-ui-object-display.c`
- `src/monster1.c`
- `src/monster2.c`
- `src/cave.c`
- `src/ui/ui-character-screen.c`
- `src/ui/ui-look-sidebar.c`

Exit when:
- side panes stop using `Term_activate()` / `Term_fresh()` as their refresh
  mechanism
- layout code no longer reads `Term->wid`, `Term->hgt`, or `Term_get_size()`
  for SDL-visible side panes
- `PW_INVEN`, `PW_EQUIP`, `PW_PLAYER_0`, `PW_MESSAGE`, `PW_OBJECT`,
  `PW_OVERHEAD`, `PW_MONSTER`, and `PW_MONLIST` no longer depend on
  term-grid `display_*()` content on the normal SDL path
- any remaining compatibility-only renderer is isolated away from the normal
  SDL runtime finish line

Parallel lanes:
- Lane A: inventory and equipment subwindow rendering in
  `src/object/object-ui-display.c`
- Lane B: object recall and monster recall renderers in
  `src/cmd/ui/cmd-ui-object-display.c`, `src/monster1.c`, and
  `src/monster2.c`
- Lane C: overhead-map renderer in `src/cave.c`
- Lane D: player sheet legacy path in `src/ui/ui-character-screen.c`
- Lane E: dispatcher and invalidation glue in `src/ui/ui-status.c` and
  `src/ui/ui-look-sidebar.c`

Not in this slice:
- `src/ui/ui-death.c`
- `src/ui/ui-story.c`
- `src/spell/spell-utility.c`
- `src/util-text.c`
- `src/squelch.c`
- `src/obj-info.c`
- `src/ui/story_font.c`

### 7. Remove `z-term` From The Build Graph
Goal:
- remove `src/z-term.c` from `CMakeLists.txt` and stop treating the
  `z-term` compatibility layer as part of the active runtime build

Primary targets:
- `CMakeLists.txt`
- `src/z-term.c`
- `src/z-term.h`
- direct compile-time dependencies that still force `z-term` into the build,
  especially `src/angband.h`, `src/dungeon.c`, `src/files.c`, and
  `src/sdl-main-internal.h`
- SDL implementation units that still inherit `z-term.h` through
  `src/sdl-main-internal.h`, especially `src/main-sdl.c`,
  `src/platform-frame.c`, `src/sdl-layout.c`, `src/sdl-render.c`,
  `src/sdl-scene-bootstrap.c`, `src/sdl-scene-dungeon.c`,
  `src/sdl-scene-information.c`, `src/sdl-scene-menu.c`, `src/sdl-scene.c`,
  `src/sdl-story-font.c`, `src/sdl-touch.c`, `src/sdl-ui-style.c`, and
  `src/util-message.c`
- the `sil-legacy-compat` target and every target that links it today:
  `sil-more`, `sil-ui1-tests`, `sil-ui8-tests`, and `sil-ui8-demo-packets`

Exit when:
- `src/z-term.c` no longer appears in active source lists in `CMakeLists.txt`
- `sil-more` no longer links `sil-legacy-compat` just to pull `z-term` in
- the remaining runtime and test targets build through narrower headers and
  interfaces that do not require `z-term.h` transitively through `angband.h`
- any intentionally retained legacy frontend code is either unbuilt,
  separately archived, or isolated behind a target that is not part of the
  normal SDL runtime finish line

## Parallel Execution
- Lane A: Workstream 2. Write set: non-side-pane term-size cleanup in
  `src/ui/ui-death.c`, `src/ui/ui-story.c`, `src/spell/spell-utility.c`, and
  `src/obj-info.c`.
- Lane B: Workstream 6, inventory/equipment lane. Write set:
  `src/object/object-ui-display.c`.
- Lane C: Workstream 6, object/monster recall lane. Write set:
  `src/cmd/ui/cmd-ui-object-display.c`, `src/monster1.c`, and
  `src/monster2.c`.
- Lane D: Workstream 6, overhead-map lane. Write set: `src/cave.c`.
- Lane E: Workstream 6, player-sheet and dispatcher lane. Write set:
  `src/ui/ui-character-screen.c`, `src/ui/ui-status.c`, and
  `src/ui/ui-look-sidebar.c`.
- Lane F: Workstreams 4 and 5. Write set: bespoke workflow files plus CLI and
  doc cleanup.
- Lane G: Workstream 7. Write set: build graph, `z-term`, and direct
  compile-time dependency files.

Rules:
- Do not add or widen fallback renderers.
- Do not reintroduce `screen_save()` / `screen_load()` or captured-term
  presentation on the SDL path.
- Keep the side-pane lanes isolated until the shared dispatcher contract in
  `src/ui/ui-status.c` is ready to merge.
- If a slice cannot delete a bridge cleanly, stop and document the blocker
  instead of preserving the bridge.

## Validation
- Run `py -3 tools/ui_debt_audit.py --check`.
- Smoke-test every touched surface in SDL:
  - main menu
  - prompt or confirm flows
  - inventory, equipment, and floor selection
  - message, overhead-map, monster list, monster recall, object recall, and
    player side panes
  - one item side flow such as pile pickup or activation
  - one bespoke flow if touched
- Confirm there is no stale-screen flash during menu or prompt transitions.

## Done When
- Normal SDL play uses semantic scenes for visible UI and no longer depends on
  terminal-grid input or layout ownership.
- Remaining audit hits are confined to documented platform, legacy subwindow,
  debug, or intentionally retained compatibility code, not the active SDL
  runtime UI path.
- `src/z-term.c` is no longer in `CMakeLists.txt` for the normal runtime and
  test build graph.
- This plan can stay short because the remaining work is a finite cleanup
  list, not another staged migration.

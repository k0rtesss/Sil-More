# UI Render Replacement Plan

Status: active on April 15, 2026. This is the current finish-line plan for
SDL runtime UI replacement. It replaces historical rollout tracking.

## End State
- Ship one SDL runtime UI renderer driven by snapshots and `app_ui_scene`.
- Preserve the Sil visual language without keeping terminal-grid layout
  ownership on the normal SDL path.
- Keep dungeon scale and overlay or menu scale independent.
- Remove the live `Term_*`, `inkey()`, and `z-term` compatibility stack from
  the repo's active source tree and build graph, not just from the visible SDL
  happy path.
- Do not keep or reintroduce fallback runtime renderers, captured-term bodies,
  no-op compatibility switches, or "temporary" term-mirror bridges.

## Current State
- Core renderer work is already landed. Dungeon chrome is built as semantic
  chrome scenes in `src/app/app-scene-dungeon.c` and rendered in
  `src/sdl-scene-dungeon.c`, so chrome is no longer the top migration lane.
- The normal SDL path is already semantic for the main settings, help, file
  viewer, score, quest status, welcome, metarun story-statistics, inventory or
  equipment overlay, and pile-pickup surfaces. Those families should stay out
  of the active plan unless a regression appears.
- Current UI debt audit on April 15, 2026
  (`py -3 tools/ui_debt_audit.py --check`):
  - `inkey()`: 0 files / 0 matches
  - `screen_save()/screen_load()`: 0 files / 0 matches
  - direct `Term_*` render or control calls: 8 files / 61 matches
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
- Workstream 6 is complete in the working tree on April 15, 2026:
  the side-pane and subwindow family no longer dominates the audit, and the
  former concentration in `src/ui/ui-status.c`,
  `src/object/object-ui-display.c`, `src/cmd/ui/cmd-ui-object-display.c`,
  `src/monster1.c`, `src/monster2.c`, `src/cave.c` overhead-map window code,
  `src/ui/ui-character-screen.c`, and `src/ui/ui-look-sidebar.c` has been
  drained off the normal SDL pane-refresh path.
- Workstream 7 is complete in the working tree on April 15, 2026:
  `src/cmd/item/cmd-item-core.c`, `src/cmd/movement/cmd-movement.c`,
  `src/cmd/ui/cmd-ui-knowledge.c`, `src/load.c`, `src/score/score_entry.c`,
  and `src/signals.c` no longer dominate the live debt count, so the refresh,
  clear, and panic-control tail is no longer the next blocker.
- Workstream 8 is complete in the working tree on April 15, 2026:
  `src/squelch.c`, `src/wizard1.c`, and `src/wizard2.c` no longer own the
  last repo-visible `inkey()` call sites or direct `Term_*` control paths in
  the debug/admin slice, so the audit floor for that family is now zero.
- The April 15 wrapper-input pass is complete in the working tree:
  `src/util-prompt.c` no longer exports `term_get_string()`, the remaining
  file-name, note, password, inscription, and show/find prompts now call
  `prompt_text_input()` directly, and the last user-facing
  `Term_xtra(TERM_XTRA_FRESH, ...)` fences on the SDL path have been replaced
  by narrower scene or platform-frame presentation calls.
- Dead snapshot-renderer fallback branches have now been deleted from the
  semantic UI entry surfaces. The remaining repo-wide audit hits are now
  treated as first-class removal work, not as acceptable residual debt.
- The remaining finish-line build blocker is still explicit: `src/z-term.c`
  appears in `CMakeLists.txt` under `SIL_MORE_SOURCES_LEGACY_COMPAT`, and
  `sil-legacy-compat` is still linked into `sil-more`,
  `sil-ui1-tests`, `sil-ui8-tests`, and `sil-ui8-demo-packets`.
- The real transitive pulls are now easy to name: `src/angband.h` still
  includes `src/z-term.h`, `src/sdl-main-internal.h` still includes
  `src/z-term.h`, and `src/dungeon.c` plus `src/files.c` still include
  `src/z-term.h` directly.
- The current audit understates the full removal scope because it does not
  count `Term_xtra()` or wrapper debt such as `move_cursor_relative()`,
  `restore_game_cursor()`, `print_rel()`, `lite_spot()`, `prt_map()`, or
  `display_map()`. Those wrappers are still in scope.
- Targeted search on April 15, 2026 now finds:
  - `term_get_string()`: 0 matches in `src/`
  - `Term_xtra(TERM_XTRA_FRESH, ...)`: confined to `src/z-term.c`
- Excluding the final backend files in Workstream 10, the remaining counted
  debt is now 6 files / 53 matches, concentrated in `src/cave.c`,
  `src/util-text.c`, `src/ui/story_font.c`, `src/util-editing.c`,
  `src/ui/ui-story.c`, and `src/obj-info.c`.
- The remaining term-legacy work now clusters into two families:
  - one consolidated runtime cleanup slice containing counted debt owners in
    `src/cave.c`, `src/util-text.c`, `src/ui/story_font.c`,
    `src/util-editing.c`, `src/ui/ui-story.c`, and `src/obj-info.c`, plus
    wrapper and caller cleanup around `move_cursor_relative()`,
    `restore_game_cursor()`, `print_rel()`, `lite_spot()`, `prt_map()`, and
    `display_map()` across `src/ui/ui-status.c`, `src/targeting.c`,
    `src/cmd/ui/cmd-ui-look.c`, `src/cmd/combat/cmd-combat.c`,
    `src/cmd/combat/cmd-ranged.c`, `src/spell/spell-projection.c`,
    `src/util-message.c`, `src/dungeon.c`,
    `src/cmd/movement/cmd-movement.c`, `src/cmd/movement/cmd-search.c`,
    `src/cmd/monster/cmd-monster-alert.c`, `src/cmd/world/cmd-interact.c`,
    `src/monster2.c`, `src/object/object-list.c`,
    `src/object/object-place.c`, `src/spell/spell-detection.c`,
    `src/spell/spell-terrain.c`, `src/player/player-song-effects.c`,
    `src/ui/smithing/ui-smithing-screen.c`, `src/wizard2.c`, and declaration
    surfaces in `src/externs.h`
  - the final build-graph and backend tail in `src/z-term.c`,
    `src/sdl-layout.c`, `src/sdl-render.c`, `src/main-sdl.c`, and related
    SDL term-host files
- Because the goal is now full term-legacy removal, no remaining audit family
  is treated as permanently acceptable. Some slices are sequenced later, but
  all of them are now in scope.

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

### 2. Retire shared terminal text, story, and editing helpers
Status:
- folded into consolidated Workstream 9 on April 15, 2026

Goal:
- remove the shared term-grid text, wrapping, cursor, and editing primitives
  that still underpin semantic information flows and prompt surfaces

Primary targets:
- `src/util-text.c`
- `src/ui/story_font.c`
- `src/util-editing.c`
- `src/ui/ui-story.c`
- `src/obj-info.c`
- remaining document-style callers that still depend on those helpers

Exit when:
- normal SDL document, story, prompt, and editing flows no longer depend on
  `Term_get_size()`, `Term_locate()`, `Term_what()`, `Term_erase()`,
  `Term_putstr()`, or `Term_addch()`
- story-font wrapping and text layout come from scene or frontend-side layout
  policy rather than terminal cursor state

### 3. Remove main-map and cursor term mirroring
Status:
- folded into consolidated Workstream 9 on April 15, 2026

Goal:
- stop mirroring dungeon map, cursor, and transient projectile highlights
  through the term buffer on the SDL runtime path

Primary targets:
- `src/cave.c`
- `src/ui/ui-status.c` map redraw path
- `src/targeting.c`
- `src/cmd/ui/cmd-ui-look.c`
- `src/cmd/combat/cmd-combat.c`
- `src/cmd/combat/cmd-ranged.c`
- `src/spell/spell-projection.c`
- `src/util-message.c`
- high-frequency `lite_spot()` or `print_rel()` callers that still expect
  direct term animation

Exit when:
- `move_cursor_relative()`, `restore_game_cursor()`, `print_rel()`,
  `lite_spot()`, `prt_map()`, and `display_map()` are gone from the normal SDL
  runtime path
- map invalidation, cursor visibility, and transient combat or projectile
  overlays flow through snapshot or scene state rather than queued term cells

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
Status:
- completed in the working tree on April 15, 2026

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

### 7. Drain refresh, clear, and panic-control tails
Status:
- completed in the working tree on April 15, 2026

Goal:
- remove the remaining file-local `Term_fresh()`, `Term_clear()`,
  `Term_putstr()`, and similar control calls that still paper over SDL state
  transitions after semantic UI work

Primary targets:
- `src/cmd/item/cmd-item-core.c`
- `src/cmd/movement/cmd-movement.c`
- `src/cmd/ui/cmd-ui-knowledge.c`
- `src/load.c`
- `src/score/score_entry.c`
- `src/signals.c`

Exit when:
- normal SDL command and info flows do not need manual `Term_fresh()` or
  `Term_clear()` fences after `handle_stuff()` or semantic scene presentation
- panic or interrupt messaging is routed through a narrower host or platform
  surface instead of term writes

### 8. Remove debug and admin term debt
Status:
- completed in the working tree on April 15, 2026

Goal:
- delete the remaining term-grid control and `inkey()` dependencies in debug
  or admin-only tools so the repo-wide debt count can actually reach zero

Primary targets:
- `src/squelch.c`
- `src/wizard1.c`
- `src/wizard2.c`

Exit when:
- the UI debt audit reports zero `inkey()` call sites
- wizard and squelch flows no longer use direct `Term_*` control or rendering

### 9. Drain all remaining runtime term debt before backend removal
Status:
- active

Goal:
- remove every remaining non-backend term-legacy artifact in one coordinated
  pass before the final `z-term` / SDL term-host teardown
- absorb the residual work previously tracked separately under Workstreams 2,
  3, and the already-landed wrapper-input pass

Primary targets:
- counted debt owners in `src/cave.c`, `src/util-text.c`,
  `src/ui/story_font.c`, `src/util-editing.c`, `src/ui/ui-story.c`, and
  `src/obj-info.c`
- wrapper and caller integration in `src/ui/ui-status.c`, `src/targeting.c`,
  `src/cmd/ui/cmd-ui-look.c`, `src/cmd/combat/cmd-combat.c`,
  `src/cmd/combat/cmd-ranged.c`, `src/spell/spell-projection.c`,
  `src/util-message.c`, `src/dungeon.c`,
  `src/cmd/movement/cmd-movement.c`, `src/cmd/movement/cmd-search.c`,
  `src/cmd/monster/cmd-monster-alert.c`, `src/cmd/world/cmd-interact.c`,
  `src/monster2.c`, `src/object/object-list.c`,
  `src/object/object-place.c`, `src/spell/spell-detection.c`,
  `src/spell/spell-terrain.c`, `src/player/player-song-effects.c`,
  `src/ui/smithing/ui-smithing-screen.c`, `src/wizard2.c`, and
  declaration surfaces in `src/externs.h`

Exit when:
- `py -3 tools/ui_debt_audit.py --details` drops from `8 files / 61 matches`
  to the final backend-only floor in `src/sdl-layout.c` and
  `src/sdl-render.c`
- targeted searches for `move_cursor_relative`, `restore_game_cursor`,
  `print_rel`, `lite_spot`, `prt_map`, and `display_map` are empty outside
  archived code or the final backend boundary
- runtime document, story-font, object-info, map redraw, cursor, and
  projectile or combat-highlight paths no longer depend on term-grid state

### 10. Remove `z-term` and the SDL term backend from the build graph
Goal:
- remove `src/z-term.c` from `CMakeLists.txt`, stop treating the `z-term`
  compatibility layer as part of the active runtime build, and delete the SDL
  term-host backend that still exists only to serve it

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
- `src/sdl-layout.c`, `src/sdl-render.c`, and the remaining SDL frontend no
  longer create or resize `term` hosts for rendering
- the remaining runtime and test targets build through narrower headers and
  interfaces that do not require `z-term.h` transitively through `angband.h`
- any intentionally retained legacy frontend code is either unbuilt,
  separately archived, or isolated behind a target that is not part of the
  normal SDL runtime finish line

## Parallel Execution
- Lane A: Workstream 9, text/story/editing core. Write set:
  `src/util-text.c`, `src/ui/story_font.c`, `src/util-editing.c`,
  `src/ui/ui-story.c`, and `src/obj-info.c`.
- Lane B: Workstream 9, map/cursor core. Write set: `src/cave.c`,
  `src/ui/ui-status.c`, `src/targeting.c`, `src/cmd/ui/cmd-ui-look.c`,
  `src/cmd/combat/cmd-combat.c`, `src/cmd/combat/cmd-ranged.c`,
  `src/spell/spell-projection.c`, `src/util-message.c`, and
  `src/externs.h`.
- Lane C: Workstream 9, caller sweep. Write set: `src/dungeon.c`,
  `src/cmd/movement/cmd-movement.c`, `src/cmd/movement/cmd-search.c`,
  `src/cmd/monster/cmd-monster-alert.c`, `src/cmd/world/cmd-interact.c`,
  `src/monster2.c`, `src/object/object-list.c`, `src/object/object-place.c`,
  `src/spell/spell-detection.c`, `src/spell/spell-terrain.c`,
  `src/player/player-song-effects.c`, `src/ui/smithing/ui-smithing-screen.c`,
  and `src/wizard2.c`.
- Lane D: Workstream 10. Write set: build graph, `z-term`, SDL term backend,
  and direct
  compile-time dependency files.

Rules:
- Do not add or widen fallback renderers.
- Do not reintroduce `screen_save()` / `screen_load()` or captured-term
  presentation on the SDL path.
- Do not preserve `print_rel()`, `lite_spot()`, `prt_map()`, or similar
  wrappers as permanent no-op bridges on the SDL path.
- If a slice cannot delete a bridge cleanly, stop and document the blocker
  instead of preserving the bridge.

## Validation
- Run `py -3 tools/ui_debt_audit.py --check`.
- Run targeted searches for debt the audit does not count, especially:
  - `move_cursor_relative`
  - `restore_game_cursor`
  - `print_rel`
  - `lite_spot`
  - `prt_map`
  - `display_map`
  - `z-term.h`
- Smoke-test every touched surface in SDL:
  - main menu
  - prompt or confirm flows
  - object info or other document-style scenes
  - look or targeting, including cursor movement
  - map redraw, movement, and one projectile or combat animation
  - inventory, equipment, and floor selection
  - message, monster recall, object recall, and player document screens
  - one wrapper-heavy path such as search, detection, or wizard map debug if
    touched
- Confirm there is no stale-screen flash during menu or prompt transitions.

## Done When
- `py -3 tools/ui_debt_audit.py --details` reports:
  - `inkey()`: 0 files / 0 matches
  - `screen_save()/screen_load()`: 0 files / 0 matches
  - direct `Term_*` render or control calls: 0 files / 0 matches
- targeted searches for `Term_xtra`, `term_get_string`, `move_cursor_relative`,
  `restore_game_cursor`, `print_rel`, `lite_spot`, `prt_map`, `display_map`,
  and `z-term.h` are either empty or intentionally confined to archived or
  unbuilt code.
- `src/z-term.c` is no longer in `CMakeLists.txt` for the normal runtime and
  test build graph, and the SDL frontend no longer renders through `term`
  hosts.
- This plan can stay short because the remaining work is a finite cleanup
  list, not another staged migration.

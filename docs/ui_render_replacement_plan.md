# UI Render Replacement Plan

Status: active on April 3, 2026. This plan supersedes
[`ui_architecture_migration_plan.md`](./ui_architecture_migration_plan.md) for
all forward UI work.

## Why This Replaces The Old Plan
- The old plan succeeded at substrate work:
  - `src/app/*` is real
  - the build split is real
  - the SDL dungeon renderer is snapshot-driven
  - the menu renderer is semantic enough to prove the direction
- The old plan is no longer useful as the execution document:
  - too much of it is historical
  - too much of it mixes bridge work with final architecture
  - the remaining work is smaller in shape and much clearer
- The real remaining problem is now simple to state:
  - replace runtime legacy `Term` UI with one semantic UI system
  - delete term-grid bridges instead of carrying them forward

## Current Status On 2026-04-03
### Landed
- `src/app/*` already carries session, input, snapshot, event, and host
  surfaces.
- `CMakeLists.txt` already splits `sil-core`, `sil-legacy-compat`, and
  `sil-platform-sdl`.
- `src/sdl-scene-dungeon.c` already renders the main dungeon from snapshots.
- `src/app/app-scene-menu.*` plus `src/sdl-scene-menu.c` already prove a
  semantic SDL renderer path for menus.
- `tools/ui_debt_audit.py` and `tests/ui_debt_audit_baseline.json` already give
  a measurable debt baseline.
- Independent scaling already exists in seed form:
  - `main_view_scale` for the main dungeon view
  - `menu_panel_font_size` for menus and left-panel text

### Not Landed
- Overlay payloads are still partly term-grid contracts:
  - `src/app/app-scene-dungeon.h` still bakes in
    `APP_DUNGEON_LEFT_PANEL_COLS`
  - overlay panels still expose `CELL_GRID`, `grid_cols`, `reserve_cells`,
    `cell_rows`, and `cell_cols`
- The menu path still contains compatibility modes instead of one final model:
  - `APP_MENU_SCENE_FLAG_PLAIN`
  - `APP_MENU_SCENE_FLAG_LEGACY_SIDEBAR`
  - `APP_MENU_SCENE_FLAG_USE_LEGACY_BACKDROP`
- The information-scene bridge is still a real runtime dependency:
  - `ui_information_scene_capture_term()`
  - `ui_information_scene_present_term()`
  - `APP_INFORMATION_SCENE_FLAG_TERM_MIRROR`
- Large UI families still own layout through `Term->wid`, `Term->hgt`,
  `screen_save()`, `screen_load()`, or blocking `inkey()` loops.

### Remaining Debt Snapshot
- `inkey()` call sites: 39 files / 89 matches
- `screen_save()` + `screen_load()` call sites: 33 files / 215 matches
- direct `Term_*` render/control calls: 65 files / 1,697 matches
- `#include "platform-ui.h"`: 0 files / 0 matches
- `get_sdl_*` / `set_sdl_*` outside platform code: 6 files / 208 matches

## Decisions
### 1. One Runtime UI System
- The SDL path will have exactly one runtime renderer for UI.
- Once a screen or UI family is migrated, the legacy `Term` render path for
  that family must be deleted rather than kept as a fallback.
- Legacy visuals may remain as a reference during migration, but not as a
  second shipped renderer.

### 2. One Semantic UI Model For Everything
- All non-map UI must converge on one shared semantic UI model.
- Do not keep dungeon overlays, menu scenes, document browsers, and bespoke
  workflows on separate long-term payload formats.
- Future work should add a new shared `app-ui-*` layer under `src/app/` rather
  than stretching `app_menu_scene` and `app_interaction_state` with more
  special flags.

### 3. Preserve The Look, Not The Terminal Contract
- Keep the classic Sil visual language:
  - colors
  - density
  - overall visual tone
  - panel hierarchy
- Treat that as the default shipped style, not as a permanent typography or
  layout cage.
- The architecture must support future style directions, including non-mono
  fonts and different panel treatments.
- Fonts should render as they are actually designed to render. Do not force UI
  typography into fake terminal-style mono-cell spacing unless a specific style
  intentionally asks for that.
- Do not preserve:
  - fixed terminal column budgets
  - hard text width limits derived from `Term`
  - layout rules that only exist because of the old grid

### 4. Remove `Term` Grid From UI Contracts
- The dungeon remains a gameplay cell map, but UI is no longer a `Term` cell
  grid.
- Left rail, message strip, footer, browsers, prompts, minimap, and modals
  must be described semantically and laid out in logical pixels.
- Raw cell dumps, mirrored term buffers, and row/column overlay contracts are
  bridge-only debt and must not be extended.

### 5. Scale Policy
- The main dungeon view stays integer-scaled from tile metrics.
- Overlay, browser, and menu UI scale independently from the dungeon view.
- Non-map UI does not need integer scale coupling to `main_view_scale`.
- Non-map tile, icon, and embedded-surface rendering may scale non-integerly.
- Menus and overlays should be able to render tiles as first-class content, not
  just text.
- The minimap should become a real minimap widget rather than a terminal clone,
  and it should be allowed to scale non-integerly.
- Changes to overlay scale must not move or resize the main dungeon viewport
  except where persistent chrome intentionally reserves space.

### 6. Execution Strategy
- Do not take the smallest safe UI slice.
- Do not port one overlay at a time by copying terminal coordinates into SDL.
- Start with the biggest architectural slices:
  - shared semantic UI engine
  - persistent chrome
  - list-detail and document shells
- After that, migrate whole UI families and delete their bridge code.

## Target Architecture
### Core Owns
- gameplay and persistence state
- snapshot and event generation
- semantic UI scene data
- focus, selection, scroll, active actions, prompt state, and widget identity
- stable IDs so the frontend can animate and restore focus without guessing

### Frontend Owns
- layout in logical pixels
- theme application and typography buckets
- viewport reservation for persistent chrome
- animation, transitions, and hit-testing
- main-view integer scaling and independent overlay scaling

### New Shared UI Layer
- Add a new shared UI layer under `src/app/`, named `app-ui-*`.
- Freeze the older bridge-shaped payloads:
  - `app_menu_scene`
  - `app_interaction_state.panel`
  - `app_dungeon_overlay_panel_snapshot` cell-grid fields
  - `app_information_scene`
- The new layer should describe one `app_ui_scene` with layered ownership:
  - persistent chrome
  - transient overlay
  - modal overlay
  - full-screen browser or workflow

### Required Semantic Primitives
- panel / rail / strip containers
- text runs and icons
- tile runs, tile thumbnails, and mixed text-plus-tile rows
- stat rows and meters
- footer action rows
- list, list-detail, and table widgets
- document and scroll widgets
- tab sets
- minimap and other non-text embed surfaces
- stable selection, focus, and scroll state

The shared UI model must support tile-bearing menus and browsers directly.
Minimap-style surfaces should be described semantically as dedicated widgets,
not as mirrored terminal regions.

These are semantic widgets, not raw glyph dumps.

## Freeze List
Do not add new SDL-path dependencies on:
- `ui_information_scene_capture_term()`
- `ui_information_scene_present_term()`
- `ui_information_scene_enter_mirror()`
- `APP_INFORMATION_SCENE_FLAG_TERM_MIRROR`
- `APP_DUNGEON_OVERLAY_PANEL_FLAG_CELL_GRID`
- `APP_DUNGEON_LEFT_PANEL_COLS`
- `app_interaction_panel_snapshot`
- `APP_MENU_SCENE_FLAG_PLAIN`
- `APP_MENU_SCENE_FLAG_LEGACY_SIDEBAR`
- `APP_MENU_SCENE_FLAG_USE_LEGACY_BACKDROP`
- SDL layout derived from `Term->wid` or `Term->hgt`

## Execution Program
### Slice A: Shared Semantic UI Engine
Goal:
- replace the current split between dungeon overlay payloads, menu payloads,
  and information-scene bridges with one shared semantic UI system

Primary write set:
- `src/app/*`
- `src/sdl-scene.c`
- `src/sdl-scene-dungeon.c`
- `src/sdl-scene-menu.c`
- `src/sdl-ui-style.c`

Deliverables:
- new `app-ui-*` scene and widget payloads
- one SDL compositor for chrome, modal, and browser surfaces
- theme tokens that reproduce the current Sil look without inheriting terminal
  layout rules
- first-class widgets for:
  - left rail
  - top message strip
  - bottom action or prompt bar
  - centered modal
  - list-detail browser
  - document screen

Exit when:
- the same compositor can render in-play chrome, a centered modal, a list-detail
  browser, and a document screen
- no new work depends on menu `PLAIN` or `LEGACY_SIDEBAR` modes
- overlay layout is expressed in logical pixels instead of term columns

### Slice B: Gameplay UI Family
Goal:
- migrate the entire in-play UI family onto the shared semantic UI system

Primary write set:
- `src/ui/ui-status.c`
- `src/util-message.c`
- `src/util-prompt.c`
- `src/runtime/runtime-game.c`
- `src/object/*`
- `src/cmd/item/*`
- `src/cmd/ui/cmd-ui-look.c`
- `src/ui/ui-look-sidebar.c`
- `src/targeting.c`

Deliverables:
- persistent chrome on the shared UI system
- prompt, confirm, quantity, text-entry, and list-selection on the same system
- inventory, equipment, identify, compare, and item actions on the same system
- look, targeting, sidebar, and minimap on the same system
- a real minimap widget with non-integer scaling support on the non-dungeon UI
  path

Delete during this slice:
- hidden left-panel overlay globals
- term-grid overlay panel contracts
- raw interaction panel grids
- transient menu overlay behavior used only to paper over legacy ownership

Exit when:
- normal SDL play no longer depends on `screen_save()` or `screen_load()` for
  these flows
- no normal SDL play path for these flows depends on `ui_information_scene`
- no layout decision for these flows depends on `Term->wid` or `Term->hgt`

### Slice C: Browser And Data Screen Family
Goal:
- migrate all menu and document-style screens onto the shared semantic UI
  system and delete the term-mirror bridge

Primary write set:
- `src/cmd/ui/cmd-ui-main-menu.c`
- `src/ui/ui-help.c`
- `src/ui/ui-file-viewer.c`
- `src/cmd/ui/cmd-ui-settings.c`
- `src/cmd/ui/cmd-ui-character.c`
- `src/ui/ui-character-screen.c`
- `src/cmd/ui/cmd-ui-knowledge.c`
- `src/score/score_ui.c`
- `src/quest/quest-ui.c`
- `src/cmd/ui/cmd-ui-nearby.c`
- `src/obj-info.c`

Deliverables:
- shared document, list-detail, table, and tab widgets
- unified browser shell for help, knowledge, settings, character, score, and
  quest screens
- main menu on the same UI system, not on a compatibility renderer
- tile-capable menu and browser rows so SDL menus can render item, monster, and
  other visual content directly

Delete during this slice:
- `app_information_scene`
- `ui_information_scene_*`
- term-mirror presentation paths in migrated screens

Exit when:
- migrated browsers no longer use `present_term()` or mirrored term capture
- the information-scene bridge is gone from normal SDL runtime UI

### Slice D: Bespoke Workflow Family And Final Removal
Goal:
- migrate the remaining bespoke workflows and finish runtime legacy UI removal

Primary write set:
- `src/birth.c`
- `src/ui/smithing/ui-smithing-screen.c`
- `src/metarun.c`
- `src/ui/ui-story.c`
- `src/ui/ui-death.c`
- `src/blitz.c`
- any remaining UI-heavy legacy modules still owning screen overlays

Deliverables:
- bespoke controllers implemented on the same semantic widgets
- removal of remaining runtime legacy UI ownership from `sil-legacy-compat`
- final deletion of bridge flags, mirror helpers, and term-grid overlay
  contracts

Exit when:
- runtime SDL UI no longer depends on legacy `Term` rendering for menus,
  overlays, browsers, or workflows
- `Term` is no longer the contract for any UI surface

## Mandatory Deletions
These are not permanent compatibility shims. They should be removed as their
replacements land.

- `APP_DUNGEON_LEFT_PANEL_COLS`
- `APP_DUNGEON_OVERLAY_PANEL_FLAG_CELL_GRID`
- overlay panel `cell_rows`, `cell_cols`, and embedded raw cell arrays
- `app_interaction_panel_snapshot`
- `APP_MENU_SCENE_FLAG_PLAIN`
- `APP_MENU_SCENE_FLAG_LEGACY_SIDEBAR`
- `APP_MENU_SCENE_FLAG_USE_LEGACY_BACKDROP`
- `APP_INFORMATION_SCENE_FLAG_TERM_MIRROR`
- `ui_information_scene_capture_term()`
- `ui_information_scene_present_term()`
- hidden left-panel overlay globals in `src/ui/ui-status.c`

## Validation Gates
- `main_view_scale` changes dungeon rendering only.
- overlay or browser scale changes non-map UI only.
- migrated UI families have one runtime renderer, not two.
- migrated UI families no longer derive SDL layout from `Term->wid` or
  `Term->hgt`.
- visual parity is judged against the existing Sil presentation, not against
  terminal-era width restrictions.
- run:
  - `py -3 tools/ui_debt_audit.py`
  - `ctest -R sil_ui0_audit --output-on-failure`
- smoke-test:
  - inventory and equipment
  - look and targeting
  - message strip, prompts, and combat overlay
  - main menu, help, settings, knowledge, character, and score screens
  - story, death, metarun, birth, blitz, and smithing

## Success Condition
This program is finished when the SDL runtime UI keeps the current Sil look,
scales the dungeon and the rest of the UI independently, and no shipped UI
surface still depends on `Term` rendering, term mirroring, or term-grid layout
contracts.

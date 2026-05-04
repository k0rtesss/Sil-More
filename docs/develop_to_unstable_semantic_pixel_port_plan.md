# Develop To Unstable Semantic Pixel Port Plan

Status: implemented integration plan, created and executed May 4, 2026.

## Goal

Move the remaining `develop` updates into `unstable` without regressing the
terminal-free SDL architecture already present on `unstable`.

The `develop` branch is treated as the behavioral source of truth for new
features, bug fixes, balance, assets, platform work, touch/mouse behavior, and
the meta-state system. It is not treated as a renderer source of truth where it
uses terminal-era UI, cell-grid overlay, `Term_*`, `screen_save()` /
`screen_load()`, `inkey()`, or faux terminal layout logic.

## Baseline

- Target branch at planning time: `unstable` at `c1cc36e4`.
- Source branch at planning time: `develop` at `0729c70e`.
- Merge base: `a1e36fce5904b3b91796e136a2638c95a6a6f0b1`.
- Source delta size: about 478 paths, with large changes in SDL input/UI,
  meta-state, content data, audio assets, Android/iOS packaging, scoring,
  metarun, object/combat/generation logic, and docs.
- Existing target architecture: `unstable` has split core modules, `src/app/*`
  semantic scenes, SDL scene rendering, semantic UI command dispatch, and
  terminal-kernel audit locked to zero.

## Hard Rules

1. Do not merge `develop` wholesale into `unstable`.
2. Do not copy terminal-based UI loops into the normal SDL path.
3. Port terminal-based behavior as semantic data plus semantic pixel rendering:
   `app_ui_scene`, `app_ui_panel`, `app_ui_command`, dungeon overlay scenes,
   SDL menu pointer hit registration, and map pointer helpers are the target
   substrate.
4. Keep gameplay logic outside SDL-facing files unless it is input binding,
   hit-testing, frontend rendering, or platform configuration.
5. Keep source template IDs and GUIDs stable; append or preserve, do not
   renumber.
6. Save/scoring/metarun persistence changes must be version-gated and
   backward-compatible.
7. If a `develop` change touches smithing difficulty, update both the engine
   implementation and `scripts/calc_artefact_difficulty.py`.
8. After each lane, run the narrow build/test gate for the touched files. Before
   integration closeout, run the full validation suite.

## Port Classification

### A. Mechanical Or Mostly Direct Ports

- Content templates under `lib/edit/`.
- Sound config and assets under `lib/pref/`, `lib/xtra/music/`,
  `lib/xtra/sound/`, and icon/copying files.
- Pure gameplay fixes where the current `unstable` file still owns the same
  logic.
- Android/iOS/build script updates that do not fight the current CMake split.
- Documentation from `develop`, after checking it is not stale against
  `unstable`.

### B. Adapted Ports

- Meta-state system, because `unstable` has split metarun, score, persistence,
  runtime, and UI ownership since `develop`.
- Object/drop/supplies/smithing changes, because `unstable` has moved code into
  `src/object/*`, `src/drop/*`, `src/smithing/*`, and `src/cmd/item/*`.
- Combat, movement, targeting, and generation fixes, because `unstable` has
  split `cmd1.c`/`cmd2.c`/`spells*.c`/`generate.c` into domain modules.
- Score and metarun UI updates, because `unstable` uses semantic browser
  surfaces.

### C. Rewrite-As-Semantic Ports

- Mouse/touch menu navigation from `develop`.
- Pane buttons, touch panes, swipe/wheel behavior, long tap, hover look, l-view,
  map click movement, skeleton-note clicks, character sheet mouse zones, scores
  and run-history pointer controls, and mobile overlays.
- Any "draw a button at terminal row/col" logic. The port target is semantic
  widgets with stable IDs, logical-pixel layout, pointer hit rectangles, focus
  state, and semantic commands.

## Workstreams And Agents

All agents use GPT-5.5 as requested. Reasoning level is `high` unless noted.

### Lane 1: Meta-State Core And Persistence

Owner model: GPT-5.5, high reasoning.

Primary source in `develop`:
- `src/meta_state.[ch]`
- `src/metarun*`
- `src/score/*`
- `src/save.c`, `src/load.c`
- `src/player/killer.*`
- `src/types.h`, `src/defines.h`

Target write set:
- New or adapted meta-state module under current `unstable` architecture.
- `src/metarun/*`, `src/score/*`, `src/fs/save*`, `src/fs/load*`,
  `src/player/killer.*`, and tightly required declarations.

Responsibilities:
- Port meta-state data model, lifecycle hooks, scoring integration, and
  persistence into the split target modules.
- Add compatibility defaults for old saves.
- Keep semantic metarun/score UI boundaries intact; do not port terminal
  renderers.
- Produce an integration note listing any shared header or version changes
  needed if they cannot be completed locally without conflicts.

Exit gate:
- Meta-state code compiles in the target split.
- Old saves default missing meta-state safely.
- Score/metarun tests or focused compile targets pass.

### Lane 2: Gameplay, Balance, Data, And Generation

Owner model: GPT-5.5, high reasoning.

Primary source in `develop`:
- `lib/edit/*.txt`
- `src/cmd1.c`, `src/cmd2.c`, `src/cmd3.c`, `src/cmd6.c`
- `src/drop_system.c`
- `src/generate.c`
- `src/object*.c`, `src/obj-info.c`, `src/supplies.c`
- `src/spells*.c`, `src/melee*.c`, `src/monster*.c`
- `src/thrall_quest.c`

Target write set:
- `lib/edit/*`
- `src/drop/*`
- `src/object/*`
- `src/cmd/item/*`, `src/cmd/world/*`, `src/cmd/movement/*`,
  `src/cmd/combat/*`
- `src/level-generation/*`, `src/spell/*`, `src/melee/*`, `src/monster/*`,
  `src/quest/*`, `src/object/supplies.c`

Responsibilities:
- Port non-UI bug fixes and balance changes into the split target modules.
- Preserve template serials and GUIDs.
- Keep smithing difficulty engine/script sync.
- Treat UI strings/prompts as behavior requirements only; use existing semantic
  prompt APIs where user-facing flow is touched.

Exit gate:
- Template parsers still load.
- Drop/object/smithing touched files compile.
- `tools/make_guid.py --dry-run` reports no required GUID churn unless new
  entries intentionally need GUIDs.

### Lane 3: Semantic Pointer, Touch, Pane, And Map Input

Owner model: GPT-5.5, high reasoning.

Primary source in `develop`:
- `src/main-sdl.c`
- `src/sdl-config.[ch]`
- `src/pane.[ch]`
- touch/mouse commits including pane buttons, swipes, wheel direction, hover
  look, long tap, map arrows, main-game highlights, and mobile overlays.

Target write set:
- `src/sdl-touch.c`
- `src/sdl-layout.c`
- `src/sdl-config.[ch]`
- `src/sdl-menu/*`
- `src/sdl-scene-dungeon.c`
- `src/app/app-ui-command.*`
- `src/app/app-movement.*`
- `src/pane-config.h`, `src/pane.c`, `src/pane.h` only if needed.

Responsibilities:
- Port input behavior as semantic commands and logical-pixel hit-testing.
- Route pointer/touch/gamepad operations through shared command IDs where
  possible.
- Preserve keyboard behavior and current semantic dispatch.
- Avoid synthetic terminal key injection as the final implementation; only use
  existing compatibility layers where a target flow has not yet been migrated.

Exit gate:
- `uix_semantic` and terminal audits do not regress.
- Map pointer/touch flow compiles and keeps pixel hit rectangles independent of
  terminal columns.

### Lane 4: Semantic UI Screens And Menus

Owner model: GPT-5.5, high reasoning.

Primary source in `develop`:
- `src/cmd4.c`, `src/score/score_ui.c`, `src/metarun.c`, `src/birth.c`,
  object/inventory UI changes, character sheet mouse zones, songs and ability
  info, scores, main menu, Mandos/run history, story statistics.

Target write set:
- `src/cmd/ui/*`
- `src/object/object-ui-*`
- `src/ui/*`
- `src/score/score_ui*`
- `src/metarun/metarun-ui-*`
- `src/runtime/runtime-dungeon-presentation.c`

Responsibilities:
- Port user-visible menu/screen features to `app_ui_scene` and shared browser
  shells.
- Add stable widget IDs, footer actions, tabs, scroll regions, and semantic
  command handling for pointer/touch.
- Do not reintroduce terminal row/column draw loops.
- Coordinate with Lane 1 before touching score/metarun persistence or state.

Exit gate:
- Touched screens build.
- No new normal-path `Term_*`, `screen_save()`/`screen_load()`, or `inkey()`
  call sites.
- Pointer/touch activation has semantic action coverage rather than only
  keyboard fallback labels.

### Lane 5: Platform, Audio, Assets, Packaging, And Docs

Owner model: GPT-5.5, medium reasoning.

Primary source in `develop`:
- `android/*`, `ios/*`, `build-*.ps1`, `create-*.ps1`
- `lib/xtra/music/*`, `lib/xtra/sound/*`, `lib/pref/sound.json`
- icons, copying files, README/docs.

Target write set:
- Platform scripts and project metadata.
- Audio/music/icon assets and sound config.
- Docs that remain accurate for `unstable`.

Responsibilities:
- Port asset/config/package updates without disturbing generated release
  artifacts.
- Keep build scripts compatible with the current split CMake targets.
- Ensure `sound.json` paths match deployed `lib/xtra/` layout.
- Do not remove current docs that describe the semantic architecture unless
  replacing them with accurate updated docs.

Exit gate:
- Standard and portable staging still copy required runtime assets.
- Sound config references existing files.
- Android/iOS script changes are syntactically valid.

### Lane 6: Integration And Quality Control

Owner: main agent.

Responsibilities:
- Maintain this plan and lane status.
- Review agent outputs for architecture violations and file conflicts.
- Integrate lane patches in this order:
  1. Lane 5 low-risk assets/config/docs that do not affect code.
  2. Lane 2 content/data and isolated gameplay fixes.
  3. Lane 1 meta-state core/persistence.
  4. Lane 3 semantic pointer/touch substrate.
  5. Lane 4 screens using Lane 3 commands.
  6. Final shared-header, CMake, save-version, and audit baseline cleanup.
- Run validation gates after each integration step.
- Reject or rewrite any copied terminal UI code.

## Merge Strategy

1. Use `git show develop:<path>` and commit-range inspection as references.
   Avoid branch merges and automated conflict resolutions.
2. For renamed/split target modules, port behavior by function ownership:
   old `cmd1.c` movement/combat goes to `src/cmd/movement/*` or
   `src/cmd/combat/*`; old object files go to `src/object/*`; old save/load
   goes to `src/fs/*`; old generation goes to `src/level-generation/*`.
3. Keep shared declarations small. Prefer static helpers or domain headers over
   adding globals to `externs.h`.
4. Update `CMakeLists.txt` only for genuinely new source files.
5. When a `develop` UI change contains both behavior and rendering, extract the
   behavior first, add semantic scene/widget data second, and render it through
   the existing SDL semantic renderer.
6. Every lane must report:
   - source commits or files examined
   - target files changed
   - terminal/UI code intentionally not copied
   - validation commands run and results

## Validation Matrix

Run after narrow lane integration:
- `cmake --build build-standard --parallel`
- focused object compile targets where useful
- `py -3 tools/ui_debt_audit.py --check`
- `py -3 tools/ui_debt_audit.py --audit terminal_kernel --details`
- `py -3 tools/ui_debt_audit.py --audit uix_semantic --details`
- `python tools/make_guid.py --dry-run`

Run before final closeout:
- `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1`
- `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1 -Target portable`
- `ctest --preset test-standard --output-on-failure` if configured
- Run standard and portable executables; check adjacent `log.txt`.

Smoke test checklist:
- keyboard movement, repeat counts, keymaps
- controller movement and prompts
- mouse/touch map hover, adjacent movement/interact, long press, wheel/scroll
- panes: show/hide, resize, drag/pin/reset if enabled
- inventory/equipment, floor item `-)` shortcut, item examine/identify
- look/targeting/sidebar/minimap
- main menu, settings, help/file viewer, character/abilities/songs
- score, Mandos, run history, metarun stats and blessing flows
- save/load with old save, death/victory, post-run restart
- sound startup, SFX/music paths, Android/iOS packaging scripts where available

## Current Risks

- The largest conflict risk is old `develop` UI code versus terminal-free
  `unstable` rendering. Treat those changes as specs only.
- The meta-state system may overlap with metarun and score refactors already
  present on `unstable`; persistence must be audited carefully.
- Many old `develop` gameplay changes were made before the current module
  split, so direct patches will land in the wrong files unless manually mapped.
- Asset deltas include large binary adds/deletes. They should be reviewed for
  deployment impact instead of blindly mirroring the branch.
- Build scripts in `develop` may assume old source lists or deployment paths.

## Status Log

- 2026-05-04: Plan created. Parallel GPT-5.5 lanes to be launched from this
  document.
- 2026-05-04: Launched five GPT-5.5 workers:
  - Lane 1 meta-state/persistence: GPT-5.5 high reasoning.
  - Lane 2 gameplay/data/generation: GPT-5.5 high reasoning.
  - Lane 3 semantic pointer/touch/panes/map input: GPT-5.5 high reasoning.
  - Lane 4 semantic UI screens/menus: GPT-5.5 high reasoning.
  - Lane 5 platform/assets/packaging/docs: GPT-5.5 medium reasoning.
- 2026-05-04: Target audit baseline before integration:
  - `py -3 tools\ui_debt_audit.py --check` passed.
  - `terminal_kernel` audit is zero.
  - `uix_semantic` matches baseline: 8 action-key fallback reads and 18 legacy
    bridge queue calls.
- 2026-05-04: Integrated all five GPT-5.5 lane outputs into the current
  `unstable` module split. Terminal-era UI behavior from `develop` was treated
  as behavior/spec input only; normal-path SDL work stayed on semantic command,
  scene, pane, and logical-pixel hit-test surfaces.
- 2026-05-04: Completed the post-agent runtime hook pass:
  - Meta-state persistence and runtime artifacts are under
    `src/metarun/metarun-meta-state.*`, with monster death event glue split into
    `src/metarun/metarun-monster-events.c`.
  - Remembered artifacts are injected into runtime artifact slots and appended
    to the drop catalog without serializing them into `drops.raw`.
  - Revenge monster state now affects player attack/evasion, monster runtime
    scaling, death memories, monster recall text, and revenge kill progression.
  - Legendary dungeon areas are captured from semantic map state, restored from
    saves, spawned through `src/level-generation/level-generation-legendary.c`,
    and tracked on player entry without terminal overlays.
  - Song observation is wired through semantic gameplay events in song effects,
    song duels, and Song of Mastery handling.
  - Touch/pointer/pane updates landed through SDL config/defaults and
    semantic touch handling rather than terminal row/column rendering.
- 2026-05-04: Final validation passed:
  - `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1`
  - `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1 -Target portable`
  - `ctest --preset test-standard --output-on-failure` passed 9/9 tests.
  - `py -3 tools\ui_debt_audit.py --check`
  - `py -3 tools\modernization_audit.py --check`
  - `py -3 tools\source_size_audit.py --check`
  - `py -3 tools\make_guid.py --dry-run`
  - `py -3 tools\check_flag_tables.py`

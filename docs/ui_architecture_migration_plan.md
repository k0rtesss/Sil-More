# UI Architecture Migration Plan

## Purpose
This plan turns the recommended UI direction into an execution plan for the
current tree.

Recommended direction:
- build a frontend-neutral session API
- expose declarative snapshots plus event streams
- keep frame cadence in the frontend
- treat the existing `Term` path as a legacy frontend, not as the future UI API

Status date: March 27, 2026.

## Current Tree Baseline
- The SDL frontend is no longer one monolithic file.
  - `src/main-sdl.c`: bootstrap, config load/apply, keyboard/gamepad
    translation, top-level event routing, and most `platform-ui.h` shims
  - `src/sdl-layout.c`: pane config state, terminal sizing, pane placement,
    `resize()`, and view creation
  - `src/sdl-render.c`: `z-term` hook backend, canvas presentation, renderer
    reset, and `TERM_XTRA_*` handling
  - `src/sdl-story-font.c`: story font cache/load/state/measurement
  - `src/sdl-touch.c`: touch pane state, rendering, bindings, and reset flow
- The old `util.c` split is also already real.
  - `src/util-input.c`: `inkey()` and `request_command()`
  - `src/util-prompt.c`: `get_check()`, `get_com()`, `pause_line()`, and
    related prompt helpers
  - `src/util-message.c`: message history, `screen_save()`, `screen_load()`
  - `src/util-text.c`: `put_str()` / `prt()` / text wrapping
- The build graph is still only partially semantic.
  - `sil-core` still links SDL directly
  - `sil-core` still includes `SIL_MORE_SOURCES_FRONTEND`
  - `platform-ui.h` is still SDL-shaped and widely imported
- The active render/input boundary is still terminal-first.
  - `src/sdl-render.c` still services `TERM_XTRA_EVENT`, `TERM_XTRA_FRESH`,
    `TERM_XTRA_CLEAR`, and `TERM_XTRA_DELAY`
  - `src/ui/ui-status.c` still owns `update_stuff()`, `redraw_stuff()`,
    `window_stuff()`, and `handle_stuff()`
  - `src/cave.c` still renders map cells with `Term_queue_char()`
  - `src/object/object-ui-select.c` still fuses gameplay item selection and
    terminal UI in `get_item()`
- Current hotspot counts from the UI0 audit baseline:
  - `inkey()` call sites in 41 files / 159 matches
  - `screen_save()` + `screen_load()` call sites in 35 files / 253 matches
  - direct `Term_*` render/control calls in 66 files / 1,795 matches
  - `#include "platform-ui.h"` in 28 files / 28 matches
  - `get_sdl_*` / `set_sdl_*` usage outside platform code in 6 files / 196 matches

## Non-Goals
- Do not migrate to a game engine.
- Do not rewrite core mechanics for style reasons.
- Do not mix UI-architecture work with gameplay balance changes.
- Do not change save, score, or metarun formats unless a stage explicitly
  requires it and includes migration/version gates.
- Do not expose SDL types or SDL-named APIs in the new public UI boundary.

## Target Architecture
- `sil-core` owns:
  - gameplay state
  - persistence
  - content/data parsing
  - scoring/metarun state
  - authoritative interaction state for gameplay-coupled prompts/selectors
  - snapshot construction
  - event emission
- Frontends own:
  - render loop and frame cadence
  - scene stack
  - animations and transitions
  - physical input binding
  - layout policy
  - toolkit-specific widgets
- The public boundary should be plain C and trivially serializable.
  - suggested neutral module namespace: `src/app/`
  - suggested first headers:
    - `app-session.h`
    - `app-input.h`
    - `app-snapshot.h`
    - `app-events.h`
    - `app-host.h`
- The new boundary should support two input layers:
  - a legacy low-level key/event feed for compatibility
  - a higher-level intent feed for new SDL/web frontends
- The new boundary should support two output layers:
  - full snapshot data for rendering
  - compact event streams for animation and frontend bookkeeping

Example shape:

```c
app_submit_input(session, &input);
app_advance_until_waiting(session);
const app_snapshot *snap = app_get_snapshot(session);
const app_event_span *events = app_drain_events(session);
```

Key rule:
- render cadence and simulation cadence must be separate
- the core can stay turn-based while the frontend renders at a regular frame
  rate from snapshots plus in-flight event animations

## Ownership Rules During Migration
- Treat all existing `Term_*`, `inkey()`, `screen_save()`, and
  `screen_load()` usage as legacy APIs.
- Do not add new call sites for those APIs outside the legacy compatibility
  path.
- Do not add new `get_sdl_*`, `set_sdl_*`, or other SDL-shaped names to
  core-facing headers.
- Prefer moving data/query logic out of legacy UI modules before moving their
  visuals.
- Keep `z-term.c` stable until the session/snapshot/event scaffolding is in
  place.

## Stage Overview
| Stage | Goal | Main outputs |
| --- | --- | --- |
| UI0 | freeze debt growth and establish metrics | rules, audit script, migration inventory |
| UI1 | add neutral boundary scaffolding | `app-*` headers, event buffer, host interface draft |
| UI2 | make the core externally drivable | session driver, wait reasons, input queue bridge |
| UI3 | build first-class dungeon snapshots/events | map/status/message/pane snapshots |
| UI4 | build new SDL scene stack | snapshot-driven dungeon renderer and frame loop |
| UI5 | extract gameplay-coupled interaction state | prompts, item selection, targeting, look |
| UI6 | move informational screens to frontend scenes | help/settings/score/story/etc. scenes |
| UI7 | make the split semantically true | SDL-free `sil-core`, isolated legacy frontend |
| UI8 | formalize WASM/web delivery | serializable ABI/protocol and host bridge |

## Stage UI0: Guardrails And Baseline
Goal:
- prevent the architecture debt from growing while later stages land

Working-tree outputs:
- ADR note: [`ui_architecture_target_adr.md`](./ui_architecture_target_adr.md)
- migration inventory: [`ui_migration_inventory.md`](./ui_migration_inventory.md)
- audit tool: [`../tools/ui_debt_audit.py`](../tools/ui_debt_audit.py)
- audit baseline: [`../tests/ui_debt_audit_baseline.json`](../tests/ui_debt_audit_baseline.json)
- test hook: `ctest -R sil_ui0_audit --output-on-failure`

Work packages:
- Write an ADR-level note describing the target session/snapshot/event model.
- Add a simple audit script or test command that reports counts for:
  - `inkey()`
  - `screen_save()` / `screen_load()`
  - direct `Term_*` rendering calls
  - `platform-ui.h` include count
  - `get_sdl_*` / `set_sdl_*` usage outside platform code
- Add a migration inventory that tags modules as:
  - early boundary
  - early snapshot
  - early frontend-only
  - mid-stage interaction extraction
  - late bespoke flow
- Add a standing rule to code review: new UI work must use the new boundary or
  live in clearly-marked legacy files.

Parallel lanes:
- Lane A: docs and ADRs.
  - Write set: `docs/*`
- Lane B: audit tooling.
  - Write set: `tools/*`, `tests/*`, optional CI hooks
- Lane C: neutral boundary skeleton headers.
  - Write set: new `src/app/*` headers only

Validation:
- standard and portable builds still pass
- audit output is reproducible from the repo root

Exit when:
- the target boundary is documented
- legacy debt growth is measurable

## Stage UI1: Neutral Boundary Scaffolding
Goal:
- add the new API surface without changing active gameplay behavior

Work packages:
- Introduce neutral public structs/enums for:
  - session lifecycle
  - input events
  - high-level intents
  - wait reasons
  - snapshot blobs
  - event records
- Add a small, core-owned event buffer with explicit ownership rules.
- Define the host-facing surface for:
  - timing
  - persistence hooks
  - capability queries
  - optional logging/resource helpers
- Keep SDL, `Term`, and platform config out of these new headers.

Parallel lanes:
- Lane A: `app-session.h`, `app-input.h`, `app-events.h`.
- Lane B: event buffer implementation and tests.
- Lane C: `app-host.h` and neutral config/capability enums.

Validation:
- new headers compile cleanly in `sil-core`
- no SDL includes leak into the new boundary
- event structs can be serialized without toolkit-specific glue

Exit when:
- the new boundary exists and is stable enough for later stages to target

## Stage UI2: Session Driver And Wait Reasons
Goal:
- make the core externally drivable instead of platform-blocking

Work packages:
- Introduce a session driver that can run until one of:
  - input required
  - scene transition required
  - shutdown requested
- Add explicit wait reasons for:
  - command input
  - confirm/cancel prompt
  - list selection
  - targeting
  - informational pause
- Add a session-owned input queue.
- Bridge the legacy SDL path into that queue first.
- Do not try to remove all `Term_inkey()` usage here.
  - keep the legacy `Term` event path alive until the new SDL scene stack is
    usable
  - use queue injection and compatibility shims first

Parallel lanes:
- Lane A: session driver and wait-state ownership.
  - Write set: new `src/app/*`, `src/runtime/*`, `src/main.c`, outer loop code
- Lane B: legacy SDL queue bridge.
  - Write set: `src/main-sdl.c`, `src/sdl-touch.c`, gamepad/touch translation
- Lane C: keymap/macro regression coverage.
  - Write set: `tests/*`

Validation:
- keyboard play still works through the legacy SDL frontend
- keymaps/macros/repeat counts still behave correctly
- save/load and game exit still work

Exit when:
- the core can be advanced by an external caller until a well-defined wait
  state

## Stage UI3: Dungeon Snapshot And Event Model
Goal:
- expose enough data to render the main game scene without `Term`

Work packages:
- Add a first snapshot for the dungeon scene:
  - visible map cells
  - entities
  - player location and cursor state
  - target/highlight state
  - status values and bars
  - message log and top-line message state
  - side-pane data now sourced through `ui-status.c`
  - combat-roll and auxiliary pane data
- Add event emission for the first visual actions:
  - message appended
  - actor moved
  - damage taken/dealt
  - projectile launched
  - object picked up/dropped
  - pane invalidated
  - prompt opened/closed
- Reuse current `PR_*` / `PW_*` style dirtiness internally if useful, but make
  them snapshot invalidation signals rather than renderer commands.

Parallel lanes:
- Lane A: map/entity snapshot.
  - Write set: `src/cave.c`, new snapshot builders, map/query helpers
- Lane B: status/pane snapshot.
  - Write set: `src/ui/ui-status.c`, combat-roll/pane data builders
- Lane C: message/event stream.
  - Write set: `src/util-message.c`, event buffer plumbing

Validation:
- a debug dump of the snapshot can reconstruct the visible dungeon state
- no gameplay behavior changes in the legacy renderer

Exit when:
- the core can describe the live dungeon state without requiring a terminal

## Stage UI4: Snapshot-Driven SDL Scene Stack
Goal:
- let SDL render the dungeon every frame from snapshots instead of `Term`

Work packages:
- Add a frontend scene stack for:
  - main dungeon scene
  - overlay scene
  - modal scene
- Build a new snapshot-driven dungeon renderer in SDL.
- Keep the legacy `Term` path available behind a runtime or build switch during
  transition.
- Drive animations from `app_event_span` rather than gameplay delays.
- Reuse the already-split SDL modules as parallel ownership slices:
  - `src/sdl-layout.c`
  - `src/sdl-render.c`
  - `src/sdl-touch.c`
  - `src/sdl-story-font.c`
  - `src/main-sdl.c`

Parallel lanes:
- Lane A: scene stack and snapshot renderer.
  - Write set: new `src/sdl-scene-*` files, `src/sdl-render.c`
- Lane B: input bridge and action dispatch.
  - Write set: `src/main-sdl.c`, `src/sdl-touch.c`
- Lane C: layout/pane integration.
  - Write set: `src/sdl-layout.c`, pane widget files

Validation:
- the main dungeon scene is playable in SDL from snapshots
- resize, renderer reset, touch pane, and gamepad continue to work
- the frontend can animate between turns without changing turn resolution

Exit when:
- the SDL dungeon scene no longer depends on `Term` for normal frame rendering

## Stage UI5: Gameplay-Coupled Interaction Extraction
Goal:
- replace blocking prompt/selector loops with explicit interaction state

Order inside the stage:
- UI5A: prompt primitives
- UI5B: item selection
- UI5C: targeting and look mode
- UI5D: smithing and other multi-step gameplay selectors
- UI5D should trail the generic prompt/list/target work and does not need to
  block the frontend-only scene work in UI6.

Work packages:
- Convert `src/util-prompt.c` into interaction descriptors instead of direct
  blocking UI.
- Replace `get_item()` in `src/object/object-ui-select.c` with a core-owned
  selection state plus frontend-rendered lists.
- Replace targeting and look-mode loops in:
  - `src/targeting.c`
  - `src/cmd/ui/cmd-ui-look.c`
- Move confirmation/input-line/list/point-target primitives onto the session
  boundary so gameplay code requests interactions instead of rendering them.

Parallel lanes:
- Lane A: prompt primitives.
  - Write set: `src/util-prompt.c`, `src/util-input.c`, new interaction types
- Lane B: item selection.
  - Write set: `src/object/object-ui-select.c`, `src/cmd/item/*`
- Lane C: targeting and look.
  - Write set: `src/targeting.c`, `src/cmd/ui/cmd-ui-look.c`

Validation:
- inventory/equipment/floor selection works under the new SDL scene stack
- targeting/look no longer requires `screen_save()` / `screen_load()` in the
  gameplay path

Exit when:
- gameplay-coupled interactions are represented as session state, not terminal
  control flow

## Stage UI6: Frontend-Owned Informational Scenes
Goal:
- move non-authoritative browsers and presentation screens out of core rendering

Recommended first batch:
- `src/cmd/ui/cmd-ui-settings.c`
- `src/ui/ui-help.c`
- `src/ui/ui-file-viewer.c`
- `src/score/score_ui.c`
- `src/quest/quest-ui.c`

Recommended later batch:
- `src/ui/ui-story.c`
- `src/ui/ui-death.c`
- character sheet and knowledge browsers
- metarun presentation screens

Work packages:
- expose query APIs for these screens
- rebuild them as frontend scenes/widgets
- keep core ownership only for the underlying data and actions
- UI6 can begin in parallel with late UI5 work once UI4 has produced a stable
  scene stack and the needed query APIs are defined.

Parallel lanes:
- Lane A: settings/help/file viewer.
- Lane B: score and quest browsers.
- Lane C: data/query APIs for story/death/character/knowledge scenes.

Validation:
- these scenes work without `screen_save()` / `screen_load()` in their normal
  execution path
- frontend scene navigation is stable

Exit when:
- informational UI is frontend-owned and no longer blocks later core cleanup

## Stage UI7: Legacy Isolation And True Platform Boundary
Goal:
- make the core/frontend split semantically true

Work packages:
- Remove frontend sources from `sil-core`.
- Stop linking SDL directly into `sil-core`.
- Replace `platform-ui.h` with neutral app/platform headers.
- Move legacy terminal rendering into its own compatibility target.
- Pull `z-term.c` behind that compatibility layer as the last large legacy
  dependency.
- Use `pane-config.h` and `gamepad-config.h` as seeds for neutral boundary
  types rather than extending SDL-shaped headers.

Parallel lanes:
- Lane A: build graph and header cleanup.
  - Write set: `CMakeLists.txt`, public headers
- Lane B: legacy frontend isolation.
  - Write set: `src/z-term.c`, legacy frontend target files
- Lane C: neutral platform/app config API.
  - Write set: replacement for `platform-ui.h`, capability/config modules

Validation:
- `sil-core` builds without SDL headers or SDL link requirements
- SDL frontend still works through the neutral boundary
- legacy frontend remains optional, not mandatory

Exit when:
- the current target split is real in semantics, not only in source layout

## Stage UI8: WASM And Web Bridge
Goal:
- make the new boundary usable from a browser-hosted frontend

Work packages:
- formalize a stable C ABI or serialization layer for:
  - snapshots
  - event spans
  - session lifecycle
  - input submission
- provide a host bridge for:
  - persistence
  - timing
  - resource access
  - logging
- build a minimal web proof of concept for:
  - dungeon scene
  - message log
  - one gameplay interaction flow

Parallel lanes:
- Lane A: ABI/serialization.
- Lane B: host bridge/persistence/resource hooks.
- Lane C: web prototype scene rendering.

Validation:
- the core builds to WASM or a host-neutral static library
- a browser client can render snapshots and submit inputs

Exit when:
- the web frontend is a consumer of the same boundary as SDL, not a special
  fork

## Suggested Subagent Grouping For Implementation
- Group A: input and prompt boundary.
  - `src/util-input.c`
  - `src/util-prompt.c`
  - new interaction/session types
- Group B: snapshot and event model.
  - `src/cave.c`
  - `src/ui/ui-status.c`
  - `src/util-message.c`
- Group C: SDL frontend scene stack.
  - `src/main-sdl.c`
  - `src/sdl-layout.c`
  - `src/sdl-render.c`
  - `src/sdl-touch.c`
  - `src/sdl-story-font.c`
- Group D: gameplay-coupled selectors.
  - `src/object/object-ui-select.c`
  - `src/targeting.c`
  - `src/cmd/ui/cmd-ui-look.c`
- Group E: frontend-only browsers and presentation.
  - `src/cmd/ui/cmd-ui-settings.c`
  - `src/ui/ui-help.c`
  - `src/ui/ui-file-viewer.c`
  - `src/score/score_ui.c`
  - `src/quest/quest-ui.c`
  - later `src/metarun.c`, `src/ui/ui-story.c`, `src/ui/ui-death.c`
- Group F: late bespoke flows.
  - `src/ui/smithing/ui-smithing-screen.c`
  - `src/birth.c`
  - `src/metarun.c`

Parallelization rules:
- only parallelize lanes with disjoint write sets
- do not run overlapping write work on:
  - `src/util-input.c`
  - `src/ui/ui-status.c`
  - `src/cave.c`
  - `src/main-sdl.c`
  - `src/sdl-render.c`
  - `src/platform-ui.h` or its replacement
  - `src/z-term.c`

## Decision Gates
- Gate 1 after UI2:
  - if the session/wait model is not yet robust, do not start selector
    extraction
- Gate 2 after UI3:
  - if the snapshot cannot reconstruct the dungeon scene accurately enough, fix
    the model before building more SDL scenes
- Gate 3 after UI5:
  - if generic interaction types cannot express smithing/birth/metarun flows,
    add workflow containers rather than reintroducing blocking loops
- Gate 4 after UI7:
  - only start the serious WASM/web bridge once the boundary is SDL-free

## Recommended Validation Suite Per Stage
- Build:
  - `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1`
  - `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1 -Target portable`
- Tests:
  - `ctest --output-on-failure`
- Smoke coverage:
  - keyboard input
  - gamepad input
  - touch pane interaction
  - resize and renderer reset
  - save/load
  - map redraw
  - inventory/equipment panes
  - item selection
  - targeting/look
  - score/help/story screens
  - death and metarun flows

## Recommended Near-Term Milestones
- Milestone M1:
  - UI0 through UI2 complete
  - core is externally drivable and wait-state aware
- Milestone M2:
  - UI3 complete
  - snapshots and events can describe the live dungeon scene
- Milestone M3:
  - UI4 complete
  - SDL dungeon rendering is snapshot-driven and frame-based
- Milestone M4:
  - UI5 complete
  - gameplay-coupled selectors no longer depend on blocking terminal UI
- Milestone M5:
  - UI6 and UI7 complete
  - `sil-core` is genuinely frontend-neutral
- Milestone M6:
  - UI8 prototype complete
  - the same boundary works for SDL and a web client

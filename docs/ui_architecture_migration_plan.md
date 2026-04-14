# Deprecated: UI Architecture Migration Plan

Deprecated on April 3, 2026.

This document is kept only as the historical record of the UI0-UI8 substrate
work and the first overlay-track plan. The active execution document is
[`ui_render_replacement_plan.md`](./ui_render_replacement_plan.md).
Do not treat the staged status labels, lane assignments, or rollout wording
below as current execution guidance.

Why it was deprecated:
- UI0-UI4, the `src/app/*` boundary, the build split, and the first SDL scene
  stack are already landed substrate
- the remaining work is no longer a stage ladder; it is runtime replacement of
  legacy `Term` rendering and the terminal layout model
- several older "closed" or "complete" labels became misleading because major
  overlay families still depend on term mirroring, cell-grid overlays,
  `screen_save()` / `screen_load()`, `inkey()`, and direct `Term_*` rendering

Historical contents of the superseded plan follow below.

## Purpose
This plan turns the recommended UI direction into an execution plan for the
current tree.

Recommended direction:
- build a frontend-neutral session API
- expose declarative snapshots plus event streams
- keep frame cadence in the frontend
- treat the existing `Term` path as a legacy frontend, not as the future UI API

Status date: April 2, 2026.

## Current Tree Baseline
- The SDL frontend is no longer one monolithic file.
  - `src/main-sdl.c`: bootstrap, config load/apply, keyboard/gamepad
    translation, top-level event routing, legacy input bridging, and the
    remaining SDL platform shims
  - `src/sdl-layout.c`: pane config state, terminal sizing, pane placement,
    `resize()`, and view creation
  - `src/sdl-render.c`: `z-term` hook backend, canvas presentation, renderer
    reset, and the remaining `TERM_XTRA_*` bridge
  - `src/sdl-story-font.c`: story font cache/load/state/measurement
  - `src/sdl-touch.c`: touch pane state, rendering, bindings, and reset flow
- The old `util.c` split is also already real.
  - `src/util-input.c`: `inkey()` and `request_command()`
  - `src/util-prompt.c`: `get_check()`, `get_com()`, `pause_line()`, and
    related prompt helpers
  - `src/util-message.c`: message history, `screen_save()`, `screen_load()`
  - `src/util-text.c`: `put_str()` / `prt()` / text wrapping
- The build graph split is now real.
  - `CMakeLists.txt` now builds `sil-core` as an SDL-free static library
  - `sil-legacy-compat` isolates `z-term.c` and the remaining legacy frontend
    bridge modules
  - `sil-platform-sdl` owns SDL-facing infrastructure
  - `platform-ui.h` has been retired in favor of narrower boundary headers, so
    the include-count audit for it is now zero
- The active render/input boundary is now dual-path: snapshot-capable, but not
  yet fully externally driven.
  - `src/sdl-render.c` still services `TERM_XTRA_EVENT`, `TERM_XTRA_FRESH`,
    `TERM_XTRA_CLEAR`, and `TERM_XTRA_DELAY` for legacy modules
  - `src/sdl-scene.c` and `src/sdl-scene-dungeon.c` render the main scene from
    `app_session` snapshots and drained event spans
  - `src/sdl-scene-menu.c` renders menus from semantic payloads in logical
    pixels
  - `src/sdl-scene-information.c` bridges legacy `Term` content for
    informational screens
  - `src/ui/ui-status.c` still owns `update_stuff()`, `redraw_stuff()`,
    `window_stuff()`, and `handle_stuff()`, and now rebuilds the dungeon
    snapshot after `handle_stuff()`
  - `src/cave.c` still renders map cells with `Term_queue_char()` for the
    legacy path
  - `src/object/object-ui-select.c`, `src/cmd/ui/cmd-ui-look.c`, and several
    other gameplay selectors still fuse blocking terminal flow with snapshot
    state
- Current hotspot counts from `py -3 tools/ui_debt_audit.py` on 2026-04-02:
  - `inkey()` call sites in 39 files / 89 matches
  - `screen_save()` + `screen_load()` call sites in 33 files / 215 matches
  - direct `Term_*` render/control calls in 65 files / 1,697 matches
  - `#include "platform-ui.h"` in 0 files / 0 matches
  - `get_sdl_*` / `set_sdl_*` usage outside platform code in 6 files / 208 matches

## April 2 Direction Reset
- Treat UI0 through UI8 as the landed architecture substrate, not as the
  pacing item for the remaining UI work.
- The active goal is now menu/browser migration plus runtime legacy-render
  removal.
- Recreate existing menus visually through the new multilayer renderers.
  Preserve the current Sil look, wording, spacing, and hierarchy, but do not
  preserve legacy term rendering as a permanent normal-path fallback.
- On the scene-backed SDL path, a scene-backed screen must not silently drop
  back to legacy term rendering as an emergency fallback. If that path fails,
  treat it as a bug and report it explicitly.
- `ui_information_scene` and mirrored `Term` capture remain temporary bridge
  tools only. They are allowed while a family is being ported, but they are
  not the final menu/document API.
- Keeping legacy draw code around as a visual reference or content-capture
  helper is acceptable during migration; keeping it as a second runtime render
  backend is not the target state.

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
| Stage | Goal | Main outputs | Status |
| --- | --- | --- | --- |
| UI0 | freeze debt growth and establish metrics | rules, audit script, migration inventory | complete |
| UI1 | add neutral boundary scaffolding | `app-*` headers, event buffer, host interface draft | complete |
| UI2 | make the core externally drivable | session driver, wait reasons, input queue bridge | complete |
| UI3 | build first-class dungeon snapshots/events | map/status/message/pane snapshots | complete |
| UI4 | build new SDL scene stack | snapshot-driven dungeon renderer and frame loop | complete |
| UI5 | extract gameplay-coupled interaction state | prompts, item selection, targeting, look | closed; carry-over in `OVER2`-`OVER5` |
| UI6 | move informational screens to frontend scenes | help/settings/score/story/etc. scenes | closed; carry-over in `OVER3`-`OVER5` |
| UI7 | make the split semantically true | SDL-free `sil-core`, isolated legacy frontend | closed; carry-over in the overlay track |
| UI8 | formalize WASM/web delivery | serializable ABI/protocol and host bridge | prototype complete |

## Status Audit On 2026-04-02
| Stage | Status | Notes |
| --- | --- | --- |
| UI0 | complete | ADR, audit tool, migration inventory, and a refreshed 2026-04-02 baseline are landed; the audit is again measuring regressions against the current tree. |
| UI1 | complete | `src/app/app-*.h`, the event buffer, host surface, and `tests/ui1_tests.c` cover the neutral boundary scaffolding. |
| UI2 | complete for the active runtime | wait reasons, input queues, the SDL legacy-input bridge, and the stepper surface are landed; the remaining blocking loops are consumer-migration work now tracked under `OVER2`-`OVER5` rather than missing driver scaffolding. |
| UI3 | complete for the current renderer path | `app-scene-dungeon`, snapshot invalidation, message/event hooks, and `ui-status.c` snapshot rebuilds provide the data the SDL scene stack consumes. |
| UI4 | complete for the current renderer path | `sdl-scene.c`, `sdl-scene-dungeon.c`, `sdl-scene-bootstrap.c`, and `sdl-scene-information.c` now render from snapshots and drained event spans. |
| UI5 | closed as a boundary stage | interaction kinds, wait scopes, and snapshot overlays are in place; the remaining look/target/item cleanup is renderer migration work in `OVER2`-`OVER5`, not missing session primitives. |
| UI6 | closed as a scene-substrate stage | the scene plumbing is landed, and scene-backed help, quest, run-history, message-recall, and hint-message entry flows no longer emergency-fallback to legacy render on the scene-backed SDL path; remaining hybrids are tracked in `OVER3`-`OVER5`. |
| UI7 | closed for planning purposes | the build split is live; the remaining semantic cleanup is now best understood as legacy-render removal inside migrated consumers, so it is carried by the overlay track rather than by another UI-stage. |
| UI8 | prototype complete | `app-wire`, `app-host-bridge`, `tests/ui8_tests.c`, and `web/ui8-demo/` exercise the packet ABI; further web hardening should wait on overlay-track reduction of runtime legacy-render debt. |

## Quality Assessment On 2026-04-02
- Strong:
  - the session/snapshot/event substrate is real, test-backed, and already
    good enough to carry SDL plus the UI8 web prototype
  - the fixed-pixel menu stack (`app_menu_scene`, `sdl-scene-menu`,
    `sdl-ui-style`) is a credible long-term renderer for visual parity work
- Medium:
  - `ui_information_scene` is doing useful bridge work, but it has also
    blurred the definition of "done" because mirrored term output can look
    shipped while ownership is still legacy
  - several documents and browsers now have scene-backed entry flows, but
    their inner panels still depend on `present_term()`, `screen_save()`, or
    term-layout code
- Weak:
  - earlier plan revisions overstated UI5-UI7 as "complete" without clearly
    separating architecture substrate from runtime renderer removal
  - package status had drifted; `MENU4` and `MENU6` in particular were lagging
    behind what the code actually does
- Immediate quality bar:
  - once a scene-backed SDL path exists, keep the old visuals only as a
    reference or migration helper; do not keep a second runtime renderer as an
    emergency escape hatch

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

Status:
- complete in the working tree on 2026-03-28
- ADR note (`docs/ui_architecture_target_adr.md`), migration inventory
  (`docs/ui_migration_inventory.md`), audit tool (`tools/ui_debt_audit.py`),
  and baseline (`tests/ui_debt_audit_baseline.json`) are all in place
- `ctest -R sil_ui0_audit --output-on-failure` is registered in CMakeLists.txt
- standing rules are documented in the migration plan and AGENTS.md

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

Status:
- complete in the working tree on 2026-03-28
- `src/app/` contains 12 header/source pairs: `app-session`, `app-input`,
  `app-snapshot`, `app-events`, `app-host`, `app-host-bridge`, `app-wire`,
  `app-interaction`, `app-scene-bootstrap`, `app-scene-dungeon`,
  `app-scene-information`, `app-scene-menu`
- event buffer (`app-events.c`) is core-owned with explicit capacity and drain
- host surface (`app-host.h`, `app-host.c`) covers timing, persistence, and
  resource hooks without SDL types
- no SDL includes in any `src/app/` header

## Stage UI2: Session Driver And Wait Reasons
Goal:
- make the core externally drivable instead of platform-blocking

Work packages:
- Introduce a session driver that can run until one of:
  - input required
  - scene transition required
  - shutdown requested
- Stabilize the public driver-facing surface with canonical wrapper entry
  points:
  - `app_submit_input()`
  - `app_submit_intent()`
  - `app_advance_until_waiting()`
  - `app_get_snapshot()`
  - `app_drain_events()`
- Add an explicit advance callback seam on `app_session` so later runtime
  extraction can plug in a real core stepper without exposing SDL or `main.c`
  details through the public boundary.
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

Status:
- complete in the working tree on 2026-03-28
- `app_session` manages state transitions (UNINITIALIZED -> IDLE -> RUNNING ->
  WAITING -> STOPPING -> STOPPED), wait reasons, and session flags
- `app_wait_reason` enum covers BOOTSTRAP, COMMAND_INPUT, CONFIRM,
  LIST_SELECTION, TARGETING, TEXT_ENTRY, INFORMATION, and SHUTDOWN
- session-owned input queue and intent queue are embedded in `app-session.c`
- legacy SDL path bridges through `APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT`
- `src/runtime/runtime-game.c` hosts the game control loop

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

Status:
- complete in the working tree on 2026-03-28
- `app-scene-dungeon.c` implements `app_build_dungeon_snapshot()` which
  atomically builds map, status, messages, panes, and interaction blobs
- snapshot structs cover `app_map_cell_snapshot` (with visibility/entity flags),
  `app_status_snapshot` (18 text panes), `app_messages_snapshot` (256-line
  history), `app_combat_roll_snapshot`, and `app_panes_snapshot` (left panel
  cells and combat entries)
- event emission is active: `APP_EVENT_KIND_MESSAGE`,
  `APP_EVENT_KIND_ACTOR_MOVED`, `APP_EVENT_KIND_DAMAGE`,
  `APP_EVENT_KIND_PROJECTILE`, `APP_EVENT_KIND_OBJECT_TRANSFER`,
  `APP_EVENT_KIND_ANIMATION_HINT`, plus session/snapshot lifecycle events
- `sdl-scene.c` consumes drained events to drive SDL-side animations
  (actor move, damage, projectile, object transfer)

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

Status:
- complete in the working tree on 2026-03-28
- five SDL scene files: `sdl-scene.c` (coordinator), `sdl-scene-dungeon.c`,
  `sdl-scene-information.c`, `sdl-scene-menu.c`, `sdl-scene-bootstrap.c`
- `sdl-scene-dungeon.c` renders entirely from `app_dungeon_snapshot` blobs;
  no `Term_queue_char` or `Term_putstr` calls in the dungeon scene renderer
- scene kinds: BOOTSTRAP, DUNGEON, OVERLAY, MENU, INFORMATION
- the legacy `Term` path remains available as a parallel rendering backend
  via `sdl-render.c` for modules not yet migrated
- animations (actor move, damage, projectile, object transfer) are driven
  from `app_event_span` records, not gameplay delays

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

Status:
- closed as an architecture stage on 2026-04-02; the boundary work is landed
  and the remaining user-visible cleanup is tracked under `OVER2`-`OVER5`
- all gameplay-coupled interactions publish wait-reason scopes and interaction
  state descriptors; blocking `inkey()` calls remain in the legacy path but
  are wrapped with session wait-state context
- UI5A (prompt primitives): complete
  - `app_interaction_state` struct with kinds PROMPT, TEXT_ENTRY, LIST,
    TARGETING, LOOK and flags CAN_CONFIRM, CAN_CANCEL, SHOW_OPTIONS, etc.
  - `util-prompt.c` publishes interaction state and wraps its single `inkey()`
    call with `prompt_inkey_with_wait_reason()`
  - `util-message.c` publishes interaction state for `-more-` prompts and
    wraps `inkey()` with wait scope
- UI5B (item selection): complete
  - `object-ui-select.c` publishes interaction state via
    `item_selector_sync_snapshot()`, tracks highlight selection, and manages
    wait scope for the entire `get_item()` lifetime
- UI5C (targeting and look): partial
  - `targeting.c` publishes interaction state via `targeting_snapshot_prompt()`
    at all 6 `inkey()` sites; all calls replaced with
    `targeting_inkey_with_wait_reason()` wrapper; interaction state cleared on
    exit via `app_session_clear_interaction()`
  - `cmd-ui-look.c` now publishes `APP_INTERACTION_KIND_LOOK` prompt state on
    the snapshot path, wraps its main wait with a targeting-scoped helper, and
    clears interaction state before nested examine flows
  - `cmd-ui-look.c` object and monster examination now mirror through
    `ui_information_scene`, and `obj-info.c` uses the same bridge for
    item-information pauses, but the outer look and targeting loops still own
    redraw/path state through legacy term control flow
- UI5D (smithing and bespoke selectors): complete
  - `ui-smithing-screen.c`: all 13 `inkey()` calls replaced with
    `smith_ui_inkey_with_wait_reason()` wrapper; outer `do_cmd_smithing_screen()`
    manages wait scope and clears interaction state on exit
  - `birth.c`: all 9 `inkey()` calls replaced with
    `birth_inkey_with_wait_reason()` wrapper with appropriate wait reasons
    (LIST_SELECTION, CONFIRM, INFORMATIONAL_PAUSE)
  - `dungeon.c`: Morgoth's hall confirmation wrapped with CONFIRM wait scope;
    story intro and blitz unlock prompts wrapped with INFORMATIONAL_PAUSE
  - `squelch.c`, `files.c`, `wizard1.c`: remaining gameplay-coupled `inkey()`
    calls wrapped with appropriate wait-reason scopes
  - `obj-info.c`, `cave.c`, `blitz.c`: informational pause `inkey()` calls
    wrapped with INFORMATIONAL_PAUSE wait scopes
- the SDL dungeon scene renders interaction overlays from snapshot data, so
  the visual path is already decoupled; the blocking path remains but is fully
  annotated with session wait-state context
- UI debt audit `inkey()` count now stands at 89 matches on 2026-04-02

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

Status:
- closed as a scene-substrate stage on 2026-04-02; the remaining work is
  renderer replacement and bridge removal in `OVER3`-`OVER5` rather than
  missing scene plumbing
- substrate: `ui-information-scene.c` provides `enter`/`leave`/`present`/
  `present_term`/`wait_key`/`capture_term` API; SDL renders through
  `sdl-scene-information.c`
- fully migrated: `ui-file-viewer.c` (all rendering and input through
  information scene; no `screen_save`/`screen_load()`/`inkey()` calls remain)
- scene-backed and authoritative on the snapshot path: `ui-help.c`,
  `quest-ui.c:do_cmd_quest_status()`, `score_ui.c:show_scores()`,
  `score_ui.c:do_cmd_run_history()`, `cmd-ui-main-menu.c:do_cmd_messages()`,
  and the hint-message browser now report failure instead of silently
  dropping to legacy rendering when the scene path is active
- hybrid or bridge-backed: `score_ui.c` detail viewers,
  `quest_typewriter_menu()`, `ui-story.c`, `ui-death.c`,
  `cmd-ui-knowledge.c`, and `cmd-ui-settings.c` still depend on mirrored
  `Term` content, ternary input routing, or other legacy layout/input control
- scene-aware via parent scope: `cmd-ui-settings.c` (`do_cmd_options()` opens
  information scene scope; all sub-functions use `settings_wait_key()` for
  main input routing and `settings_present()` for scene-aware display; raw
  `inkey()` calls remain only in key-capture and macro trigger flows which
  require special `inkey` modes), `cmd-ui-main-menu.c` (uses information
  scene scoping with ternary input routing for modal transitions)
- UI debt audit now reports 89 `inkey()` matches and 215
  `screen_save()`/`screen_load()` matches on 2026-04-02

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

Status:
- build split landed in the working tree on 2026-03-28
- `sil-core` now builds without SDL headers or SDL link requirements
- legacy frontend ownership is isolated behind `sil-legacy-compat` and
  `sil-platform-sdl`, and `platform-ui.h` has been replaced by the narrower
  neutral boundary headers
- planning for this stage is closed on 2026-04-02; the remaining semantic gap
  is no longer missing build-boundary work, it is runtime legacy-render
  removal inside migrated consumers and menu families

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

Status:
- packet ABI and browser prototype landed in the working tree on 2026-03-28
- `src/app/app-wire.*` now formalizes the versioned packet ABI for snapshots,
  event spans, session state, legacy input, and intents
- `src/app/app-host-bridge.*` now provides a neutral host bridge for timing,
  persistence, resource lookup, and logging
- `web/ui8-demo/` now renders packetized dungeon, message, and interaction
  data and encodes browser key input back into the UI8 legacy-input packet
  format
- validation is currently through the host-neutral `sil-core` static library
  plus the packet-driven browser demo described in
  [`ui8_web_demo.md`](./ui8_web_demo.md)
- treat the stage as prototype-complete on 2026-04-02; further web-facing
  follow-through should wait on overlay-track runtime legacy-render removal so
  the browser path does not inherit transitional UI debt

## Overlay UI Modernization Follow-On: Semantic Overlay System
Goal:
- keep the dungeon scene on an integer-scaled world canvas
- move inventory, equipment, look menus, the left bar, the message bar, the
  bottom bar, and the remaining browsers onto one semantic overlay system
- let overlay scale be independent from dungeon tile scale; overlay surfaces
  may use fractional scaling, and minimap-style panels do not need integer
  scale
- keep the current Sil visual language intact while deleting the normal-path
  legacy terminal renderer
- stop carrying terminal rows and columns as a hidden second layout system

Overlay-specific non-goals:
- do not redesign fonts, colors, borders, copy, highlight treatment, or screen
  composition for style reasons
- do not replace the current UI look with a new "modern UI" skin
- do not add more term-mirror or `ui_information_scene` dependencies for
  persistent overlays
- do not let overlay sizing depend on `main_view_scale`, `Term->wid`,
  `Term->hgt`, or tile metrics once a family is migrated

Direction reset on 2026-04-02:
- treat the overlay problem as one architecture problem, not as a pile of
  unrelated SDL pixel ports
- inventory, equipment, look, the left rail, the message bar, and the bottom
  bar are all overlay surfaces and should share the same widget and composition
  model
- the SDL path must be semantic end-to-end: the core publishes content, state,
  focus, selection, scroll, and layout hints; the frontend owns pixel layout,
  anchoring, scaling, and rendering
- preserving the current visuals means reproducing them with semantic widgets,
  not keeping a hidden terminal renderer alive
- `ui_information_scene` and mirrored `Term` capture remain temporary bridge
  tools only for raw-cell parity views; they are not the target architecture
  for chrome, menus, selectors, or sidebars

Visual preservation contract:
- a migrated overlay should read as the same Sil UI the player already knows,
  with the same hierarchy, wording, framing, and relative spacing
- dungeon and world-space rendering change size only through integer scaling
- overlay rendering changes size only through explicit logical-pixel metrics,
  explicit overlay scale, and approved font buckets
- overlay layout must never be inferred from dungeon tile size, terminal cell
  size, or `Term->wid` / `Term->hgt` on the SDL path
- persistent overlay chrome may reserve safe area from the world viewport, but
  that reservation must be expressed in logical pixels owned by overlay layout,
  not in terminal columns or map cells
- text widgets should keep stable snapped metrics; non-text surfaces such as
  minimaps may scale fractionally when that improves fit and readability
- the acceptable visual delta for migration work is stability, anchoring, and
  scaling correctness; stylistic changes require separate approval
- the target end state is runtime legacy-render-free: preserve the visuals, not
  the old renderer implementation

Target overlay architecture:
- `world canvas`
  - dungeon map, actors, cursor, and world-space effects
  - integer-scaled only
- `overlay compositor`
  - SDL-owned, logical-pixel layout above the world canvas
  - three z-ordered families: persistent chrome, transient overlays, and modal
    overlays
  - responsible for anchoring left/top/bottom strips, centered modals, side
    panes, floating recall panels, and minimap surfaces
- `semantic widget model`
  - lives in `src/app/*`
  - shared by dungeon overlays and full menu scenes instead of each path
    inventing its own struct
  - stable widget ids, focus ids, selection, scroll state, visibility, actions,
    and layout hints
  - minimum common primitives: panel, strip, text block, stat row, message row,
    list, list row, detail pane, tab strip, footer action bar, prompt, text
    entry, compare pane, and minimap
- `SDL overlay renderer`
  - one measurement and styling source centered on `src/sdl-ui-style.c`
  - one renderer/compositor for chrome, selectors, list/detail browsers, and
    modal scenes
- `legacy bridge island`
  - temporary raw-cell compatibility only
  - bug-fix-only
  - no new persistent overlay family should enter this path

Recommended ownership split:
- core owns semantic state and content
  - widget trees or overlay snapshots
  - focus, selection, tabs, scroll position, actions, prompt mode, and list
    identity
  - stable identifiers so frontends can animate and restore focus cleanly
- frontend owns rendering policy
  - overlay scale, font bucket choice, padding, border drawing, anchoring,
    transitions, hit-testing, and safe-area behavior
  - viewport reservation for visible chrome

Overlay family inventory:
| Family | Representative files now | Current issue | Target semantic surface |
| --- | --- | --- | --- |
| Left rail | `src/ui/ui-status.c`, `src/app/app-scene-dungeon.*`, `src/sdl-scene-dungeon.c` | visually fixed-pixel now, but still partly driven by legacy row/cell assumptions and hidden-overlay globals | anchored status-rail widget with stat rows, icon-value rows, optional collapsed sections, and explicit logical-pixel width |
| Message bar and history | `src/util-message.c`, `src/cmd/ui/cmd-ui-main-menu.c`, `src/runtime/runtime-game.c` | top-line and recall flows still straddle message-row assumptions, bridge scenes, and prompt plumbing | top strip message widget plus expandable history/document widget using the same overlay compositor |
| Bottom bar and prompts | `src/util-prompt.c`, `src/runtime/runtime-game.c`, `src/sdl-scene-dungeon.c` | prompt/footer behavior exists, but is still too tied to interaction blobs and legacy flow ownership | footer action bar and prompt widget family with confirm, quantity, and text-entry variants |
| Inventory and equipment | `src/object/object-ui-select.c`, `src/object/object-ui-display.c`, `src/object/object-ui-enhanced.c`, `src/cmd/item/*` | list rendering and ownership loops still rely on term layout, blocking redraw, and menu-state globals | modal list/detail browser with compare panel, action popup, footer actions, and shared item-row widgets |
| Look, targeting, sidebar, minimap | `src/targeting.c`, `src/cmd/ui/cmd-ui-look.c`, `src/ui/ui-look-sidebar.c`, `src/cave.c` | look and targeting publish some semantic state, but outer loops and nested detail/minimap paths remain bridge-heavy | split-pane overlay with cursor/minimap pane, recall/detail pane, footer actions, and target-state widgets |
| Documents and browsers | `src/ui/ui-help.c`, `src/ui/ui-file-viewer.c`, `src/score/score_ui.c`, `src/quest/quest-ui.c`, `src/cmd/ui/cmd-ui-character.c`, `src/cmd/ui/cmd-ui-knowledge.c` | many screens now have scene-backed entry paths, but their bodies still lean on bridge or term-derived layout | document widget, list-detail widget, tabs, tables, and scroll containers rendered on the common overlay layer |

Current blocker summary:
- `src/sdl-scene-dungeon.c` already renders fixed-pixel overlays, but the left
  rail is still not a first-class semantic chrome surface end-to-end
- `src/util-message.c` and `src/util-prompt.c` still represent important UI
  through legacy row semantics or narrow interaction blobs rather than through
  a shared widget model
- `app_interaction_state` is useful for prompts and selectors, but it is too
  small to carry left-rail chrome, bottom-bar actions, split-pane browsers,
  compare panels, or minimap widgets
- `src/object/object-ui-select.c`, `src/cmd/ui/cmd-ui-look.c`, and much of the
  inventory/equipment family still keep normal-path blocking redraw and input
  ownership
- `src/sdl-scene-information.c` and `src/ui/ui-information-scene.c` still form
  a useful bridge for raw-cell parity, but keeping that bridge alive for more
  overlay families would continue the current ad hoc architecture
- many modules still branch on `Term->wid` / `Term->hgt` for compact layouts,
  wrapping, prompt rows, and side-panel width

Standing rules for this track:
- do not port overlays one by one from terminal coordinates into SDL pixel code
- first land or extend one shared semantic widget model for chrome and modal UI
- do not keep the dungeon overlay path and the menu-scene path as separate UI
  systems that merely happen to share fonts
- treat `APP_SCENE_KIND_INFORMATION` plus
  `ui_information_scene_present_term()` as a temporary bridge for raw-cell
  viewers, not as the final API
- once a family has a semantic SDL path, remove its normal-path legacy render
  fallback instead of keeping duplicate runtime render paths
- do not add new SDL-side sizing or reinterpretation heuristics to term-mirror
  paths; that route is compatibility-only now

Status on 2026-04-02:
- the foundation for overlay rendering is real
  - `src/app/app-scene-menu.[ch]` define semantic menu payloads
  - `src/sdl-scene-menu.c` renders them in logical pixels
  - `src/sdl-ui-style.c` centralizes menu typography, measurement, and scaling
  - `src/sdl-scene-dungeon.c` already renders prompt-style overlays and the
    visible left panel on a fixed-pixel path
- that foundation is not yet the finished overlay architecture
  - prompt and selector flows publish some semantic state, but the left rail,
    message bar, and bottom bar are not yet first-class widgets
  - inventory, equipment, and look still retain important normal-path legacy
    ownership
  - several browsers and viewers still route through the raw-cell bridge
- the plan correction is: stop expanding bridge-first menu ports and instead
  finish the overlay model so persistent chrome and modal browsers use the same
  semantic system

Active execution order on 2026-04-02:
- finish the shared overlay schema only far enough to support all overlay
  families, not just prompt modals
- migrate the left rail, message bar, and bottom bar next; they are foundation,
  not polish
- move inventory, equipment, look, and targeting onto the same widget set after
  the chrome primitives land
- keep bridge-backed screens alive only as temporary compatibility surfaces
  while the shared document and list-detail widgets are filled out
- leave story, death, metarun, birth, blitz, and smithing for after the common
  widget set is proven

Status update on 2026-03-29:
- `src/sdl-scene-dungeon.c` now renders the visible main-game left panel on a
  fixed-pixel path with its own cell metrics instead of inheriting dungeon tile
  scale
- the dungeon renderer now reserves map offset from the left panel's rendered
  width rather than from the old 13 tile-scaled columns, so raising
  `main_view_scale` no longer leaves an oversized gap
- `src/sdl-scene-menu.c` and the fixed-pixel left-panel path now preserve
  terminal-style horizontal mono-cell spacing for text, which is required for
  the classic menu and status-panel presentation to read correctly
- that work remains valid, but it is now understood as early evidence for the
  overlay-first plan above rather than as permission to keep left/top/bottom
  chrome and modal menus on separate long-term architectures

### Overlay Workstreams
The previous M0-M5 menu wording is superseded by the overlay-first execution
order below. The existing `MENU0A`-`MENU11` package names remain useful as
implementation shards, but they should now be scheduled under these overlay
workstreams.

Workstream `OVER0`: overlay schema and compositor convergence.
- extend the existing semantic scene work into one shared widget schema for
  chrome and modal UI
- either evolve `app-scene-menu.*` into the shared widget carrier or add a new
  adjacent `app-ui-widget.*` / `app-scene-overlay.*` layer; the key rule is one
  schema, not more one-off blobs
- give the overlay compositor explicit layer ownership: chrome, transient, and
  modal
- make viewport reservation and overlay anchoring logical-pixel
  responsibilities of the frontend, not of term-space layout math
- status on 2026-04-02:
  - equivalent of old `MENU0A` through `MENU0D` is largely landed
  - the missing piece is not fonts or basic menu rendering; it is convergence
    of left/top/bottom chrome and richer browser widgets onto the same semantic
    model
- primary write set:
  - `src/app/*`
  - `src/sdl-scene.c`
  - `src/sdl-scene-dungeon.c`
  - `src/sdl-scene-menu.c`
  - `src/sdl-render.c`
  - `src/sdl-ui-style.c`
- exit when:
  - the left rail, top message strip, bottom bar, and a centered modal can all
    be described without terminal coordinates
  - changing `main_view_scale` affects only dungeon/world rendering
  - changing overlay scale affects only overlay surfaces

Workstream `OVER1`: persistent chrome migration.
- migrate the left bar, message bar, and bottom bar first
- remove the current split where bars are half overlay and half terminal-era
  state carriers
- convert hidden left-panel overlays, top-line messaging, and footer prompt or
  action hints into explicit widgets owned by the common overlay model
- primary write set:
  - `src/ui/ui-status.c`
  - `src/util-message.c`
  - `src/util-prompt.c`
  - `src/app/app-scene-dungeon.*`
  - `src/runtime/runtime-game.c`
  - `src/sdl-scene-dungeon.c`
- exit when:
  - no normal SDL path for left rail, message strip, or bottom bar depends on
    `Term_putstr()`, `Term_erase()`, `Term_get_size()`, `screen_save()`, or
    `screen_load()`
  - left-rail width and viewport reservation are driven by logical-pixel widget
    metrics, not term columns
  - save-status and quit-path prompts use the same bottom-bar or modal widgets
    as the rest of the UI

Workstream `OVER2`: generic interaction and item browsers.
- finish prompt, confirm, quantity, text-entry, item selection, inventory,
  equipment, identify, compare, and item-action flows on semantic widgets
- replace blocking redraw ownership with session-driven selection and action
  state
- primary write set:
  - `src/object/object-ui-select.c`
  - `src/object/object-ui-display.c`
  - `src/object/object-ui-enhanced.c`
  - `src/object/object-ui-identify.c`
  - `src/cmd/item/*`
  - `src/util-prompt.c`
  - `src/util-message.c`
- exit when:
  - inventory and equipment behave as semantic list/detail overlays with footer
    actions and compare panels
  - no normal SDL path for these flows depends on term-width branching or a
    second legacy renderer

Workstream `OVER3`: look, targeting, sidebar, and minimap.
- migrate look and targeting as a semantic split-pane overlay family rather
  than as more bridge logic
- treat the minimap as an overlay widget, not as a special terminal clone
- allow minimap and similar non-text surfaces to scale fractionally while text
  widgets keep stable snapped metrics
- primary write set:
  - `src/targeting.c`
  - `src/cmd/ui/cmd-ui-look.c`
  - `src/ui/ui-look-sidebar.c`
  - `src/cave.c`
  - `src/sdl-scene-dungeon.c`
- exit when:
  - look and targeting no longer rely on legacy outer redraw loops on the SDL
    path
  - nearby/minimap-style panels stop requiring new term-mirror behavior
  - the minimap can size independently from dungeon tile scale without
    distorting surrounding text UI

Workstream `OVER4`: document and data-heavy browsers.
- migrate help, file viewer, score, quest, character, knowledge, abilities,
  settings, and similar browsers onto the shared overlay widgets
- keep bridge consumers alive only where semantic widgets still do not exist
- primary write set:
  - `src/cmd/ui/cmd-ui-main-menu.c`
  - `src/ui/ui-help.c`
  - `src/ui/ui-file-viewer.c`
  - `src/score/score_ui.c`
  - `src/quest/quest-ui.c`
  - `src/cmd/ui/cmd-ui-settings.c`
  - `src/cmd/ui/cmd-ui-character.c`
  - `src/ui/ui-character-screen.c`
  - `src/cmd/ui/cmd-ui-knowledge.c`
  - `src/cmd/ui/cmd-ui-abilities.c`
  - `src/cmd/ui/cmd-ui-query.c`
- exit when:
  - these screens no longer derive SDL layout from `Term->wid` / `Term->hgt`
  - migrated browsers no longer use the raw-cell bridge as their normal runtime
    renderer

Workstream `OVER5`: narrative and bespoke workflows.
- migrate story, death, metarun, birth, blitz, and smithing after the shared
  widget set is proven on chrome, list/detail, and document flows
- primary write set:
  - `src/ui/ui-story.c`
  - `src/ui/ui-death.c`
  - `src/metarun.c`
  - `src/birth.c`
  - `src/blitz.c`
  - `src/ui/smithing/ui-smithing-screen.c`
- exit when:
  - these flows use shared semantic widgets instead of bespoke cell-positioned
    redraw loops
  - workflow-specific logic remains in the core, but layout and rendering are
    fully owned by the overlay compositor

Execution mapping to existing package names:
| Overlay workstream | Existing package mapping |
| --- | --- |
| `OVER0` | old `MENU0A`-`MENU0E` plus the missing chrome-schema work |
| `OVER1` | persistent-chrome slice that was previously spread across `MENU1`, dungeon overlay code, and status/message helpers |
| `OVER2` | remaining generic interaction work plus old `MENU2` and `MENU8` |
| `OVER3` | the remaining look/target/minimap/sidebar parts of old `MENU1`, `MENU3`, `MENU6`, and `MENU7` |
| `OVER4` | old `MENU3A`-`MENU3B`, `MENU4`, `MENU5`, and the browser-heavy parts of `MENU6`-`MENU7` |
| `OVER5` | old `MENU9`-`MENU11` |

### Parallel Subagent Plan
Only one subagent should own `OVER0` at a time.

After `OVER0` is stable, the following lanes can run in parallel:
| Lane | Write set | Depends on | Notes |
| --- | --- | --- | --- |
| Lane A: persistent chrome | `src/ui/ui-status.c`, `src/util-message.c`, `src/util-prompt.c`, `src/app/app-scene-dungeon.*`, `src/sdl-scene-dungeon.c` | `OVER0` | establishes the canonical left/top/bottom overlay model |
| Lane B: interaction and items | `src/object/object-ui-select.c`, `src/object/object-ui-display.c`, `src/object/object-ui-enhanced.c`, `src/object/object-ui-identify.c`, `src/cmd/item/*` | `OVER0`, preferably after Lane A starts landing | reuses the same footer, list, and compare widgets |
| Lane C: look and minimap | `src/targeting.c`, `src/cmd/ui/cmd-ui-look.c`, `src/ui/ui-look-sidebar.c`, `src/cave.c` | `OVER0`, preferably after Lane A | should consume chrome/footer primitives instead of extending the bridge |
| Lane D: documents and browsers | `src/cmd/ui/cmd-ui-main-menu.c`, `src/ui/ui-help.c`, `src/ui/ui-file-viewer.c`, `src/score/score_ui.c`, `src/quest/quest-ui.c` | `OVER0` | good first payoff after the shared document widgets exist |
| Lane E: settings and character family | `src/cmd/ui/cmd-ui-settings.c`, `src/cmd/ui/cmd-ui-character.c`, `src/ui/ui-character-screen.c`, `src/cmd/ui/cmd-ui-knowledge.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-query.c` | `OVER0`, plus early Lane D widgets | shared need: tabs, list-detail panes, footer actions |
| Lane F: narrative and bespoke | `src/ui/ui-story.c`, `src/ui/ui-death.c`, `src/metarun.c`, `src/birth.c`, `src/blitz.c`, `src/ui/smithing/ui-smithing-screen.c` | `OVER1`-`OVER4` foundations | deliberately last because these are the most stateful workflows |

Parallelization rules:
- do not split ownership of the shared overlay model in `src/app/*` or the SDL
  overlay compositor across multiple agents
- keep `src/cmd/ui/cmd-ui-settings.c`, `src/metarun.c`, `src/birth.c`, and
  `src/ui/smithing/ui-smithing-screen.c` single-owner
- prefer merging Lane A before B/C so selectors and look flows reuse the same
  chrome and footer primitives instead of re-implementing them

### Validation Gates For This Track
Gate `OVER0`:
- changing `main_view_scale` changes dungeon/world rendering only
- changing overlay scale changes overlay surfaces only
- a centered modal, the left rail, the message strip, and the bottom bar can be
  laid out without consulting `Term->wid` / `Term->hgt`
- overlay typography remains visually consistent with current Sil presentation

Gate `OVER1`:
- left rail, message bar, and bottom bar are semantic widgets on the SDL path
- those surfaces no longer depend on direct `Term_*` rendering or on
  `screen_save()` / `screen_load()` in normal play
- viewport reservation for visible chrome is driven by overlay layout metrics

Gate `OVER2`-`OVER3`:
- prompt, text-entry, item-selection, inventory, equipment, look, and targeting
  no longer rely on term-space layout in their normal SDL path
- migrated flows no longer keep a second direct legacy renderer alive
- minimap and similar non-text panels can scale independently without forcing
  text widgets into blurry arbitrary scaling

Gate `OVER4`:
- help, file viewer, main menu, score history, quest status, character sheet,
  knowledge, and settings screens no longer query `Term->wid` / `Term->hgt`
  for SDL layout
- bridge-backed information scenes are reduced to temporary compatibility-only
  surfaces, not the normal architecture for migrated browsers

Gate `OVER5`:
- story, death, metarun, birth, blitz, and smithing use shared semantic widgets
  instead of bespoke cell-positioned redraw loops
- the workflows still look like the current game UI, only decoupled from
  terminal sizing and term ownership

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
- Milestone M1: **complete**
  - UI0 through UI2 complete
  - core is externally drivable and wait-state aware
- Milestone M2: **complete**
  - UI3 complete
  - snapshots and events can describe the live dungeon scene
- Milestone M3: **complete**
  - UI4 complete
  - SDL dungeon rendering is snapshot-driven and frame-based
- Milestone M4: **complete**
  - UI5 boundary work is complete
  - gameplay-coupled selectors publish interaction state and wait-reason
    scopes; remaining cleanup is now tracked in `OVER2`-`OVER5`
- Milestone M5: **closed; remaining work moved to the overlay track**
  - UI6/UI7 are no longer the pacing items
  - the active program is now semantic overlay migration plus runtime
    legacy-render removal across chrome, menus, and browsers
- Milestone M6: **complete**
  - UI8 prototype complete
  - the same boundary works for SDL and a web client

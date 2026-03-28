# UI Architecture Migration Plan

## Purpose
This plan turns the recommended UI direction into an execution plan for the
current tree.

Recommended direction:
- build a frontend-neutral session API
- expose declarative snapshots plus event streams
- keep frame cadence in the frontend
- treat the existing `Term` path as a legacy frontend, not as the future UI API

Status date: March 28, 2026.

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

Status:
- complete in the working tree on 2026-03-28
- `sil-core` now builds without SDL headers or SDL link requirements
- legacy frontend ownership is isolated behind `sil-legacy-compat` and
  `sil-platform-sdl`, and `platform-ui.h` has been replaced by the narrower
  neutral boundary headers

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
- complete in the working tree on 2026-03-28
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

## Menu Modernization Follow-On: Fixed-Size Pixel Menus
Goal:
- keep the dungeon view on integer-scaled tiles
- move menus and modal browsers onto a dedicated pixel UI layer whose visual
  size does not change with `main_view_scale`
- stop expressing menu layout in terminal columns and rows except inside the
  temporary legacy bridge

Current blocker summary:
- `src/sdl-scene-dungeon.c` still renders interaction overlays in main-view
  cell units, so list/prompt overlays scale with the tile view
- `src/sdl-scene-information.c` still renders information scenes from
  row/column text ops against `view->cols` and `view->rows`
- `src/ui/ui-information-scene.c` still mirrors captured `Term` contents, so
  information scenes inherit legacy terminal layout even when shown through the
  new SDL scene stack
- `APP_SCENE_KIND_MENU` exists in `src/app/app-snapshot.h` and is already set
  by bootstrap in `src/main.c`, but the SDL scene stack still has no dedicated
  menu payload or renderer for it
- `app_interaction_state` is the only real semantic menu primitive today
  - it is good enough for prompt, text-entry, list, and targeting overlays
  - it is not rich enough yet for tabbed browsers, multi-column list/detail
    layouts, compare panes, or nested footer action bars
- most menu modules still branch on `Term->wid` / `Term->hgt` to choose
  "compact" layouts, wrapping, and prompt rows

Standing rule for this track:
- do not port menus one by one straight from terminal coordinates into SDL
  pixel code
- first land a shared menu scene and widget model
- treat `APP_SCENE_KIND_INFORMATION` plus `ui_information_scene_present_term()`
  as a bridge for legacy flows, not as the final menu API

### Detailed Menu Audit For Parallel Execution
| Family | Representative files | Current render and input model | Needed shared widgets | Recommended subagent |
| --- | --- | --- | --- | --- |
| Bootstrap and hub menus | `src/init2.c`, `src/cmd/ui/cmd-ui-main-menu.c` | centered `Term_putstr()` menus, `screen_save()`, `screen_load()`, and direct `inkey()` loops | modal menu scene, action list, footer hints | Hub lane |
| Message, hint, map, nearby, and combat-history views | `src/cmd/ui/cmd-ui-main-menu.c`, `src/cmd/ui/cmd-ui-nearby.c`, `src/cave.c`, `src/melee/melee-combat-display.c` | pager and list screens with term-size-derived rows and horizontal scroll | pager, fixed-width document view, simple list browser | Browser lane |
| Settings and config editors | `src/cmd/ui/cmd-ui-settings.c` | mixed menus, text entry, key capture, pane editors, and controller or touch setup in one large terminal-owned file | forms, text entry, key-capture dialog, action list, settings rows | Settings lane |
| Character and knowledge browsers | `src/cmd/ui/cmd-ui-character.c`, `src/ui/ui-character-screen.c`, `src/cmd/ui/cmd-ui-knowledge.c`, `src/ui/ui-look-sidebar.c` | split-pane browsers with tabs, grouped lists, detail panes, and recall actions; many compact-layout branches | tab strip, split-pane list-detail widget, compare pane, footer action bar | Character/knowledge lane |
| Gameplay selector overlays | `src/util-prompt.c`, `src/util-message.c`, `src/object/object-ui-select.c`, `src/targeting.c`, `src/cmd/ui/cmd-ui-look.c` | some semantic interaction state already exists, but SDL still renders in cell units and several loops still block on legacy input | fixed-pixel prompt modal, list modal, text entry, targeting overlay | Interaction lane |
| Ability, song, oath, bane, supplies, and query action menus | `src/cmd/ui/cmd-ui-abilities.c`, supplies path in `src/cmd/ui/cmd-ui-knowledge.c`, `src/cmd/ui/cmd-ui-query.c` | list-plus-detail action menus with confirm branches and recursive term redraw | action list, detail side panel, confirm dialog, stepper or selector rows | Action-menu lane |
| Inventory, equipment, identify, compare, and item-action browsers | `src/object/object-ui-display.c`, `src/object/object-ui-enhanced.c`, `src/object/object-ui-identify.c`, `src/cmd/item/*` | custom row layouts and popups, still partially coupled to term width and classic list rendering | reusable item list rows, compare panel, recall modal, action popup | Inventory lane |
| Help, file viewer, score, run history, quest, story, and death scenes | `src/ui/ui-help.c`, `src/ui/ui-file-viewer.c`, `src/score/score_ui.c`, `src/quest/quest-ui.c`, `src/ui/ui-story.c`, `src/ui/ui-death.c` | information scenes often mirror row and column text through `ui_information_scene` | document scene, history table, detail browser, narrative panel | Document lane |
| Metarun presentation and blessing exchange | `src/metarun.c` | bespoke action menus, threshold pickers, active-effects browsers, and story-stat screens; some paths already mirror through information scene | action list, document view, meters, detail browser, confirm dialog | Metarun lane |
| Birth, blitz, and smithing workflows | `src/birth.c`, `src/blitz.c`, `src/ui/smithing/ui-smithing-screen.c` | dense bespoke workflows with compact-layout math, inline prompts, previews, and multi-step state loops | proven list-detail widgets, forms, steppers, compare panels, workflow adapters | Late bespoke lane |

### Recommended Subagent Execution Map
| Package | Write set | Start after | Notes |
| --- | --- | --- | --- |
| `MENU0A` | new `src/app/app-scene-menu.[ch]`, `src/app/app-snapshot.h`, `src/app/app-session.*`, narrow query headers | start now | define semantic menu payloads, focus ids, tabs, action bars, and scroll state |
| `MENU0B` | new `src/sdl-scene-menu.*`, `src/sdl-scene.c`, `src/sdl-render.c`, `src/main-sdl.c`, `src/sdl-story-font.c` if needed | `MENU0A` | render menus in logical pixels and decouple menu fonts from tile scale |
| `MENU0C` | `src/ui/ui-information-scene.*`, `src/sdl-scene-information.c` | `MENU0A` | keep legacy information scenes working as a bridge while family lanes migrate |
| `MENU1` | `src/sdl-scene-dungeon.c`, `src/util-prompt.c`, `src/util-message.c`, `src/targeting.c`, `src/cmd/ui/cmd-ui-look.c` | `MENU0B` | lands the canonical fixed-pixel prompt, list, and targeting flow |
| `MENU2` | `src/object/object-ui-select.c`, narrow fallout in `src/cmd/item/*` | `MENU0B`, preferably after `MENU1` | finishes the `get_item()` path on top of the shared list modal |
| `MENU3` | `src/init2.c`, `src/cmd/ui/cmd-ui-main-menu.c`, `src/cmd/ui/cmd-ui-nearby.c`, `src/cave.c`, `src/melee/melee-combat-display.c` | `MENU0B`, bridge support from `MENU0C` if needed | intro, pause, message, hint, map, nearby, and combat-history entry flows |
| `MENU4` | `src/ui/ui-help.c`, `src/ui/ui-file-viewer.c`, `src/score/score_ui.c`, `src/quest/quest-ui.c`, `src/ui/ui-story.c`, `src/ui/ui-death.c` | `MENU0B`, with `MENU0C` during transition | lowest-risk browser and document lane after the foundation |
| `MENU5` | `src/cmd/ui/cmd-ui-settings.c` plus any new settings-only helpers | `MENU0B` and text-entry support from `MENU0A` | keep single-owner because this is the largest single menu file |
| `MENU6` | `src/cmd/ui/cmd-ui-character.c`, `src/ui/ui-character-screen.c`, `src/cmd/ui/cmd-ui-knowledge.c`, `src/ui/ui-look-sidebar.c` | `MENU0B` | split-pane browser lane with tabs, groups, and recall hooks |
| `MENU7` | `src/cmd/ui/cmd-ui-abilities.c`, supplies path in `src/cmd/ui/cmd-ui-knowledge.c`, `src/cmd/ui/cmd-ui-query.c` | `MENU0B`, ideally after `MENU1` | action-list and detail-side-panel lane |
| `MENU8` | `src/object/object-ui-display.c`, `src/object/object-ui-enhanced.c`, `src/object/object-ui-identify.c`, `src/cmd/item/*` | `MENU2` | inventory, equipment, identify, compare, and item-action browsers |
| `MENU9` | `src/metarun.c` | `MENU4` and `MENU7` | blessing exchange, thresholds, active-effects browser, and story stats |
| `MENU10` | `src/ui/smithing/ui-smithing-screen.c` | `MENU2`, `MENU7`, and the shared split-pane widgets from `MENU6` | isolate completely; highest-risk live gameplay editor after birth |
| `MENU11` | `src/birth.c`, `src/blitz.c`, shared bootstrap helpers in `src/init2.c` if still needed | `MENU3`, `MENU5`, `MENU6`, `MENU7` | deliberately last because it combines list selection, text entry, detail panes, and gameplay mutation in one loop |

### Menu Inventory And Target Shapes
| Family | Primary files | Current shape | Target shape |
| --- | --- | --- | --- |
| Shared prompt and selector primitives | `src/util-prompt.c`, `src/util-message.c`, `src/object/object-ui-select.c`, `src/targeting.c`, `src/sdl-scene-dungeon.c` | prompts and selectors already publish `app_interaction_state`, but SDL still renders them as cell-sized overlays inside the dungeon view | fixed-pixel modal dialog / list widgets fed by semantic interaction snapshots |
| Pause menu and lightweight overlay menus | `src/cmd/ui/cmd-ui-main-menu.c`, `src/cmd/ui/cmd-ui-nearby.c`, `src/cmd/ui/cmd-ui-query.c`, `src/cmd/ui/cmd-ui-look.c` | centered or edge-anchored text blocks rendered with `Term_putstr()` and `inkey()` | frontend-owned modal menu scene and side panels with fixed pixel metrics |
| Character, knowledge, abilities, and object browsers | `src/cmd/ui/cmd-ui-character.c`, `src/ui/ui-character-screen.c`, `src/cmd/ui/cmd-ui-knowledge.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/object/object-ui-display.c`, `src/object/object-ui-enhanced.c`, `src/object/object-ui-identify.c`, `src/cmd/item/cmd-item-core.c`, `src/cmd/item/cmd-item-activate.c` | mixed list/detail screens with lots of `Term->wid` / `Term->hgt` layout branching | reusable list-detail, tab strip, footer action, compare-panel widgets, and action popups |
| Informational documents and settings | `src/cmd/ui/cmd-ui-settings.c`, `src/ui/ui-file-viewer.c`, `src/ui/ui-help.c` | large term documents, file viewers, and settings trees with row-based paging | scrollable document scene plus frontend-owned settings forms and pickers |
| Score, quest, story, death, and metarun presentation | `src/score/score_ui.c`, `src/quest/quest-ui.c`, `src/ui/ui-story.c`, `src/ui/ui-death.c`, `src/metarun.c` | information scenes and bespoke terminal pages, often with tabs, detail panes, or typewriter effects | fixed-size history/detail browsers and narrative scenes using menu document and animation widgets |
| Birth, blitz, and smithing workflows | `src/birth.c`, `src/ui/smithing/ui-smithing-screen.c`, `src/blitz.c` | dense bespoke workflows with heavy terminal math, inline prompts, and multi-step state loops | last-wave migration onto proven menu widgets plus workflow-specific state adapters |

### Required Shared Infrastructure Before Menu Ports
Workstream M0:
- add a frontend-owned menu layer separate from the tile-scaled dungeon canvas
- give that layer its own font metrics, padding, and width caps so modal menus
  stay visually stable across SDL scale values
- introduce a semantic menu scene/model in `src/app/*`
  - recommended scope: document blocks, titled panels, list rows, footer
    actions, tabs, scroll state, and optional detail panes
- keep the existing interaction snapshot for prompt/text-entry/targeting, but
  extend it or wrap it so renderers no longer infer layout from terminal cells
- keep `ui_information_scene` available as a compatibility adapter while new
  menu scenes are landing

Exit when:
- changing `main_view_scale` changes dungeon tile size but not modal menu pixel
  size
- a menu can be centered, width-limited, and scrolled without consulting
  `Term->wid` / `Term->hgt`
- no new menu code depends on `ui_information_scene_capture_term()`

### Migration Order
Workstream M1: generic interaction consumers.
- move prompt, confirm, quantity, text-entry, item selection, and targeting to
  the fixed-pixel menu layer
- primary write set:
  - `src/util-prompt.c`
  - `src/util-message.c`
  - `src/object/object-ui-select.c`
  - `src/targeting.c`
- this is the dependency for later bespoke workflows

Workstream M2: simple document and pause scenes.
- port the main menu, message recall, hint-message browser, help viewer, file
  viewer, and nearby/object summary screens
- primary write set:
  - `src/cmd/ui/cmd-ui-main-menu.c`
  - `src/ui/ui-help.c`
  - `src/ui/ui-file-viewer.c`
  - `src/cmd/ui/cmd-ui-nearby.c`

Workstream M3: data-heavy browsers.
- port score screens, run history, quest status, character sheet, knowledge
  browsers, abilities/song menus, and enhanced inventory/equipment browsers
- primary write set:
  - `src/score/score_ui.c`
  - `src/quest/quest-ui.c`
  - `src/cmd/ui/cmd-ui-character.c`
  - `src/ui/ui-character-screen.c`
  - `src/cmd/ui/cmd-ui-knowledge.c`
  - `src/cmd/ui/cmd-ui-abilities.c`
  - `src/object/object-ui-display.c`
  - `src/object/object-ui-enhanced.c`
  - `src/object/object-ui-identify.c`
  - `src/cmd/item/cmd-item-core.c`
  - `src/cmd/item/cmd-item-activate.c`

Workstream M4: presentation-heavy narrative scenes.
- port story playback, death/review flows, and metarun presentation screens
- primary write set:
  - `src/ui/ui-story.c`
  - `src/ui/ui-death.c`
  - `src/metarun.c`
- depends on M0 and the scroll/document widgets from M2-M3

Workstream M5: bespoke workflow migrations.
- port birth, blitz setup, oath selection, and smithing after M1-M3 prove the
  widget set
- primary write set:
  - `src/birth.c`
  - `src/ui/smithing/ui-smithing-screen.c`
  - `src/blitz.c`
- this is deliberately last because these flows combine list selection,
  text-entry, scrolling details, compare panes, and gameplay mutations in a
  single loop

### Parallel Subagent Plan
Only one subagent should own M0 at a time.

After M0 merges, the following lanes can run in parallel:
| Lane | Write set | Depends on | Notes |
| --- | --- | --- | --- |
| Lane A: prompt/selectors | `src/util-prompt.c`, `src/util-message.c`, `src/object/object-ui-select.c`, `src/targeting.c` | M0 | establishes the canonical fixed-pixel prompt/list flow |
| Lane B: pause and document screens | `src/cmd/ui/cmd-ui-main-menu.c`, `src/ui/ui-help.c`, `src/ui/ui-file-viewer.c`, `src/cmd/ui/cmd-ui-nearby.c` | M0 | lowest-risk consumer lane; good first UI payoff |
| Lane C: settings | `src/cmd/ui/cmd-ui-settings.c` | M0 | keep isolated because the file is large and touches SDL config/forms |
| Lane D: score and quest browsers | `src/score/score_ui.c`, `src/quest/quest-ui.c` | M0 | shared need: tabs, list-detail panes, scrollable detail views |
| Lane E: character, knowledge, abilities, and object browsers | `src/cmd/ui/cmd-ui-character.c`, `src/ui/ui-character-screen.c`, `src/cmd/ui/cmd-ui-knowledge.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/object/object-ui-display.c`, `src/object/object-ui-enhanced.c`, `src/object/object-ui-identify.c`, `src/cmd/item/cmd-item-core.c`, `src/cmd/item/cmd-item-activate.c` | M0 | shared need: list-detail widgets, compare panes, footer action bars, and action popups |
| Lane F: story, death, and metarun presentation | `src/ui/ui-story.c`, `src/ui/ui-death.c`, `src/metarun.c` | M0 plus M2 document widgets | keep separate from score/quest because metarun already owns a large bespoke surface |
| Lane G: birth and blitz | `src/birth.c`, `src/blitz.c` | M0 plus M1/M3 | should start only after text-entry, list, and detail widgets are stable |
| Lane H: smithing | `src/ui/smithing/ui-smithing-screen.c` | M0 plus M1/M3 | isolate completely; this is the highest-risk workflow after birth |

Parallelization rules:
- do not split ownership of `src/app/*` menu model files or `src/sdl-scene*.c`
  across multiple agents
- keep `src/cmd/ui/cmd-ui-settings.c`, `src/birth.c`, `src/ui/smithing/ui-smithing-screen.c`,
  and `src/metarun.c` single-owner because each is a large bespoke surface
- prefer merging Lane A before G/H so bespoke flows reuse the generic prompt
  and selector path instead of re-implementing it

### Validation Gates For This Track
Gate M0:
- switching SDL scale changes dungeon tile size only
- modal menu width, font size, and padding remain visually stable

Gate M1:
- prompt/text-entry/item-selection/targeting no longer rely on term-space
  layout for their normal SDL path

Gate M2-M3:
- help, file viewer, main menu, score history, quest status, character sheet,
  and knowledge screens no longer query `Term->wid` / `Term->hgt` for layout in
  their SDL path

Gate M4-M5:
- story, death, metarun, birth, and smithing use the shared menu widgets
  instead of bespoke cell-positioned redraw loops

Note:
- for menu execution planning, prefer the `MENU0A`-`MENU11` packages above
- the broader grouping below is retained as architectural background, not as
  the primary menu split

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

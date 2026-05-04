# Touch And Mouse Semantic Parity Parallel Plan

Status: implementation plan, created May 4, 2026.

## Goal

Bring the missing touch and mouse behavior from `develop` into the current
semantic SDL architecture on `unstable`.

The source branch is the behavioral reference. Its terminal-era implementation
inside `src/main-sdl.c` is not a code reference for renderer architecture.
Every feature must land as semantic logical-pixel UI, semantic commands,
app-scene data, or dungeon-scene hit testing.

## Current Audit Result

The current branch already has a good semantic base:

- Menu pointer input: `src/sdl-menu/sdl-menu-pointer.c`
- Map pointer input: `src/sdl-menu/sdl-map-pointer.c`
- Touch pane and swipe input: `src/sdl-touch.c`
- Semantic menu rendering and hit registration: `src/sdl-menu/*`
- Semantic UI command queue: `src/app/app-ui-command.*`,
  `src/app/app-session.*`

The missing develop parity is concentrated in these systems:

- Touch profiles and touch-control config.
- Touch zones, center/corner bindings, and overlay marker rendering.
- Touch top panel with short/long bindings.
- Round movement wheel / round movement profile.
- Touch and mouse tutorial/profile-choice flows.
- Player action menu, pointer attack modes, right-click recall/path behavior.
- Remaining menu/screen semantic hit coverage and settings UI for the new
  touch-control features.

## Hard Rules

1. Do not merge or copy the monolithic `develop:src/main-sdl.c` implementation.
2. Do not add normal-path `Term_*`, `screen_save()` / `screen_load()`,
   `inkey()`, or raw terminal-row hit testing.
3. Touch/mouse behavior must be routed through semantic commands or logical
   dungeon hit testing.
4. Keep keyboard and gamepad behavior unchanged.
5. Keep every agent's write set narrow. Agents are not alone in the codebase and
   must not revert edits made by other agents.
6. Each agent must update this plan's status section with what it finished,
   what remains, and the validation it ran.

## Source Reference

Use these develop references for behavior only:

- `git show develop:src/main-sdl.c`
- `git show develop:src/sdl-config.h`
- `git show develop:src/sdl-config.c`
- Relevant develop commits:
  - `1c3e8fea` mouse movement
  - `47036efe` touch movement
  - `0ba4c355` mouse hover and settings
  - `1a6b4d9f` run history and more menus mouse updates
  - `4590248c` character sheet mouse zones
  - `e0adc965` map and arrows selection mouse
  - `8de38418` round menu
  - `22ea4c23` new touch profiles, monster description and songs info
  - `fcff214f` pathing and character pane possibility
  - `c95c1388` door bash, skeleton clicks
  - `7d283601` wheel direction, touch pane and attack type reset
  - `98ea669d` pane buttons and behaviour
  - `f195fbef` ctrl + mouse handling
  - `ff619d85` mouse tutorial
  - `ffe0a0a0` fixes for swipes
  - `2ddd8396` touch fixes and longer top panel

## Implementation Waves

### Wave 1: Shared Substrate

Run these agents first. They define APIs used by later work.

#### Agent A: Touch Config And Profiles

Model: GPT-5.5, high reasoning.

Write ownership:

- `src/sdl-config.h`
- `src/sdl-config.c`
- `src/platform/sdl-config-defaults.c`
- `src/platform-input.h`
- `src/sdl-touch.c` only for public getter/setter plumbing.

Responsibilities:

- Add config fields equivalent to develop behavior:
  - `touch_profile`
  - per-category touch menu command enable flags
  - `touch_movement_mode`
  - `touch_round_movement_enabled`
  - `touch_zone_overlay_mode`
  - center zone bindings
  - corner action bindings and corner up/down side
  - `touch_top_panel_mode`
  - `touch_top_panel_default_open`
  - touch top panel short and long bindings
  - touch tutorial seen/request state if needed
- Add normalized getters/setters and default getters.
- Preserve loading old `sil_sdl.json` files safely.
- Keep current `touchPane` and swipe config backward-compatible.

Exit gate:

- Config load/save compiles.
- Old configs missing new objects still receive safe defaults.
- New JSON shape is documented in a short note in this plan.

#### Agent B: Semantic Touch Command Model

Model: GPT-5.5, high reasoning.

Write ownership:

- `src/app/app-ui-command.*`
- `src/app/app-movement.*`
- `src/platform-input.h`
- `src/main-sdl.c` only for routing hook declarations/calls.
- New helper files under `src/sdl-menu/` if needed.

Responsibilities:

- Define shared semantic command/action IDs for touch zones, top panel buttons,
  round movement, player action menu, pointer attack, recall/inspect, and
  travel.
- Avoid synthetic terminal key injection for new functionality where an
  app-level command can express intent.
- Provide compatibility adapters only at subsystem boundaries that still consume
  legacy command keys.

Exit gate:

- No new terminal UI call sites.
- Agents C, D, and E have a stable API target.

### Wave 2: Parallel Feature Implementation

Run these agents after Wave 1 APIs are available. The write sets are designed to
be mostly disjoint.

#### Agent C: Touch Zones And Top Panel

Model: GPT-5.5, high reasoning.

Write ownership:

- `src/sdl-touch.c`
- `src/sdl-scene-dungeon.c`
- `src/sdl-main-internal.h`
- small declarations in `src/platform-input.h` if Agent A/B did not add them.

Responsibilities:

- Implement logical-pixel touch zones from develop:
  - center zone bindings
  - corner action zones
  - up/down side selection
  - marker/overlay visibility modes
  - long-press behavior
- Implement top touch panel:
  - short/long binding dispatch
  - default-open behavior
  - compact/longer layout from develop
  - open/close input state reset
- Ensure touch zones do not consume pointer events meant for semantic menus,
  overlay panels, or the map.

Exit gate:

- Touch zones render from logical pixels, not terminal cells.
- Press/release cancellation works when the finger leaves the zone.
- Existing touch pane and swipe behavior still works.

#### Agent D: Round Movement And Player Action Menu

Model: GPT-5.5, high reasoning.

Write ownership:

- New `src/sdl-menu/sdl-round-movement.*` or equivalent.
- New `src/sdl-menu/sdl-player-action-menu.*` or equivalent.
- `src/main-sdl.c` only for event routing and timeout flushing.
- `src/sdl-scene-dungeon.c` only for rendering hooks.

Responsibilities:

- Port round movement wheel as semantic logical-pixel overlay.
- Support round movement profile and config enablement.
- Port player action menu behavior from develop without terminal drawing.
- Long press and cancel behavior must match develop's intent.
- Do not mix this code into `src/sdl-touch.c` if it becomes large.

Exit gate:

- Round wheel movement submits the same movement intent as keyboard/gamepad.
- Cancel/timeout/reset behavior does not leave stuck modifier or touch state.

#### Agent E: Map Pointer Parity

Model: GPT-5.5, high reasoning.

Write ownership:

- `src/sdl-menu/sdl-map-pointer.c`
- New helper file under `src/sdl-menu/` if needed.
- `src/sdl-scene-dungeon.c` only for pointer overlay rendering.
- Command-facing declarations in `src/sdl-main-internal.h` if needed.

Responsibilities:

- Compare current map pointer behavior to develop's:
  - hover look
  - long tap inspect
  - right-click recall
  - path preview and path following
  - ctrl + mouse direction handling
  - pointer attack mode
  - door bash / disarm / tunnel interaction
  - skeleton click interaction
- Keep current semantic map hit testing through dungeon scene coordinates.
- Add missing behavior as semantic commands or focused domain calls.

Exit gate:

- Map pointer behavior works with scaled/compact layouts.
- Path and hover overlays render through scene data.
- Pointer attack resets after attack type changes, matching develop behavior.

#### Agent F: Menu And Screen Coverage

Model: GPT-5.5, high reasoning.

Write ownership:

- `src/cmd/ui/*`
- `src/object/object-ui-*`
- `src/score/score_ui*`
- `src/metarun/metarun-ui-*`
- `src/ui/*`
- `src/runtime/runtime-dungeon-presentation.c`

Responsibilities:

- Audit every develop menu/screen touch commit against current semantic scenes.
- Add missing row, footer, tab, and scroll-region interactions.
- Ensure first tap focuses/selects and second tap activates where the scene
  expects focus-first behavior.
- Cover:
  - main menu
  - settings
  - inventory/equipment
  - supplies
  - songs
  - abilities/oath/bane
  - character sheet zones
  - score/run history/metarun screens
  - birth screens
  - press-any-key and yes/no prompt scenes where already semantic.

Exit gate:

- Every touched screen exposes semantic widget refs for pointer hit testing.
- No screen is solved by adding terminal-coordinate click hacks.

#### Agent G: Touch Control Settings UI

Model: GPT-5.5, high reasoning.

Write ownership:

- `src/cmd/ui/cmd-ui-settings.c`
- `src/cmd/ui/cmd-ui-settings-panes.c`
- New settings helper files under `src/cmd/ui/` if needed.

Responsibilities:

- Add settings surfaces for Agent A config:
  - touch profile
  - touch movement mode
  - zone overlay mode
  - round movement toggle
  - top panel mode/default open
  - center/corner/top-panel binding editors
  - per-menu touch command categories
- Keep existing Touch Panel editor intact.
- Split user-facing structure into Touch Panel and Touch Control where useful.

Exit gate:

- Settings are reachable by keyboard/gamepad/mouse/touch.
- Reset/default actions restore Agent A defaults.

#### Agent H: Touch And Mouse Tutorials

Model: GPT-5.5, medium reasoning.

Write ownership:

- New tutorial scene files under `src/ui/` or `src/sdl-menu/`.
- `src/cmd/ui/cmd-ui-main-menu.c` only for menu entry hooks if needed.
- `src/app/*` only for scene helpers if needed.

Responsibilities:

- Recreate develop touch tutorial and mouse tutorial as semantic/pixel scenes.
- Add profile-choice flow if Agent A exposes profile config.
- Use app scene panels/footer actions instead of direct SDL terminal overlays.
- Keep tutorial text/layout responsive for compact/mobile sizes.

Exit gate:

- Tutorial can be dismissed with pointer/touch/gamepad/keyboard.
- First-run seen flag is persisted and does not repeat unexpectedly.

### Wave 3: Integration And QA

#### Agent I: Focused Test And Audit Harness

Model: GPT-5.5, high reasoning.

Write ownership:

- `tools/*` only if adding audits.
- `tests/*` or CTest definitions only if the repo already supports the pattern.
- Documentation updates in this plan.

Responsibilities:

- Add focused static checks for:
  - no new terminal UI calls in SDL/touch/menu paths
  - no missing config save/load defaults for new touch fields
  - semantic widget registration on newly migrated menus where practical
- Add a manual QA checklist for desktop and mobile-size windows.

Exit gate:

- Existing audits still pass.
- New checks do not require external services or generated release artifacts.

#### Main Agent: Integration Owner

Responsibilities:

- Launch agents by wave and keep their write sets disjoint.
- Review each result before merging into the shared workspace.
- Resolve shared-header conflicts manually.
- Reject copied `develop:src/main-sdl.c` terminal overlays.
- Keep this file updated with lane status.

Recommended integration order:

1. Agent A
2. Agent B
3. Agents C, D, E, F, G, H in parallel after A/B settle
4. Agent I in parallel with final integration review
5. Main-agent cleanup, build, audits, manual smoke checklist

## Validation Gates

Run after each wave:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1
py -3 tools\ui_debt_audit.py --check
```

Run before closeout:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1
powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1 -Target portable
ctest --preset test-standard --output-on-failure
py -3 tools\ui_debt_audit.py --check
py -3 tools\modernization_audit.py --check
py -3 tools\source_size_audit.py --check
py -3 tools\make_guid.py --dry-run
py -3 tools\check_flag_tables.py
```

Manual smoke checklist:

- Desktop mouse:
  - main menu row focus/activate
  - inventory/equipment rows and footer actions
  - character sheet clickable zones
  - score/run-history scrolling and actions
  - map hover, long press, right-click recall, travel, ctrl movement
- Touch/mobile-size window:
  - touch pane short/long press
  - second panel
  - swipes
  - touch zones
  - top panel
  - round movement
  - menu tap/focus/activate
  - any-key and yes/no semantic prompts
- Regression:
  - keyboard-only play
  - gamepad/Steam Deck confirm/back
  - compact layout and scaled window layout

## Lane Status

- Agent A: implemented touch-control config substrate. Added `touchControl`
  JSON alongside legacy `touchPane`: `profile`, `touchPaneDefaultOpen`,
  per-category menu command booleans, `movementMode`,
  `roundMovementLayerEnabled`, `cornerButtonOverlayMode`,
  `cornerButtonCenterBindings`, `cornerButtonUpDownSide`,
  `cornerButtonActionBindings`, `topPanelMode`, `topPanelDefaultOpen`,
  `topPanelBindings`, `topPanelLongBindings`, `swipeEnabled`, and
  `swipeBindings`. Legacy `touchPane` bindings/labels/panel names still load,
  and legacy `touchPane.swipe*` remains a fallback. Added touch tutorial
  seen/request state in SDL config. Validation:
  `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1` passed.
- Agent B: implemented shared semantic command/action IDs and initializer
  helpers in `src/app/app-ui-command.*`; added shared semantic movement
  adapters in `src/app/app-movement.*`; replaced the local SDL movement-command
  builder in `src/main-sdl.c` with the app-layer adapter. Validation:
  `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1` passed.
- Main integration: reviewed A/B together, added mouse tutorial seen/request
  state beside touch tutorial state, kept top-panel open/close binding
  sentinels out of the legacy key queue until the semantic top-panel renderer
  lands, and split SDL app/touch config helpers under `src/platform/` to keep
  source-size and folder-ownership audits green. Validation:
  `git diff --check`,
  `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1`,
  `py -3 tools\ui_debt_audit.py --check`,
  `py -3 tools\modernization_audit.py --check`,
  `py -3 tools\source_size_audit.py --check`, and
  `ctest --preset test-standard --output-on-failure` passed.
- Agent C: not started.
- Agent D: not started.
- Agent E: not started.
- Agent F: not started.
- Agent G: not started.
- Agent H: not started.
- Agent I: not started.

# Sil-More UIX Remaster Plan

Status: amended implementation plan for the semantic-pixel UIX remaster.

This plan replaces the first short inventory with an execution-oriented plan.
The active tree is already past terminal/grid rendering removal: the remaining
problem is not how to draw semantic pixels, but how to make every screen behave
like a modern semantic UI while keeping the compact pixel-art soul of Sil.

## Current Baseline

- `py -3 tools\ui_debt_audit.py --details` currently reports zero matches for
  `inkey()`, `screen_save()` / `screen_load()`, direct `Term_*`, terminal-model,
  and terminal-kernel debt.
- Semantic scene data exists in `src/app/app-ui.[ch]`:
  `app_ui_scene`, `app_ui_panel`, rows, tabs, footer actions, rich text,
  character metrics, minimap cells, layers, styles, focus fields, and widget
  interaction metadata.
- Semantic input data exists in `src/app/app-input.h`, and semantic movement
  data exists in `src/app/app-movement.[ch]`.
- SDL menu rendering is split under `src/sdl-menu/` and `src/sdl-scene-menu.c`.
  It already registers row/tab/footer hit targets through
  `sdl_menu_hit_register()` and routes mouse/touch through
  `src/sdl-menu/sdl-menu-pointer.c`.
- The current weakness is hybrid ownership: most screens publish semantic
  `app_ui_scene` payloads, then wait on key-shaped input with
  `ui_information_scene_wait_key*()`. Pointer/touch activation often falls back
  to `action_key` and `sdl_submit_legacy_input_byte()`.
- Panes and integer main-view scale are live configurable through
  `src/pane-config.h`, `src/pane.c`, `src/sdl-layout.c`, and `src/sdl-config.c`.
  What is missing is direct drag resize, touch gestures, explicit layout schema
  versioning, and immediate persisted layout controls.

## External Design Anchors

- Microsoft's Xbox Accessibility Guidelines emphasize that menus need semantic
  labels, roles, values, focus order, input prompts, and screen narration
  equivalents for core UI text:
  https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/106
- Microsoft's UI navigation command model is the right abstraction for keyboard,
  gamepad, and remote-style UI navigation: Up, Down, Left, Right, View, Menu,
  Accept, Cancel, page/scroll, and context commands:
  https://learn.microsoft.com/en-us/windows/uwp/gaming/ui-navigation-controller
- Apple and Material guidance both point to large effective touch targets:
  Apple recommends at least 44 x 44 points, and Material/Android commonly uses
  48 x 48 dp with spacing.
  https://developer.apple.com/design/tips/
  https://support.google.com/accessibility/android/answer/7101858
- WCAG 2.2 target-size guidance gives the minimum dense-interface floor:
  pointer targets should be at least 24 x 24 CSS px or have enough spacing or an
  equivalent target.
  https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum
- The Secret of Monkey Island: Special Edition is useful as a product analogy:
  it modernized artwork/audio while preserving the original adventure identity
  and even allowed old/new mode switching.
  https://www.wsgf.org/dr/secret-monkey-island-special-edition

## Product Direction

Sil-More UI should feel like a modern remaster of an old-school roguelike:
pixel art, dense information, fast commands, and serious First Age atmosphere
remain. The terminal-era "blue menu" feeling should not remain.

Target visual language: Pixel-Lore Modern.

- Crisp pixel art and tile glyphs remain first-class.
- Panels should use restrained stone, iron, parchment, dark glass, cold light,
  and engraved/illuminated accents instead of flat saturated blue boxes.
- Modern affordances should be visible but not glossy: focused rows, hover
  outlines, pressed states, drag handles, pinned panels, tabs, compact badges,
  small iconography, sortable headers, filters, and preview/detail panes.
- Visual assets may be pixel art, high-quality raster paintings, or generated
  bitmap backdrops, but UI controls stay semantic and data-driven.
- Keyboard speed must stay excellent. Mouse/touch/gamepad support adds parity;
  it must not slow down expert keyboard play.

## Shared UIX Rules

- Every interactive element has a stable widget id, role, action, label, state,
  focus order, optional tooltip/help text, and pointer/touch hit rectangle.
- Hover-only information is forbidden. The same detail must be available through
  mouse hover, keyboard/gamepad focus, and touch long-press.
- Dense mouse rows may use 24 logical px hit height only when spacing or an
  equivalent larger action exists. Touch-first controls target 48 logical px
  where space permits.
- Controller/gamepad navigation uses semantic UI navigation commands, not
  per-screen key labels.
- `action_key` is a compatibility fallback, not the design target.
- Main dungeon scale remains integer-scaled. UI overlays and menu typography use
  logical pixels and density presets; they may scale independently.
- New UI-facing work stays behind `src/app/*`, `src/ui/*`, `src/sdl-menu/*`,
  `src/sdl-scene-*.c`, `src/sdl-layout.c`, `src/sdl-config.c`, and documented
  UI owners. Do not reintroduce terminal APIs or terminal-layout assumptions.

## Corrections To The First Plan

- "Add interactive metadata in `app_ui_scene`" is no longer the foundation.
  The metadata exists. The missing foundation is semantic dispatch, focus,
  hover/pressed state, scroll ownership, drag/resize ownership, and screen
  consumers that stop waiting for key-shaped choices.
- "Key-bridge activation" should not be expanded as the main implementation
  strategy. Use it only to keep hybrid screens working while each family moves
  to widget commands or app intents.
- Drag/resize should not be treated as late polish. Pane resize and overlay
  movement affect layout architecture, config persistence, and hit testing, so
  they need their own lane after the focus/dispatch foundation.
- The document should not imply that terminal rendering remains the normal
  migration target. Current audits say that work is complete.

## Menu Families

### 1. Out-Of-Game Flat Screens

Representative owners:

- Bootstrap/loading: `src/app/app-scene-bootstrap.c`, `src/app/app-session.c`
- Main menu, hints, message recall, about: `src/cmd/ui/cmd-ui-main-menu.c`
- Birth, oath/stat allocation, Blitz setup: `src/app/app-scene-birth*.c`,
  `src/runtime/blitz.c`
- Death, victory, tomb, final review: `src/runtime/runtime-game.c`,
  `src/ui/ui-death.c`
- Halls of Mandos, high scores, run history/detail:
  `src/score/score_ui*.c`
- Metarun history, difficulty, blessings/curses:
  `src/metarun/*`

Target design:

- Replace flat key pages with interactive hub layouts: illustrated header or
  atmospheric backdrop, primary list/card region, detail preview, and stable
  footer actions.
- Main menu should become a quiet full-screen hub: Continue, New Run, Halls of
  Mandos, Settings, Help, Quit, with disabled states and save status surfaced
  semantically.
- Character creation should become a comparison workflow: race/house cards,
  trait tags, stat bars, warnings, oath/blessing consequences, portrait/crest
  art, and a live sheet preview. Keep mutation points safe until final confirm.
- Halls/run history should use sortable/filterable lists, run timeline/detail
  panes, trophy markers, metarun links, and clickable monster/item/artefact
  recall.
- Story/death screens should be readable modern documents with pixel-art
  chapter backdrops, a stable footer, and optional review tabs.

Guardrails:

- Birth flow must preserve final-confirm mutation timing and save safety.
- Death/score paths must preserve `close_game_aux()` ordering and score/metarun
  persistence behavior.
- High scores and run history must not change `scores.raw`, `runs.db`, or
  metarun compatibility unless a separate persistence migration explicitly
  gates it.

### 2. In-Game Overlays

Representative owners:

- Dungeon chrome: `src/app/app-scene-dungeon.c`, `src/ui/ui-status.c`,
  `src/sdl-scene-dungeon.c`, `src/sdl-menu/sdl-scene-menu-chrome.c`
- Inventory/equipment: `src/object/object-ui-enhanced.c`,
  `src/object/object-ui-display.c`
- Item selector/floor items: `src/object/object-ui-select.c`
- Identify/recharge/item prompts: `src/object/object-ui-identify.c`,
  `src/spell/spell-utility.c`
- Look/targeting/nearby/sidebar: `src/cmd/ui/cmd-ui-look.c`,
  `src/ui/ui-look-sidebar.c`, `src/ui/targeting*.c`,
  `src/cmd/ui/cmd-ui-nearby.c`
- Combat rolls/history: `src/melee/melee-combat-display.c`,
  `src/cmd/combat/cmd-combat-rolls.c`
- Smithing: `src/ui/smithing/ui-smithing-screen.c`,
  `src/ui/smithing/*-flow.h`
- Songs/abilities: `src/cmd/ui/cmd-ui-abilities*.c`

Target design:

- Persistent chrome becomes semantic game HUD: left rail, top message strip,
  bottom prompt/action strip, combat roll strip, and optional side panes.
- Modal overlays become draggable/pinnable panels where it makes sense:
  inventory, equipment, floor, look, monster/object recall, message log, combat
  history, and smithing submenus.
- Inventory/equipment become list-detail tools with icon rows, comparison
  blocks, context actions, equip/use/drop commands, floor tab, and detail
  recall links.
- Look/targeting becomes direct map/entity interaction: hover/focus/tap-hold
  inspects, right-click/long-press recalls, adjacent click/tap interacts, safe
  path preview comes before full click-to-travel.
- Smithing uses a modern crafting layout: category rail, recipe/detail pane,
  cost/difficulty preview, current item comparison, and clear invalid-state
  reasons. Keep its engine/script difficulty sync rule intact.

Guardrails:

- Preserve floor `-)` shortcut, `u`/`x` menu cycling, inventory/equipment
  comparison behavior, item selection semantics, and existing prompts.
- Preserve combat roll overlay line count/width behavior unless the combat
  layout itself changes.
- Preserve Steam Deck confirm/back behavior.
- Look/targeting must keep cursor semantics and current recall/detail behavior
  while gaining pointer routes.

### 3. Knowledge, Stats, Settings, And Data Screens

Representative owners:

- Knowledge root/browser/supplies: `src/cmd/ui/cmd-ui-knowledge*.c`,
  `src/object/supplies.c`
- Help/file viewer: `src/ui/ui-help.c`, `src/ui/ui-file-viewer.c`
- Character sheet/tutorial/dump: `src/cmd/ui/cmd-ui-character.c`,
  `src/ui/ui-character-screen.c`, `src/ui/ui-character-dump.c`
- Quest status/typewriter/rewards: `src/quest/quest-ui.c`,
  `src/quest/quest-varda.c`, `src/quest/thrall_quest.c`
- Settings/keybinds/controller/panes/visuals:
  `src/cmd/ui/cmd-ui-settings*.c`
- Self-knowledge/recharge attributes: `src/spell/spell-utility.c`
- Query/debug/spoiler-style browsers: `src/cmd/ui/cmd-ui-query.c`,
  `src/cmd/debug/*`

Target design:

- Use one shared browser shell for tabs, search, sort/filter, list-detail,
  scroll, breadcrumbs, sticky actions, and predictable focus order.
- Detail panes should support icons, compact tables, stat bars, lore text,
  warnings, and recall links generated from existing game data.
- Settings should use proper control roles: toggles, sliders/steppers, value
  selectors, capture buttons, reset buttons, and conflict dialogs.
- Help/file viewers should become document surfaces with search, section list,
  next/previous match, and touch-friendly scroll.

Guardrails:

- Settings must preserve `sil_sdl.json`, `sound.json`, controller, touch pane,
  and movement-binding persistence semantics.
- Keybind/controller capture must ignore stale held buttons/keys.
- Quest screens must preserve quest state transitions and unlock side effects.
- Help/search behavior must remain deterministic and keyboard-accessible.

## Foundation Phases

### UX0. Baseline And Metrics

Status: complete, keep as a gate.

- Keep `tools/ui_debt_audit.py --check` green.
- Keep `tools/modernization_audit.py --details` free of platform-boundary
  regressions.
- Add a UIX-specific audit once UX1 lands:
  - `action_key` activation count in new UIX code
  - screens still using `ui_information_scene_wait_key*()`
  - hit targets without semantic action
  - rows/tabs/footer actions missing tooltip/focus metadata

### UX1. Semantic Widget Command And Focus Foundation

Goal: make rows, tabs, buttons, panels, scroll regions, map cells, drag handles,
and resize handles dispatch semantic UI actions instead of synthetic keys.

Primary write set:

- `src/app/app-ui-interaction.h`
- `src/app/app-ui.[ch]`
- optional new `src/app/app-ui-command.[ch]` or adjacent command module
- `src/app/app-input.h`, `src/app/app-session.[ch]` only if needed for command
  queues or state
- `src/sdl-menu/sdl-menu-pointer.c`
- `src/sdl-menu/sdl-scene-menu*.c`
- `src/sdl-scene-menu.c`
- `src/main-sdl.c` only for event-routing integration

Deliverables:

- Stable widget identity: scene id, panel id, local id, role, action, state,
  focus order, parent/owner, and optional payload.
- UI command/intent dispatch for:
  - activate/select/cancel
  - move focus
  - scroll line/page
  - inspect
  - open context menu
  - drag
  - resize
- A focus manager for pointer hover, keyboard focus, gamepad focus, pressed
  state, and disabled state.
- A semantic fallback policy: existing `action_key` works for hybrid screens,
  but every migrated screen consumes widget commands directly.
- Tooltips/detail requests represented in data, not only drawn from hover.

Exit gate:

- Main menu, a generic browser, and item selector can be navigated and activated
  by keyboard, mouse, touch, and gamepad without screen-specific pointer hacks.

### UX2. Pointer, Touch, And Gamepad UX Parity

Goal: one interaction model across devices.

Primary write set:

- `src/sdl-menu/sdl-menu-pointer.c`
- `src/sdl-touch.c`
- `src/main-sdl.c`
- `src/platform-input.h`
- `src/sdl-menu/sdl-gamepad-labels.c`
- focus/command modules from UX1

Deliverables:

- Hover state and delayed tooltip rendering.
- Touch long-press maps to inspect/detail/context.
- Gamepad focus routing before gameplay bindings when a modal/menu owns focus.
- Wheel and touch scroll routed to semantic scroll regions.
- Per-device prompts generated from semantic actions, not hardcoded text.
- Minimum target checks in debug/audit mode.

Exit gate:

- A screen that works with keyboard also works with pointer, touch, and gamepad
  unless it explicitly declares a gameplay-only input mode.

### UX3. Visual Remaster System

Goal: replace the placeholder blue/flat look with a coherent modern-pixel UI.

Primary write set:

- `src/sdl-ui-style.c`
- `src/sdl-menu/sdl-scene-menu-base.c`
- `src/sdl-menu/sdl-scene-menu-browser.c`
- `src/sdl-menu/sdl-scene-menu-chrome.c`
- `src/sdl-menu/sdl-scene-menu-pages.c`
- `src/app/app-ui.h` for any new panel style ids
- `lib/xtra/graf/`, `lib/xtra/font/`, `lib/pref/` only for intentional assets

Deliverables:

- Design tokens: color roles, accent roles, panel materials, shadow/border
  rules, focus ring, selected/pressed/disabled states, spacing, density, and
  typography buckets.
- New panel styles where needed:
  - hub
  - browser
  - compact overlay
  - document/lore
  - item browser
  - character sheet
  - crafting/smithing
  - map/recall overlay
- A blue-menu removal pass that changes style tokens, not every screen by hand.
- Asset slots for illustrated headers/backdrops that can be enabled per screen
  without making the data model asset-dependent.

Exit gate:

- Main menu, item selector, knowledge browser, and character sheet demonstrate
  the new style without losing compact readability.

### UX4. Dungeon Map Pointer And Touch Interaction

Goal: make the main game accept modern pointer/touch input.

Primary write set:

- `src/sdl-scene-dungeon.c`
- `src/main-sdl.c`
- optional new SDL-local map hit module
- `src/cmd/ui/cmd-ui-look.c`
- `src/ui/ui-look-sidebar.c`
- `src/ui/targeting*.c`
- `src/cave/*` only for narrow query helpers
- `src/app/app-movement.[ch]` and movement consumers only if direct movement
  command consumption is part of the slice

Deliverables:

- SDL-local helper: window pixel -> main view -> map cell -> map y/x.
- Map-cell hit targets or equivalent map hit-test API.
- Hover updates cursor/inspection preview without committing a turn.
- Adjacent left click/tap submits semantic move/interact.
- Right click or touch long-press opens recall/detail/context.
- Distant left click/tap starts travel toward the clicked map cell.
- Safe path preview before any full click-to-travel implementation.

Exit gate:

- A player can inspect, recall, and move to adjacent cells with mouse/touch
  without breaking keyboard movement or targeting.

### UX5. Panes, Overlay Dragging, And Scale Controls

Goal: make panes and overlays behave like modern game UI surfaces while keeping
the main view integer-scaled.

Primary write set:

- `src/pane.c`
- `src/pane-config.h`
- `src/sdl-layout.c`
- `src/sdl-config.[ch]`
- `src/cmd/ui/cmd-ui-settings-panes.c`
- `src/sdl-menu/sdl-menu-pointer.c` and UX1 command modules only after UX1

Deliverables:

- Pane border hit targets and drag handles.
- Pixel drag -> `pane_config.rect.rows/cols` conversion with min-size clamps.
- Optional ratio-based resize persistence where fixed rows/cols are awkward.
- Layout schema version and migration defaults in `sil_sdl.json`.
- Immediate save path after pane/scale changes, not only at shutdown.
- In-game scale control: integer main view scale, overlay density, menu font
  buckets, and reset-to-default.
- Overlay panel drag/pin state stored separately from supporting pane layout.

Exit gate:

- Side/bottom panes resize live with mouse/touch, main view stays integer
  scaled, and the layout survives restart.

## Screen Migration Waves

Wave A: high-traffic semantic activation.

- Main menu: `src/cmd/ui/cmd-ui-main-menu.c`
- Item selector/floor/inventory/equipment:
  `src/object/object-ui-select.c`, `src/object/object-ui-enhanced.c`,
  `src/object/object-ui-display.c`
- Look/targeting:
  `src/cmd/ui/cmd-ui-look.c`, `src/ui/targeting*.c`,
  `src/ui/ui-look-sidebar.c`

Wave B: shared browser shell.

- Knowledge, supplies, score/run history, quest status, settings, character
  sheet.
- Move navigation/search/filter/sort/scroll/focus into common browser helpers.

Wave C: out-of-game remaster screens.

- Main hub final design, birth/character creation, Halls of Mandos, death,
  story, metarun history/stats.

Wave D: complex nested workflows.

- Smithing, blessings/curses, advanced settings capture, help/file viewer,
  query/debug/spoiler browsers.

## Parallel Agent Implementation Plan

Only one agent should own UX1 at a time. After UX1 lands, work can split by
disjoint write sets.

| Lane | Scope | Primary Write Set | Depends On | Notes |
| --- | --- | --- | --- | --- |
| A | Semantic command/focus foundation | `src/app/app-ui*`, optional `src/app/app-ui-command*`, `src/app/app-session*`, `src/sdl-menu/sdl-menu-pointer.c`, `src/sdl-menu/sdl-scene-menu*.c` | none | Single owner. Other UIX lanes wait for its public API. |
| B | Main menu and out-of-game hub v1 | `src/cmd/ui/cmd-ui-main-menu.c`, selected `src/ui/ui-story.c`, `src/ui/ui-death.c` | A | Keep save/quit and score ordering unchanged. |
| C | Item overlays and item selector | `src/object/object-ui-select.c`, `src/object/object-ui-enhanced.c`, `src/object/object-ui-display.c`, `src/object/object-ui-identify.c`, `src/cmd/item/*` | A | Preserve floor `-)`, cycling, comparison, and current item semantics. |
| D | Dungeon pointer/look/target | `src/sdl-scene-dungeon.c`, `src/main-sdl.c`, `src/cmd/ui/cmd-ui-look.c`, `src/ui/ui-look-sidebar.c`, `src/ui/targeting*.c` | A | Coordinate with Lane F if both need `src/main-sdl.c`. |
| E | Shared browser shell | `src/cmd/ui/cmd-ui-knowledge-browser.c`, `src/score/score_ui*.c`, `src/quest/quest-ui.c`, `src/ui/ui-help.c`, `src/ui/ui-file-viewer.c` | A | Extract shared helpers before restyling every browser. |
| F | Panes and scale | `src/pane.c`, `src/pane-config.h`, `src/sdl-layout.c`, `src/sdl-config.[ch]`, `src/cmd/ui/cmd-ui-settings-panes.c` | A | Own layout persistence and drag resize. Avoid D's event-router work. |
| G | Visual tokens and renderer styles | `src/sdl-ui-style.c`, `src/sdl-menu/sdl-scene-menu-base.c`, `src/sdl-menu/sdl-scene-menu-browser.c`, `src/sdl-menu/sdl-scene-menu-chrome.c`, `src/sdl-menu/sdl-scene-menu-pages.c`, assets under `lib/xtra/` | A | Token-first. Do not hardcode one-off colors per screen. |
| H | Birth/metarun/smithing workflows | `src/app/app-scene-birth*`, `src/metarun/*`, `src/ui/smithing/*` | B, C, E | Late because these flows are stateful and nested. |

Parallelization rules:

- Do not split ownership of `src/app/app-ui*` or the semantic command queue.
- Do not let two lanes edit `src/main-sdl.c` at the same time; route conflicts
  through the integrator.
- Keep `src/ui/smithing/ui-smithing-screen.c`, `src/metarun/*`, and birth
  scenes single-owner.
- Visual Lane G should change renderer tokens and shared styles before screen
  owners start custom styling.
- Each lane must list exact files changed and run the validation appropriate to
  its screen family.

## Validation Gates

Common:

- `py -3 tools\ui_debt_audit.py --check`
- `py -3 tools\modernization_audit.py --details`
- `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1`

Input/UI smoke tests:

- Keyboard, mouse, touch, and gamepad can activate rows, tabs, footer buttons,
  cancel/back, scroll, and change focus.
- Hover/focus/touch long-press all expose the same detail text.
- No migrated screen depends on a new synthetic key path for its primary
  pointer/touch behavior.

Gameplay smoke tests:

- Inventory/equipment overlays.
- Floor-item `-)` behavior.
- Unified look, monster/object recall, targeting, and sidebar.
- Combat roll overlay.
- Main menu continue/new game/save/quit states.
- Character creation final confirmation.
- Halls of Mandos/run history.
- Settings save/reload, including panes, movement, gamepad, touch pane, sound.

Done when:

- The common UIX foundation can express all current rows/tabs/buttons/panels as
  semantic controls.
- High-traffic menus work with keyboard, mouse, touch, and gamepad.
- Main dungeon supports pointer inspect/recall and adjacent movement.
- Panes resize live and persist.
- The default look no longer reads as terminal-era blue placeholder UI.

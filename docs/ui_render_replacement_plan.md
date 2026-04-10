# UI Render Replacement Plan

Status: current on April 11, 2026. This is the execution document for SDL
runtime UI replacement and supersedes
[`ui_architecture_migration_plan.md`](./ui_architecture_migration_plan.md) for
forward UI work.

## End State
- Ship one SDL runtime UI renderer.
- Use one shared semantic model for non-map UI: `app_ui_scene`.
- Preserve the shipped Sil look, but do not preserve terminal-grid layout
  rules just because they used to exist.
- Keep dungeon scale and non-map UI scale independent.
- Delete bridge code as replacements land. Do not keep parallel renderers.

## State Snapshot On 2026-04-11
### Landed
- `src/app/app-ui.[ch]` is real and is now the shared semantic UI layer.
- SDL already renders semantic panels for:
  - dungeon chrome and transient overlays
  - centered modal panels
  - browser or list-detail panels
  - document panels
  - character sheet panels
  - minimap panels
- Shared semantic primitives already exist for:
  - rows, detail text, footer actions
  - tabs
  - rich paragraphs
  - document text, cell, and cursor ops
  - character metrics and stats
  - minimap cells
- Direct `app_ui_scene` producers are already shipping for:
  - dungeon chrome
  - dungeon transient overlays
  - main-menu overlay
  - nearby monster or object overlays
  - look sidebar
  - standalone item selector and oath prompt
  - help
  - knowledge browser shells
  - map viewer
  - character sheet
- Inventory, equipment, and floor selection now publish a dedicated semantic
  browser overlay scene in SDL snapshot mode from
  `src/object/object-ui-select.c`, with tabs, item icons, weights, and footer
  actions.
- Direct `i` / `e` inventory and equipment menus now also publish semantic
  overlay panels in SDL snapshot mode from
  `src/object/object-ui-enhanced.c`, using overlay-style list presentation
  rather than browser-menu chrome, and the item commands enter those menus
  through snapshot-only wrappers instead of owning `screen_save()` around
  them.
- The main character-sheet command is semantic now, but legacy
  `display_player()` rendering still exists for other workflows such as birth,
  death, file dump, and status-adjacent helpers.
- The following bridge-era payloads or shims are already gone:
  - `app_menu_scene`
  - the raw left-rail mirror path
  - `APP_INFORMATION_SCENE_FLAG_TERM_MIRROR`
  - `ui_information_scene_enter_mirror()`
- `py -3 tools/ui_debt_audit.py --check` passes on this branch.
- Current audit counts on April 11, 2026:
  - this branch after the latest item-overlay slice:
  - `inkey()` call sites: 37 files / 87 matches
  - `screen_save()` + `screen_load()` call sites: 29 files / 204 matches
  - direct `Term_*` render/control calls: 64 files / 1,576 matches
  - `#include "platform-ui.h"`: 0 files / 0 matches
  - `get_sdl_*` / `set_sdl_*` outside platform code: 6 files / 208 matches

### Still Real Debt
- The live term-capture bridge still exists:
  - `ui_information_scene_capture_term()`
  - `ui_information_scene_present_term()`
  - internal auto-capture support in `src/sdl-render.c`
- User-facing `ui_information_scene_present_term()` callers still exist in:
  - `src/cmd/ui/cmd-ui-settings.c`
  - `src/cmd/ui/cmd-ui-main-menu.c`
  - `src/melee/melee-combat-display.c`
  - `src/quest/quest-ui.c`
  - `src/score/score_ui.c`
  - `src/metarun.c`
  - `src/ui/ui-story.c`
  - `src/ui/ui-death.c`
- The document bridge still exists:
  - `app_information_scene`
  - `app_ui_scene_from_information_document()`
  - `ui_information_scene_present_document()`
- That document bridge is still the production path for document-style screens
  such as:
  - file viewer
  - object-info and compare/info screens
  - score and quest document pages
  - main-menu about, hint-detail, hint-list, and message-recall pages
  - character tutorial
  - thrall reward
  - some knowledge detail flows
- Raw interaction panel debt still exists:
  - `app_interaction_panel_snapshot`
  - `app_session_set_interaction_panel()`
  - `sdl_scene_render_interaction_panel()`
- Inventory, equipment, and floor selectors now have their own migration slice:
  - the base selector overlay and direct `i` / `e` enhanced menus are
    semantic in SDL snapshot mode
  - the direct `i` / `e` menus are now overlay-style semantic panels rather
    than browser-menu scenes
  - but ownership still lives in legacy blocking loops in
    `src/object/object-ui-select.c` and `src/object/object-ui-enhanced.c`
  - and the wider item family still depends on legacy list layout, compare
    flows, and term-owned side screens in `src/object/object-ui-display.c`,
    `src/object/object-ui-enhanced.c`, and `src/cmd/item/*`
- Some UI families still derive behavior or layout from `Term->wid`,
  `Term->hgt`, `screen_save()`, `screen_load()`, or blocking `inkey()` loops.

## Reality Check
- Older plan text that says tabs, minimap, browser or list-detail shells, or
  character-sheet widgets are still missing is stale.
- The current blocker is no longer "invent the shared widgets first."
- The current blocker is "move the remaining bridge-owned screens onto the
  semantic widgets that already exist, then delete the bridge APIs."

## Next Slice
### Slice: Kill Live Term Capture In Browser And Detail UI
Goal:
- remove `ui_information_scene_present_term()` from normal browser or detail
  screens so SDL no longer depends on live `Term` capture for that family

Why this is next:
- it is the most concentrated remaining runtime render bridge
- it blocks deletion of `ui_information_scene_capture_term()`
- the shared semantic primitives needed for much of this work already exist

Primary targets:
- `src/cmd/ui/cmd-ui-settings.c`
- `src/cmd/ui/cmd-ui-main-menu.c` recall, hint, and message viewers
- `src/melee/melee-combat-display.c`
- `src/score/score_ui.c` run-history detail
- `src/quest/quest-ui.c` only where the flow is browser or detail shaped, not
  bespoke typewriter-story flow

Approach:
- build `app_ui_scene` directly when the screen is interactive or
  list/detail-shaped
- use the shared browser shell instead of redrawing a term buffer
- keep `app_information_scene` only for true document-like content that still
  maps cleanly to the shared document panel
- do not add new `ui_information_scene_present_term()` or new raw
  interaction-panel dependencies

Exit when:
- no normal browser or detail screen calls `ui_information_scene_present_term()`
- remaining `present_term()` callers are only bespoke workflows still scheduled
  later
- no new SDL browser layout depends on `Term->wid` or `Term->hgt`

### Slice: Item Selector Overlay Family
Goal:
- finish migrating inventory, equipment, and floor selection into dedicated
  semantic overlay menus and remove the remaining legacy selector ownership

Status:
- In progress.
- Current chunk landed:
  - `src/object/object-ui-select.c` now builds a dedicated `app_ui_scene`
    browser overlay directly instead of routing the selector through
    `app_interaction_state` plus `app_ui_scene_from_interaction()`
  - the snapshot selector now has explicit tabs, item icons, weights, footer
    actions, and selected-item detail lines
  - `src/object/object-ui-enhanced.c` now builds direct semantic overlay
    panels for the direct `i` / `e` inventory and equipment menus in SDL
    snapshot mode, using overlay list presentation instead of browser-menu
    chrome
  - `src/cmd/item/cmd-item-core.c` and `src/cmd/item/cmd-fletchery.c` now
    enter the enhanced menus through snapshot-only wrappers instead of owning
    `screen_save()` around those menu surfaces

Primary write set:
- `src/object/object-ui-select.c`
- `src/object/object-ui-display.c`
- `src/object/object-ui-enhanced.c`
- `src/cmd/item/*`

Next big chunks:
- move compare/detail and item-action side flows onto semantic detail panels
  and footer actions
- route remaining item-family submenus and chooser flows onto the same
  semantic scene family instead of bespoke term redraw loops
- delete the remaining selector-specific `screen_save()` / `screen_load()`
  ownership and term list rendering from the item family

Exit when:
- inventory, equipment, and floor selection no longer depend on legacy term
  list rendering in normal SDL play
- the item selector family no longer depends on
  `app_ui_scene_from_interaction()`
- selector flows no longer need `screen_save()` / `screen_load()` to own their
  overlays

## After That
### Slice: Remove The Document Bridge
- convert remaining `app_information_scene` producers to direct
  `app_ui_scene` document or rich-text output
- delete `ui_information_scene_present_document()`
- delete `app_information_scene`

### Slice: Replace Raw Interaction Panels
- replace `app_interaction_panel_snapshot` with semantic prompt, list, and
  text-entry payloads
- stop rendering SDL overlays from raw cell dumps

### Slice: Finish Bespoke Workflows
- remaining likely late movers:
  - story
  - death
  - metarun stats and history
  - birth
  - smithing
  - blitz
  - any typewriter or heavily scripted workflow still owning its own blocking
    loop

## Guardrails
- Preserve the shipped Sil visual treatment. Semantic migration is not
  permission to restyle screens.
- Preserve rich prose as rich prose. Do not re-encode paragraphs as fake grid
  cells just to keep inline colors.
- Preserve fixed-grid document ops only where the content is genuinely
  fixed-layout: tables, cursors, tiles, diagrams, or exact glyph alignment.
- Do not add new SDL-path dependencies on:
  - `ui_information_scene_present_term()`
  - `ui_information_scene_capture_term()`
  - `app_information_scene`
  - `app_interaction_panel_snapshot`
  - layout decisions derived from `Term->wid` or `Term->hgt`

## Validation
- run `py -3 tools/ui_debt_audit.py --check`
- smoke-test the screen family changed by the slice
- judge parity against the current shipped Sil presentation, not against
  terminal-era width limits

## Success Condition
This work is finished when no shipped SDL UI surface still depends on live
`Term` rendering, term capture, information-scene document bridging, or raw
term-grid UI contracts.

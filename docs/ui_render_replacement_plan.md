# UI Render Replacement Plan

Status: current on April 13, 2026. This is the execution document for SDL
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

## State Snapshot On 2026-04-13
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
- Direct semantic document or browser/detail scenes are now also shipping for:
  - file viewer
  - object info and compare/info screens
  - score pages
  - quest status
  - main-menu about
  - thrall reward
  - knowledge curse detail
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
- The `l` look sidebar now renders through a dedicated semantic pixel overlay
  rail in SDL snapshot mode rather than borrowing the status-rail panel
  surface, so it scales with the overlay UI like inventory and equipment
  while keeping a transparent overlay treatment instead of an opaque menu
  block.
- Main-menu hint message list and detail, message recall, and the
  information-scene main-menu fallback now publish direct semantic
  `app_ui_scene` output instead of routing normal SDL use through live term
  capture or document bridging.
- Run-history detail in `src/score/score_ui.c` now publishes a direct
  semantic browser scene instead of a live term-capture frame, with tabs,
  list/detail rails, and direct semantic detail text.
- The main character-sheet command is semantic now, but legacy
  `display_player()` rendering still exists for other workflows such as birth,
  death, file dump, and status-adjacent helpers.
- The following bridge-era payloads or shims are already gone:
  - `app_menu_scene`
  - the raw left-rail mirror path
  - `APP_INFORMATION_SCENE_FLAG_TERM_MIRROR`
  - `ui_information_scene_enter_mirror()`
- The raw interaction-panel bridge is now gone:
  - `app_interaction_panel_snapshot`
  - `app_session_set_interaction_panel()`
  - `sdl_scene_render_interaction_panel()`
  - `app_ui_scene_from_interaction()`
- User-facing document presentation has been migrated off
  `ui_information_scene_present_document()`.
- `src/cmd/ui/cmd-ui-settings.c`, `src/melee/melee-combat-display.c`, and
  `src/metarun.c` no longer call `ui_information_scene_present_term()`
  directly; they now publish through `ui_information_scene_present_ui()`
  after bridging captured term output into an `app_ui_scene`.
- `src/ui/ui-character-screen.c` tutorial pages for compact birth-context
  help now build a direct document-style `app_ui_scene`, though the rest of
  that tutorial flow still falls back to the older document adapter path.
- `src/init2.c` welcome screen now builds and presents a direct semantic
  welcome panel scene from `welcome_screen_build_ui_scene()` and
  `welcome_screen_present_ui()`.
- `src/metarun.c` now builds a direct semantic browser scene for the main
  "Current Story Statistics" screen through
  `metarun_build_stats_browser_scene()`, though the submenus launched from
  that screen still remain legacy.
- `py -3 tools/ui_debt_audit.py --check` passes on this branch.
- Current audit counts on April 13, 2026:
  - `inkey()` call sites: 37 files / 74 matches
  - `screen_save()` + `screen_load()` call sites: 28 files / 187 matches
  - direct `Term_*` render/control calls: 62 files / 1,529 matches
  - `#include "platform-ui.h"`: 0 files / 0 matches
  - `get_sdl_*` / `set_sdl_*` outside platform code: 6 files / 208 matches

### Still Real Debt
- The live term-capture bridge still exists:
  - `ui_information_scene_capture_term()`
  - `ui_information_scene_present_term()`
  - internal auto-capture support in `src/sdl-render.c`
- User-facing `ui_information_scene_present_term()` callers are now narrowed
  to bespoke workflows in:
  - `src/quest/quest-ui.c`
  - `src/ui/ui-story.c`
  - `src/ui/ui-death.c`
- The document bridge still exists:
  - `app_information_scene`
  - `app_ui_scene_from_information_document()`
  - `ui_information_scene_present_document()`
- `ui_information_scene_present_document()` no longer has in-tree callers, but
  the API still exists and should be deleted.
- The document adapter core is now effectively dead from gameplay-module
  call sites, but it still exists in the bridge implementation and should not
  be deleted until the remaining live term-capture bridge is gone too.
- The main "Current Story Statistics" screen is semantic now, but the submenu
  family launched from `src/metarun.c:print_metarun_stats()` is still legacy:
  - `show_all_active_curses()`
  - `open_blessing_exchange()`
  - `adjust_blessing_threshold_menu()`
  - `choose_difficulty_menu()`
  - `list_metaruns()`
  - `show_completed_quests_summary()`
  - shared live-term helpers `metarun_present_term()` and
    `metarun_wait_key()`
- Inventory, equipment, and floor selectors now have their own migration slice:
  - the base selector overlay and direct `i` / `e` enhanced menus are
    semantic in SDL snapshot mode
  - the direct `i` / `e` menus are now overlay-style semantic panels rather
    than browser-menu scenes
  - the snapshot selector path now owns its own semantic blocking loop in
    `src/object/object-ui-select.c`
  - but ownership still lives in legacy or term-shaped blocking loops in
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
- The current blocker is no longer "migrate the document producers first".
- The welcome screen and the main story-statistics screen are no longer part
  of the remaining debt.
- The current blocker is now the remaining legacy term-owned submenu families
  and bespoke live-term workflows:
  - the metarun story-statistics submenus
  - the remaining item-family loops
  - quest typewriter flow
  - story flow
  - death flow
  - only after those, the final bridge cleanup

## Next Slices
### Slice: Migrate Metarun Story-Statistics Submenus
Goal:
- migrate the legacy submenu family launched from the semantic
  "Current Story Statistics" screen
- remove the metarun-specific live-term helper path from those submenus

Why this is next:
- the top-level story-statistics screen is already semantic
- the remaining debt is concentrated in one file
- those submenus are user-facing and still term-owned even on the SDL path

Primary targets:
- `src/metarun.c`
  - `show_all_active_curses()`
  - `open_blessing_exchange()`
  - `adjust_blessing_threshold_menu()`
  - `choose_difficulty_menu()`
  - `list_metaruns()`
  - `show_completed_quests_summary()`

Approach:
- keep the main story-statistics screen semantic
- migrate each submenu to direct `app_ui_scene` presentation
- remove the need for `metarun_present_term()` and `metarun_wait_key()` in
  those submenu flows
- leave truly narrative or heavily scripted metarun flows for the later
  bespoke-workflow slice

Exit when:
- no submenu launched from `print_metarun_stats()` depends on live term
  presentation
- the metarun story-statistics submenu family no longer depends on
  `metarun_present_term()`
- any remaining metarun legacy flows are clearly outside the
  story-statistics submenu family

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
- selector flows no longer need `screen_save()` / `screen_load()` to own their
  overlays

## After That
### Slice: Delete The Remaining Term-Capture Bridge
- convert the remaining bespoke `present_term()` workflows off live term
  capture:
  - `src/quest/quest-ui.c` typewriter-story flow
  - `src/ui/ui-story.c`
  - `src/ui/ui-death.c`
- remove `ui_information_scene_capture_term()`
- remove `ui_information_scene_present_term()`
- remove internal auto-capture support in `src/sdl-render.c`
- delete `app_information_scene` and the information-scene snapshot path once
  no runtime flow depends on them

### Slice: Finish Bespoke Workflows
- remaining likely late movers:
  - metarun history and any remaining metarun side flows not covered by the
    story-statistics submenu slice
  - story
  - death
  - birth
  - smithing
  - blitz
  - any typewriter or heavily scripted workflow still owning its own blocking
    loop

## Parallel Rollout
This is the recommended parallel implementation plan for the remaining work.
Top-level implementation agents should use `gpt-5.4` with xhigh reasoning.
Read-only grep or code-reading subagents can use `gpt-5.4-mini` with xhigh
reasoning.

### Phase Status
- Old Phase 1 adapter work is now effectively done in:
  - `src/cmd/ui/cmd-ui-settings.c`
  - `src/melee/melee-combat-display.c`
  - `src/metarun.c` main story-statistics screen only
  - `src/ui/ui-character-screen.c`
  - `src/init2.c` welcome screen
- Old Phase 1 item-family work is not done yet:
  - `src/object/object-ui-select.c`
  - `src/object/object-ui-enhanced.c`
  - `src/object/object-ui-display.c`
  - `src/cmd/item/cmd-item-activate.c`
  - `src/cmd/item/cmd-pickup.c`
- The metarun story-statistics submenus were previously only implied under
  broad metarun cleanup. They are now an explicit next slice.

### Next Parallel Slice: Launch In Parallel Now
These write sets are disjoint and can run at the same time.

#### Agent N1-A: Metarun Story-Statistics Submenus
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/metarun.c

Goal:
- keep the main "Current Story Statistics" screen semantic
- migrate the submenu family launched from print_metarun_stats() off legacy
  term-owned presentation
- target these flows explicitly:
  - show_all_active_curses()
  - open_blessing_exchange()
  - adjust_blessing_threshold_menu()
  - choose_difficulty_menu()
  - list_metaruns()
  - show_completed_quests_summary()
- remove the need for metarun_present_term() and metarun_wait_key() in those
  submenu flows

Constraints:
- preserve the already-landed semantic main stats screen
- preserve Steam Deck prompts, action routing, and current submenu behavior
- do not edit quest-ui.c, ui-story.c, ui-death.c, or bridge/core files
- if you encounter truly narrative metarun flows, isolate them clearly and
  leave them for the later bespoke-workflow slice

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which metarun flows, if any, still intentionally depend on live
  term presentation after your change
```

#### Agent N1-B: Selector Snapshot Ownership
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/object/object-ui-select.c

Goal:
- finish the selector snapshot family in this file
- keep the semantic SDL snapshot scene path
- remove or isolate remaining selector-owned screen_save(), screen_load(),
  inkey(), and direct Term-owned overlay control where it is only serving the
  old non-snapshot overlay path

Constraints:
- preserve inventory/equipment/floor selection behavior, highlight logic,
  compare or verify side flows, and fallback behavior
- do not edit object-ui-enhanced.c, object-ui-display.c, or cmd/item/*
- if a wider change looks necessary, stop and report rather than widening the
  write set

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which selector-owned legacy branch remains, if any, and why
```

#### Agent N1-C: Enhanced Inventory And Equipment Menus
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/object/object-ui-enhanced.c

Goal:
- finish the semantic migration of the direct i/e enhanced menus
- preserve app_session_publish_dungeon_overlay_scene() semantic presentation
- remove remaining legacy inkey()/Term_* ownership from show_inven_enhanced()
  and show_equip_enhanced() wherever it only exists for the old terminal
  overlay presentation

Constraints:
- preserve story-font parity where still required
- do not edit object-ui-select.c, object-ui-display.c, or cmd/item/*
- do not regress death spectator or portable-controls behavior

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize any remaining intentional term-shaped fallback left in this file
```

#### Agent N1-D: Wider Item Family Raw Loops
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/object/object-ui-display.c
- src/cmd/item/cmd-item-activate.c
- src/cmd/item/cmd-pickup.c

Goal:
- isolate or remove remaining terminal-era item selection loops outside the
  semantic snapshot menus
- keep show_inven(), show_equip(), and show_floor() only where they are still
  needed as shared helpers or true legacy fallback
- focus on raw-loop owners such as sanctity-style choosers and pickup-from-pile
  flows

Constraints:
- do not edit object-ui-select.c or object-ui-enhanced.c
- preserve behavior and prompts
- if a workflow is truly bespoke and should stay late, isolate it clearly
  instead of half-migrating it

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which raw item loops remain and why
```

#### Agent N1-E: Quest Typewriter Workflow
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/quest/quest-ui.c

Goal:
- remove the remaining ui_information_scene_present_term() usage from the
  quest typewriter-story flow
- preserve the quest status semantic scene that already exists
- migrate only the bespoke typewriter or story-like presentation path

Constraints:
- preserve pacing, skip behavior, pagination, and prompts
- do not edit ui-story.c, ui-death.c, or bridge/core files

Validation:
- run py -3 tools/ui_debt_audit.py --check
- confirm rg -n "ui_information_scene_present_term\\(" src/quest/quest-ui.c returns no matches
```

#### Agent N1-F: Story Workflow
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/ui/ui-story.c

Goal:
- remove the remaining ui_information_scene_present_term() usage from story
  presentation
- preserve fade, paging, fast-forward, story-font behavior, and prompts
- replace live term presentation with direct semantic scene updates

Constraints:
- do not edit quest-ui.c, ui-death.c, or bridge/core files
- preserve fallback behavior if semantic presentation is unavailable

Validation:
- run py -3 tools/ui_debt_audit.py --check
- confirm rg -n "ui_information_scene_present_term\\(" src/ui/ui-story.c returns no matches
```

#### Agent N1-G: Death Workflow
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/ui/ui-death.c

Goal:
- remove the remaining ui_information_scene_present_term() usage from death and
  epilogue workflows
- preserve character info review, final menu behavior, prompts, and legacy
  fallback behavior

Constraints:
- do not edit quest-ui.c, ui-story.c, or bridge/core files
- do not restyle the death flow

Validation:
- run py -3 tools/ui_debt_audit.py --check
- confirm rg -n "ui_information_scene_present_term\\(" src/ui/ui-death.c returns no matches
```

### Final Cleanup: Launch After The Next Slice Merges
#### Agent F1-A: Delete The Dead Information-Scene Bridge
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/ui/ui-information-scene.c
- src/ui/ui-information-scene.h
- src/app/app-scene-information.c
- src/app/app-scene-information.h
- src/app/app-session.c
- src/app/app-session.h
- src/app/app-ui.c
- src/app/app-ui.h
- src/sdl-render.c
- src/sdl-scene-information.c
- src/externs.h

Goal:
- delete ui_information_scene_present_document()
- delete ui_information_scene_capture_term() and ui_information_scene_present_term()
- remove app_information_scene and app_ui_scene_from_information_document()
- remove information-scene snapshot plumbing and SDL auto-capture support once
  no runtime flow depends on them

Constraints:
- before deleting anything, verify that rg -n
  "ui_information_scene_present_term\\(|ui_information_scene_capture_term\\(|app_ui_scene_from_information_document\\(|ui_information_scene_present_document\\("
  src only returns matches inside your write set
- preserve any still-active non-document information-scene behavior only if you
  can prove it is still needed; otherwise delete dead bridge code completely
- keep build integrity and avoid widening the write set

Validation:
- run py -3 tools/ui_debt_audit.py --check
- confirm the rg above returns no matches in src
- summarize any intentionally retained bridge code and why
```

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

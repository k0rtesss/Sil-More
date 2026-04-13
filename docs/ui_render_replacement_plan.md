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
  `metarun_build_stats_browser_scene()`.
- The metarun story-statistics submenu family now also has direct semantic SDL
  paths for:
  - completed quest summary
  - active effects
  - blessing exchange
  - blessing threshold
  - difficulty selection
  - metarun history
- `src/quest/quest-ui.c`, `src/ui/ui-story.c`, and `src/ui/ui-death.c` no
  longer call `ui_information_scene_present_term()` on the SDL path.
- `py -3 tools/ui_debt_audit.py --check` passes on this branch.
- Current audit counts on April 13, 2026:
  - `inkey()` call sites: 37 files / 86 matches
  - `screen_save()` + `screen_load()` call sites: 28 files / 187 matches
  - direct `Term_*` render/control calls: 62 files / 1,511 matches
  - `#include "platform-ui.h"`: 0 files / 0 matches
  - `get_sdl_*` / `set_sdl_*` outside platform code: 6 files / 208 matches

### Still Real Debt
- The live term-capture bridge still exists:
  - `ui_information_scene_capture_term()`
  - `ui_information_scene_present_term()`
  - internal auto-capture support in `src/sdl-render.c`
- No user-facing gameplay-module callers remain for
  `ui_information_scene_present_term()`.
- The document bridge still exists:
  - `app_information_scene`
  - `app_ui_scene_from_information_document()`
  - `ui_information_scene_present_document()`
- `ui_information_scene_present_document()` no longer has in-tree callers, but
  the API still exists and should be deleted.
- No non-core callers remain for:
  - `app_ui_scene_from_information_document()`
  - `ui_information_scene_capture_term()`
- The dead information-scene bridge is now isolated to core and renderer files.
- Inventory, equipment, and floor selectors now have their own migration slice:
  - the base selector overlay and direct `i` / `e` enhanced menus are
    semantic in SDL snapshot mode
  - the direct `i` / `e` menus are now overlay-style semantic panels rather
    than browser-menu scenes
  - the snapshot selector path now owns its own semantic blocking loop in
    `src/object/object-ui-select.c`
  - and the remaining item-family debt is now concentrated in:
    - isolated legacy fallback in `src/object/object-ui-select.c`
    - shared term-era display helpers in `src/object/object-ui-display.c`
    - bespoke side flows in `src/cmd/item/cmd-item-activate.c`
    - pickup-from-pile flow in `src/cmd/item/cmd-pickup.c`
- Remaining late bespoke workflow debt is concentrated in:
  - `src/metarun.c` side or narrative flows outside the semantic
    story-statistics or history surfaces
  - `src/birth.c`
  - `src/ui/smithing/ui-smithing-screen.c`
  - `src/blitz.c`
- Some UI families still derive behavior or layout from `Term->wid`,
  `Term->hgt`, `screen_save()`, `screen_load()`, or blocking `inkey()` loops.

## Reality Check
- Older plan text that says tabs, minimap, browser or list-detail shells, or
  character-sheet widgets are still missing is stale.
- The current blocker is no longer "invent the shared widgets first."
- The current blocker is no longer "migrate the document producers first".
- The welcome screen, the main story-statistics screen, the metarun
  story-statistics submenu family, and the quest or story or death
  `present_term()` slice are no longer part of the remaining debt.
- The current blocker is now:
  - deleting the dead information-scene bridge core
  - finishing the remaining item side loops and isolated fallback paths
  - finishing the late bespoke workflow families such as birth, smithing,
    blitz, and remaining metarun side flows

## Next Slices
### Slice: Delete The Dead Information-Scene Bridge
Goal:
- delete the dead document and live-term capture bridge now that gameplay
  modules no longer call it

Why this is next:
- `ui_information_scene_present_document()` has no callers
- `app_ui_scene_from_information_document()` has no non-core callers
- `ui_information_scene_capture_term()` and
  `ui_information_scene_present_term()` have no non-core callers
- the remaining bridge is isolated to core and renderer files

Primary targets:
- `src/ui/ui-information-scene.c`
- `src/ui/ui-information-scene.h`
- `src/app/app-scene-information.c`
- `src/app/app-scene-information.h`
- `src/app/app-session.c`
- `src/app/app-session.h`
- `src/app/app-ui.c`
- `src/app/app-ui.h`
- `src/sdl-render.c`
- `src/sdl-scene-information.c`
- `src/externs.h`

Approach:
- remove `ui_information_scene_present_document()`
- remove `ui_information_scene_capture_term()` and
  `ui_information_scene_present_term()`
- remove `app_information_scene` and
  `app_ui_scene_from_information_document()`
- remove the information-scene snapshot plumbing and SDL auto-capture support
  once no runtime flow depends on them

Exit when:
- `rg -n "ui_information_scene_present_document\\(" src` returns no matches
- `rg -n "app_ui_scene_from_information_document\\(" src` returns no matches
- `rg -n "ui_information_scene_capture_term\\(|ui_information_scene_present_term\\(" src`
  only returns no matches
- no SDL runtime path depends on information-scene document bridging or live
  term capture

### Slice: Finish Remaining Item Side Loops
Goal:
- finish the remaining item-family side loops and isolated fallback paths now
  that the main SDL snapshot selector path is semantic

Status:
- In progress.
- Current chunk landed:
  - `src/object/object-ui-select.c` snapshot mode now owns semantic scene
    presentation in `item_selector_run_snapshot_loop()`
  - legacy `show_inven_enhanced()` and `show_equip_enhanced()` now forward to
    the semantic snapshot menu path in `src/object/object-ui-enhanced.c`
  - `src/cmd/item/cmd-item-core.c` direct inventory and equipment entry points
    already route through the semantic snapshot menus

Primary write set:
- `src/object/object-ui-display.c`
- `src/cmd/item/cmd-item-activate.c`
- `src/cmd/item/cmd-pickup.c`
- optional cleanup-only pass in `src/object/object-ui-select.c` if removing the
  isolated legacy fallback is safe

Next big chunks:
- remove or isolate the remaining term-era side flows that are still used in
  normal SDL play
- keep shared display helpers only where they are still needed for true legacy
  fallback or bespoke flows
- avoid regressing the semantic SDL snapshot selector path

Exit when:
- the remaining item-family raw loops are either gone or clearly isolated to
  true fallback or bespoke workflows
- normal SDL play no longer routes item side flows through term-era chooser
  loops

## After That
### Slice: Finish Bespoke Workflows
- remaining likely late movers:
  - metarun side or narrative flows outside the semantic story-statistics and
    history surfaces
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
  - `src/metarun.c` main story-statistics screen
  - `src/ui/ui-character-screen.c`
  - `src/init2.c` welcome screen
- Old Phase 2 bridge-work slice is now effectively done:
  - no gameplay-module callers remain for
    `ui_information_scene_present_term()`
  - no non-core callers remain for
    `ui_information_scene_capture_term()`
  - no non-core callers remain for
    `app_ui_scene_from_information_document()`
  - the metarun story-statistics submenu family is semantic on the SDL path
  - quest typewriter, story, and death no longer rely on live term capture on
    the SDL path
- The remaining item-family work is now mostly isolated side loops and
  fallback paths, not the main semantic selector path.

### Next Parallel Slice: Launch In Parallel Now
These write sets are disjoint and can run at the same time.

#### Agent N2-A: Delete The Dead Information-Scene Bridge
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

#### Agent N2-B: Remaining Item Side Loops
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
- keep shared display helpers only where they are still needed as shared
  helpers or true fallback
- focus on raw-loop owners such as activation choosers and pickup-from-pile
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

#### Agent N2-C: Item Selector Legacy Fallback Cleanup
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/object/object-ui-select.c

Goal:
- keep the semantic SDL snapshot selector path
- remove or further isolate the remaining non-snapshot legacy overlay fallback
  in this file if that is safe
- do not change the landed snapshot loop behavior

Constraints:
- preserve inventory/equipment/floor selection behavior, highlight logic,
  compare or verify side flows, and fallback behavior
- do not edit object-ui-enhanced.c, object-ui-display.c, or cmd/item/*
- if removing the legacy fallback is risky, isolate it more cleanly and report
  the remaining boundary rather than forcing deletion

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which selector-owned legacy fallback remains, if any, and why
```

#### Agent N2-D: Metarun Side And Narrative Workflows
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/metarun.c

Goal:
- leave the already-semantic story-statistics screen and semantic submenu
  family alone
- focus only on the remaining metarun side or narrative workflows that still
  own blocking term loops outside those landed surfaces
- isolate or migrate those remaining bespoke metarun flows as appropriate

Constraints:
- do not regress the semantic stats screen, blessing exchange, threshold,
  difficulty, active-effects, quest-summary, or metarun-history surfaces
- do not edit bridge/core files
- if a flow is truly late-stage bespoke, isolate it clearly and report it

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which metarun flows remain legacy after your change
```

#### Agent N2-E: Birth Workflow
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/birth.c

Goal:
- start or continue migrating the birth workflow off term-owned blocking loops
- preserve current birth behavior, prompts, paging, and return-to-main-menu
  behavior
- use semantic UI where safe, but do not destabilize gameplay setup

Constraints:
- do not edit main-menu, bridge/core, or unrelated gameplay files
- if the flow is too large to finish cleanly, carve out one coherent semantic
  surface and isolate the remaining legacy boundary

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what part of the birth workflow is still term-owned after your
  change
```

#### Agent N2-F: Smithing Workflow
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/ui/smithing/ui-smithing-screen.c

Goal:
- start or continue migrating the smithing workflow off term-owned blocking
  loops
- preserve smithing behavior, prompts, dense lists, and action routing
- use semantic UI where safe, but do not regress smithing calculations or state

Constraints:
- do not edit gameplay calculation code outside smithing UI unless a tiny glue
  change is strictly required
- if the flow is too large to finish cleanly, carve out one coherent semantic
  surface and isolate the remaining legacy boundary

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what part of smithing remains term-owned after your change
```

#### Agent N2-G: Blitz Workflow
```text
Use model gpt-5.4 with xhigh reasoning.

You are not alone in the codebase. Do not revert edits by other agents. You may
spawn read-only subagents for grep or code reading. If you delegate code edits,
keep the write set disjoint and within your owned files.

Write set:
- src/blitz.c

Goal:
- migrate the remaining blitz UI off term-owned blocking loops where safe
- preserve blitz behavior, prompts, and summary screens
- isolate any remaining legacy blitz boundary clearly if a full migration is
  too large for one slice

Constraints:
- do not edit unrelated metarun, bridge/core, or story files
- preserve gameplay behavior and result flow

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what part of blitz remains term-owned after your change
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

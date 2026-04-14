# UI Render Replacement Plan

Status: current on April 14, 2026. This is the execution document for SDL
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

## State Snapshot On 2026-04-14
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
- `ui_information_scene_present_ui()` remains as the SDL presentation entry
  point for already-built semantic `app_ui_scene` payloads.
- The dead information-scene bridge core is now gone from `src/`:
  - `ui_information_scene_present_document()`
  - `ui_information_scene_capture_term()`
  - `ui_information_scene_present_term()`
  - `app_ui_scene_from_information_document()`
  - `app_information_scene`
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
- Current audit counts on April 14, 2026:
  - `inkey()` call sites: 33 files / 50 matches
  - `screen_save()` + `screen_load()` call sites: 20 files / 72 matches
  - direct `Term_*` render/control calls: 57 files / 860 matches
  - `#include "platform-ui.h"`: 0 files / 0 matches
  - `get_sdl_*` / `set_sdl_*` outside platform code: 6 files / 166 matches

### Still Real Debt
- `py -3 tools/ui_debt_audit.py --check` passing only means the current tree is
  below the frozen UI0 baseline from April 2, 2026. It does not mean the
  remaining debt is small in absolute terms.
- The audit measures all `src/**/*.c` and `src/**/*.h`, not just the normal
  SDL runtime path, so legacy ownership and compatibility code still contribute
  to the visible totals.
- The dead information-scene bridge is no longer the dominant plan-level
  blocker.
- The old browser-heavy family is no longer the dominant multi-file blocker:
  - `src/cmd/ui/cmd-ui-knowledge.c` is effectively out of the audit
  - `src/cmd/ui/cmd-ui-abilities.c` is effectively out of the audit
  - `src/cmd/ui/cmd-ui-settings.c` is still a large residual tail:
    - 6 `inkey()` matches
    - 10 `screen_save()` / `screen_load()` matches
    - 79 direct `Term_*` render/control matches
    - 141 `get_sdl_*` / `set_sdl_*` matches outside platform code
- The biggest concentrated remaining cluster is now persistent chrome and
  shared display ownership:
  - `src/ui/ui-status.c`
  - `src/util-prompt.c`
  - `src/melee/melee-combat-display.c`
  - `src/quest/quest-ui.c`
  - `src/ui/ui-character-screen.c`
  - `src/ui/ui-file-viewer.c`
  - `src/object/object-ui-display.c`
  - these still carry substantial term-era layout or rendering debt even where
    their top-level SDL entry path is already semantic
- Menu-transition flash debt still exists:
  - stale previous screens can still flash briefly when opening or switching
    menus
  - the likely owners are previous-snapshot restore paths and
    `screen_save()` / `screen_load()` ownership around semantic menu
    transitions, especially in:
    - `src/cmd/ui/cmd-ui-main-menu.c`
    - `src/util-prompt.c`
    - `src/object/object-ui-select.c`
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
  - residual `src/metarun.c` side or narrative flows outside the semantic
    story-statistics or history surfaces
  - residual `src/birth.c` blitz setup and effect-pick flows
  - residual `src/ui/smithing/ui-smithing-screen.c` control-loop debt
- Some UI families still derive behavior or layout from `Term->wid`,
  `Term->hgt`, `screen_save()`, `screen_load()`, or blocking `inkey()` loops.

## Reality Check
- The UI0-UI8 substrate work is landed. The remaining program is no longer
  "build the architecture"; it is whole-program removal of term-era ownership
  from still-shipped SDL screen families.
- A large amount of user-facing SDL UI is already semantic:
  - help
  - file viewer
  - score pages
  - quest status
  - main character sheet
  - inventory/equipment selector flows
  - look sidebar
  - welcome screen
  - metarun story-statistics and its submenu family
- Older plan text that says tabs, minimap, browser or list-detail shells, or
  character-sheet widgets are still missing is stale.
- The current blocker is no longer "invent the shared widgets first."
- The current blocker is no longer "migrate the document producers first".
- The bridge-deletion story is no longer the pacing item.
- The current blocker is now:
  - migrating persistent chrome and shared display owners off term-era layout
    and rendering contracts
  - finishing the `cmd-ui-settings.c` residual tail
  - removing stale restore or snapshot flashes when opening menus
  - finishing the remaining item/display helper side loops and isolated
    fallback paths
  - finishing the residual bespoke workflow families such as birth, smithing,
    and remaining metarun side flows
- After `p1`, the highest-leverage next slice is now persistent chrome and
  shared display, with a small parallel carry-over for the remaining settings
  tail.

## Next Slices
### Slice: Persistent Chrome And Shared Display (`OVER1-B`)
Goal:
- migrate the remaining persistent chrome and shared display owners off term
  rows and term render helpers on the normal SDL path

Why this is next:
- `p1` already removed most of the browser-heavy family from the audit
- `src/ui/ui-status.c` is now the single largest remaining hotspot
- prompt/chrome ownership still leaks across multiple SDL surfaces and is now
  the main cross-cutting blocker

Primary targets:
- `src/ui/ui-status.c`
- `src/util-message.c`
- `src/util-prompt.c`
- `src/runtime/runtime-game.c`
- `src/app/app-scene-dungeon.c`
- `src/app/app-scene-dungeon.h`
- `src/sdl-scene-dungeon.c`

Approach:
- keep the current Sil chrome presentation, but move ownership to semantic
  widgets instead of term rows
- remove normal SDL-path dependencies on `Term_putstr()`, `Term_erase()`,
  `Term_get_size()`, `screen_save()`, and `screen_load()` for the left rail,
  message strip, and bottom bar
- keep true legacy or compatibility rendering only where it is intentionally
  outside the normal SDL path

Exit when:
- the left rail, message strip, and bottom bar are semantic chrome on the
  normal SDL path
- normal SDL play no longer derives chrome layout from `Term->wid`,
  `Term->hgt`, or saved-screen restore ownership

### Slice: Settings Residual Tail
Goal:
- finish the post-`p1` settings-family tail that still dominates named SDL
  calls outside platform code

Why this is next:
- `cmd-ui-settings.c` is now the only major browser-family file still carrying
  substantial audit debt
- this write set is disjoint from the persistent chrome slice and can run in
  parallel

Primary targets:
- `src/cmd/ui/cmd-ui-settings.c`

Approach:
- finish converting remaining settings subfamilies and editors to semantic SDL
  bodies where safe
- reduce `get_sdl_*` / `set_sdl_*` ownership further behind narrower helpers
- isolate any true legacy-only editor paths clearly instead of leaving them on
  the normal SDL route

Exit when:
- `cmd-ui-settings.c` is no longer the dominant named-SDL hotspot outside
  platform code
- normal SDL settings flows no longer depend on captured-term bodies or
  term-layout ownership

### Slice: Menu And Prompt Transition Cleanup
Goal:
- remove stale previous-screen flashes when opening or switching menus on the
  SDL path
- stop restoring old snapshots or saved screens as a transitional fallback
  between semantic menu surfaces

Why this stays near the top:
- the flash is a visible user-facing regression
- it is consistent with the remaining `screen_save()` / `screen_load()` and
  previous-snapshot restore ownership in menu transition code
- `p1` reduced the main-menu side of this, but `util-prompt.c` and
  `object-ui-select.c` still own transitional debt

Primary targets:
- `src/cmd/ui/cmd-ui-main-menu.c`
- `src/util-prompt.c`
- `src/object/object-ui-select.c`
- any shared restore owner still required after the settings tail and chrome
  work

Approach:
- remove menu-transition restore flashes on the SDL path
- do not preserve or add fallback branches to keep old restore behavior alive
- if a restore path is only there to hide debt, delete it

Exit when:
- opening or switching semantic menus does not briefly show stale welcome,
  background, dungeon, or previous menu content
- no SDL-path menu transition depends on `screen_save()` / `screen_load()` or
  previous-snapshot restore as a bridge between semantic surfaces

## After That
### Slice: Finish Item And Bespoke Families
- finish the remaining item-family side loops and isolated fallback paths in:
  - `src/object/object-ui-display.c`
  - `src/cmd/item/cmd-item-activate.c`
  - `src/cmd/item/cmd-pickup.c`
- remaining likely late movers:
  - `src/metarun.c` side or narrative flows outside the semantic
    story-statistics and history surfaces
  - `src/birth.c`, including the remaining blitz setup and effect-pick UI now
    owned there
  - `src/ui/smithing/ui-smithing-screen.c`
  - any typewriter or heavily scripted workflow still owning its own blocking
    loop

## Parallel Rollout
This is the recommended execution order for the remaining whole-plan work.
Debt-removal rule for every slice in this section:
- do not add, preserve, or widen fallback paths on the SDL path
- if a debt-removal slice cannot delete a fallback cleanly, stop and report the
  blocker instead of keeping the fallback alive
- do not accept stale-screen flashes during menu transitions as an acceptable
  intermediate state

### Phase Status
- The bridge-deletion slice is complete in the working tree.
- The main selector and several document or browser families are already
  semantic on the SDL path.
- `p1` materially reduced the audit:
  - `inkey()`: 67 -> 50
  - `screen_save()` / `screen_load()`: 107 -> 72
  - direct `Term_*`: 1,095 -> 860
  - `get_sdl_*` / `set_sdl_*` outside platform code: 200 -> 166
- Effective `p1` wins:
  - `cmd-ui-knowledge.c` is effectively out of the audit
  - `cmd-ui-abilities.c` is effectively out of the audit
  - `ui-smithing-screen.c` is effectively out of the `Term_*` audit
  - `birth.c`, `ui-death.c`, `metarun.c`, `cmd-ui-main-menu.c`, and
    `cmd-pickup.c` all moved in the right direction
- Partial `p1` carry-over:
  - `cmd-ui-settings.c` still has a large residual tail
  - `util-prompt.c` still owns prompt/menu transition debt
  - `object-ui-display.c` still owns shared term-era item display helpers
- The current whole-plan debt is concentrated in:
  - persistent chrome and shared display owners
  - the remaining `cmd-ui-settings.c` tail
  - menu or prompt transition ownership
  - late item or bespoke workflows

### Recommended Order
- First: Persistent chrome and shared display (`OVER1-B`)
- In parallel: settings residual tail
- Then: Menu and prompt transition cleanup
- Last: item-family remainder plus birth or smithing or remaining metarun side
  flows

### Parallelization Guidance
- Keep one owner on the remaining settings tail.
- Persistent chrome/shared display can now run in parallel with the settings
  tail because the write sets are disjoint.
- Menu/prompt transition cleanup can run after or alongside those slices only
  if `util-prompt.c` ownership is clearly assigned.
- Leave birth, smithing, and the remaining metarun side flows for after the
  common browser, prompt, and chrome patterns are settled.

### Agent Model Policy
- Top-level implementation agents in this rollout should use `gpt-5.4` with
  `xhigh` reasoning.
  - reason: these slices are multi-file UI migrations with high behavior and
    parity risk
- Read-only grep/code-reading subagents should use `gpt-5.4-mini` with `xhigh`
  reasoning.
  - reason: they are fast and cheap for search, tracing, and code-reading work
- Code-edit subagents are allowed only if the delegated write set stays fully
  inside the owning top-level agent's files.

### Launch Batch 1: Launch Now In Parallel
These write sets are disjoint and can run at the same time.

#### Agent P1-A: Settings Browser Cleanup
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents. Avoid
shared test/doc churn unless it is strictly required and still within your
owned files.
Debt-removal rule: do not preserve or add fallback paths on the SDL path. If a
cleanup is blocked by a shared helper outside your write set, stop and report
the blocker instead of widening ownership.

Write set:
- src/cmd/ui/cmd-ui-settings.c

Goal:
- remove SDL-path captured-term body ownership from the settings family
- reduce or isolate get_sdl_*/set_sdl_* usage owned here
- preserve current settings behavior, prompts, tabs, and presentation

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize the remaining inkey()/screen_save()/screen_load()/Term_* debt still
  left in this file after your change
```

#### Agent P1-B: Knowledge Browser Cleanup
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: do not preserve or add SDL-path fallback renderers. If a
shared helper outside your write set is missing, report the blocker instead of
keeping the term-era path alive.

Write set:
- src/cmd/ui/cmd-ui-knowledge.c

Goal:
- migrate the knowledge family further off term-owned body rendering on the SDL
  path
- preserve tabs, browser/detail flow, and detailed recall screens

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which SDL-path knowledge screens, if any, remain term-owned and why
```

#### Agent P1-C: Abilities Browser Cleanup
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: remove SDL-path term-era ownership where safe; do not leave
fallback branches alive "for safety".

Write set:
- src/cmd/ui/cmd-ui-abilities.c

Goal:
- migrate the abilities family further off term-owned body rendering on the SDL
  path
- preserve navigation, paging, details, and ability descriptions

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which abilities screens remain term-owned after your change
```

#### Agent P1-D: Menu And Prompt Transition Cleanup
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: stale-screen flashes are debt, not acceptable behavior. Do
not preserve restore paths on the SDL path just to mask the issue.

Write set:
- src/cmd/ui/cmd-ui-main-menu.c
- src/util-prompt.c
- src/object/object-ui-select.c

Goal:
- remove stale welcome/background/dungeon/previous-menu flashes when opening or
  switching semantic menus
- remove SDL-path screen_save()/screen_load() or snapshot restore ownership that
  is only acting as a transition bridge

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which transitions were flashing and what restore paths you removed
```

#### Agent P1-E: Remaining Item Side Loops
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: do not leave SDL-path item flows on raw chooser loops if
they can be removed or isolated cleanly.

Write set:
- src/object/object-ui-display.c
- src/cmd/item/cmd-item-activate.c
- src/cmd/item/cmd-pickup.c

Goal:
- isolate or remove remaining terminal-era item side loops outside the semantic
  snapshot selector path
- preserve prompts, activation behavior, and pickup-from-pile behavior

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which raw item loops remain and why
```

#### Agent P1-F: Metarun Side And Narrative Workflows
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: leave the already-semantic story-statistics family alone and
focus only on remaining side/narrative debt.

Write set:
- src/metarun.c

Goal:
- isolate or migrate the remaining metarun side/narrative flows that still own
  blocking term loops outside the landed semantic surfaces

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which metarun flows remain legacy after your change
```

#### Agent P1-G: Birth Workflow
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: do not preserve SDL-path fallback ownership if a clean
semantic boundary can be carved out. If the file is too large to finish, land
one coherent boundary and report the remainder.

Write set:
- src/birth.c

Goal:
- continue migrating birth off term-owned blocking loops
- also own the remaining blitz setup/effect-pick debt that lives here now

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what part of birth remains term-owned, including blitz setup/effect
  debt
```

#### Agent P1-H: Smithing Workflow
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: preserve smithing behavior, but do not keep SDL-path term
loops alive just to avoid touching dense UI code.

Write set:
- src/ui/smithing/ui-smithing-screen.c

Goal:
- continue migrating smithing off term-owned blocking loops while preserving
  dense lists, prompts, and action routing

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what part of smithing remains term-owned after your change
```

### Launch Batch 2: Launch Now After `p1`
These are the next top-level slices after the landed `p1` batch.

#### Agent P2-A: Persistent Chrome And Status
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/ui/ui-status.c
- src/util-message.c
- src/runtime/runtime-game.c
- src/app/app-scene-dungeon.c
- src/app/app-scene-dungeon.h
- src/sdl-scene-dungeon.c

Goal:
- migrate left rail, message strip, and bottom-bar ownership further off term
  rows/render helpers on the normal SDL path

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what chrome/prompt/message debt still remains after your change
```

#### Agent P2-B: Settings Tail Cleanup
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/cmd/ui/cmd-ui-settings.c

Goal:
- finish the post-p1 settings tail
- reduce remaining inkey()/screen_save()/screen_load()/Term_* debt in the SDL
  settings route
- reduce get_sdl_*/set_sdl_* ownership further behind narrower helpers

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which settings subflows remain term-owned after your change
```

#### Agent P2-C: Query And Character Helpers
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/cmd/ui/cmd-ui-query.c
- src/ui/ui-character-screen.c

Goal:
- clean up the remaining query/character helper debt after the main browser
  family settles

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which query/character helper flows remain term-owned
```

#### Agent P2-D: File Viewer And Score Detail Screens
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/ui/ui-file-viewer.c
- src/score/score_ui.c

Goal:
- remove remaining term-owned body/layout debt from file-viewer and score
  detail surfaces on the SDL path

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which file-viewer/score flows remain term-owned
```

#### Agent P2-E: Quest And Combat Display Cleanup
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/quest/quest-ui.c
- src/melee/melee-combat-display.c

Goal:
- remove remaining term-era body rendering/layout ownership from quest UI and
  combat display surfaces on the SDL path

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which quest/combat flows remain term-owned
```

### Launch Batch 3: Tail Cleanup
These are lower-volume but still meaningful cleanup slices after Batch 2.

#### Agent P3-A: Look / Nearby / Targeting
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/cmd/ui/cmd-ui-look.c
- src/cmd/ui/cmd-ui-nearby.c
- src/targeting.c
- src/ui/ui-look-sidebar.c

Goal:
- finish removing SDL-path legacy redraw/layout ownership from look/nearby/
  targeting surfaces

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which look/nearby/targeting flows remain term-owned
```

#### Agent P3-B: Object Info And Identify Family
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/obj-info.c
- src/object/object-ui-identify.c

Goal:
- remove remaining SDL-path screen_save()/screen_load() and term-layout debt in
  object info and identify flows

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which object info/identify flows remain term-owned
```

#### Agent P3-C: Dungeon Prompt/Overlay Cleanup
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/dungeon.c
- src/game-event.c
- src/level-generation/level-generation-screen.c

Goal:
- remove remaining SDL-path screen-save/prompt/overlay debt in normal dungeon
  and level-generation flows

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which dungeon/level-generation prompt or overlay debt remains
```

#### Agent P3-D: Spell And Misc Legacy UI Cleanup
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/spell/spell-utility.c
- src/ui/ui-death.c
- src/ui/ui-story.c

Goal:
- remove remaining SDL-path term-owned prompt/document loops in spell utility,
  death, and story-adjacent surfaces

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which spell/death/story flows remain term-owned
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
- For debt-removal slices, do not keep or add fallback paths on the SDL path.
  Delete the debt. If blocked, report the blocker instead of preserving a
  fallback branch.
- Do not accept stale-screen flashes during menu transitions. If opening or
  switching a menu briefly shows welcome, background, dungeon, or previous-menu
  content, treat that as remaining debt to remove.

## Validation
- run `py -3 tools/ui_debt_audit.py --check`
- smoke-test the screen family changed by the slice
- judge parity against the current shipped Sil presentation, not against
  terminal-era width limits

## Success Condition
This work is finished when no shipped SDL UI surface still depends on live
`Term` rendering, term capture, information-scene document bridging, or raw
term-grid UI contracts.

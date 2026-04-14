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
  - `inkey()` call sites: 16 files / 18 matches
  - `screen_save()` + `screen_load()` call sites: 0 files / 0 matches
  - direct `Term_*` render/control calls: 42 files / 369 matches
  - `#include "platform-ui.h"`: 0 files / 0 matches
  - `get_sdl_*` / `set_sdl_*` outside platform code: 3 files / 34 matches

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
  - `src/app/app-scene-dungeon.c`
  - `src/util-prompt.c`
  - `src/util-message.c`
  - `src/runtime/runtime-game.c`
  - `src/melee/melee-combat-display.c`
  - `src/quest/quest-ui.c`
  - `src/ui/ui-character-screen.c`
  - `src/ui/ui-file-viewer.c`
  - `src/object/object-ui-display.c`
  - these still carry substantial term-era layout or rendering debt even where
    their top-level SDL entry path is already semantic
- The remaining blocker is not only "legacy API debt"; it is still active
  grid-based render ownership on the normal SDL path:
  - left rail and bottom bar still render through fixed row/column contracts in
    `src/ui/ui-status.c`
  - prompt and confirm flows still render through terminal cursor and erase
    operations in `src/util-prompt.c`
  - compact layout decisions for dungeon chrome still consult `Term->wid` in
    `src/app/app-scene-dungeon.c`
  - object-display and object-info families still build SDL-visible UI from
    term width, height, and row math
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
  - removing grid-based render ownership from persistent chrome and shared SDL
    overlays
  - finishing the `cmd-ui-settings.c` named-SDL/config tail
  - removing the remaining prompt/transition restore debt
  - removing term-grid ownership from object-display and object-info helper
    families
  - removing dungeon/system overlay and prompt debt outside the browser
    families
  - finishing the remaining item/display helper side loops and isolated
    fallback paths
  - finishing the residual bespoke workflow families such as birth, smithing,
    and remaining metarun side flows
- After `p2` and `p3a`, the highest-leverage next slice is still persistent
  chrome and shared display, but it should now run as one lane in a broader
  parallel batch because several other remaining families have disjoint write
  sets.

## Next Slices
### Slice: Persistent Chrome And Shared Display (`OVER1-B`)
Goal:
- migrate the remaining persistent chrome and shared display owners off term
  rows and term render helpers on the normal SDL path
- remove grid-based render ownership for always-visible SDL chrome instead of
  merely wrapping the existing grid logic

Why this is next:
- `p1` already removed most of the browser-heavy family from the audit
- `p2` removed much of the browser/detail grid rendering from file viewer,
  score, quest, combat display, and character helper surfaces
- `src/ui/ui-status.c` is now the single largest remaining hotspot
- prompt/chrome ownership still leaks across multiple SDL surfaces and is now
  the main cross-cutting blocker
- this is now the main place where normal SDL still behaves like a terminal
  renderer with fixed rows, columns, and compact-width checks

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
- remove normal SDL-path layout decisions that still depend on `Term->wid`,
  `Term->hgt`, or fixed row/column placement for chrome
- keep true legacy or compatibility rendering only where it is intentionally
  outside the normal SDL path

Exit when:
- the left rail, message strip, and bottom bar are semantic chrome on the
  normal SDL path
- normal SDL play no longer derives chrome layout from `Term->wid`,
  `Term->hgt`, or saved-screen restore ownership
- normal SDL chrome is no longer fundamentally a grid renderer with semantic
  data pasted into term rows

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
- `p5` materially reduced the audit again:
  - `inkey()`: 18 -> 14
  - `screen_save()` / `screen_load()`: 0 -> 0
  - direct `Term_*`: 369 -> 220
  - `get_sdl_*` / `set_sdl_*` outside platform code: 95 -> 34
- Effective `p5` wins:
  - `ui-status.c` dropped from 76 direct `Term_*` calls to 6
  - `melee-combat-display.c` dropped from 48 direct `Term_*` calls to 1
  - `cmd-ui-settings.c` dropped from 71 named SDL calls to 14
  - `dungeon.c` dropped further from 10 direct `Term_*` calls to 7
  - `birth.c` and `ui-smithing-screen.c` are effectively out of the `inkey()`
    audit
  - `files.c` remains term-heavy, but is now more clearly utility/legacy-owned
    than a live SDL chrome blocker
- `p4` materially reduced the audit again:
  - `inkey()`: 35 -> 18
  - `screen_save()` / `screen_load()`: 44 -> 0
  - direct `Term_*`: 557 -> 369
  - `get_sdl_*` / `set_sdl_*` outside platform code: 95 -> 95
- Effective `p4` wins:
  - screen-save / screen-load bridge debt is effectively gone from the tree
  - `util-prompt.c` is out of the `Term_*` and saved-screen audit
  - `object-ui-display.c` is effectively out of the audit
  - `obj-info.c`, `object-ui-identify.c`, `spell-utility.c`, `ui-story.c`,
    `ui-death.c`, `runtime-game.c`, `quest-ui.c`, and `quest-varda.c` all
    moved down sharply
  - `cmd-ui-settings.c` is now mostly a named-SDL/config tail rather than a
    renderer-debt file
- `p3a` materially reduced the audit:
  - `inkey()`: 43 -> 35
  - `screen_save()` / `screen_load()`: 60 -> 44
  - direct `Term_*`: 695 -> 557
  - `get_sdl_*` / `set_sdl_*` outside platform code: 151 -> 95
- Effective `p3a` wins:
  - `cmd-ui-look.c` is effectively out of the audit
  - `cmd-ui-nearby.c` is effectively out of the audit
  - `targeting.c` is effectively out of the audit
  - `ui-look-sidebar.c` is effectively out of the audit
  - `cmd-ui-main-menu.c` is effectively out of the grid/render audit
  - `object-ui-select.c` moved down to an isolated input/fallback tail
- `p2` materially reduced the audit again:
  - `inkey()`: 50 -> 43
  - `screen_save()` / `screen_load()`: 72 -> 60
  - direct `Term_*`: 860 -> 695
  - `get_sdl_*` / `set_sdl_*` outside platform code: 166 -> 151
- Effective `p2` wins:
  - `ui-file-viewer.c` is effectively out of the audit
  - `score_ui.c` is effectively out of the audit
  - `quest-ui.c` is mostly off grid-era body rendering now
  - `melee-combat-display.c` moved substantially off term/grid ownership
  - `ui-character-screen.c` and `cmd-ui-query.c` both moved materially
  - `cmd-ui-settings.c` improved again, especially on `screen_save()` /
    `screen_load()`
- `p2` was primarily a browser/detail and document-body cleanup batch.
- `p2` did not land the persistent chrome/status slice:
  - `ui-status.c` is still the top `Term_*` hotspot
  - `util-prompt.c` still owns prompt grid rendering
  - `util-message.c`, `runtime-game.c`, and `app-scene-dungeon.c` still carry
    normal-SDL chrome/grid assumptions
- Effective `p2` landing status by planned slices:
  - `P2-B` settings tail: partial but meaningful progress
  - `P2-C` query/character helpers: largely landed
  - `P2-D` file-viewer/score detail: largely landed
  - `P2-E` quest/combat display: largely landed
  - `P2-A` persistent chrome/status: partially landed structurally, but still
    the main remaining chrome/grid block
- Effective `p4` landing status by planned slices:
  - `P4-E` object info/query recall: largely landed
  - `P4-F` item display helper removal: largely landed
  - `P4-G` dungeon/system overlay cleanup: partially landed
  - `P4-H` spell/story/death tail: largely landed
  - `P4-D` settings named-SDL tail: partial but meaningful progress
  - `P4-A` / `P4-B` / `P4-C`: partially landed, but the status/chrome block is
    still the main remaining normal-SDL grid-render owner
- Effective `p5` landing status by planned slices:
  - `P5-A` status/chrome contract finish: partial but major progress
  - `P5-B` combat-roll/status-adjacent cleanup: largely landed
  - `P5-C` settings/config tail: major progress, but still active
  - `P5-D` runtime/file tail: partial; utility-style legacy render still exists
  - `P5-E` dungeon/world prompt-overlay tail: partial
  - `P5-F` bespoke workflow tail: partial
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
  - the remaining status/chrome tail in `ui-status.c`
  - the remaining normal-SDL grid-render owners in `dungeon.c` and utility/UI
    support files such as `files.c`
  - the remaining `cmd-ui-settings.c` and `ui-help.c` named-SDL/config tail
  - the remaining character/quest presentation tail
  - late item or bespoke workflows

### Recommended Order
- First: finish the remaining status/chrome tail and delete dead helpers in that
  family
- In parallel: settings/help named-SDL/config tail
- In parallel: character/quest presentation tail
- In parallel: runtime/file utility tail
- Then: dungeon/world prompt and overlay cleanup
- Last: item-family remainder plus birth or smithing or remaining metarun side
  flows

### Parallelization Guidance
- Keep one owner on the remaining status/chrome tail.
- Keep one owner on the remaining settings/help config tail.
- Treat character/quest presentation as distinct from always-visible chrome.
- Treat runtime/files cleanup as distinct from dungeon-scene chrome work.
- For every slice below, delete dead legacy helpers, adapters, and fallback
  branches inside the owned write set once they are no longer used. Do not
  leave dead code behind just because the live path no longer reaches it.
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

### Launch Batch 2: Remaining After `p2`
These are the remaining top-level slices from the old post-`p1` batch.
- Current status after `p2` and `p3a`:
  - old `P2-B` is still active, but substantially smaller
  - old `P2-F` is largely landed
  - old `P2-A1` / `P2-A2` / `P2-A3` are partially landed but still active
  - do not separately launch the older `P1-D` prompt cleanup slice at the same
    time as the prompt/message batch

#### Agent P2-A1: Status Rail And Chrome Data Contract
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/ui/ui-status.c
- src/app/app-scene-dungeon.c
- src/app/app-scene-dungeon.h

Goal:
- remove left-rail and always-visible chrome data ownership from term-grid
  rendering in `ui-status.c`
- expose semantic chrome data through the dungeon-scene contract instead of
  assuming fixed term rows/columns
- remove normal-SDL chrome layout decisions that still depend on `Term->wid` or
  `Term->hgt` in the scene contract

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what status/chrome grid-render debt still remains after your change
```

#### Agent P2-A2: Prompt, Message, And Runtime Footer
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/util-prompt.c
- src/util-message.c
- src/runtime/runtime-game.c

Goal:
- remove normal-SDL prompt/message/footer ownership from terminal cursor, erase,
  and saved-screen operations
- preserve gameplay prompts, confirmations, recalls, and startup/footer flows
- delete grid-based prompt rendering on the SDL path instead of wrapping it

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what prompt/message/runtime grid-render debt still remains after
  your change
```

#### Agent P2-A3: SDL Dungeon Chrome Renderer
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/sdl-scene-dungeon.c

Goal:
- consume semantic chrome data in the SDL dungeon renderer without reintroducing
  terminal row/column assumptions
- keep dungeon scale and overlay/chrome scale independent
- preserve the shipped Sil presentation while deleting grid-era placement logic

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what SDL-side chrome/grid rendering still remains after your change
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

#### Agent P2-F: Main Menu And Object Selector Transition Cleanup
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/cmd/ui/cmd-ui-main-menu.c
- src/object/object-ui-select.c

Goal:
- remove stale transition flashes and saved-screen bridging that still remain in
  main-menu/object-selector flows
- preserve prompts and behavior, but do not touch `util-prompt.c` in this slice

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which stale-screen transitions remain after your change
```

### Launch Batch 3: Tail Cleanup
These are lower-volume but still meaningful cleanup slices after Batch 2.
- Current status after `p2` and `p3a`:
  - `P3-A` is largely landed
  - `P3-B`, `P3-C`, and `P3-D` still look valid as separate tails

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

### Launch Batch 4: Launch Now In Parallel
These are the next recommended top-level agents after the landed `p2` work and
the largely-landed `p3a` look/nearby/targeting slice. The write sets are
disjoint.

#### Agent P4-A: Status Rail And Chrome Contract
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/ui/ui-status.c
- src/app/app-scene-dungeon.c
- src/app/app-scene-dungeon.h

Goal:
- remove left-rail and always-visible chrome data ownership from term-grid
  rendering
- replace fixed row/column chrome assumptions with semantic chrome payloads
- remove normal-SDL compact-layout decisions that still depend on `Term->wid`
  or `Term->hgt` in the scene contract

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what status/chrome grid-render debt still remains after your change
```

#### Agent P4-B: Prompt, Message, And Runtime Footer
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/util-prompt.c
- src/util-message.c
- src/runtime/runtime-game.c

Goal:
- remove normal-SDL prompt/message/footer ownership from terminal cursor, erase,
  and saved-screen operations
- preserve gameplay prompts, confirmations, recalls, and startup/footer flows
- delete grid-based prompt rendering on the SDL path instead of wrapping it

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what prompt/message/runtime grid-render debt still remains after
  your change
```

#### Agent P4-C: SDL Dungeon Chrome Renderer
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/sdl-scene-dungeon.c

Goal:
- consume semantic chrome data in the SDL dungeon renderer without
  reintroducing terminal row/column assumptions
- keep dungeon scale and overlay/chrome scale independent
- preserve the shipped Sil presentation while deleting grid-era placement logic

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what SDL-side chrome/grid rendering still remains after your change
```

#### Agent P4-D: Settings Named-SDL Tail
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
- finish the remaining settings/config tail
- reduce named SDL getters/setters further behind narrower helpers
- remove remaining SDL-path term-width/height branching where possible

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which settings subflows still depend on grid-era assumptions after
  your change
```

#### Agent P4-E: Object Info And Query Recall Family
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
- src/cmd/ui/cmd-ui-query.c

Goal:
- remove remaining SDL-path screen_save()/screen_load(), scratch-term, and
  term-layout debt in object info, identify, and query recall flows
- preserve detailed object prose and recall behavior

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which object-info/query flows remain term-owned
```

#### Agent P4-F: Item Display Helper Grid Removal
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.

Write set:
- src/object/object-ui-display.c
- src/cmd/item/cmd-item-activate.c
- src/cmd/item/cmd-pickup.c

Goal:
- remove the remaining shared term-grid item display helpers from normal SDL
  play
- preserve activation behavior, pickup behavior, and item descriptions

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which item-display helpers remain truly legacy after your change
```

#### Agent P4-G: Dungeon/System Overlay Cleanup
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
- isolate any true non-SDL fallback paths clearly

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which dungeon/system overlay debt still remains after your change
```

#### Agent P4-H: Spell/Story/Death Tail
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
- preserve current behavior and prose presentation

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which spell/death/story flows remain term-owned
```

### Launch Batch 5: Launch Now After `p4`
These are the next recommended top-level agents after the landed `p4` batch.
The write sets are disjoint.

#### Agent P5-A: Status Rail And Chrome Contract Finish
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: remove dead legacy helpers and fallback branches inside your
write set once the semantic path replaces them.

Write set:
- src/ui/ui-status.c
- src/app/app-scene-dungeon.c
- src/app/app-scene-dungeon.h
- src/sdl-scene-dungeon.c

Goal:
- finish removing left-rail and always-visible chrome ownership from term-grid
  rendering
- replace remaining fixed row/column chrome assumptions with semantic chrome
  payloads and SDL-side logical-pixel layout
- keep dungeon scale and overlay/chrome scale independent

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what status/chrome grid-render debt still remains after your change
```

#### Agent P5-B: Combat Roll And Status-Adjacent Overlay Cleanup
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: remove dead term-era display helpers in your write set once
their SDL path is semantic.

Write set:
- src/melee/melee-combat-display.c

Goal:
- remove remaining term-grid ownership from combat-roll and status-adjacent
  overlays on the SDL path
- preserve current combat-roll content and placement behavior while moving
  layout ownership off the terminal grid

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which combat-display paths remain term-owned after your change
```

#### Agent P5-C: Settings Named-SDL And Layout Tail
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: delete dead wrappers/helpers you make obsolete. Do not leave
parallel config access layers behind in your write set.

Write set:
- src/cmd/ui/cmd-ui-settings.c

Goal:
- finish the remaining settings/config tail
- reduce named SDL getters/setters further behind narrower helpers
- remove remaining SDL-path term-width/height branching where possible

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which settings subflows still depend on grid-era assumptions after
  your change
```

#### Agent P5-D: Runtime And File/UI Tail
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: remove dead runtime/file presentation helpers in your write
set when they are no longer needed.

Write set:
- src/files.c
- src/util-message.c
- src/init2.c

Goal:
- remove remaining runtime/file UI ownership from live terminal rendering where
  those surfaces still affect shipped SDL behavior
- preserve character dump, screenshot capture, and startup behavior
- isolate any true non-SDL/utility-only code clearly

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which runtime/file presentation paths remain term-owned
```

#### Agent P5-E: Dungeon And World Prompt/Overlay Tail
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: remove dead overlay/prompt branches in your write set after
the SDL path is cleaned up.

Write set:
- src/dungeon.c
- src/game-event.c
- src/level-generation/level-generation-screen.c

Goal:
- remove remaining SDL-path prompt/overlay debt in normal dungeon and
  level-generation flows
- reduce inkey()/Term_* ownership where it still leaks into shipped SDL play

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize which dungeon/system overlay debt still remains after your change
```

#### Agent P5-F: Bespoke Workflow Tail
```text
Use model gpt-5.4 with xhigh reasoning.

Subagent support:
- you may spawn read-only grep/code-reading subagents using gpt-5.4-mini with
  xhigh reasoning
- if you delegate code edits, keep them fully inside your write set

You are not alone in the codebase. Do not revert edits by other agents.
Debt-removal rule: remove dead legacy helpers/fallbacks in your write set when
their semantic path lands.

Write set:
- src/birth.c
- src/ui/smithing/ui-smithing-screen.c
- src/metarun.c

Goal:
- continue shrinking the remaining bespoke workflow tails now that the common
  SDL chrome/document/browser families are much further along
- preserve behavior and prompts while removing remaining term-owned blocking UI

Validation:
- run py -3 tools/ui_debt_audit.py --check
- summarize what part of birth/smithing/metarun remains term-owned after your
  change
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
- When a slice replaces a legacy path, also delete dead helpers, dead shims,
  and dead fallback code in the owned write set. Do not leave orphaned legacy
  code behind after the live SDL path no longer uses it.
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

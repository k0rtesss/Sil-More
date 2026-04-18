# Post-Finish-Line Parallel Refactor Plan

Status date: April 18, 2026.

This document follows the completed finish-line modernization program in
`docs/finish_line_modernization_parallel_plan.md`. It turns the next round of
recommended refactors into a parallel execution plan that multiple agents and
subagents can carry out without stepping on the same merge points.

It does not replace `docs/modernization_mobile_roadmap.md`. It narrows the
next execution wave to the remaining ownership buckets, audit blind spots, and
high-value dependency cleanup.

## Purpose
- Finish the monolith reduction work that the finish-line program intentionally
  left for later waves.
- Close the current modernization-audit blind spots so the tree cannot go
  green while compiled code escapes the checks.
- Keep save, score, and metarun compatibility intact while splitting
  persistence-heavy files.
- Support parallel execution by a main integrator, lane leads, and bounded
  subagents with explicit write sets and sequencing rules.

## Current Baseline
- The finish-line plan is complete in the live tree.
- This post-finish-line program completed its final end-state closure in the
  live tree on April 18, 2026.
- `src/externs.h` is down to 181 lines with 36 `extern` declarations and 0
  direct include sites.
- `src/variable.c` is gone from the live tree.
- The checked-in modernization audit now scans normal source files plus
  compiled `*.inc` payloads reachable from the source tree, and the current
  baseline records zero compiled code-bearing `*.inc` files, zero non-platform
  SDL header leakage, and zero non-platform SDL I/O leakage.
- A checked-in source size audit now freezes oversized non-vendor source files
  against a committed baseline.
- Quick audit counts measured on April 18, 2026 show:
  - 0 non-vendor `src/**/*.c` files above 2500 lines
  - 0 non-vendor `src/**/*.c` files above 3500 lines
  - 0 non-vendor `src/**/*.c` files above 4000 lines
  - 9 non-vendor `src/**/*.h` files above 250 lines
  - 2 non-vendor `src/**/*.h` files above 1000 lines
- Remaining oversized non-vendor hotspots are headers:
  - `src/defines.h`: 4012 lines
  - `src/types.h`: 1656 lines
  - `src/level-generation/level-generation-internal.h`: 412 lines
  - `src/metarun.h`: 352 lines
  - `src/config.h`: 329 lines

## Continuation Result
- The continuation program closes the remaining quantitative gaps from the
  first ratchet snapshot.
- The final continuation metrics are:
  - 0 direct `#include "externs.h"` sites
  - 0 oversized non-vendor `src/**/*.c` files above 2500 lines
  - 0 compiled code-bearing `*.inc` files
  - 0 non-platform SDL header leakage sites
  - 0 non-platform SDL I/O leakage sites
- No follow-up rescope plan is required for the continuation targets tracked
  by this document.

## End State
- No compiled code-bearing `*.inc` files remain in the live tree.
- The modernization audit covers every compiled source form that can hide
  dependency or boundary regressions.
- A checked-in size audit enforces the post-finish-line monolith targets.
- The next-tier ownership buckets are split into subsystem-local files with
  narrow headers.
- Shared semantic-scene and browser scaffolding stops being reimplemented in
  each large UI flow.
- `src/externs.h` shrinks again as the remaining large legacy files publish
  narrower local headers.

## Scope
### In Scope
- `metarun` de-`*.inc` conversion and persistence/UI split
- `cave` split
- `runtime-dungeon` split
- `cmd-interact-chest` split
- `app-scene-birth` split
- `ui-smithing-screen` split
- `cmd-ui-knowledge` split
- `load/save` sectionization
- modernization audit hardening
- size-audit enforcement
- shared UI scene/browser helper extraction
- `externs.h` reduction follow-up

### Explicitly Out Of Scope
- gameplay balance changes
- save-format or score-format breakage without explicit version gates
- reopening completed terminal-removal work
- large algorithm rewrites where extraction and ownership cleanup are enough
- mobile-specific layout work unless a refactor directly touches the boundary

## Guardrails
- Do not add new code-bearing `*.inc` files.
- Do not break save, score, or metarun compatibility without explicit version
  gates and migration logic.
- Do not add new declarations to `src/externs.h`.
- Do not introduce new process-wide globals unless no narrower ownership exists.
- Do not let parallel lanes edit the same shared merge points in the same wave.
- Do not move non-platform file I/O back toward SDL-shaped interfaces.
- Keep Android and future iOS constraints intact when moving file, path, or UI
  boundary code.

## Quantitative Targets
- zero compiled code-bearing `*.inc` files
- modernization audit covers `*.c`, `*.h`, and any compiled include payloads or
  else explicitly fails on code-bearing `*.inc`
- checked-in size audit for source files, with vendor code allowlisted
- fewer than 10 non-vendor `src/**/*.c` files above 2500 lines after the first
  two execution waves
- no non-vendor gameplay or UI source file above 2500 lines at program close
- fewer than 15 direct `#include "externs.h"` sites by the end of the program

## Shared Merge Points Reserved For The Integrator
- root `CMakeLists.txt`
- `tools/modernization_audit.py`
- `tests/modernization_audit_baseline.json`
- new size-audit tool or size-audit baseline file
- `src/externs.h`
- any repo-wide shared UI helper header introduced for multiple lanes

## Agent Execution Model
### Roles
- Main integrator:
  - owns all shared merge points
  - freezes lane contracts
  - lands final `CMakeLists.txt` updates
  - merges audit, baseline, and repo-wide helper changes
- Lane lead agent:
  - owns one subsystem lane
  - defines lane-local headers and file-family boundaries
  - reviews and merges its subagents' work before handing the lane to the
    integrator
- Subagents:
  - operate only inside the write set assigned by the lane lead
  - do not edit shared merge points
  - do not move files outside the lane's folder set

### Recommended Concurrency
- Safe peak concurrency:
  - 1 main integrator
  - up to 4 active lane leads at once in UI-heavy waves
  - up to 3 active lane leads at once in persistence-heavy waves
  - up to 2 subagents per active lane
- Do not run two waves at once if both want the same shared helper contract.
- Do not run the level-generation lanes at maximum fan-out; they are too tightly
  coupled for broad parallel churn.

## Wave Plan
### Wave 0: Serial Audit And Contract Foundation
Owner: main integrator only.

Status: complete in the live tree on April 17, 2026.

Deliverables:
- Extend the modernization audit to cover compiled `*.inc` usage directly, or
  fail when code-bearing `*.inc` files exist outside an allowlist.
- Add a checked-in size audit with an allowlist for vendor code only.
- Record the first post-finish-line oversize baseline.
- Freeze the rule that new compiled logic must live in normal subsystem
  translation units, not `*.inc`.
- Publish any staging headers needed by later lanes so they can split large
  files without reopening contract questions mid-wave.

Write set:
- `tools/`
- `tests/`
- root `CMakeLists.txt`
- any new shared helper headers needed to unblock later lanes

Validation:
- `.\build-incremental.ps1`
- `ctest --preset test-standard`
- modernization audit
- new size audit

Completion notes:
- `tools/modernization_audit.py` now follows compiled `*.inc` payloads and
  measures the previously-hidden `metarun` SDL I/O debt.
- `tools/source_size_audit.py` and
  `tests/source_size_audit_baseline.json` are checked in and registered in
  CTest as `sil_source_size_audit`.
- No additional shared staging headers were required to unblock Wave 1; the
  lane-local headers can land with their owning subsystem splits.

### Wave 1A: Parallel UI And Interaction Lanes
Run these four lanes together after Wave 0 contracts are frozen.

#### Lane A: `Metarun`
- Lead owns:
  - `src/metarun/*`
  - `src/metarun.h`
- Subagents:
  - persistence and recovery
  - score import/export and history parsing
  - stats/history/blessing UI flows
- Deliverables:
  - replace the `metarun` `*.inc` files with normal `.c` and `.h` ownership
  - isolate persistence, score parsing, state, and UI/history concerns
  - route non-platform file handling through repo-owned file abstractions or a
    clearly bounded filesystem helper
  - leave `src/metarun/metarun.c` as orchestration only

#### Lane B: `Cmd-interact-chest`
- Lead owns:
  - `src/cmd/world/cmd-interact-chest.c`
  - new `src/cmd/world/cmd-interact-chest-*.c`
  - optional `src/cmd/world/cmd-interact-chest.h`
- Subagents:
  - chest release and trap core
  - skeleton loot preparation
  - skeleton note and hint generation
- Deliverables:
  - stop mixing chest mechanics, loot generation, and skeleton narrative logic
    in one file
  - keep command entrypoints as thin orchestration

#### Lane C: `Smithing UI`
- Lead owns:
  - `src/ui/smithing/*`
- Subagents:
  - create, enchant, and reforge menus
  - artefact flow and snapshot scenes
  - shared presentation helpers and menu models
- Deliverables:
  - reduce `ui-smithing-screen.c` to top-level flow and scene orchestration
  - split shared menu builders and artefact-specific scene logic into separate
    files

#### Lane D: `Knowledge`
- Lead owns:
  - `src/cmd/ui/cmd-ui-knowledge.c`
  - new `src/cmd/ui/cmd-ui-knowledge-*.c`
  - optional `src/cmd/ui/cmd-ui-knowledge.h`
- Subagents:
  - object and artefact collection
  - supplies browser and export flows
  - root menu and page routing
- Deliverables:
  - split the file by knowledge page family
  - keep shared scene plumbing lane-local for now; do not create the repo-wide
    helper until Wave 2

Constraints:
- Lane A is the only Wave 1A lane allowed to touch `src/metarun.h`.
- Lane C and Lane D must not both try to land the same repo-wide UI helper.
- The integrator remains the only owner of audit and baseline files.

### Wave 1B: Parallel Core Flow Lanes
Start after Wave 1A contracts are stable.

#### Lane E: `Cave`
- Lead owns:
  - `src/cave/*`
- Subagents:
  - style lists, depth colors, and level-style state
  - projection, LOS, scatter, and geometry helpers
  - tracking, disturbance, and lighting-driven UI state
- Deliverables:
  - reduce `src/cave/cave.c` to cave-state orchestration
  - publish stable cave-local headers for later consumers

#### Lane F: `Runtime-dungeon`
- Lead owns:
  - `src/runtime/runtime-dungeon.c`
  - new `src/runtime/runtime-dungeon-*.c`
  - optional `src/runtime/runtime-dungeon.h`
- Subagents:
  - input polling and runtime snapshot publication
  - player-turn loop and action processing
  - story, death, and fullscreen presentation flows
- Deliverables:
  - separate the gameplay loop from presentation and banner/UI logic
  - keep `play_game()` and top-level runtime orchestration readable

#### Lane G: `Birth`
- Lead owns:
  - `src/app/app-scene-birth.c`
  - new `src/app/app-scene-birth-*.c`
  - `src/app/app-scene-birth.h`
- Subagents:
  - player reset, start items, and starting artefacts
  - semantic scene/prompt flow
  - Blitz auto-build and auto-assignment helpers
- Deliverables:
  - split player setup, presentation, and Blitz helpers by ownership
  - leave the remaining main file as flow control only

Constraints:
- Lane E should publish stable cave-local contracts before any later
  `level-generation` or runtime follow-up uses them.
- Lane F and Lane G may duplicate small scene helpers temporarily; shared helper
  extraction waits for Wave 2.

### Wave 2: Serial Shared-Helper Integration
Owner: main integrator, with bounded helper subagents on disjoint adapter
files once the contracts are clear.

Deliverables:
- Extract shared semantic browser/fullscreen helper scaffolding from:
  - `metarun`
  - `knowledge`
  - `birth`
  - `runtime-dungeon`
  - `score_ui`
  - other touched informational scenes when the helper is already a clean fit
- Keep the helper inside `src/ui/` or `src/app/` rather than leaking platform
  details into gameplay modules.
- Normalize repeated present/wait/accept/back loops once the first lane-local
  variants have proven out.

Write set:
- repo-wide shared UI helper files
- touched caller adapters in the subsystems that already landed

Validation:
- `.\build-incremental.ps1`
- `ctest --preset test-standard`
- smoke-test metarun stats/history, knowledge, birth, and affected modal scenes

### Wave 3: Parallel Persistence And Dependency Cleanup
Run these lanes together after Wave 2 stabilizes.

#### Lane H: `Save/Load`
- Lead owns:
  - `src/fs/load.c`
  - `src/fs/save.c`
  - new `src/fs/load-*.c`
  - new `src/fs/save-*.c`
  - lane-local filesystem headers only
- Subagents:
  - primitive readers/writers and common section helpers
  - player, inventory, and randart sections
  - dungeon, notes, partition metadata, and Blitz sections
- Deliverables:
  - split save/load ownership by section instead of leaving one giant reader and
    one giant writer bucket
  - keep version gating and corrupt-input handling behavior intact

#### Lane I: `Externs Drain`
- Lead owner:
  - main integrator for `src/externs.h`
- Subagents:
  - `cave`, `runtime`, and `metarun` local header moves
  - `fs`, `score`, and UI local header moves
  - `object`, `monster`, `melee`, and `spell` local header moves
- Deliverables:
  - move declarations into narrower owning headers in disjoint subsystem
    folders
  - let the integrator perform the final `src/externs.h` removals serially

#### Lane J: `Audit Completion`
- Lead owner:
  - main integrator
- Deliverables:
  - refresh the architecture baseline after the `metarun` and size-audit
    changes are real
  - make sure the audit now reflects compiled truth rather than only scanned
    file suffixes

Constraints:
- Lane H is the only lane allowed to touch `src/fs/load.c` and `src/fs/save.c`.
- Lane I subagents may create local headers in their folders, but only the
  integrator edits `src/externs.h`.
- Lane J must wait for the `metarun` and size-audit changes to land before
  freezing new baselines.

### Wave 4: Parallel Algorithmic Monolith Lanes
Start after Wave 3 because these files are denser and benefit from the cleaner
contracts established earlier.

#### Lane K: `Level-generation`
- Lead owns:
  - `src/level-generation/*`
- Subagents:
  - connectivity, rescue traversal, and stair/trap placement
  - layout partitions, anchor carving, and big-cave/chasm/labyrinth shaping
  - room, vault, and quest placement
- Deliverables:
  - split the three remaining level-generation monoliths by secondary ownership
    families rather than by arbitrary line ranges
  - keep generation behavior stable; this is extraction work, not a gameplay
    redesign

#### Lane L: `Monster And Combat Hotspots`
- Lead owns one lane at a time from:
  - `src/monster/monster2.c`
  - `src/melee/melee-movement.c`
  - `src/spell/spell-projection.c`
- Subagents:
  - monster lifecycle vs spawn/summon vs messaging
  - movement-resolution vs action helpers
  - projection pathing vs effect application vs messaging
- Deliverables:
  - open only one of these files at a time unless a lane lead proves the write
    sets are truly disjoint

Constraints:
- Do not run Lane K with more than 2 active subagents.
- Do not combine Lane K and Lane L if both need the same low-level helper or
  ownership header in the same merge window.

### Wave 5: Final Gate Ratchet
Owner: main integrator only.

Status: complete in the live tree on April 17, 2026.

Deliverables:
- refresh docs after the code and audits are stable
- freeze the size-audit baseline and vendor allowlist
- update the modernization audit baseline
- confirm the `externs.h` include-site reduction
- close or rescope any remaining oversize-file exceptions explicitly

Validation:
- `.\build-incremental.ps1`
- `ctest --preset test-standard`
- `ctest --preset test-portable`
- `cmake --preset dev-strict`
- `cmake --build build-strict --parallel`
- `ctest --preset test-strict`
- `cmake --preset dev-sanitize`
- `cmake --build build-sanitize --parallel`
- `ctest --preset test-sanitize` when runtime support is available

Completion notes:
- `tests/source_size_audit_baseline.json` is ratcheted to the current
  oversized-file set, and the vendor allowlist remains limited to
  `src/support/cJSON.c`, `src/cJSON.h`, and `src/support/cJSON.h`.
- `tests/modernization_audit_baseline.json` is ratcheted to
  `src/externs.h` at 184 lines, 39 `extern` declarations, and 16 direct
  include sites, with zero compiled code-bearing `*.inc` files and zero
  non-platform SDL leakage.
- The remaining oversize-file exceptions are now explicitly frozen in the
  source-size baseline:
  - C files above 4000 lines:
    `src/level-generation/level-generation-connectivity.c`,
    `src/monster/monster2.c`,
    `src/level-generation/level-generation-layout.c`,
    `src/runtime/runtime-dungeon.c`
  - C files from 3501 to 4000 lines:
    `src/sdl-scene-menu.c`,
    `src/level-generation/level-generation-rooms.c`,
    `src/melee/melee-movement.c`
  - C files from 2501 to 3500 lines:
    `src/object/object-randart.c`,
    `src/cave/cave.c`,
    `src/melee/melee-attack.c`,
    `src/object/object-info.c`,
    `src/ui/targeting.c`,
    `src/score/score_ui.c`,
    `src/cmd/ui/cmd-ui-abilities.c`
  - H files above 1000 lines:
    `src/defines.h`,
    `src/types.h`
  - H files from 251 to 1000 lines:
    `src/level-generation/level-generation-internal.h`,
    `src/metarun.h`,
    `src/config.h`,
    `src/angband.h`,
    `src/app/app-ui.h`,
    `src/app/app-scene-dungeon.h`,
    `src/smithing/smithing-internal.h`
- The remaining direct `externs.h` include sites are frozen at 16 and tracked
  directly by the modernization baseline instead of by doc-only counts.
- This wave closed the first ratchet snapshot, not the full quantitative end
  state. The continuation waves below reopen only the still-failing targets.

### Wave 6: Serial Continuation Re-Scope
Owner: main integrator only.

Status: complete in the live tree on April 18, 2026.

Deliverables:
- Re-freeze the remaining target list as active closure work rather than
  baseline-only exceptions.
- Publish the continuation lane ownership for:
  - `externs.h` last-mile removal
  - `runtime-dungeon` final split
  - frontend and UI orchestration holdouts
  - remaining algorithmic monoliths
  - remaining mid-size gameplay holdouts
- Define any staging headers needed so the continuation lanes can run in
  parallel without reopening ownership questions.
- Freeze the rule that no baseline is ratcheted upward during continuation
  waves; continuation work must reduce counts.

Write set:
- `docs/`
- any new staging headers needed for continuation lanes
- shared merge-point notes owned by the integrator

Validation:
- `.\build-incremental.ps1`
- `ctest --preset test-standard`
- modernization audit
- size audit

Completion notes:
- The continuation target list is now frozen as active closure work rather than
  passive baseline exceptions.
- The continuation staging headers are now explicit:
  - `src/runtime/runtime-dungeon.h`
  - `src/runtime/runtime-dungeon-internal.h`
  - `src/cmd/ui/cmd-ui-abilities.h`
  - existing lane-owned surfaces in `src/score/score_ui.h` and
    `src/ui/targeting.h`
- The frontend menu-scene continuation surface remains frozen in
  `src/sdl-main-internal.h` rather than adding a new root-level header, so
  Wave 6 does not ratchet the root-header baseline upward.
- `src/cmd/ui/cmd-ui.h` now routes the abilities command surface through the
  lane-local header, and `src/sdl-main-internal.h` explicitly marks the
  menu-scene entrypoint as the frozen Wave 7A continuation surface.
- The integrator-owned `externs.h` consumer buckets are frozen for Wave 7A:
  - `fs` and `score`: `src/fs/load.c`, `src/fs/save.c`, `src/score/score_ui.c`
  - runtime/platform UI: `src/cmd/ui/cmd-ui-info.c`, `src/sdl-main-internal.h`,
    `src/sdl-sound.c`
  - gameplay algorithms: `src/level-generation/level-generation-connectivity.c`,
    `src/level-generation/level-generation-rooms.c`,
    `src/melee/melee-attack.c`, `src/melee/melee-movement.c`,
    `src/melee/melee-process.c`, `src/monster/monster1.c`,
    `src/monster/monster2.c`, `src/object/object-randart.c`,
    `src/object/object-use.c`, `src/spell/spell-projection.c`
- Continuation waves must ratchet baselines downward only; no continuation
  package is allowed to increase the checked-in exception counts.

### Wave 7A: Parallel Surface Cleanup Lanes
Run these four lanes together after Wave 6 contracts are frozen.

#### Lane M: `Externs` Last Mile
- Lead owner:
  - main integrator for `src/externs.h`
- Subagents:
  - `fs` and `score` consumers
  - runtime and UI consumers
  - level-generation and monster/combat consumers
- Deliverables:
  - reduce direct `externs.h` include sites from 16 to fewer than 15,
    preferably to 12 or below if the local headers are already clear
  - move the last declarations into owning subsystem headers
  - keep `src/externs.h` compatibility-only

Constraints:
- Only the integrator edits `src/externs.h`.

#### Lane N: `Runtime-dungeon` Final Split
- Lead owns:
  - `src/runtime/runtime-dungeon.c`
  - new `src/runtime/runtime-dungeon-*.c`
  - optional `src/runtime/runtime-dungeon.h`
- Subagents:
  - loop core and turn processing
  - runtime snapshot, banner, and presentation helpers
  - death and story flow helpers
- Deliverables:
  - separate gameplay-loop ownership from fullscreen and banner presentation
  - drive `runtime-dungeon.c` below 2500 lines if the split remains coherent,
    or else leave it as a thin orchestrator with the bulk of logic moved out

#### Lane O: `Frontend Scene Menu`
- Lead owns:
  - `src/sdl-scene-menu.c`
  - new `src/sdl-scene-menu-*.c`
  - optional `src/sdl-scene-menu.h`
- Subagents:
  - browser and root-menu scene scaffolding
  - save, continue, and profile/menu flows
  - scene adapters for informational frontend pages
- Deliverables:
  - reduce the platform-side menu scene monolith
  - keep frontend-specific orchestration out of core-facing modules

#### Lane P: `UI Holdouts`
- Lead owns:
  - `src/cmd/ui/cmd-ui-abilities.c`
  - `src/score/score_ui.c`
  - `src/ui/targeting.c`
- Subagents:
  - abilities scene and browser flow
  - score scene pages and navigation
  - targeting scene helpers and presentation extraction
- Deliverables:
  - reduce the remaining UI orchestration buckets under 2500 lines where
    practical
  - align these files with the shared scene/browser helper patterns already
    established by earlier waves

Constraints:
- Lane P may create lane-local helper files in its owning folders, but it must
  not introduce new repo-wide shared helper contracts during this wave.

### Wave 7B: Parallel Remaining Monolith Lanes
Start after Wave 7A contracts are stable.

#### Lane Q: `Level-generation` Closeout
- Lead owns:
  - `src/level-generation/*`
- Subagents:
  - connectivity, rescue traversal, and stair/trap placement
  - layout partitions, anchor carving, and big-cave/chasm/labyrinth shaping
  - room, vault, and quest placement
- Deliverables:
  - reduce the three remaining level-generation monoliths below 2500 lines
    where the ownership boundaries are real
  - keep generation behavior stable; this is extraction work, not a gameplay
    redesign

Constraints:
- Do not run Lane Q with more than 2 active subagents.

#### Lane R: `Monster And Combat` Closeout
- Lead owns:
  - `src/monster/monster2.c`
  - `src/melee/melee-movement.c`
- Subagents:
  - monster lifecycle, spawn/summon, and messaging
  - movement-resolution, chase/path helpers, and combat-side movement effects
- Deliverables:
  - reduce the remaining monster/combat ownership buckets below 2500 lines
    where coherent
  - avoid mixing behavior changes with extraction

#### Lane S: `Mid-Size Gameplay Holdouts`
- Lead owns:
  - `src/cave/cave.c`
  - `src/melee/melee-attack.c`
  - `src/object/object-info.c`
  - `src/object/object-randart.c`
- Subagents:
  - cave presentation/state holdouts
  - melee attack helper extraction
  - object info and randart helper extraction
- Deliverables:
  - clear the remaining 2500-3500 line gameplay files that were left frozen in
    the first ratchet snapshot
  - leave only true exceptions with an explicit follow-up owner if any remain

Constraints:
- Lane S should keep write scopes disjoint by file family to avoid cross-lane
  conflicts with Lane Q and Lane R.

### Wave 8: Final End-State Closure
Owner: main integrator only.

Status: complete in the live tree on April 18, 2026.

Deliverables:
- Re-run the full validation matrix after the continuation lanes merge.
- Ratchet the architecture and size baselines downward only if counts actually
  improved.
- Confirm the continuation goals are met:
  - fewer than 15 direct `#include "externs.h"` sites
  - zero non-vendor gameplay or UI source files above 2500 lines, or a clearly
    documented rescope into a separate follow-up plan
- Refresh the plan and surrounding docs to reflect the true final status.

Validation:
- `.\build-incremental.ps1`
- `ctest --preset test-standard`
- `ctest --preset test-portable`
- `cmake --preset dev-strict`
- `cmake --build build-strict --parallel`
- `ctest --preset test-strict`
- `cmake --preset dev-sanitize`
- `cmake --build build-sanitize --parallel`
- `ctest --preset test-sanitize` when runtime support is available

Completion notes:
- The full validation matrix is green:
  - `.\build-incremental.ps1`
  - `ctest --preset test-standard`
  - `ctest --preset test-portable`
  - `cmake --preset dev-strict`
  - `cmake --build build-strict --parallel`
  - `ctest --preset test-strict`
  - `cmake --preset dev-sanitize`
  - `cmake --build build-sanitize --parallel`
  - `ctest --preset test-sanitize`
- The sanitize preset emitted the expected configure warning on this host
  because ASan/UBSan runtime support is unavailable; the preset continued
  without sanitizer instrumentation and the test suite still passed.
- `tests/modernization_audit_baseline.json` is ratcheted downward to
  `src/externs.h` at 181 lines, 36 `extern` declarations, and 0 direct
  include sites, with zero compiled code-bearing `*.inc` files and zero
  non-platform SDL leakage.
- `tests/source_size_audit_baseline.json` is ratcheted downward to 0
  non-vendor `src/**/*.c` files above 2500, 3500, or 4000 lines; the
  remaining header exceptions stay at 9 files above 250 lines and 2 above
  1000 lines.
- The continuation goals are fully met in the live tree, so this plan closes
  without a follow-up rescope package.

## Work Package Map
| ID | Type | Scope | Owner | Depends on |
| --- | --- | --- | --- | --- |
| S0 | Serial | audit hardening, size audit, staging headers | integrator | none |
| P1 | Parallel | `metarun` de-`*.inc` split | lane lead + 3 subagents | S0 |
| P2 | Parallel | `cmd-interact-chest` split | lane lead + 3 subagents | S0 |
| P3 | Parallel | smithing UI split | lane lead + 3 subagents | S0 |
| P4 | Parallel | knowledge split | lane lead + 3 subagents | S0 |
| P5 | Parallel | cave split | lane lead + 3 subagents | P1-P4 contracts not required; S0 required |
| P6 | Parallel | `runtime-dungeon` split | lane lead + 3 subagents | P5 cave contract freeze preferred |
| P7 | Parallel | birth split | lane lead + 3 subagents | S0 |
| S1 | Serial | shared semantic-scene/browser helper extraction | integrator + bounded helpers | P1-P7 |
| P8 | Parallel | save/load sectionization | lane lead + 3 subagents | S1 |
| P9 | Parallel | local header moves for `externs` drain | integrator + 3 subagents | S1 |
| S2 | Serial | final `src/externs.h` removals and audit baseline refresh | integrator | P8-P9 |
| P10 | Parallel | level-generation split | lane lead + 2 subagents | S2 |
| P11 | Parallel | monster/combat hotspot split | lane lead + 2 subagents | S2 |
| S3 | Serial | final gate ratchet and docs refresh | integrator | P10-P11 |
| S4 | Serial | continuation re-scope and staging headers | integrator | S3 |
| P12 | Parallel | `externs.h` last-mile header drain | integrator + 3 subagents | S4 |
| P13 | Parallel | `runtime-dungeon` final split | lane lead + 3 subagents | S4 |
| P14 | Parallel | frontend scene-menu split | lane lead + 3 subagents | S4 |
| P15 | Parallel | remaining UI holdouts split | lane lead + 3 subagents | S4 |
| P16 | Parallel | level-generation closeout | lane lead + 2 subagents | P12-P15 |
| P17 | Parallel | monster/combat closeout | lane lead + 2 subagents | P12-P15 |
| P18 | Parallel | mid-size gameplay holdout split | lane lead + 3 subagents | P12-P15 |
| S5 | Serial | final end-state closure and docs refresh | integrator | P16-P18 |

## Validation Matrix
### Required After Every Merged Package
- `.\build-incremental.ps1`
- `ctest --preset test-standard`

### Required After Audit Or Boundary Packages
- modernization audit
- size audit
- folder ownership audit

### Required After `Metarun`
- smoke-test stats, history, difficulty changes, blessings, curses, and
  corruption-recovery paths
- check non-platform I/O usage from compiled `metarun` code

### Required After `Smithing`
- smoke-test create, enchant, artefact, melt, and reforge flows
- keep smithing parity green

### Required After `Knowledge`
- smoke-test root menu, supplies, objects, artefacts, kills, notes, and oath
  status export

### Required After `Cave` Or `Runtime-dungeon`
- smoke-test map redraw, overhead map, minimap, sidebar, narrative banners,
  death flow, and story intro

### Required After `Frontend Scene Menu` Or UI Holdout Work
- smoke-test main menu, save/continue flows, settings/help entrypoints, score
  scene pages, targeting flows, and abilities scene navigation

### Required After `Birth`
- smoke-test standard character creation, starting loadout, starting artefacts,
  and Blitz auto-build flow

### Required After `Save/Load`
- save/load smoke test
- corruption-focused regression tests
- metarun load/save smoke test
- score screen smoke test

### Required After `Level-generation`
- smoke-test open, close, bash, disarm, tunnel, and alter
- smoke-test partitioned maps, chasm layouts, labyrinth layouts, big caves,
  stairs, traps, and quest vault placement

### Required After `Monster And Combat` Or Gameplay Holdout Work
- smoke-test monster placement, summoning, movement/chase behavior, melee
  attack messaging, object knowledge displays, and randart generation paths

## Immediate Start Order
1. Land `S4` to reopen the frozen exceptions as active continuation work and
   freeze the continuation contracts.
2. Run `P12`, `P13`, `P14`, and `P15` together as the first continuation wave.
3. Once the surface cleanup contracts are stable, run `P16`, `P17`, and `P18`.
4. Close with `S5` only after the continuation targets are either met or
   explicitly rescaled into a separate follow-up plan.

This continuation ordering clears the surface and orchestration debt first,
then attacks the densest algorithmic files once the remaining dependency
surface is narrower and easier to reason about.

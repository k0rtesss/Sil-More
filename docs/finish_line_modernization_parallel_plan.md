# Finish-Line Modernization Parallel Execution Plan

Status date: April 17, 2026.

This document turns the finish-line modernization goals into an execution plan
that can be carried out by multiple agents and subagents in parallel without
constant merge conflicts. It does not replace the higher-level roadmap in
`docs/modernization_mobile_roadmap.md`; it translates that direction into a
parallel delivery program with explicit write sets, ordering rules, and
validation gates.

## Purpose
- Preserve save, metarun, and score compatibility.
- Keep Android and future iOS constraints active in boundary decisions.
- Finish the dependency refactor with module-owned state and narrow headers.
- Sort current and future source files into the correct subsystem folders.
- Convert the end-state goals into enforceable build, audit, and test gates.

## Current Baseline
- `CMAKE_C_STANDARD 17` is already enabled.
- The target split already exists:
  - `sil-core`
  - `sil-platform-sdl`
  - `sil-more`
- Current developer presets:
  - `dev-standard`
  - `dev-portable`
  - `dev-strict`
  - `dev-sanitize`
- Current test presets:
  - `test-standard`
  - `test-portable`
  - `test-strict`
  - `test-sanitize`
- Current registered tests in the configured desktop build trees:
  - `sil_phase01_regressions`
  - `sil_ui1_scaffolding`
  - `sil_ui8_wire`
  - `sil_ui8_demo_packets`
  - `sil_ui0_audit`
  - `sil_architecture_debt_audit`
  - `sil_folder_ownership_audit`
  - `sil_smithing_parity`
- Current source-surface metrics measured on April 18, 2026:
  - `src/externs.h`: 181 lines, 36 `extern` declarations
  - direct `externs.h` include sites: 0 files
  - `src/variable.c`: removed from the live tree
  - root-level `src/*.c`: 18 files
  - root-level `src/*.h`: 39 files
  - non-platform SDL header include sites: 0 files
  - non-platform SDL I/O usage sites: 0 files
- Remaining oversized headers:
  - `src/defines.h`: 4012 lines
  - `src/types.h`: 1656 lines
  - `src/level-generation/level-generation-internal.h`: 412 lines
  - `src/metarun.h`: 352 lines
  - `src/config.h`: 329 lines

## End State
- `src/externs.h` is transitional compatibility only.
- `src/variable.c` is reduced to true process-wide state or removed entirely.
- New code no longer lands in root `src/` unless it is an entrypoint, platform
  implementation, or an explicitly temporary facade.
- The remaining monoliths are split into subsystem folders with narrow headers.
- Non-platform code stops using SDL-shaped stream and helper APIs directly.
- `dev-strict` and `dev-sanitize` exercise the real production targets, not
  just a narrow sentinel object library.
- Architecture debt and folder ownership are enforced by checked-in baselines.

## Guardrails
- Do not break save, metarun, or score compatibility without explicit version
  gates and migration logic.
- Do not reopen the completed terminal-removal work unless an audit regresses.
- Do not add new declarations to `src/externs.h`.
- Do not add new process-wide globals unless there is no narrower module home.
- Do not let parallel lanes edit the shared merge points in the same wave:
  - `src/externs.h`
  - `src/variable.c`
  - root `CMakeLists.txt`
  - `CMakePresets.json`
  - `tests/modernization_audit_baseline.json`
- Do not move a file into a new folder until its replacement headers and source
  list changes are ready to keep the build green.

## Source Tree Ownership Rules
### Root `src/` Policy
- Root `src/` is reserved for:
  - top-level bootstrap and launch files
  - umbrella compatibility headers
  - platform entry files that already follow the `platform-*` or `sdl-*`
    convention
  - thin transitional facades that exist only while a subsystem migration is
    still in progress
- New root-level `src/*.c` implementation files are forbidden.
- Existing root-level implementation files are treated as migration debt and
  must be moved or reduced to thin facades as their owning subsystem lands.

### Folder Mapping Rules
- `src/app/`: frontend-neutral app/session/snapshot/event boundary
- `src/cmd/<family>/`: command entrypoints and command-family orchestration
- `src/ui/` and `src/ui/<domain>/`: semantic scenes and presentation helpers
- `src/smithing/`: smithing rules, state, cost, and item logic
- `src/fs/`: persistence and filesystem helpers
- `src/init/`: load-time parsing, data bootstrap, and startup helpers
- `src/player/`, `src/object/`, `src/melee/`, `src/spell/`, `src/quest/`,
  `src/score/`, `src/drop/`, `src/level-generation/`, `src/log/`,
  `src/support/`: owning subsystem folders only
- `src/platform-*`, `src/sdl-*`, `src/fs/io_sdl.*`, `src/fs/path.*`,
  `src/platform-time.*`, `src/platform-frame.*`: platform implementation only

### Folder Ownership Audit Rules
- Fail on new root-level `src/*.c` files outside an explicit allowlist.
- Fail on files placed outside their owning subsystem without an allowlist.
- Fail when `CMakeLists.txt` source lists do not match on-disk folder
  ownership.

## Agent Execution Model
### Roles
- Main integrator:
  - owns all shared merge points
  - freezes interface contracts
  - performs root `CMakeLists.txt` updates
  - lands audits, baselines, and final integration
- Lane lead agent:
  - owns one subsystem lane
  - defines the lane-local headers and staging facades
  - reviews and merges its subagents' work
- Subagents:
  - operate only inside the write set assigned by the lane lead
  - do not edit shared merge points
  - do not move files outside the lane's folder set

### Recommended Concurrency
- Safe peak concurrency:
  - 1 main integrator
  - up to 3 major lane leads at once
  - up to 2 or 3 subagents per active lane
- Do not run all major lanes at the same time; shared helper churn will erase
  the benefit. Use waves.

### Shared Merge Points Reserved For The Integrator
- `src/externs.h`
- `src/variable.c`
- root `CMakeLists.txt`
- `CMakePresets.json`
- `tools/modernization_audit.py`
- `tests/modernization_audit_baseline.json`

## Wave Plan
### Wave 0: Serial Foundation
Owner: main integrator only.

Deliverables:
- Freeze the modernization metric schema.
- Define the temporary root-file allowlist and folder ownership rules.
- Create staging headers or forwarding headers where later file moves need them.
- Define acceptance criteria for:
  - `externs.h` reduction
  - `variable.c` reduction
  - SDL leakage reduction
  - root-level file-count reduction

Write set:
- `tools/`
- `tests/`
- root `CMakeLists.txt`
- any new subsystem headers needed to unblock later lanes

Validation:
- `cmake --build build-standard --parallel`
- `ctest --output-on-failure`

### Wave 1A: Parallel Structure Lanes
Run these three lanes together.

#### Lane A: `Settings`
- Lead owns:
  - `src/cmd/ui/cmd-ui-settings.c`
  - new `src/cmd/ui/cmd-ui-settings-*.c`
  - optional `src/cmd/ui/cmd-ui-settings.h`
- Subagents:
  - shared browser and prompt scaffolding
  - options, panes, sound, controller settings
  - macros, keymaps, visuals, colors, export helpers
- Deliverables:
  - split the monolith into file-family ownership
  - keep shared UI scaffolding single-owned
  - reduce direct responsibility of the main file to orchestration

#### Lane B: `Metarun`
- Lead owns:
  - `src/metarun.c`
  - `src/metarun.h`
  - new `src/metarun/*.c`
- Subagents:
  - persistence and recovery
  - rules, blessings, oaths, challenges, scoring
  - UI, history, difficulty, and projection helpers
- Deliverables:
  - create `src/metarun/`
  - split state, persistence, rules, scoring, and UI
  - keep compatibility logic intact

#### Lane C: `Cave` Prep And First Split
- Lead owns:
  - `src/cave.c`
  - new `src/cave/*.c`
  - new cave public header
- Subagents:
  - style and tile-variant logic
  - geometry, LOS, and flow helpers
  - map scene, minimap, and render-data assembly
- Deliverables:
  - create `src/cave/`
  - stabilize a cave public surface for later consumers
  - move non-overlapping responsibilities out of `src/cave.c`

Constraints:
- Lane C must publish a stable cave header before `Cmd-interact` starts.
- `src/ui/colors.c` has a single owner during this wave; do not let `Settings`
  and `Cave` modify it in parallel.

### Wave 1B: Parallel Structure Lanes
Start after Wave 1A contracts are stable.

#### Lane D: `Smithing`
- Lead owns smithing core contracts:
  - `src/smithing/*`
  - `src/smithing/smithing.h`
  - `src/smithing/smithing-internal.h`
- Subagent owns smithing UI:
  - `src/ui/smithing/*`
- Deliverables:
  - move remaining non-UI rules out of
    `src/ui/smithing/ui-smithing-screen.c`
  - freeze a UI-facing smithing contract
  - leave the UI file as orchestration and presentation only

#### Lane E: `Cmd-interact`
- Lead owns:
  - `src/cmd/world/cmd-interact.c`
  - new `src/cmd/world/cmd-interact-*.c`
- Subagents:
  - chest logic plus skeleton-note search/generation
  - door open, close, and bash flows
  - trap, tunnel, and alter flows
- Deliverables:
  - split the command monolith by interaction family
  - stop mixing chest, door, trap, tunnel, and note systems in one file

Constraints:
- Lane E starts only after the cave public API used by command code is stable.
- Lane D freezes the smithing core interface before the UI subagent proceeds.

### Wave 2: Serial Shared-Surface Integration
Owner: main integrator only.

Deliverables:
- Update root `CMakeLists.txt` for every new or moved `.c` file.
- Land any temporary facades required to preserve include stability.
- Re-run the full build and test matrix after the Wave 1 lanes merge.
- Normalize any accidental cross-lane helper duplication.

Validation:
- `cmake --build build-standard --parallel`
- `ctest --output-on-failure`
- `cmake --preset dev-strict`
- strict build and test

### Wave 3: Dependency Surface Cleanup
Owner: main integrator, with bounded helper subagents on disjoint folders after
contracts are stable.

#### Serial Pass 3A: `externs.h` Freeze And Drain
- `src/externs.h` becomes compatibility-only.
- Move declarations into narrow subsystem headers.
- Target heavy consumers first:
  - root `src/` files
  - `src/cmd/`
  - `src/init/`
  - `src/object/`
  - `src/player/`

#### Serial Pass 3B: `variable.c` Drain
- Move remaining ownership into:
  - `util-macro`
  - `ui/colors`
  - `init/` or `init/data`
  - `cave/`
  - `object/`
  - monster ownership modules
  - `runtime/` or save ownership modules
  - path/bootstrap ownership modules
- Keep only true process-wide state that this campaign is not rewriting away.

#### Parallel Pass 3C: SDL Leakage Cleanup
Run only after the narrower interfaces exist.

Lane targets:
- persistence and score/reporting modules
- UI export and file-viewing modules
- settings and knowledge dump/export modules

Rules:
- replace non-platform `SDL_IO*` and `SDL_IOStream*` usage with repo-owned file
  abstractions
- replace SDL-shaped helper names in core-facing code with repo-neutral support
  APIs
- keep actual SDL includes inside the platform/app boundary

### Wave 4: Folder Normalization And Enforcement
Owner: main integrator plus small bounded subagents by subsystem.

Deliverables:
- move surviving root-level monoliths into their owning subsystem folders
- reduce old root files to thin compatibility facades or remove them from the
  build once replaced
- add and freeze the folder-ownership allowlist
- ensure Android stays aligned through the root build, not a divergent source
  list

### Wave 5: Final Gate Ratchet
Owner: main integrator only.

Status: complete in the live tree on April 17, 2026.

Deliverables:
- add `tools/modernization_audit.py`
- add `tests/modernization_audit_baseline.json`
- add CTest registrations for:
  - `sil_architecture_debt_audit`
  - `sil_folder_ownership_audit`
- extend strict warnings from the current narrow sentinel toward the production
  targets
- add a portable test preset or equivalent portable validation path
- refresh stale docs only after the code and baselines are stable

Completion notes:
- `tools/modernization_audit.py` and
  `tests/modernization_audit_baseline.json` are checked in and enforced by
  CTest.
- `sil_architecture_debt_audit` and `sil_folder_ownership_audit` are both
  registered in the configured desktop build trees.
- `dev-strict` and `dev-sanitize` now exercise the real production targets
  (`sil-core`, `sil-platform-sdl`, and `sil-more`).
- `test-portable` provides the portable validation path, and both
  `build-strict` and `build-portable` currently pass the full eight-test suite.

## Work Package Map
| ID | Type | Scope | Owner | Depends on |
| --- | --- | --- | --- | --- |
| S0 | Serial | audit schema, folder policy, staging headers | integrator | none |
| P1 | Parallel | settings split | lane lead + 3 subagents | S0 |
| P2 | Parallel | metarun split | lane lead + 3 subagents | S0 |
| P3 | Parallel | cave split and public header | lane lead + 2 subagents | S0 |
| P4 | Parallel | smithing core/UI split | lane lead + 1 subagent | P3 interface freeze not required; smithing contract freeze required |
| P5 | Parallel | cmd-interact split | lane lead + 3 subagents | P3 |
| S1 | Serial | CMake integration and shared helper normalization | integrator | P1-P5 |
| S2 | Serial | `externs.h` drain | integrator | S1 |
| S3 | Serial | `variable.c` drain | integrator | S2 |
| P6 | Parallel | SDL I/O and helper leakage cleanup by subsystem | integrator + bounded subagents | S2-S3 |
| S4 | Serial | folder normalization and enforcement | integrator | P6 |
| S5 | Serial | audit baseline freeze, strict-gate ratchet, docs refresh | integrator | S4 |

## Validation Matrix
### Required After Every Merged Package
- `cmake --build build-standard --parallel`
- `ctest --preset test-standard`

### Required After Shared-Surface Passes
- `cmake --preset dev-strict`
- `cmake --build build-strict --parallel`
- `ctest --preset test-strict`
- `ctest --preset test-portable`
- `cmake --preset dev-sanitize`
- `cmake --build build-sanitize --parallel`
- `ctest --preset test-sanitize` when runtime support is available

### Required After Settings Work
- smoke-test options, sound config, controller bindings, macros, keymaps,
  visuals, and colors

### Required After Metarun Work
- smoke-test stats, history, difficulty changes, blessings, oaths, and
  corruption-recovery paths

### Required After Smithing Work
- smoke-test create, enchant, artefact, numbers, melt, and reforge flows
- keep smithing parity green

### Required After `Cmd-interact` Or `Cave` Work
- smoke-test open, close, bash, disarm, tunnel, and alter
- smoke-test overhead map, minimap, sidebar, and map redraw behavior

### Required After Folder Or Boundary Work
- architecture audit
- folder ownership audit
- desktop SDL launch smoke-test
- Android debug configure and build smoke-test

## Quantitative Finish Criteria
- `src/externs.h` under 400 lines
- fewer than 25 direct `externs.h` include sites
- `src/variable.c` under 200 lines or removed
- no gameplay or UI source file above 2500 lines except vendor or generated code
- root `src/*.c` count driven down to entrypoints, platform files, and explicit
  temporary facades only
- no non-platform SDL header includes outside the approved boundary
- no non-platform SDL I/O usage outside the approved boundary
- modernization audit and folder audit both pass from checked-in baselines

## Immediate Start Order
1. Land `S0` and freeze the audit schema, root-file policy, and staging headers.
2. Run `P1`, `P2`, and `P3` together.
3. Once cave and smithing contracts are stable, run `P4` and `P5`.
4. Merge through `S1`, then perform `S2` and `S3`.
5. Run `P6`, then finish with `S4` and `S5`.

This ordering keeps the highest-conflict files out of the initial parallel
lanes, gets the big monoliths moving first, and delays shared merge-point work
until subsystem ownership is clearer.

# Modernization And Mobile Readiness Roadmap

## Purpose
This roadmap picks up where the unstable-port refactor stops.

The structural split is largely landed. The next wave is not another large
monolith breakup for its own sake. The next wave is:

- harden persistence and startup correctness
- finish the dependency cleanup that the file split enabled
- make the platform boundary truly SDL-free on the core side
- make Android and future iOS support first-class architectural constraints

Status date: March 26, 2026.

## Current Status
- `CMAKE_C_STANDARD 17` is already enabled in [`CMakeLists.txt`](../CMakeLists.txt).
- The target split is already present:
  - `sil-core`
  - `sil-platform-sdl`
  - `sil-more`
- The current desktop audit on March 26, 2026 confirmed:
  - `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1` succeeds
  - the tree is structurally past the old unstable-port milestone
- `Phase 0` is complete in the live tree:
  - corrupt savefile reads now fail boundedly
  - `meta.raw` writes use staged replacement and corruption recovery
  - raw-cache payload reads are validated
  - the `editing_buffer_init()`, `mini_screenshot()`, and `no_light()` audit bugs are closed
  - smithing difficulty parity between engine and script is restored on the shared regression corpus
- `Phase 1` is complete in the live tree:
  - `CTest` now runs `sil_phase01_regressions` plus `sil_smithing_parity`
  - `CMakePresets.json` now provides `dev-standard`, `dev-portable`, `dev-strict`, and `dev-sanitize`
  - the strict preset builds a warning-free gate for the touched `Phase 0` / `Phase 1` files
  - the sanitizer preset probes ASan/UBSan runtime availability and falls back with a configure warning on hosts where those runtimes are unavailable
- Android support is real:
  - Gradle project exists under [`android/`](../android)
  - native Android CMake entrypoint exists
  - SDL activity wrapper exists
  - asset sync exists
  - touch pane and gamepad plumbing exist
- The remaining bottlenecks are now concentrated in a smaller set of files and
  surfaces:
  - [`src/main-sdl.c`](../src/main-sdl.c): 5,377 lines
  - [`src/init2.c`](../src/init2.c): 2,724 lines
  - [`src/z-term.c`](../src/z-term.c): 2,150 lines
  - [`src/util.c`](../src/util.c): 4,099 lines
  - [`src/externs.h`](../src/externs.h): 1,074 lines / 883 `extern`s
  - [`src/variable.c`](../src/variable.c): 710 lines
- The biggest newly-audited risk areas are:
  - save/load corruption handling
  - metarun atomicity and corruption recovery
  - score persistence consistency
  - script/engine drift for smithing difficulty
  - remaining SDL leakage into core-facing code
  - mobile safe-area, device-class, and resource-path assumptions

## Guiding Rules
- Do not reopen completed unstable-port work packages unless a later phase
  exposes real fallout.
- Do not mix modernization packages with gameplay balance changes.
- Do not add new declarations to [`src/externs.h`](../src/externs.h) unless
  there is no narrower home.
- Do not add new process-wide globals unless there is no module-owned
  alternative.
- Do not add new code-bearing `*.inc` files.
- Do not break save, score, or metarun compatibility without explicit version
  gates and migration logic.
- Do not add new SDL includes to core-facing modules.

## Phase 0: Reliability And Correctness Hardening
Goal: eliminate the known live correctness hazards before doing more structural
cleanup.

Status: complete in the working tree on March 26, 2026.

Tasks:
- Make corrupt or truncated savefiles fail cleanly instead of hanging the
  loader.
  - harden `sf_get()` and all dependent read loops in
    [`src/load.c`](../src/load.c)
  - make RLE and sentinel-driven loops terminate on error
- Make `meta.raw` writes atomic.
  - stop deleting the live file before the replacement is durable
  - use the same staged-write pattern already used for player saves where
    possible
- Make `meta.raw` loads reject short payload reads and recover safely.
- Resynchronize smithing difficulty logic between:
  - [`src/drop/drop-system-difficulty.c`](../src/drop/drop-system-difficulty.c)
  - [`scripts/calc_artefact_difficulty.py`](../scripts/calc_artefact_difficulty.py)
- Fix the small but real audit bugs:
  - `editing_buffer_init()` pointer-size copy bug in
    [`src/util.c`](../src/util.c)
  - `mini_screenshot()` edge read bug in [`src/files.c`](../src/files.c)
  - confirm whether the fallback `no_light()` in [`src/util.c`](../src/util.c)
    is a temporary stub or a regression, then either restore the real behavior
    or replace the old dependency path explicitly
- Make cached `*.raw` template loading validate payload reads, not just headers.
- Decide on a score persistence policy when `runs.db` and `scores.raw` update
  paths disagree.

Validation focus:
- truncated savefile loads must fail within a bounded timeout
- truncated `meta.raw` must recover instead of partially deserializing
- smithing difficulty script output must match engine output on a shared corpus
- corrupted `*.raw` cache files must force rebuild from `lib/edit/*`

Done when:
- no known corrupt-input path can hang the game loop
- `meta.raw` writes are crash-safe
- smithing difficulty tool parity is restored
- the identified correctness bugs are closed

## Phase 1: Validation And Tooling Ratchet
Goal: make future cleanup safer by adding build and test pressure.

Status: complete in the working tree on March 26, 2026.

Tasks:
- Add one opt-in strict build preset.
  - stronger warnings than the default developer build
  - warning-free target for touched files
- Add one sanitizer build preset for Clang/GCC.
  - ASan
  - UBSan
- Add `ctest` integration for narrow, high-value tests.
- Add targeted regression harnesses for:
  - savefile corruption
  - `meta.raw` corruption
  - raw-cache corruption
  - score dual-write failure paths
  - smithing difficulty parity
- Keep `compile_commands.json` enabled and usable in all active build paths.

Validation focus:
- strict preset builds cleanly
- sanitizer preset launches and passes targeted tests
- each Phase 0 bug gets a reproducer or regression test

Done when:
- there is a repeatable strict build path
- there is a repeatable sanitizer path
- critical persistence bugs have executable regression coverage

## Phase 2: Header And Global Surface Reduction
Goal: finish the dependency refactor that the source split made possible.

Tasks:
- Reduce direct inclusion of [`src/externs.h`](../src/externs.h).
- Move declarations into narrow subsystem headers.
- Localize globals out of [`src/variable.c`](../src/variable.c) wherever
  ownership is now obvious.
- Remove dead or narrow globals first:
  - note/output state
  - sort hooks
  - old ghost helper state
  - remaining UI overlay globals
- Keep [`src/types.h`](../src/types.h) focused on types, not runtime ownership.
- Move `NavResult` / `PlayResult` and similar non-type ownership into narrower
  headers.

Near-term metric targets:
- [`src/externs.h`](../src/externs.h) under 900 lines
- [`src/variable.c`](../src/variable.c) under 650 lines

Longer-term targets:
- [`src/externs.h`](../src/externs.h) under 600 lines
- [`src/variable.c`](../src/variable.c) under 400 lines
- direct `#include "externs.h"` users under 80, then under 20

Done when:
- new code no longer depends on `externs.h` by default
- module ownership is reflected in headers, not just file names

## Phase 3: Monolith Reduction Round 3
Goal: split the remaining ownership hubs that still mix unrelated concerns.

Tasks:
- Split [`src/util.c`](../src/util.c) by ownership.
  Recommended target slices:
  - macro/keymap handling
  - message/history handling
  - text output and wrapping
  - editing buffer utilities
  - geometry/angle helpers
  - remaining generic glue
- Split [`src/init2.c`](../src/init2.c) by concern.
  Recommended target slices:
  - path and directory bootstrap
  - user-data migration and seeding
  - raw-cache loading/validation
  - intro/menu bootstrap
  - cleanup and shutdown
- Split [`src/main-sdl.c`](../src/main-sdl.c) into SDL-only implementation
  modules.
  Recommended target slices:
  - window/renderer lifecycle
  - pane layout
  - input event pump
  - gamepad input
  - touch pane UI
  - story font rendering
  - resource loading
- Revisit [`src/z-term.c`](../src/z-term.c) only after the persistence and
  platform cleanup above are stable.

Suggested size targets:
- [`src/util.c`](../src/util.c) under 1,500 lines
- [`src/init2.c`](../src/init2.c) under 1,800 lines
- [`src/main-sdl.c`](../src/main-sdl.c) under 3,000 lines, then under 2,000

Done when:
- the remaining giant files are orchestration layers, not ownership buckets

## Phase 4: True Platform Boundary
Goal: make the core/frontend split semantically true, not just target-shaped.

Tasks:
- Remove SDL from `sil-core` link requirements once the remaining direct
  dependencies are eliminated.
- Drive direct SDL includes out of core-facing modules.
- Replace SDL-shaped public APIs with neutral platform-facing interfaces.
  Examples:
  - `get_sdl_*` / `set_sdl_*` should become generic platform or app config APIs
  - touch, gamepad, and layout policies should be capability-driven
- Finish the I/O boundary cleanup.
  - route persistence and tooling through `ang_file`
  - remove raw `SDL_IOStream*` usage from core-facing code
- Add a resource layer for bundled assets and user overrides.
  - fonts
  - tiles
  - sounds
  - music
  - help assets
- Remove gameplay-side direct timing calls such as `SDL_Delay()` where they
  still exist.

Done when:
- core-facing code no longer includes SDL headers directly
- `sil-core` no longer links SDL directly
- the public platform boundary is implementation-neutral

## Phase 5: Mobile Device And Layout Adaptation
Goal: support handhelds as first-class targets instead of clamped desktop
layouts.

Tasks:
- Add safe-area aware layout and input regions.
- Add device classes, at minimum:
  - compact phone
  - large phone
  - tablet
  - handheld-console
- Replace one-size-fits-all defaults with preset families for:
  - minimum terminal geometry
  - pane visibility
  - touch-pane layout
  - default scale
  - font scaling
- Make touch pane configuration responsive instead of fixed-grid only.
- Consolidate Android-specific policy behind capability helpers instead of
  scattered `#ifdef __ANDROID__` branches.
- Review suspend/resume, renderer reset, and background/foreground transitions.
- Decide whether landscape lock remains intentional for phones and tablets.

Validation focus:
- safe-area devices
- tiny phone landscape
- large tablet
- touch-only play
- external gamepad play
- suspend/resume
- renderer reset and context loss

Done when:
- mobile defaults are intentional per device class
- safe areas and insets are accounted for explicitly

## Phase 6: Packaging, Cross-Platform Follow-Through, And Documentation
Goal: make the mobile and cross-platform story operational, not just source-true.

Tasks:
- Prefer app-scoped writable roots consistently for future iOS portability.
- Remove assumptions that bundled assets are ordinary filesystem paths.
- Make Android packaging release-ready.
  - signing
  - ABI policy
  - release build path
  - smoke checklist
- Decide on the first iOS-facing packaging experiment only after the resource
  and platform boundary work is done.
- Refresh documentation:
  - [`README.md`](../README.md)
  - [`android/README.md`](../android/README.md)
  - any stale SDL migration notes

Done when:
- packaging guidance matches the live build system
- mobile runtime assumptions are documented and reproducible

## Serial Work Packages
These are best handled by the main integrator or one agent at a time.

| ID | Scope | Main files | Depends on |
| --- | --- | --- | --- |
| WP100 | Baseline metrics, strict-build design, validation matrix | `docs/modernization_mobile_roadmap.md`, `session_notes.md`, build scripts | none |
| WP199 | Final integration, metric pass, roadmap closeout | cross-cutting | all other packages |

## Parallel Work Packages
These can run in parallel as long as write sets stay disjoint.

| ID | Scope | Main files | Depends on |
| --- | --- | --- | --- |
| WP110 | save/load corruption hardening | `src/load.c`, `src/save.c`, `src/fs/io_sdl.c` | WP100 |
| WP111 | metarun atomic save + corruption recovery | `src/metarun.c`, related fs helpers | WP100 |
| WP112 | score persistence consistency cleanup | `src/runtime/runtime-game.c`, `src/score/*` | WP100 |
| WP113 | smithing difficulty sync | `src/drop/drop-system-difficulty.c`, `scripts/calc_artefact_difficulty.py` | WP100 |
| WP114 | raw-cache payload validation | `src/init2.c`, `src/init/*` helpers | WP100 |
| WP115 | targeted bug fixes | `src/util.c`, `src/files.c`, any direct call sites | WP100 |
| WP120 | strict build + sanitizer presets | `CMakeLists.txt`, build scripts | WP100 |
| WP121 | corruption/parity regression tests | new test harness files, build/test config | WP110-WP115, WP120 |
| WP130 | `externs.h` reduction wave 1 | `src/externs.h`, subsystem headers | WP100 |
| WP131 | `variable.c` ownership cleanup wave 1 | `src/variable.c`, owning modules | WP130 |
| WP140 | `util.c` split | `src/util.c`, new utility modules | WP121, WP130-WP131 |
| WP141 | `init2.c` split | `src/init2.c`, new init/fs/runtime helpers | WP114, WP121 |
| WP142 | `main-sdl.c` split | `src/main-sdl.c`, new SDL implementation modules | WP120, WP121 |
| WP150 | true platform boundary cleanup | `CMakeLists.txt`, `src/platform-*.h`, core callers | WP130-WP142 |
| WP151 | resource layer | new resource helpers, `src/main-sdl.c`, `src/init2.c`, `src/score/*` | WP141, WP142, WP150 |
| WP160 | mobile device classes + safe areas | SDL implementation modules, `src/pane.c`, Android activity | WP142, WP150, WP151 |
| WP161 | mobile lifecycle + packaging follow-through | `android/*`, platform startup/shutdown code | WP160 |
| WP170 | documentation refresh | `README.md`, `android/README.md`, docs | WP150-WP161 |

## Validation Matrix

### Must Run After Every Package
- `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1`

### Must Run After Persistence Or Path Packages
- corruption-focused targeted tests
- save/load smoke test
- metarun load/save smoke test
- score screen smoke test

### Must Run After Platform Or Mobile Packages
- desktop SDL launch smoke test
- Android debug build path
- touch-pane smoke test
- external gamepad smoke test
- suspend/resume smoke test
- safe-area smoke test on at least one inset device

### Must Smoke-Test For UI Packages
- inventory/equipment overlays
- floor-item `-)` behavior
- unified look and sidebar
- combat roll overlay
- help/file viewer if touched
- story/death screens if touched

## Recommended Execution Order
1. Treat `WP110` through `WP121` as complete in the live tree.
2. Start the dependency cleanup wave: `WP130`, `WP131`, `WP140`, `WP141`, `WP142`.
3. Treat the true platform boundary as the gate before any iOS-facing work: `WP150`, `WP151`.
4. Do mobile adaptation after the boundary is real: `WP160`, `WP161`.
5. Close with docs and integration: `WP170`, `WP199`.

## Recommended Immediate Start
1. Start `WP130` to keep reducing direct reliance on [`src/externs.h`](../src/externs.h).
2. Follow with `WP131` to keep localizing obvious ownership out of [`src/variable.c`](../src/variable.c).
3. Open the next monolith reduction slice only after the header/global surface starts shrinking again.

The hardening and validation ratchet are in place now; the next highest-value
work is dependency-surface reduction rather than more Phase 0/1 fallout.

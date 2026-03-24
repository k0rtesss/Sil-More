# Unstable Refactor Port Plan

## Purpose
Port the structural refactor already proven on `unstable` into `quests-and-refactor` without losing the newer gameplay, Android, quest, UI, and drop-system work on the current branch. After the unstable structure is in place, continue past unstable to finish the job: shrink `externs.h`, localize globals from `variable.c`, and separate core game code from SDL-facing code so the codebase is safer for Android now and iOS later.

## Current Status
- Status date: 2026-03-24.
- `Phase 1` is effectively complete in the working tree:
  - `CMakeLists.txt` is grouped by subsystem instead of one flat source list.
  - Target scaffold directories now exist under `src/` for `cmd/`, `drop/`, `init/`, `level-generation/`, `melee/`, `object/`, `smithing/`, `spell/`, and `ui/smithing/`.
  - Transitional subsystem headers now own the first extracted declaration blocks for `drop`, `level-generation`, and `smithing` instead of keeping those blocks inline in `externs.h`.
- `Phase 2` has started through `WP10` (`init/` split):
  - `src/init/init-parser-core.c` now owns `parse_tile_line()`, `init_info_txt()`, `add_text()`, and `add_name()`.
  - `src/init/init-flags.c` now owns `info_flags`, `grab_one_flag()`, and `dbg_show_active_flags()`.
  - `src/init/init-object-bonuses.c` now owns the shared object/artefact/ego stat-skill bonus parsing helpers that were previously embedded in `init1.c`.
  - `src/init/init-parse-monster.c` now owns `parse_r_info()` and its local monster flag/blow parsing helpers.
  - `src/init1.c` has already been reduced by those extractions and now includes `init/init-parse-internal.h` for shared parser helpers.
- Validation so far:
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` succeeded after the scaffold landing and again after the `init/` extractions.
- Not started yet:
  - `WP11`, `WP12`, `WP13`, and `WP14`.
  - Any of the Phase 3+ object/drop/cmd/melee/spell/frontend separation work.

## Branch Facts
- Current working branch studied for this plan: `quests-and-refactor` at `750b4e601b1c2afba2ec7f573763e1a2ed22e367`.
- Refactor reference branch studied for this plan: `unstable` at `ce8d936796ba509c6aedf661f45f9e9e3eab8da9`.
- Merge base between them: `4ab3ad887a6e2fc5746d45c6977305c7e9af52e4`.
- `unstable` and `quests-and-refactor` diverged in parallel. `unstable` is not an ancestor of the current branch, and the current branch is not an ancestor of `unstable`.
- Consequence: do not blindly cherry-pick unstable refactor commits. Use them as a structural reference and port each subsystem onto the current branch with three-way diffing.

## Evidence From The Current Tree
- Biggest monoliths on the current branch:
  - `src/cmd4.c`: 20,357 lines
  - `src/generate.c`: 17,554 lines
  - `src/files.c`: 9,413 lines
  - `src/xtra2.c`: 9,145 lines
  - `src/object1.c`: 6,645 lines
  - `src/init1.c`: 6,268 lines
  - `src/spells1.c`: 6,507 lines
  - `src/object2.c`: 5,751 lines
  - `src/melee2.c`: 5,324 lines
  - `src/xtra1.c`: 5,037 lines
- Current global/header surface:
  - `src/externs.h`: 1,752 lines and about 1,414 `extern` declarations
  - `src/variable.c`: 789 lines and about 217 global definitions
- `unstable` already created these target directories:
  - `src/cmd/`
  - `src/drop/`
  - `src/init/`
  - `src/level-generation/`
  - `src/melee/`
  - `src/object/`
  - `src/player/`
  - `src/smithing/`
  - `src/spell/`
  - `src/ui/`
- `unstable` improved header sprawl but did not finish it:
  - `unstable:src/externs.h`: 1,145 lines and about 979 `extern` declarations
  - `unstable:src/variable.c`: 784 lines
- Consequence: unstable is a good intermediate architecture, not the final one.

## Refactor Principles
1. Refactor mechanically first, simplify second. Preserve behavior while moving code.
2. Keep save/data compatibility intact. Any format change must still follow `defines.h`, `save.c`, and `load.c` versioning rules.
3. Treat `unstable` as the target layout, not as a patch queue.
4. Every new `.c` file starts with `#include "angband.h"`.
5. New modules get narrow headers. Do not add new exports to `externs.h` unless the declaration is explicitly transitional.
6. Prefer `static` internal helpers and one private `*-internal.h` per subsystem where needed.
7. For new APIs, prefer `bool` success/failure, `const` inputs, and `size_t` for counts/byte lengths where practical.
8. Keep gameplay logic free of SDL/UI details wherever possible. Mobile work depends on that boundary.
9. One subagent owns one write set. Avoid overlapping edits to the same subsystem.
10. Each work package ends with a compiling build and a focused smoke test before the next wave lands.

## Target Architecture

### Source Layout Target
Use the unstable layout as the baseline and keep growing it:

| Current monolith | Target modules |
| --- | --- |
| `cmd1.c`, `cmd3.c`, `cmd5.c`, `cmd6.c`, part of `cmd2.c` | `src/cmd/combat/`, `src/cmd/item/`, `src/cmd/movement/`, `src/cmd/monster/`, `src/cmd/ui/`, `src/cmd/world/` |
| `generate.c` | `src/level-generation/` |
| `object1.c` | `src/object/object-desc.c`, `object-display.c`, `object-flags.c`, `object-flavor.c`, `object-slot.c`, `object-ui-display.c`, `object-util.c` |
| `object2.c` | `src/object/object-allocation.c`, `object-inventory.c`, `object-knowledge.c`, `object-list.c`, `object-make.c`, `object-place.c` |
| `drop_system.c` | `src/drop/drop-system-catalog.c`, `drop-system-difficulty.c`, `drop-system-selection.c` plus a thin `drop_system.c` facade if still needed |
| `init1.c` | `src/init/` parser and loader modules |
| `files.c` | `src/signals.c`, `src/ui/ui-file-viewer.c`, `src/ui/ui-help.c`, keep non-UI filesystem logic in `files.c` or move later to a clearer module |
| `xtra1.c` | `src/player/ability_log.c`, `encumbrance.c`, `identification.c`, `weapon_stats.c` |
| `xtra2.c` | `src/targeting.c`, `src/player-status.c`, `src/player-xp.c`, `src/monster-death.c` |
| `spells1.c`, `spells2.c` | `src/spell/` plus `src/player/player-songs*.c`, `player-calc.c`, `game-event.c` |
| `melee1.c`, `melee2.c` | `src/melee/` |
| `ui/ui-smithing.c` | `src/smithing/` and `src/ui/smithing/` |

### Header Layout Target
- Public headers only for real subsystem APIs.
- Private headers stay inside the owning directory:
  - `src/object/object-internal.h`
  - `src/drop/drop-system-internal.h`
  - `src/level-generation/level-generation-internal.h`
  - `src/init/init-parse-internal.h`
  - `src/ui/smithing/ui-smithing-internal.h`
- Long term:
  - `externs.h` becomes transitional compatibility only.
  - `variable.c` keeps only true process-wide or save-global state.
  - SDL-specific headers stop leaking into gameplay modules.

## Port Strategy

### Rule: Recreate The Split On Current Branch, Then Reconcile
For each subsystem:
1. Diff the subsystem between merge-base and unstable.
2. Recreate the file moves and extraction boundaries on the current branch.
3. Move current-branch logic into the new files without changing behavior.
4. Only after the split compiles, compare against unstable again for any helper APIs or smaller cleanup that should also be ported.

### Rule: Never Mix Mechanical Split With New Design Work
Each subsystem should land in this order:
1. File creation, moves, and include/CMake updates.
2. Compile fixes and linker fixes.
3. Behavior-preserving smoke validation.
4. Only then local cleanup such as `static`, `const`, narrower headers, or API tightening.

## Delivery Phases

### Phase 0: Baseline And Freeze Rules
Goal: make the refactor safe to run for several days of parallel work.

Tasks:
- Create a dedicated integration branch for the refactor.
- Build before any changes with `.\build-cmake.bat` and record warnings.
- Capture a quick smoke-test baseline:
  - start game
  - save/load
  - inventory/equipment overlays
  - unified look
  - combat roll overlay
  - one quest accept/complete flow
  - one metarun open/save flow
- Announce a temporary rule: gameplay feature work touching refactor hotspots must be short-lived or rebased quickly.

Done when:
- Baseline build is green.
- Smoke checklist exists in `session_notes.md`.

### Phase 1: Build Graph And Header Scaffolding
Goal: prepare the repo for many small files instead of a few giant ones.

Status:
- Completed in the working tree on 2026-03-24.

Tasks:
- Restructure `CMakeLists.txt` into grouped source lists by subsystem even before all files are split.
- Add empty or minimal public headers for the target subsystems.
- Add private internal headers where unstable already proved them useful.
- Define naming rules:
  - public headers: subsystem nouns
  - private headers: `*-internal.h`
  - no new generic `misc.h`, `common.h`, `helpers.h`
- Add transitional wrappers only where needed to keep linker churn low.

Done when:
- Source list is grouped by subsystem.
- New directories exist and are accepted by the build.

### Phase 2: Port The Lowest-Conflict Structural Splits First
Goal: land the splits that create clear ownership and do not heavily depend on each other.

Status:
- In progress as of 2026-03-24.
- `WP10` has started; `init-parser-core.c`, `init-flags.c`, `init-object-bonuses.c`, and `init-parse-monster.c` have already been extracted from `init1.c`.

Recommended order:
1. `src/init/` from `init1.c`
2. `src/level-generation/` from `generate.c`
3. `src/files.c` breakout into `signals.c`, `ui/ui-file-viewer.c`, `ui/ui-help.c`
4. `src/xtra2.c` breakout into `targeting.c`, `player-status.c`, `player-xp.c`, `monster-death.c`
5. `src/xtra1.c` breakout into `src/player/` utility modules

Why this order:
- These splits create clean subsystem boundaries early.
- They reduce the size of the hottest files before command/combat/object work starts.
- They create reusable headers that later phases can depend on.

Done when:
- `init1.c`, `generate.c`, `files.c`, `xtra1.c`, and `xtra2.c` are substantially reduced or fully replaced by targeted modules.

### Phase 3: Object And Drop Foundations
Goal: isolate the object model and drop system before touching the higher-level command/combat code that consumes them.

Tasks:
- Port the `object1.c` split into `src/object/` first.
- Port the `object2.c` split next.
- Port the `drop_system.c` split into `src/drop/`.
- Keep shared object/drop declarations out of `externs.h`; move them into `object/*.h` and `drop_system.h`.
- Keep the smithing-analysis sync rule in place: if `object_difficulty()` changes, update `scripts/calc_artefact_difficulty.py` in the same changeset.

Done when:
- Object description, UI display, allocation, creation, placement, and drop selection live in explicit modules.
- Object/drop modules compile without needing new `externs.h` growth.

### Phase 4: Command Split
Goal: remove the command-file bottleneck and make gameplay input flows separately ownable.

Tasks:
- Port unstable's `src/cmd/` layout.
- Keep command entry points stable at first so key handling does not regress.
- Split by user action domain, not by arbitrary line count:
  - combat
  - ranged
  - movement/search/travel
  - item core/drop/utility/activate/pickup/fletchery
  - UI look/query/object display
  - world/interact
- Move only declarations needed by a command family into family headers.

Validation focus:
- floor item `-)` behavior and `-` shortcut
- auto list drop-downs
- menu cycling `u`/`x`
- screen overlays using `screen_save()` / `screen_load()`

### Phase 5: Smithing, Spells, Melee
Goal: finish the gameplay-heavy splits once object/player foundations exist.

Tasks:
- Port unstable's `src/smithing/` and `src/ui/smithing/` split.
- Port unstable's `src/spell/`, `src/player/player-songs*.c`, `player-calc.c`, and `game-event.c`.
- Port unstable's `src/melee/`.
- Keep command/combat display code separate from combat rules.
- Avoid mixing UI strings, combat math, and targeting state in the same module.

Validation focus:
- combat history viewer
- combat roll overlay position and clear width
- songs and projected effects
- smithing screens and number calculations
- ranged attacks and pathing

### Phase 6: Rolling Header And Global-State Cleanup
Goal: go past unstable and remove the historical coupling that unstable only reduced.

Tasks:
- Replace broad `externs.h` usage with narrow subsystem headers as each subsystem lands.
- Move obviously local globals out of `variable.c` into owning modules.
- Apply the existing `global_state_localization_plan.md` items first:
  - input-state flags into `util.c`
  - mini-screenshot buffers into `files.c`
  - projectile-path ignore context into function arguments
  - runtime CLI flags out of global storage
  - background-color policy into UI color state
- Continue with similar treatment for any globals that are read by only one subsystem.

Target metrics:
- `externs.h` should stop growing immediately.
- After the major splits, target `externs.h` under 600 lines.
- Longer term, target `externs.h` under 400 lines and `variable.c` under 200 lines.

### Phase 7: Core Versus Frontend Separation
Goal: make the codebase friendlier to Android now and iOS later.

Tasks:
- Split build targets into at least:
  - `sil-core`: gameplay, data parsing, save/load, score, RNG, content systems
  - `sil-platform-sdl`: SDL main loop, panes, config, sound, rendering glue
  - launcher target or app target that links them
- Ensure gameplay modules do not include SDL headers directly unless absolutely required.
- Push path/config/input adapter code toward platform-facing modules.
- Keep `z-term.c` stable during earlier phases, then isolate it behind the SDL frontend boundary as one of the last steps.

Done when:
- Android and desktop both link the same core library.
- An eventual iOS frontend can link the same core without touching dungeon/object/combat code.

### Phase 8: Final Simplification
Goal: delete transitional glue once the architecture is stable.

Tasks:
- Remove compatibility wrappers that only existed during the move.
- Collapse duplicated declarations that exist in both `externs.h` and new subsystem headers.
- Tighten `const` correctness and return types.
- Trim any empty or one-function modules that do not deserve their own file after the dust settles.

## Subagent Work Packages

### Serial Setup Packages
These should be done by the main integrator or one agent at a time:

| ID | Scope | Main files | Depends on |
| --- | --- | --- | --- |
| WP00 | Baseline build, smoke matrix, branch prep | `session_notes.md`, build scripts | none |
| WP01 | CMake grouping and directory/header scaffolding | `CMakeLists.txt`, new headers | WP00 |
| WP99 | Final integration and header/global cleanup | cross-cutting | all other packages |

Current serial package status:
- `WP01`: completed in the working tree on 2026-03-24.
- `WP00`: partially covered operationally by successful builds, but the full smoke matrix has not yet been formalized in this plan or `session_notes.md`.
- `WP99`: not started.

### Parallel Packages
These can be run in parallel once `WP01` lands, as long as write sets stay disjoint:

| ID | Scope | Main files | Depends on |
| --- | --- | --- | --- |
| WP10 | `init/` split | `src/init1.c`, `src/init/*` | WP01 |
| WP11 | `level-generation/` split | `src/generate.c`, `src/level-generation/*` | WP01 |
| WP12 | `files` UI/help/signal split | `src/files.c`, `src/signals.c`, `src/ui/ui-file-viewer.c`, `src/ui/ui-help.c` | WP01 |
| WP13 | `xtra2` split | `src/xtra2.c`, `src/targeting.c`, `src/player-status.c`, `src/player-xp.c`, `src/monster-death.c` | WP01 |
| WP14 | `xtra1` to `player/` split | `src/xtra1.c`, `src/player/ability_log.*`, `encumbrance.*`, `identification.*`, `weapon_stats.*` | WP01 |
| WP20 | `object1` split | `src/object1.c`, `src/object/*display*`, `*desc*`, `*flags*`, `*flavor*`, `*slot*`, `*util*` | WP01 |
| WP21 | `object2` split | `src/object2.c`, `src/object/*allocation*`, `*inventory*`, `*knowledge*`, `*list*`, `*make*`, `*place*` | WP20 |
| WP22 | `drop/` split | `src/drop_system.c`, `src/drop/*`, `src/drop_system.h` | WP20 |
| WP30 | `cmd/` split | `src/cmd1.c`, `src/cmd2.c`, `src/cmd3.c`, `src/cmd5.c`, `src/cmd6.c`, `src/cmd/*` | WP01 |
| WP40 | smithing split | `src/ui/ui-smithing.c`, `src/smithing/*`, `src/ui/smithing/*` | WP20 |
| WP41 | spell/player-song split | `src/spells1.c`, `src/spells2.c`, `src/spell/*`, `src/player/player-song*`, `src/player/player-calc.*`, `src/game-event.*` | WP14, WP20 |
| WP42 | melee split | `src/melee1.c`, `src/melee2.c`, `src/melee/*` | WP30, WP41 |
| WP50 | core/frontend split | `CMakeLists.txt`, SDL-facing files | WP10-WP42 |

Current parallel package status:
- `WP10`: in progress; parser-core, flag/debug helpers, shared object-bonus parsing helpers, and the monster parser have already been landed in the working tree.
- `WP11`, `WP12`, `WP13`, `WP14`, `WP20`, `WP21`, `WP22`, `WP30`, `WP40`, `WP41`, `WP42`, `WP50`: not started.

### Package Rules For Subagents
- The agent working a package should own only the files listed in that package.
- The package should preserve behavior first and not perform unrelated gameplay edits.
- The package should list every file it changed and every public header it added.
- The package should run at least one build before returning.
- If a package needs declarations from another package, add the narrowest possible header, not a new `externs.h` export.

## Validation Matrix

### Must Run After Every Package
- `.\build-incremental.ps1` or `cmake --build build-standard --parallel`
- For big merges, `.\build-cmake.bat`

### Must Smoke-Test For Gameplay/UI Packages
- inventory/equipment overlays
- floor-item `-)` list behavior
- unified look and sidebar
- combat roll overlay
- save/load
- help/file viewer if touched
- metarun and score screens if touched
- touch/compact pane behavior if touched

### Must Smoke-Test For Platform Packages
- desktop SDL launch
- Android build path if `CMakeLists.txt`, `main-sdl.c`, or frontend linkage changed
- config path creation and asset loading

## What Not To Do
- Do not rename content serials or GUIDs in `lib/edit/*`.
- Do not mix refactor commits with balance changes or new features.
- Do not revert unrelated user changes in a dirty worktree.
- Do not push more declarations into `externs.h` just to make a split compile.
- Do not delete `z-term.c` early. Isolate it last.
- Do not attempt a giant all-at-once merge from unstable.

## Recommended Immediate Start
1. `WP01` is already landed in the working tree; `WP00` is only partially captured and still needs the explicit smoke matrix bookkeeping.
2. Continue `WP10`, then run `WP11`, `WP12`, `WP13`, and `WP14` in parallel.
3. Integrate those and then run `WP20`, `WP21`, `WP22`, and `WP30`.
4. After object/cmd foundations settle, run `WP40`, `WP41`, and `WP42`.
5. Reserve `WP99` and `WP50` for the main integrator.

This order keeps the highest-conflict object/combat/UI work off the table until the repo already has the target directory structure and narrower headers.

# Unstable Refactor Port Plan

## Purpose
Port the structural refactor already proven on `unstable` into `quests-and-refactor` without losing the newer gameplay, Android, quest, UI, and drop-system work on the current branch. After the unstable structure is in place, continue past unstable to finish the job: shrink `externs.h`, localize globals from `variable.c`, and separate core game code from SDL-facing code so the codebase is safer for Android now and iOS later.

## Current Status
- Status date: 2026-03-25.
- Current-tree verification on 2026-03-25:
  - `WP60`-`WP63` are complete in the live tree, not just in the written plan.
  - Refactor-generated gameplay body includes under `src/` have now been eliminated:
    - `src/xtra1-body.inc`
    - `src/xtra2-body.inc`
    - `src/spell/spells1-body.inc`
    - `src/spell/spells2-body.inc`
  - The old wrapper entry files are now tiny legacy notes outside the build:
    - `src/xtra1.c`: 11 lines
    - `src/xtra2.c`: 6 lines
    - `src/spells1.c`: 14 lines
    - `src/spells2.c`: 12 lines
  - `src/generate.c` is now also reduced to an 18-line legacy note outside the build; no excluded legacy carryover monolith remains in the tree.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` succeeds on the current tree.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` also succeeds on the current tree.
- The unstable-port milestone is structurally landed, but the overall refactor is not finished completely yet:
  - `WP70` is now landed in the working tree: `src/cmd/ui/cmd-ui-character.c`, `cmd-ui-abilities.c`, `cmd-ui-main-menu.c`, `cmd-ui-settings.c`, `cmd-ui-knowledge.c`, `cmd-ui-nearby.c`, and `src/ui/ui-look-sidebar.[ch]` now own the remaining `cmd4.c` menu/UI families, and `src/cmd4.c` is reduced to the redraw/note/version/feeling/ghost remainder
  - `WP70A` is now landed in the working tree: `src/player/player-abilities.[ch]`, `player-oaths.[ch]`, and `player-bane.[ch]` now own the first gameplay-helper extraction out of `src/cmd4.c`
  - `WP71A` is now landed in the working tree: `src/fs/pref-files.[ch]` plus `src/fs/pref-time.[ch]` now own the pref/time logic extracted from `src/files.c`
  - `WP71B` is now landed in the working tree: `src/ui/ui-character-screen.[ch]` now owns the character sheet, compact-layout rendering, and character-sheet tutorial flow extracted from `src/files.c`, while the legacy block is excluded in place until the later `files.c` cleanup slices finish
  - `WP71C` is now landed in the working tree: `src/fs/savefile-name.[ch]` owns `process_player_name()` and `src/ui/ui-character-name.[ch]` owns `get_name()`
  - `WP71D` is now landed in the working tree: `src/ui/ui-story.[ch]` plus `src/ui/ui-death.[ch]` now own the story, tomb, and death/victory presentation helpers extracted from `src/files.c`
  - `WP71E` is now landed in the working tree: `src/score/score-entry.[ch]` now own `create_score()`, `build_live_preview_score()`, `highscore_is_empty()`, and the remaining score-entry helpers such as the kinslayer scorefile path
  - `WP71F` is now landed in the working tree: `src/runtime/runtime-game.[ch]` now own the save/close/panic/autoload/metarun lifecycle flow that used to live in `src/files.c`
  - `WP71G` is now landed in the working tree: `src/files.c` is reduced to a 486-line facade that only keeps the privilege helpers, escape/suicide commands, and character-dump/mini-screenshot helpers
  - a follow-on utility slice is also landed: `comma_number()` / `atomonth()` now live in `src/format.c`, and `silmarils_possessed()` / `has_iron_crown()` now live in `src/player/player-resources.c`
  - `WP80` is now landed in the working tree: `src/externs.h` now relies on narrow subsystem headers for the `fs/`, `runtime/`, `player/`, `object/`, `score/`, `spell/`, and `ui/` ownership already created by `WP60`-`WP71`, and the obvious combat/item/drop/runtime/level-generation/metarun globals now live in their owning modules instead of `src/variable.c`
  - `WP81` is now landed in the working tree: `src/generate.c` is reduced to a legacy note like the other retired monolith entry files, and the remaining live generate-owner comments now point at the split `src/level-generation/` modules instead
  - there is no remaining active giant frontend monolith; `src/files.c` is down to 486 lines and `src/cmd4.c` is down to 240 lines
  - the current header/global surface is still above the intended end state:
    - `src/externs.h`: 1,074 lines and 882 `extern` declarations
    - `src/variable.c`: 710 lines
  - the next real ownership wave is now `WP90`
- `Phase 1` is effectively complete in the working tree:
  - `CMakeLists.txt` is grouped by subsystem instead of one flat source list.
  - Target scaffold directories now exist under `src/` for `cmd/`, `drop/`, `init/`, `level-generation/`, `melee/`, `object/`, `smithing/`, `spell/`, and `ui/smithing/`.
  - Transitional subsystem headers now own the first extracted declaration blocks for `drop`, `level-generation`, and `smithing` instead of keeping those blocks inline in `externs.h`.
- `Phase 2` is substantially landed through `WP14`, with both `WP10` and `WP11` now structurally complete:
  - `src/init/init-parser-core.c` now owns `parse_tile_line()`, `init_info_txt()`, `add_text()`, and `add_name()`.
  - `src/init/init-flags.c` now owns `info_flags`, `grab_one_flag()`, and `dbg_show_active_flags()`.
  - `src/init/init-object-bonuses.c` now owns the shared object/artefact/ego stat-skill bonus parsing helpers that were previously embedded in `init1.c`.
  - `src/init/init-parse-monster.c` now owns `parse_r_info()` and its local monster flag/blow parsing helpers.
  - `src/init/init-parse-object-kind.c`, `init-parse-ability.c`, `init-parse-artefact.c`, `init-parse-names.c`, `init-parse-skeleton-notes.c`, `init-parse-ego.c`, `init-parse-player.c`, `init-parse-stores.c`, `init-parse-curses.c`, `init-parse-major-blessings.c`, `init-parse-flavor.c`, `init-parse-effects.c`, and `init-parse-quests.c` are now live.
  - `src/init1.c` is now reduced to the legacy note form used on `unstable` and is no longer built.
  - `src/signals.c`, `src/ui/ui-file-viewer.c`, and `src/ui/ui-help.c` now own the `files.c` breakout covered by `WP12`.
  - `src/targeting.c`, `src/player-status.c`, `src/player-xp.c`, and `src/monster-death.c` now own the `xtra2.c` breakout covered by `WP13`.
  - `src/player/ability_log.c`, `src/player/encumbrance.c`, `src/player/identification.c`, and `src/player/weapon_stats.c` now own the `xtra1.c` breakout covered by `WP14`.
  - `src/level-generation/level-generation.c` now replaces `src/generate.c` in the build as the active remaining level-generation core.
  - `src/level-generation/level-generation-layout-anchors.c` now owns `style_at_color()`, `layout_anchor_reset()`, `mark_room_anchor_meta()`, `layout_anchor_capture_existing_rooms()`, and `seed_prefab_anchors()`.
  - `src/level-generation/level-generation-layout.c` now owns `room_kind_is_vault()`, `record_partition_metadata()`, `fallback_partition_grid_from_blocks()`, `area_is_reserved_or_dense()`, `compute_partition_bounds()`, `level_has_chasm_partition()`, `apply_chasm_partition_tags()`, `apply_partition_and_room_glow_rules()`, `scaled_attempts()`, `pick_weighted_mode()`, `mode_weight_for_depth()`, `room_build_in_bounds()`, `place_room_with_budget()`, `cave_set_feat_style()`, `scatter_quartz_veins_in_bounds()`, `bounds_have_chasm_tag()`, `carve_ca_blob_anchor_bounds()`, `carve_bsp_slice_anchor_bounds()`, `prune_big_cave_detached_components()`, and `carve_big_cave_bounds()`, with those helpers no longer duplicated in `level-generation.c`.
  - `src/level-generation/level-generation-big-cave.c`, `level-generation-connectivity.c`, `level-generation-layout-morgoth.c`, `level-generation-quests.c`, `level-generation-rooms.c`, `level-generation-screen.c`, `level-generation-state.c`, and `level-generation-terrain.c` are now live.
  - `src/level-generation/level-generation.c` is now reduced to the intended top-level driver/orchestration layer (`cave_gen()`, `gates_gen()`, `throne_gen()`, `spawn_niena_morgoth_hall()`, `unring_a_bell()`, and `generate_cave()`), matching the role that `unstable` kept in its final `level-generation.c`.
- `Phase 3` object and drop foundations are now structurally landed through `WP22` in the working tree:
  - `src/object/object-desc.[ch]`, `object-display.[ch]`, `object-flavor.[ch]`, `object-flags.[ch]`, `object-slot.[ch]`, `object-util.[ch]`, and `object-ui-display.[ch]` now own the first mechanical extractions from `object1.c`, including the classic inventory/equipment/floor list rendering path.
  - `src/object/object-ui-enhanced.[ch]`, `object-ui-identify.[ch]`, and `object-ui-select.[ch]` now own the larger object UI flow from `object1.c`, including the enhanced inventory/equipment menus, the unified identify menu, and the `get_item()`/selection path.
  - `src/object/object-allocation.c`, `object-inventory.c`, `object-knowledge.c`, `object-list.c`, `object-make.c`, and `object-place.c` now own the former `object2.c` monolith, and `object2.c` is reduced to a legacy note outside the build.
  - `src/drop/drop-system-catalog.c`, `drop-system-difficulty.c`, and `drop-system-selection.c` now own the former `drop_system.c` monolith, `src/drop_system.c` is reduced to the intended thin facade, and `src/drop_system.h` now carries the public drop API outside the old inline `externs.h` declaration block.
- `Phase 4` command work is now structurally complete through `WP30` in the working tree:
  - `src/cmd/combat/cmd-combat-rolls.c`, `cmd-combat.c`, and `cmd-ranged.c` now own the former `cmd1.c`/`cmd2.c` combat command flow.
  - `src/cmd/item/cmd-fletchery.c`, `cmd-identify.c`, `cmd-item-activate.c`, `cmd-item-core.c`, `cmd-item-drop.c`, `cmd-item-utility.c`, and `cmd-pickup.c` now own the former item command flow from `cmd1.c`, `cmd2.c`, `cmd3.c`, and `cmd6.c`.
  - `src/cmd/monster/cmd-monster-alert.c`, `src/cmd/movement/cmd-movement.c`, `cmd-search.c`, `cmd-travel.c`, `src/cmd/ui/cmd-ui-look.c`, `cmd-ui-object-display.c`, `cmd-ui-query.c`, and `src/cmd/world/cmd-interact.c` now own the remaining command families from the old monoliths.
  - `src/cmd1.c`, `src/cmd2.c`, `src/cmd3.c`, `src/cmd5.c`, and `src/cmd6.c` are now reduced to legacy notes outside the build.
- `Phase 5` is now structurally complete through `WP42` in the working tree:
  - `src/smithing/smithing-state.c`, `smithing-materials.c`, `smithing-difficulty.c`, `smithing-cost.c`, and `smithing-item.c` now own the smithing state, alloy/material helpers, difficulty/cost logic, artefact-finalization helpers, and create/finalize path that previously lived inside `cmd4.c`.
  - `src/ui/smithing/ui-smithing-screen.c` now owns the active smithing UI flow from the current branch, including the create/enchant/artefact/numbers/melt/repair-reforge screens.
  - `src/cmd4.c` no longer owns smithing and is reduced to the remaining non-smithing menu/UI code.
  - `src/spell/spell-damage.c`, `spell-detection.c`, `spell-monster.c`, `spell-projection.c`, `spell-teleport.c`, `spell-terrain.c`, and `spell-utility.c`, plus `src/player/player-songs.c`, `player-song-disguise.c`, `player-song-duels.c`, `player-song-effects.c`, `player-calc.c`, and `src/game-event.c`, now own the former `spells1.c`/`spells2.c` monoliths plus the extracted player-song/player-calc/game-event helpers; `spells1.c` and `spells2.c` are reduced to legacy notes outside the build.
  - `src/melee/melee-attack.c`, `melee-combat-display.c`, `melee-movement.c`, `melee-process.c`, and `melee-util.c` now own the former `melee1.c`/`melee2.c` monoliths, and `melee1.c` plus `melee2.c` are reduced to legacy notes outside the build.
- `Phase 6/7` is now structurally complete through `WP50` in the working tree:
  - `CMakeLists.txt` now builds `sil-core` as the static gameplay core archive, `sil-platform-sdl` as the SDL-facing object target, and keeps `sil-more` as the thin launcher/app target.
  - Standard and portable builds now both link the same `sil-core` target instead of compiling the full source set directly into `sil-more`.
  - `src/platform-ui.h`, `src/platform-audio.h`, `src/gamepad-config.h`, and `src/pane-config.h` now form the core-facing platform boundary; `src/main-sdl.h` plus `src/sdl-sound.h` remain as thin compatibility wrappers for the SDL implementation side.
  - `src/pane.h` is now SDL-only layout glue, while generic pane enums/config now live in `src/pane-config.h`, and public gamepad/touch-pane constants no longer depend on SDL headers.
  - `src/angband.h` no longer pulls in SDL globally, `src/support/strl.[ch]` now own the `SDL_strlcpy`/`SDL_strlcat` compatibility layer, and the remaining explicit SDL includes are now localized to the files that actually need SDL types or runtime calls.
- `WP99` is now structurally complete in the working tree:
  - `src/util.c` now owns the transient `inkey` base/scan/flush/command/cursor-hidden state behind helper functions, so `variable.c` and `externs.h` no longer expose those globals directly.
  - Menu/story/smithing/birth callers now use `inkey_set_cursor_hidden()` / `inkey_cursor_hidden()` instead of writing `hide_cursor` directly.
  - `src/ui/colors.c` now owns the background-color policy behind `ui_colors_use_backgrounds()`, so `cave.c` no longer reads `use_background_colors` directly.
  - `src/runtime-cli.[ch]` now own the runtime CLI flag state, so the old `arg_*` globals no longer live in `variable.c` / `externs.h`.
  - `src/project-path.h` plus the new `projectable_with_ignore()` path now replace the old `project_path_ignore*` globals with per-call projectile masking for monster ranged-position scoring.
- `WP60` is now structurally complete in the working tree:
  - `src/player/player-calc.c`, `src/player/player-resources.c`, `src/player/player-songs.c`, and `src/ui/ui-status.c` now own the former active `xtra1` body sections as normal translation units.
  - `src/xtra1.c` is now reduced to a legacy note outside the build.
  - `src/xtra1-body.inc` has been removed from the tree.
- `WP61` is now structurally complete in the working tree:
  - `src/game-event.c` plus `src/quest/quest-rewards.c`, `quest-ui.c`, `quest-tulkas.c`, `quest-varda.c`, `quest-niena.c`, `quest-orome.c`, and `quest-valar.c` now own the former active `xtra2` body sections as normal translation units.
  - `src/xtra2.c` is now reduced to a legacy note outside the build.
  - `src/xtra2-body.inc` has been removed from the tree.
- `WP62` is now structurally complete in the working tree:
  - `src/spell/*` plus `src/player/player-song-*.c` now own the former active `spells1` body sections as normal translation units.
  - `src/spell/spells1-body.inc` has been removed from the tree.
- `WP63` is now structurally complete in the working tree:
  - the split `src/spell/*` modules now own the former active `spells2` body sections as normal translation units.
  - `src/spell/spells2-body.inc` has been removed from the tree.
- Validation so far:
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` succeeded after the scaffold landing and again after the `init/` extractions.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` succeeded again after the first `WP20` object-module extraction, again after the `object-desc` extraction, again after the first `object-ui-display` helper extraction, again after moving the classic list rendering path into `object-ui-display`, again after moving the enhanced/unified object menus into dedicated modules, again after moving the selection flow into `object-ui-select`, again after removing `object1.c` from the build as a completed legacy note, again after landing the `WP11` `level-generation-connectivity.c` and `level-generation-rooms.c` split, again after moving the remaining layout/planning core into `level-generation-layout.c`, again after landing `level-generation-screen.c` plus the final state/helper moves, again after completing the remaining `WP10` parser-module split and removing `init1.c` from the build, again after revalidating the active `WP22` drop split with the thin `drop_system.c` facade on 2026-03-25, again after the first `WP30` command-module extraction on 2026-03-25, and again after completing the full `WP30` command split with the new `src/cmd/*` build on 2026-03-25.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` succeeded again after landing the active `WP40` smithing split on 2026-03-25.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` succeeded again after landing the active `WP41` spell/player-song split and revalidating the `WP42` melee split on 2026-03-25.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` succeeded again after landing the first mechanical `WP50` core/frontend target split on 2026-03-25, and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` succeeded on the portable tree the same day.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` succeeded again after finishing the `WP50` SDL header/API boundary cleanup on 2026-03-25, and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` succeeded again on the portable tree the same day.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` succeeded again after starting the first `WP99` header/global cleanup slice on 2026-03-25.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` succeeded again after completing the remaining `WP99` projectile-path/runtime-CLI cleanup on 2026-03-25, and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` succeeded on the portable tree the same day.
  - `.\build-cmake.bat` succeeded again after completing `WP99` on 2026-03-25.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` succeeded again after completing `WP61` on 2026-03-25, and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` succeeded on the portable tree the same day.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` both succeeded again during the 2026-03-25 current-tree revalidation that confirmed `WP60`-`WP63` are still landed and that `cmd4.c` plus `files.c` remain the last big live monoliths.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` both succeeded again after landing `WP70A` on 2026-03-25.
- `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` both succeeded again after landing `WP71A` on 2026-03-25.
- `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` both succeeded again after landing `WP71B` on 2026-03-25.
- `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` both succeeded again after landing `WP71C` on 2026-03-25.
- `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` both succeeded again after completing the remaining `WP71E`-`WP71G` score/runtime/files cleanup on 2026-03-25.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` both succeeded again after the follow-on `files.c` utility extraction on 2026-03-25.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` both succeeded again after completing `WP70` on 2026-03-25.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` both succeeded again after completing `WP80` on 2026-03-25.
  - `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1` and `powershell -ExecutionPolicy Bypass -File .\\build-incremental.ps1 -Target portable` both succeeded again after completing `WP81` on 2026-03-25.

## Completion Assessment
- The original unstable-port objective is effectively complete as an intermediate architecture:
  - the grouped subsystem build graph is live
  - most former monolith entry files are already reduced to legacy notes outside the build
  - standard and portable builds both pass on the new target layout
- The broader refactor is not complete:
  - the `WP60`-`WP63` body-include cleanup wave is complete and should not be reopened except for fallout fixes from later moves
  - the `WP71` `files.c` split is now complete and no longer blocks the next cleanup wave
  - the `WP80` second header/global cleanup pass and `WP81` legacy carryover cleanup are now complete, but `WP90` platform-boundary follow-through is still outstanding
- Priority judgement for the next wave:
  - move directly to `WP90` now that the excluded legacy carryover cleanup is complete
  - keep `WP90` focused on the mobile/platform boundary follow-through instead of reopening broad `externs.h` / `variable.c` churn
  - treat any further header/global edits as narrow fallout fixes only, not as another large cleanup wave
- Lower priority:
  - keep the remaining legacy note stubs as historical breadcrumbs unless a later cleanup needs to remove them for packaging or tooling reasons

## Branch Facts
- Current working branch studied for this plan: `quests-and-refactor` at `750b4e601b1c2afba2ec7f573763e1a2ed22e367`.
- Refactor reference branch studied for this plan: `unstable` at `ce8d936796ba509c6aedf661f45f9e9e3eab8da9`.
- Merge base between them: `4ab3ad887a6e2fc5746d45c6977305c7e9af52e4`.
- `unstable` and `quests-and-refactor` diverged in parallel. `unstable` is not an ancestor of the current branch, and the current branch is not an ancestor of `unstable`.
- Consequence: do not blindly cherry-pick unstable refactor commits. Use them as a structural reference and port each subsystem onto the current branch with three-way diffing.

## Evidence From The Current Tree
- Remaining active refactor debt as of 2026-03-25, revalidated against the live tree:
  - Active refactor-generated code-body includes:
    - none under `src/`
  - Legacy note wrappers now outside the build:
    - `src/generate.c`: 18 lines
    - `src/xtra1.c`: 11 lines
    - `src/xtra2.c`: 6 lines
    - `src/spells1.c`: 14 lines
    - `src/spells2.c`: 12 lines
  - Largest active built catch-all files:
    - `src/files.c`: 486 lines
    - `src/cmd4.c`: 240 lines
  - Excluded legacy carryover monoliths still present in the tree:
    - none
  - Current global/header surface:
    - `src/externs.h`: 1,074 lines and 882 `extern` declarations
    - `src/variable.c`: 710 lines
- Original baseline hotspot snapshot that motivated the port:
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
11. Do not introduce new code-bearing `*.inc` files under `src/`. The current body includes are transitional debt to remove, not a pattern to extend.

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
| `files.c` | `src/signals.c`, `src/ui/ui-file-viewer.c`, `src/ui/ui-help.c`, then split the remaining non-UI filesystem/runtime work into clearer `src/fs/`, `src/ui/`, and score/runtime-owned modules |
| `xtra1.c` | `src/player/ability_log.c`, `encumbrance.c`, `identification.c`, `weapon_stats.c`, then finish the remainder in `src/player/player-resources.c` and `src/ui/ui-status.c` |
| `xtra2.c` | `src/targeting.c`, `src/player-status.c`, `src/player-xp.c`, `src/monster-death.c`, `src/game-event.c`, then finish the remaining quest/UI code under a new `src/quest/` directory |
| `spells1.c`, `spells2.c` | `src/spell/` plus `src/player/player-songs*.c`, `player-calc.c`, `game-event.c`, with the temporary `spells*-body.inc` ownership removed into normal translation units |
| `melee1.c`, `melee2.c` | `src/melee/` |
| `ui/ui-smithing.c` | `src/smithing/` and `src/ui/smithing/` |
| `cmd4.c` | additional `src/cmd/ui/` family files plus `src/ui/` render/support helpers instead of one catch-all command/UI source |

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
- In progress as of 2026-03-25.
- `WP10` is structurally landed: the remaining parser modules now live under `src/init/`, `init1.c` is reduced to the legacy note, and the build compiles without `init1.c`.
- `WP11` is structurally landed: `level-generation-layout-anchors.c`, `level-generation-layout.c`, `level-generation-connectivity.c`, `level-generation-rooms.c`, `level-generation-screen.c`, `level-generation-big-cave.c`, `level-generation-layout-morgoth.c`, `level-generation-quests.c`, `level-generation-state.c`, and `level-generation-terrain.c` are now live, and `level-generation.c` is reduced to the intended top-level driver/gates flow.
- `WP12`, `WP13`, and `WP14` are structurally landed in the working tree and compile on the current branch.

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
- Keep the smithing-analysis sync rule in place: if the smithing difficulty logic in `src/drop/drop-system-difficulty.c` changes, update `scripts/calc_artefact_difficulty.py` in the same changeset.

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
- Remove refactor-generated `*.inc` body includes by moving each section into real owning `.c` files.
- Reduce or delete compatibility wrappers that only existed to include those transitional bodies.
- Split the remaining active catch-all files that still block ownership (`cmd4.c`, `files.c`).
- Collapse duplicated declarations that exist in both `externs.h` and new subsystem headers.
- Tighten `const` correctness and return types.
- Trim any empty or one-function modules that do not deserve their own file after the dust settles.
- Convert excluded legacy carryover monoliths into true notes only after active code no longer depends on transitional bodies.

Done when:
- No code-bearing `*.inc` file remains under `src/`.
- `cmd4.c` and `files.c` are reduced to small facades or legacy notes.
- Transitional declaration duplication is removed from `externs.h`.

### Phase 9: De-Inc The Active Split Modules
Goal: replace the temporary include-body refactor pattern with normal C translation units and clear subsystem ownership.

Tasks:
- `xtra1` remainder:
  - Move status-line, frame, redraw, and `handle_stuff()` ownership into `src/ui/`.
  - Move `calc_voice()`, `calc_hitpoints()`, `calc_torch()`, and other player-resource calculations into dedicated `src/player/` modules.
  - Delete `src/xtra1-body.inc` and reduce `src/xtra1.c` to a note or remove it from the build.
- `xtra2` remainder:
  - Move quest interaction, quest reward, and quest UI/typewriter logic into a new `src/quest/` directory with narrow headers.
  - Keep only genuinely generic narrative/event helpers in `src/game-event.c`.
  - Delete `src/xtra2-body.inc` and reduce `src/xtra2.c` to a note or remove it from the build.
- `spells1`/`spells2` remainder:
  - Move each body section into the owning `src/spell/*.c` or `src/player/player-song*.c` file.
  - Add one `spell-internal.h` private header only if shared private declarations are truly needed.
  - Delete `src/spell/spells1-body.inc` and `src/spell/spells2-body.inc`.

Rules:
- This phase is mechanical first. No gameplay rebalance mixed into the move.
- Prefer real private headers plus `static` helpers over macro-selected body includes.
- Preserve mobile-friendly boundaries: no SDL leakage back into gameplay modules.

Done when:
- `rg --files -g '*.inc' src` no longer returns gameplay-owned code-body includes.

### Phase 10: Finish The Remaining Live Monolith Splits
Goal: remove the last large active files that still own too many unrelated behaviors.

Tasks:
- Before moving menus, extract the non-command gameplay helpers that still live in `src/cmd4.c`:
  - `src/player/player-abilities.[ch]` for `abilities_in_skill()`, `prereqs()`, and related ability-prerequisite bookkeeping
  - `src/player/player-oaths.[ch]` for `chosen_oath()` / `oath_invalid()`
  - `src/player/player-bane.[ch]` for bane bonus helpers now used by combat, movement, world, score, and quest code
- Split the remaining command/UI entry points into additional `src/cmd/ui/` families:
  - `cmd-ui-character.c`
  - `cmd-ui-abilities.c`
  - `cmd-ui-main-menu.c`
  - `cmd-ui-settings.c`
  - `cmd-ui-keybinds.c`
  - `cmd-ui-visuals.c`
  - `cmd-ui-knowledge.c`
  - `cmd-ui-nearby.c`
- Move pure rendering/browser helpers out of command files into `src/ui/` when they no longer need command-state ownership:
  - `src/ui/ui-look-sidebar.[ch]`
  - `src/ui/ui-knowledge-browser.[ch]` if the browser render/state surface proves large enough to justify its own module
- Split `src/files.c` by concern, preferring existing subsystem directories before inventing new ones:
  - `src/fs/` for pref-file parsing, time restrictions, and savefile/name helpers
  - `src/ui/` for character/tutorial/story/tomb screens
  - `src/score/` for score entry, live preview, and score-formatting helpers
  - a new `src/runtime/` directory only if the close-game / death / autoload / metarun lifecycle flow cannot be housed cleanly in existing subsystems

Validation focus:
- character sheet and compact layouts
- options/keybind/controller menus
- knowledge browser and nearby-object/monster views
- story/tomb/death flow
- save/close-game path

Done when:
- `cmd4.c` is no longer a general UI catch-all.
- `files.c` no longer mixes pref parsing, player display, story screens, score entry, and shutdown flow in one file.

### Phase 11: Header And Global Cleanup Round 2
Goal: capitalize on the real module ownership created by Phases 9 and 10.

Tasks:
- Remove declarations from `externs.h` that now have obvious subsystem homes.
- Move any remaining file-local or subsystem-local globals out of `variable.c`.
- Replace ad hoc cross-subsystem includes with narrow public headers and private `*-internal.h` files.

Near-term targets after this phase:
- `src/externs.h` under 1,100 lines
- `src/variable.c` under 650 lines

Longer-term targets:
- keep the original ambition of driving `externs.h` below 600 lines
- keep driving `variable.c` toward true process-wide state only

### Phase 12: Mobile-Friendly Boundary Follow-Through
Goal: keep Android and future iOS support as a first-class architectural constraint.

Tasks:
- Ensure Phases 9-11 do not reintroduce SDL includes into gameplay modules.
- Push more config/path/input/story-font glue toward platform-facing or UI-facing layers where that improves portability.
- Revisit `z-term.c` isolation only after the de-inc and monolith-split work is stable.
- Add an extra Android smoke path whenever `CMakeLists.txt`, `z-term.c`, platform headers, or core/frontend linkage changes.

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
- `WP00`: marked complete in `session_notes.md` on 2026-03-25.
- `WP99`: completed in the working tree on 2026-03-25; the package localized the planned transient input, UI color, projectile-path, and runtime CLI state out of `variable.c` / `externs.h`, and passed standard + portable incremental builds plus `.\build-cmake.bat`.

### Parallel Packages
These can be run in parallel once `WP01` lands, as long as write sets stay disjoint:

| ID | Scope | Main files | Depends on |
| --- | --- | --- | --- |
| WP10 | `init/` split | `src/init1.c`, `src/init/*` | WP01 |
| WP11 | `level-generation/` split | `src/level-generation/level-generation.c`, `src/level-generation/*` | WP01 |
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
- `WP10`: completed in the working tree on 2026-03-25; the full `src/init/` parser split is landed and `init1.c` is reduced to a legacy note outside the build.
- `WP11`: completed in the working tree on 2026-03-25; the remaining `level-generation.c` file is now only the intended top-level driver/orchestration layer.
- `WP12`: completed in the working tree on 2026-03-24.
- `WP13`: completed in the working tree on 2026-03-24.
- `WP14`: completed in the working tree on 2026-03-24.
- `WP20`: completed in the working tree on 2026-03-24; `object1.c` has been reduced to a legacy note and removed from the build.
- `WP21`: completed in the working tree on 2026-03-24; `object2.c` has been reduced to a legacy note and the split object2 modules are active in the build.
- `WP22`: completed in the working tree on 2026-03-25; the split drop catalog/difficulty/selection modules are active in the build, `drop_system.c` is reduced to the intended thin facade, and the public drop API now lives in `drop_system.h`.
- `WP30`: completed in the working tree on 2026-03-25; the full `src/cmd/*` split is active in the build, and `cmd1.c`, `cmd2.c`, `cmd3.c`, `cmd5.c`, and `cmd6.c` are reduced to legacy notes outside the build.
- `WP40`: completed in the working tree on 2026-03-25; smithing no longer lives in `cmd4.c`, the split smithing core now builds from `src/smithing/`, and the current branch's create/enchant/artefact/numbers/melt/repair-reforge UI now runs from `src/ui/smithing/ui-smithing-screen.c`.
- `WP41`: completed in the working tree on 2026-03-25; the split spell/player-song/player-calc/game-event modules are active in the build, and `spells1.c` plus `spells2.c` are reduced to legacy notes outside the build.
- `WP42`: completed in the working tree on 2026-03-25; the full `src/melee/*` split is active in the build, and `melee1.c` plus `melee2.c` are reduced to legacy notes outside the build.
- `WP50`: completed in the working tree on 2026-03-25; the target split is live in `CMakeLists.txt`, the SDL-facing public surface is now funneled through `platform-ui.h`/`platform-audio.h` plus SDL-free pane/gamepad config headers, `angband.h` no longer imports SDL globally, and both standard plus portable incremental builds pass with the shared `sil-core` archive.

### Next-Wave Parallel Packages
These are the recommended continuation packages after the unstable-port milestone.

- `WP60`: completed in the working tree on 2026-03-25; `xtra1` no longer depends on a body include, the live code now sits in `src/player/player-calc.c`, `src/player/player-resources.c`, `src/player/player-songs.c`, and `src/ui/ui-status.c`, and standard plus portable incremental builds both pass.
- `WP61`: completed in the working tree on 2026-03-25; `xtra2` no longer depends on a body include, the live code now sits in `src/game-event.c` plus `src/quest/*`, `src/xtra2.c` is a legacy note outside the build, and standard plus portable incremental builds both pass.
- `WP62`: completed in the working tree on 2026-03-25; `spells1` no longer depends on a body include, the live code now sits in `src/spell/*` plus `src/player/player-song-*.c`, and `src/spell/spells1-body.inc` has been removed from the tree.
- `WP63`: completed in the working tree on 2026-03-25; `spells2` no longer depends on a body include, the live code now sits in the owning split spell modules, and `src/spell/spells2-body.inc` has been removed from the tree.
- `WP70`: completed in the working tree on 2026-03-25; `src/cmd/ui/cmd-ui-character.c`, `cmd-ui-abilities.c`, `cmd-ui-main-menu.c`, `cmd-ui-settings.c`, `cmd-ui-knowledge.c`, `cmd-ui-nearby.c`, and `src/ui/ui-look-sidebar.[ch]` now own the remaining active `cmd4.c` command/UI families, and `src/cmd4.c` is reduced to the redraw/note/version/feeling/ghost remainder.
- `WP70A`: completed in the working tree on 2026-03-25; the first gameplay-helper eviction out of `src/cmd4.c` is live in `src/player/player-abilities.[ch]`, `src/player/player-oaths.[ch]`, and `src/player/player-bane.[ch]`, while the remaining bane/oath/ability menus stay in `cmd4.c` for the later UI-family splits.
- `WP71A`: completed in the working tree on 2026-03-25; `src/fs/pref-files.[ch]` plus `src/fs/pref-time.[ch]` now own the extracted pref/time logic, and the old `files.c` block is now excluded while the wider `files.c` breakup continues.
- `WP71B`: completed in the working tree on 2026-03-25; `src/ui/ui-character-screen.[ch]` now owns the extracted character-sheet, compact-layout, and tutorial rendering path, direct callers now include the new UI header, and the old `files.c` block is excluded while the wider `files.c` breakup continues.
- `WP71C`: completed in the working tree on 2026-03-25; `src/fs/savefile-name.[ch]` now owns `process_player_name()`, `src/ui/ui-character-name.[ch]` now owns `get_name()`, and `files.c` no longer owns the player-name/savefile prompt path.
- `WP71D`: completed in the working tree on 2026-03-25; `src/ui/ui-story.[ch]` plus `src/ui/ui-death.[ch]` now own the extracted story, tomb, and death/victory presentation helpers from `src/files.c`.
- `WP71E`: completed in the working tree on 2026-03-25; `src/score/score-entry.[ch]` now own score creation/live-preview/submission ownership plus the remaining score-entry helpers that used to live in `src/files.c`, and `clear_scorefile()` now lives in `src/score/score_io.c`.
- `WP71F`: completed in the working tree on 2026-03-25; `src/runtime/runtime-game.[ch]` now own the save/close/panic/autoload/metarun lifecycle flow that used to live in `src/files.c`.
- `WP71G`: completed in the working tree on 2026-03-25; `src/files.c` is now reduced to the small privilege/escape/dump/screenshot facade, and the former score/runtime ownership no longer lives there.

| ID | Scope | Main files | Depends on |
| --- | --- | --- | --- |
| WP60 | `xtra1` de-inc | `src/xtra1.c`, `src/xtra1-body.inc`, `src/player/player-calc.*`, `src/player/player-songs.*`, new `src/player/player-resources.*`, new `src/ui/ui-status.*` | WP14, WP41, WP50 |
| WP61 | `xtra2` de-inc + quest ownership | `src/xtra2.c`, `src/xtra2-body.inc`, `src/game-event.*`, new `src/quest/*` | WP13, WP50 |
| WP62 | `spells1` de-inc | `src/spell/spells1-body.inc`, `src/spell/*`, `src/player/player-song-*.c` | WP41 |
| WP63 | `spells2` de-inc | `src/spell/spells2-body.inc`, `src/spell/*` | WP41 |
| WP70 | `cmd4` split + gameplay helper eviction | `src/cmd4.c`, new/existing `src/cmd/ui/*`, new `src/player/player-abilities.*`, `player-oaths.*`, `player-bane.*`, any new `src/ui/*` helpers needed by the split | WP30, WP40, WP60-WP63 |
| WP71 | `files` split + score/lifecycle breakup | `src/files.c`, `src/fs/*`, `src/ui/*`, `src/score/*`, and only if justified new `src/runtime/*` lifecycle modules | WP12, WP50, WP60-WP63 |

Recommended `WP60` split targets:
- `src/ui/ui-status.[ch]`: status lines, compact vitals, frame drawing, health formatting/attributes, plus the refresh/window flow that still shares a large private helper surface with status rendering
- `src/player/player-resources.[ch]`: `calc_torch()`, weapon glow helpers, and related light/resource helpers

Recommended `WP61` split targets:
- `src/quest/quest-ui.[ch]`: quest status screen, typewriter menu, wrapped quest text helpers
- `src/quest/quest-rewards.[ch]`: generic reward/follow-up reward plumbing
- `src/quest/quest-tulkas.c`, `quest-varda.c`, `quest-niena.c`, `quest-orome.c`, `quest-valar.c`: per-quest interaction ownership

Recommended `WP62`/`WP63` rule:
- Prefer moving each section into the module that already owns its public header.
- Only introduce a shared private spell helper module when multiple current spell files already depend on the same private behavior.

Detailed `WP70` split sequence:
- `WP70A`: extract the non-UI gameplay helpers out of `src/cmd4.c` first:
  - `src/player/player-abilities.[ch]`
  - `src/player/player-oaths.[ch]`
  - `src/player/player-bane.[ch]`
- `WP70B`: move character-sheet command flow into `src/cmd/ui/cmd-ui-character.c`, keeping pure rendering in `src/ui/` once `WP71B` exposes stable headers.
- `WP70C`: move songs, ability menus, oath/bane choice menus, and `do_cmd_ability_screen()` / `do_cmd_change_song()` into `src/cmd/ui/cmd-ui-abilities.c`.
- `WP70D`: move main-menu, hint-message, and message-recall ownership into `src/cmd/ui/cmd-ui-main-menu.c` plus a small dedicated message module if that keeps the write set cleaner.
- `WP70E`: move options, pane settings, controller settings, keybinds, and macros into `src/cmd/ui/cmd-ui-settings.c` plus `src/cmd/ui/cmd-ui-keybinds.c`.
- `WP70F`: move visuals/colors ownership into `src/cmd/ui/cmd-ui-visuals.c`, keeping palette-edit helpers together.
- `WP70G`: move knowledge browser, nearby views, and unified-look sidebar ownership into `src/cmd/ui/cmd-ui-knowledge.c`, `src/cmd/ui/cmd-ui-nearby.c`, and `src/ui/ui-look-sidebar.[ch]`.
- `WP70H`: reduce `src/cmd4.c` to a thin facade or legacy note, then remove any transitional `externs.h` exports that only existed for the monolith.

Detailed `WP71` split sequence:
- `WP71A`: extract pref parsing and time restrictions into `src/fs/pref-files.[ch]` and, if it earns its keep, `src/fs/pref-time.[ch]`.
- `WP71B`: extract the character sheet, compact layout, and tutorial path into `src/ui/ui-character-screen.[ch]`.
- `WP71C`: extract savefile/name normalization into `src/fs/savefile-name.[ch]`; keep `get_name()` in UI only if it still owns the prompt loop.
- `WP71D`: extract story, tomb, and death/victory presentation into `src/ui/ui-story.[ch]` and `src/ui/ui-death.[ch]`.
- `WP71E`: move `create_score()`, `build_live_preview_score()`, score-formatting helpers, and score-table presentation helpers into existing `src/score/*` modules or a narrow new `src/score/score-entry.[ch]`.
- `WP71F`: move close-game, save, panic-exit, autoload, and metarun score/save cleanup out of `files.c`; prefer existing subsystems first and create `src/runtime/` only if the lifecycle flow still has no coherent home.
- `WP71G`: reduce `src/files.c` to a small facade or legacy note and replace its `externs.h` block with narrow subsystem headers.

Recommended execution order for the next wave:
- With `WP70`, `WP71`, `WP80`, and `WP81` now complete, move directly into `WP90`.
- Do not reopen `WP60`-`WP63` or `WP71` except for fallout fixes directly caused by later cleanup.

### Next-Wave Serial Packages
These are best kept with the main integrator after the next parallel wave lands.

| ID | Scope | Main files | Depends on |
| --- | --- | --- | --- |
| WP80 | header/global cleanup round 2 | `src/externs.h`, `src/variable.c`, subsystem headers | WP60-WP71 |
| WP81 | legacy carryover cleanup | excluded legacy monoliths such as `src/generate.c` and any remaining wrapper notes | WP60-WP71 |
| WP90 | mobile/platform boundary follow-through | `CMakeLists.txt`, platform headers, `src/z-term.c`, frontend/core boundary files | WP80, WP81 |

- `WP80`: completed in the working tree on 2026-03-25; the package replaced large duplicate `externs.h` blocks with narrow subsystem headers, moved the obvious combat/item/drop/runtime/level-generation/metarun globals out of `variable.c`, removed dead leftover state, and passed standard + portable incremental builds.
- `WP81`: completed in the working tree on 2026-03-25; `src/generate.c` is now a legacy note outside the build like the other retired monolith entry files, the last live generate-owner comments now point at the split `src/level-generation/` surface, and standard plus portable incremental builds both pass.

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
- Do not add new code-bearing `*.inc` files under `src/`.
- Do not delete `z-term.c` early. Isolate it last.
- Do not attempt a giant all-at-once merge from unstable.

## Recommended Immediate Start
1. With `WP80` and `WP81` complete, move to `WP90` as the main integrator pass for the remaining mobile/platform boundary work.
2. Treat any remaining `WP80`/`WP81`-area edits as narrow fallout fixes only.

This order starts with the helper extractions that create clean ownership boundaries, then attacks the last live monoliths, and only then spends the big integrator effort on `externs.h`, `variable.c`, and the next mobile/platform cleanup pass.

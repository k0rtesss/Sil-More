# Port Plan: `New-quest-structure` to `develop`

## Goal

Port only the new quests and quest structure from `origin/New-quest-structure` onto current `origin/develop`.

Do not merge the old branch tip. Current `develop` is much newer and contains quest, generation, save/load, metarun, UI, and gameplay fixes that must remain authoritative. Treat the old branch as a source of quest-specific design and implementation patches to reapply manually.

## Branch Baseline

- Target branch: `origin/develop` (`437a9384`, fetched during planning).
- Source branch: `origin/New-quest-structure` (`8ff00b01`).
- Merge base: `9994c474fff16f7400e0682098d9103e26bfda5e`.
- Current `develop` version at planning time: `0.9.7.1` in `src/defines.h`.
- Source branch save gates use old `0.9.1.10` assumptions; do not copy those gates directly.
- Quest-structure source commits to mine:
  - `76dab818` `new quest structure first update`
  - `c11e858a` `mandos all quests`
  - `0ca7519d` `added orome 2nd quest and challenge`
  - `224775ea` `add Orome quests`
  - `f55fe8d3` `add 2nd Nienna's quest`
  - `840ec563` `implement 3rd Nienna's quest`
  - `ad52b57e` `implement 2nd Tulkas's quest`
  - `e064c848` `implement Tulkas's 3rd quest`
  - `02cb6c1a` `implement Varda's 2nd quest`
  - `0650988e` `implement Varda's 3rd quest`
  - `cc838e6f` `Update generate.c`
  - `06f0648d` `resolves issues with save load`
  - `8ff00b01` `updates to quest texts`

Ignore the source branch merge commits from older `develop` history except when they explain local context.

## Phase 0 Implementation Record

Status: completed by the top agent on branch `port-new-quest-structure`.

Branch setup:

- Created `port-new-quest-structure` from local `develop`.
- Verified local `develop` and `origin/develop` both point to `437a93841c7b0f2c8146515d0c037fc2267c27f2`.
- Verified source branch `origin/New-quest-structure` points to `8ff00b01df43dbc897b8097755fc6aa1362c2fd3`.
- Verified merge base remains `9994c474fff16f7400e0682098d9103e26bfda5e`.
- Worktree at setup had no code changes; only this untracked planning file existed.

Reference capture:

- Full source log command captured: `git log --right-only --cherry-pick --oneline origin/develop...origin/New-quest-structure`.
- Full source file list command captured: `git diff --name-status origin/develop...origin/New-quest-structure`.
- Quest-critical diff stat command captured over the planned implementation files. Result: 27 quest-critical files, 3630 insertions, 447 deletions.
- Full branch diff also shows out-of-scope files: `.claude/*`, `.gitignore`, `session_notes.md`, `src/main-sdl.c`, `src/wizard2.c`, and `src/z-term.c`.

Compatibility gate decision:

- Current target version is `0.9.7.1`.
- Persistent quest-state implementation should bump `VERSION_STRING` to `0.9.7.2` and `VERSION_EXTRA` to `2`.
- New save reads for added quest fields should use `savefile_version_at_least(0, 9, 7, 2)`.
- Do not use source branch gates like `savefile_version_at_least(0, 9, 1, 10)`.
- Keep `MIN_VERSION_EXTRA` at `0` unless a later implementation step intentionally drops support for older `0.9.x` saves.
- Metarun file version follows `VERSION_*`; expanding `METARUN_QUEST_SLOT_MAX` from 8 to 24 must include a compact legacy reader for existing 8-slot metarun files.

## Phase 1 Implementation Record

Status: completed by the top agent after integrating three subagents.

Subagents used:

- Worker A `Copernicus`: `gpt-5.5`, xhigh reasoning. Owned quest schema/parser files because the slice affects version constants, struct layout, and template parsing.
- Worker B `Hubble`: `gpt-5.5`, xhigh reasoning. Owned metarun and quest accounting because the slice affects persistence compatibility and metarun scoring.
- Worker C `Boyle`: `gpt-5.4-mini`, xhigh reasoning. Owned quest data migration because the slice was data-heavy and validation-oriented.

Integrated files:

- `src/defines.h`: bumped the working version to `0.9.7.2`, added new monster/special ability constants, Valar IDs, quest IDs 7-16, generic quest states/flags, challenge IDs, and option aliases.
- `src/types.h`: extended `quest_type` for `Z/J/F/H/L` metadata and added foundation player fields for later quest runtime phases while preserving `varda_reserved` for current build compatibility.
- `src/init1.c`: added parser support for `Z:`, `J:`, `F:`, `H:`, and `L:` quest fields.
- `src/externs.h`: exported only Phase 1 quest helper APIs; branch-only Phase 2 externs were intentionally not kept.
- `src/metarun.h`, `src/metarun.c`, `src/metarun_legacy.h`, `src/metarun_legacy.c`: expanded quest accounting to 24 slots, added challenge/quest reserved helpers, and added a compact 8-slot metarun reader for existing `develop` files.
- `src/quest.c`: added generic quest state/title/flag/completion-cap helpers and extended metarun check/restore paths for quests 1-16.
- `lib/edit/quest.txt`: added metadata to quests 1-6, appended quests 7-16, and preserved newer `develop` text/probability fixes where they did not conflict with the new structure.
- `lib/edit/vault.txt`: appended four quest vaults as active IDs 464-467 without renumbering existing active vaults.
- `lib/edit/ability.txt`: added special reward ability records for IDs 10-13.
- `lib/edit/limits.txt`: raised quest max from `M:Q:8` to `M:Q:18`.

Top-agent corrections after worker integration:

- Removed premature extern declarations for Phase 2 runtime functions that are not implemented in `develop` yet.
- Fixed `quest_display_title()` fallback to use `quest_name_text` for quest names rather than the quest body text pool.
- Added missing special ability records for `A:8:10`, `A:8:11`, `A:8:12`, and the reserved Varda light boon.
- Removed a misleading Varda `A:8:9` line; current Varda code unlocks the Oath of Light through its custom reward path, matching the source branch structure.

Validation completed:

- `git diff --check` passed; only normal LF/CRLF warnings were reported.
- Data serial checks passed for `quest.txt`, `vault.txt`, and `ability.txt`.
- `lib/edit/limits.txt` has `M:Q:18`, exceeding highest quest ID 16.
- All quest `A:8` reward IDs now resolve to `lib/edit/ability.txt` records.
- New source monster index constants resolve to current `lib/edit/monster.txt` entries.
- No old source-branch `0.9.1.x` version gates or constants remain in Phase 1 files.
- `.\build-incremental.ps1` passed.
- `.\build-cmake.bat` passed for both standard and portable builds.
- Portable runtime initialization reached the interactive/menu state after deleting and regenerating `limits.raw`, `quest.raw`, `ability.raw`, and `vault.raw`; no parse/init errors were found in the portable log.

## Phase 2 and Phase 3 Implementation Record

Status: completed on branch `port-new-quest-structure`.

Commits:

- `457cfb26` `phase 2`
- `cd8d6117` `phase3`

Integrated runtime files:

- `src/generate.c`: quest lottery/runtime generation, follow-up quest availability checks, forced quest vault placement, Tulkas stronghold scheduling, Varda shadow bastion placement, Nienna Morgoth-hall giver handling, quest-vault retry cleanup, and special vault monster reservation checks.
- `src/xtra2.c`: quest interactions, reward application, status display, Varda gift selection, follow-up completion handlers, global quest completion, Nienna mercy/Morgoth/pacifist hooks, Orome hunt tracking, Tulkas orc/Morgoth hooks, and Varda shadow/Ungoliant hooks.
- `src/save.c`, `src/load.c`, `src/birth.c`, `src/types.h`: persistent quest fields, defaults, and `0.9.7.2` save gates.
- `src/cmd1.c`, `src/cmd2.c`, `src/dungeon.c`, `src/monster2.c`, `src/object2.c`, `src/spells1.c`, `src/spells2.c`, `src/tables.c`, `src/drop_system.c`, `src/cmd4.c`: gameplay hooks for quest failure/completion, monster restrictions, challenge behavior, rewards, and status/menu integration.

Final review corrections:

- `src/generate.c`: replaced fixed 8-slot roulette scratch arrays with `ROULETTE_QUEST_MAX` based on `QUEST_SLOT_MAX`, and added an overflow guard. Current data has seven roulette quests, but this prevents future `Y:1` additions from writing past the registry.

Comparison notes against `origin/New-quest-structure`:

- Ported the quest-specific implementation from source commits through `8ff00b01`.
- Preserved current `develop` as authoritative for newer generation, scoring, UI, save/load, drop-system, SDL, and data behavior.
- Intentionally did not port out-of-scope old-branch files: `.claude/*`, `.gitignore`, source `session_notes.md`, `src/main-sdl.c`, `src/wizard2.c`, and `src/z-term.c`.
- Did not port the source branch `src/cave.c` Oath of Light light-intensity tweak because current `develop` already applies Oath of Light as a light-radius reward in `src/xtra1.c`, matching current oath and quest reward text.
- Did not copy old source-branch save gates such as `0.9.1.10`; new quest fields use `savefile_version_at_least(0, 9, 7, 2)`.

Final validation completed:

- `git diff --check` passed, with only the repo's normal LF/CRLF warning for `src/generate.c`.
- Structured data check passed: 16 contiguous quest IDs, `M:Q:18`, seven roulette quests, all quest `A:8` reward IDs resolve to `lib/edit/ability.txt`, and vaults 464-467 exist with the `QUEST` flag.
- `powershell -ExecutionPolicy Bypass -File .\build-incremental.ps1` passed.
- `.\build-cmake.bat` passed for both standard and portable builds after the final review fix.
- Portable startup smoke test regenerated `limits.raw`, `quest.raw`, `ability.raw`, `vault.raw`, and `oath.raw`; no quest/ability/vault parser errors were found in `sil-more-windows-sdl3-portable/log.txt`.

## Scope To Port

Port these quest-structure concepts:

- Multi-stage Valar quest chains using quest metadata.
- New quest template fields:
  - `Z:` Vala owner.
  - `J:` sequence within that Vala chain.
  - `F:` flags such as `GLOBAL`.
  - `H:` challenge unlock.
  - `L:` per-metarun completion cap.
- Quest IDs 7-16:
  - Mandos stage 2: Easterling traitors.
  - Mandos stage 3: Maeglin.
  - Orome stage 2: dragon hunt.
  - Orome stage 3: great hunt.
  - Nienna stage 2: Morgoth-hall mercy theft.
  - Nienna stage 3: pacifist escape.
  - Tulkas stage 2: orc stronghold.
  - Tulkas stage 3: wound Morgoth.
  - Varda stage 2: Shadow Bastion.
  - Varda stage 3: Ungoliant.
- New quest vaults:
  - Den of Maeglin the Traitor.
  - Easterling Fortress.
  - Orc Stronghold.
  - Shadow Bastion.
- Required reward abilities, quest-state fields, metarun quest counters, challenge unlock state, and save/load compatibility.

## Explicitly Out Of Scope

Do not port these unless a later compile or runtime check proves a direct quest dependency:

- `.claude/*`
- `.gitignore`
- `session_notes.md`
- `src/main-sdl.c` logging noise
- unrelated `src/z-term.c` rendering/input changes
- debug-only `src/wizard2.c` convenience unlocks
- combat, object, spell, monster, or table balance changes not required by the new quest rewards or quest restrictions
- old branch version numbers such as `0.9.1.10`; adapt save gates to current `develop` versioning instead

## Top-Agent Control Model

Use one top agent as coordinator and integrator. The top agent owns branch hygiene, dependency ordering, final conflict resolution, validation, and the decision of what not to port.

Recommended model choices:

- Top agent: `gpt-5.5`, xhigh reasoning. The work crosses save formats, generation, metaruns, and gameplay flow, so final integration should use the strongest reasoning budget.
- Implementation worker agents: `gpt-5.5`, high to xhigh reasoning depending on slice risk. Use xhigh for generation, save/load, metarun, and quest interaction work; high is acceptable only for smaller bounded hooks.
- Data and validation agents: `gpt-5.4-mini`, high to xhigh reasoning depending on task size. Use xhigh for data migration review and end-to-end validation planning.

When spawning workers, tell each one they are not alone in the codebase, must not revert others' edits, and must keep to its assigned files.

## Parallel Implementation Plan

### Phase 0: Top-Agent Setup

Top agent actions:

1. Create a working branch from current `develop`.
2. Confirm the worktree is clean or record unrelated user changes.
3. Capture reference diffs:
   - `git diff origin/develop...origin/New-quest-structure -- <files>`
   - `git log --right-only --cherry-pick --oneline origin/develop...origin/New-quest-structure`
4. Establish the current `develop` save version and choose the new compatibility gate before any persistent-field edits.

Deliverable: branch ready, source diffs available, no files edited yet.

### Phase 1: Parallel Foundation

Run these workers in parallel because their write sets are mostly disjoint, with top-agent review before integration.

Worker A, Quest Schema and Parser:

- Owns `src/defines.h`, `src/types.h`, `src/init1.c`, `src/externs.h`.
- Add Valar IDs, generic quest states, new quest IDs, challenge IDs, quest flags, and parser support for `Z/J/F/H/L`.
- Extend `quest_type` while preserving current `develop` fields and parser behavior.
- Do not change save/load yet beyond declaring fields.

Worker B, Metarun Quest Accounting:

- Owns `src/metarun.h`, `src/metarun.c`, `src/metarun_legacy.h`, `src/metarun_legacy.c`, `src/quest.c`.
- Expand metarun quest slots and known quest flags.
- Add generic quest state helpers in `quest.c`: `quest_get_state`, `quest_set_state`, `quest_metarun_flag`, `quest_display_title`, `quest_completion_cap`.
- Add challenge unlock/completion storage only where required by the new quests.
- Preserve current `develop` scoring/metarun fixes, especially load-time restoration and quest completion de-duplication.

Worker C, Quest Data:

- Owns `lib/edit/quest.txt`, `lib/edit/vault.txt`, `lib/edit/ability.txt`, `lib/edit/oath.txt`, `lib/edit/limits.txt`.
- Port new metadata and quests 7-16.
- Preserve newer `develop` text fixes to existing quests unless the new structure requires a targeted rewrite.
- Raise quest limit consistently.
- Add only quest-required vaults and reward ability records.

Top-agent checkpoint after Phase 1:

- Resolve schema constant mismatches.
- Confirm `lib/edit/quest.txt` records remain strictly increasing.
- Confirm `M:Q` limit exceeds highest quest index.
- Confirm no source branch version constants were copied blindly.

### Phase 2: Parallel Runtime Port

Start these workers after Phase 1 compiles far enough for constants and structs to exist.

Worker D, Generation and Quest Vaults:

- Owns `src/generate.c`.
- Port only quest-generation behavior:
  - registry updates for multi-stage quests;
  - per-quest metarun cap checks;
  - Mandos follow-up vault selection;
  - Tulkas Orc Stronghold scheduling and forced placement;
  - Varda Shadow Bastion forced placement;
  - Nienna Morgoth-hall giver placement;
  - quest-vault regeneration handling.
- Preserve current `develop` generation fixes, skeleton notes, drop-system hooks, minimap/debug notes, and existing quest-vault bugfixes.

Worker E, Quest Interactions and Rewards:

- Owns `src/xtra2.c`.
- Port quest interaction handlers, reward application, status summaries, and global quest checks for Nienna, Orome, Tulkas, Mandos, and Varda follow-up quests.
- Keep current `develop` UI/menu behavior and typewriter/menu improvements.
- Avoid moving unrelated UI code.

Worker F, Save/Load and Birth Defaults:

- Owns `src/save.c`, `src/load.c`, `src/birth.c`.
- Add new player fields to wipe, write, read, and default paths.
- Gate new reads using the current `develop` version scheme, not the old branch's `0.9.1.10`.
- Preserve compatibility for older `0.9.x` develop saves.

Top-agent checkpoint after Phase 2:

- Do a compile pass.
- Fix missing exports, duplicate helpers, and stale branch-only assumptions.
- Review all persistent field ordering before continuing.

### Phase 3: Parallel Hooks

Run smaller hook workers after the core runtime compiles.

Worker G, Gameplay Event Hooks:

- Owns `src/cmd1.c`, `src/cmd2.c`, `src/dungeon.c`.
- Port only hooks needed for:
  - Morgoth attack detection for Nienna's quest;
  - Orome bow/spear rhythm;
  - Tulkas Wrath;
  - Mandos resurrection;
  - leave-level quest reset/abandonment logic;
  - per-turn global quest activation.
- Preserve newer `develop` movement, touch/mouse, prompt, and UI fixes.

Worker H, Monster/Object/Spell Restrictions:

- Owns `src/monster2.c`, `src/object2.c`, `src/spells1.c`, `src/spells2.c`, `src/tables.c`.
- Port only required hooks:
  - block quest-reserved uniques outside quest contexts;
  - count Orome dragon/great-hunt kills;
  - mark player-sourced Morgoth damage for Nienna;
  - object filters for quest-unlocked challenges if those challenges are kept.
- Reject unrelated balance changes.

Worker I, Validation Harness:

- Owns no production code by default.
- Builds after each integration point.
- Maintains a short checklist of commands, failures, and fixes needed.
- May add temporary debug notes for the top agent, but must not commit generated artifacts.

Top-agent checkpoint after Phase 3:

- Re-run full build.
- Search for old hardcoded six-quest assumptions.
- Search for unhandled quest IDs, missing exports, stale comments, and version gates.

## Integration Rules

- Prefer manual porting over cherry-picking whole commits.
- Do not apply a file wholesale from `New-quest-structure`; `develop` has newer code in almost every touched file.
- Keep existing `develop` behavior when a conflict is not quest-specific.
- Keep the first six quests compatible with current `develop` bugfixes.
- Add comments only around non-obvious save/layout and quest-regeneration logic.
- Any persistent state change must update:
  - `src/types.h`
  - `src/birth.c`
  - `src/save.c`
  - `src/load.c`
  - version compatibility notes/gates
- Any new `.c` file would need `CMakeLists.txt`, but this plan should avoid new C files.

## Conflict Hot Spots

- `src/generate.c`: largest conflict surface. Must preserve current generation fixes while adding follow-up quest selection and forced quest vault placement.
- `src/xtra2.c`: quest interactions, reward text, status menu, and helper function ownership overlap.
- `src/types.h`: player layout changes affect save compatibility.
- `src/load.c` and `src/save.c`: field order and version gates are high risk.
- `src/metarun.c` and `src/metarun.h`: current `develop` scoring and metarun fixes must stay intact.
- `lib/edit/quest.txt`: existing quest text may have newer fixes; port metadata and new quests carefully.

## Validation Plan

Minimum build validation:

1. Run `.\build-cmake.bat`.
2. If only C changes follow after a successful configure, run `.\build-incremental.ps1`.
3. Launch the standard build and inspect adjacent `log.txt`.

Data validation:

1. Delete active `quest.raw`, `vault.raw`, `ability.raw`, `oath.raw`, and related data caches before runtime testing.
2. Confirm `quest.txt` parses all 16 quests.
3. Confirm the four new quest vaults are loaded and marked `QUEST`.

Save/load validation:

1. New character starts with all quest states zeroed.
2. Save and reload with each major state: `GIVER_PRESENT`, `ACTIVE`, `SUCCESS`, `REWARDED`.
3. Load an older `develop` save and verify new fields default safely.
4. Verify metarun quest counts increment once per completion and respect `L:` caps.

Gameplay smoke tests:

1. Base six quests still spawn, complete, reward, and record metarun completion.
2. One full Vala chain completes through stage 3.
3. Mandos stage 2 places Easterling Fortress and tracks Ulfang/Uldor.
4. Mandos stage 3 places Maeglin and grants resurrection without corrupting death flow.
5. Tulkas stage 2 schedules Orc Stronghold and blocks target captains elsewhere.
6. Tulkas stage 3 tracks Morgoth damage by the player.
7. Orome stage 2 counts non-hatchling dragons.
8. Orome stage 3 tracks named great-hunt uniques across the metarun.
9. Nienna stage 2 detects player-sourced Morgoth damage from melee, spells, and quakes.
10. Nienna stage 3 fails on any kill and rewards only on clean escape.
11. Varda stage 2 places Shadow Bastion and reserves Belegwath.
12. Varda stage 3 completes on Ungoliant.

Regression checks:

1. One-quest-per-run rule still works.
2. Quest-vault regeneration does not leave stale quest state.
3. Abandoning a quest by changing level resets only the appropriate in-run state.
4. Existing inventory/equipment overlays, unified look, and combat roll overlay still smoke-test cleanly if touched.

## Final Acceptance Criteria

- The project builds on current `develop`.
- No unrelated old-branch files or metadata are ported.
- Existing quests retain `develop` bugfixes.
- New quest metadata parses from `lib/edit/quest.txt`.
- Quests 1-16 have stable IDs and usable metarun accounting.
- Save/load remains backward compatible with current `develop` saves.
- All new quest chains can be spawned, completed, rewarded, and restored after reload.

## Planning Notes

Two read-only explorer subagents were used to prepare this plan:

- `James`: `gpt-5.4-mini`, high reasoning, assigned to source-branch quest archaeology. Chosen because the task was bounded diff analysis but needed careful separation of quest and non-quest changes.
- `Cicero`: `gpt-5.4-mini`, high reasoning, assigned to current-`develop` quest surface and conflict analysis. Chosen for the same bounded but detail-heavy archaeology profile.

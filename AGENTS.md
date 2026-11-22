# Sil-More agent guide

Use these instructions throughout the repository. When working in a subtree with its own `AGENTS.md`, apply its more specific guidance there. Follow the user's task and preserve unrelated working-tree changes.

## Doing the work

- Trace the requested behavior from its normal entry point. Reuse existing engine and SDL conventions, and preserve the behavior callers and older saves rely on.
- Carry the change through implementation and verification. Make reversible implementation decisions within scope. Ask when the user's decision is needed to resolve a material ambiguity or authorize a destructive action.
- Choose checks that exercise the changed behavior. In the final response, identify the outcome, the checks run, and any material assumption or gap in verification.

## Subagents

- Work directly on routine or tightly coupled changes. Use a subagent when an independent investigation, review, or validation can improve the result or save enough time or context to justify coordination. Honor a user request to work without subagents.
- Give each agent a bounded question, the evidence or result to return, and, if editing, ownership of distinct files. The main agent verifies the findings against the checkout and integrates the result.
- Choose either `gpt-6.2-sol` or `gpt-6.2-astra` for every subagent, including visual and computer-use work. Use only `low`, `medium`, `high`, `xhigh`, or `max` reasoning effort for either model. If neither model is available, continue locally.
- Use Sol as the economical starting point. A lookup with a clear check can use `low`. A focused code trace or review can start at `medium`. For a broad bug hunt with a cost constraint, consider Sol `max`. Choose Astra for difficult cross-subsystem diagnosis or when a missed defect carries substantial rework. Astra `high` is a reasonable starting point in those cases.
- Select effort for the task's uncertainty and the cost of a wrong result. The linked bug-fixing runs cover `low` through `max` on two codebases. Use local evidence for visual tasks. [Bug Hunt Bench results](https://bughunt.productcompass.pm/data/benchmark.json)
- When reporting delegated work, name the model and effort, the subtask, and what delegation contributed.

## Repository map

Sil-More is a C17 game with an SDL3 frontend. Start at the relevant owner.

| Area | Where to look |
| --- | --- |
| SDL frontend and UI | `src/main-sdl.c`, `src/sdl/`, `src/ui/`, `src/z-term.c` (terminal and key queue), `src/pane.c` (subwindows), `src/format.c` |
| Input and commands | `inkey()` in `src/support/input.c`, `request_command()` in `src/support/command.c`, dispatch in `src/dungeon/dungeon-command.c`, handlers in `src/cmd/` |
| Dungeon, generation, and map | `src/dungeon/`, `src/level-generation/`, `src/cave/` |
| Monsters, combat, and spells | `src/monster/`, `src/melee/`, `src/spell/` |
| Objects and drops | `src/object/` for descriptions, inventory, creation, and UI. `src/drop_system.c` builds the drop catalog and caches it in `ANGBAND_DIR_DATA/drops.raw`. |
| Persistence | `src/fs/` for save/load, `src/init/init-paths.c` for runtime paths, `src/metarun/` for metaruns and cleanup, `src/metarun_legacy.c` for compatibility, `src/score/` for scoring. See `docs/score_system_overhaul.md`. |
| Settings and sound | `src/sdl-config.c` for `sil_sdl.json`. `src/sdl-sound.c` and `src/sound-config.c` for `sound.json`. Sound event names are in `src/variable.c`. |
| Content and tools | `lib/edit/` for templates, `lib/pref/` for JSON defaults, `lib/xtra/` for graphics, fonts, music, and sound. Developer tools live in `tools/`. Python checks and simulations live in `scripts/`. |

## Build and validate on Windows

- Use `.\build-cmake.bat` for a fresh SDL3 build and deployment. It sets up MSYS2 MinGW64 at `C:\msys64` and uses pinned SDL sources in `external/`. The default builds standard, portable, and Steam targets. Pass `standard`, `portable`, or `steam` to build and stage one.
- For an already configured build, run `.\build-incremental.ps1 -Target standard` in PowerShell (also accepts `portable` and `steam`). It rebuilds in `build-<target>/`. Use the batch script to refresh the deployed executable and assets. From an initialized MSYS2 MinGW64 shell, `cmake --build build-standard --parallel` also works. A direct CMake invocation needs both MSYS2 binary directories on PATH so GCC can load its helpers.
- Launch the staged Windows build with `.\sil-more-windows-sdl3\sil-more.exe` or `.\sil-more-windows-sdl3-portable\sil-more.exe`. For runtime changes, exercise the affected path and inspect `%APPDATA%\Sil-QH\sil-more\log.txt`. Portable builds first try `log.txt` beside the executable. For SDL UI changes, also smoke-test inventory/equipment, unified look, and the combat roll overlay.
- When changing assets, check the staging commands in `build-cmake.bat` and verify the changed file in the deployment. Add a targeted copy if needed. Preserve user data in the portable deployment's `lib/` directory.
- Keep generated build directories, deployed binaries, object files, raw caches, logs, release ZIPs, and analysis output out of commits. Use `create-release-build.ps1` and `create-distribution-archive.ps1` for distribution work.

## Runtime data

`init_file_paths()` in `src/init/init-paths.c` resolves the active directories. Check the startup log when diagnosing path issues.

| Data | Standard Windows build | Portable Windows build |
| --- | --- | --- |
| Root | Normally `Saved Games\sil-more` in the Windows user profile. On a fresh installation with an unusable save location, check the alternate path reported at startup. | Deployed `lib/` |
| Saves and generated templates | `save/`, `data/` under the user root | `lib/save/`, `lib/data/` |
| User settings | `sil_sdl.json` and `sound.json` under the user root. Missing sound settings are seeded from `lib/pref/sound.json`. | User directory `lib/user/`, sound configuration `lib/pref/sound.json` |
| Scores and metaruns | `meta/` for score files and databases, `meta/metaruns/` for individual runs | `lib/apex/` for scores, `lib/apex/metaruns/` for individual runs |

## Templates and save compatibility

- Edit the source templates in `lib/edit/`, including `monster.txt`, `vault.txt`, `object.txt`, `artefact.txt`, and `terrain.txt`. The game compiles them into `ANGBAND_DIR_DATA`. To force regeneration, remove only the affected generated `*.raw` cache from the active data directory and relaunch.
- Preserve shipped serials and GUIDs. In templates keyed by `N:<serial>:`, append entries with strictly increasing serials. For templates with `Q:<hex-guid>`, preview missing GUIDs with `python tools/make_guid.py --dry-run` before generating them.
- For a major, minor, or patch bump, update the numeric macros and `VERSION_STRING` in `src/defines.h` and the `V:` header of every active game-versioned template. An extra-only bump retains the three-component headers. Run `python scripts/check_template_versions.py --self-test --runtime-check --cmake-check`. For an Android release, add `--aab <finished-bundle>`. The checker handles independently versioned schemas such as `dungeon-themes.txt`.
- When changing a save, scoring, or metarun format, bump the relevant compatibility version. For new save fields, update the matching writer and reader under `src/fs/`, gate reads with `savefile_version_at_least()`, and provide defaults for older saves. Check the version macros in `src/defines.h`, including `MIN_VERSION_EXTRA`. Log and recover from load failures where possible.

## Engine conventions and gameplay contracts

- Begin new C translation units with `#include "angband.h"` and add them to `SIL_MORE_SOURCES` in `CMakeLists.txt`. Keep file-local helpers `static`. When a new cross-file global is necessary, define it in `src/variable.c` and declare it in `src/externs.h`.
- Use bounded string helpers such as `my_strcpy`, `strnfmt`, and `SDL_strlcpy`. Use the project's `log_trace`, `log_debug`, `log_info`, `log_warn`, and `log_error` functions from `src/log/` for diagnostics.
- Keep the smithing difficulty calculation used by the drop catalog (`object_difficulty_base()` in `src/object/object-difficulty.c`) in sync with `scripts/calc_artefact_difficulty.py`. Match penalty flags, bonuses, and multipliers. Run `python scripts/check_calc_artefact_difficulty.py` after changing the calculation.
- For monster perception vs. stealth, use `monster_has_visual_perception_bonus()` for the light-dependent recognition bonus and its stealth indicators. Unwary monsters must pass detection before acquiring the player's current position. Alert monsters, including archers, can fight and shoot in darkness. Use `monster_ai_can_see_player()` for alerted targeting and `monster_has_sight()` / `monster_sight_range()` for physical sight and range. Keep combat, pursuit, retreat, spells, and squad posts independent of recognition and player light. See `src/monster/monster-senses.h` and `docs/monster-ai-plan.md`.

## SDL and command UI contracts

- Reuse `show_inven()` and `show_equip()` layout calculations. Keep the `-)` floor-item prefix and the `-` shortcut. Reset `item_tester_*` and related selection state immediately after use.
- Save and restore overlays with `screen_save()` and `screen_load()`. Set the required `p_ptr->redraw` and `p_ptr->window` bits so `handle_stuff()` repaints after restoration.
- Anchor the combat roll overlay at `COL_MAP`. Preserve its default range of 0–3 lines and its 65-character clear width when retaining the current layout. `SCREEN_HGT` depends on `op_ptr->main_combat_rolls`. The combat history viewer is in `src/melee/melee-combat-display.c`.
- Preserve `OPT_auto_display_lists` and the `u` / `x` menu cycle. With `STEAMDECK_SUPPORT`, Space confirms stairs and shafts. Other builds use explicit `y/n` prompts.
- For unified look, start at `do_cmd_unified_look()` in `src/cmd/ui/cmd-ui-look.c` and `show_unified_sidebar()` in `src/cmd/ui/cmd-ui-nearby.c`.

## Working notes

If a task needs working notes, use only the repository-root `session_notes.md` for that chat. Record what changed, where, why, and how to validate it. Treat `.github/instructions/` as historical context and verify its claims against the current source.

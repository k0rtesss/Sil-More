# Meta States Implementation Plan

Status: `Finished` on 2026-04-27.

Target version: `0.9.7.1`

Completion notes:

- Agents A-E were integrated into the main checkout.
- The shared meta-state layer, forged artefacts, revenge monsters, legendary song areas, save/load support, and QA checklist are present.
- Final validation passed with `git diff --check`, `.\build-incremental.ps1`, and `.\build-cmake.bat` for standard and portable builds after integration.
- Manual gameplay smoke coverage is tracked in `docs/meta_states_validation_checklist.md`.

This plan covers metarun-local persistence for player-forged artefacts, revenge monsters, and legendary song dungeon areas. All new player-facing prose should use original First Age styled language: grave, compact, named for Beleriand, Angband, Doriath, Gondolin, Nargothrond, Hithlum, Himring, Sirion, Valinor, Mandos, Aule, Varda, and the Eldar/Edain/Naugrim. Do not quote Tolkien text.

## Agent Plan

Use these implementation agents only:

| Agent | Model | Reasoning | Ownership |
| --- | --- | --- | --- |
| A: Meta DB and lifecycle | GPT-5.5 | xhigh | Shared meta-state DB helpers, metarun cleanup, version gates, file paths |
| B: Forged artefacts | GPT-5.4 | high | Smithing hook, `artefact.db`, runtime artefact injection, drop-system integration, description generation |
| C: Revenge monsters | GPT-5.4 | high | Killer capture, `monster.db`, unique/rank upgrades, revenge bonus, character template `R:` lines |
| D: Legendary songs and dungeon areas | GPT-5.5 | xhigh | Song effect observer, dungeon capture/spawn, active-level area map, entry ability aura |
| E: QA and save compatibility | GPT-5.4 | high | Save/load tests, DB migration tests, run-to-run smoke tests, text review |

Agents are not alone in the codebase. Each must avoid reverting unrelated edits and must keep write ownership disjoint. Agent A defines shared APIs first; B/C/D build against them; E verifies after integration.

## Parallel Execution Order

Use one branch/worktree per agent, or use an agent runner that gives every worker an isolated workspace and then uploads patches for review. Do not run B, C, or D in the same mutable checkout at the same time unless the runner guarantees isolated patches.

### Round 0: Orchestrator Prep

Run from the main checkout:

```powershell
git status --short
rg -n "VERSION_STRING|VERSION_EXTRA|MIN_VERSION_EXTRA" src/defines.h
rg -n "wr_extra|rd_extra|wr_dungeon|rd_dungeon|level_partition_meta_get|level_partition_meta_set" src/save.c src/load.c src/generate.c
rg -n "score_artefact_register|create_smithing_item|add_artefact_details|object_difficulty|drop_system_init|build_artifact_variants" src/cmd4.c src/drop_system.c src/score
rg -n "take_hit|killer_mark_monster|mon_take_hit|monster_desc|RF1_UNIQUE" src/player src/spells1.c src/melee1.c src/xtra2.c src/monster1.c src/monster2.c
rg -n "change_song|singing\\(|void sing\\(|song_effective_skill|SNG_MASTERY|skip_this_turn" src/spells1.c src/xtra1.c src/monster2.c
```

If using git worktrees:

```powershell
git worktree add ..\Sil-More-meta-A -b meta-state-core
git worktree add ..\Sil-More-meta-B -b meta-state-artefacts
git worktree add ..\Sil-More-meta-C -b meta-state-monsters
git worktree add ..\Sil-More-meta-D -b meta-state-dungeon
git worktree add ..\Sil-More-meta-E -b meta-state-qa
```

If a branch already exists, replace `-b <branch>` with the existing branch name or create a fresh unique branch. Keep `AGENTS.md` and this plan file out of worker ownership unless a worker is explicitly assigned documentation changes.

### Round 1: Core API Barrier

Run Agent A first. B, C, and D can inspect in parallel, but they must not edit until Agent A publishes the shared API contract.

Agent A command:

```text
Model: GPT-5.5
Reasoning: xhigh
Worktree: ..\Sil-More-meta-A
Role: worker

Implement only the shared Meta States core.

Read docs/meta_states_implementation_plan.md, AGENTS.md, src/metarun.c, src/init2.c, src/files.c, src/save.c, src/load.c, src/score/score_artefact.c, src/score/score_format.h, src/score/score_guid.h, and CMakeLists.txt.

Own these files:
- src/meta_state.h
- src/meta_state.c
- CMakeLists.txt
- src/metarun.c only for metarun DB cleanup hook declarations/calls
- src/externs.h only if absolutely necessary for exported functions

Tasks:
1. Add src/meta_state.h/.c with DB path helpers for artefact.db, monster.db, and dungeon.db under ANGBAND_DIR_APEX.
2. Add common magic/version/header structs and safe read/write helpers using SDL_IOStream patterns already used by src/score.
3. Add metarun-id filtering helpers based on metar.id.
4. Add meta_state_clear_current_metarun_files() and call it from start_new_metarun() after clear_scorefile() and before save_metaruns().
5. Add public API stubs that B/C/D can link against:
   - meta_state_init()
   - meta_state_shutdown()
   - meta_state_clear_current_metarun_files()
   - meta_artifact_load_for_current_metarun(...)
   - meta_artifact_register_created(...)
   - meta_monster_load_for_current_metarun(...)
   - meta_monster_record_player_death(...)
   - meta_dungeon_load_for_current_metarun(...)
   - meta_dungeon_register_legendary_area(...)
6. Keep subsystem functions stubbed if needed, but define stable structs/enums and exact ownership comments in the header.
7. Bump CMakeLists.txt for the new file. Do not bump VERSION_EXTRA yet unless you also add savefile fields.

Validation:
- Build or at least run: .\build-incremental.ps1
- Report changed files, public API, and any functions left as stubs.
```

Barrier check after Agent A:

```powershell
git -C ..\Sil-More-meta-A diff -- src/meta_state.h src/meta_state.c CMakeLists.txt src/metarun.c
git -C ..\Sil-More-meta-A diff --check
```

Merge or apply Agent A first. Then rebase/refresh B, C, D, and E worktrees onto the core branch.

### Round 2: Parallel Feature Work

After Agent A is integrated, launch B, C, and D together. Launch E in read-only QA planning mode at the same time.

Agent B command:

```text
Model: GPT-5.4
Reasoning: high
Worktree: ..\Sil-More-meta-B
Role: worker

Implement forged artefact meta-state only. You are not alone in the codebase; do not edit monster, song, dungeon capture, or QA files except for compile fixes directly caused by your API use.

Read docs/meta_states_implementation_plan.md, src/meta_state.h, src/cmd4.c, src/drop_system.c, src/obj-info.c, src/score/score_artefact.c, src/types.h, lib/edit/artefact.txt, and scripts/calc_artefact_difficulty.py.

Own these files:
- src/meta_state.c only for artefact DB implementation behind Agent A's API
- src/meta_state.h only for artefact-specific struct/API refinements coordinated with Agent A's names
- src/cmd4.c
- src/drop_system.c
- src/obj-info.c
- scripts/calc_artefact_difficulty.py only if object_difficulty logic is changed; otherwise leave untouched

Tasks:
1. At artefact creation in create_smithing_item()/add_artefact_details(), if saved object_difficulty(smith_o_ptr) >= 15, show the First Age naming warning and persist the artefact to artefact.db through meta_artifact_register_created().
2. Store creation depth, saved difficulty, creator character GUID/name, Masterpiece/Aule usage, stats, abilities, spawn_num, rarity weight, and generated description.
3. Implement meta artefact rarity exactly as in the plan.
4. Generate deterministic First Age style descriptions from artefact flags/stats/materials and store them in DB.
5. Load current-metarun artefacts into runtime a_info slots before drop catalogue construction or append them after drops.raw load.
6. Ensure drop_system uses a_ptr->rarity as weight and creation depth as minimum drop depth.
7. Show DB description in obj-info.c for meta artefacts.

Validation:
- .\build-incremental.ps1
- Manual/log smoke path or a small debug-only helper proving difficulty 14 is not saved and difficulty 15 is saved.
- Report changed files and exact hook points.
```

Agent C command:

```text
Model: GPT-5.4
Reasoning: high
Worktree: ..\Sil-More-meta-C
Role: worker

Implement revenge monsters only. You are not alone in the codebase; do not edit artefact, song, dungeon capture, or QA files except for compile fixes directly caused by your API use.

Read docs/meta_states_implementation_plan.md, src/meta_state.h, src/player/killer.c, src/player/killer.h, src/spells1.c, src/melee1.c, src/xtra2.c, src/monster1.c, src/monster2.c, src/init1.c, src/types.h, src/save.c, src/load.c, lib/edit/character.txt, and lib/edit/monster.txt.

Own these files:
- src/meta_state.c only for monster DB implementation behind Agent A's API
- src/meta_state.h only for monster-specific struct/API refinements coordinated with Agent A's names
- src/player/killer.c
- src/player/killer.h
- src/spells1.c
- src/xtra2.c
- src/monster1.c
- src/monster2.c
- src/init1.c
- src/types.h
- src/save.c and src/load.c only for new persistent player/template fields
- lib/edit/character.txt only for R: lines after verifying monster GUIDs

Tasks:
1. On player death in take_hit(), resolve the killing monster through the existing killer tracker and write/update monster.db through meta_monster_record_player_death().
2. Add ranked monster records with metarun id, monster race GUID, rank 1..3, killed character names/GUIDs, depth, and cause.
3. Inject ranked monsters in later runs as runtime unique overrides: name has 1-3 stars, RF1_UNIQUE behavior, max_num 1, level original + rank, combat stats scaled from base by 100 + 30 * rank percent.
4. Add First Age kill-memory text to monster recall/description.
5. Implement global revenge marks and character-specific R: parser lines in character.txt.
6. Implement revenge bonus milestones: 1 kill -> 1, 3 kills -> 3, 6 kills -> 6, 10 kills -> 10.
7. Apply the current revenge bonus additively in the same combat layer as banes/unique bane bonuses.

Validation:
- .\build-incremental.ps1
- Unit-style helper or debug log proving rank caps at *** and bonus milestones match the plan.
- Report changed files, save/load fields, and any character R: additions.
```

Agent D command:

```text
Model: GPT-5.5
Reasoning: xhigh
Worktree: ..\Sil-More-meta-D
Role: worker

Implement legendary song dungeon meta-state only. You are not alone in the codebase; do not edit artefact, revenge monster, or QA files except for compile fixes directly caused by your API use.

Read docs/meta_states_implementation_plan.md, src/meta_state.h, src/generate.c, src/dungeon.c, src/cave.c, src/spells1.c, src/xtra1.c, src/cmd4.c, src/save.c, src/load.c, src/types.h, src/variable.c, src/externs.h, src/defines.h, lib/edit/ability.txt, and lib/edit/character.txt.

Own these files:
- src/meta_state.c only for dungeon DB implementation behind Agent A's API
- src/meta_state.h only for dungeon-specific struct/API refinements coordinated with Agent A's names
- src/generate.c
- src/dungeon.c
- src/spells1.c
- src/xtra1.c
- src/cmd4.c only for song menu availability
- src/save.c
- src/load.c
- src/types.h if player state fields are unavoidable
- src/variable.c
- src/externs.h
- src/defines.h only for VERSION_EXTRA/MIN_VERSION_EXTRA and constants

Tasks:
1. Add legendary song observer begin/monster/end hooks. Only count songs that actually affect at least one monster.
2. Detect starting songs from character C: abilities where skill is S_SNG.
3. At effective song score >= 15 and qualifying monster effect, roll the planned chance and register a dungeon.db record.
4. Capture area from live cave arrays, not dun->corner/layout_anchors. Use source classification, flood fill, cap/crop, and fallback exactly as in the plan.
5. Store normalized tile mask, cave_feat, cave_color, source metadata, singer, song id, affected monster race GUIDs, and entry text.
6. Add legendary_area_id active-level map, allocate/reset it with cave arrays, save/load it with a marker and version gate.
7. Spawn at most one matching legendary area per depth in cave_gen(), exact saved depth only, with safe placement and connection.
8. On entry, show First Age message once per level and grant area aura: missing song is singable inside; known song gets +5 effective skill inside; area-only song stops on leaving.
9. Bump VERSION_EXTRA only when active-level save/load is added, and gate older saves safely.

Validation:
- .\build-incremental.ps1
- Debug path proving score 14 no capture, score 15 with no affected monster no capture, score 15 with affected monster can capture.
- Save/load smoke for spawned legendary_area_id map.
- Report changed files, save format marker, and capture/spawn limitations.
```

Agent E command:

```text
Model: GPT-5.4
Reasoning: high
Worktree: ..\Sil-More-meta-E
Role: worker

Prepare QA while B/C/D implement. Start read-only; do not edit production code until the integration round. You are not alone in the codebase.

Read docs/meta_states_implementation_plan.md, AGENTS.md, src/save.c, src/load.c, src/meta_state.h after Agent A lands, src/cmd4.c, src/drop_system.c, src/spells1.c, src/generate.c, src/dungeon.c, src/xtra2.c, and lib/edit/character.txt.

Own these files initially:
- docs/meta_states_validation_checklist.md, if useful
- no production files until Round 3

Tasks:
1. Build a precise manual validation checklist for artefacts, revenge monsters, and legendary song areas.
2. Identify save/load compatibility risks and expected markers.
3. Identify likely conflict points among B/C/D before integration.
4. After B/C/D land, switch to integration QA: run build, inspect compile failures, and propose minimal fixes.

Validation:
- No production edits in the first pass.
- Report checklist path, risk list, and integration order recommendations.
```

### Round 3: Integration Order

Integrate in this order:

1. Agent A core API.
2. Agent B artefacts.
3. Agent C monsters.
4. Agent D dungeon/song.
5. Agent E QA fixes.

After each integration step:

```powershell
git status --short
git diff --check
.\build-incremental.ps1
```

Expected conflict points:

- `src/meta_state.h` and `src/meta_state.c`: all feature agents touch these. Resolve by keeping Agent A's structure and merging B/C/D implementations under their subsystem sections.
- `src/save.c` and `src/load.c`: C and D may both add fields. Keep marker-based optional blocks and read in the same order they are written.
- `src/defines.h`: D may bump `VERSION_EXTRA`; no other agent should change version constants.
- `src/cmd4.c`: B touches smithing; D touches song menu. Keep edits separated by function.
- `CMakeLists.txt`: only A should add `src/meta_state.c`.

### Round 4: Final Verification

Run from the integrated main checkout:

```powershell
git status --short
rg -n "meta_state|artefact.db|monster.db|dungeon.db|legendary_area_id|VERSION_EXTRA" src docs
.\build-cmake.bat
```

Manual smoke tests:

```text
1. Start a character with Artifice, forge difficulty 14 artefact, confirm no artefact.db record.
2. Forge difficulty 15 artefact, confirm naming warning, artefact.db record, and First Age description.
3. Start a later character in same metarun, confirm meta artefact can appear in drop pool and is not duplicated after seen.
4. Kill player with a monster, confirm monster.db rank 1 and later unique starred runtime monster.
5. Kill the starred monster, confirm revenge bonus milestone.
6. Trigger a starting song at effective score 15+ affecting at least one monster, confirm dungeon.db record.
7. Enter the saved depth on a later character, confirm one legendary area can spawn, entry message appears once, missing song is granted inside, known song gets +5, and area-only song stops outside.
8. Save/reload on a level with a legendary area and confirm area behavior persists.
9. Start a new metarun and confirm old artefact.db, monster.db, and dungeon.db content is cleared or ignored.
```

## Shared Meta-State Layer

Add a new module pair:

- `src/meta_state.h`
- `src/meta_state.c`

Add it to `CMakeLists.txt`.

Use `ANGBAND_DIR_APEX` for DB files, matching score/metarun storage:

- `artefact.db` for metarun-forged artefacts, singular as requested. Do not overload existing score `artefacts.db`; it lacks metarun ownership, creation depth, saved difficulty, masterwork state, and descriptions.
- `monster.db` for ranked revenge monsters.
- `dungeon.db` for legendary song areas.

Every DB record must include:

- magic and format version
- owning `metar.id`
- record GUID/id
- creation run/turn/depth
- active/deleted flag

Clear these DBs when a new metarun begins in `src/metarun.c:start_new_metarun()` after `clear_scorefile()` and before `save_metaruns()`. Also filter by `metar.id` at load, so stale records cannot leak if a cleanup fails.

## Current Dungeon Data Model

The dungeon generator stores rich layout state only during generation:

- `src/generate.c:dun_data` records room `kind[]`, `is_quest[]`, `cent[]`, `corner[]`, `piece[]`, and room connectivity.
- `layout_anchor_t` records generated anchors: normal rooms, prefabs, CA blobs, BSP slices, setpieces, style, bounds, and room slot.
- Partition generation keeps static arrays:
  - `current_partition_rows`
  - `current_partition_cols`
  - `current_partition_count`
  - `current_partition_modes[25]`
  - `current_partition_densities[25]`
  - `current_partition_big_cave_types[25]`
  - `current_partition_bridge_styles[25]`
  - `current_partition_population_meta[25]`

Only part of this survives save/load:

- `wr_extra()` writes partition metadata marker `0x53`: rows, cols, count, modes, and big cave types.
- Densities, bridge styles, population recipes, `dun->corner`, room centers, and `layout_anchors` are not persisted.
- `wr_dungeon()` writes the live level: depth, player position, map size, `cave_info` low/high important bits, `cave_feat`, `cave_color`, door style choices, objects, monsters, and wandering monster state.
- `rd_dungeon()` reconstructs only those live arrays and the saved partition summary.

Therefore legendary song capture must not depend on `dun->corner`, `layout_anchors`, or room slots at song time. Those are not reliable after loading a saved game and are not exported outside generation. Capture must scan the live runtime arrays:

- `cave_feat[y][x]`
- `cave_info[y][x]`
- `cave_color[y][x]`
- `cave_m_idx[y][x]`
- `cave_o_idx[y][x]` only for filtering, not persistence
- `level_partition_index_for_point(y, x)`
- `level_partition_kind_for_point(y, x)`
- `level_partition_big_cave_type_for_point(y, x)`

There is also no safe free `CAVE_*` bit. Current important bits already occupy the low and high persisted `u16b` range, including `CAVE_CHASM_AREA` and `CAVE_MORGOTH_TUNNEL`. Legendary areas need a separate per-level area-id map, not another `cave_info` flag.

## Legendary Song Capture

### Eligibility

A song can become legendary when all conditions are true:

- The character has that song as a starting character ability in `lib/edit/character.txt` through a parsed `C:` pair where skill is `S_SNG`.
- The effective song score is at least 15, using `song_effective_skill()` and normal modifiers.
- The song affected at least one monster on that turn.
- The current depth is 1..`MORGOTH_DEPTH - 1`.
- The current run has not already created a legendary record for the same `song_id`, `depth`, and singer GUID.

Use a small per-turn chance after a qualifying effect, proposed:

```c
chance_percent = clamp(2, 20, 2 + (effective_song_score - 15) / 2);
```

This makes score 15 possible but rare and lets high Song matter without flooding `dungeon.db`.

### Song Effect Observer

Add a small observer to `spells1.c` or `meta_state.c`:

- `legendary_song_observe_begin(song_id, effective_score)`
- `legendary_song_observe_monster(m_idx, effect_kind)`
- `legendary_song_observe_end(song_id, effective_score)`

Call `legendary_song_observe_monster()` only when a song actually changes or harms a monster:

- Challenge changes alertness/morale/stance.
- Elbereth causes fear/retreat effect.
- Trees damages, stuns, or slays light-sensitive monsters.
- Lorien applies sleep/entrancement progress.
- Shattering weakens weapon/armour/stone.
- Contest/Lament applies duel penalties.
- Mastery should be instrumented where its action-prevention effect is applied. If it currently only pays cost without an effect hook, D must first locate or implement the actual Mastery skip check and then observe it.

At `legendary_song_observe_end`, require at least one still-valid observed monster. Persist up to 8 affected monster race GUIDs and their relative positions; do not require those monsters to be reproduced later.

### Capture Source Classification

At trigger time, classify the singer's location in this order:

1. `CAVE_G_VAULT`: treat as special-room terrain, but never capture the whole greater vault. Use the same cap/crop rules below.
2. `CAVE_ICKY | CAVE_ROOM`: special room, lesser vault, interesting room, or prefab.
3. `CAVE_ROOM`: normal room or generated cave room.
4. Effective partition kind from `level_partition_kind_for_point()`:
   - `LEVEL_PART_BIG_CAVE`
   - `LEVEL_PART_CHASM`
   - `LEVEL_PART_LABYRINTH`
   - `LEVEL_PART_CAVEY`
   - `LEVEL_PART_RUINED`
5. Corridor/fallback.

### Component Scan

Use a bounded flood fill rooted at the singer.

Membership predicates:

- Special room/vault: walkable cells and doors where `(cave_info & CAVE_ROOM)` and `(cave_info & CAVE_ICKY)` match the singer source. Include adjacent wall border after the flood fill.
- Normal room: walkable cells and doors with `CAVE_ROOM` and not `CAVE_ICKY`. Include adjacent wall border.
- Big cave/cavey/chasm: same partition index as the singer, native walkable cells only, and not `CAVE_G_VAULT`/`CAVE_MORGOTH_TUNNEL`.
- Labyrinth/corridor: same partition index, walkable cells and doors connected to the singer.

Use 4-neighbor flood fill for room identity. After the component is found, include one tile of bordering wall/door/forge/glyph/chasm features in the saved mask. Do not include objects, monsters, traps, stairs, quest-only features, `CAVE_MARK`, `CAVE_SEEN`, `CAVE_VIEW`, or transient light/view flags.

### Cap And Fallback

Proposed sane cap:

- hard dimensions: max `32 x 20`
- hard masked cells: max 420
- minimum useful size: `7 x 7`

Algorithm:

1. Build the source component.
2. Build an affected-monster bounding box from observed monster positions that are still in bounds.
3. If the component bbox is within `32 x 20` and masked cells <= 420, save it.
4. Otherwise, crop around the union of singer and affected-monster bboxes expanded by 5 cells.
5. If the expanded crop exceeds `32 x 20`, center on a weighted point: singer weight 3, affected monsters weight 1 each, then clamp to map bounds.
6. Rebuild the mask inside the crop. Keep only cells connected to the singer plus affected-monster cells if they are outside the component but inside the crop.
7. If no affected monster is included after cropping, fallback to a rectangle around singer plus nearest affected monster, expanded by 4, clamped to `32 x 20`.

This makes "room component around the singer" precise and avoids recording an entire partition, greater vault, or huge cavern.

### Dungeon DB Record

`dungeon.db` record fields:

- `u32b metarun_id`
- `guid64 record_guid`
- `byte song_id`
- `byte depth`
- `char singer_name[32]`
- `guid64 singer_character_guid`
- `s32b creation_turn`
- `byte source_kind`
- `byte partition_kind`
- `byte big_cave_type`
- `s16b singer_y, singer_x` relative to saved area
- `byte hgt, wid`
- `u16b mask_cell_count`
- `byte style_primary`
- affected monster count and race GUIDs
- First Age entry message text, fixed max 256 bytes
- tile blob:
  - mask bitset
  - `cave_feat` bytes for masked cells
  - `cave_color` bytes for masked cells
  - small `info_role` byte for room/icky/glow/chasm-role only

Use raw feature ids because this is metarun-local and same-version. Still normalize unsafe features before writing:

- stairs -> floor
- traps -> floor
- quest-only/boss features -> floor or wall by terrain family
- forge can remain only if ordinary forge, not quest-forced unique forge
- objects are never stored

### Runtime Area Map

Add a separate active-level map:

```c
u16b (*legendary_area_id)[MAX_DUNGEON_WID];
```

Allocate/reset with other cave arrays. Save it as an optional dungeon block gated by a 0.9.7 extra-version bump:

- marker proposal: `0x4C53` (`LS`)
- RLE encode `u16b` ids after `cave_color` and door choices, or after monsters with a strong marker.
- Older saves default to zero.

Do not use `cave_info` for this.

### Spawning Legendary Areas

Spawn from `dungeon.db` during `cave_gen()` on the exact saved depth. Current run limit: at most one legendary area per level.

Recommended placement point:

- after primary partition/room generation creates the map
- before final connectivity validation and before the late monster/object scatter

Placement rules:

- choose records where `metarun_id == metar.id`, `depth == p_ptr->depth`, not already spawned this character, and not disabled
- weighted random if multiple records match
- require target rectangle inside map bounds
- require no `CAVE_G_VAULT`, `CAVE_MORGOTH_TUNNEL`, quest vault, permanent wall overlap, or heavy `CAVE_ICKY` overlap
- prefer solid granite or low-density room/corridor boundary, using a fit test similar to `place_room_forced_internal()`
- apply saved masked cells, mark `CAVE_ROOM | CAVE_ICKY`, restore style through `cave_color`
- set `legendary_area_id[y][x] = record_local_id`
- add a room center/corner to `dun` so the normal connection pass can attach it; if inserted after connection, carve a short connector to nearest valid floor

### Entry Effect

Add handling near `dungeon.c:handle_partition_entry()` and greater-vault entry messaging:

- When player steps into a nonzero `legendary_area_id`, show the record message once per level.
- Example message: `Here once <singer> lifted the Song of <song>, and the stone remembers.`
- While inside the area:
  - the song menu treats the record song as available
  - if the character already has the song as a normal ability, `song_effective_skill(song)` gains +5
  - if the character lacks it, they may sing it at normal effective Song skill
- If the player leaves while singing a song granted only by the area, stop that song.

This is recommended as an area aura, not a permanent character grant, because the feature is tied to place memory.

## Forged Artefacts

### Current Hook Points

Player-forged artefacts are finalized in `src/cmd4.c:create_smithing_item()`:

- `add_artefact_details()` copies the current object into `smith_a_ptr`.
- `smith_a_ptr->level = object_difficulty(smith_o_ptr)`.
- `smith_a_ptr->rarity = 10`.
- Runtime artefact slot is assigned from `z_info->art_rand_max + p_ptr->self_made_arts`.
- A GUID is generated if missing.
- `score_artefact_register(created)` writes to existing score `artefacts.db`.

Rename prompt is `src/cmd4.c:rename_artefact()`.

`object_difficulty()` includes character discounts such as Feanor/Telchar, so the saved difficulty must be the computed value at creation time, not recalculated later.

### Persistence Rule

When an artefact is created and saved difficulty >= 15:

- prompt before final acceptance or rename:
  - `This work may be remembered through the long years of Beleriand. Name it with care.`
- persist it to `artefact.db`
- store exact creation depth and saved difficulty
- store whether Masterpiece or Aule's Forge was used
- store generated description text
- make it eligible only for later characters in the same metarun

Do not add the current character's artefact to the current run's drop pool.

### Artefact DB Record

Fields:

- `u32b metarun_id`
- `guid64 artefact_guid`
- `guid64 creator_character_guid`
- `char creator_name[32]`
- `char artefact_name[MAX_LEN_ART_NAME]`
- creation depth
- saved difficulty
- rarity weight
- used Masterpiece/Aule flag
- object/artefact stats copied from `artefact_type`
- abilities and bane types
- `spawn_num`
- description text blob, max 512 bytes
- seen/spawned counters for the current metarun

### Drop Pool Injection

`src/drop_system.c:build_artifact_variants()` already builds artefact entries from `a_info` and uses `a_ptr->rarity` as group weight. Higher weight means more common in the drop catalog.

Plan:

1. Load `artefact.db` after edit files/raw data initialize and after `a_info` exists.
2. Reserve runtime artefact slots for meta artefacts. If current `art_rand_max`/self-made area is too small, add a fixed cap such as `META_ARTEFACT_MAX 64` and ensure `a_info` has capacity.
3. Copy each eligible DB record into a runtime `a_info` slot.
4. Set:
   - `a_ptr->level = creation_depth`
   - `a_ptr->rarity = computed_rarity_weight`
   - `a_ptr->cur_num = 0`
   - `a_ptr->found_num = 0`
   - `a_ptr->seen = 0`
5. Extend `drop_system_init()` so the drop catalog includes injected meta artefacts. If the raw `drops.raw` cache was built before meta injection, either rebuild in memory after injection or append meta artefact entries after raw load.
6. Modify `obj-info.c` description rendering so meta artefacts use the DB description text when `a_info[o_ptr->name1].text` is not in `a_text`.

### Current Artefact Rarity Analysis

Parsed `lib/edit/artefact.txt` normal playable entries with both `W:` and `# Smithing difficulty`:

- Count: 129
- Difficulty min/median/mean/max: 12 / 39 / 39.0 / 68
- Rarity weight min/median/mean/max: 1 / 20 / 21.4 / 40
- Depth min/median/mean/max: 1 / 12 / 12.5 / 35
- Correlation between difficulty and rarity weight: about -0.24, weak but real

Difficulty buckets:

| Difficulty | Count | Avg weight | Median weight |
| --- | ---: | ---: | ---: |
| 15-19 | 8 | 23.8 | 25 |
| 20-24 | 7 | 32.1 | 35 |
| 25-29 | 19 | 22.9 | 25 |
| 30-34 | 11 | 22.4 | 20 |
| 35-39 | 23 | 20.7 | 20 |
| 40-44 | 15 | 20.5 | 20 |
| 45-49 | 16 | 20.9 | 20 |
| 50-54 | 13 | 18.8 | 20 |
| 55-59 | 14 | 20.8 | 20 |
| 60-64 | 1 | 15.0 | 15 |
| 65-69 | 1 | 1.0 | 1 |

Current canon artefacts cluster around weight 20. Low and moderate difficulty artefacts often sit at 25-40. The rarest weights 1-10 are reserved for outliers like Calris, Glend, Draugluin, Thuringwethil, Boldog, and a few high-power weapons.

### Suggested Rarity Formula

Use saved difficulty for weight. Use creation depth separately as the artefact's minimum drop depth:

```c
int meta_artifact_rarity_weight(int saved_difficulty, bool used_masterpiece)
{
    int d = saved_difficulty;
    if (d < 15) d = 15;

    /* Higher is more common in drop_system.c. */
    int weight = 32 - ((d - 15) * 2) / 5;
    if (weight > 30) weight = 30;
    if (weight < 8) weight = 8;

    if (used_masterpiece)
    {
        int boost = weight / 3;
        if (boost < 4) boost = 4;
        weight += boost;
        if (weight > 40) weight = 40;
    }

    return weight;
}
```

Reference table:

| Saved difficulty | Normal weight | Masterpiece/Aule weight |
| ---: | ---: | ---: |
| 15 | 30 | 40 |
| 20 | 30 | 40 |
| 25 | 28 | 37 |
| 30 | 26 | 34 |
| 35 | 24 | 32 |
| 40 | 22 | 29 |
| 45 | 20 | 26 |
| 50 | 18 | 24 |
| 55 | 16 | 21 |
| 60 | 14 | 18 |
| 65 | 12 | 16 |
| 70 | 10 | 14 |

This keeps most custom artefacts below the canon median flood level unless they are modest works or made with Masterpiece. It also satisfies the requirement that Masterpiece artefacts get a bigger rarity weight.

### Artefact Description Algorithm

Generate descriptions once at creation time and store them in `artefact.db`.

Inputs:

- artefact name
- base object kind name
- creator name
- character template name/GUID
- race if useful
- creation depth
- saved difficulty
- material flags: mithril, star-iron, galvorn-like, jewel, light
- alignment flags: noble, evil, traitor, haunted, cursed
- strongest powers: stats, skills, slays, brands, sharpness, light, speed, song, will, smithing, stealth, perception
- Masterpiece/Aule flag
- metarun id and artefact GUID for deterministic RNG seed

Generation structure:

1. Form/material sentence.
2. Maker sentence.
3. Power sentence based on the top one to three traits.
4. Memory/fate sentence.

Example templates:

- `A <kind> of <material>, wrought in the deep places where <depth_phrase>, it bears a cold brightness under the hand.`
- `It was shaped by <creator>, and the labour of that hour has not wholly passed from it.`
- `Its virtue is bent toward <power_phrase>, and foes of <bane_phrase> feel its answer.`
- `So long as it endures, it keeps the memory of <place_phrase> and of the will that made it.`

Tone rules:

- Noble items: use light, oath, memory, endurance, star, hidden city, western fire.
- Evil/cursed items: use shadow, oath-burden, iron, hunger, treachery, Angband, Nan Elmoth, Taur-nu-Fuin.
- Dwarven/smithing items: use stone, deep halls, hammer, rune, Nogrod, Belegost.
- Song items: use voice, lament, remembered music, halls answering.
- Avoid later-age references and avoid direct Tolkien quotations.

Wrap to the same style as `D:` text, ideally 2-4 short lines and max 512 bytes.

## Revenge Monsters

### Capture

The death path is `src/spells1.c:take_hit()`. The last physical monster attacker is already tracked through `player/killer.c:killer_mark_monster(m_ptr)`.

On player death by a monster:

1. Resolve the killer monster instance and race GUID.
2. If valid and not Morgoth/quest-forbidden, write or update `monster.db`.
3. Increment rank to max 3.
4. Store killed character name, character GUID, depth, turn, and cause string.
5. Record as global revenge for all future characters in this metarun.

### Runtime Monster Form

When the ranked monster is generated in a later run:

- Treat it as unique in all respects.
- Display one `*` per rank after or before the name, max `***`.
- Increase monster level by rank, so each promotion adds 1 level and rank 3 is original level + 3.
- Increase combat stats from the base race by `100 + 30 * rank` percent. Do not compound repeatedly from an already-upgraded runtime copy.
- Mark `RF1_UNIQUE` in the runtime copied race only, not in base `r_info`.
- Set `max_num = 1`.
- Description includes who it killed:
  - `<Name>* slew <character> in the deeps of <depth phrase>, and the memory of that death clings to it.`

Do not mutate base `r_info` permanently. Use a runtime override/copy mechanism similar to the existing `r_base` snapshot approach.

### Revenge Bonus

Maintain a global metarun revenge bonus. Interpret the requested totals literally:

- after 1 revenged kill, total bonus is 1
- after the next 2 revenged kills, total bonus is 3
- after the next 3 revenged kills, total bonus is 6
- after the next 4 revenged kills, total bonus is 10

That means the bonus increases when a tier is completed. Tier 1 requires 1 kill, tier 2 requires 2 more kills, tier 3 requires 3 more kills, and so on. Store both total revenged kills and current bonus, or derive bonus from total kills with:

```c
int revenge_bonus_from_kills(int kills)
{
    int bonus = 0;
    int tier = 1;
    int remaining = kills;

    while (remaining >= tier)
    {
        bonus += tier;
        remaining -= tier;
        tier++;
    }

    return bonus;
}
```

The current total bonus is additive to banes and other combat bonuses. Apply it in the same layer as bane/artifact-bane/unique-bane bonuses so combat math stays auditable.

### Character Template Revenge Lines

Add `R:` to `lib/edit/character.txt`:

```text
R:<monster-guid-or-name>:<reason text>
```

Parser changes:

- extend `character_profile` with a bounded list, for example 8 revenge monster GUIDs
- parse `R:` after `Q:`/`C:`/`E:`
- resolve by GUID first, then by exact monster name only as a developer convenience
- at birth, copy character-specific revenge marks into runtime revenge set

Agents should research lore from existing character descriptions and monster list first. Only add `R:` lines where the relevant killer/enemy is clearly present in `lib/edit/monster.txt`.

## Text Style Requirements

All new messages should be original First Age styled prose. Keep UI text short enough for message lines.

Examples:

- Artefact naming prompt: `This work may be remembered through the long years of Beleriand. Name it with care.`
- Artefact saved message: `<name> is set among the works that may outlive its maker.`
- Revenge mark in monster recall: `A doom lies on this foe for <character>; vengeance remembers its name.`
- Ranked monster kill memory: `<monster>* slew <character>, and its shadow has grown since that hour.`
- Legendary area entry: `Here once <singer> lifted the Song of <song>, and the stone remembers.`
- Legendary area song grant: `The old music rises again in your thought.`

Avoid modern phrasing such as "meta", "database", "bonus stack", "proc", or "trigger" in player-facing strings.

## Save And Versioning

Because this adds active-level state and new persistent DBs:

- bump `VERSION_EXTRA` from `0` to `1`
- set `MIN_VERSION_EXTRA` appropriately after deciding whether 0.9.7.0 saves remain loadable
- add optional load defaults for older saves
- save/load new player fields:
  - current revenge bonus/kills if placed on `player_type`
  - current legendary area visited id/mask if needed
  - any area-granted song state if permanent; if area aura only, do not save as player state
- save/load active dungeon `legendary_area_id` map

DB files can be independent of savefile version if their header contains a format version and metarun id.

## Validation

Build:

```powershell
.\build-cmake.bat
```

Targeted tests and smoke tests:

- Forge artefact difficulty 14: not saved.
- Forge artefact difficulty 15: prompt appears, record written with saved difficulty and creation depth.
- Forge with Masterpiece/Aule: rarity weight higher than non-masterpiece.
- Start next character in same metarun: forged artefact can enter drop pool.
- Start next metarun: `artefact.db`, `monster.db`, `dungeon.db` cleared or ignored by metarun id.
- Monster kills player: record rank 1, unique name has `*`, description names killed character.
- Same ranked monster kills more characters: rank reaches max `***`.
- Revenge bonus milestones match requested totals: 1 kill -> 1, 3 kills -> 3, 6 kills -> 6, 10 kills -> 10.
- Character `R:` line marks the correct lore enemy as revenge.
- Song score 14 affecting monsters: no legendary capture.
- Starting song score 15 affecting no monster: no legendary capture.
- Starting song score 15 affecting at least one monster: chance can create `dungeon.db` record.
- Reload a save on a level with spawned legendary area: area ids and entry message survive.
- Enter legendary area without the song: song becomes singable while inside.
- Enter legendary area with the song: effective song score is +5 while inside.
- Leave area while singing an area-only song: song stops.

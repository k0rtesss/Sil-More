# Manwe/Sauron Light and Unlight Questline Stage Plan

Status: planning document.

Goal: implement the post-Valar-chain Manwe deception quest, then branch the metarun into Light or Unlight long-term paths.

This plan is intentionally staged so each stage can be landed and validated before the next one. It assumes one top-level coordinator integrates isolated worker patches. Workers are not alone in the codebase; every worker must keep to its assigned files, avoid reverting unrelated edits, and report exact changed paths.

## Current Repo Facts

- The current quest ladder has quest IDs `1..16` in `lib/edit/quest.txt`, with `QUEST_SLOT_MAX` and `METARUN_QUEST_SLOT_MAX` set to `24`.
- `lib/edit/limits.txt` currently has `M:Q:18`; adding quest template records requires raising this.
- Metarun records are raw `struct metarun` records in `meta.raw`. Prefer existing reserved bytes first; if the struct grows, add a real legacy reader/converter in `src/metarun_legacy.*`.
- Current dungeon logic treats `MORGOTH_DEPTH == 20` as both final depth and Morgoth throne depth. This quest needs final depth 23 with no normal Morgoth throne room, so those meanings must be separated.
- Existing content already includes useful anchors:
  - `R_IDX_UNGOLIANT`, `R_IDX_GOTHMOG`, `R_IDX_GLAURUNG`, `R_IDX_GORTHAUR`, plus monster records for Ancalagon, Manwe, and the Eagle of Manwe in `lib/edit/monster.txt`.
  - `Ungoliant's lair`, `Gothmog's hall`, `Ancalagon's Aerie`, `Morgoth's throne room`, and `Gates of Angband` vault content in `lib/edit/vault.txt`.
  - quest UI/text helpers in `src/xtra2.c`, intro/story presentation in `src/dungeon.c`, and end-of-run flow in `src/metarun.c` / `src/files.c`.

## Open Design Questions

These are deliberately isolated so implementation can start without pretending they are settled.

1. Unlight ally persuasion needs a peaceful/non-lethal resolution after defeating Gothmog, Ancalagon, and Glaurung.
2. Unlight side oaths are not designed yet. Stage 9 should add placeholders and hooks, not final oath mechanics, unless those oaths are specified.
3. The refusal flow after Ungoliant has one ambiguity: if the player refuses Manwe's request before forging, decide whether Sauron must be defeated before the forge can be used, or whether this refusal is an alternate immediate combat branch.
4. The Light endgame "Fall of Gondolin" scene needs a specific boss/objective. Fall of Gondolin should have Gothmog as the first boss, and Balrog of Sudden Flame in the end. 

## Agent Model Policy For This Work

Recommended coordinator: `gpt-5.5`, xhigh reasoning. The coordinator owns sequencing, merge review, save/metarun compatibility, and final validation.

Implementation workers: `gpt-5.5`, high or xhigh. Use xhigh for metarun/save/load, dungeon generation, depth semantics, AI/faction work, and branch endgame flows. Use high for bounded UI/status hooks or contained interaction logic.

Data/content workers: `gpt-5.4-mini`, xhigh reasoning for small data-selection jobs; use `gpt-5.5` high if the data task touches parser limits, exact vault token tables, or save-facing constants.

Subagents used while preparing this plan:

| Subagent | Model | Reasoning | Purpose |
| --- | --- | --- | --- |
| Metarun/quest explorer | `gpt-5.5` | high | Metarun, quest state, save/scoring risks |
| Generation explorer | `gpt-5.5` | high | Depth, vault, forge, throne, and Gates hooks |
| Content explorer | `gpt-5.4-mini` | xhigh | Existing monster/vault/story/artifact content anchors |

## Stage 0: Coordinator Setup

Owner: top agent, `gpt-5.5`, xhigh.

Actions:

1. Create a feature branch from current `develop`.
2. Record current worktree state with `git status --short`; do not overwrite user changes.
3. Confirm current version constants in `src/defines.h`.
4. Confirm highest quest ID, `M:Q`, and metarun quest slots:
   - `rg -n "QUEST_ID_|QUEST_SLOT_MAX|METARUN_QUEST_SLOT_MAX|M:Q" src lib/edit`
5. Decide the version bump before persistent fields are written.

Deliverable: no gameplay edits; branch ready and compatibility gate chosen.

Validation:

```powershell
git status --short
rg -n "VERSION_STRING|VERSION_EXTRA|MIN_VERSION_EXTRA" src/defines.h
rg -n "QUEST_ID_|QUEST_SLOT_MAX|METARUN_QUEST_SLOT_MAX|M:Q" src lib/edit
```

## Stage 1: Branch State Model

Purpose: define persistent state for Light/Unlight without touching generation yet.

Parallel workers:

| Worker | Model | Reasoning | Ownership |
| --- | --- | --- | --- |
| A: Metarun state | `gpt-5.5` | xhigh | `src/metarun.h`, `src/metarun.c`, `src/metarun_legacy.*`, `src/quest.c` |
| B: Player/save state | `gpt-5.5` | xhigh | `src/types.h`, `src/save.c`, `src/load.c`, `src/birth.c`, `src/defines.h` |
| C: Data skeleton | `gpt-5.4-mini` | high | `lib/edit/quest.txt`, `lib/edit/limits.txt`, optional text placeholders |

Required state:

- Metarun branch:
  - `NONE`
  - `MANWE_QUEST_PENDING`
  - `MANWE_QUEST_ACTIVE`
  - `LIGHT_CHOSEN`
  - `UNLIGHT_CHOSEN`
  - `LIGHT_ENDGAME_ACTIVE`
  - `UNLIGHT_FINAL_ACTIVE`
  - `COMPLETE`
- Run-local state for the deception run:
  - Manwe appeared/offered.
  - Depth-23 vault generated.
  - Ungoliant slain.
  - Light of the Trees acquired.
  - Silmaril forged.
  - Sauron revealed.
  - Sauron bargain accepted/refused.
  - Morgoth chase/summon triggered.
- Unlight ally mask:
  - Gothmog persuaded.
  - Ancalagon persuaded.
  - Glaurung persuaded.
- Light endgame scene counters:
  - success/fall counts for Feanor vs Gothmog, Fingolfin vs Morgoth, Turin vs Glaurung, Earendil vs Ancalagon, Fall of Gondolin.

Implementation notes:

- First try to use `metar.reserved_runtime[]` for branch bytes and counters. Document every consumed slot in `src/metarun.h`.
- If more state is needed than reserved bytes can safely hold, grow `struct metarun` only in this stage and add a legacy layout in `src/metarun_legacy.*`.
- Do not burn one `METARUN_QUEST_*` slot per cutscene unless the quest-slot count is deliberately expanded beyond 24.
- Add helper APIs instead of exposing raw reserved slots:
  - `metarun_branch_state()`
  - `metarun_set_branch_state(...)`
  - `metarun_unlight_ally_mask()`
  - `metarun_mark_unlight_ally(...)`
  - `metarun_light_scene_record(...)`
  - `metarun_manwe_quest_unlocked()`

Acceptance criteria:

- Old savefiles still load with default branch state `NONE`.
- Old `meta.raw` still loads.
- New metarun state saves and reloads.
- No gameplay behavior changes yet.

Validation:

```powershell
git diff --check
.\build-incremental.ps1
```

## Stage 2: Unlock Rules And Challenge Completion Accounting

Purpose: unlock Manwe only after a complete three-quest Valar chain plus that chain's challenge completion.

Owner: `gpt-5.5`, high or xhigh.

Likely files:

- `src/metarun.c`
- `src/metarun.h`
- `src/quest.c`
- `src/xtra2.c`
- challenge/run-mode files found by inspection

Tasks:

1. Audit challenge completion. Current counters exist, but challenge completion marking may not be fully wired.
2. Add a helper that answers: "has any Vala line been completed through stage 3, and has its challenge been completed?"
3. When the condition first becomes true, set metarun branch state to `MANWE_QUEST_PENDING`.
4. Do not start the quest in the middle of the current character. Manwe appears on the next new story character.
5. Add logs at unlock time.

Suggested unlock helper shape:

```c
bool metarun_manwe_quest_unlocked(void);
bool metarun_completed_any_vala_chain_with_challenge(void);
```

Acceptance criteria:

- Completing any full chain plus its challenge marks the Manwe quest pending.
- Completing only stage 1/2/3 without challenge does not.
- Existing Valar quest rewards and repeat caps still work.

Validation:

```powershell
rg -n "metarun_mark_challenge_completed|CHALLENGE_|metarun_manwe" src
.\build-incremental.ps1
```

## Stage 3: Run-Type Semantics For Special Story Runs

Purpose: separate normal story runs, the Manwe deception run, Light cutscene runs, Unlight ally runs, and Unlight final battle.

Parallel workers:

| Worker | Model | Reasoning | Ownership |
| --- | --- | --- | --- |
| A: run helpers | `gpt-5.5` | xhigh | new `src/story_branch.h/.c`, `CMakeLists.txt`, `src/externs.h` only if needed |
| B: startup integration | `gpt-5.5` | high | `src/dungeon.c`, `src/birth.c`, `src/metarun.c` |

Tasks:

1. Add branch helper APIs:
   - `story_branch_run_kind()`
   - `story_branch_is_manwe_deception_run()`
   - `story_branch_is_light_cutscene_run()`
   - `story_branch_is_unlight_ally_run()`
   - `story_branch_is_unlight_final_run()`
2. At new character startup, activate the correct special run kind from metarun branch state.
3. If the player dies during the Manwe deception run before choosing a side, keep `MANWE_QUEST_ACTIVE` so the next run repeats the same quest.
4. Suppress normal Valar quest lottery during the Manwe deception run.
5. For Unlight, suppress normal Valar quests and oath acquisition once `UNLIGHT_CHOSEN`.

Acceptance criteria:

- Normal story startup is unchanged.
- When state is `MANWE_QUEST_PENDING`, the next character becomes a Manwe deception run.
- Dying before Light/Unlight choice repeats the Manwe deception run.

Validation:

```powershell
git diff --check
.\build-incremental.ps1
```

## Stage 4: Final Depth Refactor

Purpose: support final depth 23 without creating a Morgoth throne room there.

Owner: `gpt-5.5`, xhigh.

Likely files:

- `src/defines.h`
- `src/generate.c`
- `src/dungeon.c`
- `src/load.c`
- `src/save.c`
- `src/object2.c`
- `src/monster2.c`
- any file found by `rg -n "MORGOTH_DEPTH"`

Tasks:

1. Add helpers:
   - `int run_final_depth(void);`
   - `int run_morgoth_depth(void);`
   - `bool run_has_morgoth_throne_room(void);`
   - `bool current_depth_is_final_depth(void);`
   - `bool current_depth_is_morgoth_throne(void);`
2. Normal story:
   - final depth = 20.
   - Morgoth throne depth = 20.
3. Manwe deception run:
   - final depth = 23.
   - no Morgoth throne room.
4. Replace `MORGOTH_DEPTH` where it means "current final depth".
5. Keep literal `MORGOTH_DEPTH` only where it truly means the normal story's Morgoth throne depth.
6. Update load validation so depths 21-23 are legal only for savefiles whose branch state allows them.
7. Ensure stairs at depth 23 only go up unless a scripted escape changes the level flow.

Acceptance criteria:

- Normal depth 20 still generates the throne room.
- Manwe depth 23 never generates the normal throne room.
- Saves at depths 21-23 load only for the special run.
- Normal save validation remains strict.

Validation:

```powershell
rg -n "MORGOTH_DEPTH" src
.\build-incremental.ps1
.\build-cmake.bat
```

## Stage 5: Manwe Deception Quest Content And Depth-23 Vault

Purpose: create the actual Manwe quest run: descend to 23, find Grond's forge, kill Ungoliant, claim the Light of the Trees.

Parallel workers:

| Worker | Model | Reasoning | Ownership |
| --- | --- | --- | --- |
| A: vault/data | `gpt-5.4-mini` | high | `lib/edit/vault.txt`, `lib/edit/quest.txt`, `lib/edit/object.txt` or `artefact.txt`, `lib/edit/limits.txt` |
| B: forced generation | `gpt-5.5` | xhigh | `src/generate.c`, exact vault token helpers if needed |
| C: quest objective hooks | `gpt-5.5` | high | `src/xtra2.c`, `src/monster2.c`, `src/cmd1.c`/death hooks as needed |

Data tasks:

1. Append a new quest record after ID 16, for example `Q:17:Manwe, The Forge of Grond`.
2. Raise `M:Q` in `lib/edit/limits.txt`.
3. Append a new type-8 great vault for the Forge of Grond, with:
   - an exact `U` token for Ungoliant, or a new exact token if needed;
   - a distinctive forge area;
   - no Morgoth throne room geometry;
   - stable GUIDs where the template supports them.
4. Add a quest-only item if the Light of the Trees should be carried separately from the forged Silmaril.

Generation tasks:

1. Force the Forge of Grond vault on depth 23 during Manwe deception runs.
2. Make failure to place the vault regenerate the level.
3. Ensure the level is marked as final depth but not Morgoth throne depth.
4. Keep the forced vault out of normal `quest_vault_used` accounting unless explicitly needed.

Objective tasks:

1. Manwe appears at the beginning of the run and gives the quest.
2. Ungoliant death marks `UNGOLIANT_SLAIN`.
3. Ungoliant death or the vault center produces the Light of the Trees.
4. Manwe appears again after Ungoliant dies.

Acceptance criteria:

- In the special run, depth 23 always has the Forge of Grond vault.
- Ungoliant cannot spawn outside its intended vault.
- Killing Ungoliant advances the quest state.
- Death before side choice repeats the same quest next run.

Validation:

```powershell
python tools/make_guid.py --dry-run lib/edit/vault.txt lib/edit/quest.txt lib/edit/object.txt lib/edit/artefact.txt
.\build-incremental.ps1
```

## Stage 6: Silmaril Forge And Sauron Reveal

Purpose: implement the post-Ungoliant choice, quest-only Silmaril creation, Sauron reveal, and Light/Unlight branch choice.

Owner: `gpt-5.5`, xhigh.

Likely files:

- `src/xtra2.c`
- `src/cmd4.c`
- `src/object2.c`
- `src/files.c`
- `src/metarun.c`
- `src/monster2.c`
- `src/generate.c`

Tasks:

1. Add a Manwe dialogue prompt after Ungoliant is slain:
   - yes: ask the player to use the forge.
   - no: reveal Sauron and trigger combat, then continue into the forge/reveal path according to the resolved design.
2. Implement quest-only forge interaction:
   - normal smithing should not be required;
   - existing Silmaril `NO_SMITHING` rules remain intact;
   - the quest forge directly creates the new Silmaril/light object after objective checks pass.
3. After the Silmaril is forged, reveal Sauron:
   - Sauron says it was him all along.
   - Sauron asks the player to join him against Morgoth.
4. If the player agrees:
   - set metarun branch state `UNLIGHT_CHOSEN`;
   - end the current run cleanly;
   - record notes/score branch outcome.
5. If the player refuses:
   - set metarun branch state `LIGHT_CHOSEN` immediately;
   - Sauron summons Morgoth;
   - set `p_ptr->on_the_run` or a branch-specific chase flag;
   - whether the player escapes or dies, the branch remains Light.

Acceptance criteria:

- The player cannot choose both branches.
- Agreeing ends the run and starts future Unlight behavior.
- Refusing chooses Light even if the player dies in the chase.
- The forged quest Silmaril is not available in normal smithing.

Validation:

```powershell
.\build-incremental.ps1
```

Manual smoke:

- Say yes, forge, agree with Sauron.
- Say yes, forge, refuse Sauron, die during chase.
- Say yes, forge, refuse Sauron, escape.
- Say no at first Manwe prompt.

## Stage 7: Light Branch Continuation

Purpose: after choosing Light, the next run resumes the existing metarun structure with an Eagle of Manwe explanation.

Parallel workers:

| Worker | Model | Reasoning | Ownership |
| --- | --- | --- | --- |
| A: startup scene | `gpt-5.5` | high | `src/dungeon.c`, `src/xtra2.c`, `src/metarun.c` |
| B: text/data | `gpt-5.4-mini` | high | `lib/edit/story.txt`, optional `lib/edit/quest.txt` text |

Tasks:

1. On the first new run after `LIGHT_CHOSEN`, spawn/show the Eagle of Manwe message.
2. Mark the message as seen in metarun state.
3. Re-enable normal Valar quest/metarun flow after the message.
4. Keep old blessing/oath/scoring behavior unchanged.

Acceptance criteria:

- Eagle message appears once.
- Normal metarun progression resumes.
- Light branch remains recorded for later endgame scenes.

Validation:

```powershell
.\build-incremental.ps1
```

## Stage 8: Light Endgame Cutscene Framework

Purpose: when the Light metarun reaches its end and the true name is revealed, run five special cinematic challenge scenes.

Owner: `gpt-5.5`, xhigh.

Do this before authoring all five scenes. The framework is the risky part.

Core design:

- These are not normal campaign runs.
- Each scene is a bounded special level with a fixed identity mantle:
  - Feanor vs Gothmog.
  - Fingolfin vs Morgoth.
  - Turin vs Glaurung.
  - Earendil vs Ancalagon.
  - Fall of Gondolin.
- Each scene records success or fall.
- Death in a scene does not erase the Light branch. It increments a scene failure counter and advances to the next scene.
- Completed Valar challenges grant scene assistance. Start with simple, explicit boons:
  - completed Tulkas chain/challenge: melee/combat aid;
  - Aule: forge/equipment aid;
  - Mandos: one reprieve or death mitigation;
  - Nienna: mercy/healing;
  - Orome: hunt/archery/speed;
  - Varda: light against darkness.

Tasks:

1. Add a cutscene run-kind and scene index.
2. Add a special-level generator selector per scene.
3. Add scene-end handling that records success/fall and advances.
4. Add an end summary that shows successful and fallen attempts in original First Age styled prose.
5. Only after the framework works, add bespoke vaults/text for each scene.

Parallel after framework:

| Worker | Model | Reasoning | Ownership |
| --- | --- | --- | --- |
| Scene data A | `gpt-5.4-mini` | high | Feanor/Gothmog and Fingolfin/Morgoth vault/text |
| Scene data B | `gpt-5.4-mini` | high | Turin/Glaurung and Earendil/Ancalagon vault/text |
| Scene data C | `gpt-5.5` | high | Fall of Gondolin mechanics if it needs waves/escape |

Acceptance criteria:

- The five scenes run in order.
- A death advances to the next scene instead of normal metarun death handling.
- The final summary includes success/fall counts.
- Valar help appears only for completed challenge lines.

Validation:

```powershell
.\build-incremental.ps1
.\build-cmake.bat
```

## Stage 9: Unlight Branch Restrictions

Purpose: after choosing Unlight, the metarun becomes a new campaign path.

Parallel workers:

| Worker | Model | Reasoning | Ownership |
| --- | --- | --- | --- |
| A: quest/oath restrictions | `gpt-5.5` | high | `src/generate.c`, `src/quest.c`, `src/birth.c`, oath selection code |
| B: UI/status text | `gpt-5.4-mini` | high | `src/cmd4.c`, `lib/edit/story.txt`, `lib/edit/oath.txt` placeholders |

Tasks:

1. Disable Valar quest lottery and Valar quest-giver spawns for `UNLIGHT_CHOSEN`.
2. Disable acquiring new Valar oaths.
3. Preserve already-recorded metarun history, but do not reveal the true name.
4. Add visible status text that the character is forgotten/nameless.
5. Add placeholder hooks for future Unlight oaths gained from persuaded allies.

Acceptance criteria:

- No Valar quests appear in Unlight runs.
- No new Valar oaths can be chosen.
- Existing saves/metaruns outside Unlight are unchanged.

Validation:

```powershell
.\build-incremental.ps1
```

## Stage 10: Unlight Ally Vault Runs

Purpose: each Unlight run has a guaranteed depth-20 great vault for one random unpersuaded ally.

Parallel workers:

| Worker | Model | Reasoning | Ownership |
| --- | --- | --- | --- |
| A: vault data | `gpt-5.4-mini` | high | `lib/edit/vault.txt`, stable GUID updates |
| B: forced placement | `gpt-5.5` | xhigh | `src/generate.c`, exact vault/token helpers |
| C: monster scaling | `gpt-5.5` | xhigh | `src/monster2.c`, combat stat calculation hooks as needed |
| D: persuasion objective | `gpt-5.5` | high | `src/xtra2.c`, `src/metarun.c` |

Tasks:

1. Choose randomly among unpersuaded Gothmog, Ancalagon, and Glaurung.
2. Force the chosen ally vault on depth 20.
3. Do not generate the normal Morgoth throne room for these Unlight ally runs.
4. Apply vault-local stat scaling:
   - all monsters in the ally vault: +30% relevant stats;
   - ally boss: +50% relevant stats.
5. Avoid global `r_info` mutations if possible. Prefer per-monster instance scaling, tagged by vault placement.
6. Implement the persuasion completion:
   - defeat the ally;
   - run the peaceful/non-lethal resolution placeholder;
   - mark the ally in the metarun ally mask;
   - unlock a placeholder ally oath/reward hook.

Acceptance criteria:

- Every Unlight ally run has exactly one unpersuaded ally vault at depth 20.
- The same ally is not selected again after persuasion.
- Scaling applies only inside the ally vault.
- After all three allies are persuaded, branch state moves to `UNLIGHT_FINAL_ACTIVE`.

Validation:

```powershell
.\build-incremental.ps1
```

Manual smoke:

- Start three Unlight runs and confirm all three allies can be persuaded.
- Save/load before and after depth 20.
- Confirm normal story depth 20 still has Morgoth in non-Unlight runs.

## Stage 11: Unlight Final Battle At The Gates

Purpose: final Unlight battle: Sauron plus persuaded allies against Morgoth at the Gates of Angband.

Owner: `gpt-5.5`, xhigh.

Likely files:

- `src/generate.c`
- `src/monster2.c`
- `src/melee2.c`
- `src/xtra2.c`
- `src/files.c`
- `src/metarun.c`
- `lib/edit/vault.txt`

Tasks:

1. Reuse depth 0 / Gates of Angband as the final map.
2. Add a run-aware Gates vault selector only if a different layout is required; the current type-10 path hardcodes the existing Gates template.
3. Spawn Morgoth as enemy.
4. Spawn Sauron plus persuaded allies on the player's side.
5. Implement minimal allied behavior:
   - allies do not attack player;
   - allies target Morgoth and hostile forces;
   - Morgoth can target allies and player.
6. On victory, end the metarun as the new Dark Lord alongside Sauron.
7. On failure, preserve the intended Unlight failure/defeat summary.

Acceptance criteria:

- Final battle starts only after all three allies are persuaded.
- Allies and Morgoth fight correctly.
- Victory ends the Unlight metarun with a distinct ending.

Validation:

```powershell
.\build-incremental.ps1
.\build-cmake.bat
```

## Stage 12: Score, Run History, And Knowledge UI

Purpose: make the branch visible to players and analytics.

Parallel workers:

| Worker | Model | Reasoning | Ownership |
| --- | --- | --- | --- |
| A: score/run history | `gpt-5.5` | high | `src/score/`, `src/files.c`, score format gates |
| B: UI/status pages | `gpt-5.4-mini` | high | `src/cmd4.c`, quest/metarun status screens |
| C: data text polish | `gpt-5.4-mini` | high | `lib/edit/story.txt`, `lib/edit/quest.txt`, `lib/edit/oath.txt` |

Tasks:

1. Add branch outcome to run history:
   - Manwe quest started/completed.
   - Light chosen.
   - Unlight chosen.
   - allies persuaded.
   - Light cutscene success/fall counts.
   - Unlight final victory.
2. Add metarun status lines for current branch.
3. Add notes for key story events.
4. Keep text original and compact; do not quote Tolkien.

Acceptance criteria:

- A player can see which branch they are on.
- Score records distinguish Light and Unlight outcomes.
- Existing score files migrate or default safely.

Validation:

```powershell
.\build-incremental.ps1
```

## Stage 13: Full Validation Matrix

Owner: top agent plus optional `gpt-5.4-mini` validation subagent for data checks.

Automated checks:

```powershell
git diff --check
python tools/make_guid.py --dry-run
.\build-incremental.ps1
.\build-cmake.bat
```

Manual smoke tests:

1. Load old save and old `meta.raw`.
2. Normal story run to depth 20 still creates Morgoth throne room.
3. Manwe unlock does not happen before a full chain plus challenge completion.
4. Manwe deception run repeats after death before branch choice.
5. Manwe deception run reaches depth 23 with Forge of Grond, no throne room.
6. Ungoliant death triggers Manwe return.
7. Forge creates the quest Silmaril only in the quest context.
8. Agreeing with Sauron starts Unlight path.
9. Refusing Sauron starts Light path even if the player dies during the chase.
10. Light path resumes normal metarun after Eagle message.
11. Light endgame scenes count wins and falls.
12. Unlight path blocks Valar quests and new Valar oaths.
13. Unlight ally vault appears at depth 20 and uses vault-local scaling.
14. All three allies unlock the Gates final battle.
15. Unlight final victory ends with the Dark Lord ending.

## Recommended Implementation Order

1. Stages 0-2: state and unlock correctness.
2. Stage 4 before Stage 5: depth semantics must be safe before the depth-23 quest content lands.
3. Stages 5-6: Manwe deception run and branch choice.
4. Stage 7: Light continuation, because it restores normal gameplay and is smaller.
5. Stage 9: Unlight restrictions, because it prevents content conflicts.
6. Stage 10: Unlight ally runs.
7. Stage 11: Unlight final battle.
8. Stage 8 can run in parallel after Stage 7 if a separate worker owns the Light cutscene framework.
9. Stage 12-13: UI, score, and full validation.

## Worker Prompt Templates

Use these as starting prompts when spawning implementation workers.

### High-Risk Implementation Worker

```text
Model: GPT-5.5
Reasoning: xhigh
Role: worker

You are not alone in the codebase. Do not revert edits made by others. Keep to the files explicitly assigned below and adapt to neighboring changes.

Read AGENTS.md and docs/manwe_sauron_light_unlight_questline_stage_plan.md.

Stage: <stage name>
Owned files:
- <file list>

Implement only this stage. Preserve normal story behavior unless the stage explicitly changes it. Report changed files, validation run, and any unresolved design decision.
```

### Data Selection Worker

```text
Model: GPT-5.4-mini
Reasoning: high
Role: worker

You are not alone in the codebase. Do not edit code unless explicitly assigned. Keep template IDs strictly increasing and preserve stable GUIDs.

Read AGENTS.md and docs/manwe_sauron_light_unlight_questline_stage_plan.md.

Stage: <stage name>
Owned files:
- lib/edit/<files>

Add or revise only the requested content. Use original First Age styled prose; do not quote Tolkien. Run or request `python tools/make_guid.py --dry-run` after data edits.
```

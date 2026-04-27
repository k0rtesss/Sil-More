# Meta States Validation Checklist

Manual QA checklist for Round 2 integration of metarun-local artefacts, revenge monsters, and legendary song areas.

This pass assumes:
- production code is still landing in B/C/D branches
- Agent A's DB layer is the shared baseline
- validation should confirm both gameplay behavior and persistence behavior

## Test Roots

Use the active data root for the build under test.

- Standard build:
  - saves: `<user-root>/save/`
  - meta DBs: `<user-root>/meta/`
- Portable build:
  - saves: `lib/save/`
  - meta DBs: `lib/apex/`

Expected meta DB filenames:
- `artefact.db`
- `monster.db`
- `dungeon.db`

Expected DB header magics from Agent A:
- artefacts: `MSAF`
- monsters: `MSMO`
- dungeon areas: `MSDG`

## Global Smoke Before Feature QA

1. Build and launch the target branch.
2. Start a fresh metarun.
3. Confirm old meta DBs are absent or recreated cleanly under the active meta path.
4. Confirm `log.txt` has no startup warnings about incompatible meta DBs unless you intentionally seeded an old-format file.
5. If save compatibility changed, confirm save header extra version changed from `0` to the intended new value.

## Artefacts

### A1. Difficulty 14 does not persist

1. Start a metarun with a strong smith.
2. Forge a custom artefact with computed difficulty `14`.
3. Complete smithing and finish the run or inspect the DB immediately.

Expected:
- no remembrance prompt appears
- no new `artefact.db` record is written
- the item exists only for the current character/run as normal self-made artefact state
- the next character in the same metarun cannot find it in the drop pool

### A2. Difficulty 15 persists with creation metadata

1. Forge a custom artefact with computed difficulty `15`.
2. Rename it when prompted.
3. Leave the level, save, quit, and inspect `artefact.db`.

Expected:
- prompt text appears before final acceptance
- one new record exists in `artefact.db`
- record stores:
  - current `metar.id`
  - artefact GUID
  - creator character GUID and name
  - creation depth
  - saved difficulty `15`
  - persisted description text
- current run drop pool still does not include this artefact

### A3. Masterpiece or Aule's Forge raises drop weight

1. Forge two eligible remembered artefacts at the same saved difficulty:
   - normal
   - Masterpiece or Aule's Forge
2. Compare stored rarity-related fields and later drop frequency behavior.

Expected:
- both records persist
- the special-forge record stores the correct flag
- the injected runtime artefact for the special-forge record is more common than the normal one

### A4. Next character in same metarun can find remembered artefact

1. End the creator character.
2. Start a second character in the same metarun.
3. Reach at least the creation depth band where the remembered artefact can appear.
4. Force or sample artefact drops until the record appears.

Expected:
- remembered artefact is eligible only for later characters in the same metarun
- item spawns from the drop system with the stored depth, stats, name, and description
- current character's own newly forged artefacts do not enter its own run's pool

### A5. New metarun clears remembered artefacts

1. After confirming remembered artefacts exist, start a new metarun.
2. Inspect the meta directory and launch a new character.

Expected:
- `artefact.db` is removed or ignored by metarun id
- remembered artefacts from the old metarun never enter the new metarun drop pool

## Revenge Monsters

### R1. First death writes one revenge record

1. Start a metarun.
2. Die to a normal monster with a stable race GUID.
3. Inspect `monster.db`.

Expected:
- exactly one record is created for that monster
- record stores:
  - current `metar.id`
  - monster GUID
  - killed character GUID and name
  - depth and turn
  - cause string
  - rank `1`
- Morgoth and any explicitly forbidden quest monster do not create a record

### R2. Repeated deaths update the same monster up to rank 3

1. In the same metarun, let the same monster race kill later characters again.
2. Inspect `monster.db` after each death.

Expected:
- the feature behaves as one evolving revenge entry, not as duplicate live entries
- rank increases to `2`, then `3`, and stops there
- kill memories retain the latest relevant slain characters

### R3. Spawned revenge monster is runtime-unique

1. Start another character in the same metarun after at least one revenge record exists.
2. Encounter the revenge monster.

Expected:
- displayed name carries `*`, `**`, or `***` by rank
- runtime copy behaves as unique:
  - unique flag behavior
  - `max_num = 1`
  - no permanent mutation of base `r_info`
- description or recall text names the slain character

### R4. Revenge bonus milestones match exact totals

1. Kill revenged monsters across multiple characters in one metarun.
2. Track cumulative revenged kills and the actual combat bonus.

Expected totals:
- 1 kill -> bonus 1
- 3 kills -> bonus 3
- 6 kills -> bonus 6
- 10 kills -> bonus 10

### R5. Character `R:` lines mark intended enemies

1. Use a character template that gains an `R:` mark after C lands.
2. Start a new character from that template.
3. Verify the intended enemy is marked before any new deaths occur.

Expected:
- parser accepts `R:` lines cleanly
- GUID resolution wins over name resolution
- birth copies the configured revenge mark into runtime state

## Legendary Song Areas

Suggested starting-song templates already present in `character.txt`:
- Finarfin / Earendil: `Song of the Trees`
- Luthien: `Lorien`
- Elu Thingol / Melian Maia: `Mastery`, `Thresholds`
- Maglor: `Lament`
- Daeron: `Staunching`
- Turgon: `Disguise`

### L1. No capture below threshold

1. Use a starting-song character.
2. Produce song effects at effective score `14`.

Expected:
- no `dungeon.db` record is created

### L2. No capture without affected monster

1. Use a starting-song character at effective score `15+`.
2. Sing in a situation where no monster is actually changed, damaged, feared, slept, or weakened.

Expected:
- no `dungeon.db` record is created

### L3. Eligible song can create a legendary record

1. Use a starting-song character at effective score `15+`.
2. Sing in a legal depth with at least one real monster effect.
3. Repeat until the chance roll succeeds.
4. Inspect `dungeon.db`.

Expected:
- record stores:
  - current `metar.id`
  - song id
  - singer name and character GUID
  - creation turn and depth
  - source kind / partition kind / big cave type
  - relative singer coordinates
  - bounded dimensions and mask cell count
  - affected monster GUID list
  - entry text
  - tile blob
- capture does not include objects, traps, stairs, or transient view/light state

### L4. Large-area capture crops instead of recording the whole partition

1. Trigger a capture in:
   - a greater vault edge case
   - a large big-cave partition
   - a labyrinth or corridor section
2. Compare the saved area bounds with the live map.

Expected:
- capture stays within the hard size cap
- greater vault capture is cropped, not whole-vault
- big caves do not serialize an entire partition
- saved mask remains connected to the singer or intended affected focus

### L5. Spawned area appears on matching depth only

1. After a record exists, start a later character in the same metarun.
2. Visit the saved depth and nearby non-matching depths.

Expected:
- at most one legendary area appears on a level
- only matching depth levels are eligible
- fit rules reject bad overlap with greater vaults, Morgoth tunnels, quest vaults, or permanent walls

### L6. Entry aura and exit behavior

1. Enter a spawned legendary area without the song.
2. Begin singing the granted song inside the area.
3. Leave the area while still singing.

Expected:
- entry message appears once per level
- song becomes available while inside
- if the character already knows the song, effective skill is increased while inside
- if the character knows it only from the area, leaving the area stops the song

### L7. Save and reload on the area level

1. Stand inside a spawned legendary area.
2. Save and quit.
3. Reload the save and re-enter or move within the area.

Expected:
- area id map survives reload
- area membership remains stable after reload
- entry message does not spam repeatedly after reload
- granted song availability still matches inside/outside state

## Save And Compatibility Checks

### S1. Old saves still load with defaults

1. Load a pre-meta-states save from the same major/minor/patch line.

Expected:
- load succeeds if the intended compatibility floor allows it
- new player/runtime state defaults safely
- no object-count corruption or dungeon desync occurs

### S2. New saves round-trip with legendary-area state

1. Save on a level containing a spawned legendary area.
2. Reload the save.

Expected:
- save/load order is stable
- any new dungeon block marker is recognized
- if the new block is absent, loader defaults the map to zero

### S3. New metarun isolation

1. Create all three record types in one metarun.
2. Start a new metarun.

Expected:
- stale records do not leak into gameplay even if file deletion failed
- load paths still filter by `metar.id`

## Logs And Evidence To Capture

For each completed pass, capture:
- build flavor: standard or portable
- exact save path used
- exact meta DB path used
- whether the DB file was absent, recreated, or updated
- relevant `log.txt` lines for save/load and metarun rollover
- one reproduction save for:
  - remembered artefact creator
  - revenge monster spawn candidate
  - legendary area spawn level

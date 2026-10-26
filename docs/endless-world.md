# Endless dungeon partitions

Each dungeon depth has a signed 64-bit world address. The engine presents a
moving 222 by 222 local window: five by five 44-tile partitions and a guard
border. Walking near the window's outer band shifts the local frame by a
partition and constructs the incoming band. The player, camera, nearby terrain,
actors, object stacks, fluids and map memory keep their world positions.
Shifting does not consume energy or advance the game clock. Mouse travel keeps
its world destination and recomputes the local route.

Ordinary partitions leaving the window are discarded. Their rooms, loose loot,
occupants, terrain edits, explored map and local skeleton clues are forgotten.
Returning builds a fresh ordinary layout and population. Surviving local clues
remain available; forgetting older clues renews skeleton clue selection.
The static tutorial reference remains available.

## Stable geography

A saved world seed and depth select coordinate-based watersheds. Their smooth
channels, widened lake sections, material and downstream direction are evaluated
locally without allocating an explored-world history. Depth rules choose water,
poison, lava or chasms. Adjacent partitions derive matching walking entrances
from a shared world edge. The same geography is sampled on either side of zero
and in any discovery order.

Ordinary interiors use a separate generation RNG and a saved monotonic
materialization counter. Their layouts change after forgetting while the major
geography and boundary entrances remain stable. Bounded generation reuses the
six partition modes, their population/drop policies and themed carvers. It does
not rerun quest or great-vault lotteries for each incoming strip; existing run
limits still apply.

## Permanent places

Great vaults, quest rooms and giver sites have sparse world-addressed snapshots
of their footprint and a small approach apron. Authored surface and Utumno
complexes are preserved as sites as well. Ordinary distant maps are not saved.

Snapshots retain changed terrain, opened/emptied containers, remaining actors
and their health, held/floor objects, environmental cells and reservoir budgets,
paired-vault state, quest coordinates and discovery/reward credit. Killed
occupants and removed treasure are not rebuilt from their templates. Unloaded
site timers are suspended instead of applying an off-screen backlog on return.

Site-owned entities have stable tokens independent of local object/monster
indexes. Transfers to another depth, carried loot and split stacks preserve
ownership without duplicating archived copies. Sleeping uniques and artefacts
remain reserved. Quest progress is not reset when its ordinary floor leaves
the cache.

Discovered locations appear under **Remembered places** in **Hints & Quests**,
with depth and north/south/east/west tile distances from the player. Unknown
sites stay hidden. A remembered place does not retain the ordinary route to it
or an expired skeleton clue.

All resident depths use the same world origin. Stairs, shafts, chasms and fluid
portals keep their shared world column while their local projections move.
Background regeneration receives a private copy of existing site geometry, so
access is checked against real saved doors and stairs before the floor is made
available. Paired vaults can overlap the window edge with signed local origins.

## Persistence and implementation

Save revision `1.1.0.10` adds frames, generation counters, site ownership and the
site archive. Earlier saves retain their current terrain when imported; existing
great-vault flags, quest givers and fixed quest coordinates identify the places
to preserve before the first window shift. New records have bounded counts,
coordinate/ownership checks and an explicit archive end marker.

Relevant sources:

- `level-stream-generation.c`: pure geography, shared entrances and bounded
  ordinary partition construction.
- `level-window.c`: transactional translation of grids, actors, references,
  environment state, hints and local metadata.
- `level-stream.c`: world frame, generation counter and site definitions.
- `level-stream-sites.c`: sparse site snapshots, ownership, bookmarks and saves.
- `level-cache.c`: coordinated resident-floor shifts and worker integration.

Focused validation:

```
python scripts/check_stream_geography.py
python scripts/check_stream_generation.py
python scripts/check_level_window.py
python scripts/check_endless_stream.py
python scripts/check_stream_mouse_path.py
```

The fixtures link production generation, cache, SDL route and serializer code.
They cover seams and negative coordinates, retention/forgetting, fresh return
layouts, altered sites, cross-depth ownership, inventory splits, old saves,
malformed/truncated records, paused timers, clipped paired vaults, camera
translation and real asynchronous floor regeneration. Actual game-control
playtesting complements these fixtures; physical-device replay is separate.

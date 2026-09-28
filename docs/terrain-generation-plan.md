# Underground terrain generation

Level generation plans unequal basins, narrow streams, tributaries, pools, occasional broad rivers and branching fractures before constructing the dungeon. Each system has a history: old geology with construction along its banks, a later disaster, or overflowing old basins. Up to two systems with different materials can coexist. Reach and width are separate choices. [The research record](terrain-underground-research.md) explains the rejected broad-channel design and the replacement.

## Order and geometry

`terrain_history_begin()` runs after basic granite and anchor reset, before the first guaranteed forge, quest vault or prefab. It selects each system's material, family and history, then plans its geometry. Old systems are carved immediately, with dry shore anchors for the corridor graph. A portion of ordinary room/vault site attempts favor banks; footprint checks prevent construction from covering the old course or its source backing.

The construction feature hook preserves preexisting liquid and source caps through natural cavern and labyrinth carving. Incompatible elemental partition recipes yield to older geology. `terrain_history_start_tunnels()` lets actual corridor excavation build oriented bridge decks across the old course. Shore anchors survive carving and remain candidates in the ordinary connection graph.

After corridors, initial stairs and door cleanup, `terrain_history_finish()` realizes old systems and overflow events first, then later disasters. It validates a complete shadow map and preserves authored content transactionally. A late disaster can adapt its route within its planned family when critical construction blocks the initial proposal. `place_dungeon_terrain()` then adds local accents before rubble, player placement and ordinary population. Work is bounded per system to 24 proposals, six basins, 1,024 cells per routed edge, 20 tracked bridge spans and 2,048 relocation records.

| History | Result |
| --- | --- |
| Ancient | Rooms fit the banks; narrow streams and separate pools retain their shape. No blanket flood/erosion pass. |
| Disaster | A later fissure, river, lava or poison system breaches the finished dungeon, with rubble and selected structural damage. |
| Overflow | Two to four shoreline expansion waves from up to two old basins, followed by nearby flood/breach incidents. Narrow connecting throats, bridges and source backing remain protected. An unsafe rise falls back to the old system. |

| Family | Geometry |
| --- | --- |
| Pool chain | Unequal independent basins joined by thin streams; retains the configured long reach. |
| Tributaries | A receiving passage with up to two basins and two or three distinct feeder merges. |
| Flooded chamber | One basin and narrow inlet in a chamber or permitted ruin; no map-spanning route prerequisite. |
| Regional pools | Several basins near a regional origin, with some visible connections and some separate pools. |
| Broad river | Occasional 2–4-cell main channel, one basin and an optional second. |
| Fractures | Chasms use narrow angular fissures, branches and a few larger openings. |

Basins have their own local floor-relief field, aspect ratio and fill level. Their size is independent of connector width. Planned basin footprints cannot overlap. History systems preserve their chosen morphology: a chain cannot silently turn into one flooded chamber, and incidents protect thin connector strips. Basin counts use actual broad cores in the final footprint. The legacy isolated landmark entry point remains for synthetic regression fixtures.

Routed edges prefer a coherent course, avoid immutable geometry and may cut granite and ordinary walls. Feeders stop at their first actual merge with an existing channel, rather than painting over the whole receiving stream. Distinct merges require at least six cells of separation, two existing receiving-spine neighbors and at least nine new feeder cells. The model is static generation, not a physical fluid simulation or saved water-level system.

Dry space varies too: wider basin shelves, occasional one-sided ledges, and fissures filled by the channel. There is no mandatory constant floor outline. Isolated newly excavated shore pockets stay rock. Smaller accents can coexist in touched partitions. Incompatible existing liquids are protected from arbitrary overwriting; dynamic mixing and hidden traversable connections are not introduced.

## Flow scenarios and endings

The planner assigns sources and receiving outlets before carving. A long bounding box alone is insufficient: the course must explain where its visible flow begins and ends.

| Scenario | Source and destination |
| --- | --- |
| Lake chain | The first and last real basins are the endpoints; narrow links connect the intervening basins. |
| Through-flow | Both ends reach the inner map boundary, leaving the permanent outer wall intact. All broad rivers use this scenario; some other long systems do too. |
| Spring and receiving basin | A wall-backed one-cell mouth feeds the course. It ends in an actual basin or a pinched wall-backed drain. |
| Volcanic flow | A rock-backed lava vent opens into a small melt pocket, feeding pools and runnels. Broad volcanic traverses can reach map edges. |
| Regional pools | Actual separate or linked basins are source/receiving bodies; hidden geology does not count as a player connection. |
| Flooded works | A receiving structure expands into a broad flooded or destroyed chamber, sometimes joining planned pools. |
| Fractures | Angular branches terminate in wall-backed one-cell fissure tips, larger openings, or map edges. |

Feeders start at wall-backed sources and stop at their first confluence. Non-edge channel tips taper to one cell; source backing is preserved during shoreline and structure expansion. Validation rejects an interior edge mouth, a broad cut-off labelled as a spring, a vent without a melt pocket, and an endpoint on dry bridge floor. A causeway covering a basin centre moves the basin marker to actual liquid in that same basin.

## Crossings, vaults and contents

The pass preserves connections between surviving cells of each original reversible walking/one-tile-jump component. New accessible terrain must join the dungeon; separate authored puzzles are not forced together. Existing gameplay Leaping, run-up, lava heat and other exposure mechanics remain unchanged.

Bridges follow constructed passage axes, including labyrinths and vault halls, with intact approaches on both banks. Required bridges can rebuild across breached masonry where the span serves an existing constructed route. Optional bridges must save at least eight steps on the corresponding dry route; that is an architectural score, not a requirement that every player route avoids water or jumping. Equal-priority candidate ties are randomized. Optional crossing counts vary instead of targeting exactly two. Spans and approaches are reserved against rubble; ordinary encounters remain possible. Floor-feature placement keeps the bridge's encoded underlay intact. Final dungeon connectivity still runs.

Bridges now use feature IDs 88–97: horizontal/vertical pairs over water, chasm, lava, poison and ice. Timber decks cross water/chasms; stone decks cross lava/poison/ice. Production SDL rendering keeps the underlying material visible at the deck edges and uses its existing animation; ASCII uses `=` and `|`. Decks use dry traversal, scent and contact rules, support actors and dropped items, and retain their encoded material/orientation in the existing feature-byte save data. Save compatibility advances to 0.9.8.8 so an older executable cannot misread a bridge. All variants share one contextual bridge lesson.

Other systems' liquid cores, source caps and bridge decks are protected from accidental repainting. Final rescue routing excludes lava/poison just as it excludes chasms: treating those squares as an excavatable dry route could repeatedly find the same unusable connection. Reserved source backing cannot be removed by rescue tunnels.

Vault metadata records exact transformed non-space cells, template ID, placement instance, policy and original glyph. It resets per attempt and is not saved. Ordinary rooms and vault **types below 8, including quest type 7**, can be flooded or breached by default. Types **8, 9 and 10** remain immutable; these are vault categories, not template serial IDs.

| Theme hint | Behavior | Tagged templates |
| --- | --- | --- |
| Default | Flooding, breaches and occasional repair incidents according to material. | All ordinary constructed rooms and vault types below 8. |
| `TERRAIN_FLOOD` | Existing flood flag now marks a ruin prone to deeper erosion. | 46, 54, 132 Collapsed Keep, 210 Cave in, 324 Collapsed Chamber. |
| `TERRAIN_REPAIRED` | Bias toward a surviving causeway with paired masonry footings. | 331 Fort, 461 Duruin Bastion, 462 Orc Armory. |
| `TERRAIN_CROSSING` | Legacy accepted tag; crossing is now part of the default policy. | Existing tags remain valid. |

The structure pass chooses a coherent incident per exposed placement: water-facing flooding, a breached wall with rubble shoulders, or a maintained causeway. Ruins allow deeper erosion; lava favors destructive breaches. Chasms collapse adjacent masonry while keeping their planned floor width, so every room intersection does not turn into a wide canyon. Generated constructed rooms participate; a whole labyrinth partition is not treated as one room. Rubble replaces unoccupied old wall cells, never a surviving floor route or actor.

Stairs, forges, glyphs, permanent boundaries, protected Morgoth geometry, structural chasm areas, artefacts, notes and unique monsters remain protected individually. Liquids avoid incompatible elemental cave cores while allowing excavation outside their actual footprint. Fractures may cross elemental geography.

Flooded ordinary monsters and loot prefer dry empty destinations in the same placement and partition. When necessary, contents of a flooded structure can evacuate onto unowned floor within twelve Manhattan steps in that partition. Existing accessible-component membership is preserved where the original cell was walkable. Identity, stacks, held objects and actor state are preserved. A proposal is rejected atomically if relocation fails.

## Editable themes

`lib/edit/dungeon-themes.txt` uses independent schema `V:3`, with one D/M/N record per depth 1–20:

```text
D:depth:name:water:chasm:lava:poison:ice:chance:min_length:max_length:max_width:pool_chance
M:depth:chance:min_span_percent:max_span_percent:min_width:max_width:lake_chance:branch_chance
N:depth:chain:tributaries:chamber:pools:river:min_basins:max_basins:min_radius:max_radius:one_tile_percent
H:depth:ancient:disaster:overflow:second_system_chance
```

D controls material weights and local accents. M controls network chance, long-system reach and optional details. N controls family weights, basin geometry and one-tile runs. Optional H records control history weights and a second system's chance. Shipped history weights are ancient 45, disaster 30, overflow 25, with a 35% second-system roll when another material has positive D weight. Missing H records use those defaults. Rusty metallic depths favor poison; deeper profiles increasingly favor lava and pools. Shipped M profiles attempt a main terrain scenario on every ordinary level. The initial chance is not rerolled after construction; zero still disables it.

Shipped ordinary links use widths 1–2 with a 70% literal one-tile run roll. Wider runs use at least two cells up to M.max_width; maximum 1 keeps all links narrow. Corners, junctions and basin shores make measured cell-width shares differ from run-selection probability. Broad rivers use 2–4 cells independently; chasm fissures use a one-cell spine.

N basin counts apply to chains and regional pools. Chambers always have one basin; tributaries use up to two within the configured maximum. Broad rivers have one, plus an M.lake_chance roll for a second within that maximum. Basin radii stay within N limits. M.branch_chance controls optional feeders for chains, chambers and broad rivers; tributaries and fractures have family-specific branching.

V1/V2 files remain readable with default network/history profiles. V3 requires complete D/M/N records and accepts optional H records. Malformed input falls back atomically to built-in defaults. Templates reload at startup. Incremental builds require staging the executable, terrain/limits/theme templates and bridge tutorial catalogue. Existing feature IDs stay stable; bridge IDs are appended.

## Validation and visual review

The parser suite checks strict bounds, legacy behavior, shipped/default parity and atomic fallback. Vault metadata, generation access, content preservation, existing water/lava/poison/ice behavior and 600 local-accent cases remain covered.

The full pipeline harness calls real `generate_cave` with isolated writable paths. `--fixtures` forces families on synthetic cave/room layouts. Independent morphology checks measure distance-to-shore basin cores, unequal basin sizes, sustained narrow links, actual junctions, family-specific extent, and final terrain/bridge integrity. A wide ribbon is explicitly rejected by the network shape self-test.

`--tiles` produces full-map, room-scale, endpoint and structure previews using production `map_info()` and layered SDL tile drawing, with revealed static diagnostic lighting. Synthetic layouts are labeled separately from full generated levels. Independent checks inspect actual liquid at terminals, wall backing, broad basin cores, relocated contents, rubble, repairs and immutable vaults. These previews support renderer and visual design review, not manual gameplay or balance claims.

```powershell
.\build-incremental.ps1
python scripts/check_dungeon_terrain_pipeline.py --fixtures --tiles
python scripts/check_terrain_landmarks.py --input scripts/output/dungeon-terrain-pipeline/fixtures
python scripts/check_dungeon_terrain_pipeline.py --depths 1 2 3 7 10 13 16 19 20 --tiles --output scripts/output/dungeon-terrain-pipeline/history-normal
python scripts/check_terrain_landmarks.py --input scripts/output/dungeon-terrain-pipeline/history-normal
python scripts/check_dungeon_terrain_pipeline.py --depths 10 --seeds 1301 --history overflow --systems 2 --tiles --output scripts/output/dungeon-terrain-pipeline/history-overflow
python scripts/check_bridges.py
```

The current gallery is `scripts/output/dungeon-terrain-pipeline/history-normal/landmark-gallery/index.html`. The rejected broad-channel baseline is archived separately under `baseline-before-networks`; the previous post-layout gallery remains in the parent output directory.

Before history integration, the fixed sample used depths 1, 2, 3, 7, 10, 13, 16, 19 and 20 with seeds 1301, 7307 and 9931. All 27 levels completed with networks, but that post-layout pass was still too prone to widening structures. Its gallery and metrics are retained as the previous baseline, rather than evidence for the new historical ordering.

The history harness exports each system's initial geology, epoch, final material mask, basins, channels, terminals, caps, structures and typed bridges separately. Wrappers around each realization verify actor/object identity, critical content and accessibility. Forced ancient/disaster/overflow cases and a forced two-material setting exercise the phase ordering; positive overflow expansion is measured against the original hydraulic mask. An unchanged system is not counted as an observed flood.

The final history sample completed all 27 maps: 27 historical systems across 24 ordinary maps (twelve ancient, five disaster, ten overflow), plus three Morgoth-depth maps using the existing protected layout path. Three ordinary maps contain two systems. All six shape families occur. The sample contains 58 bridge spans/315 actual deck tiles, 35 flooded and 39 breached placements, 46 rubble cells and 1,117 newly flooded cells. Maximum generation attempts were four per level and nine per system realization; measured generation time was 192–1,567 ms, excluding rendering. No repaired-placement roll occurred in this random sample; controlled structure fixtures cover those incidents.

All 672 forced family/material/layout cases and 192 elemental compatibility cases passed, along with independent fixture and 27 normal-map morphology/endpoint audits. Standalone structure tests cover default mutable rooms, ruined quest type 7, immutable types 8/9/10, repairs, rollback and the chasm wall-collapse regression. Forced two-system ancient/disaster/overflow cases pass; the overflow example adds 156 water cells while retaining separate basin cores, thin reaches and three confluences. Ten bridge material/orientation variants pass dry-contact, movement, object, SDL pixel and real feature-RLE save tests. The 115 parser cases, 600 local-accent cases, water/poison/lava/ice behavior, access, vault-depth eligibility, authored lava-vault checks and tutorial integration/catalogue checks pass. Shared runtime fixtures must run serially where they reuse output paths. The executable, templates and tutorial catalogue are staged to the standard deployment with matching SHA256 hashes. These are engine checks and production SDL previews; gameplay balance remains a playtesting question.

Useful ordinary-level preview crops under `scripts/output/dungeon-terrain-pipeline/history-normal/`:

- `depth-7-seed-7307-system-0-room-tiles.png`: old water with constructed timber crossings.
- `depth-2-seed-1301-system-0-structure-tiles.png`: overflow reaches constructed rooms.
- `depth-7-seed-9931-system-0-structure-tiles.png`: a later disaster breaches rooms and leaves rubble.
- `depth-16-seed-1301-system-0-room-tiles.png`: stone bridges over lava.

# Terrain generation plan

Analysis of the current working tree on branch `0.9.8`, 12 September 2026. This is a design and implementation plan; generation code has not been changed. The scope is procedural chasm terrain and water, lava, and poison rivers. Existing chasm partitions remain a separate layout system.

The recommended approach is to plan complete terrain features inside suitable geography, including their endpoints, banks, and crossings, and validate them before changing the map. A shared placement system should support different shapes and rules for each material.

**1. What the generator currently does**

The live sequence is:

```text
Choose map size, quest reservations, and prefab anchors
    -> Select partition modes, elemental types, styles, and density
    -> Build caves, constructed rooms, labyrinths, vaults, and chasm partitions
    -> Repair room boundaries and connect rooms/partition hubs
    -> Place most stairs and quartz streamers
    -> Generate legacy terrain chasms, unless a chasm partition exists
    -> Randomize and clean up doors
    -> Generate water/ice, then lava, then poison
    -> Place rubble and the player
    -> Check connectivity and carve rescue tunnels if necessary
    -> Place decorative fixtures and ordinary objects/monsters
    -> Finish quest/special placement and final level processing
```

Vaults and some special population are already present before the liquid passes. A pass cannot assume that monsters, objects, or authored features are absent merely because ordinary population happens later.

The principal integration point is [level-generation.c:591](C:/dev/Sil-More/src/level-generation/level-generation.c:591), with liquids at line 655 and final connectivity at line 678. Stair placement and the earlier chasm call are in [terrain-connectivity.c:926](C:/dev/Sil-More/src/level-generation/level-generation-terrain-connectivity.c:926).

| System | Verified behavior | Limitation relevant to this work |
| --- | --- | --- |
| Terrain chasms | Random cardinal walk, length 4d8; placement retries until accepted. Runs at depths 3–19 when its depth-based roll succeeds, with up to 12 requested chasms. | No endpoint, biome, or architectural plan. Acceptance needs only one affected floor square. No explicit protection for stairs, objects, fixtures, or natural versus constructed origin. |
| Water | Compact lakes, streams inside caves, and connecting rivers in ordinary CAVEY areas. Rivers can excavate selected rock and tunnel cells. | Connecting channels are narrow BFS paths between sampled cave cells. Protection rules differ between natural-floor and excavation paths. A connecting river copies its starting style along its route. |
| Ice | Frozen rivers and pools in ice big caves, with partition and anchor protection. | Must remain supported when shared water/river helpers change. |
| Lava | Directed, meandering rivers and pools in fire big caves. Protects special cells and existing dry connectivity. | Rivers start inside a cavern without a planned outlet. The local connectivity test restricts broader shapes, even where a distant bypass could work. |
| Poison | Two or three small seep/pool attempts in poison big caves. | No procedural poison river yet. |

Sources: [terrain chasms](C:/dev/Sil-More/src/level-generation/level-generation-terrain-features.c:150), [water eligibility](C:/dev/Sil-More/src/level-generation/level-generation-water.c:12), [water routing](C:/dev/Sil-More/src/level-generation/level-generation-water.c:118), [lava routing](C:/dev/Sil-More/src/level-generation/level-generation-lava.c:97), [poison generation](C:/dev/Sil-More/src/level-generation/level-generation-lava.c:204).

The final connectivity check is weaker than the desired terrain contract. Its broad flood treats chasms and rubble as traversable; its stricter descent check ordinarily requires access to some down stair, with special-case exemptions. Lava and poison are excluded from both floods. It does not prove that all important destinations remain accessible without crossing a new chasm. Its permissive vault-interior handling also makes it unsuitable as an exact ordinary-walking validator. See [check_connectivity](C:/dev/Sil-More/src/level-generation/level-generation-terrain-connectivity.c:281) and [player_passable](C:/dev/Sil-More/src/level-generation/level-generation-access.c:6).

**2. Interpret depth and biome correctly**

Current generation separates partition shape, elemental subtype, and visual style. Labyrinths start at depth 7, elemental big caves at depth 10, and chasm partitions at depth 14. The latter is independent of terrain chasms beginning at depth 3. Special partition counts are capped by depth. See [mode selection](C:/dev/Sil-More/src/level-generation/level-generation-state.c:724) and [depth constants](C:/dev/Sil-More/src/level-generation/level-generation-internal.h:40).

[style-levels.txt](C:/dev/Sil-More/lib/edit/style-levels.txt:5) has depth-dependent visual pools, partition style rules, and elemental type weights. Its current `B:1:20: ICE:1 FIRE:1 POIS:1` gives equal conditional weights to elemental types wherever big caves are eligible. Thus a deeper red palette does not itself mean that the region is a fire biome.

Use the actual partition subtype as the primary material rule. Add explicit terrain profiles selected by depth and region role; never infer gameplay material from a wall color or style number. Keep appearance in the existing style system. If elemental frequency should shift with depth, express that through the existing `B:` rules rather than a second competing elemental lottery.

Suggested direction for ordinary regions, to tune with generated examples:

| Depth / feet | Terrain emphasis |
| --- | --- |
| 1–4 / 50–200 | Mostly dry rooms; short cave streams and small pools. Small terrain fractures only from the existing depth-3 threshold. |
| 5–7 / 250–350 | More water in natural caves, linked pools, winding channels, occasional dry islands. The green/moss band supports this direction visually. |
| 8–10 / 400–500 | More fractures and streams disappearing into rock. Preserve labyrinth geometry. At depth 10, elemental caves use their actual subtype. |
| 11–14 / 550–700 | Longer cave rivers and more pronounced ravines; complete lava/poison/ice compositions inside their respective elemental regions. |
| 15–17 / 750–850 | Larger eligible features and more tactical crossings. A proposed fire-weight increase belongs in the existing elemental table; ordinary caves still retain water and fractures. |
| 18–19 / 900–950 | Rare broad fissures or strong river landmarks, with guaranteed routes through the surrounding dungeon. Retain quiet regions between hazards. |
| 20 / 1,000 | Preserve throne geometry, seals, forced entrances, and escape requirements. Apply ordinary terrain rules only outside protected regions and after dedicated regression checks. |

These are proposed design biases, not measured balance or a claim that the current game already uses these terrain profiles. Do not introduce early elemental rivers merely because a shallow style looks green or red.

**3. Give every cell a reliable generation role**

Build a temporary context map using existing `cave_natural`, room/anchor metadata, partition identity, vault tags, and actual connection locations. Record missing provenance when geometry is carved. `CAVE_ROOM` alone cannot distinguish a natural cavern from a constructed chamber, and a room bounding rectangle cannot safely stand in for its exact footprint.

The context needs to distinguish natural floor, natural rock, constructed floor/walls, ordinary corridors, natural cave necks, door approaches, structural crossings, and protected authored content. Record ownership when an overlapping generation pass changes a cell so old natural tags cannot authorize cutting new masonry.

Rules should protect actual features and access roles:

- Never overwrite stairs/shafts, forges, doors or secret doors, traps, glyphs, fixtures, occupied cells, quest/greater-vault footprints, permanent boundaries, or Morgoth's sealed geometry.
- Preserve constructed room envelopes and functional entrances. A river must not cut a fresh entrance beside a door or run along a one-cell access corridor by accident.
- Leave existing chasm-partition platforms, bridges, and footprint tags intact. New standalone fractures do not receive `CAVE_CHASM_AREA`, which has partition-specific meaning.
- Permit bounded excavation through explicitly natural separators between suitable cave spaces. Unknown or constructed walls are forbidden, even when their terrain ID matches ordinary rock.
- Initially preserve normal built rooms and labyrinths. A later flooded ruin or broken hall requires an explicit eligible room role or template; RUINED mode alone is insufficient permission.
- Cross a partition boundary only through a validated continuation into compatible natural geography. End at a plausible cave wall or basin if a continuation is unavailable. Preserve each region's local style.

Use small, purposeful clearances for entrances and landings. Avoid expanding arbitrary exclusion circles until most of a cavern becomes unusable.

**4. Compose regions instead of independently scattering every feature**

Select one main terrain composition, or a dry result, per suitable region. A useful initial budget is zero or one dominant feature and zero to two supporting features, scaled by eligible floor area and shape. Count authored hazards and chasm footprints when assessing how crowded the surrounding level already is. These are starting parameters, not compulsory quotas.

| Material | Shape vocabulary | Tactical purpose |
| --- | --- | --- |
| Water | Spring-to-pool stream, river joining two cave chambers, pool with an outlet, occasional branch around a dry island. | A quicker wet crossing versus a longer dry bank, altered pursuit and scent, alternate approaches around the water. |
| Lava | Fissure-fed channel widening into a molten basin, with occasional dry rock peninsulas. | Dangerous edges and displacement, a deliberate crossing, or a longer route around the basin. |
| Poison | Seep-fed winding river with shallow-looking side pools; a broad contaminated basin only in a sufficiently large poison cavern. | Exposure avoidance and meaningful bank choice, without forcing prolonged contact on the main route. |
| Chasm terrain | Tapered fracture with directional persistence and angular bends; occasionally a wider chamber ravine. | A visible separation, bridge or land neck, optional leap shortcut, and alternate flanking route. |
| Ice | Frozen versions of the appropriate river/pool shapes. | Preserve the existing ice terrain behavior while keeping geometric variety. |

Water bends smoothly; fractures have more angular changes; poison has more side pools. Sharing routing code should not make all four materials look like recolored versions of the same channel.

Start with mostly 1–3-cell channel widths and tapered ends. Permit broader pools and occasional wider ravines only where the available chamber and routes support them. Reserve branching and islands for larger spaces. Avoid rapid tile-by-tile width noise, checkerboards, repeated tiny disconnected patches, and a bridge every few cells.

**5. Plan endpoints, route, banks, and crossings together**

Each proposal contains a material, owning region, source and destination roles, centerline, complete footprint, banks, crossing cells, and the cells whose access must be preserved.

Choose endpoints before routing: a natural wall inlet, an existing or proposed basin, a cave mouth, a fissure, or a compatible continuation. A chasm may taper into rock at both ends. A channel that simply stops at an obstacle in the middle of a floor is rejected or redesigned as a deliberate basin.

Use bounded weighted pathfinding between endpoints. Favor eligible natural space, modest excavation, sufficient bank room, and a consistent direction. Penalize unnecessary length, sharp repeated reversals, narrow access passages, and proximity to protected entrances. Forbid protected cells outright. Use coherent seeded variation in the route costs rather than unconstrained random steps.

Expand the centerline into the complete footprint, then evaluate all expanded cells against the same eligibility rules. Cardinal continuity is required for the channel shape; a diagonal touch is not a continuous river. A planned bridge may interrupt the visible liquid/chasm cells, so continuity checks must account for explicitly recorded crossings rather than treating them as accidental fragments.

Place a crossing where both banks have useful landing space, the channel is narrow enough, and the connection serves actual routes. Prefer a short transverse crossing. Larger landmarks may support a second crossing or an exterior loop, but do not require two bridges in every small stream. If a footprint needs many repairs or awkward bridges, choose another candidate.

Existing chasm bridges are floor cells with a chosen style, so initial land bridges and safe stone crossings can reuse existing terrain and style IDs. Do not copy their partition tags into standalone terrain. Overpasses, under-bridge liquid simulation, new bridge mechanics, and new save fields are unnecessary for this scope. See [existing bridge construction](C:/dev/Sil-More/src/level-generation/level-generation-layout-big-cave.c:700).

**6. Validate the complete candidate before painting**

First capture the structural baseline before new standalone terrain proposals, retaining authored hazards and existing chasm partitions: room entrances and partition links, known stairs and special destinations, plus valid entry candidates. Preserve connections within each existing safe component. Do not force previously separate components across authored vault puzzles or chasm layouts to connect; the new pass must not silently redesign them.

Use a shadow footprint for proposed changes. A failed proposal leaves feature, style, flags, natural provenance, objects, and fixtures untouched. Validate the proposal together with all features already accepted this level, so two individually reasonable rivers cannot jointly block a route.

For lava, poison, and chasm terrain, preserve previously available ordinary walking connections between surviving floor areas and relevant destinations. No new mandatory connection may require Leaping, flight, resistance, poison exposure, or falling. Check actual route entrances and reachable interaction squares, rather than assuming a room center represents all access. Maintain safe approaches to stairs, forges, quest entrances, and already placed content.

Water and ice remain traversable using their current rules. Evaluate exposure length and dry alternatives separately: a deliberate short ford can be acceptable, while converting a long compulsory corridor into water is not an interesting river. A dry route should remain an important option around major water features, without demanding a dry bypass around every water tile.

Movement validation must follow the game's legal eight-direction steps and feature rules. Do not use the channel's four-connected shape rule as a replacement for gameplay movement, and do not use the legacy permissive vault-wall flood as proof of ordinary walking access.

The current lava/poison 3×3 test can remain a fast sufficient check. Failure of that local check should defer to complete-candidate validation where a farther bypass or planned crossing may work; making it a mandatory early veto would prevent the broader features this plan is meant to allow.

Measure detours as well as reachability. Compare ordinary route distances before and after terrain; reject excessive detours relative to the room or region scale. Avoid hardcoding a single arbitrary distance limit for every map size. Reject unintended isolated landing pockets and islands. Designed islands need a usable route or an explicitly optional role, and must not receive required content.

Bound candidate count, path length, excavation, width adjustment, and total work per region. Failed candidates can be rerouted, narrowed, shortened with valid new endpoints, or skipped. Remove the unbounded chasm placement loop. Optional terrain failure must not trigger endless whole-level regeneration.

**7. Integrate without letting later passes undo the design**

Consolidate standalone terrain planning at the existing liquid-pass location, after structural tunnels, existing stair placement, streamers, and door cleanup. Remove the old standalone chasm call from `connect_rooms_stairs()` once its replacement is ready. Run baseline structural access repair before proposals, using an explicit temporary traversal root while the player has not yet been placed.

Carry temporary crossing and entrance reservations through rubble, player placement, and ordinary population. Existing fixture placement already occurs after terrain; retain that ordering. Recheck reserved paths after rubble and after any later quest-specific map mutation. Narrow generation placement predicates should keep ordinary rubble, blocking objects, or inappropriate spawns from consuming the sole crossing or required landing; do not globally ban combat encounters on bridges.

Retain the final level connectivity check as a fallback. A new feature should normally be rerouted or rolled back if it breaks access, rather than relying on rescue tunneling to carve an arbitrary replacement route. Revalidate appearance and access if any late structural repair touches its neighborhood.

Population budgets must count suitable usable floor, not just any tile that has no wall flag. This is a concrete existing mismatch: ordinary partition floor counting uses `cave_floor_bold`, while ordinary spawn location selection uses `partition_population_naked_bold`, which requires plain floor. Enlarging rivers could therefore reduce placement space without reducing the floor-based population budget. Check intended density and placement failures when changing these counts. See [floor classification](C:/dev/Sil-More/src/level-generation/level-generation-partition-population.c:562) and [population budgets](C:/dev/Sil-More/src/level-generation/level-generation-population.c:859).

Preserve existing actor/terrain eligibility; a poison-themed monster is not automatically poison-immune. Ordinary item placement should not turn isolated decorative islands into unreachable rewards. Keep water/ice placement and traversal semantics consistent with their current gameplay.

**8. Implementation sequence**

1. **Capture a baseline and add explicit context.** Export full generated maps with seed/depth, partition roles, protected cells, terrain, crossings, connectivity repairs, and timing. Add provenance and required-access metadata without changing terrain distribution. Include a reproducible snapshot immediately before terrain so old and new terrain can be compared on the same base geometry.
2. **Replace standalone chasm generation.** Implement bounded fracture candidates, endpoint/footprint checks, explicit crossings, and access validation. Initially retain the current whole-level exclusion when a chasm partition exists. Once tested, permit standalone fractures elsewhere by protecting exact partition footprints instead of globally suppressing them.
3. **Improve water and preserve ice.** Reuse the common planner for coherent endpoints, width variation, basins, and cave continuations. Unify protections across every footprint cell; preserve local styles at region boundaries. Extend the existing ice tests when shared helpers change.
4. **Add poison rivers and improve lava.** Add seep-to-basin poison channels and destination-aware lava rivers using their real elemental subtype. Introduce candidate-wide connectivity and purposeful safe crossings before widening hazardous channels.
5. **Tune depth and variety in data.** Add terrain profiles for feature weights, width/length ranges, budgets, excavation permissions, and placement roles. Prefer a dedicated generation template such as `lib/edit/terrain-generation.txt`; continue using existing `B:` rules for elemental subtype selection and existing style rules for appearance. Include parser validation, default behavior, and deployment staging. New generation-only context does not require persistent save fields.
6. **Expand selected architectural cases.** Only after cave behavior looks good, add deliberately eligible ruined chambers and rare compatible cross-region features. Keep authored vault layouts unchanged unless separately designing their terrain.

Likely code boundaries: a terrain coordinator/context module, shape/path helpers, and whole-candidate validation under `src/level-generation/`; narrow call-site changes in `level-generation.c`, `level-generation-terrain-connectivity.c`, and existing water/lava modules. Add any new C files to `CMakeLists.txt`. Update Help/tutorial descriptions if player-visible generation descriptions or crossing behavior change; preserve existing terrain mechanics and IDs.

**9. Acceptance and validation**

Automated cases should include a natural two-chamber stream, a one-cell cave neck, a constructed room beside a cave, open/closed/secret door approaches, stairs and forges next to a proposed bank, a quest vault surrounded by natural rock, mixed adjacent elemental regions, pre-existing water/ice, adjacent chasm partitions, bridge landings, two interacting hazards, and a map with no legal placement. Verify bounded termination, atomic rejection, correct metadata reset, and repeatability for a seed.

Run a stratified full-generation sample across depths 1–20, small/large maps, normal/quest/forge/Morgoth levels, arrival directions, and escape state. An initial target is 100 seeds per ordinary depth plus targeted special-level fixtures; expand only where coverage or failures warrant it. Record accepted/rejected features by reason, material and motif frequency, eligible-area coverage, excavated cells, connected footprint sizes, crossing counts, route detours, isolated floor, rescue edits, and generation-time percentiles. Compare retry rates and timing with the captured baseline.

Review actual map galleries at whole-level and room scale, covering successful features, dry selections, and awkward/high-rejection seeds. Then manually play representative cases for pursuit, alternate routes, Leaping, forced displacement, start safety, and hazard readability. Connectivity tests cannot establish that a dungeon is interesting or that banks and crossings look sensible.

Acceptance means: no protected geometry is overwritten; no new terrain removes required ordinary access; no accidental channel fragments or arbitrary open-floor endpoints remain; retries are bounded; depth/material rules are respected; the gallery contains distinct compositions and quiet areas; and ordinary population and late repairs preserve the result.

**Validation performed for this analysis:** `python scripts/check_lava_generation.py` passed its 200 seeded lava maps, 100 seeded poison maps, protected-cell/narrow-neck checks, and the dry-route checks for Gothmog's hall (402) and Flame Pits (420). These are isolated production-generator tests, not a full dungeon sampling run. `scripts/check_water.py` was inspected but not run; it has unrelated existing working-tree edits. No gameplay, balance, or visual replay was claimed. No generation implementation was changed.

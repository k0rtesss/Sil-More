# Underground terrain: research and revised design

Status: research behind the basin/network replacement. This records the in-game screenshot review and the rejected broad-channel design. Implemented families, configuration and validation are now described in [terrain-generation-plan.md](terrain-generation-plan.md). The subsequent structure/flow revision makes ordinary rooms and vault types below 8 mutable by default, adds flood/breach/repair incidents, and validates actual springs, drains, vents, receiving basins and boundary mouths. The proposals below retain their original research context; physical fluid simulation and hidden traversable conduits were not added.

The intended result is a dungeon-scale **network** of distinct underground spaces: small pools, narrow streams, occasional large basins, confluences, springs, sinks, and crossings through inhabited structures. Long reach and broad coverage must be independent choices. Simply reducing every current river's width would produce another repetitive result.

## Why the earlier generator produced the screenshot

The following observations came from the source at the time of that screenshot:

| Earlier decision | Consequence |
| --- | --- |
| `lm_endpoints()` chooses a long map-spanning course; `place_terrain_landmark()` chooses one width for it, then varies it by only -1/0/+1. | One continuous band remains the dominant shape. |
| Shipped M records use minimum widths 3 at shallow depths and 4 deeper, with maxima 6–10. The schema itself disallows a one-cell major channel. | The main system cannot have the requested one-tile connecting streams. |
| `lm_lake()` stamps one ellipse-like basin onto the midpoint or a permitted vault waypoint. | A lake is a bulge in an already broad river. Multiple distinct lakes are not a first-class structure. |
| There is at most one tributary, with width `MAX(2, lm_width - 2)`. | No genuine hierarchy of tiny feeders, junctions, connectors, and wider receiving channels. |
| `lm_banks()` adds a shore around nearly every exposed channel edge. | Excavation creates a recognizable liquid corridor with a repeated border treatment. |
| Local features are suppressed throughout partitions touched by the landmark. | The main feature removes much of the surrounding fine-scale variety. |
| Bridges prefer short existing straight spans, and optional placement stops at two. | Bridge count can pass without creating useful or visually convincing crossings of the dominant channel. |
| Shape tests require half-level span and bulk; “lake width” is whole-system area divided by its longest map extent. | A large ribbon passes. A few small lakes joined by thin streams can fail. This is the wrong aesthetic acceptance test. |

Source pointers: `src/level-generation/level-generation-landmarks.c` (`lm_endpoints`, `lm_lake`, `lm_banks`, `lm_bridge`, `lm_shape_statistics`, `place_terrain_landmark`); `src/level-generation/level-generation-terrain.c` (local suppression); `lib/edit/dungeon-themes.txt`; `scripts/check_terrain_landmarks.py` (`validate`).

## What real underground systems suggest

Water can enter through one sinking stream or many recharge points. Separate inlets can feed passages that join downstream; exits may be springs. This supports tributaries and sources/sinks within rock, rather than requiring every visible channel to run between map edges. [National Park Service: Solution Caves](https://www.nps.gov/subjects/caves/solution-caves.htm).

Present water occupies only part of a cave's history and geometry. Great Basin's cave geology describes passages associated with the water table, faults, and pools retained behind mineral dams. Mammoth Cave's guide describes newer lower streams and abandoned higher passages. For the game, this suggests wet chambers, narrow connecting streams, dry shelves and abandoned branches. It does not imply that every cave needs all of them. [NPS: Cave Geology in Depth](https://www.nps.gov/grba/planyourvisit/cave-geology-in-depth.htm), [NPS: Mammoth Cave Map & Guide](https://www.nps.gov/maca/planyourvisit/mammoth-cave-map-guide.htm).

Karst examples concern soluble rock; they do not justify dissolving granite in the same way. Sil-More can retain its granite terrain abstraction while making intrusion follow fractures, previously opened cavities, and collapse zones. This is a design approximation, not a geological simulation of the game's rock type.

Lava has a different history: confined flows develop solid margins and may roof over into tubes; when supply ends, tubes can drain. A useful game interpretation is pools linked by exposed channels, solid shelves, stretches beneath intact rock, and separate vent pools. A long exposed flow is one possible event, not the universal lava form. [USGS: What is a lava tube?](https://www.usgs.gov/media/audio/what-lava-tube), [NPS: Lava Caves/Tubes](https://www.nps.gov/subjects/caves/lava-caves-or-tubes.htm).

## Useful game and procedural-generation examples

**BrogueCE:** `designLakes()` makes independently shaped blobs in decreasing size ranges and rejects placements that break its dry connectivity rule. `buildABridge()` accepts spans based partly on how much existing travel they save. The transferable ideas are basin-first geometry and route value; Sil-More should retain its own one-tile jump rules instead of copying Brogue's dry-only requirement. [Production source: Architect.c](https://github.com/tmewett/BrogueCE/blob/master/src/brogue/Architect.c#L2381).

**Dungeon Crawl Stone Soup:** `serial/rivers.des` places 4–6 river-themed map pieces, including very small passage crossings, chamber-edge pools and irregular larger sections. This is a vocabulary of authored motifs, not evidence of simulated connected drainage. Its separate procedural `_build_river()` still sweeps a varying-width band across a level; that is closer to the current problem and is not the approach to copy. [Authored stream motifs](https://github.com/crawl/crawl/blob/master/crawl-ref/source/dat/des/serial/rivers.des#L1), [procedural river/lake source](https://github.com/crawl/crawl/blob/master/crawl-ref/source/dgn-layouts.cc#L1102).

**Minecraft:** Mojang describes cave shapes and aquifers separately: broad caverns, winding tunnels and very thin tunnels coexist with local liquid levels, including deeper lava aquifers. The transferable distinction is between the underground cavity and the amount of it filled with liquid. This is a published design description, not an audit of Minecraft's implementation. [Mojang: snapshot 21w37a, Noise caves and Aquifers](https://www.minecraft.net/en-us/article/minecraft-snapshot-21w37a).

**KarstNSim:** the research team's method builds paths between inlets and outlets on a graph, with direction-dependent costs informed by geology, fractures and permeability. This supports a lightweight graph plus a geological preference field for Sil-More. It is not necessary to simulate fluid dynamics or port a 3D geological package. [RING team: method description](https://www.ring-team.org/30-news/seminar/502-karstnsim-a-public-code-for-3d-geologically-driven-simulation-of-karst-networks), [public implementation](https://github.com/ring-team/KarstNSim_Public).

## Proposed shape families

These numbers are starting design ranges to evaluate visually, not measured properties of real caves or approved balance constants. Material weights and shape-family weights should be separate, configurable per depth/theme.

| Family | Distinctive geometry | Extent |
| --- | --- | --- |
| Pool chain | 2–5 differently sized basins connected mostly by one-tile streams, with occasional two-tile reaches. | Commonly spans multiple partitions despite a modest wet footprint. |
| Tributary network | 2–4 narrow feeders join one receiving stream; one or two basins occur along it. | Potentially level-scale; width increases locally after joins. |
| Flooded chamber | One large irregular basin occupying a natural chamber or permitted ruin; small inlet/outlet. | Compact or regional; does not need to span half the level. |
| Perched/isolated pools | Several separate pools, some joined by a small surface trickle, others fed through cracks. | Scattered within a coherent geological region. |
| Major underground river | A substantial channel, narrow throats and occasional wide reaches, with consequential architectural crossings. | Dramatic, occasional. It must not dominate nearly every wet level. |
| Lava field | Small vents, unequal melt pools, short runnels, one larger receiving pool, solid shelves or interrupted visible conduit. | Clustered or linked across a region; broad continuous lava is a distinct rare case. |
| Fracture network | Mostly narrow angular fissures with branches and a few widened pits or collapsed chambers. | Can cross half a level without making half of its course a broad canyon. |

For ordinary water networks, begin with roughly 60–75% of **channel length outside basins** one tile wide, most remaining reaches two tiles, and a few three-tile throats or receiving sections. Give basin interiors several times the local connector width. Tune these distributions separately for broad rivers, lava and fractures; do not treat this as a global width clamp. Keep cross-partition networks common. Reserve the current large-river appearance for an occasional family, initially around 10–15% of water-bearing layouts.

Poison uses hydrological shapes with a theme-appropriate source: seep pools, contaminated cisterns, drainage from rusty works. It need not be a geometrically identical recoloring of lava. Keep existing poison exposure, lava damage, water movement and jump mechanics unchanged.

## Generation sequence

1. **Choose a family and an influence region.** Region size controls reach, separately from wet area. Some systems connect several partitions; others are local chamber events. Preserve the existing authored-vault permissions and critical content protections.
2. **Place sources, basins, junctions and outlets.** Favor existing cave chambers for basins but allow newly excavated cavities in granite. Place some sources/sinks at wall fissures. Not every basin must have a visible outlet: it can be perched or feed an inaccessible lower conduit.
3. **Connect the nodes into a coherent network.** Prefer fractures and intended geological directions, and selectively join existing wet nodes. Use a temporary drainage ordering to avoid arbitrary same-level uphill connections. The graph may have split/rejoin routes around rock islands, but two water paths meeting at the same level form one confluence. They cannot cross independently like roads.
4. **Create basin floors and spill points separately from connecting channels.** Use low-resolution local floor relief and a chosen fill level, clipped by real boundaries. Basins can be lobed, shallow-ended or interrupted by a substantial rock island. Narrow sills between basins remain narrow. Do not smooth the whole liquid mask until narrow links disappear.
5. **Excavate the cavity around the water selectively.** Some reaches have dry ledges on one or both sides; others fill a narrow fissure. Broader dry chambers may surround a pool. Avoid a constant one-tile floor outline around the entire network.
6. **Reconcile the inhabitants' construction.** Read where useful existing routes intersect the network. An intact hall may carry an aligned bridge or channelled crossing; a ruined chamber may have a coherent breach and flooding. Preserve arch approaches or piers where appropriate. Reject or reroute a basin that would leave many isolated protected-cell islands.
7. **Validate and commit transactionally.** Reuse the existing access graph, vault ownership, relocation and rollback work. Permit ordinary one-tile jump opportunities. Any hidden liquid connection is geological metadata only and must never count as a traversable player route.

A future full generation-order redesign could place geology before rooms so builders naturally straddle it. That is not required for the first revision: the existing post-layout stage has useful ownership and route information, and can produce a believable historical overlay if its basin/network model changes.

## Crossings should describe something

There are two distinct types of “cross” to generate deliberately. Water meeting water should merge or split around an island and rejoin; a passage meeting water should present a readable crossing. In a two-dimensional tile map, roofed conduits must be explicitly represented as inaccessible geology beneath a dry surface, rather than pretending two visible liquid paths can cross without mixing.

Bridge selection should consider the rooms/routes served and the travel saved, with sound approaches and recognizable architecture. One short bridge near a river tip must not count as proof that the long dividing channel has a useful crossing. Conversely, long detours and dangerous crossings can be appropriate dungeon features; route measurements are diagnostic and part of placement scoring, not a universal maximum-detour rule. Narrow cracks/streams should create real one-tile crossings without constructing a bridge at each one.

For incompatible materials, explicitly choose avoidance, a solid separating sill, or an authored interaction. Do not overwrite one liquid with another at a junction and call the result realistic. Dynamic fluid mixing or new chemistry mechanics are outside this generation revision.

## Replace the aesthetic acceptance tests

Keep bounded retries, critical reachability, vault policy, identity-preserving relocation and final generation checks. Change the shape evidence:

- Measure reach and wet area independently, by selected family. Do not demand that isolated pools or lakes span half a map.
- Measure local channel width outside basin interiors. Whole-area/longest-extent is not a stream-width metric.
- Detect distinct broad basin cores separated by narrow necks, even when all water belongs to one connected component. A useful independent check is distance-to-shore plus neck segmentation; count visible basins, not just requested basin nodes.
- Check that pool-chain samples retain multiple unequal basins and sustained one-tile links after rasterization and smoothing.
- Check actual junctions, source/sink termination and selected corridor crossings. Measure bridge route benefit and location, not only bridge count.
- Track introduced passage connections, wet/rock excavation ratio, dry ledge variation, and unchanged room/vault structure. Flag repeated uniform bands and many isolated protected-cell islands for review.
- Review the same fixed seeds in full-level and room-scale **in-game tile renders**. Pair this with unbiased samples across depth bands and forced family fixtures. The schematic alone is insufficient: the rejected screenshot showed a problem that the previous colored gallery understated.

The next implementation should first produce inspectable production-map prototypes of pool chains, tributaries and lava pools, then integrate the selected geometry into generation. Success is the requested variety and readable crossings at play scale, with occasional dramatic geography retained.

# Ordinary cave floor palettes

Ordinary cellular-automata caves keep their main wall/floor style from `P:CA`
in `lib/edit/style-levels.txt`. Small connected patches introduce secondary
floor materials from both the original hand-drawn atlas and Verdant tiles.

## Configuration

```text
M:<base style>:<maximum floor percentage>:<patch count>: <accent style>:<weight> ...
M:52:24:3: 44:4 54:3 13:2
```

The example keeps at least 76% of eligible floor in the cave's original style
52. Up to three patches use at most two distinct accent artworks, selected
from the weighted list. The target is a cap: tiny or disconnected caves may
receive fewer patches. Each patch has at least four cardinally connected
cells. Original styles sharing the same selected floor artwork do not create
invisible or duplicate material changes.

The shipped rules cap accents at 20-24%. Original hand-drawn bases 5, 7, 9,
10, 14, 27, 37, 52 and 54 remain dominant and can receive original and Verdant
accents. Verdant bases 42 and 43 receive original hand-drawn accents. All
artwork comes from the existing atlas; style IDs and source pixels are unchanged.

The parser accepts base/accent IDs 0-63, coverage 1-40%, one to four patches,
and one to eight weighted entries with weights 1-1000. A missing rule disables
patches for that base. The last valid rule replaces an earlier one; invalid
input is rejected without partially replacing a palette. `V:` clears palette
configuration on reparse. Loaded style references are validated before use.

## Generation and compatibility

The patch pass runs after an ordinary cave has been carved and its quartz
veins placed. It changes only `cave_color`, using a private random stream
derived from the generation state and cave position. It consumes no gameplay
RNG, so palette tuning does not alter subsequent layout, loot or monsters.
Growth uses a bounded sample of the frontier and prefers cells touching more
of the current patch, producing compact connected areas rather than speckles.

Only natural `FEAT_FLOOR` room cells still carrying the exact base style are
eligible. Walls, doors, hazards, bridges, stairs, vaults, first-variant halos,
chasm partitions, the Morgoth tunnel, items, monsters and fixtures are protected.
Ordinary CA fallbacks also use these rules. Elemental big-cave floor treatments,
authored rooms/vaults and crystal chasm partition materials retain their own
generation paths.

The existing save format already stores per-cell `cave_color`. There is no new
persistent field or version change. Existing levels keep their stored artwork;
newly generated caves use the palette configuration.

## Checks

Build with `build-incremental.ps1`, then run:

```powershell
python scripts/check_cave_floor_palette_rules.py
python scripts/check_cave_floor_patches.py
```

The parser suite covers bounds, malformed input, atomic replacement, load
order and reparse. The generation suite compares palette-enabled and disabled
production cave carving, verifies coverage/connectivity and protected state,
and renders original/Verdant before-and-after scenes with the production SDL
renderer.

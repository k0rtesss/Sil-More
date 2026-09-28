#!/usr/bin/env python3
"""Import Verdant 01 banks, Verdant 03 still water and Verdant 04 currents.

The Tidewater pack contains two different kinds of water animation.  Its
``shoal`` and ``surf`` families are ping-pong surface animations intended for
coasts, while ``river_on_silt`` and the ``anim_river_*`` fills are forward
moving currents.  Caves use the latter for directional water, while Verdant
03's ``anim_water_surface`` frames provide the moving calm-lake surface.  The generated atlas keeps
bank/depth geometry fixed while only the water pixels move.

The generated freshwater and acid atlases have the same layout.  They are 16
pages high (four calm frames followed by four directions with three current
frames each), each page containing the normal 16x16-mask atlas (256x256
pixels):

    page = frame                                  (direction 0, calm)
    page = 4 + (direction - 1) * 3 + frame       (direction 1..4)
    source rectangle = (mask % 16, mask // 16 + page * 16) * 16

Direction 0 is a calm lake with four Verdant 03 frames.  Directions 1 and 4
use vertically/horizontally flipped copies of the native south/east river
animation, so their current travels north/west while the bank remains in the
same orientation.  Deep water is the same current art with a dark freshwater
palette; it does not use the Tidewater sea fill.

Requires Pillow and the local Verdant 01 and Verdant 04 packs.  The Verdant 03
frames are read from the already-staged graphics directory.  By default the
importer also refreshes the existing atlas row used by the bank-ring floor
graphics.  Pass ``--water-only`` when another import owns that atlas.
"""

import argparse
import json
import shutil
from pathlib import Path

from PIL import Image

from import_verdant_worlds_floors import normalize_mask

ROOT = Path(__file__).resolve().parents[1]
GRAF = ROOT / "lib/xtra/graf"
TILE_SIZE = 16
MASK_ATLAS_SIZE = 16 * TILE_SIZE
STILL_FRAME_COUNT = 4
CURRENT_FRAME_COUNT = 3
DIRECTION_COUNT = 5
PAGE_COUNT = STILL_FRAME_COUNT + (DIRECTION_COUNT - 1) * CURRENT_FRAME_COUNT

# Verdant 01, Verdant 03 and Verdant 04 freshwater art share this exact blue palette.
WATER = ((22, 58, 92), (31, 86, 128), (45, 122, 168),
         (85, 168, 204), (168, 220, 234), (203, 238, 245))
# Keep the movement pattern but give deep water a visibly lower value.  These
# are deliberately derived from freshwater pixels instead of copying the
# Tidewater sea texture, whose stippled surface reads as open ocean.
DEEP_WATER = {
    (22, 58, 92): (8, 28, 48),
    (31, 86, 128): (14, 46, 75),
    (45, 122, 168): (22, 70, 104),
    (85, 168, 204): (45, 108, 140),
    (168, 220, 234): (96, 164, 187),
    (203, 238, 245): (142, 196, 211),
}
ACID = ((24, 58, 18), (39, 98, 24), (75, 154, 32),
        (138, 204, 52), (208, 240, 105), (235, 255, 168))


def metadata(pack):
    return {tile["id"]: tile
            for tile in json.loads((pack / "tiles.json").read_text(
                encoding="utf-8"))["tiles"]}


def tile_image(pack, tiles, name):
    tile = Image.open(pack / tiles[name]["file"]).convert("RGBA")
    if tile.size != (TILE_SIZE, TILE_SIZE):
        raise ValueError(f"Expected a 16x16 source: {name}")
    return tile


def autotile_images(pack, tiles, family, frame_count=CURRENT_FRAME_COUNT):
    """Return every authored 47-mask/frame tile in an autotile family."""
    result = {}
    for tile in tiles.values():
        if tile.get("autotileSet") != family:
            continue
        key = (tile["mask"], tile.get("animFrame", 0))
        result[key] = tile_image(pack, tiles, tile["id"])
    expected = {(normalize_mask(mask), frame)
                for mask in range(256) for frame in range(frame_count)}
    if set(result) != expected:
        raise ValueError(
            f"Expected 47 masks and {frame_count} frames for {family}; "
            f"got {len(result)} entries")
    return result


def colors(tiles):
    """Return RGB colors used by a group of opaque source tiles."""
    return {pixel[:3] for tile in tiles.values() for pixel in tile.getdata()}


def replace_water_pixels(template, surface, water_colors=WATER, mapping=None):
    """Copy ``surface`` into water pixels while retaining bank geometry."""
    output = template.copy()
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            pixel = template.getpixel((x, y))
            if pixel[:3] not in water_colors:
                continue
            replacement = surface.getpixel((x, y))
            if mapping is not None:
                rgb = mapping.get(replacement[:3])
                if rgb is None:
                    raise ValueError(
                        f"Unexpected freshwater pixel {replacement[:3]}")
                replacement = (*rgb, replacement[3])
            output.putpixel((x, y), replacement)
    return output


def transparent_water(template, water_colors=WATER):
    """Keep only a transition's bank pixels, making water transparent."""
    output = template.copy()
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            if template.getpixel((x, y))[:3] in water_colors:
                output.putpixel((x, y), (0, 0, 0, 0))
    return output


def page_position(raw_mask, page):
    return ((raw_mask % 16) * TILE_SIZE,
            (raw_mask // 16 + page * 16) * TILE_SIZE)


def write_atlas(name, pages):
    atlas = Image.new("RGBA", (MASK_ATLAS_SIZE, MASK_ATLAS_SIZE * PAGE_COUNT),
                      (0, 0, 0, 0))
    for raw_mask in range(256):
        for page, page_tiles in enumerate(pages):
            tile = page_tiles[raw_mask]
            atlas.paste(tile, page_position(raw_mask, page))
    path = GRAF / f"transition_{name}.png"
    atlas.save(path)
    return path


def staged_water_frames():
    """Read the Verdant 03 frames staged in the shared graphics directory."""
    frames = []
    for frame in range(STILL_FRAME_COUNT):
        path = GRAF / f"anim_water_surface_f{frame}.png"
        image = Image.open(path).convert("RGBA")
        if image.size != (TILE_SIZE, TILE_SIZE):
            raise ValueError(f"Expected a 16x16 source: {path}")
        frames.append(image)
    return frames


def directional_surfaces(overworld, land, tide, tide_tiles):
    """Build calm/N/E/S/W freshwater surface source tiles."""
    calm = staged_water_frames()
    ns = [tile_image(tide, tide_tiles, f"anim_river_ns_f{frame}")
          for frame in range(CURRENT_FRAME_COUNT)]
    ew = [tile_image(tide, tide_tiles, f"anim_river_ew_f{frame}")
          for frame in range(CURRENT_FRAME_COUNT)]
    return (
        calm,
        [tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM) for tile in ns],
        ew,
        ns,
        [tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT) for tile in ew],
    )


def import_freshwater(overworld, land, tide, tide_tiles):
    """Write shallow/deep base, depth seam and bank-overlay atlases."""
    shores = autotile_images(tide, tide_tiles, "river_on_silt")
    depth_edges = autotile_images(tide, tide_tiles, "shoal_on_sea")
    surfaces = directional_surfaces(overworld, land, tide, tide_tiles)
    shallow_pages = []
    deep_pages = []
    bank_pages = []

    for frames in surfaces:
        for surface in frames:
            # Frame zero is a fixed geometry template.  The native river set
            # animates its foam pixels too; using its frame 0 as the template
            # avoids making bank/depth topology depend on animation phase.
            shallow = {}
            deep = {}
            bank = {}
            for raw_mask in range(256):
                mask = normalize_mask(raw_mask)
                template = shores[mask, 0]
                shallow[raw_mask] = replace_water_pixels(template, surface)
                deep[raw_mask] = replace_water_pixels(
                    template, surface, mapping=DEEP_WATER)
                bank[raw_mask] = transparent_water(template)
            shallow_pages.append(shallow)
            deep_pages.append(deep)
            bank_pages.append(bank)

    # The depth atlas is a water-on-water edge.  The native shoal-on-sea
    # geometry is retained, but its turquoise/dark-ocean pixels are replaced
    # by the same directional shallow/deep freshwater surfaces.  It has no
    # bank pixels; bank_pages is composited afterwards by the renderer.
    depth_pages = []
    source_shoal = {
        (58, 155, 164), (95, 194, 189),
        (146, 224, 212), (35, 111, 128),
    }
    source_colors = colors(depth_edges)
    source_sea = source_colors - source_shoal
    if source_colors != source_shoal | source_sea:
        raise ValueError("Unexpected shoal-on-sea source palette")
    for frames in surfaces:
        for surface in frames:
            deep = replace_water_pixels(surface, surface, mapping=DEEP_WATER)
            page = {}
            for raw_mask in range(256):
                template = depth_edges[normalize_mask(raw_mask), 0]
                output = template.copy()
                for y in range(TILE_SIZE):
                    for x in range(TILE_SIZE):
                        pixel = template.getpixel((x, y))[:3]
                        if pixel in source_shoal:
                            output.putpixel((x, y), surface.getpixel((x, y)))
                        elif pixel in source_sea:
                            output.putpixel((x, y), deep.getpixel((x, y)))
                        else:
                            raise ValueError(
                                f"Unexpected shoal-on-sea pixel {pixel}")
                page[raw_mask] = output
            depth_pages.append(page)

    paths = [write_atlas("freshwater", shallow_pages),
             write_atlas("freshwater_deep", deep_pages),
             write_atlas("freshwater_depth", depth_pages),
             write_atlas("freshwater_bank", bank_pages)]
    print("Imported freshwater lake/current atlases:")
    for path in paths:
        print(f"  {path.name}: {path.stat().st_size} bytes, "
              f"{DIRECTION_COUNT} directions ({STILL_FRAME_COUNT} calm + "
              f"{CURRENT_FRAME_COUNT} current frames)")


def acid_surface_tile(template, surface, stone):
    """Recolour a freshwater surface while retaining an acid stone bank."""
    output = replace_water_pixels(template, surface,
                                  mapping=dict(zip(WATER, ACID)))
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            if template.getpixel((x, y))[:3] not in WATER:
                output.putpixel((x, y), stone.getpixel((x, y)))
    return output


def import_acid_and_river(tide, tide_tiles, banks):
    """Write legacy forward river art and the calm/current acid atlas."""
    shores = autotile_images(tide, tide_tiles, "river_on_silt")
    silt_colors = colors({i: tile for i, tile in enumerate(banks[:4])})
    silt_colors = {pixel for pixel in silt_colors if pixel not in WATER}
    source_colors = colors(shores)
    if not silt_colors.issubset(source_colors):
        raise ValueError("River transitions do not contain the silt palette")
    for name, palette in (("river_on_silt", None), ("acid_on_stone", ACID)):
        pages = []
        if name == "acid_on_stone":
            # Acid uses the same four calm lake frames and four oriented
            # current families as freshwater, but keeps the stone bank from
            # the acid transition rather than the freshwater silt bank.
            surfaces = directional_surfaces(None, None, tide, tide_tiles)
            for frames in surfaces:
                for surface in frames:
                    page = {}
                    for raw_mask in range(256):
                        template = shores[normalize_mask(raw_mask), 0]
                        page[raw_mask] = acid_surface_tile(
                            template, surface, banks[8])
                    pages.append(page)
            page_count = PAGE_COUNT
        else:
            for frame in range(CURRENT_FRAME_COUNT):
                page = {}
                for raw_mask in range(256):
                    page[raw_mask] = shores[normalize_mask(raw_mask), frame]
                pages.append(page)
            page_count = CURRENT_FRAME_COUNT
        atlas = Image.new("RGBA", (MASK_ATLAS_SIZE,
                                    MASK_ATLAS_SIZE * page_count))
        for raw_mask in range(256):
            for frame, page in enumerate(pages):
                atlas.paste(page[raw_mask],
                            ((raw_mask % 16) * TILE_SIZE,
                             (raw_mask // 16 + frame * 16) * TILE_SIZE))
        atlas.save(GRAF / f"transition_{name}.png")
        if name == "acid_on_stone":
            print(f"Imported {name}: 47 shore shapes, {STILL_FRAME_COUNT} "
                  f"calm frames and {DIRECTION_COUNT - 1} current directions.")
        else:
            print(f"Imported {name}: 47 shore shapes, "
                  f"{CURRENT_FRAME_COUNT} forward frames.")


def refresh_bank_row(banks):
    """Refresh the reserved bank-ring tiles in the shared atlas."""
    path = GRAF / "16x16.png"
    old = Image.open(path).convert("RGBA")
    if old.width != 512 or old.height < 36 * TILE_SIZE:
        raise ValueError("Expected the existing 512-wide atlas with rows 0..35")
    atlas = Image.new("RGBA", (512, max(old.height, 37 * TILE_SIZE)))
    atlas.paste(old, (0, 0))
    for index, tile in enumerate(banks):
        atlas.paste(tile, (index * 32, 36 * TILE_SIZE))
        r, g, b, a = tile.split()
        dark = Image.merge("RGBA", (r.point(lambda v: v * 3 // 8),
            g.point(lambda v: v * 3 // 8), b.point(lambda v: v * 3 // 8), a))
        atlas.paste(dark, (index * 32 + 16, 36 * TILE_SIZE))
    atlas.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("overworld", type=Path)
    parser.add_argument("tidewater", type=Path)
    parser.add_argument("--water-only", action="store_true",
                        help="do not refresh the shared 16x16 bank atlas")
    args = parser.parse_args()
    land, tide = metadata(args.overworld), metadata(args.tidewater)
    banks = [tile_image(args.tidewater, tide, f"silt_{i}") for i in range(1, 5)]
    banks += [tile_image(args.overworld, land, f"{kind}_{i}")
              for kind in ("dirt", "stone") for i in range(1, 5)]
    if not args.water_only:
        refresh_bank_row(banks)
    import_freshwater(args.overworld, land, args.tidewater, tide)
    import_acid_and_river(args.tidewater, tide, banks)
    for pack, name in ((args.overworld, "overworld"),
                       (args.tidewater, "tidewater")):
        shutil.copyfile(pack / "LICENSE.txt", GRAF / f"LICENSE-verdant-{name}.txt")
    print("Imported four silt, four dirt and four stone bank tiles, with dark variants.")


if __name__ == "__main__":
    main()

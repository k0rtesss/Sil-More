#!/usr/bin/env python3
"""Pack licensed Verdant 03 floors and connected ice/lava transition atlases.

Existing coordinates and pixels remain intact. Requires Pillow and the local
Verdant 03 pack. All 47 authored transition shapes are selected using the
pack's clockwise eight-neighbour masks. Lava rims retain the source pixels;
only matching interior pixels use the pack's existing animation frames.
"""
import argparse
import json
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FLOORS = ("snow_1", "snow_2", "snow_3", "snow_4", "basalt")


def normalize_mask(mask):
    for diagonal, sides in ((2, 5), (8, 20), (32, 80), (128, 65)):
        if mask & sides != sides:
            mask &= ~diagonal
    return mask


def import_transitions(pack, tiles):
    output = ROOT / "lib/xtra/graf"
    lava_fill = Image.open(pack / tiles["lava_still"]["file"]).convert("RGBA")
    frames = [Image.open(pack / tiles[f"anim_lava_flow_f{i}"]["file"]).convert("RGBA")
              for i in range(4)]
    for family in ("ice_on_snow", "lava_on_basalt"):
        shapes = {tile["mask"]: Image.open(pack / tile["file"]).convert("RGBA")
                  for tile in tiles.values() if tile.get("autotileSet") == family}
        assert set(shapes) == {normalize_mask(i) for i in range(256)}
        assert len(shapes) == 47 and all(tile.size == (16, 16) for tile in shapes.values())
        count = 4 if family == "lava_on_basalt" else 1
        atlas = Image.new("RGBA", (256, 256 * count))
        animated = {}
        for mask, tile in shapes.items():
            # Keep the transition outline and a one-pixel buffer around every
            # authored rim pixel. Only the unmodified molten interior moves.
            interior = []
            if count == 4:
                for y in range(16):
                    for x in range(16):
                        neighbours = [(nx, ny) for ny in range(max(0, y-1), min(16, y+2))
                                      for nx in range(max(0, x-1), min(16, x+2))]
                        if all(tile.getpixel(p) == lava_fill.getpixel(p) for p in neighbours) \
                                and any(tile.getpixel(p)[0] > 100 for p in neighbours):
                            interior.append((x, y))
            for frame in range(count):
                variant = tile.copy()
                for point in interior:
                    variant.putpixel(point, frames[frame].getpixel(point))
                if mask == 255 and count == 4:
                    variant = frames[frame]
                animated[mask, frame] = variant
        for raw in range(256):
            for frame in range(count):
                atlas.paste(animated[normalize_mask(raw), frame],
                            ((raw % 16) * 16, (raw // 16) * 16 + frame * 256))
        atlas.save(output / f"transition_{family}.png")
        print(f"Imported {family}: all 47 transition shapes, {count} frame(s).")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", type=Path)
    args = parser.parse_args()
    metadata = json.loads((args.pack / "tiles.json").read_text(encoding="utf-8"))
    tiles = {tile["id"]: tile for tile in metadata["tiles"]}
    path = ROOT / "lib/xtra/graf/16x16.png"
    old = Image.open(path).convert("RGBA")
    if old.width != 512 or old.height < 35 * 16:
        raise ValueError("Expected the existing 512-wide atlas with rows 0..34")
    atlas = Image.new("RGBA", (old.width, max(old.height, 36 * 16)))
    atlas.paste(old, (0, 0))
    for index, name in enumerate(FLOORS):
        tile = Image.open(args.pack / tiles[name]["file"]).convert("RGBA")
        if tile.size != (16, 16):
            raise ValueError(f"Expected a 16x16 source: {name}")
        atlas.paste(tile, (index * 32, 35 * 16))
        channels = tile.split()
        dark = Image.merge("RGBA", tuple(c.point(lambda v: v * 3 // 8)
                           for c in channels[:3]) + (channels[3],))
        atlas.paste(dark, (index * 32 + 16, 35 * 16))
    atlas.save(path)
    print("Imported four snow floors and basalt, with adjacent dark variants.")
    import_transitions(args.pack, tiles)


if __name__ == "__main__":
    main()

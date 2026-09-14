#!/usr/bin/env python3
"""Import Verdant 01 banks and Verdant 04 compatible water families.

Requires Pillow and both purchased packs. Appends bank tiles at atlas row 36,
preserving existing art. Saved shallow/deep water uses the native calm shoal/sea
family, with shallow-to-deep edges and the original 0,1,2,1 surface sequence.
River-flow art is retained as source art, not used to imply currents in lakes.
Acid adapts river water to green with Verdant 01 stone replacing the silt.
"""
import argparse
import json
import shutil
from pathlib import Path

from PIL import Image

from import_verdant_worlds_floors import normalize_mask

ROOT = Path(__file__).resolve().parents[1]
GRAF = ROOT / "lib/xtra/graf"
WATER = ((22, 58, 92), (31, 86, 128), (45, 122, 168),
         (85, 168, 204), (168, 220, 234), (203, 238, 245))
ACID = ((24, 58, 18), (39, 98, 24), (75, 154, 32),
        (138, 204, 52), (208, 240, 105), (235, 255, 168))


def metadata(pack):
    return {tile["id"]: tile for tile in
            json.loads((pack / "tiles.json").read_text(encoding="utf-8"))["tiles"]}


def tile_image(pack, tiles, name):
    tile = Image.open(pack / tiles[name]["file"]).convert("RGBA")
    assert tile.size == (16, 16), name
    return tile


def import_calm_water(pack, tiles, silt):
    surfaces = {}
    for family in ("surf_on_wet", "shoal_on_sea"):
        surfaces[family] = {(t["mask"], t["animFrame"]): tile_image(pack, tiles, t["id"])
            for t in tiles.values() if t.get("autotileSet") == family}
        assert len(surfaces[family]) == 47 * 3
    shoals = [tile_image(pack, tiles, f"anim_shoal_f{f}") for f in range(3)]
    seas = [tile_image(pack, tiles, f"anim_sea_f{f}") for f in range(3)]
    shoal_colors = {p for tile in shoals for _, p in tile.getcolors(256)}
    foam = {(244, 252, 254, 255), (203, 238, 245, 255), (168, 220, 234, 255)}
    outputs = {name: Image.new("RGBA", (256, 1024)) for name in (
        "shoal_on_silt", "sea_on_silt", "shoal_on_sea", "water_bank_overlay")}
    for raw in range(256):
        mask = normalize_mask(raw)
        for stage, frame in enumerate((0, 1, 2, 1)):
            shallow = surfaces["surf_on_wet"][mask, frame].copy()
            deep = shallow.copy()
            bank = shallow.copy()
            for y in range(16):
                for x in range(16):
                    point = (x, y)
                    pixel = shallow.getpixel(point)
                    if pixel in shoal_colors:
                        deep.putpixel(point, seas[frame].getpixel(point))
                        bank.putpixel(point, (0, 0, 0, 0))
                    elif pixel not in foam:
                        # Only the outer ground changes: wet sand to cave silt.
                        ground = silt.getpixel(point)
                        shallow.putpixel(point, ground)
                        deep.putpixel(point, ground)
                        bank.putpixel(point, ground)
            assert mask != 255 or shallow.tobytes() == shoals[frame].tobytes()
            assert mask != 255 or deep.tobytes() == seas[frame].tobytes()
            dst = ((raw % 16) * 16, (raw // 16) * 16 + stage * 256)
            for name, tile in (("shoal_on_silt", shallow), ("sea_on_silt", deep),
                    ("shoal_on_sea", surfaces["shoal_on_sea"][mask, frame]),
                    ("water_bank_overlay", bank)):
                outputs[name].paste(tile, dst)
    for name, atlas in outputs.items():
        atlas.save(GRAF / f"transition_{name}.png")
    print("Imported calm shoal/sea water, native depth edges and one shoreline overlay (0,1,2,1).")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("overworld", type=Path)
    parser.add_argument("tidewater", type=Path)
    args = parser.parse_args()
    land, tide = metadata(args.overworld), metadata(args.tidewater)
    banks = [tile_image(args.tidewater, tide, f"silt_{i}") for i in range(1, 5)]
    banks += [tile_image(args.overworld, land, f"{kind}_{i}")
              for kind in ("dirt", "stone") for i in range(1, 5)]
    path = GRAF / "16x16.png"
    old = Image.open(path).convert("RGBA")
    assert old.width == 512 and old.height >= 36 * 16
    atlas = Image.new("RGBA", (512, max(old.height, 37 * 16)))
    atlas.paste(old, (0, 0))
    for index, tile in enumerate(banks):
        atlas.paste(tile, (index * 32, 36 * 16))
        r, g, b, a = tile.split()
        dark = Image.merge("RGBA", (r.point(lambda v: v * 3 // 8),
            g.point(lambda v: v * 3 // 8), b.point(lambda v: v * 3 // 8), a))
        atlas.paste(dark, (index * 32 + 16, 36 * 16))
    atlas.save(path)

    shores = {(tile["mask"], tile["animFrame"]):
              tile_image(args.tidewater, tide, tile["id"])
              for tile in tide.values() if tile.get("autotileSet") == "river_on_silt"}
    masks = {normalize_mask(raw) for raw in range(256)}
    assert len(masks) == 47 and set(shores) == {(m, f) for m in masks for f in range(3)}
    silt_colors = {p[:3] for tile in banks[:4] for _, p in tile.getcolors(256)}
    assert not silt_colors.intersection(WATER)
    colors = {p[:3] for tile in shores.values() for _, p in tile.getcolors(256)}
    assert colors == silt_colors.union(WATER), colors
    for name, palette in (("river_on_silt", None), ("acid_on_stone", ACID)):
        atlas = Image.new("RGBA", (256, 768))
        mapping = dict(zip(WATER, palette)) if palette else {}
        for raw in range(256):
            mask = normalize_mask(raw)
            for frame in range(3):
                tile = shores[mask, frame].copy()
                if palette:
                    for y in range(16):
                        for x in range(16):
                            pixel = tile.getpixel((x, y))
                            if pixel[:3] in mapping:
                                tile.putpixel((x, y), (*mapping[pixel[:3]], pixel[3]))
                            elif name == "acid_on_stone":
                                tile.putpixel((x, y), banks[8].getpixel((x, y)))
                atlas.paste(tile, ((raw % 16) * 16, (raw // 16) * 16 + frame * 256))
        atlas.save(GRAF / f"transition_{name}.png")
        print(f"Imported {name}: 47 shore shapes, 3 forward animation frames.")
    import_calm_water(args.tidewater, tide, banks[0])
    for pack, name in ((args.overworld, "overworld"), (args.tidewater, "tidewater")):
        shutil.copyfile(pack / "LICENSE.txt", GRAF / f"LICENSE-verdant-{name}.txt")
    print("Imported four silt, four dirt and four stone bank tiles, with dark variants.")


if __name__ == "__main__":
    main()

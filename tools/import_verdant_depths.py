#!/usr/bin/env python3
"""Import Verdant 05 flagstone, masonry and moss walls plus one plain chasm tile.

Requires Pillow. Existing atlas rows 0..32 and their tile coordinates are kept.
The source pack is local licensed artwork, not downloaded by this utility.
"""
import argparse
import json
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", type=Path)
    args = parser.parse_args()
    metadata = json.loads((args.pack / "tiles.json").read_text(encoding="utf-8"))
    tiles = {tile["id"]: tile for tile in metadata["tiles"]}
    atlas_path = ROOT / "lib/xtra/graf/16x16.png"
    old = Image.open(atlas_path).convert("RGBA")
    # Rows 33 onward were added by this importer. Replace that reserved area,
    # including the retired chasm edge atlas, while preserving original art.
    if old.size not in ((512, 528), (512, 560), (512, 608)):
        raise ValueError("Unexpected atlas dimensions; check reserved rows before importing")
    atlas = Image.new("RGBA", (512, 35 * 16))
    atlas.paste(old.crop((0, 0, 512, 33 * 16)), (0, 0))

    def pair(name, row, col):
        tile = Image.open(args.pack / tiles[name]["file"]).convert("RGBA")
        assert tile.size == (16, 16), name
        atlas.paste(tile, (col * 16, row * 16))
        # Existing terrain lighting selects the adjacent column for darkness.
        channels = tile.split()
        dark = Image.merge("RGBA", tuple(c.point(lambda v: v * 3 // 8)
                           for c in channels[:3]) + (channels[3],))
        atlas.paste(dark, ((col + 1) * 16, row * 16))

    for index, name in enumerate(("flag_1", "mason_1", "moss_1")):
        pair(name, 33, index * 2)
    pair("chasm_2", 34, 0)
    atlas.save(atlas_path)
    print("Imported flagstone, masonry and moss walls, plus the plain chasm_2 tile.")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Import Verdant 05 cave materials and static chasm fill variants.

Requires Pillow. Existing atlas rows 0..32 and their tile coordinates are kept.
Rows 33 and 34 hold the static wall/chasm art. The three chasm fill variants
are placed together in row 34; the renderer tiles those fills without any
edge/lip transition art. The source pack is local licensed artwork, not
downloaded by this utility.
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
    # Rows 33..34 belong to this importer. Keep newer floor art in row 35
    # and beyond when refreshing these walls/chasm.
    if old.width != 512 or old.height < 35 * 16:
        raise ValueError("Unexpected atlas dimensions; check reserved rows before importing")
    atlas = Image.new("RGBA", (512, max(old.height, 35 * 16)))
    atlas.paste(old, (0, 0))
    atlas.paste((0, 0, 0, 0), (0, 33 * 16, 512, 35 * 16))

    def pair(name, row, col):
        tile = Image.open(args.pack / tiles[name]["file"]).convert("RGBA")
        assert tile.size == (16, 16), name
        atlas.paste(tile, (col * 16, row * 16))
        # Existing terrain lighting selects the adjacent column for darkness.
        channels = tile.split()
        dark = Image.merge("RGBA", tuple(c.point(lambda v: v * 3 // 8)
                           for c in channels[:3]) + (channels[3],))
        atlas.paste(dark, ((col + 1) * 16, row * 16))

    # Keep the four authored wall materials together. The adjacent columns
    # are their dark variants, matching the atlas convention used by style W:
    # coordinates. The first three materials are selected by existing styles;
    # brick is available for the additional cave style below.
    for index, name in enumerate(("flag_1", "mason_1", "moss_1", "brick_1")):
        pair(name, 33, index * 2)
    for index, name in enumerate(("chasm_1", "chasm_2", "chasm_3")):
        pair(name, 34, index * 2)
    atlas.save(atlas_path)
    print("Imported flagstone, masonry, moss and brick walls, plus chasm_1/2/3 "
          "static fill variants without transition/lip art.")


if __name__ == "__main__":
    main()

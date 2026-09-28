#!/usr/bin/env python3
"""Import Verdant 03 melting frames and connected ice-shore animation.

Accepts the licensed pack ZIP or its extracted directory. The existing
lib/xtra/graf/LICENSE-verdant-worlds.txt covers these incorporated game assets.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile
from collections import Counter
from io import BytesIO

from PIL import Image

from import_verdant_worlds_floors import normalize_mask

ROOT = Path(__file__).resolve().parents[1]


def import_frames(read):
    metadata = json.loads(read("tiles.json"))
    tiles = {tile["id"]: tile for tile in metadata["tiles"]}
    output = ROOT / "lib/xtra/graf"
    frames = []
    for frame in range(4):
        name = f"anim_ice_melt_f{frame}"
        tile = tiles[name]
        assert tile["animSet"] == "anim_ice_melt"
        assert tile["animFrame"] == frame and tile["animFrameCount"] == 4
        data = read(tile["file"])
        assert data[:8] == b"\x89PNG\r\n\x1a\n"
        assert struct.unpack(">II", data[16:24]) == (16, 16)
        path = output / f"{name}.png"
        path.write_bytes(data)
        frames.append(Image.open(BytesIO(data)).convert("RGBA"))
        print(f"{path.name}: unchanged source, SHA256 {hashlib.sha256(data).hexdigest()}")

    # Reuse the authored snow banks and textured ice, rather than putting a
    # flat blue square in the middle of a connected sheet. Only the native
    # dark melt pockets replace ice pixels. Keep an ice seam at tile borders
    # and a one-pixel buffer around authored shoreline details so either ice
    # variant can adjoin this tile in any direction, on every frame.
    fill = Image.open(BytesIO(read(tiles["ice_sheet"]["file"]))).convert("RGBA")
    shapes = {tile["mask"]: Image.open(BytesIO(read(tile["file"]))).convert("RGBA")
              for tile in tiles.values() if tile.get("autotileSet") == "ice_on_snow"}
    assert set(shapes) == {normalize_mask(i) for i in range(256)}
    assert len(shapes) == 47
    atlas = Image.new("RGBA", (256, 1024))
    variants = {}
    for mask, shape in shapes.items():
        interior = [(x, y) for y in range(1, 15) for x in range(1, 15)
                    if all(shape.getpixel((xx, yy)) == fill.getpixel((xx, yy))
                           for yy in range(y - 1, y + 2)
                           for xx in range(x - 1, x + 2))]
        for frame, source in enumerate(frames):
            background = Counter(source.getpixel((x, y)) for y in range(16)
                                 for x in range(16)).most_common(1)[0][0]
            variant = shape.copy()
            for point in interior:
                if source.getpixel(point) != background:
                    variant.putpixel(point, source.getpixel(point))
            variants[mask, frame] = variant
    for raw in range(256):
        for frame in range(4):
            atlas.paste(variants[normalize_mask(raw), frame],
                        ((raw % 16) * 16, (raw // 16) * 16 + frame * 256))
    atlas.save(output / "transition_melting_ice_on_snow.png")
    print("Imported connected melting ice: 47 shore shapes, four frames.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", type=Path)
    args = parser.parse_args()
    if args.pack.is_dir():
        import_frames(lambda name: (args.pack / name).read_bytes())
    else:
        with zipfile.ZipFile(args.pack) as archive:
            import_frames(archive.read)


if __name__ == "__main__":
    main()

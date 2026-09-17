#!/usr/bin/env python3
"""Composite native Verdant ice and freshwater with one snowy outer shoreline.

No new artwork is painted: the licensed Verdant 03 ice_on_snow shapes supply
the exact geometry, ice lips and snow pixels; staged freshwater atlases supply
the existing animation surfaces. LICENSE-verdant-worlds.txt and
LICENSE-verdant-tidewater.txt cover the incorporated game assets.

Shallow/deep/bank atlases contain the same 16 pages as transition_freshwater:
four calm frames, then three current frames for each of N/E/S/W. Select these
with the combined ice/water neighbour mask. Solid (one page) and melting
(four pages) are transparent ice overlays selected with the ice-only mask.
The existing freshwater_depth atlas can be drawn beneath the snow bank.

Run with the original Verdant 03 ZIP or extracted directory. All source
classification and compositing invariants are checked before writing.
"""

import argparse
from io import BytesIO
import json
from pathlib import Path
import zipfile

from PIL import Image

from import_verdant_worlds_floors import normalize_mask
from import_verdant_tidewater import WATER, DEEP_WATER

ROOT = Path(__file__).resolve().parents[1]
GRAF = ROOT / "lib/xtra/graf"
TILE = 16
PAGE = 256
WATER_PAGES = 16
CLEAR = (0, 0, 0, 0)


def pixels(image):
    return list(image.get_flattened_data())


def tile_at(atlas, mask, page=0):
    x, y = mask % 16 * TILE, mask // 16 * TILE + page * PAGE
    return atlas.crop((x, y, x + TILE, y + TILE))


def classify_ice(shape, fill, snow):
    """Recover exact native ice support, including its shared white highlights.

    Snow and ice use one common white. Color-only removal would erase ice
    highlights; preserving every white would leave flecks of snow on water.
    The original fills disambiguate that white at each pixel position. The
    authored ice lip belongs to the exclusive ice palette and stays opaque.
    """
    ice_colors, snow_colors = set(pixels(fill)), set(pixels(snow))
    exclusive_ice = ice_colors - snow_colors
    result = []
    for index, color in enumerate(pixels(shape)):
        x, y = index % TILE, index // TILE
        ice_pixel, snow_pixel = fill.getpixel((x, y)), snow.getpixel((x, y))
        if color in exclusive_ice:
            inside = True
        elif color in ice_colors & snow_colors:
            # A shared color must occur in only one of the two native fills
            # at this position, otherwise provenance cannot be established.
            assert (color == ice_pixel) != (color == snow_pixel), (x, y, color)
            inside = color == ice_pixel
        else:
            inside = False
        assert color[3] == 255
        assert color in ice_colors if inside else color == snow_pixel
        result.append(inside)
    return tuple(result)


def keep_area(source, support, inside):
    output = Image.new("RGBA", (TILE, TILE), CLEAR)
    output.putdata([color if selected == inside else CLEAR
                    for color, selected in zip(pixels(source), support)])
    return output


def import_assets(read):
    tiles = {tile["id"]: tile for tile in json.loads(read("tiles.json"))["tiles"]}

    def native(tile):
        image = Image.open(BytesIO(read(tile["file"]))).convert("RGBA")
        assert image.size == (TILE, TILE)
        return image

    fill, snow = native(tiles["ice_sheet"]), native(tiles["snow_1"])
    shapes = {tile["mask"]: native(tile) for tile in tiles.values()
              if tile.get("autotileSet") == "ice_on_snow"}
    expected = {normalize_mask(raw) for raw in range(256)}
    assert set(shapes) == expected and len(shapes) == 47
    support = {mask: classify_ice(shape, fill, snow) for mask, shape in shapes.items()}
    assert all(support[255]) and 0 < sum(support[0]) < TILE * TILE
    # Every native ice shape is connected, with arms reaching only the
    # requested sides. White highlights must not become stray alpha dots.
    # Do not assume strictly nested masks: native jittered edges can differ
    # by a pixel when a side becomes a concave corner.
    for mask, area in support.items():
        visited, pending = set(), [area.index(True)]
        while pending:
            index = pending.pop()
            if index in visited:
                continue
            visited.add(index)
            x, y = index % TILE, index // TILE
            for nx, ny in ((x-1,y), (x+1,y), (x,y-1), (x,y+1)):
                if 0 <= nx < TILE and 0 <= ny < TILE and area[ny*TILE+nx]:
                    pending.append(ny*TILE+nx)
        assert len(visited) == sum(area)
        for bit, edge in ((1, area[:TILE]), (4, area[15::TILE]),
                          (16, area[-TILE:]), (64, area[::TILE])):
            assert any(edge) == bool(mask & bit)

    staged_solid = Image.open(GRAF / "transition_ice_on_snow.png").convert("RGBA")
    staged_melting = Image.open(GRAF / "transition_melting_ice_on_snow.png").convert("RGBA")
    assert staged_solid.size == (PAGE, PAGE)
    assert staged_melting.size == (PAGE, PAGE * 4)
    shallow = Image.open(GRAF / "transition_freshwater.png").convert("RGBA")
    deep = Image.open(GRAF / "transition_freshwater_deep.png").convert("RGBA")
    assert shallow.size == deep.size == (PAGE, PAGE * WATER_PAGES)
    surfaces = [[tile_at(atlas, 255, page) for page in range(WATER_PAGES)]
                for atlas in (shallow, deep)]
    for frames, palette in zip(surfaces, (set(WATER), set(DEEP_WATER.values()))):
        assert all(color[:3] in palette and color[3] == 255
                   for frame in frames for color in pixels(frame))
    outputs = {name: Image.new("RGBA", (PAGE, PAGE * pages), CLEAR)
               for name, pages in (("shallow", WATER_PAGES), ("deep", WATER_PAGES),
                                   ("bank", WATER_PAGES), ("solid", 1), ("melting", 4))}

    for raw in range(256):
        mask = normalize_mask(raw)
        shape, area = shapes[mask], support[mask]
        assert pixels(tile_at(staged_solid, raw)) == pixels(shape)
        ice = keep_area(shape, area, True)
        bank = keep_area(shape, area, False)
        # Native pixels reconstruct exactly; no color approximations or
        # erased highlights hide within the transparent decomposition.
        assert pixels(Image.alpha_composite(bank, ice)) == pixels(shape)
        x, y = raw % 16 * TILE, raw // 16 * TILE
        outputs["solid"].paste(ice, (x, y))
        for frame in range(4):
            melting = tile_at(staged_melting, raw, frame)
            overlay = keep_area(melting, area, True)
            assert pixels(Image.alpha_composite(bank, overlay)) == pixels(melting)
            assert pixels(overlay.getchannel("A")) == pixels(ice.getchannel("A"))
            outputs["melting"].paste(overlay, (x, y + frame * PAGE))
        for page in range(WATER_PAGES):
            position = (x, y + page * PAGE)
            outputs["bank"].paste(bank, position)
            for name, frames in zip(("shallow", "deep"), surfaces):
                surface = keep_area(frames[page], area, True)
                combined = Image.alpha_composite(bank, surface)
                assert all(color[3] == 255 for color in pixels(combined))
                outputs[name].paste(combined, position)

    for name, atlas in outputs.items():
        path = GRAF / f"transition_ice_water_{name}.png"
        atlas.save(path)
        print(f"{path.name}: {atlas.width}x{atlas.height}")
    print("PASS: 47 native shapes, exact snow/ice reconstruction, retained highlights,")
    print("      connected side-correct geometry, 256 masks, fixed four-frame ice alpha,")
    print("      16 existing freshwater pages, opaque snow shores and no silt pixels.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", type=Path)
    args = parser.parse_args()
    if args.pack.is_dir():
        import_assets(lambda name: (args.pack / name).read_bytes())
    else:
        with zipfile.ZipFile(args.pack) as archive:
            import_assets(archive.read)


if __name__ == "__main__":
    main()

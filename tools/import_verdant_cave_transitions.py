#!/usr/bin/env python3
"""Generate custom-pixel elemental cave transitions from the Verdant 47-mask algorithm.

The HTML generator normally writes palette keys into ``Canvas``. This importer is the raster
equivalent: it copies pixels directly from licensed Verdant source tiles, applies the generator's
``insideMask``/``rimPixels`` geometry, and expands the 47 normalized masks to the 256 raw-mask
atlas format used by the SDL renderer.

The transition direction is intentional:

    snow over dirt
    basalt over dirt

The dirt tile comes from Verdant 01 and the upper material tiles come from Verdant 03. A static
atlas is sufficient because these are floor-material transitions, not animated hazards.
"""

import argparse
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "lib/xtra/graf"
SIZE = 16
ATLAS_SIZE = SIZE * SIZE

N, NE, E, SE, S, SW, W, NW = 1, 2, 4, 8, 16, 32, 64, 128
JITTER_SEED = 31337


def hash2(i, j, seed):
    """The HTML generator's low-32-bit integer hash, in Python form."""
    mask = 0xFFFFFFFF
    h = ((i & mask) * 374761393 + (j & mask) * 668265263
         + (seed & mask) * 2147483647) & mask
    h = (h ^ (h >> 13)) & mask
    h = (h * 1274126177) & mask
    return ((h ^ (h >> 16)) & mask) / 4294967296.0


def smooth(t):
    return t * t * (3.0 - 2.0 * t)


def torus_sample(size, freq, seed, x, y):
    cell = size / freq
    gx, gy = x / cell, y / cell
    fx = smooth(gx - int(gx))
    fy = smooth(gy - int(gy))

    def mod(value):
        return value % freq

    i0, j0 = mod(int(gx)), mod(int(gy))
    i1, j1 = mod(i0 + 1), mod(j0 + 1)
    v00 = hash2(i0, j0, seed)
    v10 = hash2(i1, j0, seed)
    v01 = hash2(i0, j1, seed)
    v11 = hash2(i1, j1, seed)
    a = v00 + (v10 - v00) * fx
    b = v01 + (v11 - v01) * fx
    return a + (b - a) * fy


def make_jitter(cell, amount):
    """Mirror makeJitter() from VerdantForge.html."""
    lo = 0.19 * amount
    hi = 1.0 - (1.0 - 0.86) * amount

    def jitter(t, axis):
        value = (torus_sample(cell, 4, JITTER_SEED, 0.5, t)
                 if axis == 0
                 else torus_sample(cell, 4, JITTER_SEED + 77, t, 0.5))
        return -1 if value < lo else (1 if value > hi else 0)

    return jitter


def normalize_mask(mask):
    """Clear diagonals that do not touch both adjacent cardinal sides."""
    for diagonal, sides in ((NE, N | E), (SE, E | S),
                            (SW, S | W), (NW, W | N)):
        if mask & sides != sides:
            mask &= ~diagonal
    return mask


def inside_mask(mask, size=SIZE, inset=3, notch=3, jitter=None):
    """Mirror insideMask() from VerdantForge.html for a 16px tile."""
    jitter = jitter or (lambda _t, _axis: 0)
    half = size / 2
    out = [[False] * size for _ in range(size)]
    for y in range(size):
        for x in range(size):
            left, top = x < half, y < half
            u = x if left else size - 1 - x
            v = y if top else size - 1 - y
            has_h = (mask & W) if left else (mask & E)
            has_v = (mask & N) if top else (mask & S)
            if left:
                has_c = (mask & NW) if top else (mask & SW)
            else:
                has_c = (mask & NE) if top else (mask & SE)
            jv = jitter(y, 0)
            jh = jitter(x, 1)
            if has_h and has_v:
                inside = bool(has_c) or (u + v) >= notch
            elif has_v and not has_h:
                inside = u >= inset + jv
            elif has_h and not has_v:
                inside = v >= inset + jh
            else:
                inside = (u >= inset + jv) and (v >= inset + jh)
            out[y][x] = inside
    return out


def rim_pixels(mask, neighbourhood):
    """Mirror rimPixels(): cardinal-edge pixels on the upper material side."""
    size = len(mask)

    def at(x, y):
        if x < 0:
            return bool(neighbourhood & W)
        if x >= size:
            return bool(neighbourhood & E)
        if y < 0:
            return bool(neighbourhood & N)
        if y >= size:
            return bool(neighbourhood & S)
        return mask[y][x]

    out = []
    for y in range(size):
        for x in range(size):
            if mask[y][x] and not (at(x - 1, y) and at(x + 1, y)
                                   and at(x, y - 1) and at(x, y + 1)):
                out.append((x, y))
    return out


def source_colour(image, darkest=True):
    pixels = [p for p in image.getdata() if p[3]]
    if not pixels:
        raise ValueError("source tile has no opaque pixels")
    return min(pixels, key=lambda p: sum(p[:3])) if darkest else pixels[0]


def compose_tile(upper, lower, neighbourhood, jitter):
    if upper.size != (SIZE, SIZE) or lower.size != (SIZE, SIZE):
        raise ValueError("custom cave transition sources must be 16x16")
    mask = inside_mask(neighbourhood, jitter=jitter)
    out = Image.new("RGBA", (SIZE, SIZE))
    for y in range(SIZE):
        for x in range(SIZE):
            out.putpixel((x, y), upper.getpixel((x, y))
                         if mask[y][x] else lower.getpixel((x, y)))
    rim = source_colour(upper)
    for x, y in rim_pixels(mask, neighbourhood):
        out.putpixel((x, y), rim)
    return out


def transition_atlas(upper, lower):
    atlas = Image.new("RGBA", (ATLAS_SIZE, ATLAS_SIZE))
    jitter = make_jitter(SIZE, 1)
    for raw_mask in range(256):
        tile = compose_tile(upper, lower, normalize_mask(raw_mask), jitter)
        atlas.paste(tile, ((raw_mask % SIZE) * SIZE, (raw_mask // SIZE) * SIZE))
    return atlas


def load(path):
    image = Image.open(path).convert("RGBA")
    if image.size != (SIZE, SIZE):
        raise ValueError(f"expected a 16x16 tile, got {image.size}: {path}")
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("overworld", type=Path,
                        help="Verdant 01 overworld pack directory")
    parser.add_argument("worlds", type=Path,
                        help="Verdant 03 worlds pack directory")
    args = parser.parse_args()

    dirt = load(args.overworld / "tiles/16x16/dirt_1.png")
    snow = load(args.worlds / "tiles/16x16/snow_1.png")
    basalt = load(args.worlds / "tiles/16x16/basalt.png")
    OUTPUT.mkdir(parents=True, exist_ok=True)

    masks = {normalize_mask(mask) for mask in range(256)}
    if len(masks) != 47:
        raise AssertionError(f"expected 47 normalized masks, got {len(masks)}")

    outputs = {
        "transition_snow_on_dirt.png": transition_atlas(snow, dirt),
        "transition_basalt_on_dirt.png": transition_atlas(basalt, dirt),
    }
    for name, atlas in outputs.items():
        atlas.save(OUTPUT / name)
        print(f"Generated {name}: 256 raw masks from 47 normalized shapes.")


if __name__ == "__main__":
    main()

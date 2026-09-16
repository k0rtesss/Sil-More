const test = require('node:test');
const assert = require('node:assert/strict');
const TileForge = require('./engine.js');

function pixelSource(size, fn) {
  const data = new Uint8ClampedArray(size * size * 4);
  for (let y = 0; y < size; y += 1) {
    for (let x = 0; x < size; x += 1) {
      const p = fn(x, y);
      data.set(p, (y * size + x) * 4);
    }
  }
  return { width: size, height: size, data };
}

function solid(size, p) {
  return pixelSource(size, () => p);
}

function rgbaAt(image, x, y) {
  return Array.from(image.data.subarray((y * image.width + x) * 4,
    (y * image.width + x + 1) * 4));
}

const SIZE = 16;
const FG = pixelSource(SIZE, (x, y) => [180 + (x % 5), 80 + y, 35, 255]);
const BG = pixelSource(SIZE, (x, y) => [12, 24 + (y % 3), 52 + x, 255]);
const FACE = solid(SIZE, [28, 32, 38, 255]);

test('normalization exposes the sorted 47 canonical masks', () => {
  assert.equal(TileForge.MASKS.length, 47);
  assert.deepEqual(TileForge.MASKS, [...TileForge.MASKS].sort((a, b) => a - b));
  assert.equal(TileForge.MASKS[0], 0);
  assert.equal(TileForge.MASKS[TileForge.MASKS.length - 1], 255);
  for (let raw = 0; raw < 256; raw += 1) {
    const canonical = TileForge.normalizeMask(raw);
    assert.equal(TileForge.MASKS.includes(canonical), true);
    assert.equal(TileForge.normalizeMask(canonical), canonical);
  }
  // A diagonal without both supporting cardinals cannot survive reduction.
  assert.equal(TileForge.normalizeMask(2), 0);
  assert.equal(TileForge.normalizeMask(1 | 4 | 2), 7);
});

test('maskAt compares values and treats the map exterior as the center cell', () => {
  const grid = [
    [0, 1, 1],
    [0, 0, 1],
    [0, 1, 1]
  ];
  const raw = TileForge.maskAt(grid, 1, 1);
  assert.equal(raw, 224); // SW, W and NW are the only equal-valued neighbours.
  assert.equal(TileForge.normalizeMask(raw), 64); // only W remains canonical.

  const classes = [[1, 2], [2, 1]];
  const custom = TileForge.maskAt(classes, 0, 0, (neighbour, centre) =>
    neighbour > centre);
  assert.equal(custom, 4 | 16);
});

test('full floor and chasm tiles preserve every source byte', () => {
  for (const mode of ['floor', 'chasm']) {
    const rendered = TileForge.renderTile({
      size: SIZE, mask: 255, mode, foreground: FG, background: BG, face: FACE
    });
    assert.deepEqual(Array.from(rendered.data), Array.from(FG.data));
    assert.equal(rendered.width, SIZE);
    assert.equal(rendered.height, SIZE);
  }
});

test('palette locked floor styles select source pixels without inventing colours', () => {
  const sources = new Set();
  for (let y = 0; y < SIZE; y += 1) {
    for (let x = 0; x < SIZE; x += 1) {
      sources.add(rgbaAt(FG, x, y).join(','));
      sources.add(rgbaAt(BG, x, y).join(','));
    }
  }
  for (const style of ['organic', 'cut', 'dither', 'soft']) {
    const rendered = TileForge.renderTile({
      size: SIZE, mask: 0, style, mode: 'floor', seed: 9,
      foreground: FG, background: BG, paletteLock: true
    });
    for (let y = 0; y < SIZE; y += 1) {
      for (let x = 0; x < SIZE; x += 1) {
        assert.equal(sources.has(rgbaAt(rendered, x, y).join(',')), true,
          `${style} generated a colour outside its source palettes`);
      }
    }
  }
});

test('soft mode can blend RGBA sources when palette locking is disabled', () => {
  const rendered = TileForge.renderTile({
    size: SIZE, mask: 0, style: 'soft', mode: 'floor', blend: 4,
    foreground: solid(SIZE, [200, 100, 20, 255]),
    background: solid(SIZE, [20, 30, 80, 255]),
    paletteLock: false
  });
  const colours = new Set();
  for (let i = 0; i < rendered.data.length; i += 4) {
    colours.add(Array.from(rendered.data.subarray(i, i + 4)).join(','));
  }
  assert.equal(colours.has('200,100,20,255'), true);
  assert.ok(colours.size > 2, 'soft edge should contain at least one blend');
});

test('organic edge profiles are seeded and independent of unrelated mask bits', () => {
  const opts = {
    size: SIZE, style: 'organic', mode: 'floor', seed: 1234, roughness: 2,
    foreground: solid(SIZE, [255, 255, 255, 255]),
    background: solid(SIZE, [0, 0, 0, 255])
  };
  const first = TileForge.renderTile({ ...opts, mask: 0 });
  const same = TileForge.renderTile({ ...opts, mask: 0 });
  const changedSeed = TileForge.renderTile({ ...opts, mask: 0, seed: 1235 });
  const changedMask = TileForge.renderTile({ ...opts, mask: 64 });
  assert.deepEqual(Array.from(first.data), Array.from(same.data));
  assert.notDeepEqual(Array.from(first.data), Array.from(changedSeed.data));

  // Both masks have an eastern cut.  Its row profile must not move when W is
  // toggled, which is the local seam invariant used by adjacent tiles.
  for (let y = 0; y < SIZE; y += 1) {
    let rightA = -1;
    let rightB = -1;
    for (let x = SIZE - 1; x >= 0; x -= 1) {
      if (rgbaAt(first, x, y)[0] !== 0) { rightA = x; break; }
    }
    for (let x = SIZE - 1; x >= 0; x -= 1) {
      if (rgbaAt(changedMask, x, y)[0] !== 0) { rightB = x; break; }
    }
    assert.equal(rightA, rightB, `eastern profile changed at row ${y}`);
  }
});

test('all valid neighboring foreground cells agree at shared tile seams', () => {
  const solidForeground = solid(SIZE, [255, 255, 255, 255]);
  const solidBackground = solid(SIZE, [0, 0, 0, 255]);
  const cache = new Map();
  const tileFor = mask => {
    const canonical = TileForge.normalizeMask(mask);
    if (!cache.has(canonical)) {
      cache.set(canonical, TileForge.renderTile({
        size: SIZE, mask: canonical, mode: 'floor', style: 'organic',
        roughness: 1, seed: 42,
        foreground: solidForeground, background: solidBackground
      }));
    }
    return cache.get(canonical);
  };
  const channel = (image, x, y) => image.data[(y * SIZE + x) * 4];

  // Enumerate every 4x3 binary neighborhood.  Equal foreground cells should
  // meet at both columns of a horizontal seam and both rows of a vertical
  // seam, including T-junctions around concave diagonal corners.
  for (let state = 0; state < (1 << 12); state += 1) {
    const grid = Array.from({ length: 3 }, (_, y) =>
      Array.from({ length: 4 }, (_, x) => (state >> (y * 4 + x)) & 1));
    for (let y = 0; y < 3; y += 1) {
      for (let x = 0; x < 3; x += 1) {
        if (!grid[y][x] || !grid[y][x + 1]) continue;
        const left = tileFor(TileForge.maskAt(grid, x, y));
        const right = tileFor(TileForge.maskAt(grid, x + 1, y));
        for (let row = 0; row < SIZE; row += 1) {
          assert.equal(channel(left, SIZE - 1, row), channel(right, 0, row),
            `horizontal seam state ${state}, cells ${x},${y}`);
        }
      }
    }
    for (let y = 0; y < 2; y += 1) {
      for (let x = 0; x < 4; x += 1) {
        if (!grid[y][x] || !grid[y + 1][x]) continue;
        const upper = tileFor(TileForge.maskAt(grid, x, y));
        const lower = tileFor(TileForge.maskAt(grid, x, y + 1));
        for (let column = 0; column < SIZE; column += 1) {
          assert.equal(channel(upper, column, SIZE - 1), channel(lower, column, 0),
            `vertical seam state ${state}, cells ${x},${y}`);
        }
      }
    }
  }
});

test('zero blend is a hard edge for dither and soft styles', () => {
  const options = {
    size: SIZE, mask: 0, mode: 'floor', blend: 0,
    foreground: solid(SIZE, [220, 120, 30, 255]),
    background: solid(SIZE, [10, 20, 80, 255]), paletteLock: false
  };
  for (const style of ['dither', 'soft']) {
    const rendered = TileForge.renderTile({ ...options, style });
    for (let i = 0; i < rendered.data.length; i += 4) {
      const colour = Array.from(rendered.data.subarray(i, i + 4)).join(',');
      assert.ok(colour === '220,120,30,255' || colour === '10,20,80,255',
        `${style} with zero blend produced an interpolated pixel`);
    }
  }
});

test('all modes return valid alpha rasters and wall/chasm apply a south face', () => {
  for (const mode of ['floor', 'wall', 'chasm']) {
    for (const style of ['organic', 'cut', 'dither', 'soft']) {
      const rendered = TileForge.renderTile({
        size: SIZE, mask: 1 | 64, mode, style, seed: 77,
        foreground: FG, background: BG, face: FACE
      });
      assert.equal(rendered.data instanceof Uint8ClampedArray, true);
      assert.equal(rendered.data.length, SIZE * SIZE * 4);
      for (let i = 3; i < rendered.data.length; i += 4) {
        assert.ok(rendered.data[i] >= 0 && rendered.data[i] <= 255);
      }
    }
  }
  const wall = TileForge.renderTile({
    size: SIZE, mask: 1 | 64, mode: 'wall', style: 'cut',
    foreground: solid(SIZE, [220, 180, 120, 255]),
    background: solid(SIZE, [10, 10, 10, 255]),
    face: FACE, depth: 4
  });
  assert.equal(rgbaAt(wall, 8, 15).join(','), '28,32,38,255');
});

test('transparent source and face alpha are retained through composition', () => {
  const transparent = solid(SIZE, [200, 20, 20, 0]);
  const halfFace = solid(SIZE, [20, 200, 20, 128]);
  const rendered = TileForge.renderTile({
    size: SIZE, mask: 1 | 64, mode: 'wall', style: 'cut', depth: 4,
    foreground: transparent, background: transparent, face: halfFace
  });
  const facePixel = rgbaAt(rendered, 8, 15);
  assert.equal(facePixel[3], 128);
  assert.equal(facePixel[0], 20);
  assert.equal(facePixel[1], 200);
});

test('palette-locked lips use existing directional colors and concave south faces', () => {
  const upper = pixelSource(SIZE, (x) => x < SIZE / 2
    ? [230, 210, 160, 255] : [35, 45, 55, 255]);
  const lower = solid(SIZE, [4, 8, 12, 255]);
  const face = solid(SIZE, [18, 24, 30, 255]);
  const allowed = new Set();
  for (let i = 0; i < upper.data.length; i += 4) {
    allowed.add(Array.from(upper.data.subarray(i, i + 4)).join(','));
    allowed.add(Array.from(face.data.subarray(i, i + 4)).join(','));
    allowed.add(Array.from(lower.data.subarray(i, i + 4)).join(','));
  }
  const chasm = TileForge.renderTile({
    size: SIZE, mask: 0, mode: 'chasm', style: 'cut', rim: 1, depth: 2,
    foreground: upper, background: lower, face, paletteLock: true
  });
  for (let i = 0; i < chasm.data.length; i += 4) {
    assert.equal(allowed.has(Array.from(chasm.data.subarray(i, i + 4)).join(',')), true);
  }
  const wall = TileForge.renderTile({
    size: SIZE, mask: 4 | 16, mode: 'wall', style: 'cut', rim: 0, depth: 4,
    foreground: upper, background: lower, face, paletteLock: true
  });
  assert.equal(rgbaAt(wall, SIZE - 1, SIZE - 1).join(','), '18,24,30,255');
});

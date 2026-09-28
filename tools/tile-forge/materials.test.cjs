const test = require('node:test');
const assert = require('node:assert/strict');
const ForgeMaterials = require('./materials.js');

function solid(size, colour) {
  const data = new Uint8ClampedArray(size * size * 4);
  for (let i = 0; i < size * size; i += 1) data.set(colour, i * 4);
  return { width: size, height: size, data };
}

function pixelAt(image, x, y) {
  const offset = (y * image.width + x) * 4;
  return Array.from(image.data.subarray(offset, offset + 4));
}

function tilePixel(image, size, tileX, tileY, x, y) {
  return pixelAt(image, tileX * size + x, tileY * size + y);
}

const SIZE = 8;
const RED = [220, 40, 30, 255];
const GREEN = [30, 190, 70, 255];
const BLUE = [35, 75, 220, 255];

test('mixed floor-floor, floor-wall, and wall-wall contacts use both source materials', () => {
  const pairs = [
    [{ kind: 'floor', colour: RED }, { kind: 'floor', colour: GREEN }],
    [{ kind: 'floor', colour: RED }, { kind: 'wall', colour: BLUE }],
    [{ kind: 'wall', colour: BLUE }, { kind: 'wall', colour: GREEN }]
  ];

  for (const [left, right] of pairs) {
    const image = ForgeMaterials.renderMap({
      size: SIZE,
      grid: [[0, 1]],
      materials: [
        { kind: left.kind, pixels: solid(SIZE, left.colour) },
        { kind: right.kind, pixels: solid(SIZE, right.colour) }
      ],
      style: 'soft',
      paletteLock: false,
      blend: 3,
      seed: 13
    });

    assert.deepEqual(tilePixel(image, SIZE, 0, 0, 3, 3), left.colour,
      'the center of the first material remains its source texel');
    assert.deepEqual(tilePixel(image, SIZE, 1, 0, 4, 3), right.colour,
      'the center of the second material remains its source texel');
    assert.notDeepEqual(tilePixel(image, SIZE, 0, 0, SIZE - 1, 3), left.colour,
      'the first material edge changes at contact');
    assert.notDeepEqual(tilePixel(image, SIZE, 1, 0, 0, 3), right.colour,
      'the second material edge changes at contact');
  }
});

test('palette-locked pair seams choose one canonical material texel on both sides', () => {
  const first = solid(SIZE, [160, 20, 30, 255]);
  const second = solid(SIZE, [20, 180, 70, 255]);
  const solidImage = ForgeMaterials.renderMap({
    size: SIZE,
    grid: [[0, 1]],
    materials: [
      { kind: 'floor', pixels: first },
      { kind: 'floor', pixels: second }
    ],
    style: 'cut',
    paletteLock: true
  });
  assert.deepEqual(pixelAt(solidImage, SIZE - 1, 3), pixelAt(solidImage, SIZE, 3));
  assert.deepEqual(pixelAt(solidImage, SIZE - 1, 3), [160, 20, 30, 255]);

  const texturedFirst = {
    width: SIZE,
    height: SIZE,
    data: new Uint8ClampedArray(SIZE * SIZE * 4)
  };
  const texturedSecond = solid(SIZE, [20, 180, 70, 255]);
  for (let y = 0; y < SIZE; y += 1) {
    for (let x = 0; x < SIZE; x += 1) {
      texturedFirst.data.set([80 + x + y, 40 + x, 10 + y, 255],
        (y * SIZE + x) * 4);
    }
  }
  const texturedImage = ForgeMaterials.renderMap({
    size: SIZE,
    grid: [[0, 1]],
    materials: [
      { kind: 'floor', pixels: texturedFirst },
      { kind: 'floor', pixels: texturedSecond }
    ],
    style: 'cut',
    paletteLock: true
  });
  /* The lower-ID source's east endpoint is mirrored into the right tile. */
  assert.deepEqual(pixelAt(texturedImage, SIZE - 1, 4), pixelAt(texturedImage, SIZE, 4));
  assert.deepEqual(pixelAt(texturedImage, SIZE - 1, 4), pixelAt(texturedFirst, SIZE - 1, 4));
});

test('palette locking also keeps wall and chasm shading on imported RGBA colours', () => {
  const palette = [
    [190, 150, 90, 255],
    [70, 55, 45, 255],
    [8, 12, 18, 255]
  ];
  const image = ForgeMaterials.renderMap({
    size: SIZE,
    grid: [[0, 1], [2, 0]],
    materials: palette.map((colour, index) => ({
      kind: index === 1 ? 'wall' : index === 2 ? 'chasm' : 'floor',
      pixels: solid(SIZE, colour)
    })),
    style: 'cut',
    paletteLock: true,
    rim: 3,
    depth: 3
  });
  const allowed = new Set(palette.map(colour => colour.join(',')));
  for (let i = 0; i < image.data.length; i += 4) {
    assert.equal(allowed.has(Array.from(image.data.subarray(i, i + 4)).join(',')), true);
  }
});

test('a map made from one material is byte-identical to tiled source pixels', () => {
  const source = { width: SIZE, height: SIZE, data: new Uint8ClampedArray(SIZE * SIZE * 4) };
  for (let y = 0; y < SIZE; y += 1) {
    for (let x = 0; x < SIZE; x += 1) {
      source.data.set([x * 17, y * 19, 80 + x + y, (x + y) % 2 ? 255 : 0],
        (y * SIZE + x) * 4);
    }
  }
  const image = ForgeMaterials.renderMap({
    size: SIZE,
    grid: [[0, 0], [0, 0]],
    materials: [{ kind: 'floor', pixels: source }],
    paletteLock: false,
    seed: 9001
  });
  for (let tileY = 0; tileY < 2; tileY += 1) {
    for (let tileX = 0; tileX < 2; tileX += 1) {
      for (let y = 0; y < SIZE; y += 1) {
        for (let x = 0; x < SIZE; x += 1) {
          assert.deepEqual(tilePixel(image, SIZE, tileX, tileY, x, y),
            pixelAt(source, x, y));
        }
      }
    }
  }
});

test('a solid floor above a chasm keeps its center and gets a dark edge', () => {
  const image = ForgeMaterials.renderMap({
    size: SIZE,
    grid: [[0], [1]],
    materials: [
      { kind: 'chasm', pixels: solid(SIZE, [8, 12, 20, 255]) },
      { kind: 'floor', pixels: solid(SIZE, [210, 170, 90, 255]) }
    ],
    style: 'cut',
    rim: 1,
    seed: 5
  });
  assert.deepEqual(tilePixel(image, SIZE, 0, 1, 3, 4), [210, 170, 90, 255]);
  assert.notDeepEqual(tilePixel(image, SIZE, 0, 1, 3, 0), [210, 170, 90, 255]);
});

test('different materials remain independently visible when a third material touches the corner', () => {
  const image = ForgeMaterials.renderMap({
    size: SIZE,
    grid: [
      [0, 1, 2],
      [1, 3, 2],
      [2, 2, 2]
    ],
    materials: [
      { kind: 'floor', pixels: solid(SIZE, RED) },
      { kind: 'wall', pixels: solid(SIZE, GREEN) },
      { kind: 'floor', pixels: solid(SIZE, BLUE) },
      { kind: 'wall', pixels: solid(SIZE, [230, 180, 40, 255]) }
    ],
    style: 'organic',
    roughness: 2,
    blend: 3,
    paletteLock: false,
    seed: 44
  });
  const centerX = SIZE + 3;
  const centerY = SIZE + 3;
  assert.deepEqual(pixelAt(image, centerX, centerY), [230, 180, 40, 255]);
  assert.notDeepEqual(pixelAt(image, SIZE + 3, SIZE), [230, 180, 40, 255],
    'north contact is rendered even with another material at northeast');
  assert.notDeepEqual(pixelAt(image, SIZE * 2 - 1, SIZE + 3), [230, 180, 40, 255],
    'east contact is rendered independently');
  assert.notDeepEqual(pixelAt(image, SIZE * 2 - 1, SIZE), [230, 180, 40, 255],
    'the concave corner receives diagonal treatment');
});

test('a diagonal chasm marks the solid corner without disturbing its center', () => {
  const floor = [190, 180, 150, 255];
  const image = ForgeMaterials.renderMap({
    size: SIZE,
    grid: [
      [0, 1],
      [1, 1]
    ],
    materials: [
      { kind: 'chasm', pixels: solid(SIZE, [0, 0, 0, 0]) },
      { kind: 'floor', pixels: solid(SIZE, floor) }
    ],
    style: 'organic',
    paletteLock: false,
    rim: 1,
    seed: 7
  });
  assert.deepEqual(tilePixel(image, SIZE, 1, 1, 4, 4), floor);
  assert.notDeepEqual(tilePixel(image, SIZE, 1, 1, 0, 0), floor);
});

test('transparent source alpha participates in edge composition and stays bounded', () => {
  const image = ForgeMaterials.renderMap({
    size: SIZE,
    grid: [[0, 1]],
    materials: [
      { kind: 'floor', pixels: solid(SIZE, [180, 80, 40, 255]) },
      { kind: 'chasm', pixels: solid(SIZE, [30, 40, 60, 0]) }
    ],
    style: 'soft',
    paletteLock: false,
    blend: 2,
    seed: 88
  });
  const edgeAlpha = pixelAt(image, SIZE - 1, 3)[3];
  assert.ok(edgeAlpha > 0 && edgeAlpha < 255);
  for (let i = 3; i < image.data.length; i += 4) {
    assert.ok(image.data[i] >= 0 && image.data[i] <= 255);
  }
});

test('organic maps are deterministic, use no global random state, and stay within a three-pixel edge', () => {
  const input = {
    size: SIZE,
    grid: [[0, 1]],
    materials: [
      { kind: 'floor', pixels: solid(SIZE, RED) },
      { kind: 'wall', pixels: solid(SIZE, BLUE) }
    ],
    style: 'organic',
    blend: 3,
    roughness: 3,
    paletteLock: false,
    seed: 123
  };
  const first = ForgeMaterials.renderMap(input);
  const second = ForgeMaterials.renderMap(input);
  assert.deepEqual(Array.from(first.data), Array.from(second.data));
  assert.deepEqual(pixelAt(first, 2, 3), RED,
    'the source remains untouched outside the maximum three-pixel neighborhood');
  assert.deepEqual(pixelAt(first, SIZE + 5, 3), BLUE,
    'the second source remains untouched outside the maximum three-pixel neighborhood');
});

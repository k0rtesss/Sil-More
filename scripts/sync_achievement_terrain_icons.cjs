/* Copy existing gameplay terrain art into unused atlas cells for book/store icons.
 * Usage: node scripts/sync_achievement_terrain_icons.cjs [path-to-sharp]
 * No source sprite is resized, recoloured, or redrawn.
 */
const assert = require('node:assert/strict');
const path = require('node:path');
const sharp = require(process.argv[2] || 'sharp');
const root = path.resolve(__dirname, '..');
const graf = path.join(root, 'lib/xtra/graf');
const targets = [
  ['anim_water_surface_f0.png', 26],
  ['ice_sheet.png', 27],
];

(async () => {
  const atlas = path.join(graf, '16x16.png');
  const { data: before, info } = await sharp(atlas).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  assert.equal(info.width, 512);
  const after = Buffer.from(before);
  for (const [name, col] of targets) {
    const tile = await sharp(path.join(graf, name)).extract({ left: 0, top: 0, width: 16, height: 16 })
      .ensureAlpha().raw().toBuffer();
    const previous = await sharp(atlas).extract({ left: col * 16, top: 32 * 16, width: 16, height: 16 })
      .ensureAlpha().raw().toBuffer();
    assert(previous.equals(tile) || Array.from({ length: 256 }, (_, i) => previous[i * 4 + 3]).every(a => a === 0),
      `Refusing to overwrite occupied atlas cell 32,${col}`);
    for (let y = 0; y < 16; y++) {
      tile.copy(after, ((32 * 16 + y) * info.width + col * 16) * 4, y * 64, (y + 1) * 64);
    }
  }
  for (let y = 0; y < info.height; y++) for (let x = 0; x < info.width; x++) {
    if (y >= 512 && y < 528 && x >= 416 && x < 448) continue;
    const offset = (y * info.width + x) * 4;
    assert(before.subarray(offset, offset + 4).equals(after.subarray(offset, offset + 4)));
  }
  await sharp(after, { raw: info }).png().toFile(atlas);
  console.log('PASS: two exact gameplay sprite copies; every previously occupied atlas pixel is unchanged.');
})().catch(error => { console.error(error); process.exitCode = 1; });

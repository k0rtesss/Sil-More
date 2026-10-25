/* Copy gameplay terrain art and reproduce the native fleeing status marker
 * in reserved atlas cells for book/store icons.
 * Usage: node scripts/sync_achievement_terrain_icons.cjs [path-to-sharp]
 * Terrain sprites are copied without changes. The status marker uses the
 * native exclamation-mark shape and TERM_VIOLET from src/variable.c.
 */
const assert = require('node:assert/strict');
const path = require('node:path');
const sharp = require(process.argv[2] || 'sharp');
const root = path.resolve(__dirname, '..');
const graf = path.join(root, 'lib/xtra/graf');
const targets = [
  ['anim_water_surface_f0.png', 26],
  ['ice_sheet.png', 27],
  ['anim_lava_flow_f0.png', 28],
  ['native-violet-flee-marker', 29],
];

(async () => {
  const atlas = path.join(graf, '16x16.png');
  const { data: before, info } = await sharp(atlas).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  assert.equal(info.width, 512);
  const after = Buffer.from(before);
  for (const [name, col] of targets) {
    const tile = name === 'native-violet-flee-marker'
      ? await sharp(Buffer.from('<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16">'
          + '<path d="M7 2h3v8H7zm0 10h3v3H7z" fill="black" fill-opacity="0.75"/>'
          + '<path d="M6 1h3v8H6zm0 10h3v3H6z" fill="#a000ff"/></svg>'))
          .ensureAlpha().raw().toBuffer()
      : await sharp(path.join(graf, name)).extract({ left: 0, top: 0, width: 16, height: 16 })
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
    if (y >= 512 && y < 528 && targets.some(([, col]) => x >= col * 16 && x < (col + 1) * 16)) continue;
    const offset = (y * info.width + x) * 4;
    assert(before.subarray(offset, offset + 4).equals(after.subarray(offset, offset + 4)));
  }
  await sharp(after, { raw: info }).png().toFile(atlas);
  console.log(`PASS: ${targets.length - 1} exact terrain copies and native violet fleeing marker; every other atlas pixel is unchanged.`);
})().catch(error => { console.error(error); process.exitCode = 1; });

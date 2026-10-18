#!/usr/bin/env python3
"""Export the native achievement catalogue for Steamworks and Play Console.

Uses existing gameplay tiles, with nearest-neighbor scaling via Node.js/sharp.
Provider IDs are assigned by Google after import; this exporter never invents IDs.
"""
import argparse
import csv
import html
import io
import json
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]

# Points are multiples of five, with harder victories worth more (1000 total).
POINTS = {
    "INTO_THE_DEEP": 10, "LIGHT_IN_DARKNESS": 25,
    "BACK_TO_STARLIGHT": 50, "THREE_LIGHTS_IN_EXILE": 100,
    "THE_SHADOW_FALLS": 100, "THE_QUIET_ROAD": 100,
    "WORD_UNBROKEN": 50, "FORGED_BY_YOUR_HAND": 10,
    "A_NAME_WORTH_REMEMBERING": 25, "SONG_BENEATH_THE_MOUNTAIN": 50,
    "THERE_AND_BACK_AGAIN": 25, "BY_A_THREAD": 25,
    "LIGHT_OF_THE_ELDAR": 10, "THE_LONG_TALE": 100,
    "NO_RINGS_ATTACHED": 15,
    "CABED_EN_ARAS": 15,
    "OFT_EVIL_WILL": 15,
    "THE_HASTY_STROKE": 15,
    "A_FEW_LEAVES": 15,
    "TWO_WEAPONS": 15,
    "FOR_EXTERNAL_USE_ONLY": 15,
    "FLY_YOU_FOOL": 15,
    "KNOCK_WITH_YOUR_HEAD": 15,
    "DOWN_TO_GOBLIN_TOWN": 15,
    "ATHELAS_OUT_OF_STOCK": 15,
    "ANGBAND_MAINTENANCE": 15,
    "THE_ROAD_GOES_EVER_ON": 5,
    "SOMETHING_TOOKISH": 5,
    "FLIES_AND_SPIDERS": 5,
    "NOT_EVEN_CARCHAROTH": 5,
    "THE_FORDS_OF_SIRION": 5,
    "HELCARAXE": 5,
    "FIRE_AND_WATER": 5,
    "THE_FIRES_OF_THANGORODRIM": 5,
    "THROUGH_NAN_DUNGORTHEB": 5,
    "A_JOURNEY_IN_THE_DARK": 5,
    "A_MAP_OF_THE_IRON_HELLS": 5,
    "A_ELBERETH_GILTHONIEL": 5,
    "RELEASE_FROM_BONDAGE": 5,
    "TWO_THEMES_ONE_VOICE": 5,
    "YOU_CANNOT_PASS": 5,
    "FELAGUNDS_ANSWER": 5,
    "A_WILL_OF_YOUR_OWN": 5,
    "HURINS_DEFIANCE": 5,
    "NO_LONGER_BINDING": 5,
    "A_FOOLS_HOPE": 5,
    "A_SINGLE_SPARK": 5,
    "ON_THE_OTHER_HAND": 5,
    "IRON_CLEAVER": 5,
    "OUT_OF_THIN_ORE": 5,
    "CLAD_IN_THE_NIGHT": 5,
    "A_DREAM_UPON_THE_DARK_LORD": 5,
    "NEITHER_VOICE_DIMINISHED": 5,
    "DOOM_CAN_WAIT": 5,
}

# Existing 16x16 gameplay tiles. Coordinates are (row, column), matching the
# T: entries in the named lib/edit template. No new artwork is drawn.
def icon_tiles():
    """Read the same existing-sprite mapping used by the in-game book."""
    source = (ROOT / "src/achievement-icons.h").read_text(encoding="utf-8")
    icons = {}
    pattern = r"^SIL_ACHIEVEMENT_ICON\(([A-Z_]+), (\d+), (\d+), (\d+), (\d+), (\d+)\) /\* (.*?) \*/$"
    for key, row, col, row2, col2, count, origin in re.findall(pattern, source, re.M):
        first, last = (int(row), int(col)), (int(row2), int(col2))
        tiles = [first] * int(count) if first == last else [first, last]
        icons[key] = {"tiles": tiles, "source": origin}
    return icons


ICON_TILES = icon_tiles()

# Google rejects an icon already assigned to another achievement. Preserve the
# Tolkien references and distinguish repeated base sprites with related game art.
# Steam and the in-game book retain the shared primary sprite mapping above.
GOOGLE_ICON_ACCENTS = {
    "THE_ROAD_GOES_EVER_ON": [(32, 19)],  # Traveller and descending stairs.
    "A_WILL_OF_YOUR_OWN": [(5, 18)],  # Ring and shield: resisting hostile will.
    "HURINS_DEFIANCE": [(5, 18)],  # Sword and shield: defiance under attack.
    "A_ELBERETH_GILTHONIEL": [(2, 27)],  # Horn and shining jewel: the Star-kindler.
    "A_JOURNEY_IN_THE_DARK": [(32, 25)],  # Lamp and map: finding a way through.
    "A_MAP_OF_THE_IRON_HELLS": [(32, 26)],  # Map and water: terrain discoveries.
    "RELEASE_FROM_BONDAGE": [(32, 24)],  # Thrall and song: Freedom.
    "IRON_CLEAVER": [(12, 16)],  # Blade and the Silmaril it cuts free.
    "A_DREAM_UPON_THE_DARK_LORD": [(32, 24)],  # Morgoth and Luthien's song.
    "NEITHER_VOICE_DIMINISHED": [(32, 24)],  # Two full-strength musical voices.
}

def catalogue():
    source = (ROOT / "src/achievements.c").read_text(encoding="utf-8")
    block = re.search(r"catalogue\[SIL_ACH_COUNT\]\s*=\s*\{(.*?)\n\};", source, re.S)
    if not block:
        raise ValueError("Cannot find native achievement catalogue")
    literal = r'"(?:[^"\\]|\\.)*"'
    rows = []
    for match in re.finditer(r"\{\s*(" + literal + r")\s*,\s*(" + literal + r")\s*,\s*(" + literal + r")\s*,\s*(true|false)\s*\}", block[1]):
        key, title, description, hidden = (json.loads(value) for value in match.groups())
        rows.append({"key": key, "title": title, "description": description, "hidden": hidden})
    header = (ROOT / "src/achievements.h").read_text(encoding="utf-8")
    enum = re.search(r"typedef enum sil_achievement_id\s*\{(.*?)\}", header, re.S)
    keys = re.findall(r"\bSIL_ACH_([A-Z_]+)\b", enum[1])[:-1]
    if [row["key"] for row in rows] != keys or set(keys) != set(POINTS) or set(keys) != set(ICON_TILES):
        raise ValueError("Native catalogue, enum, point values and icon artwork must match")
    if not all(5 <= value <= 200 and value % 5 == 0 for value in POINTS.values()) or sum(POINTS.values()) > 1000:
        raise ValueError("Invalid Google achievement points")
    return rows


def csv_text(rows):
    stream = io.StringIO(newline="")
    csv.writer(stream, lineterminator="\n").writerows(rows)
    return stream.getvalue()


def export(output, node, sharp):
    rows = catalogue()
    steam_dir = output / "steam"
    play_dir = output / "google-play"
    for folder in (steam_dir, play_dir):
        folder.mkdir(parents=True, exist_ok=True)
    definitions = [["api_name", "setid", "permission", "spoiler", "archived", "progress_stat_name", "progress_stat_min", "progress_stat_max"]]
    localizations = [["api_name", "field", "english"]]
    metadata = []
    mappings = []
    for index, row in enumerate(rows, 1):
        key = row["key"]
        definitions.append([key, "", "Client", str(row["hidden"]).lower(), "false", "", 0, 0])
        localizations.extend([[key, "name", row["title"]], [key, "description", row["description"]]])
        # Google disallows commas in these fields even when CSV-quoted.
        description = row["description"].replace(", 20 trained Song and", " and 20 trained Song with").replace(", before", " before")
        title = row["title"].replace(",", "")
        description = description.replace(",", ";")
        if len(title) > 100 or len(description) > 500:
            raise ValueError("Invalid Google metadata for " + key)
        metadata.append([title, description, "False", "", "Hidden" if row["hidden"] else "Revealed", POINTS[key], index])
        mappings.append([title, key + ".png"])
    (steam_dir / "5442160-achievements-definitions.csv").write_text(csv_text(definitions), encoding="utf-8")
    (steam_dir / "5442160-achievements-localization.csv").write_text(csv_text(localizations), encoding="utf-8")
    (play_dir / "AchievementsMetadata.csv").write_text(csv_text(metadata), encoding="utf-8")
    (play_dir / "AchievementsIconsMappings.csv").write_text(csv_text(mappings), encoding="utf-8")
    (output / "catalogue.json").write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")
    atlas = ROOT / "lib/xtra/graf/16x16.png"
    manifest = {
        "atlas": "lib/xtra/graf/16x16.png",
        "atlas_sha256": __import__("hashlib").sha256(atlas.read_bytes()).hexdigest(),
        "icons": ICON_TILES,
        "google_icon_accents": GOOGLE_ICON_ACCENTS,
    }
    (output / "icon-sources.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    render = r'''
const fs = require('fs'), path = require('path');
const sharp = require(process.argv[1]);
const root = process.argv[2], atlas = process.argv[3];
(async () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(root, 'icon-sources.json')));
  for (const [key, info] of Object.entries(manifest.icons)) {
    for (const [folder, size, singleScale] of [['steam',256,12],['google-play',512,24]]) {
      const tiles = folder === 'google-play'
        ? [...info.tiles, ...(manifest.google_icon_accents[key] || [])] : info.tiles;
      const scale = tiles.length === 1 ? singleScale : Math.floor(singleScale / tiles.length);
      const width = 16 * scale, total = width * tiles.length;
      const layers = [];
      for (const [i, [row, col]] of tiles.entries()) {
        const tile = await sharp(atlas).extract({left:col*16, top:row*16, width:16, height:16})
          .resize(width,width,{kernel:'nearest'}).png().toBuffer();
        layers.push({input:tile,left:(size-total)/2+i*width,top:(size-width)/2});
      }
      const name = key + (folder === 'steam' ? '_ACHIEVED.png' : '.png');
      await sharp({create:{width:size,height:size,channels:3,background:'#000000'}})
        .composite(layers).removeAlpha().png().toFile(path.join(root,folder,name));
    }
  }
})().catch(error => { console.error(error.message); process.exit(1); });
'''
    subprocess.run([node, "-e", render, sharp, str(output), str(atlas)], check=True)
    google_hashes = [__import__("hashlib").sha256((play_dir / (row["key"] + ".png")).read_bytes()).hexdigest()
                     for row in rows]
    if len(set(google_hashes)) != len(rows):
        raise ValueError("Every Google achievement must have a distinct icon")
    import base64
    cards = []
    for index, row in enumerate(rows):
        x, y = (index % 4) * 300, (index // 4) * 275
        encoded = base64.b64encode((play_dir / (row["key"] + ".png")).read_bytes()).decode("ascii")
        icon = f'<image x="{x + 62}" y="{y + 20}" width="176" height="176" href="data:image/png;base64,{encoded}" style="image-rendering:pixelated"/>'
        words = row["title"].split()
        lines, line = [], ""
        for word in words:
            if len(line + " " + word) > 27:
                lines.append(line)
                line = word
            else:
                line = (line + " " + word).strip()
        lines.append(line)
        label = ''.join(f'<tspan x="{x + 150}" dy="{0 if i == 0 else 24}">{html.escape(text)}</tspan>' for i, text in enumerate(lines))
        cards.append(icon + f'<text x="{x + 150}" y="{y + 224}" text-anchor="middle" fill="#f2e7d1" font-family="sans-serif" font-size="19">{label}</text>')
    height = ((len(rows) + 3) // 4) * 275
    sheet = f'<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="{height}"><rect width="1200" height="{height}" fill="#0d1420"/>' + ''.join(cards) + '</svg>'
    (output / "preview.svg").write_text(sheet, encoding="utf-8")
    subprocess.run([node, "-e", "require(process.argv[1])(process.argv[2]).png().toFile(process.argv[3]).catch(e=>{console.error(e.message);process.exit(1)})", sharp, str(output / "preview.svg"), str(output / "preview.png")], check=True)
    bundle = output / "google-play-achievements.zip"
    with zipfile.ZipFile(bundle, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(play_dir.iterdir()):
            if path.suffix.lower() in (".csv", ".png"):
                if path.stat().st_size >= 1000000:
                    raise ValueError("Google import files must be under 1 MB")
                archive.write(path, path.name)
    # Existing projects must append only the new rows; importing all rows creates
    # duplicates instead of preserving the original opaque provider IDs.
    first_new = next(i for i, row in enumerate(rows) if row["key"] == "THE_LONG_TALE") + 1
    expansion = output / "google-play-new-achievements.zip"
    with zipfile.ZipFile(expansion, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("AchievementsMetadata.csv", csv_text(metadata[first_new:]))
        archive.writestr("AchievementsIconsMappings.csv", csv_text(mappings[first_new:]))
        for row in rows[first_new:]:
            path = play_dir / (row["key"] + ".png")
            archive.write(path, path.name)
    print(f"Exported {len(rows)} achievements ({sum(POINTS.values())} Google points) to {output}")
    print(f"Steam: upload the two CSV files and all *_ACHIEVED.png icons in {steam_dir}")
    print(f"Google Play new project: import {bundle}")
    print(f"Google Play existing project: append only {expansion}; edit existing metadata by existing ID")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "scripts/output/achievement-stores")
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--sharp", default="sharp", help="sharp package name or absolute module path")
    args = parser.parse_args()
    if not args.node:
        parser.error("Node.js is required; supply --node with its executable path")
    export(args.output.resolve(), args.node, args.sharp)


if __name__ == "__main__":
    main()

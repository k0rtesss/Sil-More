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

# Points are multiples of five, with harder victories worth more (680 total).
POINTS = {
    "INTO_THE_DEEP": 10, "LIGHT_IN_DARKNESS": 25,
    "BACK_TO_STARLIGHT": 50, "THREE_LIGHTS_IN_EXILE": 100,
    "THE_SHADOW_FALLS": 100, "THE_QUIET_ROAD": 100,
    "WORD_UNBROKEN": 50, "FORGED_BY_YOUR_HAND": 10,
    "A_NAME_WORTH_REMEMBERING": 25, "SONG_BENEATH_THE_MOUNTAIN": 50,
    "THERE_AND_BACK_AGAIN": 25, "BY_A_THREAD": 25,
    "LIGHT_OF_THE_ELDAR": 10, "THE_LONG_TALE": 100,
}

# Existing 16x16 gameplay tiles. Coordinates are (row, column), matching the
# T: entries in the named lib/edit template. No new artwork is drawn.
ICON_TILES = {
    "INTO_THE_DEEP": {"tiles": [(0, 25)], "source": "terrain.txt N:81 down staircase"},
    "LIGHT_IN_DARKNESS": {"tiles": [(5, 28)], "source": "object.txt N:20 Massive Iron Crown"},
    "BACK_TO_STARLIGHT": {"tiles": [(12, 16)], "source": "object.txt N:131 Silmaril"},
    "THREE_LIGHTS_IN_EXILE": {"tiles": [(12, 16)] * 3, "source": "object.txt N:131 Silmaril, three copies"},
    "THE_SHADOW_FALLS": {"tiles": [(10, 18)], "source": "monster.txt N:251 Morgoth"},
    "THE_QUIET_ROAD": {"tiles": [(5, 2)], "source": "object.txt N:106 Cloak"},
    "WORD_UNBROKEN": {"tiles": [(6, 20)], "source": "flavor.txt N:1 Serpentine Ring of Barahir"},
    "FORGED_BY_YOUR_HAND": {"tiles": [(15, 0)], "source": "terrain.txt N:69 forge"},
    "A_NAME_WORTH_REMEMBERING": {"tiles": [(6, 6)], "source": "object.txt N:70 Star-Iron Greatsword"},
    "SONG_BENEATH_THE_MOUNTAIN": {"tiles": [(2, 22)], "source": "flavor.txt N:101 Silver horn"},
    "THERE_AND_BACK_AGAIN": {"tiles": [(0, 22)], "source": "terrain.txt N:80 up staircase"},
    "BY_A_THREAD": {"tiles": [(3, 0)], "source": "flavor.txt N:193 Miruvor"},
    "LIGHT_OF_THE_ELDAR": {"tiles": [(2, 31)], "source": "object.txt N:130 Feanorian Lamp"},
    "THE_LONG_TALE": {"tiles": [(32, 25)], "source": "object.txt N:492 Scroll of the First Sentence"},
}


def catalogue():
    source = (ROOT / "src/achievements.c").read_text(encoding="utf-8")
    block = re.search(r"catalogue\[SIL_ACH_COUNT\]\s*=\s*\{(.*?)\n\};", source, re.S)
    if not block:
        raise ValueError("Cannot find native achievement catalogue")
    literal = r'"(?:[^"\\]|\\.)*"'
    rows = []
    for match in re.finditer(r"\{\s*(" + literal + r")\s*,\s*(" + literal + r")\s*,\s*(" + literal + r")\s*\}", block[1]):
        key, title, description = (json.loads(value) for value in match.groups())
        rows.append({"key": key, "title": title, "description": description})
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
        definitions.append([key, "", "Client", "false", "false", "", 0, 0])
        localizations.extend([[key, "name", row["title"]], [key, "description", row["description"]]])
        # Google disallows commas in these fields even when CSV-quoted.
        description = row["description"].replace(", 20 trained Song and", " and 20 trained Song with").replace(", before", " before")
        if "," in row["title"] or "," in description or len(row["title"]) > 100 or len(description) > 500:
            raise ValueError("Invalid Google metadata for " + key)
        metadata.append([row["title"], description, "False", "", "Revealed", POINTS[key], index])
        mappings.append([row["title"], key + ".png"])
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
    }
    (output / "icon-sources.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    render = r'''
const fs = require('fs'), path = require('path');
const sharp = require(process.argv[1]);
const root = process.argv[2], atlas = process.argv[3];
(async () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(root, 'icon-sources.json')));
  for (const [key, info] of Object.entries(manifest.icons)) {
    for (const [folder, size, singleScale, tripleScale] of [['steam',256,12,4],['google-play',512,24,8]]) {
      const scale = info.tiles.length === 1 ? singleScale : tripleScale;
      const width = 16 * scale, total = width * info.tiles.length;
      const layers = [];
      for (const [i, [row, col]] of info.tiles.entries()) {
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
    sheet = '<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="1100"><rect width="1200" height="1100" fill="#0d1420"/>' + ''.join(cards) + '</svg>'
    (output / "preview.svg").write_text(sheet, encoding="utf-8")
    subprocess.run([node, "-e", "require(process.argv[1])(process.argv[2]).png().toFile(process.argv[3]).catch(e=>{console.error(e.message);process.exit(1)})", sharp, str(output / "preview.svg"), str(output / "preview.png")], check=True)
    bundle = output / "google-play-achievements.zip"
    with zipfile.ZipFile(bundle, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(play_dir.iterdir()):
            if path.suffix.lower() in (".csv", ".png"):
                if path.stat().st_size >= 1000000:
                    raise ValueError("Google import files must be under 1 MB")
                archive.write(path, path.name)
    print(f"Exported {len(rows)} achievements ({sum(POINTS.values())} Google points) to {output}")
    print(f"Steam: upload the two CSV files and all *_ACHIEVED.png icons in {steam_dir}")
    print(f"Google Play: import {bundle}")


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

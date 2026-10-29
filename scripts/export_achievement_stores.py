#!/usr/bin/env python3
"""Export the native achievement catalogue for Steam, Google Play and Game Center.

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

# Published point values stay fixed. Reserve room within Google's 2000-point cap.
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
    "THE_HIGH_CAPTAIN_FALLS": 25,
    "DAGNIR_GLAURUNGA": 25,
    "THE_ABHORRED": 25,
    "UNLIGHT_UNWOVEN": 25,
    "THE_DRAGON_HOST_BROKEN": 25,
    "THE_RED_MAW": 25,
    "LORD_OF_THE_FOUNTAIN": 50,
    "MASTER_OF_DOOM": 50,
    "FOR_IDRIL_AND_EARENDIL": 50,
    "BEFORE_THE_RISING_OF_THE_SUN": 50,
    "TULKAS_LAST_LAUGH": 10,
    "AULES_APPRENTICE": 10,
    "THE_DOOMSMANS_DUE": 10,
    "PITY_FOR_THE_FORSAKEN": 10,
    "THE_GREAT_HUNT": 10,
    "A_STAR_AGAINST_THE_BALROG": 10,
    "NO_STEPS_REQUIRED": 25,
    "THERE_AND_ALMOST_BACK_AGAIN": 5,
    "THE_HALLS_OF_UTUMNO": 10,
    "WHERE_GROND_WAS_WROUGHT": 10,
    "NOT_THE_DOOM_OF_THE_SILMARILS": 10,
    "THE_KING_WHO_KEPT_HIS_OATH": 10,
    "THURINGWETHIL_I_AM": 10,
    "HIS_DEATH_AND_FINAL_END": 10,
    "SEVEN_TIMES_HE_CRIED": 10,
    "THE_CAT_AND_THE_NECROMANCER": 10,
    "EVER_FOUND_AGAIN": 10,
    "THE_GOLDEN_FLOWER_STANDS": 10,
    "BELEGOST_REMEMBERS": 10,
    "BITER_AND_BEATER": 10,
    "AT_MOST_SEVEN": 10,
    "HEAVY_IS_THE_STOLEN_CROWN": 10,
    "THESE_THINGS_ARE_MINE": 10,
    "THE_OATH_HE_SHOULD_HAVE_SWORN": 10,
    "HIS_FATHERS_LIGHT": 10,
    "HEIRLOOMS_OF_THE_HIDDEN_KING": 10,
    "THE_RING_MAKER_OVERRULED": 10,
    "NO_PHIAL_REQUIRED": 10,
    "THREE_UNTOUCHED": 10,
    "A_BURGLARS_BUSINESS": 10,
    "STRONGBOW": 10,
    "ONE_JEWEL_IS_ENOUGH": 10,
    "A_DESPERATE_VICTORY": 25,
    "THEIR_WORK_HAD_NO_RIVAL": 50,
    "FIND_NAUGLAMIR": 15,
    "FIND_GURTHANG": 25,
    "FIND_MELKOR": 25,
    "FIND_GOTHMOG_AXE": 25,
    "FIND_ANGRIST": 10,
    "FIND_BELTHRONDING": 10,
    "FIND_IDRIL_ROBE": 10,
    "FIND_CELEBRIMBOR_GLOVES": 10,
    "FIND_MELIAN_RING": 10,
    "FIND_ULMO_CLOAK": 10,
    "FIND_FINGOLFIN_SHIELD": 10,
    "FIND_TURGON_STAFF": 10,
    "SWIFT_AS_THE_WIND": 10,
    "TENFOLD_DOOM": 25,
}

# Apple's independent schedule leaves room within its 1,000-point budget.
# Once live these values, like the IDs, must stay fixed. Google points above
# belong to already published achievements and must never be rescaled.
APPLE_POINT_TIERS = {5: 5, 10: 5, 15: 5, 25: 10, 50: 20, 100: 40}
APPLE_POINTS = {key: APPLE_POINT_TIERS[value] for key, value in POINTS.items()}
STORE_ACHIEVEMENT_LIMIT = 100


def apple_identifier(key):
    source = (ROOT / "src/achievements-game-center.h").read_text(encoding="utf-8")
    prefix = re.search(r'^#define SIL_GAME_CENTER_ID_PREFIX "([^"]+)"', source, re.M)
    if not prefix:
        raise ValueError("Missing permanent Game Center ID prefix")
    identifier = prefix[1] + key
    if len(identifier) > 100 or not re.fullmatch(r"[A-Za-z0-9_.]+", identifier):
        raise ValueError("Invalid Game Center identifier for " + key)
    return identifier

# Existing 16x16 gameplay tiles. Coordinates are (row, column), matching the
# T: entries in the named lib/edit template. No new artwork is drawn.
def icon_tiles():
    """Read the same existing-sprite mapping used by the in-game book."""
    source = (ROOT / "src/achievement-icons.h").read_text(encoding="utf-8")
    icons = {}
    pattern = r"^SIL_ACHIEVEMENT_ICON\(([A-Z_]+), (\d+), (\d+), (\d+), (\d+), (\d+)\) /\* (.*?) \*/$"
    rectangle = r"^SIL_ACHIEVEMENT_ICON_RECT\(([A-Z_]+), (\d+), (\d+), (\d+)\) /\* (.*?) \*/$"
    for line in source.splitlines():
        match = re.fullmatch(pattern, line)
        if match:
            key, row, col, row2, col2, count, origin = match.groups()
            first, last = (int(row), int(col)), (int(row2), int(col2))
            tiles = [first] * int(count) if first == last else [first, last]
            icons[key] = {"tiles": tiles, "source": origin}
        else:
            match = re.fullmatch(rectangle, line)
            if match:
                key, row, col, size, origin = match.groups()
                icons[key] = {"tiles": [(int(row), int(col))],
                              "source": origin, "tile_size": int(size)}
    return icons


ICON_TILES = icon_tiles()

# Google rejects an icon already assigned to another achievement. Preserve the
# Tolkien references and distinguish repeated base sprites with related game art.
# Steam and the in-game book use the shared sprite compositions above.
GOOGLE_ICON_ACCENTS = {
    "THE_ROAD_GOES_EVER_ON": [(32, 19)],  # Traveller and descending stairs.
    "A_MAP_OF_THE_IRON_HELLS": [(32, 26)],  # Map and water: terrain discoveries.
    "IRON_CLEAVER": [(12, 16)],  # Blade and the Silmaril it cuts free.
    "A_DREAM_UPON_THE_DARK_LORD": [(32, 24)],  # Morgoth and Luthien's song.
    "FOR_IDRIL_AND_EARENDIL": [(5, 18)],  # Tuor protects his family: Maeglin, blade and shield.
    "FIND_MELIAN_RING": [(32, 0)],  # Ring and the existing discovery symbol: Melian's foresight.
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
    if not all(5 <= value <= 200 and value % 5 == 0 for value in POINTS.values()) or sum(POINTS.values()) > 2000:
        raise ValueError("Invalid Google achievement points")
    return rows


def csv_text(rows):
    stream = io.StringIO(newline="")
    csv.writer(stream, lineterminator="\n").writerows(rows)
    return stream.getvalue()


def display_order():
    source = (ROOT / "src/achievement-order.h").read_text(encoding="utf-8")
    keys = re.findall(r'^SIL_ACHIEVEMENT_ORDER\(([A-Z_]+), "[^"]+"\)', source, re.M)
    native = [row["key"] for row in catalogue()]
    if len(keys) != len(native) or len(set(keys)) != len(keys) or set(keys) != set(native):
        raise ValueError("Achievement display order must contain every stable key exactly once")
    return {key: position for position, key in enumerate(keys, 1)}


def steam_excluded_keys():
    source = (ROOT / "src/achievement-steam-exclusions.h").read_text(encoding="utf-8")
    keys = set(re.findall(r"^SIL_ACH_STEAM_EXCLUDED\(([A-Z_]+)\)", source, re.M))
    if not keys <= {row["key"] for row in catalogue()}:
        raise ValueError("Steam exclusions must refer to stable achievement keys")
    return keys


def store_readiness(rows, steam_excluded, apple_excluded):
    keys = {row["key"] for row in rows}
    if not steam_excluded <= keys or not apple_excluded <= keys:
        raise ValueError("Store exclusions must refer to stable achievement keys")
    readiness = {}
    for provider, excluded in (("steam", steam_excluded), ("apple-game-center", apple_excluded)):
        selected = keys - excluded
        remaining = max(0, len(selected) - STORE_ACHIEVEMENT_LIMIT)
        points = sum(APPLE_POINTS[key] for key in selected) if provider == "apple-game-center" else None
        readiness[provider] = {
            "achievement_count": len(selected),
            "achievement_limit": STORE_ACHIEVEMENT_LIMIT,
            "additional_exclusions_required": remaining,
            "ready_for_submission": remaining == 0 and (points is None or points <= 1000),
            "excluded_keys": sorted(excluded),
        }
        if points is not None:
            readiness[provider].update(point_total=points, point_limit=1000)
    return readiness


def apple_catalogue(rows, excluded, order):
    entries = []
    for row in sorted(rows, key=lambda row: order[row["key"]]):
        key = row["key"]
        if key in excluded:
            continue
        if len(row["title"]) > 100 or len(row["description"]) > 500:
            raise ValueError("Invalid Game Center localization for " + key)
        entries.append({
            "key": key,
            "referenceName": key,
            "achievementID": apple_identifier(key),
            "points": APPLE_POINTS[key],
            "hidden": row["hidden"],
            "repeatable": False,
            "displayOrder": order[key],
            "localizations": [{
                "locale": "en-US",
                "name": row["title"],
                "beforeEarnedDescription": row["description"],
                "afterEarnedDescription": row["description"],
                "image": key + ".png",
            }],
        })
    return entries


def export(output, node, sharp, apple_excluded=None, require_ready=False):
    rows = catalogue()
    excluded = steam_excluded_keys()
    # Start the Apple review from the existing Steam selection. No additional
    # removals are guessed to make a draft fit either store's entry limit.
    apple_excluded = excluded if apple_excluded is None else set(apple_excluded)
    order = display_order()
    readiness = store_readiness(rows, excluded, apple_excluded)
    if require_ready and not all(item["ready_for_submission"] for item in readiness.values()):
        raise ValueError("Steam/Game Center selection is not ready; choose the remaining exclusions first")
    output.mkdir(parents=True, exist_ok=True)
    (output / "store-readiness.json").write_text(json.dumps(readiness, indent=2) + "\n", encoding="utf-8")
    steam_folder = "steam" if readiness["steam"]["ready_for_submission"] else "steam-draft"
    apple_folder = "apple-game-center" if readiness["apple-game-center"]["ready_for_submission"] else "apple-game-center-draft"
    steam_dir = output / steam_folder
    play_dir = output / "google-play"
    apple_dir = output / apple_folder
    for folder in (steam_dir, play_dir, apple_dir):
        folder.mkdir(parents=True, exist_ok=True)
    if steam_folder != "steam":
        # Invalidate only the two generated upload CSVs from a previous export.
        for name in ("5442160-achievements-definitions.csv", "5442160-achievements-localization.csv"):
            (output / "steam" / name).unlink(missing_ok=True)
    if apple_folder != "apple-game-center":
        (output / "apple-game-center/achievements.json").unlink(missing_ok=True)
    # Remove obsolete Steam upload artwork while retaining Google artwork.
    for key in excluded:
        (steam_dir / (key + "_ACHIEVED.png")).unlink(missing_ok=True)
    definitions = [["api_name", "setid", "permission", "spoiler", "archived", "progress_stat_name", "progress_stat_min", "progress_stat_max"]]
    localizations = [["api_name", "field", "english"]]
    metadata = []
    mappings = []
    for index, row in enumerate(rows, 1):
        key = row["key"]
        if key not in excluded:
            definitions.append([key, "", "Client", str(row["hidden"]).lower(), "false", "", 0, 0])
            localizations.extend([[key, "name", row["title"]], [key, "description", row["description"]]])
        # Google disallows commas in these fields even when CSV-quoted.
        description = row["description"].replace(", 20 trained Song and", " and 20 trained Song with").replace(", before", " before")
        title = row["title"].replace(",", "")
        description = description.replace(",", ";")
        if len(title) > 100 or len(description) > 500:
            raise ValueError("Invalid Google metadata for " + key)
        metadata.append([title, description, "False", "", "Hidden" if row["hidden"] else "Revealed", POINTS[key], order[key]])
        mappings.append([title, key + ".png"])
    definitions[1:] = sorted(definitions[1:], key=lambda row: order[row[0]])
    localizations[1:] = sorted(localizations[1:], key=lambda row: order[row[0]])
    (steam_dir / "5442160-achievements-definitions.csv").write_text(csv_text(definitions), encoding="utf-8")
    (steam_dir / "5442160-achievements-localization.csv").write_text(csv_text(localizations), encoding="utf-8")
    (play_dir / "AchievementsMetadata.csv").write_text(csv_text(metadata), encoding="utf-8")
    (play_dir / "AchievementsIconsMappings.csv").write_text(csv_text(mappings), encoding="utf-8")
    (output / "catalogue.json").write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")
    (output / "steam-catalogue.json").write_text(json.dumps(
        [row for row in rows if row["key"] not in excluded], indent=2) + "\n", encoding="utf-8")
    apple_entries = apple_catalogue(rows, apple_excluded, order)
    (apple_dir / "achievements.json").write_text(json.dumps({
        "format": "sil-more-game-center-catalogue-v1",
        "readyForSubmission": readiness["apple-game-center"]["ready_for_submission"],
        "note": "Review metadata for App Store Connect; this JSON is not an Apple bulk-import format.",
        "achievements": apple_entries,
    }, indent=2) + "\n", encoding="utf-8")
    (output / "display-order.json").write_text(json.dumps(
        [row["key"] for row in sorted(rows, key=lambda row: order[row["key"]])], indent=2) + "\n",
        encoding="utf-8")
    atlas = ROOT / "lib/xtra/graf/16x16.png"
    manifest = {
        "atlas": "lib/xtra/graf/16x16.png",
        "atlas_sha256": __import__("hashlib").sha256(atlas.read_bytes()).hexdigest(),
        "icons": ICON_TILES,
        "google_icon_accents": GOOGLE_ICON_ACCENTS,
        "steam_excluded": sorted(excluded),
        "apple_excluded": sorted(apple_excluded),
        "steam_folder": steam_folder,
        "apple_folder": apple_folder,
    }
    (output / "icon-sources.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    render = r'''
const fs = require('fs'), path = require('path');
const sharp = require(process.argv[1]);
const root = process.argv[2], atlas = process.argv[3];
(async () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(root, 'icon-sources.json')));
  for (const [key, info] of Object.entries(manifest.icons)) {
    for (const [folder, size, singleScale] of [[manifest.steam_folder,256,12],['google-play',512,24],[manifest.apple_folder,1024,48]]) {
      const isSteam = folder === manifest.steam_folder, isApple = folder === manifest.apple_folder;
      if (isSteam && manifest.steam_excluded.includes(key)) continue;
      if (isApple && manifest.apple_excluded.includes(key)) continue;
      const sourceSize = info.tile_size || 16;
      const tiles = folder === 'google-play' && sourceSize === 16
        ? [...info.tiles, ...(manifest.google_icon_accents[key] || [])] : info.tiles;
      const scale = tiles.length === 1 ? singleScale : Math.floor(singleScale / tiles.length);
      const width = 16 * scale, total = width * tiles.length;
      const layers = [];
      for (const [i, [row, col]] of tiles.entries()) {
        const tile = await sharp(atlas).extract({left:col*16, top:row*16, width:sourceSize, height:sourceSize})
          .resize(width,width,{kernel:'nearest'}).png().toBuffer();
        layers.push({input:tile,left:(size-total)/2+i*width,top:(size-width)/2});
      }
      const name = key + (isSteam ? '_ACHIEVED.png' : '.png');
      await sharp({create:{width:size,height:size,channels:3,background:'#000000'}})
        .composite(layers).removeAlpha().toColourspace('srgb').withMetadata({density:72})
        .png().toFile(path.join(root,folder,name));
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
    for index, row in enumerate(sorted(rows, key=lambda row: order[row["key"]])):
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
    configured = dict(re.findall(r"^SIL_MORE_PLAY_ACH_([A-Z_]+)=(\S*)\s*$",
                                (ROOT / "android/play-games.properties").read_text(), re.M))
    new_indexes = [i for i, row in enumerate(rows) if not configured.get(row["key"])]
    expansion = output / "google-play-new-achievements.zip"
    with zipfile.ZipFile(expansion, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("AchievementsMetadata.csv", csv_text([metadata[i] for i in new_indexes]))
        archive.writestr("AchievementsIconsMappings.csv", csv_text([mappings[i] for i in new_indexes]))
        for i in new_indexes:
            row = rows[i]
            path = play_dir / (row["key"] + ".png")
            archive.write(path, path.name)
    print(f"Exported {len(rows) - len(excluded)} Steam and {len(rows)} Google achievements ({sum(POINTS.values())} Google points) to {output}")
    for provider, item in readiness.items():
        if not item["ready_for_submission"]:
            print(f"DRAFT {provider}: {item['additional_exclusions_required']} additional exclusions required; do not upload")
    print(f"Steam {'upload' if readiness['steam']['ready_for_submission'] else 'review draft'}: {steam_dir}")
    print(f"Game Center {'metadata' if readiness['apple-game-center']['ready_for_submission'] else 'review draft'}: {apple_dir} ({readiness['apple-game-center']['point_total']} Apple points)")
    print(f"Google Play new project: import {bundle}")
    print(f"Google Play existing project: append only {expansion}; edit existing metadata by existing ID")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "scripts/output/achievement-stores")
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--sharp", default="sharp", help="sharp package name or absolute module path")
    parser.add_argument("--apple-exclusions", type=Path,
                        help="JSON array of excluded native keys; defaults to the existing Steam exclusions")
    parser.add_argument("--require-ready", action="store_true", help="Fail before exporting if either 100-entry store selection is unfinished")
    args = parser.parse_args()
    if not args.node:
        parser.error("Node.js is required; supply --node with its executable path")
    apple_excluded = None if args.apple_exclusions is None else json.loads(args.apple_exclusions.read_text(encoding="utf-8"))
    if apple_excluded is not None and (not isinstance(apple_excluded, list)
                                       or not all(isinstance(key, str) for key in apple_excluded)):
        parser.error("--apple-exclusions must contain a JSON array of stable achievement keys")
    export(args.output.resolve(), args.node, args.sharp, apple_excluded, args.require_ready)


if __name__ == "__main__":
    main()

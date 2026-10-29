#!/usr/bin/env python3
"""Check real store metadata and raster exports, including unfinished selections."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import struct
from unittest.mock import patch

from export_achievement_stores import (
    APPLE_POINTS, POINTS, apple_catalogue, catalogue, display_order, export,
    steam_excluded_keys, store_readiness, game_center_excluded_keys,
    game_center_selection_exclusions,
)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/achievement-store-export-check"


def png_properties(path):
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    width, height, depth, color = struct.unpack(">IIBB", data[16:26])
    density = None
    offset = 8
    while offset < len(data):
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        kind = data[offset + 4:offset + 8]
        if kind == b"pHYs":
            density = struct.unpack(">IIB", data[offset + 8:offset + 8 + length])
        offset += length + 12
    return width, height, depth, color, density


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--sharp", default="sharp")
    args = parser.parse_args()
    rows = catalogue()
    keys = {row["key"] for row in rows}
    assert len(rows) == 112
    assert {"SWIFT_AS_THE_WIND", "TENFOLD_DOOM"} <= keys
    assert sum(POINTS.values()) == 1960, "Published Google points changed"
    assert sum(APPLE_POINTS.values()) <= 1000
    excluded = steam_excluded_keys()
    explicitly_excluded = {"ANGBAND_MAINTENANCE", "FOR_IDRIL_AND_EARENDIL"}
    assert len(excluded) == 12 and explicitly_excluded <= excluded
    apple_excluded = game_center_selection_exclusions(excluded)
    assert game_center_excluded_keys() == explicitly_excluded
    assert apple_excluded == excluded
    assert game_center_selection_exclusions(excluded, set()) == explicitly_excluded
    status = store_readiness(rows, excluded, apple_excluded)
    for item in status.values():
        assert item["achievement_count"] == 100 and item["additional_exclusions_required"] == 0
        assert item["ready_for_submission"]
    assert status["apple-game-center"]["point_total"] == 825
    order = display_order()
    apple = apple_catalogue(rows, apple_excluded, order)
    assert len({item["achievementID"] for item in apple}) == 100
    assert {item["key"] for item in apple} == keys - apple_excluded
    for item in apple:
        assert item["achievementID"] == "com.silqh.silmore.achievement." + item["key"]
        assert 0 < item["points"] <= 100 and not item["repeatable"]
        assert item["localizations"][0]["locale"] == "en-US"
    # A synthetic 101-entry selection exercises the draft and release guard
    # without changing the user's selected exclusions.
    fixture_excluded = excluded - {"THE_FORDS_OF_SIRION"}
    draft = store_readiness(rows, fixture_excluded,
                            game_center_selection_exclusions(fixture_excluded))
    assert all(not item["ready_for_submission"] and item["achievement_count"] == 101
               for item in draft.values())
    try:
        store_readiness(rows, excluded, {"NOT_A_NATIVE_KEY"})
        raise AssertionError("Unknown key accepted")
    except ValueError:
        pass
    OUT.mkdir(parents=True, exist_ok=True)
    blocked = OUT / "require-ready-must-not-write"
    try:
        export(blocked, args.node, args.sharp, apple_excluded=fixture_excluded,
               require_ready=True)
        raise AssertionError("Unfinished selections accepted for release")
    except ValueError as error:
        assert "not ready" in str(error)
        assert not blocked.exists()
    # Invalidate stale upload CSVs from an earlier ready export, without
    # deleting unrelated files. The new candidates live in draft folders.
    previous = OUT / "steam"
    previous.mkdir(exist_ok=True)
    stale = previous / "5442160-achievements-definitions.csv"
    stale.write_text("stale upload", encoding="utf-8")
    sentinel = previous / "preserve.txt"
    sentinel.write_text("preserve", encoding="utf-8")
    with patch("export_achievement_stores.steam_excluded_keys", return_value=fixture_excluded):
        export(OUT, args.node, args.sharp)
    assert not stale.exists() and sentinel.read_text(encoding="utf-8") == "preserve"
    status_file = json.loads((OUT / "store-readiness.json").read_text(encoding="utf-8"))
    assert status_file == draft
    assert (OUT / "apple-game-center-draft/achievements.json").exists()
    # A previously rendered excluded icon must not remain in either final folder.
    stale_steam_icon = previous / "FOR_IDRIL_AND_EARENDIL_ACHIEVED.png"
    stale_steam_icon.write_bytes(b"obsolete")
    apple_dir = OUT / "apple-game-center"
    apple_dir.mkdir(exist_ok=True)
    stale_apple_icon = apple_dir / "FOR_IDRIL_AND_EARENDIL.png"
    stale_apple_icon.write_bytes(b"obsolete")
    export(OUT, args.node, args.sharp, require_ready=True)
    assert not stale_steam_icon.exists() and not stale_apple_icon.exists()
    assert sentinel.read_text(encoding="utf-8") == "preserve"
    status_file = json.loads((OUT / "store-readiness.json").read_text(encoding="utf-8"))
    assert status_file == status
    definitions = (previous / "5442160-achievements-definitions.csv").read_text(encoding="utf-8")
    assert "FOR_IDRIL_AND_EARENDIL" not in definitions
    exported = json.loads((apple_dir / "achievements.json").read_text(encoding="utf-8"))
    assert exported["readyForSubmission"] and exported["achievements"] == apple
    for item in apple:
        image = apple_dir / item["localizations"][0]["image"]
        width, height, depth, color, density = png_properties(image)
        assert (width, height, depth, color) == (1024, 1024, 8, 2), (image, width, height, depth, color)
        assert density is not None and density[2] == 1 and density[0] >= 2834 and density[1] >= 2834
    for key in ("SWIFT_AS_THE_WIND", "TENFOLD_DOOM"):
        assert (OUT / "steam" / (key + "_ACHIEVED.png")).exists()
        assert (OUT / "google-play" / (key + ".png")).exists()
    assert (OUT / "google-play/FOR_IDRIL_AND_EARENDIL.png").exists()
    google_hashes = {hashlib.sha256((OUT / "google-play" / (row["key"] + ".png")).read_bytes()).hexdigest()
                     for row in rows}
    assert len(google_hashes) == 112
    import zipfile
    properties = (ROOT / "android/play-games.properties").read_text(encoding="utf-8")
    configured = {key: value.strip() for key, value in re.findall(
        r"^SIL_MORE_PLAY_ACH_([A-Z_]+)=([^\r\n]*)$", properties, re.M)}
    unconfigured = {row["key"] for row in rows if not configured.get(row["key"])}
    with zipfile.ZipFile(OUT / "google-play-new-achievements.zip") as bundle:
        assert set(bundle.namelist()) == {"AchievementsMetadata.csv", "AchievementsIconsMappings.csv",
                                          *(key + ".png" for key in unconfigured)}
    print("PASS store exports: 112 native/Google, 100 Steam/Apple, explicit exclusions, draft/release guard, Apple IDs/points/RGB/72ppi artwork and unconfigured-only Google bundle")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check real store metadata and raster exports, including unfinished selections."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import struct

from export_achievement_stores import (
    APPLE_POINTS, POINTS, apple_catalogue, catalogue, display_order, export,
    steam_excluded_keys, store_readiness,
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
    assert len(excluded) == 10, "Do not choose the user's pending exclusions"
    status = store_readiness(rows, excluded, excluded)
    for item in status.values():
        assert item["achievement_count"] == 102 and item["additional_exclusions_required"] == 2
        assert not item["ready_for_submission"]
    order = display_order()
    apple = apple_catalogue(rows, excluded, order)
    assert len({item["achievementID"] for item in apple}) == 102
    assert {item["key"] for item in apple} == keys - excluded
    for item in apple:
        assert item["achievementID"] == "com.silqh.silmore.achievement." + item["key"]
        assert 0 < item["points"] <= 100 and not item["repeatable"]
        assert item["localizations"][0]["locale"] == "en-US"
    # Synthetic choices exercise ready/invalid selection validation without
    # recording any real exclusion decision in the repository or store.
    fixture_excluded = excluded | set(sorted(keys - excluded - {"SWIFT_AS_THE_WIND", "TENFOLD_DOOM"})[:2])
    ready = store_readiness(rows, fixture_excluded, fixture_excluded)
    assert all(item["ready_for_submission"] and item["achievement_count"] == 100 for item in ready.values())
    try:
        store_readiness(rows, excluded, {"NOT_A_NATIVE_KEY"})
        raise AssertionError("Unknown key accepted")
    except ValueError:
        pass
    OUT.mkdir(parents=True, exist_ok=True)
    blocked = OUT / "require-ready-must-not-write"
    try:
        export(blocked, args.node, args.sharp, require_ready=True)
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
    export(OUT, args.node, args.sharp)
    assert not stale.exists() and sentinel.read_text(encoding="utf-8") == "preserve"
    status_file = json.loads((OUT / "store-readiness.json").read_text(encoding="utf-8"))
    assert status_file == status
    exported = json.loads((OUT / "apple-game-center-draft/achievements.json").read_text(encoding="utf-8"))
    assert not exported["readyForSubmission"] and exported["achievements"] == apple
    for item in apple:
        image = OUT / "apple-game-center-draft" / item["localizations"][0]["image"]
        width, height, depth, color, density = png_properties(image)
        assert (width, height, depth, color) == (1024, 1024, 8, 2), (image, width, height, depth, color)
        assert density is not None and density[2] == 1 and density[0] >= 2834 and density[1] >= 2834
    for key in ("SWIFT_AS_THE_WIND", "TENFOLD_DOOM"):
        assert (OUT / "steam-draft" / (key + "_ACHIEVED.png")).exists()
        assert (OUT / "google-play" / (key + ".png")).exists()
    google_hashes = {hashlib.sha256((OUT / "google-play" / (row["key"] + ".png")).read_bytes()).hexdigest()
                     for row in rows}
    assert len(google_hashes) == 112
    import zipfile
    with zipfile.ZipFile(OUT / "google-play-new-achievements.zip") as bundle:
        assert set(bundle.namelist()) == {"AchievementsMetadata.csv", "AchievementsIconsMappings.csv",
                                          "SWIFT_AS_THE_WIND.png", "TENFOLD_DOOM.png"}
    print("PASS store exports: 112 native/Google, 102 Steam/Apple draft, unchanged exclusions, release guard, Apple IDs/points/RGB/72ppi artwork and two-new Google bundle")


if __name__ == "__main__":
    main()

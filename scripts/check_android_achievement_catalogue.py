#!/usr/bin/env python3
"""Check JNI catalogue ordering; --gradle also exercises real Play configuration.

The Gradle checks use an isolated configuration file and synthetic Console IDs,
then restore default resources. Run without other Gradle builds in this checkout.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

from export_achievement_stores import catalogue

ROOT = Path(__file__).resolve().parents[1]
ANDROID = ROOT / "android"


def catalogue_keys():
    header = (ROOT / "src/achievements.h").read_text(encoding="utf-8")
    block = re.search(
        r"typedef\s+enum\s+sil_achievement_id\s*\{(.*?)\}\s*sil_achievement_id\s*;",
        header, re.S,
    )
    assert block, "Missing native achievement enum"
    entries = re.sub(r"/\*.*?\*/|//[^\r\n]*", "", block[1], flags=re.S)
    names = []
    for index, entry in enumerate(part.strip() for part in entries.split(",") if part.strip()):
        match = re.fullmatch(r"SIL_ACH_([A-Z][A-Z0-9_]*)(?:\s*=\s*(\d+))?", entry)
        assert match, f"Unsupported achievement enum entry: {entry}"
        assert match[2] is None or int(match[2]) == index, "Native masks require contiguous indexes from zero"
        names.append(match[1])
    assert names[-1] == "COUNT", "COUNT must terminate the stable native catalogue"
    keys = names[:-1]
    assert keys[:26] == [
        "INTO_THE_DEEP", "LIGHT_IN_DARKNESS", "BACK_TO_STARLIGHT",
        "THREE_LIGHTS_IN_EXILE", "THE_SHADOW_FALLS", "THE_QUIET_ROAD",
        "WORD_UNBROKEN", "FORGED_BY_YOUR_HAND", "A_NAME_WORTH_REMEMBERING",
        "SONG_BENEATH_THE_MOUNTAIN", "THERE_AND_BACK_AGAIN", "BY_A_THREAD",
        "LIGHT_OF_THE_ELDAR", "THE_LONG_TALE",
        "NO_RINGS_ATTACHED", "CABED_EN_ARAS", "OFT_EVIL_WILL", "THE_HASTY_STROKE",
        "A_FEW_LEAVES", "TWO_WEAPONS", "FOR_EXTERNAL_USE_ONLY", "FLY_YOU_FOOL",
        "KNOCK_WITH_YOUR_HEAD", "DOWN_TO_GOBLIN_TOWN", "ATHELAS_OUT_OF_STOCK",
        "ANGBAND_MAINTENANCE"], "Existing achievement indexes must stay stable"
    assert 0 < len(keys) < 64 and len(keys) == len(set(keys))
    definitions = catalogue()
    assert [row["key"] for row in definitions] == keys
    assert all(isinstance(row["hidden"], bool) for row in definitions)
    java = (ANDROID / "app/src/play/java/com/silqh/silmore/PlayGamesAchievements.java").read_text(encoding="utf-8")
    resources = re.findall(r"activity\.getString\(R\.string\.play_ach_([a-z0-9_]+)\)", java)
    assert resources == [key.lower() for key in keys], (
        "Java's provider ID array must match native enum order exactly", keys, resources)
    assert "allMask = (1L << ids.length) - 1" in java
    assert "long unlockedMask()" in java and "sync(long unlockedMask)" in java
    activity = (ANDROID / "app/src/main/java/com/silqh/silmore/SilMoreActivity.java").read_text(encoding="utf-8")
    proguard = (ANDROID / "app/proguard-rules.pro").read_text(encoding="utf-8")
    assert "syncPlayGamesAchievements(long unlockedMask)" in activity
    assert "public long getPlayGamesAchievementMask()" in activity
    assert "syncPlayGamesAchievements(long)" in proguard
    assert "public long getPlayGamesAchievementMask()" in proguard
    assert "Long.lowestOneBit(pending)" in java
    assert "Long.numberOfTrailingZeros(bit)" in java
    assert "private volatile long remoteMask" in java
    assert "localMask & ~confirmedMask & availableMask" in java
    assert '!ids[i].isEmpty()' in java, "Unmapped provider IDs must be skipped"
    assert "unlockImmediate(ids[index])" in java and not re.search(r"\.reveal(?:Immediate)?\s*\(", java), "Hidden achievements reveal only on unlock"
    properties = (ANDROID / "play-games.properties").read_text(encoding="utf-8")
    assert re.findall(r"^SIL_MORE_PLAY_ACH_([A-Z0-9_]+)=", properties, re.M) == keys
    native = (ROOT / "src/achievements-play-games.c").read_text(encoding="utf-8")
    assert '"(J)V"' in native and '"()J"' in native and "CallLongMethod" in native
    assert native.count("& SIL_ACH_ALL_MASK") == 2, "JNI must mask both directions with the shared catalogue"
    gradle = (ANDROID / "app/build.gradle").read_text(encoding="utf-8")
    assert "new File(repoRoot, 'src/achievements.h')" in gradle
    assert "playAchievementKeys.eachWithIndex" in gradle
    assert "'play_ach_' + key.toLowerCase(Locale.ROOT)" in gradle
    print(f"PASS: native/JNI/Java catalogue order, partial ID migration and masks ({len(keys)} achievements)")
    return keys


def check_gradle(keys):
    env = {key: value for key, value in os.environ.items()
           if not key.startswith("SIL_MORE_PLAY_ACH_") and key != "SIL_MORE_PLAY_GAMES_PROJECT_ID"}
    jdk = Path("C:/Program Files/BellSoft/LibericaJDK-17")
    if jdk.exists():
        env["JAVA_HOME"] = str(jdk)
    wrapper = str(ANDROID / ("gradlew.bat" if os.name == "nt" else "gradlew"))

    config_area = tempfile.TemporaryDirectory(prefix="sil-play-config-")
    isolated_config = Path(config_area.name) / "empty.properties"
    isolated_config.write_text("# Isolated validation configuration\n")

    def run(values, error=None, use_default=False):
        command = [wrapper, ":app:generatePlayDebugResValues", "--console=plain"]
        if not use_default:
            command.append(f"-PSIL_MORE_PLAY_GAMES_CONFIG={isolated_config}")
        command += [f"-P{key}={value}" for key, value in values.items()]
        result = subprocess.run(command, cwd=ANDROID, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if error is not None:
            assert result.returncode != 0 and error in result.stdout, result.stdout
        else:
            assert result.returncode == 0, result.stdout

    def resources():
        path = ANDROID / "app/build/generated/res/resValues/play/debug/values/gradleResValues.xml"
        return {item.attrib["name"]: item.text or "" for item in ET.parse(path).getroot()}

    config = {"SIL_MORE_PLAY_GAMES_PROJECT_ID": "123456789"}
    config.update({f"SIL_MORE_PLAY_ACH_{key}": f"Synthetic_{index:02d}"
                   for index, key in enumerate(keys)})
    try:
        run({})
        assert resources()["play_games_configured"] == "false"
        run({"SIL_MORE_PLAY_GAMES_PROJECT_ID": "123456789"})
        assert resources()["play_games_configured"] == "false"
        run({f"SIL_MORE_PLAY_ACH_{keys[-1]}": "Synthetic_last"}, "project ID together")
        run({key: value for key, value in config.items() if key != f"SIL_MORE_PLAY_ACH_{keys[-1]}"})
        assert resources()["play_games_configured"] == "true"
        assert resources()["play_ach_" + keys[-1].lower()] == ""
        run(dict(config, SIL_MORE_PLAY_GAMES_PROJECT_ID="invalid"), "numeric Games services project ID")
        duplicate = dict(config)
        duplicate[f"SIL_MORE_PLAY_ACH_{keys[-1]}"] = duplicate[f"SIL_MORE_PLAY_ACH_{keys[0]}"]
        run(duplicate, "distinct Console ID")
        malformed = dict(config)
        malformed[f"SIL_MORE_PLAY_ACH_{keys[-1]}"] = "invalid.id"
        run(malformed, "only letters, digits")
        run(config)
        actual = resources()
        assert actual["play_games_configured"] == "true"
        assert actual["play_games_project_id"] == "123456789"
        expected = {f"play_ach_{key.lower()}": config[f"SIL_MORE_PLAY_ACH_{key}"] for key in keys}
        assert {key: value for key, value in actual.items() if key.startswith("play_ach_")} == expected
        print("PASS: Gradle supports partial migration, rejects missing project/duplicate/malformed IDs; all IDs map correctly")
    finally:
        try:
            run({}, use_default=True)
        finally:
            config_area.cleanup()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gradle", action="store_true", help="exercise actual Gradle configuration and generated resources")
    args = parser.parse_args()
    keys = catalogue_keys()
    if args.gradle:
        check_gradle(keys)

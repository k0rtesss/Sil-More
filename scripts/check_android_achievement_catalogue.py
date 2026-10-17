#!/usr/bin/env python3
"""Check JNI catalogue ordering; --gradle also exercises real Play configuration.

The Gradle checks use clearly synthetic Console IDs and restore unconfigured
generated resources afterward. Run without other Gradle builds in this checkout.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import xml.etree.ElementTree as ET

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
    assert keys[:3] == ["INTO_THE_DEEP", "LIGHT_IN_DARKNESS", "BACK_TO_STARLIGHT"], "Shipped achievement indexes must stay stable"
    assert 0 < len(keys) < 32 and len(keys) == len(set(keys))
    java = (ANDROID / "app/src/play/java/com/silqh/silmore/PlayGamesAchievements.java").read_text(encoding="utf-8")
    resources = re.findall(r"activity\.getString\(R\.string\.play_ach_([a-z0-9_]+)\)", java)
    assert resources == [key.lower() for key in keys], (
        "Java's provider ID array must match native enum order exactly", keys, resources)
    assert "allMask = (1 << ids.length) - 1" in java
    native = (ROOT / "src/achievements-play-games.c").read_text(encoding="utf-8")
    assert native.count("& SIL_ACH_ALL_MASK") == 2, "JNI must mask both directions with the shared catalogue"
    gradle = (ANDROID / "app/build.gradle").read_text(encoding="utf-8")
    assert "new File(repoRoot, 'src/achievements.h')" in gradle
    assert "playAchievementKeys.eachWithIndex" in gradle
    assert "'play_ach_' + key.toLowerCase(Locale.ROOT)" in gradle
    print(f"PASS: native/JNI/Java catalogue order and masks ({len(keys)} achievements)")
    return keys


def check_gradle(keys):
    env = {key: value for key, value in os.environ.items()
           if not key.startswith("SIL_MORE_PLAY_ACH_") and key != "SIL_MORE_PLAY_GAMES_PROJECT_ID"}
    jdk = Path("C:/Program Files/BellSoft/LibericaJDK-17")
    if jdk.exists():
        env["JAVA_HOME"] = str(jdk)
    wrapper = str(ANDROID / ("gradlew.bat" if os.name == "nt" else "gradlew"))

    def run(values, error=None):
        command = [wrapper, ":app:generatePlayDebugResValues", "--console=plain"]
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
        run({"SIL_MORE_PLAY_GAMES_PROJECT_ID": "123456789"}, "IDs together")
        run({f"SIL_MORE_PLAY_ACH_{keys[-1]}": "Synthetic_last"}, "IDs together")
        run({key: value for key, value in config.items() if key != f"SIL_MORE_PLAY_ACH_{keys[-1]}"}, "IDs together")
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
        print("PASS: Gradle rejects incomplete, duplicate and malformed configuration; all IDs map correctly")
    finally:
        run({})
        assert resources()["play_games_configured"] == "false"
        assert all(not value for key, value in resources().items() if key.startswith("play_ach_"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gradle", action="store_true", help="exercise actual Gradle configuration and generated resources")
    args = parser.parse_args()
    keys = catalogue_keys()
    if args.gradle:
        check_gradle(keys)

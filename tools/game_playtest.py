#!/usr/bin/env python3
"""Isolated real-game playtests through GameControl, using a portable build."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import uuid

from game_control import GameControl, launch

ROOT = Path(__file__).resolve().parents[1]
DEPLOY = ROOT / "sil-more-windows-sdl3-portable"
OUT = ROOT / "scripts/output/control-playtests"


def create_session(name: str, wizard: bool = True) -> dict:
    if not name or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-_" for c in name):
        raise ValueError("Use a lowercase playtest name with letters, digits, hyphens or underscores")
    profile = OUT / (name + "-" + uuid.uuid4().hex[:8])
    profile.mkdir(parents=True)
    for file in DEPLOY.iterdir():
        if file.is_file() and file.suffix.lower() in (".exe", ".dll"):
            shutil.copy2(file, profile / file.name)
    # Copy only shipped read-only assets. Never import real saves, scores,
    # preferences, or generated data from either deployment or user profile.
    for folder in ("docs", "edit", "help", "pref", "xtra"):
        source = DEPLOY / "lib" / folder
        if source.is_dir():
            shutil.copytree(source, profile / "lib" / folder)
    for folder in ("apex", "apex/metaruns", "data", "save", "user", "bone"):
        (profile / "lib" / folder).mkdir(parents=True, exist_ok=True)
    (profile / "screens").mkdir()
    control = GameControl(profile / "control", timeout=30)
    session = launch(control, profile / "sil-more.exe", headless=True, wizard=wizard)
    session.update(profile=str(profile), control=str(control.directory), wizard=wizard)
    (profile / "playtest.json").write_text(json.dumps(session, indent=2), encoding="utf-8")
    return session


def connect(profile: str | Path) -> GameControl:
    return GameControl(Path(profile) / "control", timeout=30)


def capture(profile: str | Path, label: str) -> dict:
    profile = Path(profile)
    response = connect(profile).observe(output=profile / "screens" / (label + ".png"))
    (profile / "screens" / (label + ".json")).write_text(json.dumps(response, indent=2), encoding="utf-8")
    return response


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name")
    parser.add_argument("--normal", action="store_true")
    args = parser.parse_args()
    print(json.dumps(create_session(args.name, wizard=not args.normal), indent=2))

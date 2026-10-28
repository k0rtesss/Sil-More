#!/usr/bin/env python3
"""Check default sword-text contrast in every shipped palette."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]


def luminance(rgb):
    def linear(component):
        value = component / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    return sum(weight * linear(component)
               for weight, component in zip((0.2126, 0.7152, 0.0722), rgb))


def main():
    colors = json.loads((ROOT / "lib/pref/object_text_colors.json").read_text(encoding="utf-8"))
    presets = json.loads((ROOT / "lib/pref/palette_presets.json").read_text(encoding="utf-8"))
    sword = next(entry for entry in colors["entries"] if entry["name"] == "SWORD")
    # This check is deliberately about the shipped preference and presets;
    # a player-supplied color or palette remains their own choice.
    assert 0 <= sword["attr"] < 16, "Sword needs a base palette color"
    for preset in presets["presets"]:
        foreground = luminance(preset["colors"][sword["attr"]])
        background = luminance(preset["colors"][0])
        ratio = (max(foreground, background) + 0.05) / (min(foreground, background) + 0.05)
        assert ratio >= 4.5, f"{preset['id']}: sword text contrast is {ratio:.2f}:1"
        print(f"{preset['id']}: sword text contrast {ratio:.2f}:1 PASS")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Exercise territorial doorway defense and lair limits through real SDL input.

Build standard first. Reuses the isolated encounter/save-reload harness;
all player actions go through GameControl and monster traits stay unchanged.
"""
from __future__ import annotations

import json
from pathlib import Path

import check_monster_ai_extended_gameplay as extended

play = extended.play
ROOT = play.ROOT
OUT = ROOT / "scripts/output/territorial-defense"


def point(orientation: int, dy: int, dx: int) -> tuple[int, int]:
    if orientation & 4:
        dx = -dx
    for _ in range(orientation & 3):
        dy, dx = dx, -dy
    return 20 + dy, 40 + dx


def corner(race: int, orientation: int) -> dict:
    tiles = [dict(y1=1, y2=38, x1=1, x2=62, feat=63)]
    for dy in range(-6, 5):
        for dx in range(-1, 2):
            if dx == 0 or dy >= 2:
                y, x = point(orientation, dy, dx)
                tiles.append(dict(y1=y, x1=x, feat=1))
    icky = []
    for dy in range(1, 5):
        for dx in range(-1, 2):
            y, x = point(orientation, dy, dx)
            icky.append(dict(y1=y, x1=x))
    y, x = point(orientation, 2, -1)
    return dict(seed=20261116 + orientation, player_y=20, player_x=40,
                tiles=tiles, icky=icky, recognition=1, glow=orientation % 2,
                stealth=0, evasion=35, constitution=35,
                monsters=[dict(race=race, y=y, x=x, heard=1, morale=100)])


def assert_inside(frames: list[dict]) -> None:
    for frame in frames:
        for m in frame["monsters"]:
            dy, dx = abs(m["y"] - m["home_y"]), abs(m["x"] - m["home_x"])
            assert max(dy, dx) + min(dy, dx) // 2 <= 6, (frame, m)
            assert m["icky"] == m["home_icky"], (frame, m)
            if m["territory"]:
                assert m["partition"] == m["territory"] - 1, (frame, m)


def frames_of(result: dict) -> list[dict]:
    return json.loads((Path(result["profile"]) / "trace.json").read_text())


def main() -> None:
    extended.OUT = OUT
    extended.instrument()
    exe, env = play.build()
    results = []
    report = dict(passed=False, cases=results)
    try:
        # Original random traits remain enabled. The actor can spend erratic
        # actions, but must take the doorway block during ordinary pursuit.
        away_keys = ["KP8", "KP6", "KP2", "KP4"]
        for race in (124, 15, 32):
            for orientation in range(8):
                spec = corner(race, orientation)
                keys = ["KP5"] * 12 + [away_keys[orientation & 3]] * 5 + ["KP5"] * 8
                result = play.run(exe, env, f"door-{race}-{orientation}", spec, keys)
                frames = frames_of(result)
                assert not frames[0]["monsters"][0]["sees_player"]
                by, bx = point(orientation, 1, 0)
                assert any((m["y"], m["x"]) == (by, bx)
                           for f in frames[1:13] for m in f["monsters"]), result
                assert_inside(frames)
                result["checks"] += ["heard threat reaches doorway block", "all movement stays in lair"]
                results.append(result)

        # No vault walls hide the range boundary: a visible retreating player
        # tempts ordinary pursuit and the hatchling's erratic movement alike.
        hall = [dict(y1=1, y2=38, x1=1, x2=62, feat=63),
                dict(y1=20, x1=24, x2=55, feat=1)]
        for partition in (False, True):
            for race in (124, 15, 32):
                spec = dict(seed=20261116, player_y=20,
                    player_x=31 if partition else 40, tiles=hall,
                    evasion=35, constitution=35, lair_partition=int(partition),
                    monsters=[dict(race=race, y=20, x=30 if partition else 38,
                                   heard=1, morale=100)])
                result = play.run(exe, env, f"outside-{race}-{int(partition)}", spec,
                                  ["KP5"] * 4 + ["KP6"] * 10 + ["KP5"] * 12)
                frames = frames_of(result)
                assert frames[-1]["player"]["x"] == spec["player_x"] + 10
                assert_inside(frames)
                results.append(result)

        # Home and sensory state must survive real saving/loading. Two equal
        # loaded input streams are already compared by the shared harness.
        checkpoint = extended.run_persistence(exe, env, "territorial-doorway",
            corner(124, 0), ["KP5"] * 3, ["KP8"] * 5 + ["KP5"] * 8,
            "territorial_defense")
        for branch in checkpoint["branches"]:
            assert_inside(frames_of(branch))
        report["persistence"] = checkpoint
        report["passed"] = True
        report["case_count"] = len(results)
        print(f"Territorial defense: {len(results)} SDL encounters and two identical save/reload continuations PASS.")
    finally:
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()

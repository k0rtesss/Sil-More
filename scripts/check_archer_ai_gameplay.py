#!/usr/bin/env python3
"""Play archer retreat and firing regressions through actual headless SDL input.

Prepared encounters use disposable profiles and the production dungeon loop.
Wrathful controls equip the real shield ego rather than overriding aggravation.
Keep original failing traces with --discovery; accepted runs enforce outcomes.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import traceback

import check_monster_ai_extended_gameplay as engine
import check_monster_coordination_gameplay as coordination

play = engine.play
ROOT = play.ROOT
OUT = ROOT / "scripts/output/archer-ai"


def instrument(destination):
    engine.OUT = destination
    engine.instrument()
    play.FIXTURE = play.FIXTURE.replace(
        'cJSON_AddNumberToObject(player, "light", p_ptr->cur_light);',
        'cJSON_AddNumberToObject(player, "light", p_ptr->cur_light);\n'
        '        cJSON_AddNumberToObject(player, "aggravate", p_ptr->aggravate);')


def box(y1, y2, x1, x2, feat=1):
    return dict(y1=y1, y2=y2, x1=x1, x2=x2, feat=feat)


def archer(race=51, y=20, x=39, **kw):
    return dict(race=race, y=y, x=x) | dict(morale=0, heard=1) | kw


def focused_cases():
    for name, spec, keys, _ in coordination.cases():
        if name == "crowded-orc-squad":
            yield "crowded-preferred-range-lanes", spec, keys, dict(retreat=True)
    # Screenshot topology: player at the southwest room corner, a champion
    # above, and an adjacent archer with open floor to the north and east.
    corner = [box(1, 38, 1, 62, 63), box(15, 20, 38, 49),
              box(20, 20, 30, 38), box(20, 25, 37, 37)]
    screenshot = [box(1, 38, 1, 62, 63), box(16, 20, 38, 47),
                  box(20, 20, 32, 38), box(21, 23, 34, 36),
                  box(18, 19, 37, 37)]
    for wrath in (0, 15):
        for glow in (0, 1):
            yield f"screenshot-diagonal-glow{glow}-wrath{wrath}", dict(
                player_y=20, player_x=38, glow=glow, shield_ego=wrath,
                tiles=screenshot, monsters=[archer(x=37), archer(61, 19, 38)]), \
                ["KP5"] * 22, dict(retreat=True)
            yield f"open-dark{1-glow}-wrath{wrath}", dict(glow=glow, shield_ego=wrath,
                monsters=[archer()]), ["KP5"] * 22, dict(retreat=True)
        for position, y, x in (("west", 20, 37), ("northwest", 19, 37),
                               ("north", 19, 38), ("east", 20, 39)):
            tiles = copy.deepcopy(corner)
            # Keep each adjacent starting square and its outward route open.
            tiles += [box(y, y, x, x), box(18, 19, 37, 38)]
            champion = (19, 38) if (y, x) != (19, 38) else (19, 39)
            yield f"corner-{position}-wrath{wrath}", dict(
                player_y=20, player_x=38, shield_ego=wrath,
                tiles=tiles, monsters=[archer(y=y, x=x),
                    archer(61, *champion)]), ["KP5"] * 22, dict(retreat=True)
        yield f"open-adjacent-wrath{wrath}", dict(shield_ego=wrath,
            monsters=[archer()]), ["KP5"] * 22, dict(retreat=True)
        corridor = [box(1, 38, 1, 62, 63), box(20, 20, 25, 50)]
        yield f"corridor-adjacent-wrath{wrath}", dict(shield_ego=wrath,
            tiles=corridor, monsters=[archer()]), ["KP5"] * 22, dict(retreat=True)


def bow_races():
    result, current = [], None
    for line in (ROOT / "lib/edit/monster.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith("N:"):
            _, race, name = line.split(":", 2)
            current = dict(race=int(race), name=name)
        elif current and line.startswith("S:") and "SPELL_PCT_" in line:
            current["frequency"] = int(re.search(r"SPELL_PCT_(\d+)", line)[1])
        elif current and line.startswith("S:") and re.search(r"\bARROW[12]\b", line):
            result.append(current)
    return result


def rotate(spec, quarter):
    spec = copy.deepcopy(spec)
    py, px = spec.get("player_y", 20), spec.get("player_x", 40)
    def point(y, x):
        dy, dx = y - py, x - px
        for _ in range(quarter):
            dy, dx = dx, -dy
        return py + dy, px + dx
    for m in spec["monsters"]:
        m["y"], m["x"] = point(m["y"], m["x"])
    for tile in spec.get("tiles", []):
        # Leave the outer wall fill intact; rotate the encounter inside it.
        if (tile["y1"], tile["y2"], tile["x1"], tile["x2"]) == (1, 38, 1, 62):
            continue
        y1, x1 = point(tile["y1"], tile["x1"])
        y2, x2 = point(tile["y2"], tile["x2"])
        tile.update(y1=min(y1, y2), y2=max(y1, y2), x1=min(x1, x2), x2=max(x1, x2))
    return spec


def matrix_cases():
    for row in bow_races():
        for glow in (0, 1):
            for wrath in (0, 15):
                yield f"catalogue-{row['race']}-glow{glow}-wrath{wrath}", dict(
                    glow=glow, shield_ego=wrath, constitution=35, evasion=24,
                    monsters=[archer(row["race"])]), ["KP5"] * 28, \
                    dict(retreat=row["frequency"] > 15, strict=row["frequency"] > 15,
                         min_arrows=max(2, row["frequency"] // 15),
                         min_late=1 if row["frequency"] <= 20 else 2)
    # The sole outward step is diagonal; rotate to cover all four directions.
    diagonal = [box(1, 38, 1, 62, 63), box(19, 21, 40, 49),
                box(20, 20, 39, 40), box(21, 24, 35, 38), box(21, 21, 39, 39)]
    for quarter in range(4):
        for wrath in (0, 15):
            for glow in (0, 1):
                spec = dict(glow=glow, shield_ego=wrath, tiles=diagonal,
                            monsters=[archer(), archer(61, 19, 40)])
                yield f"direct-diagonal-r{quarter}-glow{glow}-wrath{wrath}", \
                    rotate(spec, quarter), ["KP5"] * 26, dict(retreat=True, strict=True)
    for race in (51, 103):
        for glow in (0, 1):
            for wrath in (0, 15):
                for mana in (0, 2):
                    yield f"low-mana-{race}-mana{mana}-glow{glow}-wrath{wrath}", dict(
                        glow=glow, shield_ego=wrath, monsters=[archer(race, mana=mana)]), \
                        ["KP5"] * 26, dict(retreat=True, strict=True)
                yield f"chase-{race}-glow{glow}-wrath{wrath}", dict(glow=glow,
                    shield_ego=wrath, melee=0, evasion=24, monsters=[archer(race, x=37)]), \
                    ["KP4"] * 12 + ["KP5"] * 18, dict(retreat=True, strict=True)


def control_cases():
    for race in (51, 103):
        for wrath in (0, 15):
            yield f"cornered-{race}-wrath{wrath}", dict(shield_ego=wrath,
                tiles=[box(1, 38, 1, 62, 63), box(20, 20, 39, 40)],
                monsters=[archer(race)]), ["KP5"] * 22, dict(melee=True, no_arrows=True)
            yield f"challenge-{race}-wrath{wrath}", dict(shield_ego=wrath,
                song=1, song_skill=35, monsters=[archer(race, morale=1000)]), \
                ["KP5"] * 22, dict(melee=True, no_arrows=True, challenge=True)
            yield f"challenge-ends-{race}-wrath{wrath}", dict(shield_ego=wrath,
                glow=0, song=1, song_skill=35, monsters=[archer(race, morale=1000)]), \
                ["KP5"] * 8 + ["s", "s"] + ["KP5"] * 24, dict(retreat=True, challenge_ends=True)
        corridor = [box(1, 38, 1, 62, 63), box(20, 20, 28, 50)]
        for feat, name in ((84, "shallow"), (86, "ice")):
            yield f"terrain-{name}-{race}", dict(tiles=corridor + [box(20, 20, 34, 38, feat)],
                monsters=[archer(race)]), ["KP5"] * 30, dict(retreat=True)
    for wrath in (0, 15):
        yield f"unlit-alert-combat-wrath{wrath}", dict(glow=0, torch_fuel=0,
            shield_ego=wrath, monsters=[archer(x=35)]), ["KP5"] * 26, dict(retreat=True, dark_combat=True)
        yield f"unlit-unwary-stealth-wrath{wrath}", dict(glow=0, torch_fuel=0,
            stealth=60, shield_ego=wrath, monsters=[archer(x=35, alertness=-10, heard=0)]), \
            ["KP5"] * 22, dict(no_arrows=True, undetected=True)
        yield f"extinguished-contact-wrath{wrath}", dict(glow=0, torch_fuel=6,
            shield_ego=wrath, monsters=[archer()]), ["KP5"] * 28, dict(retreat=True, extinguishes=True)
    yield "wall-separated-hearing", dict(glow=0, monsters=[archer(x=36)],
        tiles=[box(1, 38, 38, 38, 63)]), ["KP5"] * 26, dict(no_arrows=True)


def cases(suites):
    groups = dict(focused=focused_cases, matrix=matrix_cases, controls=control_cases)
    for suite in suites.split(","):
        yield from groups[suite]()


def metrics(result):
    frames, events = engine.read_run(result)
    ids = {m["actor_id"] for m in frames[0]["monsters"] if m.get("missile")}
    per_actor = {}
    for actor in ids:
        own = [e for e in events if e["actor_id"] == actor]
        history = [(f, m) for f in frames for m in f["monsters"]
                   if m["actor_id"] == actor]
        per_actor[str(actor)] = dict(
            race=history[0][1]["race"],
            arrows=sum(e["kind"] == "ranged" and e["detail"] in (96, 97) for e in own),
            melee=sum(e["kind"] == "melee" for e in own),
            moves=sum(e["kind"] == "action" and
                      (e["y"], e["x"]) != (e["old_y"], e["old_x"]) for e in own),
            max_distance=max(engine.dist(f, m) for f, m in history),
            late_arrows=sum(e["kind"] == "ranged" and e["detail"] in (96, 97)
                            and e["player_turn"] >= frames[0]["player_turn"] + 10 for e in own))
    return per_actor


def verify(result, spec, check):
    frames, events = engine.read_run(result)
    assert frames[0]["player"]["aggravate"] == bool(spec.get("shield_ego")), "Shield control ineffective"
    for actor, data in result["archers"].items():
        if check.get("retreat"):
            assert data["moves"] and data["max_distance"] >= 2, f"Archer {actor} never retreats: {data}"
            assert data["arrows"] >= check.get("min_arrows", 5) and data["late_arrows"] >= check.get("min_late", 2), f"Archer {actor} fails to sustain fire: {data}"
        if check.get("strict"):
            assert not data["melee"], f"Archer {actor} uses melee with an open retreat: {data}"
        if check.get("no_arrows"):
            assert not data["arrows"], f"Archer {actor} fires in a forbidden situation: {data}"
        if check.get("melee"):
            assert data["melee"] >= 5, f"Archer {actor} fails to fight when retreat/shooting is unavailable: {data}"
    if check.get("extinguishes"):
        assert frames[-1]["player"]["light"] == 0
        assert all(e["sees_player"] for e in events if e["kind"] == "ranged")
        assert any(e["kind"] == "ranged" and e["player_turn"] >= frames[0]["player_turn"] + 12 for e in events)
    if check.get("dark_combat"):
        assert all(f["player"]["light"] == 0 for f in frames)
        assert any(not m["visual_perception_bonus"] and m["sees_player"] for f in frames for m in f["monsters"])
    if check.get("undetected"):
        assert all(m["alertness"] < 0 for f in frames for m in f["monsters"])
        assert not any(e["kind"] == "melee" for e in events)
    if check.get("challenge_ends"):
        end = next(f["player_turn"] for f in frames if f["player"]["song"] == 100)
        active_until = frames[0]["player_turn"] + 6
        assert end > active_until
        assert any(e["kind"] == "melee" and e["player_turn"] < active_until for e in events)
        assert not any(e["kind"] == "ranged" and e["player_turn"] < active_until for e in events)
    if check.get("challenge"):
        assert all(f["player"]["song"] == 1 for f in frames)
        assert all(m["stance"] == 3 for f in frames if f["player_turn"] >= frames[0]["player_turn"] + 2 for m in f["monsters"])
    # Attack-time sight and projection observations guard against omniscient
    # tracking, occupied lanes and shooting while swimming during all scenes.
    for e in events:
        if e["kind"] == "ranged" and e["detail"] in (96, 97):
            assert e["sees_player"] and e["projection"] == 2, f"Illegal arrow: {e}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=OUT)
    parser.add_argument("--seed", type=int, default=1701)
    parser.add_argument("--filter", default="")
    parser.add_argument("--suite", default="focused,matrix,controls")
    parser.add_argument("--discovery", action="store_true")
    args = parser.parse_args()
    destination = args.output_dir.resolve()
    instrument(destination)
    exe, env = play.build()
    env["SIL_LOG_LEVEL"] = "DEBUG"
    manifest = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in (Path(__file__), Path(play.__file__), Path(engine.__file__),
                          ROOT / "src/melee/melee-movement-tactics.c",
                          ROOT / "src/melee/melee-movement.c",
                          ROOT / "src/melee/melee-process.c")}
    manifest["production_executable"] = hashlib.sha256((play.BUILD / "sil-more.exe").read_bytes()).hexdigest()
    manifest["fixture_executable"] = hashlib.sha256(exe.read_bytes()).hexdigest()
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2))
    results, failures = [], []
    for name, spec, keys, check in cases(args.suite):
        if args.filter and not re.search(args.filter, name):
            continue
        spec = dict(copy.deepcopy(spec), seed=args.seed)
        try:
            result = play.run(exe, env, name, spec, keys)
            result["archers"] = metrics(result)
            result["archer_check"] = check
            results.append(result)
            print(json.dumps(dict(case=name, archers=result["archers"])), flush=True)
            if not args.discovery:
                verify(result, spec, check)
        except Exception as error:
            failures.append(dict(name=name, error=str(error), traceback=traceback.format_exc()))
            print(json.dumps(failures[-1]), flush=True)
        (destination / f"report-{args.seed}.json").write_text(json.dumps(
            dict(results=results, failures=failures, discovery=args.discovery), indent=2))
    print(f"Archer encounters: {len(results)}, failures: {len(failures)}", flush=True)
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

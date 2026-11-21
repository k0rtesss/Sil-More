#!/usr/bin/env python3
"""Audit the stealth-recognition setting across real monster gameplay.

All combat-capable templates act in darkness with recognition enabled. Selected
families also replay identical seeds/inputs with the option off: their combat
actions must match even though perception-roll bonuses and alertness can differ.
Physical LOS, short sight, alertness and attack legality remain production code.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import traceback

import check_monster_ai_catalogue_gameplay as catalogue

play, engine, audit = catalogue.play, catalogue.engine, catalogue.audit
ROOT = play.ROOT
OUT = ROOT / "scripts/output/monster-recognition"


def combat_trace(result):
    _, events = engine.read_run(result)
    return [tuple(e.get(k) for k in ("kind", "player_turn", "race", "detail",
                                    "y", "x", "old_y", "old_x", "melee", "skipped"))
            for e in events if e["kind"] in ("action", "ranged", "melee", "reaction")]


def cases():
    for name, spec, keys, check in catalogue.catalogue_cases():
        yield name, dict(spec, recognition=1, glow=0, torch_fuel=0,
                         constitution=35), keys, check
    # Melee, bows, spellcasters, breathers, spies, shadow/support and commanders.
    for race in (41, 51, 103, 115, 121, 142, 146, 175, 407, 408, 409):
        for setting in (0, 1):
            yield f"paired-{race}-recognition{setting}", dict(
                recognition=setting, glow=0, torch_fuel=0, constitution=35,
                evasion=35, monsters=[catalogue.actor(race, x=36)]), \
                ["KP5"] * 26, dict(paired=race, catalogue_race=race)
    for setting in (0, 1):
        for race in (41, 51, 103, 175):
            yield f"quiet-unwary-{race}-recognition{setting}", dict(
                recognition=setting, glow=0, torch_fuel=0, stealth=60,
                evasion=35, monsters=[dict(catalogue.actor(race, x=34, alertness=-10), heard=0)]), \
                ["KP5"] * 22, dict(quiet=True)


def verify(result, check):
    catalogue.verify(result, check)
    frames, events = engine.read_run(result)
    if check.get("quiet"):
        assert all(m["alertness"] < 0 for f in frames for m in f["monsters"]), "Quiet character detected"
        assert not any(e["kind"] in ("ranged", "melee", "reaction") for e in events), "Unwary monster attacks without detection"
    else:
        # All initially placed actors have already detected the player. No
        # remembered visual target is needed to fight in complete darkness.
        initial = frames[0]["monsters"][0]
        if initial["sight_range"] >= 4:
            assert initial["sees_player"], "Darkness blinded an alerted actor"
        assert all(f["player"]["light"] == 0 for f in frames)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=OUT)
    parser.add_argument("--seed", type=int, default=9919)
    parser.add_argument("--filter", default="")
    parser.add_argument("--shard-index", type=int, default=0)
    parser.add_argument("--shard-count", type=int, default=1)
    args = parser.parse_args()
    destination = args.output_dir.resolve()
    catalogue.OUT = destination
    catalogue.instrument()
    exe, env = audit.build()
    env["SIL_LOG_LEVEL"] = "DEBUG"
    manifest = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in (Path(__file__), Path(play.__file__), Path(engine.__file__),
                          Path(catalogue.__file__), ROOT / "src/monster/monster-senses.c",
                          ROOT / "src/monster/monster-ai.c", ROOT / "src/melee/melee-process.c")}
    manifest["production_executable"] = hashlib.sha256((play.BUILD / "sil-more.exe").read_bytes()).hexdigest()
    manifest["fixture_source"] = hashlib.sha256(exe.with_suffix(".c").read_bytes()).hexdigest()
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2))
    results, failures, paired = [], [], {}
    # Keep both halves of each comparison in one shard.
    groups = {}
    for name, spec, keys, check in cases():
        group = f"paired-{check['paired']}" if "paired" in check else name
        groups.setdefault(group, []).append((name, spec, keys, check))
    for group_index, (_, group) in enumerate(groups.items()):
        if group_index % args.shard_count != args.shard_index:
            continue
        for name, spec, keys, check in group:
            if args.filter and not re.search(args.filter, name):
                continue
            spec = dict(copy.deepcopy(spec), seed=args.seed)
            try:
                result = play.run(exe, audit.case_env(env, spec), name, spec, keys)
                verify(result, check)
                result["recognition_check"] = check
                if "paired" in check:
                    if check["paired"] in paired:
                        assert combat_trace(paired[check["paired"]]) == combat_trace(result), "Recognition option changes alerted combat decisions"
                        result["paired_reference"] = paired[check["paired"]]["profile"]
                    else:
                        paired[check["paired"]] = result
                results.append(result)
            except Exception as error:
                failures.append(dict(name=name, error=str(error), traceback=traceback.format_exc()))
                print(json.dumps(failures[-1]), flush=True)
            (destination / "report.json").write_text(json.dumps(dict(
                results=results, failures=failures, shard=args.shard_index, shards=args.shard_count), indent=2))
    print(f"Recognition encounters: {len(results)} passed, {len(failures)} failed", flush=True)
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

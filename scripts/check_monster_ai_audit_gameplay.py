#!/usr/bin/env python3
"""Run a repeatable old/new monster audit through the headless SDL interface.

Uses the production dungeon loop, disposable profiles and read-only observers
from the existing encounter suites. No player action bypasses GameControl.
Reports include exact inputs, fixture/source hashes, logs, PNGs and full traces.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import random
import re
import traceback

import check_monster_ai_catalogue_gameplay as catalogue

play, engine = catalogue.play, catalogue.engine
transparency, audit = catalogue.transparency, catalogue.audit
coordination = transparency.coordination
ROOT = play.ROOT
OUT = ROOT / "scripts/output/monster-ai-audit"


def endurance_cases(seed):
    """Long moving-target encounters and crowded formations absent from solos."""
    families = {
        "orcs": [91, 81, 61, 51, 51, 41],
        "men": [409, 408, 82, 103, 103, 82],
        "cats": [185, 177, 154, 175, 175, 154],
        "giants": [135, 146, 114, 114, 114],
        "trolls": [214, 192, 151, 74, 74],
        "cross-kin": [241, 145, 91, 61, 51, 51],
    }
    positions = [(20, 34), (18, 33), (22, 33), (17, 31), (23, 31), (20, 30)]
    randomizer = random.Random(seed)
    # Closed walks keep the target away from the map border. Each walk also
    # includes a stationary phase in which firing queues must make progress.
    keys = ["KP5"] * 18
    for _ in range(6):
        first, second = randomizer.choice([
            ("KP8", "KP2"), ("KP2", "KP8"), ("KP6", "KP4"), ("KP4", "KP6")])
        keys += [first] * 3 + ["KP5"] * 4 + [second] * 3 + ["KP5"] * 6
    for family, races in families.items():
        for layout in ("open", "screens"):
            tiles = [] if layout == "open" else [
                dict(y1=15, y2=19, x1=36, feat=63),
                dict(y1=22, y2=26, x1=38, feat=63)]
            party = [dict(race=race, y=y, x=x, heard=1, morale=0)
                     for race, (y, x) in zip(races, positions)]
            spec = dict(monsters=party, tiles=tiles, evasion=38, constitution=35,
                        will=25, allow_death=True)
            yield f"moving-{family}-{layout}", spec, keys, dict(solo=True)
    # More than eight members exercise capacity, independent leadership and
    # distinct reservations rather than only the usual four-actor formation.
    for rows in (3, 5):
        party = [dict(race=91 if i == 0 else (51 if i % 3 == 0 else 61),
                      y=15 + i // 4 * 2, x=28 + i % 4 * 2, heard=1, morale=0)
                 for i in range(rows * 4)]
        yield f"crowded-{len(party)}-orcs", dict(monsters=party, evasion=42,
            constitution=35, allow_death=True), keys, dict(solo=True, shots="any")
    for race in (23, 64):
        yield f"legacy-dim-{race}", dict(recognition=0, evasion=35,
            monsters=[dict(race=race, x=36, heard=1)]), ["KP5"] * 80, dict(cast=113, solo=True)
    for race in (403, 406, 407, 408, 410, 411, 253):
        yield f"sustained-unique-{race}", dict(recognition=0, evasion=35,
            constitution=35, allow_death=True, confirm_traps=race == 410, decline_chasms=race == 410,
            monsters=[dict(race=race, x=39, heard=1)]), (
                ["KP5"] * 36 + ["KP8", "KP6", "KP2", "KP4"] * 8 + ["KP5"] * 36), dict(solo=True)


def cases(suite, seed):
    if suite == "catalogue":
        return catalogue.catalogue_cases()
    if suite == "legacy":
        return catalogue.legacy_cases()
    if suite == "baseline":
        return [(n, s, k, {}) for n, s, k in play.scenarios()
                if not n.startswith(("species-", "ordinary-"))]
    if suite == "coordination":
        return [(n, s, k, dict(coordination_check=c)) for n, s, k, c in coordination.cases()] + coordination.varied_cases()
    if suite == "adversarial":
        return audit.cases()
    if suite == "visibility":
        return transparency.cases() + transparency.pressure_cases()
    if suite == "water":
        return catalogue.water_cases()
    if suite == "extended":
        return [(n, s, k, dict(engine_check=c)) for n, s, k, c in engine.scenarios()
                if not n.startswith(("legacy-", "dragon-lair-", "grotesque-los", "archers-torch"))]
    if suite == "endurance":
        return list(endurance_cases(seed))
    if suite == "persistence":
        return engine.persistence_cases()
    if suite == "equivalence":
        return [(n, s, k, c) for n, s, k, c in transparency.cases()
                if n in ("captain-screens-recovery", "uldor-two-crossings-song-changes",
                         "hallucinating-command-and-archer")]
    raise ValueError(suite)


def verify(result, suite, check):
    catalogue.verify(result, check)
    if suite == "coordination":
        # The original small matrix and varied matrix have separate checks.
        original = {n for n, *_ in coordination.cases()}
        if result["case_name"] in original:
            coordination.verify(result, check["coordination_check"])
        else:
            coordination.verify_varied(result, check)
    elif suite == "adversarial":
        audit.verify(result, check)
    elif suite in ("visibility", "equivalence"):
        transparency.verify(result, check)
    elif suite == "endurance":
        coordination.verify_varied(result, {k: v for k, v in check.items()
            if k in ("solo", "shots")})
    result.update(suite=suite, audit_check=check)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", default="catalogue,legacy,baseline,coordination,adversarial,visibility,water,extended,endurance,persistence,equivalence")
    parser.add_argument("--seed", type=int, default=20261115)
    parser.add_argument("--filter", default="")
    parser.add_argument("--output-dir", type=Path, default=OUT)
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--exclude-report", type=Path, action="append", default=[],
                        help="Skip completed cases with matching production/fixture provenance")
    args = parser.parse_args()
    destination = args.output_dir.resolve()
    catalogue.OUT = destination
    catalogue.instrument()
    exe, env = audit.build()
    env["SIL_LOG_LEVEL"] = "DEBUG"
    modules = [Path(m.__file__) for m in (catalogue, play, engine, transparency, audit, coordination)]
    modules.append(Path(__file__))
    manifest = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(set(modules))}
    manifest["production_executable"] = hashlib.sha256((play.BUILD / "sil-more.exe").read_bytes()).hexdigest()
    manifest["fixture_executable"] = hashlib.sha256(exe.read_bytes()).hexdigest()
    manifest["fixture_source"] = hashlib.sha256(exe.with_suffix(".c").read_bytes()).hexdigest()
    stable_manifest = {k: v for k, v in manifest.items() if k != "fixture_executable"}
    env["AI_PLAYTEST_SIGNATURE"] = hashlib.sha256(
        (env["AI_PLAYTEST_SIGNATURE"] + json.dumps(stable_manifest, sort_keys=True)).encode()).hexdigest()
    (destination / "source-manifest.json").write_text(json.dumps(manifest, indent=2))
    report = destination / f"audit-{args.seed}.json"
    previous = json.loads(report.read_text()) if args.resume and report.exists() else {}
    results = previous.get("results", [])
    complete = {(r["suite"], r["case_name"]) for r in results}
    excluded = set()
    for earlier in args.exclude_report:
        provenance = json.loads((earlier.parent / "source-manifest.json").read_text())
        for key in ("production_executable", "fixture_source"):
            assert provenance[key] == manifest[key], f"Excluded report has different {key}"
        excluded.update((r["suite"], r["case_name"])
                        for r in json.loads(earlier.read_text()).get("results", []))
    failures, comparisons = [], previous.get("comparisons", [])
    for suite in args.suite.split(","):
        for name, spec, *tail in cases(suite, args.seed):
            if args.filter and not re.search(args.filter, name):
                continue
            if (suite, name) in excluded:
                continue
            if (suite, name) in complete:
                assert all(r["build_signature"] == env["AI_PLAYTEST_SIGNATURE"] for r in results), (
                    "Cannot resume results from different sources or executable")
                continue
            spec = dict(copy.deepcopy(spec), seed=args.seed)
            try:
                if suite == "persistence":
                    prefix, suffix, check = tail
                    result = engine.run_persistence(exe, audit.case_env(env, spec), name, spec, prefix, suffix, check)
                    for branch in result["branches"]:
                        catalogue.verify(branch, {})
                else:
                    keys, check = tail
                    result = play.run(exe, audit.case_env(env, spec), suite + "-" + name, spec, keys)
                    result["case_name"] = name
                    verify(result, suite, check)
                    if suite == "equivalence":
                        for level in ("WARN", "TRACE"):
                            other = play.run(exe, audit.case_env(dict(env, SIL_LOG_LEVEL=level), spec),
                                             suite + "-" + name + "-" + level, spec, keys)
                            transparency.compare_runs(result, other)
                            other.update(suite=suite, case_name=name + "-" + level)
                            results.append(other)
                            comparisons.append(dict(reference=result["profile"], other=other["profile"], level=level))
                result.update(suite=suite, case_name=name)
                results.append(result)
            except Exception as error:
                failure = dict(suite=suite, name=name, error=str(error), traceback=traceback.format_exc())
                failures.append(failure)
                print(json.dumps(failure), flush=True)
            temporary = report.with_suffix(".tmp")
            temporary.write_text(json.dumps(dict(results=results, failures=failures,
                                                comparisons=comparisons), indent=2))
            temporary.replace(report)
    print(f"Audit: {len(results)} passed cases, {len(failures)} failures", flush=True)
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

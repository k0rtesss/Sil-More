#!/usr/bin/env python3

# Copyright (C) 2025-2026 Sil-More contributors
#
# This file is part of Sil-More.
#
# Sil-More is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# Sil-More is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
# for more details.

"""Audit oversized source files against a checked-in baseline."""

from __future__ import annotations

import argparse
import fnmatch
import json
import sys
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BASELINE = REPO_ROOT / "tests" / "source_size_audit_baseline.json"
EXCLUDED_SOURCE_DIR_NAMES = {".venv"}


@dataclass(frozen=True)
class MetricResult:
    label: str
    files: int
    value: int
    notes: str
    file_matches: dict[str, int]


def read_text(path: Path) -> str:
    return path.read_bytes().decode("utf-8", errors="ignore")


def count_lines(path: Path) -> int:
    data = path.read_bytes()
    if not data:
        return 0
    newline_count = data.count(b"\n")
    if data.endswith(b"\n"):
        return newline_count
    return newline_count + 1


def rel_posix(path: Path) -> str:
    return path.relative_to(REPO_ROOT).as_posix()


def matches_any(path: str, patterns: list[str]) -> bool:
    return any(fnmatch.fnmatch(path, pattern) for pattern in patterns)


def is_excluded_source_path(path: Path) -> bool:
    try:
        relative = path.relative_to(REPO_ROOT / "src")
    except ValueError:
        return False

    return bool(relative.parts and relative.parts[0] in EXCLUDED_SOURCE_DIR_NAMES)


def src_files(suffixes: set[str]) -> list[Path]:
    return sorted(
        path
        for path in (REPO_ROOT / "src").rglob("*")
        if path.is_file() and path.suffix in suffixes
        and not is_excluded_source_path(path)
    )


def load_payload(path: Path) -> dict:
    return json.loads(read_text(path))


def policy_from_payload(payload: dict) -> dict:
    return payload.get("policy", {})


def metric_for_threshold(policy: dict, threshold: dict) -> MetricResult:
    allowlist = list(policy.get("allowlist_globs", []))
    allowlist.extend(threshold.get("allowlist_globs", []))

    suffixes = set(threshold.get("suffixes", []))
    max_lines = int(threshold.get("max_lines", 0))
    file_matches: dict[str, int] = {}

    for path in src_files(suffixes):
        rel_path = rel_posix(path)
        if matches_any(rel_path, allowlist):
            continue
        lines = count_lines(path)
        if lines > max_lines:
            file_matches[rel_path] = lines

    return MetricResult(
        label=threshold["label"],
        files=len(file_matches),
        value=sum(file_matches.values()),
        notes=threshold.get("notes", ""),
        file_matches=dict(sorted(file_matches.items())),
    )


def build_audit(policy: dict) -> dict:
    thresholds = policy.get("thresholds", [])
    metrics: dict[str, dict] = {}

    for threshold in thresholds:
        result = metric_for_threshold(policy, threshold)
        metrics[threshold["key"]] = {
            "label": result.label,
            "files": result.files,
            "value": result.value,
            "notes": result.notes,
            "file_matches": result.file_matches,
        }

    return {
        "repo_root": str(REPO_ROOT),
        "policy_summary": {
            "allowlist_entries": len(policy.get("allowlist_globs", [])),
            "threshold_entries": len(thresholds),
        },
        "audits": {
            "source_size": {
                "label": "Source size audit",
                "notes": "Wave 0 baseline for oversized non-vendor source files.",
                "metrics": metrics,
            }
        },
    }


def compare_against_baseline(current: dict, baseline_path: Path) -> list[str]:
    baseline = load_payload(baseline_path)
    failures: list[str] = []
    current_metrics = current.get("audits", {}).get("source_size", {}).get("metrics", {})
    baseline_metrics = (
        baseline.get("audits", {}).get("source_size", {}).get("metrics", {})
    )

    for metric_key, current_metric in current_metrics.items():
        expected_metric = baseline_metrics.get(metric_key)
        if expected_metric is None:
            failures.append(f"Missing metric in baseline source_size: {metric_key}")
            continue

        if current_metric["files"] > expected_metric.get("files", 0):
            failures.append(
                f"[source_size] {current_metric['label']}: files "
                f"{current_metric['files']} > baseline {expected_metric.get('files', 0)}"
            )
        if current_metric["value"] > expected_metric.get("value", 0):
            failures.append(
                f"[source_size] {current_metric['label']}: total lines "
                f"{current_metric['value']} > baseline {expected_metric.get('value', 0)}"
            )

        expected_matches = expected_metric.get("file_matches", {})
        for path, lines in current_metric["file_matches"].items():
            baseline_lines = expected_matches.get(path)
            if baseline_lines is None:
                failures.append(
                    f"[source_size] {current_metric['label']}: new oversized file {path}"
                )
            elif lines > baseline_lines:
                failures.append(
                    f"[source_size] {current_metric['label']}: {path} lines "
                    f"{lines} > baseline {baseline_lines}"
                )

    return failures


def render_summary(audit: dict, include_details: bool) -> str:
    lines = [
        "Source size audit",
        f"Repo root: {audit['repo_root']}",
        (
            "Policy summary: "
            f"allowlist={audit['policy_summary']['allowlist_entries']}, "
            f"thresholds={audit['policy_summary']['threshold_entries']}"
        ),
    ]
    audit_payload = audit["audits"]["source_size"]
    lines.append(f"[source_size] {audit_payload['label']}")
    lines.append("Metric                                  Files   Value")
    lines.append("--------------------------------------- ------ ------")
    for metric in audit_payload.get("metrics", {}).values():
        lines.append(
            f"{metric['label'][:39]:39} {metric['files']:6d} {metric['value']:6d}"
        )
        if include_details and metric["file_matches"]:
            lines.append(f"  notes: {metric['notes']}")
            for path, count in metric["file_matches"].items():
                lines.append(f"  {count:6d}  {path}")
            lines.append("")
    return "\n".join(lines).rstrip()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Audit oversized source files against a checked-in baseline."
    )
    parser.add_argument(
        "--details",
        action="store_true",
        help="Include per-file counts in text output.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit JSON instead of the text summary.",
    )
    parser.add_argument(
        "--check",
        nargs="?",
        const=str(DEFAULT_BASELINE),
        metavar="BASELINE",
        help="Fail if any audited metric exceeds the checked-in baseline.",
    )
    parser.add_argument(
        "--config",
        metavar="CONFIG",
        help="Load policy from a specific JSON file.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    config_path = Path(args.config) if args.config else DEFAULT_BASELINE
    if not config_path.is_absolute():
        config_path = (REPO_ROOT / config_path).resolve()

    policy: dict = {}
    if config_path.is_file():
        policy = policy_from_payload(load_payload(config_path))

    audit = build_audit(policy)

    if args.json:
        print(json.dumps(audit, indent=2, sort_keys=True))
    else:
        print(render_summary(audit, include_details=args.details))

    if not args.check:
        return 0

    baseline_path = Path(args.check)
    if not baseline_path.is_absolute():
        baseline_path = (REPO_ROOT / baseline_path).resolve()
    if not baseline_path.is_file():
        print(f"Missing baseline: {baseline_path}", file=sys.stderr)
        return 2

    failures = compare_against_baseline(audit, baseline_path)
    if failures:
        print("")
        print(f"Baseline check failed: {baseline_path}")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print("")
    print(f"Baseline check passed: {baseline_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

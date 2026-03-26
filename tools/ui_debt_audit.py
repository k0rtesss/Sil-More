#!/usr/bin/env python3
"""Audit legacy UI debt hotspots for the UI architecture migration."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable


REPO_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = REPO_ROOT / "src"
DEFAULT_BASELINE = REPO_ROOT / "tests" / "ui_debt_audit_baseline.json"
SOURCE_SUFFIXES = {".c", ".h"}
COMMENT_RE = re.compile(r"//.*?$|/\*.*?\*/", re.MULTILINE | re.DOTALL)
PLATFORM_SDL_GLOBS = (
    "src/main-sdl.c",
    "src/main-sdl.h",
    "src/platform-ui.h",
    "src/sdl-*.c",
    "src/sdl-*.h",
)


@dataclass(frozen=True)
class MetricSpec:
    key: str
    label: str
    pattern: re.Pattern[str]
    exclude_paths: tuple[str, ...] = ()
    exclude_globs: tuple[str, ...] = ()
    notes: str = ""


METRICS = (
    MetricSpec(
        key="inkey_calls",
        label="inkey() call sites",
        pattern=re.compile(r"\binkey\s*\("),
        exclude_paths=("src/externs.h", "src/util-input.c"),
        notes="Excludes the declaration in externs.h and the legacy implementation in util-input.c.",
    ),
    MetricSpec(
        key="screen_overlay_calls",
        label="screen_save()/screen_load() call sites",
        pattern=re.compile(r"\b(?:screen_save|screen_load)\s*\("),
        exclude_paths=("src/externs.h", "src/util-message.c"),
        notes="Excludes declarations and the screen stack implementation owner.",
    ),
    MetricSpec(
        key="term_render_control_calls",
        label="direct Term_* render/control calls",
        pattern=re.compile(r"\bTerm_(?!inkey\b|key_\w+\b|xtra\b)[A-Za-z0-9_]+\s*\("),
        exclude_paths=("src/z-term.c", "src/z-term.h"),
        notes="Counts direct Term_* render/control usage outside z-term declarations/implementation; excludes input queue helpers and Term_xtra().",
    ),
    MetricSpec(
        key="platform_ui_includes",
        label="#include \"platform-ui.h\"",
        pattern=re.compile(r'^\s*#\s*include\s+"platform-ui\.h"', re.MULTILINE),
        notes="Counts direct platform-ui.h imports across the live source tree.",
    ),
    MetricSpec(
        key="sdl_named_ui_calls_outside_platform",
        label="get_sdl_*/set_sdl_* usage outside platform code",
        pattern=re.compile(r"\b(?:get|set)_sdl_[A-Za-z0-9_]+\s*\("),
        exclude_globs=PLATFORM_SDL_GLOBS,
        notes="Excludes SDL platform implementation files and the platform-ui.h declaration surface.",
    ),
)


def strip_comments(text: str) -> str:
    return COMMENT_RE.sub("", text)


def is_source_file(path: Path) -> bool:
    return path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES


def iter_source_files() -> Iterable[Path]:
    for path in sorted(SOURCE_ROOT.rglob("*")):
        if is_source_file(path):
            yield path


def rel_path(path: Path) -> str:
    return path.relative_to(REPO_ROOT).as_posix()


def is_excluded(path: Path, spec: MetricSpec) -> bool:
    relative = rel_path(path)
    if relative in spec.exclude_paths:
        return True
    return any(path.match(glob) for glob in spec.exclude_globs)


def collect_metric(spec: MetricSpec, files: Iterable[Path]) -> dict:
    file_matches: Dict[str, int] = {}

    for path in files:
        if is_excluded(path, spec):
            continue

        text = strip_comments(path.read_text(encoding="utf-8", errors="ignore"))
        match_count = sum(1 for _ in spec.pattern.finditer(text))
        if match_count:
            file_matches[rel_path(path)] = match_count

    return {
        "label": spec.label,
        "files": len(file_matches),
        "matches": sum(file_matches.values()),
        "notes": spec.notes,
        "file_matches": dict(sorted(file_matches.items())),
    }


def collect_audit() -> dict:
    files = tuple(iter_source_files())
    metrics = {spec.key: collect_metric(spec, files) for spec in METRICS}
    return {
        "version": 1,
        "repo_root": str(REPO_ROOT),
        "scope": ["src/**/*.c", "src/**/*.h"],
        "platform_sdl_exclusions": list(PLATFORM_SDL_GLOBS),
        "metrics": metrics,
    }


def render_summary(audit: dict, include_details: bool) -> str:
    lines = [
        "UI debt audit",
        f"Repo root: {audit['repo_root']}",
        f"Scope: {', '.join(audit['scope'])}",
        "SDL platform exclusions for get_sdl_*/set_sdl_*: "
        + ", ".join(audit["platform_sdl_exclusions"]),
        "",
        f"{'Metric':39} {'Files':>5} {'Matches':>7}",
        f"{'-' * 39} {'-' * 5} {'-' * 7}",
    ]

    for spec in METRICS:
        metric = audit["metrics"][spec.key]
        lines.append(
            f"{metric['label'][:39]:39} {metric['files']:5d} {metric['matches']:7d}"
        )
        if include_details and metric["file_matches"]:
            lines.append(f"  notes: {metric['notes']}")
            for path, count in metric["file_matches"].items():
                lines.append(f"  {count:4d}  {path}")
            lines.append("")

    return "\n".join(lines)


def compare_against_baseline(audit: dict, baseline_path: Path) -> list[str]:
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    failures: list[str] = []

    for spec in METRICS:
        current = audit["metrics"][spec.key]
        expected = baseline["metrics"][spec.key]

        if current["files"] > expected["files"]:
            failures.append(
                f"{current['label']}: files {current['files']} > baseline {expected['files']}"
            )
        if current["matches"] > expected["matches"]:
            failures.append(
                f"{current['label']}: matches {current['matches']} > baseline {expected['matches']}"
            )

    return failures


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit the audit as JSON.",
    )
    parser.add_argument(
        "--details",
        action="store_true",
        help="Include per-file match counts in text output.",
    )
    parser.add_argument(
        "--check",
        nargs="?",
        const=str(DEFAULT_BASELINE),
        metavar="BASELINE",
        help="Fail if any audited metric exceeds the baseline JSON.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    audit = collect_audit()

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

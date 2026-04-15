#!/usr/bin/env python3
"""Audit modernization architecture debt and folder ownership policy."""

from __future__ import annotations

import argparse
import fnmatch
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BASELINE = REPO_ROOT / "tests" / "modernization_audit_baseline.json"
COMMENT_RE = re.compile(r"//.*?$|/\*.*?\*/", re.MULTILINE | re.DOTALL)
EXTERN_DECL_RE = re.compile(r"^\s*extern\b", re.MULTILINE)
EXTERN_INCLUDE_RE = re.compile(
    r'^\s*#\s*include\s+"(?:\.\./)?externs\.h"', re.MULTILINE
)
SDL_HEADER_INCLUDE_RE = re.compile(r"^\s*#\s*include\s+<SDL3/[^>]+>", re.MULTILINE)
SDL_IO_USAGE_RE = re.compile(
    r"\b(?:SDL_IO[A-Za-z0-9_]*|SDL_IOStream|sdl_fopen|sdl_fclose)\b"
)
CMAKE_SOURCE_RE = re.compile(r"src/[A-Za-z0-9_./-]+\.c")
SOURCE_SUFFIXES = {".c", ".h"}


@dataclass(frozen=True)
class MetricResult:
    label: str
    files: int
    value: int
    notes: str
    file_matches: dict[str, int]


@dataclass(frozen=True)
class AuditMetric:
    key: str
    label: str
    notes: str
    collector: callable


@dataclass(frozen=True)
class AuditSpec:
    key: str
    label: str
    notes: str
    metrics: tuple[AuditMetric, ...]


def read_text(path: Path) -> str:
    """Load text for ASCII-oriented audits without failing on legacy comments."""
    return path.read_bytes().decode("utf-8", errors="ignore")


def count_lines(path: Path) -> int:
    data = path.read_bytes()
    if not data:
        return 0
    newline_count = data.count(b"\n")
    if data.endswith(b"\n"):
        return newline_count
    return newline_count + 1


def strip_comments(text: str) -> str:
    return COMMENT_RE.sub("", text)


def rel_posix(path: Path) -> str:
    return path.relative_to(REPO_ROOT).as_posix()


def matches_any(path: str, patterns: list[str]) -> bool:
    return any(fnmatch.fnmatch(path, pattern) for pattern in patterns)


def src_files(suffixes: set[str]) -> list[Path]:
    return sorted(
        path
        for path in (REPO_ROOT / "src").rglob("*")
        if path.is_file() and path.suffix in suffixes
    )


def src_root_files(suffix: str) -> list[Path]:
    return sorted((REPO_ROOT / "src").glob(f"*{suffix}"))


def source_text_map() -> dict[str, str]:
    texts: dict[str, str] = {}
    for path in src_files(SOURCE_SUFFIXES):
        texts[rel_posix(path)] = strip_comments(read_text(path))
    return texts


def load_payload(path: Path) -> dict:
    return json.loads(read_text(path))


def policy_from_payload(payload: dict) -> dict:
    return payload.get("policy", {})


def acceptance_from_payload(payload: dict) -> dict:
    return payload.get("acceptance_criteria", {})


def metric_for_single_file(
    path: str,
    label: str,
    notes: str,
    value: int,
) -> MetricResult:
    return MetricResult(
        label=label,
        files=1,
        value=value,
        notes=notes,
        file_matches={path: value},
    )


def metric_extern_header_lines(_: dict, __: dict[str, str]) -> MetricResult:
    path = REPO_ROOT / "src" / "externs.h"
    value = count_lines(path)
    return metric_for_single_file(
        "src/externs.h",
        "externs.h line count",
        "Tracks the transitional umbrella header size. The finish-line target is under 400 lines.",
        value,
    )


def metric_extern_decl_count(_: dict, __: dict[str, str]) -> MetricResult:
    path = REPO_ROOT / "src" / "externs.h"
    value = len(EXTERN_DECL_RE.findall(read_text(path)))
    return metric_for_single_file(
        "src/externs.h",
        "extern declarations in externs.h",
        "Tracks how much runtime ownership still lives in the compatibility umbrella header.",
        value,
    )


def metric_extern_include_sites(_: dict, texts: dict[str, str]) -> MetricResult:
    file_matches: dict[str, int] = {}
    for rel_path, text in texts.items():
        if not rel_path.startswith("src/"):
            continue
        count = len(EXTERN_INCLUDE_RE.findall(text))
        if count:
            file_matches[rel_path] = count
    return MetricResult(
        label='direct #include "externs.h" sites',
        files=len(file_matches),
        value=len(file_matches),
        notes="Tracks the direct include fan-out of the compatibility header.",
        file_matches=file_matches,
    )


def metric_variable_source_lines(_: dict, __: dict[str, str]) -> MetricResult:
    path = REPO_ROOT / "src" / "variable.c"
    value = count_lines(path)
    return metric_for_single_file(
        "src/variable.c",
        "variable.c line count",
        "Tracks the remaining size of the global runtime ownership bucket.",
        value,
    )


def metric_root_src_c_files(_: dict, __: dict[str, str]) -> MetricResult:
    file_matches = {rel_posix(path): 1 for path in src_root_files(".c")}
    return MetricResult(
        label="root-level src/*.c files",
        files=len(file_matches),
        value=len(file_matches),
        notes="Tracks root implementation file count to ensure the root source bucket stops growing.",
        file_matches=file_matches,
    )


def metric_root_src_h_files(_: dict, __: dict[str, str]) -> MetricResult:
    file_matches = {rel_posix(path): 1 for path in src_root_files(".h")}
    return MetricResult(
        label="root-level src/*.h files",
        files=len(file_matches),
        value=len(file_matches),
        notes="Tracks root header count while the ownership surface is still transitional.",
        file_matches=file_matches,
    )


def metric_nonplatform_sdl_header_includes(
    policy: dict, texts: dict[str, str]
) -> MetricResult:
    platform_globs = policy.get("platform_boundary_globs", [])
    file_matches: dict[str, int] = {}
    for rel_path, text in texts.items():
        if matches_any(rel_path, platform_globs):
            continue
        count = len(SDL_HEADER_INCLUDE_RE.findall(text))
        if count:
            file_matches[rel_path] = count
    return MetricResult(
        label="SDL3 header includes outside platform boundary",
        files=len(file_matches),
        value=len(file_matches),
        notes="Tracks direct SDL header imports that still leak into non-platform code.",
        file_matches=file_matches,
    )


def metric_nonplatform_sdl_io_usages(
    policy: dict, texts: dict[str, str]
) -> MetricResult:
    platform_globs = policy.get("platform_boundary_globs", [])
    file_matches: dict[str, int] = {}
    for rel_path, text in texts.items():
        if matches_any(rel_path, platform_globs):
            continue
        count = len(SDL_IO_USAGE_RE.findall(text))
        if count:
            file_matches[rel_path] = count
    return MetricResult(
        label="SDL I/O symbols outside platform boundary",
        files=len(file_matches),
        value=len(file_matches),
        notes="Tracks SDL_IO*, SDL_IOStream, and sdl_fopen/sdl_fclose usage outside platform-facing code.",
        file_matches=file_matches,
    )


def metric_unallowlisted_root_src_c_files(
    policy: dict, __: dict[str, str]
) -> MetricResult:
    allowlist = policy.get("root_src_c_allowlist", [])
    file_matches = {
        rel_posix(path): 1
        for path in src_root_files(".c")
        if not matches_any(rel_posix(path), allowlist)
    }
    return MetricResult(
        label="root src/*.c files outside allowlist",
        files=len(file_matches),
        value=len(file_matches),
        notes="Fails when new root implementation files appear outside the Wave 0 temporary allowlist.",
        file_matches=file_matches,
    )


def metric_folder_rule_violations(policy: dict, __: dict[str, str]) -> MetricResult:
    file_matches: dict[str, int] = {}
    allowlist = policy.get("folder_rule_allowlist", [])
    rules = policy.get("folder_rules", [])
    for path in src_files(SOURCE_SUFFIXES):
        rel_path = rel_posix(path)
        if matches_any(rel_path, allowlist):
            continue
        for rule in rules:
            match_globs = rule.get("match_globs", [])
            allowed_globs = rule.get("allowed_globs", [])
            if not match_globs or not matches_any(rel_path, match_globs):
                continue
            if not matches_any(rel_path, allowed_globs):
                key = f"{rel_path} [{rule['key']}]"
                file_matches[key] = 1
    unique_files = {
        entry.split(" [", 1)[0] for entry in file_matches
    }
    return MetricResult(
        label="folder ownership rule violations",
        files=len(unique_files),
        value=len(file_matches),
        notes="Tracks files whose placement violates the explicit Wave 0 folder mapping rules.",
        file_matches=file_matches,
    )


def metric_source_files_missing_from_cmake(
    policy: dict, __: dict[str, str]
) -> MetricResult:
    cmake_text = read_text(REPO_ROOT / "CMakeLists.txt")
    referenced = {
        match.group(0)
        for match in CMAKE_SOURCE_RE.finditer(cmake_text)
    }
    allowlist = policy.get("source_files_missing_from_cmake_allowlist", [])
    actual = {
        rel_posix(path)
        for path in src_files({".c"})
    }
    missing = sorted(
        path
        for path in actual
        if path not in referenced and not matches_any(path, allowlist)
    )
    file_matches = {path: 1 for path in missing}
    return MetricResult(
        label="src/*.c files missing from root CMakeLists.txt",
        files=len(file_matches),
        value=len(file_matches),
        notes="Tracks translation units under src/ that are not referenced by the root source lists, excluding intentional legacy-note exceptions.",
        file_matches=file_matches,
    )


def metric_cmake_referenced_missing_on_disk(
    _: dict, __: dict[str, str]
) -> MetricResult:
    cmake_text = read_text(REPO_ROOT / "CMakeLists.txt")
    referenced = sorted(
        {
            match.group(0)
            for match in CMAKE_SOURCE_RE.finditer(cmake_text)
        }
    )
    file_matches = {
        path: 1 for path in referenced if not (REPO_ROOT / path).is_file()
    }
    return MetricResult(
        label="CMake source entries missing on disk",
        files=len(file_matches),
        value=len(file_matches),
        notes="Tracks stale source-list entries that point at nonexistent files.",
        file_matches=file_matches,
    )


AUDITS = (
    AuditSpec(
        key="architecture",
        label="Architecture debt audit",
        notes="Wave 0 baseline for the shared modernization debt surface.",
        metrics=(
            AuditMetric(
                key="externs_header_lines",
                label="externs.h line count",
                notes="Tracks the compatibility umbrella header size.",
                collector=metric_extern_header_lines,
            ),
            AuditMetric(
                key="extern_declarations",
                label="extern declarations in externs.h",
                notes="Tracks runtime ownership still exposed through externs.h.",
                collector=metric_extern_decl_count,
            ),
            AuditMetric(
                key="extern_include_sites",
                label='direct #include "externs.h" sites',
                notes="Tracks the include fan-out of externs.h.",
                collector=metric_extern_include_sites,
            ),
            AuditMetric(
                key="variable_source_lines",
                label="variable.c line count",
                notes="Tracks the remaining global-state ownership bucket.",
                collector=metric_variable_source_lines,
            ),
            AuditMetric(
                key="root_src_c_files",
                label="root-level src/*.c files",
                notes="Tracks root implementation file count.",
                collector=metric_root_src_c_files,
            ),
            AuditMetric(
                key="root_src_h_files",
                label="root-level src/*.h files",
                notes="Tracks root header count.",
                collector=metric_root_src_h_files,
            ),
            AuditMetric(
                key="nonplatform_sdl_header_includes",
                label="SDL3 header includes outside platform boundary",
                notes="Tracks non-platform SDL header leakage.",
                collector=metric_nonplatform_sdl_header_includes,
            ),
            AuditMetric(
                key="nonplatform_sdl_io_usages",
                label="SDL I/O symbols outside platform boundary",
                notes="Tracks non-platform SDL I/O leakage.",
                collector=metric_nonplatform_sdl_io_usages,
            ),
        ),
    ),
    AuditSpec(
        key="folder_ownership",
        label="Folder ownership audit",
        notes="Wave 0 guardrail against growing the root src/ bucket or placing files outside their owning subsystem.",
        metrics=(
            AuditMetric(
                key="unallowlisted_root_src_c_files",
                label="root src/*.c files outside allowlist",
                notes="Tracks new root implementation files outside the temporary allowlist.",
                collector=metric_unallowlisted_root_src_c_files,
            ),
            AuditMetric(
                key="folder_rule_violations",
                label="folder ownership rule violations",
                notes="Tracks explicit folder placement policy violations.",
                collector=metric_folder_rule_violations,
            ),
            AuditMetric(
                key="source_files_missing_from_cmake",
                label="src/*.c files missing from root CMakeLists.txt",
                notes="Tracks source files not listed in the canonical build source lists.",
                collector=metric_source_files_missing_from_cmake,
            ),
            AuditMetric(
                key="cmake_referenced_missing_on_disk",
                label="CMake source entries missing on disk",
                notes="Tracks stale or invalid source-list references.",
                collector=metric_cmake_referenced_missing_on_disk,
            ),
        ),
    ),
)

AUDITS_BY_KEY = {audit.key: audit for audit in AUDITS}


def build_audit(policy: dict, audit_keys: list[str]) -> dict:
    texts = source_text_map()
    selected = [AUDITS_BY_KEY[key] for key in audit_keys]
    payload = {
        "repo_root": str(REPO_ROOT),
        "policy_summary": {
            "root_src_c_allowlist_entries": len(policy.get("root_src_c_allowlist", [])),
            "folder_rule_entries": len(policy.get("folder_rules", [])),
            "platform_boundary_globs": len(policy.get("platform_boundary_globs", [])),
            "cmake_missing_allowlist_entries": len(
                policy.get("source_files_missing_from_cmake_allowlist", [])
            ),
        },
        "audits": {},
    }
    for audit in selected:
        metrics: dict[str, dict] = {}
        for metric in audit.metrics:
            result = metric.collector(policy, texts)
            metrics[metric.key] = {
                "label": result.label,
                "files": result.files,
                "value": result.value,
                "notes": result.notes,
                "file_matches": dict(sorted(result.file_matches.items())),
            }
        payload["audits"][audit.key] = {
            "label": audit.label,
            "notes": audit.notes,
            "metrics": metrics,
        }
    return payload


def compare_against_baseline(current: dict, baseline_path: Path) -> list[str]:
    baseline = load_payload(baseline_path)
    failures: list[str] = []
    baseline_audits = baseline.get("audits", {})
    for audit_key, current_audit in current.get("audits", {}).items():
        expected_audit = baseline_audits.get(audit_key)
        if expected_audit is None:
            failures.append(f"Missing audit in baseline: {audit_key}")
            continue
        expected_metrics = expected_audit.get("metrics", {})
        for metric_key, current_metric in current_audit.get("metrics", {}).items():
            expected_metric = expected_metrics.get(metric_key)
            if expected_metric is None:
                failures.append(
                    f"Missing metric in baseline {audit_key}: {metric_key}"
                )
                continue
            if current_metric["files"] > expected_metric.get("files", 0):
                failures.append(
                    f"[{audit_key}] {current_metric['label']}: files "
                    f"{current_metric['files']} > baseline {expected_metric.get('files', 0)}"
                )
            if current_metric["value"] > expected_metric.get("value", 0):
                failures.append(
                    f"[{audit_key}] {current_metric['label']}: value "
                    f"{current_metric['value']} > baseline {expected_metric.get('value', 0)}"
                )
    return failures


def render_summary(audit: dict, include_details: bool) -> str:
    lines = [
        "Modernization audit",
        f"Repo root: {audit['repo_root']}",
        (
            "Policy summary: "
            f"root-src allowlist={audit['policy_summary']['root_src_c_allowlist_entries']}, "
            f"folder rules={audit['policy_summary']['folder_rule_entries']}, "
            f"platform globs={audit['policy_summary']['platform_boundary_globs']}, "
            f"cmake allowlist={audit['policy_summary']['cmake_missing_allowlist_entries']}"
        ),
    ]
    for audit_key, audit_payload in audit.get("audits", {}).items():
        lines.append(f"[{audit_key}] {audit_payload['label']}")
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
        if not include_details:
            lines.append("")
    return "\n".join(lines).rstrip()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Audit modernization debt and folder ownership policy."
    )
    parser.add_argument(
        "--audit",
        action="append",
        choices=sorted(AUDITS_BY_KEY),
        help="Limit output to a specific audit family. Can be passed multiple times.",
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
        help="Load policy and acceptance criteria from a specific JSON file.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    audit_keys = args.audit or sorted(AUDITS_BY_KEY)

    config_path = Path(args.config) if args.config else DEFAULT_BASELINE
    if not config_path.is_absolute():
        config_path = (REPO_ROOT / config_path).resolve()

    policy: dict = {}
    if config_path.is_file():
        policy = policy_from_payload(load_payload(config_path))

    audit = build_audit(policy, audit_keys)

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

#!/usr/bin/env python3
"""Audit legacy UI debt hotspots for the UI architecture migration."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Dict, Iterable


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BASELINE = REPO_ROOT / "tests" / "ui_debt_audit_baseline.json"
SOURCE_SUFFIXES = {".c", ".h"}
PREF_SUFFIXES = {".prf"}
AUDIT_SUFFIXES = SOURCE_SUFFIXES | PREF_SUFFIXES
COMMENT_RE = re.compile(r"//.*?$|/\*.*?\*/", re.MULTILINE | re.DOTALL)
PRF_COMMENT_LINE_RE = re.compile(r"^\s*#.*$", re.MULTILINE)
PLATFORM_SDL_GLOBS = (
    "src/main-sdl.c",
    "src/main-sdl.h",
    "src/platform-*.h",
    "src/sdl-*.c",
    "src/sdl-*.h",
)
TERMINAL_MODEL_SCOPE = ("src/**/*.c", "src/**/*.h")
TERMINAL_KERNEL_SCOPE = ("src/**/*.c", "src/**/*.h")
MOVEMENT_INPUT_SCOPE = (
    "src/externs.h",
    "src/util-input.c",
    "src/dungeon.c",
    "src/targeting.c",
    "src/main-sdl.c",
    "src/sdl-main-internal.h",
    "src/cmd/movement/cmd-movement.c",
    "src/cmd/world/cmd-interact.c",
    "src/cmd/ui/cmd-ui-settings.c",
    "lib/pref/pref.prf",
    "lib/pref/pref-sdl.prf",
)


@dataclass(frozen=True)
class MetricSpec:
    key: str
    label: str
    pattern: re.Pattern[str]
    include_paths: tuple[str, ...] = ()
    include_globs: tuple[str, ...] = ()
    exclude_paths: tuple[str, ...] = ()
    exclude_globs: tuple[str, ...] = ()
    notes: str = ""


@dataclass(frozen=True)
class AuditSpec:
    key: str
    label: str
    scope: tuple[str, ...]
    metrics: tuple[MetricSpec, ...]
    notes: str = ""
    platform_sdl_exclusions: tuple[str, ...] = ()


UI0_METRICS = (
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
        notes="Excludes SDL platform implementation files and platform-facing declaration headers.",
    ),
)

MOVEMENT_INPUT_METRICS = (
    MetricSpec(
        key="movement_inkey_waits",
        label="movement direction char fallbacks",
        pattern=re.compile(r"\btarget_dir\s*\(\s*ch\s*\)"),
        include_paths=("src/targeting.c",),
        notes="Tracks remaining character-to-direction fallback in get_aim_dir()/get_rep_dir() after semantic movement input is wired on the SDL path.",
    ),
    MetricSpec(
        key="movement_request_command_ownership",
        label="movement request_command() ownership",
        pattern=re.compile(r"\brequest_command\s*\("),
        include_paths=("src/externs.h", "src/util-input.c", "src/dungeon.c"),
        notes="Tracks the legacy command acquisition loop that still owns top-level movement input.",
    ),
    MetricSpec(
        key="movement_flush_calls",
        label="movement-related flush() usage",
        pattern=re.compile(r"\bflush\s*\("),
        include_paths=(
            "src/dungeon.c",
            "src/cmd/movement/cmd-movement.c",
            "src/cmd/world/cmd-interact.c",
        ),
        notes="Tracks delayed flush ownership in movement gameplay consumers after excluding the generic flush implementation and unrelated settings/macro helpers.",
    ),
    MetricSpec(
        key="movement_sdl_direction_macro_bridge",
        label="SDL directional macro bridge symbols",
        pattern=re.compile(
            r"\b(?:sdl_send_modified_direction_action|sdl_try_send_modified_direction_key|sdl_try_send_modified_direction_event|sdl_gamepad_send_direction_mods)\s*\("
        ),
        include_paths=("src/main-sdl.c", "src/sdl-main-internal.h"),
        notes="Tracks the SDL helper family that still resolves directional modifiers by feeding legacy macro-trigger input.",
    ),
    MetricSpec(
        key="movement_pref_keymap_defaults",
        label="movement action defaults in pref.prf",
        pattern=re.compile(
            r"^A:(?:z|Z|/5|;[12346789]|\.[12346789])$",
            re.MULTILINE,
        ),
        include_paths=("lib/pref/pref.prf",),
        notes="Counts active shipped movement action defaults in pref.prf after removing commented lines.",
    ),
    MetricSpec(
        key="movement_pref_sdl_macro_defaults",
        label="movement macro defaults in pref-sdl.prf",
        pattern=re.compile(
            r"^(?:A:\\a\\(?:\\\.[12346789]|\\Z|\\/[123456789])|P:\^_[SC]x[0-9A-Fa-f]{2}\\r)$",
            re.MULTILINE,
        ),
        include_paths=("lib/pref/pref-sdl.prf",),
        notes="Counts active shift/control directional macro defaults in pref-sdl.prf after removing commented lines.",
    ),
)

TERMINAL_MODEL_METRICS = (
    MetricSpec(
        key="legacy_inkey_symbols",
        label="inkey() symbols",
        pattern=re.compile(r"\binkey\s*\("),
        notes="Counts the live legacy byte-input loop, including declarations, definitions, and callers.",
    ),
    MetricSpec(
        key="legacy_request_command_symbols",
        label="request_command() symbols",
        pattern=re.compile(r"\brequest_command\s*\("),
        notes="Tracks reintroduction of the removed request_command() API anywhere in the active source tree.",
    ),
    MetricSpec(
        key="legacy_flush_symbols",
        label="flush() symbols",
        pattern=re.compile(r"\bflush\s*\("),
        notes="Counts delayed flush ownership across the active source tree, including the remaining owner and gameplay/UI callers.",
    ),
    MetricSpec(
        key="compat_text_wrapper_symbols",
        label="compat text wrapper symbols",
        pattern=re.compile(
            r"\b(?:c_put_str|put_str|c_prt|prt|clear_from)\s*\(|\btext_out_to_screen\b"
        ),
        notes="Counts the row/column compat text authoring family, including declarations, definitions, calls, and text_out_hook bindings.",
    ),
    MetricSpec(
        key="terminal_size_query_symbols",
        label="terminal-size query symbols",
        pattern=re.compile(
            r"\b(?:platform_frame_main_view_cols|platform_frame_main_view_rows|platform_frame_active_view_cols|platform_frame_active_view_rows)\s*\("
        ),
        notes="Tracks terminal-sized layout queries that still leak into document and utility flows.",
    ),
    MetricSpec(
        key="document_op_cell_grid_symbols",
        label="document-op cell-grid symbols",
        pattern=re.compile(
            r"\bAPP_UI_DOCUMENT_OP_[A-Z0-9_]+\b|\bapp_ui_panel_add_document_text(?:_ex)?\s*\(|\bapp_ui_panel_add_document_cell_ex\s*\(|\bapp_ui_panel_add_document_cursor\s*\("
        ),
        notes="Counts the live document row/column/cell/cursor surface, including enum values and builder helpers.",
    ),
    MetricSpec(
        key="sdl_term_host_symbols",
        label="SDL term-host symbols",
        pattern=re.compile(
            r"\b(?:term_ready|term_init|term_nuke|sdl_view_link_term|sdl_term_host_redraw|sdl_redraw_all_term_hosts)\b"
        ),
        notes="Tracks the SDL view lifetime/redraw layer that still passes through terminal hosts.",
    ),
    MetricSpec(
        key="term_host_story_font_state",
        label="term-host story-font state",
        pattern=re.compile(
            r"\b(?:story_font_active|story_font_grid|story_chunk_active|sdl_apply_story_font_state|sdl_apply_story_grid_state|sdl_story_font_reset_state|sdl_story_font_set_grid|sdl_is_story_font_grid|sdl_render_story_text_grid)\b"
        ),
        notes="Tracks story-font activation, grid, and chunk state that is still stored on or driven through terminal hosts.",
    ),
)

TERMINAL_KERNEL_METRICS = (
    MetricSpec(
        key="term_xtra_symbols",
        label="TERM_XTRA_* symbols",
        pattern=re.compile(r"\bTERM_XTRA_[A-Z0-9_]+\b"),
        include_paths=("src/sdl-main-internal.h", "src/sdl-render.c"),
        notes="Tracks terminal-style xtra constants and dispatch that still drive SDL event, clear, present, delay, and react behavior.",
    ),
    MetricSpec(
        key="sdl_view_state_hook_symbols",
        label="SDL view-state hook symbols",
        pattern=re.compile(
            r"\b(?:xtra_hook|curs_hook|bigcurs_hook|wipe_hook|text_hook|pict_hook)\b"
        ),
        include_paths=("src/sdl-main-internal.h", "src/sdl-render.c"),
        notes="Tracks the legacy hook table still embedded in SDL view state and renderer setup.",
    ),
    MetricSpec(
        key="sdl_view_state_key_queue_symbols",
        label="SDL view-state key-queue symbols",
        pattern=re.compile(r"\b(?:key_queue|key_head|key_tail|key_xtra|key_size)\b"),
        include_paths=("src/sdl-main-internal.h", "src/sdl-render.c"),
        notes="Tracks the compatibility key queue still carried by the SDL view state.",
    ),
    MetricSpec(
        key="sdl_view_state_shadow_buffer_symbols",
        label="SDL shadow-buffer state symbols",
        pattern=re.compile(
            r"\b(?:wid|hgt)\b(?=\s*;)"
            r"|(?:\bx[12]\b(?=\s*;))"
            r"|(?:\b(?:old|scr)\b(?=\s*;))"
            r"|(?:state->(?:wid|hgt|x1|x2|old|scr))"
        ),
        include_paths=("src/sdl-main-internal.h", "src/sdl-render.c"),
        notes="Tracks terminal-sized dimensions, dirty spans, and shadow buffers that still back SDL view compatibility rendering.",
    ),
    MetricSpec(
        key="sdl_buffer_replay_redraw_symbols",
        label="SDL buffer-replay redraw symbols",
        pattern=re.compile(
            r"\b(?:sdl_view_copy_buffer|sdl_view_clear_dirty|sdl_view_redraw_text_row)\b"
            r"|state->scr->"
        ),
        include_paths=("src/sdl-render.c",),
        notes="Tracks redraw paths that still replay compatibility row/cell buffers instead of consuming direct render state.",
    ),
    MetricSpec(
        key="mini_screenshot_mirror_symbols",
        label="mini screenshot mirror symbols",
        pattern=re.compile(r"\bmini_screenshot_(?:char|attr)\b"),
        include_paths=("src/files.c",),
        notes="Tracks char/attr screenshot mirrors that should be replaced by semantic export data.",
    ),
)

AUDITS = (
    AuditSpec(
        key="ui0",
        label="UI0 debt audit",
        scope=("src/**/*.c", "src/**/*.h"),
        metrics=UI0_METRICS,
        notes="High-level guardrail for terminal-era UI APIs that should not grow during the extermination work.",
        platform_sdl_exclusions=PLATFORM_SDL_GLOBS,
    ),
    AuditSpec(
        key="movement_input",
        label="Movement input debt audit",
        scope=MOVEMENT_INPUT_SCOPE,
        metrics=MOVEMENT_INPUT_METRICS,
        notes="Slice-0 guardrail for the legacy movement input stack that the movement rewrite is expected to delete.",
    ),
    AuditSpec(
        key="terminal_model",
        label="Terminal-model debt audit",
        scope=TERMINAL_MODEL_SCOPE,
        metrics=TERMINAL_MODEL_METRICS,
        notes="Expanded guardrail for the remaining terminal-era UI ownership that still survives on the SDL path after the movement rewrite.",
    ),
    AuditSpec(
        key="terminal_kernel",
        label="Terminal-kernel debt audit",
        scope=TERMINAL_KERNEL_SCOPE,
        metrics=TERMINAL_KERNEL_METRICS,
        notes="Slice-6A guardrail for the internal SDL terminal kernel and export-mirror compatibility state that still survives after the public term-host surface was removed.",
    ),
)
AUDITS_BY_KEY = {audit.key: audit for audit in AUDITS}


def strip_comments(text: str) -> str:
    return COMMENT_RE.sub("", text)


def strip_pref_comments(text: str) -> str:
    return PRF_COMMENT_LINE_RE.sub("", text)


def normalize_text(path: Path) -> str:
    text = path.read_text(encoding="utf-8", errors="ignore")
    suffix = path.suffix.lower()
    if suffix in SOURCE_SUFFIXES:
        return strip_comments(text)
    if suffix in PREF_SUFFIXES:
        return strip_pref_comments(text)
    return text


def is_audited_file(path: Path) -> bool:
    return path.is_file() and path.suffix.lower() in AUDIT_SUFFIXES


def rel_path(path: Path) -> str:
    return path.relative_to(REPO_ROOT).as_posix()


def path_matches(path: Path, pattern: str) -> bool:
    return PurePosixPath(rel_path(path)).match(pattern)


def iter_scope_files(scope: tuple[str, ...]) -> Iterable[Path]:
    seen: set[str] = set()

    for pattern in scope:
        for path in sorted(REPO_ROOT.glob(pattern)):
            if not is_audited_file(path):
                continue

            relative = rel_path(path)
            if relative in seen:
                continue

            seen.add(relative)
            yield path


def is_included(path: Path, spec: MetricSpec) -> bool:
    if not spec.include_paths and not spec.include_globs:
        return True

    relative = rel_path(path)
    if relative in spec.include_paths:
        return True

    return any(path_matches(path, glob) for glob in spec.include_globs)


def is_excluded(path: Path, spec: MetricSpec) -> bool:
    relative = rel_path(path)
    if relative in spec.exclude_paths:
        return True

    return any(path_matches(path, glob) for glob in spec.exclude_globs)


def collect_metric(
    spec: MetricSpec, files: Iterable[Path], text_cache: Dict[str, str]
) -> dict:
    file_matches: Dict[str, int] = {}

    for path in files:
        if not is_included(path, spec) or is_excluded(path, spec):
            continue

        relative = rel_path(path)
        text = text_cache.setdefault(relative, normalize_text(path))
        match_count = sum(1 for _ in spec.pattern.finditer(text))
        if match_count:
            file_matches[relative] = match_count

    return {
        "label": spec.label,
        "files": len(file_matches),
        "matches": sum(file_matches.values()),
        "notes": spec.notes,
        "file_matches": dict(sorted(file_matches.items())),
    }


def collect_audit_family(audit_spec: AuditSpec) -> dict:
    files = tuple(iter_scope_files(audit_spec.scope))
    text_cache: Dict[str, str] = {}
    metrics = {
        spec.key: collect_metric(spec, files, text_cache) for spec in audit_spec.metrics
    }
    result = {
        "label": audit_spec.label,
        "scope": list(audit_spec.scope),
        "notes": audit_spec.notes,
        "metrics": metrics,
    }
    if audit_spec.platform_sdl_exclusions:
        result["platform_sdl_exclusions"] = list(audit_spec.platform_sdl_exclusions)
    return result


def collect_audit(selected_audits: tuple[str, ...]) -> dict:
    audits = {
        audit_key: collect_audit_family(AUDITS_BY_KEY[audit_key])
        for audit_key in selected_audits
    }
    result = {
        "version": 2,
        "repo_root": str(REPO_ROOT),
        "audits": audits,
    }

    # Keep the original top-level UI0 shape for compatibility with older
    # docs and local automation that still expects the legacy keys.
    if "ui0" in audits:
        result["scope"] = audits["ui0"]["scope"]
        result["metrics"] = audits["ui0"]["metrics"]
        if "platform_sdl_exclusions" in audits["ui0"]:
            result["platform_sdl_exclusions"] = audits["ui0"][
                "platform_sdl_exclusions"
            ]

    return result


def render_summary(audit: dict, include_details: bool) -> str:
    lines = [
        "UI debt audit",
        f"Repo root: {audit['repo_root']}",
    ]

    for index, (audit_key, audit_data) in enumerate(audit["audits"].items()):
        audit_spec = AUDITS_BY_KEY[audit_key]
        if index:
            lines.append("")

        lines.append(f"[{audit_key}] {audit_data['label']}")
        lines.append(f"Scope: {', '.join(audit_data['scope'])}")
        if audit_data.get("platform_sdl_exclusions"):
            lines.append(
                "SDL platform exclusions for get_sdl_*/set_sdl_*: "
                + ", ".join(audit_data["platform_sdl_exclusions"])
            )
        lines.append("")
        lines.append(f"{'Metric':39} {'Files':>5} {'Matches':>7}")
        lines.append(f"{'-' * 39} {'-' * 5} {'-' * 7}")

        for metric_spec in audit_spec.metrics:
            metric = audit_data["metrics"][metric_spec.key]
            lines.append(
                f"{metric['label'][:39]:39} {metric['files']:5d} {metric['matches']:7d}"
            )
            if include_details and metric["file_matches"]:
                lines.append(f"  notes: {metric['notes']}")
                for path, count in metric["file_matches"].items():
                    lines.append(f"  {count:4d}  {path}")
                lines.append("")

    return "\n".join(lines)


def extract_audits(payload: dict) -> dict[str, dict]:
    if "audits" in payload:
        return payload["audits"]

    # Backward-compatible view for pre-v2 baseline files.
    return {
        "ui0": {
            "label": "UI0 debt audit",
            "scope": payload.get("scope", []),
            "platform_sdl_exclusions": payload.get("platform_sdl_exclusions", []),
            "metrics": payload.get("metrics", {}),
        }
    }


def compare_against_baseline(audit: dict, baseline_path: Path) -> list[str]:
    baseline = json.loads(baseline_path.read_text(encoding="utf-8-sig"))
    current_audits = extract_audits(audit)
    baseline_audits = extract_audits(baseline)
    failures: list[str] = []

    for audit_key, current_audit in current_audits.items():
        audit_spec = AUDITS_BY_KEY[audit_key]
        expected_audit = baseline_audits.get(audit_key)

        if expected_audit is None:
            if "audits" in baseline:
                failures.append(f"Missing audit in baseline: {audit_key}")
            continue

        expected_metrics = expected_audit.get("metrics", {})
        for metric_spec in audit_spec.metrics:
            current = current_audit["metrics"][metric_spec.key]
            expected = expected_metrics.get(metric_spec.key)

            if expected is None:
                if "audits" in baseline:
                    failures.append(
                        f"Missing metric in baseline {audit_key}: {metric_spec.key}"
                    )
                continue

            if current["files"] > expected["files"]:
                failures.append(
                    f"[{audit_key}] {current['label']}: files {current['files']} > baseline {expected['files']}"
                )
            if current["matches"] > expected["matches"]:
                failures.append(
                    f"[{audit_key}] {current['label']}: matches {current['matches']} > baseline {expected['matches']}"
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
        "--audit",
        choices=("all",) + tuple(AUDITS_BY_KEY),
        default="all",
        help="Select a single audit family to emit or check. Defaults to all families.",
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
    selected_audits = (
        tuple(AUDITS_BY_KEY) if args.audit == "all" else (args.audit,)
    )
    audit = collect_audit(selected_audits)

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

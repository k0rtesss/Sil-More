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

from __future__ import annotations

import csv
import sys
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _load_cases(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def _build_item(case: dict[str, str]) -> dict:
    return {
        "name": case["name"],
        "type": case["type"],
        "tval": int(case["tval"]),
        "sval": 1,
        "flags": [token for token in case["flags"].split("|") if token],
        "att": 0,
        "evn": 0,
        "ds": 0,
        "pval": 0,
        "ps": 0,
        "pd": 0,
        "dd": 0,
        "weight": 0,
        "base_att": 0,
        "base_evn": 0,
        "base_ds": 0,
        "base_ps": 0,
        "base_pd": 0,
        "base_dd": 0,
        "base_pval": 0,
        "base_level": 0,
        "bonus_overrides": {},
        "ability_list": [],
    }


def main() -> int:
    repo_root = _repo_root()
    sys.path.insert(0, str(repo_root / "scripts"))
    import calc_artefact_difficulty as cad

    # Keep the corpus self-contained: these parity cases only exercise the
    # flag deltas that previously drifted between engine and script.
    cad.get_base_weight = lambda _tval, _sval: 0
    cad.get_base_flags = lambda _tval, _sval: set()

    cases = _load_cases(repo_root / "tests" / "smithing_parity_cases.txt")
    failures = 0

    for case in cases:
        item = _build_item(case)
        actual = cad.calculate_difficulty(item)
        expected = int(case["expected"])
        if actual != expected:
            print(
                f"parity mismatch for {case['name']}: expected {expected}, got {actual}",
                file=sys.stderr,
            )
            failures += 1

    if failures:
        print(f"{failures} smithing parity case(s) failed.", file=sys.stderr)
        return 1

    print("Smithing parity cases passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

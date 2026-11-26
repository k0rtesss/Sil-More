#!/usr/bin/env python3
"""Apply the reviewed UI translation overlays to the shipped catalogues."""

import argparse
import json
from pathlib import Path
import sys
from tempfile import NamedTemporaryFile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from check_ui_translation import available_languages, check, load, unique_object

GROUPS = ("strings", "messages")
OVERLAYS = ("ui_translation_overrides.json", "ui_translation_review2.json")


def read_overlays(locales, source_groups):
    reviewed = {locale: {} for locale in locales}
    for filename in OVERLAYS:
        path = ROOT / "tools" / filename
        overlay = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object)
        if not isinstance(overlay, dict):
            raise ValueError(f"{filename} must contain a locale map")
        for locale, entries in overlay.items():
            if locale not in reviewed or not isinstance(entries, dict):
                raise ValueError(f"{filename} has an invalid locale map for {locale}")
            for source, translation in entries.items():
                if source not in source_groups or not isinstance(translation, str):
                    raise ValueError(f"{filename} has an invalid {locale} entry for {source!r}")
            reviewed[locale].update(entries)
    return reviewed


def save(path, catalogue):
    staged = None
    try:
        with NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n",
                                dir=path.parent, delete=False) as file:
            staged = Path(file.name)
            json.dump(catalogue, file, ensure_ascii=False, indent=2)
            file.write("\n")
        staged.replace(path)
    finally:
        if staged:
            staged.unlink(missing_ok=True)


def main():
    locales = [code for code in available_languages() if code not in ("en", "de")]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--language", choices=locales)
    parser.add_argument("--check", action="store_true", help="validate without writing files")
    args = parser.parse_args()

    help_dir = ROOT / "lib/help"
    baseline = load(help_dir / "ui.de.json")
    source_groups = {source: group for group in GROUPS for source in baseline[group]}
    reviewed = read_overlays(locales, source_groups)
    updates = []
    for locale in [args.language] if args.language else locales:
        path = help_dir / f"ui.{locale}.json"
        catalogue = load(path)
        changed = 0
        for source, translation in reviewed[locale].items():
            group = catalogue[source_groups[source]]
            if source not in group:
                raise ValueError(f"{path.name} is missing the source entry {source!r}")
            if group[source] != translation:
                group[source] = translation
                changed += 1
        check(locale, catalogue, baseline)
        if changed:
            updates.append((path, catalogue, changed))

    if args.check:
        for path, _, changed in updates:
            print(f"{path.name} has {changed} unapplied overrides")
        return int(bool(updates))

    for path, catalogue, changed in updates:
        save(path, catalogue)
        print(f"Applied {changed} overrides to {path.name}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, OSError, ValueError) as error:
        raise SystemExit(f"Invalid UI catalogue or overlay: {error}") from error

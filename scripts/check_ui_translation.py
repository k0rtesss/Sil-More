#!/usr/bin/env python3
"""Validate UI catalogue coverage and formatting for every selectable locale.

This checks structural correctness, not linguistic quality. English is the
source fallback; the established German catalogue defines the translated scope.
"""
import argparse
import json
from pathlib import Path
import re
import unicodedata

ROOT = Path(__file__).resolve().parents[1]
FORMATS = re.compile(
    r"%(?:\d+\$)?[#0\- +'^I]*(?:\*(?:\d+\$)?|\d+)?"
    r"(?:\.(?:\*(?:\d+\$)?|\d*))?[hljztLqZ]*[diouxXfFeEgGaAcCsSpnrvbB%]|%"
)
SCRIPTS = {
    "bg": ("LATIN", "CYRILLIC"), "ru": ("LATIN", "CYRILLIC"),
    "uk": ("LATIN", "CYRILLIC"),
    "ja": ("LATIN", "CJK", "HIRAGANA", "KATAKANA", "IDEOGRAPHIC"),
    "ko": ("LATIN", "HANGUL", "CJK", "IDEOGRAPHIC"),
    "zh": ("LATIN", "CJK", "IDEOGRAPHIC"),
}


def unique_object(pairs):
    result = dict(pairs)
    assert len(result) == len(pairs), "duplicate JSON key"
    return result


def load(path):
    assert path.stat().st_size <= 2 * 1024 * 1024, (path, "runtime size limit")
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object)


def available_languages():
    source = (ROOT / "src/tutorial/tutorial.c").read_text(encoding="utf-8")
    array = source.split("static const tutorial_locale languages[] = {", 1)[1].split("};", 1)[0]
    return re.findall(r'\{"([a-z]+)",', array)


def check(code, catalogue, reference):
    assert catalogue["version"] == 1, code
    if code != "de":
        assert catalogue["language"] == code, (code, "language metadata")
    assert catalogue["strings"].keys().isdisjoint(catalogue["messages"]), code
    # These ordinary UI labels are not names, abbreviations or shared
    # vocabulary in any supported locale. Key coverage alone must not let an
    # English copy-through catalogue pass as translated.
    for label in ("Configure movement", "World Generation", "Compact Health Bar",
                  "Weapon", "Arrows", "Rename", "Enabled"):
        assert catalogue["strings"][label] != label, (code, "untranslated UI label", label)
    unchanged = 0
    for group in ("strings", "messages"):
        entries = catalogue[group]
        assert entries.keys() == reference[group].keys(), (
            code, group, "missing", reference[group].keys() - entries.keys(),
            "extra", entries.keys() - reference[group].keys())
        for source, text in entries.items():
            where = (code, group, source)
            assert isinstance(text, str) and text.strip(), (*where, "empty translation")
            assert not any(char in text for char in ("\ufffd", "\u2047", "\x00", "▁")), (*where, "invalid text")
            assert not any(unicodedata.category(ch) in ("Co", "Cs", "Cn")
                           or (unicodedata.category(ch) == "Cc" and ch not in "\n\t")
                           for ch in text), (*where, "invalid Unicode character")
            assert "<unk>" not in text, (*where, "unconverted token")
            for artifact in ("Wikipedia", "Wayback Machine"):
                assert artifact not in text or artifact in source, (*where, "model artifact")
            tokens = FORMATS.findall(text)
            assert tokens == FORMATS.findall(source), (*where, "format tokens", tokens)
            assert all(not token.endswith("n") for token in tokens), (*where, "unsafe format")
            assert source.count("\n") == text.count("\n"), (*where, "newlines")
            # Proper names may retain source orthography. Other alphabetic
            # characters must belong to the target script (or Latin bindings).
            allowed = SCRIPTS.get(code, ("LATIN",))
            unexpected = {ch for ch in text if ch not in source
                          and unicodedata.category(ch) in ("Lu", "Ll", "Lt", "Lo")
                          and not unicodedata.name(ch, "").startswith(allowed)}
            assert not unexpected, (*where, "wrong script", sorted(unexpected))
            unchanged += text == source
    count = sum(len(catalogue[group]) for group in ("strings", "messages"))
    assert unchanged < count // 10, (code, "too many unchanged source entries", unchanged)
    print(f"{code}: PASS; {count} entries; {unchanged} unchanged labels/names")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--language", choices=available_languages()[1:])
    args = parser.parse_args()
    help_dir = ROOT / "lib/help"
    reference = load(help_dir / "ui.de.json")
    codes = [args.language] if args.language else available_languages()[1:]
    for code in codes:
        check(code, load(help_dir / f"ui.{code}.json"), reference)


if __name__ == "__main__":
    main()

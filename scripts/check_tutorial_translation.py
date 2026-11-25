#!/usr/bin/env python3
"""Check shipped tutorial translations against the English catalogue."""
import hashlib
import json
from pathlib import Path
import re
import unicodedata

ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = ("ru", "fr", "de", "es", "it", "pt", "uk", "pl", "nl",
             "tr", "ro", "cs", "hu", "sv", "bg", "da", "fi", "nb", "ja", "ko", "zh")
CJK_SCRIPTS = {
    "ja": ("CJK", "HIRAGANA", "KATAKANA", "IDEOGRAPHIC"),
    "ko": ("HANGUL",),
    "zh": ("CJK",),
}
DICE = re.compile(r"(?<![a-z0-9])(?:\d+d(?:\d+|X)|Xd1)(?![a-z0-9])", re.ASCII | re.IGNORECASE)
BAD_TEXT = re.compile(r"[\ufffd\u2047▁]|\?{3}")


def source_hash(lesson):
    text = "\0".join([lesson["title"], *(step["text"] for step in lesson["steps"])])
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def check(language, locale, english, keys):
    assert locale["version"] == 1 and locale["language"] == language
    assert set(locale["lessons"]) == {lesson["id"] for lesson in english["lessons"]}
    placeholders = lambda text: sorted(re.findall(r"\{[^}]*\}", text))
    count = 0
    alphabets = ("LATIN", "CYRILLIC") if language in ("ru", "uk", "bg") else ("LATIN",)
    alphabets += CJK_SCRIPTS.get(language, ())
    def check_alphabet(value, location):
        unexpected = {ch for ch in value if unicodedata.category(ch) in ("Lu", "Ll", "Lt", "Lo")
                      and not unicodedata.name(ch, "").startswith(alphabets)}
        assert not unexpected, (language, location, "unexpected characters", sorted(unexpected))
    for lesson in english["lessons"]:
        translated = locale["lessons"][lesson["id"]]
        assert translated["source_sha256"] == source_hash(lesson), (lesson["id"], "translation needs review")
        assert translated["title"].strip() and len(translated["title"].encode("utf-8")) < 320
        assert not BAD_TEXT.search(translated["title"]), (language, lesson["id"])
        check_alphabet(translated["title"], lesson["id"])
        assert len(translated["steps"]) == len(lesson["steps"]), lesson["id"]
        for source, text in zip(lesson["steps"], translated["steps"]):
            assert text.strip(), lesson["id"]
            check_alphabet(text, lesson["id"])
            if language in ("ru", "uk", "bg"):
                assert re.search("[А-Яа-яЁё]", text), lesson["id"]
            elif language in CJK_SCRIPTS:
                assert any(unicodedata.name(ch, "").startswith(CJK_SCRIPTS[language])
                           for ch in text), (language, lesson["id"], "untranslated page")
            elif len(source["text"].split()) >= 5:
                assert text != source["text"], (language, lesson["id"], "untranslated page")
            assert placeholders(source["text"]) == placeholders(text), lesson["id"]
            assert sorted(DICE.findall(source["text"])) == sorted(DICE.findall(text)), (language, lesson["id"], "dice")
            # Leave room for the longest localized public runtime context.
            assert len(text.encode("utf-8")) < 3072, lesson["id"]
            if language not in CJK_SCRIPTS:
                assert max(len(word.encode("utf-8")) for word in text.split()) < 192
            assert not BAD_TEXT.search(text), (language, lesson["id"])
            count += 1
    for group in ("strings", "patterns"):
        assert set(locale[group]) == keys[group], (language, group)
        for source, text in locale[group].items():
            formats = lambda value: re.findall(r"%(?:%|[+\-.\d]*[dsuf])", value)
            assert formats(source) == formats(text), source
            assert text.strip() and not BAD_TEXT.search(text), source
            check_alphabet(text, source)
    print(f"{language} tutorial catalogue: PASS ({len(locale['lessons'])} lessons, {count} pages)")


def main():
    help_dir = ROOT / "lib/help"
    english = json.loads((help_dir / "tutorials.json").read_text(encoding="utf-8"))
    locales = {code: json.loads((help_dir / f"tutorials.{code}.json").read_text(encoding="utf-8"))
               for code in LANGUAGES}
    keys = {group: set(locales["ru"][group]) for group in ("strings", "patterns")}
    references = {code: json.loads((help_dir / f"reference.{code}.json").read_text(encoding="utf-8"))
                  for code in LANGUAGES}
    expected_help = set(references["ru"]["help"])
    expected_device = set(references["ru"]["device"]) | set(locales["ru"].get("device", {}))
    string_literal = r'"(?:\\.|[^"\\])*"'
    for path in ("src/birth/birth-skills.c", "src/sdl/input/sdl-touch-tutorial.c"):
        source_code = (ROOT / path).read_text(encoding="utf-8")
        calls = re.findall(r"tutorial_device_text\(\s*((?:" + string_literal + r"\s*)+)\)", source_code)
        used = {"".join(json.loads(part) for part in re.findall(string_literal, call)) for call in calls}
        assert used <= expected_device, (path, "missing control-guide strings", used - expected_device)
    formats = re.compile(r"%(?:%|[-+.0-9]*[csdiuf])")
    tags = re.compile(r"</?[atyngvru]>|</>")
    for code, locale in locales.items():
        check(code, locale, english, keys)
        reference = references[code]
        assert reference["version"] == 1 and reference["language"] == code
        device = {**reference.get("device", {}), **locale.get("device", {})}
        assert set(reference["help"]) == expected_help, (code, "help pages")
        assert set(device) == expected_device, (code, "device tutorials")
        for source, translated in (*reference["help"].items(), *device.items()):
            assert translated.strip() and not BAD_TEXT.search(translated), (code, source)
            assert "▁" not in translated and "<unk>" not in translated, (code, source, "unconverted text")
            assert not re.search(r"\b([^\W\d_]{2,})(?:\s+\1){3,}\b", translated, re.IGNORECASE), (code, source, "repeated words")
            assert formats.findall(source) == formats.findall(translated), (code, source, "control bindings")
            if code in CJK_SCRIPTS and source in device:
                # Localized callouts can emphasize a shorter phrase or move a
                # control name. Every color span must still close on this card.
                active = None
                for tag in re.findall(r"<[^>]+>|</>", translated):
                    assert tags.fullmatch(tag), (code, source, tag)
                    if tag.startswith("</"):
                        assert active and tag in ("</>", f"</{active}>"), (code, source, tag)
                        active = None
                    else:
                        assert not active, (code, source, tag)
                        active = tag[1]
                assert not active, (code, source, "unclosed color span")
            else:
                assert sorted(tags.findall(source)) == sorted(tags.findall(translated)), (code, source, "markup")
        print(f"{code}: {len(expected_help)} help entries and {len(device)} control-guide entries")


if __name__ == "__main__":
    main()

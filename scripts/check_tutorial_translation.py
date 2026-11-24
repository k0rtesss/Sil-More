#!/usr/bin/env python3
"""Check Russian tutorial coverage, source drift, placeholders and UTF-8 limits."""
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def source_hash(lesson):
    text = "\0".join([lesson["title"], *(step["text"] for step in lesson["steps"])])
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def main():
    english = json.loads((ROOT / "lib/help/tutorials.json").read_text(encoding="utf-8"))
    russian = json.loads((ROOT / "lib/help/tutorials.ru.json").read_text(encoding="utf-8"))
    assert russian["version"] == 1 and russian["language"] == "ru"
    assert set(russian["lessons"]) == {lesson["id"] for lesson in english["lessons"]}
    placeholders = lambda text: sorted(re.findall(r"\{(?:subject|detail|context)\}", text))
    count = 0
    for lesson in english["lessons"]:
        translated = russian["lessons"][lesson["id"]]
        assert translated["source_sha256"] == source_hash(lesson), (lesson["id"], "translation needs review")
        assert len(translated["title"].encode("utf-8")) < 320
        assert len(translated["steps"]) == len(lesson["steps"]), lesson["id"]
        for source, text in zip(lesson["steps"], translated["steps"]):
            assert re.search("[А-Яа-яЁё]", text), lesson["id"]
            assert placeholders(source["text"]) == placeholders(text), lesson["id"]
            # Leave room for the longest localized public runtime context.
            assert len(text.encode("utf-8")) < 3072, lesson["id"]
            assert max(map(lambda word: len(word.encode("utf-8")), text.split())) < 192
            assert "\ufffd" not in text and "???" not in text, lesson["id"]
            count += 1
    for group in ("strings", "patterns"):
        for source, text in russian[group].items():
            formats = lambda value: re.findall(r"%(?:%|[+\-.\d]*[dsuf])", value)
            assert formats(source) == formats(text), source
            assert text and "\ufffd" not in text, source
    print(f"Russian tutorial catalogue: PASS ({len(russian['lessons'])} lessons, {count} pages)")


if __name__ == "__main__":
    main()

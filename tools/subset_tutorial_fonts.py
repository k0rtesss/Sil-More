"""Build the tutorial OTFs from the three upstream Noto Sans CJK regular faces.

Usage: python tools/subset_tutorial_fonts.py DIRECTORY (requires fonttools).
Use the jp, kr and sc OTF files from notofonts/noto-cjk tag Sans2.004.
"""
import argparse
import json
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont


ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / "lib/xtra/font"
REGIONS = {"ja": "JP", "ko": "KR", "zh": "SC"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("upstream", type=Path)
    args = parser.parse_args()
    for language, region in REGIONS.items():
        catalog = (ROOT / f"lib/help/tutorials.{language}.json").read_text(encoding="utf-8")
        # Include all picker names and common Latin/Cyrillic characters for
        # bindings, names, titles and English text inserted by the engine.
        text = json.dumps(json.loads(catalog), ensure_ascii=False) + "日本語한국어简体中文"
        reference = ROOT / f"lib/help/reference.{language}.json"
        if reference.is_file():
            text += json.dumps(json.loads(reference.read_text(encoding="utf-8")), ensure_ascii=False)
        font = TTFont(args.upstream / f"NotoSansCJK{region.lower()}-Regular.otf", recalcTimestamp=False)
        repertoire = set(map(ord, text)) | set(range(32, 0x250)) | set(range(0x400, 0x530))
        options = subset.Options()
        options.name_IDs = ["*"]
        options.name_languages = ["*"]
        sub = subset.Subsetter(options=options)
        sub.populate(unicodes=repertoire)
        sub.subset(font)

        # Use a distinct family and PostScript name for the modified font.
        family = f"Tutorial Sans {region}"
        ps = f"TutorialSans{region}-Regular"
        names = {1: family, 3: ps, 4: family + " Regular", 6: ps, 16: family}
        for record in font["name"].names:
            if record.nameID in names:
                record.string = names[record.nameID].encode(record.getEncoding())
        cff = font["CFF "].cff
        cff.fontNames = [ps]
        top = cff.topDictIndex[0]
        top.FamilyName = family
        top.FullName = family + " Regular"
        font.save(FONT_DIR / f"TutorialSans{region}.otf")


if __name__ == "__main__":
    main()

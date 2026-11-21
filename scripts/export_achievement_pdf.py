#!/usr/bin/env python3
"""Build a spoiler-complete review PDF in the native achievement display order.

Run export_achievement_stores.py first to refresh the shared Steam icon PNGs.
Requires reportlab and pypdf; fonts default to Windows Arial/Georgia or DejaVu.
"""
import argparse
from collections import Counter
from hashlib import sha256
from html import escape
import json
import os
from pathlib import Path
import re

from pypdf import PdfReader
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas
from reportlab.platypus import Paragraph

from export_achievement_stores import (
    APPLE_POINTS, POINTS, ROOT, catalogue, display_order,
    game_center_selection_exclusions, icon_tiles, steam_excluded_keys,
)

INK = colors.HexColor("#173C34")
TEXT = colors.HexColor("#293A36")
MUTED = colors.HexColor("#65756E")
PAPER = colors.HexColor("#FAF8F1")
LINE = colors.HexColor("#D8DDD3")
GOLD = colors.HexColor("#9D7331")
PAGE_W, PAGE_H = A4
MARGIN = 42
WIDTH = PAGE_W - 2 * MARGIN

# Book references concern the lore invoked by each goal; they do not assert
# Tolkien described every game relic in that exact form.
LORE_SOURCES = {
    "FIND_ANGRIST": ("The Silmarillion, ch. 19: Of Beren and Lúthien", "https://tolkiengateway.net/wiki/Angrist"),
    "FIND_GURTHANG": ("The Silmarillion, ch. 21: Of Túrin Turambar", "https://tolkiengateway.net/wiki/Gurthang"),
    "FIND_GOTHMOG_AXE": ("The Silmarillion, ch. 20: Of the Fifth Battle: Nirnaeth Arnoediad", "https://www.encyclopedia-of-arda.com/g/gothmog.php"),
    "FIND_MELKOR": ("The Silmarillion, ch. 8: Of the Darkening of Valinor", "https://tolkiengateway.net/wiki/Melkor"),
    "FIND_TURGON_STAFF": ("The Silmarillion, ch. 16: Of Maeglin", "https://tolkiengateway.net/wiki/Turgon"),
    "FIND_BELTHRONDING": ("The Silmarillion, ch. 21: Of Túrin Turambar", "https://tolkiengateway.net/wiki/Belthronding"),
    "FIND_IDRIL_ROBE": ("The Silmarillion, ch. 23: Of Tuor and the Fall of Gondolin", "https://tolkiengateway.net/wiki/Idril"),
    "FIND_FINGOLFIN_SHIELD": ("The Silmarillion, ch. 18: Of the Ruin of Beleriand and the Fall of Fingolfin", "https://tolkiengateway.net/wiki/Fingolfin"),
    "FIND_ULMO_CLOAK": ("The Silmarillion, ch. 23: Of Tuor and the Fall of Gondolin", "https://tolkiengateway.net/wiki/Ulmo"),
    "FIND_CELEBRIMBOR_GLOVES": ("The Silmarillion: Of the Rings of Power and the Third Age", "https://tolkiengateway.net/wiki/Celebrimbor"),
    "FIND_MELIAN_RING": ("The Silmarillion, chs. 4 and 10: Of Thingol and Melian; Of the Sindar", "https://tolkiengateway.net/wiki/Melian"),
    "FIND_NAUGLAMIR": ("The Silmarillion, ch. 13: Of the Return of the Noldor", "https://thainsbook.minastirith.cz/objects.html"),
}


def register_fonts(font_dir=None):
    candidates = [Path(font_dir)] if font_dir else [
        Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts",
        Path("/usr/share/fonts/truetype/dejavu"),
    ]
    for folder in candidates:
        windows = (folder / "arial.ttf").exists()
        names = ("arial.ttf", "arialbd.ttf", "georgia.ttf") if windows else (
            "DejaVuSans.ttf", "DejaVuSans-Bold.ttf", "DejaVuSerif.ttf")
        if all((folder / name).exists() for name in names):
            for family, filename in zip(("Body", "Strong", "Display"), names):
                pdfmetrics.registerFont(TTFont(family, str(folder / filename)))
            return
    raise FileNotFoundError("Unicode fonts not found; pass --font-dir with Arial/Georgia or DejaVu fonts")


def paragraph(text, width, size=10, leading=13, font="Body", color=TEXT):
    style = ParagraphStyle("text", fontName=font, fontSize=size, leading=leading,
                           textColor=color, alignment=TA_LEFT, splitLongWords=False)
    result = Paragraph(escape(text), style)
    result.wrap(width, PAGE_H)
    return result


def load_rows(icon_dir):
    native = catalogue()
    order = display_order()
    steam_excluded = steam_excluded_keys()
    apple_excluded = game_center_selection_exclusions(steam_excluded)
    properties = (ROOT / "android/play-games.properties").read_text(encoding="utf-8")
    google_ids = {key: value.strip() for key, value in re.findall(
        r"^SIL_MORE_PLAY_ACH_([A-Z_]+)=([^\r\n]*)$", properties, re.M)}
    mapping = dict(re.findall(r'^SIL_ACHIEVEMENT_ORDER\(([A-Z_]+), "([^"]+)"\)',
                             (ROOT / "src/achievement-order.h").read_text(encoding="utf-8"), re.M))
    if set(mapping) != set(order) or set(icon_tiles()) != set(order):
        raise ValueError("Order, category and icon coverage must match the native catalogue")
    rows = sorted(native, key=lambda row: order[row["key"]])
    seen_categories = set()
    previous = None
    for row in rows:
        row["number"] = order[row["key"]]
        row["category"] = mapping[row["key"]]
        row["google_points"] = POINTS[row["key"]]
        row["google_id_pending"] = not google_ids.get(row["key"])
        row["apple_points"] = None if row["key"] in apple_excluded else APPLE_POINTS[row["key"]]
        row["steam_selected"] = row["key"] not in steam_excluded
        if row["category"] != previous:
            if row["category"] in seen_categories:
                raise ValueError("Achievement categories must remain contiguous")
            seen_categories.add(row["category"])
            previous = row["category"]
        row["icon"] = icon_dir / (row["key"] + "_ACHIEVED.png")
        if not row["icon"].is_file():
            row["icon"] = icon_dir.parent / "google-play" / (row["key"] + ".png")
        if not row["icon"].is_file():
            raise FileNotFoundError(f"Missing icon: {row['icon']}; refresh the store export first")
        row["title_para"] = paragraph(row["title"], WIDTH - 160, 11.5, 14, "Strong", INK)
        row["description_para"] = paragraph(row["description"], WIDTH - 90, 10, 13)
        row["find_requirement"] = ("Requirement: Identify this artefact in your inventory or on a visible floor."
                                   if row["key"].startswith("FIND_") else None)
        row["find_para"] = (paragraph(row["find_requirement"], WIDTH - 90, 9, 12, "Body", MUTED)
                            if row["find_requirement"] else None)
        google = f"Google Play: {row['google_points']} pts"
        if row["google_id_pending"]:
            google += " (ID pending)"
        apple = (f"Game Center: {row['apple_points']} pts" if row["apple_points"] is not None
                 else "Game Center: not selected")
        steam = "Steam: selected" if row["steam_selected"] else "Steam: not selected"
        row["store_text"] = f"{google} | {apple} | {steam}"
        row["store_para"] = paragraph(row["store_text"], WIDTH - 90, 8.5, 11, "Strong", MUTED)
        find_height = 5 + row["find_para"].height if row["find_para"] else 0
        row["height"] = max(80, 11 + row["title_para"].height + 6
                            + row["description_para"].height + find_height
                            + 6 + row["store_para"].height + 12)
    return rows


def paginate(rows):
    pages = []
    current = []
    top = PAGE_H - 190
    bottom = 56
    category = None
    for row in rows:
        heading = row["category"] != category
        needed = row["height"] + (29 if heading else 0)
        if top - needed < bottom:
            pages.append(current)
            current = []
            top = PAGE_H - 99
            heading = True
        if heading:
            continuation = row["category"] == category
            current.append(("heading", top, row["category"], continuation))
            top -= 29
        current.append(("card", top, row))
        top -= row["height"] + 7
        category = row["category"]
    if current:
        pages.append(current)
    return pages


def draw_card(pdf, top, row):
    height = row["height"]
    bottom = top - height
    pdf.setFillColor(colors.white)
    pdf.setStrokeColor(LINE)
    pdf.roundRect(MARGIN, bottom, WIDTH, height, 6, fill=1, stroke=1)
    pdf.drawImage(str(row["icon"]), MARGIN + 12, top - 57, width=45, height=45)
    x = MARGIN + 70
    pdf.setFont("Strong", 8)
    pdf.setFillColor(MUTED)
    pdf.drawString(x, top - 15, f"#{row['number']:03d}")
    title = row["title_para"]
    title.drawOn(pdf, x + 33, top - 11 - title.height)
    badge = "Hidden" if row["hidden"] else "Visible"
    badge_width = 46
    badge_x = PAGE_W - MARGIN - badge_width - 10
    pdf.setFillColor(colors.HexColor("#EFE5D3") if row["hidden"] else colors.HexColor("#E4EEE7"))
    pdf.roundRect(badge_x, top - 24, badge_width, 15, 7, fill=1, stroke=0)
    pdf.setFillColor(GOLD if row["hidden"] else INK)
    pdf.setFont("Strong", 8)
    pdf.drawCentredString(badge_x + badge_width / 2, top - 19.5, badge)
    description = row["description_para"]
    y = top - 11 - title.height - 6 - description.height
    description.drawOn(pdf, x, y)
    if row["find_para"]:
        y -= 5 + row["find_para"].height
        row["find_para"].drawOn(pdf, x, y)
    y -= 6 + row["store_para"].height
    row["store_para"].drawOn(pdf, x, y)


def normalize(value):
    return " ".join(value.split())


def draw_lore_sources(pdf, rows):
    pdf.setFont("Display", 23)
    pdf.setFillColor(INK)
    pdf.drawString(MARGIN, PAGE_H - 70, "Artefact lore sources")
    note = paragraph("Book references identify the history and character deeds invoked by the find goals. "
                     "Corroboration links below are secondary reference sources. Idril's robe, Celebrimbor's "
                     "gloves and Melian's ring are game relics inspired by canonical characters; the books "
                     "do not attest these specific relics. Item forms and game powers remain adaptations.",
                     WIDTH, 9, 12)
    note.drawOn(pdf, MARGIN, PAGE_H - 87 - note.height)
    top = PAGE_H - 153
    for row in rows:
        if row["key"] not in LORE_SOURCES:
            continue
        reference, url = LORE_SOURCES[row["key"]]
        pdf.setFillColor(INK)
        pdf.setFont("Strong", 10)
        pdf.drawString(MARGIN, top - 10, row["title"])
        pdf.setFillColor(TEXT)
        pdf.setFont("Body", 9)
        pdf.drawString(MARGIN, top - 24, reference)
        pdf.setFillColor(colors.HexColor("#276C66"))
        pdf.setFont("Body", 8)
        pdf.drawString(MARGIN, top - 37, url)
        pdf.linkURL(url, (MARGIN, top - 40, MARGIN + pdfmetrics.stringWidth(url, "Body", 8), top - 29), relative=0)
        pdf.setStrokeColor(LINE)
        pdf.line(MARGIN, top - 44, PAGE_W - MARGIN, top - 44)
        top -= 49
    if top < 56:
        raise ValueError("Lore-source appendix overflows the page")


def verify(output, rows, pages):
    reader = PdfReader(output)
    texts = [page.extract_text() for page in reader.pages]
    text = normalize("\n".join(texts))
    cursor = 0
    for row in rows:
        values = [f"#{row['number']:03d}", row["title"],
                  "Hidden" if row["hidden"] else "Visible", row["description"]]
        if row["find_requirement"]:
            values.append(row["find_requirement"])
        values.append(row["store_text"])
        for value in values:
            needle = normalize(value)
            position = text.find(needle, cursor)
            if position < 0:
                raise ValueError(f"Missing or out-of-order PDF text for {row['key']}: {value}")
            cursor = position + len(needle)
    numbers = re.findall(r"#(\d{3})\b", text)
    if numbers != [f"{n:03d}" for n in range(1, len(rows) + 1)]:
        raise ValueError("PDF must contain each display number exactly once")
    if len(reader.pages) != len(pages) + 1:
        raise ValueError("Unexpected PDF page count")
    links = [annotation.get_object().get("/A", {}).get("/URI")
             for annotation in reader.pages[-1].get("/Annots", [])]
    if set(links) != {reference[1] for reference in LORE_SOURCES.values()}:
        raise ValueError("Missing clickable lore-source links")
    return {"achievement_count": len(rows), "page_count": len(reader.pages),
            "catalogue_page_count": len(pages), "lore_source_links": len(links),
            "hidden_count": sum(row["hidden"] for row in rows),
            "visible_count": sum(not row["hidden"] for row in rows),
            "google_point_total": sum(row["google_points"] for row in rows),
            "google_ids_pending": [row["key"] for row in rows if row["google_id_pending"]],
            "game_center_count": sum(row["apple_points"] is not None for row in rows),
            "game_center_point_total": sum(row["apple_points"] or 0 for row in rows),
            "steam_count": sum(row["steam_selected"] for row in rows),
            "categories": dict(Counter(row["category"] for row in rows)),
            "ordered_keys": [row["key"] for row in rows],
            "verification": "All titles, descriptions, find requirements, points and store selections extracted in native display order",
            "pdf_sha256": sha256(output.read_bytes()).hexdigest()}


def export(output, icon_dir, font_dir=None):
    register_fonts(font_dir)
    rows = load_rows(icon_dir)
    if {row["key"] for row in rows if row["key"].startswith("FIND_")} != set(LORE_SOURCES):
        raise ValueError("Update lore-source references to cover every find achievement")
    pages = paginate(rows)
    output.parent.mkdir(parents=True, exist_ok=True)
    pdf = canvas.Canvas(str(output), pagesize=A4, pageCompression=1)
    pdf.setTitle("Sil-More - Complete Achievement Catalogue")
    pdf.setAuthor("Sil-More")
    pdf.setSubject("Spoiler-complete review catalogue in native display order")
    hidden = sum(row["hidden"] for row in rows)
    for page_number, elements in enumerate(pages + [None], 1):
        pdf.setFillColor(PAPER)
        pdf.rect(0, 0, PAGE_W, PAGE_H, fill=1, stroke=0)
        pdf.setFillColor(INK)
        pdf.rect(0, PAGE_H - 12, PAGE_W, 12, fill=1, stroke=0)
        pdf.setFont("Strong", 8)
        pdf.drawString(MARGIN, PAGE_H - 38, "SIL-MORE  /  ACHIEVEMENT BOOK")
        if elements is None:
            draw_lore_sources(pdf, rows)
        else:
            pdf.setFont("Display", 25 if page_number == 1 else 19)
            pdf.drawString(MARGIN, PAGE_H - 70,
                           "Complete Achievement Catalogue" if page_number == 1 else "Achievement Catalogue")
        if page_number == 1:
            pdf.setFont("Body", 10)
            pdf.setFillColor(MUTED)
            pdf.drawString(MARGIN, PAGE_H - 92,
                           f"{len(rows)} achievements  |  {len(set(row['category'] for row in rows))} categories  |  {hidden} hidden  |  {len(rows)-hidden} visible")
            google_points = sum(row["google_points"] for row in rows)
            apple_count = sum(row["apple_points"] is not None for row in rows)
            apple_points = sum(row["apple_points"] or 0 for row in rows)
            steam_count = sum(row["steam_selected"] for row in rows)
            pdf.setFont("Strong", 9)
            pdf.drawString(MARGIN, PAGE_H - 107,
                f"Google Play: {len(rows)} / {google_points} pts  |  Game Center: {apple_count} / {apple_points} pts  |  Steam: {steam_count}")
            note = paragraph("Spoiler review edition. All hidden names and requirements are revealed. "
                             "Hidden / Visible refers to the in-game book; published Google visibility may differ. "
                             "Points and store selections reflect project metadata; Apple is a proposal, "
                             "and live store settings are not verified. ID pending means a Google ID is not configured.",
                             WIDTH, 8.5, 11)
            note.drawOn(pdf, MARGIN, PAGE_H - 115 - note.height)
        for element in elements or []:
            if element[0] == "heading":
                _, top, category, continued = element
                pdf.setFillColor(INK)
                pdf.setFont("Strong", 11)
                pdf.drawString(MARGIN, top - 13, category + (" (continued)" if continued else ""))
                pdf.setStrokeColor(LINE)
                pdf.line(MARGIN, top - 21, PAGE_W - MARGIN, top - 21)
            else:
                draw_card(pdf, element[1], element[2])
        pdf.setStrokeColor(LINE)
        pdf.line(MARGIN, 42, PAGE_W - MARGIN, 42)
        pdf.setFillColor(MUTED)
        pdf.setFont("Body", 8)
        pdf.drawString(MARGIN, 28, "Review edition - all hidden details revealed")
        pdf.drawRightString(PAGE_W - MARGIN, 28, f"{page_number} / {len(pages) + 1}")
        pdf.showPage()
    pdf.save()
    report = verify(output, rows, pages)
    output.with_suffix(".verification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items() if key not in ("ordered_keys", "categories")}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "output/pdf/sil-more-achievements.pdf")
    parser.add_argument("--icon-dir", type=Path, default=ROOT / "scripts/output/achievement-stores/steam")
    parser.add_argument("--font-dir", type=Path)
    args = parser.parse_args()
    export(args.output.resolve(), args.icon_dir.resolve(), args.font_dir)

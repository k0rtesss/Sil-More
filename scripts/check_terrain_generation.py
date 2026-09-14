#!/usr/bin/env python3
"""Exercise the production local terrain planner on isolated topology fixtures.

No player state is loaded. Gallery images are exact cell maps emitted by the C
harness, not illustrations. The SDL water suite additionally covers real CA
and big-cavern geometry with the full game linked. The major landmark pass is
explicitly stubbed here and tested in check_terrain_landmarks.py separately.
"""
from pathlib import Path
import os
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/terrain-generation-check"


def gallery():
    colors = {
        "#": (35, 40, 48), ".": (202, 193, 167), "~": (45, 132, 206),
        "C": (6, 9, 15), "L": (249, 85, 33), "P": (94, 177, 62),
        "I": (147, 224, 241), "r": (235, 194, 71), "!": (237, 237, 237),
        "D": (9, 29, 94), "B": (140, 105, 65),
    }
    cards = []
    for path in sorted(OUT.glob("map-*.txt")):
        if path.name.startswith("map-failure"):
            continue
        rows = path.read_text().splitlines()
        scale = 7
        width, height = len(rows[0])*scale, len(rows)*scale
        scanlines = b"".join(b"\0" + b"".join(bytes(colors[c])*scale for c in row)
                             for row in rows for _ in range(scale))
        def chunk(kind, content):
            return struct.pack(">I", len(content)) + kind + content + struct.pack(">I", zlib.crc32(kind+content))
        png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width,height,8,2,0,0,0))
        png += chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b"")
        image = path.with_suffix(".png")
        image.write_bytes(png)
        cards.append(f'<figure><img src="{image.name}"><figcaption>{path.stem[4:]}</figcaption></figure>')
    (OUT / "gallery.html").write_text('''<!doctype html><meta charset="utf-8"><title>Generated terrain fixtures</title>
<style>body{background:#111820;color:#e4e8eb;font:16px system-ui;margin:30px}main{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:20px}figure{margin:0;padding:15px;background:#202b35}img{width:100%;image-rendering:pixelated}figcaption{margin-top:12px}span{margin-right:20px}</style>
<h1>Production local terrain accents</h1><p>Exact generated fixture cells, with the major landmark pass disabled. Gold = protected crossing footing; white = authored or occupied cell. These fixtures test local topology; real cavern renders are produced by check_water.py.</p>
<p><span style="color:#2d84ce">Water</span><span>Black: chasm</span><span style="color:#f95521">Lava</span><span style="color:#5eb13e">Poison</span><span style="color:#93e0f1">Ice</span></p><main>''' + "".join(cards) + "</main>", encoding="utf-8")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    themes = OUT / "themes.c"
    themes.write_text('''#include "level-generation/level-generation-themes.c"
cptr ANGBAND_DIR_EDIT;
bool path_build(char* out,size_t n,cptr d,cptr f) {(void)out;(void)n;(void)d;(void)f;return false;}
bool test_themes_parse(const char* text,size_t size) {
    terrain_themes_loaded=terrain_themes_parse(text,size,"test themes");
    return terrain_themes_loaded;
}
''', encoding="utf-8")
    # Compile the unchanged production late-rubble operation with minimal
    # headers; linking the entire generation-access module pulls in the game.
    access = (ROOT / "src/level-generation/level-generation-access.c").read_text(encoding="utf-8")
    start = access.index("void place_rubble(")
    end = access.index("\n}", start) + 2
    rubble = OUT / "rubble.c"
    rubble.write_text('#include "angband.h"\n#include "externs.h"\n#include "level-generation/level-generation-terrain.h"\n'
                      + access[start:end], encoding="utf-8")
    env["PATH"] = str(ROOT / "build-standard/_deps/SDL") + os.pathsep + env["PATH"]
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-O1", "-g",
                    "-ffunction-sections", "-fdata-sections", "@CMakeFiles/sil-more.dir/includes_C.rsp",
                    str(ROOT / "scripts/tests/terrain-generation-test.c"),
                    str(ROOT / "src/level-generation/level-generation-terrain-access.c"),
                    str(ROOT / "src/cave/cave-bridge.c"),
                    str(themes), str(rubble), str(ROOT / "build-standard/_deps/SDL/libSDL3.dll.a"),
                    "-Wl,--gc-sections", "-o", str(exe)],
                   cwd=ROOT / "build-standard", env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=60)
    gallery()
    print(f"Exact generated map gallery: {OUT / 'gallery.html'}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check production item comparisons and description wrapping using isolated data.

Requires a current standard build. No player saves or settings are opened.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/equipment-comparison"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    fixture = OUT / "description.c"
    fixture.write_text(
        '#include "' + (ROOT / "src/object/object-info.c").as_posix() + '"\n'
        'void fixture_description(const object_type** objects, '
        'const char** headings, int count, bool compare) {\n'
        'object_info_screen_multi_body(objects, headings, count, true, compare);\n}\n',
        encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + item + '"' for item in objects
        if not item.endswith(("/src/main.c.obj", "/src/object/object-info.c.obj"))),
        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-UNDEBUG",
        "@CMakeFiles/sil-more.dir/includes_C.rsp",
        str(ROOT / "scripts/tests/equipment-comparison-test.c"), str(fixture),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=sdl_get_layout_screen_rect,--wrap=sdl_overlay_pane_anchor_rect,"
        "--wrap=sdl_present_if_needed,--wrap=inkey",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data, str(OUT)],
            cwd=data, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

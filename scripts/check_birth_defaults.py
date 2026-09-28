#!/usr/bin/env python3
"""Exercise production birth presets and refundable skill allocation with mocked UI.

Requires a configured build-standard with its SDL dependencies. Compiles the
relevant production sources directly; does not need unrelated gameplay objects,
launch the game, or read/write saves, player config, or runtime templates.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/birth-defaults-check"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
         str(BUILD / "_deps/SDL"), str(BUILD / "_deps/SDL_ttf"),
         str(BUILD / "_deps/SDL_image"), str(BUILD / "_deps/SDL_mixer"),
         env["PATH"]]
    )
    exe = OUT / "check.exe"
    subprocess.run(
        [
            "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            # MinGW unwind tables otherwise retain unused production functions.
            "-fno-asynchronous-unwind-tables", "-fno-unwind-tables",
            "@CMakeFiles/sil-more.dir/includes_C.rsp",
            str(ROOT / "scripts/tests/birth-defaults-test.c"),
            str(ROOT / "src/birth/birth-allocation.c"),
            str(ROOT / "src/birth/birth-setup.c"),
            str(ROOT / "src/variable.c"),
            "@CMakeFiles/sil-more.dir/linkLibs.rsp",
            "-o", str(exe),
        ],
        cwd=BUILD, env=env, check=True,
    )
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

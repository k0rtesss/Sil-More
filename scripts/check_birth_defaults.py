#!/usr/bin/env python3
"""Exercise production birth presets and refundable skill allocation with mocked UI.

Build first with build-incremental.ps1. Does not launch the game or
read/write saves, player config, or runtime templates.
"""
from pathlib import Path
import os
import shlex
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
    cmake_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/birth/birth-skills.c.obj")
    objects = [obj for obj in objects if not obj.endswith(excluded)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects), encoding="utf-8")
    exe = OUT / "check.exe"
    subprocess.run(
        [
            "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
            "@CMakeFiles/sil-more.dir/includes_C.rsp",
            str(ROOT / "scripts/tests/birth-defaults-test.c"),
            "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
            "-o", str(exe),
        ],
        cwd=BUILD, env=env, check=True,
    )
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

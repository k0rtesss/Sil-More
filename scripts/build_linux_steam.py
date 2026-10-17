#!/usr/bin/env python3
"""Build and package native x86-64 Linux binaries inside the Steam Runtime SDK.

Run with Python 3 in the Steam Runtime 4 SDK, not a Windows configuration probe.
Requires CMake 3.24+, Ninja, GCC/G++, patchelf, binutils and SDL submodules.
The output ZIP is rooted for a Linux-only Steam depot; the tarball has a folder.
This helper never uploads or activates a Steam build.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import struct
import subprocess
import tarfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]


def run(*args, capture=False):
    return subprocess.run(args, check=True, text=True,
                          stdout=subprocess.PIPE if capture else None).stdout


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def check_elf(path):
    header = path.read_bytes()[:20]
    if (header[:6] != b"\x7fELF\x02\x01"
            or struct.unpack_from("<H", header, 18)[0] != 62):
        raise RuntimeError(f"Not an x86-64 Linux ELF file: {path}")


def package(build, output, version):
    output.mkdir(parents=True, exist_ok=True)
    stage = output / "sil-more-linux-steam"
    stage.mkdir()  # Refuse to mix a new release with an existing package.
    binary = build / "sil-more"
    check_elf(binary)
    shutil.copy2(binary, stage / binary.name)

    # All non-system direct dependencies are deliberately limited to the four
    # SDL libraries and Steam API. Font/image codecs are built into SDL add-ons.
    dependencies = run("ldd", str(binary), capture=True)
    (output / "build-dependencies.txt").write_text(dependencies)
    if "not found" in dependencies:
        raise RuntimeError("Build has unresolved shared-library dependencies")
    bundled = []
    runtime_libraries = {"libc.so.6", "libm.so.6", "libdl.so.2", "libpthread.so.0",
                         "librt.so.1", "libgcc_s.so.1", "libstdc++.so.6"}
    for name, source in re.findall(r"^\s*(\S+) => (/\S+)", dependencies, re.M):
        if name.startswith("libSDL3") or name == "libsteam_api.so":
            target = stage / name
            shutil.copy2(Path(source).resolve(), target)
            check_elf(target)
            bundled.append(name)
        elif name not in runtime_libraries:
            raise RuntimeError(f"Unexpected dependency needs a packaging decision: {name}")
    if len(bundled) != 5:
        raise RuntimeError(f"Expected SDL, image, ttf, mixer and Steam API: {bundled}")
    for target in [stage / binary.name, *(stage / name for name in bundled)]:
        run("strip", "--strip-unneeded", str(target))
        run("patchelf", "--set-rpath", "$ORIGIN", str(target))
        target.chmod(0o755)

    # Copy shipped assets only. Never ship developer profiles, generated raw
    # caches, logs, saves, databases, or optional downloaded sound packs.
    for name in ("edit", "file", "help", "info", "pref", "xtra"):
        source = ROOT / "lib" / name
        if source.is_dir():
            # Match the existing Windows release policy: OGG-only audio,
            # documented redistributable fonts, and no template work files.
            patterns = [".git*", ".keepalive", "packs", "*.raw", "*.sav*",
                        "*.log", "log.txt", "*.db", "*.sqlite*", "*.bak*",
                        "*.wav", "*.WAV", "InputMono-Bold.ttf"]
            if name == "edit":
                patterns.append("misc")
            shutil.copytree(source, stage / "lib" / name,
                            ignore=shutil.ignore_patterns(*patterns))
    for name in ("apex", "bone", "data", "save", "user"):
        (stage / "lib" / name).mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "LICENSE.md", stage / "LICENSE.md")
    notices = {
        "SDL.txt": "external/SDL/LICENSE.txt",
        "SDL_image.txt": "external/SDL_image/LICENSE.txt",
        "SDL_ttf.txt": "external/SDL_ttf/LICENSE.txt",
        "SDL_mixer.txt": "external/SDL_mixer/LICENSE.txt",
        "FreeType.txt": "external/SDL_ttf/external/freetype/docs/FTL.TXT",
        "HarfBuzz.txt": "external/SDL_ttf/external/harfbuzz/COPYING",
        "PlutoSVG.txt": "external/SDL_ttf/external/plutosvg/LICENSE",
        "PlutoVG.txt": "external/SDL_ttf/external/plutovg/LICENSE",
        "libpng.txt": "external/SDL_image/external/libpng/LICENSE",
        "zlib.txt": "external/SDL_image/external/zlib/LICENSE",
        "dr_libs.txt": "external/SDL_mixer/src/dr_libs/LICENSE",
        "stb_vorbis.h": "external/SDL_mixer/src/stb_vorbis/stb_vorbis.h",
    }
    (stage / "licenses").mkdir()
    for name, source in notices.items():
        if (ROOT / source).is_file():
            shutil.copy2(ROOT / source, stage / "licenses" / name)
    launcher = stage / "sil-more.sh"
    launcher.write_text('#!/bin/sh\nset -eu\n'
                        'base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                        'cd "$base"\nexec "$base/sil-more" "$@"\n')
    launcher.chmod(0o755)
    (stage / "README-linux.txt").write_text(
        f"Sil-More {version} - native Linux x86-64\n\n"
        "Launch ./sil-more.sh. Steam launch executable: sil-more.sh.\n"
        "Built for Steam Linux Runtime 4.0 (steamrt4).\n"
        "SDL, PNG decoding and font rendering dependencies are bundled.\n"
        "Steam supplies the system runtime and graphics/audio drivers.\n"
        "Saves and settings are stored outside the installation, normally\n"
        "under ~/sil-more/. Do not distribute steam_appid.txt.\n")
    relocated = run("ldd", str(stage / "sil-more"), capture=True)
    (output / "package-dependencies.txt").write_text(relocated)
    if "not found" in relocated:
        raise RuntimeError("Packaged executable has unresolved dependencies")
    for name in bundled:
        if not re.search(re.escape(name) + r" => " + re.escape(str(stage)) + r"/", relocated):
            raise RuntimeError(f"Package is using an external copy of {name}")

    manifest = {str(p.relative_to(stage)): digest(p)
                for p in sorted(stage.rglob("*")) if p.is_file()}
    stem = f"sil-more-{version}-steam-linux-x86_64"
    zip_path, tar_path = output / f"{stem}.zip", output / f"{stem}.tar.gz"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(stage.rglob("*")):
            archive.write(path, path.relative_to(stage))
    with tarfile.open(tar_path, "w:gz") as archive:
        archive.add(stage, arcname="sil-more-linux-steam")
    report = {"version": version, "architecture": "x86_64", "runtime": "steamrt4",
              "os_release": Path("/etc/os-release").read_text(),
              "bundled_libraries": sorted(bundled), "files": manifest,
              "archives": {p.name: {"sha256": digest(p), "bytes": p.stat().st_size}
                           for p in (zip_path, tar_path)}}
    (output / "package-receipt.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"output": str(output), "archives": report["archives"]}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build-linux-steam")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument("--package-only", action="store_true")
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        parser.error("Run inside an x86-64 Linux Steam Runtime SDK")
    if "steamrt4" not in Path("/etc/os-release").read_text():
        parser.error("This package targets Steam Runtime 4; use its SDK")
    defines = (ROOT / "src/defines.h").read_text()
    version = ".".join(re.search(rf'^#define\s+VERSION_{part}\s+(\d+)',
                                defines, re.M).group(1)
                       for part in ("MAJOR", "MINOR", "PATCH", "EXTRA"))
    build = args.build_dir.resolve()
    output = (args.output or ROOT / "scripts/output" / f"linux-steam-{version}").resolve()
    if not args.package_only:
        options = {"CMAKE_BUILD_TYPE": "Release", "SIL_STEAMWORKS": "ON",
                   "CMAKE_PROJECT_TOP_LEVEL_INCLUDES": str(ROOT / "cmake/SplitHarfBuzz.cmake"),
                   "SIL_BUILD_WITH_SDL_SOURCES": "ON", "SIL_USE_LOCAL_DATA": "OFF",
                   "CMAKE_POSITION_INDEPENDENT_CODE": "ON", "SDL_STATIC": "OFF",
                   "SDL_TEST_LIBRARY": "OFF", "SDL_TESTS": "OFF",
                   "SDLIMAGE_VENDORED": "ON", "SDLIMAGE_DEPS_SHARED": "OFF",
                   "SDLIMAGE_SAMPLES": "OFF", "SDLTTF_VENDORED": "ON",
                   "SDLTTF_SAMPLES": "OFF"}
        for codec in ("ANI", "AVIF", "BMP", "GIF", "JPG", "JXL", "LBM", "PCX",
                      "PNM", "QOI", "SVG", "TGA", "TIF", "WEBP", "XCF", "XPM", "XV"):
            options[f"SDLIMAGE_{codec}"] = "OFF"
        run("cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
            *(f"-D{k}={v}" for k, v in options.items()))
        run("cmake", "--build", str(build), "--parallel", str(args.jobs), "--target", "sil-more")
    package(build, output, version)


if __name__ == "__main__":
    main()

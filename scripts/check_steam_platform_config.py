#!/usr/bin/env python3
"""Check Steam SDK routing and simulate desktop CMake generation.

The Linux/macOS probes use the host compiler and stand-in SDL package targets:
they generate build files, but do not compile or run native game binaries.
Native SDK binary headers are checked independently of those stand-ins.
"""
from pathlib import Path
import argparse
import json
import os
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/native-build-checks"
SDK = Path(os.environ.get("STEAMWORKS_SDK_ROOT", ROOT / "external/steamworks_sdk/sdk"))


def run(command, log, env):
    result = subprocess.run(command, cwd=ROOT, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
    log.write_text(result.stdout, encoding="utf-8")
    return result


def binary_architectures(path):
    data = path.read_bytes()
    if data[:2] == b"MZ":
        offset = struct.unpack_from("<I", data, 0x3C)[0]
        assert data[offset:offset + 4] == b"PE\0\0", path
        machine = struct.unpack_from("<H", data, offset + 4)[0]
        return {0x8664: {"x86_64"}, 0x14C: {"x86"}, 0xAA64: {"arm64"}}[machine]
    if data[:4] == b"\x7fELF":
        endian = "<" if data[5] == 1 else ">"
        machine = struct.unpack_from(endian + "H", data, 18)[0]
        return {3: {"x86"}, 62: {"x86_64"}, 183: {"arm64"}}[machine]
    assert data[:4] == b"\xca\xfe\xba\xbe", f"Expected a universal Mach-O: {path}"
    count = struct.unpack_from(">I", data, 4)[0]
    machines = [struct.unpack_from(">I", data, 8 + 20 * i)[0] for i in range(count)]
    return {{0x01000007: "x86_64", 0x0100000C: "arm64"}[machine] for machine in machines}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", default="cmake")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    if os.name == "nt":
        env["PATH"] = "C:/msys64/mingw64/bin;C:/msys64/usr/bin;" + env["PATH"]
    cmake = shutil.which(args.cmake, path=env["PATH"])
    assert cmake, "CMake is required"
    report = {"scope": "configuration simulation; no native Linux/macOS build or execution",
              "sdk_architectures": {}, "routing_cases": [], "generation_cases": [],
              "remaining_native_validation": ["native C/C++ and Apple toolchains",
                                              "real SDL/font dependency configuration and compilation",
                                              "complete game/assets/runtime packaging",
                                              "macOS app bundle, signing and notarization",
                                              "game startup, graphics, audio, controls and saves"]}
    binaries = {
        "win64/steam_api64.dll": {"x86_64"},
        "steam_api.dll": {"x86"},
        "linux64/libsteam_api.so": {"x86_64"},
        "linux32/libsteam_api.so": {"x86"},
        "linuxarm64/libsteam_api.so": {"arm64"},
        "osx/libsteam_api.dylib": {"x86_64", "arm64"},
    }
    for relative, expected in binaries.items():
        actual = binary_architectures(SDK / "redistributable_bin" / relative)
        assert actual == expected, (relative, actual, expected)
        report["sdk_architectures"][relative] = sorted(actual)

    script = OUT / "sdk-selection.cmake"
    script.write_text(f'''cmake_minimum_required(VERSION 3.18)
set(WIN32 FALSE)
set(APPLE FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(WIN32 TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(APPLE TRUE)
endif()
include("{(ROOT / 'cmake/Steamworks.cmake').as_posix()}")
sil_steamworks_sdk_files(library implib)
file(WRITE "${{RESULT_FILE}}" "${{library}}\\n${{implib}}\\n")
''', encoding="utf-8")
    cases = [
        ("windows-x64", "Windows", "AMD64", 8, "", "win64/steam_api64.dll"),
        ("windows-x86", "Windows", "x86", 4, "", "steam_api.dll"),
        ("linux-x64", "Linux", "x86_64", 8, "", "linux64/libsteam_api.so"),
        ("linux-x86", "Linux", "i686", 4, "", "linux32/libsteam_api.so"),
        ("linux-arm64", "Linux", "aarch64", 8, "", "linuxarm64/libsteam_api.so"),
        ("mac-intel", "Darwin", "x86_64", 8, "x86_64", "osx/libsteam_api.dylib"),
        ("mac-arm64", "Darwin", "arm64", 8, "arm64", "osx/libsteam_api.dylib"),
        ("mac-universal", "Darwin", "arm64", 8, "x86_64;arm64", "osx/libsteam_api.dylib"),
        ("reject-windows-arm64", "Windows", "ARM64", 8, "", "Windows SDK requires"),
        ("reject-linux-armv7", "Linux", "armv7l", 4, "", "Unsupported Linux"),
        ("reject-mac-x86", "Darwin", "i386", 4, "i386", "requires a 64-bit"),
        ("reject-mac-ppc", "Darwin", "x86_64", 8, "ppc64", "Unsupported macOS"),
        ("reject-ios", "iOS", "arm64", 8, "arm64", "supports desktop"),
        ("reject-android", "Android", "aarch64", 8, "", "Unsupported Steamworks"),
        ("reject-freebsd", "FreeBSD", "amd64", 8, "", "Unsupported Steamworks"),
        ("reject-app-id", "Linux", "x86_64", 8, "", "positive 32-bit"),
        ("reject-missing-sdk", "Linux", "x86_64", 8, "", "Missing Steamworks SDK"),
    ]
    for name, system, processor, size, architectures, expected in cases:
        result_file = OUT / f"{name}.txt"
        command = [cmake, f"-DCMAKE_SYSTEM_NAME={system}",
                   f"-DCMAKE_SYSTEM_PROCESSOR={processor}", f"-DCMAKE_SIZEOF_VOID_P={size}",
                   f"-DCMAKE_OSX_ARCHITECTURES={architectures}",
                   f"-DSTEAMWORKS_SDK_ROOT={(OUT / 'absent-sdk' if name == 'reject-missing-sdk' else SDK).as_posix()}",
                   f"-DSIL_STEAM_APP_ID={'4294967296' if name == 'reject-app-id' else '5442160'}",
                   f"-DRESULT_FILE={result_file.as_posix()}", "-P", str(script)]
        result = run(command, OUT / f"{name}.log", env)
        if name.startswith("reject-"):
            assert result.returncode != 0 and expected in result.stdout, (name, result.stdout)
        else:
            assert result.returncode == 0, (name, result.stdout)
            selected = result_file.read_text(encoding="utf-8").splitlines()[0]
            assert selected == (SDK / "redistributable_bin" / expected).as_posix(), selected
        report["routing_cases"].append({"name": name, "passed": True})

    # Configure the complete production CMake project. SDL stand-ins isolate
    # application target generation from unavailable native SDL/toolchain builds.
    packages = OUT / "stand-in-packages"
    for package in ("SDL3", "SDL3_image", "SDL3_ttf", "SDL3_mixer"):
        directory = packages / package
        directory.mkdir(parents=True, exist_ok=True)
        (directory / f"{package}Config.cmake").write_text(
            f"add_library({package}::{package} INTERFACE IMPORTED)\n"
            f"set({package}_FOUND TRUE)\n", encoding="utf-8")
    cc = shutil.which("gcc" if os.name == "nt" else "cc", path=env["PATH"])
    cxx = shutil.which("g++" if os.name == "nt" else "c++", path=env["PATH"])
    assert cc and cxx, "A host C/C++ compiler is required for configuration probes"
    override = OUT / "simulation-compiler-features.cmake"
    override.write_text("set(CMAKE_CXX_COMPILE_FEATURES cxx_std_11)\n"
                        "set(CMAKE_CXX_STANDARD_COMPUTED_DEFAULT 17)\n", encoding="utf-8")
    for name, system, processor, architectures in (
            ("linux-generation", "Linux", "x86_64", ""),
            ("mac-generation", "Darwin", "arm64", "x86_64;arm64")):
        build = OUT / name
        command = [cmake, "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
                   f"-DCMAKE_SYSTEM_NAME={system}", f"-DCMAKE_SYSTEM_PROCESSOR={processor}",
                   f"-DCMAKE_C_COMPILER={Path(cc).as_posix()}",
                   f"-DCMAKE_CXX_COMPILER={Path(cxx).as_posix()}",
                   "-DCMAKE_C_COMPILER_FORCED=TRUE", "-DCMAKE_CXX_COMPILER_FORCED=TRUE",
                   f"-DCMAKE_USER_MAKE_RULES_OVERRIDE_CXX={override.as_posix()}",
                   "-DCMAKE_SIZEOF_VOID_P=8", f"-DCMAKE_OSX_ARCHITECTURES={architectures}",
                   # Placeholder only: generation never invokes install_name_tool.
                   f"-DCMAKE_INSTALL_NAME_TOOL={Path(cmake).as_posix()}",
                   "-DCMAKE_OSX_SYSROOT=", f"-DCMAKE_PREFIX_PATH={packages.as_posix()}",
                   "-DSIL_BUILD_WITH_SDL_SOURCES=OFF", "-DSIL_STEAMWORKS=ON",
                   f"-DSTEAMWORKS_SDK_ROOT={SDK.as_posix()}"]
        result = run(command, OUT / f"{name}.log", env)
        assert result.returncode == 0, (name, result.stdout)
        generated = (build / "build.ninja").read_text(encoding="utf-8")
        assert "steamworks.cpp" in generated and "libsteam_api" in generated, name
        report["generation_cases"].append({"name": name, "passed": True,
                                           "native_compilation": False, "stand_in_sdl": True,
                                           "stand_in_toolchain": True})
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: {len(binaries)} SDK binary architectures, {len(cases)} routing/error cases, "
          f"{len(report['generation_cases'])} simulated CMake generations")
    print("Native Linux/macOS compilation, packaging, signing and runtime remain untested.")
    print(OUT / "report.json")


if __name__ == "__main__":
    main()

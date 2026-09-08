#!/usr/bin/env python3
"""Check the version stamps of templates actually loaded by the game.

Run with no arguments for source assets, or add --deployed <installation-root>
to check a staged build too (repeatable). Unused drafts are deliberately ignored.
--self-test exercises malformed/stale fixtures without modifying game data.
--runtime-check also compiles the actual C version gate using build-standard's
configured SDL toolchain. --cmake-check tests the configure guard in an isolated
V-only fixture project. VERSION_EXTRA is a raw-cache/save revision, not part
of the three-component text-template version.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DIRECT_PARSERS = {
    "parse_style_levels": "style-levels",
    "parse_partition_info": "partition",
    "parse_set_info": "set",
}


def uncomment(text):
    # Preserve string/character literals, including comment-looking contents.
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*',
                  lambda m: " " if m[0].startswith(("/*", "//")) else m[0],
                  text, flags=re.S)


def game_version(text):
    text = uncomment(text)
    numbers = []
    for component in ("MAJOR", "MINOR", "PATCH"):
        match = re.search(r'^\s*#\s*define\s+VERSION_' + component +
                          r'\s+\(?\s*(0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*\s*\)?\s*$',
                          text, re.M)
        if not match:
            raise ValueError(f"Cannot read numeric VERSION_{component} from defines.h")
        number = match[1]
        numbers.append(int(number, 16 if number.lower().startswith("0x") else 10))
    match = re.search(r'^\s*#\s*define\s+VERSION_STRING\s+"([^"]+)"', text, re.M)
    triple = tuple(numbers)
    if not match or match[1] != version_string(triple):
        raise ValueError("VERSION_STRING must agree with VERSION_MAJOR/MINOR/PATCH")
    return triple


def version_string(version):
    return ".".join(map(str, version))


def active_templates():
    names = set()
    found_direct = set()
    for path in (ROOT / "src").rglob("*.c"):
        source = uncomment(path.read_text(encoding="utf-8"))
        names.update(re.findall(r'\binit_info\s*\(\s*"([\w-]+)"\s*,', source))
        for call in re.finditer(r'\binit_info_txt\s*\(([^;{}]*)\)\s*;', source):
            parser = call[1].rsplit(",", 1)[-1].strip()
            if parser == "head->parse_info_txt" and path.name == "init-info.c":
                continue
            if parser not in DIRECT_PARSERS:
                raise ValueError(f"Audit new direct template consumer in {path}: {parser}")
            # Bind the parser to its actual immediately preceding path construction.
            preceding = source[:call.start()]
            path_start = preceding.rfind("path_build(")
            bindings = re.findall(r'format\s*\(\s*"%s\.txt"\s*,\s*"([\w-]+)"',
                                  preceding[path_start:])
            if bindings != [DIRECT_PARSERS[parser]]:
                raise ValueError(f"Audit changed filename binding for {parser} in {path}")
            names.add(bindings[0])
            found_direct.add(parser)
    if found_direct != DIRECT_PARSERS.keys() or not names:
        raise ValueError("Runtime template loader discovery is incomplete")
    loader = uncomment((ROOT / "src/init/init-info.c").read_text(encoding="utf-8"))
    for field, macro in (("major", "MAJOR"), ("minor", "MINOR"), ("patch", "PATCH")):
        if not re.search(r'head->v_' + field + r'\s*=\s*VERSION_' + macro + r'\s*;', loader):
            raise ValueError(f"Audit changed init_header version source: {field}")
    return sorted(names)


def check_template(path, expected):
    errors = []
    seen = False
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as error:
        return [f"{path}: {error}"]
    for line_number, line in enumerate(lines, 1):
        if not line or line.startswith("#"):
            continue
        if line.startswith("V:"):
            match = re.fullmatch(r'V:([0-9]+)\.([0-9]+)\.([0-9]+)\s*', line)
            if not match or tuple(map(int, match.groups())) != expected:
                errors.append(f"{path}:{line_number}: {line!r}; expected V:{version_string(expected)}")
            seen = True
        elif not seen:
            errors.append(f"{path}:{line_number}: data precedes required V:{version_string(expected)}")
            break
    if not seen:
        errors.append(f"{path}: missing V:{version_string(expected)}")
    return errors


def self_test():
    expected = (2, 14, 31)
    assert game_version('#define VERSION_STRING "2.14.31"\n#define VERSION_MAJOR (2)\n'
                        '#define VERSION_MINOR 14U // comment\n#define VERSION_PATCH 31\n') == expected
    with tempfile.TemporaryDirectory(prefix="sil-template-version-") as temporary:
        path = Path(temporary) / "limits.txt"
        cases = [("# comment\n\nV:2.14.31\nM:1:1\n", False),
                 ("V:2.14.30\nM:1:1\n", True),
                 ("V:1.14.31\n", True), ("V:2.13.31\n", True),
                 ("# missing\n", True), ("M:1:1\nV:2.14.31\n", True),
                 ("V:2.14\n", True), ("V:2.14.31\nV:2.14.30\n", True)]
        for content, should_fail in cases:
            path.write_text(content, encoding="utf-8")
            assert bool(check_template(path, expected)) == should_fail, content
    print("Template checker fixtures: PASS (major/minor/patch, stale, missing, malformed, ordering)")


def runtime_check():
    parser = (ROOT / "src/init/init-parser-core.c").read_text(encoding="utf-8")
    match = re.search(r'\nerrr init_info_txt\(.*?\n\}', parser, re.S)
    if not match:
        raise ValueError("Cannot extract actual init_info_txt version gate")
    build = ROOT / "build-standard"
    out = ROOT / "scripts/output/template-version-check"
    out.mkdir(parents=True, exist_ok=True)
    source = out / "check.c"
    source.write_text(RUNTIME_HARNESS.replace("/* ACTUAL_PARSER */", match[0]), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
                                   str(build / "_deps/SDL"), env["PATH"]])
    exe = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
                    "-Wall", "-Wextra", "@CMakeFiles/sil-more.dir/includes_C.rsp",
                    str(source), "_deps/SDL/libSDL3.dll.a", "-o", str(exe)],
                   cwd=build, env=env, check=True)
    subprocess.run([str(exe)], cwd=out, env=env, check=True, timeout=30)


def cmake_check(names, expected):
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    with tempfile.TemporaryDirectory(prefix="sil-template-cmake-") as temporary:
        fixture = Path(temporary)
        (fixture / "src/init").mkdir(parents=True)
        (fixture / "lib/edit").mkdir(parents=True)
        loader = ROOT / "src/init/init-info.c"
        (fixture / "src/init/init-info.c").write_bytes(loader.read_bytes())
        for name in names:
            (fixture / f"lib/edit/{name}.txt").write_text(
                f"V:{version_string(expected)}\n", encoding="utf-8")
        # This unreferenced draft must not be checked by either loader discovery.
        (fixture / "lib/edit/character - updates.txt").write_text("V:0.0.0\n", encoding="utf-8")
        guard = (ROOT / "cmake/ValidateTemplateVersions.cmake").as_posix()
        (fixture / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.16)\nproject(TemplateGuard NONE)\n'
            f'set(SIL_VERSION_STRING "{version_string(expected)}")\ninclude("{guard}")\n',
            encoding="utf-8")
        limits = fixture / "lib/edit/limits.txt"
        wrong = (*expected[:2], expected[2] + 1)
        limits.write_text(f"V:{version_string(wrong)}\n", encoding="utf-8")
        command = ["C:/msys64/mingw64/bin/cmake.exe", "-S", str(fixture),
                   "-B", str(fixture / "build"), "-G", "MinGW Makefiles"]
        rejected = subprocess.run(command, env=env, capture_output=True, text=True, timeout=30)
        assert rejected.returncode != 0 and "limits.txt has" in rejected.stderr, rejected.stdout + rejected.stderr
        assert f"expected V:{version_string(expected)}" in rejected.stderr, rejected.stderr
        limits.write_text(f"V:{version_string(expected)}\n", encoding="utf-8")
        accepted = subprocess.run(command, env=env, capture_output=True, text=True, timeout=30)
        assert accepted.returncode == 0, accepted.stdout + accepted.stderr
    print("CMake configure guard: PASS (mismatched limits rejected, corrected assets accepted, unused draft ignored)")


RUNTIME_HARNESS = r'''
#include "angband.h"
#include "init.h"
#include "fs/io_sdl.h"
#include <assert.h>
int error_idx, error_line;
errr sdl_fgets(SDL_IOStream *stream, char *buf, size_t size)
{
    size_t used = 0;
    char next;
    while (used + 1 < size && SDL_ReadIO(stream, &next, 1) == 1) {
        if (next == '\n') { buf[used] = '\0'; return 0; }
        if (next != '\r') buf[used++] = next;
    }
    buf[used] = '\0';
    return used ? 0 : 1;
}
/* ACTUAL_PARSER */
static int records;
static errr accept_record(char *buf, header *head)
{ (void)buf; (void)head; ++records; return 0; }
static void check(const char *text, int major, int minor, int patch, errr expected)
{
    header head = {0}; char buf[1024];
    head.v_major = major; head.v_minor = minor; head.v_patch = patch;
    head.v_extra = 99; /* Text stamps intentionally have no EXTRA component. */
    SDL_IOStream *stream = SDL_IOFromConstMem(text, strlen(text));
    assert(stream); records = 0;
    assert(init_info_txt(stream, buf, &head, accept_record) == expected);
    if (expected) assert(records == 0);
    else assert(records == 1);
    assert(SDL_CloseIO(stream));
}
int main(void)
{
    assert(SDL_Init(0));
    check("# comment\r\n\r\nV:2.14.31\r\nN:1:test\r\n", 2,14,31,0);
    check("V:2.14.30\nN:1:test\n", 2,14,31,PARSE_ERROR_OBSOLETE_FILE);
    check("V:1.14.31\nN:1:test\n", 2,14,31,PARSE_ERROR_OBSOLETE_FILE);
    check("V:2.13.31\nN:1:test\n", 2,14,31,PARSE_ERROR_OBSOLETE_FILE);
    check("# missing\n", 2,14,31,PARSE_ERROR_OBSOLETE_FILE);
    check("N:1:test\nV:2.14.31\n", 2,14,31,PARSE_ERROR_OBSOLETE_FILE);
    check("V:2.14\n", 2,14,31,PARSE_ERROR_OBSOLETE_FILE);
    /* Reproduce the shipped startup failure against the actual engine gate. */
    check("V:0.9.7\nM:F:256\n", 0,9,8,PARSE_ERROR_OBSOLETE_FILE);
    check("V:0.9.8\nM:F:256\n", 0,9,8,0);
    SDL_Quit();
    puts("Actual init_info_txt gate: PASS (including 0.9.7 rejected by 0.9.8)");
    return 0;
}
'''


def main():
    arguments = argparse.ArgumentParser(description=__doc__)
    arguments.add_argument("--deployed", action="append", type=Path, default=[],
                           metavar="INSTALLATION", help="also check this installation's lib/edit")
    arguments.add_argument("--self-test", action="store_true")
    arguments.add_argument("--runtime-check", action="store_true")
    arguments.add_argument("--cmake-check", action="store_true")
    args = arguments.parse_args()
    try:
        expected = game_version((ROOT / "src/defines.h").read_text(encoding="utf-8"))
        names = active_templates()
        errors = []
        for directory in [ROOT / "lib/edit"] + [p / "lib/edit" for p in args.deployed]:
            for name in names:
                errors.extend(check_template(directory / f"{name}.txt", expected))
        if args.self_test:
            self_test()
        if args.runtime_check:
            runtime_check()
        if args.cmake_check:
            cmake_check(names, expected)
        if errors:
            print("\n".join(errors))
            return 1
        print(f"Template versions: PASS ({len(names)} active templates, "
              f"{1 + len(args.deployed)} asset root(s), V:{version_string(expected)}; "
              "unused drafts ignored)")
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Template version check failed: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

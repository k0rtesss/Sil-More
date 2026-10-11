#!/usr/bin/env python3
"""Check option metadata at named save-compatible IDs using the real C tables.

--baseline compiles HEAD's tables against the same semantic expectations.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/option-metadata-check"
TOKEN = re.compile(r'/\*.*?\*/|"(?:\\.|[^"\\])*"|\b(?:NULL|false|true)\b', re.S)


def named_entries(source: str, table: str) -> list[tuple[str, str, int]]:
    body = source.split(table + "[OPT_MAX] = {", 1)[1].split("};", 1)[0]
    value = None
    index = -1
    entries = []
    for match in TOKEN.finditer(body):
        token = match[0]
        if token.startswith("/*"):
            name = re.match(r"/\*\s+(OPT_[a-z]\w*)\b", token)
            if name:
                assert value is not None
                entries.append((name[1], value, index))
        else:
            value = token
            index += 1
    assert index < 256
    assert len({name for name, _, _ in entries}) == len(entries)
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    tables = ROOT / "src/tables.c"
    source = tables.read_text(encoding="utf-8")
    checks = []
    for table in ("option_text", "option_desc", "option_norm"):
        entries = named_entries(source, table)
        for name, value, index in entries:
            expression = (f"{table}[{name}] && strcmp({table}[{name}], {value}) == 0"
                          if value.startswith('"') else f"{table}[{name}] == {value}")
            checks.append(f'    CHECK({name} == {index}, "{table}[{name}] initializer position");')
            checks.append(f'    CHECK({expression}, "{table}[{name}]");')
        checks.append(f'    puts("{table}: {len(entries)} named IDs PASS");')
    fixture = OUT / "check.c"
    fixture.write_text('#include "angband.h"\n#include "externs.h"\n#include <stdio.h>\n#include <string.h>\n'
                      '#define CHECK(expr, name) do { if (!(expr)) { fprintf(stderr, "Metadata mismatch: %s\\n", name); return 1; } } while (0)\n'
                      'int main(void) {\n' + "\n".join(checks) + '\nreturn 0;\n}\n', encoding="utf-8")
    if args.baseline:
        tables = OUT / "tables-baseline.c"
        tables.write_text(subprocess.check_output(
            ["git", "show", "HEAD:src/tables.c"], cwd=ROOT, text=True), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = "C:/msys64/mingw64/bin;C:/msys64/usr/bin;" + env["PATH"]
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(fixture), str(tables),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True)


if __name__ == "__main__":
    main()

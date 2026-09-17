#!/usr/bin/env python3
"""Check production formatter bounds and Angband formatting extensions."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/format-bounds-check"

HARNESS = r'''
#include "format.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char untouched = 'Z';
    assert(strnfmt(&untouched, 0, "") == 0);
    assert(untouched == 'Z');
    assert(strnfmt(NULL, 0, "%s", "ignored") == 0);
    assert(strnfmt(&untouched, 1, "ignored") == 0 && untouched == '\0');

    struct { char text[16]; char sentinel[16]; } guarded;
    const char *formats[] = {"%2048d", "%2048u", "%2048x", "%2048c",
        "%2048p", "%.2048f"};
    for (unsigned i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
    {
        memset(&guarded, 'Z', sizeof(guarded));
        size_t length;
        if (i == 4) length = strnfmt(guarded.text, sizeof(guarded.text), formats[i], &guarded);
        else if (i == 5) length = strnfmt(guarded.text, sizeof(guarded.text), formats[i], 1.0);
        else if (i == 1 || i == 2) length = strnfmt(guarded.text, sizeof(guarded.text), formats[i], 1u);
        else length = strnfmt(guarded.text, sizeof(guarded.text), formats[i], 1);
        assert(length == sizeof(guarded.text) - 1);
        assert(guarded.text[length] == '\0');
        for (unsigned j = 0; j < sizeof(guarded.sentinel); j++)
            assert(guarded.sentinel[j] == 'Z');
    }

    char output[80];
    size_t count = 0;
    strnfmt(output, sizeof(output), "%^s %.*s %+d %lu %% %n",
        "  dragon", 3, "flight", -4, 1234567890ul, &count);
    assert(strcmp(output, "  Dragon fli -4 1234567890 % ") == 0);
    assert(count == strlen(output));
    strnfcat(output, sizeof(output), &count, "%s", "tail");
    assert(strcmp(output, "  Dragon fli -4 1234567890 % tail") == 0);
    assert(count == strlen(output));
    puts("Formatter zero-capacity, wide numeric fields, canaries, extensions and append: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra",
                    "-fstack-protector-all", "@CMakeFiles/sil-more.dir/includes_C.rsp",
                    str(source), str(ROOT / "src/format.c"), "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

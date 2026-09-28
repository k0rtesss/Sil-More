#!/usr/bin/env python3
"""Exercise production notes serialization and byte IO against temporary streams.

Checks exact legacy record bytes, marker-containing notes, long/full buffers,
dead-character loads, and every truncation point without opening player saves.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/save-notes"


def function(path, name):
    source = path.read_text(encoding="utf-8-sig")
    match = re.search(r"^(?:static )?(?:void|bool|byte)\s+" + name + r"\(", source, re.M)
    assert match, name
    brace = source.index("{", match.start())
    tokens = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    depth = 0
    for token in tokens.finditer(source, brace):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():token.end()]
    raise AssertionError(name)


STUBS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t byte;
typedef uint32_t u32b;
typedef const char* cptr;
static FILE* fff;
static const char* stream_path;
static byte xor_byte;
static u32b v_check, x_check, v_stamp, x_stamp, load_byte_offset, save_byte_offset;
static bool write_error, arg_wizard;
static struct { bool is_dead; } player;
#define p_ptr (&player)
static char notes_buffer[NOTES_LENGTH];
static void note(cptr text) { (void)text; }
static void fake_log(cptr text, ...) { (void)text; }
#define log_error fake_log
#define SDL_ReadIO(stream, buf, count) fread(buf, 1, count, stream)
#define SDL_WriteIO(stream, buf, count) fwrite(buf, 1, count, stream)
#define SDL_GetError() "test stream error"
'''

TESTS = r'''
static byte saved[NOTES_LENGTH + 100];
static char expected[NOTES_LENGTH];
static size_t save_notes(const char* text)
{
    assert(strlen(text) < sizeof(notes_buffer));
    memcpy(notes_buffer, text, strlen(text) + 1);
    fff = fopen(stream_path, "w+b"); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    wr_notes();
    size_t length = save_byte_offset;
    assert(!write_error && length < sizeof(saved));
    rewind(fff);
    assert(fread(saved, 1, length, fff) == length);
    fclose(fff);
    return length;
}
static bool load_notes(size_t length, bool dead)
{
    fff = fopen(stream_path, "w+b"); assert(fff);
    assert(fwrite(saved, 1, length, fff) == length);
    rewind(fff);
    xor_byte = 0; v_check = x_check = load_byte_offset = 0;
    player.is_dead = dead;
    memset(notes_buffer, 'X', sizeof(notes_buffer));
    bool failed = rd_notes();
    fclose(fff);
    return failed;
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    stream_path = argv[1];
    const char old_records[] = "First\0\0Last\0" NOTES_MARK;
    size_t length = save_notes("First\n\nLast\n");
    assert(length == sizeof(old_records));
    byte previous = 0;
    for (size_t i = 0; i < length; i++)
    {
        assert((byte)(saved[i] ^ previous) == (byte)old_records[i]);
        previous = saved[i];
    }
    assert(!load_notes(length, false));
    assert(!strcmp(notes_buffer, "First\n\nLast\n"));
    assert(load_byte_offset == length);

    const char embedded[] = "Turn 10: " NOTES_MARK " quoted\nNext note\n";
    length = save_notes(embedded);
    assert(!load_notes(length, false));
    assert(!strcmp(notes_buffer, embedded));
    assert(load_byte_offset == length);
    assert(!load_notes(length, true) && !notes_buffer[0]);
    arg_wizard = true;
    assert(!load_notes(length, true) && !strcmp(notes_buffer, embedded));
    arg_wizard = false;
    for (size_t cut = 0; cut < length; cut++)
    {
        assert(load_notes(cut, false));
        assert(load_notes(cut, true));
    }

    memset(expected, 'a', 4096);
    expected[4096] = '\n'; expected[4097] = '\0';
    length = save_notes(expected);
    assert(!load_notes(length, false) && !strcmp(notes_buffer, expected));
    memset(expected, 'b', sizeof(expected) - 1);
    expected[sizeof(expected) - 1] = '\0';
    length = save_notes(expected);
    assert(!load_notes(length, false) && !strcmp(notes_buffer, expected));
    assert(load_byte_offset == length);
    length = save_notes("unfinished last line");
    assert(!load_notes(length, false));
    assert(!strcmp(notes_buffer, "unfinished last line\n"));
    length = save_notes("");
    assert(!load_notes(length, false) && !notes_buffer[0]);
    puts("Save notes: legacy bytes, embedded marker, long/full buffers, partial final line, alive/dead/wizard loads, and every truncated prefix PASS");
    return 0;
}
'''


def main():
    defines = (ROOT / "src/defines.h").read_text(encoding="utf-8-sig")
    constants = "\n".join(re.search(r"^#define " + name + r" .*$", defines, re.M).group()
                          for name in ("NOTES_LENGTH", "NOTES_MARK"))
    source = constants + "\n" + STUBS
    for path, names in (
        ("load.c", ("sf_get", "rd_byte")),
        ("save.c", ("sf_put", "wr_byte", "wr_string")),
        ("load-notes-inventory.c", ("rd_notes",)),
        ("save-notes-inventory.c", ("wr_notes",)),
    ):
        for name in names:
            source += "\n" + function(ROOT / "src/fs" / path, name)
    OUT.mkdir(parents=True, exist_ok=True)
    fixture = OUT / "check.c"
    fixture.write_text(source + "\n" + TESTS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-Wall", "-Wextra",
                    "-Werror", "-O0", str(fixture), "-o", str(exe)], env=env, check=True)
    with tempfile.TemporaryDirectory(dir=OUT) as directory:
        subprocess.run([str(exe), str(Path(directory) / "notes.bin")],
                       env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()

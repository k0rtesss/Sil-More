#!/usr/bin/env python3
"""Publish real control JSON containing valid Unicode and damaged old messages.

Extracts the production publisher and reuses CMake's objects and SDL file IO.
The ordinary strict GameControl JSON reader is the independent wire oracle.
--baseline restores the old publisher and must fail on malformed history.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/control-json-utf8"
sys.path.insert(0, str(ROOT / "tools"))
from game_control import read_json

REPLACEMENT = "\ufffd"
VALID = [
    "", "plain ASCII", 'quote\" slash\\ newline\n tab\t',
    "M\u00e9moire: F\u00ebanor, for\u00eat, \u00e9p\u00e9e", "\u4e2d\u6587", "\U0001f525\U0001f3ae",
    "\u0080\u07ff\u0800\ud7ff\ue000\uffff\U00010000\U0010ffff",
]
CASES = [(text.encode("utf-8"), text) for text in VALID] + [
    (b"old note {\xc3}", "old note {" + REPLACEMENT + "}"),
    (b"\x80", REPLACEMENT), (b"\xbf", REPLACEMENT),
    (b"\xc0\xaf", REPLACEMENT * 2), (b"\xc1\xbf", REPLACEMENT * 2),
    (b"\xc2", REPLACEMENT), (b"\xe2\x82", REPLACEMENT * 2),
    (b"\xf0\x9f\x92", REPLACEMENT * 3),
    (b"\xe0\x80\xaf", REPLACEMENT * 3),
    (b"\xed\xa0\x80", REPLACEMENT * 3),
    (b"\xf0\x80\x80\xaf", REPLACEMENT * 4),
    (b"\xf4\x90\x80\x80", REPLACEMENT * 4),
    (b"\xf5\x80\x80\x80", REPLACEMENT * 4),
    (b"\xff\xfe", REPLACEMENT * 2),
    (b'\xc3\"\\\n\xc3\xa9\xe2!\x82', REPLACEMENT + '\"\\\n\u00e9' + REPLACEMENT + "!" + REPLACEMENT),
]

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "cJSON.h"
#include <assert.h>
#include <stdint.h>
static const char* fixture_directory;
static bool control_path(char* out, size_t size, const char* name)
{
    return path_build(out, size, fixture_directory, name);
}
/* PRODUCTION */
static bool deny_allocation;
static size_t allocations, frees;
static void* fixture_malloc(size_t size)
{
    if (deny_allocation) return NULL;
    void* result = malloc(size);
    if (result) allocations++;
    return result;
}
static void fixture_free(void* value)
{
    if (value) frees++;
    free(value);
}
static void check_ownership(void)
{
    assert(!control_utf8_span(NULL, 0));
    assert(!control_replace_invalid_utf8(NULL));
    char* valid = cJSON_malloc(6);
    assert(valid);memcpy(valid, "valid", 6);
    assert(control_replace_invalid_utf8(valid) == valid);
    cJSON_free(valid);
    char* empty = cJSON_malloc(1);
    assert(empty);empty[0] = 0;
    assert(control_replace_invalid_utf8(empty) == empty);
    cJSON_free(empty);
    char* invalid = cJSON_malloc(2);
    assert(invalid);invalid[0] = (char)0xc3;invalid[1] = 0;
    size_t previous_frees = frees;
    deny_allocation = true;
    assert(!control_replace_invalid_utf8(invalid));
    deny_allocation = false;
    assert(frees == previous_frees + 1);
}
static const char* cases[] = { /* CASES */ };
int main(int argc, char** argv)
{
    assert(argc == 2);fixture_directory = argv[1];
    assert(SDL_Init(SDL_INIT_EVENTS));
    cJSON_Hooks hooks = {fixture_malloc, fixture_free};
    cJSON_InitHooks(&hooks);check_ownership();
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        cJSON* root = cJSON_CreateObject();
        assert(root);
        cJSON_AddNumberToObject(root, "protocol", 1);
        cJSON_AddStringToObject(root, "session", "fixture-\xc3\xa9");
        cJSON* state = cJSON_AddObjectToObject(root, "state");
        cJSON* messages = cJSON_AddArrayToObject(state, "messages");
        cJSON* message = cJSON_CreateString(cases[i]);
        cJSON_AddItemToArray(messages, message);
        cJSON* name = cJSON_AddStringToObject(state, "name", cases[i]);
        const char* stored_key = i + 1 == sizeof(cases)/sizeof(cases[0])
            ? "key-\xc3" : "key-safe";
        cJSON* key = cJSON_AddStringToObject(state, stored_key, "unchanged");
        cJSON_AddBoolToObject(state, "waiting_for_input", true);
        char file[32];strnfmt(file, sizeof(file), "case-%02u.json", (unsigned)i);
        assert(control_write_json(file, root));
        /* Publishing must leave both valid and damaged stored text untouched. */
        assert(!strcmp(message->valuestring, cases[i]));
        assert(!strcmp(name->valuestring, cases[i]));
        assert(!strcmp(key->string, stored_key));
        cJSON_Delete(root);
    }
    assert(allocations == frees);
    cJSON_InitHooks(NULL);SDL_Quit();
    puts("Real publisher: ownership, allocation failure, immutable source strings PASS");
    return 0;
}
'''


def c_bytes(value: bytes) -> str:
    return '"' + "".join(f"\\x{byte:02x}" for byte in value) + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    out = OUT / "baseline" if args.baseline else OUT
    out.mkdir(parents=True, exist_ok=True)
    production = (ROOT / "src/sdl/control/sdl-control.c").read_text(encoding="utf-8")
    production = production[production.index("static size_t control_utf8_span("):
                            production.index("typedef enum control_read_status")]
    if args.baseline:
        current = "control_replace_invalid_utf8(cJSON_PrintUnformatted(json))"
        assert current in production
        production = production.replace(current, "cJSON_PrintUnformatted(json)", 1)
    source = out / "check.c"
    source.write_text(HARNESS.replace("/* PRODUCTION */", production)
                     .replace("/* CASES */", ",\n".join(c_bytes(raw) for raw, _ in CASES)),
                     encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects
                                  if not obj.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe), str(out)], cwd=ROOT, env=env, check=True, timeout=30)
    for index, (_, expected) in enumerate(CASES):
        path = out / f"case-{index:02d}.json"
        result = read_json(path)
        assert result["protocol"] == 1 and result["session"] == "fixture-\u00e9"
        state = result["state"]
        assert state["messages"] == [expected], (index, state, expected)
        expected_key = "key-" + REPLACEMENT if index + 1 == len(CASES) else "key-safe"
        assert state["name"] == expected and state[expected_key] == "unchanged"
        assert state["waiting_for_input"] is True
        assert not path.with_suffix(".json.tmp").exists()
    print(f"Strict control reader: {len(CASES)} Unicode/damaged-history cases, JSON framing PASS")


if __name__ == "__main__":
    main()

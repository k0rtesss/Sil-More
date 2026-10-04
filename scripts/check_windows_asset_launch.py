#!/usr/bin/env python3
"""Check Windows install-directory selection without opening a player profile.

Run after building. All fixture directories are under ignored scripts/output.
The engine's startup helper is compiled unchanged; only SDL's executable base
path is supplied by the fixture, as a shortcut or launcher would supply it.
"""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / (sys.argv[1] if len(sys.argv) > 1 else "build-standard")
OUT = ROOT / "scripts/output/windows-asset-launch-check"

HARNESS = r'''
#define main game_main
#include "main.c"
#undef main
#include <assert.h>

static const char *fixture_base;
static const char *unicode_install = __UNICODE_INSTALL__;
const char *__wrap_SDL_GetBasePath(void) { return fixture_base; }

static void expect_directory(const char *expected)
{
    char *actual = SDL_GetCurrentDirectory();
    char normalized[1024];
    assert(actual && path_parse(normalized, sizeof(normalized), actual));
    /* SDL's working directory has a trailing separator. */
    size_t n = strlen(normalized);
    while (n && (normalized[n - 1] == '/' || normalized[n - 1] == '\\'))
        normalized[--n] = '\0';
    assert(SDL_strcasecmp(normalized, expected) == 0);
    SDL_free(actual);
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    log_set_quiet(true);
    assert(_putenv_s("ANGBAND_PATH", "") == 0);

    fixture_base = argv[2];
    assert(_chdir(argv[1]) == 0);
    init_install_working_directory(0, NULL);
    expect_directory(argv[2]);
    puts("Packaged launch from an unrelated working directory: PASS");

    assert(_putenv_s("ANGBAND_PATH", "custom-data") == 0);
    assert(_chdir(argv[1]) == 0);
    init_install_working_directory(0, NULL);
    expect_directory(argv[1]);
    puts("Explicit relative ANGBAND_PATH retains its launch directory: PASS");

    assert(_putenv_s("ANGBAND_PATH", "") == 0);
    char *data_args[] = {"sil-more", "-ddata=custom-data"};
    init_install_working_directory(2, data_args);
    expect_directory(argv[1]);
    puts("Explicit relative -d data path retains its launch directory: PASS");

    fixture_base = argv[3];
    init_install_working_directory(0, NULL);
    expect_directory(argv[1]);
    puts("Build-tree executable retains the caller's data directory: PASS");

    fixture_base = unicode_install;
    init_install_working_directory(0, NULL);
    expect_directory(unicode_install);
    puts("Unicode install-directory launch: PASS");

    assert(_chdir(argv[1]) == 0);
    fixture_base = NULL;
    init_install_working_directory(0, NULL);
    expect_directory(argv[1]);
    puts("Unavailable executable base path retains the launch directory: PASS");
    return 0;
}
'''


def main():
    launch = OUT / "launcher"
    install = OUT / "install"
    build_tree = OUT / "build-tree"
    unicode_install = OUT / "install-é-λ"
    for directory in (launch, build_tree, install / "lib/edit", unicode_install / "lib/edit"):
        directory.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS.replace("__UNICODE_INSTALL__",
        json.dumps(str(unicode_install), ensure_ascii=False)), encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [obj for obj in objects if not obj.endswith("/src/main.c.obj")]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(
        [str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")] +
        ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=SDL_GetBasePath",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe), str(launch), str(install), str(build_tree)],
                   cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

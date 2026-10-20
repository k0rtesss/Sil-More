#!/usr/bin/env python3
"""Check the production writable profile layout and legacy progress migration."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-steam"

HARNESS = r'''
#include "angband.h"
#include "achievements.h"
#include "log/log.h"
#include <assert.h>
#include <stdio.h>
static const char* private_home;
static const char* profile_user_folder(SDL_Folder folder) { (void)folder; return private_home; }
#define SDL_GetUserFolder profile_user_folder
#include "init/init-paths.c"
#undef SDL_GetUserFolder

static bool same_path(const char* left, const char* right)
{
    while (*left && *right) {
        char a = *left++;
        char b = *right++;
        if ((a == '\\' ? '/' : a) != (b == '\\' ? '/' : b)) return false;
    }
    return !*left && !*right;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    log_set_quiet(true);
    assert(SDL_Init(0));
    char home[1024], install[1024], apex[1024], path[1024], target[1024];
    SDL_snprintf(home, sizeof(home), "%s/home", argv[1]);
    SDL_snprintf(install, sizeof(install), "%s/install/lib", argv[1]);
    SDL_snprintf(apex, sizeof(apex), "%s/apex", install);
    assert(SDL_CreateDirectory(home)); assert(SDL_CreateDirectory(apex));
    const char* earned = "{\"version\":1,\"unlocked\":{\"INTO_THE_DEEP\":17}}";
    SDL_snprintf(path, sizeof(path), "%s/achievements.json", apex);
    assert(SDL_SaveFile(path, earned, strlen(earned)));
    const unsigned char artefacts[] = {'A','R','D','B',0,0,1,0,0,0,0,0,0,0,0,0};
    SDL_snprintf(path, sizeof(path), "%s/artefacts.db", apex);
    assert(SDL_SaveFile(path, artefacts, sizeof(artefacts)));
    private_home = home;
    SDL_snprintf(path, sizeof(path), "%s/", install);
    init_file_paths(path);
    SDL_snprintf(target, sizeof(target), "%s/sil-more/meta", home);
    assert(same_path(ANGBAND_DIR_APEX, target));
    SDL_snprintf(target, sizeof(target), "%s/sil-more/save", home);
    assert(same_path(ANGBAND_DIR_SAVE, target));
    SDL_snprintf(target, sizeof(target), "%s/sil-more/meta/metaruns", home);
    assert(same_path(ANGBAND_DIR_METARUN, target));
    SDL_snprintf(path, sizeof(path), "%s/artefacts.db", ANGBAND_DIR_APEX);
    size_t length = 0;
    void* bytes = SDL_LoadFile(path, &length);
    assert(bytes && length == sizeof(artefacts) && !memcmp(bytes, artefacts, length)); SDL_free(bytes);
    sil_achievements_init();
    assert(sil_achievements_mask(0) == 1 && sil_achievement_unlocked_at(0) == 17);
    // A later launch must preserve the active profile rather than re-importing.
    SDL_snprintf(path, sizeof(path), "%s/achievements.json", apex);
    const char* changed = "{\"version\":1,\"unlocked\":{\"LIGHT_IN_DARKNESS\":23}}";
    assert(SDL_SaveFile(path, changed, strlen(changed)));
    SDL_snprintf(path, sizeof(path), "%s/", install);
    init_file_paths(path);
    SDL_snprintf(path, sizeof(path), "%s/achievements.json", ANGBAND_DIR_APEX);
    bytes = SDL_LoadFile(path, &length);
    assert(bytes && length == strlen(earned) && !memcmp(bytes, earned, length)); SDL_free(bytes);
    SDL_snprintf(path, sizeof(path), "%s/achievements.json", apex);
    bytes = SDL_LoadFile(path, &length);
    assert(bytes && length == strlen(changed) && !memcmp(bytes, changed, length)); SDL_free(bytes);
    SDL_Quit();
    puts("PASS: production profile paths, legacy achievements/artefacts, original preservation and no overwrite on repeat startup");
    return 0;
}
'''


def main():
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([str(ROOT / "sil-more-windows-steam"),
                                 "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    with tempfile.TemporaryDirectory(prefix="sil-cloud-paths-") as temporary:
        work = Path(temporary)
        source = work / "check.c"
        source.write_text(HARNESS, encoding="utf-8")
        obj = work / "check.o"
        exe = work / "check.exe"
        objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
        response = work / "objects.rsp"
        response.write_text("\n".join('"' + name + '"' for name in objects
                                     if not name.endswith(("/src/main.c.obj", "/src/init/init-paths.c.obj"))))
        subprocess.run(["C:/msys64/mingw64/bin/gcc.exe", "-std=c17", "-DUSE_SDL", "-DWINDOWS",
                        "@CMakeFiles/sil-more.dir/includes_C.rsp", "-c", str(source), "-o", str(obj)],
                       cwd=BUILD, env=env, check=True)
        subprocess.run(["C:/msys64/mingw64/bin/g++.exe", str(obj), "@" + str(response),
                        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)], cwd=BUILD, env=env, check=True)
        subprocess.run([str(exe), str(work)], cwd=work, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

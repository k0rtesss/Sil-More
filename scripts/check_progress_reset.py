#!/usr/bin/env python3
"""Exercise production reset code against disposable trees, never personal data.

Includes a real detached desktop relaunch and real filesystem failures on
Windows or native Linux. This does not drive SDL UI or claim Android/iOS
device restart coverage. Set SIL_PROGRESS_RESET_BUILD to choose a build tree.
"""
from pathlib import Path
import ctypes
from ctypes import wintypes
import json
import os
import shlex
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get("SIL_PROGRESS_RESET_BUILD", ROOT /
    ("build-standard" if os.name == "nt" else "build-linux-steam"))).resolve()
OUT = ROOT / "scripts/output/progress-reset"

HARNESS = r'''
#include "angband.h"
#include "game/progress-reset.c"
#include <assert.h>

cptr ANGBAND_DIR_USER, ANGBAND_DIR_SAVE, ANGBAND_DIR_APEX, ANGBAND_DIR_METARUN;
static char install_path[4096], user_path[4096], save_path[4096];
static char apex_path[4096], meta_path[4096];
static char fixture_root[4096];
static bool fail_remove, fail_launch;

void quit(cptr message) { assert(!message); exit(0); }
void log_log(int level, const char* file, int line, const char* fmt, ...) {}

bool __real_SDL_RemovePath(const char*);
bool __wrap_SDL_RemovePath(const char* path)
{
    if (fail_remove && strstr(path, "runs.db"))
        return SDL_SetError("Fixture removal failure");
    return __real_SDL_RemovePath(path);
}
SDL_Process* __real_SDL_CreateProcessWithProperties(SDL_PropertiesID);
SDL_Process* __wrap_SDL_CreateProcessWithProperties(SDL_PropertiesID properties)
{
    if (fail_launch) { SDL_SetError("Fixture relaunch failure"); return NULL; }
    return __real_SDL_CreateProcessWithProperties(properties);
}

static void put(const char* relative, const char* text)
{
    char path[4096], parent[4096];
    assert(path_build(path, sizeof(path), fixture_root, relative));
    SDL_strlcpy(parent, path, sizeof(parent));
    char* slash = strrchr(parent, '\\');
    if (!slash) slash = strrchr(parent, '/');
    assert(slash); *slash = '\0'; assert(SDL_CreateDirectory(parent));
    assert(SDL_SaveFile(path, text, strlen(text)));
}
static bool exists(const char* relative)
{
    char path[4096]; assert(path_build(path, sizeof(path), fixture_root, relative));
    return SDL_GetPathInfo(path, NULL);
}
static void configure(const char* root)
{
    SDL_strlcpy(fixture_root, root, sizeof(fixture_root));
    assert(path_build(install_path, sizeof(install_path), root, "lib"));
#ifdef SIL_USE_LOCAL_DATA
    assert(path_build(user_path, sizeof(user_path), install_path, "user"));
    assert(path_build(save_path, sizeof(save_path), install_path, "save"));
    assert(path_build(meta_path, sizeof(meta_path), install_path, "apex"));
#else
    assert(path_build(user_path, sizeof(user_path), root, "sil-more"));
    assert(path_build(save_path, sizeof(save_path), user_path, "save"));
    assert(path_build(meta_path, sizeof(meta_path), user_path, "meta/metaruns"));
#endif
#ifdef SIL_USE_LOCAL_DATA
    assert(path_build(apex_path, sizeof(apex_path), install_path, "apex"));
#else
    assert(path_build(apex_path, sizeof(apex_path), user_path, "meta"));
#endif
    ANGBAND_DIR_USER = user_path; ANGBAND_DIR_SAVE = save_path;
    ANGBAND_DIR_APEX = apex_path; ANGBAND_DIR_METARUN = meta_path;
    assert(SDL_CreateDirectory(install_path));
    assert(SDL_CreateDirectory(user_path));
    assert(SDL_CreateDirectory(save_path));
    assert(SDL_CreateDirectory(apex_path));
    assert(SDL_CreateDirectory(meta_path));
    put("lib/edit/monster.txt", "asset template");
    put("lib/xtra/font/test.ttf", "asset font");
    put("lib/help/tutorials.json", "lesson catalogue");
    put("lib/pref/sound.json", "old sound preferences");
    put("lib/pref/palette_presets.json", "asset palettes");
    put("outside/keep.txt", "unrelated data");
}
static void seed(void)
{
#ifdef SIL_USE_LOCAL_DATA
    put("lib/user/sil_sdl.json", "old settings");
    put("lib/user/log.txt", "diagnostics");
    put("lib/save/hero", "old save");
    put("lib/apex/runs.db", "old scores");
    put("lib/apex/runs.db.bak", "old backup");
    put("lib/apex/123/tutorials.json", "old tutorial progress");
    put("lib/apex/achievements.json", "old achievements");
#else
    put("sil-more/sil_sdl.json", "old settings");
    put("sil-more/sil_sdl.json.tmp", "old temporary settings");
    put("sil-more/sound.json", "old sound");
    put("sil-more/log.txt", "diagnostics");
    put("sil-more/save/hero", "old save");
    put("sil-more/meta/runs.db", "old scores");
    put("sil-more/meta/runs.db.bak", "old backup");
    put("sil-more/meta/scores-old.raw", "archived scores");
    put("sil-more/meta/metaruns/123/tutorials.json", "old tutorial progress");
    put("sil-more/data/monster.raw", "derived data");
    put("lib/apex/achievements.json", "old achievements");
    put("lib/apex/artefacts.db", "old discoveries");
    put("lib/save/legacy-hero", "legacy installation save");
#endif
}
static void clean(void)
{
    assert(exists("lib/edit/monster.txt") && exists("lib/xtra/font/test.ttf"));
    assert(exists("lib/help/tutorials.json") && exists("lib/pref/palette_presets.json"));
    assert(exists("outside/keep.txt"));
#ifdef SIL_USE_LOCAL_DATA
    assert(!exists("lib/user/sil_sdl.json") && exists("lib/user/log.txt"));
    assert(!exists("lib/save/hero") && !exists("lib/apex/runs.db"));
    assert(!exists("lib/apex/runs.db.bak") && !exists("lib/apex/123/tutorials.json"));
    assert(!exists("lib/apex/achievements.json"));
    char sound[4096]; assert(path_build(sound, sizeof(sound), install_path, "pref/sound.json"));
    size_t size; char* data = SDL_LoadFile(sound, &size); assert(data);
    assert(size == sizeof(sil_factory_sound_json) - 1);
    assert(memcmp(data, sil_factory_sound_json, size) == 0); SDL_free(data);
#else
    assert(!exists("sil-more/sil_sdl.json") && !exists("sil-more/sil_sdl.json.tmp"));
    assert(!exists("sil-more/sound.json") && exists("sil-more/log.txt"));
    assert(!exists("sil-more/save/hero") && !exists("sil-more/meta/runs.db"));
    assert(!exists("sil-more/meta/runs.db.bak") && !exists("sil-more/meta/scores-old.raw"));
    assert(!exists("sil-more/meta/metaruns/123/tutorials.json"));
    assert(!exists("sil-more/data/monster.raw") && exists("lib/save/legacy-hero"));
#ifdef SIL_IOS
    assert(exists("lib/apex/achievements.json")); /* Bundle remains read-only. */
#else
    assert(!exists("lib/apex/achievements.json") && !exists("lib/apex/artefacts.db"));
#endif
#endif
    assert(sil_progress_reset_skip_legacy_import());
}
int main(int argc, char** argv)
{
    assert(sil_progress_reset_launch_init(&argc, &argv));
    if (argc == 1) {
        const char* root = SDL_getenv("SIL_PROGRESS_RESET_FIXTURE"); assert(root);
        configure(root);
        assert(sil_progress_reset_startup(install_path, user_path)); clean();
        char pid[32];
#ifdef WINDOWS
        unsigned long process_id = GetCurrentProcessId();
#else
        unsigned long process_id = (unsigned long)getpid();
#endif
        strnfmt(pid, sizeof(pid), "%lu", process_id);
        put("restarted.txt", pid);
        return 0;
    }
    assert(argc == 3);
    configure(argv[2]); seed();
    assert(sil_progress_reset_startup(install_path, user_path));
    assert(!sil_progress_reset_skip_legacy_import());
    if (streq(argv[1], "launch")) {
        char error[1024]; sil_progress_reset_request_restart(error, sizeof(error));
        assert(!"restart failed");
    }
    if (streq(argv[1], "launch-failure")) {
        char error[1024]; fail_launch = true;
        assert(!sil_progress_reset_request_restart(error, sizeof(error)));
        assert(strstr(error, "restart") && !sil_progress_reset_restarting());
        assert(!SDL_GetPathInfo(reset_marker, NULL));
        assert(SDL_GetPathInfo(save_path, NULL));
        return 0;
    }
    if (streq(argv[1], "custom") || streq(argv[1], "custom-apex")) {
        char error[1024];
        if (streq(argv[1], "custom")) ANGBAND_DIR_SAVE = fixture_root;
        else ANGBAND_DIR_APEX = install_path;
        assert(!sil_progress_reset_request_restart(error, sizeof(error)));
        assert(strstr(error, "custom") && !SDL_GetPathInfo(reset_marker, NULL));
        return 0;
    }
    if (streq(argv[1], "linked")) {
        char target[4096], junction[4096];
        assert(path_build(target, sizeof(target), fixture_root, "outside"));
        assert(path_build(junction, sizeof(junction), user_path, "linked"));
        /* The Python parent creates the link before this fixture runs. */
        assert(reset_path_type(junction) < 0);
        assert(!reset_check_roots());
        char error[1024];
        assert(!sil_progress_reset_request_restart(error, sizeof(error)));
        assert(!SDL_GetPathInfo(reset_marker, NULL));
        assert(exists("outside/keep.txt"));
        return 0;
    }
    if (streq(argv[1], "invalid")) {
        assert(SDL_SaveFile(reset_marker, "invalid\n", 8));
        assert(!sil_progress_reset_startup(install_path, user_path));
        assert(!sil_progress_reset_skip_legacy_import());
#ifdef SIL_USE_LOCAL_DATA
        assert(exists("lib/save/hero"));
#else
        assert(exists("sil-more/save/hero"));
#endif
        return 0;
    }
    assert(reset_write_marker("pending\n"));
    if (streq(argv[1], "failure")) {
#ifdef WINDOWS
        char uppercase[4096];
        assert(path_build(uppercase, sizeof(uppercase), user_path, ".SIL-MORE-PROGRESS-RESET"));
        assert(SDL_RenamePath(reset_marker, uppercase));
#endif
        fail_remove = true;
        assert(!sil_progress_reset_startup(install_path, user_path));
        assert(!sil_progress_reset_skip_legacy_import());
        assert(SDL_GetPathInfo(reset_marker, NULL));
        fail_remove = false;
    }
    assert(sil_progress_reset_startup(install_path, user_path)); clean();
    /* A completed reset does not erase newly earned progress on later boots. */
#ifdef SIL_USE_LOCAL_DATA
    put("lib/save/new-hero", "new progress");
#else
    put("sil-more/save/new-hero", "new progress");
#endif
    assert(sil_progress_reset_startup(install_path, user_path));
#ifdef SIL_USE_LOCAL_DATA
    assert(exists("lib/save/new-hero"));
#else
    assert(exists("sil-more/save/new-hero"));
#endif
    return 0;
}
'''

STARTUP_HARNESS = r'''
#include "angband.h"
#include "externs.h"
#include "fs/path.h"
#include "game/progress-reset.h"
#include "log/log.h"
#include <assert.h>

bool game_in_progress;
static const char* folder;
const char* __wrap_SDL_GetUserFolder(SDL_Folder type) { return folder; }
char* __wrap_SDL_GetPrefPath(const char* org, const char* app) { return SDL_strdup(folder); }
SDL_Process* __wrap_SDL_CreateProcessWithProperties(SDL_PropertiesID props)
{
    SDL_SetError("Fixture startup relaunch failure"); return NULL;
}
static void put(const char* base, const char* name, const char* text)
{
    char path[4096]; assert(path_build(path, sizeof(path), base, name));
    assert(SDL_SaveFile(path, text, strlen(text)));
}
static bool exists(const char* base, const char* name)
{
    char path[4096]; assert(path_build(path, sizeof(path), base, name));
    return SDL_GetPathInfo(path, NULL);
}
int main(int argc, char** argv)
{
    assert(sil_progress_reset_launch_init(&argc, &argv));
    assert(argc == 2); folder = argv[1]; log_set_quiet(true); assert(SDL_Init(0));
    char install[4096], child[4096], input[4096];
    assert(path_build(install, sizeof(install), folder, "lib"));
    const char* dirs[] = {"save", "apex", "pref"};
    for (int i = 0; i < 3; ++i) {
        assert(path_build(child, sizeof(child), install, dirs[i]));
        assert(SDL_CreateDirectory(child));
    }
    put(install, "save/legacy-hero", "old installation save");
    put(install, "pref/sound.json", "{}");
    strnfmt(input, sizeof(input), "%s/", install); init_file_paths(input);
    assert(strstr(ANGBAND_DIR_USER, "sil-more"));
    assert(exists(ANGBAND_DIR_SAVE, "legacy-hero")); /* Positive import control. */
    assert(!SDL_GetPathInfo(ANGBAND_DIR_METARUN, NULL)); /* Empty migration cleanup. */
    put(ANGBAND_DIR_USER, "sil_sdl.json", "old settings");
    /* The public action must accept the paths assigned by actual startup,
     * reach relaunch, and roll back its request when launch fails. */
    char error[1024];
    assert(!sil_progress_reset_request_restart(error, sizeof(error)));
    if (!strstr(error, "Fixture startup relaunch failure"))
        fprintf(stderr, "Unexpected reset error: %s\nPaths: user=%s save=%s apex=%s metarun=%s\n",
            error, ANGBAND_DIR_USER, ANGBAND_DIR_SAVE, ANGBAND_DIR_APEX, ANGBAND_DIR_METARUN);
    assert(strstr(error, "Fixture startup relaunch failure"));
    assert(!exists(ANGBAND_DIR_USER, ".sil-more-progress-reset"));
    assert(exists(ANGBAND_DIR_SAVE, "legacy-hero"));
    assert(exists(ANGBAND_DIR_USER, "sil_sdl.json"));
    put(ANGBAND_DIR_USER, ".sil-more-progress-reset", "pending\n");
    strnfmt(input, sizeof(input), "%s/", install); init_file_paths(input);
    assert(!exists(ANGBAND_DIR_SAVE, "legacy-hero"));
    assert(!exists(ANGBAND_DIR_USER, "sil_sdl.json"));
    assert(exists(install, "save/legacy-hero"));
    assert(sil_progress_reset_skip_legacy_import());
    put(ANGBAND_DIR_SAVE, "new-hero", "new progress");
    strnfmt(input, sizeof(input), "%s/", install); init_file_paths(input);
    assert(exists(ANGBAND_DIR_SAVE, "new-hero"));
    assert(!exists(ANGBAND_DIR_SAVE, "legacy-hero"));
    assert(sil_progress_reset_skip_legacy_import());
    SDL_Quit(); puts("Production init_file_paths: real-path reset request, initial import control, reset before settings/migration, durable import suppression and new progress preservation: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "reset-check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        str(BUILD / "_deps/SDL"), "C:/msys64/mingw64/bin",
        "C:/msys64/usr/bin", env["PATH"]])
    if os.name == "nt":
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel.WaitForSingleObject.restype = wintypes.DWORD
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel.CloseHandle.restype = wintypes.BOOL
        compiler = ["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp"]
        support = ["CMakeFiles/sil-more.dir/src/fs/path.c.obj",
                   "CMakeFiles/sil-more.dir/src/format.c.obj", "_deps/SDL/libSDL3.dll.a"]
    else:
        commands = json.loads((BUILD / "compile_commands.json").read_text())
        command = next(c for c in commands if c["file"].endswith("/src/game/progress-reset.c"))
        flags = shlex.split(command["command"])
        compiler = [flags[0], "-DUSE_SDL", "-std=gnu17", "-O0",
                    *(f for f in flags if f.startswith("-I"))]
        support = ["CMakeFiles/sil-more.dir/src/fs/path.c.o",
                   "CMakeFiles/sil-more.dir/src/format.c.o", "_deps/SDL/libSDL3.so"]
    common = [*compiler, "-I", str(BUILD / "generated"), str(source), *support,
        "-Wl,--wrap=SDL_RemovePath", "-Wl,--wrap=SDL_CreateProcessWithProperties"]
    for name, defines in (("standard", []), ("portable", ["-DSIL_USE_LOCAL_DATA"]),
                          ("ios-policy", ["-DSIL_IOS"])):
        executable = OUT / f"reset-check-{name}.exe"
        subprocess.run([*common, *defines, "-o", str(executable)],
                       cwd=BUILD, env=env, check=True)
        for case in ("clear", "failure", "custom", "custom-apex", "linked", "invalid", "launch-failure", "launch"):
            if name == "ios-policy" and case in ("launch", "launch-failure"):
                continue
            with tempfile.TemporaryDirectory(prefix=f"{name}-{case}-", dir=OUT) as temp:
                state = Path(temp)
                assert state.resolve().is_relative_to(OUT.resolve())
                if case == "linked":
                    user = state / ("lib/user" if name == "portable" else "sil-more")
                    user.mkdir(parents=True)
                    (state / "outside").mkdir()
                    if os.name == "nt":
                        subprocess.run(["cmd", "/c", "mklink", "/J", str(user / "linked"),
                                        str(state / "outside")], check=True, capture_output=True)
                    else:
                        (user / "linked").symlink_to(state / "outside", target_is_directory=True)
                case_env = {**env, "SIL_PROGRESS_RESET_FIXTURE": str(state)}
                subprocess.run([str(executable), case, str(state)], cwd=state,
                               env=case_env, check=True, timeout=30)
                if case == "launch":
                    deadline = time.monotonic() + 15
                    while not (state / "restarted.txt").exists() and time.monotonic() < deadline:
                        time.sleep(0.05)
                    assert (state / "restarted.txt").exists(), "detached relaunch did not complete"
                    pid = int((state / "restarted.txt").read_text())
                    if os.name == "nt":
                        process = kernel.OpenProcess(0x00100000, False, pid)
                        if process:
                            try:
                                assert kernel.WaitForSingleObject(process, 15000) == 0
                            finally:
                                kernel.CloseHandle(process)
                print(f"{name}: {case}: PASS", flush=True)
    print("Temporary-tree reset, backups/tutorials/preferences, asset preservation, "
          "junction rejection, retry, custom-path/relaunch failure and real desktop relaunch: PASS")
    # Exercise the actual startup/migration wiring as well as the reset module.
    source = OUT / "startup-check.c"
    source.write_text(STARTUP_HARNESS, encoding="utf-8")
    if os.name == "nt":
        objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
        link_libs = ["@CMakeFiles/sil-more.dir/linkLibs.rsp"]
        linker = [compiler[0]]
    else:
        commands = subprocess.run(["ninja", "-t", "commands", "sil-more"], cwd=BUILD,
                                  check=True, capture_output=True, text=True).stdout
        link = shlex.split(next(line for line in commands.splitlines() if " -o sil-more " in line))
        objects = [token for token in link[:link.index("-o")] if token.endswith(".o")]
        start = link.index("-o") + 2
        link_libs = link[start:link.index("&&", start)]
        linker = ["c++"]
    response = OUT / "startup-objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
        if not p.endswith(("/src/main.c.obj", "/src/main.c.o"))), encoding="utf-8")
    executable = OUT / "startup-check.exe"
    harness_obj = OUT / "startup-check.o"
    subprocess.run([*compiler, "-c", str(source), "-o", str(harness_obj)],
                   cwd=BUILD, env=env, check=True)
    subprocess.run([*linker, str(harness_obj), "@" + str(response), *link_libs,
        "-Wl,--wrap=SDL_GetUserFolder", "-Wl,--wrap=SDL_GetPrefPath",
        "-Wl,--wrap=SDL_CreateProcessWithProperties", "-o", str(executable)],
        cwd=BUILD, env=env, check=True)
    startup_env = env.copy()
    startup_env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / name)
        for name in ("SDL_ttf", "SDL_image", "SDL_mixer")), env["PATH"]])
    with tempfile.TemporaryDirectory(prefix="startup-", dir=OUT) as temp:
        assert Path(temp).resolve().is_relative_to(OUT.resolve())
        subprocess.run([str(executable), temp], cwd=temp, env=startup_env,
                       check=True, timeout=30)


if __name__ == "__main__":
    main()

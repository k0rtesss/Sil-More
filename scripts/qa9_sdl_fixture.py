"""Shared isolated SDL/template bootstrap for the QA9 regression fixtures."""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/qa9-sdl-fixture"

HARNESS = r'''

#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "log/bootstrap.h"
#include "ui/question.h"
#include <assert.h>

static cptr make_path(const char* root, const char* tail, bool writable)
{
    char path[1200];
    assert(path_build(path, sizeof(path), root, tail));
    if (writable) assert(SDL_CreateDirectory(path));
    return str_dup(path);
}

int main(int argc, char** argv)
{
    const char* assets = SDL_getenv("CONTROL_TEST_ASSETS");
    const char* profile = SDL_getenv("CONTROL_TEST_PROFILE");
    assert(assets && profile);
    const char* log_base = SDL_getenv("CONTROL_TEST_LOG_BASE");
    init_logger(true, log_base ? log_base : argv[0]);
    ANGBAND_DIR = make_path(assets, "", false);
    ANGBAND_DIR_EDIT = make_path(assets, "edit", false);
    ANGBAND_DIR_FILE = make_path(assets, "file", false);
    ANGBAND_DIR_HELP = make_path(assets, "help", false);
    ANGBAND_DIR_INFO = make_path(assets, "info", false);
    ANGBAND_DIR_PREF = make_path(assets, "pref", false);
    ANGBAND_DIR_XTRA = make_path(assets, "xtra", false);
    ANGBAND_DIR_SCRIPT = make_path(assets, "script", false);
    ANGBAND_DIR_USER = make_path(profile, "user", true);
    ANGBAND_DIR_SAVE = make_path(profile, "save", true);
    ANGBAND_DIR_DATA = make_path(profile, "data", true);
    ANGBAND_DIR_BONE = make_path(profile, "bone", true);
    ANGBAND_DIR_APEX = make_path(profile, "meta", true);
    ANGBAND_DIR_METARUN = make_path(profile, "meta/metaruns", true);
    assert(init_sdl(argc, argv) == 0);
    init_angband();
    sdl_welcome_screen_hide();
    screen_set_startup_supporting_panes_hidden(false);
    screen_set_startup_touch_pane_hidden(false);
    int last = 0;
    (void)last;
    sdl_quit_hook(NULL);
    return 0;
}
'''


def build_harness():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"'+p+'"' for p in objects if not p.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software", SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@"+str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
        cwd=BUILD, env=env, check=True)
    return exe, env
